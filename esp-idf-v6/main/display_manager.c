/**
 * @file display_manager.c
 * @brief Управление TFT дисплеем ST7789V (240x280) через LVGL v9
 */

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_st7789.h"
#include "lvgl.h"

#include "config.h"
#include "common.h"

static const char *TAG = "Display";

// ==================================================
// ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ
// ==================================================

static esp_lcd_panel_handle_t panel_handle = NULL;
static lv_disp_t *disp = NULL;

// LVGL UI элементы
static lv_obj_t *label_lat = NULL;
static lv_obj_t *label_lon = NULL;
static lv_obj_t *label_alt = NULL;
static lv_obj_t *label_sat = NULL;
static lv_obj_t *label_fix = NULL;
static lv_obj_t *label_status = NULL;

// ==================================================
// LVGL TICK CALLBACK
// ==================================================

/**
 * @brief LVGL tick callback - получает текущее время в миллисекундах
 */
static uint32_t lvgl_get_tick_cb(void) {
    return (uint32_t)(esp_timer_get_time() / 1000);
}

// ==================================================
// LVGL FLUSH CALLBACK
// ==================================================

/**
 * @brief Callback для отрисовки буфера LVGL на дисплей
 */
static void lvgl_flush_cb(lv_display_t *display, const lv_area_t *area, uint8_t *px_map) {
    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;

    // Отправляем данные на дисплей
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1,
                              offsetx2 + 1, offsety2 + 1, px_map);

    // Сообщаем LVGL что отрисовка завершена
    lv_display_flush_ready(display);
}

// ==================================================
// ИНИЦИАЛИЗАЦИЯ SPI ДИСПЛЕЯ
// ==================================================

/**
 * @brief Инициализация SPI шины и TFT дисплея ST7789V
 */
static esp_err_t init_spi_display(void) {
    ESP_LOGI(TAG, "Initializing SPI display ST7789V 240x280...");
    ESP_LOGI(TAG, "Pins: MOSI=%d, SCLK=%d, DC=%d, RST=%d, BL=%d",
             TFT_MOSI_PIN, TFT_SCLK_PIN, TFT_DC_PIN, TFT_RST_PIN, TFT_BL_PIN);

    esp_err_t ret;

    // ========== ШАГ 1: Инициализация SPI шины ==========
    spi_bus_config_t buscfg = {
        .sclk_io_num = TFT_SCLK_PIN,
        .mosi_io_num = TFT_MOSI_PIN,
        .miso_io_num = -1,  // Не используется для TFT
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LVGL_BUFFER_SIZE * sizeof(uint16_t),
    };

    ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "SPI bus initialized on SPI2_HOST");

    // ========== ШАГ 2: Настройка Panel IO (DC pin) ==========
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = TFT_DC_PIN,
        .cs_gpio_num = TFT_CS_PIN,
        .pclk_hz = LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = LCD_CMD_BITS,
        .lcd_param_bits = LCD_PARAM_BITS,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };

    ret = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_config, &io_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create panel IO: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "Panel IO created");

    // ========== ШАГ 3: Настройка панели ST7789 ==========
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = TFT_RST_PIN,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };

    ret = esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create ST7789 panel: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "ST7789 panel created");

    // ========== ШАГ 4: Инициализация панели ==========
    ret = esp_lcd_panel_reset(panel_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Panel reset failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_lcd_panel_init(panel_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Panel init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "Panel initialized");

    // ========== ШАГ 5: Настройка ориентации ==========
    // Инвертируем цвета для правильного отображения
    esp_lcd_panel_invert_color(panel_handle, true);

    // Устанавливаем ориентацию для 240x280 (вертикальная, высота больше ширины)
    // Для ST7789V с разрешением 240x280 используем стандартную ориентацию
    esp_lcd_panel_swap_xy(panel_handle, false);
    esp_lcd_panel_mirror(panel_handle, false, false);

    // Включаем дисплей
    esp_lcd_panel_disp_on_off(panel_handle, true);
    ESP_LOGI(TAG, "Display turned on");

    // ========== ШАГ 6: Включение подсветки ==========
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << TFT_BL_PIN
    };
    gpio_config(&bk_gpio_config);
    gpio_set_level(TFT_BL_PIN, LCD_BK_LIGHT_ON_LEVEL);
    ESP_LOGI(TAG, "Backlight enabled on GPIO %d", TFT_BL_PIN);

    ESP_LOGI(TAG, "SPI display initialized successfully");
    return ESP_OK;
}

