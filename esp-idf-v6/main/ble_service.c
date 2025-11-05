/**
 * @file ble_service.c
 * @brief BLE Nordic UART Service для передачи GPS данных
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/ble_gatt.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#include "config.h"
#include "common.h"

static const char *TAG = "BLE";

// Forward declarations
static void ble_advertise(void);

// Nordic UART Service UUIDs
static const ble_uuid128_t gatt_svr_svc_uuid =
    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
                     0x93, 0xf3, 0xa3, 0xb5, 0x01, 0x00, 0x40, 0x6e);

static const ble_uuid128_t gatt_svr_chr_tx_uuid =
    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
                     0x93, 0xf3, 0xa3, 0xb5, 0x03, 0x00, 0x40, 0x6e);

static const ble_uuid128_t gatt_svr_chr_rx_uuid =
    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
                     0x93, 0xf3, 0xa3, 0xb5, 0x02, 0x00, 0x40, 0x6e);

// Глобальные переменные
static uint16_t conn_handle = BLE_HS_CONN_HANDLE_NONE;
static uint16_t tx_char_val_handle;
static bool notify_enabled = false;

/**
 * @brief Callback для TX характеристики
 */
static int gatt_svr_chr_access_tx(uint16_t conn_handle, uint16_t attr_handle,
                                   struct ble_gatt_access_ctxt *ctxt, void *arg) {
    switch (ctxt->op) {
    case BLE_GATT_ACCESS_OP_WRITE_DSC:
        // Client подписался на уведомления (CCCD)
        {
            uint16_t val;
            os_mbuf_copydata(ctxt->om, 0, sizeof(val), &val);
            notify_enabled = (val == 1);
            ESP_LOGI(TAG, "TX Notify %s", notify_enabled ? "enabled" : "disabled");
        }
        return 0;

    default:
        return BLE_ATT_ERR_UNLIKELY;
    }
}

/**
 * @brief Callback для RX характеристики
 */
static int gatt_svr_chr_access_rx(uint16_t conn_handle, uint16_t attr_handle,
                                   struct ble_gatt_access_ctxt *ctxt, void *arg) {
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        // Проверка инициализации буфера
        if (!g_ble_rx_buffer) {
            ESP_LOGW(TAG, "RX buffer not initialized yet");
            return BLE_ATT_ERR_UNLIKELY;
        }

        uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
        uint8_t data[512];

        if (len > sizeof(data)) len = sizeof(data);
        os_mbuf_copydata(ctxt->om, 0, len, data);

        // Записываем в RX буфер для отправки в GPS
        ring_buffer_write(g_ble_rx_buffer, data, len);
        ESP_LOGI(TAG, "RX received %d bytes", len);
        return 0;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

/**
 * @brief GATT сервис определение
 */
static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &gatt_svr_svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) { {
            // RX характеристика (WRITE) - ДОЛЖНА БЫТЬ ПЕРВОЙ по стандарту Nordic UART!
            .uuid = &gatt_svr_chr_rx_uuid.u,
            .access_cb = gatt_svr_chr_access_rx,
            .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
        }, {
            // TX характеристика (NOTIFY) - вторая, только NOTIFY без READ
            .uuid = &gatt_svr_chr_tx_uuid.u,
            .access_cb = gatt_svr_chr_access_tx,
            .val_handle = &tx_char_val_handle,
            .flags = BLE_GATT_CHR_F_NOTIFY,
        }, {
            0,
        } },
    },
    { 0 }
};

/**
 * @brief GAP событие callback
 */
