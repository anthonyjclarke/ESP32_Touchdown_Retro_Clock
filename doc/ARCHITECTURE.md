# Architectural Overview & Integration Guide

**Project:** ESP32 Touchdown Retro Clock v2.0.0  
**Purpose:** Quick reference for understanding code organization and data flow  
**Audience:** Developers integrating into CYD project

---

## 🏗️ System Architecture Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                      MAIN APPLICATION LOOP                      │
│                     (src/main.cpp:2220-2249)                    │
└───────────────────────────┬─────────────────────────────────────┘
                            │
        ┌───────────────────┼───────────────────┐
        │                   │                   │
        ▼                   ▼                   ▼
┌──────────────────┐  ┌──────────────┐  ┌─────────────┐
│  OTA Handler     │  │ Web Server   │  │   Sensor    │
│ ArduinoOTA       │  │  Requests    │  │  Updates    │
│ (every loop)     │  │ (every loop) │  │ (60s timer) │
└──────────────────┘  └──────────────┘  └─────────────┘
        │                   │                   │
        └───────────────────┼───────────────────┘
                            │
                ┌───────────┴────────────┐
                │                        │
                ▼                        ▼
        ┌──────────────────┐   ┌────────────────┐
        │  Update Display  │   │ Check Rotation │
        │  (if needed)     │   │ (auto-rotate)  │
        └──────────────────┘   └────────────────┘
                │
                ▼
    ┌───────────────────────────┐
    │  Update Clock Time Logic  │ ◄─── updateClockLogic()
    │  (detects second change)  │      checks if 1 sec passed
    └───────┬───────────────────┘
            │
            ├──────────────────────────────────────┐
            │                                      │
            ▼                                      ▼
    ┌───────────────────┐            ┌──────────────────────┐
    │ MODE = 7-SEGMENT  │            │  MODE = TETRIS       │
    │                   │            │                      │
    │ renderCurrentMode │            │ renderCurrentMode    │
    │  └─drawFrame()    │            │  └─drawFrameTetris() │
    │    • 7-seg bitmaps│            │    • TetrisClock     │
    │    • morphing     │            │    • falling blocks  │
    │    • particles    │            │    • animation frame │
    │                   │            │                      │
    └─────────┬─────────┘            └──────────┬───────────┘
              │                                  │
              └──────────────────┬───────────────┘
                                 │
                    ┌────────────┴────────────┐
                    │                         │
                    ▼                         ▼
          ┌──────────────────┐      ┌──────────────────┐
          │ Apply Fade       │      │ Render to TFT    │
          │ (if transition)  │      │ renderFBToTFT()  │
          └──────────────────┘      │                  │
                    │               │ • Calc pitch     │
                    │               │ • Draw LEDs      │
                    │               │ • Push sprite OR │
                    │               │   direct draw    │
                    │               │                  │
                    │               └──────────────────┘
                    │                     │
                    └─────────┬───────────┘
                              │
                              ▼
                    ┌──────────────────────┐
                    │   Update Status Bar  │
                    │   (if time to draw)  │
                    │                      │
                    │ • Temperature/Humid  │
                    │ • Date/Timezone      │
                    └──────────────────────┘
```

---

## 📊 Data Flow: Framebuffer Architecture

```
┌──────────────────────────────────────────────────────────────┐
│              VIRTUAL 64×32 RGB565 FRAMEBUFFER                │
│  (4,096 bytes = 64 columns × 32 rows × 2 bytes per pixel)   │
│                                                               │
│  fb[32][64]        ← Current frame data                      │
│  fbPrev[32][64]    ← Previous frame (for delta rendering)    │
└────────┬───────────────────────────────────────────────────┬─┘
         │                                                     │
         │  Input: Clock rendering (7-seg or Tetris)          │
         │  Output: RGB565 pixels (0xRRGGBBB format)           │
         │                                                     │
    ┌────▼─────────────────────────────────────────────────┐  │
    │         DISPLAY SCALING & PITCH CALCULATION          │  │
    │                                                        │  │
    │  fbPitch = min(tft.width/64, avail_height/32)        │  │
    │                                                        │  │
    │  Touchdown: min(480/64, 250/32) = min(7, 7.8) = 7   │  │
    │  CYD:       min(320/64, 200/32) = min(5, 6.25) = 5  │  │
    │                                                        │  │
    │  Result: Each LED pixel becomes fbPitch×fbPitch TFT  │  │
    │  pixels (e.g., 7×7 on Touchdown, 5×5 on CYD)         │  │
    └────┬────────────────────────────────────────────────┘  │
         │                                                     │
         ▼                                                     ▼
