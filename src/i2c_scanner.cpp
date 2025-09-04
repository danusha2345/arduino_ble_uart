#include <Arduino.h>
#include <Wire.h>

// Возможные пины для ESP8266
int sdaPins[] = {0, 1, 2, 3, 4, 5, 12, 13, 14, 15, 16};
int sclPins[] = {0, 1, 2, 3, 4, 5, 12, 13, 14, 15, 16};
int numPins = 11;

void scanI2C(int sda, int scl) {
    Wire.begin(sda, scl);
    delay(100); // Дать время на инициализацию
    
    byte count = 0;
    
    for (byte addr = 8; addr < 120; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.print("    Device found at 0x");
            if (addr < 16) Serial.print("0");
            Serial.print(addr, HEX);
            Serial.println();
            count++;
        }
    }
    
    if (count > 0) {
        Serial.print("  >>> FOUND ");
        Serial.print(count);
        Serial.println(" DEVICE(S) <<<");
    }
    
    // Wire.end() не поддерживается на ESP8266
}

void setup() {
    Serial.begin(115200);
    Serial.println("\n=== ESP8266 Universal I2C Scanner ===");
    Serial.println("Testing all possible SDA/SCL pin combinations...\n");
    
    for (int i = 0; i < numPins; i++) {
        for (int j = 0; j < numPins; j++) {
            if (sdaPins[i] != sclPins[j]) { // SDA и SCL должны быть разными
                Serial.print("Testing SDA=GPIO");
                Serial.print(sdaPins[i]);
                Serial.print(" (D");
                
                // Перевод GPIO в D-номера для NodeMCU
                switch(sdaPins[i]) {
                    case 16: Serial.print("0"); break;
                    case 5:  Serial.print("1"); break;
                    case 4:  Serial.print("2"); break;
                    case 0:  Serial.print("3"); break;
                    case 2:  Serial.print("4"); break;
                    case 14: Serial.print("5"); break;
                    case 12: Serial.print("6"); break;
                    case 13: Serial.print("7"); break;
                    case 15: Serial.print("8"); break;
                    case 3:  Serial.print("9/RX"); break;
                    case 1:  Serial.print("10/TX"); break;
                    default: Serial.print("?"); break;
                }
                
                Serial.print("), SCL=GPIO");
                Serial.print(sclPins[j]);
                Serial.print(" (D");
                
                switch(sclPins[j]) {
                    case 16: Serial.print("0"); break;
                    case 5:  Serial.print("1"); break;
                    case 4:  Serial.print("2"); break;
                    case 0:  Serial.print("3"); break;
                    case 2:  Serial.print("4"); break;
                    case 14: Serial.print("5"); break;
                    case 12: Serial.print("6"); break;
                    case 13: Serial.print("7"); break;
                    case 15: Serial.print("8"); break;
                    case 3:  Serial.print("9/RX"); break;
                    case 1:  Serial.print("10/TX"); break;
                    default: Serial.print("?"); break;
                }
                
                Serial.println(")");
                
                scanI2C(sdaPins[i], sclPins[j]);
                
                delay(500); // Пауза между тестами
            }
        }
    }
    
    Serial.println("\n=== Scan complete ===");
    Serial.println("Check results above for working pin combinations.");
}

void loop() {
    // После завершения сканирования просто ждём
    delay(10000);
    Serial.println("Scan completed. Reset to run again.");
}