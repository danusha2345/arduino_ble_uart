#!/bin/bash

# Быстрое исправление UUID проблемы
# Проблема: Bonding кэш хранит старый GATT

echo "=========================================="
echo "Исправление BLE UUID проблемы"
echo "=========================================="

cd /home/user/arduino_ble_uart/esp-idf-v6

# Шаг 1: Стереть ТОЛЬКО NVS partition (где хранится bonding)
echo ""
echo "Шаг 1: Стираем NVS partition (bonding ключи)..."
idf.py erase-flash

# Шаг 2: Прошить заново
echo ""
echo "Шаг 2: Прошиваем устройство заново..."
idf.py -p /dev/ttyUSB0 flash

echo ""
echo "=========================================="
echo "✅ Готово!"
echo "=========================================="
echo ""
echo "Теперь на телефоне:"
echo "1. Settings → Bluetooth → Забудь 'UM980_C6_GPS'"
echo "2. Settings → Apps → nRF Connect → Clear Data"
echo "3. Перезагрузи телефон"
echo "4. Сканируй заново в nRF Connect"
echo ""
echo "UUID должен быть: 6E400001-B5A3-F393-E0A9-E50E24DCCA9E ✅"
echo "=========================================="
