# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Advanced ESP32-C3 GPS/GNSS to BLE bridge with real-time positioning data transmission and dual display support. The firmware creates a transparent wireless bridge between GNSS receiver and BLE Nordic UART Service (NUS), with comprehensive NMEA parsing and accuracy monitoring for RTK applications.

## Key Architecture Components

### Hardware Configuration
- **MCU**: ESP32-C3 Super Mini (configured as `esp32dev` with `esp32-c3-devkitm-1` board)
- **BLE Stack**: NimBLE-Arduino library for efficient memory usage
- **UART Configuration**: 
  - UART0: USB serial monitor/debug (115200 baud)
  - UART1: GNSS data (RX: GPIO8, TX: GPIO10, 460800 baud)

### Display Support
- **OLED I2C**: 128x64 SSD1306 (SDA: GPIO3, SCL: GPIO4, Address: 0x78)
- **TFT SPI**: 135x240 ST7789V (SCLK: GPIO0, MOSI: GPIO1, DC: GPIO2, RST: GPIO9, BL: GPIO5)

### GNSS Features
- **Multi-constellation support**: GPS, GLONASS, Galileo, BeiDou, QZSS
- **RTK support**: Fixed/Float modes with extended accuracy timeout
- **NMEA parsing**: GNS (position), GST (accuracy), GSA (satellites)
- **Accuracy tracking**: Latitude/Longitude/Altitude standard deviation in meters

### BLE Configuration
- **Service**: Nordic UART Service (NUS)
- **Security**: PIN pairing (123456)
- **Performance**: MTU 517 bytes, connection interval 7.5-15ms, TX power +9 dBm

## Common Development Commands

### Build and Upload
```bash
pio run --target upload
```

### Build Only
```bash
pio run
```

### Clean Build
```bash
pio run --target clean
pio run
```

### Monitor Serial Output
```bash
pio device monitor
```

### Build for Release
```bash
pio run --environment esp32dev
```

### Project Structure Commands
```bash
pio project init --board esp32-c3-devkitm-1 --project-option "framework=arduino"
```

## Hardware Configuration

The project uses specific GPIO pins for UART1:
- **GPIO8**: RX (receive data from external device)
- **GPIO10**: TX (transmit data to external device)

**Note**: README.md incorrectly mentions GPIO4/GPIO5 - the actual implementation uses GPIO8/GPIO10 as defined in src/main.cpp:57.

## BLE Service Implementation

The firmware implements Nordic UART Service (NUS) with standard UUIDs:
- Service: `6E400001-B5A3-F393-E0A9-E50E24DCCA9E`
- RX Characteristic: `6E400002-B5A3-F393-E0A9-E50E24DCCA9E` (write from client)
- TX Characteristic: `6E400003-B5A3-F393-E0A9-E50E24DCCA9E` (notify to client)

## Dependencies

Key library dependency defined in platformio.ini:
- `h2zero/NimBLE-Arduino@^1.4.1`

## CI/CD Integration

GitHub Actions workflow (`.github/workflows/release.yml`) automatically builds and releases firmware binaries on version tags (`v*`). The build produces three artifacts:
- firmware.bin
- partitions.bin  
- bootloader.bin

## Development Notes

- The project uses a single source file `src/main.cpp` with embedded callback classes
- PIN code is hardcoded to `123456` for pairing
- Upload port is configured for `/dev/ttyACM0` (Linux)
- Monitor speed matches UART speed: 115200 baud
- Device advertises as "ESP32-BLE-UART"

## Testing and Debugging

Recommended BLE client apps for testing:
- nRF Connect for Mobile (Android/iOS)
- Serial Bluetooth Terminal (Android)

Serial debugging output is available through USB at 460800 baud showing connection status and BLE events.