# ESP32-S3-Zero Pin Connections

## Общая информация
- **Плата**: ESP32-S3-Zero (Waveshare или аналог)
- **Flash**: 4MB
- **PSRAM**: 2MB (Octal)
- **USB**: Встроенный USB-Serial/JTAG через GPIO43/44

## ⚠️ Зарезервированные пины (НЕ ИСПОЛЬЗОВАТЬ)
- **GPIO33-37**: Зарезервированы для Octal PSRAM
- **GPIO21**: Встроенный WS2812 RGB LED на плате
- **GPIO43-44**: USB Serial/JTAG (UART0 для USB CDC отладки)

## ✅ Используемые пины
- **GPIO1-2**: I2C для OLED дисплея (SCL=1, SDA=2)
- **GPIO5-6**: UART1 для GPS модуля
- **GPIO9-13**: SPI для TFT дисплея (временно отключен)
- **GPIO43-44**: USB CDC для отладки (встроенный)

---

## 📡 GPS модуль (UART1 - GPIO5/6)

| Функция | ESP32-S3 Pin | GPS Модуль Pin | Описание |
|---------|--------------|----------------|----------|
| RX      | **GPIO5**    | TX             | Прием данных от GPS (UART1 RX) |
| TX      | **GPIO6**    | RX             | Передача команд GPS (UART1 TX) |
| GND     | GND          | GND            | Земля |
| VCC     | 3.3V         | VCC            | Питание 3.3V |

**Примечание**: Используется UART1 на GPIO5/6. UART0 (GPIO43/44) зарезервирован для USB CDC отладки.

---

## 🖥️ OLED дисплей (I2C - GPIO1/2)

| Функция | ESP32-S3 Pin | OLED Pin | Описание |
|---------|--------------|----------|----------|
| SCL     | **GPIO1**    | SCL      | I2C тактирование |
| SDA     | **GPIO2**    | SDA      | I2C данные |
| GND     | GND          | GND      | Земля |
| VCC     | 3.3V         | VCC      | Питание 3.3V |

**I2C адрес**: 0x3C (или 0x78 в зависимости от библиотеки)

---

## 📺 TFT дисплей ST7789V (SPI - GPIO9-13) ⚠️ **Currently Disabled**

| Функция | ESP32-S3 Pin | TFT Pin | Описание |
|---------|--------------|---------|----------|
| DC      | **GPIO9**    | DC      | Data/Command выбор |
| RST     | **GPIO10**   | RST     | Reset |
| MOSI    | **GPIO11**   | SDA     | SPI данные (Master Out) |
| SCLK    | **GPIO12**   | SCL     | SPI тактирование |
| BL      | **GPIO13**   | BL      | Backlight (подсветка) |
| CS      | GND          | CS      | Chip Select (подключен к GND) |
| GND     | GND          | GND     | Земля |
| VCC     | 3.3V         | VCC     | Питание 3.3V |

**⚠️ ВАЖНО**: TFT дисплей **отключен в прошивке** по умолчанию. TFT_eSPI библиотека требует физически подключенного дисплея - попытка инициализации без дисплея вызывает краш устройства.

**Чтобы включить TFT (при подключенном дисплее)**:
1. Подключите TFT дисплей к пинам GPIO9-13
2. Раскомментируйте код инициализации TFT в `src/main.cpp` (найдите "TODO: Раскомментируйте")
3. Удалите защиты `#ifndef ESP32_S3` в функциях `clearDisplayLine()` и `updateDisplayLine()`
4. Пересоберите и загрузите прошивку

**Примечание**: Если подсветка всегда включена, GPIO13 можно подключить к 3.3V напрямую.

---

## 💡 RGB LED (встроенный)

| Функция | ESP32-S3 Pin | Описание |
|---------|--------------|----------|
| LED     | **GPIO21**   | WS2812 RGB LED (встроенный на плате) |

**⚠️ Внимание**: GPIO21 уже занят встроенным светодиодом WS2812 и не должен использоваться для других целей.

---

## 🔌 Доступные свободные пины

Если нужно подключить дополнительные устройства:
- GPIO0, GPIO1, GPIO2, GPIO3, GPIO4
- GPIO14, GPIO15, GPIO16, GPIO17, GPIO18, GPIO19, GPIO20
- GPIO38, GPIO39, GPIO40, GPIO41, GPIO42, GPIO45, GPIO46, GPIO47, GPIO48

