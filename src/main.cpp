/*
 * ESP32 Touchdown LED Matrix (HUB75) Retro Clock
 * Version: See FIRMWARE_VERSION in include/config.h
 *
 * A retro-style RGB LED Matrix (HUB75) clock emulator for the ESP32 Touchdown with ILI9488 480×320 TFT display
 *
 * FEATURES:
 * - 64×32 virtual RGB LED Matrix (HUB75) emulation on 480×320 TFT display
 * - Multiple clock display modes:
 *   - Morphing (Classic): LED digits with smooth morphing animations (HariFun)
 *   - Tetris: Animated falling blocks form time digits (TetrisAnimation by toblum)
 *   - Morphing (Remix): Segment-based morphing with LED-style display (lmirel)
 *   - More modes coming: Analog, Binary, Word Clock, etc.
 * - Clock mode selection and auto-rotation via web interface
 * - WiFi configuration via WiFiManager (AP mode fallback)
 * - NTP time synchronization with timezone support (88 timezones across 13 regions)
 * - Web-based configuration interface with live display mirror (RGB565 color support)
 * - Adjustable LED appearance (diameter, gap, color, brightness)
 * - Instant auto-apply for all configuration changes (no save button required)
 * - Status bar on TFT showing WiFi, IP, date, and timezone
 * - Comprehensive system diagnostics panel in web UI:
 *   - Time & Network (time, date, WiFi status, IP)
 *   - Hardware (board, display, sensors, firmware, OTA status)
 *   - System Resources (uptime, free heap, heap usage %, CPU freq)
 *   - Debug Settings (runtime-adjustable debug level)
 * - Enhanced serial logging with before/after change tracking
 * - Date format selection (5 formats: ISO, European, US, German, Verbose)
 * - NTP server dropdown with 9 preset servers (global + regional pools)
 * - Runtime-adjustable debug level (Off, Error, Warning, Info, Verbose)
 * - OTA firmware updates for easy maintenance
 * - LittleFS-based web file serving
 *
 * HARDWARE:
 * - ESP32 Touchdown - ILI9488 480×320 TFT display with capacitive touch (FT62x6)
 * - Hardware by Dustin Watts: https://github.com/DustinWatts/esp32-touchdown
 * - Backlight control via GPIO32 (PWM capable)
 * - WiFi connectivity (2.4GHz)
 * - Battery management (optional LiPo battery)
 * - Emulates 64×32 RGB LED Matrix Panel (HUB75 style)
 *
 * DISPLAY MODES:
 * - Morphing (Classic) Mode: Large LED-style digits with smooth morphing animations
 *   - Based on Morphing Clock by Hari Wiguna (HariFun)
 *   - GitHub: https://github.com/hwiguna/HariFun_166_Morphing_Clock
 *   - Adapted for RGB LED Matrix (HUB75) emulation
 * - Tetris Mode: Falling Tetris blocks build up the time display
 *   - Uses TetrisAnimation library by Tobias Blum (toblum)
 *   - GitHub: https://github.com/toblum/TetrisAnimation
 *   - Authentic multi-color Tetris blocks (RGB565 framebuffer)
 * - Morphing (Remix) Mode: Segment-based morphing with LED-style display
 *   - Based on MorphingClockRemix by lmirel
 *   - GitHub: https://github.com/lmirel/MorphingClockRemix
 *   - LED-style digit rendering with smooth segment transitions
 * - Auto-Rotation: Cycle through modes at configurable intervals
 *
 * DISPLAY LAYOUT:
 * - Top: Large clock digits rendered as RGB LED Matrix (HUB75)
 * - Bottom: Status bar (IP address, date, timezone)
 *
 * CONFIGURATION:
 * - First boot: Creates WiFi AP "Touchdown-RetroClock-Setup"
 * - Connect and configure WiFi credentials via captive portal
 * - Access web UI at device IP address
 * - Choose clock mode (Morphing, Tetris) or enable auto-rotation
 * - Adjust timezone (88 options), NTP server (9 presets), time/date format
 * - Customize LED appearance, brightness, and debug level
 * - All changes apply instantly and persist to NVS
 *
 * WEB API ENDPOINTS:
 * - GET  /              - Main web interface
 * - GET  /api/state     - Current system state (JSON with diagnostics)
 * - POST /api/config    - Update configuration (logs changes to Serial)
 * - GET  /api/mirror    - Raw framebuffer data for display mirror (RGB565)
 * - GET  /api/timezones - List of 88 global timezones grouped by region
 *
 * CREDITS & ACKNOWLEDGMENTS:
 * - Hardware: ESP32 Touchdown by Dustin Watts
 *   https://github.com/DustinWatts/esp32-touchdown
 * - Morphing Clock (Classic): Morphing Clock by Hari Wiguna (HariFun)
 *   https://github.com/hwiguna/HariFun_166_Morphing_Clock
 * - Morphing Clock (Remix): MorphingClockRemix by lmirel
 *   https://github.com/lmirel/MorphingClockRemix
 * - Tetris Animation: TetrisAnimation library by Tobias Blum (toblum)
 *   https://github.com/toblum/TetrisAnimation
 * - TFT Display: TFT_eSPI library by Bodmer
 * - Graphics: Adafruit GFX Library by Adafruit
 * - WiFi Management: WiFiManager by tzapu
 *
 * FUTURE ENHANCEMENTS:
 * - [ ] Additional clock modes (Analog, Binary, Word Clock)
 * - [ ] Touch panel support for direct UI interaction
 * - [ ] Per-LED color control for RGB LED Matrix effects
 * - [ ] Color themes and presets
 * - [ ] MQTT integration for remote control and monitoring
 * - [ ] Home Assistant integration
 * - [ ] Battery level indicator and power management
 *
 * Author: Anthony Clarke with assistance from Claude Code
 * License: MIT
 */

#include <Arduino.h>

#include <WiFi.h>
#include <WebServer.h>
#include <WiFiManager.h>

#include <ArduinoJson.h>
#include <Preferences.h>
#include <LittleFS.h>

#include <TFT_eSPI.h>

#include <ArduinoOTA.h>
#include <time.h>
#include <Wire.h>

#include "config.h"
#include "timezones.h"
#include "TetrisClock.h"
#include "MorphingDigit.h"

// Touch controller library
#if ENABLE_TOUCH
  #include <Adafruit_FT6206.h>
#endif

// Sensor libraries (only one will be used based on config.h)
#ifdef USE_BME280
  #include <Adafruit_BME280.h>
#endif
#ifdef USE_BMP280
  #include <Adafruit_BMP280.h>
#endif
#ifdef USE_BMP180
  #include <Adafruit_BMP085.h>
#endif
#ifdef USE_SHT3X
  #include <Adafruit_SHT31.h>
#endif
#ifdef USE_HTU21D
  #include <Adafruit_HTU21DF.h>
#endif


// =========================
// Debug System
// =========================
/**
 * Leveled debug logging system with runtime control
 *
 * DEBUG LEVELS:
 *   0 = Off      - No debug output
 *   1 = Error    - Critical errors only
 *   2 = Warn     - Warnings + Errors
 *   3 = Info     - General info + Warnings + Errors (default)
 *   4 = Verbose  - All debug output including frequent events
 *
 * USAGE:
 *   DBG_ERROR(...)   - Critical errors (level 1+)
 *   DBG_WARN(...)    - Warnings (level 2+)
 *   DBG_INFO(...)    - General information (level 3+)
 *   DBG_VERBOSE(...) - Verbose/frequent output (level 4)
 *
 * RUNTIME CONTROL:
 *   Set debugLevel variable (0-4) to change verbosity at runtime
 *   Can be controlled via web API or serial commands
 *
 * EXAMPLES:
 *   DBG_ERROR("Failed to mount filesystem\n");
 *   DBG_INFO("WiFi connected: %s\n", WiFi.SSID().c_str());
 *   DBG_VERBOSE("Render frame: %d ms\n", elapsed);
 */
#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL 3  // Default: Info level
#endif

#define DBG_LEVEL_OFF     0
#define DBG_LEVEL_ERROR   1
#define DBG_LEVEL_WARN    2
#define DBG_LEVEL_INFO    3
#define DBG_LEVEL_VERBOSE 4

// Runtime debug level control (can be changed via web API)
static uint8_t debugLevel = DEBUG_LEVEL;

// Conditional debug macros based on debug level
#define DBG_ERROR(...)   do { if (debugLevel >= DBG_LEVEL_ERROR) { Serial.print("[ERR ] "); Serial.printf(__VA_ARGS__); } } while(0)
#define DBG_WARN(...)    do { if (debugLevel >= DBG_LEVEL_WARN) { Serial.print("[WARN] "); Serial.printf(__VA_ARGS__); } } while(0)
#define DBG_INFO(...)    do { if (debugLevel >= DBG_LEVEL_INFO) { Serial.print("[INFO] "); Serial.printf(__VA_ARGS__); } } while(0)
#define DBG_VERBOSE(...) do { if (debugLevel >= DBG_LEVEL_VERBOSE) { Serial.print("[VERB] "); Serial.printf(__VA_ARGS__); } } while(0)

// Legacy compatibility macros
#define DBG(...)      DBG_INFO(__VA_ARGS__)
#define DBGLN(s)      DBG_INFO("%s\n", s)
#define DBG_STEP(s)   DBG_INFO("%s\n", s)
#define DBG_OK(s)     DBG_INFO("✓ %s\n", s)
#define DBG_ERR(s)    DBG_ERROR("%s\n", s)

// =========================
// Global Objects & Application State
// =========================
TFT_eSPI tft = TFT_eSPI();

WebServer server(HTTP_PORT);
Preferences prefs;

// Touch controller object
#if ENABLE_TOUCH
  Adafruit_FT6206 touch = Adafruit_FT6206();
  unsigned long lastTouchTime = 0;      // For touch debouncing
  unsigned long touchStartTime = 0;     // When touch began
  unsigned long infoPageStartTime = 0;  // When info page was activated
  bool touchHeld = false;               // Is touch being held down
  bool infoPageActive = false;          // Is info page currently displayed
  uint8_t infoPageNum = 0;              // Current info page (0=settings, 1=diagnostics)
  TS_Point lastTouchPoint;              // Store touch point while finger is down

  #define INFO_PAGE_TIMEOUT_MS 30000    // Auto-exit info pages after 30s of inactivity
#endif

// Sensor objects (only one will be initialized based on configuration)
#ifdef USE_BME280
  Adafruit_BME280 bme280;
#endif
#ifdef USE_BMP280
  Adafruit_BMP280 bmp280(&Wire);
#endif
#ifdef USE_BMP180
  Adafruit_BMP085 bmp180;
#endif
#ifdef USE_SHT3X
  Adafruit_SHT31 sht3x = Adafruit_SHT31();
#endif
#ifdef USE_HTU21D
  Adafruit_HTU21DF htu21d = Adafruit_HTU21DF();
#endif

// Helper macro to get effective status bar height based on clock mode
// Morphing Remix mode (CLOCK_MODE_MORPH) automatically hides status bar for full display height
#define GET_STATUS_BAR_H() ( \
  (cfg.clockMode == CLOCK_MODE_MORPH) ? 0 : STATUS_BAR_H \
)

struct AppConfig {
  char tz[48]   = DEFAULT_TZ;
  char ntp[64]  = DEFAULT_NTP;
  bool use24h   = DEFAULT_24H;
  uint8_t dateFormat = 0;  // 0=YYYY-MM-DD, 1=DD/MM/YYYY, 2=MM/DD/YYYY, 3=DD.MM.YYYY, 4=Mon DD YYYY

  uint8_t ledDiameter = DEFAULT_LED_DIAMETER;
  uint8_t ledGap      = DEFAULT_LED_GAP;

  // LED color in 24-bit for web + convert to 565 for TFT
  uint32_t ledColor = 0xFF0000; // red
  uint8_t brightness = 255;     // 0..255

  bool flipDisplay = false;    // false=rotation 1 (IO ports top, USB left), true=rotation 3 (180° flip)

  // Morphing animation speed (multiplier: 1=fast, 10=very slow)
  uint8_t morphSpeed = 1;      // 1-10, controls digit morphing duration

  // Clock display mode settings
  uint8_t clockMode = DEFAULT_CLOCK_MODE;           // 0=7-seg, 1=Tetris, 2=Morph Remix
  bool autoRotate = DEFAULT_AUTO_ROTATE;            // Auto-rotate through modes
  uint8_t rotateInterval = DEFAULT_ROTATE_INTERVAL; // Minutes between rotations

  // Sensor settings
  bool useFahrenheit = false;   // false=Celsius, true=Fahrenheit

  // Morphing (Remix) mode display options
  bool morphShowSensor = true;  // Show sensor data at top in Morph Remix mode
  bool morphShowDate = true;    // Show date at bottom in Morph Remix mode
  uint32_t morphSensorColor = 0xFFFF00;  // Sensor text color (default: yellow)
  uint32_t morphDateColor = 0xFFFF00;    // Date text color (default: yellow)

  // Touch calibration offsets (for fine-tuning touch coordinate mapping)
  int16_t touchOffsetX = 0;     // X offset adjustment (-50 to +50)
  int16_t touchOffsetY = 0;     // Y offset adjustment (-50 to +50)
};

AppConfig cfg;

// Sensor state variables
bool sensorAvailable = false;
int temperature = 0;
int humidity = 0;
int pressure = 0;
const char* sensorType = "NONE";  // Will be set based on detected sensor
unsigned long lastSensorUpdate = 0;

// Logical RGB LED Matrix (HUB75) framebuffer: RGB565 color
static uint16_t fb[LED_MATRIX_H][LED_MATRIX_W];
static uint16_t fbPrev[LED_MATRIX_H][LED_MATRIX_W];  // Previous frame for delta rendering

// =========================
// Small 3x5 Bitmap Font for LED Matrix
// =========================
// Simple 3-pixel wide, 5-pixel tall font for displaying text on matrix
// Each character is represented as 5 bytes (one per row)
// Bit layout: bits 0-2 are the 3 columns (LSB = left column)

const uint8_t font3x5[][5] = {
  {0b111, 0b101, 0b101, 0b101, 0b111}, // 0
  {0b010, 0b110, 0b010, 0b010, 0b111}, // 1
  {0b111, 0b001, 0b111, 0b100, 0b111}, // 2
  {0b111, 0b001, 0b111, 0b001, 0b111}, // 3
  {0b101, 0b101, 0b111, 0b001, 0b001}, // 4
  {0b111, 0b100, 0b111, 0b001, 0b111}, // 5
  {0b111, 0b100, 0b111, 0b101, 0b111}, // 6
  {0b111, 0b001, 0b001, 0b001, 0b001}, // 7
  {0b111, 0b101, 0b111, 0b101, 0b111}, // 8
  {0b111, 0b101, 0b111, 0b001, 0b111}, // 9
  {0b000, 0b000, 0b000, 0b000, 0b000}, // : (space)
  {0b000, 0b000, 0b000, 0b000, 0b000}, // ; (space)
  {0b000, 0b000, 0b000, 0b000, 0b000}, // < (space)
  {0b000, 0b000, 0b000, 0b000, 0b000}, // = (space)
  {0b000, 0b000, 0b000, 0b000, 0b000}, // > (space)
  {0b000, 0b000, 0b000, 0b000, 0b000}, // ? (space)
  {0b000, 0b000, 0b000, 0b000, 0b000}, // @ (space)
  {0b111, 0b101, 0b111, 0b101, 0b101}, // A
  {0b110, 0b101, 0b110, 0b101, 0b110}, // B
  {0b111, 0b100, 0b100, 0b100, 0b111}, // C
  {0b110, 0b101, 0b101, 0b101, 0b110}, // D
  {0b111, 0b100, 0b111, 0b100, 0b111}, // E
  {0b111, 0b100, 0b111, 0b100, 0b100}, // F
  {0b111, 0b100, 0b101, 0b101, 0b111}, // G
  {0b101, 0b101, 0b111, 0b101, 0b101}, // H
  {0b111, 0b010, 0b010, 0b010, 0b111}, // I
  {0b111, 0b001, 0b001, 0b101, 0b111}, // J
  {0b101, 0b101, 0b110, 0b101, 0b101}, // K
  {0b100, 0b100, 0b100, 0b100, 0b111}, // L
  {0b101, 0b111, 0b111, 0b101, 0b101}, // M
  {0b101, 0b111, 0b111, 0b111, 0b101}, // N
  {0b111, 0b101, 0b101, 0b101, 0b111}, // O
  {0b111, 0b101, 0b111, 0b100, 0b100}, // P
  {0b111, 0b101, 0b101, 0b111, 0b001}, // Q
  {0b111, 0b101, 0b110, 0b101, 0b101}, // R
  {0b111, 0b100, 0b111, 0b001, 0b111}, // S (same as 5)
  {0b111, 0b010, 0b010, 0b010, 0b010}, // T
  {0b101, 0b101, 0b101, 0b101, 0b111}, // U
  {0b101, 0b101, 0b101, 0b101, 0b010}, // V
  {0b101, 0b101, 0b111, 0b111, 0b101}, // W
  {0b101, 0b101, 0b010, 0b101, 0b101}, // X
  {0b101, 0b101, 0b010, 0b010, 0b010}, // Y
  {0b111, 0b001, 0b010, 0b100, 0b111}, // Z
};

// Character offset: '0' = index 0, 'A' = index 17
static int getFont3x5Index(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'Z') return c - 'A' + 17;
  if (c >= 'a' && c <= 'z') return c - 'a' + 17; // lowercase -> uppercase
  return 10; // space for unknown chars
}

// Draw a single character using the 3x5 font
// Fixed: Draw correctly without horizontal flip
static void drawChar3x5(char c, int x, int y, uint16_t color) {
  int idx = getFont3x5Index(c);
  const uint8_t* glyph = font3x5[idx];

  for (int row = 0; row < 5; row++) {
    for (int col = 0; col < 3; col++) {
      // Font bits: bit 0 is rightmost, bit 2 is leftmost
      // We need to draw col 0 at x, col 2 at x+2 (not flipped)
      // So check bit (2-col) instead of col
      if (glyph[row] & (1 << (2 - col))) {
        int px = x + col;
        int py = y + row;
        if (px >= 0 && px < LED_MATRIX_W && py >= 0 && py < LED_MATRIX_H) {
          fb[py][px] = color;
        }
      }
    }
  }
}

// Calculate the width of a string using the 3x5 font
static int getTextWidth3x5(const char* text) {
  int width = 0;
  while (*text) {
    if (*text == ' ') {
      width += 3;
    } else if (*text == '.') {
      width += 2;
    } else if (*text == '/') {
      width += 3;
    } else if (*text == '-') {
      width += 4;
    } else if (*text == '%') {
      width += 4;
    } else {
      width += 4;  // 3 pixels + 1 pixel spacing
    }
    text++;
  }
  return width;
}

