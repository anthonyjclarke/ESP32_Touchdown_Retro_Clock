# ESP32 Touchdown Retro Clock - Comprehensive Codebase Review

**Project:** ESP32 Touchdown RGB LED Matrix (HUB75) Retro Clock  
**Version:** 2.0.0  
**Date:** January 11, 2026  
**Reviewer:** Code Analysis  
**Target Migration:** CYD ESP32 Clock (320×240 display)

---

## 📋 Executive Summary

This is a fully-featured retro LED matrix clock emulator running on an ESP32 Touchdown with a 480×320 ILI9488 TFT display. The codebase is well-structured, modular, and feature-rich. Migration to a smaller 320×240 CYD display will require **display scaling adaptation**, **memory optimization**, and **UI restructuring** due to the 59% reduction in display area (from 153,600 to 76,800 pixels).

---

## 🏗️ Codebase Architecture

### Core Structure
```
src/main.cpp (2,249 lines)
├── Debug System (leveled logging)
├── Global State & Configuration
├── 7-Segment Display Logic
├── Tetris Clock Integration
├── Display Rendering Pipeline
├── Web Interface & API
├── Sensor Integration
├── WiFi & OTA
└── Main loop

include/
├── config.h (clock modes, defaults, hardware pins)
├── TetrisClock.h (Tetris animation wrapper)
├── timezones.h (88 global timezones)
└── User_Setup.h (TFT_eSPI display configuration)

data/ (Web UI - served from LittleFS)
├── index.html (217 lines)
├── app.js (JavaScript frontend)
└── style.css (styling)
```

### Key Dependencies
```ini
TFT_eSPI @ ^2.5.43           - Display driver (ILI9488)
WiFiManager @ ^2.0.16-rc.2   - WiFi setup & configuration
ArduinoJson @ ^7.0.4         - JSON parsing for API
TetrisAnimation              - GitHub library for Tetris animations
Adafruit Unified Sensor      - Sensor abstraction layer
Adafruit BME280/SHT31/HTU21D - Environmental sensors (one selected)
Adafruit GFX @ ^1.11.11      - Graphics library
```

---

## 🎯 Core Features & Implementation

### 1. **Virtual LED Matrix Framebuffer** (64×32 LED grid)
- **Location:** `src/main.cpp:238-239` (global)
- **Format:** RGB565 (uint16_t, 2 bytes per pixel)
- **Size:** 64W × 32H = 4,096 bytes (very memory efficient)
- **Purpose:** Emulates a physical HUB75 RGB LED Matrix Panel
- **Rendering:** Scaled to TFT display with configurable LED diameter/gap

```cpp
static uint16_t fb[LED_MATRIX_H][LED_MATRIX_W];           // RGB565 framebuffer
static uint16_t fbPrev[LED_MATRIX_H][LED_MATRIX_W];       // Previous frame (delta rendering)
```

**For CYD Migration:**
- LED matrix size is HARDWARE-DEFINED and should **remain 64×32**
- Scaling will change via LED diameter and pitch calculation
- Memory footprint is negligible (only 4KB for framebuffer)

---

### 2. **Clock Display Modes** (Two implemented)

#### A. **7-Segment Morphing Mode** (CLOCK_MODE_7SEG)
- **Implementation:** `src/main.cpp:312-520` (bitmap generation, morphing logic)
- **Features:**
  - Pre-rendered 7-segment digit bitmaps for digits 0-9
  - Smooth morphing animation between digit transitions
  - Two morphing strategies:
    - `drawMorph()`: Simple interpolation (lines 395-410)
    - `drawParticleMorph()`: Advanced particle matching (lines 437-517)
  - Colon blinking every 500ms (for AM/PM separator)
  - Configurable morph speed (1-50x multiplier, stored in config)

**Key Code:**
```cpp
static Bitmap DIGITS[10];  // Pre-rendered 7-seg bitmaps
static void drawFrame();   // Main 7-seg render (implementation in main.cpp)
static void drawMorph(...);
static void drawParticleMorph(...);
```

