#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// SPI OLED дисплей настройки
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// SPI пины для ESP8266
#define OLED_MOSI  D7  // SDA - Data
#define OLED_CLK   D5  // SCL - Clock  
#define OLED_DC    D2  // Data/Command
#define OLED_CS    D8  // Chip Select
#define OLED_RESET D3  // Reset

// Создаем объект дисплея для SPI
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT,
  OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS);

void setup() {
    Serial.begin(115200);
    Serial.println("\nTesting ESP8266 SSD1306 display via SPI...");
    
    // Инициализация дисплея по SPI
    Serial.println("Initializing SPI OLED display...");
    if(!display.begin(SSD1306_SWITCHCAPVCC)) {
        Serial.println("ERROR: SSD1306 SPI allocation failed!");
        Serial.println("Check SPI wiring:");
        Serial.println("- MOSI/SDA = D7");
        Serial.println("- CLK/SCL = D5"); 
        Serial.println("- DC = D2");
        Serial.println("- CS = D8");
        Serial.println("- RST = D3");
        for(;;); // Don't proceed, loop forever
    }
    
    Serial.println("SPI Display initialized successfully!");
    
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0,0);
    display.println("SPI OLED Test");
    display.println("ESP8266 Display");
    display.println("SSD1306 via SPI");
    display.println("MOSI=D7, CLK=D5");
    display.println("DC=D2, CS=D8");
    display.println("RST=D3");
    display.display();
    
    Serial.println("SPI display test complete!");
}

void loop() {
    // Мигание каждые 2 секунды для проверки
    delay(2000);
    
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0,0);
    display.println("SPI WORKING!");
    display.print("Time: ");
    display.print(millis()/1000);
    display.println("s");
    display.println("");
    display.println("SPI Interface");
    display.println("Test Success");
    display.display();
    
    delay(2000);
    
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(0,0);
    display.println("SPI");
    display.println("OLED");
    display.setTextSize(1);
    display.print("Counter: ");
    display.println(millis() / 4000);
    display.display();
}