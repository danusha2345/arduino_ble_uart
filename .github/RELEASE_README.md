# GPS/GNSS to BLE Bridge - Release Files

## 🚀 Quick Start

### ESP32-C3 (Single-Core)
```bash
esptool.py write_flash 0x0 unified-esp32c3.bin
```

### ESP32-S3 (Dual-Core)
```bash
esptool.py write_flash 0x0 unified-esp32s3.bin
```

## 📦 Files Included

### Unified Firmware (Recommended)
- **unified-esp32c3.bin** - Complete ESP32-C3 firmware (~1.1MB)
- **unified-esp32s3.bin** - Complete ESP32-S3 firmware (~1.1MB)

These files include bootloader, partition table, and application firmware with proper offsets.

### Individual Components (Advanced)
If you need to flash components separately:

**ESP32-C3:**
- `bootloader-esp32c3.bin` @ 0x0000
- `partitions-esp32c3.bin` @ 0x8000
- `firmware-esp32c3.bin` @ 0x10000

**ESP32-S3:**
- `bootloader-esp32s3.bin` @ 0x0000
- `partitions-esp32s3.bin` @ 0x8000
- `firmware-esp32s3.bin` @ 0x10000

### Information Files
- `esp32c3-info.txt` - Detailed ESP32-C3 firmware information
- `esp32s3-info.txt` - Detailed ESP32-S3 firmware information

## 🔧 Installation Methods

### Method 1: Web Flasher (Easiest)
Use [ESP Web Tools](https://esp.huhn.me/) in Chrome/Edge browser

### Method 2: esptool.py
```bash
# Install esptool
pip install esptool

# Flash unified firmware
esptool.py --port /dev/ttyUSB0 write_flash 0x0 unified-esp32XX.bin

# Or flash components separately
esptool.py --port /dev/ttyUSB0 write_flash \
  0x0 bootloader-esp32XX.bin \
  0x8000 partitions-esp32XX.bin \
  0x10000 firmware-esp32XX.bin
```

### Method 3: PlatformIO
```bash
# Clone repository
git clone https://github.com/your-repo/arduino_ble_uart.git
cd arduino_ble_uart

# Flash ESP32-C3
pio run -e esp32-c3 -t upload

# Flash ESP32-S3
pio run -e esp32-s3 -t upload
```

## 📋 Platform Information

- **PlatformIO Platform**: espressif32@6.12.0
- **Arduino Core**: v2.0.17
- **ESP-IDF**: v4.4.7
- **NimBLE-Arduino**: v2.3.6
- **Arduino_GFX**: v1.4.7

## 🛠️ Troubleshooting

### Flash Issues
If flashing fails, try:
```bash
# Erase flash first
esptool.py --port /dev/ttyUSB0 erase_flash

# Then flash again
esptool.py --port /dev/ttyUSB0 write_flash 0x0 unified-esp32XX.bin
```

### Serial Port Not Found
- **Linux**: Check `/dev/ttyUSB*` or `/dev/ttyACM*`
- **macOS**: Check `/dev/cu.usbserial-*`
- **Windows**: Check Device Manager for COM port

### Permission Denied (Linux)
```bash
sudo usermod -a -G dialout $USER
# Then logout and login again
```

## 📖 Full Documentation
See the [main repository](https://github.com/your-repo/arduino_ble_uart) for complete documentation.