// Draw a string using the 3x5 font
static void drawText3x5(const char* text, int x, int y, uint16_t color) {
  int cursorX = x;
  while (*text) {
    if (*text == ' ') {
      cursorX += 3; // space width (reduced from 4)
    } else if (*text == '.') {
      // Draw a dot (period) - single pixel at bottom
      if (cursorX >= 0 && cursorX < LED_MATRIX_W && y + 4 >= 0 && y + 4 < LED_MATRIX_H) {
        fb[y + 4][cursorX] = color;
      }
      cursorX += 2;
    } else if (*text == '/') {
      // Draw a forward slash - diagonal line from bottom-left to top-right
      // Row 0: x+2, Row 1: x+2, Row 2: x+1, Row 3: x+1, Row 4: x+0
      for (int row = 0; row < 5; row++) {
        int px = cursorX + (2 - row / 2);  // Creates pattern: 2,2,1,1,0
        int py = y + row;
        if (px >= 0 && px < LED_MATRIX_W && py >= 0 && py < LED_MATRIX_H) {
          fb[py][px] = color;
        }
      }
      cursorX += 3;
    } else if (*text == '-') {
      // Draw a dash/minus - horizontal line in middle
      for (int px = cursorX; px < cursorX + 3; px++) {
        if (px >= 0 && px < LED_MATRIX_W && y + 2 >= 0 && y + 2 < LED_MATRIX_H) {
          fb[y + 2][px] = color;
        }
      }
      cursorX += 4;
    } else if (*text == '%') {
      // Draw percent sign - dot, slash, dot pattern
      // Top dot at (0,0)
      if (y >= 0 && y < LED_MATRIX_H && cursorX >= 0 && cursorX < LED_MATRIX_W) {
        fb[y][cursorX] = color;
      }
      // Diagonal slash
      if (y + 1 >= 0 && y + 1 < LED_MATRIX_H && cursorX + 1 >= 0 && cursorX + 1 < LED_MATRIX_W) {
        fb[y + 1][cursorX + 1] = color;
      }
      if (y + 2 >= 0 && y + 2 < LED_MATRIX_H && cursorX + 1 >= 0 && cursorX + 1 < LED_MATRIX_W) {
        fb[y + 2][cursorX + 1] = color;
      }
      if (y + 3 >= 0 && y + 3 < LED_MATRIX_H && cursorX + 2 >= 0 && cursorX + 2 < LED_MATRIX_W) {
        fb[y + 3][cursorX + 2] = color;
      }
      // Bottom dot at (2,4)
      if (y + 4 >= 0 && y + 4 < LED_MATRIX_H && cursorX + 2 >= 0 && cursorX + 2 < LED_MATRIX_W) {
        fb[y + 4][cursorX + 2] = color;
      }
      cursorX += 4;  // Reduced from 6 for tighter spacing
    } else {
      drawChar3x5(*text, cursorX, y, color);
      cursorX += 4; // 3 pixels + 1 pixel spacing
    }
    text++;
  }
}

// Cached date string for status bar (13 chars for "Mon DD, YYYY" format + null)
static char currDate[14] = "----/--/--";

// Rendering pitch (logical LED -> TFT pixels, computed from TFT size + config)
static int fbPitch = 2;

// Clock mode management
TetrisClock* tetrisClock = nullptr;  // Tetris clock instance (created in setup)
unsigned long lastModeRotation = 0;  // Last time clock mode was rotated
const uint8_t TOTAL_CLOCK_MODES = 3; // 0=7-seg, 1=Tetris, 2=Morph

// Morphing clock digits (for CLOCK_MODE_MORPH)
MorphingDigit morphHourTens, morphHourUnits;
MorphingDigit morphMinuteTens, morphMinuteUnits;
MorphingDigit morphSecondTens, morphSecondUnits;
unsigned long lastMorphUpdate = 0;  // Last morphing animation update time
bool clockColon = true;              // Colon blink state
unsigned long lastColonToggle = 0;   // Last colon toggle time
unsigned long lastTetrisUpdate = 0;  // Last Tetris animation update time
bool firstRender = true;             // Force initial render after boot

// Forward declarations
static void switchClockMode(uint8_t newMode);

// =========================
// Status LED (not available on ESP32 Touchdown)
// Consider using GPIO breakout pins if status indication is needed
// =========================
// Utility Functions
// =========================

/**
 * Convert 24-bit RGB (0xRRGGBB) to 16-bit RGB565 format for TFT display
 * @param rgb 24-bit RGB color value
 * @return 16-bit RGB565 color value
 */
