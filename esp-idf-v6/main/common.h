/**
 * @file common.h
 * @brief Общие определения для всех модулей проекта
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_err.h"

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
    // Время
    int hour;           // Часы UTC (0-23)
    int minute;         // Минуты (0-59)
    int second;         // Секунды (0-59)
    int timezone_offset_minutes;  // Смещение часового пояса в минутах
    bool time_valid;    // Флаг валидности времени
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

extern gps_data_t g_gps_data;
extern sat_data_t g_sat_data;
extern ring_buffer_t *g_ble_tx_buffer;
extern ring_buffer_t *g_ble_rx_buffer;

// ==================================================
// ФУНКЦИИ RING BUFFER
// ==================================================

ring_buffer_t* ring_buffer_create(size_t size);
size_t ring_buffer_write(ring_buffer_t *rb, const uint8_t *data, size_t len);
size_t ring_buffer_read(ring_buffer_t *rb, uint8_t *data, size_t max_len);
size_t ring_buffer_available(ring_buffer_t *rb);

// ==================================================
// BLE SERVICE
// ==================================================

esp_err_t ble_service_init(void);
void ble_broadcast_data(const uint8_t *data, size_t len);  // Отправка данных по BLE

// ==================================================
// WIFI SERVICE
// ==================================================

esp_err_t wifi_service_init(void);
void wifi_broadcast_data(const uint8_t *data, size_t len);  // Отправка данных всем WiFi клиентам

// ==================================================
// DISPLAY MANAGER
// ==================================================

esp_err_t display_manager_init(void);
void display_task(void *pvParameters);

// ==================================================
// GPS PARSER
// ==================================================

esp_err_t parse_nmea_sentence(const char *sentence);
void gps_parse_byte(uint8_t byte);  // Парсинг побайтно (вызывается из UART task)
void gps_parser_task(void *pvParameters);  // Задача мониторинга (таймауты, логи)
