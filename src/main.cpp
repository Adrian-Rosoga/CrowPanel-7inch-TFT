/*
 * CrowPanel 7.0" HMI ESP32-S3 Display - Main Application
 *
 * Hardware: 800x480 RGB TFT LCD + GT911 Touch + PCA9557 I/O Expander
 * Libraries: Arduino_GFX (display) + TAMC_GT911 (touch) + LVGL (UI)
 */

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h>
#include <Arduino_GFX_Library.h>
#include <TAMC_GT911.h>
#include <lvgl.h>
#include "pins.h"
#include "credentials.h"

/* ===== Display Setup (Arduino_GFX) ===== */
Arduino_ESP32RGBPanel *rgbpanel = new Arduino_ESP32RGBPanel(
    LCD_DE, LCD_VSYNC, LCD_HSYNC, LCD_PCLK,
    LCD_R0, LCD_R1, LCD_R2, LCD_R3, LCD_R4,
    LCD_G0, LCD_G1, LCD_G2, LCD_G3, LCD_G4, LCD_G5,
    LCD_B0, LCD_B1, LCD_B2, LCD_B3, LCD_B4,
    LCD_HSYNC_POLARITY, LCD_HSYNC_FRONT, LCD_HSYNC_PULSE, LCD_HSYNC_BACK,
    LCD_VSYNC_POLARITY, LCD_VSYNC_FRONT, LCD_VSYNC_PULSE, LCD_VSYNC_BACK,
    LCD_PCLK_ACTIVE_NEG, LCD_PCLK_HZ,
    false /* useBigEndian */, 0 /* de_idle_high */, 0 /* pclk_idle_high */,
    LCD_WIDTH * 10 /* bounce_buffer_size_px — prevents PSRAM bus contention */
);

Arduino_RGB_Display *gfx = new Arduino_RGB_Display(
    LCD_WIDTH, LCD_HEIGHT, rgbpanel, 0 /* rotation */, true /* auto_flush */
);

/* ===== Touch Setup (GT911 at 0x5D) ===== */
TAMC_GT911 ts(TOUCH_SDA, TOUCH_SCL, TOUCH_INT, TOUCH_RST, LCD_WIDTH, LCD_HEIGHT);

/* ===== PCA9557 I/O Expander ===== */
static void pca9557_write(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(PCA9557_ADDR);
    Wire.write(reg);
    Wire.write(val);
    Wire.endTransmission();
}

static void pca9557_init() {
    pca9557_write(0x01, 0x00); /* Output register: all LOW */
    pca9557_write(0x03, 0x00); /* Config register: all OUTPUT */
    delay(20);
    pca9557_write(0x01, 0x01); /* IO0 HIGH (display enable) */
    delay(100);
    pca9557_write(0x03, 0x02); /* IO1 as input */
}

/* ===== LVGL Buffers & Drivers ===== */
static const uint32_t DISP_BUF_ROWS = 48;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t *buf1 = nullptr;
static lv_color_t *buf2 = nullptr;

static void lvgl_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

#if (LV_COLOR_16_SWAP != 0)
    gfx->draw16bitBeRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#else
    gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#endif

    lv_disp_flush_ready(disp);
}

