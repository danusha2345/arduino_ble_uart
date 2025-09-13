#include <Arduino.h>
#include <NimBLEDevice.h>
#include <HardwareSerial.h>
#include "esp_wifi.h" // Required for disabling power saving
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <TinyGPS++.h>
#include <Arduino_GFX_Library.h>
#include <SPI.h>

// Используем второй аппаратный UART (Serial1, т.к. Serial0 занят USB)
// RX-пин: GPIO8, TX-пин: GPIO10
HardwareSerial SerialPort(1);

// I2C OLED Display настройки
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x78 // I2C адрес дисплея
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// TFT ST7789V Display - Arduino_GFX библиотека
// Пины подключения: GPIO0=SCLK, GPIO1=MOSI, GPIO2=DC, GPIO9=RST, GPIO5=BL
#define TFT_CS    -1  // Не используется
#define TFT_RST    9  // Reset pin
#define TFT_DC     2  // Data/Command pin  
#define TFT_MOSI   1  // SPI Data
#define TFT_SCLK   0  // SPI Clock
#define TFT_BL     5  // Backlight pin

// Arduino_GFX инициализация для ST7789V
Arduino_DataBus *bus = new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCLK, TFT_MOSI, -1);
Arduino_GFX *tft = new Arduino_ST7789(bus, TFT_RST, 1 /* rotation 90° */, true /* IPS */, 135 /* width */, 240 /* height */, 52 /* col offset 1 */, 40 /* row offset 1 */, 53 /* col offset 2 */, 40 /* row offset 2 */);

// Arduino_GFX уже имеет встроенные цветовые константы
#define TFT_BLACK   BLACK
#define TFT_BLUE    BLUE  
#define TFT_RED     RED
#define TFT_GREEN   GREEN
#define TFT_CYAN    CYAN
#define TFT_MAGENTA MAGENTA
#define TFT_YELLOW  YELLOW
#define TFT_WHITE   WHITE
#define TFT_ORANGE  ORANGE

// GPS
TinyGPSPlus gps;

// I2C пины для ESP32-C3
#define SDA_PIN 3
#define SCL_PIN 4

// Информация о спутниках для каждой системы
struct SatInfo {
    int visible = 0;   // сколько видимых (из GSV)
    int used    = 0;   // сколько реально участвуют в решении (из GSA)
    unsigned long lastUpdate = 0;
};

// GPS данные
struct GPSData {
    double latitude = 0.0, longitude = 0.0, altitude = 0.0;
    double latAccuracy = 999.9;  // Точность по широте
    double lonAccuracy = 999.9;  // Точность по долготе
    double verticalAccuracy = 999.9;
    int satellites = 0;  // Общее количество спутников в фиксе из GNS
    // Качество фикса из GNS (mode indicator):
    // 0=NO FIX, 1=AUTONOMOUS, 2=DGPS, 3=HIGH PREC, 4=RTK FIXED, 5=RTK FLOAT, 6=ESTIMATED, 7=MANUAL, 8=SIMULATOR
    int fixQuality = 0;
    bool valid = false;
    unsigned long lastUpdate = 0;
    unsigned long lastGstUpdate = 0;  // Последнее обновление GST данных (точность)
} gpsData;

// Данные спутников по системам
struct {
    SatInfo gps;
    SatInfo glonass;
    SatInfo galileo;
    SatInfo beidou;
    SatInfo qzss;
} satData;

// Буфер для парсинга NMEA строк
String nmeaBuffer = "";

// Объявления функций
double convertToDecimalDegrees(double ddmm);

// ==============================================
// RING BUFFER IMPLEMENTATION FOR BLE DATA
// ==============================================

#define RING_BUFFER_SIZE 2048

// Общий спинлок для секций кольцевого буфера
static portMUX_TYPE ringbufMux = portMUX_INITIALIZER_UNLOCKED;

struct RingBuffer {
    uint8_t data[RING_BUFFER_SIZE];
    volatile size_t head;      // Индекс для записи (производитель)
    volatile size_t tail;      // Индекс для чтения (потребитель)
    volatile bool overflow;    // Флаг переполнения буфера
    
    RingBuffer() : head(0), tail(0), overflow(false) {}
    
    // Запись данных в кольцевой буфер (thread-safe)
    size_t write(const uint8_t* src, size_t len) {
        if (!src || len == 0) return 0;
        
        size_t written = 0;
        
        // Критическая секция под общим спинлоком
        portENTER_CRITICAL(&ringbufMux);
        
        for (size_t i = 0; i < len; i++) {
            size_t next_head = (head + 1) % RING_BUFFER_SIZE;
            
            if (next_head == tail) {
                // Буфер полон - перезаписываем самые старые данные
                tail = (tail + 1) % RING_BUFFER_SIZE;
                overflow = true;
            }
            
            data[head] = src[i];
            head = next_head;
            written++;
        }
        
        portEXIT_CRITICAL(&ringbufMux);
        return written;
    }
    
    // Чтение данных из кольцевого буфера (thread-safe)
    size_t read(uint8_t* dest, size_t maxLen) {
        if (!dest || maxLen == 0) return 0;
        
        size_t bytesRead = 0;
        
        // Критическая секция под общим спинлоком
        portENTER_CRITICAL(&ringbufMux);
        
        while (tail != head && bytesRead < maxLen) {
            dest[bytesRead] = data[tail];
            tail = (tail + 1) % RING_BUFFER_SIZE;
            bytesRead++;
        }
        
        // Сбрасываем флаг переполнения после чтения
        if (overflow && bytesRead > 0) {
            overflow = false;
        }
        
        portEXIT_CRITICAL(&ringbufMux);
        return bytesRead;
    }
    