┌──────────────────────┐                        ┌───────────────────┐
│  SPRITE RENDERING    │                        │  DIRECT RENDERING │
│  (preferred, smooth) │                        │  (fallback)       │
│                      │                        │                   │
│ TFT_eSprite spr      │                        │ tft.drawCircle() │
│ ├─ spr.fillSprite()  │                        │ ... pixel by      │
│ ├─ spr.drawCircle()  │                        │ pixel to TFT      │
│ │  (each LED)        │                        │                   │
│ └─ spr.pushSprite()  │                        │ (may flicker)     │
│    (single update)   │                        │                   │
└──────────┬───────────┘                        └──────────┬────────┘
           │                                                │
           └────────────────────┬─────────────────────────┘
                                │
                                ▼
                    ┌──────────────────────────┐
                    │   TFT Display Output     │
                    │   (480×320 or 320×240)  │
                    └──────────────────────────┘
```

---

## 🔄 Configuration & Persistence Flow

```
┌─────────────────────────────────────────────────────┐
│           NVS (Non-Volatile Storage)               │
│          ESP32 Flash Memory Storage                │
│                                                     │
│  Keys: "tz", "ntp", "24h", "ledd", "ledg",        │
│        "col", "bl", "flip", "morph", "dbglvl"    │
│        "clockMode", "autoRotate", "rotateInt"     │
└──────────┬──────────────────────────────────────┬─┘
           │                                        │
    ┌──────▼──────┐                        ┌───────▼─────┐
    │ loadConfig()│ (on boot)              │ saveConfig()│
    │ (startup)   │                        │ (on change) │
    │             │                        │             │
    │ load ──────▶│                        │◀────── save │
    └──────┬──────┘                        └───────┬─────┘
           │                                       ▲
           │ initialize                            │ on config change
           │ AppConfig struct                      │
           │                                       │
           ▼                                       │
    ┌──────────────────────┐                      │
    │ struct AppConfig cfg │                      │
    │ {                    │                      │
    │   tz[48]             │────────────┐          │
    │   ntp[64]            │            │          │
    │   use24h             │            │          │
    │   dateFormat         │            │    handlePostConfig()
    │   ledDiameter        │            │    (from web API)
    │   ledGap             │            │    ◄─ logs changes
    │   ledColor           │            │    ◄─ validates
    │   brightness         │            │    ◄─ applies
    │   flipDisplay        │            │
    │   morphSpeed         │            │
    │   clockMode          │            │
    │   autoRotate         │            │
    │   rotateInterval     │            │
    │   useFahrenheit      │            │
    │ }                    │────────────┘
    └──────────────────────┘
           │
           │ runtime use
           │
    ┌──────┴────────────────────────────────┐
    │                                        │
    ▼                                        ▼
Display rendering decision          Web API state/mirror
(which mode, colors, size)          (JSON responses, images)
```

---

## 🕐 Time Update & Clock Logic

```
┌────────────────────────────────────────┐
│    MAIN LOOP: updateClockLogic()       │
│    (checks every loop, updates 1x/sec) │
└───────────────┬────────────────────────┘
                │
                ▼
        ┌──────────────────┐
        │  Get current     │
        │  system time     │
        │  (localtime())   │
        └────────┬─────────┘
                 │
                 ▼
        ┌──────────────────┐
        │  Compare to      │
        │  last second     │
        │  detected        │
        └────────┬─────────┘
                 │
         ┌───────┴────────┐
         │                │
    NO   │            YES │
    (skip)            (continue)
         │                │
         │                ▼
         │        ┌──────────────────┐
         │        │  Reset morph     │
         │        │  animation step  │
         │        │  to 0            │
         │        └────────┬─────────┘
         │                 │
         │                 ▼
         │        ┌──────────────────┐
         │        │ Format time      │
         │        │ string (HH:MM)   │
         │        │ or (H:MM)        │
         │        │ 12h/24h format   │
         │        └────────┬─────────┘
         │                 │
         │        ┌────────▼──────────┐
         │        │                   │
         │        ▼                   ▼
         │   7-SEGMENT            TETRIS
         │   drawFrame()          drawFrameTetris()
         │   • Compare time       • tetrisClock->
         │   • Get old/new        update(timeStr,
         │     digit bitmaps        use24h,
         │   • Start morph        clockColon,
         │     animation          isPM)
         │     (0→MORPH_STEPS)   • Returns animation
         │   • Renders to fb      state
         │                        • Updates block
         │                          positions
         │                        • Renders to fb
         │                        
         └────────┬───────────────┬───┘
                  │               │
                  └───────┬───────┘
                          │
                          ▼
                 ┌──────────────────┐
                 │  Format date &   │
                 │  store in curr   │
                 │  Date (status    │
                 │  bar display)    │
                 └──────────────────┘
                         │
                         ▼
                 ┌──────────────────┐
                 │  RETURN          │
                 │  timeChanged:    │
                 │  TRUE            │
                 │  (signal render) │
                 └──────────────────┘
