#pragma once

#define USER_SETUP_INFO "User_Setup - ST7796 320x480"

#define ST7796_DRIVER

#define TFT_WIDTH 320
#define TFT_HEIGHT 480

#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS 15
#define TFT_DC 2
#define TFT_RST -1
#define TFT_BL 27

#define TOUCH_CS 33
#define TOUCH_IRQ 36

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define SMOOTH_FONT

#define SPI_FREQUENCY 27000000
