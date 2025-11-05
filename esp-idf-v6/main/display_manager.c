/**
 * @file display_manager.c
 * @brief Управление дисплеями через LVGL
 */

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
// #include "driver/i2c.h" // Устарело в ESP-IDF v6
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "lvgl.h"

#include "config.h"
#include "common.h"

static const char *TAG = "Display";

/**
 * @brief LVGL flush callback для SPI дисплея
 * TODO: Обновить на API LVGL v9 (lv_disp_drv_t больше не существует)
 */
/* ЗАКОММЕНТИРОВАНО - lv_disp_drv_t не существует в LVGL v9
static void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map) {
    // TODO: Реализовать отрисовку на дисплей

    lv_disp_flush_ready(drv);
}
*/

/**
 * @brief Инициализация I2C для OLED
 * TODO: Обновить на новый I2C API (driver/i2c_master.h) в ESP-IDF v6
 */
/* ЗАКОММЕНТИРОВАНО - устаревший API driver/i2c.h
static esp_err_t init_i2c(void) {
#if defined(TARGET_ESP32_C3) || defined(TARGET_ESP32_S3)
    ESP_LOGI(TAG, "Initializing I2C on SDA:%d SCL:%d", I2C_SDA_PIN, I2C_SCL_PIN);

    i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA_PIN,
        .scl_io_num = I2C_SCL_PIN,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
    };

    ESP_ERROR_CHECK(i2c_param_config(I2C_NUM_0, &i2c_conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0));

    ESP_LOGI(TAG, "I2C initialized");
#endif
    return ESP_OK;
}
*/

/**
 * @brief Инициализация SPI дисплея
 */
static esp_err_t init_spi_display(void) {
    ESP_LOGI(TAG, "Initializing SPI display...");

    // TODO: Реализовать инициализацию SPI дисплея
    // - Инициализация SPI шины
    // - Создание panel IO
    // - Создание панели ST7789
    // - Настройка ориентации
    // - Включение подсветки

    ESP_LOGI(TAG, "SPI display initialized (stub)");
    return ESP_OK;
}

/**
 * @brief Инициализация LVGL
 */
esp_err_t display_manager_init(void) {
    ESP_LOGI(TAG, "Initializing display manager...");

    // Инициализация I2C для OLED (закомментировано)
    // init_i2c();

    // Инициализация SPI для TFT
    init_spi_display();

    // TODO: Инициализация LVGL
    // - lv_init()
    // - Создание буфера отрисовки
    // - Регистрация драйвера дисплея
    // - Создание UI элементов

    ESP_LOGI(TAG, "Display manager initialized (stub)");
    return ESP_OK;
}

/**
 * @brief Обновление данных на дисплее
 */
void display_update_gps_data(void) {
    // TODO: Обновить LVGL labels с GPS данными
}

/**
 * @brief Задача обработки дисплея
 */
void display_task(void *pvParameters) {
    ESP_LOGI(TAG, "Display task started on core %d", xPortGetCoreID());

    while (1) {
        // TODO: Вызов lv_task_handler()
        // TODO: Обновление UI с GPS данными

        vTaskDelay(pdMS_TO_TICKS(10));
    }

    vTaskDelete(NULL);
}
