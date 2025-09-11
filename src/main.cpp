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

// GPS данные
struct GPSData {
    double latitude = 0.0, longitude = 0.0, altitude = 0.0;
    double latAccuracy = 999.9;  // Точность по широте
    double lonAccuracy = 999.9;  // Точность по долготе
    double verticalAccuracy = 999.9;
    int satellites = 0;  // Количество спутников в фиксе из GNS
    int total_gsa_sats = 0;  // Общий подсчет из GSA сообщений для сравнения
    int gps_sats = 0, glonass_sats = 0, galileo_sats = 0, beidou_sats = 0, qzss_sats = 0;  // Спутники в фиксе по системам
    int fixQuality = 0;  // Качество фикса из GNS (0-нет, 1-GPS, 2-DGPS, 4-RTK, 5-Float, 6-Estimated)
    bool valid = false;
    unsigned long lastUpdate = 0;
    unsigned long lastGsaUpdate = 0;  // Общее время последнего GSA обновления
    unsigned long lastGpsUpdate = 0, lastGlonassUpdate = 0, lastGalileoUpdate = 0, lastBeidouUpdate = 0, lastQzssUpdate = 0;  // Время по системам
    unsigned long lastGstUpdate = 0;  // Последнее обновление GST данных (точность)
} gpsData;

// Буфер для парсинга NMEA строк
String nmeaBuffer = "";

// Объявления функций
double convertToDecimalDegrees(double ddmm);

// ==============================================
// RING BUFFER IMPLEMENTATION FOR BLE DATA
// ==============================================

