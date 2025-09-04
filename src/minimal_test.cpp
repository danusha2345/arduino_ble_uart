#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

void testDisplay(uint8_t address, uint8_t sda, uint8_t scl) {
    Serial.print("Testing I2C: SDA=D");
    Serial.print((sda == 14) ? 6 : (sda == 12) ? 5 : (sda == 4) ? 2 : (sda == 5) ? 1 : sda);
    Serial.print("(GPIO");
    Serial.print(sda);
    Serial.print("), SCL=D");
    Serial.print((scl == 14) ? 6 : (scl == 12) ? 5 : (scl == 4) ? 2 : (scl == 5) ? 1 : scl);
    Serial.print("(GPIO");
    Serial.print(scl);
    Serial.print("), Address=0x");
    Serial.println(address, HEX);
    
    // Инициализация I2C с заданными пинами
    Wire.begin(sda, scl);
    delay(100);
    
    // Проверяем, отвечает ли устройство по I2C
    Wire.beginTransmission(address);
    uint8_t error = Wire.endTransmission();
    
    if (error == 0) {
        Serial.println("  ✓ I2C device found!");
        
        // Пробуем инициализировать дисплей
        Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
        
        if(display.begin(SSD1306_SWITCHCAPVCC, address)) {
            Serial.println("  ✓ Display initialized successfully!");
            
            display.clearDisplay();
            display.setTextSize(1);
            display.setTextColor(SSD1306_WHITE);
            display.setCursor(0,0);
            display.println("*** FOUND! ***");
            display.print("SDA=D");
            display.print((sda == 14) ? 6 : (sda == 12) ? 5 : (sda == 4) ? 2 : (sda == 5) ? 1 : sda);
            display.print(" SCL=D");
            display.println((scl == 14) ? 6 : (scl == 12) ? 5 : (scl == 4) ? 2 : (scl == 5) ? 1 : scl);
            display.print("Addr: 0x");
            display.println(address, HEX);
            display.println("SUCCESS!");
            display.display();
            
            Serial.println("  *** DISPLAY IS WORKING! ***");
            delay(5000);
            return;
        } else {
            Serial.println("  ✗ Display init failed");
        }
    } else {
        Serial.print("  ✗ I2C error: ");
        Serial.println(error);
    }
    
    delay(200);
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("\n=== COMPREHENSIVE OLED DISPLAY TEST ===");
    Serial.println("Testing all common pin combinations and addresses");
    Serial.println();
    
    // Тестируем все возможные комбинации
    uint8_t addresses[] = {0x3C, 0x3D};
    
    // Стандартные пины ESP8266 NodeMCU (GPIO номера)
    uint8_t pinCombos[][2] = {
        {14, 12},   // D6, D5 (ваша текущая комбинация)
        {4, 5},     // D2, D1 (стандартные I2C)
        {5, 4},     // D1, D2 (обратный порядок)
        {12, 14},   // D5, D6 (обратный порядок)
        {0, 2},     // D3, D4
        {13, 15},   // D7, D8
        {16, 5},    // D0, D1
    };
    
    int numCombos = sizeof(pinCombos) / sizeof(pinCombos[0]);
    int numAddresses = sizeof(addresses) / sizeof(addresses[0]);
    
    Serial.print("Testing ");
    Serial.print(numCombos);
    Serial.print(" pin combinations with ");
    Serial.print(numAddresses);
    Serial.println(" addresses each");
    Serial.println("===========================================");
    
    for (int i = 0; i < numCombos; i++) {
        for (int j = 0; j < numAddresses; j++) {
            testDisplay(addresses[j], pinCombos[i][0], pinCombos[i][1]);
        }
        Serial.println();
    }
    
    Serial.println("===========================================");
    Serial.println("Test complete. If no display worked, check:");
    Serial.println("1. Physical connections");
    Serial.println("2. Power supply to display (3.3V)");
    Serial.println("3. Display type (should be SSD1306)");
    Serial.println("4. Try SPI connection instead of I2C");
    Serial.println("5. Check if display is damaged");
}

void loop() {
    // Мигающий светодиод показывает что ESP работает
    digitalWrite(LED_BUILTIN, HIGH);
    delay(1000);
    digitalWrite(LED_BUILTIN, LOW);
    delay(1000);
    
    static int counter = 0;
    counter++;
    if (counter % 10 == 0) {
        Serial.println("ESP8266 is running. Check results above.");
    }
}