**For CYD Migration:**
- 7-segment rendering is resolution-independent (works on any size)
- Morphing algorithms are pure mathematics (no display dependencies)
- Colon blinking logic is timing-based, not resolution-based
- **No changes needed** - will scale automatically

---

#### B. **Tetris Mode** (CLOCK_MODE_TETRIS)
- **Implementation:** `include/TetrisClock.h` (60+ lines), uses external `TetrisAnimation` library
- **Features:**
  - Falling Tetris blocks form digit shapes
  - Multi-colored authentic Tetris blocks (RGB565 colors)
  - Configurable animation speed (default: 1800ms between frames)
  - Automatic animation state management
  - FramebufferGFX adapter bridges Adafruit_GFX interface to RGB565 framebuffer

**Key Code:**
```cpp
class FramebufferGFX : public Adafruit_GFX {
  void drawPixel(int16_t x, int16_t y, uint16_t color) override;
};

class TetrisClock {
  bool update(const String& timeStr, bool use24h, bool showColon, bool isPM);
  bool isAnimating();
  void reset();
};
```

**For CYD Migration:**
- Tetris animation is pure framebuffer rendering (resolution-independent)
- External `TetrisAnimation` library is dependency (must be included in CYD project)
- Speed control via `TETRIS_ANIMATION_SPEED` macro (lines 1848ms in config.h)
- **No changes needed** - will scale automatically

---

### 3. **Display Rendering Pipeline**

#### Display Scaling & Pitch Calculation
- **Location:** `src/main.cpp:535-575`
- **Concept:** Converts 64×32 logical LED matrix to TFT pixels
- **Pitch:** Multiplier that scales each LED matrix pixel

```cpp
// For 480×320 TFT with 64×32 matrix:
// fbPitch = min(480/64, 320/32) = min(7, 10) = 7
// Result: 64*7 = 448px wide, 32*7 = 224px tall (fits in 480×320 with status bar)
static int computeRenderPitch() {
  int maxPitch = min(tft.width() / LED_MATRIX_W, 
                     (tft.height() - STATUS_BAR_H) / LED_MATRIX_H);
  return maxPitch < 1 ? 1 : maxPitch;
}
```

**For CYD 320×240 Migration:**
```
fbPitch = min(320/64, (240-70)/32) = min(5, 5) = 5
Result: 64*5 = 320px wide, 32*5 = 160px tall (perfect fit for 320×240!)
Status bar: 70px (will need to be reduced from current 70px to ~40px)
```
**Impact:** CYD display will fit slightly tighter but MORE efficiently than Touchdown!

---

#### Sprite vs Direct Rendering
- **Location:** `src/main.cpp:551-575`
- **Sprite Mode:** TFT_eSprite (flicker-free, requires RAM)
- **Direct Mode:** Direct TFT drawing (fallback, may flicker)

```cpp
// Create sprite for smooth rendering (if RAM permits)
bool useSprite = false;
TFT_eSprite spr(&tft);

// Rebuild sprite when pitch changes
void rebuildSprite(int pitch) {
  int sprW = LED_MATRIX_W * pitch;  // 64 * 5 = 320px for CYD
  int sprH = LED_MATRIX_H * pitch;  // 32 * 5 = 160px for CYD
  spr.createSprite(sprW, sprH);     // ~102KB for CYD (manageable)
}
```

**For CYD Migration:**
- Sprite size: 320×160 × 2 bytes (RGB565) = ~102KB (very manageable)
- Current sprite on Touchdown: 448×224 × 2 bytes = ~200KB
- **CYD will actually use LESS memory** for sprite!

---

#### Rendering to TFT Display
- **Location:** `src/main.cpp:623-707`
- **Two Modes:**
  1. **Sprite Mode (default):** Push sprite to TFT at computed position
  2. **Direct Mode (fallback):** Draw pixels directly via TFT_eSPI

