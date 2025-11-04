/**
 * @file main.c
 * @brief Основной файл GNSS-BLE-WiFi моста на ESP-IDF v6
 *
 * Портировано с Arduino на ESP-IDF v6 с поддержкой:
 * - ESP32-C3, ESP32-C6, ESP32-S3
 * - LVGL v9 для всех дисплеев
 * - NimBLE для Bluetooth
 * - WiFi AP с TCP сервером
 * - UART для GPS/GNSS модулей
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "lvgl.h"

#include "config.h"

// Теги для логирования
static const char *TAG = "MAIN";

// ==================================================
// СТРУКТУРЫ ДАННЫХ
// ==================================================

/**
 * @brief Информация о спутниках для каждой GNSS системы
 */
typedef struct {
    int visible;        // Видимые спутники (из GSV)
    int used;          // Используемые спутники (из GSA)
    uint32_t lastUpdate;
} sat_info_t;

/**
 * @brief Данные GPS/GNSS
 */
typedef struct {
    double latitude;
    double longitude;
    double altitude;
    double lat_accuracy;
    double lon_accuracy;
    double vert_accuracy;
    int satellites;
    int fix_quality;    // 0=NO FIX, 1=GPS, 2=DGPS, 4=RTK FIXED, 5=RTK FLOAT
    bool valid;
    uint32_t last_update;
    uint32_t last_gst_update;
} gps_data_t;

/**
 * @brief Данные спутников по системам
 */
typedef struct {
    sat_info_t gps;
    sat_info_t glonass;
    sat_info_t galileo;
    sat_info_t beidou;
    sat_info_t qzss;
} sat_data_t;

/**
 * @brief Кольцевой буфер для BLE/WiFi данных
 */
typedef struct {
    uint8_t *data;
    volatile size_t head;
    volatile size_t tail;
    volatile bool overflow;
    size_t size;
    SemaphoreHandle_t mutex;
} ring_buffer_t;

// ==================================================
// ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ
// ==================================================

static gps_data_t g_gps_data = {0};
static sat_data_t g_sat_data = {0};

// Буферы для данных
static ring_buffer_t *g_ble_tx_buffer = NULL;  // TX буфер (GNSS -> BLE/WiFi)
static ring_buffer_t *g_ble_rx_buffer = NULL;  // RX буфер (BLE/WiFi -> GNSS)

// Хэндлы задач
static TaskHandle_t uart_task_handle = NULL;
static TaskHandle_t ble_task_handle = NULL;
static TaskHandle_t wifi_task_handle = NULL;
static TaskHandle_t display_task_handle = NULL;
static TaskHandle_t gps_parser_task_handle = NULL;

// LVGL объекты
static lv_disp_t *g_disp = NULL;
static lv_obj_t *g_screen = NULL;

// Дескрипторы устройств
static esp_lcd_panel_handle_t g_panel_handle = NULL;
static spi_device_handle_t g_spi_handle = NULL;

// ==================================================
// РЕАЛИЗАЦИЯ КОЛЬЦЕВОГО БУФЕРА
// ==================================================

/**
 * @brief Создание кольцевого буфера
 */
ring_buffer_t* ring_buffer_create(size_t size) {
    ring_buffer_t *rb = malloc(sizeof(ring_buffer_t));
    if (!rb) {
        ESP_LOGE(TAG, "Failed to allocate ring buffer");
        return NULL;
    }

    rb->data = malloc(size);
    if (!rb->data) {
        ESP_LOGE(TAG, "Failed to allocate ring buffer data");
        free(rb);
        return NULL;
    }

    rb->head = 0;
    rb->tail = 0;
    rb->overflow = false;
    rb->size = size;
    rb->mutex = xSemaphoreCreateMutex();

    if (!rb->mutex) {
        ESP_LOGE(TAG, "Failed to create mutex");
        free(rb->data);
        free(rb);
        return NULL;
    }

    return rb;
}

/**
 * @brief Запись данных в кольцевой буфер
 */
size_t ring_buffer_write(ring_buffer_t *rb, const uint8_t *data, size_t len) {
    if (!rb || !data || len == 0) return 0;

    xSemaphoreTake(rb->mutex, portMAX_DELAY);

    size_t written = 0;
    for (size_t i = 0; i < len; i++) {
        size_t next_head = (rb->head + 1) % rb->size;

        if (next_head == rb->tail) {
            // Буфер полон - перезаписываем старые данные
            rb->tail = (rb->tail + 1) % rb->size;
            rb->overflow = true;
        }

        rb->data[rb->head] = data[i];
        rb->head = next_head;
        written++;
    }

    xSemaphoreGive(rb->mutex);
    return written;
}

/**
 * @brief Чтение данных из кольцевого буфера
 */