// ==================================================
// ИНИЦИАЛИЗАЦИЯ LVGL
// ==================================================

/**
 * @brief Инициализация библиотеки LVGL v9
 */
static esp_err_t init_lvgl(void) {
    ESP_LOGI(TAG, "Initializing LVGL v9...");

    // Размеры дисплея ST7789V 240x280
    const int display_width = 240;
    const int display_height = 280;

    // ========== ШАГ 1: Инициализация LVGL ==========
    lv_init();
    ESP_LOGI(TAG, "LVGL initialized");

    // ========== ШАГ 2: Настройка tick callback ==========
    lv_tick_set_cb(lvgl_get_tick_cb);

    // ========== ШАГ 3: Создание дисплея ==========
    disp = lv_display_create(display_width, display_height);
    if (!disp) {
        ESP_LOGE(TAG, "Failed to create LVGL display");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "LVGL display created: %dx%d", display_width, display_height);

    // ========== ШАГ 4: Выделение буферов отрисовки ==========
    // Буфер на 20 строк для 240 пикселей ширины
    const size_t buffer_pixels = display_width * 20;
    size_t buffer_size = buffer_pixels * sizeof(lv_color_t);
    void *buf1 = heap_caps_malloc(buffer_size, MALLOC_CAP_DMA);
    void *buf2 = heap_caps_malloc(buffer_size, MALLOC_CAP_DMA);

    if (!buf1 || !buf2) {
        ESP_LOGE(TAG, "Failed to allocate LVGL buffers");
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "LVGL buffers allocated: %d bytes each (%d pixels)", buffer_size, buffer_pixels);

    lv_display_set_buffers(disp, buf1, buf2, buffer_size, LV_DISPLAY_RENDER_MODE_PARTIAL);

    // ========== ШАГ 5: Установка flush callback ==========
    lv_display_set_flush_cb(disp, lvgl_flush_cb);

    ESP_LOGI(TAG, "LVGL initialized successfully");
    return ESP_OK;
}

// ==================================================
// СОЗДАНИЕ UI
// ==================================================

/**
 * @brief Создание пользовательского интерфейса
 */
static void create_ui(void) {
    ESP_LOGI(TAG, "Creating UI...");

    // Основной экран с черным фоном
    lv_obj_t *screen = lv_scr_act();
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);

    // Заголовок
    lv_obj_t *label_title = lv_label_create(screen);
    lv_label_set_text(label_title, "GNSS Bridge");
    lv_obj_set_style_text_color(label_title, lv_color_white(), 0);
    lv_obj_align(label_title, LV_ALIGN_TOP_MID, 0, 5);

    // Статус подключения
    label_status = lv_label_create(screen);
    lv_label_set_text(label_status, "Status: Waiting...");
    lv_obj_set_style_text_color(label_status, lv_color_make(255, 255, 0), 0);
    lv_obj_align(label_status, LV_ALIGN_TOP_LEFT, 5, 25);

    // Координаты и данные GPS
    label_lat = lv_label_create(screen);
    lv_label_set_text(label_lat, "Lat: --");
    lv_obj_set_style_text_color(label_lat, lv_color_white(), 0);
    lv_obj_align(label_lat, LV_ALIGN_TOP_LEFT, 5, 50);

    label_lon = lv_label_create(screen);
    lv_label_set_text(label_lon, "Lon: --");
    lv_obj_set_style_text_color(label_lon, lv_color_white(), 0);
    lv_obj_align(label_lon, LV_ALIGN_TOP_LEFT, 5, 70);

    label_alt = lv_label_create(screen);
    lv_label_set_text(label_alt, "Alt: --");
    lv_obj_set_style_text_color(label_alt, lv_color_white(), 0);
    lv_obj_align(label_alt, LV_ALIGN_TOP_LEFT, 5, 90);

    // Спутники
    label_sat = lv_label_create(screen);
    lv_label_set_text(label_sat, "Satellites: 0");
    lv_obj_set_style_text_color(label_sat, lv_color_make(0, 255, 255), 0);
    lv_obj_align(label_sat, LV_ALIGN_TOP_LEFT, 5, 115);

    // Тип фиксации
    label_fix = lv_label_create(screen);
    lv_label_set_text(label_fix, "Fix: NO FIX");
    lv_obj_set_style_text_color(label_fix, lv_color_make(255, 100, 100), 0);
    lv_obj_align(label_fix, LV_ALIGN_TOP_LEFT, 5, 135);

    ESP_LOGI(TAG, "UI created successfully");
}