static int gap_event_handler(struct ble_gap_event *event, void *arg) {
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        ESP_LOGI(TAG, "Connection %s; status=%d",
                 event->connect.status == 0 ? "established" : "failed",
                 event->connect.status);

        if (event->connect.status == 0) {
            conn_handle = event->connect.conn_handle;

            // Запрашиваем обмен MTU сразу после подключения
            int rc = ble_gattc_exchange_mtu(conn_handle, NULL, NULL);
            if (rc != 0) {
                ESP_LOGW(TAG, "Failed to exchange MTU: %d", rc);
            } else {
                ESP_LOGI(TAG, "MTU exchange initiated");
            }

            // Запрашиваем обновление параметров соединения для максимальной скорости
            // Интервалы 7.5-15 мс для максимальной пропускной способности
            struct ble_gap_upd_params params = {
                .itvl_min = 0x0006,  // 6 * 1.25 мс = 7.5 мс
                .itvl_max = 0x000C,  // 12 * 1.25 мс = 15 мс
                .latency = 0,
                .supervision_timeout = 500, // 500 * 10 мс = 5 сек
                .min_ce_len = 0x0010,       // Мин. длительность события подключения
                .max_ce_len = 0x0300        // Макс. длительность события подключения
            };
            rc = ble_gap_update_params(conn_handle, &params);
            if (rc != 0) {
                ESP_LOGW(TAG, "Failed to update connection params: %d", rc);
            }

            // Инициируем процесс безопасности для bonding
            rc = ble_gap_security_initiate(conn_handle);
            if (rc == BLE_HS_EALREADY) {
                // Устройство уже спарено, проверяем статус шифрования
                ESP_LOGI(TAG, "Device already paired, checking encryption status");
                struct ble_gap_conn_desc desc;
                if (ble_gap_conn_find(conn_handle, &desc) == 0 && desc.sec_state.encrypted) {
                    ESP_LOGI(TAG, "Connection already encrypted with bonded device");
                } else {
                    // Повторяем инициацию безопасности
                    ESP_LOGW(TAG, "Not encrypted, retrying security initiation");
                    ble_gap_security_initiate(conn_handle);
                }
            } else if (rc != 0) {
                ESP_LOGW(TAG, "Security initiate failed: %d", rc);
            } else {
                ESP_LOGI(TAG, "Security/bonding initiated");
            }
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "Disconnect; reason=%d", event->disconnect.reason);
        conn_handle = BLE_HS_CONN_HANDLE_NONE;
        notify_enabled = false;

        // Перезапускаем advertising
        ble_advertise();
        break;

    case BLE_GAP_EVENT_MTU:
        ESP_LOGI(TAG, "MTU update event; conn_handle=%d cid=%d mtu=%d",
                 event->mtu.conn_handle, event->mtu.channel_id, event->mtu.value);
        break;

    case BLE_GAP_EVENT_ENC_CHANGE:
        if (event->enc_change.status == 0) {
            ESP_LOGI(TAG, "Encryption established successfully");
        } else {
            ESP_LOGW(TAG, "Encryption failed: %d", event->enc_change.status);
        }
        break;

    case BLE_GAP_EVENT_PASSKEY_ACTION:
        // Just Works pairing - автоматически подтверждаем
        ESP_LOGI(TAG, "Passkey action: %d", event->passkey.params.action);
        {
            struct ble_sm_io pkey = {0};
            pkey.action = event->passkey.params.action;

            // Для Just Works просто подтверждаем
            if (event->passkey.params.action == BLE_SM_IOACT_NUMCMP) {
                pkey.numcmp_accept = 1;  // Автоматически принимаем
            }

            int rc = ble_sm_inject_io(event->passkey.conn_handle, &pkey);
            ESP_LOGI(TAG, "Passkey inject result: %d", rc);
        }
        break;

    case BLE_GAP_EVENT_REPEAT_PAIRING:
        // Устройство пытается повторно соединиться - удаляем старую запись bonding
        ESP_LOGI(TAG, "Repeat pairing detected, deleting old bond");
        {
            int rc = ble_gap_conn_find(event->repeat_pairing.conn_handle, NULL);
            if (rc == 0) {
                ble_store_util_delete_peer(&event->repeat_pairing.peer_addr);
            }
        }
        return BLE_GAP_REPEAT_PAIRING_RETRY;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        ESP_LOGI(TAG, "Advertising completed, restarting...");
        ble_advertise();
        break;

    default:
        break;
    }
    return 0;
}

/**
 * @brief Запуск advertising
 */
static void ble_advertise(void) {
    struct ble_hs_adv_fields fields = {0};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_TX_POWER;

    fields.name = (uint8_t *)BLE_DEVICE_NAME;
    fields.name_len = strlen(BLE_DEVICE_NAME);
    fields.name_is_complete = 1;

    ble_gap_adv_set_fields(&fields);

    struct ble_gap_adv_params adv_params = {0};
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER,
                     &adv_params, gap_event_handler, NULL);

    ESP_LOGI(TAG, "Advertising started: %s", BLE_DEVICE_NAME);
}

/**
 * @brief NimBLE host callback
 */
static void on_ble_sync(void) {
    ESP_LOGI(TAG, "BLE host synced");

    // Установка предпочтительного PHY на 2M для увеличения скорости
    int rc = ble_gap_set_prefered_default_le_phy(BLE_GAP_LE_PHY_2M_MASK, BLE_GAP_LE_PHY_2M_MASK);
    if (rc == 0) {
        ESP_LOGI(TAG, "2M PHY enabled for maximum throughput");
    } else {
        ESP_LOGW(TAG, "Failed to set 2M PHY: %d", rc);
    }

    // Установка предпочтительного MTU для максимальной скорости передачи
    rc = ble_att_set_preferred_mtu(BLE_MTU);
    if (rc == 0) {
        ESP_LOGI(TAG, "Preferred MTU set to %d bytes", BLE_MTU);
    }

    // Запуск рекламы
    ble_advertise();
}