```cpp
void renderFBToTFT() {
  // Calculate LED appearance
  int dot = (cfg.ledDiameter * fbPitch) / 2;      // LED diameter in TFT pixels
  int gap = (cfg.ledGap * fbPitch) / 2;           // LED spacing in TFT pixels
  int pitch = dot + gap;

  if (useSprite) {
    // Sprite-based: faster, smooth
    for (y=0; y<LED_MATRIX_H; y++) {
      for (x=0; x<LED_MATRIX_W; x++) {
        uint16_t color = fb[y][x];
        if (color != 0) spr.drawCircle(...);
      }
    }
    spr.pushSprite(x0, y0);  // Single push to TFT
  } else {
    // Direct draw: pixel-by-pixel to TFT
    for (y=0; y<LED_MATRIX_H; y++) {
      for (x=0; x<LED_MATRIX_W; x++) {
        // Draw circle at (tftX, tftY)
        tft.drawCircle(tftX, tftY, dot/2, color);
      }
    }
  }
}
```

---

### 4. **Status Bar** (Bottom of display)
- **Location:** `src/main.cpp:577-620`
- **Height:** 70px (defined in config.h)
- **Content:**
  - Line 1: Temperature, humidity (if sensor available)
  - Line 2: Date, timezone

```cpp
#define STATUS_BAR_H 70  // Pixels reserved for status bar
static void drawStatusBar() {
  // Displays:
  // Line 1: "Temp: 22°C  Humidity: 45%"
  // Line 2: "2024/01/11  Sydney, Australia"
}
```

**For CYD Migration:**
- Status bar should be **reduced to ~40px** to maximize matrix display area
- Content can remain same (two lines)
- Font size may need adjustment

---

### 5. **Configuration Management**

#### Configuration Structure
- **Location:** `src/main.cpp:200-227`
- **Storage:** NVS (Non-Volatile Storage) via Arduino Preferences API
- **Persistence:** Automatic save after any change

```cpp
struct AppConfig {
  char tz[48];              // Timezone (e.g., "Sydney, Australia")
  char ntp[64];             // NTP server
  bool use24h;              // 24-hour vs 12-hour format
  uint8_t dateFormat;       // 5 formats: ISO, EU, US, DE, Verbose
  uint8_t ledDiameter;      // 1-10 pixels
  uint8_t ledGap;           // 0-8 pixels
  uint32_t ledColor;        // RGB888 (24-bit)
  uint8_t brightness;       // 0-255
  bool flipDisplay;         // Display rotation
  uint8_t morphSpeed;       // 1-50x (morph animation speed)
  uint8_t clockMode;        // 0=7-seg, 1=Tetris
  bool autoRotate;          // Cycle through modes
  uint8_t rotateInterval;   // Minutes between rotations
  bool useFahrenheit;       // Temperature unit
};
```

#### Configuration Load/Save
```cpp
void loadConfig()  // Lines 710-740: Load from NVS
void saveConfig()  // Lines 751-775: Save to NVS
void handlePostConfig() // Lines 1062-1415: Web API handler with change logging
```

**For CYD Migration:**
- All configuration keys are abstracted
- **No changes needed** - will use same NVS structure
- All settings will carry over automatically

---

### 6. **Sensor Integration**

#### Supported Sensors
- **BME280:** Temperature, Humidity, Pressure
- **SHT31/SHT3X:** Temperature, Humidity (no pressure)
- **HTU21D:** Temperature, Humidity (no pressure) - **DEFAULT**

```cpp
#define USE_HTU21D  // Selected in config.h

#ifdef USE_BME280
  Adafruit_BME280 bme280;
#endif
// ... similar for SHT3X and HTU21D

I2C Pins:
#define SENSOR_SDA_PIN 21  // Shared with touch controller
#define SENSOR_SCL_PIN 22
```

#### Sensor Update Cycle
- **Interval:** 60 seconds (SENSOR_UPDATE_INTERVAL)
- **Update Location:** `main loop() lines 2230-2232`
- **Display:** Status bar, status panel in web UI

```cpp
if (sensorAvailable && (now - lastSensorUpdate >= SENSOR_UPDATE_INTERVAL)) {
  updateSensorData();
  lastSensorUpdate = now;
}
```

