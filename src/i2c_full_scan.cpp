#include <Wire.h>

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n=== ПОЛНОЕ СКАНИРОВАНИЕ I2C ===");
    Serial.println("Проверяем все возможные комбинации...\n");
    
    // Массив возможных пинов SDA и SCL
    uint8_t sda_pins[] = {0, 2, 4, 5, 12, 13, 14, 15, 16}; // D3, D4, D2, D1, D6, D7, D5, D8, D0
    uint8_t scl_pins[] = {0, 2, 4, 5, 12, 13, 14, 15, 16};
    
    // Проверяем стандартные комбинации первыми
    Serial.println("=== СТАНДАРТНЫЕ КОМБИНАЦИИ ===");
    testPinCombination(14, 12, "D5(GPIO14)=SDA, D6(GPIO12)=SCL (стандарт NodeMCU)");
    testPinCombination(4, 5, "D2(GPIO4)=SDA, D1(GPIO5)=SCL (стандарт ESP8266)");
    testPinCombination(12, 14, "D6(GPIO12)=SDA, D5(GPIO14)=SCL (обратная)");
    testPinCombination(5, 4, "D1(GPIO5)=SDA, D2(GPIO4)=SCL (обратная)");
    
    Serial.println("\n=== ВСЕ ВОЗМОЖНЫЕ КОМБИНАЦИИ ===");
    for (int s = 0; s < 9; s++) {
        for (int c = 0; c < 9; c++) {
            if (sda_pins[s] != scl_pins[c]) { // SDA и SCL должны быть разными
                String desc = "GPIO" + String(sda_pins[s]) + "=SDA, GPIO" + String(scl_pins[c]) + "=SCL";
                testPinCombination(sda_pins[s], scl_pins[c], desc);
            }
        }
    }
    
    Serial.println("\n=== СКАНИРОВАНИЕ ЗАВЕРШЕНО ===");
}

void testPinCombination(uint8_t sda, uint8_t scl, String description) {
    Serial.print("Тест: ");
    Serial.println(description);
    
    Wire.begin(sda, scl);
    delay(100);
    
    bool found = false;
    for (uint8_t address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        uint8_t error = Wire.endTransmission();
        
        if (error == 0) {
            Serial.print("  ✓ НАЙДЕНО устройство по адресу 0x");
            if (address < 16) Serial.print("0");
            Serial.print(address, HEX);
            
            // Пробуем определить тип устройства
            if (address == 0x3C || address == 0x3D) {
                Serial.print(" (возможно SSD1306 OLED)");
            } else if (address == 0x27) {
                Serial.print(" (возможно LCD с I2C)");
            } else if (address == 0x48 || address == 0x49) {
                Serial.print(" (возможно ADS1115 ADC)");
            } else if (address == 0x68) {
                Serial.print(" (возможно DS1307 RTC)");
            }
            Serial.println();
            found = true;
        }
    }
    
    if (!found) {
        Serial.println("  ✗ Устройства не найдены");
    }
    
    delay(500);
}

void loop() {
    // Пустой loop
}