```

---

## 🌐 Web Interface Data Exchange

```
┌─────────────────────────────────────────────┐
│         BROWSER / JAVASCRIPT FRONTEND       │
│         (data/app.js)                       │
└──────────────┬──────────────────────────────┘
               │
        ┌──────┴──────┬──────────┬──────────┐
        │             │          │          │
        ▼             ▼          ▼          ▼
   GET /     GET /     POST /    GET /
   (HTML)  /api/      /api/      /api/
            state    config      mirror
            (JSON)   (JSON)      (binary)
            │         │          │
            │         │          ▼
            │         │      Raw RGB565
            │         │      framebuffer
            │         │      4096 bytes
            │         │      
            │         ▼
            │     handlePostConfig()
            │     ┌─────────────────┐
            │     │ Parse JSON      │
            │     │ Extract fields  │
            │     │ Validate        │
            │     │ Compare old→new │
            │     │ Log changes     │
            │     │ Apply to cfg    │
            │     │ Save to NVS     │
            │     │ Rebuild if      │
            │     │  needed         │
            │     │ Return 200 OK   │
            │     └─────────────────┘
            │
            ▼
      handleGetState()
      ┌──────────────────────┐
      │ Collect system data  │
      │ • Current time       │
      │ • WiFi status        │
      │ • IP address         │
      │ • Temp/humidity      │
      │ • Sensor type        │
      │ • Uptime             │
      │ • Free heap          │
      │ • CPU frequency      │
      │ • Debug level        │
      │ Build JSON response  │
      │ Return with CORS     │
      └──────────────────────┘
            │
            ▼
      Browser displays:
      • Status panel
      • Config values
      • System diagnostics
      • Display mirror
```

---

## 🎯 Mode Rendering Architecture

```
┌──────────────────────────────────────────────────┐
│        renderCurrentMode()                       │
│   (dispatcher selecting mode renderer)           │
└─────────────────┬────────────────────────────────┘
                  │
          ┌───────┴────────────┐
          │                    │
          ▼                    ▼
    ┌──────────────────┐  ┌─────────────────┐
    │ clockMode == 0   │  │ clockMode == 1  │
    │ (7-SEGMENT)      │  │ (TETRIS)        │
    │                  │  │                 │
    │ drawFrame()      │  │ drawFrameTetris()
    └────────┬─────────┘  └────────┬────────┘
             │                     │
             ▼                     ▼
    ┌──────────────────┐  ┌─────────────────┐
    │ 7-SEG RENDERING  │  │ TETRIS RENDERING
    │                  │  │                 │
    │ 1. Get time      │  │ 1. Get time     │
    │ 2. Break digits  │  │ 2. Call         │
    │ 3. Compare with  │  │    tetrisClock  │
    │    previous      │  │    ->update()   │
    │ 4. Changed digit?│  │ 3. Tetris lib   │
    │    Start morph   │  │    handles:     │
    │ 5. Get bitmaps   │  │    • Block drop │
    │    for digits    │  │    • Placement  │
    │ 6. Interpolate   │  │    • Rotation   │
    │    morph frames  │  │ 4. Draws to fb  │
    │ 7. Render pixels │  │    via          │
    │    to fb[y][x]   │  │    FramebufferGFX
    │                  │  │                 │
    │ Morph animation: │  │ Animation:      │
    │ • 20 steps       │  │ • Continuous    │
    │ • 1s per digit   │  │ • Speed: 1800ms │
    │ • Particle       │  │   (config)      │
    │   tracking       │  │ • Uses RGB565   │
    │                  │  │   colors        │
    └────────┬─────────┘  └────────┬────────┘
             │                     │
             └──────────┬──────────┘
                        │
                        ▼
            ┌──────────────────────┐
            │  Apply Fade Effect   │
            │  (if transitioning)  │
            │                      │
            │ • Scale brightness  │
            │ • Multiplier 0-255  │
            │ • For mode switch   │
            │   transitions       │
            └──────────┬───────────┘
                       │
                       ▼
            ┌──────────────────────┐
            │  Status Bar Rendering│
            │  drawStatusBar()     │
            │                      │
            │ • Temperature/humid  │
            │ • Date/timezone      │
            │ • Only if changed    │
            │ • (optimize TFT      │
            │   bandwidth)         │
            └──────────────────────┘
