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

bool displayInitialized = false;

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("\n=== MINIMAL MAIN PROGRAM ===");
    Serial.println("Testing same sequence as main program but minimal...");
    
    // Точно такая же инициализация как в main_esp8266.cpp
    Serial.println("Initializing I2C and display...");
    
    // Принудительная инициализация I2C с увеличенными задержками
    Serial.println("Forcing I2C power cycle...");
    pinMode(D6, OUTPUT); // SDA
    pinMode(D5, OUTPUT); // SCL
    digitalWrite(D6, LOW);
    digitalWrite(D5, LOW);
    delay(100);
    digitalWrite(D6, HIGH);
    digitalWrite(D5, HIGH);
    delay(100);
    
    // Повторная инициализация I2C
    Wire.begin(D6, D5);
    Wire.setClock(100000); // Снижаем частоту до 100kHz для надежности
    delay(500); // Большая задержка для стабилизации
    
    // Проверяем наличие I2C устройства несколько раз
    bool deviceFound = false;
    for (int attempt = 0; attempt < 5; attempt++) {
        Serial.print("Attempt ");
        Serial.print(attempt + 1);
        Serial.print(": ");
        
        Wire.beginTransmission(SCREEN_ADDRESS);
        uint8_t error = Wire.endTransmission();
        
        if (error == 0) {
            Serial.println("Device found!");
            deviceFound = true;
            break;
        } else {
            Serial.print("Error ");
            Serial.println(error);
            delay(200);
        }
    }
    
    if (!deviceFound) {
        Serial.println("ERROR: Display not responding after 5 attempts");
        Serial.println("Display will be disabled");
        displayInitialized = false;
        return;
    }
    
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
    display.println("MINIMAL MAIN");
    display.println("Display works!");
    display.println("");
    display.println("Next: add WiFi");
    display.display();
    
    Serial.println("SUCCESS: Display should show text!");
}

void loop() {
    if (!displayInitialized) return;
    
    // Обновляем дисплей каждые 2 секунды
    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate > 2000) {
        lastUpdate = millis();
        
        display.clearDisplay();
        display.setCursor(0,0);
        display.setTextSize(1);
        display.println("MINIMAL MAIN");
        display.println("Running...");
        display.println("");
        display.print("Uptime: ");
        display.print(millis() / 1000);
        display.println("s");
        display.println("");
        display.println("Ready for WiFi!");
        display.display();
        
        Serial.print("Display updated, uptime: ");
        Serial.print(millis() / 1000);
        Serial.println("s");
    }
}