# Инструкция по исправлению BLE

## Проблема
Телефон показывает неправильный UUID: `0000aaa0-0000-1000-8000-aabbccddeeff`
Ожидается Nordic UART Service UUID: `6E400001-B5A3-F393-E0A9-E50E24DCCA9E`

## Причина
На устройстве ESP32-C6 запущена старая прошивка. Код в репозитории правильный, но не прошит на устройство.

## Решение

### Шаг 1: Активировать ESP-IDF v6

```bash
cd /home/user/arduino_ble_uart/esp-idf-v6

# Активировать ESP-IDF v6 (команда может отличаться в вашей системе)
# Варианты:
source ~/esp/esp-idf/export.sh  # Если ESP-IDF в ~/esp/esp-idf
source ~/.espressif/esp-idf/export.sh  # Альтернативный путь
idf6  # Если настроен alias
```

### Шаг 2: Очистить и пересобрать проект

```bash
# Полная очистка (рекомендуется)
idf.py fullclean

# Выбрать целевую платформу (если нужно)
idf.py set-target esp32c6

# Собрать проект
idf.py build
```

### Шаг 3: Прошить устройство

```bash
# Прошить (замените /dev/ttyUSB0 на ваш порт)
idf.py -p /dev/ttyUSB0 flash

# Или если порт другой:
# Linux: /dev/ttyUSB0, /dev/ttyACM0
# macOS: /dev/cu.usbserial-*
# Windows: COM3, COM4, etc.
```

### Шаг 4: Проверить логи

```bash
# Запустить монитор для просмотра логов
idf.py -p /dev/ttyUSB0 monitor

# Ожидаемые логи при успешной инициализации BLE:
# I (xxx) BLE: Registered service 6e400001-b5a3-f393-e0a9-e50e24dcca9e with handle=...
# I (xxx) BLE: Registered characteristic 6e400002-... (RX)
# I (xxx) BLE: Registered characteristic 6e400003-... (TX)
# I (xxx) BLE: Advertising started: UM980_C6_GPS (UUID in adv, name in scan rsp)
```

### Шаг 5: Очистить BLE кэш на телефоне

**Android:**
1. Откройте "Настройки" → "Bluetooth"
2. Найдите "UM980_C6_GPS" или "UM980_*_GPS"
3. Нажмите на значок шестеренки → "Забыть"
4. Перезапустите Bluetooth на телефоне
5. Повторно подключитесь к устройству

**iOS:**
1. "Настройки" → "Bluetooth"
2. Нажмите (i) рядом с устройством
3. "Забыть это устройство"
4. Перезапустите Bluetooth
5. Повторно подключитесь

### Шаг 6: Проверить подключение

Откройте Nordic UART приложение на телефоне и найдите устройство:
- **Имя:** `UM980_C6_GPS` (для ESP32-C6)
- **UUID сервиса:** `6E400001-B5A3-F393-E0A9-E50E24DCCA9E`

Если всё работает, вы увидите:
- Устройство в списке BLE устройств
- Возможность подключиться
- Две характеристики: TX (6E400003) и RX (6E400002)
- Поток NMEA данных от GPS модуля

## Диагностика проблем

### Проблема: ESP-IDF не найден

Если команда `idf.py` не найдена, установите ESP-IDF v6:

```bash
# Клонировать ESP-IDF v6
cd ~
mkdir esp
cd esp
git clone --recursive --branch v6.0 https://github.com/espressif/esp-idf.git

# Установить зависимости
cd esp-idf
./install.sh esp32c6

# Активировать окружение
source ~/esp/esp-idf/export.sh

# Добавить alias в ~/.bashrc (опционально)
echo 'alias idf6="source ~/esp/esp-idf/export.sh"' >> ~/.bashrc
```

### Проблема: Ошибка компиляции

```bash
# Проверьте версию ESP-IDF
idf.py --version  # Должно быть v6.0 или выше

# Убедитесь что LVGL установлен
ls components/lvgl/src  # Должны быть файлы LVGL

# Если LVGL отсутствует:
cd components
git clone --depth 1 --branch v9.4.0 https://github.com/lvgl/lvgl.git
cd ..

# Пересоберите
idf.py fullclean
idf.py build
```

### Проблема: Устройство не подключается

```bash
# Проверьте доступные порты
ls /dev/tty*  # Linux/macOS
# Или
ls /dev/cu.*  # macOS

# Добавьте пользователя в группу dialout (Linux)
sudo usermod -a -G dialout $USER
# Перезайдите в систему после этого
```

### Проблема: BLE всё ещё не работает

1. **Проверьте логи через serial monitor:**
   ```bash
   idf.py -p /dev/ttyUSB0 monitor
   ```

2. **Ищите ошибки:**
   - `E (xxx) BLE: ...` - ошибки BLE
   - `W (xxx) BLE: ...` - предупреждения BLE

3. **Проверьте регистрацию GATT:**
   Должны быть логи:
   ```
   I (xxx) BLE: Registered service 6e400001-...
   I (xxx) BLE: Registered characteristic 6e400002-... (RX)
   I (xxx) BLE: Registered characteristic 6e400003-... (TX)
   I (xxx) BLE: CCCD descriptor registered
   ```

4. **Очистите NVS (Non-Volatile Storage):**
   ```bash
   idf.py -p /dev/ttyUSB0 erase-flash
   idf.py -p /dev/ttyUSB0 flash
   ```

## Проверка текущего кода

Код в репозитории **ПРАВИЛЬНЫЙ**:

```c
// ble_service.c:37-39
static const ble_uuid128_t gatt_svr_svc_uuid =
    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
                     0x93, 0xf3, 0xa3, 0xb5, 0x01, 0x00, 0x40, 0x6e);

// = 6E400001-B5A3-F393-E0A9-E50E24DCCA9E ✓

// Advertising включает UUID сервиса (ble_service.c:277-279):
adv_fields.uuids128 = &gatt_svr_svc_uuid;
adv_fields.num_uuids128 = 1;
adv_fields.uuids128_is_complete = 1;

// GATT сервис регистрируется правильно (ble_service.c:140-141):
.type = BLE_GATT_SVC_TYPE_PRIMARY,
.uuid = &gatt_svr_svc_uuid.u,
```

UUID `0x0000AAA0` НЕ НАЙДЕН нигде в коде. Это старая прошивка на устройстве.

## Итог

**Проблема:** Старая прошивка на ESP32-C6
**Решение:** Пересобрать и прошить актуальный код из репозитория
**Результат:** Nordic UART Service будет работать с правильным UUID
