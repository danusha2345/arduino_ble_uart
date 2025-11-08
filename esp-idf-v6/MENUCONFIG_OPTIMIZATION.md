# Оптимальные настройки menuconfig для Nordic UART Service

## Быстрая настройка

```bash
cd /home/user/arduino_ble_uart/esp-idf-v6
idf.py menuconfig
```

---

## 1. Bluetooth Controller Mode

**Путь:** `Component config → Bluetooth → Bluetooth controller mode`

**Настройка:**
```
[*] Bluetooth LE only
[ ] Bluetooth Classic (отключить для экономии памяти)
```

**Зачем:** Освобождает память от Classic Bluetooth, который не используется.

---

## 2. Bluetooth Host

**Путь:** `Component config → Bluetooth → Bluetooth host`

**Настройка:**
```
[*] NimBLE - Host from Apache Mynewt project
```

**Зачем:** NimBLE легче и быстрее чем Bluedroid.

---

## 3. NimBLE Options (КРИТИЧНЫЕ НАСТРОЙКИ)

**Путь:** `Component config → Bluetooth → NimBLE Options`

### 3.1. Основные опции
```
[*] Enable NimBLE host
[*] Enable BLE role peripheral (ОБЯЗАТЕЛЬНО для сервера)
[ ] Enable BLE role central (отключить если не нужен)
```

### 3.2. Соединения
```
Maximum number of connections: 4
```
**Зачем:** Поддержка до 4 одновременных клиентов (можно уменьшить до 1 для экономии памяти).

### 3.3. MTU и payload (ПРОИЗВОДИТЕЛЬНОСТЬ!)
```
Maximum ATT MTU size: 512
Maximum ACL payload size: 251
```
**Зачем:**
- MTU 512 позволяет передавать больше данных за пакет
- ACL 251 - максимум для BLE 5.0
- ✅ Ваш код уже использует BLE_MTU=517 (config.h)

### 3.4. PHY Support (СКОРОСТЬ!)
```
[*] Enable 2M PHY support
[*] Enable Coded PHY support (опционально)
```
**Зачем:**
- 2M PHY удваивает скорость передачи
- ✅ Ваш код уже включает 2M PHY (строка 413 в ble_service.c)

### 3.5. Буферы (СТАБИЛЬНОСТЬ!)
```
Number of buffers for ATT server: 16
Number of buffers for ATT client: 16
Number of buffers for L2CAP signaling: 10
Number of buffers for L2CAP LE CoC: 20
```
**Зачем:** Больше буферов = меньше потерь при высокой нагрузке.

### 3.6. GATT Services
```
Maximum number of GATT services: 10
Maximum number of GATT characteristics: 20
```
**Зачем:** Достаточно для Nordic UART + другие сервисы.

### 3.7. Security (для Bonding/Pairing)
```
[*] Enable SM (Security Manager)
[*] Support bonding
[*] Store bonding keys in NVS
```
**Зачем:**
- Поддержка спаривания с PIN кодом
- ✅ Ваш код уже настроен (sm_bonding=1, sm_mitm=1)

---

## 4. ESP System Settings (ПАМЯТЬ)

**Путь:** `Component config → ESP System Settings`

### 4.1. Task stack sizes
```
Main task stack size: 8192
Minimum free heap size for initialization: 20000
```
**Зачем:** NimBLE требует больше стека.

### 4.2. Event loop
```
Event loop task stack size: 4096
```

---

## 5. FreeRTOS Settings (ПРОИЗВОДИТЕЛЬНОСТЬ)

**Путь:** `Component config → FreeRTOS`

### 5.1. Kernel
```
Tick rate (Hz): 1000
```
**Зачем:** Более точная синхронизация для BLE таймингов.

### 5.2. Task priorities
```
Idle task priority: 0
Timer task priority: 1
```

---

## 6. LVGL Settings (если используется дисплей)

**Путь:** `Component config → LVGL configuration`

```
Color depth: 16 bits
Buffer size: 10% of screen
```
**Зачем:** Оптимизация для ST7789 дисплея 240x280.

---

## 7. Partition Table (ПАМЯТЬ)

**Путь:** `Partition Table`

**Рекомендация:**
```
Partition Table: Custom partition table CSV
Custom partition CSV file: partitions.csv
```

