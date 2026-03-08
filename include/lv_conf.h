/*
 * LVGL Configuration for CrowPanel 7.0" (800x480 RGB565)
 * LVGL v8.4.x
 */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*====================
   COLOR SETTINGS
 *====================*/
#define LV_COLOR_DEPTH          16
#define LV_COLOR_16_SWAP        0    /* No swap needed for RGB panel (not SPI) */

/*====================
   MEMORY SETTINGS
 *====================*/
#define LV_MEM_CUSTOM           0
#define LV_MEM_SIZE             (48U * 1024U)
#define LV_MEM_ADR              0
#define LV_MEM_BUF_MAX_NUM      16

/*====================
   HAL SETTINGS
 *====================*/
#define LV_DISP_DEF_REFR_PERIOD 16   /* ~60 fps */
#define LV_INDEV_DEF_READ_PERIOD 30
#define LV_TICK_CUSTOM          1
#define LV_TICK_CUSTOM_INCLUDE  "Arduino.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())
#define LV_DPI_DEF              130  /* ~130 DPI for 7" at 800x480 */

/*====================
   FEATURE CONFIGURATION
 *====================*/
#define LV_USE_PERF_MONITOR     0
#define LV_USE_MEM_MONITOR      0

/*====================
   DRAWING
 *====================*/
#define LV_DRAW_COMPLEX         1
#define LV_SHADOW_CACHE_SIZE    0
#define LV_CIRCLE_CACHE_SIZE    4
#define LV_LAYER_SIMPLE_BUF_SIZE (24 * 1024)
#define LV_IMG_CACHE_DEF_SIZE   0

/*====================
   LOGGING
 *====================*/
#define LV_USE_LOG              0
#if LV_USE_LOG
    #define LV_LOG_LEVEL        LV_LOG_LEVEL_WARN
#endif

/*====================
   ASSERTS
 *====================*/
#define LV_USE_ASSERT_NULL          1
#define LV_USE_ASSERT_MALLOC        1
#define LV_USE_ASSERT_STYLE         0
#define LV_USE_ASSERT_MEM_INTEGRITY 0
#define LV_USE_ASSERT_OBJ           0

/*====================
   FONT USAGE
 *====================*/
#define LV_FONT_MONTSERRAT_8    0
#define LV_FONT_MONTSERRAT_10   0
#define LV_FONT_MONTSERRAT_12   1
#define LV_FONT_MONTSERRAT_14   1
#define LV_FONT_MONTSERRAT_16   1
#define LV_FONT_MONTSERRAT_18   0
#define LV_FONT_MONTSERRAT_20   1
#define LV_FONT_MONTSERRAT_22   0
#define LV_FONT_MONTSERRAT_24   1
#define LV_FONT_MONTSERRAT_26   0
#define LV_FONT_MONTSERRAT_28   0
#define LV_FONT_MONTSERRAT_30   0
#define LV_FONT_MONTSERRAT_32   0
#define LV_FONT_MONTSERRAT_34   0
#define LV_FONT_MONTSERRAT_36   0
#define LV_FONT_MONTSERRAT_38   0
#define LV_FONT_MONTSERRAT_40   0
#define LV_FONT_MONTSERRAT_42   0
#define LV_FONT_MONTSERRAT_44   0
#define LV_FONT_MONTSERRAT_46   0
#define LV_FONT_MONTSERRAT_48   0

#define LV_FONT_MONTSERRAT_12_SUBPX 0
#define LV_FONT_MONTSERRAT_28_COMPRESSED 0
#define LV_FONT_DEJAVU_16_PERSIAN_HEBREW 0
#define LV_FONT_SIMSUN_16_CJK  0
#define LV_FONT_UNSCII_8       0
#define LV_FONT_UNSCII_16      0

#define LV_FONT_DEFAULT         &lv_font_montserrat_14
#define LV_FONT_FMT_TXT_LARGE  0
#define LV_USE_FONT_COMPRESSED  0
#define LV_USE_FONT_SUBPX      0
#if LV_USE_FONT_SUBPX
    #define LV_FONT_SUBPX_BGR  0
#endif
#define LV_USE_FONT_PLACEHOLDER 1

/*====================
   TEXT SETTINGS
 *====================*/
#define LV_TXT_ENC             LV_TXT_ENC_UTF8
#define LV_TXT_BREAK_CHARS     " ,.;:-_"
#define LV_TXT_LINE_BREAK_LONG_LEN 0
#define LV_TXT_LINE_BREAK_LONG_PRE_MIN_LEN 3
#define LV_TXT_LINE_BREAK_LONG_POST_MIN_LEN 3
#define LV_TXT_COLOR_CMD       "#"
#define LV_USE_BIDI            0
#define LV_USE_ARABIC_PERSIAN_CHARS 0

/*====================
   WIDGETS
 *====================*/
#define LV_USE_ARC             1
#define LV_USE_BAR             1
#define LV_USE_BTN             1
#define LV_USE_BTNMATRIX       1
#define LV_USE_CANVAS          1
#define LV_USE_CHECKBOX        1
#define LV_USE_DROPDOWN        1
#define LV_USE_IMG             1
#define LV_USE_LABEL           1
#define LV_USE_LINE            1
#define LV_USE_ROLLER          1
#define LV_USE_SLIDER          1
#define LV_USE_SWITCH          1
#define LV_USE_TEXTAREA        1
#define LV_USE_TABLE           1

/*====================
   EXTRA WIDGETS
 *====================*/
#define LV_USE_ANIMIMG         0
#define LV_USE_CALENDAR        0
#define LV_USE_CHART           1
#define LV_USE_COLORWHEEL      0
#define LV_USE_IMGBTN          0
#define LV_USE_KEYBOARD        1
#define LV_USE_LED             1
#define LV_USE_LIST            1
#define LV_USE_MENU            1
#define LV_USE_METER           1
#define LV_USE_MSGBOX          1
#define LV_USE_SPAN            0
#define LV_USE_SPINBOX         0
#define LV_USE_SPINNER         1
#define LV_USE_TABVIEW         1
#define LV_USE_TILEVIEW        0
#define LV_USE_WIN             0

/*====================
   THEMES
 *====================*/
#define LV_USE_THEME_DEFAULT   1
#if LV_USE_THEME_DEFAULT
    #define LV_THEME_DEFAULT_DARK       0
    #define LV_THEME_DEFAULT_GROW       1
    #define LV_THEME_DEFAULT_TRANSITION_TIME 80
#endif
#define LV_USE_THEME_BASIC     1

/*====================
   LAYOUTS
 *====================*/
#define LV_USE_FLEX            1
#define LV_USE_GRID            1

/*====================
   3RD PARTY LIBRARIES
 *====================*/
#define LV_USE_FS_STDIO        0
#define LV_USE_FS_POSIX        0
#define LV_USE_FS_WIN32        0
#define LV_USE_FS_FATFS        0
#define LV_USE_PNG             0
#define LV_USE_BMP             0
#define LV_USE_SJPG            0
#define LV_USE_GIF             0
#define LV_USE_QRCODE          0
#define LV_USE_FREETYPE        0
#define LV_USE_RLOTTIE         0
#define LV_USE_FFMPEG          0

/*====================
   DEMOS
 *====================*/
#define LV_USE_DEMO_WIDGETS        0
#define LV_USE_DEMO_KEYPAD_AND_ENCODER 0
#define LV_USE_DEMO_BENCHMARK      0
#define LV_USE_DEMO_STRESS         0
#define LV_USE_DEMO_MUSIC          0

#endif /* LV_CONF_H */
