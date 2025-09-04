#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// OLED дисплей настройки  
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("\n=== SIMPLE MAIN (like working test) ===");
    
    // ТОЧНО КАК В ПРОСТОЙ РАБОЧЕЙ ПРОГРАММЕ - без принудительного сброса
    Wire.begin(D6, D5); // SDA=D6(GPIO14), SCL=D5(GPIO12)
    delay(100);
    
    Serial.print("Initializing display at address 0x");
    Serial.println(SCREEN_ADDRESS, HEX);
    
    if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println("ERROR: SSD1306 allocation failed!");
        return;
    }
    
    Serial.println("Display initialized successfully!");
    
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0,0);
    display.println("SIMPLE MAIN");
    display.println("Works like test!");
    display.println("");
    display.println("Same as minimal");
    display.println("test program");
    display.display();
    
    Serial.println("SUCCESS: Display should show text!");
}

void loop() {
    // Обновляем дисплей каждые 2 секунды
    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate > 2000) {
        lastUpdate = millis();
        
        display.clearDisplay();
        display.setCursor(0,0);
        display.setTextSize(1);
        display.println("SIMPLE MAIN");
        display.println("Running...");
        display.println("");
        display.print("Uptime: ");
        display.print(millis() / 1000);
        display.println("s");
        display.println("");
        display.println("No power cycle!");
        display.display();
        
        Serial.print("Display updated, uptime: ");
        Serial.print(millis() / 1000);
        Serial.println("s");
    }
}