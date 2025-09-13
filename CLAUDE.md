# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Advanced ESP32-C3 GPS/GNSS to BLE bridge with real-time positioning data transmission and dual display support. The firmware creates a transparent wireless bridge between GNSS receiver and BLE Nordic UART Service (NUS), with comprehensive NMEA parsing and accuracy monitoring for RTK applications.

## Key Architecture Components

### Hardware Configuration
- **MCU**: ESP32-C3 Super Mini (configured as `esp32dev` with `esp32-c3-devkitm-1` board)
- **BLE Stack**: NimBLE-Arduino library for efficient memory usage
- **UART Configuration**: 
  - UART0: USB serial monitor/debug (460800 baud)
  - UART1: GNSS data (RX: GPIO8, TX: GPIO10, 460800 baud)

### Display Support
- **OLED I2C**: 128x64 SSD1306 (SDA: GPIO3, SCL: GPIO4, Address: 0x78)
- **TFT SPI**: 135x240 ST7789V (SCLK: GPIO0, MOSI: GPIO1, DC: GPIO2, RST: GPIO9, BL: GPIO5)

### GNSS Features
- **Multi-constellation support**: GPS, GLONASS, Galileo, BeiDou, QZSS
- **RTK support**: Fixed/Float modes with extended accuracy timeout
- **Advanced NMEA parsing**: Modular architecture with specialized parsers
  - GNS: Position, fix quality, total satellite count
  - GST: Coordinate accuracy (lat/lon/alt standard deviation in meters)
  - GSA: Used satellites per system with System ID support for GNGSA
  - GSV: Visible satellites per system
- **Satellite tracking**: Separate visible/used satellite counts per GNSS system
- **System breakdown display**: Real-time satellite count by system (G:X R:Y E:Z B:W Q:V)

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

**Note**: UART1 uses GPIO8/GPIO10 as defined in `src/main.cpp`.

## BLE Service Implementation

The firmware implements Nordic UART Service (NUS) with standard UUIDs:
- Service: `6E400001-B5A3-F393-E0A9-E50E24DCCA9E`
- RX Characteristic: `6E400002-B5A3-F393-E0A9-E50E24DCCA9E` (write from client)
- TX Characteristic: `6E400003-B5A3-F393-E0A9-E50E24DCCA9E` (notify to client; also `READ` fallback via `onRead` pulling from ring buffer up to MTU-3)

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
- Monitor speed: 460800 baud
- Device advertises as "ESP32-BLE-UART_2"
- TX supports `NOTIFY` + `READ`; notifications only when subscribed (subscription-aware); ring buffer (2048 B) is not drained without subscribers
- Display lines: OLED ≈21 chars (size 1), TFT ≈20 chars (size 2); Lat/Lon precision 6–8 decimals auto-fit; accuracy <1 m shown in cm with tenths; TFT accuracy labels shrink to `NS:`/`EW:` if needed
- Altitude line adds local time `HH:MM:SS` if it fits; local time offset estimated from longitude (round(lon/15), clamped −12..+14), DST not applied

## NMEA Parser Architecture

The firmware implements a modular NMEA parsing system with specialized parsers:

### Core Data Structures
```cpp
struct SatInfo {
    int visible = 0;   // Visible satellites (from GSV)
    int used = 0;      // Used satellites (from GSA)
    unsigned long lastUpdate = 0;
};

struct {
    SatInfo gps, glonass, galileo, beidou, qzss;
} satData;
```

### Parser Functions
- `parseNMEA()`: Universal dispatcher for NMEA messages
- `parseGSV()`: Parses visible satellites by system (GP/GL/GA/GB/GQ + GSV)
- `parseGSA()`: Parses used satellites with System ID support for GNGSA messages
- `parseGST()`: Parses coordinate accuracy data (standard deviations)
- `parseGNS()`: Parses position, fix quality, and total satellite count
- `splitFields()`: Efficient NMEA field separator (replaces strtok_r)
- `checkSatelliteTimeouts()`: Resets stale satellite data (10-second timeout)

### System ID Mapping (GNGSA messages)
- 1: GPS
- 2: GLONASS  
- 3: Galileo
- 4: BeiDou
- 5: QZSS

### Display Format
Satellite counts shown as: `G:9 R:5 E:3 B:2 Q:1` (used satellites per system)

## Testing and Debugging

Recommended BLE client apps for testing:
- SW Maps (Android/iOS): External GNSS → Generic NMEA (Bluetooth LE)
- nRF Connect for Mobile (Android/iOS) for diagnostics

Serial debugging output is available through USB at 460800 baud showing connection status and BLE events.
