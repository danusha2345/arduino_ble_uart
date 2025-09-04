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

// OLED дисплей настройки
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// WiFi настройки Access Point
const char* ap_ssid = "ESP8266-UART-Bridge";
const char* ap_password = "123456789";

void debugPrint(const char* step) {
    Serial.print("[DEBUG] ");
    Serial.println(step);
    delay(500); // Пауза для стабильности
}

bool setupDisplaySafe() {
    debugPrint("Starting display setup...");
    
    debugPrint("Checking I2C device...");
    Wire.beginTransmission(SCREEN_ADDRESS);
    uint8_t error = Wire.endTransmission();
    
    if (error != 0) {
        Serial.print("[ERROR] I2C device not found at 0x");
        Serial.print(SCREEN_ADDRESS, HEX);
        Serial.print(", error: ");
        Serial.println(error);
        return false;
    }
    debugPrint("I2C device found!");
    
    debugPrint("Initializing display...");
    if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        debugPrint("[ERROR] Display initialization failed!");
        return false;
    }
    debugPrint("Display initialized successfully!");
    
    debugPrint("Testing display output...");
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0,0);
    display.println("DEBUG MODE");
    display.println("Display working!");
    display.println("Address: 0x3C");
    display.println("Pins: D6,D5");
    display.display();
    debugPrint("Display test complete!");
    
    return true;
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("\n=== ESP8266 DEBUG MODE ===");
    
    debugPrint("Step 1: Initialize I2C");
    Wire.begin(D6, D5); // SDA=D6(GPIO14), SCL=D5(GPIO12)
    debugPrint("I2C initialized on D6/D5");
    
    debugPrint("Step 2: Setup Display");
    bool displayOK = setupDisplaySafe();
    if (!displayOK) {
        debugPrint("Display setup failed - continuing without display");
    } else {
        debugPrint("Display setup successful!");
    }
    
    debugPrint("Step 3: WiFi AP setup");
    WiFi.mode(WIFI_AP);
    bool wifiOK = WiFi.softAP(ap_ssid, ap_password);
    if (!wifiOK) {
        debugPrint("[ERROR] WiFi AP setup failed!");
        return;
    }
    debugPrint("WiFi AP created successfully");
    
    IPAddress myIP = WiFi.softAPIP();
    Serial.print("[DEBUG] AP IP: ");
    Serial.println(myIP);
    
    debugPrint("Step 4: Update display with WiFi info");
    if (displayOK) {
        display.clearDisplay();
        display.setCursor(0,0);
        display.setTextSize(1);
        display.println("WiFi Ready!");
        display.print("SSID: ");
        display.println(ap_ssid);
        display.print("IP: ");
        display.println(myIP);
        display.println("");
        display.println("Status: OK");
        display.display();
        debugPrint("Display updated with WiFi info");
    }
    
    debugPrint("Step 5: Setup complete!");
    Serial.println("\n=== SETUP COMPLETE ===");
    Serial.println("Check display - it should show WiFi info");
    Serial.println("Connect to WiFi network: " + String(ap_ssid));
    Serial.println("Password: " + String(ap_password));
    Serial.println("========================");
}

void loop() {
    static unsigned long lastUpdate = 0;
    static int counter = 0;
    
    if (millis() - lastUpdate > 5000) {
        counter++;
        
        Serial.print("[LOOP] Running... Counter: ");
        Serial.println(counter);
        
        // Обновляем дисплей каждые 5 секунд
        display.clearDisplay();
        display.setCursor(0,0);
        display.setTextSize(1);
        display.println("System Running");
        display.print("Uptime: ");
        display.print(millis()/1000);
        display.println("s");
        display.print("Counter: ");
        display.println(counter);
        display.println("");
        display.println("WiFi: Active");
        display.print("IP: ");
        display.println(WiFi.softAPIP());
        display.display();
        
        lastUpdate = millis();
    }
    
    delay(100);
}