**For CYD Migration:**
- **Check CYD hardware** for sensor compatibility
- I2C pins may differ - update `SENSOR_SDA_PIN` and `SENSOR_SCL_PIN`
- All sensor abstraction is modular and easily adapted
- Temperature/humidity display logic is independent

---

### 7. **Web Interface & API**

#### Static Files (LittleFS)
- **Location:** `/data/` folder
- **Served via:** TFT_eSPI LittleFS integration
- **Files:**
  - `index.html` (217 lines) - UI layout
  - `app.js` - Frontend logic
  - `style.css` - Styling

#### Web API Endpoints

| Endpoint | Method | Purpose |
|----------|--------|---------|
| `/` | GET | Serve index.html |
| `/api/state` | GET | Current system state (JSON) |
| `/api/config` | POST | Update configuration |
| `/api/mirror` | GET | Raw framebuffer data (RGB565 binary) |
| `/api/timezones` | GET | List 88 timezones |
| `/api/reset-wifi` | POST | Clear WiFi credentials |
| `/app.js`, `/style.css` | GET | Static assets |

```cpp
// Example: GET /api/state returns JSON with:
{
  "time": "14:35:42",
  "date": "2024-01-11",
  "wifi": "connected",
  "ip": "192.168.1.100",
  "temp": 22,
  "humidity": 45,
  "sensorType": "HTU21D",
  "uptime": 86400,
  "freeHeap": 123456,
  "heapUsage": 42.5,
  "cpuFreq": 240,
  ...
}
```

**For CYD Migration:**
- All web API code is **display-independent**
- Web UI will need **CSS layout adjustments** for smaller screens
- Mirror display will scale automatically
- **No backend changes needed**

---

### 8. **WiFi & Network**

#### WiFiManager Integration
- **Location:** `src/main.cpp:1530-1560`
- **Features:**
  - Automatic connection to known networks
  - Captive portal for initial setup
  - AP mode fallback if connection fails
  - Config portal timeout: 180s
  - Connection timeout: 20s

```cpp
static void startWifi() {
  WiFiManager wm;
  wm.setConfigPortalTimeout(180);
  wm.setConnectTimeout(20);
  bool ok = wm.autoConnect("Touchdown-RetroClock-Setup");
  // Fallback to AP mode if failed
}
```

#### AP Credentials
- **SSID:** `Touchdown-RetroClock-Setup` (initial setup)
- **SSID:** `Touchdown-RetroClock-AP` (fallback)

**For CYD Migration:**
- Update SSID names for CYD in config
- Otherwise **no changes needed**

---

### 9. **NTP Time Synchronization**

#### Timezone Support
- **Location:** `include/timezones.h` (88 global timezones)
- **Format:** POSIX TZ strings with automatic DST handling
- **Coverage:** 13 geographic regions
  - Australia/Oceania (12 timezones)
  - North America (11)
  - South America (6)
  - Western Europe (12)
  - Northern Europe (4)
  - Central/Eastern Europe (8)
  - Middle East (5)
  - South Asia (7)
  - Southeast Asia (7)
  - East Asia (6)
  - Central Asia (3)
  - Caucasus (3)
  - Africa (4)

```cpp
const TimezoneInfo timezones[] = {
  {"Sydney, Australia", "AEST-10AEDT,M10.1.0,M4.1.0/3"},  // INDEX 0 - DEFAULT
  {"Tokyo, Japan", "JST-9"},
  ... // 87 more
};
```

#### NTP Update
- **Location:** `src/main.cpp` (startNtp() function)
- **Default Server:** `pool.ntp.org`
- **Preset Options:** 9 global/regional NTP servers

**For CYD Migration:**
- Timezone data is **self-contained** in timezones.h
- **No changes needed** - use as-is

---

### 10. **OTA Firmware Updates**

#### OTA Configuration
- **Location:** `src/main.cpp:1565-1630`
- **Hostname:** `Touchdown-RetroClock`
- **Password:** `change-me` (SHOULD BE CHANGED)
- **Update Method:** Arduino OTA protocol

```cpp
#define OTA_HOSTNAME "Touchdown-RetroClock"
#define OTA_PASSWORD "change-me"

void startOta() {
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);
  
  // Progress display on TFT during update
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    drawOTAProgress((progress / (total / 100)));
  });
}
```

