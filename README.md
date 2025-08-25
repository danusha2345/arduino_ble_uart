# ESP32-C3 UART to BLE Bridge (Arduino/PlatformIO)

This project turns an **ESP32-C3 Super Mini** microcontroller into a high-speed, secure wireless bridge, forwarding data between its UART (serial) port and a Bluetooth Low Energy (BLE) connection.

This version is built using the **Arduino framework** and the **PlatformIO** build system for maximum stability and ease of use.

---

## Описание на русском

Этот проект превращает микроконтроллер **ESP32-C3 Super Mini** в высокоскоростной и безопасный беспроводной мост, который пересылает данные между его UART-портом и Bluetooth Low Energy (BLE) соединением.

Прошивка настроена на максимальную производительность: используется быстрый UART, увеличенный размер BLE-пакета (MTU) и оптимизированные интервалы соединения. Для защиты используется 6-значный PIN-код.

---

## Features

-   **Wireless Bridge**: Transparently sends data from UART to a connected BLE device and vice-versa.
-   **High Speed**: The UART port is configured to a baud rate of **460800**.
-   **Optimized for Performance**:
    -   Uses a large MTU size (517 bytes) to maximize throughput.
    -   Sets fast connection intervals (7.5ms - 15ms).
    -   Disables Wi-Fi/BLE power-saving for minimum latency.
-   **Standard Service**: Implements the Nordic UART Service (NUS) for broad compatibility.
-   **Secure**: Uses a static 6-digit PIN code **`123456`** for pairing.
-   **Efficient BLE Stack**: Uses the `NimBLE-Arduino` library for lower memory usage and better performance.

## Hardware Requirements

1.  An **ESP32-C3 Super Mini** board (or a similar ESP32-C3 board).
2.  A device to communicate with over UART.

### Pinout

The firmware is configured to use **UART1** for data communication, as UART0 is used for the USB serial monitor.

-   **GPIO5**: UART TX (Transmit from ESP32)
-   **GPIO4**: UART RX (Receive on ESP32)

**Important**: Connect your device's `RX` to the ESP32's `TX` (GPIO5) and your device's `TX` to the ESP32's `RX` (GPIO4). You must also connect the `GND` pins of both devices.

## How to Build and Flash

This project is intended to be built with [PlatformIO](https://platformio.org/).

### Using VSCode with the PlatformIO IDE Extension (Recommended)

1.  **Install Visual Studio Code**.
2.  **Install the PlatformIO IDE extension** from the VSCode marketplace.
3.  **Open the Project**: In VSCode, go to `File > Open Folder...` and select this project's directory.
4.  **Build & Upload**:
    -   Connect your ESP32-C3 board via USB.
    -   Click the **PlatformIO icon** on the left sidebar.
    -   Under `Project Tasks`, find your environment (`esp32-c3-devkitm-1`) and click **Upload**.

    PlatformIO will automatically download the necessary libraries, compile the code, and flash it to your device.

### Using the PlatformIO CLI

1.  **Install the PlatformIO Core (CLI)**.
2.  **Open a terminal** and navigate into the project directory.
3.  **Build & Upload**: Connect your ESP32 board and run the command:
    ```bash
    pio run --target upload
    ```

## How to Use

1.  Power on the ESP32. It will begin advertising as **"ESP32-BLE-UART"**.
2.  On your phone or computer, scan for BLE devices.
3.  Connect to the device. You will be prompted to enter the PIN code: **`123456`**.
4.  Once connected, any data sent to the ESP32 on GPIO4 will be wirelessly transmitted to your phone/computer. Any data you send from the BLE app will be output on GPIO5.

Recommended mobile apps:
-   **nRF Connect for Mobile** (Android/iOS)
-   **Serial Bluetooth Terminal** (Android)