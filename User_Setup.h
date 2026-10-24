// User setup for this project (copiado de platformio.ini build_flags)
#define USER_SETUP_INFO "User_Setup - ST7796 320x480"

// Display driver
#define ST7796_DRIVER

// Display size in portrait
#define TFT_WIDTH 320
#define TFT_HEIGHT 480

// SPI pin assignments (ESP32)
#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15
#define TFT_DC   2
#define TFT_RST  -1
#define TFT_BL   27

// Touch controller (XPT2046 / compatible)
// Ajusta este pin si tu módulo táctil usa otro CS. En muchos módulos
// ESP32 + pantalla 3.2/3.5" con touch, suele ser GPIO 33.
#define TOUCH_CS 33
#define TOUCH_IRQ 36

// Fonts
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define SMOOTH_FONT

// SPI frequency (optional)
#define SPI_FREQUENCY  27000000