    // Получить количество доступных для чтения байт
    size_t available() const {
        portENTER_CRITICAL(&ringbufMux);
        
        size_t avail;
        if (head >= tail) {
            avail = head - tail;
        } else {
            avail = RING_BUFFER_SIZE - tail + head;
        }
        
        portEXIT_CRITICAL(&ringbufMux);
        return avail;
    }
    
    // Получить количество свободного места в байтах
    size_t freeSpace() const {
        return RING_BUFFER_SIZE - available() - 1; // -1 для различения полного/пустого буфера
    }
    
    // Проверить, был ли переполнен буфер
    bool hasOverflowed() const {
        return overflow;
    }
    
    // Очистить буфер
    void clear() {
        portENTER_CRITICAL(&ringbufMux);
        
        head = 0;
        tail = 0;
        overflow = false;
        
        portEXIT_CRITICAL(&ringbufMux);
    }
    
    // Получить размер буфера
    size_t capacity() const {
        return RING_BUFFER_SIZE - 1; // -1 из-за алгоритма различения полный/пустой
    }
};

// Глобальный экземпляр кольцевого буфера для BLE данных
static RingBuffer bleRingBuffer;

// Временный буфер для чтения из кольцевого буфера
static uint8_t bleTempBuffer[512]; // Оптимальный размер для BLE пакетов
static unsigned long lastBleFlush = 0;

// Вспомогательные функции для работы с кольцевым буфером
inline size_t writeToRingBuffer(const uint8_t* data, size_t len) {
    return bleRingBuffer.write(data, len);
}

inline size_t readFromRingBuffer(uint8_t* data, size_t maxLen) {
    return bleRingBuffer.read(data, maxLen);
}

inline size_t getRingBufferAvailable() {
    return bleRingBuffer.available();
}

inline size_t getRingBufferFree() {
    return bleRingBuffer.freeSpace();
}

inline bool getRingBufferOverflow() {
    return bleRingBuffer.hasOverflowed();
}

inline void clearRingBuffer() {
    bleRingBuffer.clear();
}

// UUIDs для Nordic UART Service (NUS)
#define SERVICE_UUID        "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

static NimBLECharacteristic *pTxCharacteristic;
static bool deviceConnected = false;
static bool oldDeviceConnected = false;

// Класс для обработки событий подключения/отключения
class ServerCallbacks: public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer) {
        deviceConnected = true;
        Serial.println("Client connected");
    }

    void onDisconnect(NimBLEServer* pServer) {
        deviceConnected = false;
        Serial.println("Client disconnected");
        // Очищаем кольцевой буфер при отключении
        clearRingBuffer();
        Serial.println("Ring buffer cleared");
        // Перезапускаем advertising, чтобы устройство снова было видно
        NimBLEDevice::startAdvertising();
    }
};

// Класс для обработки записи в RX-характеристику
class RxCallbacks: public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic *pCharacteristic) {
        std::string rxValue = pCharacteristic->getValue();

        if (rxValue.length() > 0) {
            // Отправляем данные в UART1
            SerialPort.write(rxValue.c_str(), rxValue.length());
        }
    }
};

// Класс для обработки чтения TX-характеристики (fallback для клиентов без Notify)
class TxCallbacks: public NimBLECharacteristicCallbacks {
    void onRead(NimBLECharacteristic* pCharacteristic, ble_gap_conn_desc* desc) override {
        // Попробуем отдать клиенту свежие данные из кольцевого буфера
        uint16_t peerMtu = NimBLEDevice::getServer()->getPeerMTU(desc->conn_handle);
        size_t maxPayload = peerMtu > 3 ? (peerMtu - 3) : 20; // ATT header 3 байта

        size_t avail = getRingBufferAvailable();
        size_t toRead = avail;
        if (toRead > maxPayload) toRead = maxPayload;
        if (toRead > sizeof(bleTempBuffer)) toRead = sizeof(bleTempBuffer);

        if (toRead > 0) {
            size_t n = readFromRingBuffer(bleTempBuffer, toRead);
            pCharacteristic->setValue(bleTempBuffer, n);
        } else {
            // Нет данных — возвращаем пустое значение
            pCharacteristic->setValue((uint8_t*)"", 0);
        }
    }
};

// Оптимизированная функция парсинга NMEA для получения точности и спутников
// Использует только операции с C-строками, без объектов String
// Вспомогательная функция для разделения NMEA строки на поля
static int splitFields(const char *nmea, const char *fields[], int maxFields) {
    int count = 0;
    const char *p = nmea;
    while (*p && count < maxFields) {
        fields[count++] = p;
        const char *c = strchr(p, ',');
        if (!c) break;
        p = c + 1;
    }
    return count;
}

// Парсер GSV (видимые спутники)
void parseGSV(const char *nmea) {
    const char *fields[32];
    int n = splitFields(nmea, fields, 32);
    if (n < 4) return;

    int total = atoi(fields[3]); // поле 3 = общее число видимых спутников
    unsigned long now = millis();

    if (strncmp(nmea, "$GPGSV", 6) == 0) {
        satData.gps.visible = total;
        satData.gps.lastUpdate = now;
    } else if (strncmp(nmea, "$GLGSV", 6) == 0) {
        satData.glonass.visible = total;
        satData.glonass.lastUpdate = now;
    } else if (strncmp(nmea, "$GAGSV", 6) == 0) {
        satData.galileo.visible = total;
        satData.galileo.lastUpdate = now;
    } else if (strncmp(nmea, "$GBGSV", 6) == 0) {
        satData.beidou.visible = total;
        satData.beidou.lastUpdate = now;
    } else if (strncmp(nmea, "$GQGSV", 6) == 0) {
        satData.qzss.visible = total;
        satData.qzss.lastUpdate = now;
    }
}

