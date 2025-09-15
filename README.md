# ESP32-C3 GPS/GNSS to BLE Bridge with Dual Display

Advanced positioning data bridge that receives GPS/GNSS data via UART and transmits it over Bluetooth Low Energy (BLE), with real-time visualization on OLED and TFT displays.

<img width="1920" height="2560" alt="photo_2025-09-13_22-07-39" src="https://github.com/user-attachments/assets/d3c95b29-41e1-4857-b998-03b8f2fd4cc4" />

## Features

### GNSS/GPS Capabilities
- **Multi-constellation support**: GPS, GLONASS, Galileo, BeiDou, QZSS
- **RTK support**: Fixed and Float modes with accuracy monitoring
- **Advanced NMEA 0183 parsing** with modular architecture:
  - **GNS**: Position, altitude, fix quality, total satellite count
  - **GST**: Accuracy metrics (lat/lon/alt standard deviation in meters)
  - **GSA**: Used satellites per system with System ID support for GNGSA
  - **GSV**: Visible satellites per system
- **Satellite tracking**: Separate visible/used counts per GNSS constellation
- **Real-time display**: System breakdown (G:9 R:5 E:3 B:2 Q:1)
- **Fix modes**: GPS, DGPS, High Precision, RTK Fixed, RTK Float, Manual, Simulator

### Display Support
- **OLED I2C Display** (128x64 SSD1306):
  - Coordinates with dynamic precision (6–8 decimals to fit width)
  - Altitude, plus local time HH:MM:SS if it fits the line
  - Accuracy in meters/centimeters; when < 1 m shows centimeters with tenths: `N/S:xx.xcm E/W:yy.ycm`, `H:zz.zcm`
  - **GNSS constellation breakdown** (G:X R:Y E:Z B:W Q:V)
  - Line width ≈ 21 characters (textSize=1)
- **TFT SPI Display** (135x240 ST7789V):
  - Same data with larger font and colors
  - If accuracy line exceeds width (≈20 chars at textSize=2), labels shrink to `NS:`/`EW:` automatically

### BLE Features
- **Nordic UART Service (NUS)** for universal compatibility
- **TX characteristic: NOTIFY + READ** — Notify for streaming, READ as fallback for clients that only perform reads (NimBLE v2.x auto-manages subscriptions)
- **Subscription-aware sending**: data sent only when a client is subscribed; if none, the ring buffer is not drained
- **High-speed path**: UART1 at 460800 baud → ring buffer → BLE (MTU up to 517)
- **Optimized connection**: 7.5–15 ms interval, TX power +9 dBm
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
  - NimBLE-Arduino (v2.3.6+)
  - Adafruit SSD1306 (v2.5.7+)
  - Adafruit GFX Library (v1.11.9+)
  - Arduino_GFX (v1.6.1+ for ST7789V)
  - TinyGPSPlus (v1.0.3+)

## Build and Upload

### Using PlatformIO CLI
```bash
# Build and upload
pio run --target upload

# Just build
pio run

# Monitor serial output
pio device monitor -b 460800
```

### Using VSCode with PlatformIO
1. Open project folder in VSCode
2. Wait for PlatformIO to initialize
3. Click "Upload" button in status bar

## BLE Connection

### Device Name
`ESP32-BLE-UART_1`

### Service UUIDs
- **Service**: `6E400001-B5A3-F393-E0A9-E50E24DCCA9E`
- **RX Characteristic**: `6E400002-B5A3-F393-E0A9-E50E24DCCA9E`
- **TX Characteristic**: `6E400003-B5A3-F393-E0A9-E50E24DCCA9E`

### PIN Code
`123456`

## Testing Apps

- **SW Maps (Android/iOS)** — External GNSS → Generic NMEA (Bluetooth LE). App enables Notify automatically and can send corrections (NTRIP) back over RX.
- **nRF Connect** (Android/iOS) — diagnostics: connect, enable Notify on TX, write to RX.
- Note: Classic SPP apps won’t work; this is BLE GATT (NUS).

## NMEA Data Flow

1. GNSS → UART1 (460800 baud)
2. ESP32-C3 parses messages:
   - **GSV**: visible satellites per system
   - **GSA**: used satellites (incl. GNGSA system mapping)
   - **GNS**: position, fix quality, satellites used
   - **GST**: coordinate accuracy (std dev)
3. Display updates (OLED/TFT) with dynamic precision and cm (tenths)
4. Raw NMEA forwarded over BLE NUS (Notify when subscribed; READ fallback)

## Performance Notes

- Display updates: OLED ≈2 FPS, TFT ≈3 FPS; forced refresh every 30 s
- BLE send conditions: ≥500 bytes ready or 20–50 ms since last send
- UART ring buffer: 2048 bytes; overflow flagged in Serial log
- Subscription-aware: buffer not drained if no clients subscribed
- Time zone: local time shown; auto offset ~ round(longitude/15°), no DST
- Satellite timeout: 10 s; RTK accuracy timeout: 30 s

## Local Time & Time Zones

- What you see: the altitude line can show local time in `HH:MM:SS`.
- Default behavior: the firmware estimates time zone from longitude
  - Offset ≈ round(longitude / 15°) hours, clamped from −12 to +14 hours.
  - Daylight saving time (DST) is not applied.
  - Because political time zones don’t strictly follow longitude, the
    auto‑offset can differ by ±1 hour in some regions.
- Force a specific time zone offset (recommended if auto is wrong):
  - Edit `platformio.ini` in your environment block and add:
    - `build_flags = -DTZ_FORCE_OFFSET_MINUTES=<minutes>`
  - Examples (minutes from UTC):
    - St. Petersburg/Moscow: `180`
    - Berlin (standard time): `60`
    - Tokyo: `540`
    - New York (standard time): `-300` (or `-240` for daylight time)
  - Remove the flag to return to automatic estimation.
- Notes:
  - NMEA time is UTC; the firmware only adjusts display time.
  - No runtime UI yet; if needed, a BLE command/NVS setting can be added.

## Version History

- **v1.2.0** (2025) - NimBLE-Arduino 2.x compatibility update
- **v1.1.x** - Added local time display with timezone support
- **v1.0.x** - Initial release with dual display and RTK support

## License

MIT License - See LICENSE file for details
