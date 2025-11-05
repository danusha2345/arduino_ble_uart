# План реализации оставшихся модулей

## Статус проекта

### ✅ Реализовано (100%)
- **main.c** - UART драйвер, ring buffers, инициализация
- **ble_service.c** - NimBLE Nordic UART Service (277 строк)
- **wifi_service.c** - WiFi AP + TCP сервер (230 строк)
- **config.h** - конфигурация для C3/C6/S3

### 🚧 К реализации
- **gps_parser.c** - NMEA парсер (заглушка 52 строки → ~400 строк)
- **display_manager.c** - LVGL UI (заглушка 114 строк → ~300 строк)

---

## 1. GPS Parser (gps_parser.c)

### 1.1 Структуры данных

```c
// Информация о спутниках одной системы
typedef struct {
    int visible;              // Видимые (из GSV)
    int used;                 // Используемые в решении (из GSA)
    uint32_t last_update;     // Время последнего обновления (esp_timer_get_time())
} sat_info_t;

// Данные GPS/GNSS
typedef struct {
    double latitude;          // Широта в градусах
    double longitude;         // Долгота в градусах
    double altitude;          // Высота в метрах
    double lat_accuracy;      // Точность широты (м)
    double lon_accuracy;      // Точность долготы (м)
    double vertical_accuracy; // Точность высоты (м)
    int satellites;           // Общее кол-во спутников
    int fix_quality;          // 0=NO FIX, 1=GPS, 2=DGPS, 4=RTK-FIX, 5=RTK-FLT
    bool valid;               // Валидность координат
    uint32_t last_update;     // Время последнего обновления
    uint32_t last_gst_update; // Последнее обновление GST
} gps_data_t;

// Спутники по системам
typedef struct {
    sat_info_t gps;
    sat_info_t glonass;
    sat_info_t galileo;
    sat_info_t beidou;
    sat_info_t qzss;
} sat_data_t;

// Глобальные переменные
extern gps_data_t g_gps_data;
extern sat_data_t g_sat_data;
```

### 1.2 Вспомогательные функции

#### splitFields() - разделение NMEA строки
```c
/**
 * Разделяет NMEA строку на поля
 * @param nmea Буфер NMEA строки (модифицируется!)
 * @param fields Массив указателей на поля
 * @param max_fields Максимальное количество полей
 * @return Количество полей
 */
static int split_fields(char *nmea, char *fields[], int max_fields);
```

**Логика** (из main.cpp:720-753):
- Проходит по строке, заменяя ',' и '*' на '\0'
- Сохраняет указатели на начало каждого поля
- Останавливается при '*' (начало checksum)

#### convertToDecimalDegrees() - конвертация координат
```c
/**
 * Конвертирует DDMM.MMMM в десятичные градусы
 * @param ddmm Координата в формате DDMM.MMMM
 * @return Градусы в десятичном формате
 */
static double convert_to_decimal_degrees(double ddmm);
```

**Формула** (из main.cpp:1056-1060):
```c
int degrees = (int)(ddmm / 100);
double minutes = ddmm - (degrees * 100);
return degrees + minutes / 60.0;
```

### 1.3 NMEA парсеры

#### parseGSV() - видимые спутники
**Источник**: main.cpp:756-784

**Логика**:
- Поле 3 = общее количество видимых спутников
- Определить систему по префиксу:
  - `$GPGSV` → satData.gps.visible
  - `$GLGSV` → satData.glonass.visible
  - `$GAGSV` → satData.galileo.visible
  - `$GBGSV` → satData.beidou.visible
  - `$GQGSV` → satData.qzss.visible

#### parseGSA() - используемые спутники
**Источник**: main.cpp:787-846

**Логика**:
- Поля 3-14 = PRN используемых спутников
- Подсчитать непустые поля
- Для `$GNGSA`: поле 18 содержит System ID (1=GPS, 2=GLO, 3=GAL, 4=BDS, 5=QZSS)
- Для остальных префиксов ($GPGSA, $GLGSA и т.д.) - напрямую

#### parseGST() - точность координат
**Источник**: main.cpp:849-877

**Поля**:
- Поле 6 → gpsData.latAccuracy (широта, м)
- Поле 7 → gpsData.lonAccuracy (долгота, м)
- Поле 8 → gpsData.verticalAccuracy (высота, м)

**Валидация**: значение > 0 и < 100 м

#### parseGNS() - координаты и фикс
**Источник**: main.cpp:880-963