```

---

## 📱 Sensor Integration Flow

```
┌───────────────────────────────┐
│    Loop: 60s timer check      │
│    (SENSOR_UPDATE_INTERVAL)   │
└─────────────┬─────────────────┘
              │
              ▼
    ┌──────────────────────┐
    │ sensorAvailable =    │
    │ testSensor()         │ (setup only)
    │                      │
    │ Probe:               │
    │ • BME280 @ 0x77/0x76│
    │ • SHT31 @ 0x44/0x45 │
    │ • HTU21D @ 0x40      │
    │                      │
    │ Return: true/false   │
    │ Set sensorType       │
    └──────────┬───────────┘
               │
         ┌─────┴────────┐
         │              │
    NOT  │         YES  │
  FOUND  │         FOUND
         │              │
         ▼              ▼
    Display         ┌──────────────────┐
    "No Sensor"     │ updateSensorData()
                    │                  │
                    │ Read sensor:     │
                    │ • Temperature    │
                    │ • Humidity       │
                    │ • Pressure (opt) │
                    │                  │
                    │ Store in globals:
                    │ • temperature    │
                    │ • humidity       │
                    │ • pressure       │
                    │                  │
                    │ Display in:      │
                    │ • Status bar     │
                    │ • Web UI         │
                    │                  │
                    │ Update flags:    │
                    │ • lastSensorUpd  │
                    │ • redraw needed  │
                    └──────────────────┘
                            │
                            ▼
                    Wait for next 60s
```

---

## 🔐 Hardware Abstraction Layers

```
┌─────────────────────────────────────────────────────┐
│             APPLICATION LOGIC LAYER                 │
│  (display-independent clock, config, web API)      │
├─────────────────────────────────────────────────────┤
│                                                     │
│   ┌──────────────┐         ┌────────────────┐    │
│   │ Clock Logic  │         │ Config Mgmt    │    │
│   │ • Mode sel   │         │ • NVS store    │    │
│   │ • Time fmt   │         │ • JSON web API │    │
│   │ • Animation  │         │ • Validation   │    │
│   └──────────────┘         └────────────────┘    │
│                                                     │
├─────────────────────────────────────────────────────┤
│             HARDWARE ABSTRACTION LAYER             │
│  (abstracted via TFT_eSPI, Adafruit, Arduino)    │
├─────────────────────────────────────────────────────┤
│                                                     │
│   ┌──────────────┐  ┌──────────────┐  ┌────────┐ │
│   │  TFT Display │  │   Sensor I2C │  │ WiFi  │ │
│   │ (SPI 40MHz)  │  │  (I2C 100k)  │  │ (OTA) │ │
│   │              │  │              │  │       │ │
│   │ • TFT_eSPI   │  │ • Adafruit   │  │ • Ard │ │
│   │ • Sprite buf │  │   Unified    │  │   OTA │ │
│   │ • setRotat   │  │ • BME280     │  │       │ │
│   │ • drawCircle │  │ • SHT31      │  │       │ │
│   │ • fillRect   │  │ • HTU21D     │  │       │ │
│   └──────────────┘  └──────────────┘  └────────┘ │
│                                                     │
├─────────────────────────────────────────────────────┤
│                 HARDWARE LAYER                     │
│  (board-specific GPIO, SPI, I2C, WiFi radios)    │
├─────────────────────────────────────────────────────┤
│                                                     │
│   ┌────────────┐  ┌────────────┐  ┌────────────┐ │
│   │ Touchdown  │  │    CYD     │  │   Generic  │ │
│   │ ILI9488    │  │  ST7789    │  │  ESP32dev  │ │
│   │ 480×320    │  │  320×240   │  │            │ │
│   │            │  │            │  │  (untested)│ │
│   └────────────┘  └────────────┘  └────────────┘ │
│                                                     │
└─────────────────────────────────────────────────────┘
```

**Key Point:** Application logic is in Layer 1. Swapping hardware (Layer 2-3) requires only configuration changes (User_Setup.h, GPIO pins), not application code changes.

---

## 🧩 Feature Dependency Map

```
                         MAIN LOOP
                             │
          ┌──────────────────┼──────────────────┐
          │                  │                  │
          ▼                  ▼                  ▼
      TFT Display      WiFi & OTA          Sensor I2C
      (required)       (optional)          (optional)
          │                  │                  │
          ├─────────┬────────┤       ┌──────────┤
          │         │        │       │          │
          ▼         ▼        ▼       ▼          ▼
      7-SEG    Tetris    NTP Sync  Web Server  Temp/Humidity
      Mode     Mode      (time)    (config)    Display
          │         │        │       │          │
          └────┬────┘        │       │          │
               │             │       │          │
               ▼             ▼       ▼          ▼
          Clock Display   Accurate   Config    Status Bar
          (all the time)  Time      Storage    (optional)
                          (daily)   (instant)


