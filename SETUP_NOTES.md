# CrowPanel 7.0" HMI ESP32-S3 — PlatformIO Setup Notes

Comprehensive documentation of the entire setup and debugging process for getting
the CrowPanel 7.0" HMI ESP32-S3 Display (800x480 RGB TFT LCD Touch Screen)
working with PlatformIO, Arduino framework, Arduino_GFX, TAMC_GT911, and LVGL.

---

## 1. Hardware Identification

### Board Specifications (confirmed via esptool & diagnostics)
- **MCU**: ESP32-S3 (QFN56, revision v0.2)
- **Flash**: 4MB (DIO mode) — despite the silkscreen labeling "N16R8"
- **PSRAM**: 8MB Embedded (AP_3v3, OPI mode)
- **Wi-Fi/BT**: Wi-Fi + BT 5 (LE), Dual Core + LP Core, 240MHz
- **Crystal**: 40MHz
- **USB-UART**: CH340 chip (appears as COM7) — **NOT USB CDC**
- **Display**: 800×480 RGB565 TFT LCD, driven by EK9716 + EK73002
- **Touch Controller**: GT911 Capacitive (I2C)
- **I/O Expander**: PCA9557 (I2C at 0x18) — controls display enable/reset
- **SD Card Slot**: SPI interface

### I2C Bus Devices
| Address | Device         | Purpose                        |
|---------|----------------|--------------------------------|
| 0x18    | PCA9557        | I/O expander, display enable   |
| 0x5D    | GT911          | Capacitive touch controller    |

### Board Version
This is a **V3.0** board. Elecrow has released multiple hardware revisions (V1.X,
V2.X, V3.0). The V3.0 version uses the PCA9557 I/O expander for display
initialization, which is **not present** on earlier versions. The version label is
on the back of the board.

---

## 2. PlatformIO Project Configuration

### platformio.ini
```ini
[env:crowpanel-7]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino

; Flash & PSRAM — 4MB Flash (DIO) + 8MB OPI PSRAM
board_build.arduino.memory_type = dio_opi
board_build.flash_mode = dio
board_build.flash_size = 4MB
board_upload.flash_size = 4MB
board_upload.maximum_size = 4194304
board_build.partitions = huge_app.csv

monitor_speed = 115200
upload_speed = 921600

build_flags =
    -DBOARD_HAS_PSRAM
    -DARDUINO_USB_CDC_ON_BOOT=0
    -DLV_CONF_INCLUDE_SIMPLE
    -I include
lib_ldf_mode = deep+

lib_deps =
    moononournation/GFX Library for Arduino@^1.4.9
    tamctec/TAMC_GT911@^1.0.2
    lvgl/lvgl@^8.4.0
```

### Key Configuration Decisions

