# ESP32-C3/ESP32-S3 GPS/GNSS to BLE Bridge with Dual Display

Advanced **bidirectional** positioning data bridge that receives GPS/GNSS data via UART and transmits it over Bluetooth Low Energy (BLE), with real-time visualization on OLED and TFT displays. **Now supports sending commands to GNSS modules via Bluetooth terminal on your phone! Universal compatibility for both ESP32-C3 and ESP32-S3 boards.**


<img width="1920" height="2560" alt="photo_2025-09-15_14-08-49" src="https://github.com/user-attachments/assets/6384bac2-2426-4ae1-ac8b-758c5f1b6196" />

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
- **Real-time display**: System breakdown with smart formatting:
  - Auto-switches to compact format when string exceeds 20 characters
  - Normal: `G:9 R:5 E:3 B:2 Q:1` (with spaces for readability)
  - Compact: `G:12R:8E:10B:5Q:1` (no spaces to fit display)
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
- **🔄 Bidirectional communication**: Send commands to GNSS module via Bluetooth terminal
- **Nordic UART Service (NUS)** for universal compatibility with NimBLE-Arduino 2.3.6
- **TX characteristic**: NOTIFY + READ (receives GNSS data from module)
- **RX characteristic**: WRITE + WRITE_NO_RSP (sends commands to module)
- **Subscription-aware sending**: data sent only when a client is subscribed
- **High-speed path**: UART1 at 460800 baud ↔ ring buffer ↔ BLE (MTU up to 517)
- **Subscription-aware sending**: data sent only when a client is subscribed
- **High-speed path**: UART1 at 460800 baud ↔ ring buffer ↔ BLE (MTU up to 517)
- **Optimized connection**: 7.5–15 ms interval, TX power +9 dBm
- **No security**: Direct connection without pairing for easy access

## Hardware Requirements

- **ESP32-C3 Super Mini** OR **ESP32-S3** board
- **GNSS/GPS module** with UART output (460800 baud)
- **Optional displays**:
  - OLED SSD1306 I2C (128x64)
  - TFT ST7789V SPI (135x240)

## Supported Boards

### ESP32-C3 (Original Configuration)
- Uses Arduino_GFX library for TFT display
- Original pin layout maintained
- Single-core operation

### ESP32-S3 (Enhanced Configuration) 
- Uses TFT_eSPI library for better compatibility
- Dual-core operation (BLE on core 0, data processing on core 1)
- Improved performance with multi-threading
- Addressable LED support on GP21
- Optimized pin layout for boards with GP1-GP18, GP38-GP45 pin ranges

## Pin Configuration

### ESP32-C3 Pin Configuration

#### UART (GNSS Data)
- **GPIO8**: RX (receive from GNSS)
- **GPIO10**: TX (transmit to GNSS)

#### I2C (OLED Display)
- **GPIO3**: SDA
- **GPIO4**: SCL
- **Address**: 0x78 (or 0x3C in 7-bit)

#### SPI (TFT Display)
- **GPIO0**: SCLK (Clock)
- **GPIO1**: MOSI (Data)
- **GPIO2**: DC (Data/Command)
- **GPIO9**: RST (Reset)
- **GPIO5**: BL (Backlight)

### ESP32-S3 Pin Configuration (New Layout)

#### UART (GNSS Data)
- **GP5**: RX (receive from GNSS)
- **GP6**: TX (transmit to GNSS)

#### I2C (OLED Display)
- **GP7**: SDA
- **GP8**: SCL
- **Address**: 0x78 (or 0x3C in 7-bit)

#### SPI (TFT Display) - Sequential from GP9
- **GP9**: RST (Reset)
- **GP10**: DC (Data/Command)
- **GP11**: MOSI (Data)
- **GP12**: SCLK (Clock)
- **GP13**: BL (Backlight)
- **CS**: Not used (-1)

#### Addressable LED
- **GP21**: Addressable LED control

## Build and Upload

### For ESP32-C3
```bash
# Build and upload
pio run --environment esp32dev --target upload

# Just build
pio run --environment esp32dev

# Monitor serial output
pio device monitor -b 460800
```

### For ESP32-S3
```bash
# Build and upload
pio run --environment esp32s3 --target upload

# Just build
pio run --environment esp32s3

# Monitor serial output
pio device monitor -b 460800
```