// Парсер GSA (используемые спутники)
void parseGSA(const char *nmea) {
    const char *fields[32];
    int n = splitFields(nmea, fields, 32);
    if (n < 15) return;

    int count = 0;
    // поля 4–15 = PRN используемых спутников
    for (int i = 3; i <= 14 && i < n; i++) {
        if (fields[i] && *fields[i] != '\0' && *fields[i] != ',')
            count++;
    }
    unsigned long now = millis();

    if (strncmp(nmea, "$GPGSA", 6) == 0) {
        satData.gps.used = count;
        satData.gps.lastUpdate = now;
    } else if (strncmp(nmea, "$GLGSA", 6) == 0) {
        satData.glonass.used = count;
        satData.glonass.lastUpdate = now;
    } else if (strncmp(nmea, "$GAGSA", 6) == 0) {
        satData.galileo.used = count;
        satData.galileo.lastUpdate = now;
    } else if (strncmp(nmea, "$BDGSA", 6) == 0) {
        satData.beidou.used = count;
        satData.beidou.lastUpdate = now;
    } else if (strncmp(nmea, "$GQGSA", 6) == 0) {
        satData.qzss.used = count;
        satData.qzss.lastUpdate = now;
    } else if (strncmp(nmea, "$GNGSA", 6) == 0) {
        // Для GNGSA нужно определить System ID
        if (n > 18) {
            // System ID в поле 19 (индекс 18)
            const char* sysField = fields[18];
            if (sysField) {
                int systemId = atoi(sysField);
                if (systemId == 1) {
                    satData.gps.used = count;
                    satData.gps.lastUpdate = now;
                } else if (systemId == 2) {
                    satData.glonass.used = count;
                    satData.glonass.lastUpdate = now;
                } else if (systemId == 3) {
                    satData.galileo.used = count;
                    satData.galileo.lastUpdate = now;
                } else if (systemId == 4) {
                    satData.beidou.used = count;
                    satData.beidou.lastUpdate = now;
                } else if (systemId == 5) {
                    satData.qzss.used = count;
                    satData.qzss.lastUpdate = now;
                }
            }
        }
    }
}

// Парсер GST для точности
void parseGST(const char *nmea) {
    const char *fields[32];
    int n = splitFields(nmea, fields, 32);
    if (n < 9) return;

    // Field 7: Lat accuracy
    if (fields[6] && *fields[6]) {
        float val = atof(fields[6]);
        if (val > 0.0 && val < 100.0) gpsData.latAccuracy = val;
    }
    
    // Field 8: Lon accuracy
    if (fields[7] && *fields[7]) {
        float val = atof(fields[7]);
        if (val > 0.0 && val < 100.0) gpsData.lonAccuracy = val;
    }
    
    // Field 9: Alt accuracy
    if (fields[8] && *fields[8]) {
        float val = atof(fields[8]);
        if (val > 0.0 && val < 100.0) gpsData.verticalAccuracy = val;
    }
    
    gpsData.lastGstUpdate = millis();
}

// Парсер GNS для координат
void parseGNS(const char *nmea) {
    const char *fields[32];
    int n = splitFields(nmea, fields, 32);
    if (n < 11) return;

    // Field 3: Latitude
    if (fields[2] && fields[3] && *fields[2] && *fields[3]) {
        double lat = convertToDecimalDegrees(atof(fields[2]));
        if (fields[3][0] == 'S') lat = -lat;
        gpsData.latitude = lat;
        gpsData.lastUpdate = millis();
    }
    
    // Field 5: Longitude
    if (fields[4] && fields[5] && *fields[4] && *fields[5]) {
        double lon = convertToDecimalDegrees(atof(fields[4]));
        if (fields[5][0] == 'W') lon = -lon;
        gpsData.longitude = lon;
    }
    
    // Field 7: Mode indicators (по 1 символу на систему: GPS, GLONASS, Galileo, BDS, QZSS, NavIC)
    if (fields[6] && *fields[6]) {
        auto modeRank = [](char m) -> int {
            switch (m) {
                case 'R': return 6; // RTK integer (fixed)
                case 'F': return 5; // RTK float
                case 'P': return 4; // High precision
                case 'D': return 3; // Differential
                case 'A': return 2; // Autonomous
                case 'M': return 1; // Manual input
                case 'S': return 0; // Simulator
                default:  return -1; // N / unknown
            }
        };

        // Определяем длину поля до запятой или '*', учитываем максимум 6 систем
        size_t modesLen = 0;
        while (fields[6][modesLen] && fields[6][modesLen] != ',' && fields[6][modesLen] != '*') modesLen++;
        if (modesLen > 6) modesLen = 6;

        char bestMode = 'N';
        int bestRank = -1;
        bool hasValidFix = false; // Валиден только для A/D/P/F/R

        for (size_t i = 0; i < modesLen; i++) {
            char mode = fields[6][i];
            int rank = modeRank(mode);
            if (mode == 'A' || mode == 'D' || mode == 'P' || mode == 'F' || mode == 'R') hasValidFix = true;
            if (rank > bestRank) { bestRank = rank; bestMode = mode; }
        }

        // Устанавливаем fixQuality по лучшему режиму
        switch (bestMode) {
            case 'A': gpsData.fixQuality = 1; break;
            case 'D': gpsData.fixQuality = 2; break;
            case 'P': gpsData.fixQuality = 3; break;
            case 'R': gpsData.fixQuality = 4; break;
            case 'F': gpsData.fixQuality = 5; break;
            case 'M': gpsData.fixQuality = 7; break; // MANUAL
            case 'S': gpsData.fixQuality = 8; break; // SIMULATOR
            default:  gpsData.fixQuality = 0; break;  // NO FIX / unknown
        }

        gpsData.valid = hasValidFix;
    }
    
    // Field 8: Number of satellites
    if (fields[7] && *fields[7]) {
        gpsData.satellites = atoi(fields[7]);
    }
    
    // Field 10: Altitude
    if (n > 9 && fields[9] && *fields[9]) {
        gpsData.altitude = atof(fields[9]);
    }
}

