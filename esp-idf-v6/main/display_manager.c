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
#include "lvgl.h"

#include "config.h"
#include "common.h"
#include "Vernon_ST7789T/Vernon_ST7789T.h"  // Кастомный драйвер ST7789T

static const char *TAG = "Display";

// ==================================================
// ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ
// ==================================================

static esp_lcd_panel_handle_t panel_handle = NULL;
static lv_disp_t *disp = NULL;

// LVGL UI элементы для экрана 240x280
static lv_obj_t *label_line[9];  // 9 строк текста (без пустых строк)
static int num_lines = 9;

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
// SPI DMA CALLBACK
// ==================================================

/**
 * @brief Callback вызывается когда SPI DMA завершает передачу данных
 * Это правильный момент для уведомления LVGL что отрисовка завершена
 */
static bool lcd_color_trans_done_cb(esp_lcd_panel_io_handle_t panel_io,
                                     esp_lcd_panel_io_event_data_t *edata,
                                     void *user_ctx) {
    // user_ctx содержит адрес указателя на display (&disp)
    lv_display_t **disp_ptr = (lv_display_t **)user_ctx;
    if (disp_ptr && *disp_ptr) {
        lv_display_flush_ready(*disp_ptr);
    }
    return false;
}

// ==================================================
// LVGL FLUSH CALLBACK
// ==================================================

/**
 * @brief Callback для отрисовки буфера LVGL на дисплей
 * НЕ вызываем lv_display_flush_ready здесь - это делает lcd_color_trans_done_cb
 */
static void lvgl_flush_cb(lv_display_t *display, const lv_area_t *area, uint8_t *px_map) {
    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;

    // Отправляем данные на дисплей (асинхронно через SPI DMA)
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1,
                              offsetx2 + 1, offsety2 + 1, px_map);

    // НЕ вызываем lv_display_flush_ready здесь!
    // Это сделает lcd_color_trans_done_cb когда DMA закончит передачу
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

    ret = spi_bus_initialize(LCD_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "SPI bus initialized on LCD_SPI_HOST");

    // ========== ШАГ 2: Panel IO с callback для DMA ==========
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = TFT_DC_PIN,
        .cs_gpio_num = TFT_CS_PIN,  // CS pin обязателен для ST7789T
        .pclk_hz = LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = LCD_CMD_BITS,
        .lcd_param_bits = LCD_PARAM_BITS,
        .spi_mode = 3,  // SPI mode 3 для этого дисплея
        .trans_queue_depth = 10,
        .on_color_trans_done = lcd_color_trans_done_cb,  // Callback когда DMA завершён
        .user_ctx = &disp,  // Передаём адрес указателя на display
    };

    ret = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_SPI_HOST, &io_config, &io_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create panel IO: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "Panel IO created");

    // ========== ШАГ 3: Настройка панели ST7789T (кастомный драйвер) ==========
    esp_lcd_panel_dev_st7789t_config_t panel_config = {
        .reset_gpio_num = TFT_RST_PIN,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,  // BGR порядок
        .bits_per_pixel = 16,
    };
    ESP_LOGI(TAG, "Panel config: ST7789T custom driver, RGB order=BGR, bits_per_pixel=16");

    // Создаём панель ST7789T (кастомный драйвер Vernon)
    ret = esp_lcd_new_panel_st7789t(io_handle, &panel_config, &panel_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create ST7789 panel: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "ST7789 panel created");

    // ========== ШАГ 4: Инициализация ST7789T панели ==========
    ESP_LOGI(TAG, "Resetting panel...");
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));

    ESP_LOGI(TAG, "Initializing panel...");
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

    // Gap offset для ST7789V 240x280 - сдвиг по Y на 20 пикселей
    ESP_LOGI(TAG, "Setting gap offset (0, 20)");
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel_handle, 0, 20));

    // Зеркалирование как в рабочем примере
    ESP_LOGI(TAG, "Setting mirror");
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, true, false));

    // Включение дисплея
    ESP_LOGI(TAG, "Turning display ON");
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    // ========== ШАГ 5: Подсветка ==========
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << TFT_BL_PIN
    };
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));
    gpio_set_level(TFT_BL_PIN, LCD_BK_LIGHT_ON_LEVEL);
    ESP_LOGI(TAG, "Backlight ON");

    ESP_LOGI(TAG, "===== SPI DISPLAY INITIALIZED SUCCESSFULLY =====");
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
 * @brief Создание пользовательского интерфейса для экрана 240x280
 */