**Поля**:
- Поле 2,3 → latitude (N/S)
- Поле 4,5 → longitude (E/W)
- Поле 6 → mode indicators (до 6 символов):
  - 'R' = RTK fixed (fixQuality=4)
  - 'F' = RTK float (fixQuality=5)
  - 'D' = DGPS (fixQuality=2)
  - 'A' = Autonomous (fixQuality=1)
  - 'N' = NO FIX (fixQuality=0)
- Поле 7 → satellites (ТОЛЬКО для $GNGNS!)
- Поле 9 → altitude (м)

**Приоритет**: выбирать лучший mode indicator из всех систем

#### parseGGA() - RTK статус (ПРИОРИТЕТ!)
**Источник**: main.cpp:966-1011

**ВАЖНО**: GGA имеет точное поле quality, ПРИОРИТЕТНЕЕ чем GNS!

**Парсить ТОЛЬКО $GNGGA** (комбинированное)

**Поле 6 - Quality indicator**:
- 0 = NO FIX
- 1 = GPS
- 2 = DGPS
- 4 = RTK Fixed
- 5 = RTK Float
- 7 = Manual
- 8 = Simulator

#### parseNMEA() - главный диспетчер
**Источник**: main.cpp:1014-1025

```c
void parse_nmea_sentence(const char *sentence) {
    // Проверка префикса $GP/$GA/$GL/$GB/$GQ/$GN
    if (strstr(sentence, "GSV")) parseGSV(sentence);
    else if (strstr(sentence, "GSA")) parseGSA(sentence);
    else if (strstr(sentence, "GST")) parseGST(sentence);
    else if (strstr(sentence, "GGA")) parseGGA(sentence);  // GGA ПЕРВЫМ!
    else if (strstr(sentence, "GNS")) parseGNS(sentence);
}
```

### 1.4 Таймауты
**Источник**: main.cpp:1028-1053

```c
void check_satellite_timeouts(void) {
    uint32_t now = esp_timer_get_time() / 1000; // мс
    const uint32_t timeout = 10000; // 10 секунд

    if (now - g_sat_data.gps.last_update > timeout) {
        g_sat_data.gps.visible = 0;
        g_sat_data.gps.used = 0;
    }
    // ... для остальных систем
}
```

### 1.5 GPS Parser Task

```c
void gps_parser_task(void *pvParameters) {
    ESP_LOGI(TAG, "GPS parser task started on core %d", xPortGetCoreID());

    char nmea_buffer[256];
    int buffer_pos = 0;
    uint8_t rx_byte;

    while (1) {
        // Читать из TX буфера (GPS → BLE/WiFi)
        size_t avail = ring_buffer_available(g_ble_tx_buffer);
        if (avail > 0) {
            ring_buffer_read(g_ble_tx_buffer, &rx_byte, 1);

            if (rx_byte == '$') {
                buffer_pos = 0;
            }

            if (buffer_pos < sizeof(nmea_buffer) - 1) {
                nmea_buffer[buffer_pos++] = rx_byte;

                if (rx_byte == '\n') {
                    nmea_buffer[buffer_pos] = '\0';
                    parse_nmea_sentence(nmea_buffer);
                    buffer_pos = 0;
                }
            }
        }

        // Проверка таймаутов каждые 5 секунд
        static uint32_t last_timeout_check = 0;
        uint32_t now = esp_timer_get_time() / 1000000;
        if (now - last_timeout_check > 5) {
            check_satellite_timeouts();
            last_timeout_check = now;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

---

## 2. Display Manager (display_manager.c)

### 2.1 Инициализация SPI + ST7789V

#### Шаг 1: Инициализация SPI bus
```c
spi_bus_config_t buscfg = {
    .mosi_io_num = SPI_MOSI_PIN,
    .sclk_io_num = SPI_SCLK_PIN,
    .miso_io_num = -1,
    .quadwp_io_num = -1,
    .quadhd_io_num = -1,
    .max_transfer_sz = TFT_WIDTH * TFT_HEIGHT * 2,
};
esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_SPI_HOST, &buscfg, &io_handle);
```

#### Шаг 2: Создание panel IO
```c
esp_lcd_panel_io_spi_config_t io_config = {
    .dc_gpio_num = TFT_DC_PIN,
    .cs_gpio_num = -1,
    .pclk_hz = 80 * 1000 * 1000,
    .lcd_cmd_bits = 8,
    .lcd_param_bits = 8,
    .spi_mode = 0,
    .trans_queue_depth = 10,
};
esp_lcd_new_panel_io_spi(&io_config, &io_handle);
```

#### Шаг 3: Создание ST7789 panel
```c
esp_lcd_panel_dev_config_t panel_config = {
    .reset_gpio_num = TFT_RST_PIN,
    .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
    .bits_per_pixel = 16,
};
esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle);
```

#### Шаг 4: Инициализация панели
```c
esp_lcd_panel_reset(panel_handle);
esp_lcd_panel_init(panel_handle);
esp_lcd_panel_invert_color(panel_handle, true);
esp_lcd_panel_swap_xy(panel_handle, true);
esp_lcd_panel_mirror(panel_handle, false, true);