#### OTA Progress Display
- **Location:** `src/main.cpp:1565-1630`
- **Visual:** Progress bar on TFT with color gradient (red → yellow → green)

**For CYD Migration:**
- Update hostname to `CYD-RetroClock`
- Change password for security
- Progress bar will scale to 320×240 display automatically

---

### 11. **Mode Rotation (Auto-Rotate)**

#### Auto-Mode Switching
- **Location:** `src/main.cpp:1750-1770`
- **Features:**
  - Cycle through clock modes (7-seg → Tetris → 7-seg...)
  - Configurable interval (1-60 minutes)
  - Fade transition between modes
  - Disabled by default

```cpp
static void checkAutoRotation() {
  if (!cfg.autoRotate) return;
  
  unsigned long interval = (unsigned long)cfg.rotateInterval * 60000UL;
  if (now - lastModeRotation >= interval) {
    uint8_t nextMode = (cfg.clockMode + 1) % TOTAL_CLOCK_MODES;
    switchClockMode(nextMode);
    lastModeRotation = now;
  }
}
```

**For CYD Migration:**
- Mode rotation logic is **display-independent**
- **No changes needed**

---

### 12. **Debug System**

#### Leveled Debug Logging
- **Location:** `src/main.cpp:117-167`
- **Levels:** 0=Off, 1=Error, 2=Warning, 3=Info, 4=Verbose
- **Runtime Control:** Web UI allows level adjustment without restart
- **Prefix Tags:** [ERR], [WARN], [INFO], [VERB]

```cpp
#define DEBUG_LEVEL 3  // Default: Info

static uint8_t debugLevel = DEBUG_LEVEL;  // Runtime-adjustable

#define DBG_ERROR(...)   do { if (debugLevel >= DBG_LEVEL_ERROR)   { Serial.print("[ERR ] "); ... } } while(0)
#define DBG_WARN(...)    do { if (debugLevel >= DBG_LEVEL_WARN)    { Serial.print("[WARN] "); ... } } while(0)
#define DBG_INFO(...)    do { if (debugLevel >= DBG_LEVEL_INFO)    { Serial.print("[INFO] "); ... } } while(0)
#define DBG_VERBOSE(...) do { if (debugLevel >= DBG_LEVEL_VERBOSE) { Serial.print("[VERB] "); ... } } while(0)
```

**For CYD Migration:**
- Debug system is **hardware-independent**
- **No changes needed**

---

## 🔧 Hardware-Specific Configuration

### TFT Display Configuration (User_Setup.h)
- **Display:** ILI9488 480×320 (landscape)
- **Pins:**
  - MOSI: GPIO23
  - SCLK: GPIO18
  - CS: GPIO15
  - DC: GPIO2
  - RST: GPIO4
  - BL: GPIO32 (PWM backlight)
- **SPI Speed:** 40MHz (read: 20MHz)
- **DMA:** Enabled for performance
- **Fonts:** All TFT_eSPI fonts loaded

```cpp
#define ILI9488_DRIVER
#define TFT_MOSI  23
#define TFT_SCLK  18
#define TFT_CS    15
#define TFT_DC     2
#define TFT_RST    4
#define TFT_BL    32
```

**For CYD Migration:**
- **CRITICAL:** Update User_Setup.h with CYD pin configuration
- CYD uses different GPIO pins
- May use ST7789 or similar driver (NOT ILI9488)
- Backlight pin may differ
- Update display rotation if needed

---

## 📊 Main Loop & Timing

### Loop Execution Order
- **Location:** `src/main.cpp:2220-2249`
- **Frame Rate:** ~20 FPS (FRAME_MS = 50ms)
- **Morph Steps:** 20 frames per digit transition
- **Tetris Update:** 1800ms between animation frames (configurable)