#define RING_BUFFER_SIZE 2048

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
        
        // Отключаем прерывания для атомарности операций
        portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
        portENTER_CRITICAL(&mux);
        
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
        
        portEXIT_CRITICAL(&mux);
        return written;
    }
    
    // Чтение данных из кольцевого буфера (thread-safe)
    size_t read(uint8_t* dest, size_t maxLen) {
        if (!dest || maxLen == 0) return 0;
        
        size_t bytesRead = 0;
        
        // Отключаем прерывания для атомарности операций
        portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
        portENTER_CRITICAL(&mux);
        
        while (tail != head && bytesRead < maxLen) {
            dest[bytesRead] = data[tail];
            tail = (tail + 1) % RING_BUFFER_SIZE;
            bytesRead++;
        }
        
        // Сбрасываем флаг переполнения после чтения
        if (overflow && bytesRead > 0) {
            overflow = false;
        }
        
        portEXIT_CRITICAL(&mux);
        return bytesRead;
    }
    
    // Получить количество доступных для чтения байт
    size_t available() const {
        portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
        portENTER_CRITICAL(&mux);
        
        size_t avail;
        if (head >= tail) {
            avail = head - tail;
        } else {
            avail = RING_BUFFER_SIZE - tail + head;
        }
        
        portEXIT_CRITICAL(&mux);
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
        portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
        portENTER_CRITICAL(&mux);
        
        head = 0;
        tail = 0;
        overflow = false;
        
        portEXIT_CRITICAL(&mux);
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

// Оптимизированная функция парсинга NMEA для получения точности и спутников
// Использует только операции с C-строками, без объектов String
void parseNmeaAccuracy(const char* nmea) {
    // Статический буфер для работы со строкой (thread-safe в однопоточной среде)
    static char workBuffer[256];
    
    // Копируем входную строку в рабочий буфер для безопасного парсинга
    size_t nmeaLen = strlen(nmea);
    if (nmeaLen >= sizeof(workBuffer)) {
        return; // Строка слишком длинная
    }
    strcpy(workBuffer, nmea);
    
    // Парсим GST для получения точности в метрах
    // Формат: $--GST,hhmmss.ss,x.x,x.x,x.x,x.x,x.x,x.x,x.x*hh
    //         Field: 1,        2,  3,  4,  5,  6,  7,  8,  9
    if (strncmp(workBuffer, "$GNGST", 6) == 0 || strncmp(workBuffer, "$GPGST", 6) == 0) {
        char* fields[10];
        int fieldCount = 0;
        char* saveptr;
        
        // Разбиваем строку на поля по запятым
        char* token = strtok_r(workBuffer, ",", &saveptr);
        while (token && fieldCount < 10) {
            fields[fieldCount++] = token;
            token = strtok_r(NULL, ",", &saveptr);
        }
        
        // GST должен иметь минимум 9 полей для всех полей точности
        if (fieldCount >= 9) {
            bool hasValidAccuracy = false;
            
            // Поле 7: Standard deviation of latitude error (fields[6])
            if (fields[6] && strlen(fields[6]) > 0) {
                float latAcc = atof(fields[6]);
                if (latAcc > 0.0 && latAcc < 100.0) {  // Фильтруем нереальные значения
                    gpsData.latAccuracy = latAcc;
                    hasValidAccuracy = true;
                }
            }
            
            // Поле 8: Standard deviation of longitude error (fields[7])
            if (fields[7] && strlen(fields[7]) > 0) {
                float lonAcc = atof(fields[7]);
                if (lonAcc > 0.0 && lonAcc < 100.0) {  // Фильтруем нереальные значения
                    gpsData.lonAccuracy = lonAcc;
                    hasValidAccuracy = true;
                }
            }
            
            // Поле 9: Standard deviation of altitude error (fields[8])
            if (fields[8] && strlen(fields[8]) > 0) {
                // Удаляем контрольную сумму если есть
                char* asterisk = strchr(fields[8], '*');
                if (asterisk) *asterisk = '\0';
                
                if (strlen(fields[8]) > 0) {
                    float altAcc = atof(fields[8]);
                    if (altAcc > 0.0 && altAcc < 100.0) {  // Фильтруем нереальные значения
                        gpsData.verticalAccuracy = altAcc;
                        hasValidAccuracy = true;
                    }
                }
            }
            
            // Обновляем время только если получили валидные данные точности
            if (hasValidAccuracy) {
                gpsData.lastGstUpdate = millis();
            }
        }
        return; // Выходим после обработки GST
    }
    
    // Парсим GNS для получения координат, количества спутников и качества фикса
    // Формат: $--GNS,hhmmss.ss,IIII.II,a,yyyyy.yy,a,c--c,xx,x.x,x.x,x.x,x.x,x.x,a,*hh
    //         Field: 1,        2,       3,4,        5,6,   7, 8, 9, 10, 11, 12, 13,14
    if (strncmp(workBuffer, "$GNGNS", 6) == 0 || strncmp(workBuffer, "$GPGNS", 6) == 0 || 
        strncmp(workBuffer, "$GLGNS", 6) == 0 || strncmp(workBuffer, "$GAGNS", 6) == 0) {
        
        char* fields[15];
        int fieldCount = 0;
        char* saveptr;
        
        // Разбиваем строку на поля по запятым
        char* token = strtok_r(workBuffer, ",", &saveptr);
        while (token && fieldCount < 15) {
            fields[fieldCount++] = token;
            token = strtok_r(NULL, ",", &saveptr);
        }
        
        if (fieldCount >= 11) {  // Минимум 11 полей для валидного GNS
            // Поле 3: Широта (fields[2])
            // Поле 4: Направление широты (fields[3])
            if (fields[2] && fields[3] && strlen(fields[2]) > 0 && strlen(fields[3]) > 0) {
                double lat = convertToDecimalDegrees(atof(fields[2]));
                if (fields[3][0] == 'S') lat = -lat;
                gpsData.latitude = lat;
                gpsData.lastUpdate = millis();
            }
            
            // Поле 5: Долгота (fields[4])
            // Поле 6: Направление долготы (fields[5])
            if (fields[4] && fields[5] && strlen(fields[4]) > 0 && strlen(fields[5]) > 0) {
                double lon = convertToDecimalDegrees(atof(fields[4]));
                if (fields[5][0] == 'W') lon = -lon;
                gpsData.longitude = lon;
            }
            
            // Поле 7: Mode indicators (fields[6])
            if (fields[6] && strlen(fields[6]) > 0) {
                bool hasValidFix = false;
                char bestMode = 'N';
                
                // Анализируем все символы режима для разных GNSS систем
                for (int i = 0; i < strlen(fields[6]) && i < 4; i++) {
                    char mode = fields[6][i];
                    if (mode != 'N' && mode != ' ') {
                        hasValidFix = true;
                        // Приоритет: R > F > D > P > A > E > остальное
                        if (mode == 'R' || (bestMode != 'R' && mode == 'F') ||
                            (bestMode != 'R' && bestMode != 'F' && mode == 'D') ||
                            (bestMode != 'R' && bestMode != 'F' && bestMode != 'D' && mode == 'P') ||
                            (bestMode != 'R' && bestMode != 'F' && bestMode != 'D' && bestMode != 'P' && mode == 'A')) {
                            bestMode = mode;
                        }
                    }
                }
                
                // Устанавливаем качество фикса на основе лучшего режима
                if (bestMode == 'N') gpsData.fixQuality = 0;      // No fix
                else if (bestMode == 'A') gpsData.fixQuality = 1; // Autonomous
                else if (bestMode == 'D') gpsData.fixQuality = 2; // Differential
                else if (bestMode == 'P') gpsData.fixQuality = 3; // High precision
                else if (bestMode == 'R') gpsData.fixQuality = 4; // RTK Int (fixed)
                else if (bestMode == 'F') gpsData.fixQuality = 5; // RTK Float
                else if (bestMode == 'E') gpsData.fixQuality = 6; // Estimated/Dead reckoning
                else if (bestMode == 'M') gpsData.fixQuality = 1; // Manual (treat as GPS)
                else if (bestMode == 'S') gpsData.fixQuality = 1; // Simulator (treat as GPS)
                else gpsData.fixQuality = 1; // По умолчанию GPS
                
                gpsData.valid = hasValidFix;
            }
            
            // Поле 8: Количество используемых спутников (fields[7])
            if (fields[7] && strlen(fields[7]) > 0) {
                gpsData.satellites = atoi(fields[7]);
            }
            
            // Поле 10: Высота антенны (fields[9])
            if (fieldCount >= 10 && fields[9] && strlen(fields[9]) > 0) {
                gpsData.altitude = atof(fields[9]);
            }
            
            // Поле 14: Навигационный статус (fields[13])
            // S=Safe, C=Caution, U=Unsafe, V=Not valid
            if (fieldCount >= 14 && fields[13] && strlen(fields[13]) > 0) {
                // Удаляем контрольную сумму если есть
                char* asterisk = strchr(fields[13], '*');
                if (asterisk) *asterisk = '\0';
                
                if (fields[13][0] == 'V' || fields[13][0] == 'U') {
                    gpsData.valid = false;  // Переопределяем если статус не валидный или небезопасный
                }
            }
        }
        return; // Выходим после обработки GNS
    }
    
    // Парсим GSA сообщения для подсчёта спутников по системам
    // Формат: $--GSA,a,x,xx,xx,xx,xx,xx,xx,xx,xx,xx,xx,xx,xx,x.x,x.x,x.x,h*hh
    //         Field: 1,2,3,4, 5, 6, 7, 8, 9, 10,11,12,13,14,15,16, 17, 18, 19
    if (strncmp(workBuffer, "$GNGSA", 6) == 0 || strncmp(workBuffer, "$GPGSA", 6) == 0 || 
        strncmp(workBuffer, "$GLGSA", 6) == 0 || strncmp(workBuffer, "$GAGSA", 6) == 0 || 
        strncmp(workBuffer, "$BDGSA", 6) == 0 || strncmp(workBuffer, "$GQGSA", 6) == 0) {
        
        // Сохраняем тип сообщения ДО парсинга, так как strtok_r изменит workBuffer
        char msgType[7];
        strncpy(msgType, workBuffer, 6);
        msgType[6] = '\0';
        
        char* fields[20];
        int fieldCount = 0;
        char* saveptr;
        
        // Разбиваем строку на поля по запятым
        char* token = strtok_r(workBuffer, ",", &saveptr);
        while (token && fieldCount < 20) {
            fields[fieldCount++] = token;
            token = strtok_r(NULL, ",", &saveptr);
        }
        
        if (fieldCount >= 15) { // Нужно минимум 15 полей согласно спецификации
            int count = 0; // Общий счетчик спутников для данного GSA
            
            // Проверяем поле 3 (mode 123) - должно быть 2 или 3 для активного фикса
            if (fields[2] && (fields[2][0] == '2' || fields[2][0] == '3')) {
                // Поля 4-15: ID спутников (fields[3] до fields[14])
                for (int i = 3; i <= 14 && i < fieldCount; i++) {
                    if (fields[i] && strlen(fields[i]) > 0) {
                        int satId = atoi(fields[i]);
                        if (satId > 0) {
                            count++;
                        }
                    }
                }
            }
            
            unsigned long currentTime = millis();
            
            // Определяем систему по префиксу GSA или System ID для GNGSA
            if (strncmp(msgType, "$GPGSA", 6) == 0) {
                gpsData.gps_sats = count;        
                gpsData.lastGpsUpdate = currentTime;
            } else if (strncmp(msgType, "$GLGSA", 6) == 0) {
                gpsData.glonass_sats = count;    
                gpsData.lastGlonassUpdate = currentTime;
            } else if (strncmp(msgType, "$GAGSA", 6) == 0) {
                gpsData.galileo_sats = count;    
                gpsData.lastGalileoUpdate = currentTime;
            } else if (strncmp(msgType, "$BDGSA", 6) == 0) {
                gpsData.beidou_sats = count;     
                gpsData.lastBeidouUpdate = currentTime;
            } else if (strncmp(msgType, "$GQGSA", 6) == 0) {
                gpsData.qzss_sats = count;       
                gpsData.lastQzssUpdate = currentTime;
            } else if (strncmp(msgType, "$GNGSA", 6) == 0) {
                // Для GNGSA определяем систему по полю 19 (System ID)
                if (fieldCount >= 19 && fields[18] && strlen(fields[18]) > 0) {
                    char* asterisk = strchr(fields[18], '*');
                    if (asterisk) *asterisk = '\0';
                    
                    int sysId = atoi(fields[18]);
                    
                    // Присваиваем спутники по System ID согласно Table 7-34
                    if (sysId == 1) {
                        gpsData.gps_sats = count;        // GPS
                        gpsData.lastGpsUpdate = currentTime;
                    } else if (sysId == 2) {
                        gpsData.glonass_sats = count;    // GLONASS
                        gpsData.lastGlonassUpdate = currentTime;
                    } else if (sysId == 3) {
                        gpsData.galileo_sats = count;    // Galileo
                        gpsData.lastGalileoUpdate = currentTime;
                    } else if (sysId == 4) {
                        gpsData.beidou_sats = count;     // BeiDou
                        gpsData.lastBeidouUpdate = currentTime;
                    } else if (sysId == 5) {
                        gpsData.qzss_sats = count;       // QZSS
                        gpsData.lastQzssUpdate = currentTime;
                    } else if (sysId == 6) {
                        // NavIC (IRNSS) - можно добавить поддержку если нужно
                        // gpsData.navic_sats = count;
                    }
                }
            }
            
            // Пересчитываем общую сумму GSA спутников для сравнения
            gpsData.total_gsa_sats = gpsData.gps_sats + gpsData.glonass_sats + gpsData.galileo_sats + gpsData.beidou_sats + gpsData.qzss_sats;
            
            // Обновляем время последнего GSA сообщения
            gpsData.lastGsaUpdate = millis();
        }
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
        case 3: return "PPS";      // High precision 
        case 4: return "RTK FIXED";
        case 5: return "RTK FLOAT";
        case 6: return "ESTIMATED";
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
    String satStr = "G:" + String(gpsData.gps_sats) + 
                   " R:" + String(gpsData.glonass_sats) +
                   " E:" + String(gpsData.galileo_sats) +
                   " B:" + String(gpsData.beidou_sats);
    
    if (gpsData.qzss_sats > 0) {
        satStr += " Q:" + String(gpsData.qzss_sats);
    }
    
    return satStr;
}

String formatAccuracyString(int lineType) {
    if (gpsData.latAccuracy >= 99.9 && gpsData.lonAccuracy >= 99.9) {
        return ""; // Нет данных о точности
    }
    
    if (lineType == 1) { // Первая строка точности
        // При высокой точности (< 1м) показываем в сантиметрах
        if (gpsData.latAccuracy < 1.0 && gpsData.lonAccuracy < 1.0) {
            return "N/S:" + String(gpsData.latAccuracy * 100, 0) + " E/W:" + String(gpsData.lonAccuracy * 100, 0) + "cm";
        } else {
            return "N/S:" + String(gpsData.latAccuracy, 1) + " E/W:" + String(gpsData.lonAccuracy, 1) + "m";
        }
    } else { // Вторая строка точности
        // При высокой точности (< 1м) показываем в сантиметрах
        if (gpsData.verticalAccuracy < 1.0) {
            return "H:" + String(gpsData.verticalAccuracy * 100, 0) + "cm";
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
        
        // Строка 1: Широта
        String line1 = "Lat: " + String(gpsData.latitude, 6);
        if (canUpdateOled) {
            oledUpdated |= updateDisplayLine(1, line1, SSD1306_WHITE, true);
        }
        if (canUpdateTft) {
            tftUpdated |= updateDisplayLine(1, line1, TFT_WHITE, false);
        }
        
        // Строка 2: Долгота
        String line2 = "Lon: " + String(gpsData.longitude, 6);
        if (canUpdateOled) {
            oledUpdated |= updateDisplayLine(2, line2, SSD1306_WHITE, true);
        }
        if (canUpdateTft) {
            tftUpdated |= updateDisplayLine(2, line2, TFT_WHITE, false);
        }
        
        // Строка 3: Высота
        String line3 = "Alt: " + String(gpsData.altitude, 1) + "m";
        if (canUpdateOled) {
            oledUpdated |= updateDisplayLine(3, line3, SSD1306_WHITE, true);
        }
        if (canUpdateTft) {
            tftUpdated |= updateDisplayLine(3, line3, TFT_YELLOW, false);
        }
        
        // Строки точности (если доступны)
        int nextLine = 4;
        String accLine1 = formatAccuracyString(1);
        String accLine2 = formatAccuracyString(2);
        
        if (accLine1.length() > 0) {
            if (canUpdateOled) {
                oledUpdated |= updateDisplayLine(nextLine, accLine1, SSD1306_WHITE, true);
            }
            if (canUpdateTft) {
                tftUpdated |= updateDisplayLine(nextLine, accLine1, TFT_MAGENTA, false);
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
        
        if (gpsData.total_gsa_sats > 0) {
            // Строка 1: Количество спутников
            String line1 = "Sats: " + String(gpsData.total_gsa_sats);
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
        NIMBLE_PROPERTY::NOTIFY
    );

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
    
    // Сбрасываем данные о спутниках каждой системы индивидуально если не обновлялись > 3 секунд
    if (currentTime - gpsData.lastGpsUpdate > 3000) {
        gpsData.gps_sats = 0;
    }
    if (currentTime - gpsData.lastGlonassUpdate > 3000) {
        gpsData.glonass_sats = 0;
    }
    if (currentTime - gpsData.lastGalileoUpdate > 3000) {
        gpsData.galileo_sats = 0;
    }
    if (currentTime - gpsData.lastBeidouUpdate > 3000) {
        gpsData.beidou_sats = 0;
    }
    if (currentTime - gpsData.lastQzssUpdate > 3000) {
        gpsData.qzss_sats = 0;
    }
    
    // Пересчитываем общий счетчик GSA спутников
    gpsData.total_gsa_sats = gpsData.gps_sats + gpsData.glonass_sats + gpsData.galileo_sats + gpsData.beidou_sats + gpsData.qzss_sats;
    
    // Сбрасываем данные о точности если GST не обновлялись > 10 секунд И нет GPS фикса
    // При RTK точность может обновляться реже, поэтому увеличиваем таймаут
    if ((currentTime - gpsData.lastGstUpdate > 10000 && gpsData.fixQuality < 4) ||  // Обычный GPS - 10 секунд
        (currentTime - gpsData.lastGstUpdate > 30000 && gpsData.fixQuality >= 4) ||  // RTK режимы - 30 секунд
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
                parseNmeaAccuracy(nmeaBuffer.c_str());
                nmeaBuffer = "";
            }
            
            // Парсим GPS данные через TinyGPS++
            if (gps.encode(c)) {
                if (gps.location.isValid()) {
                    gpsData.latitude = gps.location.lat();
                    gpsData.longitude = gps.location.lng();
                    gpsData.valid = true;
                    gpsData.lastUpdate = millis();
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
                // Читаем оптимальное количество данных (не больше 512 байт)
                size_t toRead = (available > 512) ? 512 : available;
                size_t bytesRead = readFromRingBuffer(bleTempBuffer, toRead);
                
                if (bytesRead > 0) {
                    // Отправляем данные через BLE
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