// Универсальный диспетчер NMEA
void parseNMEA(const char *nmea) {
    if (strncmp(nmea, "$GP", 3) == 0 || strncmp(nmea, "$GA", 3) == 0 ||
        strncmp(nmea, "$GL", 3) == 0 || strncmp(nmea, "$GB", 3) == 0 ||
        strncmp(nmea, "$GQ", 3) == 0 || strncmp(nmea, "$GN", 3) == 0) {
        
        if (strstr(nmea, "GSV")) parseGSV(nmea);
        else if (strstr(nmea, "GSA")) parseGSA(nmea);
        else if (strstr(nmea, "GST")) parseGST(nmea);
        else if (strstr(nmea, "GNS")) parseGNS(nmea);
    }
}

// Проверка таймаутов для данных спутников
void checkSatelliteTimeouts() {
    unsigned long now = millis();
    const unsigned long timeout = 10000; // 10 секунд таймаут
    
    // Сбрасываем количество спутников, если данные устарели
    if (now - satData.gps.lastUpdate > timeout) {
        satData.gps.visible = 0;
        satData.gps.used = 0;
    }
    if (now - satData.glonass.lastUpdate > timeout) {
        satData.glonass.visible = 0;
        satData.glonass.used = 0;
    }
    if (now - satData.galileo.lastUpdate > timeout) {
        satData.galileo.visible = 0;
        satData.galileo.used = 0;
    }
    if (now - satData.beidou.lastUpdate > timeout) {
        satData.beidou.visible = 0;
        satData.beidou.used = 0;
    }
    if (now - satData.qzss.lastUpdate > timeout) {
        satData.qzss.visible = 0;
        satData.qzss.used = 0;
    }
}

// Конвертация из DDMM.MMMM в десятичные градусы
double convertToDecimalDegrees(double ddmm) {
    int degrees = (int)(ddmm / 100);
    double minutes = ddmm - (degrees * 100);
    return degrees + minutes / 60.0;
}

String getFixTypeString(int quality) {
    switch(quality) {
        case 0: return "NO FIX";
        case 1: return "GPS";
        case 2: return "DGPS";
        case 3: return "HIGH PREC"; // High precision (Precise)
        case 4: return "RTK FIXED";
        case 5: return "RTK FLOAT";
        case 6: return "ESTIMATED"; // резерв под возможные источники вне GNS
        case 7: return "MANUAL";
        case 8: return "SIM";
        default: return "UNKNOWN";
    }
}

// Структура для хранения состояния каждой строки дисплея
struct DisplayLineState {
    String text;
    uint16_t color;    // Для TFT
    bool needsUpdate;
    int16_t x, y;      // Позиция строки
    uint8_t textSize;
    bool hasAccuracy;  // Для многострочной точности
};

// Состояние строк для OLED и TFT дисплеев
static DisplayLineState oledLines[8];  // OLED макс. 8 строк (64px/8px)
static DisplayLineState tftLines[12];  // TFT макс. 12 строк (240px/20px)
static bool oledInitialized = false;
static bool tftInitialized = false;
static unsigned long lastForceUpdate = 0;

// Константы для позиционирования
#define OLED_LINE_HEIGHT 8
#define TFT_LINE_HEIGHT 20  // При размере текста 2
#define MAX_OLED_LINES 8
#define MAX_TFT_LINES 12
// Максимум символов в строке при текущих размерах шрифта
#define OLED_MAX_CHARS 21   // 128px / (6px*1)
#define TFT_MAX_CHARS 20    // 240px / (6px*2)

// Вспомогательные функции для работы с дисплеями
void initializeDisplayStates() {
    if (!oledInitialized) {
        for (int i = 0; i < MAX_OLED_LINES; i++) {
            oledLines[i].text = "";
            oledLines[i].color = SSD1306_WHITE;
            oledLines[i].needsUpdate = true;
            oledLines[i].x = 0;
            oledLines[i].y = i * OLED_LINE_HEIGHT;
            oledLines[i].textSize = 1;
            oledLines[i].hasAccuracy = false;
        }
        oledInitialized = true;
    }
    
    if (!tftInitialized) {
        for (int i = 0; i < MAX_TFT_LINES; i++) {
            tftLines[i].text = "";
            tftLines[i].color = TFT_WHITE;
            tftLines[i].needsUpdate = true;
            tftLines[i].x = 0;
            tftLines[i].y = i * TFT_LINE_HEIGHT;
            tftLines[i].textSize = 2;
            tftLines[i].hasAccuracy = false;
        }
        tftInitialized = true;
    }
}

void clearDisplayLine(int lineNum, bool isOled) {
    if (isOled && lineNum < MAX_OLED_LINES) {
        // Очищаем конкретную область строки в OLED
        display.fillRect(0, oledLines[lineNum].y, SCREEN_WIDTH, OLED_LINE_HEIGHT, SSD1306_BLACK);
    } else if (!isOled && lineNum < MAX_TFT_LINES) {
        // Очищаем конкретную область строки в TFT
        tft->fillRect(0, tftLines[lineNum].y, 240, TFT_LINE_HEIGHT, TFT_BLACK);
    }
}