static uint16_t rgb888_to_565(uint32_t rgb) {
  uint8_t r = (rgb >> 16) & 0xFF;
  uint8_t g = (rgb >> 8) & 0xFF;
  uint8_t b = (rgb >> 0) & 0xFF;
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

/**
 * Clear the entire framebuffer to a specific color
 * @param color RGB565 color value, default 0 (black/off)
 */
static void fbClear(uint16_t color = 0) {
  for (int y = 0; y < LED_MATRIX_H; y++) {
    for (int x = 0; x < LED_MATRIX_W; x++) {
      fb[y][x] = color;
    }
  }
}

/**
 * Set a single pixel in the framebuffer with bounds checking
 * @param x X coordinate (0 to LED_MATRIX_W-1)
 * @param y Y coordinate (0 to LED_MATRIX_H-1)
 * @param color RGB565 color value
 */
static inline void fbSet(int x, int y, uint16_t color) {
  if (x < 0 || y < 0 || x >= LED_MATRIX_W || y >= LED_MATRIX_H) return;
  fb[y][x] = color;
}

// =========================
// 7-Segment Digit Bitmaps & Layout Constants
// =========================
static const int DIGIT_W = 9;       // Width of each digit in pixels (9px fits HH:MM:SS with gaps in 64px)
static const int DIGIT_H = LED_MATRIX_H;  // Height matches full matrix height (32px)
static const int COLON_W = 2;       // Width of colon separator
static const int DIGIT_GAP = 1;     // 1px gap between digits for improved readability

struct Bitmap {
  uint16_t rows[DIGIT_H];  // each row is 16 bits, MSB left
};

static Bitmap DIGITS[10];  // Array of digit bitmaps (0-9)
static Bitmap COLON;       // Colon separator bitmap

/**
 * Generate a 7-segment style digit bitmap
 * Segments are labeled a-g in standard 7-segment notation:
 *     aaa
 *    f   b
 *     ggg
 *    e   c
 *     ddd
 * @param d Digit value (0-9)
 * @return Bitmap structure containing the rendered digit
 */
static Bitmap makeDigit7Seg(uint8_t d) {
  bool seg[7] = {0};
  switch (d) {
    case 0: seg[0]=seg[1]=seg[2]=seg[3]=seg[4]=seg[5]=1; break;
    case 1: seg[1]=seg[2]=1; break;
    case 2: seg[0]=seg[1]=seg[6]=seg[4]=seg[3]=1; break;
    case 3: seg[0]=seg[1]=seg[6]=seg[2]=seg[3]=1; break;
    case 4: seg[5]=seg[6]=seg[1]=seg[2]=1; break;
    case 5: seg[0]=seg[5]=seg[6]=seg[2]=seg[3]=1; break;
    case 6: seg[0]=seg[5]=seg[6]=seg[4]=seg[2]=seg[3]=1; break;
    case 7: seg[0]=seg[1]=seg[2]=1; break;
    case 8: seg[0]=seg[1]=seg[2]=seg[3]=seg[4]=seg[5]=seg[6]=1; break;
    case 9: seg[0]=seg[1]=seg[2]=seg[3]=seg[5]=seg[6]=1; break;
    default: break;
  }

  Bitmap bm{};
  for (int y=0; y<DIGIT_H; y++) bm.rows[y]=0;

  auto setPx = [&](int x, int y){
    if (x<0||y<0||x>=DIGIT_W||y>=DIGIT_H) return;
    bm.rows[y] |= (1u << (15-x));
  };

  const int padX = 0;
  const int padY = 1;
  const int th = 4;
  const int w = DIGIT_W;
  const int h = DIGIT_H;
  const int midY = h/2;

  if (seg[0]) for(int y=padY; y<padY+th; y++) for(int x=padX; x<w-padX; x++) setPx(x,y);            // a
  if (seg[3]) for(int y=h-padY-th; y<h-padY; y++) for(int x=padX; x<w-padX; x++) setPx(x,y);          // d
  if (seg[6]) for(int y=midY - th/2; y<midY - th/2 + th; y++) for(int x=padX; x<w-padX; x++) setPx(x,y); // g

  if (seg[5]) for(int x=padX; x<padX+th; x++) for(int y=padY; y<midY; y++) setPx(x,y);               // f
  if (seg[1]) for(int x=w-padX-th; x<w-padX; x++) for(int y=padY; y<midY; y++) setPx(x,y);           // b
  if (seg[4]) for(int x=padX; x<padX+th; x++) for(int y=midY; y<h-padY; y++) setPx(x,y);             // e
  if (seg[2]) for(int x=w-padX-th; x<w-padX; x++) for(int y=midY; y<h-padY; y++) setPx(x,y);         // c

  return bm;
}

/**
 * Initialize all digit and colon bitmaps
 * Called once during setup to pre-render all characters
 */
static void initBitmaps() {
  DBG_STEP("Building digit bitmaps...");
  for (int i=0;i<10;i++) DIGITS[i] = makeDigit7Seg(i);

  for (int y=0;y<DIGIT_H;y++) COLON.rows[y]=0;
  auto setPx = [&](int x, int y){
    if (x<0||y<0||x>=COLON_W||y>=DIGIT_H) return;
    COLON.rows[y] |= (1u << (15-x));
  };
  for(int yy=10; yy<13; yy++) for(int xx=0; xx<COLON_W; xx++) setPx(xx,yy);
  for(int yy=19; yy<22; yy++) for(int xx=0; xx<COLON_W; xx++) setPx(xx,yy);

  DBG_OK("Digit bitmaps ready.");
}

// =========================
// Morphing Helper Functions
// =========================

struct Pt { int8_t x, y; };

static int buildPixelsFromBitmap(const Bitmap& bm, int w, Pt* out, int maxOut) {
  int n = 0;
  for (int y = 0; y < DIGIT_H; y++) {
    uint16_t row = bm.rows[y];
    for (int x = 0; x < w; x++) {
      bool on = (row >> (15 - x)) & 0x1;
      if (!on) continue;
      if (n < maxOut) out[n] = Pt{(int8_t)x, (int8_t)y};
      n++;
    }
  }
  return n;
}

// =========================
// Backlight (PWM if TFT_BL exists)
// =========================
static void setBacklight(uint8_t b) {
#ifdef TFT_BL
  static bool init = false;
  if (!init) {
    ledcSetup(0, 5000, 8);
    ledcAttachPin(TFT_BL, 0);
    init = true;
  }
  ledcWrite(0, b);
#else
  (void)b;
#endif
}

// =========================
// Flicker-free renderer using SMALL sprite (with intensity)
// =========================
static int computeRenderPitch() {
  int matrixAreaH = tft.height() - GET_STATUS_BAR_H();
  if (matrixAreaH < 1) matrixAreaH = tft.height();

  // For Morph Remix mode (CLOCK_MODE_MORPH), use 75% of max vertical space
  // Since it has no status bar and shows date/sensor in framebuffer itself
  // Reduced by 25% from pitch=10 to pitch=7.5 for better display fit
  if (cfg.clockMode == CLOCK_MODE_MORPH) {
    // Calculate pitch: 320px / 32 LEDs = 10, reduced by 25% = 7.5
    // This gives us 64*7.5 = 480px width (perfect fit on 480px screen!)
    // and 32*7.5 = 240px height (centered in 320px with margins)
    int pitchH = (matrixAreaH * 3) / (LED_MATRIX_H * 4);  // 75% of max: (320 * 3) / (32 * 4) = 7.5 rounds to 7
    if (pitchH < 1) pitchH = 1;
    return pitchH;
  }

  // Other modes: Use min to ensure it fits in both dimensions (square pixels)
  int maxPitch = min(tft.width() / LED_MATRIX_W, matrixAreaH / LED_MATRIX_H);
  if (maxPitch < 1) maxPitch = 1;
  return maxPitch;
}

static void updateRenderPitch(bool force = false) {
  int pitch = computeRenderPitch();
  if (!force && pitch == fbPitch) return;
  fbPitch = pitch;
}

// Forward declaration
static void drawStatusBar();

static bool g_forceStatusBarRedraw = false;

/**
 * Reset status bar state to force redraw on next call
 * Used when display is cleared or flipped
 */
static void resetStatusBar() {
  g_forceStatusBarRedraw = true;
}

static void drawStatusBar() {
#if STATUS_BAR_H > 0
  // Get mode-specific status bar height (0 for Morph Remix when enabled)
  int effectiveStatusBarH = GET_STATUS_BAR_H();
  if (effectiveStatusBarH == 0) return;  // Status bar hidden for this mode

  static uint32_t lastDrawMs = 0;
  static char lastLine1[64] = "";
  static char lastLine2[64] = "";

  int barY = tft.height() - effectiveStatusBarH;
  if (barY < 0) barY = tft.height();

  char line1[64];
  // Line 1: Temperature, Humidity (if available), Pressure (if available)
  if (sensorAvailable) {
    int displayTemp = cfg.useFahrenheit ? (temperature * 9 / 5 + 32) : temperature;
    const char* tempUnit = cfg.useFahrenheit ? "oF" : "oC";  // Using 'o' as degree symbol

    // Build status string based on sensor capabilities
    char tempStr[32];
    snprintf(tempStr, sizeof(tempStr), "Temp: %d%s", displayTemp, tempUnit);

#if defined(USE_BME280)
    // BME280 has all three: temp, humidity, pressure
    snprintf(line1, sizeof(line1), "%s  Humid: %d%%  Press: %dhPa", tempStr, humidity, pressure);
#elif defined(USE_BMP280) || defined(USE_BMP180)
    // BMP280/BMP180 have temp and pressure only
    snprintf(line1, sizeof(line1), "%s  Pressure: %d hPa", tempStr, pressure);
#elif defined(USE_SHT3X) || defined(USE_HTU21D)
    // SHT3X/HTU21D have temp and humidity only
    snprintf(line1, sizeof(line1), "%s  Humidity: %d%%", tempStr, humidity);
#else
    // Unknown sensor, just show temp
    snprintf(line1, sizeof(line1), "%s", tempStr);
#endif
  } else {
    snprintf(line1, sizeof(line1), "Sensor: Not detected");
  }

  char line2[64];
  // Line 2: Date and Timezone
  snprintf(line2, sizeof(line2), "%s  %s", currDate, cfg.tz);

  uint32_t now = millis();
  bool changed = (strncmp(line1, lastLine1, sizeof(line1)) != 0) ||
                 (strncmp(line2, lastLine2, sizeof(line2)) != 0) ||
                 g_forceStatusBarRedraw;

  // Only redraw if content actually changed (prevents flashing every second)
  if (!changed) return;

  // Clear force flag and update cache
  g_forceStatusBarRedraw = false;
  strlcpy(lastLine1, line1, sizeof(lastLine1));
  strlcpy(lastLine2, line2, sizeof(lastLine2));
  lastDrawMs = now;

  tft.fillRect(0, barY, tft.width(), STATUS_BAR_H, TFT_BLACK);
  tft.drawFastHLine(0, barY, tft.width(), TFT_DARKGREY);

  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setTextFont(2);
  tft.setTextSize(1);
  tft.drawString(line1, 6, barY + 6);

  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.drawString(line2, 6, barY + 24);
#endif
}

static void renderFBToTFT() {
  // For Morph Remix mode (CLOCK_MODE_MORPH), use non-square pixels to fill full 480×320 screen
  // Other modes use square pixels with standard pitch
  int pitchX, pitchY;

  if (cfg.clockMode == CLOCK_MODE_MORPH) {
    // Non-square pixels for full screen coverage (configurable in config.h)
    pitchX = MORPH_PITCH_X;  // Default: 8 pixels per LED horizontally (64*8=512px)
    pitchY = MORPH_PITCH_Y;  // Default: 9 pixels per LED vertically (32*9=288px)
  } else {
    // Square pixels for other modes
    pitchX = fbPitch;
    pitchY = fbPitch;
  }

  const int sprW = LED_MATRIX_W * pitchX;
  const int sprH = LED_MATRIX_H * pitchY;

  // Use mode-specific status bar height
  int matrixAreaH = tft.height() - GET_STATUS_BAR_H();
  if (matrixAreaH < sprH) matrixAreaH = tft.height();

  int x0, y0;
  if (cfg.clockMode == CLOCK_MODE_MORPH) {
    // Morph Remix mode: center both horizontally and vertically
    // pitchX=8, pitchY=9: 64*8=512px width, 32*9=288px height
    // Center horizontally: (480-512)/2 = -16 (will clip left/right edges)
    // Center vertically: (320-288)/2 = 16 (margins top/bottom)
    x0 = (tft.width()  - sprW) / 2;  // Center horizontally
    y0 = (tft.height() - sprH) / 2;  // Center vertically
  } else {
    // Other modes: center in matrix area with square pixels
    x0 = (tft.width()  - sprW) / 2;
    y0 = (matrixAreaH - sprH) / 2;
  }

  // Calculate LED dot size based on pitch (use pitchX for gap/dot calculation)
  int pitch = (pitchX < pitchY) ? pitchX : pitchY;  // Use smaller pitch for dot size calc
  int gapWanted = (int)cfg.ledGap;
  if (gapWanted < 0) gapWanted = 0;
  if (gapWanted > pitch - 1) gapWanted = pitch - 1;

  int dot = pitch - gapWanted;
  int maxDot = (int)cfg.ledDiameter;
  if (maxDot < 1) maxDot = 1;
  if (dot > maxDot) dot = maxDot;
  if (dot < 1) dot = 1;

  int gap = pitch - dot;
  const int insetX = (pitchX - dot) / 2;
  const int insetY = (pitchY - dot) / 2;

  // Verbose debug output (print once per second)
  static uint32_t lastDbg = 0;
  if (millis() - lastDbg > 1000) {
    DBG_VERBOSE("Render: pitchX=%d pitchY=%d dot=%d gap=%d ledD=%d ledG=%d\n",
                pitchX, pitchY, dot, gap, cfg.ledDiameter, cfg.ledGap);
    lastDbg = millis();
  }

  // -------------------------
  // Direct TFT rendering: delta rendering (only update changed pixels)
  // Note: Sprite rendering is disabled (DISABLE_SPRITE_RENDERING=1) for smooth morphing
  // -------------------------
#if !DISABLE_SPRITE_RENDERING
  // Sprite rendering path (currently disabled)
  if (useSprite) {
    spr.fillSprite(TFT_BLACK);
    for (int y = 0; y < LED_MATRIX_H; y++) {
      for (int x = 0; x < LED_MATRIX_W; x++) {
        uint16_t color = fb[y][x];
        if (color == 0) continue;
        spr.fillRect(x * pitchX + insetX, y * pitchY + insetY, dot, dot, color);
      }
    }
    tft.startWrite();
    spr.pushSprite(x0, y0);
    tft.endWrite();
    drawStatusBar();
    return;
  }
#endif

  tft.startWrite();  // Batch all SPI writes for speed

  for (int y = 0; y < LED_MATRIX_H; y++) {
    for (int x = 0; x < LED_MATRIX_W; x++) {
      uint16_t color = fb[y][x];
      uint16_t colorPrev = fbPrev[y][x];

      // Only update pixels that changed
      if (color == colorPrev) continue;

      // Use color directly (already RGB565)
      // For Morph mode: non-square pixels (pitchX=7, pitchY=10)
      // For other modes: square pixels (pitchX=pitchY)
      tft.fillRect(x0 + x * pitchX + insetX, y0 + y * pitchY + insetY, dot, dot, color);
    }
  }
  tft.endWrite();  // Flush all batched writes
  
  // Save current frame as previous for next iteration
  memcpy(fbPrev, fb, sizeof(fb));

  drawStatusBar();
}


// =========================
// Config persistence
// =========================
static void loadConfig() {
  DBG_STEP("Loading config from NVS...");
  prefs.begin("retroclock", true);

  String s;

  s = prefs.getString("tz", DEFAULT_TZ);
  strlcpy(cfg.tz, s.c_str(), sizeof(cfg.tz));

  s = prefs.getString("ntp", DEFAULT_NTP);
  strlcpy(cfg.ntp, s.c_str(), sizeof(cfg.ntp));

  cfg.use24h = prefs.getBool("24h", DEFAULT_24H);
  cfg.dateFormat = (uint8_t)prefs.getUChar("dfmt", 0);  // Default: YYYY-MM-DD
  cfg.ledDiameter = (uint8_t)prefs.getUChar("ledd", DEFAULT_LED_DIAMETER);
  cfg.ledGap = (uint8_t)prefs.getUChar("ledg", DEFAULT_LED_GAP);
  cfg.ledColor = prefs.getUInt("col", 0xFF0000);
  cfg.brightness = (uint8_t)prefs.getUChar("bl", 255);
  cfg.flipDisplay = prefs.getBool("flip", false);
  cfg.morphSpeed = (uint8_t)prefs.getUChar("morph", 1);  // Default: 1x speed (20 frames)
  cfg.useFahrenheit = prefs.getBool("useFahr", false);
  cfg.clockMode = (uint8_t)prefs.getUChar("clockMode", DEFAULT_CLOCK_MODE);
  cfg.autoRotate = prefs.getBool("autoRotate", DEFAULT_AUTO_ROTATE);
  cfg.rotateInterval = (uint8_t)prefs.getUChar("rotateInt", DEFAULT_ROTATE_INTERVAL);
  cfg.morphShowSensor = prefs.getBool("mShowSens", true);
  cfg.morphShowDate = prefs.getBool("mShowDate", true);
  cfg.morphSensorColor = prefs.getUInt("mSensCol", 0xFFFF00);  // Default: yellow
  cfg.morphDateColor = prefs.getUInt("mDateCol", 0xFFFF00);    // Default: yellow
  debugLevel = (uint8_t)prefs.getUChar("dbglvl", DEBUG_LEVEL);

  prefs.end();

  DBG("  TZ: %s\n", cfg.tz);
  DBG("  NTP: %s\n", cfg.ntp);
  DBG("  24h: %s\n", cfg.use24h ? "true" : "false");
  DBG("  DateFmt: %u\n", cfg.dateFormat);
  DBG("  Color: #%06X\n", (unsigned)cfg.ledColor);
  DBG("  Brightness: %u\n", cfg.brightness);
  DBG("  FlipDisplay: %s\n", cfg.flipDisplay ? "true" : "false");
  DBG("  UseFahrenheit: %s\n", cfg.useFahrenheit ? "true" : "false");
  DBG("  DebugLevel: %u\n", debugLevel);

  DBG_OK("Config loaded.");
}

static void saveConfig() {
  DBG_STEP("Saving config to NVS...");
  prefs.begin("retroclock", false);
  prefs.putString("tz", cfg.tz);
  prefs.putString("ntp", cfg.ntp);
  prefs.putBool("24h", cfg.use24h);
  prefs.putUChar("dfmt", cfg.dateFormat);
  prefs.putUChar("ledd", cfg.ledDiameter);
  prefs.putUChar("ledg", cfg.ledGap);
  prefs.putUInt("col", cfg.ledColor);
  prefs.putUChar("bl", cfg.brightness);
  prefs.putBool("flip", cfg.flipDisplay);
  prefs.putUChar("morph", cfg.morphSpeed);
  prefs.putBool("useFahr", cfg.useFahrenheit);
  prefs.putUChar("clockMode", cfg.clockMode);
  prefs.putBool("autoRotate", cfg.autoRotate);
  prefs.putUChar("rotateInt", cfg.rotateInterval);
  prefs.putBool("mShowSens", cfg.morphShowSensor);
  prefs.putBool("mShowDate", cfg.morphShowDate);
  prefs.putUInt("mSensCol", cfg.morphSensorColor);
  prefs.putUInt("mDateCol", cfg.morphDateColor);
  prefs.putUChar("dbglvl", debugLevel);
  prefs.end();
  DBG_OK("Config saved.");
}

// =========================
// Display Rotation
// =========================
/**
 * Apply display rotation based on flipDisplay setting
 * - rotation 1 (normal): IO ports at top, USB on left
 * - rotation 3 (flipped): IO ports at bottom, USB on right (180° rotation)
 */
static void applyDisplayRotation() {
  uint8_t rotation = cfg.flipDisplay ? 3 : 1;
  tft.setRotation(rotation);
  DBG_VERBOSE("Display rotation set to %d (%s)\n", rotation, cfg.flipDisplay ? "flipped" : "normal");
}

// =========================
// Touch Controller Functions
// =========================
#if ENABLE_TOUCH
/**
 * Initialize capacitive touch controller (FT6236/FT6206)
 * Shares I2C bus with sensors on GPIO21/GPIO22
 * @return true if touch controller initialized successfully, false otherwise
 */
static bool initTouch() {
  DBG_STEP("Initializing touch controller...");

  // Wire.begin() is called in testSensor() - touch shares same I2C bus
  // If no sensor, we need to initialize Wire here
  #if !defined(USE_SENSOR)
    Wire.begin(TOUCH_SDA_PIN, TOUCH_SCL_PIN);
  #endif

  if (!touch.begin(TOUCH_I2C_ADDR, &Wire)) {
    DBG_WARN("Touch controller (FT6236/FT6206) not found at address 0x%02X\n", TOUCH_I2C_ADDR);
    return false;
  }

  // Read touch controller info for diagnostics
  uint8_t vendorID = 0, chipID = 0;
  Wire.beginTransmission(TOUCH_I2C_ADDR);
  Wire.write(0xA8);  // Vendor ID register
  Wire.endTransmission();
  Wire.requestFrom(TOUCH_I2C_ADDR, 1);
  if (Wire.available()) vendorID = Wire.read();

  Wire.beginTransmission(TOUCH_I2C_ADDR);
  Wire.write(0xA3);  // Chip ID register
  Wire.endTransmission();
  Wire.requestFrom(TOUCH_I2C_ADDR, 1);
  if (Wire.available()) chipID = Wire.read();

  DBG_INFO("✓ Touch controller initialized - Vendor:0x%02X Chip:0x%02X\n", vendorID, chipID);
  return true;
}

/**
 * Simple button structure for touch areas
 */
struct Button {
  int x, y, w, h;
  const char* label;
  uint16_t color;
};

// Navigation buttons (top right corner)
static Button btnPrev = {330, 5, 45, 60, "<", TFT_DARKGREY};      // Previous page
static Button btnNext = {380, 5, 45, 60, ">", TFT_DARKGREY};      // Next page
static Button btnClose = {430, 5, 45, 60, "X", TFT_RED};          // Close/Exit

// Action buttons for User Settings page (right side, below nav)
static Button btnFlipDisplay = {330, 80, 140, 45, "Flip", TFT_ORANGE};

// Action buttons for System Diagnostics page (right side, below nav)
static Button btnResetWiFi = {330, 80, 140, 45, "WiFi", TFT_RED};
static Button btnReboot = {330, 135, 140, 45, "Reboot", TFT_ORANGE};

/**
 * Draw a button on the screen
 */
static void drawButton(Button& btn, bool pressed = false) {
  uint16_t bgColor = pressed ? TFT_WHITE : btn.color;
  uint16_t textColor = pressed ? TFT_BLACK : TFT_WHITE;

  // Draw button background
  tft.fillRoundRect(btn.x, btn.y, btn.w, btn.h, 5, bgColor);

  // Draw button border
  tft.drawRoundRect(btn.x, btn.y, btn.w, btn.h, 5, TFT_WHITE);

  // Draw button text (centered)
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(textColor, bgColor);
  tft.setTextFont(2);
  tft.drawString(btn.label, btn.x + btn.w/2, btn.y + btn.h/2);
}

/**
 * Check if touch point is within button bounds
 */
static bool isButtonPressed(Button& btn, int touchX, int touchY) {
  bool pressed = (touchX >= btn.x && touchX <= (btn.x + btn.w) &&
                  touchY >= btn.y && touchY <= (btn.y + btn.h));
  if (pressed) {
    DBG_INFO("Button '%s' pressed at (%d,%d) within bounds [%d,%d,%d,%d]\n",
             btn.label, touchX, touchY, btn.x, btn.y, btn.x+btn.w, btn.y+btn.h);
  }
  return pressed;
}

/**
 * Draw text with automatic truncation if it exceeds maxWidth
 * Adds "..." to end if truncated
 */
static void drawClippedString(const char* text, int x, int y, int maxWidth) {
  char buf[100];
  strncpy(buf, text, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';

  // Check if text fits
  int textW = tft.textWidth(buf);
  if (textW <= maxWidth) {
    tft.drawString(buf, x, y);
    return;
  }

  // Truncate and add ellipsis
  int ellipsisW = tft.textWidth("...");
  int availableW = maxWidth - ellipsisW;

  // Binary search for longest fitting substring
  int len = strlen(buf);
  while (len > 0 && tft.textWidth(buf) > availableW) {
    len--;
    buf[len] = '\0';
  }

  strcat(buf, "...");
  tft.drawString(buf, x, y);
}

/**
 * Display User Settings info page
 * Shows all configurable settings from WebUI
 */
static void showUserSettingsPage() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(TL_DATUM);

  // Header
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setTextFont(2);
  tft.drawString("USER SETTINGS (1/2)", 10, 10);

  // Navigation buttons (top right)
  drawButton(btnPrev);
  drawButton(btnNext);
  drawButton(btnClose);

  // Divider (stops before buttons)
  tft.drawFastHLine(0, 40, 320, TFT_DARKGREY);

  // Vertical separator between content and buttons
  tft.drawFastVLine(320, 0, tft.height(), TFT_DARKGREY);

  int y = 48;  // Start below divider
  int lineHeight = 18;
  const int contentWidth = 305;  // Max width for text (320 - 15px margin)

  // Reset text settings after drawing buttons
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextFont(2);

  // Clock Mode
  const char* modes[] = {"Morphing (Classic)", "Tetris Animation"};
  char buf[100];
  snprintf(buf, sizeof(buf), "Display: %s", modes[cfg.clockMode]);
  drawClippedString(buf, 10, y, contentWidth); y += lineHeight;

  // Mode Switching
  snprintf(buf, sizeof(buf), "Switching: %s", cfg.autoRotate ? "Auto-Cycle" : "Manual");
  drawClippedString(buf, 10, y, contentWidth); y += lineHeight;

  if (cfg.autoRotate) {
    snprintf(buf, sizeof(buf), "Interval: %d min", cfg.rotateInterval);
    drawClippedString(buf, 10, y, contentWidth); y += lineHeight;
  }

  y += 5;  // Spacing

  // Time Settings
  drawClippedString("TIME & DATE", 10, y, contentWidth); y += lineHeight;
  snprintf(buf, sizeof(buf), "  Format: %s", cfg.use24h ? "24-hour" : "12-hour");
  drawClippedString(buf, 10, y, contentWidth); y += lineHeight;

  const char* dateFormats[] = {"YYYY-MM-DD", "DD/MM/YYYY", "MM/DD/YYYY", "DD.MM.YYYY", "Mon DD, YYYY"};
  snprintf(buf, sizeof(buf), "  Date: %s", dateFormats[cfg.dateFormat]);
  drawClippedString(buf, 10, y, contentWidth); y += lineHeight;

  snprintf(buf, sizeof(buf), "  Timezone: %s", cfg.tz);
  drawClippedString(buf, 10, y, contentWidth); y += lineHeight;

  snprintf(buf, sizeof(buf), "  Temp Unit: %s", cfg.useFahrenheit ? "Fahrenheit" : "Celsius");
  drawClippedString(buf, 10, y, contentWidth); y += lineHeight;

  y += 5;  // Spacing

  // LED Appearance
  drawClippedString("LED APPEARANCE", 10, y, contentWidth); y += lineHeight;
  snprintf(buf, sizeof(buf), "  Diameter: %d px", cfg.ledDiameter);
  drawClippedString(buf, 10, y, contentWidth); y += lineHeight;

  snprintf(buf, sizeof(buf), "  Gap: %d px", cfg.ledGap);
  drawClippedString(buf, 10, y, contentWidth); y += lineHeight;

  snprintf(buf, sizeof(buf), "  Color: RGB #%04X", cfg.ledColor);
  drawClippedString(buf, 10, y, contentWidth); y += lineHeight;

  snprintf(buf, sizeof(buf), "  Brightness: %d", cfg.brightness);
  drawClippedString(buf, 10, y, contentWidth); y += lineHeight;

  snprintf(buf, sizeof(buf), "  Morph Speed: %dx", cfg.morphSpeed);
  drawClippedString(buf, 10, y, contentWidth); y += lineHeight;

  snprintf(buf, sizeof(buf), "  Display Flip: %s", cfg.flipDisplay ? "180\xF7" : "Normal");  // 0xF7 = degree symbol
  drawClippedString(buf, 10, y, contentWidth); y += lineHeight;

  // Action Button (right side)
  drawButton(btnFlipDisplay);
}

/**
 * Display System Diagnostics info page
 * Shows system status and diagnostics
 */
static void showDiagnosticsPage() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(TL_DATUM);

  // Header
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setTextFont(2);
  tft.drawString("SYSTEM DIAGNOSTICS (2/2)", 10, 10);

  // Navigation buttons (top right)
  drawButton(btnPrev);
  drawButton(btnNext);
  drawButton(btnClose);

  // Divider (stops before buttons)
  tft.drawFastHLine(0, 40, 320, TFT_DARKGREY);

  // Vertical separator between content and buttons
  tft.drawFastVLine(320, 0, tft.height(), TFT_DARKGREY);

  int y = 48;  // Start below divider
  int lineHeight = 18;
  const int contentWidth = 305;  // Max width for text (320 - 15px margin)

  // Reset text settings after drawing buttons
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextFont(2);
  char buf[100];

  // Network
  drawClippedString("NETWORK", 10, y, contentWidth); y += lineHeight;
  snprintf(buf, sizeof(buf), "  WiFi: %s", WiFi.SSID().c_str());
  drawClippedString(buf, 10, y, contentWidth); y += lineHeight;

  snprintf(buf, sizeof(buf), "  IP: %s", WiFi.localIP().toString().c_str());
  drawClippedString(buf, 10, y, contentWidth); y += lineHeight;

  snprintf(buf, sizeof(buf), "  Signal: %d dBm", WiFi.RSSI());
  drawClippedString(buf, 10, y, contentWidth); y += lineHeight;

  y += 5;  // Spacing

  // Hardware
  drawClippedString("HARDWARE", 10, y, contentWidth); y += lineHeight;
  snprintf(buf, sizeof(buf), "  Board: ESP32 Touchdown");
  drawClippedString(buf, 10, y, contentWidth); y += lineHeight;

  snprintf(buf, sizeof(buf), "  Display: 480x320 ILI9488");
  drawClippedString(buf, 10, y, contentWidth); y += lineHeight;

  if (sensorAvailable) {
    snprintf(buf, sizeof(buf), "  Sensor: %s", sensorType);
    drawClippedString(buf, 10, y, contentWidth); y += lineHeight;
  }

  y += 5;  // Spacing

  // System Resources
  drawClippedString("SYSTEM RESOURCES", 10, y, contentWidth); y += lineHeight;

  unsigned long uptimeSec = millis() / 1000;
  int days = uptimeSec / 86400;
  int hours = (uptimeSec % 86400) / 3600;
  int mins = (uptimeSec % 3600) / 60;
  snprintf(buf, sizeof(buf), "  Uptime: %dd %dh %dm", days, hours, mins);
  drawClippedString(buf, 10, y, contentWidth); y += lineHeight;

  uint32_t freeHeap = ESP.getFreeHeap();
  uint32_t heapSize = ESP.getHeapSize();
  snprintf(buf, sizeof(buf), "  Free Heap: %d KB", freeHeap / 1024);
  drawClippedString(buf, 10, y, contentWidth); y += lineHeight;

  snprintf(buf, sizeof(buf), "  CPU: %d MHz", ESP.getCpuFreqMHz());
  drawClippedString(buf, 10, y, contentWidth); y += lineHeight;

  snprintf(buf, sizeof(buf), "  Firmware: v%s", FIRMWARE_VERSION);
  drawClippedString(buf, 10, y, contentWidth); y += lineHeight;

  // Action Buttons (right side)
  drawButton(btnResetWiFi);
  drawButton(btnReboot);
}

/**
 * Check for touch events:
 * - When info pages active: Navigation buttons (< > X) or action buttons
 * - When clock active: Long press (3s) shows info pages, short tap switches mode
 * Implements debouncing to prevent multiple triggers
 */