```cpp
void loop() {
  // 1. Handle OTA updates
  ArduinoOTA.handle();
  
  // 2. Process web requests
  server.handleClient();
  
  // 3. Update sensor every 60s
  if (sensorAvailable && (now - lastSensorUpdate >= SENSOR_UPDATE_INTERVAL)) {
    updateSensorData();
  }
  
  // 4. Check auto-rotation timer
  checkAutoRotation();
  
  // 5. Toggle colon every 1000ms (for Tetris)
  if (now - lastColonToggle >= 1000) {
    clockColon = !clockColon;
  }
  
  // 6. Update clock time (once per second)
  bool timeChanged = updateClockLogic();
  
  // 7. Determine if display needs update
  bool needsUpdate = false;
  if (cfg.clockMode == CLOCK_MODE_7SEG) {
    needsUpdate = timeChanged || morphStep < MORPH_STEPS;
  } else if (cfg.clockMode == CLOCK_MODE_TETRIS) {
    needsUpdate = timeChanged || modeNeedsAnimation();
    if (now - lastTetrisUpdate >= TETRIS_ANIMATION_SPEED) {
      needsUpdate = true;
      lastTetrisUpdate = now;
    }
  }
  
  // 8. Render and display if needed
  if (needsUpdate) {
    renderCurrentMode();
    renderFBToTFT();
  }
}
```

**For CYD Migration:**
- Loop timing is **display-independent**
- **No changes needed** to main loop logic
- Rendering will be faster on smaller display (less pixels to update)

---

## 💾 Memory Usage Analysis

### Stack & Heap
- **Global Framebuffer:** 8,192 bytes (64×32×2 for RGB565 + backup)
- **Sprite Buffer:** ~200KB on Touchdown (480×224×2), ~102KB on CYD (320×160×2)
- **Configuration:** ~200 bytes (struct + strings)
- **Display Buffer Cache:** Minimal (computed on-the-fly)

**CYD Impact:**
- Sprite buffer reduction: 200KB → 102KB (**48% reduction**)
- Framebuffer sizes unchanged
- **Overall memory footprint will DECREASE on CYD**

---

## 🔀 Display Rotation & Flipping

### Display Rotation Modes
- **Location:** `src/main.cpp:781-788`
- **Rotation 1 (Normal):** USB port left, I/O ports top
- **Rotation 3 (Flipped):** 180° rotation (USB port right)

```cpp
static void applyDisplayRotation() {
  uint8_t rotation = cfg.flipDisplay ? 3 : 1;
  tft.setRotation(rotation);
}
```

**For CYD Migration:**
- May need to adjust rotation constants
- Test on actual hardware for correct orientation

---

## 📝 Date & Time Formatting

### Date Format Options (5 formats)
- **0:** YYYY-MM-DD (ISO)
- **1:** DD/MM/YYYY (European)
- **2:** MM/DD/YYYY (US)
- **3:** DD.MM.YYYY (German)
- **4:** Mon DD, YYYY (Verbose)

```cpp
// Format date based on cfg.dateFormat setting
const char* formats[] = {"YYYY-MM-DD", "DD/MM/YYYY", "MM/DD/YYYY", "DD.MM.YYYY", "Mon DD YYYY"};
```

---

## 🎨 LED Appearance Customization

### LED Rendering Parameters
- **Diameter:** 1-10 pixels (visual size of LED dot)
- **Gap:** 0-8 pixels (spacing between LEDs)
- **Pitch:** Computed from diameter + gap
- **Color:** 24-bit RGB (0xRRGGBB) → converted to RGB565
- **Brightness:** 0-255 (backlight PWM)

```cpp
void renderFBToTFT() {
  int dot = (cfg.ledDiameter * fbPitch) / 2;  // LED size in TFT pixels
  int gap = (cfg.ledGap * fbPitch) / 2;       // Space between LEDs
  int pitch = dot + gap;                      // Total pitch
  
  // Draw circles for each LED
  tft.drawCircle(tftX, tftY, dot/2, color);
}
```

---

## 🚀 Startup Sequence

### Boot Display System
- **Location:** `src/main.cpp:1683-1740` (initialization)
- **Display Steps:**
  - TFT initialization
  - Title and firmware version
  - Step-by-step startup messages with status colors
  - Auto-wrap if messages exceed screen height