// ==================================================
// ОБНОВЛЕНИЕ ДАННЫХ
// ==================================================

/**
 * @brief Обновление данных GPS на дисплее
 */
void display_update_gps_data(void) {
    static char buf[64];

    if (!label_lat || !label_lon || !label_alt || !label_sat || !label_fix || !label_status) {
        return;  // UI еще не создан
    }

    // Обновляем статус
    uint32_t now = esp_timer_get_time() / 1000;
    if (g_gps_data.valid && (now - g_gps_data.last_update) < GPS_TIMEOUT_MS) {
        lv_label_set_text(label_status, "Status: GPS OK");
        lv_obj_set_style_text_color(label_status, lv_color_make(0, 255, 0), 0);
    } else {
        lv_label_set_text(label_status, "Status: No GPS");
        lv_obj_set_style_text_color(label_status, lv_color_make(255, 0, 0), 0);
    }

    // Обновляем координаты
    if (g_gps_data.valid) {
        snprintf(buf, sizeof(buf), "Lat: %.7f", g_gps_data.latitude);
        lv_label_set_text(label_lat, buf);

        snprintf(buf, sizeof(buf), "Lon: %.7f", g_gps_data.longitude);
        lv_label_set_text(label_lon, buf);

        snprintf(buf, sizeof(buf), "Alt: %.2f m", g_gps_data.altitude);
        lv_label_set_text(label_alt, buf);
    } else {
        lv_label_set_text(label_lat, "Lat: --");
        lv_label_set_text(label_lon, "Lon: --");
        lv_label_set_text(label_alt, "Alt: --");
    }

    // Обновляем спутники
    snprintf(buf, sizeof(buf), "Satellites: %d", g_gps_data.satellites);
    lv_label_set_text(label_sat, buf);

    // Обновляем тип фиксации
    const char *fix_text[] = {
        "Fix: NO FIX",      // 0
        "Fix: GPS",         // 1
        "Fix: DGPS",        // 2
        "Fix: PPS",         // 3
        "Fix: RTK FIXED",   // 4
        "Fix: RTK FLOAT"    // 5
    };

    int fix_idx = g_gps_data.fix_quality;
    if (fix_idx < 0 || fix_idx > 5) fix_idx = 0;

    lv_label_set_text(label_fix, fix_text[fix_idx]);

    // Цвет в зависимости от качества фиксации
    if (fix_idx >= 4) {
        lv_obj_set_style_text_color(label_fix, lv_color_make(0, 255, 0), 0);  // Зеленый для RTK
    } else if (fix_idx >= 1) {
        lv_obj_set_style_text_color(label_fix, lv_color_make(255, 255, 0), 0);  // Желтый для GPS
    } else {
        lv_obj_set_style_text_color(label_fix, lv_color_make(255, 0, 0), 0);  // Красный для NO FIX
    }
}

// ==================================================
// ИНИЦИАЛИЗАЦИЯ DISPLAY MANAGER
// ==================================================

/**
 * @brief Инициализация дисплея
 */
esp_err_t display_manager_init(void) {
    ESP_LOGI(TAG, "Initializing display manager...");

    // Инициализация SPI дисплея
    esp_err_t ret = init_spi_display();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI display");
        return ret;
    }

    // Инициализация LVGL
    ret = init_lvgl();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize LVGL");
        return ret;
    }

    // Создание UI
    create_ui();

    ESP_LOGI(TAG, "Display manager initialized successfully");
    return ESP_OK;
}

// ==================================================
// ЗАДАЧА ДИСПЛЕЯ
// ==================================================

/**
 * @brief Задача обработки дисплея
 */
void display_task(void *pvParameters) {
    ESP_LOGI(TAG, "Display task started on core %d", xPortGetCoreID());

    const TickType_t delay = pdMS_TO_TICKS(10);  // 10ms = 100Hz

    while (1) {
        // Обработка событий LVGL
        uint32_t time_till_next = lv_task_handler();

        // Обновление данных GPS на экране каждые 500ms
        static uint32_t last_update = 0;
        uint32_t now = esp_timer_get_time() / 1000;
        if (now - last_update >= 500) {
            display_update_gps_data();
            last_update = now;
        }

        // Ожидание перед следующей итерацией
        if (time_till_next > 10) {
            vTaskDelay(delay);
        } else {
            vTaskDelay(1);
        }
    }

    vTaskDelete(NULL);
}
