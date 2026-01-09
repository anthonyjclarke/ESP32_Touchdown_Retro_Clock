/*
 * ESP32 Touchdown LED Matrix (HUB75) Retro Clock
 * Version: See FIRMWARE_VERSION in include/config.h
 *
 * A retro-style RGB LED Matrix (HUB75) clock emulator for the ESP32 Touchdown with ILI9488 480×320 TFT display
 *
 * FEATURES:
 * - 64×32 virtual RGB LED Matrix (HUB75) emulation on 480×320 TFT display
 * - Large 7-segment style digits with morphing animations
 * - WiFi configuration via WiFiManager (AP mode fallback)
 * - NTP time synchronization with timezone support (88 timezones across 13 regions)
 * - Web-based configuration interface with live display mirror
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
 * - Backlight control via GPIO32 (PWM capable)
 * - WiFi connectivity (2.4GHz)
 * - Battery management (optional LiPo battery)
 * - Emulates 64×32 RGB LED Matrix Panel (HUB75 style)
 *
 * DISPLAY LAYOUT:
 * - Top: Large clock digits (HH:MM:SS) rendered as RGB LED Matrix (HUB75)
 * - Bottom: Status bar (IP address, date, timezone)
 *
 * CONFIGURATION:
 * - First boot: Creates WiFi AP "Touchdown-RetroClock-Setup"
 * - Connect and configure WiFi credentials via captive portal
 * - Access web UI at device IP address
 * - Adjust timezone (88 options), NTP server (9 presets), time/date format
 * - Customize LED appearance, brightness, and debug level
 * - All changes apply instantly and persist to NVS
 *
 * WEB API ENDPOINTS:
 * - GET  /              - Main web interface
 * - GET  /api/state     - Current system state (JSON with diagnostics)
 * - POST /api/config    - Update configuration (logs changes to Serial)
 * - GET  /api/mirror    - Raw framebuffer data for display mirror
 * - GET  /api/timezones - List of 88 global timezones grouped by region
 *
 * FUTURE ENHANCEMENTS:
 * - [ ] Touch panel support for direct UI interaction
 * - [ ] Multiple display modes (clock, date, temp, custom messages)
 * - [ ] Per-LED color control for RGB LED Matrix effects
 * - [ ] Color themes and presets
 * - [ ] MQTT integration for remote control and monitoring
 * - [ ] Mobile-friendly web interface enhancements
 * - [ ] Home Assistant integration
 * - [ ] Web-based OTA upload interface
 * - [ ] Customizable animations and transition effects
 * - [ ] Battery level indicator and power management
 *
 * Author: Built with the help of Claude Code
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

// Sensor libraries (only one will be used based on config.h)
#ifdef USE_BME280
  #include <Adafruit_BME280.h>
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
TFT_eSprite spr = TFT_eSprite(&tft);

WebServer server(HTTP_PORT);
Preferences prefs;

// Sensor objects (only one will be initialized based on configuration)
#ifdef USE_BME280
  Adafruit_BME280 bme280;
#endif
#ifdef USE_SHT3X
  Adafruit_SHT31 sht3x = Adafruit_SHT31();
#endif
#ifdef USE_HTU21D
  Adafruit_HTU21DF htu21d = Adafruit_HTU21DF();
#endif

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
  uint8_t clockMode = DEFAULT_CLOCK_MODE;           // 0=7-seg, 1=Tetris, etc.
  bool autoRotate = DEFAULT_AUTO_ROTATE;            // Auto-rotate through modes
  uint8_t rotateInterval = DEFAULT_ROTATE_INTERVAL; // Minutes between rotations

  // Sensor settings
  bool useFahrenheit = false;   // false=Celsius, true=Fahrenheit
};

AppConfig cfg;

// Sensor state variables
bool sensorAvailable = false;
int temperature = 0;
int humidity = 0;
int pressure = 0;
const char* sensorType = "NONE";  // Will be set based on detected sensor
unsigned long lastSensorUpdate = 0;

// Logical RGB LED Matrix (HUB75) framebuffer: 0..255 intensity
static uint8_t fb[LED_MATRIX_H][LED_MATRIX_W];
static uint8_t fbPrev[LED_MATRIX_H][LED_MATRIX_W];  // Previous frame for delta rendering

// Cached date string for status bar
static char currDate[11] = "----/--/--";
static uint8_t appliedDot = 0;
static uint8_t appliedGap = 0;
static uint8_t appliedPitch = 0;

// Sprite settings for flicker-free debug renderer
static int fbPitch = 2;      // logical LED -> TFT pixels (computed from TFT size + config)
static bool useSprite = false;

// Clock mode management
TetrisClock* tetrisClock = nullptr;  // Tetris clock instance (created in setup)
unsigned long lastModeRotation = 0;  // Last time clock mode was rotated
const uint8_t TOTAL_CLOCK_MODES = 2; // 0=7-seg, 1=Tetris
bool clockColon = true;              // Colon blink state
unsigned long lastColonToggle = 0;   // Last colon toggle time
uint8_t fadeLevel = 255;             // Current fade level (0-255) for mode transitions
bool inTransition = false;           // True during mode transition fade

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
 * Clear the entire framebuffer to a specific intensity value
 * @param v Intensity value (0-255), default 0 (off)
 */
