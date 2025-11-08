/**
 * @file config.h
 * @brief Конфигурационные определения для разных платформ ESP32
 */

#pragma once

#include "sdkconfig.h"

// Определяем целевую платформу
#if CONFIG_TARGET_ESP32_C3
    #define TARGET_ESP32_C3 1
    #define BOARD_NAME "ESP32-C3"
    #define BLE_DEVICE_NAME "UM980_C3_GPS"
    #define WIFI_AP_SSID "UM980_GPS_BRIDGE"

    // Пины для ESP32-C3
    #define UART_RX_PIN     CONFIG_C3_UART_RX_PIN
    #define UART_TX_PIN     CONFIG_C3_UART_TX_PIN
    #define I2C_SDA_PIN     CONFIG_C3_I2C_SDA_PIN
    #define I2C_SCL_PIN     CONFIG_C3_I2C_SCL_PIN
    #define SPI_MOSI_PIN    CONFIG_C3_SPI_MOSI_PIN
    #define SPI_SCLK_PIN    CONFIG_C3_SPI_SCLK_PIN
    #define TFT_DC_PIN      CONFIG_C3_TFT_DC_PIN
    #define TFT_RST_PIN     CONFIG_C3_TFT_RST_PIN
    #define TFT_BL_PIN      CONFIG_C3_TFT_BL_PIN
    #define TFT_CS_PIN      -1

    // Определяем TFT_MOSI_PIN для включения дисплея
    #define TFT_MOSI_PIN    SPI_MOSI_PIN
    #define TFT_SCLK_PIN    SPI_SCLK_PIN

#elif CONFIG_TARGET_ESP32_C6
    #define TARGET_ESP32_C6 1
    #define BOARD_NAME "ESP32-C6"
    #define BLE_DEVICE_NAME "UM980_NEW_TEST"
    #define WIFI_AP_SSID "UM980_GPS_BRIDGE_C6"

    // Пины для ESP32-C6 (Tenstar SuperMini) с fallback значениями
    #ifndef CONFIG_C6_UART_RX_PIN
        #define CONFIG_C6_UART_RX_PIN 6
    #endif
    #ifndef CONFIG_C6_UART_TX_PIN
        #define CONFIG_C6_UART_TX_PIN 7
    #endif
    #ifndef CONFIG_C6_SPI_MOSI_PIN
        #define CONFIG_C6_SPI_MOSI_PIN 18
    #endif
    #ifndef CONFIG_C6_SPI_SCLK_PIN
        #define CONFIG_C6_SPI_SCLK_PIN 19
    #endif
    #ifndef CONFIG_C6_TFT_DC_PIN
        #define CONFIG_C6_TFT_DC_PIN 20
    #endif
    #ifndef CONFIG_C6_TFT_RST_PIN
        #define CONFIG_C6_TFT_RST_PIN 21
    #endif
    #ifndef CONFIG_C6_TFT_BL_PIN
        #define CONFIG_C6_TFT_BL_PIN 22
    #endif
    #ifndef CONFIG_C6_TFT_CS_PIN
        #define CONFIG_C6_TFT_CS_PIN 14
    #endif
    #ifndef CONFIG_C6_LED_PIN
        #define CONFIG_C6_LED_PIN 8
    #endif
    #ifndef CONFIG_C6_LED_PIN_2
        #define CONFIG_C6_LED_PIN_2 15
    #endif

    #define UART_RX_PIN     CONFIG_C6_UART_RX_PIN
    #define UART_TX_PIN     CONFIG_C6_UART_TX_PIN
    #define SPI_MOSI_PIN    CONFIG_C6_SPI_MOSI_PIN
    #define SPI_SCLK_PIN    CONFIG_C6_SPI_SCLK_PIN
    #define TFT_DC_PIN      CONFIG_C6_TFT_DC_PIN
    #define TFT_RST_PIN     CONFIG_C6_TFT_RST_PIN
    #define TFT_BL_PIN      CONFIG_C6_TFT_BL_PIN
    #define TFT_CS_PIN      CONFIG_C6_TFT_CS_PIN
    #define LED_PIN         CONFIG_C6_LED_PIN
    #define LED_PIN_2       CONFIG_C6_LED_PIN_2
    #define LCD_SPI_HOST    SPI2_HOST  // ESP32-C6 поддерживает только SPI2

    // Определяем TFT_MOSI_PIN для включения дисплея
    #define TFT_MOSI_PIN    SPI_MOSI_PIN
    #define TFT_SCLK_PIN    SPI_SCLK_PIN

