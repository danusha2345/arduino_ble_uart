# Changelog

## Latest Release

### ✨ Features
- ✅ Dual board support: ESP32-C3 and ESP32-S3
- ✅ Unified firmware files for single-file flashing
- ✅ Updated to PlatformIO espressif32@6.12.0
- ✅ Arduino Core v2.0.17 with ESP-IDF v4.4.7
- ✅ NimBLE-Arduino v2.3.6
- ✅ Arduino_GFX v1.4.7 (stable version)

### 🔧 Improvements
- Fixed GitHub Actions workflow to use correct environment names
- Updated build scripts to handle both ESP32-C3 and ESP32-S3
- Proper unified firmware generation with correct padding
- Separate info files for each board variant

### 📦 Release Contents
- `unified-esp32c3.bin` - Complete ESP32-C3 firmware
- `unified-esp32s3.bin` - Complete ESP32-S3 firmware
- Individual components for advanced users
- Detailed info files with flashing instructions

### 🐛 Bug Fixes
- Fixed environment name mismatch in GitHub Actions
- Corrected build paths in release scripts
- Fixed Python padding script syntax errors

### 📝 Build Information
- Build System: PlatformIO
- Platform: espressif32@6.12.0
- Framework: Arduino v2.0.17
- Toolchain: GCC 8.4.0
