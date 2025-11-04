# Быстрый старт

> ESP-IDF v6 уже установлен, активируется командой `idf6`

## Интеграция LVGL v9.4.0

```bash
cd esp-idf-v6/components

# Клонировать LVGL версии 9.4.0
git clone --depth 1 --branch v9.4.0 https://github.com/lvgl/lvgl.git

# lv_conf.h уже создан в components/lv_conf.h
# CMakeLists.txt для LVGL тоже готов
```

## Сборка для ESP32-C6

```bash
cd esp-idf-v6

# Активировать ESP-IDF v6
idf6

# Выбрать целевую платформу
idf.py set-target esp32c6

# Настроить проект (опционально)
idf.py menuconfig
# Проверьте: GNSS BLE Bridge Configuration -> Target Board -> ESP32-C6 Tenstar SuperMini

# Собрать
idf.py build

# Прошить (замените /dev/ttyUSB0 на ваш порт)
idf.py -p /dev/ttyUSB0 flash

# Мониторинг
idf.py -p /dev/ttyUSB0 monitor
```

## Конфигурация пинов ESP32-C6

По умолчанию настроены следующие пины:

| Функция | GPIO | Описание |
|---------|------|----------|
| UART RX | 6 | Прием от GPS модуля |
| UART TX | 7 | Передача в GPS модуль |
| SPI MOSI | 18 | Данные для TFT дисплея |
| SPI SCLK | 19 | Clock для TFT дисплея |
| TFT DC | 20 | Data/Command |
| TFT RST | 21 | Reset |
| TFT BL | 22 | Backlight |
| LED | 8 | Адресный светодиод WS2812 |
| LED_2 | 15 | Обычный светодиод |

## Проверка работы

После прошивки вы увидите:

```
I (xxx) MAIN: ===========================================
I (xxx) MAIN: GNSS BLE/WiFi Bridge - ESP-IDF v6
I (xxx) MAIN: Board: ESP32-C6
I (xxx) MAIN: ===========================================
I (xxx) MAIN: Initializing UART on RX:6 TX:7
I (xxx) MAIN: UART initialized successfully
I (xxx) UART: UART task started on core 0
I (xxx) MAIN: System initialized successfully
I (xxx) MAIN: Free heap: XXXXXX bytes
```

## Следующие шаги

1. **Реализовать BLE сервис** (см. [ble_service.c](main/ble_service.c))
2. **Реализовать WiFi AP** (см. [wifi_service.c](main/wifi_service.c))
3. **Портировать NMEA парсер** (см. [gps_parser.c](main/gps_parser.c))
4. **Создать LVGL UI** (см. [display_manager.c](main/display_manager.c))

## Полезные команды

```bash
# Очистить всё
idf.py fullclean

# Только прошивка без сборки
idf.py flash

# Только мониторинг
idf.py monitor

# Выход из монитора
Ctrl + ]

# Изменить настройки пинов
idf.py menuconfig
# GNSS BLE Bridge Configuration -> Пины для ESP32-C6 Tenstar SuperMini

# Проверить размер прошивки
idf.py size
```

## Устранение проблем

### LVGL не компилируется

```bash
# Проверьте что lv_conf.h существует
ls components/lv_conf.h

# Убедитесь что CMakeLists.txt для LVGL создан
ls components/lvgl/CMakeLists.txt
```

### Ошибка "LVGL not found"

```bash
# Убедитесь что LVGL клонирован
ls components/lvgl/src

# Пересоберите
idf.py fullclean
idf.py build
```

### Недостаточно памяти

В [sdkconfig.defaults.esp32c6](sdkconfig.defaults.esp32c6) уже настроено:
- Flash: 4MB
- Partition table для 4MB

Если проблемы продолжаются, уменьшите `LV_MEM_SIZE` в [components/lv_conf.h](components/lv_conf.h).

## Поддержка

Смотрите полную документацию:
- [README.md](README.md) - Основная документация
- [INTEGRATION_GUIDE.md](INTEGRATION_GUIDE.md) - Интеграция компонентов
- [MIGRATION_NOTES.md](MIGRATION_NOTES.md) - Заметки о миграции
