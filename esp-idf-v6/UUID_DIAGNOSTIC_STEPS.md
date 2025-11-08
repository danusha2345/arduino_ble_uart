# Диагностика UUID проблемы - Пошаговая инструкция

## ШАГ 1: Убедитесь что собрали ПОСЛЕДНИЙ код

На вашем компьютере:

```bash
cd /path/to/arduino_ble_uart/esp-idf-v6

# Получить последние изменения
git pull origin claude/idf-v6-work-011CUptwuzUc6iwb4a2kTcke

# Проверить что файлы обновлены
git log --oneline -3
```

Должны быть commits:
```
b030bf0 Add comprehensive menuconfig optimization guide
a107a0c Fix critical Nordic UART Service architecture issues
e9905db Add NimBLE analysis documentation
```

---

## ШАГ 2: Полная пересборка

```bash
# Полная очистка
idf.py fullclean

# Пересборка
idf.py build

# Прошивка
idf.py -p /dev/ttyUSB0 flash
```

---

## ШАГ 3: Проверка логов ESP32

Запустите монитор:

```bash
idf.py -p /dev/ttyUSB0 monitor
```

**ОБЯЗАТЕЛЬНО найдите эти строки:**

```
I (xxx) BLE: Registered service 6e400001-b5a3-f393-e0a9-e50e24dcca9e with handle=XX
I (xxx) BLE: Registered characteristic 6e400002-b5a3-f393-e0a9-e50e24dcca9e with def_handle=XX val_handle=XX
I (xxx) BLE: Registered characteristic 6e400003-b5a3-f393-e0a9-e50e24dcca9e with def_handle=XX val_handle=XX
I (xxx) BLE: Registered descriptor 2902 with handle=XX
I (xxx) BLE: Advertising started: UM980_C6_GPS (UUID in adv, name in scan rsp)
```

### ✅ Если видите эти UUID - код ПРАВИЛЬНЫЙ!

### ❌ Если видите ДРУГИЕ UUID - скопируйте и пришлите!

---

## ШАГ 4: Проверка в nRF Connect

### 4.1. Очистите кэш nRF Connect

**Android:**
1. Настройки → Приложения → nRF Connect
2. Хранилище → **Очистить данные**
3. Перезапустить nRF Connect

**iOS:**
1. Удалить приложение nRF Connect
2. Переустановить из App Store

### 4.2. Сканирование

1. Откройте nRF Connect
2. Нажмите **SCAN**
3. Найдите устройство **UM980_C6_GPS**

**ЧТО вы видите в advertising данных?**

Должно быть:
```
Complete List of 128-bit Service UUIDs:
  6E400001-B5A3-F393-E0A9-E50E24DCCA9E
```

### ❌ Если видите ДРУГОЙ UUID:
Скопируйте ТОЧНО ЧТО видите и пришлите!

### 4.3. Подключение

1. Нажмите **CONNECT** (не просто смотрите!)
2. Введите PIN **123456** если попросит
3. Дождитесь "Connected"

### 4.4. Проверка сервисов

После подключения должны появиться:

```
Generic Access (0x1800)
Generic Attribute (0x1801)
Unknown Service (6E400001-B5A3-F393-E0A9-E50E24DCCA9E)
  ├─ Unknown Characteristic (6E400003-...) [NOTIFY]
  │  └─ Client Characteristic Configuration (0x2902)
  └─ Unknown Characteristic (6E400002-...) [WRITE, WRITE WITHOUT RESPONSE]
```

**ВАЖНО:** nRF Connect может показывать "Unknown Service" - это нормально!
Главное чтобы UUID был `6E400001-...`

### ❌ Если видите ДРУГОЙ UUID:
Сделайте скриншот и пришлите!

---

## ШАГ 5: Что сообщить если проблема сохраняется

Соберите следующую информацию:

### 5.1. Логи ESP32

Скопируйте **ВСЕ** строки от запуска до advertising:

```bash
idf.py -p /dev/ttyUSB0 monitor | tee esp32_full_log.txt
```

Найдите в логах:
- Строки с `BLE: Registered service`
- Строки с `BLE: Registered characteristic`
- Строки с `BLE: Advertising started`

### 5.2. nRF Connect скриншоты

Сделайте 3 скриншота:

1. **Scanner tab** - показывающий advertising data
2. **После CONNECT** - показывающий список сервисов (CLIENT section)
3. **Развернутый сервис** - показывающий характеристики

### 5.3. Точный UUID который вы видите

**В nRF Connect что ТОЧНО написано?**

Например:
```
0000AAA0-0000-1000-8000-AABBCCDDEEFF  ❌ Неправильный!
6E400001-B5A3-F393-E0A9-E50E24DCCA9E  ✅ Правильный!
```

Пришлите ТОЧНЫЙ UUID который видите!

---

## Возможные причины проблемы

### Причина 1: Старая прошивка на ESP32
**Решение:** Полная перепрошивка (erase flash)
```bash
idf.py -p /dev/ttyUSB0 erase-flash
idf.py -p /dev/ttyUSB0 flash
```

### Причина 2: Кэш nRF Connect
**Решение:** Очистить данные приложения (см. Шаг 4.1)

### Причина 3: Несколько ESP32 устройств
**Решение:** Проверьте что подключаетесь к правильному!
- Посмотрите MAC адрес в логах ESP32
- Сверьте с MAC в nRF Connect

### Причина 4: Собрали не ту ветку
**Решение:** Проверьте git branch:
```bash
git branch  # Должно быть: * claude/idf-v6-work-011CUptwuzUc6iwb4a2kTcke
git log --oneline -1  # Должно быть: b030bf0 или новее
```

---

## Экспресс-проверка (БЫСТРО)

```bash
# 1. На вашем компьютере - проверить UUID в коде
grep -A 2 "gatt_svr_svc_uuid" main/ble_service.c | grep 0x01

# Должно быть: ... 0x01, 0x00, 0x40, 0x6e
# Это означает UUID заканчивается на ...0001 (service)

# 2. На ESP32 - проверить логи
idf.py monitor | grep "Registered service"

# Должно быть: 6e400001-b5a3-f393-e0a9-e50e24dcca9e
```

---

## Итог

Если UUID в коде правильный (`6e400001-...`) И в логах ESP32 правильный (`6e400001-...`),
НО в nRF Connect неправильный - это **100% кэш nRF Connect**!

Очистите данные приложения nRF Connect и пробуйте снова.