// Включение подсветки
gpio_set_level(TFT_BL_PIN, 1);
```

### 2.2 Инициализация LVGL v9.4.0

#### Шаг 1: Инициализация библиотеки
```c
lv_init();
```

#### Шаг 2: Создание буфера отрисовки
```c
static lv_color_t buf1[TFT_WIDTH * 10];
static lv_color_t buf2[TFT_WIDTH * 10];

lv_disp_draw_buf_t draw_buf;
lv_disp_draw_buf_init(&draw_buf, buf1, buf2, TFT_WIDTH * 10);
```

#### Шаг 3: Регистрация драйвера
```c
static void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map) {
    int offsetx1 = area->x1;
    int offsety1 = area->y1;
    int offsetx2 = area->x2;
    int offsety2 = area->y2;

    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1,
                              offsetx2 + 1, offsety2 + 1, color_map);
    lv_disp_flush_ready(drv);
}

lv_disp_drv_t disp_drv;
lv_disp_drv_init(&disp_drv);
disp_drv.hor_res = TFT_WIDTH;
disp_drv.ver_res = TFT_HEIGHT;
disp_drv.flush_cb = lvgl_flush_cb;
disp_drv.draw_buf = &draw_buf;
lv_disp_drv_register(&disp_drv);
```

### 2.3 UI элементы

#### Создание labels
```c
static lv_obj_t *label_lat;
static lv_obj_t *label_lon;
static lv_obj_t *label_alt;
static lv_obj_t *label_sat;
static lv_obj_t *label_fix;
static lv_obj_t *label_acc;