/**
 * @brief NimBLE host task
 */
static void ble_host_task(void *param) {
    nimble_port_run();
    nimble_port_freertos_deinit();
}

/**
 * @brief Инициализация BLE сервиса
 */
esp_err_t ble_service_init(void) {
    ESP_LOGI(TAG, "Initializing BLE service...");

    // ШАГ 1: Инициализация NimBLE порта (автоматически инициализирует BT контроллер)
    esp_err_t ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nimble_port_init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "NimBLE port initialized (BT controller auto-initialized)");

    // ШАГ 2: Настройка параметров безопасности для спаривания
    ble_hs_cfg.sync_cb = on_ble_sync;
    ble_hs_cfg.gatts_register_cb = NULL;
    ble_hs_cfg.store_status_cb = NULL;

    // Параметры безопасности - включаем bonding для запоминания устройства
    ble_hs_cfg.sm_io_cap = BLE_HS_IO_NO_INPUT_OUTPUT;  // Без UI (нет дисплея/кнопок) - Just Works
    ble_hs_cfg.sm_bonding = 1;                          // Включить bonding (сохраняем ключи)
    ble_hs_cfg.sm_mitm = 0;                             // Отключить MITM (нет UI для подтверждения)
    ble_hs_cfg.sm_sc = 0;                               // Отключить SC - используем Legacy pairing для совместимости
    ble_hs_cfg.sm_min_key_size = 7;                     // Минимальная длина ключа для Legacy pairing (7-16 байт)
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC;

    ESP_LOGI(TAG, "Security configured: Bonding=%d, MITM=%d, SC=%d",
             ble_hs_cfg.sm_bonding, ble_hs_cfg.sm_mitm, ble_hs_cfg.sm_sc);

    // ШАГ 3: Регистрация GAP и GATT сервисов
    ble_svc_gap_init();
    ble_svc_gatt_init();

    ret = ble_gatts_count_cfg(gatt_svr_svcs);
    if (ret != 0) {
        ESP_LOGE(TAG, "ble_gatts_count_cfg failed: %d", ret);
        return ESP_FAIL;
    }

    ret = ble_gatts_add_svcs(gatt_svr_svcs);
    if (ret != 0) {
        ESP_LOGE(TAG, "ble_gatts_add_svcs failed: %d", ret);
        return ESP_FAIL;
    }

    // ШАГ 4: Установка имени устройства
    ret = ble_svc_gap_device_name_set(BLE_DEVICE_NAME);
    if (ret != 0) {
        ESP_LOGW(TAG, "Failed to set device name: %d", ret);
    }

    // ШАГ 5: Запуск NimBLE host task
    nimble_port_freertos_init(ble_host_task);

    ESP_LOGI(TAG, "BLE service initialized successfully");
    return ESP_OK;
}

/**
 * @brief Задача отправки данных по BLE
 */
void ble_task(void *pvParameters) {
    ESP_LOGI(TAG, "BLE task started on core %d", xPortGetCoreID());

    uint8_t buffer[512];

    while (1) {
        // Явная проверка инициализации буферов
        if (!g_ble_tx_buffer || !g_ble_rx_buffer) {
            ESP_LOGE(TAG, "BLE buffers not initialized!");
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        // Проверяем подключение и уведомления
        if (conn_handle != BLE_HS_CONN_HANDLE_NONE && notify_enabled) {
            size_t avail = ring_buffer_available(g_ble_tx_buffer);

            if (avail > 0) {
                // Получаем MTU
                uint16_t mtu = ble_att_mtu(conn_handle);
                size_t max_len = mtu - 3; // ATT header

                if (max_len > sizeof(buffer)) max_len = sizeof(buffer);

                size_t to_send = avail > max_len ? max_len : avail;
                size_t read = ring_buffer_read(g_ble_tx_buffer, buffer, to_send);

                if (read > 0) {
                    struct os_mbuf *om = ble_hs_mbuf_from_flat(buffer, read);
                    if (om) {
                        int rc = ble_gatts_notify_custom(conn_handle, tx_char_val_handle, om);
                        if (rc != 0) {
                            ESP_LOGW(TAG, "Notify failed: %d", rc);
                        }
                    }
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }

    vTaskDelete(NULL);
}