static void lvgl_touch_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
    ts.read();
    if (ts.isTouched) {
        data->state = LV_INDEV_STATE_PR;
        data->point.x = ts.points[0].x;
        data->point.y = ts.points[0].y;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

static void lvgl_init() {
    lv_init();

    uint32_t buf_size = LCD_WIDTH * DISP_BUF_ROWS;
    buf1 = (lv_color_t *)heap_caps_malloc(buf_size * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    buf2 = (lv_color_t *)heap_caps_malloc(buf_size * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    if (!buf1 || !buf2) {
        Serial.println("WARNING: PSRAM alloc failed, using smaller internal buffer");
        buf_size = LCD_WIDTH * 10;
        buf1 = (lv_color_t *)malloc(buf_size * sizeof(lv_color_t));
        buf2 = nullptr;
    }

    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, buf_size);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = LCD_WIDTH;
    disp_drv.ver_res = LCD_HEIGHT;
    disp_drv.flush_cb = lvgl_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = lvgl_touch_read;
    lv_indev_drv_register(&indev_drv);
}

/* ===== WiFi ===== */
static unsigned long last_wifi_check = 0;
static const unsigned long WIFI_CHECK_INTERVAL = 30000; /* 30 seconds */
static bool lvgl_ready = false;

static bool wifi_connect() {
    Serial.printf("Connecting to WiFi '%s'...", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
        if (lvgl_ready) {
            lv_timer_handler();
        }
        delay(20);
        Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf(" Connected! IP: %s\n", WiFi.localIP().toString().c_str());
        return true;
    }
    Serial.println(" FAILED (timeout)");
    return false;
}

static void wifi_ensure_connected() {
    if (WiFi.status() == WL_CONNECTED) return;
    Serial.println("WiFi disconnected, reconnecting...");
    WiFi.disconnect();
    wifi_connect();
}

/* ===== NTP Time Sync ===== */
static const char *NTP_SERVER = "pool.ntp.org";
static const char *TZ_LONDON = "GMT0BST,M3.5.0/1,M10.5.0"; /* Auto BST/GMT */
static unsigned long last_ntp_sync = 0;
static const unsigned long NTP_SYNC_INTERVAL = 24UL * 60 * 60 * 1000; /* 24 hours */

static bool ntp_synced = false;

static void ntp_sync() {
    if (WiFi.status() != WL_CONNECTED) return;
    configTzTime(TZ_LONDON, NTP_SERVER);
    struct tm ti;
    if (getLocalTime(&ti, 5000)) {
        Serial.printf("NTP synced: %04d-%02d-%02d %02d:%02d:%02d\n",
                      ti.tm_year + 1900, ti.tm_mon + 1, ti.tm_mday,
                      ti.tm_hour, ti.tm_min, ti.tm_sec);
        ntp_synced = true;
        last_ntp_sync = millis();
    } else {
        Serial.println("NTP sync failed, will retry in 10s");
        /* Retry in 10 seconds instead of waiting 24 hours */
        last_ntp_sync = millis() - NTP_SYNC_INTERVAL + 10000;
    }
}

/* ===== Weather (Open-Meteo, SE9 4JH ≈ 51.45°N, 0.02°E) ===== */
static float current_temp = -999.0f;
static float temp_min = -999.0f;
static float temp_max = -999.0f;
static unsigned long last_weather_fetch = 0;
static const unsigned long WEATHER_INTERVAL = 15UL * 60 * 1000; /* 15 minutes */

static void fetch_weather() {
    if (WiFi.status() != WL_CONNECTED) return;
    HTTPClient http;
    http.setTimeout(10000);
    http.begin("http://api.open-meteo.com/v1/forecast?latitude=51.45&longitude=0.02&current=temperature_2m&daily=temperature_2m_min,temperature_2m_max&timezone=Europe%2FLondon&forecast_days=1");
    int code = http.GET();
    if (code == 200) {
        String body = http.getString();
        Serial.printf("Weather response: %s\n", body.c_str());
        /* New API: "current":{"temperature_2m":...} */
        int cw = body.indexOf("\"current\":{");
        if (cw >= 0) {
            int idx = body.indexOf("\"temperature_2m\"", cw);
            if (idx >= 0) {
                int colon = body.indexOf(':', idx);
                int comma = body.indexOf(',', colon);
                int brace = body.indexOf('}', colon);
                int end = (comma >= 0 && comma < brace) ? comma : brace;
                if (colon >= 0 && end >= 0) {
                    current_temp = body.substring(colon + 1, end).toFloat();
                    Serial.printf("Weather: %.1f°C\n", current_temp);
                }
            }
        }
        /* Parse daily min/max from "daily":{..."temperature_2m_min":[X],"temperature_2m_max":[Y]} */
        int daily = body.indexOf("\"daily\":");
        if (daily >= 0) {
            int mi = body.indexOf("\"temperature_2m_min\":", daily);
            if (mi >= 0) {
                int br = body.indexOf('[', mi);
                int br2 = body.indexOf(']', br);
                if (br >= 0 && br2 > br + 1)
                    temp_min = body.substring(br + 1, br2).toFloat();
            }
            int ma = body.indexOf("\"temperature_2m_max\":", daily);
            if (ma >= 0) {
                int br = body.indexOf('[', ma);
                int br2 = body.indexOf(']', br);
                if (br >= 0 && br2 > br + 1)
                    temp_max = body.substring(br + 1, br2).toFloat();
            }
        }
    } else {
        Serial.printf("Weather fetch failed: %d\n", code);
    }
    http.end();
    /* On failure, retry in 30s instead of waiting the full 15 min */
    last_weather_fetch = (current_temp > -999.0f) ? millis() : millis() - WEATHER_INTERVAL + 30000;
}

/* ===== UI ===== */
static lv_obj_t *lbl_wifi = nullptr;
static lv_obj_t *lbl_ip = nullptr;
static lv_obj_t *lbl_datetime = nullptr;
static lv_obj_t *lbl_temp = nullptr;
static int btn_count = 0;
static unsigned long last_ui_update = 0;
static const unsigned long UI_UPDATE_INTERVAL = 1000; /* 1 second */

static void btn_event_cb(lv_event_t *e) {
    lv_obj_t *label = (lv_obj_t *)lv_event_get_user_data(e);
    btn_count++;
    lv_label_set_text_fmt(label, "Pressed: %d", btn_count);
}

static void update_status_labels() {
    /* WiFi status */
    bool connected = (WiFi.status() == WL_CONNECTED);
    lv_label_set_text_fmt(lbl_wifi, "WiFi: %s (%s)", connected ? "Connected" : "Disconnected", WIFI_SSID);
    lv_obj_set_style_text_color(lbl_wifi, connected ? lv_color_hex(0x00FF00) : lv_color_hex(0xFF0000), 0);

    /* IP address */
    if (connected) {
        lv_label_set_text_fmt(lbl_ip, "IP: %s", WiFi.localIP().toString().c_str());
    } else {
        lv_label_set_text(lbl_ip, "IP: ---.---.---.---");
    }

    /* Date & time */
    struct tm ti;
    if (getLocalTime(&ti, 0)) {
        lv_label_set_text_fmt(lbl_datetime, "%02d/%02d/%04d  %02d:%02d:%02d",
                              ti.tm_mday, ti.tm_mon + 1, ti.tm_year + 1900,
                              ti.tm_hour, ti.tm_min, ti.tm_sec);
    } else {
        lv_label_set_text(lbl_datetime, "Time not synced");
    }

    /* Temperature */
    if (current_temp > -999.0f) {
        char temp_buf[64];
        if (temp_min > -999.0f && temp_max > -999.0f) {
            snprintf(temp_buf, sizeof(temp_buf), "SE9 4JH: %.1f °C (%.1f/%.1f)", current_temp, temp_min, temp_max);
        } else {
            snprintf(temp_buf, sizeof(temp_buf), "SE9 4JH: %.1f °C", current_temp);
        }
        lv_label_set_text(lbl_temp, temp_buf);
    } else {
        lv_label_set_text(lbl_temp, "SE9 4JH: fetching...");
    }
}

static void create_ui() {
    lv_obj_t *title = lv_label_create(lv_scr_act());
    lv_label_set_text(title, "CrowPanel 7.0\" HMI - Ready!");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_48, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

    /* Status info labels below title */
    lbl_wifi = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(lbl_wifi, &lv_font_montserrat_48, 0);
    lv_obj_align(lbl_wifi, LV_ALIGN_TOP_MID, 0, 80);

    lbl_ip = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(lbl_ip, &lv_font_montserrat_48, 0);
    lv_obj_align(lbl_ip, LV_ALIGN_TOP_MID, 0, 140);

    lbl_datetime = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(lbl_datetime, &lv_font_montserrat_48, 0);
    lv_obj_align(lbl_datetime, LV_ALIGN_TOP_MID, 0, 200);

    lbl_temp = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(lbl_temp, &lv_font_montserrat_48, 0);
    lv_obj_align(lbl_temp, LV_ALIGN_TOP_MID, 0, 260);

    /* Button — bottom left */
    lv_obj_t *btn = lv_btn_create(lv_scr_act());
    lv_obj_set_size(btn, 200, 60);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_LEFT, 20, -60);

    lv_obj_t *btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "Touch Me!!!");
    lv_obj_center(btn_label);

    lv_obj_t *counter = lv_label_create(lv_scr_act());
    lv_label_set_text(counter, "Pressed: 0");
    lv_obj_set_style_text_font(counter, &lv_font_montserrat_20, 0);
    lv_obj_align(counter, LV_ALIGN_BOTTOM_LEFT, 20, -20);

    lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_CLICKED, counter);

    update_status_labels();
}

