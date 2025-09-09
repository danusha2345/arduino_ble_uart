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
    int satellites = 0;  // Количество спутников в фиксе из GGA
    int total_gsa_sats = 0;  // Общий подсчет из GSA сообщений для сравнения
    int gps_sats = 0, glonass_sats = 0, galileo_sats = 0, beidou_sats = 0, qzss_sats = 0;  // Спутники в фиксе по системам
    int fixQuality = 0;  // Качество фикса из GGA (0-нет, 1-GPS, 2-DGPS, 4-RTK, 5-Float)
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

// Буфер для быстрой BLE передачи
uint8_t bleBuffer[512];
int bleBufferPos = 0;
unsigned long lastBleFlush = 0;

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

// Функция парсинга NMEA для получения точности и спутников
void parseNmeaAccuracy(String nmea) {
    // Парсим GST для получения точности в метрах
    if (nmea.startsWith("$GNGST") || nmea.startsWith("$GPGST")) {
        int commaIndex[9];
        int commaCount = 0;
        for (int i = 0; i < nmea.length() && commaCount < 9; i++) {
            if (nmea[i] == ',') {
                commaIndex[commaCount++] = i;
            }
        }
        if (commaCount >= 8) {
            // Поле 6: RMS lat error (север-юг)
            String latAccStr = nmea.substring(commaIndex[5] + 1, commaIndex[6]);
            latAccStr.trim();
            
            // Поле 7: RMS lon error (восток-запад)
            String lonAccStr = nmea.substring(commaIndex[6] + 1, commaIndex[7]);
            lonAccStr.trim();
            
            // Поле 8: RMS alt error (высота)
            String altAccStr = nmea.substring(commaIndex[7] + 1, commaIndex[8]);
            altAccStr.trim();
            
            bool hasValidAccuracy = false;
            
            if (latAccStr.length() > 0 && latAccStr.toFloat() > 0.0) {
                gpsData.latAccuracy = latAccStr.toFloat();
                hasValidAccuracy = true;
            }
            
            if (lonAccStr.length() > 0 && lonAccStr.toFloat() > 0.0) {
                gpsData.lonAccuracy = lonAccStr.toFloat();
                hasValidAccuracy = true;
            }
            
            if (altAccStr.length() > 0 && altAccStr.toFloat() > 0.0) {
                gpsData.verticalAccuracy = altAccStr.toFloat();
                hasValidAccuracy = true;
            }
            
            // Обновляем время только если получили валидные данные точности
            if (hasValidAccuracy) {
                gpsData.lastGstUpdate = millis();
            }
        }
    }
    
    
    // Парсим GNS для получения координат, количества спутников и качества фикса
    if (nmea.startsWith("$GNGNS") || nmea.startsWith("$GPGNS")) {
        int commaIndex[15]; int commaCount = 0;
        for (int i = 0; i < nmea.length() && commaCount < 15; i++) {
            if (nmea[i] == ',') commaIndex[commaCount++] = i;
        }
        if (commaCount >= 9) {
            // Координаты - поля 2-5
            String latStr = nmea.substring(commaIndex[1] + 1, commaIndex[2]);
            String latHem = nmea.substring(commaIndex[2] + 1, commaIndex[3]);
            String lonStr = nmea.substring(commaIndex[3] + 1, commaIndex[4]);
            String lonHem = nmea.substring(commaIndex[4] + 1, commaIndex[5]);
            
            if (latStr.length() > 0 && lonStr.length() > 0) {
                gpsData.latitude = convertToDecimalDegrees(latStr.toFloat());
                if (latHem == "S") gpsData.latitude = -gpsData.latitude;
                
                gpsData.longitude = convertToDecimalDegrees(lonStr.toFloat());
                if (lonHem == "W") gpsData.longitude = -gpsData.longitude;
                
                gpsData.valid = true;
                gpsData.lastUpdate = millis();
            }
            
            // Высота - поле 9
            String altStr = nmea.substring(commaIndex[8] + 1, commaIndex[9]);
            if (altStr.length() > 0) {
                gpsData.altitude = altStr.toFloat();
            }
            
            // Mode indicators - 6-ое поле (определяет качество фикса)
            String modeStr = nmea.substring(commaIndex[5] + 1, commaIndex[6]);
            if (modeStr.length() > 0) {
                char mode = modeStr[0]; // Первый символ показывает общий режим
                if (mode == 'N') gpsData.fixQuality = 0;      // No fix
                else if (mode == 'A') gpsData.fixQuality = 1; // Autonomous GPS
                else if (mode == 'D') gpsData.fixQuality = 2; // Differential GPS
                else if (mode == 'R') gpsData.fixQuality = 4; // RTK fixed
                else if (mode == 'F') gpsData.fixQuality = 5; // RTK float
                else gpsData.fixQuality = 1; // По умолчанию GPS
            }
            
            // Количество спутников в фиксе - 7-ое поле
            String satCountStr = nmea.substring(commaIndex[6] + 1, commaIndex[7]);
            if (satCountStr.length() > 0) {
                gpsData.satellites = satCountStr.toInt();
            }
        }
    }
    
    // Парсим GSA сообщения с полем talker ID (GNGSA, GPGSA, GLGSA, GAGSA, BDGSA)
    if (nmea.startsWith("$GNGSA") || nmea.startsWith("$GPGSA") || nmea.startsWith("$GLGSA") || 
        nmea.startsWith("$GAGSA") || nmea.startsWith("$BDGSA")) {
        // Разбиваем на поля
        int commaIndex[20]; int commaCount = 0;
        for (int i = 0; i < nmea.length() && commaCount < 20; i++) {
            if (nmea[i] == ',') commaIndex[commaCount++] = i;
        }
        
        if (commaCount >= 14) { // Нужно минимум поля с ID спутников
            // Подсчитываем непустые ID спутников в полях 3-14
            int count = 0;
            for (int i = 2; i < 14 && i < commaCount; i++) { // поля 3-14 (индексы 2-13)
                String satId = nmea.substring(commaIndex[i] + 1, commaIndex[i + 1]);
                satId.trim(); // Убираем пробелы
                if (satId.length() > 0 && !satId.equals("")) {
                    count++;
                }
            }
            
            unsigned long currentTime = millis();
            
            // Определяем систему по префиксу сообщения
            if (nmea.startsWith("$GPGSA")) {
                gpsData.gps_sats = count;        // GPS
                gpsData.lastGpsUpdate = currentTime;
            } else if (nmea.startsWith("$GLGSA")) {
                gpsData.glonass_sats = count;    // GLONASS
                gpsData.lastGlonassUpdate = currentTime;
            } else if (nmea.startsWith("$GAGSA")) {
                gpsData.galileo_sats = count;    // Galileo
                gpsData.lastGalileoUpdate = currentTime;
            } else if (nmea.startsWith("$BDGSA")) {
                gpsData.beidou_sats = count;     // BeiDou
                gpsData.lastBeidouUpdate = currentTime;
            } else if (nmea.startsWith("$GNGSA")) {
                // Для GNGSA используем talker ID
                if (commaCount >= 17) {
                    int asteriskPos = nmea.indexOf('*');
                    int lastComma = nmea.lastIndexOf(',');
                    String talkerStr = nmea.substring(lastComma + 1, asteriskPos);
                    int talkerId = talkerStr.toInt();
                    
                    if (talkerId == 1) {
                        gpsData.gps_sats = count;        // GPS
                        gpsData.lastGpsUpdate = currentTime;
                    } else if (talkerId == 2) {
                        gpsData.glonass_sats = count; // GLONASS
                        gpsData.lastGlonassUpdate = currentTime;
                    } else if (talkerId == 3) {
                        gpsData.galileo_sats = count; // Galileo
                        gpsData.lastGalileoUpdate = currentTime;
                    } else if (talkerId == 4) {
                        gpsData.beidou_sats = count;  // BeiDou
                        gpsData.lastBeidouUpdate = currentTime;
                    } else if (talkerId == 5) {
                        gpsData.qzss_sats = count;    // QZSS
                        gpsData.lastQzssUpdate = currentTime;
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
        case 4: return "RTK FIXED";
        case 5: return "RTK FLOAT";
        case 6: return "ESTIMATED";
        default: return "UNKNOWN";
    }
}

void updateDisplay() {
    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate < 1000) return; // Обновляем раз в секунду
    lastUpdate = millis();
    
    // Обновляем OLED дисплей
    display.clearDisplay();
    display.setCursor(0,0);
    display.setTextSize(1);
    
    // Обновляем TFT дисплей с кастомной конфигурацией
    tft->fillScreen(TFT_BLACK);
    tft->setCursor(0, 0);
    tft->setTextSize(2);
    
    if (gpsData.valid && (millis() - gpsData.lastUpdate < 5000)) {
        // OLED дисплей
        display.print("Sats: "); display.print(gpsData.total_gsa_sats);
        display.print(" Fix: "); display.println(getFixTypeString(gpsData.fixQuality));
        display.print("Lat: "); display.println(gpsData.latitude, 6);
        display.print("Lon: "); display.println(gpsData.longitude, 6);
        display.print("Alt: "); display.print(gpsData.altitude, 1); display.println("m");
        if (gpsData.latAccuracy < 99.9 || gpsData.lonAccuracy < 99.9) {
            display.print("N/S:"); display.print(gpsData.latAccuracy, 1);
            display.print(" E/W:"); display.print(gpsData.lonAccuracy, 1); display.println("m");
            display.print("H:"); display.print(gpsData.verticalAccuracy, 1); display.println("m");
        }
        display.print("G:"); display.print(gpsData.gps_sats);
        display.print(" R:"); display.print(gpsData.glonass_sats);
        display.print(" E:"); display.print(gpsData.galileo_sats);
        display.print(" B:"); display.print(gpsData.beidou_sats);
        if (gpsData.qzss_sats > 0) {
            display.print(" Q:"); display.print(gpsData.qzss_sats);
        }
        display.println();
        
        // TFT дисплей с цветным форматированием
        tft->setTextColor(TFT_GREEN);
        tft->print("Sats: "); tft->print(gpsData.total_gsa_sats);
        tft->setTextColor(TFT_CYAN);
        tft->print(" Fix: "); tft->println(getFixTypeString(gpsData.fixQuality));
        tft->setTextColor(TFT_WHITE);
        tft->print("Lat: "); tft->println(gpsData.latitude, 6);
        tft->print("Lon: "); tft->println(gpsData.longitude, 6);
        tft->setTextColor(TFT_YELLOW);
        tft->print("Alt: "); tft->print(gpsData.altitude, 1); tft->println("m");
        
        if (gpsData.latAccuracy < 99.9 || gpsData.lonAccuracy < 99.9) {
            tft->setTextColor(TFT_MAGENTA);
            tft->print("N/S: "); tft->print(gpsData.latAccuracy, 1);
            tft->print(" E/W: "); tft->print(gpsData.lonAccuracy, 1); tft->println("m");
            tft->print("H: "); tft->print(gpsData.verticalAccuracy, 1); tft->println("m");
        }
        
        tft->setTextColor(TFT_WHITE);
        tft->print("G:"); tft->print(gpsData.gps_sats);
        tft->print(" R:"); tft->print(gpsData.glonass_sats);
        tft->print(" E:"); tft->print(gpsData.galileo_sats);
        tft->print(" B:"); tft->print(gpsData.beidou_sats);
        if (gpsData.qzss_sats > 0) {
            tft->print(" Q:"); tft->print(gpsData.qzss_sats);
        }
        tft->println();
        
    } else {
        // OLED дисплей
        display.print("Fix: "); display.println(getFixTypeString(gpsData.fixQuality));
        if (gpsData.total_gsa_sats > 0) {
            display.print("Sats: "); display.println(gpsData.total_gsa_sats);
            display.print("G:"); display.print(gpsData.gps_sats);
            display.print(" R:"); display.print(gpsData.glonass_sats);
            display.print(" E:"); display.print(gpsData.galileo_sats);
            display.print(" B:"); display.print(gpsData.beidou_sats);
            if (gpsData.qzss_sats > 0) {
                display.print(" Q:"); display.print(gpsData.qzss_sats);
            }
            display.println();
        } else {
            display.println("Searching GPS...");
        }
        
        // TFT дисплей
        tft->setTextColor(TFT_ORANGE);
        tft->print("Fix: "); tft->println(getFixTypeString(gpsData.fixQuality));
        if (gpsData.total_gsa_sats > 0) {
            tft->setTextColor(TFT_GREEN);
            tft->print("Sats: "); tft->println(gpsData.total_gsa_sats);
            tft->setTextColor(TFT_WHITE);
            tft->print("G:"); tft->print(gpsData.gps_sats);
            tft->print(" R:"); tft->print(gpsData.glonass_sats);
            tft->print(" E:"); tft->print(gpsData.galileo_sats);
            tft->print(" B:"); tft->print(gpsData.beidou_sats);
            if (gpsData.qzss_sats > 0) {
                tft->print(" Q:"); tft->print(gpsData.qzss_sats);
            }
            tft->println();
        } else {
            tft->setTextColor(TFT_BLUE);
            tft->println("Searching GPS...");
        }
    }
    
    display.display();
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
        display.setCursor(0,0);
        display.println("GPS Bridge ESP32-C3");
        display.println("Waiting for GPS...");
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
    tft->setCursor(0, 0);
    tft->println("GPS Bridge ESP32-C3");
    tft->println("ST7789V TFT Ready");
    tft->println("Arduino_GFX v1.6.1");
    tft->println("Waiting for GPS...");
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
}

void checkDataTimeouts() {
    unsigned long currentTime = millis();
    
    // Сбрасываем данные о спутниках каждой системы индивидуально если не обновлялись > 5 секунд
    if (currentTime - gpsData.lastGpsUpdate > 5000) {
        gpsData.gps_sats = 0;
    }
    if (currentTime - gpsData.lastGlonassUpdate > 5000) {
        gpsData.glonass_sats = 0;
    }
    if (currentTime - gpsData.lastGalileoUpdate > 5000) {
        gpsData.galileo_sats = 0;
    }
    if (currentTime - gpsData.lastBeidouUpdate > 5000) {
        gpsData.beidou_sats = 0;
    }
    if (currentTime - gpsData.lastQzssUpdate > 5000) {
        gpsData.qzss_sats = 0;
    }
    
    // Пересчитываем общий счетчик GSA спутников
    gpsData.total_gsa_sats = gpsData.gps_sats + gpsData.glonass_sats + gpsData.galileo_sats + gpsData.beidou_sats + gpsData.qzss_sats;
    
    // Сбрасываем данные о точности если GST не обновлялись > 5 секунд ИЛИ нет GPS фикса
    if (currentTime - gpsData.lastGstUpdate > 5000 || !gpsData.valid || gpsData.fixQuality == 0) {
        gpsData.latAccuracy = 999.9;
        gpsData.lonAccuracy = 999.9;
        gpsData.verticalAccuracy = 999.9;
    }
}

void loop() {
    // Читаем и парсим GPS данные
    while (SerialPort.available()) {
        char c = SerialPort.read();
        
        // Собираем NMEA строку для парсинга точности
        nmeaBuffer += c;
        if (c == '\n') {
            parseNmeaAccuracy(nmeaBuffer);
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
        
        // Буферизуем данные для BLE
        if (deviceConnected) {
            bleBuffer[bleBufferPos++] = c;
            
            // Отправляем полные пакеты или по таймеру
            if (bleBufferPos >= 500 || (bleBufferPos > 0 && millis() - lastBleFlush > 20)) {
                pTxCharacteristic->setValue(bleBuffer, bleBufferPos);
                pTxCharacteristic->notify();
                bleBufferPos = 0;
                lastBleFlush = millis();
            }
        }
    }

    // Принудительно отправляем оставшиеся BLE данные по таймеру
    if (deviceConnected && bleBufferPos > 0 && millis() - lastBleFlush > 50) {
        pTxCharacteristic->setValue(bleBuffer, bleBufferPos);
        pTxCharacteristic->notify();
        bleBufferPos = 0;
        lastBleFlush = millis();
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