#### Memory Type: `dio_opi`
- **DIO** = Dual I/O for flash (the board's 4MB flash uses DIO mode)
- **OPI** = Octal SPI for PSRAM (the embedded 8MB PSRAM uses OPI interface)
- Using the wrong memory type causes `PSRAM ID read error` on boot

#### Flash Size: 4MB (not 16MB)
- The board silkscreen says "N16R8" suggesting 16MB flash + 8MB PSRAM
- **In reality, esptool confirmed only 4MB flash**
- Using a 16MB partition table causes: `Detected flash size 4MB is smaller than the size in partition table (6MB)`

#### Partition Table: `huge_app.csv`
- Standard for 4MB flash with a single large app partition (~3MB)
- Leaves no OTA or SPIFFS partition — adequate for development

#### ARDUINO_USB_CDC_ON_BOOT=0
- The board uses a **CH340 USB-UART** chip, not native USB CDC
- Setting this to 1 (default for ESP32-S3) redirects Serial output to the native USB port, making the CH340 serial port silent
- **Must be 0** to see any Serial output on the CH340 COM port

#### lib_ldf_mode: `deep+`
- Required for LVGL to find `lv_conf.h` in the project's `include/` directory
- Without this, LVGL fonts fail to link (undefined references to `lv_font_montserrat_*`)

#### `-I include` Build Flag
- Ensures the compiler searches the project's `include/` directory for headers
- Required because `lv_conf.h` lives in `include/` and LVGL needs to find it

---

## 3. Pin Mapping

### The Pin Mapping Problem
Initial pin assignments were guessed from common ESP32-S3 RGB panel examples.
**Every single pin was wrong.** The correct pin mapping was obtained from Elecrow's
official V3.0 source code on GitHub:
`Elecrow-RD/CrowPanel-ESP32-Display-Course-File` → `7.0 v3.0 touch new code`

### Correct Pin Assignments (pins.h)

#### RGB Data Pins (16-bit RGB565)
```
Red   (5 bits): R0=GPIO14, R1=GPIO21, R2=GPIO47, R3=GPIO48, R4=GPIO45
Green (6 bits): G0=GPIO9,  G1=GPIO46, G2=GPIO3,  G3=GPIO8,  G4=GPIO16, G5=GPIO1
Blue  (5 bits): B0=GPIO15, B1=GPIO7,  B2=GPIO6,  B3=GPIO5,  B4=GPIO4
```

#### RGB Control Pins
```
DE (Data Enable) = GPIO41
VSYNC            = GPIO40
HSYNC            = GPIO39
PCLK             = GPIO0    ← This was the most surprising — NOT GPIO42
Backlight        = GPIO2
```

#### Touch Pins (GT911 I2C)
```
SDA = GPIO19
SCL = GPIO20
INT = GPIO18
RST = GPIO38
```

#### SD Card Pins (SPI)
```
CS   = GPIO10
MOSI = GPIO11
SCLK = GPIO12
MISO = GPIO13
```

### LCD Timing Parameters (from Elecrow official code)
```
Pixel Clock:      15 MHz
HSYNC Polarity:   0 (active low)
HSYNC Front Porch: 40
HSYNC Pulse Width: 48
HSYNC Back Porch:  40
VSYNC Polarity:   0 (active low)
VSYNC Front Porch: 1
VSYNC Pulse Width: 31
VSYNC Back Porch:  13
PCLK Active Neg:   1
```

---

## 4. Initialization Sequence

The CrowPanel V3.0 requires a specific initialization order. Deviating from this
sequence results in a black screen or corrupted display.

### Required Boot Sequence
```
1. Serial.begin(115200)
2. Pull GPIO38 (touch RST) LOW          ← Required before I2C init
3. Wire.begin(GPIO19, GPIO20)           ← Start I2C bus
4. PCA9557 init sequence:               ← Enable display via I/O expander
     a. Write reg 0x01 = 0x00            (all outputs LOW)
     b. Write reg 0x03 = 0x00            (all pins as OUTPUT)
     c. delay(20ms)
     d. Write reg 0x01 = 0x01            (IO0 HIGH = display enable)
     e. delay(100ms)
     f. Write reg 0x03 = 0x02            (IO1 as input)
5. gfx->begin()                         ← Initialize RGB LCD panel
6. gfx->fillScreen(BLACK)
7. Backlight ON (GPIO2 HIGH)
8. ts.begin(0x5D)                        ← Initialize GT911 touch
9. ts.setRotation(ROTATION_INVERTED)     ← Raw coordinate passthrough
10. LVGL init, register drivers, create UI
```

### PCA9557 I/O Expander (0x18)
This is the **most critical discovery** of the entire setup process. The PCA9557
is a simple I2C I/O expander. On the CrowPanel V3.0 board, it controls the display
enable signal. Without initializing the PCA9557 and setting IO0 HIGH, the display
receives no data and shows only a backlit blank screen.

**Register Map (used registers):**
| Register | Address | Purpose           |
|----------|---------|-------------------|
| Output   | 0x01    | Pin output values |
| Config   | 0x03    | Pin direction (0=output, 1=input) |

The init sequence sets all pins as outputs, drives them LOW briefly, then raises
IO0 (display enable) while configuring IO1 as input.

---

## 5. Issues Encountered and Solutions

### Issue 1: Build Errors — `BLACK` Not Declared
- **Symptom**: `'BLACK' was not declared in this scope`
- **Cause**: Arduino_GFX doesn't define `BLACK` as a constant
- **Fix**: Use `0x0000` instead of `BLACK`

### Issue 2: Build Errors — `LV_LOG_LEVEL` Redefined
- **Symptom**: Warning about `LV_LOG_LEVEL` being redefined
- **Cause**: Defined in both lv_conf.h and LVGL defaults
- **Fix**: Wrapped in `#if LV_USE_LOG` guard

### Issue 3: Linker Error — `lv_demo_widgets` Undefined
- **Symptom**: `undefined reference to 'lv_demo_widgets'`
- **Cause**: LVGL demo functions are disabled in lv_conf.h
- **Fix**: Replaced with custom UI (button + counter)

### Issue 4: Linker Errors — LVGL Fonts Undefined
- **Symptom**: `undefined reference to 'lv_font_montserrat_14'` etc.
- **Cause**: LVGL couldn't find `lv_conf.h` in the project's include directory
- **Fix**: Added `-I include` build flag + `lib_ldf_mode = deep+`

### Issue 5: Flash Partition Too Large
- **Symptom**: `Partition table offset 0x640000 exceeds flash chip size 0x400000`
- **Cause**: Used a 16MB partition table on a 4MB flash chip
- **Fix**: Changed to `board_build.partitions = huge_app.csv` (4MB partition table)

### Issue 6: PSRAM Init Failure
- **Symptom**: `quad_psram: PSRAM ID read error: 0x00000000`
- **Cause**: Wrong `memory_type` setting — tried `qio_opi`, `qio_qspi`, `dio_opi`
- **Fix**: `board_build.arduino.memory_type = dio_opi` matches the hardware
- **Verification**: Diagnostic sketch confirmed `PSRAM size: 8388608 bytes`

### Issue 7: No Serial Output
- **Symptom**: Serial monitor on COM7 showed nothing
- **Cause**: `ARDUINO_USB_CDC_ON_BOOT=1` routes Serial to native USB, not CH340
- **Fix**: Set `ARDUINO_USB_CDC_ON_BOOT=0` to route Serial to the CH340 UART

### Issue 8: I2C Errors in Loop
- **Symptom**: Continuous I2C error messages from GT911 touch polling
- **Cause**: GT911 was being polled at address 0x14 but actual address is 0x5D
- **Fix**: I2C scan discovered GT911 at 0x5D; changed `ts.begin(0x5D)`

### Issue 9: Display Blank — Backlight On But No Pixels (THE BIG ONE)
- **Symptom**: `gfx->begin()` returns OK, framebuffer valid, all draw operations
  complete without error, but screen shows only backlight with no visible pixels
- **Root Cause**: THREE separate problems:
  1. **PCA9557 not initialized** — display enable signal never asserted
  2. **All RGB data pins were wrong** — incorrect GPIO assignments
  3. **PCLK pin wrong** — was GPIO42, should be GPIO0
  4. **LCD timing parameters wrong** — polarity, porch, pulse values all different
- **Fix**: Obtained correct configuration from Elecrow's official V3.0 source code
- **Key Insight**: The I2C device at 0x18 (initially mysterious) is the PCA9557
  I/O expander that gates the display. This is specific to V3.0 boards.

### Issue 10: Touch Coordinates Inverted
- **Symptom**: Pressing the button doesn't register; pressing elsewhere triggers it
- **Cause**: `ROTATION_NORMAL` in TAMC_GT911 inverts both X and Y (`x = width - x;
  y = height - y;`), but the CrowPanel's touch panel already has correct orientation
- **Fix**: Changed to `ROTATION_INVERTED` which passes coordinates through unchanged

### Issue 11: Display Tearing/Shifting on Touch
- **Symptom**: Display content shifts position and tears when touching the screen
- **Cause**: I2C transactions (GT911 touch reads) interrupt the DMA that feeds LCD
  pixel data. Without a bounce buffer, the RGB LCD signal timing is disrupted.
- **Fix**: Added bounce buffer to the RGB panel constructor:
  `LCD_WIDTH * 10 /* bounce_buffer_size_px */`
- **How it works**: The bounce buffer is an intermediate SRAM buffer. The DMA reads
  from the bounce buffer (in fast internal SRAM) instead of directly from the
  framebuffer (in slower PSRAM). This decouples the LCD refresh from PSRAM access
  and prevents I2C bus contention from causing display glitches.

---

## 6. Libraries and Versions

| Library                      | Version | Purpose                          |
|------------------------------|---------|----------------------------------|
| espressif32 (platform)       | 53.3.10 | ESP32 PlatformIO platform        |
| framework-arduinoespressif32 | 3.1.0   | Arduino core for ESP32           |
| framework-arduinoespressif32-libs | 5.3.0 | ESP-IDF 5.x libraries        |
| GFX Library for Arduino      | 1.6.5   | Display driver (Arduino_GFX)     |
| TAMC_GT911                   | 1.0.2   | GT911 capacitive touch driver    |
| lvgl                         | 8.4.0   | Embedded GUI framework           |

---

## 7. File Structure

```
Elecrow 7 inch/
├── platformio.ini          # PlatformIO project configuration
├── include/
│   ├── pins.h              # All hardware pin definitions & LCD timing
│   └── lv_conf.h           # LVGL configuration (RGB565, fonts, etc.)
├── src/
│   ├── main.cpp            # Main application (LVGL + touch + display)
│   └── main.cpp.bak        # Backup of old LVGL app (wrong pin config)
└── SETUP_NOTES.md          # This file
```

---

## 8. Display Constructor Explained

```cpp
// RGB Panel — hardware timing layer
Arduino_ESP32RGBPanel *rgbpanel = new Arduino_ESP32RGBPanel(
    LCD_DE, LCD_VSYNC, LCD_HSYNC, LCD_PCLK,      // Control pins
    LCD_R0..R4, LCD_G0..G5, LCD_B0..B4,           // 16 data pins (RGB565)
    LCD_HSYNC_POLARITY, front, pulse, back,        // HSYNC timing
    LCD_VSYNC_POLARITY, front, pulse, back,        // VSYNC timing
    LCD_PCLK_ACTIVE_NEG, LCD_PCLK_HZ,             // Pixel clock config
    false,  // useBigEndian — RGB565 is little-endian
    0,      // de_idle_high — DE idle state
    0,      // pclk_idle_high — PCLK idle state
    LCD_WIDTH * 10  // bounce_buffer_size_px — CRITICAL for I2C coexistence
);

// Display — high-level drawing API
Arduino_RGB_Display *gfx = new Arduino_RGB_Display(
    LCD_WIDTH, LCD_HEIGHT, rgbpanel,
    0,     // rotation
    true   // auto_flush — push pixels to LCD automatically
);
```

### Why the Bounce Buffer Matters
The ESP32-S3 RGB LCD peripheral uses DMA to continuously read pixel data from the
framebuffer in PSRAM and stream it to the LCD. When I2C transactions occur (e.g.,
reading touch data from the GT911), they can interfere with PSRAM access timing,
causing the DMA to read corrupted or offset data. The bounce buffer solves this by
copying pixel lines to fast internal SRAM first, so the DMA reads from SRAM (which
is immune to I2C bus contention) instead of directly from PSRAM.

---

## 9. LVGL Configuration Notes (lv_conf.h)

Key settings in `include/lv_conf.h`:
- Color depth: 16-bit (RGB565), `LV_COLOR_DEPTH 16`
- Color swap: `LV_COLOR_16_SWAP 0` (native byte order)
- Memory: `LV_MEM_SIZE 48KB` (LVGL internal heap)
- Display buffers: 2× double-buffered, 800×48 pixels each, allocated in PSRAM
- Fonts enabled: Montserrat 12, 14, 16, 18, 20, 24, 40, 48
- Tick source: `LV_TICK_CUSTOM 1` using Arduino `millis()`
- Demos: All disabled (custom UI in `create_ui()`)

---

## 10. Useful Commands

### Build
```
pio run -d "c:\# PROJECTS #2\Elecrow 7 inch"
```

### Upload (COM7)
```
pio run -d "c:\# PROJECTS #2\Elecrow 7 inch" -t upload --upload-port COM7
```

### Erase Flash (if needed)
```
python -m esptool --chip esp32s3 --port COM7 erase-flash
```

### Serial Monitor
```
pio device monitor --port COM7 --baud 115200
```

### Clean Build
```
pio run -d "c:\# PROJECTS #2\Elecrow 7 inch" -t clean
```

---

## 11. Elecrow Official Resources

- **GitHub**: `Elecrow-RD/CrowPanel-ESP32-Display-Course-File`
  - V3.0 code: `CrowPanel_ESP32_Tutorial/Code/V3.0/`
  - 7" V3.0 touch code: `CrowPanel_ESP32_Tutorial/Code/7.0 v3.0 touch new code/`
- **PCA9557 library**: Required by Elecrow's Arduino IDE examples (we use raw I2C
  instead to avoid adding another library dependency)
