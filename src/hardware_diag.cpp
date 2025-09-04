#include <Arduino.h>
#include <Wire.h>

void setup() {
    Serial.begin(115200);
    delay(2000);
    
    Serial.println("\n=== ESP8266 HARDWARE DIAGNOSTICS ===");
    Serial.println("Testing display connection step by step...\n");
    
    // Тест 1: Базовая информация о системе
    Serial.println("1. SYSTEM INFO:");
    Serial.print("   Chip ID: 0x");
    Serial.println(ESP.getChipId(), HEX);
    Serial.print("   Free Heap: ");
    Serial.println(ESP.getFreeHeap());
    Serial.print("   CPU Frequency: ");
    Serial.println(ESP.getCpuFreqMHz());
    Serial.println();
    
    // Тест 2: Проверка пинов GPIO
    Serial.println("2. GPIO PIN TEST:");
    Serial.println("   Testing D5 (GPIO12 - SCL) and D6 (GPIO14 - SDA)");
    
    pinMode(D5, OUTPUT);
    pinMode(D6, OUTPUT);
    
    Serial.print("   D5 HIGH: ");
    digitalWrite(D5, HIGH);
    delay(100);
    Serial.println(digitalRead(D5) ? "OK" : "FAIL");
    
    Serial.print("   D5 LOW:  ");
    digitalWrite(D5, LOW);
    delay(100);
    Serial.println(digitalRead(D5) ? "FAIL" : "OK");
    
    Serial.print("   D6 HIGH: ");
    digitalWrite(D6, HIGH);
    delay(100);
    Serial.println(digitalRead(D6) ? "OK" : "FAIL");
    
    Serial.print("   D6 LOW:  ");
    digitalWrite(D6, LOW);
    delay(100);
    Serial.println(digitalRead(D6) ? "FAIL" : "OK");
    Serial.println();
    
    // Тест 3: I2C инициализация на разных частотах
    Serial.println("3. I2C FREQUENCY TEST:");
    uint32_t frequencies[] = {50000, 100000, 200000, 400000};
    
    for (int i = 0; i < 4; i++) {
        Serial.print("   Frequency ");
        Serial.print(frequencies[i]);
        Serial.print(" Hz: ");
        
        Wire.begin(D6, D5);
        Wire.setClock(frequencies[i]);
        delay(100);
        
        Wire.beginTransmission(0x3C);
        uint8_t error = Wire.endTransmission();
        
        if (error == 0) {
            Serial.println("DEVICE FOUND!");
            break;
        } else {
            Serial.print("Error ");
            Serial.println(error);
        }
        delay(200);
    }
    Serial.println();
    
    // Тест 4: Полное сканирование I2C адресов
    Serial.println("4. I2C ADDRESS SCAN:");
    Wire.begin(D6, D5);
    Wire.setClock(100000);
    delay(500);
    
    int deviceCount = 0;
    for (uint8_t address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        uint8_t error = Wire.endTransmission();
        
        if (error == 0) {
            Serial.print("   Device found at 0x");
            if (address < 16) Serial.print("0");
            Serial.print(address, HEX);
            
            if (address == 0x3C) Serial.print(" <- SSD1306 OLED");
            else if (address == 0x3D) Serial.print(" <- SSD1306 OLED (alt)");
            
            Serial.println();
            deviceCount++;
        }
        delay(10);
    }
    
    if (deviceCount == 0) {
        Serial.println("   NO I2C DEVICES FOUND!");
    } else {
        Serial.print("   Total devices found: ");
        Serial.println(deviceCount);
    }
    Serial.println();
    
    // Тест 5: Проверка питания
    Serial.println("5. POWER TEST:");
    Serial.print("   VCC: ");
    Serial.print(ESP.getVcc() / 1024.0);
    Serial.println("V");
    Serial.println();
    
    // Тест 6: I2C сигналы
    Serial.println("6. I2C SIGNAL TEST:");
    Serial.println("   Sending I2C signals manually...");
    
    pinMode(D6, OUTPUT); // SDA
    pinMode(D5, OUTPUT); // SCL
    
    // I2C Start condition
    digitalWrite(D6, HIGH);
    digitalWrite(D5, HIGH);
    delayMicroseconds(10);
    digitalWrite(D6, LOW);  // SDA low while SCL high = START
    delayMicroseconds(10);
    digitalWrite(D5, LOW);
    delayMicroseconds(10);
    
    Serial.println("   Start condition sent");
    
    // I2C Stop condition  
    digitalWrite(D6, LOW);
    digitalWrite(D5, HIGH);
    delayMicroseconds(10);
    digitalWrite(D6, HIGH);  // SDA high while SCL high = STOP
    delayMicroseconds(10);
    
    Serial.println("   Stop condition sent");
    Serial.println();
    
    Serial.println("=== DIAGNOSTICS COMPLETE ===");
    Serial.println("Connect oscilloscope or logic analyzer to:");
    Serial.println("- D5 (GPIO12) - SCL");  
    Serial.println("- D6 (GPIO14) - SDA");
    Serial.println("- GND");
    Serial.println("- 3.3V");
}

void loop() {
    // Continuous I2C scanning every 5 seconds
    delay(5000);
    
    Serial.println("--- Continuous scan ---");
    Wire.begin(D6, D5);
    Wire.setClock(100000);
    
    Wire.beginTransmission(0x3C);
    uint8_t error = Wire.endTransmission();
    
    Serial.print("0x3C: ");
    if (error == 0) {
        Serial.println("FOUND");
    } else {
        Serial.print("Error ");
        Serial.println(error);
    }
}