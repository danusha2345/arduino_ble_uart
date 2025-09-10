# ESP32-C3 GPS/GNSS to BLE Bridge with Dual Display

Advanced positioning data bridge that receives GPS/GNSS data via UART and transmits it over Bluetooth Low Energy (BLE), with real-time visualization on OLED and TFT displays.

## Features

### GNSS/GPS Capabilities
- **Multi-constellation support**: GPS, GLONASS, Galileo, BeiDou, QZSS
- **RTK support**: Fixed and Float modes with accuracy monitoring
- **NMEA 0183 parsing**:
  - GNS: Position, altitude, fix quality
  - GST: Accuracy metrics (latitude/longitude/altitude standard deviation)
  - GSA: Satellite details by constellation
- **Fix modes**: GPS, DGPS, PPS, RTK Fixed, RTK Float, Estimated

### Display Support
- **OLED I2C Display** (128x64 SSD1306):
  - Coordinates, altitude, satellites count
  - Fix type and accuracy in meters
  - Constellation breakdown
- **TFT SPI Display** (135x240 ST7789V):
  - Color-coded information
  - Larger, more readable format

### BLE Features
- **Nordic UART Service (NUS)** for universal compatibility
- **High-speed data transfer**: 460800 baud UART
- **Optimized performance**:
  - Large MTU (517 bytes)
  - Fast connection intervals (7.5-15ms)
  - Maximum TX power (+9 dBm)
- **Security**: PIN-based pairing (123456)

## Hardware Requirements

- **ESP32-C3 Super Mini** board
- **GNSS/GPS module** with UART output (460800 baud)
- **Optional displays**:
  - OLED SSD1306 I2C (128x64)
  - TFT ST7789V SPI (135x240)

## Pin Configuration

### UART (GNSS Data)
- **GPIO8**: RX (receive from GNSS)
- **GPIO10**: TX (transmit to GNSS)

### I2C (OLED Display)
- **GPIO3**: SDA
- **GPIO4**: SCL
- **Address**: 0x78 (or 0x3C in 7-bit)

### SPI (TFT Display)
- **GPIO0**: SCLK (Clock)
- **GPIO1**: MOSI (Data)
- **GPIO2**: DC (Data/Command)
- **GPIO9**: RST (Reset)
- **GPIO5**: BL (Backlight)

## Software Requirements

- [PlatformIO](https://platformio.org/) IDE
- Libraries (auto-installed):
  - NimBLE-Arduino
  - Adafruit SSD1306
  - Arduino_GFX (for ST7789V)
  - TinyGPSPlus

## Build and Upload

### Using PlatformIO CLI
```bash
# Build and upload
pio run --target upload

# Just build
pio run

# Monitor serial output
pio device monitor
```

### Using VSCode with PlatformIO
1. Open project folder in VSCode
2. Wait for PlatformIO to initialize
3. Click "Upload" button in status bar

## BLE Connection

### Device Name
`ESP32-BLE-UART`

### Service UUIDs
- **Service**: `6E400001-B5A3-F393-E0A9-E50E24DCCA9E`
- **RX Characteristic**: `6E400002-B5A3-F393-E0A9-E50E24DCCA9E`
- **TX Characteristic**: `6E400003-B5A3-F393-E0A9-E50E24DCCA9E`

### PIN Code
`123456`

## Testing Apps

- **nRF Connect** (Android/iOS) - BLE debugging
- **Serial Bluetooth Terminal** (Android) - Data monitoring
- **Any app supporting Nordic UART Service**

## NMEA Data Flow

1. GNSS module sends NMEA sentences via UART (460800 baud)
2. ESP32-C3 parses GNS, GST, GSA messages
3. Extracted data displayed on OLED/TFT
4. Raw NMEA forwarded over BLE to connected device

## Performance Notes

- Display updates: 1-5 seconds (adaptive)
- BLE latency: <20ms typical
- UART buffer: 512 bytes
- Satellite timeout: 5 seconds per constellation
- RTK accuracy timeout: 30 seconds

## License

MIT License - See LICENSE file for details