- **Board version video**: Search "Get Started with ESP32: Comparing ESP32 Board
  Versions" on YouTube

---

## Summary

Setting up the CrowPanel 7.0" HMI ESP32-S3 with PlatformIO required solving 11
distinct issues spanning build configuration, hardware identification, and runtime
debugging:

1. **The board has 4MB flash (not 16MB)** despite the "N16R8" label, but does have
   the full 8MB PSRAM. Use `dio_opi` memory type and `huge_app.csv` partition.

2. **Serial output requires `ARDUINO_USB_CDC_ON_BOOT=0`** because the board uses a
   CH340 USB-UART chip, not the ESP32-S3's native USB.

3. **The display won't show anything without PCA9557 initialization**. The V3.0
   board uses an I2C I/O expander at address 0x18 to control the display enable
   signal. This is the single most important discovery — without it, the screen is
   permanently blank.

4. **Every pin assignment from generic ESP32-S3 examples was wrong.** The correct
   pin mapping must come from Elecrow's official source code. Most critically,
   PCLK is GPIO0 (not GPIO42), and DE/VSYNC are swapped vs common assumptions.

5. **GT911 touch is at I2C address 0x5D**, not 0x14. Use `ROTATION_INVERTED` for
   correct coordinate mapping (no inversion needed).

