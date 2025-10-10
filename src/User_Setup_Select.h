// User_Setup_Select.h для TFT_eSPI - выбор конфигурации для ESP32-S3
#define USER_SETUP_INFO "User setup for ESP32-S3 ST7789V"

#define ST7789_DRIVER

#define TFT_WIDTH  135
#define TFT_HEIGHT 240

#define CGRAM_OFFSET      // Library will add offsets required

//#define TFT_RGB_ORDER TFT_BGR  // Colour order Blue-Green-Red

//#define TFT_INVERSION_ON

// Пины подключения для ESP32-S3 (новая планировка)
#define TFT_MISO -1
#define TFT_MOSI 11  // GP11
#define TFT_SCLK 12  // GP12
#define TFT_CS   -1  // Не используется
#define TFT_DC   10  // GP10
#define TFT_RST  9   // GP9

#define LOAD_GLCD   // Font 1. Original Adafruit 8 pixel font needs ~1820 bytes in FLASH
#define LOAD_FONT2  // Font 2. Small 16 pixel high font, needs ~3534 bytes in FLASH
#define LOAD_FONT4  // Font 4. Medium 26 pixel high font, needs ~5848 bytes in FLASH
#define LOAD_FONT6  // Font 6. Large 48 pixel font, needs ~2666 bytes in FLASH
#define LOAD_FONT7  // Font 7. 7 segment 48 pixel font, needs ~2438 bytes in FLASH
#define LOAD_FONT8  // Font 8. Large 75 pixel font needs ~3256 bytes in FLASH
#define LOAD_GFXFF  // FreeFonts. Include access to FreeFonts
#define SMOOTH_FONT

#define SPI_FREQUENCY  27000000
#define SPI_READ_FREQUENCY  20000000

// Подсветка
#define TFT_BL 13  // GP13
#define TFT_BACKLIGHT_ON HIGH