/* ===== Arduino Setup ===== */
void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("CrowPanel 7.0\" HMI - Starting...");

    /* Pull touch RST low before I2C init */
    pinMode(TOUCH_RST, OUTPUT);
    digitalWrite(TOUCH_RST, LOW);

    /* Initialize I2C and PCA9557 (display enable) */
    Wire.begin(TOUCH_SDA, TOUCH_SCL);
    pca9557_init();
    Serial.println("PCA9557 OK");

    /* Initialize display */
    if (!gfx->begin()) {
        Serial.println("ERROR: Display init failed!");
        while (1) delay(100);
    }
    gfx->fillScreen(0x0000);

    /* Backlight on */
    pinMode(LCD_BL, OUTPUT);
    digitalWrite(LCD_BL, HIGH);
    Serial.println("Display OK");

    if (false)
    {
        // --- Diagnostic tests (uncomment to run instead of LVGL) ---
        // Direct framebuffer fill test
        uint16_t *fb = (uint16_t *)gfx->getFramebuffer();
        Serial.printf("Framebuffer pointer: %p\n", fb);
        if (fb) {
            Serial.println("Direct FB: filling RED...");
            for (int i = 0; i < LCD_WIDTH * LCD_HEIGHT; i++) fb[i] = 0xF800;
            delay(2000);
            Serial.println("Direct FB: filling GREEN...");
            for (int i = 0; i < LCD_WIDTH * LCD_HEIGHT; i++) fb[i] = 0x07E0;
            delay(2000);
            Serial.println("Direct FB: filling BLUE...");
            for (int i = 0; i < LCD_WIDTH * LCD_HEIGHT; i++) fb[i] = 0x001F;
            delay(2000);
        }
        // GFX API fill test
        gfx->fillScreen(0xF800); delay(2000); // RED
        gfx->fillScreen(0x07E0); delay(2000); // GREEN
        gfx->fillScreen(0x001F); delay(2000); // BLUE
        gfx->fillScreen(0xFFFF); delay(2000); // WHITE
        // Text test
        gfx->fillScreen(0x0000);
        gfx->setTextColor(0xFFFF);
        gfx->setTextSize(3);
        gfx->setCursor(200, 200);
        gfx->println("CrowPanel 7\" Test OK!");
        // I2C scan
        for (uint8_t addr = 1; addr < 127; addr++) {
            Wire.beginTransmission(addr);
            if (Wire.endTransmission() == 0)
                Serial.printf("  I2C device at 0x%02X\n", addr);
        }
        Serial.println("=== DIAGNOSTIC COMPLETE ===");
        //while (1) delay(1000);
        // --- End diagnostic tests ---
    }

    /* Initialize touch - GT911 at 0x5D */
    ts.begin(0x5D);
    ts.setRotation(ROTATION_INVERTED);
    Serial.println("Touch OK");

    /* Connect WiFi, sync time & fetch weather */
    wifi_connect();
    ntp_sync();
    fetch_weather();

    /* Initialize LVGL */
    lvgl_init();
    create_ui();
    lvgl_ready = true;
    Serial.println("LVGL UI ready!");
}

/* ===== Arduino Loop ===== */
void loop() {
    lv_timer_handler();

    unsigned long now = millis();

    /* WiFi reconnection check */
    if (now - last_wifi_check >= WIFI_CHECK_INTERVAL) {
        wifi_ensure_connected();
        last_wifi_check = now;
    }

    /* NTP sync: retry every 10s until synced, then every 24h */
    if (now - last_ntp_sync >= NTP_SYNC_INTERVAL) {
        ntp_sync();
    }

    /* Periodic weather update */
    if (now - last_weather_fetch >= WEATHER_INTERVAL) {
        fetch_weather();
    }

    /* Update UI labels every second */
    if (now - last_ui_update >= UI_UPDATE_INTERVAL) {
        update_status_labels();
        last_ui_update = now;
    }

    delay(5);
}
