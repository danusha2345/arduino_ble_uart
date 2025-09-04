#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

// Попробуем разные адреса
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void testDisplay(uint8_t address) {
    Serial.print("Тестируем адрес 0x");
    Serial.println(address, HEX);
    
    // Создаем объект с нужным адресом
    if(!display.begin(SSD1306_SWITCHCAPVCC, address)) {
        Serial.println("Ошибка инициализации дисплея");
        return;
    }
    
    Serial.println("Дисплей инициализирован!");
    
    // Очищаем и тестируем
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0,0);
    display.println("OLED Test");
    display.println("Address: 0x" + String(address, HEX));
    display.println("");
    display.println("SDA=D6 (GPIO14)");
    display.println("SCL=D5 (GPIO12)");
    display.display();
    
    Serial.println("Тест отображен на экране!");
    delay(3000);
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("=== OLED Test ===");
    
    // Инициализация I2C
    Wire.begin(14, 12); // SDA=14, SCL=12 (D6, D5)
    Serial.println("I2C инициализирован: SDA=14, SCL=12");
    
    // I2C сканирование
    Serial.println("Сканирование I2C...");
    byte count = 0;
    for (byte i = 8; i < 120; i++) {
        Wire.beginTransmission(i);
        if (Wire.endTransmission() == 0) {
            Serial.print("Найдено I2C устройство: 0x");
            if (i < 16) Serial.print("0");
            Serial.println(i, HEX);
            count++;
        }
    }
    
    if (count == 0) {
        Serial.println("I2C устройства не найдены!");
    } else {
        Serial.print("Найдено устройств: ");
        Serial.println(count);
    }
    
    // Тестируем популярные адреса
    Serial.println("\nТестирование дисплея...");
    testDisplay(0x3C);
    testDisplay(0x3D);
}

void loop() {
    Serial.println("Тест завершен. Сброс для повтора.");
    delay(10000);
}