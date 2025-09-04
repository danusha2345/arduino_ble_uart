#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>

// Создаем объект дисплея u8g2 для SSD1306 128x64 I2C
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

void setup() {
    Serial.begin(115200);
    Serial.println("\nTesting ESP8266 SSD1306 display with u8g2...");
    
    // Установка кастомных пинов I2C
    Wire.begin(D6, D5); // SDA=D6, SCL=D5
    
    // Инициализация дисплея
    Serial.println("Initializing u8g2 display...");
    u8g2.begin();
    
    Serial.println("u8g2 Display initialized!");
    
    // Настройка дисплея
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);
    
    u8g2.drawStr(0, 10, "Hello u8g2!");
    u8g2.drawStr(0, 22, "ESP8266 Display");
    u8g2.drawStr(0, 34, "SSD1306 Test");
    u8g2.drawStr(0, 46, "Address: 0x3D");
    u8g2.drawStr(0, 58, "Pins: D6(SDA),D5(SCL)");
    
    u8g2.sendBuffer();
    Serial.println("Display output sent!");
}

void loop() {
    // Мигание каждые 2 секунды для проверки
    delay(2000);
    
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_10x20_tf);
    u8g2.drawStr(0, 20, "WORKING!");
    
    u8g2.setFont(u8g2_font_6x10_tf);
    String timeStr = "Time: " + String(millis()/1000) + "s";
    u8g2.drawStr(0, 40, timeStr.c_str());
    
    u8g2.sendBuffer();
    
    delay(2000);
    
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(0, 10, "u8g2 Test Display");
    
    String counterStr = "Counter: " + String(millis() / 4000);
    u8g2.drawStr(0, 25, counterStr.c_str());
    
    u8g2.drawStr(0, 40, "I2C: D6(SDA), D5(SCL)");
    u8g2.drawStr(0, 55, "Address: 0x3D");
    
    u8g2.sendBuffer();
}