#elif CONFIG_TARGET_ESP32_S3
    #define TARGET_ESP32_S3 1
    #define BOARD_NAME "ESP32-S3"
    #define BLE_DEVICE_NAME "UM980_S3_GPS"
    #define WIFI_AP_SSID "UM980_GPS_BRIDGE_S3"

    // Пины для ESP32-S3
    #define UART_RX_PIN     CONFIG_S3_UART_RX_PIN
    #define UART_TX_PIN     CONFIG_S3_UART_TX_PIN
    #define I2C_SDA_PIN     CONFIG_S3_I2C_SDA_PIN
    #define I2C_SCL_PIN     CONFIG_S3_I2C_SCL_PIN
    #define SPI_MOSI_PIN    CONFIG_S3_SPI_MOSI_PIN
    #define SPI_SCLK_PIN    CONFIG_S3_SPI_SCLK_PIN
    #define TFT_DC_PIN      CONFIG_S3_TFT_DC_PIN
    #define TFT_RST_PIN     CONFIG_S3_TFT_RST_PIN
    #define TFT_BL_PIN      CONFIG_S3_TFT_BL_PIN
    #define TFT_CS_PIN      -1

    // Определяем TFT_MOSI_PIN для включения дисплея
    #define TFT_MOSI_PIN    SPI_MOSI_PIN
    #define TFT_SCLK_PIN    SPI_SCLK_PIN

#else
    // Fallback на ESP32-C6 по умолчанию если платформа не выбрана
    #warning "Target board not selected in menuconfig, defaulting to ESP32-C6"
    #define TARGET_ESP32_C6 1
    #define BOARD_NAME "ESP32-C6"
    #define BLE_DEVICE_NAME "UM980_NEW_TEST"
    #define WIFI_AP_SSID "UM980_GPS_BRIDGE_C6"

    // Пины для ESP32-C6 (значения по умолчанию)
    #define UART_RX_PIN     6
    #define UART_TX_PIN     7
    #define SPI_MOSI_PIN    18
    #define SPI_SCLK_PIN    19
    #define TFT_DC_PIN      20
    #define TFT_RST_PIN     21
    #define TFT_BL_PIN      22
    #define TFT_CS_PIN      14
    #define LED_PIN         8
    #define LED_PIN_2       15
    #define LCD_SPI_HOST    SPI2_HOST  // ESP32-C6 поддерживает только SPI2

    // Определяем TFT_MOSI_PIN для включения дисплея
    #define TFT_MOSI_PIN    SPI_MOSI_PIN
    #define TFT_SCLK_PIN    SPI_SCLK_PIN
#endif

// Общие настройки
#define GPS_UART_BAUD_RATE      460800

// WiFi пароль с fallback
#ifdef CONFIG_WIFI_PASSWORD
    #define WIFI_PASSWORD       CONFIG_WIFI_PASSWORD
#else
    #define WIFI_PASSWORD       "12345678"  // Default пароль (минимум 8 символов для WPA2)
#endif

#define WIFI_PORT               23

// Настройки дисплеев
#if defined(TARGET_ESP32_C3) || defined(TARGET_ESP32_S3)
    // OLED только для C3 и S3
    #define OLED_WIDTH              128
    #define OLED_HEIGHT             64
    #define OLED_I2C_ADDR           0x3C
#endif

// TFT дисплей ST7789V для всех платформ (240x280)
#define TFT_WIDTH               240
#define TFT_HEIGHT              280
#define TFT_SPI_FREQ            12000000  // 12MHz как в рабочем примере
#define TFT_COLOR_BITS          16

// Настройки BLE
#define BLE_MTU                 517
#define BLE_TX_POWER            9  // dBm

// Настройки буферов
#define RING_BUFFER_SIZE        16384
#define RX_BUFFER_SIZE          4096
#define UART_BUF_SIZE           2048

// Настройки WiFi
#define MAX_WIFI_CLIENTS        4

// ==================================================
// КОНСТАНТЫ ДИСПЛЕЯ ST7789V (для display_manager.c)
// ==================================================

#define LCD_H_RES               TFT_WIDTH   // Горизонтальное разрешение
#define LCD_V_RES               TFT_HEIGHT  // Вертикальное разрешение
#define LCD_PIXEL_CLOCK_HZ      (TFT_SPI_FREQ)  // 40MHz
#define LCD_BK_LIGHT_ON_LEVEL   1
#define LCD_BK_LIGHT_OFF_LEVEL  0
#define LCD_CMD_BITS            8
#define LCD_PARAM_BITS          8

// Размер буфера LVGL (в пикселях)
#define LVGL_BUFFER_SIZE        (LCD_H_RES * 20)  // 20 строк

// Константы GPS
#define GPS_UPDATE_INTERVAL_MS  100   // Интервал обновления GPS данных
#define GPS_TIMEOUT_MS          5000  // Таймаут потери сигнала GPS