**Создать файл `partitions.csv`:**
```csv
# Name,   Type, SubType, Offset,  Size, Flags
nvs,      data, nvs,     0x9000,  0x6000,
phy_init, data, phy,     0xf000,  0x1000,
factory,  app,  factory, 0x10000, 1M,
```
**Зачем:** Больше места для NVS (bonding ключи).

---

## 8. Compiler Optimization (СКОРОСТЬ vs ОТЛАДКА)

**Путь:** `Compiler options`

### Для РАЗРАБОТКИ:
```
Optimization Level: Debug (-Og)
Assertion level: Enable
```

### Для PRODUCTION:
```
Optimization Level: Optimize for performance (-O2)
Assertion level: Silent (saves code size)
```

---

## 9. Log Output (ОТЛАДКА)

**Путь:** `Component config → Log output`

### Для РАЗРАБОТКИ:
```
Default log verbosity: Info
[*] Use ANSI terminal colors in log output
```

### Для PRODUCTION:
```
Default log verbosity: Warning
[ ] Use ANSI terminal colors (экономит ROM)
```

---

## 10. Bluetooth Debug Logs (ДИАГНОСТИКА)

**Путь:** `Component config → Bluetooth → NimBLE Options → Log Level`

### Для РАЗРАБОТКИ:
```
NimBLE default log verbosity: Debug
```

### Для PRODUCTION:
```
NimBLE default log verbosity: Warning
```

---

## Проверка текущих настроек

После настройки menuconfig, проверьте `sdkconfig`:

```bash
grep -E "CONFIG_BT_NIMBLE|CONFIG_BT_LE_|CONFIG_BT_CTRL" sdkconfig
```

**Должны быть:**
```
CONFIG_BT_NIMBLE_ENABLED=y
CONFIG_BT_NIMBLE_ROLE_PERIPHERAL=y
CONFIG_BT_NIMBLE_MAX_CONNECTIONS=4
CONFIG_BT_NIMBLE_ATT_PREFERRED_MTU=512
CONFIG_BT_NIMBLE_ACL_BUF_SIZE=251
CONFIG_BT_NIMBLE_2M_PHY=y
CONFIG_BT_NIMBLE_SM_BONDING=y
```

---

## Сборка после настройки

```bash
idf.py fullclean
idf.py build
idf.py -p /dev/ttyUSB0 flash
```

---

## Проверка производительности

После прошивки проверьте логи:

```bash
idf.py -p /dev/ttyUSB0 monitor
```

**Ожидаемые логи:**
```
I (xxx) BLE: 2M PHY enabled for maximum throughput
I (xxx) BLE: Preferred MTU set to 517 bytes
I (xxx) BLE: MTU UPDATED: 517 bytes (payload: 514 bytes)
I (xxx) BLE: Connection params: interval=6 (7.5ms), latency=0, timeout=500
```

**Теоретическая пропускная способность:**
- **2M PHY + MTU 517**: ~68 kbps (68 килобайт/сек)
- **1M PHY + MTU 23**: ~1 kbps (1 килобайт/сек)

**Ваша конфигурация даёт ~68x ускорение!** 🚀

---

## Возможные проблемы

### Проблема 1: "Not enough memory"

**Решение:** Уменьшите количество буферов:
```
Number of buffers for ATT server: 8
Number of buffers for L2CAP LE CoC: 10
```

### Проблема 2: "MTU negotiation failed"

**Решение:** Убедитесь что клиент поддерживает большой MTU:
- iOS: всегда поддерживает до 512
- Android: зависит от версии (4.3+ обычно 512)

### Проблема 3: "Connection timeout"

**Решение:** Проверьте параметры соединения:
```c
// В вашем коде можно добавить:
struct ble_gap_upd_params params = {
    .itvl_min = 6,   // 7.5ms
    .itvl_max = 12,  // 15ms
    .latency = 0,
    .supervision_timeout = 500,
};
ble_gap_update_params(conn_handle, &params);
```

---

## Итог

Эта конфигурация обеспечивает:
- ✅ Максимальную пропускную способность (68 kbps)
- ✅ Минимальную латентность (7.5ms)
- ✅ Совместимость с iOS/Android
- ✅ Стабильную работу при высокой нагрузке
- ✅ Поддержку bonding/pairing
- ✅ Эффективное использование памяти