Legend:
┌─────┐
│box  │ = feature or component
└─────┘

Required:  • Main loop, TFT display, clock rendering
Optional:  • WiFi/OTA, NTP, web interface, sensors
Cascading: • NTP requires WiFi; Web requires WiFi; 
             Sensors enhance status bar but not required
```

---

## 🔌 I2C Address Configuration Map

```
┌─────────────────────────────────────────────┐
│   ESP32 I2C Bus (GPIO21=SDA, GPIO22=SCL)    │
└────────────┬────────────────────────────────┘
             │
    ┌────────┼────────┬───────────────┐
    │        │        │               │
    ▼        ▼        ▼               ▼
  0x38     0x40     0x44-0x45      0x77-0x76
  Touch    HTU21D   SHT31          BME280
  (FT62x6) (default) (optional)     (optional)

Usage:
├─ Touch: FT6236/FT6206 capacitive (currently unused UI)
├─ Sensor: Select ONE in config.h via #define USE_*
│  • HTU21D @ 0x40 (DEFAULT)
│  • SHT31 @ 0x44 or 0x45
│  • BME280 @ 0x77 or 0x76
└─ I2C shared bus: Can run all simultaneously

For CYD: Verify addresses with I2C scan
(see troubleshooting guide for scan code)
```

---

## ⏱️ Timing & Frame Rate Overview

```
Main Loop Timing (target ~20 FPS):

┌─────────────────────────────────────────────┐
│   Loop Iteration (~50ms target)             │
├─────────────────────────────────────────────┤
│                                             │
│  ArduinoOTA.handle()      ← <1ms usually    │
│  server.handleClient()    ← variable (web)  │
│  updateSensorData()       ← 60s timer       │
│  checkAutoRotation()      ← <1ms            │
│  Colon toggle check       ← 1s timer        │
│  updateClockLogic()       ← <5ms            │
│  renderCurrentMode()      ← 5-20ms depends  │
│  renderFBToTFT()          ← 5-50ms depends  │
│                                             │
│  Total: ~50ms typical (20 FPS)              │
│         varies with features active         │
│                                             │
└─────────────────────────────────────────────┘