bool updateDisplayLine(int lineNum, const String& newText, uint16_t newColor, bool isOled) {
    DisplayLineState* lines = isOled ? oledLines : tftLines;
    int maxLines = isOled ? MAX_OLED_LINES : MAX_TFT_LINES;
    
    if (lineNum >= maxLines) return false;
    
    // Проверяем, нужно ли обновление
    bool needsUpdate = (lines[lineNum].text != newText) || 
                       (lines[lineNum].color != newColor) ||
                       lines[lineNum].needsUpdate;
    
    if (needsUpdate) {
        // Очищаем старую строку
        clearDisplayLine(lineNum, isOled);
        
        // Обновляем состояние
        lines[lineNum].text = newText;
        lines[lineNum].color = newColor;
        lines[lineNum].needsUpdate = false;
        
        // Выводим новый текст
        if (isOled) {
            display.setCursor(lines[lineNum].x, lines[lineNum].y);
            display.setTextSize(lines[lineNum].textSize);
            display.setTextColor(lines[lineNum].color);
            display.print(newText);
        } else {
            tft->setCursor(lines[lineNum].x, lines[lineNum].y);
            tft->setTextSize(lines[lineNum].textSize);
            tft->setTextColor(lines[lineNum].color);
            tft->print(newText);
        }
        
        return true; // Строка была обновлена
    }
    
    return false; // Обновление не требовалось
}

String formatSatelliteString() {
    String satStr = "G:" + String(satData.gps.used) + 
                   " R:" + String(satData.glonass.used) +
                   " E:" + String(satData.galileo.used) +
                   " B:" + String(satData.beidou.used);
    
    if (satData.qzss.used > 0) {
        satStr += " Q:" + String(satData.qzss.used);
    }
    
    return satStr;
}

// Форматирование строки координаты под ограничение количества символов
static String formatCoordLine(const char* label, double value, int maxChars, int minDecimals = 6, int maxDecimals = 8) {
    String best = String(label) + String(value, minDecimals);
    // Увеличиваем число знаков после запятой, пока строка помещается
    for (int d = minDecimals + 1; d <= maxDecimals; ++d) {
        String candidate = String(label) + String(value, d);
        if ((int)candidate.length() <= maxChars) {
            best = candidate;
        } else {
            break;
        }
    }
    return best;
}

// Настройки часового пояса
#ifdef TZ_FORCE_OFFSET_MINUTES
static volatile bool tzAuto = false;                         // ручной режим
static volatile int  tzOffsetMinutes = TZ_FORCE_OFFSET_MINUTES; // смещение от UTC в минутах (-720..+840)
#else
static volatile bool tzAuto = true;                          // авто по долготе
static volatile int  tzOffsetMinutes = 0;                    // смещение от UTC в минутах
#endif

// Оценка смещения часового пояса по долготе (грубая, без учёта политических границ и DST)
static int estimateOffsetMinutesFromLongitude(double lon) {
    // Округляем к ближайшему часовому поясу с шагом 15° (UTC±14 максимум)
    int hours = (int)floor((lon + 7.5) / 15.0);
    if (hours < -12) hours = -12;
    if (hours > 14) hours = 14;
    return hours * 60;
}

// Форматирование локального времени как HH:MM:SS с учётом tzOffsetMinutes
static String formatLocalTime() {
    if (!gps.time.isValid()) return String("");
    long sec = gps.time.hour() * 3600L + gps.time.minute() * 60L + gps.time.second();
    sec += (long)tzOffsetMinutes * 60L;
    // Нормализация в пределы суток
    sec %= 86400L;
    if (sec < 0) sec += 86400L;
    int hh = (int)(sec / 3600L);
    int mm = (int)((sec % 3600L) / 60L);
    int ss = (int)(sec % 60L);
    char buf[9];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", hh, mm, ss);
    return String(buf);
}

// Форматирование строки высоты с возможным добавлением времени при наличии места
static String formatAltitudeLine(int maxChars) {
    // Базовый формат с 1 знаком после запятой
    String base = String("Alt: ") + String(gpsData.altitude, 1) + "m";
    String t = formatLocalTime();
    if (t.length() == 0) return base;

    // Предпочтительно с пробелом
    if ((int)base.length() + 1 + (int)t.length() <= maxChars) {
        return base + " " + t;
    }

    // Попробуем уменьшить точность до 0 знаков
    String base0 = String("Alt: ") + String(gpsData.altitude, 0) + "m";
    if ((int)base0.length() + 1 + (int)t.length() <= maxChars) {
        return base0 + " " + t;
    }

    // Попробуем без пробела
    if ((int)base.length() + (int)t.length() <= maxChars) {
        return base + t;
    }

    // Последняя попытка: убрать единицу измерения для экономии 1 символа
    String baseNoUnit = String("Alt: ") + String(gpsData.altitude, 0);
    if ((int)baseNoUnit.length() + 1 + (int)t.length() <= maxChars) {
        return baseNoUnit + " " + t;
    }

    // Не помещается — оставляем только высоту
    return base;
}

String formatAccuracyString(int lineType) {
    if (gpsData.latAccuracy >= 99.9 && gpsData.lonAccuracy >= 99.9) {
        return ""; // Нет данных о точности
    }
    
    if (lineType == 1) { // Первая строка точности
        // При высокой точности (< 1м) показываем в сантиметрах с десятыми
        if (gpsData.latAccuracy < 1.0 && gpsData.lonAccuracy < 1.0) {
            return String("N/S:") + String(gpsData.latAccuracy * 100, 1) + "cm "
                 + "E/W:" + String(gpsData.lonAccuracy * 100, 1) + "cm";
        } else {
            // Метры: добавляем 'm' после обоих значений
            return String("N/S:") + String(gpsData.latAccuracy, 1) + "m "
                 + "E/W:" + String(gpsData.lonAccuracy, 1) + "m";
        }
    } else { // Вторая строка точности
        // При высокой точности (< 1м) показываем в сантиметрах с десятыми
        if (gpsData.verticalAccuracy < 1.0) {
            return "H:" + String(gpsData.verticalAccuracy * 100, 1) + "cm";
        } else {
            return "H:" + String(gpsData.verticalAccuracy, 1) + "m";
        }
    }
}