static void create_ui(void) {
    ESP_LOGI(TAG, "Creating UI for 240x280 display...");

    // Основной экран с черным фоном
    lv_obj_t *screen = lv_scr_act();
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);

    // Создаем 9 строк текста с цветовым кодированием
    // Высота экрана 280px / 9 строк = ~31px на строку
    int y_pos = 5;  // Отступ сверху
    int line_height = 30;  // Межстрочный интервал 31px
    int x_offset = 15;  // Отступ слева 15 пикселей

    // Цвета для каждой строки
    lv_color_t line_colors[9] = {
                lv_color_white(),                           // 0: Заголовок
                lv_palette_main(LV_PALETTE_YELLOW),        // 1: Спутники/Fix
                lv_color_white(),         // 2: Широта
                lv_color_white(),         // 3: Долгота
                lv_palette_main(LV_PALETTE_LIGHT_BLUE),    // 4: Высота
                lv_color_white(),                           // 5: Время
                lv_palette_main(LV_PALETTE_ORANGE),        // 6: Точность N/S
                lv_palette_main(LV_PALETTE_ORANGE),        // 7: Точность E/W
                lv_palette_main(LV_PALETTE_ORANGE)         // 8: Точность Vert
    };

    for (int i = 0; i < num_lines; i++) {
        label_line[i] = lv_label_create(screen);
        lv_label_set_text(label_line[i], "");
        lv_obj_set_style_text_color(label_line[i], line_colors[i], 0);
        // Используем шрифт Montserrat 14 (стандартный, гарантированно работает)
        lv_obj_set_style_text_font(label_line[i], &lv_font_montserrat_20, 0);
        lv_obj_align(label_line[i], LV_ALIGN_TOP_LEFT, x_offset, y_pos);
        y_pos += line_height;
    }

    // Инициализируем начальный экран
    lv_label_set_text(label_line[0], "=== GNSS Bridge ===");
    lv_label_set_text(label_line[1], "ESP32-C6");
    lv_label_set_text(label_line[2], "Display OK!");
    lv_label_set_text(label_line[3], "");
    lv_label_set_text(label_line[4], "Waiting for GPS...");

    ESP_LOGI(TAG, "===== UI CREATED - 9 COLORED TEXT LINES =====");
}

// ==================================================
// ОБНОВЛЕНИЕ ДАННЫХ
// ==================================================

/**
 * @brief Форматирование времени с учетом часового пояса
 * @param hour Часы UTC
 * @param minute Минуты UTC
 * @param second Секунды UTC
 * @param timezone_offset_minutes Смещение часового пояса в минутах
 * @param buf Буфер для записи результата
 * @param buf_size Размер буфера
 */
static void format_local_time(int hour, int minute, int second, int timezone_offset_minutes,
                              char *buf, size_t buf_size) {
    // Конвертируем время в секунды с начала дня
    long sec = hour * 3600L + minute * 60L + second;

    // Добавляем смещение часового пояса
    sec += (long)timezone_offset_minutes * 60L;

    // Нормализуем в пределах 0-86399 (секунды в дне)
    sec %= 86400L;
    if (sec < 0) sec += 86400L;

    // Конвертируем обратно в часы/минуты/секунды
    int hh = (int)(sec / 3600L);
    int mm = (int)((sec % 3600L) / 60L);
    int ss = (int)(sec % 60L);

    snprintf(buf, buf_size, "%02d:%02d:%02d", hh, mm, ss);
}

/**
 * @brief Обновление данных GPS на дисплее (полная информация для 240x280)
 */