## Software Requirements

- [PlatformIO](https://platformio.org/) IDE
- Libraries (auto-installed per board):

### ESP32-C3 Libraries
- NimBLE-Arduino (v2.3.6+)
- Adafruit SSD1306 (v2.5.7+)
- Adafruit GFX Library (v1.11.9+)
- **Arduino_GFX (v1.6.1+ for ST7789V)**
- TinyGPSPlus (v1.0.3+)

### ESP32-S3 Libraries
- NimBLE-Arduino (v2.3.6+)
- Adafruit SSD1306 (v2.5.7+)
- Adafruit GFX Library (v1.11.9+)
- **TFT_eSPI (v2.5.0+ for ST7789V)**
- TinyGPSPlus (v1.0.3+)

## Performance Comparison

### ESP32-C3 (Single-Core)
- **Processor**: RISC-V single-core 160MHz
- **RAM**: 400KB SRAM
- **Flash**: 4MB
- **Operation**: Single-threaded processing
- **Display Library**: Arduino_GFX
- **Performance**: Good for basic GNSS bridging

### ESP32-S3 (Dual-Core Enhanced)
- **Processor**: Xtensa dual-core 240MHz
- **RAM**: 512KB SRAM + optional PSRAM
- **Flash**: 8MB+
- **Operation**: Multi-threaded (BLE on core 0, data on core 1)
- **Display Library**: TFT_eSPI (optimized)
- **Performance**: Superior for high-rate GNSS + RTK + multiple clients
- **Features**: 
  - Improved throughput with dual-core architecture
  - Better handling of high-speed NTRIP corrections
  - Addressable LED support
  - Enhanced ring buffer management

### Using VSCode with PlatformIO
1. Open project folder in VSCode
2. Wait for PlatformIO to initialize
3. Select target environment (esp32dev or esp32s3)
4. Click "Upload" button in status bar

## BLE Connection
`um980_2`
### Device Name
`um980_2`

### Service UUIDs
- **Service**: `6E400001-B5A3-F393-E0A9-E50E24DCCA9E`
- **RX Characteristic**: `6E400002-B5A3-F393-E0A9-E50E24DCCA9E`
- **TX Characteristic**: `6E400003-B5A3-F393-E0A9-E50E24DCCA9E`

### PIN Code
`123456`

## WiFi Connection
**New Feature**: The device also supports WiFi connectivity for direct access to the GNSS module via TCP port 23.

### WiFi Access Point Settings
- **SSID**: `UM980_GPS_BRIDGE`
- **Password**: `123456789`
- **IP Address**: `192.168.4.1`
- **Port**: `23` (telnet-like access)

### Connecting via WiFi
1. Connect your device (phone/computer) to the WiFi network `UM980_GPS_BRIDGE` using password `123456789`
2. Use any TCP client or telnet application to connect to `192.168.4.1:23`
3. Send commands directly to the GNSS module and receive responses in real-time
4. Commands sent via WiFi are forwarded to the GNSS module just like BLE commands
5. GNSS data is forwarded to all connected WiFi clients simultaneously with BLE clients

### WiFi Performance
- Supports up to 4 concurrent WiFi clients
- Low latency communication with `setNoDelay(true)` setting
- Bidirectional communication like BLE connection

## Using Bidirectional Communication

### Send Commands to GNSS Module
1. Connect to `um980_2` device via Bluetooth
2. Use any BLE terminal app (Serial Bluetooth Terminal, nRF Connect, etc.)
3. Send commands directly to GNSS module:
   ```
   CONFIG POS 55.7558 37.6176 123.45
   SAVECONFIG
   ```
4. Receive responses immediately in the same terminal

### Recommended Apps
- **Serial Bluetooth Terminal (Android/iOS)** — Best for command sending, auto-connects to Nordic UART Service
- **SW Maps (Android/iOS)** — External GNSS → Generic NMEA (Bluetooth LE). Great for NTRIP corrections and positioning
- **nRF Connect (Android/iOS)** — Advanced diagnostics: connect, enable Notify on TX, write to RX
- Note: Classic SPP apps won't work; this is BLE GATT (NUS)
1. Connect to `um980_2` device via Bluetooth
2. Use any BLE terminal app (Serial Bluetooth Terminal, nRF Connect, etc.)
3. Send commands directly to GNSS module:
### Bidirectional Communication
1. **Phone → ESP32 → GNSS**: Commands sent via BLE RX characteristic go directly to UART1 TX
2. **GNSS → ESP32 → Phone**: NMEA data from UART1 RX goes to BLE TX characteristic

### Data Processing
1. GNSS ↔ UART1 (460800 baud, bidirectional)
2. ESP32-C3 parses incoming NMEA messages:
   SAVECONFIG
   ```
4. Receive responses immediately in the same terminal

### Recommended Apps
4. Raw NMEA data forwarded over BLE NUS (Notify when subscribed; READ fallback)
- **SW Maps (Android/iOS)** — External GNSS → Generic NMEA (Bluetooth LE). Great for NTRIP corrections and positioning
- **nRF Connect (Android/iOS)** — Advanced diagnostics: connect, enable Notify on TX, write to RX
- Note: Classic SPP apps won't work; this is BLE GATT (NUS)

## NMEA Data Flow

### Bidirectional Communication
1. **Phone → ESP32 → GNSS**: Commands sent via BLE RX characteristic go directly to UART1 TX
2. **GNSS → ESP32 → Phone**: NMEA data from UART1 RX goes to BLE TX characteristic

### Data Processing
1. GNSS ↔ UART1 (460800 baud, bidirectional)
2. ESP32-C3 parses incoming NMEA messages:
   - **GSV**: visible satellites per system
   - **GSA**: used satellites (incl. GNGSA system mapping)
   - **GNS**: position, fix quality, satellites used
   - **GST**: coordinate accuracy (std dev)
3. Display updates (OLED/TFT) with dynamic precision and cm (tenths)
4. Raw NMEA data forwarded over BLE NUS (Notify when subscribed; READ fallback)

## Performance Notes

- Display updates: OLED ≈2 FPS, TFT ≈3 FPS; forced refresh every 30 s
- BLE send conditions: ≥500 bytes ready or 20–50 ms since last send
- UART ring buffer: 2048 bytes; overflow flagged in Serial log
- Subscription-aware: buffer not drained if no clients subscribed
- Time zone: local time shown; auto offset ~ round(longitude/15°), no DST
- Satellite timeout: 10 s; RTK accuracy timeout: 30 s

## Local Time & Time Zones

- **Time source**: UTC time extracted from NMEA GGA sentences (GPS time)
- What you see: the altitude line can show local time in `HH:MM:SS`.
- Default behavior: the firmware estimates time zone from longitude
  - Offset ≈ round(longitude / 15°) hours, clamped from −12 to +14 hours.
  - Daylight saving time (DST) is not applied.
  - Because political time zones don't strictly follow longitude, the
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

- **v2.5.0** (2025-10-10) - Universal ESP32-C3/ESP32-S3 support!
  - Added ESP32-S3 dual-core architecture support
  - ESP32-S3 uses TFT_eSPI library for better compatibility
  - ESP32-C3 maintains Arduino_GFX library compatibility
  - Optimized pin layout for ESP32-S3 boards (GP1-GP18, GP38-GP45)
  - ESP32-S3: UART on GP5-GP6, sequential pins from GP7
  - ESP32-S3: Addressable LED support on GP21
  - Multi-threaded operation on ESP32-S3 (BLE on core 0, data on core 1)
  - Conditional compilation for universal codebase
- **v2.0.0** (2025-01-20) - Full bidirectional Bluetooth communication! Send commands to GNSS module via phone
  - Fixed NimBLE-Arduino 2.x callback signatures
  - Resolved "multiple write characteristics" issue
  - Added bidirectional UART-BLE bridge
  - Device renamed to `um980_2` for UM980 module support
- **v1.2.0** (2025) - NimBLE-Arduino 2.x compatibility update
- **v1.1.x** - Added local time display with timezone support
- **v1.0.x** - Initial release with dual display and RTK support

<img width="1920" height="2560" alt="photo_2025-09-13_22-07-39" src="https://github.com/user-attachments/assets/d3c95b29-41e1-4857-b998-03b8f2fd4cc4" />

## License

MIT License - See LICENSE file for details