Sub-timers:
• Time update:        1s (updateClockLogic)
• Sensor update:      60s (updateSensorData)
• Morph animation:    0-1s (20 frames × 50ms)
• Tetris animation:   1.8s default (TETRIS_ANIMATION_SPEED)
• Mode rotation:      5+ minutes (configurable)
• Status bar redraw:  when content changes
• Web requests:       as they arrive
```

---

## 📚 Module Dependencies Summary

```
main.cpp (2,249 lines)
├── Includes
│   ├── Arduino core
│   ├── WiFi.h, WebServer.h, WiFiManager
│   ├── ArduinoJson.h
│   ├── Preferences.h (NVS)
│   ├── LittleFS.h
│   ├── TFT_eSPI.h (display + sprite)
│   ├── ArduinoOTA.h
│   ├── time.h, Wire.h (I2C/NTP)
│   ├── config.h (project settings)
│   ├── timezones.h (88 timezones)
│   ├── TetrisClock.h (Tetris wrapper)
│   └── Adafruit sensor libs (BME280/SHT31/HTU21D)
│
├── Core Components
│   ├── Debug system (5 levels)
│   ├── Global state (fb, cfg, etc.)
│   ├── 7-segment bitmaps
│   ├── Utility functions (RGB888→565, fbSet, fbClear)
│   └── Morphing algorithms (drawMorph, drawParticleMorph)
│
├── Display Management
│   ├── Sprite rendering (rebuildSprite, updateRenderPitch)
│   ├── TFT rendering (renderFBToTFT)
│   ├── Status bar (drawStatusBar)
│   ├── Backlight control (setBacklight)
│   └── Display rotation (applyDisplayRotation)
│
├── Clock Logic
│   ├── Mode management (switchClockMode, checkAutoRotation)
│   ├── 7-segment rendering (drawFrame)
│   ├── Tetris rendering (drawFrameTetris)
│   ├── Animation control (applyFade, renderCurrentMode)
│   ├── Time logic (updateClockLogic, format time)
│   └── Tetris clock wrapper
│
├── Configuration
│   ├── Load/save (loadConfig, saveConfig)
│   ├── Web API handler (handlePostConfig)
│   ├── Validation & change logging
│   └── NVS persistence
│
├── Network
│   ├── WiFi setup (startWifi, WiFiManager)
│   ├── NTP sync (startNtp, setTimezone)
│   ├── OTA updates (startOta, drawOTAProgress)
│   └── Web server (serveStaticFiles, API endpoints)
│
├── Sensors
│   ├── Sensor detection (testSensor)
│   ├── Data updates (updateSensorData)
│   └── I2C communication
│
├── Startup
│   ├── Display init (initStartupDisplay)
│   ├── Startup messages (showStartupStep, showStartupStatus)
│   └── Setup sequence
│
├── Web API Handlers
│   ├── GET / (index.html)
│   ├── GET /api/state (JSON)
│   ├── POST /api/config (JSON)
│   ├── GET /api/mirror (binary RGB565)
│   ├── GET /api/timezones (JSON)
│   ├── POST /api/reset-wifi
│   └── Static file serving (/app.js, /style.css)
│
└── Main Loop
    ├── OTA handler
    ├── Web server client processing
    ├── Sensor updates (60s)
    ├── Auto-rotation check
    ├── Colon toggle (1s)
    ├── Time update detection
    ├── Display rendering (conditional)
    └── Status bar update (conditional)

TetrisClock.h (164 lines)
├── FramebufferGFX class (Adafruit_GFX adapter)
│   └── Bridges TetrisAnimation library to RGB565 framebuffer
└── TetrisClock class
    ├── update() - animate blocks into time
    ├── isAnimating() - check animation state
    ├── reset() - force rebuild
    └── Uses TetrisAnimation library (GitHub)

config.h (200 lines)
├── Compile-time settings
├── Hardware pins
├── Defaults (timezone, NTP, LED size, etc.)
├── Debug level
├── Animation speeds
├── Sensor selection
└── OTA credentials

timezones.h (200 lines)
├── 88 global timezones with POSIX TZ strings
└── Organized by 13 geographic regions

User_Setup.h (60 lines)
├── TFT_eSPI configuration
├── Display driver (ILI9488 for Touchdown)
├── GPIO pins
├── SPI frequency
├── DMA settings
└── Font selection

data/index.html, app.js, style.css
├── Web UI for configuration
├── Live display mirror
├── System diagnostics panel
├── Timezone/NTP dropdowns
├── Color picker, brightness, LED sizing
└── Real-time updates via AJAX

```

---

This architecture is designed for:
- ✓ **Modularity**: Each component is independent
- ✓ **Portability**: Display-independent logic
- ✓ **Extensibility**: Easy to add new modes or features
- ✓ **Maintainability**: Clear separation of concerns
- ✓ **Performance**: Optimized rendering pipeline

---

**End of Architecture Documentation**
