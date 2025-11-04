# Руководство по интеграции компонентов

Это руководство описывает шаги для интеграции необходимых компонентов в ESP-IDF v6 проект.

## 1. Интеграция LVGL v9.4.0

LVGL v9.4.0 - это графическая библиотека для встраиваемых систем, используется для TFT дисплея ST7789V.

### Шаг 1: Клонирование LVGL v9.4.0

```bash
cd esp-idf-v6/components

# Клонируйте ИМЕННО версию 9.4.0
git clone --depth 1 --branch v9.4.0 https://github.com/lvgl/lvgl.git
cd lvgl
```

### Шаг 2: Использование готового lv_conf.h

Файл `components/lv_conf.h` уже создан с оптимальными настройками для ESP32-C6/C3/S3.

Содержимое (для справки):

```c
#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_CONF_INCLUDE_SIMPLE 1

// Цветовой формат
#define LV_COLOR_DEPTH 16

// Параметры памяти
#define LV_MEM_SIZE (64 * 1024U)
#define LV_MEM_CUSTOM 0

// Оптимизация производительности
#define LV_USE_PERF_MONITOR 1
#define LV_USE_MEM_MONITOR 1

// Включение виджетов
#define LV_USE_LABEL 1
#define LV_USE_BTN 1
#define LV_USE_IMG 1

// Шрифты
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1

// Логирование
#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_INFO
#define LV_LOG_PRINTF 1

#endif
```

### Шаг 3: Создание CMakeLists.txt для LVGL

Создайте `components/lvgl/CMakeLists.txt`:

```cmake
file(GLOB_RECURSE SOURCES
    src/*.c
    src/core/*.c
    src/draw/*.c
    src/font/*.c
    src/hal/*.c
    src/misc/*.c
    src/widgets/*.c
)

idf_component_register(
    SRCS ${SOURCES}
    INCLUDE_DIRS "." "src"
    REQUIRES driver
)

target_compile_definitions(${COMPONENT_LIB} PUBLIC "-DLV_CONF_INCLUDE_SIMPLE")
```

## 2. Интеграция ESP-IDF LCD компонента

ESP-IDF уже включает драйверы для LCD дисплеев через `esp_lcd`.

### Для SPI дисплея (ST7789V)

Компонент `esp_lcd` уже включен в ESP-IDF. Убедитесь что в `main/CMakeLists.txt` есть:

```cmake
REQUIRES esp_lcd spi_flash driver
```

### Для I2C дисплея (SSD1306)

Потребуется дополнительный драйвер для SSD1306. Можно использовать компонент от Espressif:

```bash
cd esp-idf-v6/components
git clone https://github.com/espressif/esp-bsp.git
# Используйте драйверы из esp-bsp/components/lcd
```

Или создать свой драйвер на основе примеров ESP-IDF.

## 3. Интеграция NimBLE

NimBLE уже включен в ESP-IDF, но нужно правильно настроить.

### Шаг 1: Включение в sdkconfig

Убедитесь что в `sdkconfig.defaults.*` есть:

```
CONFIG_BT_ENABLED=y
CONFIG_BT_NIMBLE_ENABLED=y
CONFIG_BTDM_CTRL_MODE_BLE_ONLY=y
```

### Шаг 2: Добавление зависимостей

В `main/CMakeLists.txt` добавьте:

```cmake
REQUIRES bt
```

### Шаг 3: Включение заголовков

В `ble_service.c` используйте:

```c
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/ble_gatt.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
```

## 4. Структура компонентов

После интеграции структура должна выглядеть так:

```
esp-idf-v6/
├── components/
│   ├── lvgl/
│   │   ├── CMakeLists.txt
│   │   ├── lv_conf.h
│   │   └── src/...
│   └── esp-bsp/ (опционально, для SSD1306)
│       └── components/lcd/
├── main/
│   ├── CMakeLists.txt
│   ├── main.c
│   ├── ble_service.c
│   ├── wifi_service.c
│   ├── gps_parser.c
│   └── display_manager.c
└── CMakeLists.txt
```

## 5. Обновление main/CMakeLists.txt

Полный файл `main/CMakeLists.txt` должен содержать:

```cmake
idf_component_register(
    SRCS
        "main.c"
        "ble_service.c"
        "wifi_service.c"
        "gps_parser.c"
        "display_manager.c"
    INCLUDE_DIRS "."
    REQUIRES
        nvs_flash
        esp_wifi
        esp_netif
        lwip
        bt
        driver
        esp_lcd
        spi_flash
        freertos
        lvgl
)
```

## 6. Тестирование компиляции

После интеграции всех компонентов:

```bash
cd esp-idf-v6

# Для ESP32-C3
idf.py set-target esp32c3
idf.py build

# Для ESP32-C6
idf.py set-target esp32c6
idf.py build

# Для ESP32-S3
idf.py set-target esp32s3
idf.py build
```

## 7. Возможные проблемы и решения

### Проблема: LVGL не компилируется

**Решение**: Проверьте что `lv_conf.h` находится в корне компонента LVGL и содержит `#define LV_CONF_INCLUDE_SIMPLE 1`

### Проблема: NimBLE не найден

**Решение**: Убедитесь что в `sdkconfig` включены опции Bluetooth:
```bash
idf.py menuconfig
# Component config -> Bluetooth -> Enabled
```

### Проблема: esp_lcd не найден

**Решение**: Добавьте в `main/CMakeLists.txt`:
```cmake
REQUIRES esp_lcd
```

### Проблема: Недостаточно памяти

**Решение**:
- Уменьшите `LV_MEM_SIZE` в `lv_conf.h`
- Для ESP32-S3 включите PSRAM в sdkconfig
- Оптимизируйте буферы в `config.h`

## 8. Следующие шаги

После успешной интеграции компонентов:

1. Реализуйте BLE сервис в `ble_service.c`
2. Реализуйте WiFi AP в `wifi_service.c`
3. Реализуйте NMEA парсер в `gps_parser.c`
4. Создайте UI с LVGL в `display_manager.c`

## 9. Полезные ссылки

- [LVGL документация](https://docs.lvgl.io/)
- [ESP-IDF LCD API](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/lcd.html)
- [NimBLE документация](https://mynewt.apache.org/latest/network/index.html)
- [ESP-IDF примеры](https://github.com/espressif/esp-idf/tree/master/examples)
