# Исправление неопределенных макросов

## Проблема

В [main.c:177](main.c#L177) используются макросы `UART_RX_PIN` и `UART_TX_PIN`, которые не были определены при отсутствии настроенного Kconfig.

```c
ESP_LOGI(TAG, "Initializing UART on RX:%d TX:%d", UART_RX_PIN, UART_TX_PIN);
```

## Причина

Макросы `UART_RX_PIN` и `UART_TX_PIN` были определены только внутри условных блоков:
```c
#if CONFIG_TARGET_ESP32_C3
    #define UART_RX_PIN CONFIG_C3_UART_RX_PIN
#elif CONFIG_TARGET_ESP32_C6
    #define UART_RX_PIN CONFIG_C6_UART_RX_PIN
#elif CONFIG_TARGET_ESP32_S3
    #define UART_RX_PIN CONFIG_S3_UART_RX_PIN
#endif
```

Если ни одна из констант `CONFIG_TARGET_*` не была определена в `sdkconfig`, то `UART_RX_PIN` оставался неопределенным.

## Решение

### 1. Добавлены fallback значения для ESP32-C6

Для каждой CONFIG константы добавлена проверка с значением по умолчанию:

```c
#elif CONFIG_TARGET_ESP32_C6
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
    // ... и т.д.

    #define UART_RX_PIN CONFIG_C6_UART_RX_PIN
    #define UART_TX_PIN CONFIG_C6_UART_TX_PIN
```

### 2. Добавлен блок #else с дефолтными значениями

Если ни одна платформа не выбрана, используется ESP32-C6 по умолчанию:

```c
#else
    // Fallback на ESP32-C6 по умолчанию если платформа не выбрана
    #warning "Target board not selected in menuconfig, defaulting to ESP32-C6"
    #define TARGET_ESP32_C6 1
    #define BOARD_NAME "ESP32-C6"

    // Пины для ESP32-C6 (значения по умолчанию)
    #define UART_RX_PIN     6
    #define UART_TX_PIN     7
    #define SPI_MOSI_PIN    18
    #define SPI_SCLK_PIN    19
    #define TFT_DC_PIN      20
    #define TFT_RST_PIN     21
    #define TFT_BL_PIN      22
    #define LED_PIN         8
    #define LED_PIN_2       15
    #define LCD_SPI_HOST    SPI3_HOST
#endif
```

### 3. Добавлен fallback для WIFI_PASSWORD

```c
#ifdef CONFIG_WIFI_PASSWORD
    #define WIFI_PASSWORD CONFIG_WIFI_PASSWORD
#else
    #define WIFI_PASSWORD "12345678"  // Default пароль (минимум 8 символов для WPA2)
#endif
```

## Результат

✅ Все макросы теперь определены при любой конфигурации
✅ Проект компилируется без настроенного menuconfig (с warning)
✅ При выборе платформы в menuconfig используются соответствующие значения
✅ Для ESP32-C6 всегда доступны корректные значения пинов

## Дефолтные значения для ESP32-C6 Tenstar SuperMini

| Макрос | Значение | Назначение |
|--------|----------|------------|
| `UART_RX_PIN` | 6 | GPS UART RX |
| `UART_TX_PIN` | 7 | GPS UART TX |
| `SPI_MOSI_PIN` | 18 | TFT SPI MOSI |
| `SPI_SCLK_PIN` | 19 | TFT SPI SCLK |
| `TFT_DC_PIN` | 20 | TFT Data/Command |
| `TFT_RST_PIN` | 21 | TFT Reset |
| `TFT_BL_PIN` | 22 | TFT Backlight |
| `LED_PIN` | 8 | WS2812 LED |
| `LED_PIN_2` | 15 | Обычный LED |
| `LCD_SPI_HOST` | SPI3_HOST | SPI контроллер |
| `WIFI_PASSWORD` | "12345678" | WiFi AP пароль |

## Warning при компиляции

Если платформа не выбрана в menuconfig, компилятор выдаст предупреждение:
```
warning: Target board not selected in menuconfig, defaulting to ESP32-C6
```

Это безопасное предупреждение, указывающее что используются дефолтные настройки.

---

**Дата исправления**: 2025-11-04
**Статус**: Все макросы определены корректно с fallback значениями