6. **A bounce buffer is mandatory** to prevent display tearing during I2C touch
   reads. Without it, DMA-to-LCD timing is disrupted by I2C bus activity, causing
   the display image to shift and corrupt on every touch event.

7. **LVGL requires `lib_ldf_mode = deep+` and `-I include`** for PlatformIO to
   correctly locate `lv_conf.h` in the project's include directory.

The final working configuration uses: Arduino_GFX for display output, TAMC_GT911
for capacitive touch input, LVGL 8.4 for the GUI framework, and raw I2C writes to
the PCA9557 for display enable control — all running on the Arduino framework with
ESP-IDF 5.x underneath.

---

## Changelog

### [2026-04-19] Display flicker fix, UI improvements, weather enhancements

**Display stability (critical fix):**
- Restored platform to `espressif32@53.3.10` (ESP-IDF 5.x) — was accidentally
  downgraded to v6.6.0 (ESP-IDF 4.4) during a prior debug session, which removed
  bounce buffer support and caused periodic display flicker/blanking.
- Upgraded GFX Library for Arduino to 1.6.5 (`^1.4.9`) for bounce buffer support.
- Added bounce buffer (`LCD_WIDTH * 10` pixels) to `Arduino_ESP32RGBPanel`
  constructor — decouples LCD DMA refresh from PSRAM bus contention caused by
  WiFi and I2C activity.
- Added LVGL servicing (`lv_timer_handler()`) during WiFi reconnect blocking loops
  to prevent display stalls during network operations.

**UI layout changes:**
- Increased all status label fonts from `lv_font_montserrat_24` to
  `lv_font_montserrat_48` for better readability.
- Moved touch button to bottom-left corner (`LV_ALIGN_BOTTOM_LEFT, 20, -60`).
- Moved press counter to bottom-left (`LV_ALIGN_BOTTOM_LEFT, 20, -20`).
- Status labels spaced at y=20, 80, 140, 200, 260 from top.

**Weather API fix:**
- Updated Open-Meteo API from deprecated `current_weather` endpoint to
  `current=temperature_2m` with proper JSON field parsing.
- Added 10-second HTTP timeout to prevent indefinite hangs.
- Added daily min/max temperature display: API now requests
  `daily=temperature_2m_min,temperature_2m_max` and displays as
  `SE9 4JH: X.X °C (min/max)` format.
- Reduced weather retry interval from 15 minutes to 30 seconds on failure.

**NTP sync fix:**
- NTP now retries every 10 seconds on failure instead of waiting 24 hours.
- Once successfully synced, reverts to normal 24-hour re-sync interval.
- Added `ntp_synced` flag to track sync state.