void create_ui(void) {
    lv_obj_t *screen = lv_scr_act();

    // Координаты
    label_lat = lv_label_create(screen);
    lv_obj_set_pos(label_lat, 0, 0);
    lv_obj_set_size(label_lat, TFT_WIDTH, 20);
    lv_label_set_text(label_lat, "Lat: --");

    label_lon = lv_label_create(screen);
    lv_obj_set_pos(label_lon, 0, 22);
    lv_obj_set_size(label_lon, TFT_WIDTH, 20);
    lv_label_set_text(label_lon, "Lon: --");

    // Высота
    label_alt = lv_label_create(screen);
    lv_obj_set_pos(label_alt, 0, 44);
    lv_obj_set_size(label_alt, TFT_WIDTH, 20);
    lv_label_set_text(label_alt, "Alt: --");

    // Спутники
    label_sat = lv_label_create(screen);
    lv_obj_set_pos(label_sat, 0, 66);
    lv_obj_set_size(label_sat, TFT_WIDTH, 20);
    lv_label_set_text(label_sat, "Sats: --");

    // Fix качество
    label_fix = lv_label_create(screen);
    lv_obj_set_pos(label_fix, 0, 88);
    lv_obj_set_size(label_fix, TFT_WIDTH, 20);
    lv_label_set_text(label_fix, "Fix: NO FIX");

    // Точность
    label_acc = lv_label_create(screen);
    lv_obj_set_pos(label_acc, 0, 110);
    lv_obj_set_size(label_acc, TFT_WIDTH, 40);
    lv_label_set_text(label_acc, "Acc: --");
}
```

### 2.4 Функции форматирования

#### get_fix_type_string() - тип фикса
**Источник**: main.cpp:1062-1075
```c
const char* get_fix_type_string(int quality) {
    switch(quality) {
        case 0: return "NO FIX";
        case 1: return "GPS";
        case 2: return "DGPS";
        case 3: return "PPS";
        case 4: return "RTK-FIX";
        case 5: return "RTK-FLT";
        case 6: return "EST";
        case 7: return "MANUAL";
        case 8: return "SIM";
        default: return "UNKNOWN";
    }
}
```

#### format_satellite_string() - спутники
**Источник**: main.cpp:1188-1213
```c
void format_satellite_string(char *buf, size_t len) {
    snprintf(buf, len, "G:%d R:%d E:%d B:%d",
             g_sat_data.gps.used,
             g_sat_data.glonass.used,
             g_sat_data.galileo.used,
             g_sat_data.beidou.used);

    if (g_sat_data.qzss.used > 0) {
        char qzss[16];
        snprintf(qzss, sizeof(qzss), " Q:%d", g_sat_data.qzss.used);
        strncat(buf, qzss, len - strlen(buf) - 1);
    }
}
```

#### format_coord_line() - координаты
**Источник**: main.cpp:1216-1242
```c
void format_coord_line(char *buf, size_t len, const char *label, double value) {
    int decimals = 7; // Высокая точность для RTK
    snprintf(buf, len, "%s: %.7f", label, value);
}
```

### 2.5 Display Task

```c
void display_task(void *pvParameters) {
    ESP_LOGI(TAG, "Display task started on core %d", xPortGetCoreID());

    while (1) {
        // Обновление UI с GPS данными
        char buf[64];

        // Широта
        format_coord_line(buf, sizeof(buf), "Lat", g_gps_data.latitude);
        lv_label_set_text(label_lat, buf);

        // Долгота
        format_coord_line(buf, sizeof(buf), "Lon", g_gps_data.longitude);
        lv_label_set_text(label_lon, buf);

        // Высота
        snprintf(buf, sizeof(buf), "Alt: %.1fm", g_gps_data.altitude);
        lv_label_set_text(label_alt, buf);

        // Спутники
        format_satellite_string(buf, sizeof(buf));
        lv_label_set_text(label_sat, buf);

        // Fix качество
        snprintf(buf, sizeof(buf), "Fix: %s", get_fix_type_string(g_gps_data.fix_quality));
        lv_label_set_text(label_fix, buf);

        // Точность
        if (g_gps_data.lat_accuracy < 100.0) {
            snprintf(buf, sizeof(buf), "Acc: %.2fm %.2fm %.2fm",
                     g_gps_data.lat_accuracy,
                     g_gps_data.lon_accuracy,
                     g_gps_data.vertical_accuracy);
        } else {
            snprintf(buf, sizeof(buf), "Acc: --");
        }
        lv_label_set_text(label_acc, buf);

        // LVGL handler
        lv_task_handler();

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

---

## 3. Интеграция в main.c

### Добавить вызовы в app_main()
```c
void app_main(void) {
    // ... существующая инициализация

    // Инициализация GPS парсера
    // (нет init функции, только запуск задачи)

    // Инициализация дисплея
    ESP_ERROR_CHECK(display_manager_init());

    // Запуск задач
    xTaskCreatePinnedToCore(uart_task, "uart_task", 4096, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(ble_task, "ble_task", 4096, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(wifi_task, "wifi_task", 4096, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(gps_parser_task, "gps_task", 4096, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(display_task, "display_task", 4096, NULL, 5, NULL, 0);
}
```

---

## 4. Порядок реализации

### Шаг 1: GPS Parser (приоритет 1)
1. Добавить структуры данных в config.h
2. Реализовать вспомогательные функции (splitFields, convertToDecimalDegrees)
3. Реализовать парсеры в порядке:
   - parseGSV (простейший)
   - parseGST (простой)
   - parseGSA (средний)
   - parseGNS (сложный)
   - parseGGA (средний, но приоритетный)
4. Реализовать parseNMEA() диспетчер
5. Реализовать check_satellite_timeouts()
6. Обновить gps_parser_task() с логикой чтения NMEA

### Шаг 2: Display Manager (приоритет 2)
1. Инициализация SPI bus + ST7789V panel
2. Инициализация LVGL v9.4.0
3. Создание UI элементов
4. Реализовать функции форматирования
5. Обновить display_task() с обновлением labels

### Шаг 3: Тестирование
1. Подключить GPS модуль к UART (RX=6, TX=7)
2. Проверить парсинг NMEA в логах
3. Проверить отображение на TFT дисплее
4. Проверить передачу по BLE/WiFi

---

## 5. Ключевые отличия от Arduino кода

| Аспект | Arduino | ESP-IDF v6 |
|--------|---------|-----------|
| **Строки** | `String` | `char[]` + `snprintf()` |
| **Время** | `millis()` | `esp_timer_get_time()` / 1000 |
| **Буферы** | Динамические String | Статические массивы |
| **Точность** | `String(value, decimals)` | `snprintf(buf, len, "%.7f", value)` |
| **Дисплей** | TFT_eSPI | esp_lcd + LVGL v9.4.0 |

---

## 6. Оценка объема кода

- **gps_parser.c**: ~400 строк (парсеры + структуры + task)
- **display_manager.c**: ~300 строк (init + LVGL + UI + task)
- **Обновление config.h**: +50 строк (структуры данных)
- **Обновление main.c**: +20 строк (вызовы init + tasks)

**Итого**: ~770 строк нового кода
