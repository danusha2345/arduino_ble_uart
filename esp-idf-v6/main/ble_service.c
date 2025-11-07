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
#include "host/ble_store.h"
#include "host/ble_hs_pvcy.h"
#include "store/config/ble_store_config.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#include "config.h"
#include "common.h"

// Forward declarations для NimBLE store config функций (из esp-idf/components/bt)
void ble_store_config_init(void);
int ble_store_util_status_rr(struct ble_store_status_event *event, void *arg);

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
    case BLE_GATT_ACCESS_OP_READ_CHR:
        // Client читает TX характеристику - отдаем данные из буфера (fallback для клиентов без Notify)
        {
            uint8_t temp_buf[512];
            size_t avail = ring_buffer_available(g_ble_tx_buffer);
            size_t to_read = avail > 512 ? 512 : avail;

            if (to_read > 0 && g_ble_tx_buffer) {
                size_t read = ring_buffer_read(g_ble_tx_buffer, temp_buf, to_read);
                int rc = os_mbuf_append(ctxt->om, temp_buf, read);
                ESP_LOGI(TAG, "TX characteristic READ by client: %d bytes (conn_handle=%d)", read, conn_handle);
                return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
            } else {
                // Нет данных - возвращаем пустое значение
                static uint8_t empty_val = 0;
                int rc = os_mbuf_append(ctxt->om, &empty_val, sizeof(empty_val));
                ESP_LOGI(TAG, "TX characteristic READ by client: empty (conn_handle=%d)", conn_handle);
                return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
            }
        }

    case BLE_GATT_ACCESS_OP_READ_DSC:
        // Client читает CCCD дескриптор
        {
            uint16_t val = notify_enabled ? 0x0001 : 0x0000;
            int rc = os_mbuf_append(ctxt->om, &val, sizeof(val));
            ESP_LOGI(TAG, "CCCD read: returning 0x%04x (notify %s)", val, notify_enabled ? "enabled" : "disabled");
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }

    case BLE_GATT_ACCESS_OP_WRITE_DSC:
        // Client подписался на уведомления (CCCD)
        {
            uint16_t val = 0;
            os_mbuf_copydata(ctxt->om, 0, sizeof(val), &val);
            ESP_LOGI(TAG, "CCCD write: value=0x%04x (bytes read=%d)", val, sizeof(val));

            // CCCD: 0x0001 = notifications, 0x0002 = indications, 0x0000 = disabled
            notify_enabled = (val == 0x0001);
            ESP_LOGI(TAG, "TX Notify %s (CCCD=0x%04x)", notify_enabled ? "ENABLED" : "DISABLED", val);
        }
        return 0;

    default:
        ESP_LOGW(TAG, "TX characteristic: unhandled operation %d", ctxt->op);
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
            // Характеристика видна без pairing, но MITM автоматически запросит PIN при записи
            .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
        }, {
            // TX характеристика (NOTIFY + READ) - позволяет клиенту прочитать перед подпиской
            .uuid = &gatt_svr_chr_tx_uuid.u,
            .access_cb = gatt_svr_chr_access_tx,
            .val_handle = &tx_char_val_handle,
            // Характеристика видна без pairing, но MITM автоматически запросит PIN при чтении/notify
            .flags = BLE_GATT_CHR_F_NOTIFY | BLE_GATT_CHR_F_READ,
            .descriptors = (struct ble_gatt_dsc_def[]) { {
                // CCCD дескриптор для управления notifications (ОБЯЗАТЕЛЕН!)
                .uuid = BLE_UUID16_DECLARE(BLE_GATT_DSC_CLT_CFG_UUID16),
                // CCCD также видимый без pairing
                .att_flags = BLE_ATT_F_READ | BLE_ATT_F_WRITE,
                .access_cb = gatt_svr_chr_access_tx,
            }, {
                0  // Терминатор массива дескрипторов
            } },
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
            int rc;

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

            // ВАЖНО: Инициируем pairing сразу при подключении (для Display Only режима)
            // Это заставит телефон запросить PIN-код ДО GATT discovery
            ESP_LOGI(TAG, "Connection established, initiating security/pairing...");
            rc = ble_gap_security_initiate(conn_handle);
            if (rc != 0) {
                ESP_LOGW(TAG, "Failed to initiate security: %d (error: %s)", rc,
                         rc == BLE_HS_ENOTCONN ? "not connected" :
                         rc == BLE_HS_EALREADY ? "already paired/pairing" : "other");
            } else {
                ESP_LOGI(TAG, "Security initiation started - waiting for PIN request");
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
        // Обработка запроса PIN-кода
        ESP_LOGI(TAG, "Passkey action: %d", event->passkey.params.action);
        {
            struct ble_sm_io pkey = {0};
            pkey.action = event->passkey.params.action;

            // Фиксированный PIN-код (6 цифр, максимум 999999)
            #define BLE_FIXED_PASSKEY 123456

            if (event->passkey.params.action == BLE_SM_IOACT_DISP) {
                // Display Only - показываем PIN-код
                pkey.passkey = BLE_FIXED_PASSKEY;
                ESP_LOGI(TAG, "===========================================");
                ESP_LOGI(TAG, "  BLE PAIRING PIN CODE: %06lu", (unsigned long)BLE_FIXED_PASSKEY);
                ESP_LOGI(TAG, "  Введите этот код на телефоне");
                ESP_LOGI(TAG, "===========================================");
            } else if (event->passkey.params.action == BLE_SM_IOACT_NUMCMP) {
                // Numeric Comparison (для LE Secure Connections с Display+YesNo)
                pkey.numcmp_accept = 1;
                ESP_LOGI(TAG, "Numeric comparison: auto-accepting");
            }

            int rc = ble_sm_inject_io(event->passkey.conn_handle, &pkey);
            ESP_LOGI(TAG, "Passkey inject result: %d", rc);
        }
        break;

    case BLE_GAP_EVENT_REPEAT_PAIRING:
        // Устройство пытается повторно соединиться - удаляем старую запись bonding
        ESP_LOGI(TAG, "Repeat pairing detected, deleting old bond");
        {
            struct ble_gap_conn_desc desc;
            int rc = ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
            if (rc == 0) {
                ble_store_util_delete_peer(&desc.peer_id_addr);
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
    // Advertising packet (до 31 байта): Flags + TX Power + UUID
    struct ble_hs_adv_fields adv_fields = {0};
    adv_fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    adv_fields.tx_pwr_lvl_is_present = 1;
    adv_fields.tx_pwr_lvl = BLE_TX_POWER;

    // КРИТИЧЕСКИ ВАЖНО! Включаем Service UUID в advertising
    // чтобы Android приложения могли найти Nordic UART Service
    adv_fields.uuids128 = &gatt_svr_svc_uuid;
    adv_fields.num_uuids128 = 1;
    adv_fields.uuids128_is_complete = 1;

    int rc = ble_gap_adv_set_fields(&adv_fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to set advertising fields: %d", rc);
        return;
    }

    // Scan response packet (до 31 байта): Имя устройства
    struct ble_hs_adv_fields rsp_fields = {0};
    rsp_fields.name = (uint8_t *)BLE_DEVICE_NAME;
    rsp_fields.name_len = strlen(BLE_DEVICE_NAME);
    rsp_fields.name_is_complete = 1;

    rc = ble_gap_adv_rsp_set_fields(&rsp_fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to set scan response fields: %d", rc);
        return;
    }

    // Параметры advertising
    struct ble_gap_adv_params adv_params = {0};
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER,
                          &adv_params, gap_event_handler, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to start advertising: %d", rc);
        return;
    }

    ESP_LOGI(TAG, "Advertising started: %s (UUID in adv, name in scan rsp)", BLE_DEVICE_NAME);
}

/**
 * @brief GATT registration callback
 */
static void gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg) {
    char buf[BLE_UUID_STR_LEN];

    switch (ctxt->op) {
    case BLE_GATT_REGISTER_OP_SVC:
        ESP_LOGI(TAG, "Registered service %s with handle=%d",
                 ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf),
                 ctxt->svc.handle);
        break;

    case BLE_GATT_REGISTER_OP_CHR:
        ESP_LOGI(TAG, "Registered characteristic %s with def_handle=%d val_handle=%d",
                 ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf),
                 ctxt->chr.def_handle,
                 ctxt->chr.val_handle);
        break;

    case BLE_GATT_REGISTER_OP_DSC:
        ESP_LOGI(TAG, "Registered descriptor %s with handle=%d",
                 ble_uuid_to_str(ctxt->dsc.dsc_def->uuid, buf),
                 ctxt->dsc.handle);
        break;

    default:
        break;
    }
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

    // ШАГ 0: Освобождаем память от классического Bluetooth (ESP-IDF v6 требование)
    esp_err_t ret = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to release classic BT memory: %s", esp_err_to_name(ret));
    }

    // ШАГ 1: Инициализация NimBLE порта (автоматически инициализирует и включает контроллер)
    ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nimble_port_init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "NimBLE port initialized (BT controller auto-initialized)");

    // ШАГ 2: Настройка параметров безопасности для спаривания
    ble_hs_cfg.sync_cb = on_ble_sync;
    ble_hs_cfg.gatts_register_cb = gatt_svr_register_cb;  // Callback для логирования регистрации GATT
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;  // Callback для управления bonding storage

    // Параметры безопасности - включаем bonding с фиксированным PIN-кодом
    ble_hs_cfg.sm_io_cap = BLE_HS_IO_DISPLAY_ONLY;     // Display Only - показываем PIN (фиксированный)
    ble_hs_cfg.sm_bonding = 1;                          // Включить bonding (сохраняем ключи)
    ble_hs_cfg.sm_mitm = 1;                             // ВКЛЮЧИТЬ MITM для запроса PIN-кода
    ble_hs_cfg.sm_sc = 1;                               // ВКЛЮЧИТЬ Secure Connections для совместимости с Android
    ble_hs_cfg.sm_keypress = 0;                         // Отключить keypress notifications

    // КРИТИЧЕСКИ ВАЖНО для bonding: распространяем ВСЕ типы ключей
    // ENC - encryption, ID - identity (IRK), SIGN - signing (CSRK)
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID | BLE_SM_PAIR_KEY_DIST_SIGN;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID | BLE_SM_PAIR_KEY_DIST_SIGN;

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

    // ШАГ 5: КРИТИЧЕСКИ ВАЖНО! Инициализация хранилища для bonding ключей
    // Без этого bonding не будет работать - ключи негде сохранять!
    ble_store_config_init();
    ESP_LOGI(TAG, "Bonding storage initialized (NVS)");

    // ШАГ 6: Запуск NimBLE host task
    nimble_port_freertos_init(ble_host_task);

    ESP_LOGI(TAG, "BLE service initialized successfully");
    return ESP_OK;
}