**Избегайте**:
- GPIO21 (WS2812 LED)
- GPIO33-37 (PSRAM)
- GPIO43-44 (USB CDC)

**Уже используются**:
- GPIO5-6 (UART1 для GPS)
- GPIO7-8 (I2C для OLED)
- GPIO9-13 (SPI для TFT)
- GPIO43-44 (USB CDC отладка)

---

## 📋 Конфигурация PlatformIO

```ini
[env:esp32-s3]
platform = espressif32@6.12.0
board = esp32-s3-devkitc-1
framework = arduino
board_upload.flash_size = 4MB
board_build.partitions = default.csv
monitor_speed = 115200
upload_port = COM39
build_flags = 
    -DARDUINO_USB_CDC_ON_BOOT=1
    -DBOARD_HAS_PSRAM
    -DTZ_FORCE_OFFSET_MINUTES=180
    -DESP32_S3=1
    -DCORE_DEBUG_LEVEL=3
lib_deps = 
    h2zero/NimBLE-Arduino@^2.3.6
    adafruit/Adafruit SSD1306@^2.5.7
    adafruit/Adafruit GFX Library@^1.11.9
    mikalhart/TinyGPSPlus@^1.0.3
    bodmer/TFT_eSPI@^2.5.0
```

---

## 🛠️ Подключение и прошивка

1. **Подключите USB-C** кабель к ESP32-S3-Zero
2. **Определите COM порт** (обычно COM39)
3. **Компиляция**:
   ```bash
   platformio run -e esp32-s3
   ```
4. **Прошивка**:
   ```bash
   platformio run -e esp32-s3 --target upload
   ```
5. **Мониторинг**:
   ```bash
   platformio device monitor -e esp32-s3 -b 115200
   ```

---

## ✅ Проверка работы

После успешной прошивки:
1. ✅ Нет циклической перезагрузки
2. ✅ USB CDC Serial работает на COM порту
3. ✅ OLED дисплей показывает информацию
4. ✅ TFT дисплей показывает GPS данные
5. ✅ BLE доступен для подключения
6. ✅ WiFi может раздавать NTRIP данные

---

## 🐛 Решение проблем

### Циклическая перезагрузка
- **Причина**: Неправильные флаги PSRAM или board configuration
- **Решение**: Используйте указанную выше конфигурацию с `board_build.partitions = default.csv`

### Нет вывода в Serial Monitor через USB
- **Причина**: USB кабель не подключен или неправильный COM порт
- **Решение**: Проверьте подключение USB-C кабеля и убедитесь, что выбран правильный COM порт. В коде добавлена задержка `delay(2000)` после `Serial.begin()` для стабилизации USB CDC

### Устройства не определяются (GPS/OLED/TFT)
- **Причина**: Неправильные пины
- **Решение**: Проверьте подключение согласно таблице выше

---

## 📶 Беспроводные интерфейсы ESP32-S3

### WiFi Access Point
- **SSID**: `UM980_GPS_BRIDGE_S3` (отличается от ESP32-C3: `UM980_GPS_BRIDGE`)
- **IP адрес**: 192.168.4.1
- **Порт**: 23 (Telnet-like протокол)
- **Пароль**: 123456789

### Bluetooth Low Energy (BLE)
- **Имя устройства**: `UM980_S3_GPS` (отличается от ESP32-C3: `UM980_C3_GPS`)
- **Протокол**: Nordic UART Service (NUS) - стандартный
- **MTU**: 512 байт
- **Service UUID**: `6E400001-B5A3-F393-E0A9-E50E24DCCA9E` (стандартный NUS)
- **RX Characteristic UUID**: `6E400002-B5A3-F393-E0A9-E50E24DCCA9E`
- **TX Characteristic UUID**: `6E400003-B5A3-F393-E0A9-E50E24DCCA9E`

**Примечание**: Используются стандартные Nordic UART Service UUID для совместимости с приложениями. Устройства S3 и C3 различаются по именам (`UM980_S3_GPS` vs `UM980_C3_GPS`).

---

**Создано**: 2025-01-12
**Обновлено**: 2025-01-12
**Версия прошивки**: ESP32-S3 с уникальными BLE UUID и исправленной конфигурацией