static void handleTouch() {
  bool isTouched = touch.touched();
  unsigned long now = millis();

  // Check for info page timeout (30s of inactivity)
      if (infoPageActive && (now - infoPageStartTime >= INFO_PAGE_TIMEOUT_MS)) {
        DBG_INFO("Info page timeout - returning to clock display\n");
        infoPageActive = false;
        tft.fillScreen(TFT_BLACK);  // Clear entire TFT to remove any clock artifacts
        memset(fbPrev, 0, sizeof(fbPrev));  // Force full redraw
        resetStatusBar();
        return;
      }

  if (isTouched) {
    if (!touchHeld) {
      // Touch just started - capture coordinates NOW while finger is down
      lastTouchPoint = touch.getPoint();
      touchStartTime = now;
      touchHeld = true;
      DBG_INFO("Touch started at raw(x=%d,y=%d)\n", lastTouchPoint.x, lastTouchPoint.y);
    } else {
      // Touch is being held - check if it's a long press (only when info page not active)
      if (!infoPageActive && (now - touchStartTime >= TOUCH_LONG_PRESS_MS)) {
        // Long press detected - show info page
        DBG_INFO("Long press detected - showing info pages\n");
        infoPageActive = true;
        infoPageNum = 0;  // Start with User Settings page
        infoPageStartTime = now;  // Start timeout timer
        showUserSettingsPage();
        touchStartTime = now;  // Reset for debouncing
      }
    }
  } else {
    // Touch released
    if (touchHeld) {
      unsigned long pressDuration = now - touchStartTime;

      // Debounce check
      if (now - lastTouchTime < TOUCH_DEBOUNCE_MS) {
        touchHeld = false;
        return;
      }

      lastTouchTime = now;
      touchHeld = false;

      if (infoPageActive) {
        // Info page is active - check for button presses using stored touch point
        // Reset timeout timer on any touch interaction
        infoPageStartTime = now;

        TS_Point point = lastTouchPoint;

        // Map touch coordinates to screen coordinates
        // FT6206 reports in portrait mode (0-320 x 0-480)
        // Display is 480x320 in landscape mode
        //
        // Calibration data from testing:
        // X button at screen(430-475, 5-35) -> raw touch(303, 476)
        // This confirms: touchY -> screenX, touchX -> screenY (inverted)

        int touchX, touchY;

        if (cfg.flipDisplay) {
          // Rotation 3 (display flipped 180°)
          // Touch coordinates are inverted in both axes
          touchX = map(point.y, 0, 480, 479, 0);  // Touch Y -> Screen X (inverted)
          touchY = map(point.x, 0, 320, 0, 319);  // Touch X -> Screen Y (normal)
        } else {
          // Rotation 1 (normal landscape, USB on right)
          touchX = map(point.y, 0, 480, 0, 479);  // Touch Y -> Screen X (1:1 mapping)
          touchY = map(point.x, 0, 320, 319, 0);  // Touch X -> Screen Y (inverted)
        }

        // Apply calibration offsets
        touchX += cfg.touchOffsetX;
        touchY += cfg.touchOffsetY;

        // Constrain to screen bounds
        touchX = constrain(touchX, 0, 479);
        touchY = constrain(touchY, 0, 319);

        DBG_INFO("Touch raw(x=%d,y=%d) -> screen(x=%d,y=%d) [flip=%d, offset=%d,%d]\n",
                 point.x, point.y, touchX, touchY, cfg.flipDisplay, cfg.touchOffsetX, cfg.touchOffsetY);

        // Check navigation buttons first (always available)
        if (isButtonPressed(btnClose, touchX, touchY)) {
          // X button - exit info pages and return to clock
          DBG_INFO("Close button pressed - exiting info pages\n");
          drawButton(btnClose, true);
          delay(150);
          infoPageActive = false;
          tft.fillScreen(TFT_BLACK);
          memset(fbPrev, 0, sizeof(fbPrev));  // Force full redraw
          resetStatusBar();
          return;
        }

        if (isButtonPressed(btnPrev, touchX, touchY)) {
          // < button - previous page
          DBG_INFO("Previous button pressed\n");
          drawButton(btnPrev, true);
          delay(150);
          infoPageNum = (infoPageNum == 0) ? (TOUCH_INFO_PAGES - 1) : (infoPageNum - 1);
          DBG_INFO("Switching to info page %d\n", infoPageNum);

          if (infoPageNum == 0) {
            showUserSettingsPage();
          } else if (infoPageNum == 1) {
            showDiagnosticsPage();
          }
          return;
        }

        if (isButtonPressed(btnNext, touchX, touchY)) {
          // > button - next page
          DBG_INFO("Next button pressed\n");
          drawButton(btnNext, true);
          delay(150);
          infoPageNum = (infoPageNum + 1) % TOUCH_INFO_PAGES;
          DBG_INFO("Switching to info page %d\n", infoPageNum);

          if (infoPageNum == 0) {
            showUserSettingsPage();
          } else if (infoPageNum == 1) {
            showDiagnosticsPage();
          }
          return;
        }

        // Check action buttons based on current page
        if (infoPageNum == 0) {
          // User Settings page - check Flip Display button
          if (isButtonPressed(btnFlipDisplay, touchX, touchY)) {
            DBG_INFO("Flip Display button pressed\n");
            drawButton(btnFlipDisplay, true);
            delay(200);

            // Toggle flip display
            cfg.flipDisplay = !cfg.flipDisplay;
            saveConfig();
            applyDisplayRotation();
            tft.fillScreen(TFT_BLACK);
            memset(fbPrev, 0, sizeof(fbPrev));
            resetStatusBar();
            showUserSettingsPage();  // Refresh page with new value
            return;
          }
        } else if (infoPageNum == 1) {
          // System Diagnostics page - check Reset WiFi and Reboot buttons
          if (isButtonPressed(btnResetWiFi, touchX, touchY)) {
            DBG_INFO("Reset WiFi button pressed\n");
            drawButton(btnResetWiFi, true);
            delay(200);

            // Reset WiFi credentials and restart
            DBG_OK("Resetting WiFi credentials via info page...");
            prefs.begin("nvs", false);
            prefs.clear();
            prefs.end();

            tft.fillScreen(TFT_BLACK);
            tft.setTextColor(TFT_RED, TFT_BLACK);
            tft.setTextDatum(MC_DATUM);
            tft.setTextFont(4);
            tft.drawString("WiFi Reset", tft.width()/2, tft.height()/2 - 20);
            tft.setTextFont(2);
            tft.drawString("Restarting...", tft.width()/2, tft.height()/2 + 20);
            delay(2000);
            ESP.restart();
            return;
          }

          if (isButtonPressed(btnReboot, touchX, touchY)) {
            DBG_INFO("Reboot button pressed\n");
            drawButton(btnReboot, true);
            delay(200);

            // Reboot device
            DBG_OK("Rebooting device via info page...");
            tft.fillScreen(TFT_BLACK);
            tft.setTextColor(TFT_ORANGE, TFT_BLACK);
            tft.setTextDatum(MC_DATUM);
            tft.setTextFont(4);
            tft.drawString("Rebooting", tft.width()/2, tft.height()/2 - 20);
            tft.setTextFont(2);
            tft.drawString("Please wait...", tft.width()/2, tft.height()/2 + 20);
            delay(1000);
            ESP.restart();
            return;
          }
        }
      } else {
        // Clock is active - long press shows info, short tap switches mode
        if (pressDuration < TOUCH_LONG_PRESS_MS) {
          // Short tap - switch to next clock mode
          uint8_t nextMode = (cfg.clockMode + 1) % TOTAL_CLOCK_MODES;
          DBG_INFO("Touch - switching to clock mode %d\n", nextMode);

          switchClockMode(nextMode);

          // If auto-rotate is enabled, reset the timer
          if (cfg.autoRotate) {
            lastModeRotation = now;
          }
        }
      }
    }
  }
}
#endif

// =========================
// Sensor Functions
// =========================
/**
 * Test and initialize I2C sensor
 * @return true if sensor detected and working, false otherwise
 */
static bool testSensor() {
  Wire.begin(SENSOR_SDA_PIN, SENSOR_SCL_PIN);
  DBG_STEP("Testing I2C sensor...");

#ifdef USE_BME280
  // Test BME280 sensor
  if (!bme280.begin(0x76, &Wire)) {
    DBG_WARN("BME280 sensor not found at 0x76\n");
    if (!bme280.begin(0x77, &Wire)) {
      DBG_WARN("BME280 sensor not found at 0x77 either\n");
      return false;
    }
  }

  bme280.setSampling(Adafruit_BME280::MODE_FORCED,
                     Adafruit_BME280::SAMPLING_X1,
                     Adafruit_BME280::SAMPLING_X1,
                     Adafruit_BME280::SAMPLING_X1,
                     Adafruit_BME280::FILTER_OFF);

  float temp = bme280.readTemperature();
  float hum = bme280.readHumidity();

  if (isnan(temp) || isnan(hum) || temp < -50 || temp > 100 || hum < 0 || hum > 100) {
    DBG_WARN("BME280 readings invalid\n");
    return false;
  }

  DBG_INFO("BME280 OK: %.1f°C, %.1f%%\n", temp, hum);
  sensorType = "BME280";
  return true;

#elif defined(USE_BMP280)
  // Test BMP280 sensor (temperature and pressure only, no humidity)
  if (!bmp280.begin(0x76)) {
    DBG_WARN("BMP280 sensor not found at 0x76\n");
    if (!bmp280.begin(0x77)) {
      DBG_WARN("BMP280 sensor not found at 0x77 either\n");
      return false;
    }
  }

  // Configure BMP280 for weather monitoring
  bmp280.setSampling(Adafruit_BMP280::MODE_NORMAL,     // Operating mode
                     Adafruit_BMP280::SAMPLING_X2,     // Temperature oversampling
                     Adafruit_BMP280::SAMPLING_X16,    // Pressure oversampling
                     Adafruit_BMP280::FILTER_X16,      // Filtering
                     Adafruit_BMP280::STANDBY_MS_500); // Standby time

  float temp = bmp280.readTemperature();
  float pressure = bmp280.readPressure() / 100.0F;  // Convert Pa to hPa

  if (isnan(temp) || isnan(pressure) || temp < -50 || temp > 100 || pressure < 300 || pressure > 1100) {
    DBG_WARN("BMP280 readings invalid\n");
    return false;
  }

  DBG_INFO("BMP280 OK: %.1f°C, %.1f hPa\n", temp, pressure);
  sensorType = "BMP280";
  return true;

#elif defined(USE_BMP180)
  // Test BMP180 sensor (temperature and pressure only, no humidity)
  // BMP180 has fixed I2C address 0x77
  if (!bmp180.begin(BMP085_MODE_ULTRAHIGHRES)) {
    DBG_WARN("BMP180 sensor not found at 0x77\n");
    return false;
  }

  // Read initial values to verify sensor is working
  float temp = bmp180.readTemperature();
  float pressure = bmp180.readPressure() / 100.0F;  // Convert Pa to hPa

  if (isnan(temp) || isnan(pressure) || temp < -50 || temp > 100 || pressure < 300 || pressure > 1100) {
    DBG_WARN("BMP180 readings invalid\n");
    return false;
  }

  DBG_INFO("BMP180 OK: %.1f°C, %.1f hPa\n", temp, pressure);
  sensorType = "BMP180";
  return true;

#elif defined(USE_SHT3X)
  // Test SHT3X sensor
  if (!sht3x.begin(0x44)) {  // Default I2C address for SHT3X
    DBG_WARN("SHT3X sensor not found at 0x44\n");
    if (!sht3x.begin(0x45)) {  // Alternative I2C address
      DBG_WARN("SHT3X sensor not found at 0x45 either\n");
      return false;
    }
  }

  // Read initial values to verify sensor is working
  float temp = sht3x.readTemperature();
  float hum = sht3x.readHumidity();

  if (isnan(temp) || isnan(hum) || temp < -50 || temp > 100 || hum < 0 || hum > 100) {
    DBG_WARN("SHT3X readings invalid\n");
    return false;
  }

  DBG_INFO("SHT3X OK: %.1f°C, %.1f%%\n", temp, hum);
  sensorType = "SHT3X";
  return true;

#elif defined(USE_HTU21D)
  // Test HTU21D sensor
  if (!htu21d.begin()) {  // HTU21D uses fixed I2C address 0x40
    DBG_WARN("HTU21D sensor not found at 0x40\n");
    return false;
  }

  // Read initial values to verify sensor is working
  float temp = htu21d.readTemperature();
  float hum = htu21d.readHumidity();

  if (isnan(temp) || isnan(hum) || temp < -50 || temp > 100 || hum < 0 || hum > 100) {
    DBG_WARN("HTU21D readings invalid\n");
    return false;
  }

  DBG_INFO("HTU21D OK: %.1f°C, %.1f%%\n", temp, hum);
  sensorType = "HTU21D";
  return true;

#else
  DBG_WARN("No sensor type defined in configuration\n");
  return false;
#endif
}

/**
 * Update sensor readings from I2C sensor
 */
static void updateSensorData() {
  if (!sensorAvailable) return;

  float temp = NAN;
  float hum = NAN;
  float pres = NAN;

#ifdef USE_BME280
  // Read BME280 sensor
  bme280.takeForcedMeasurement();
  temp = bme280.readTemperature();
  hum = bme280.readHumidity();
  pres = bme280.readPressure() / 100.0F;

#elif defined(USE_BMP280)
  // Read BMP280 sensor (temperature and pressure only, no humidity)
  temp = bmp280.readTemperature();
  pres = bmp280.readPressure() / 100.0F;  // Convert Pa to hPa
  // BMP280 doesn't have humidity sensor, so humidity remains NAN

#elif defined(USE_BMP180)
  // Read BMP180 sensor (temperature and pressure only, no humidity)
  temp = bmp180.readTemperature();
  pres = bmp180.readPressure() / 100.0F;  // Convert Pa to hPa
  // BMP180 doesn't have humidity sensor, so humidity remains NAN

#elif defined(USE_SHT3X)
  // Read SHT3X sensor
  temp = sht3x.readTemperature();
  hum = sht3x.readHumidity();
  // SHT3X doesn't have a pressure sensor, so pressure remains NAN

#elif defined(USE_HTU21D)
  // Read HTU21D sensor
  temp = htu21d.readTemperature();
  hum = htu21d.readHumidity();
  // HTU21D doesn't have a pressure sensor, so pressure remains NAN
#endif

  // Update temperature if valid
  if (!isnan(temp) && temp >= -50 && temp <= 100) {
    temperature = (int)round(temp);
  }

  // Update humidity if valid
  if (!isnan(hum) && hum >= 0 && hum <= 100) {
    humidity = (int)round(hum);
  }

  // Update pressure if valid (BME280 and BMP280 sensors)
  if (!isnan(pres) && pres >= 800 && pres <= 1200) {
    pressure = (int)round(pres);
  }

  // Output sensor readings to serial (always at INFO level for visibility)
  if (debugLevel >= DBG_LEVEL_INFO) {
    if (cfg.useFahrenheit) {
      int tempF = temperature * 9 / 5 + 32;
      Serial.printf("[INFO] Sensor Update - %s: %d°F (%d°C)", sensorType, tempF, temperature);
    } else {
      Serial.printf("[INFO] Sensor Update - %s: %d°C", sensorType, temperature);
    }

    // Add humidity if sensor supports it (BME280, SHT3X, HTU21D)
#if defined(USE_BME280) || defined(USE_SHT3X) || defined(USE_HTU21D)
    if (humidity >= 0) {
      Serial.printf(", Humidity: %d%%", humidity);
    }
#endif

    // Add pressure if sensor supports it (BME280, BMP280, BMP180)
#if defined(USE_BME280) || defined(USE_BMP280) || defined(USE_BMP180)
    if (pressure > 0) {
      Serial.printf(", Pressure: %d hPa", pressure);
    }
#endif

    Serial.printf("\n");
  }
}

// =========================
// Time / NTP
// =========================
/**
 * Lookup timezone POSIX string from timezone name
 * @param tzName Timezone name (e.g., "Sydney, Australia")
 * @return POSIX timezone string, or "UTC0" if not found
 */
static const char* lookupTimezone(const char* tzName) {
  if (!tzName || !tzName[0]) return timezones[0].tzString;  // Default to Sydney

  // Search for timezone by name
  for (int i = 0; i < numTimezones; i++) {
    if (strcmp(tzName, timezones[i].name) == 0) {
      return timezones[i].tzString;
    }
  }

  // If not found, return first (default)
  DBG_WARN("Timezone '%s' not found, using default\n", tzName);
  return timezones[0].tzString;
}

static void startNtp() {
  DBG_STEP("Starting NTP...");
  const char* tzEnv = lookupTimezone(cfg.tz);
  DBG_INFO("Timezone: %s -> %s\n", cfg.tz, tzEnv);
  configTzTime(tzEnv, cfg.ntp);
  DBG_OK("NTP configured.");
}

static bool getLocalTimeSafe(struct tm& timeinfo, uint32_t timeoutMs = 2000) {
  uint32_t start = millis();
  while ((millis() - start) < timeoutMs) {
    if (getLocalTime(&timeinfo, 50)) return true;
    delay(10);
  }
  return false;
}

// =========================
// Web handlers
// =========================
// Forward declaration (defined later in Clock logic section)
static void formatDate(struct tm& ti, char* out, size_t n);