```cpp
void setup() {
  // Initialization order:
  1. Serial (115200 baud)
  2. TFT display + startup sequence
  3. Digit bitmaps
  4. Configuration load
  5. LittleFS mount
  6. Display rotation
  7. Sprite creation
  8. Tetris clock init
  9. WiFi connection
 10. Sensor detection
 11. NTP sync
 12. OTA server
 13. Web server
}
```

---

## 📱 Web UI Features

### Frontend Components
- **Live Display Mirror:** Real-time framebuffer visualization
- **System Diagnostics Panel:**
  - Time & Network (time, date, WiFi, IP)
  - Environment (temperature, humidity, sensor type)
  - Hardware (board, display, firmware version, OTA status)
  - System Resources (uptime, free heap, heap %, CPU freq)
  - Debug Settings (runtime-adjustable level)
- **Configuration Controls:** All settings with instant apply
- **Display Mirror:** 480×320 canvas showing 64×32 LED matrix (upscaled)

### JavaScript Frontend (app.js)
- Fetch configuration state via `/api/state`
- Update configuration via POST to `/api/config`
- Real-time framebuffer mirror via `/api/mirror`
- Timezone + NTP dropdown population
- Change logging (before/after values)

---

## 🔌 Hardware Pinout (Touchdown)

| Function | GPIO | Purpose |
|----------|------|---------|
| TFT MOSI | 23 | Display SPI data |
| TFT SCLK | 18 | Display SPI clock |
| TFT CS | 15 | Display chip select |
| TFT DC | 2 | Display data/command |
| TFT RST | 4 | Display reset |
| TFT BL | 32 | Backlight PWM |
| Touch SDA | 21 | I2C data (shared with sensor) |
| Touch SCL | 22 | I2C clock (shared with sensor) |
| Touch IRQ | 27 | Touch interrupt (unused) |
| Sensor SDA | 21 | I2C data (shared with touch) |
| Sensor SCL | 22 | I2C clock (shared with touch) |
| Buzzer | 26 | Passive buzzer (unused) |
| Battery | 35 | Battery voltage ADC |

**For CYD Migration:**
- Identify CYD GPIO mapping
- Update User_Setup.h and config.h with new pins
- Verify I2C addresses (especially for touch and sensors)

---

## 🐛 Known Limitations & Issues

1. **Touch Support:** Touch controller present but not utilized in UI
2. **Buzzer:** GPIO26 buzzer defined but not implemented
3. **Battery Monitoring:** ADC input defined but not displayed
4. **Status LEDs:** Commented out (no RGB LED on Touchdown)
5. **Display Flashing:** Sprite mode required for smooth rendering (requires RAM)
6. **FT62x6 Touch:** I2C address 0x38 - verify on CYD

---

## 📋 Porting Checklist for CYD ESP32 Clock (320×240)

### Phase 1: Hardware Configuration
- [ ] Verify CYD display driver (likely ST7789, not ILI9488)
- [ ] Update User_Setup.h with CYD GPIO pins
- [ ] Update SPI frequency if needed
- [ ] Verify I2C pins for sensor
- [ ] Test display initialization
- [ ] Calibrate display rotation

### Phase 2: Display Scaling
- [ ] LED matrix remains 64×32 (logical size)
- [ ] Update STATUS_BAR_H from 70 to ~40 pixels
- [ ] Test fbPitch calculation: should be 5 for CYD
- [ ] Verify sprite buffer size: ~102KB (acceptable)
- [ ] Test LED rendering at new scale

### Phase 3: UI Adaptation
- [ ] Resize web UI for mobile/smaller screens
- [ ] Adjust CSS for narrower layout
- [ ] Test display mirror on web interface
- [ ] Adjust font sizes in status bar
- [ ] Test startup display on smaller TFT

### Phase 4: Sensor & Features
- [ ] Verify sensor I2C address on CYD
- [ ] Test sensor data collection
- [ ] Verify temperature/humidity display
- [ ] Test WiFi connection
- [ ] Test OTA update process

