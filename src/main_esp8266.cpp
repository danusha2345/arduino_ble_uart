#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <TinyGPS++.h>
#include <SoftwareSerial.h>

// --- OLED Дисплей ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --- WiFi ---
const char* ap_ssid = "ESP8266-UART-Bridge";
const char* ap_password = "123456789";

// --- Веб-сервер и WebSocket ---
ESP8266WebServer server(80);
WebSocketsServer webSocket(81);

// --- GPS ---
#define GPS_BAUD 460800 // Ваша скорость  
TinyGPSPlus gps;

// --- Глобальные переменные ---
bool clientConnected = false;
bool displayInitialized = false;
unsigned long lastDisplayUpdate = 0;
String uartBuffer = "";

struct GPSData {
    double latitude = 0.0, longitude = 0.0;
    double hdop = 999.9, verticalAccuracy = 999.9, horizontalAccuracy = 999.9;
    int satellites = 0, fixQuality = 0;
    String fixType = "NO FIX";
    bool valid = false;
    unsigned long lastUpdate = 0;
} gpsData;

// --- Функции ---

void setupDisplay() {
    Wire.begin(D5, D6); // SDA, SCL
    if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println("ERROR: SSD1306 allocation failed!");
        displayInitialized = false;
        return;
    }
    displayInitialized = true;
    Serial.println("Display initialized successfully!");
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0,0);
    display.println("Display OK!");
    display.display();
}

String getFixTypeString(int quality) {
    switch(quality) {
        case 1: return "GPS"; case 2: return "DGPS"; case 4: return "RTK";
        case 5: return "FLOAT"; default: return "NO FIX";
    }
}

void parseNmeaAccuracy(String nmea) {
    if (nmea.startsWith("$GNGST") || nmea.startsWith("$GPGST")) {
        int commaIndex[8]; int commaCount = 0;
        for (int i = 0; i < nmea.length() && commaCount < 8; i++) if (nmea[i] == ',') commaIndex[commaCount++] = i;
        if (commaCount >= 7) {
            String hAccStr = nmea.substring(commaIndex[5] + 1, commaIndex[6]);
            String vAccStr = nmea.substring(commaIndex[6] + 1, commaIndex[7]);
            if (hAccStr.length() > 0) gpsData.horizontalAccuracy = hAccStr.toFloat();
            if (vAccStr.length() > 0) gpsData.verticalAccuracy = vAccStr.toFloat();
        }
    }
    if (nmea.startsWith("$GNGGA") || nmea.startsWith("$GPGGA")) {
        int commaIndex[15]; int commaCount = 0;
        for (int i = 0; i < nmea.length() && commaCount < 15; i++) if (nmea[i] == ',') commaIndex[commaCount++] = i;
        if (commaCount >= 8) {
            String qualityStr = nmea.substring(commaIndex[5] + 1, commaIndex[6]);
            if (qualityStr.length() > 0) {
                gpsData.fixQuality = qualityStr.toInt();
                gpsData.fixType = getFixTypeString(gpsData.fixQuality);
            }
            String satStr = nmea.substring(commaIndex[6] + 1, commaIndex[7]);
            if (satStr.length() > 0) gpsData.satellites = satStr.toInt();
            String hdopStr = nmea.substring(commaIndex[7] + 1, commaIndex[8]);
            if (hdopStr.length() > 0) gpsData.hdop = hdopStr.toFloat();
        }
    }
}

void updateDisplay() {
    if (!displayInitialized || millis() - lastDisplayUpdate < 1000) return;
    
    display.clearDisplay();
    display.setCursor(0,0);
    display.setTextSize(1);
    display.println("GPS Bridge");
    display.println("---------------");
    
    if (gpsData.fixQuality > 0 && (millis() - gpsData.lastUpdate < 5000)) {
        display.print("Fix: "); display.print(gpsData.fixType);
        display.print(" SAT: "); display.println(gpsData.satellites);
        display.print("Lat: "); display.println(gpsData.latitude, 6);
        display.print("Lon: "); display.println(gpsData.longitude, 6);
        display.print("H: "); display.print(gpsData.horizontalAccuracy, 1);
        display.print(" V: "); display.print(gpsData.verticalAccuracy, 1); display.println("m");
    } else {
        display.print("Fix: "); display.print(gpsData.fixType);
        display.print(" SAT: "); display.println(gpsData.satellites);
        display.println("Searching for GPS...");
        display.print("IP: "); display.println(WiFi.softAPIP());
        display.print("WS: "); display.println(clientConnected ? "ON" : "OFF");
    }
    display.display();
    lastDisplayUpdate = millis();
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    if (type == WStype_CONNECTED) clientConnected = true;
    else if (type == WStype_DISCONNECTED) clientConnected = false;
}

void setup() {
    Serial.begin(115200); // Стандартная скорость для логов
    Serial.println("\nStarting ESP8266 Bridge with Hardware UART...");
    
    setupDisplay();
    
    // GPS подключен к RX пину (GPIO3) - это Serial0 RX!
    // НЕ можем использовать Serial для GPS, так как это USB порт
    // Используем SoftwareSerial на GPIO3
    Serial.printf("WARNING: GPS на RX пине (GPIO3) конфликтует с USB!\n");
    Serial.println("Рекомендуется переподключить GPS к другому пину");
    
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ap_ssid, ap_password);
    Serial.print("AP IP address: ");
    Serial.println(WiFi.softAPIP());
    
    server.on("/", HTTP_GET, []() {
        server.send(200, "text/plain", "Bridge is active.");
    });
    server.begin();
    
    webSocket.begin();
    webSocket.onEvent(webSocketEvent);
    
    Serial.println("Setup finished. Ready.");
}

void loop() {
    server.handleClient();
    webSocket.loop();

    static unsigned long lastGpsCheck = 0;
    String wsBuffer = "";
    int bytesRead = 0;
    
    while (Serial1.available() > 0) {
        char c = Serial1.read();
        bytesRead++;
        
        if (clientConnected) {
            wsBuffer += c;
        }
        
        uartBuffer += c;
        
        if (gps.encode(c)) {
            if (gps.location.isValid()) {
                gpsData.latitude = gps.location.lat();
                gpsData.longitude = gps.location.lng();
                gpsData.valid = true;
                gpsData.lastUpdate = millis();
                Serial.printf("GPS: %.6f, %.6f\n", gpsData.latitude, gpsData.longitude);
            }
            if (gps.satellites.isValid()) {
                gpsData.satellites = gps.satellites.value();
                Serial.printf("Satellites: %d\n", gpsData.satellites);
            }
        }
    }

    // Debug каждые 5 секунд
    if (millis() - lastGpsCheck > 5000) {
        lastGpsCheck = millis();
        if (bytesRead == 0) {
            Serial.println("WARNING: No GPS data received in last 5 seconds!");
            Serial.println("Check GPS connection to D4 (GPIO2)");
        } else {
            Serial.printf("GPS OK: %d bytes in last 5 sec\n", bytesRead);
        }
        bytesRead = 0;
    }

    // Отправляем накопленные данные в WebSocket одним пакетом
    if (clientConnected && wsBuffer.length() > 0) {
        webSocket.broadcastTXT(wsBuffer);
    }

    // Обрабатываем все полные строки в буфере
    int lineEnd;
    while ((lineEnd = uartBuffer.indexOf('\n')) != -1) {
        String line = uartBuffer.substring(0, lineEnd + 1);
        Serial.print("NMEA: ");
        Serial.print(line);
        parseNmeaAccuracy(line);
        uartBuffer = uartBuffer.substring(lineEnd + 1);
    }
    
    updateDisplay();
    yield();
}