static void handleGetTimezones() {
  DBG_VERBOSE("Web: GET /api/timezones from %s\n", server.client().remoteIP().toString().c_str());

  JsonDocument doc;
  JsonArray regions = doc["regions"].to<JsonArray>();

  // Define region boundaries (indices from timezones.h)
  struct Region {
    const char* name;
    int start;
    int end;
  };

  const Region regionDefs[] = {
    {"Australia & Oceania", 0, 11},
    {"North America", 12, 22},
    {"South America", 23, 28},
    {"Western Europe", 29, 39},
    {"Northern Europe", 40, 43},
    {"Central & Eastern Europe", 44, 51},
    {"Middle East", 52, 56},
    {"South Asia", 57, 63},
    {"Southeast Asia", 64, 70},
    {"East Asia", 71, 76},
    {"Central Asia", 77, 79},
    {"Caucasus", 80, 82},
    {"Africa", 83, 86}
  };

  for (const auto& region : regionDefs) {
    JsonObject regionObj = regions.add<JsonObject>();
    regionObj["name"] = region.name;
    JsonArray tzArray = regionObj["timezones"].to<JsonArray>();

    for (int i = region.start; i <= region.end && i < numTimezones; i++) {
      JsonObject tz = tzArray.add<JsonObject>();
      tz["name"] = timezones[i].name;
      tz["tz"] = timezones[i].tzString;
    }
  }

  doc["count"] = numTimezones;

  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

/**
 * POST /api/reset-wifi
 *
 * Resets WiFi credentials and restarts the device into config portal mode.
 * This allows users to reconfigure WiFi settings via the web interface.
 */
static void handleResetWiFi() {
  String clientIP = server.client().remoteIP().toString();
  DBG_INFO("Web: POST /api/reset-wifi from %s\n", clientIP.c_str());

  server.send(200, "application/json", "{\"status\":\"WiFi reset initiated. Device will restart...\"}");

  delay(1000);

  DBG_OK("Resetting WiFi credentials via web interface...");
  WiFiManager wm;
  wm.resetSettings();

  delay(1000);
  ESP.restart();
}

/**
 * POST /api/reboot
 *
 * Reboots the device cleanly. Useful for applying settings or recovering from issues.
 */
static void handleReboot() {
  String clientIP = server.client().remoteIP().toString();
  DBG_INFO("Web: POST /api/reboot from %s\n", clientIP.c_str());

  server.send(200, "application/json", "{\"status\":\"Device rebooting...\"}");

  delay(1000);

  DBG_OK("Rebooting device via web interface...");
  ESP.restart();
}

/**
 * GET /api/state
 *
 * Returns comprehensive system state as JSON for web interface.
 *
 * Response includes:
 * - Time & Network: current time, date, WiFi SSID, IP address
 * - Configuration: timezone, NTP server, time format, date format, LED settings, brightness, debug level
 * - System Diagnostics: uptime (seconds), free heap, total heap size, CPU frequency
 * - Hardware Info: board type, display model, sensor status, firmware version, OTA status
 *
 * This endpoint is polled by the web interface every second to update:
 * - Live clock display
 * - System diagnostics panel
 * - Configuration field values
 * - Display mirror state
 */
static void handleGetState() {
  DBG_VERBOSE("Web: GET /api/state from %s\n", server.client().remoteIP().toString().c_str());

  struct tm ti{};
  bool ok = getLocalTimeSafe(ti, 300);
  char tbuf[16] = "--:--:--";
  char dbuf[16] = "----/--/--";
  if (ok) {
    strftime(tbuf, sizeof(tbuf), "%H:%M:%S", &ti);
    formatDate(ti, dbuf, sizeof(dbuf));  // Use configured date format
  }

  JsonDocument doc;

  // Time & Network
  doc["time"] = tbuf;
  doc["date"] = dbuf;
  doc["wifi"] = (WiFi.isConnected() ? WiFi.SSID() : String("DISCONNECTED"));
  doc["ip"] = WiFi.isConnected() ? WiFi.localIP().toString() : String("0.0.0.0");

  // Config
  doc["tz"] = cfg.tz;
  doc["ntp"] = cfg.ntp;
  doc["use24h"] = cfg.use24h;
  doc["dateFormat"] = cfg.dateFormat;
  doc["ledDiameter"] = cfg.ledDiameter;
  doc["ledGap"] = cfg.ledGap;
  doc["ledColor"] = cfg.ledColor;
  doc["brightness"] = cfg.brightness;
  doc["morphSpeed"] = cfg.morphSpeed;
  doc["flipDisplay"] = cfg.flipDisplay;
  doc["clockMode"] = cfg.clockMode;
  doc["autoRotate"] = cfg.autoRotate;
  doc["rotateInterval"] = cfg.rotateInterval;

  // Morphing (Remix) mode options
  doc["morphShowSensor"] = cfg.morphShowSensor;
  doc["morphShowDate"] = cfg.morphShowDate;
  doc["morphSensorColor"] = cfg.morphSensorColor;
  doc["morphDateColor"] = cfg.morphDateColor;

  // System diagnostics
  uint32_t uptime = millis() / 1000;  // seconds
  doc["uptime"] = uptime;
  doc["freeHeap"] = ESP.getFreeHeap();
  doc["heapSize"] = ESP.getHeapSize();
  doc["cpuFreq"] = ESP.getCpuFreqMHz();
  doc["debugLevel"] = debugLevel;

  // Sensor data
  doc["sensorAvailable"] = sensorAvailable;
  doc["sensorType"] = sensorType;
  doc["temperature"] = temperature;
  doc["humidity"] = humidity;
  doc["pressure"] = pressure;
  doc["useFahrenheit"] = cfg.useFahrenheit;

  // Status bar text (matches TFT display exactly)
  char statusLine1[64];
  if (sensorAvailable) {
    int displayTemp = cfg.useFahrenheit ? (temperature * 9 / 5 + 32) : temperature;
    const char* tempUnit = cfg.useFahrenheit ? "°F" : "°C";
    char tempStr[32];
    snprintf(tempStr, sizeof(tempStr), "Temp: %d%s", displayTemp, tempUnit);

#if defined(USE_BME280)
    snprintf(statusLine1, sizeof(statusLine1), "%s  Humid: %d%%  Press: %dhPa", tempStr, humidity, pressure);
#elif defined(USE_BMP280) || defined(USE_BMP180)
    snprintf(statusLine1, sizeof(statusLine1), "%s  Pressure: %d hPa", tempStr, pressure);
#elif defined(USE_SHT3X) || defined(USE_HTU21D)
    snprintf(statusLine1, sizeof(statusLine1), "%s  Humidity: %d%%", tempStr, humidity);
#else
    snprintf(statusLine1, sizeof(statusLine1), "%s", tempStr);
#endif
  } else {
    snprintf(statusLine1, sizeof(statusLine1), "Sensor: Not detected");
  }
  doc["statusLine1"] = statusLine1;
  doc["statusLine2"] = String(dbuf) + "  " + String(cfg.tz);

  // Hardware info (static)
  doc["board"] = "ESP32 Touchdown";
  doc["display"] = "480×320 ILI9488";

  // Build sensor info string
  String sensorInfo;
  if (sensorAvailable) {
    sensorInfo = String(sensorType);
#if defined(USE_BME280)
    sensorInfo += " (Temp/Humid/Press)";
#elif defined(USE_BMP280)
    sensorInfo += " (Temp/Press)";
#elif defined(USE_BMP180)
    sensorInfo += " (Temp/Press)";
#else
    sensorInfo += " (Temp/Humid)";
#endif
  } else {
    sensorInfo = "None detected";
  }
  doc["sensors"] = sensorInfo;

  doc["firmware"] = FIRMWARE_VERSION;
  doc["otaEnabled"] = true;

  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

/**
 * POST /api/config
 *
 * Updates device configuration from web interface.
 *
 * Features:
 * - Captures old values before changes for comparison logging
 * - Logs before/after values for each changed field to Serial monitor
 * - Includes client IP address in all log messages
 * - Validates and constrains all input values
 * - Persists changes to NVS (Non-Volatile Storage)
 * - Applies time configuration (timezone, NTP, format) immediately
 *
 * Accepts JSON body with optional fields:
 * - tz: Timezone name (IANA format)
 * - ntp: NTP server hostname
 * - use24h: Boolean for 24-hour time format
 * - dateFormat: Integer 0-4 for date format selection
 * - ledDiameter: Integer 1-10 for LED dot size
 * - ledGap: Integer 0-8 for spacing between LEDs
 * - ledColor: RGB888 color value (0-16777215)
 * - brightness: Integer 0-255 for backlight brightness
 * - flipDisplay: Boolean for display rotation (false=normal, true=180° flip)
 * - debugLevel: Integer 0-4 for logging verbosity
 */
static void handlePostConfig() {
  String clientIP = server.client().remoteIP().toString();
  DBG_INFO("Web: POST /api/config from %s\n", clientIP.c_str());

  if (!server.hasArg("plain")) {
    DBG_WARN("Config update failed: missing body\n");
    server.send(400, "text/plain", "missing body");
    return;
  }

  JsonDocument doc;
  auto err = deserializeJson(doc, server.arg("plain"));
  if (err) {
    DBG_WARN("Config update failed: bad json\n");
    server.send(400, "text/plain", "bad json");
    return;
  }

  // Capture old values for logging
  char oldTz[64];
  char oldNtp[64];
  bool oldUse24h = cfg.use24h;
  uint8_t oldDateFormat = cfg.dateFormat;
  uint8_t oldLedDiameter = cfg.ledDiameter;
  uint8_t oldLedGap = cfg.ledGap;
  uint32_t oldLedColor = cfg.ledColor;
  uint8_t oldBrightness = cfg.brightness;
  bool oldFlipDisplay = cfg.flipDisplay;
  strlcpy(oldTz, cfg.tz, sizeof(oldTz));
  strlcpy(oldNtp, cfg.ntp, sizeof(oldNtp));

  // Update config and log each change
  if (!doc["tz"].isNull()) {
    strlcpy(cfg.tz, doc["tz"].as<const char*>(), sizeof(cfg.tz));
    if (strcmp(oldTz, cfg.tz) != 0) {
      DBG_INFO("  [%s] Timezone changed: '%s' -> '%s'\n", clientIP.c_str(), oldTz, cfg.tz);
    }
  }

  if (!doc["ntp"].isNull()) {
    strlcpy(cfg.ntp, doc["ntp"].as<const char*>(), sizeof(cfg.ntp));
    if (strcmp(oldNtp, cfg.ntp) != 0) {
      DBG_INFO("  [%s] NTP server changed: '%s' -> '%s'\n", clientIP.c_str(), oldNtp, cfg.ntp);
    }
  }

  if (!doc["use24h"].isNull()) {
    cfg.use24h = doc["use24h"].as<bool>();
    if (oldUse24h != cfg.use24h) {
      DBG_INFO("  [%s] Time format changed: %s -> %s\n", clientIP.c_str(),
               oldUse24h ? "24h" : "12h", cfg.use24h ? "24h" : "12h");
    }
  }

  if (!doc["dateFormat"].isNull()) {
    cfg.dateFormat = (uint8_t)constrain(doc["dateFormat"].as<int>(), 0, 4);
    if (oldDateFormat != cfg.dateFormat) {
      const char* formats[] = {"YYYY-MM-DD", "DD/MM/YYYY", "MM/DD/YYYY", "DD.MM.YYYY", "Mon DD, YYYY"};
      DBG_INFO("  [%s] Date format changed: %s -> %s\n", clientIP.c_str(),
               formats[oldDateFormat], formats[cfg.dateFormat]);
    }
  }

  if (!doc["ledDiameter"].isNull()) {
    cfg.ledDiameter = (uint8_t)doc["ledDiameter"].as<int>();
    if (oldLedDiameter != cfg.ledDiameter) {
      DBG_INFO("  [%s] LED diameter changed: %d -> %d px\n", clientIP.c_str(),
               oldLedDiameter, cfg.ledDiameter);
    }
  }

  if (!doc["ledGap"].isNull()) {
    cfg.ledGap = (uint8_t)doc["ledGap"].as<int>();
    if (oldLedGap != cfg.ledGap) {
      DBG_INFO("  [%s] LED gap changed: %d -> %d px\n", clientIP.c_str(),
               oldLedGap, cfg.ledGap);
    }
  }

  if (!doc["ledColor"].isNull()) {
    cfg.ledColor = doc["ledColor"].as<uint32_t>();
    if (oldLedColor != cfg.ledColor) {
      DBG_INFO("  [%s] LED color changed: #%06X -> #%06X\n", clientIP.c_str(),
               (unsigned int)oldLedColor, (unsigned int)cfg.ledColor);
    }
  }

  if (!doc["brightness"].isNull()) {
    cfg.brightness = (uint8_t)doc["brightness"].as<int>();
    if (oldBrightness != cfg.brightness) {
      DBG_INFO("  [%s] Brightness changed: %d -> %d\n", clientIP.c_str(),
               oldBrightness, cfg.brightness);
    }
  }

  // Morphing speed
  if (!doc["morphSpeed"].isNull()) {
    uint8_t oldMorphSpeed = cfg.morphSpeed;
    cfg.morphSpeed = (uint8_t)constrain(doc["morphSpeed"].as<int>(), 1, 50);
    if (oldMorphSpeed != cfg.morphSpeed) {
      DBG_INFO("  [%s] Morph speed changed: %dx -> %dx\n", clientIP.c_str(),
               oldMorphSpeed, cfg.morphSpeed);
    }
  }

  // Debug level
  if (!doc["debugLevel"].isNull()) {
    uint8_t oldDebugLevel = debugLevel;
    debugLevel = (uint8_t)constrain(doc["debugLevel"].as<int>(), 0, 4);
    if (oldDebugLevel != debugLevel) {
      const char* levels[] = {"Off", "Error", "Warning", "Info", "Verbose"};
      DBG_INFO("  [%s] Debug level changed: %s -> %s\n", clientIP.c_str(),
               levels[oldDebugLevel], levels[debugLevel]);
    }
  }

  // Flip display
  if (!doc["flipDisplay"].isNull()) {
    cfg.flipDisplay = doc["flipDisplay"].as<bool>();
    if (oldFlipDisplay != cfg.flipDisplay) {
      DBG_INFO("  [%s] Display flip changed: %s -> %s\n", clientIP.c_str(),
               oldFlipDisplay ? "flipped" : "normal",
               cfg.flipDisplay ? "flipped" : "normal");
      applyDisplayRotation();  // Apply rotation immediately
      tft.fillScreen(TFT_BLACK);  // Clear screen after rotation change
      memset(fbPrev, 0, sizeof(fbPrev));  // Reset delta buffer to force full redraw
      resetStatusBar();  // Force status bar redraw
    }
  }

  // Temperature unit
  if (!doc["useFahrenheit"].isNull()) {
    bool oldUseFahrenheit = cfg.useFahrenheit;
    cfg.useFahrenheit = doc["useFahrenheit"].as<bool>();
    if (oldUseFahrenheit != cfg.useFahrenheit) {
      DBG_INFO("  [%s] Temperature unit changed: %s -> %s\n", clientIP.c_str(),
               oldUseFahrenheit ? "°F" : "°C",
               cfg.useFahrenheit ? "°F" : "°C");
    }
  }

  // Clock Mode
  if (!doc["clockMode"].isNull()) {
    uint8_t oldClockMode = cfg.clockMode;
    uint8_t newClockMode = (uint8_t)constrain(doc["clockMode"].as<int>(), 0, TOTAL_CLOCK_MODES - 1);
    if (oldClockMode != newClockMode) {
      const char* modes[] = {"Morphing (Classic)", "Tetris", "Morphing (Remix)"};
      DBG_INFO("  [%s] Clock mode changed: %s -> %s\n", clientIP.c_str(),
               modes[oldClockMode], modes[newClockMode]);
      // Update config first
      cfg.clockMode = newClockMode;
      // Update render pitch for new mode (affects status bar height) BEFORE clearing
      updateRenderPitch();
      // Full TFT clear for clean transition - must happen AFTER updateRenderPitch
      tft.fillScreen(TFT_BLACK);
      // Clear the framebuffer
      fbClear();
      // Sync fbPrev with the cleared TFT state (all zeros) for clean comparison
      memset(fbPrev, 0, sizeof(fbPrev));
      resetStatusBar();
      // Reset Tetris clock to force all digits to rebuild with falling blocks
      if (newClockMode == CLOCK_MODE_TETRIS && tetrisClock) {
        tetrisClock->reset();
      }
    }
  }

  // Auto-Rotate
  if (!doc["autoRotate"].isNull()) {
    bool oldAutoRotate = cfg.autoRotate;
    cfg.autoRotate = doc["autoRotate"].as<bool>();
    if (oldAutoRotate != cfg.autoRotate) {
      DBG_INFO("  [%s] Auto-rotate changed: %s -> %s\n", clientIP.c_str(),
               oldAutoRotate ? "ON" : "OFF",
               cfg.autoRotate ? "ON" : "OFF");
      if (cfg.autoRotate) {
        lastModeRotation = millis();  // Reset timer when enabling
      }
    }
  }

  // Rotation Interval
  if (!doc["rotateInterval"].isNull()) {
    uint8_t oldRotateInterval = cfg.rotateInterval;
    cfg.rotateInterval = (uint8_t)constrain(doc["rotateInterval"].as<int>(), 1, 60);
    if (oldRotateInterval != cfg.rotateInterval) {
      DBG_INFO("  [%s] Rotation interval changed: %d -> %d min\n", clientIP.c_str(),
               oldRotateInterval, cfg.rotateInterval);
    }
  }

  // Morphing (Remix) mode - Show Sensor
  if (!doc["morphShowSensor"].isNull()) {
    bool oldShowSensor = cfg.morphShowSensor;
    cfg.morphShowSensor = doc["morphShowSensor"].as<bool>();
    if (oldShowSensor != cfg.morphShowSensor) {
      DBG_INFO("  [%s] Morph show sensor changed: %s -> %s\n", clientIP.c_str(),
               oldShowSensor ? "ON" : "OFF",
               cfg.morphShowSensor ? "ON" : "OFF");
    }
  }

  // Morphing (Remix) mode - Show Date
  if (!doc["morphShowDate"].isNull()) {
    bool oldShowDate = cfg.morphShowDate;
    cfg.morphShowDate = doc["morphShowDate"].as<bool>();
    if (oldShowDate != cfg.morphShowDate) {
      DBG_INFO("  [%s] Morph show date changed: %s -> %s\n", clientIP.c_str(),
               oldShowDate ? "ON" : "OFF",
               cfg.morphShowDate ? "ON" : "OFF");
    }
  }

  // Morphing (Remix) mode - Sensor Color
  if (!doc["morphSensorColor"].isNull()) {
    uint32_t oldColor = cfg.morphSensorColor;
    cfg.morphSensorColor = doc["morphSensorColor"].as<uint32_t>();
    if (oldColor != cfg.morphSensorColor) {
      DBG_INFO("  [%s] Morph sensor color changed: #%06X -> #%06X\n", clientIP.c_str(),
               (unsigned)oldColor, (unsigned)cfg.morphSensorColor);
    }
  }

  // Morphing (Remix) mode - Date Color
  if (!doc["morphDateColor"].isNull()) {
    uint32_t oldColor = cfg.morphDateColor;
    cfg.morphDateColor = doc["morphDateColor"].as<uint32_t>();
    if (oldColor != cfg.morphDateColor) {
      DBG_INFO("  [%s] Morph date color changed: #%06X -> #%06X\n", clientIP.c_str(),
               (unsigned)oldColor, (unsigned)cfg.morphDateColor);
    }
  }

  // Constrain LED rendering parameters
  // ledDiameter: max size of each LED dot (pitch is typically 7 for 480x320)
  // ledGap: space between LEDs (gap + dot <= pitch)
  cfg.ledDiameter = constrain(cfg.ledDiameter, 1, 10);
  cfg.ledGap      = constrain(cfg.ledGap, 0, 8);

  saveConfig();
  updateRenderPitch();  // Rebuild sprite if pitch changed
  startNtp();
  setBacklight(cfg.brightness);

  server.send(200, "application/json", "{\"ok\":true}");
}

static void handleGetMirror() {
  // Framebuffer is now RGB565 (uint16_t), so 2 bytes per pixel
  const size_t fbSize = LED_MATRIX_W * LED_MATRIX_H * sizeof(uint16_t);  // 64 * 32 * 2 = 4096
  DBG_VERBOSE("Mirror: Sending %u bytes (RGB565)\n", (unsigned)fbSize);
  server.sendHeader("Cache-Control", "no-store");
  server.send_P(200, "application/octet-stream", (const char*)fb, fbSize);
}

static void serveStaticFiles() {
  server.on("/", HTTP_GET, []() {
    DBG_VERBOSE("Web: GET / (index.html) from %s\n", server.client().remoteIP().toString().c_str());
    File f = LittleFS.open("/index.html", "r");
    if (!f) {
      DBG_WARN("Web: index.html not found\n");
      server.send(404, "text/plain", "Not found");
      return;
    }
    server.streamFile(f, "text/html");
    f.close();
  });
  server.serveStatic("/app.js", LittleFS, "/app.js");
  server.serveStatic("/style.css", LittleFS, "/style.css");

  server.onNotFound([]() {
    DBG_VERBOSE("Web: 404 %s from %s\n", server.uri().c_str(), server.client().remoteIP().toString().c_str());
    server.send(404, "text/plain", "Not found");
  });
}

// =========================
// WiFi setup
// =========================

/**
 * Callback when WiFiManager enters config portal mode
 * Note: No status LEDs on Touchdown - status visible via web interface
 */
static void configModeCallback(WiFiManager* myWiFiManager) {
  DBG_INFO("Entered WiFi config mode\n");
  DBG("Connect to AP: %s\n", myWiFiManager->getConfigPortalSSID().c_str());
  DBG("Config portal IP: %s\n", WiFi.softAPIP().toString().c_str());
}

static void startWifi() {
  DBG_STEP("Starting WiFi (STA) + WiFiManager...");
  WiFi.mode(WIFI_STA);

  WiFiManager wm;
  wm.setConfigPortalTimeout(180);
  wm.setConnectTimeout(20);
  wm.setAPCallback(configModeCallback);  // Set callback for config portal

  bool ok = wm.autoConnect("Touchdown-RetroClock-Setup");
  if (!ok) {
    DBG_WARN("WiFiManager autoConnect failed/timeout. Starting fallback AP...");
    WiFi.mode(WIFI_AP);
    WiFi.softAP("Touchdown-RetroClock-AP");
  }

  if (WiFi.isConnected()) {
    DBG("WiFi connected: SSID=%s IP=%s\n",
        WiFi.SSID().c_str(),
        WiFi.localIP().toString().c_str());
    DBG_OK("WiFi ready.");
  } else {
    DBG_WARN("WiFi not connected (AP mode).");
  }
}

// =========================
// OTA
// =========================

/**
 * Draw OTA progress bar on TFT display
 * @param progress Progress percentage (0-100)
 */
static void drawOTAProgress(unsigned int progress) {
  const int barWidth = 280;
  const int barHeight = 40;
  const int barX = (tft.width() - barWidth) / 2;
  const int barY = (tft.height() - barHeight) / 2;

  // Clear screen on first progress update
  static bool firstDraw = true;
  if (firstDraw) {
    tft.fillScreen(TFT_BLACK);
    firstDraw = false;
  }

  // Draw title
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setTextFont(4);
  tft.drawString("OTA Update", tft.width() / 2, barY - 50);

  // Draw progress bar border
  tft.drawRect(barX - 2, barY - 2, barWidth + 4, barHeight + 4, TFT_WHITE);

  // Fill progress bar
  int fillWidth = (barWidth * progress) / 100;
  if (fillWidth > 0) {
    // Use gradient colors: red -> yellow -> green
    uint16_t barColor;
    if (progress < 33) {
      barColor = TFT_RED;
    } else if (progress < 66) {
      barColor = TFT_YELLOW;
    } else {
      barColor = TFT_GREEN;
    }
    tft.fillRect(barX, barY, fillWidth, barHeight, barColor);
  }

  // Clear remaining area
  if (fillWidth < barWidth) {
    tft.fillRect(barX + fillWidth, barY, barWidth - fillWidth, barHeight, TFT_BLACK);
  }

  // Draw percentage text
  char progressText[8];
  snprintf(progressText, sizeof(progressText), "%u%%", progress);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextFont(4);
  tft.drawString(progressText, tft.width() / 2, barY + barHeight / 2);

  // Draw size info below bar
  tft.setTextDatum(TC_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.drawString("Please wait...", tft.width() / 2, barY + barHeight + 20);
}

static void startOta() {
  DBG_STEP("Starting OTA...");
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);

  ArduinoOTA.onStart([]() {
    DBG_INFO("OTA update started\n");
    // Clear screen for progress bar
    tft.fillScreen(TFT_BLACK);
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    unsigned int percent = (progress * 100) / total;
    DBG_VERBOSE("OTA Progress: %u%% (%u/%u bytes)\n", percent, progress, total);
    drawOTAProgress(percent);
  });

  ArduinoOTA.onEnd([]() {
    DBG_INFO("OTA update completed\n");
    // Display completion message
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextFont(4);
    tft.drawString("Update Complete!", tft.width() / 2, tft.height() / 2 - 20);
    tft.setTextFont(2);
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawString("Restarting...", tft.width() / 2, tft.height() / 2 + 20);
  });

  ArduinoOTA.onError([](ota_error_t error) {
    DBG_ERROR("OTA update failed: error code %u\n", (unsigned)error);
    // Display error message
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.setTextFont(4);
    tft.drawString("Update Failed!", tft.width() / 2, tft.height() / 2 - 20);

    // Show error details
    tft.setTextFont(2);
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    const char* errorMsg = "Unknown error";
    switch (error) {
      case OTA_AUTH_ERROR: errorMsg = "Auth Failed"; break;
      case OTA_BEGIN_ERROR: errorMsg = "Begin Failed"; break;
      case OTA_CONNECT_ERROR: errorMsg = "Connect Failed"; break;
      case OTA_RECEIVE_ERROR: errorMsg = "Receive Failed"; break;
      case OTA_END_ERROR: errorMsg = "End Failed"; break;
    }
    tft.drawString(errorMsg, tft.width() / 2, tft.height() / 2 + 20);

    // Note: OTA error - check serial console or web interface for details
    delay(3000);

    // Return to normal display after 3 seconds
    tft.fillScreen(TFT_BLACK);
  });

  ArduinoOTA.begin();
  DBG_OK("OTA ready.");
}

// =========================
// Clock logic & drawing
// =========================
static void formatTimeHHMMSS(struct tm& ti, char* out, size_t n) {
  if (cfg.use24h) strftime(out, n, "%H%M%S", &ti);
  else strftime(out, n, "%I%M%S", &ti);
}


static uint32_t lastSecond = 0;
static char prevT[7] = "------";
static char currT[7] = "------";
static int morphStep = MORPH_STEPS;

/**
 * Format date according to user's selected format
 * @param ti Time structure
 * @param out Output buffer
 * @param n Buffer size
 */
static void formatDate(struct tm& ti, char* out, size_t n) {
  switch (cfg.dateFormat) {
    case 1:  // DD/MM/YYYY
      strftime(out, n, "%d/%m/%Y", &ti);
      break;
    case 2:  // MM/DD/YYYY
      strftime(out, n, "%m/%d/%Y", &ti);
      break;
    case 3:  // DD.MM.YYYY
      strftime(out, n, "%d.%m.%Y", &ti);
      break;
    case 4:  // Mon DD, YYYY
      strftime(out, n, "%b %d, %Y", &ti);
      break;
    default:  // 0 = YYYY-MM-DD (ISO 8601)
      strftime(out, n, "%Y-%m-%d", &ti);
      break;
  }
}

static bool updateClockLogic() {
  struct tm ti{};
  if (!getLocalTimeSafe(ti, 50)) return false;

  if ((uint32_t)ti.tm_sec == lastSecond) return false;
  lastSecond = (uint32_t)ti.tm_sec;

  char t6[7] = {0};
  formatTimeHHMMSS(ti, t6, sizeof(t6));
  formatDate(ti, currDate, sizeof(currDate));

  if (strncmp(t6, currT, 6) != 0) {
    memcpy(prevT, currT, 7);
    memcpy(currT, t6, 7);
    morphStep = 0;
    DBG("[TIME] %.2s:%.2s:%.2s\n", currT, currT+2, currT+4);
    return true;  // Time changed, need redraw
  }
  return true;  // Second changed (for morphing animation), need redraw
}

/**
 * Draw a bitmap to the framebuffer with specified intensity
 * @param bm Bitmap to render
 * @param x0 X position in framebuffer
 * @param y0 Y position in framebuffer
 * @param w Width of bitmap
 * @param intensity Brightness level (0-255), default 255
 */
static void drawBitmapSolid(const Bitmap& bm, int x0, int y0, int w, uint8_t intensity = 255) {
  // Convert user's LED color to RGB565
  uint16_t baseColor = rgb888_to_565(cfg.ledColor);

  // Apply intensity scaling to color
  uint8_t r = ((baseColor >> 11) & 0x1F) * intensity / 255;
  uint8_t g = ((baseColor >> 5) & 0x3F) * intensity / 255;
  uint8_t b = (baseColor & 0x1F) * intensity / 255;
  uint16_t color = (r << 11) | (g << 5) | b;

  for (int y=0; y<DIGIT_H; y++) {
    for (int x=0; x<w; x++) {
      bool on = (bm.rows[y] >> (15-x)) & 0x1;
      if (!on) continue;
      int yScaled = (y * LED_MATRIX_H) / DIGIT_H;
      fbSet(x0 + x, y0 + yScaled, color);
    }
  }
}

/**
 * Animated "spawn" morph effect for digit transitions
 * Pixels appear from random positions and move into their final positions
 * @param toBm Target bitmap to morph into
 * @param step Current animation step (0 to MORPH_STEPS)
 * @param x0 X position in framebuffer
 * @param y0 Y position in framebuffer
 * @param w Width of bitmap
 */
static void drawSpawnMorphToTarget(const Bitmap& toBm, int step, int x0, int y0, int w) {
  // Gather all ON pixels in target glyph
  static Pt toPts[420];
  int toN = buildPixelsFromBitmap(toBm, w, toPts, 420);
  if (toN > 420) toN = 420;

  // 0..1
  float t = (float)step / (float)MORPH_STEPS;
  if (t < 0) t = 0;
  if (t > 1) t = 1;

  // Ease-out (nice “snap into place”)
  float te = 1.0f - (1.0f - t) * (1.0f - t);

  // Spawn origin inside the glyph (center-ish)
  const float sx = (float)(w - 1) * 0.5f;
  const float sy = (float)(DIGIT_H - 1) * 0.5f;

  // Fade-in as it moves
  uint8_t alpha = (uint8_t)(255 * t);

  // Convert user's LED color to RGB565 with alpha
  uint16_t baseColor = rgb888_to_565(cfg.ledColor);
  uint8_t r = ((baseColor >> 11) & 0x1F) * alpha / 255;
  uint8_t g = ((baseColor >> 5) & 0x3F) * alpha / 255;
  uint8_t b = (baseColor & 0x1F) * alpha / 255;
  uint16_t color = (r << 11) | (g << 5) | b;

  for (int i=0; i<toN; i++) {
    float tx = (float)toPts[i].x;
    float ty = (float)toPts[i].y;

    float xf = sx + (tx - sx) * te;
    float yf = sy + (ty - sy) * te;

    int x = (int)lroundf(xf);
    int y = (int)lroundf(yf);

    int yScaled = (y * LED_MATRIX_H) / DIGIT_H;
    fbSet(x0 + x, y0 + yScaled, color);
  }
}

/**
 * Main frame rendering function - draws the complete clock display
 * Renders HH:MM:SS format with morphing animations on digit changes
 * Layout: 6 digits + 2 colons + 5 gaps, centered horizontally at top
 */
static void drawFrame() {
  fbClear(0);

  const int digitW = DIGIT_W;
  const int colonW = COLON_W;
  const int gap = DIGIT_GAP;

  // HH:MM:SS with gaps between digit pairs for readability
  // Total width = (6 * digitW) + (2 * colonW) + (5 * gap)
  // Gaps: after each digit except the last one in each pair
  const int totalW = (6 * digitW) + (2 * colonW) + (5 * gap);
  int x0 = (LED_MATRIX_W - totalW) / 2;
  if (x0 < 0) x0 = 0;
  const int y0 = 0;  // Clock at top of display

  auto digitIdx = [&](char c)->int { return (c>='0' && c<='9') ? (c-'0') : 0; };

  // Indices for each digit
  int c[6] = {
    digitIdx(currT[0]), digitIdx(currT[1]),
    digitIdx(currT[2]), digitIdx(currT[3]),
    digitIdx(currT[4]), digitIdx(currT[5])
  };
  int p[6] = {
    digitIdx(prevT[0]), digitIdx(prevT[1]),
    digitIdx(prevT[2]), digitIdx(prevT[3]),
    digitIdx(prevT[4]), digitIdx(prevT[5])
  };

  int step = morphStep;
  if (step > MORPH_STEPS) step = MORPH_STEPS;

  auto drawDigit = [&](int pos, int xx) {
    if (currT[pos] != prevT[pos] && step < MORPH_STEPS) {
      // Digit changed → redraw whole digit with spawn morph
      drawSpawnMorphToTarget(DIGITS[c[pos]], step, xx, y0, digitW);
    } else {
      // Digit unchanged or morph finished → solid draw
      drawBitmapSolid(DIGITS[c[pos]], xx, y0, digitW, 255);
    }
  };

  // HH with gap between digits
  drawDigit(0, x0);
  drawDigit(1, x0 + digitW + gap);

  // : (flashing colon)
  if (clockColon) {
    drawBitmapSolid(COLON, x0 + 2*digitW + gap, y0, colonW, 255);
  }

  // MM with gap between digits
  drawDigit(2, x0 + 2*digitW + gap + colonW + gap);
  drawDigit(3, x0 + 3*digitW + 2*gap + colonW + gap);

  // : (flashing colon)
  if (clockColon) {
    drawBitmapSolid(COLON, x0 + 4*digitW + 2*gap + colonW + gap, y0, colonW, 255);
  }

  // SS with gap between digits
  drawDigit(4, x0 + 4*digitW + 2*gap + 2*colonW + 2*gap);
  drawDigit(5, x0 + 5*digitW + 3*gap + 2*colonW + 2*gap);

  // Calculate effective morph steps based on morphSpeed multiplier (1-10)
  // morphSpeed=1: 20 frames, morphSpeed=10: 200 frames
  int effectiveMorphSteps = MORPH_STEPS * cfg.morphSpeed;
  if (morphStep < effectiveMorphSteps) morphStep++;
}

/**
 * Tetris Clock Mode - Renders time using falling Tetris block animations
 * Uses TetrisMatrixDraw library to create animated digit transitions
 * Respects 12/24 hour format and shows AM/PM indicator for 12-hour mode
 */
static void drawFrameTetris() {
  if (!tetrisClock) return;  // Safety check

  fbClear(0);  // Clear framebuffer

  // Get the 24-hour format hour for AM/PM determination
  int hour24 = (currT[0] - '0') * 10 + (currT[1] - '0');
  bool isPM = (hour24 >= 12);

  // Format time string based on 12/24 hour preference
  String timeStr;
  if (cfg.use24h) {
    // 24-hour format: "HH:MM" (00:00 to 23:59)
    timeStr = String(currT[0]) + String(currT[1]) + ":" +
              String(currT[2]) + String(currT[3]);
  } else {
    // 12-hour format: " H:MM" or "HH:MM" (space-padded for single digit hours)
    int hour = hour24;
    if (hour == 0) hour = 12;  // Midnight is 12 AM
    else if (hour > 12) hour -= 12;  // Convert to 12-hour

    if (hour < 10) {
      timeStr = " " + String(hour) + ":" + String(currT[2]) + String(currT[3]);
    } else {
      timeStr = String(hour) + ":" + String(currT[2]) + String(currT[3]);
    }
  }

  // Update Tetris clock (handles animation internally)
  // Returns true when animation is complete, false while animating
  tetrisClock->update(timeStr, cfg.use24h, clockColon, isPM);
}

/**
 * Draw a segment pixel with bounds checking for the LED matrix
 */
static void drawLEDSegmentPixel(int x, int y, uint16_t color) {
  if (x >= 0 && x < LED_MATRIX_W && y >= 0 && y < LED_MATRIX_H) {
    fb[y][x] = color;
  }
}

/**
 * Helper function to draw LED segments with proper bounds checking
 * Segments are drawn as 1-pixel thick lines (thickness handled by caller)
 */
static void drawLEDSegment(int x1, int y1, int x2, int y2, int thickness, uint8_t brightness, uint16_t baseColor) {
  if (brightness == 0) return;  // Don't draw invisible segments

  // Apply brightness to color
  uint8_t r = ((baseColor >> 11) & 0x1F) * brightness / 255;
  uint8_t g = ((baseColor >> 5) & 0x3F) * brightness / 255;
  uint8_t b = (baseColor & 0x1F) * brightness / 255;
  uint16_t color = (r << 11) | (g << 5) | b;

  // Draw line from (x1,y1) to (x2,y2) with bounds checking
  int dx = abs(x2 - x1);
  int dy = abs(y2 - y1);
  int steps = max(dx, dy);
  
  for (int i = 0; i <= steps; i++) {
    float t = steps > 0 ? (float)i / steps : 0.0f;
    int x = x1 + (int)((x2 - x1) * t);
    int y = y1 + (int)((y2 - y1) * t);
    drawLEDSegmentPixel(x, y, color);
  }

  // Draw thickness (1 pixel on each side of the center line)
  if (dx > dy) {
    // Horizontal segment - add thickness above/below
    for (int i = 0; i <= steps; i++) {
      float t = steps > 0 ? (float)i / steps : 0.0f;
      int x = x1 + (int)((x2 - x1) * t);
      int y = y1 + (int)((y2 - y1) * t);
      drawLEDSegmentPixel(x, y + 1, color);  // One pixel below
    }
  } else if (dy > dx) {
    // Vertical segment - add thickness to the right
    for (int i = 0; i <= steps; i++) {
      float t = steps > 0 ? (float)i / steps : 0.0f;
      int x = x1 + (int)((x2 - x1) * t);
      int y = y1 + (int)((y2 - y1) * t);
      drawLEDSegmentPixel(x + 1, y, color);  // One pixel to the right
    }
  }
}

/**
 * Draw a single LED dot at the specified position
 * LED has a glowing effect with center brighter than edges
 * @param x X position in framebuffer
 * @param y Y position in framebuffer
 * @param color Base LED color (RGB565)
 * @param brightness Brightness level (0-255)
 */
static void drawLEDDot(int x, int y, uint16_t color, uint8_t brightness) {
  if (x < 0 || y < 0 || x >= LED_MATRIX_W || y >= LED_MATRIX_H) return;

  // Apply brightness to color
  uint8_t r = ((color >> 11) & 0x1F) * brightness / 255;
  uint8_t g = ((color >> 5) & 0x3F) * brightness / 255;
  uint8_t b = (color & 0x1F) * brightness / 255;
  uint16_t scaledColor = (r << 11) | (g << 5) | b;

  // Draw LED dot as a small filled circle pattern
  // Center pixel (brightest)
  fb[y][x] = scaledColor;

  // Small glow effect (neighboring pixels at reduced brightness)
  uint8_t glow = brightness / 3;
  uint8_t gr = ((color >> 11) & 0x1F) * glow / 255;
  uint8_t gg = ((color >> 5) & 0x3F) * glow / 255;
  uint8_t gb = (color & 0x1F) * glow / 255;
  uint16_t glowColor = (gr << 11) | (gg << 5) | gb;

  // Draw surrounding glow pixels if within bounds
  if (x > 0) fb[y][x - 1] = glowColor;
  if (x < LED_MATRIX_W - 1) fb[y][x + 1] = glowColor;
  if (y > 0) fb[y - 1][x] = glowColor;
  if (y < LED_MATRIX_H - 1) fb[y + 1][x] = glowColor;
}

/**
 * Draw a segment as a row of LED dots (classic LED display look)
 * @param x1 Start X position
 * @param y1 Start Y position
 * @param x2 End X position
 * @param y2 End Y position
 * @param numLEDs Number of LEDs in this segment
 * @param brightness Brightness level (0-255)
 * @param color Base LED color (RGB565)
 */
static void drawLEDSegmentDots(int x1, int y1, int x2, int y2, int numLEDs, uint8_t brightness, uint16_t color) {
  if (brightness == 0) return;

  // Calculate LED positions along the segment
  for (int i = 0; i < numLEDs; i++) {
    float t = (float)i / (float)(numLEDs - 1);
    int x = x1 + (int)((x2 - x1) * t);
    int y = y1 + (int)((y2 - y1) * t);
    drawLEDDot(x, y, color, brightness);
  }
}

/**
 * Render a single morphing digit at the specified position
 * Draws each segment as a row of LED dots with the digit's color
 * @param digit The MorphingDigit instance to render
 * @param offsetX X offset in matrix coordinates
 * @param offsetY Y offset in matrix coordinates
 */
static void renderMorphingDigit(MorphingDigit* digit, int offsetX, int offsetY, uint16_t color) {
  // Use the provided color (user's configured LED color) instead of per-digit colors

  // Render all 7 segments
  for (int seg = 0; seg < 7; seg++) {
    uint8_t brightness = digit->getSegmentBrightness(seg);
    if (brightness == 0) continue;  // Skip off segments

    const SegmentCoords& coords = SEGMENT_COORDS[seg];

    // Apply offset and render segment as LED dots
    int x1 = offsetX + coords.x1;
    int y1 = offsetY + coords.y1;
    int x2 = offsetX + coords.x2;
    int y2 = offsetY + coords.y2;

    // thickness field is now used to store number of LEDs per segment
    int numLEDs = coords.thickness;
    if (numLEDs < 2) numLEDs = 2;  // Minimum 2 LEDs per segment

    drawLEDSegmentDots(x1, y1, x2, y2, numLEDs, brightness, color);
  }
}

/**
 * Draw morphing clock display (CLOCK_MODE_MORPH)
 * Layout: HH:MM:SS centered on 64x32 matrix with 8x32 digit slots
 * Seconds update instantly without morphing for clear readability
 */
static void drawFrameMorph() {
  fbClear(0);  // Clear framebuffer

  // Parse current time
  uint8_t hourTens = currT[0] - '0';
  uint8_t hourUnits = currT[1] - '0';
  uint8_t minuteTens = currT[2] - '0';
  uint8_t minuteUnits = currT[3] - '0';
  uint8_t secondTens = currT[4] - '0';
  uint8_t secondUnits = currT[5] - '0';

  // Only update morphing targets when the digit actually changes (for HH and MM)
  // Seconds update instantly without morphing for clear readability
  if (currT[0] != prevT[0]) morphHourTens.setTarget(hourTens);
  if (currT[1] != prevT[1]) morphHourUnits.setTarget(hourUnits);
  if (currT[2] != prevT[2]) morphMinuteTens.setTarget(minuteTens);
  if (currT[3] != prevT[3]) morphMinuteUnits.setTarget(minuteUnits);
  
  // Seconds: update instantly without morphing
  morphSecondTens.setCurrent(secondTens);
  morphSecondUnits.setCurrent(secondUnits);

  // Update morphing animations every frame for smooth transitions (HH and MM only)
  unsigned long now = millis();
  unsigned long delta = now - lastMorphUpdate;
  if (delta > 100) delta = 100;  // Cap delta to prevent jumps

  morphHourTens.update(delta);
  morphHourUnits.update(delta);
  morphMinuteTens.update(delta);
  morphMinuteUnits.update(delta);

  lastMorphUpdate = now;

  // Digit positioning for 64x32 matrix
  // Layout: HH : MM : SS with proper spacing and visible colons
  // Compact digits (18 rows) fit between sensor (y=0-4) and date (y=27-31)
  // 6 digits at 7px wide = 42px, 2 colons at 2px = 4px, 2 digitGaps at 1px = 2px, 4 colonGaps at 1px = 4px
  // Total width = 42 + 4 + 2 + 4 = 52px (shifted left slightly with 5px left margin)

  const int digitWidth = 7;    // Digit width (matches segment coords)
  const int digitGap = 1;      // Gap between digits within same group (HH, MM, SS)
  const int colonGap = 1;      // Gap before/after colons (1 LED pixel)
  const int colonWidth = 2;    // Colon width (2px for 2x2 LED dots)
  const int startX = 5;        // Start with 5px margin on left (shifted left by 1 LED)
  const int startY = 6;        // Start at y=6 (digits span y=7-24, leaving y=25-26 as 2-row gap before date at y=27)

  uint16_t ledColor = cfg.ledColor;

  // Create dimmed color for colons (75% brightness)
  // Extract RGB565 components and dim them
  uint16_t r = (ledColor >> 11) & 0x1F;
  uint16_t g = (ledColor >> 5) & 0x3F;
  uint16_t b = ledColor & 0x1F;
  r = (r * 3) / 4;
  g = (g * 3) / 4;
  b = (b * 3) / 4;
  uint16_t colonColor = (r << 11) | (g << 5) | b;

  // Calculate positions for each digit with proper spacing
  int x = startX;

  // HH (hours)
  renderMorphingDigit(&morphHourTens, x, startY, ledColor);
  x += digitWidth + digitGap;
  renderMorphingDigit(&morphHourUnits, x, startY, ledColor);
  x += digitWidth + colonGap;

  // First colon (between hours and minutes) - 2x2 LED dots with dimmed color
  if (clockColon) {
    int colonY1 = startY + 5;   // Upper dot position (aligned with upper segment)
    int colonY2 = startY + 13;  // Lower dot position (aligned with lower segment)
    // Draw 2x2 colon dots
    for (int dy = 0; dy < 2; dy++) {
      for (int dx = 0; dx < 2; dx++) {
        int px = x + dx;
        int py1 = colonY1 + dy;
        int py2 = colonY2 + dy;
        if (px >= 0 && px < LED_MATRIX_W) {
          if (py1 >= 0 && py1 < LED_MATRIX_H) fb[py1][px] = colonColor;
          if (py2 >= 0 && py2 < LED_MATRIX_H) fb[py2][px] = colonColor;
        }
      }
    }
  }
  x += colonWidth + colonGap;

  // MM (minutes)
  renderMorphingDigit(&morphMinuteTens, x, startY, ledColor);
  x += digitWidth + digitGap;
  renderMorphingDigit(&morphMinuteUnits, x, startY, ledColor);
  x += digitWidth + colonGap;

  // Second colon (between minutes and seconds) - 2x2 LED dots with dimmed color
  if (clockColon) {
    int colonY1 = startY + 5;   // Upper dot position (aligned with upper segment)
    int colonY2 = startY + 13;  // Lower dot position (aligned with lower segment)
    // Draw 2x2 colon dots
    for (int dy = 0; dy < 2; dy++) {
      for (int dx = 0; dx < 2; dx++) {
        int px = x + dx;
        int py1 = colonY1 + dy;
        int py2 = colonY2 + dy;
        if (px >= 0 && px < LED_MATRIX_W) {
          if (py1 >= 0 && py1 < LED_MATRIX_H) fb[py1][px] = colonColor;
          if (py2 >= 0 && py2 < LED_MATRIX_H) fb[py2][px] = colonColor;
        }
      }
    }
  }
  x += colonWidth + colonGap;

  // SS (seconds)
  renderMorphingDigit(&morphSecondTens, x, startY, ledColor);
  x += digitWidth + digitGap;
  renderMorphingDigit(&morphSecondUnits, x, startY, ledColor);

  // Add date display at BOTTOM of matrix (if enabled)
  // Using y=27 ensures date (5 rows tall) spans y=27-31 within 32-row framebuffer (y=0-31)
  // This leaves y=26 as gap between clock digits and date (1 row gap)
  // Format currDate string (e.g., "12/01/2026" or "2026-01-12")
  if (cfg.morphShowDate && currDate[0] != '-') {  // Check if enabled and date is valid
    // Center date horizontally
    int dateWidth = getTextWidth3x5(currDate);
    int dateX = (LED_MATRIX_W - dateWidth) / 2;
    // Use custom date color (RGB888 -> RGB565)
    uint16_t dateColor = rgb888_to_565(cfg.morphDateColor);
    drawText3x5(currDate, dateX, 27, dateColor);
  }

  // Add sensor data at TOP of matrix (if enabled)
  // Display format based on available sensor data
  if (cfg.morphShowSensor && sensorAvailable) {
    char sensorLine[64];  // Buffer for sensor text

    // Convert temperature to display unit
    int displayTemp = temperature;
    const char* tempUnit = cfg.useFahrenheit ? "F" : "C";
    if (cfg.useFahrenheit) {
      displayTemp = (temperature * 9 / 5) + 32;
    }

    // Format sensor string based on available sensor capabilities
    // Compact format to fit in 64 LED matrix width
    #if defined(USE_BME280)
      // BME280: Temperature, Humidity, Pressure
      snprintf(sensorLine, sizeof(sensorLine), "%d%s %d%% %dHPA",
               displayTemp, tempUnit, humidity, pressure);
    #elif defined(USE_BMP280) || defined(USE_BMP180)
      // BMP280/BMP180: Temperature and Pressure only
      snprintf(sensorLine, sizeof(sensorLine), "%d%s %dHPA",
               displayTemp, tempUnit, pressure);
    #elif defined(USE_SHT3X) || defined(USE_HTU21D)
      // SHT3X/HTU21D: Temperature and Humidity only
      snprintf(sensorLine, sizeof(sensorLine), "%d%s %d%%",
               displayTemp, tempUnit, humidity);
    #else
      // Unknown sensor, just show temp
      snprintf(sensorLine, sizeof(sensorLine), "%d%s", displayTemp, tempUnit);
    #endif

    // Draw at top of matrix (y=0), centered horizontally like the date
    int sensorWidth = getTextWidth3x5(sensorLine);
    int sensorX = (LED_MATRIX_W - sensorWidth) / 2;
    // Use custom sensor color (RGB888 -> RGB565)
    uint16_t sensorColor = rgb888_to_565(cfg.morphSensorColor);
    drawText3x5(sensorLine, sensorX, 0, sensorColor);
  }
}


// =========================
// Clock Mode Management
// =========================

/**
 * Switch to a new clock mode with fade transition
 * @param newMode The clock mode to switch to (0=7-seg, 1=Tetris)
 */
static void switchClockMode(uint8_t newMode) {
  if (newMode >= TOTAL_CLOCK_MODES) return;  // Invalid mode
  if (newMode == cfg.clockMode) return;  // Already in this mode

  DBG_INFO("Switching clock mode: %d -> %d\n", cfg.clockMode, newMode);

  // Update mode
  cfg.clockMode = newMode;

  // Update render pitch for new mode (affects status bar height)
  updateRenderPitch();

  // Full TFT clear for clean transition
  tft.fillScreen(TFT_BLACK);

  // Clear the framebuffer
  fbClear();

  // Sync fbPrev with the cleared TFT state (all zeros) for clean comparison
  memset(fbPrev, 0, sizeof(fbPrev));

  // Reset status bar
  resetStatusBar();

  // Reset Tetris clock to force all digits to rebuild with falling blocks
  if (newMode == CLOCK_MODE_TETRIS && tetrisClock) {
    tetrisClock->reset();
  }

  // Save to NVS
  saveConfig();
}

/**
 * Check if auto-rotation should trigger a mode change
 */
static void checkAutoRotation() {
  if (!cfg.autoRotate) return;

  unsigned long now = millis();
  unsigned long interval = (unsigned long)cfg.rotateInterval * 60000UL;  // Convert minutes to milliseconds

  if (now - lastModeRotation >= interval) {
    // Rotate to next mode
    uint8_t nextMode = (cfg.clockMode + 1) % TOTAL_CLOCK_MODES;
    switchClockMode(nextMode);
    lastModeRotation = now;
  }
}

/**
 * Render the current clock mode
 */
static void renderCurrentMode() {
  switch (cfg.clockMode) {
    case CLOCK_MODE_7SEG:
      drawFrame();
      break;

    case CLOCK_MODE_TETRIS:
      drawFrameTetris();
      break;

    case CLOCK_MODE_MORPH:
      drawFrameMorph();
      break;

    default:
      drawFrame();  // Fallback to 7-seg
      break;
  }
}

/**
 * Check if current mode needs continuous updates (for animations)
 * @return true if mode is animating and needs frequent updates
 */
static bool modeNeedsAnimation() {
  if (cfg.clockMode == CLOCK_MODE_TETRIS && tetrisClock) {
    return tetrisClock->isAnimating();
  }
  return false;
}

// =========================
// LED Matrix Splash Screen
// =========================

// Full TFT display dimensions
#define SPLASH_TFT_W 480
#define SPLASH_TFT_H 320

// LED grid for text display (scaled to fill TFT)
#define SPLASH_LED_SIZE 7      // Size of each LED dot
#define SPLASH_LED_GAP 1       // Gap between LEDs
#define SPLASH_PITCH (SPLASH_LED_SIZE + SPLASH_LED_GAP)  // Total pitch per LED

// Check if touch occurred (for skipping splash)
static bool splashTouchDetected() {
#if ENABLE_TOUCH
  return touch.touched();
#else
  return false;
#endif
}

/**
 * Draw a single LED dot for splash screen (64x32 grid scaled to TFT)
 */
static void drawSplashLED(int gridX, int gridY, uint16_t color) {
  // Calculate screen position (centered on display)
  int offsetX = (SPLASH_TFT_W - (64 * SPLASH_PITCH)) / 2;
  int offsetY = (SPLASH_TFT_H - (32 * SPLASH_PITCH)) / 2;
  int screenX = offsetX + gridX * SPLASH_PITCH;
  int screenY = offsetY + gridY * SPLASH_PITCH;

  // Draw rounded LED dot
  tft.fillRoundRect(screenX, screenY, SPLASH_LED_SIZE, SPLASH_LED_SIZE, 1, color);
}

/**
 * Phase 1: RGB Test Pattern - Full screen LED pixel sweeps
 * Returns true if touch detected (to skip)
 */
static bool splashRGBTest() {
  const uint16_t colors[] = {TFT_RED, TFT_GREEN, TFT_BLUE};
  const int dotSize = 6;       // Size of each LED dot
  const int dotGap = 2;        // Gap between dots
  const int pitch = dotSize + dotGap;  // Total pitch per LED
  const int cols = SPLASH_TFT_W / pitch;
  const int rows = SPLASH_TFT_H / pitch;

  for (int c = 0; c < 3; c++) {
    // Sweep from left to right - draw LED dots 2 columns at a time
    for (int col = 0; col < cols; col += 2) {
      if (splashTouchDetected()) return true;
      for (int cc = 0; cc < 2 && (col + cc) < cols; cc++) {
        int x = (col + cc) * pitch;
        for (int row = 0; row < rows; row++) {
          int y = row * pitch;
          tft.fillRoundRect(x, y, dotSize, dotSize, 1, colors[c]);
        }
      }
      delay(4);
    }
    delay(150);  // Pause at full color

    // Clear sweep from left to right - 2 columns at a time
    for (int col = 0; col < cols; col += 2) {
      if (splashTouchDetected()) return true;
      for (int cc = 0; cc < 2 && (col + cc) < cols; cc++) {
        int x = (col + cc) * pitch;
        for (int row = 0; row < rows; row++) {
          int y = row * pitch;
          tft.fillRoundRect(x, y, dotSize, dotSize, 1, TFT_BLACK);
        }
      }
      delay(2);
    }
    delay(80);  // Pause between colors
  }
  return false;
}

/**
 * Phase 2: Random pixel noise across full TFT that settles
 * Returns true if touch detected (to skip)
 */
static bool splashPixelNoise() {
  const uint16_t colors[] = {TFT_RED, TFT_GREEN, TFT_BLUE, TFT_YELLOW, TFT_CYAN, TFT_MAGENTA, TFT_WHITE};
  const int dotSize = 6;

  // Fill with random colored pixels across full TFT
  for (int i = 0; i < 800; i++) {
    if (splashTouchDetected()) return true;
    int x = random(SPLASH_TFT_W - dotSize);
    int y = random(SPLASH_TFT_H - dotSize);
    uint16_t color = colors[random(7)];
    tft.fillRoundRect(x, y, dotSize, dotSize, 1, color);
    if (i % 15 == 0) delay(3);
  }
  delay(500);  // Pause to see the noise

  // Gradually clear with black dots
  for (int i = 0; i < 600; i++) {
    if (splashTouchDetected()) return true;
    int x = random(SPLASH_TFT_W - dotSize);
    int y = random(SPLASH_TFT_H - dotSize);
    tft.fillRoundRect(x, y, dotSize, dotSize, 1, TFT_BLACK);
    if (i % 15 == 0) delay(3);
  }

  return false;
}

/**
 * 3x5 pixel font data for splash screen text
 * Each character is 3 pixels wide, 5 pixels tall
 * Stored as 5 bytes (one per row), LSB = leftmost pixel
 */
static const uint8_t splashFont3x5[][5] = {
  // Space (index 0)
  {0b000, 0b000, 0b000, 0b000, 0b000},
  // A-Z (indices 1-26)
  {0b010, 0b101, 0b111, 0b101, 0b101},  // A
  {0b110, 0b101, 0b110, 0b101, 0b110},  // B
  {0b011, 0b100, 0b100, 0b100, 0b011},  // C
  {0b110, 0b101, 0b101, 0b101, 0b110},  // D
  {0b111, 0b100, 0b110, 0b100, 0b111},  // E
  {0b111, 0b100, 0b110, 0b100, 0b100},  // F
  {0b011, 0b100, 0b101, 0b101, 0b011},  // G
  {0b101, 0b101, 0b111, 0b101, 0b101},  // H
  {0b111, 0b010, 0b010, 0b010, 0b111},  // I
  {0b001, 0b001, 0b001, 0b101, 0b010},  // J
  {0b101, 0b110, 0b100, 0b110, 0b101},  // K
  {0b100, 0b100, 0b100, 0b100, 0b111},  // L
  {0b101, 0b111, 0b111, 0b101, 0b101},  // M
  {0b101, 0b111, 0b111, 0b111, 0b101},  // N
  {0b010, 0b101, 0b101, 0b101, 0b010},  // O
  {0b110, 0b101, 0b110, 0b100, 0b100},  // P
  {0b010, 0b101, 0b101, 0b110, 0b011},  // Q
  {0b110, 0b101, 0b110, 0b101, 0b101},  // R
  {0b011, 0b100, 0b010, 0b001, 0b110},  // S
  {0b111, 0b010, 0b010, 0b010, 0b010},  // T
  {0b101, 0b101, 0b101, 0b101, 0b010},  // U
  {0b101, 0b101, 0b101, 0b010, 0b010},  // V
  {0b101, 0b101, 0b111, 0b111, 0b101},  // W
  {0b101, 0b101, 0b010, 0b101, 0b101},  // X
  {0b101, 0b101, 0b010, 0b010, 0b010},  // Y
  {0b111, 0b001, 0b010, 0b100, 0b111},  // Z
  // 0-9 (indices 27-36)
  {0b010, 0b101, 0b101, 0b101, 0b010},  // 0
  {0b010, 0b110, 0b010, 0b010, 0b111},  // 1
  {0b110, 0b001, 0b010, 0b100, 0b111},  // 2
  {0b110, 0b001, 0b010, 0b001, 0b110},  // 3
  {0b101, 0b101, 0b111, 0b001, 0b001},  // 4
  {0b111, 0b100, 0b110, 0b001, 0b110},  // 5
  {0b011, 0b100, 0b110, 0b101, 0b010},  // 6
  {0b111, 0b001, 0b010, 0b010, 0b010},  // 7
  {0b010, 0b101, 0b010, 0b101, 0b010},  // 8
  {0b010, 0b101, 0b011, 0b001, 0b110},  // 9
};

/**
 * Get font index for a character
 */
static int getSplashFontIndex(char c) {
  if (c == ' ') return 0;
  if (c >= 'A' && c <= 'Z') return c - 'A' + 1;
  if (c >= 'a' && c <= 'z') return c - 'a' + 1;  // Treat lowercase as uppercase
  if (c >= '0' && c <= '9') return c - '0' + 27;
  return 0;  // Default to space
}

/**
 * Structure to track pixel positions for animation
 */
struct SplashPixel {
  int8_t x;
  int8_t y;
  uint16_t color;
  int8_t velocityY;  // For falling animation
  bool active;
};

#define MAX_SPLASH_PIXELS 512
static SplashPixel splashPixels[MAX_SPLASH_PIXELS];
static int splashPixelCount = 0;

/**
 * Phase 3: Display text pixel-by-pixel
 * Returns true if touch detected (to skip)
 */
static bool splashShowText(const char* text, int startY, uint16_t color) {
  splashPixelCount = 0;

  // Calculate text width
  int textLen = strlen(text);
  int textWidth = textLen * 4 - 1;  // 3px char + 1px gap, minus last gap
  int startX = (64 - textWidth) / 2;

  // Build pixel list and draw pixel-by-pixel
  for (int i = 0; i < textLen && splashPixelCount < MAX_SPLASH_PIXELS; i++) {
    if (splashTouchDetected()) return true;

    int charIndex = getSplashFontIndex(text[i]);
    int charX = startX + i * 4;

    for (int row = 0; row < 5; row++) {
      uint8_t rowData = splashFont3x5[charIndex][row];
      for (int col = 0; col < 3; col++) {
        if (rowData & (1 << (2 - col))) {  // MSB is leftmost pixel
          int px = charX + col;
          int py = startY + row;

          // Store pixel for later animation
          if (splashPixelCount < MAX_SPLASH_PIXELS) {
            splashPixels[splashPixelCount].x = px;
            splashPixels[splashPixelCount].y = py;
            splashPixels[splashPixelCount].color = color;
            splashPixels[splashPixelCount].velocityY = 0;
            splashPixels[splashPixelCount].active = true;
            splashPixelCount++;
          }

          // Draw with slight delay for pixel-by-pixel effect
          drawSplashLED(px, py, color);
          delay(8);  // Delay per pixel for visible effect
        }
      }
    }
    delay(80);  // Delay between characters (slowed from 30ms)
  }

  return false;
}

/**
 * Phase 4: Dissolve text - pixels fall off screen
 * Returns true if touch detected (to skip)
 */
static bool splashDissolveText() {
  // Initially all pixels are stationary (velocityY = 0)
  for (int i = 0; i < splashPixelCount; i++) {
    splashPixels[i].velocityY = 0;
  }

  // Animate until all pixels are off screen
  bool anyActive = true;
  int frameCount = 0;

  while (anyActive && frameCount < 150) {  // More frames for slower animation
    if (splashTouchDetected()) return true;

    anyActive = false;

    // Randomly start some pixels falling (slower rate)
    for (int i = 0; i < splashPixelCount; i++) {
      if (splashPixels[i].active && splashPixels[i].velocityY == 0) {
        if (random(100) < 8) {  // 8% chance to start falling (was 15%)
          splashPixels[i].velocityY = 1;  // Start slow
        }
      }
    }

    // Update and draw all pixels
    for (int i = 0; i < splashPixelCount; i++) {
      if (!splashPixels[i].active) continue;

      // Erase old position
      drawSplashLED(splashPixels[i].x, splashPixels[i].y, TFT_BLACK);

      // Update position if falling
      if (splashPixels[i].velocityY > 0) {
        splashPixels[i].y += splashPixels[i].velocityY;

        // Accelerate more gradually
        if (frameCount % 5 == 0 && splashPixels[i].velocityY < 3) {
          splashPixels[i].velocityY++;
        }
      }

      // Check if still on screen
      if (splashPixels[i].y < 32) {
        // Draw at new position
        drawSplashLED(splashPixels[i].x, splashPixels[i].y, splashPixels[i].color);
        anyActive = true;
      } else {
        splashPixels[i].active = false;
      }
    }

    delay(60);  // Slower frame rate (was 40ms)
    frameCount++;
  }

  return false;
}

/**
 * Show "64x32" size indicator with corner brackets on full TFT
 * Returns true if touch detected (to skip)
 */
static bool splashShowGridSize() {
  // Draw corner brackets at TFT corners - animated
  uint16_t bracketColor = TFT_DARKGREY;
  const int bracketLen = 40;   // Length of bracket arms in pixels
  const int bracketThick = 4;  // Thickness of bracket lines

  // Animate corners appearing (draw in steps)
  for (int i = 0; i <= bracketLen; i += 4) {
    if (splashTouchDetected()) return true;

    // Top-left corner
    tft.fillRect(0, 0, i, bracketThick, bracketColor);           // Horizontal
    tft.fillRect(0, 0, bracketThick, i, bracketColor);           // Vertical

    // Top-right corner
    tft.fillRect(SPLASH_TFT_W - i, 0, i, bracketThick, bracketColor);
    tft.fillRect(SPLASH_TFT_W - bracketThick, 0, bracketThick, i, bracketColor);

    // Bottom-left corner
    tft.fillRect(0, SPLASH_TFT_H - bracketThick, i, bracketThick, bracketColor);
    tft.fillRect(0, SPLASH_TFT_H - i, bracketThick, i, bracketColor);

    // Bottom-right corner
    tft.fillRect(SPLASH_TFT_W - i, SPLASH_TFT_H - bracketThick, i, bracketThick, bracketColor);
    tft.fillRect(SPLASH_TFT_W - bracketThick, SPLASH_TFT_H - i, bracketThick, i, bracketColor);

    delay(25);
  }

  if (splashTouchDetected()) return true;
  delay(400);

  // Show "64X32" in center using LED grid
  if (splashShowText("64X32", 14, TFT_WHITE)) return true;
  delay(1500);  // Longer pause to read

  return false;
}

/**
 * Main splash screen sequence
 * Runs the full LED matrix demonstration
 */
static void showSplashScreen() {
  tft.fillScreen(TFT_BLACK);

  // Phase 1: RGB color test sweeps
  if (splashRGBTest()) {
    tft.fillScreen(TFT_BLACK);
    return;
  }

  // Phase 2: Random pixel noise
  if (splashPixelNoise()) {
    tft.fillScreen(TFT_BLACK);
    return;
  }

  // Clear for text
  tft.fillScreen(TFT_BLACK);
  delay(400);

  // Phase 3: Show grid size with corner brackets
  if (splashShowGridSize()) {
    tft.fillScreen(TFT_BLACK);
    return;
  }

  // Dissolve "64x32"
  if (splashDissolveText()) {
    tft.fillScreen(TFT_BLACK);
    return;
  }

  delay(500);
  tft.fillScreen(TFT_BLACK);
  delay(300);

  // Phase 4: Show "HUB75 LED MATRIX" text
  if (splashShowText("HUB75 LED", 10, TFT_GREEN)) {
    tft.fillScreen(TFT_BLACK);
    return;
  }
  delay(300);
  if (splashShowText("MATRIX", 18, TFT_GREEN)) {
    tft.fillScreen(TFT_BLACK);
    return;
  }
  delay(600);

  // Phase 5: Show "EMULATOR" below
  if (splashShowText("EMULATOR", 26, TFT_CYAN)) {
    tft.fillScreen(TFT_BLACK);
    return;
  }
  delay(1200);  // Pause to read the full message

  // Final dissolve - all text falls
  if (splashDissolveText()) {
    tft.fillScreen(TFT_BLACK);
    return;
  }

  delay(500);
  tft.fillScreen(TFT_BLACK);
}

// =========================
// Startup Display Functions
// =========================

static int startupY = 10;  // Current Y position for startup text
static const int startupLineHeight = 18;  // Line height for startup messages (Font 2: 16px height)

/**
 * Initialize the startup display
 */
static void initStartupDisplay() {
  tft.init();
  tft.setRotation(1);  // Landscape orientation for ESP32 Touchdown
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextFont(2);  // Font 2 (16px height) - good middle ground between size 1 and 2
  startupY = 10;

  // Title
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("ESP32 TOUCHDOWN RETRO CLOCK", 10, startupY);
  startupY += startupLineHeight;

  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawString("Firmware v" FIRMWARE_VERSION, 10, startupY);
  startupY += startupLineHeight + 2;

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
}

/**
 * Display a startup step message on TFT with simple wrap-around
 * @param msg Message to display
 * @param color Text color (default white)
 */
static void showStartupStep(const char* msg, uint16_t color = TFT_WHITE) {
  // Check if we need to wrap to top (use more of the screen - 320px height)
  if (startupY > tft.height() - startupLineHeight - 10) {
    // Simple approach: wrap back to top and clear that area
    startupY = 46;  // Start below title area (reduced to fit more on screen)

    // Clear the next 3 lines where we'll write
    tft.fillRect(0, startupY, tft.width(), startupLineHeight * 3, TFT_BLACK);
  }

  tft.setTextColor(color, TFT_BLACK);
  tft.drawString(msg, 10, startupY);
  startupY += startupLineHeight;
  yield();  // Feed watchdog after drawing
}

/**
 * Display a startup status message (OK, ERROR, WARNING)
 * @param status Status indicator ("OK", "ERROR", "WARN", etc.)
 * @param msg Message to display
 */
static void showStartupStatus(const char* status, const char* msg) {
  uint16_t statusColor = TFT_GREEN;

  if (strcmp(status, "ERROR") == 0 || strcmp(status, "ERR") == 0) {
    statusColor = TFT_RED;
  } else if (strcmp(status, "WARN") == 0) {
    statusColor = TFT_ORANGE;
  } else if (strcmp(status, "OK") == 0) {
    statusColor = TFT_GREEN;
  } else {
    statusColor = TFT_CYAN;
  }

  char buffer[80];
  snprintf(buffer, sizeof(buffer), "[%s] %s", status, msg);
  showStartupStep(buffer, statusColor);
}

/**
 * Display a startup step with inline status
 * @param msg Message to display (e.g., "Mounting filesystem...")
 * @param status Status indicator ("OK", "ERROR", "WARN", etc.)
 */
static void showStartupStepWithStatus(const char* msg, const char* status) {
  uint16_t statusColor = TFT_GREEN;

  if (strcmp(status, "ERROR") == 0 || strcmp(status, "ERR") == 0) {
    statusColor = TFT_RED;
  } else if (strcmp(status, "WARN") == 0) {
    statusColor = TFT_ORANGE;
  } else if (strcmp(status, "OK") == 0) {
    statusColor = TFT_GREEN;
  } else {
    statusColor = TFT_CYAN;
  }

  // Draw message in white
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(msg, 10, startupY);

  // Draw status in color on same line
  char statusBuf[20];
  snprintf(statusBuf, sizeof(statusBuf), "[%s]", status);
  tft.setTextColor(statusColor, TFT_BLACK);
  tft.drawString(statusBuf, tft.textWidth(msg) + 10, startupY);

  startupY += startupLineHeight;
  yield();  // Feed watchdog after drawing
}

// =========================
// Setup / Loop
// =========================
void setup() {
  Serial.begin(115200);
  delay(250);

  DBGLN("");
  DBGLN("========================================");
  DBGLN(" ESP32 Touchdown RGB LED Matrix (HUB75) Retro Clock - DEBUG BOOT");
  DBGLN("========================================");

  DBG("Build: %s %s\n", __DATE__, __TIME__);
  DBG("LED grid: %dx%d (fb size: %u bytes)\n", LED_MATRIX_W, LED_MATRIX_H, (unsigned)sizeof(fb));
  DBG("TFT_eSPI version check...\n");

  // Initialize TFT for startup display FIRST
  initStartupDisplay();

  char buildInfo[60];
  snprintf(buildInfo, sizeof(buildInfo), "Build: %s %s", __DATE__, __TIME__);
  showStartupStep(buildInfo);

  // Note: ESP32 Touchdown does not have built-in status LEDs
  // WiFi reset can be handled via web interface or serial commands
  bool resetWiFi = false;

  initBitmaps();
  loadConfig();
  showStartupStepWithStatus("Loading bitmaps & config... ", "OK");

  // Reset WiFi if button was held during boot
  if (resetWiFi) {
    DBG_INFO("Resetting WiFi credentials...\n");
    showStartupStatus("INFO", "Resetting WiFi...");
    WiFiManager wm;
    wm.resetSettings();
    delay(1000);
    DBG_OK("WiFi credentials cleared!");
    showStartupStatus("OK", "WiFi reset");
  }

  // Mount filesystem
  DBG_STEP("Mounting LittleFS...");
  if (!LittleFS.begin(true)) {
    DBG_ERR("LittleFS mount failed");
    showStartupStepWithStatus("Mounting filesystem... ", "ERROR");
  } else {
    DBG_OK("LittleFS mounted");
    showStartupStepWithStatus("Mounting filesystem... ", "OK");
  }

  // Apply display settings from config
  applyDisplayRotation();  // Apply rotation based on config
  setBacklight(cfg.brightness);
  DBG("TFT size (w x h): %d x %d\n", tft.width(), tft.height());
  DBG_OK("TFT ready.");
  showStartupStepWithStatus("Configuring display... ", "OK");

  // Initialize framebuffer rendering (direct TFT mode - sprite disabled for smooth morphing)
  updateRenderPitch(true);

#if DISABLE_SPRITE_RENDERING
  DBG_INFO("Using direct TFT rendering (sprite disabled for smooth performance)\n");
  showStartupStepWithStatus("Initializing framebuffer... ", "OK");
#else
  // Sprite rendering mode (currently disabled)
  DBG_STEP("Creating framebuffer sprite...");
  int sprW = LED_MATRIX_W * fbPitch;
  int sprH = LED_MATRIX_H * fbPitch;

  if (useSprite) {
    DBG("Sprite OK: %dx%d\n", sprW, sprH);
    DBG_OK("Sprite ready.");
    showStartupStepWithStatus("Initializing framebuffer... ", "OK");
  } else {
    DBG_WARN("Sprite create failed, using direct TFT\n");
    showStartupStepWithStatus("Initializing framebuffer... ", "WARN");
  }
#endif

  // Initialize Tetris Clock (silent - no TFT output needed)
  DBG_STEP("Initializing Tetris clock...");
  tetrisClock = new TetrisClock(fb);
  DBG_OK("Tetris clock ready.");

  // Initialize mode rotation timer
  lastModeRotation = millis();
  DBG("Clock mode: %d, Auto-rotate: %s, Interval: %d min\n",
      cfg.clockMode,
      cfg.autoRotate ? "ON" : "OFF",
      cfg.rotateInterval);

  // WiFi
  startWifi();
  showStartupStepWithStatus("Starting WiFi... ", "OK");

  // Sensor
  sensorAvailable = testSensor();
  if (sensorAvailable) {
    updateSensorData();
    lastSensorUpdate = millis();
    DBG_OK("Sensor initialized and reading.");
    showStartupStepWithStatus("Checking sensor... ", "OK");
  } else {
    DBG_WARN("No sensor detected. Temperature/humidity features disabled.");
    showStartupStepWithStatus("Checking sensor... ", "WARN");
  }
  delay(500);

  // Touch Controller
#if ENABLE_TOUCH
  bool touchAvailable = initTouch();
  if (touchAvailable) {
    showStartupStepWithStatus("Initializing touch... ", "OK");
  } else {
    showStartupStepWithStatus("Initializing touch... ", "WARN");
  }
  delay(500);
#endif

  // NTP, OTA, Web
  startNtp();
  startOta();

  DBG_STEP("Starting WebServer + routes...");
  serveStaticFiles();
  server.on("/api/state", HTTP_GET, handleGetState);
  server.on("/api/config", HTTP_POST, handlePostConfig);
  server.on("/api/mirror", HTTP_GET, handleGetMirror);
  server.on("/api/timezones", HTTP_GET, handleGetTimezones);
  server.on("/api/reset-wifi", HTTP_POST, handleResetWiFi);
  server.on("/api/reboot", HTTP_POST, handleReboot);
  server.begin();
  DBG_OK("WebServer ready.");
  showStartupStepWithStatus("Starting services... ", "OK");

  // Show IP address
  char ipMsg[50];
  if (WiFi.isConnected()) {
    snprintf(ipMsg, sizeof(ipMsg), "IP: %s ", WiFi.localIP().toString().c_str());
    showStartupStepWithStatus(ipMsg, "OK");
  } else {
    showStartupStepWithStatus("IP: Not connected ", "WARN");
  }
  DBG("Ready. IP: %s\n", WiFi.isConnected() ? WiFi.localIP().toString().c_str() : "0.0.0.0");

  // Show "Ready" message (removed empty line to prevent wrap)
  showStartupStatus("READY", "System initialized!");
  delay(1500);  // Brief pause to see final status

  // Show LED Matrix splash screen animation
  showSplashScreen();

  // Initialize morphing clock digits with current time to avoid morphing from 00:00:00
  // Wait for NTP to sync and get initial time
  struct tm ti{};
  if (getLocalTimeSafe(ti, 1000)) {  // Wait up to 1 second for time
    char t6[7] = {0};
    formatTimeHHMMSS(ti, t6, sizeof(t6));
    memcpy(currT, t6, 7);  // Set currT to current time

    // Initialize morphing digits to current time (no morphing on startup)
    morphHourTens.setCurrent(currT[0] - '0');
    morphHourUnits.setCurrent(currT[1] - '0');
    morphMinuteTens.setCurrent(currT[2] - '0');
    morphMinuteUnits.setCurrent(currT[3] - '0');
    morphSecondTens.setCurrent(currT[4] - '0');
    morphSecondUnits.setCurrent(currT[5] - '0');

    DBG("Morphing digits initialized to: %.2s:%.2s:%.2s\n", currT, currT+2, currT+4);
  }

  // Clear startup display and start normal operation
  tft.fillScreen(TFT_BLACK);
  memset(fbPrev, 0, sizeof(fbPrev));  // Initialize delta buffer for clean first frame
  resetStatusBar();  // Force status bar to draw on first frame
}

void loop() {
  ArduinoOTA.handle();
  server.handleClient();

  uint32_t now = millis();

  // Update sensor data periodically
  if (sensorAvailable && (now - lastSensorUpdate >= SENSOR_UPDATE_INTERVAL)) {
    updateSensorData();
    lastSensorUpdate = now;
  }

  // Check auto-rotation timer
  checkAutoRotation();

  // Handle touch input
#if ENABLE_TOUCH
  handleTouch();
#endif

  // Toggle colon blink every second (for Tetris mode)
  if (now - lastColonToggle >= 1000) {
    clockColon = !clockColon;
    lastColonToggle = now;
  }

  // Skip clock rendering if info page is active
#if ENABLE_TOUCH
  if (infoPageActive) {
    return;  // Info page is displayed, don't render clock
  }
#endif

  // Update clock logic only on second change (once per second)
  // This is where we detect time changes
  bool timeChanged = updateClockLogic();

  // Determine if display needs update
  bool needsUpdate = false;

  // Force first render after boot
  if (firstRender) {
    needsUpdate = true;
    firstRender = false;
  }

  if (cfg.clockMode == CLOCK_MODE_7SEG) {
    // Morphing mode: update on time change or during morph animation
    needsUpdate = timeChanged || morphStep < MORPH_STEPS;
  } else if (cfg.clockMode == CLOCK_MODE_TETRIS) {
    // Tetris mode: update at controlled interval for visible block animation
    if (timeChanged || modeNeedsAnimation()) {
      needsUpdate = true;
    }
    // Also update at regular interval for animation frames (controlled by TETRIS_ANIMATION_SPEED)
    if (now - lastTetrisUpdate >= TETRIS_ANIMATION_SPEED) {
      needsUpdate = true;
      lastTetrisUpdate = now;
    }
  } else if (cfg.clockMode == CLOCK_MODE_MORPH) {
    // Morphing Remix mode: update on time change or while any digit is morphing
    bool anyMorphing = morphHourTens.isMorphing() || morphHourUnits.isMorphing() ||
                       morphMinuteTens.isMorphing() || morphMinuteUnits.isMorphing() ||
                       morphSecondTens.isMorphing() || morphSecondUnits.isMorphing();
    needsUpdate = timeChanged || anyMorphing;

    // Also update at regular interval for smooth animation (60 FPS = ~16ms)
    static unsigned long lastMorphRender = 0;
    if (now - lastMorphRender >= 16) {
      needsUpdate = true;
      lastMorphRender = now;
    }
  }

  // Render and display if needed
  if (needsUpdate) {
    renderCurrentMode();
    renderFBToTFT();
  }
}