void display_update_gps_data(void) {
    static char buf[48];
    static bool first_run = true;

    if (!label_line[0]) {
        return;  // UI еще не создан
    }

    // Проверяем валидность GPS данных
    uint32_t now = esp_timer_get_time() / 1000;
    bool gps_valid = g_gps_data.valid && (now - g_gps_data.last_update) < GPS_TIMEOUT_MS;

    // ВСЕГДА показываем все поля - используем прочерки когда GPS не валиден
    int line = 0;

    // Строка 0: Заголовок (белый)
    lv_label_set_text(label_line[line++], "=== GNSS Bridge ===");

    // Строка 1: Спутники и тип фикса (жёлтый)
    const char *fix_text[] = {
        "NO FIX", "GPS", "DGPS", "PPS",
        "RTK FIXED", "RTK FLOAT"
    };
    int fix_idx = g_gps_data.fix_quality;
    if (fix_idx < 0 || fix_idx > 5) fix_idx = 0;

    if (gps_valid) {
        snprintf(buf, sizeof(buf), "Sats:%d   Fix:%s",
                 g_gps_data.satellites, fix_text[fix_idx]);
    } else {
        snprintf(buf, sizeof(buf), "Sats:---   Fix:---");
    }
    lv_label_set_text(label_line[line++], buf);

    // Строка 2: Широта (зелёный)
    if (gps_valid) {
        snprintf(buf, sizeof(buf), "Lat: %.8f", g_gps_data.latitude);
    } else {
        snprintf(buf, sizeof(buf), "Lat: ---");
    }
    lv_label_set_text(label_line[line++], buf);

    // Строка 3: Долгота (зелёный)
    if (gps_valid) {
        snprintf(buf, sizeof(buf), "Lon: %.8f", g_gps_data.longitude);
    } else {
        snprintf(buf, sizeof(buf), "Lon: ---");
    }
    lv_label_set_text(label_line[line++], buf);

    // Строка 4: Высота (голубой)
    if (gps_valid) {
        snprintf(buf, sizeof(buf), "Alt: %.2f m", g_gps_data.altitude);
    } else {
        snprintf(buf, sizeof(buf), "Alt: --- m");
    }
    lv_label_set_text(label_line[line++], buf);

    // Строка 5: Время (белый)
    if (gps_valid && g_gps_data.time_valid) {
        char time_str[16];
        format_local_time(g_gps_data.hour, g_gps_data.minute, g_gps_data.second,
                        g_gps_data.timezone_offset_minutes, time_str, sizeof(time_str));
        snprintf(buf, sizeof(buf), "Time: %s", time_str);
    } else {
        snprintf(buf, sizeof(buf), "Time: --:--:--");
    }
    lv_label_set_text(label_line[line++], buf);

    // Строка 6: Точность Lat/Lon (N/S) (оранжевый)
    if (gps_valid && g_gps_data.lat_accuracy < 999.0) {
        snprintf(buf, sizeof(buf), "Acc N/S: %.3f m", g_gps_data.lat_accuracy);
    } else {
        snprintf(buf, sizeof(buf), "Acc N/S: --- m");
    }
    lv_label_set_text(label_line[line++], buf);

    // Строка 7: Точность Lat/Lon (E/W) (оранжевый)
    if (gps_valid && g_gps_data.lon_accuracy < 999.0) {
        snprintf(buf, sizeof(buf), "Acc E/W: %.3f m", g_gps_data.lon_accuracy);
    } else {
        snprintf(buf, sizeof(buf), "Acc E/W: --- m");
    }
    lv_label_set_text(label_line[line++], buf);

    // Строка 8: Вертикальная точность (оранжевый)
    if (gps_valid && g_gps_data.vert_accuracy < 999.0) {
        snprintf(buf, sizeof(buf), "Acc Vert: %.3f m", g_gps_data.vert_accuracy);
    } else {
        snprintf(buf, sizeof(buf), "Acc Vert: --- m");
    }
    lv_label_set_text(label_line[line++], buf);

    // Логирование для первого запуска
    if (first_run) {
        ESP_LOGI(TAG, "Display layout: 9 colored lines, showing %s",
                 gps_valid ? "GPS data" : "placeholders (---)");
        first_run = false;
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