static void fbClear(uint8_t v = 0) { memset(fb, v, sizeof(fb)); }

/**
 * Set a single pixel in the framebuffer with bounds checking
 * @param x X coordinate (0 to LED_MATRIX_W-1)
 * @param y Y coordinate (0 to LED_MATRIX_H-1)
 * @param v Intensity value (0-255)
 */
static inline void fbSet(int x, int y, uint8_t v) {
  if (x < 0 || y < 0 || x >= LED_MATRIX_W || y >= LED_MATRIX_H) return;
  fb[y][x] = v;
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

// Morph between two bitmaps into fb, step=0..MORPH_STEPS
static void drawMorph(const Bitmap& a, const Bitmap& b, int step, int x0, int y0, int w) {
  for (int y=0; y<DIGIT_H; y++) {
    for (int x=0; x<w; x++) {
      bool aon = (a.rows[y] >> (15-x)) & 0x1;
      bool bon = (b.rows[y] >> (15-x)) & 0x1;

      uint8_t val = 0;
      if (aon && bon) val = 255;
      else if (aon && !bon) val = (uint8_t)(255 * (MORPH_STEPS - step) / MORPH_STEPS);
      else if (!aon && bon) val = (uint8_t)(255 * step / MORPH_STEPS);

      if (val == 0) continue;

      int yScaled = (y * LED_MATRIX_H) / DIGIT_H;
      fbSet(x0 + x, y0 + yScaled, val);
    }
  }
}

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

static inline int dist2(const Pt& a, const Pt& b) {
  int dx = (int)a.x - (int)b.x;
  int dy = (int)a.y - (int)b.y;
  return dx*dx + dy*dy;
}

// Particle morph between two bitmaps into fb, step=0..MORPH_STEPS
static void drawParticleMorph(const Bitmap& fromBm,
                              const Bitmap& toBm,
                              int step, int x0, int y0,
                              int w)
{
  // Worst case: almost full 16x24 = 384 pixels.
  // Our 7-seg glyphs are much smaller, but allocate safely.
  static Pt fromPts[420];
  static Pt toPts[420];
  static int matchTo[420];
  static bool toUsed[420];

  int fromN = buildPixelsFromBitmap(fromBm, w, fromPts, 420);
  int toN   = buildPixelsFromBitmap(toBm,   w, toPts,   420);

  // Clamp counts to our buffers
  if (fromN > 420) fromN = 420;
  if (toN > 420) toN = 420;

  // Greedy nearest-neighbour matching (good enough for small glyphs)
  for (int j=0; j<toN; j++) toUsed[j] = false;

  int pairs = min(fromN, toN);
  for (int i=0; i<pairs; i++) {
    int bestJ = -1;
    int bestD = 1e9;
    for (int j=0; j<toN; j++) {
      if (toUsed[j]) continue;
      int d = dist2(fromPts[i], toPts[j]);
      if (d < bestD) { bestD = d; bestJ = j; }
    }
    if (bestJ < 0) bestJ = 0;
    matchTo[i] = bestJ;
    toUsed[bestJ] = true;
  }

  // Interp factor 0..1
  float t = (float)step / (float)MORPH_STEPS;

  // 1) Move matched particles
  for (int i=0; i<pairs; i++) {
    Pt a = fromPts[i];
    Pt b = toPts[matchTo[i]];

    float xf = a.x + (b.x - a.x) * t;
    float yf = a.y + (b.y - a.y) * t;

    int x = (int)lroundf(xf);
    int y = (int)lroundf(yf);

    int yScaled = (y * LED_MATRIX_H) / DIGIT_H;

    // Full intensity (motion provides the morph effect)
    fbSet(x0 + x, y0 + yScaled, 255);
  }

  // 2) Pixels that exist only in TO: fade in
  if (toN > fromN) {
    int extra = toN - fromN;
    float alpha = t; // 0->1
    for (int j=0; j<toN && extra>0; j++) {
      if (toUsed[j]) continue;
      Pt p = toPts[j];
      int yScaled = (p.y * LED_MATRIX_H) / DIGIT_H;
      fbSet(x0 + p.x, y0 + yScaled, (uint8_t)(255 * alpha));
      extra--;
    }
  }

  // 3) Pixels that exist only in FROM: fade out
  if (fromN > toN) {
    float alpha = 1.0f - t; // 1->0
    for (int i=toN; i<fromN; i++) {
      Pt p = fromPts[i];
      int yScaled = (p.y * LED_MATRIX_H) / DIGIT_H;
      fbSet(x0 + p.x, y0 + yScaled, (uint8_t)(255 * alpha));
    }
  }
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
  int matrixAreaH = tft.height() - STATUS_BAR_H;
  if (matrixAreaH < 1) matrixAreaH = tft.height();

  // With Touchdown 480x320 landscape:
  // pitch = min(480/64, matrixAreaH/32) = min(7, matrixAreaH/32)
  // Maximum pitch is 7 (limited by width), giving 480×224 display
  // Use min to ensure it fits in both dimensions
  int maxPitch = min(tft.width() / LED_MATRIX_W, matrixAreaH / LED_MATRIX_H);
  if (maxPitch < 1) maxPitch = 1;
  return maxPitch;
}

static void rebuildSprite(int pitch) {
  if (useSprite) {
    spr.deleteSprite();
    useSprite = false;
  }

  spr.setColorDepth(16);
  int sprW = LED_MATRIX_W * pitch;
  int sprH = LED_MATRIX_H * pitch;
  if (sprW <= 0 || sprH <= 0) return;

  if (spr.createSprite(sprW, sprH)) {
    useSprite = true;
    spr.fillSprite(TFT_BLACK);
  } else {
    useSprite = false;
  }
}

static void updateRenderPitch(bool force = false) {
  int pitch = computeRenderPitch();
  if (!force && pitch == fbPitch && useSprite) return;
  fbPitch = pitch;
  rebuildSprite(fbPitch);
}

static void drawStatusBar() {
#if STATUS_BAR_H > 0
  static uint32_t lastDrawMs = 0;
  static char lastLine1[64] = "";
  static char lastLine2[64] = "";

  int barY = tft.height() - STATUS_BAR_H;
  if (barY < 0) barY = tft.height();

  char line1[64];
  // Line 1: Temperature and Humidity
  if (sensorAvailable) {
    int displayTemp = cfg.useFahrenheit ? (temperature * 9 / 5 + 32) : temperature;
    const char* tempUnit = cfg.useFahrenheit ? "oF" : "oC";  // Using 'o' as degree symbol
    snprintf(line1, sizeof(line1), "Temp: %d%s  Humidity: %d%%", displayTemp, tempUnit, humidity);
  } else {
    snprintf(line1, sizeof(line1), "Sensor: Not detected");
  }

  char line2[64];
  // Line 2: Date and Timezone
  snprintf(line2, sizeof(line2), "%s  %s", currDate, cfg.tz);

  uint32_t now = millis();
  bool changed = (strncmp(line1, lastLine1, sizeof(line1)) != 0) ||
                 (strncmp(line2, lastLine2, sizeof(line2)) != 0);
  if (!changed && (now - lastDrawMs) < 1000) return;

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
  const int pitch = fbPitch;
  const int sprW = LED_MATRIX_W * pitch; // 320 when 64x32 with pitch 5
  const int sprH = LED_MATRIX_H * pitch; // 160 when 64x32 with pitch 5

  int matrixAreaH = tft.height() - STATUS_BAR_H;
  if (matrixAreaH < sprH) matrixAreaH = tft.height();

  int x0 = (tft.width()  - sprW) / 2;
  int y0 = (matrixAreaH - sprH) / 2;

  // Base RGB components from cfg.ledColor
  const uint8_t baseR = (cfg.ledColor >> 16) & 0xFF;
  const uint8_t baseG = (cfg.ledColor >> 8)  & 0xFF;
  const uint8_t baseB = (cfg.ledColor >> 0)  & 0xFF;

  int gapWanted = (int)cfg.ledGap;
  if (gapWanted < 0) gapWanted = 0;
  if (gapWanted > pitch - 1) gapWanted = pitch - 1;

  int dot = pitch - gapWanted;
  int maxDot = (int)cfg.ledDiameter;
  if (maxDot < 1) maxDot = 1;
  if (dot > maxDot) dot = maxDot;
  if (dot < 1) dot = 1;

  int gap = pitch - dot;
  const int inset = (pitch - dot) / 2;
  appliedDot = (uint8_t)dot;
  appliedGap = (uint8_t)gap;
  appliedPitch = (uint8_t)pitch;

  // Verbose debug output (print once per second)
  static uint32_t lastDbg = 0;
  if (millis() - lastDbg > 1000) {
    DBG_VERBOSE("Render: pitch=%d dot=%d gap=%d ledD=%d ledG=%d\n",
                pitch, dot, gap, cfg.ledDiameter, cfg.ledGap);
    lastDbg = millis();
  }

  if (useSprite && !DISABLE_SPRITE_RENDERING) {
    spr.fillSprite(TFT_BLACK);

    for (int y = 0; y < LED_MATRIX_H; y++) {
      for (int x = 0; x < LED_MATRIX_W; x++) {
        uint8_t v = fb[y][x];
        if (!v) continue;

        // Scale base color by intensity v (0..255)
        uint8_t r = (uint8_t)((baseR * (uint16_t)v) / 255);
        uint8_t g = (uint8_t)((baseG * (uint16_t)v) / 255);
        uint8_t b = (uint8_t)((baseB * (uint16_t)v) / 255);

        uint16_t colScaled = rgb888_to_565(((uint32_t)r << 16) | ((uint32_t)g << 8) | b);

        spr.fillRect(x * pitch + inset, y * pitch + inset, dot, dot, colScaled);
      }
    }

    // Batch SPI operations: startWrite/endWrite reduces flashing
    tft.startWrite();
    spr.pushSprite(x0, y0);
    tft.endWrite();
    drawStatusBar();
    return;
  }

  // -------------------------
  // Fallback (direct draw): delta rendering
  // Only update pixels that changed from previous frame (no flashing from full clears)
  // -------------------------

  tft.startWrite();  // Batch all SPI writes for speed
  
  for (int y = 0; y < LED_MATRIX_H; y++) {
    for (int x = 0; x < LED_MATRIX_W; x++) {
      uint8_t v = fb[y][x];
      uint8_t vPrev = fbPrev[y][x];
      
      // Only update pixels that changed
      if (v == vPrev) continue;

      uint16_t colScaled;
      if (v == 0) {
        // Pixel turned off - draw black
        colScaled = TFT_BLACK;
      } else {
        // Pixel is on - draw with intensity
        uint8_t r = (uint8_t)((baseR * (uint16_t)v) / 255);
        uint8_t g = (uint8_t)((baseG * (uint16_t)v) / 255);
        uint8_t b = (uint8_t)((baseB * (uint16_t)v) / 255);
        colScaled = rgb888_to_565(((uint32_t)r << 16) | ((uint32_t)g << 8) | b);
      }

      tft.fillRect(x0 + x * pitch + inset, y0 + y * pitch + inset, dot, dot, colScaled);
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

  // Update pressure if valid (only for BME280)
  if (!isnan(pres) && pres >= 800 && pres <= 1200) {
    pressure = (int)round(pres);
  }

  // Output sensor readings to serial (always at INFO level for visibility)
  if (cfg.useFahrenheit) {
    int tempF = temperature * 9 / 5 + 32;
    DBG_INFO("Sensor Update - %s: %d°F (%d°C), Humidity: %d%%",
             sensorType, tempF, temperature, humidity);
  } else {
    DBG_INFO("Sensor Update - %s: %d°C, Humidity: %d%%",
             sensorType, temperature, humidity);
  }

#ifdef USE_BME280
  if (pressure > 0) {
    DBG_INFO(", Pressure: %d hPa\n", pressure);
  } else {
    DBG_INFO("\n");
  }
#else
  DBG_INFO("\n");
#endif
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

  // Hardware info (static)
  doc["board"] = "ESP32 Touchdown";
  doc["display"] = "480×320 ILI9488";

  // Build sensor info string
  String sensorInfo;
  if (sensorAvailable) {
    sensorInfo = String(sensorType);
#ifdef USE_BME280
    sensorInfo += " (Temp/Humid/Press)";
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
      const char* modes[] = {"7-Segment", "Tetris"};
      DBG_INFO("  [%s] Clock mode changed: %s -> %s\n", clientIP.c_str(),
               modes[oldClockMode], modes[newClockMode]);
      switchClockMode(newClockMode);  // Use fade transition
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
  const size_t fbSize = LED_MATRIX_W * LED_MATRIX_H;  // 64 * 32 = 2048
  DBG_VERBOSE("Mirror: Sending %u bytes\n", (unsigned)fbSize);
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
  for (int y=0; y<DIGIT_H; y++) {
    for (int x=0; x<w; x++) {
      bool on = (bm.rows[y] >> (15-x)) & 0x1;
      if (!on) continue;
      int yScaled = (y * LED_MATRIX_H) / DIGIT_H;
      fbSet(x0 + x, y0 + yScaled, intensity);
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

  for (int i=0; i<toN; i++) {
    float tx = (float)toPts[i].x;
    float ty = (float)toPts[i].y;

    float xf = sx + (tx - sx) * te;
    float yf = sy + (ty - sy) * te;

    int x = (int)lroundf(xf);
    int y = (int)lroundf(yf);

    int yScaled = (y * LED_MATRIX_H) / DIGIT_H;
    fbSet(x0 + x, y0 + yScaled, alpha);
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

  // :
  drawBitmapSolid(COLON, x0 + 2*digitW + gap, y0, colonW, 255);

  // MM with gap between digits
  drawDigit(2, x0 + 2*digitW + gap + colonW + gap);
  drawDigit(3, x0 + 3*digitW + 2*gap + colonW + gap);

  // :
  drawBitmapSolid(COLON, x0 + 4*digitW + 2*gap + colonW + gap, y0, colonW, 255);

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

  // Format time string based on 12/24 hour preference
  String timeStr;
  if (cfg.use24h) {
    // 24-hour format: "HH:MM" (00:00 to 23:59)
    timeStr = String(currT[0]) + String(currT[1]) + ":" +
              String(currT[2]) + String(currT[3]);
  } else {
    // 12-hour format: " H:MM" or "HH:MM" (space-padded for single digit hours)
    int hour = (currT[0] - '0') * 10 + (currT[1] - '0');
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
  tetrisClock->update(timeStr, cfg.use24h, clockColon);
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

  // Start fade out transition
  inTransition = true;
  fadeLevel = 255;

  // Update mode
  cfg.clockMode = newMode;

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
 * Apply fade effect to framebuffer
 * @param level Brightness level (0-255, where 0=black, 255=full brightness)
 */
static void applyFade(uint8_t level) {
  if (level == 255) return;  // No fade needed

  for (int y = 0; y < LED_MATRIX_H; y++) {
    for (int x = 0; x < LED_MATRIX_W; x++) {
      // Scale pixel intensity by fade level
      uint16_t pixel = fb[y][x];
      fb[y][x] = (pixel * level) / 255;
    }
  }
}

/**
 * Render the current clock mode with fade transition support
 */
static void renderCurrentMode() {
  // Handle fade transition
  if (inTransition) {
    // Fade out (255 -> 128)
    if (fadeLevel > 128) {
      fadeLevel -= 8;  // Fade out speed
      if (fadeLevel < 128) fadeLevel = 128;
    }
    // Fade in (128 -> 255)
    else {
      fadeLevel += 8;  // Fade in speed
      if (fadeLevel >= 255) {
        fadeLevel = 255;
        inTransition = false;
      }
    }
  }

  // Render based on current mode
  switch (cfg.clockMode) {
    case CLOCK_MODE_7SEG:
      drawFrame();
      break;

    case CLOCK_MODE_TETRIS:
      drawFrameTetris();
      break;

    default:
      drawFrame();  // Fallback to 7-seg
      break;
  }

  // Apply fade if in transition
  if (inTransition || fadeLevel < 255) {
    applyFade(fadeLevel);
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

  // Note: ESP32 Touchdown does not have built-in status LEDs
  // WiFi reset can be handled via web interface or serial commands
  bool resetWiFi = false;

  initBitmaps();
  loadConfig();

  // Reset WiFi if button was held during boot
  if (resetWiFi) {
    DBG_INFO("Resetting WiFi credentials...\n");
    WiFiManager wm;
    wm.resetSettings();
    delay(1000);
    DBG_OK("WiFi credentials cleared!");
  }

  DBG_STEP("Mounting LittleFS...");
  if (!LittleFS.begin(true)) {
    DBG_ERR("LittleFS mount failed");
  } else {
    DBG_OK("LittleFS mounted");
  }

  // TFT init
  DBG_STEP("Initialising TFT...");
  tft.init();
  applyDisplayRotation();  // Apply rotation based on config
  tft.fillScreen(TFT_BLACK);
  setBacklight(cfg.brightness);
  DBG("TFT size (w x h): %d x %d\n", tft.width(), tft.height());
  DBG_OK("TFT ready.");

  // Create SMALL sprite for framebuffer rendering (avoid RAM issues)
  DBG_STEP("Creating framebuffer sprite (small)...");
  updateRenderPitch(true);
  int sprW = LED_MATRIX_W * fbPitch;
  int sprH = LED_MATRIX_H * fbPitch;

  if (useSprite) {
    int matrixAreaH = tft.height() - STATUS_BAR_H;
    if (matrixAreaH < sprH) matrixAreaH = tft.height();
    int x0 = (tft.width()-sprW)/2;
    int y0 = (matrixAreaH - sprH)/2;
    tft.fillScreen(TFT_BLACK);
    spr.pushSprite(x0, y0);
    DBG("Sprite OK: %dx%d\n", sprW, sprH);
    DBG_OK("Sprite ready.");
  } else {
    DBG_WARN("Sprite create FAILED. Falling back to direct draw (may flicker).");
  }

  // Initialize Tetris Clock
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

  // Sensor
  sensorAvailable = testSensor();
  if (sensorAvailable) {
    updateSensorData();
    lastSensorUpdate = millis();
    DBG_OK("Sensor initialized and reading.");
  } else {
    DBG_WARN("No sensor detected. Temperature/humidity features disabled.");
  }

  // NTP
  startNtp();

  // OTA
  startOta();

  // Web
  DBG_STEP("Starting WebServer + routes...");
  serveStaticFiles();
  server.on("/api/state", HTTP_GET, handleGetState);
  server.on("/api/config", HTTP_POST, handlePostConfig);
  server.on("/api/mirror", HTTP_GET, handleGetMirror);
  server.on("/api/timezones", HTTP_GET, handleGetTimezones);
  server.on("/api/reset-wifi", HTTP_POST, handleResetWiFi);
  server.begin();
  DBG_OK("WebServer ready.");

  DBG("Ready. IP: %s\n", WiFi.isConnected() ? WiFi.localIP().toString().c_str() : "0.0.0.0");
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

  // Toggle colon blink every second (for Tetris mode)
  if (now - lastColonToggle >= 1000) {
    clockColon = !clockColon;
    lastColonToggle = now;
  }

  // Update clock logic only on second change (once per second)
  // This is where we detect time changes
  bool timeChanged = updateClockLogic();

  // Determine if display needs update
  bool needsUpdate = false;

  if (cfg.clockMode == CLOCK_MODE_7SEG) {
    // 7-segment mode: update on time change or during morph animation
    needsUpdate = timeChanged || morphStep < MORPH_STEPS;
  } else if (cfg.clockMode == CLOCK_MODE_TETRIS) {
    // Tetris mode: update frequently during animation, or on time change
    needsUpdate = timeChanged || modeNeedsAnimation() || (now % 100 == 0);
  }

  // Also update during fade transitions
  if (inTransition || fadeLevel < 255) {
    needsUpdate = true;
  }

  // Render and display if needed
  if (needsUpdate) {
    renderCurrentMode();
    renderFBToTFT();
  }
}