void updateDisplay() {
    static unsigned long lastOledUpdate = 0;
    static unsigned long lastTftUpdate = 0;
    
    // Инициализируем состояния дисплеев при первом вызове
    initializeDisplayStates();
    
    // Проверяем, нужно ли принудительное обновление (раз в 30 секунд)
    bool forceUpdate = (millis() - lastForceUpdate > 30000);
    if (forceUpdate) {
        lastForceUpdate = millis();
        // Помечаем все строки для обновления
        for (int i = 0; i < MAX_OLED_LINES; i++) oledLines[i].needsUpdate = true;
        for (int i = 0; i < MAX_TFT_LINES; i++) tftLines[i].needsUpdate = true;
    }
    
    // Проверяем частоту обновления
    bool canUpdateOled = (millis() - lastOledUpdate > 500) || forceUpdate; // OLED: 2 FPS
    bool canUpdateTft = (millis() - lastTftUpdate > 333) || forceUpdate;   // TFT: 3 FPS
    
    bool oledUpdated = false;
    bool tftUpdated = false;
    
    if (gpsData.valid && (millis() - gpsData.lastUpdate < 5000)) {
        // GPS валиден - отображаем полные данные
        
        // Строка 0: Спутники и тип фикса
        String line0 = "Sats: " + String(gpsData.satellites) + " Fix: " + getFixTypeString(gpsData.fixQuality);
        if (canUpdateOled) {
            oledUpdated |= updateDisplayLine(0, line0, SSD1306_WHITE, true);
        }
        if (canUpdateTft) {
            tftUpdated |= updateDisplayLine(0, line0, TFT_GREEN, false);
        }
        
        // Строка 1: Широта (динамическая точность по оставшемуся месту)
        String line1_oled = formatCoordLine("Lat: ", gpsData.latitude, OLED_MAX_CHARS);
        String line1_tft  = formatCoordLine("Lat: ", gpsData.latitude, TFT_MAX_CHARS);
        if (canUpdateOled) {
            oledUpdated |= updateDisplayLine(1, line1_oled, SSD1306_WHITE, true);
        }
        if (canUpdateTft) {
            tftUpdated |= updateDisplayLine(1, line1_tft, TFT_WHITE, false);
        }
        
        // Строка 2: Долгота (динамическая точность по оставшемуся месту)
        String line2_oled = formatCoordLine("Lon: ", gpsData.longitude, OLED_MAX_CHARS);
        String line2_tft  = formatCoordLine("Lon: ", gpsData.longitude, TFT_MAX_CHARS);
        if (canUpdateOled) {
            oledUpdated |= updateDisplayLine(2, line2_oled, SSD1306_WHITE, true);
        }
        if (canUpdateTft) {
            tftUpdated |= updateDisplayLine(2, line2_tft, TFT_WHITE, false);
        }
        
        // Строка 3: Высота (+ время, если помещается по ширине)
        String line3_oled = formatAltitudeLine(OLED_MAX_CHARS);
        String line3_tft  = formatAltitudeLine(TFT_MAX_CHARS);
        if (canUpdateOled) {
            oledUpdated |= updateDisplayLine(3, line3_oled, SSD1306_WHITE, true);
        }
        if (canUpdateTft) {
            tftUpdated |= updateDisplayLine(3, line3_tft, TFT_YELLOW, false);
        }
        
        // Строки точности (если доступны)
        int nextLine = 4;
        String accLine1 = formatAccuracyString(1);
        String accLine2 = formatAccuracyString(2);
        // Подгон по ширине для TFT (240px, textSize=2 => ~20 символов)
        String accLine1Tft = accLine1;
        if (accLine1Tft.length() > 20) {
            accLine1Tft.replace("N/S:", "NS:");
            accLine1Tft.replace("E/W:", "EW:");
        }
        
        if (accLine1.length() > 0) {
            if (canUpdateOled) {
                oledUpdated |= updateDisplayLine(nextLine, accLine1, SSD1306_WHITE, true);
            }
            if (canUpdateTft) {
                tftUpdated |= updateDisplayLine(nextLine, accLine1Tft, TFT_MAGENTA, false);
            }
            nextLine++;
            
            if (accLine2.length() > 0) {
                if (canUpdateOled) {
                    oledUpdated |= updateDisplayLine(nextLine, accLine2, SSD1306_WHITE, true);
                }
                if (canUpdateTft) {
                    tftUpdated |= updateDisplayLine(nextLine, accLine2, TFT_MAGENTA, false);
                }
                nextLine++;
            }
        }
        
        // Строка спутников по системам
        String satLine = formatSatelliteString();
        if (canUpdateOled) {
            oledUpdated |= updateDisplayLine(nextLine, satLine, SSD1306_WHITE, true);
        }
        if (canUpdateTft) {
            tftUpdated |= updateDisplayLine(nextLine, satLine, TFT_WHITE, false);
        }
        nextLine++;
        
        // Очищаем неиспользуемые строки
        for (int i = nextLine; i < MAX_OLED_LINES; i++) {
            if (canUpdateOled && oledLines[i].text.length() > 0) {
                oledUpdated |= updateDisplayLine(i, "", SSD1306_WHITE, true);
            }
        }
        for (int i = nextLine; i < MAX_TFT_LINES; i++) {
            if (canUpdateTft && tftLines[i].text.length() > 0) {
                tftUpdated |= updateDisplayLine(i, "", TFT_WHITE, false);
            }
        }
        
    } else {
        // GPS не валиден - отображаем статус поиска
        
        // Строка 0: Тип фикса
        String line0 = "Fix: " + getFixTypeString(gpsData.fixQuality);
        if (canUpdateOled) {
            oledUpdated |= updateDisplayLine(0, line0, SSD1306_WHITE, true);
        }
        if (canUpdateTft) {
            tftUpdated |= updateDisplayLine(0, line0, TFT_ORANGE, false);
        }
        
        int nextLine = 1;
        
        int totalUsedSats = satData.gps.used + satData.glonass.used + satData.galileo.used + satData.beidou.used + satData.qzss.used;
        if (totalUsedSats > 0) {
            // Строка 1: Количество спутников
            String line1 = "Sats: " + String(totalUsedSats);
            if (canUpdateOled) {
                oledUpdated |= updateDisplayLine(nextLine, line1, SSD1306_WHITE, true);
            }
            if (canUpdateTft) {
                tftUpdated |= updateDisplayLine(nextLine, line1, TFT_GREEN, false);
            }
            nextLine++;
            
            // Строка 2: Спутники по системам
            String satLine = formatSatelliteString();
            if (canUpdateOled) {
                oledUpdated |= updateDisplayLine(nextLine, satLine, SSD1306_WHITE, true);
            }
            if (canUpdateTft) {
                tftUpdated |= updateDisplayLine(nextLine, satLine, TFT_WHITE, false);
            }
            nextLine++;
        } else {
            // Строка 1: Статус поиска
            String line1 = "Searching GPS...";
            if (canUpdateOled) {
                oledUpdated |= updateDisplayLine(nextLine, line1, SSD1306_WHITE, true);
            }
            if (canUpdateTft) {
                tftUpdated |= updateDisplayLine(nextLine, line1, TFT_BLUE, false);
            }
            nextLine++;
        }
        
        // Очищаем неиспользуемые строки
        for (int i = nextLine; i < MAX_OLED_LINES; i++) {
            if (canUpdateOled && oledLines[i].text.length() > 0) {
                oledUpdated |= updateDisplayLine(i, "", SSD1306_WHITE, true);
            }
        }
        for (int i = nextLine; i < MAX_TFT_LINES; i++) {
            if (canUpdateTft && tftLines[i].text.length() > 0) {
                tftUpdated |= updateDisplayLine(i, "", TFT_WHITE, false);
            }
        }
    }
    
    // Обновляем дисплей только если были изменения
    if (oledUpdated && canUpdateOled) {
        display.display();
        lastOledUpdate = millis();
    }
    
    // TFT не требует дополнительного вызова display(), обновления происходят сразу
    if (tftUpdated && canUpdateTft) {
        lastTftUpdate = millis();
    }
}