/**
 * @brief Отправка данных по BLE (вызывается из broadcast_task)
 * НЕ читает из g_ble_tx_buffer - получает данные готовыми!
 */
void ble_broadcast_data(const uint8_t *data, size_t len) {
    if (!data || len == 0) return;

    // Проверяем подключение и уведомления
    if (conn_handle == BLE_HS_CONN_HANDLE_NONE || !notify_enabled) {
        // Диагностика (но не слишком часто)
        static uint32_t last_warn = 0;
        uint32_t now = xTaskGetTickCount();
        if (now - last_warn > pdMS_TO_TICKS(5000)) {  // Раз в 5 секунд
            if (conn_handle == BLE_HS_CONN_HANDLE_NONE) {
                ESP_LOGW(TAG, "BLE not connected, skipping %d bytes", len);
            } else if (!notify_enabled) {
                ESP_LOGW(TAG, "BLE notifications not enabled by client, skipping %d bytes", len);
            }
            last_warn = now;
        }
        return;
    }

    // Получаем MTU
    uint16_t mtu = ble_att_mtu(conn_handle);
    size_t max_payload = mtu - 3; // ATT header = 3 байта

    // Отправляем данные порциями по MTU
    size_t offset = 0;
    size_t total_sent = 0;
    while (offset < len) {
        size_t chunk_size = (len - offset) > max_payload ? max_payload : (len - offset);

        struct os_mbuf *om = ble_hs_mbuf_from_flat(data + offset, chunk_size);
        if (om) {
            int rc = ble_gatts_notify_custom(conn_handle, tx_char_val_handle, om);
            if (rc != 0) {
                ESP_LOGW(TAG, "Notify failed: %d", rc);
                break;  // Прерываем при ошибке
            }
            total_sent += chunk_size;
        } else {
            ESP_LOGE(TAG, "Failed to allocate mbuf");
            break;
        }

        offset += chunk_size;
    }

    // Логируем успешную отправку (но не слишком часто)
    static uint32_t last_log = 0;
    static size_t bytes_since_log = 0;
    bytes_since_log += total_sent;
    uint32_t now = xTaskGetTickCount();
    if (now - last_log > pdMS_TO_TICKS(5000)) {  // Раз в 5 секунд
        ESP_LOGI(TAG, "BLE TX: sent %d bytes (total %d in last 5s)", total_sent, bytes_since_log);
        bytes_since_log = 0;
        last_log = now;
    }
}
