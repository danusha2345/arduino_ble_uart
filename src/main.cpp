#include <Arduino.h>
#include <NimBLEDevice.h>
#include <HardwareSerial.h>
#include "esp_wifi.h" // Required for disabling power saving
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <TinyGPS++.h>

// Используем второй аппаратный UART (Serial1, т.к. Serial0 занят USB)
// RX-пин: GPIO8, TX-пин: GPIO10
HardwareSerial SerialPort(1);

// I2C Display настройки
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x78 // I2C адрес дисплея
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// GPS
TinyGPSPlus gps;

// I2C пины для ESP32-C3
#define SDA_PIN 3
#define SCL_PIN 4

// GPS данные
struct GPSData {
    double latitude = 0.0, longitude = 0.0;
    double horizontalAccuracy = 999.9;
    double verticalAccuracy = 999.9;
    int satellites = 0;  // Количество спутников в фиксе из GGA
    int gps_sats = 0, glonass_sats = 0, galileo_sats = 0, beidou_sats = 0;  // Спутники в фиксе по системам
    bool valid = false;
    unsigned long lastUpdate = 0;
} gpsData;

// Буфер для парсинга NMEA строк
String nmeaBuffer = "";

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
            // Горизонтальная точность - 6-ое поле (индекс 5)
            String hAccStr = nmea.substring(commaIndex[5] + 1, commaIndex[6]);
            hAccStr.trim();
            if (hAccStr.length() > 0) {
                gpsData.horizontalAccuracy = hAccStr.toFloat();
            }
            // Вертикальная точность - 7-ое поле (индекс 6)
            String vAccStr = nmea.substring(commaIndex[6] + 1, commaIndex[7]);
            vAccStr.trim();
            if (vAccStr.length() > 0) {
                gpsData.verticalAccuracy = vAccStr.toFloat();
            }
        }
    }
    
    
    // Парсим GGA для получения количества спутников в фиксе
    if (nmea.startsWith("$GNGGA") || nmea.startsWith("$GPGGA")) {
        int commaIndex[15]; int commaCount = 0;
        for (int i = 0; i < nmea.length() && commaCount < 15; i++) {
            if (nmea[i] == ',') commaIndex[commaCount++] = i;
        }
        if (commaCount >= 7) {
            // Количество спутников в фиксе - 7-ое поле
            String satCountStr = nmea.substring(commaIndex[6] + 1, commaIndex[7]);
            if (satCountStr.length() > 0) {
                gpsData.satellites = satCountStr.toInt();
            }
        }
    }
    
    // Парсим GNGSA сообщения с полем talker ID
    if (nmea.startsWith("$GNGSA")) {
        // Разбиваем на поля
        int commaIndex[20]; int commaCount = 0;
        for (int i = 0; i < nmea.length() && commaCount < 20; i++) {
            if (nmea[i] == ',') commaIndex[commaCount++] = i;
        }
        
        if (commaCount >= 17) { // Нужно минимум 17 полей
            // Получаем talker ID - последнее поле перед чексуммой
            int asteriskPos = nmea.indexOf('*');
            int lastComma = nmea.lastIndexOf(',');
            String talkerStr = nmea.substring(lastComma + 1, asteriskPos);
            int talkerId = talkerStr.toInt();
            
            // Подсчитываем непустые ID спутников в полях 3-14
            int count = 0;
            for (int i = 2; i < 14 && i < commaCount - 3; i++) { // поля 3-14 (индексы 2-13)
                String satId = nmea.substring(commaIndex[i] + 1, commaIndex[i + 1]);
                satId.trim(); // Убираем пробелы
                if (satId.length() > 0 && !satId.equals("")) {
                    count++;
                }
            }
            
            // Определяем систему по talker ID
            if (talkerId == 1) gpsData.gps_sats = count;        // GPS
            else if (talkerId == 2) gpsData.glonass_sats = count; // GLONASS  
            else if (talkerId == 3) gpsData.galileo_sats = count; // Galileo
            else if (talkerId == 4 || talkerId == 5) gpsData.beidou_sats = count;  // BeiDou
        }
    }
}

void updateDisplay() {
    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate < 1000) return; // Обновляем раз в секунду
    lastUpdate = millis();
    
    display.clearDisplay();
    display.setCursor(0,0);
    display.setTextSize(1);
    
    if (gpsData.valid && (millis() - gpsData.lastUpdate < 5000)) {
        display.print("Sats: "); display.println(gpsData.satellites);
        display.print("Lat: "); display.println(gpsData.latitude, 6);
        display.print("Lon: "); display.println(gpsData.longitude, 6);
        if (gpsData.horizontalAccuracy < 99.9) {
            display.print("H: "); display.print(gpsData.horizontalAccuracy, 1);
            display.print(" V: "); display.print(gpsData.verticalAccuracy, 1); display.println("m");
        }
        display.print("G:"); display.print(gpsData.gps_sats);
        display.print(" R:"); display.print(gpsData.glonass_sats);
        display.print(" E:"); display.print(gpsData.galileo_sats);
        display.print(" C:"); display.println(gpsData.beidou_sats);
    } else {
        display.println("Searching GPS...");
        if (gpsData.satellites > 0) {
            display.print("G:"); display.print(gpsData.gps_sats);
            display.print(" R:"); display.print(gpsData.glonass_sats);
            display.print(" E:"); display.print(gpsData.galileo_sats);
            display.print(" C:"); display.println(gpsData.beidou_sats);
        } else {
            display.print("Sats: "); display.println(gpsData.satellites);
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

    // Инициализация I2C и дисплея
    Wire.begin(SDA_PIN, SCL_PIN);
    if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS >> 1)) { // Адрес делим на 2!
        Serial.println("ERROR: SSD1306 allocation failed!");
    } else {
        Serial.println("Display initialized successfully!");
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(0,0);
        display.println("GPS Bridge ESP32-C3");
        display.println("Waiting for GPS...");
        display.display();
    }

    // Запускаем UART1 для передачи данных
    // RX = 8, TX = 10
    SerialPort.begin(460800, SERIAL_8N1, 8, 10);

    // Инициализация BLE
    NimBLEDevice::init("ESP32-BLE-UART");

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
    // Устанавливаем минимальный и максимальный интервал подключения для высокой скорости
    pAdvertising->setMinPreferred(0x06); // 7.5ms
    pAdvertising->setMaxPreferred(0x0C); // 15ms
    NimBLEDevice::startAdvertising();
    Serial.println("Advertising started. Waiting for a client connection...");
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
        
        // Если устройство подключено, отправляем сырые данные по BLE
        if (deviceConnected) {
            uint8_t data = c;
            pTxCharacteristic->setValue(&data, 1);
            pTxCharacteristic->notify();
        }
    }

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