size_t ring_buffer_read(ring_buffer_t *rb, uint8_t *data, size_t max_len) {
    if (!rb || !data || max_len == 0) return 0;

    xSemaphoreTake(rb->mutex, portMAX_DELAY);

    size_t read = 0;
    while (rb->tail != rb->head && read < max_len) {
        data[read++] = rb->data[rb->tail];
        rb->tail = (rb->tail + 1) % rb->size;
    }

    if (rb->overflow && read > 0) {
        rb->overflow = false;
    }

    xSemaphoreGive(rb->mutex);
    return read;
}

/**
 * @brief Получить количество доступных байт
 */
size_t ring_buffer_available(ring_buffer_t *rb) {
    if (!rb) return 0;

    xSemaphoreTake(rb->mutex, portMAX_DELAY);

    size_t avail;
    if (rb->head >= rb->tail) {
        avail = rb->head - rb->tail;
    } else {
        avail = rb->size - rb->tail + rb->head;
    }

    xSemaphoreGive(rb->mutex);
    return avail;
}

// ==================================================
// ИНИЦИАЛИЗАЦИЯ UART
// ==================================================

/**
 * @brief Инициализация UART для GPS модуля
 */
esp_err_t init_uart(void) {
    ESP_LOGI(TAG, "Initializing UART on RX:%d TX:%d", UART_RX_PIN, UART_TX_PIN);

    uart_config_t uart_config = {
        .baud_rate = GPS_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_1, UART_BUF_SIZE * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_1, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_1, UART_TX_PIN, UART_RX_PIN,
                                  UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    ESP_LOGI(TAG, "UART initialized successfully");
    return ESP_OK;
}

// ==================================================
// ЗАДАЧА UART
// ==================================================

/**
 * @brief Задача обработки UART
 * Читает данные из GPS и пишет в TX буфер
 * Читает команды из RX буфера и отправляет в GPS
 */
static void uart_task(void *pvParameters) {
    ESP_LOGI(TAG, "UART task started on core %d", xPortGetCoreID());

    uint8_t *uart_data = malloc(UART_BUF_SIZE);
    uint8_t *rx_data = malloc(RX_BUFFER_SIZE);

    while (1) {
        // Читаем данные из GPS
        int len = uart_read_bytes(UART_NUM_1, uart_data, UART_BUF_SIZE,
                                   pdMS_TO_TICKS(20));
        if (len > 0) {
            // Записываем в TX буфер для отправки по BLE/WiFi
            ring_buffer_write(g_ble_tx_buffer, uart_data, len);
        }

        // Проверяем буфер RX на наличие команд от BLE/WiFi
        size_t rx_avail = ring_buffer_available(g_ble_rx_buffer);
        if (rx_avail > 0) {
            size_t to_read = rx_avail > RX_BUFFER_SIZE ? RX_BUFFER_SIZE : rx_avail;
            size_t read = ring_buffer_read(g_ble_rx_buffer, rx_data, to_read);

            if (read > 0) {
                // Отправляем команды в GPS модуль
                uart_write_bytes(UART_NUM_1, (const char*)rx_data, read);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }

    free(uart_data);
    free(rx_data);
    vTaskDelete(NULL);
}

// ==================================================
// ГЛАВНАЯ ФУНКЦИЯ
// ==================================================

void app_main(void) {
    ESP_LOGI(TAG, "===========================================");
    ESP_LOGI(TAG, "GNSS BLE/WiFi Bridge - ESP-IDF v6");
    ESP_LOGI(TAG, "Board: %s", BOARD_NAME);
    ESP_LOGI(TAG, "===========================================");

    // Инициализация NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Создание буферов
    g_ble_tx_buffer = ring_buffer_create(RING_BUFFER_SIZE);
    g_ble_rx_buffer = ring_buffer_create(RX_BUFFER_SIZE);

    if (!g_ble_tx_buffer || !g_ble_rx_buffer) {
        ESP_LOGE(TAG, "Failed to create ring buffers");
        return;
    }

    // Инициализация UART
    ESP_ERROR_CHECK(init_uart());

    // Создание задач
    xTaskCreatePinnedToCore(uart_task, "uart_task", 4096, NULL, 5,
                           &uart_task_handle,
#if defined(TARGET_ESP32_S3)
                           1);  // На S3 UART на ядре 1
#else
                           0);  // На C3/C6 одно ядро
#endif

    ESP_LOGI(TAG, "System initialized successfully");
    ESP_LOGI(TAG, "Free heap: %lu bytes", esp_get_free_heap_size());

    // TODO: Запустить остальные задачи (BLE, WiFi, Display, GPS Parser)
    // Они будут добавлены в отдельных файлах
}
