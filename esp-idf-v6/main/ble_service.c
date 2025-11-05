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
        ESP_LOGI(TAG, "TX Read request");
        return 0;

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
            // TX характеристика (NOTIFY)
            .uuid = &gatt_svr_chr_tx_uuid.u,
            .access_cb = gatt_svr_chr_access_tx,
            .val_handle = &tx_char_val_handle,
            .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
        }, {
            // RX характеристика (WRITE)
            .uuid = &gatt_svr_chr_rx_uuid.u,
            .access_cb = gatt_svr_chr_access_rx,
            .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
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

            // Запрашиваем обновление параметров соединения
            struct ble_gap_upd_params params = {
                .itvl_min = BLE_GAP_INITIAL_CONN_ITVL_MIN,
                .itvl_max = BLE_GAP_INITIAL_CONN_ITVL_MAX,
                .latency = 0,
                .supervision_timeout = BLE_GAP_INITIAL_SUPERVISION_TIMEOUT,
            };
            ble_gap_update_params(conn_handle, &params);
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "Disconnect; reason=%d", event->disconnect.reason);
        conn_handle = BLE_HS_CONN_HANDLE_NONE;
        notify_enabled = false;

        // Перезапускаем advertising
        ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER,
                         &(struct ble_gap_adv_params){ 0 }, gap_event_handler, NULL);
        break;

    case BLE_GAP_EVENT_MTU:
        ESP_LOGI(TAG, "MTU update event; conn_handle=%d cid=%d mtu=%d",
                 event->mtu.conn_handle, event->mtu.channel_id, event->mtu.value);
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
    ble_hs_id_infer_auto(0, NULL);
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

    esp_err_t ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nimble_port_init failed: %d", ret);
        return ret;
    }

    ble_hs_cfg.sync_cb = on_ble_sync;
    ble_hs_cfg.gatts_register_cb = NULL;
    ble_hs_cfg.store_status_cb = NULL;

    // Регистрация сервисов
    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_gatts_count_cfg(gatt_svr_svcs);
    ble_gatts_add_svcs(gatt_svr_svcs);

    // Установка имени устройства
    ble_svc_gap_device_name_set(BLE_DEVICE_NAME);

    // Запуск NimBLE host task
    nimble_port_freertos_init(ble_host_task);

    ESP_LOGI(TAG, "BLE service initialized");
    return ESP_OK;
}

/**
 * @brief Задача отправки данных по BLE
 */
void ble_task(void *pvParameters) {
    ESP_LOGI(TAG, "BLE task started on core %d", xPortGetCoreID());

    uint8_t buffer[512];

    while (1) {
        // Проверяем есть ли подключение и включены ли уведомления
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
