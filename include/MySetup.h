#pragma once

// ---------- Driver ----------
#define ILI9341_DRIVER

// ---------- ขนาดจอ (portrait native) ----------
#define TFT_WIDTH  240
#define TFT_HEIGHT 320

// ---------- ขาต่อ VSPI ----------
#define TFT_MISO 19
#define TFT_MOSI 23
#define TFT_SCLK 18
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST   4
#define TFT_BL   -1     // ใส่เลขขาถ้าคุม backlight ผ่าน GPIO

// ---------- ฟอนต์ ----------
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF
#define SMOOTH_FONT     // จำเป็นสำหรับฟอนต์ไทย .vlw

// ---------- SPI ----------
#define SPI_FREQUENCY       40000000
#define SPI_READ_FREQUENCY  20000000