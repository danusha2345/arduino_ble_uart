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

// LVGL UI элементы (больше строк для экрана 240x280)
static lv_obj_t *label_line[12];  // 12 строк текста
static int num_lines = 12;

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
        // Попробуем BGR порядок (частая проблема с дисплеями)
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,
    };
    ESP_LOGI(TAG, "Panel config: RGB order=BGR, bits_per_pixel=16");

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
    ESP_LOGI(TAG, "Panel reset done, waiting 100ms...");
    vTaskDelay(pdMS_TO_TICKS(100)); // Задержка после reset

    ret = esp_lcd_panel_init(panel_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Panel init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "Panel initialized");

    // ВАЖНО: Установка GAP (offset) для ST7789V 240x280
    // Многие дисплеи ST7789V требуют смещения координат
    // Попробуем разные варианты gap
    ESP_LOGI(TAG, "Setting display GAP (offset)...");

    // Вариант 1: Gap для стандартного ST7789V 240x280
    // x_gap=0, y_gap=20 (некоторые дисплеи)
    esp_lcd_panel_set_gap(panel_handle, 0, 20);
    ESP_LOGI(TAG, "Gap set: x=0, y=20");

    // ========== ШАГ 5: Настройка ориентации ==========

    // ДИАГНОСТИКА: Попробуем разные варианты конфигурации
    ESP_LOGI(TAG, "Testing different display configurations...");

    // Вариант 1: БЕЗ инверсии цветов
    ESP_LOGI(TAG, "Config 1: invert=false, swap=false, mirror=false,false");
    esp_lcd_panel_invert_color(panel_handle, false);
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
    ESP_LOGI(TAG, "Configuring backlight GPIO %d...", TFT_BL_PIN);
    ret = gpio_config(&bk_gpio_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure backlight GPIO: %s", esp_err_to_name(ret));
        return ret;
    }

    gpio_set_level(TFT_BL_PIN, LCD_BK_LIGHT_ON_LEVEL);
    ESP_LOGI(TAG, "Backlight enabled on GPIO %d (level=%d)", TFT_BL_PIN, LCD_BK_LIGHT_ON_LEVEL);

    // ========== ТЕСТ: Заливка экрана разными цветами для проверки ==========
    ESP_LOGI(TAG, "Testing display with color fills...");

    size_t test_buffer_size = LCD_H_RES * 10 * 2; // 10 строк * 2 байта на пиксель
    uint16_t *test_buffer = heap_caps_malloc(test_buffer_size, MALLOC_CAP_DMA);

    if (test_buffer) {
        // Тест 1: БЕЛЫЙ экран (0xFFFF)
        ESP_LOGI(TAG, "Test 1: WHITE fill (0xFFFF)");
        for (int i = 0; i < LCD_H_RES * 10; i++) {
            test_buffer[i] = 0xFFFF; // Белый
        }
        for (int y = 0; y < LCD_V_RES; y += 10) {
            esp_lcd_panel_draw_bitmap(panel_handle, 0, y, LCD_H_RES, y + 10, test_buffer);
        }
        vTaskDelay(pdMS_TO_TICKS(1000)); // Пауза 1 сек

        // Тест 2: КРАСНЫЙ экран (0xF800)
        ESP_LOGI(TAG, "Test 2: RED fill (0xF800)");
        for (int i = 0; i < LCD_H_RES * 10; i++) {
            test_buffer[i] = 0xF800; // Красный
        }
        for (int y = 0; y < LCD_V_RES; y += 10) {
            esp_lcd_panel_draw_bitmap(panel_handle, 0, y, LCD_H_RES, y + 10, test_buffer);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));

        // Тест 3: ЗЕЛЁНЫЙ экран (0x07E0)
        ESP_LOGI(TAG, "Test 3: GREEN fill (0x07E0)");
        for (int i = 0; i < LCD_H_RES * 10; i++) {
            test_buffer[i] = 0x07E0; // Зелёный
        }
        for (int y = 0; y < LCD_V_RES; y += 10) {
            esp_lcd_panel_draw_bitmap(panel_handle, 0, y, LCD_H_RES, y + 10, test_buffer);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));

        // Тест 4: СИНИЙ экран (0x001F)
        ESP_LOGI(TAG, "Test 4: BLUE fill (0x001F)");
        for (int i = 0; i < LCD_H_RES * 10; i++) {
            test_buffer[i] = 0x001F; // Синий
        }
        for (int y = 0; y < LCD_V_RES; y += 10) {
            esp_lcd_panel_draw_bitmap(panel_handle, 0, y, LCD_H_RES, y + 10, test_buffer);
        }

        free(test_buffer);
        ESP_LOGI(TAG, "========================================");
        ESP_LOGI(TAG, "Display test complete!");
        ESP_LOGI(TAG, "If screen is STILL BLANK after WHITE/RED/GREEN/BLUE:");
        ESP_LOGI(TAG, "  1) Check wiring (MOSI=18, SCLK=19, DC=20, RST=21, BL=22)");
        ESP_LOGI(TAG, "  2) Check display type (is it really ST7789V?)");
        ESP_LOGI(TAG, "  3) Check resolution (is it really 240x280?)");
        ESP_LOGI(TAG, "========================================");
    } else {
        ESP_LOGE(TAG, "Failed to allocate test buffer");
    }

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

    // Создаем 12 строк белого текста на черном фоне
    // Высота экрана 280px / 12 строк = ~23px на строку
    int y_pos = 2;
    int line_height = 23;

    for (int i = 0; i < num_lines; i++) {
        label_line[i] = lv_label_create(screen);
        lv_label_set_text(label_line[i], "");
        lv_obj_set_style_text_color(label_line[i], lv_color_white(), 0);
        lv_obj_align(label_line[i], LV_ALIGN_TOP_LEFT, 3, y_pos);
        y_pos += line_height;
    }

    // Инициализируем тестовый экран
    lv_label_set_text(label_line[0], "===================");
    lv_label_set_text(label_line[1], "  GNSS Bridge");
    lv_label_set_text(label_line[2], "  ESP32-C6");
    lv_label_set_text(label_line[3], "===================");
    lv_label_set_text(label_line[4], "");
    lv_label_set_text(label_line[5], "Display OK!");
    lv_label_set_text(label_line[6], "");
    lv_label_set_text(label_line[7], "Waiting for GPS...");

    // ТЕСТОВЫЙ ПРЯМОУГОЛЬНИК для проверки отрисовки
    lv_obj_t *test_rect = lv_obj_create(screen);
    lv_obj_set_size(test_rect, 100, 50);
    lv_obj_align(test_rect, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
    lv_obj_set_style_bg_color(test_rect, lv_color_make(255, 0, 0), 0);  // Красный
    lv_obj_set_style_border_width(test_rect, 2, 0);
    lv_obj_set_style_border_color(test_rect, lv_color_white(), 0);
    ESP_LOGI(TAG, "Test rectangle added (red, bottom-right)");

    ESP_LOGI(TAG, "===== UI CREATED - 12 LINES + TEST RECT =====");
    ESP_LOGI(TAG, "Test screen displayed - check if visible on physical display");
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

    // ВАЖНО: Пропускаем обновление экрана, пока GPS не валиден
    // Это позволяет увидеть тестовый экран "Display OK!" для диагностики
    if (!gps_valid) {
        if (first_run) {
            ESP_LOGI(TAG, "Display update skipped - GPS not valid, showing test screen");
            first_run = false;  // Логируем только один раз
        }
        // Обновляем строку 7 со счётчиком спутников (если есть)
        if (g_gps_data.satellites > 0) {
            snprintf(buf, sizeof(buf), "Satellites: %d", g_gps_data.satellites);
            lv_label_set_text(label_line[7], buf);
        }
        return;
    }

    // GPS валиден - сбрасываем флаг и обновляем весь экран
    first_run = true;
    int line = 0;

    if (gps_valid) {
        // GPS валиден - отображаем полные данные

        // Строка 0: Заголовок + статус
        lv_label_set_text(label_line[line++], "=== GNSS Bridge ===");

        // Строка 1: Спутники и тип фикса
        const char *fix_text[] = {
            "NO FIX", "GPS", "DGPS", "PPS",
            "RTK FIXED", "RTK FLOAT"
        };
        int fix_idx = g_gps_data.fix_quality;
        if (fix_idx < 0 || fix_idx > 5) fix_idx = 0;

        snprintf(buf, sizeof(buf), "Sats:%d Fix:%s",
                 g_gps_data.satellites, fix_text[fix_idx]);
        lv_label_set_text(label_line[line++], buf);

        // Строка 2: Пустая строка (разделитель)
        lv_label_set_text(label_line[line++], "");

        // Строка 3: Широта
        snprintf(buf, sizeof(buf), "Lat: %.8f", g_gps_data.latitude);
        lv_label_set_text(label_line[line++], buf);

        // Строка 4: Долгота
        snprintf(buf, sizeof(buf), "Lon: %.8f", g_gps_data.longitude);
        lv_label_set_text(label_line[line++], buf);

        // Строка 5: Высота
        snprintf(buf, sizeof(buf), "Alt: %.2f m", g_gps_data.altitude);
        lv_label_set_text(label_line[line++], buf);

        // Строка 6: Время (если доступно)
        if (g_gps_data.time_valid) {
            char time_str[16];
            format_local_time(g_gps_data.hour, g_gps_data.minute, g_gps_data.second,
                            g_gps_data.timezone_offset_minutes, time_str, sizeof(time_str));
            snprintf(buf, sizeof(buf), "Time: %s", time_str);
            lv_label_set_text(label_line[line++], buf);
        } else {
            lv_label_set_text(label_line[line++], "Time: --:--:--");
        }

        // Строка 6: Пустая строка (разделитель)
        lv_label_set_text(label_line[line++], "");

        // Строка 7: Точность Lat/Lon (если доступна)
        if (g_gps_data.lat_accuracy < 999.0) {
            snprintf(buf, sizeof(buf), "Acc N/S: %.3f m", g_gps_data.lat_accuracy);
            lv_label_set_text(label_line[line++], buf);

            snprintf(buf, sizeof(buf), "Acc E/W: %.3f m", g_gps_data.lon_accuracy);
            lv_label_set_text(label_line[line++], buf);
        } else {
            lv_label_set_text(label_line[line++], "Acc: N/A");
            line++;  // Пропускаем еще одну строку
        }

        // Строка 9: Вертикальная точность
        if (g_gps_data.vert_accuracy < 999.0) {
            snprintf(buf, sizeof(buf), "Acc Vert: %.3f m", g_gps_data.vert_accuracy);
            lv_label_set_text(label_line[line++], buf);
        } else {
            line++;  // Пропускаем строку
        }

        // Оставшиеся строки - очищаем
        for (; line < num_lines; line++) {
            lv_label_set_text(label_line[line], "");
        }

    } else {
        // GPS не валиден - отображаем статус поиска

        // Строка 0: Заголовок
        lv_label_set_text(label_line[line++], "=== GNSS Bridge ===");

        // Строка 1: Статус
        const char *fix_text[] = {
            "NO FIX", "GPS", "DGPS", "PPS",
            "RTK FIXED", "RTK FLOAT"
        };
        int fix_idx = g_gps_data.fix_quality;
        if (fix_idx < 0 || fix_idx > 5) fix_idx = 0;

        snprintf(buf, sizeof(buf), "Status: %s", fix_text[fix_idx]);
        lv_label_set_text(label_line[line++], buf);

        // Строка 2: Пустая
        lv_label_set_text(label_line[line++], "");

        // Строка 3: Количество спутников или поиск
        if (g_gps_data.satellites > 0) {
            snprintf(buf, sizeof(buf), "Satellites: %d", g_gps_data.satellites);
            lv_label_set_text(label_line[line++], buf);
        } else {
            lv_label_set_text(label_line[line++], "Searching GPS...");
        }

        // Оставшиеся строки - очищаем
        for (; line < num_lines; line++) {
            lv_label_set_text(label_line[line], "");
        }
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