### Phase 5: Testing & Optimization
- [ ] Verify 7-segment morphing at new scale
- [ ] Test Tetris animation speed
- [ ] Monitor heap usage and memory leaks
- [ ] Test all web API endpoints
- [ ] Stress test with rapid config changes
- [ ] Verify OTA updates work

### Phase 6: Documentation
- [ ] Update firmware version in config.h
- [ ] Document CYD pinout changes
- [ ] Create CYD-specific README
- [ ] Update PlatformIO configuration for CYD board
- [ ] Document any workarounds or modifications

---

## 💡 Recommendations for CYD Migration

### Keep As-Is
1. **Framebuffer architecture** - RGB565 format is ideal
2. **7-segment rendering** - pure math, resolution-independent
3. **Tetris animation** - will work perfectly on smaller display
4. **Web API** - no display dependencies
5. **Configuration system** - abstracted and portable
6. **WiFi/NTP integration** - standard Arduino libraries
7. **Sensor integration** - hardware-abstracted via Adafruit libraries

### Adapt for CYD
1. **User_Setup.h** - new display driver, new GPIO pins
2. **statusbar height** - reduce from 70 to 40px for better display ratio
3. **Sprite rendering** - verify RAM is sufficient (should be fine)
4. **Touch support** - CYD may use different touch controller
5. **Power/battery** - CYD may have different battery monitoring
6. **Web UI layout** - responsive design for mobile

### Consider Adding
1. **Touch panel UI** - for direct interaction without web browser
2. **More clock modes** - Analog, Binary, Word clock (logical layer is ready)
3. **Color presets** - theme system for LED colors
4. **Per-LED customization** - different colors for different segments
5. **MQTT integration** - for home automation
6. **Home Assistant integration** - for smart home systems

---

## 📊 Code Metrics

| Metric | Value |
|--------|-------|
| Total Lines (main.cpp) | 2,249 |
| Header Files | 3 (config.h, TetrisClock.h, timezones.h) |
| Web Files | 3 (index.html, app.js, style.css) |
| Global Variables | ~30 |
| Functions | ~25+ |
| Supported Timezones | 88 |
| API Endpoints | 6 |
| Display Modes | 2 (expandable) |
| Configuration Options | 15+ |
| Debug Levels | 5 |

---

## 📚 External Dependencies & Licenses

| Library | Version | Purpose | License |
|---------|---------|---------|---------|
| TFT_eSPI | ^2.5.43 | Display driver | MIT |
| WiFiManager | ^2.0.16 | WiFi setup | MIT |
| ArduinoJson | ^7.0.4 | JSON parsing | MIT |
| TetrisAnimation | git | Tetris blocks | (check repo) |
| Adafruit Unified Sensor | ^1.1.14 | Sensor API | Apache 2.0 |
| Adafruit GFX | ^1.11.11 | Graphics | BSD |
| Adafruit BME280/SHT31/HTU21D | ^2.2.4 | Environmental sensors | BSD |

---

## 🎯 Summary

The **ESP32 Touchdown Retro Clock** is a well-architected, feature-complete project that is **highly portable** to the CYD ESP32 clock. The majority of the code is **display-resolution-independent** and uses **abstracted hardware interfaces**. 

### Key Porting Advantages:
1. **Smaller display = less memory needed** (102KB sprite vs 200KB)
2. **Perfect pixel fit** (fbPitch = 5 for CYD vs 7 for Touchdown)
3. **All rendering logic transfers directly** (7-seg and Tetris)
4. **Configuration persists** across platforms
5. **Web UI is responsive** (needs CSS adjustments only)
6. **No display-dependent business logic** (rendering is isolated)

### Estimated Effort:
- **Hardware setup:** 2-4 hours (GPIO configuration, testing)
- **UI adaptation:** 2-3 hours (CSS, status bar sizing)
- **Testing & debugging:** 4-6 hours (full feature verification)
- **Total:** 8-13 hours for a complete port

### Success Likelihood: **Very High** ✓
The codebase is clean, modular, and specifically engineered for TFT displays. The smaller CYD display will actually work *more efficiently* in terms of memory.

---

**End of Review** | Generated: January 11, 2026
