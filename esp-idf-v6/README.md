# GNSS BLE/WiFi Bridge - ESP-IDF v6

Порт проекта на ESP-IDF v6 с LVGL v9.4.0 для всех дисплеев.

## Поддерживаемые платформы

- **ESP32-C3** - OLED + TFT дисплеи
- **ESP32-C6** Tenstar SuperMini - только TFT дисплей (240x280)
- **ESP32-S3** - OLED + TFT дисплеи, dual-core

## Быстрый старт для ESP32-C6

```bash
# Активировать ESP-IDF v6
idf6

# Интегрировать LVGL v9.4.0
cd components
git clone --depth 1 --branch v9.4.0 https://github.com/lvgl/lvgl.git
cd ..

# Собрать для C6
idf.py set-target esp32c6
idf.py build

# Прошить
idf.py -p /dev/ttyUSB0 flash monitor
```

## Пины ESP32-C6 (по умолчанию)

| Функция | GPIO |
|---------|------|
| UART RX (GPS) | 6 |
| UART TX (GPS) | 7 |
| SPI MOSI | 18 |
| SPI SCLK | 19 |
| TFT DC | 20 |
| TFT RST | 21 |
| TFT BL | 22 |
| LED WS2812 | 8 |
| LED обычный | 15 |

## Характеристики

### Реализовано ✅
- Базовая структура проекта
- Модульная архитектура
- UART инициализация (460800 baud)
- Ring buffers (16KB TX, 4KB RX)
- Условная компиляция C3/C6/S3
- Kconfig для настройки пинов

### TODO 🚧
- BLE сервис (NimBLE Nordic UART)
- WiFi AP + TCP сервер (порт 23)
- NMEA парсер (GNS, GST, GSA, GSV)
- LVGL UI для TFT дисплея

## Настройка пинов

```bash
idf.py menuconfig
# GNSS BLE Bridge Configuration -> Пины для ESP32-C6 Tenstar SuperMini
```

## Структура

```
esp-idf-v6/
├── main/
│   ├── main.c              # Инициализация, UART, буферы
│   ├── ble_service.c       # TODO: NimBLE
│   ├── wifi_service.c      # TODO: WiFi AP
│   ├── gps_parser.c        # TODO: NMEA парсинг
│   └── display_manager.c   # TODO: LVGL UI
├── components/
│   ├── lv_conf.h           # Конфигурация LVGL 9.4.0
│   └── lvgl/               # git clone сюда
└── sdkconfig.defaults.*    # Конфиги для C3/C6/S3
```

## Отличия от Arduino версии

| | Arduino | ESP-IDF v6 |
|---|---------|-----------|
| **Дисплеи** | TFT_eSPI, Arduino_GFX | LVGL v9.4.0 + esp_lcd |
| **BLE** | NimBLE-Arduino | esp-nimble |
| **WiFi** | WiFi.h | esp_wifi |
| **Задачи** | loop() | FreeRTOS tasks |
| **Память** | String | стандартный C |

## Полезные команды

```bash
idf.py fullclean          # Очистить всё
idf.py size               # Размер прошивки
idf.py monitor            # Только монитор (Ctrl+] выход)
```

## Память (ESP32-C6)

- Flash: 4MB
- RAM: ~80KB используется
- LVGL буфер: 64KB

## Документация

- [QUICK_START.md](QUICK_START.md) - Пошаговая инструкция
- [CHANGELOG.md](CHANGELOG.md) - История изменений
- [MIGRATION_NOTES.md](MIGRATION_NOTES.md) - Проблемы Arduino кода

## Лицензия

MIT License