void setup() {
    // Запускаем основной UART для логирования
    Serial.begin(460800);
    Serial.println("Starting BLE to UART bridge...");

    // NEW: Disable WiFi/BLE modem power saving
    esp_wifi_set_ps(WIFI_PS_NONE);
    Serial.println("Modem power saving disabled.");

    // Инициализация I2C и OLED дисплея
    Wire.begin(SDA_PIN, SCL_PIN);
    if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS >> 1)) { // Адрес делим на 2!
        Serial.println("ERROR: SSD1306 allocation failed!");
    } else {
        Serial.println("OLED Display initialized successfully!");
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);
        display.display();
    }

    // Инициализация SPI и TFT дисплея с кастомной конфигурацией
    pinMode(TFT_BL, OUTPUT); // TFT_BL = GPIO5
    digitalWrite(TFT_BL, HIGH); // Включаем подсветку
    
    // Инициализируем TFT с Arduino_GFX библиотекой
    tft->begin();
    tft->fillScreen(TFT_BLACK);
    tft->setTextColor(TFT_WHITE);
    tft->setTextSize(2);
    
    // Инициализируем состояния дисплеев для корректной работы частичных обновлений
    initializeDisplayStates();
    
    // НЕ выводим текст при инициализации - пусть updateDisplay() сам управляет экраном
    Serial.println("TFT Display initialized with Arduino_GFX!");

    // Запускаем UART1 для передачи данных
    // RX = 8, TX = 10
    SerialPort.begin(460800, SERIAL_8N1, 8, 10);

    // Инициализация BLE
    NimBLEDevice::init("ESP32-BLE-UART_2");

    // Увеличиваем MTU для максимальной скорости
    NimBLEDevice::setMTU(517);

    // NEW: Set BLE TX power to the maximum (+9 dBm)
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);
    Serial.println("BLE TX power set to maximum.");

    // Настройка безопасности для сопряжения с PIN-кодом
    NimBLEDevice::setSecurityAuth(
        true, // bonding
        true, // mitm (man-in-the-middle protection)
        true  // secure_connections
    );
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY);
    uint32_t passkey = 123456; // Задаем 6-значный PIN-код
    NimBLEDevice::setSecurityPasskey(passkey);

    // Создание BLE-сервера
    NimBLEServer *pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    // Создание BLE-сервиса (NUS)
    NimBLEService *pService = pServer->createService(SERVICE_UUID);

    // Создание TX-характеристики (для отправки данных на телефон)
    pTxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_TX,
        // READ добавлен как резервный путь для клиентов, которые только читают
        NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::READ
    );
    pTxCharacteristic->setCallbacks(new TxCallbacks());

    // Создание RX-характеристики (для приёма данных с телефона)
    NimBLECharacteristic *pRxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_RX,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
    );
    pRxCharacteristic->setCallbacks(new RxCallbacks());

    // Запуск сервиса
    pService->start();

    // Настройка и запуск advertising
    NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    // Максимальная скорость - минимальные интервалы
    pAdvertising->setMinPreferred(0x06); // 7.5ms - самый быстрый
    pAdvertising->setMaxPreferred(0x06); // 7.5ms - фиксированная скорость
    NimBLEDevice::startAdvertising();
    Serial.println("Advertising started. Waiting for a client connection...");
    
    // Информация о кольцевом буфере
    Serial.printf("Ring buffer initialized: %d bytes capacity\n", bleRingBuffer.capacity());
    Serial.printf("Ring buffer available: %d bytes\n", getRingBufferAvailable());
    Serial.printf("Ring buffer free: %d bytes\n", getRingBufferFree());
}

