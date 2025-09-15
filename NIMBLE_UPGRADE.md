# 🔄 Обновление до NimBLE-Arduino 2.3.6

## ✅ Что обновлено

### 📦 Версия библиотеки
- **platformio.ini**: Обновлена версия с `^1.4.1` на `^2.3.6`

### 🔧 API изменения

#### 1. Коллбэки серверов
```cpp
// Старая версия (1.x):
void onConnect(NimBLEServer* pServer) { ... }
void onDisconnect(NimBLEServer* pServer) { ... }

// Новая версия (2.x):
void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) { ... }
void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) { ... }
```

#### 2. Коллбэки характеристик
```cpp
// Старая версия:
void onRead(NimBLECharacteristic* pChar, ble_gap_conn_desc* desc) { ... }

// Новая версия:
void onRead(NimBLECharacteristic* pChar, NimBLEConnInfo& connInfo) { ... }
```

#### 3. Константы свойств характеристик
```cpp
// Старая версия:
NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::READ
NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR

// Новая версия:
BLE_GATT_CHR_PROP_NOTIFY | BLE_GATT_CHR_PROP_READ
BLE_GATT_CHR_PROP_WRITE | BLE_GATT_CHR_PROP_WRITE_NO_RSP
```

#### 4. Методы Advertising
```cpp
// Старая версия:
pAdvertising->setScanResponse(true);
pAdvertising->setMinPreferred(0x06);
pAdvertising->setMaxPreferred(0x06);

// Новая версия:
pAdvertising->enableScanResponse(true);
pAdvertising->setPreferredParams(0x06, 0x06);
```

#### 5. Настройка мощности
```cpp
// Старая версия:
NimBLEDevice::setPower(ESP_PWR_LVL_P9);

// Новая версия:
NimBLEDevice::setPower(9); // Прямое значение в dBm
```

#### 6. Удаленные методы
```cpp
// Этот метод больше не существует в версии 2.x:
pTxCharacteristic->getSubscribedCount()

// Решение: стек сам управляет подписками,
// просто вызывайте notify() без проверок
pTxCharacteristic->notify();
```

## 🚀 Компиляция проекта

### Через VS Code + PlatformIO
1. Установите расширение PlatformIO IDE
2. Откройте проект в VS Code
3. Нажмите на иконку PlatformIO слева
4. Выберите "Build" или используйте Ctrl+Alt+B

### Через командную строку
```bash
# Установите PlatformIO Core, если не установлено:
pip install platformio

# Компиляция:
pio run

# Прошивка:
pio run --target upload

# Мониторинг:
pio device monitor -p COM_PORT
```

### Через Arduino IDE (альтернатива)
1. Установите поддержку ESP32 в Arduino IDE
2. Установите библиотеку "NimBLE-Arduino" версии 2.3.6
3. Откройте файл `src/main.cpp`
4. Переименуйте в `main.ino`
5. Выберите плату ESP32C3 Dev Module
6. Компилируйте

## 🆕 Новые возможности версии 2.3.6

- **Расширенная реклама**: До 1650 байт данных
- **Улучшения isConnectable()**: Корректная работа проверки подключаемости
- **Исправления в Extended advertisements**: Полная отчетность данных
- **Общие улучшения**: Стабильность и производительность

## ⚠️ Известные проблемы и решения

### 1. Ошибка компиляции константы
```
error: 'BLE_GATT_CHR_PROPS_NOTIFY' was not declared
```
**Решение**: Замените `BLE_GATT_CHR_PROPS_*` на `BLE_GATT_CHR_PROP_*` (без S)

### 2. Ошибка getSubscribedCount()
```  
error: 'class NimBLECharacteristic' has no member named 'getSubscribedCount'
```
**Решение**: Этот метод удален в версии 2.x. Просто вызывайте `notify()` напрямую.

### 3. Ошибки коллбэков
```
error: cannot convert 'ble_gap_conn_desc*' to 'NimBLEConnInfo&'
```
**Решение**: Обновите сигнатуры коллбэков, используйте `NimBLEConnInfo&` вместо `ble_gap_conn_desc*`

## 📋 Проверочный список

- [x] Обновлена версия библиотеки до 2.3.6
- [x] Исправлены константы характеристик
- [x] Обновлены коллбэки серверов и характеристик  
- [x] Исправлены методы advertising
- [x] Убрана проверка getSubscribedCount()
- [x] Обновлен setPower для работы с dBm
- [x] Все GPS функции сохранили совместимость

## 🎯 Результат

Проект полностью совместим с **NimBLE-Arduino 2.3.6** и готов к компиляции. Все основные функции GPS парсинга, UART-BLE моста и дисплеев сохранены без изменений.