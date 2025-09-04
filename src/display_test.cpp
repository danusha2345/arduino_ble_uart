#include <Arduino.h>
#include "SSD1306Wire.h"

// Создаем объект дисплея: адрес 0x3D, SDA=D6, SCL=D5
SSD1306Wire display(0x3D, D6, D5);

void setup() {
    Serial.begin(115200);
    Serial.println("\nTesting ESP8266 SSD1306 display...");
    
    // Инициализация дисплея
    Serial.println("Initializing display...");
    display.init();
    
    Serial.println("Display initialized!");
    display.flipScreenVertically();
    
    Serial.println("Testing display output...");
    display.clear();
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(ArialMT_Plain_10);
    
    display.drawString(0, 0, "Hello World!");
    display.drawString(0, 12, "ESP8266 Display");
    display.drawString(0, 24, "SSD1306 Test");
    display.drawString(0, 36, "Address: 0x3D");
    display.drawString(0, 48, "Pins: D6,D5");
    
    display.display();
    Serial.println("Display output sent!");
}

void loop() {
    // Мигание каждые 2 секунды для проверки
    delay(2000);
    
    display.clear();
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(ArialMT_Plain_10);
    display.drawString(0, 0, "WORKING!");
    display.drawString(0, 12, String(millis()));
    display.display();
    
    delay(2000);
    
    display.clear();
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(ArialMT_Plain_10);
    display.drawString(0, 0, "Test Display");
    display.drawString(0, 12, "Counter: " + String(millis() / 4000));
    display.display();
}