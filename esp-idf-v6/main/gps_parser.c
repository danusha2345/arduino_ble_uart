/**
 * @file gps_parser.c
 * @brief Парсер NMEA сообщений от GPS/GNSS модулей
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "esp_log.h"

#include "config.h"

static const char *TAG = "GPS";

/**
 * @brief Парсинг NMEA строки
 */
esp_err_t parse_nmea_sentence(const char *sentence) {
    // TODO: Реализовать парсер NMEA
    // - Проверка контрольной суммы
    // - Парсинг GNS (координаты, высота, качество фикса)
    // - Парсинг GST (точность координат)
    // - Парсинг GSA (используемые спутники)
    // - Парсинг GSV (видимые спутники)
    // - Парсинг GGA (время UTC, RTK статус)

    return ESP_OK;
}

/**
 * @brief Задача парсинга GPS данных
 */
void gps_parser_task(void *pvParameters) {
    ESP_LOGI(TAG, "GPS parser task started on core %d", xPortGetCoreID());

    char nmea_buffer[256];
    int buffer_pos = 0;

    // TODO: Основной цикл парсинга
    // - Чтение данных из TX буфера
    // - Поиск NMEA строк ($...* или $...\r\n)
    // - Парсинг найденных строк
    // - Обновление глобальных структур данных

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    vTaskDelete(NULL);
}