void checkDataTimeouts() {
    unsigned long currentTime = millis();
    
    // Проверяем таймауты для новой структуры satData
    checkSatelliteTimeouts();
    
    // Сбрасываем данные о точности если GST не обновлялись > 10 секунд И нет GPS фикса
    // При RTK точность может обновляться реже, поэтому увеличиваем таймаут
    bool isRtk = (gpsData.fixQuality == 4 || gpsData.fixQuality == 5);
    if ((currentTime - gpsData.lastGstUpdate > 10000 && !isRtk) ||  // Обычный GPS - 10 секунд
        (currentTime - gpsData.lastGstUpdate > 30000 && isRtk) ||   // RTK режимы - 30 секунд
        (!gpsData.valid && currentTime - gpsData.lastGstUpdate > 5000) ||           // Нет фикса - 5 секунд
        gpsData.fixQuality == 0) {                                                   // Совсем нет фикса
        gpsData.latAccuracy = 999.9;
        gpsData.lonAccuracy = 999.9;
        gpsData.verticalAccuracy = 999.9;
    }
}

void loop() {
    // Оптимизированное чтение и обработка GPS данных
    static uint8_t uartReadBuffer[256]; // Буфер для пакетного чтения UART
    
    int bytesAvailable = SerialPort.available();
    if (bytesAvailable > 0) {
        // Читаем данные пакетами для лучшей производительности
        int bytesToRead = (bytesAvailable > 256) ? 256 : bytesAvailable;
        int bytesRead = SerialPort.readBytes(uartReadBuffer, bytesToRead);
        
        
        // Записываем весь пакет в кольцевой буфер одной операцией
        if (deviceConnected && bytesRead > 0) {
            writeToRingBuffer(uartReadBuffer, bytesRead);
        }
        
        // Обрабатываем каждый байт для парсинга GPS и NMEA
        for (int i = 0; i < bytesRead; i++) {
            char c = uartReadBuffer[i];
            
            // Собираем NMEA строку для парсинга точности
            nmeaBuffer += c;
            if (c == '\n') {
                parseNMEA(nmeaBuffer.c_str());
                nmeaBuffer = "";
            }
            
            // Парсим GPS данные через TinyGPS++
            if (gps.encode(c)) {
                if (gps.location.isValid()) {
                    gpsData.latitude = gps.location.lat();
                    gpsData.longitude = gps.location.lng();
                    gpsData.valid = true;
                    gpsData.lastUpdate = millis();
                    // Автоматическая коррекция часового пояса по долготе
                    if (tzAuto) {
                        tzOffsetMinutes = estimateOffsetMinutesFromLongitude(gpsData.longitude);
                    }
                }
            }
        }
    }

    // Отправляем данные из кольцевого буфера через BLE
    if (deviceConnected) {
        size_t available = getRingBufferAvailable();
        
        // Условия для отправки данных:
        // 1. Буфер содержит >= 500 байт (почти полный BLE пакет)
        // 2. Есть данные и прошло > 20мс с последней отправки (для уменьшения задержки)
        // 3. Принудительная отправка через 50мс если есть любые данные
        if (available > 0) {
            bool shouldSend = false;
            unsigned long currentTime = millis();
            
            if (available >= 500) {
                shouldSend = true; // Отправляем большие пакеты сразу
            } else if (available > 0 && (currentTime - lastBleFlush > 20)) {
                shouldSend = true; // Отправляем небольшие пакеты через 20мс
            } else if (available > 0 && (currentTime - lastBleFlush > 50)) {
                shouldSend = true; // Принудительная отправка через 50мс
            }
            
            if (shouldSend) {
                // Отправляем только если есть подписчики, чтобы не терять данные
                if (pTxCharacteristic->getSubscribedCount() > 0) {
                    // Читаем оптимальное количество данных (не больше 512 байт)
                    size_t toRead = (available > 512) ? 512 : available;
                    size_t bytesRead = readFromRingBuffer(bleTempBuffer, toRead);

                    if (bytesRead > 0) {
                        pTxCharacteristic->setValue(bleTempBuffer, bytesRead);
                        pTxCharacteristic->notify();
                        lastBleFlush = currentTime;

                        // Логирование переполнения буфера
                        if (getRingBufferOverflow()) {
                            Serial.println("WARNING: Ring buffer overflow occurred!");
                        }
                    }
                }
            }
        }
    }
    
    // Проверяем устаревшие данные
    checkDataTimeouts();
    
    // Обновляем дисплей
    updateDisplay();

    // Обработка подключения/отключения для вывода в лог
    if (!deviceConnected && oldDeviceConnected) {
        delay(500); // Даем время для BLE-стека
        oldDeviceConnected = deviceConnected;
    }
    if (deviceConnected && !oldDeviceConnected) {
        oldDeviceConnected = deviceConnected;
    }
}
