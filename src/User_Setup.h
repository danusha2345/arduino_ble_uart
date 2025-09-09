//                            USER SETUP FOR ST7789V ESP32-C3
//   Set driver type, fonts to be loaded, pins used and SPI control method etc
//
// This is a custom setup for ESP32-C3 Super Mini with ST7789V 135x240 TFT display

// ST7789V driver
#define ST7789_DRIVER      // Full configuration option, define additional parameters below for this display

// Configure ST7789V display 
#define TFT_WIDTH  135
#define TFT_HEIGHT 240

// Define the color order for ST7789V
#define TFT_RGB_ORDER TFT_BGR  // Colour order Blue-Green-Red

// For ESP32-C3 Super Mini with ST7789V on SPI
#define TFT_MISO  -1      // Not connected for ST7789V
#define TFT_MOSI   1      // SPI Data  - GPIO1
#define TFT_SCLK   0      // SPI Clock - GPIO0
#define TFT_CS    -1      // Not used with this display (permanently connected to GND or tied low)
#define TFT_DC     2      // Data Command control pin - GPIO2  
#define TFT_RST    9      // Reset pin - GPIO9
#define TFT_BL    10      // LED back-light - GPIO10

// Can be used with SPI port 1 or 2
#define TFT_SPI_PORT 1

// Processor type and SPI speed
#define SPI_FREQUENCY  20000000 // 20MHz SPI clock

// Optional reduced SPI frequency for reading TFT
#define SPI_READ_FREQUENCY  10000000

// SPI port will be initialized when tft.begin() is called.
#define LOAD_GLCD   // Font 1. Original Adafruit 8 pixel font needs ~1820 bytes in FLASH
#define LOAD_FONT2  // Font 2. Small 16 pixel high font, needs ~3534 bytes in FLASH, 96 characters
#define LOAD_FONT4  // Font 4. Medium 26 pixel high font, needs ~5848 bytes in FLASH, 96 characters
#define LOAD_FONT6  // Font 6. Large 48 pixel font, needs ~2666 bytes in FLASH, only characters 1234567890:-.apm
#define LOAD_FONT7  // Font 7. 7 segment 48 pixel font, needs ~2438 bytes in FLASH, only characters 1234567890:-.
#define LOAD_FONT8  // Font 8. Large 75 pixel font needs ~3256 bytes in FLASH, only characters 1234567890:-.
#define LOAD_GFXFF  // FreeFonts. Include access to the 48 Adafruit_GFX free fonts FF1 to FF48 and custom fonts

// Comment out the #define below to stop the SPIFFS filing system and smooth font code being loaded
// this will save ~20kbytes of FLASH
#define SMOOTH_FONT

// Comment out the #define below to stop the Select SPI port option appearing in the library
#define TFT_SPI_OVERLAP