#include <Arduino.h>

void setup() {
    Serial.begin(9600);   // Медленная скорость
    delay(2000);
    Serial.println("ESP8266 Test - 9600 baud");
    Serial.println("If you see this, communication works!");
    
    Serial.begin(115200); // Переключаемся на быструю
    delay(1000);
    Serial.println("ESP8266 Test - 115200 baud");
    Serial.println("Communication test successful!");
}

void loop() {
    Serial.println("ESP8266 is running...");
    delay(2000);
}