/*
 * Pin Definitions for CrowPanel 7.0" HMI ESP32-S3 Display
 * Board: ESP32-S3-WROOM-1-N16R8
 * Display: 800x480 RGB565 (EK9716 + EK73002)
 * Touch: GT911 Capacitive
 */

#ifndef PINS_H
#define PINS_H

/* ===== Display (RGB LCD) ===== */
#define LCD_WIDTH       800
#define LCD_HEIGHT      480

/* RGB Data Pins - Red (5 bits: R0=LSB, R4=MSB) */
#define LCD_R0          14
#define LCD_R1          21
#define LCD_R2          47
#define LCD_R3          48
#define LCD_R4          45

/* RGB Data Pins - Green (6 bits: G0=LSB, G5=MSB) */
#define LCD_G0          9
#define LCD_G1          46
#define LCD_G2          3
#define LCD_G3          8
#define LCD_G4          16
#define LCD_G5          1

/* RGB Data Pins - Blue (5 bits: B0=LSB, B4=MSB) */
#define LCD_B0          15
#define LCD_B1          7
#define LCD_B2          6
#define LCD_B3          5
#define LCD_B4          4

/* RGB Control Pins */
#define LCD_DE          41
#define LCD_VSYNC       40
#define LCD_HSYNC       39
#define LCD_PCLK        0

/* Backlight */
#define LCD_BL          2

/* LCD Timing Parameters (from Elecrow official V3.0 code) */
#define LCD_PCLK_HZ         15000000  /* 15 MHz pixel clock */
#define LCD_HSYNC_POLARITY  0
#define LCD_HSYNC_FRONT     40
#define LCD_HSYNC_PULSE     48
#define LCD_HSYNC_BACK      40
#define LCD_VSYNC_POLARITY  0
#define LCD_VSYNC_FRONT     1
#define LCD_VSYNC_PULSE     31
#define LCD_VSYNC_BACK      13
#define LCD_PCLK_ACTIVE_NEG 1

/* PCA9557 I/O Expander (controls display enable) */
#define PCA9557_ADDR        0x18

/* ===== Touch (GT911 I2C) ===== */
#define TOUCH_SDA       19
#define TOUCH_SCL       20
#define TOUCH_INT       18
#define TOUCH_RST       38

/* ===== SD Card (SPI) ===== */
#define SD_CS           10
#define SD_MOSI         11
#define SD_SCLK         12
#define SD_MISO         13

#endif /* PINS_H */
