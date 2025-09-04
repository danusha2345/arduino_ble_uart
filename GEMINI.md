# Project: ESP32-C3 UART to BLE Bridge

## Project Overview

This project transforms an ESP32-C3 microcontroller into a high-speed, secure wireless bridge. It forwards data between its UART (serial) port and a Bluetooth Low Energy (BLE) connection. The project is built using the Arduino framework and the PlatformIO build system.

The main application is in `src/main.cpp`. It uses the `NimBLE-Arduino` library to create a BLE server that implements the Nordic UART Service (NUS) for broad compatibility. The BLE connection is secured with a 6-digit PIN code.

The project is configured to use UART1 for data communication, as UART0 is used for the USB serial monitor. The pinout is:
-   **GPIO5**: UART TX
-   **GPIO4**: UART RX

The `platformio.ini` file defines multiple environments, suggesting that this repository contains not just the main application, but also a collection of related firmware, tests, and examples for both ESP32 and ESP8266 platforms.

## Building and Running

This project is intended to be built with PlatformIO.

### Using VSCode with the PlatformIO IDE Extension (Recommended)

1.  **Install Visual Studio Code**.
2.  **Install the PlatformIO IDE extension** from the VSCode marketplace.
3.  **Open the Project**: In VSCode, go to `File > Open Folder...` and select this project's directory.
4.  **Build & Upload**:
    -   Connect your ESP32-C3 board via USB.
    -   Click the **PlatformIO icon** on the left sidebar.
    -   Under `Project Tasks`, find your environment (`esp32-c3-devkitm-1`) and click **Upload**.

### Using the PlatformIO CLI

1.  **Install the PlatformIO Core (CLI)**.
2.  **Open a terminal** and navigate into the project directory.
3.  **Build & Upload**: Connect your ESP32 board and run the command:
    ```bash
    pio run --environment esp32dev --target upload
    ```

## Development Conventions

*   **Framework**: Arduino
*   **Build System**: PlatformIO
*   **BLE Stack**: `NimBLE-Arduino`
*   **Main Source File**: The primary application logic is located in `src/main.cpp`.
*   **Environments**: The `platformio.ini` file defines multiple environments for different hardware platforms and test configurations. When working on the main application, ensure you are using the `esp32dev` environment.
