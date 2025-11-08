# Changelog

## v2.7.0 (2025-10-12) - ESP32-S3-Zero Support & Configuration Fixes

### ✨ New Features
- ✅ **ESP32-S3-Zero full support** with optimized pin layout
- ✅ **Fixed bootloop issue** on ESP32-S3 with corrected platformio.ini
- ✅ **Separate network identifiers** for S3 and C3 devices
  - WiFi AP: `UM980_GPS_BRIDGE_S3` (S3) / `UM980_GPS_BRIDGE` (C3)
  - BLE Device: `UM980_S3_GPS` (S3) / `UM980_C3_GPS` (C3)
- ✅ **Dual-core architecture** on ESP32-S3 (BLE on core 0, data on core 1)
- ✅ **Standard Nordic UART Service UUIDs** for maximum app compatibility
- ✅ **Conditional compilation** with `#ifdef ESP32_S3` for universal codebase
- ✅ **TFT_eSPI library** for ESP32-S3, Arduino_GFX for ESP32-C3

### 🔧 ESP32-S3-Zero Configuration
- **OLED I2C**: GPIO1 (SCL), GPIO2 (SDA) - ✅ Working
- **GPS UART1**: GPIO5 (RX), GPIO6 (TX) - ✅ Working
- **TFT SPI**: GPIO9-13 - ⏸️ Temporarily disabled (requires SPI setup)
- **LED**: GPIO21 (WS2812 RGB built-in)
- **USB Debug**: GPIO43/44 (built-in USB CDC)
- **Memory**: RAM 26.9% (88KB/327KB), Flash 76.7% (1005KB/1310KB)

### 📚 Documentation
- ✅ Added `ESP32-S3-ZERO_PIN_CONNECTIONS.md` - Complete pin mapping guide
- ✅ Added `PIN_WIRING_DIAGRAM.txt` - Visual wiring diagrams
- ✅ Updated README.md with S3 configuration details
- ✅ Documented reserved pins and free GPIO list

### 🐛 Bug Fixes
- Fixed ESP32-S3 bootloop caused by incorrect PSRAM flags
- Removed problematic `-mfix-esp32-psram-cache-issue` flag
- Added proper `board_build.partitions = default.csv`
- Fixed task flag initialization (bleTaskRunning/dataTaskRunning)
- Protected TFT access with conditional compilation guards

### ✅ Verification
- ESP32-C3: RAM 24.8%, Flash 78.5% - ✅ Tested
- ESP32-S3: RAM 26.9%, Flash 76.7% - ✅ Tested
- Both platforms compile and run successfully
- BLE connectivity verified on both platforms
- Network names properly differentiated

### 📝 Build Information
- Build System: PlatformIO
- Platform: espressif32@6.12.0
- Framework: Arduino v2.0.17 (ESP-IDF v4.4.7)
- NimBLE-Arduino: v2.3.6
- Arduino_GFX: v1.4.7 (for ESP32-C3)
- TFT_eSPI: v2.5.0+ (for ESP32-S3)

---

## v2.6.0 - Previous Release

### Features
- Dual board support: ESP32-C3 and ESP32-S3
- Unified firmware files for single-file flashing
- Updated to PlatformIO espressif32@6.12.0
