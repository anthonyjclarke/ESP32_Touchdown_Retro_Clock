# Project: ESP32 Touchdown RGB LED Matrix (HUB75) Retro Clock

## Overview
A retro-style RGB LED Matrix clock that emulates a physical 64×32 HUB75 LED panel on a 480×320 ILI9488 TFT display. Features three clock display modes with smooth morphing animations (Classic, Tetris blocks, and Remix with bezier curves), capacitive touch interface, WiFi connectivity, NTP time synchronization, environmental sensor support, and a full-featured web-based configuration interface with live display mirroring. Built specifically for the ESP32 Touchdown development board by Dustin Watts.

## Hardware
- **MCU**: ESP32-WROOM-32D (dual-core 240MHz, 520KB SRAM, 4MB Flash)
- **Display**: ILI9488 480×320 TFT (RGB565, 4-wire SPI with DMA)
- **Touch**: FT6236/FT6206 capacitive touch controller (I2C @ 0x38)
- **Peripherals**:
  - Passive buzzer (GPIO26) - not currently used
  - MicroSD card slot (SD mode) - not currently used
  - Battery voltage monitor (GPIO35) - not currently used
  - Optional I2C environmental sensors (BME280, BMP280, BMP180, SHT3X, HTU21D)
- **Power**: USB-C (5V), supports LiPo battery operation with MCP73831/SD8016 charge management
- **Board**: ESP32 Touchdown by Dustin Watts
  - GitHub: https://github.com/DustinWatts/esp32-touchdown
  - Purchase: Tindie, Lectronz (NL), Eplop Electronics (UK), PCBWay (China)

## Build Environment
- **Framework**: Arduino (espressif32 platform)
- **Platform**: espressif32
- **IDE**: PlatformIO (VS Code extension)
- **Key Libraries**:
  - `TFT_eSPI @ ^2.5.43` - Fast TFT display driver with DMA
  - `WiFiManager @ ^2.0.16-rc.2` - Captive portal WiFi configuration
  - `ArduinoJson @ ^7.0.4` - JSON parsing/serialization
  - `TetrisAnimation` (GitHub) - Tetris block animation
  - `Adafruit GFX Library @ ^1.11.11` - Graphics primitives
  - `Adafruit FT6206 Library @ ^1.1.0` - Capacitive touch
  - Adafruit sensor libraries (BME280, BMP280, BMP085, SHT31, HTU21DF)
- **Filesystem**: LittleFS for web UI files
- **Storage**: NVS (Preferences) for configuration persistence
- **Code Size**: ~4,500 lines (main.cpp: 3,873 lines)

## Project Structure
```
ESP32_Touchdown_Retro_Clock/
├── platformio.ini              # Build configuration
├── README.md                   # User documentation
├── CHANGELOG.md                # Version history (v2.6.0)
├── CLAUDE.md                   # This file - AI assistant context
├── LICENSE                     # MIT License
│
├── include/                    # Header files (644 lines total)
│   ├── config.h               # Configuration constants & feature flags
│   ├── User_Setup.h           # TFT_eSPI pin configuration
│   ├── timezones.h            # 88 IANA timezones grouped by region
│   ├── MorphingDigit.h        # Bezier curve segment morphing
│   └── TetrisClock.h          # Tetris animation wrapper
│
├── src/                        # Source code
│   ├── main.cpp               # Main application (3,873 lines)
│   └── MorphingDigit.cpp      # Morphing digit implementation (2,394 bytes)
│
├── data/                       # Web UI (uploaded to LittleFS)
│   ├── index.html             # Configuration interface
│   ├── app.js                 # Live display mirror & API calls
│   └── style.css              # Styling
│
└── doc/                        # Technical documentation
    ├── ARCHITECTURE.md
    ├── CODEBASE_REVIEW.md
    ├── CYD_PORTING_GUIDE.md
    ├── MIGRATION_NOTES.md
    ├── PARTITIONING_GUIDE.md
    └── README.md
```

## Pin Mapping

### Display (SPI)
| Function | GPIO | Notes |
|----------|------|-------|
| TFT_MOSI | 23 | SPI MOSI (shared with SD card) |
| TFT_SCLK | 18 | SPI Clock @ 40MHz (shared with SD) |
| TFT_CS   | 15 | Chip Select |
| TFT_DC   | 2  | Data/Command (DC_RS) |
| TFT_RST  | 4  | Reset |
| TFT_BL   | 32 | Backlight PWM control (0-255) |
| TFT_MISO | -1 | Not used (4-wire SPI) |

### Touch Controller (I2C)
| Function | GPIO | Notes |
|----------|------|-------|
| TOUCH_SDA | 21 | I2C Data (shared with sensors) |
| TOUCH_SCL | 22 | I2C Clock (shared with sensors) |
| TOUCH_IRQ | 27 | Touch interrupt (optional) |
| - | 0x38 | Fixed I2C address |

### Environmental Sensors (I2C, optional)
| Function | GPIO | Notes |
|----------|------|-------|
| SENSOR_SDA | 21 | Shared I2C bus with touch |
| SENSOR_SCL | 22 | Shared I2C bus with touch |
| - | varies | BME280/BMP280: 0x76/0x77<br>BMP180: 0x77<br>SHT3X: 0x44/0x45<br>HTU21D: 0x40 |

### MicroSD Card (SD mode, not used)
| Function | GPIO | Notes |
|----------|------|-------|
| SD_DAT3  | 25 | CD/DAT3 |
| SD_CMD   | 23 | Shared with TFT_MOSI |
| SD_CLK   | 18 | Shared with TFT_SCLK |
| SD_DAT0  | 19 | - |

### Other Peripherals
| Function | GPIO | Notes |
|----------|------|-------|
| Buzzer | 26 | Passive buzzer (not used) |
| Battery ADC | 35 | Voltage divider (not used) |
| Available GPIO | 12, 13, 14, 16, 17, 33, 34 | Unused breakout pins |

## Configuration

### Main Config Files
- **`include/config.h`**: Core configuration constants
  - Firmware version (currently 2.6.0)
  - LED matrix dimensions (64×32 virtual HUB75 panel)
  - Clock modes, rendering settings
  - Pin definitions
  - Feature flags (sensor selection, touch enable)
  - OTA credentials

- **`include/User_Setup.h`**: TFT_eSPI hardware configuration
  - Display driver selection (ILI9488)
  - Pin mappings
  - SPI frequency and DMA settings

### Runtime Configuration (NVS Storage)
All settings persist across reboots and are configurable via web UI:

**Time & Date:**
- Timezone (88 IANA timezones across 13 regions)
- NTP server (9 preset options)
- 12/24 hour format
- Date format (5 options: ISO, European, US, German, Verbose)
- Temperature unit (Celsius/Fahrenheit)

**Clock Display:**
- Clock mode (Morphing Classic, Tetris, Morphing Remix)
- Auto-rotation enable/disable
- Rotation interval (1-60 minutes)

**LED Appearance:**
- LED color (RGB888, color picker)
- LED diameter (1-10 pixels)
- LED gap (0-8 pixels)
- Brightness (0-255)
- Morph speed (1-50x multiplier)
- Display flip (0° or 180° rotation)

**Morphing (Remix) Mode:**
- Show sensor data (on/off)
- Sensor color (RGB888)
- Show date (on/off)
- Date color (RGB888)

**System:**
- Debug level (0=Off, 1=Error, 2=Warn, 3=Info, 4=Verbose)
- WiFi credentials (via WiFiManager)

### Key Constants (config.h)
```cpp
#define FIRMWARE_VERSION "2.6.0"
#define LED_MATRIX_W 64                 // Virtual HUB75 panel width
#define LED_MATRIX_H 32                 // Virtual HUB75 panel height
#define DEFAULT_LED_DIAMETER 7          // LED dot size (pixels)
#define DEFAULT_LED_GAP 0               // Gap between LEDs (pixels)
#define DEFAULT_TZ "Sydney, Australia"  // Timezone name
#define DEFAULT_NTP "pool.ntp.org"      // NTP server
#define DEFAULT_24H true                // 24-hour format
#define DEFAULT_CLOCK_MODE CLOCK_MODE_MORPH  // Mode 2 (Morphing Remix)
#define DEFAULT_AUTO_ROTATE false       // Manual mode switching
#define DEFAULT_ROTATE_INTERVAL 5       // Minutes between mode changes
#define DEFAULT_TEMP_C true             // Celsius vs Fahrenheit
#define FRAME_MS 50                     // ~20 FPS rendering
#define MORPH_STEPS 20                  // Animation frame count
#define MORPH_PITCH_X 8                 // Horiz scaling (64×8=512px)
#define MORPH_PITCH_Y 9                 // Vert scaling (32×9=288px)
#define TETRIS_ANIMATION_SPEED 1800     // 1.8s per frame
#define SENSOR_UPDATE_INTERVAL 60000    // 60 seconds
#define TOUCH_DEBOUNCE_MS 300           // Touch debounce
#define TOUCH_LONG_PRESS_MS 3000        // Long press threshold
#define OTA_HOSTNAME "Touchdown-RetroClock"
#define OTA_PASSWORD "change-me"        // ⚠️ Change before deployment!
#define HTTP_PORT 80                    // Web server port
```

### Web API Endpoints
- `GET /` - Web UI interface
- `GET /api/state` - Full system state JSON
- `GET /api/timezones` - Timezone list JSON
- `GET /api/mirror` - Raw framebuffer (4096 bytes, RGB565 format: 64×32×2)
- `POST /api/config` - Update configuration
- `POST /api/reset-wifi` - Reset WiFi credentials

## Current State

**Version**: 2.6.0 (2026-01-15)

**Completed Features:**
- ✅ Three clock display modes with smooth animations
- ✅ Virtual 64×32 HUB75 LED matrix emulation
- ✅ Capacitive touch interface with info pages
- ✅ WiFi configuration via captive portal
- ✅ NTP time synchronization with timezone support
- ✅ Web-based configuration interface
- ✅ Live display mirror in web UI
- ✅ OTA firmware updates
- ✅ Environmental sensor support (compile-time selection)
- ✅ Persistent configuration storage (NVS)
- ✅ Runtime-adjustable debug logging
- ✅ Touch-based mode switching and info pages
- ✅ Animated startup splash screen (RGB sweep, text animation)
- ✅ Independent color settings for Morphing Remix mode

**Functional Status**: Production-ready, actively maintained

**Latest Changes (v2.6.0)**:
- Added independent color pickers for sensor/date in Morphing Remix mode
- Fixed date truncation bug in "Mon DD, YYYY" format (buffer 11→14 chars)
- Removed fade transition for instant mode switching at full brightness
- Added animated startup splash screen (RGB sweep, noise, "64X32" + "HUB75 LED MATRIX EMULATOR" text)
- Sped up splash screen text transitions
- Fixed colon flashing in Morphing Classic mode (now uses clockColon state)
- Increased colon brightness in Morphing Remix mode (50%→75%)

## Architecture Notes

### Core Design Patterns

**1. Virtual LED Matrix Emulation**
```
Framebuffer (64×32 @ RGB565)    →  TFT Rendering (480×320 @ RGB565)
────────────────────────────       ────────────────────────────────
uint16_t fb[32][64] = RGB565       Each LED → 7×7px rounded rect
Size: 4096 bytes (64×32×2)         Pitch: 7-8px horiz, 8-10px vert
Full color per LED                 Delta rendering (changed pixels only)
                                   Configurable: diameter, gap, brightness
```

**2. Multi-Mode Clock System (Strategy Pattern)**
```cpp
const uint8_t TOTAL_CLOCK_MODES = 3; // 0=7-seg, 1=Tetris, 2=Morph

// Mode rendering functions:
drawFrame()        // Mode 0: Morphing Classic (7-segment with particle morphing)
drawFrameTetris()  // Mode 1: Tetris blocks falling into place
drawFrameMorph()   // Mode 2: Morphing Remix (bezier curve segment transitions)

// Colon blinking (clockColon toggled every 1000ms)
bool clockColon = true;  // Global state for colon visibility
// - Morphing Classic: Colons flash on/off (uses clockColon check)
// - Tetris: Colons flash on/off (passed to tetrisClock->update())
// - Morphing Remix: Colons flash on/off at 75% brightness
```
- Mode switching: Single tap (touch) or web UI
- Auto-rotation: Cycles through all 3 modes at configurable interval (1-60 minutes)
- All modes share the same 1-second colon blink state

**3. Configuration Management**
```cpp
struct AppConfig {
  char tz[48], ntp[64];
  bool use24h;
  uint8_t dateFormat;              // 0-4 (ISO, EU, US, German, Verbose)
  uint8_t ledDiameter, ledGap;
  uint32_t ledColor;                // RGB888 for web, converted to RGB565 for TFT
  uint8_t brightness;               // 0-255
  bool flipDisplay;                 // 180° rotation toggle
  uint8_t morphSpeed;               // 1-50 multiplier
  uint8_t clockMode;                // 0-2 (7-seg, Tetris, Morph)
  bool autoRotate;                  // Auto-cycle through modes
  uint8_t rotateInterval;           // 1-60 minutes
  bool useFahrenheit;
  bool morphShowSensor, morphShowDate;
  uint32_t morphSensorColor, morphDateColor;  // RGB888
  int16_t touchOffsetX, touchOffsetY;
};
```
- NVS (Preferences) for persistent storage (18 keys)
- In-memory config struct for runtime access
- Web API for remote configuration
- Automatic save on any change
- Before/after logging for all config updates

**4. Touch State Machine**
```
IDLE (Clock)
  ├─ Single Tap → Switch clock mode
  └─ Long Press (3s) → Info Page 0 (User Settings)
                        ├─ < > buttons → Navigate pages
                        ├─ X button → Return to clock
                        ├─ Flip Display → Rotate 180°
                        └─ Swipe → Info Page 1 (Diagnostics)
                                    ├─ Reset WiFi
                                    ├─ Reboot
                                    └─ X → Return to clock
```

**5. Rendering Pipeline**
```
Clock Logic → Framebuffer → TFT Display
    ↓             ↓              ↓
  Time        RGB565 LED     Scaled pixels
  Parse       colors in      with rounded
  Digits      64×32 grid     rectangle LEDs
```
- Each clock mode renders to the same fb[32][64] framebuffer
- renderFBToTFT() called every FRAME_MS (50ms = ~20 FPS)
- Delta rendering: Only updates changed pixels (fbPrev comparison)

**6. Morphing Animation (Bezier Curves)**
- MorphingDigit class manages per-digit state
- Stores current/target 7-segment states
- Interpolates using bezier curves over MORPH_STEPS frames
- Configurable speed multiplier (1-50x)

**7. Debug Logging (Leveled System)**
```cpp
0 = Off, 1 = Error, 2 = Warn, 3 = Info, 4 = Verbose
Runtime-adjustable via web API
Macros: DBG_ERROR, DBG_WARN, DBG_INFO, DBG_VERBOSE
```

**8. Startup Splash Screen Animation (~350 lines)**
A cinematic LED matrix demonstration sequence:
```
Phase 1: RGB Test Pattern
  - Full-screen LED-style dot sweeps (Red → Green → Blue)
  - 2 columns at a time for speed (4ms delay)
  - Uses 6×6 pixel rounded dots with 2px gaps

Phase 2: Random Pixel Noise
  - 800 random colored LED dots appear
  - 600 black dots gradually clear the screen

Phase 3: Grid Size Display
  - Corner brackets animate at TFT edges (480×320)
  - "64X32" text appears pixel-by-pixel in center
  - Dissolves with gravity (pixels fall off screen)

Phase 4: LED Matrix Text
  - "HUB75 LED" appears in green
  - "MATRIX" continues in green
  - Sped up transition delay (600ms)

Phase 5: Emulator Text
  - "EMULATOR" appears in cyan
  - All text dissolves with falling animation (1200ms pause)

Features:
  - Touch to skip any phase
  - 3×5 pixel font for retro LED text look
  - Pixel-by-pixel rendering with delays (8ms per pixel)
  - Gravity-based dissolve effect
```

### Key Design Decisions

**Why virtual LED matrix?**
- Authentic retro LED panel aesthetic
- Scales cleanly to any TFT size
- Consistent look across clock modes
- Easy to reason about positioning (64×32 grid vs 480×320 pixels)

**Why three clock modes?**
- Visual variety (auto-rotation support)
- Different animation styles for user preference
- Demonstrates multiple rendering techniques
- Credits original creators (HariFun, toblum, lmirel)

**Why framebuffer instead of direct TFT?**
- Decouples logic from display hardware
- Enables web UI mirroring via /api/mirror
- Simplifies animation calculations
- Consistent LED emulation rendering

**Why NVS instead of JSON file?**
- Faster access (no filesystem overhead)
- More reliable (no SD card required)
- Built-in wear leveling
- Simple key-value API

**Critical Synchronization Requirement:**
- `MORPH_PITCH_X` and `MORPH_PITCH_Y` in config.h must match app.js
- Manual sync required when changing display scaling
- TODO: Could be automated via API endpoint

## Known Issues

### Documentation
- ⚠️ README.md version badge shows v2.5.0, should be v2.6.0 (line 4)
- ⚠️ Manual synchronization required between config.h and app.js for MORPH_PITCH values
  - Critical note exists in config.h lines 70-72
  - Forgetting to sync causes web UI mirror to misalign

### Hardware Limitations
- **Single sensor support**: Only one sensor type can be active (compile-time selection in config.h)
  - No runtime detection or multi-sensor capability
  - Must uncomment ONE sensor define and recompile
  - Currently configured: `#define USE_BMP280`
- **I2C address conflicts**: Cannot use multiple sensors with same address simultaneously
- **Touch calibration**: No on-screen calibration utility (must manually adjust offsets in config)

### Unused Hardware Features
- Passive buzzer (GPIO26) - no sound/alarm functionality implemented
- MicroSD card slot - not used for logging, config backup, or data storage
- Battery voltage monitor (GPIO35) - no battery status display or low-battery warning
- 7+ GPIO breakout pins (12, 13, 14, 16, 17, 33, 34) - not exposed for extensions

### Design Constraints
- **Pitch synchronization**: Web UI mirror depends on hardcoded pitch values matching firmware
  - `MORPH_PITCH_X` and `MORPH_PITCH_Y` must match in both config.h and app.js
  - No API endpoint to query current pitch values
- **Single mode display**: Only one clock mode visible at a time (no split-screen or preview)
- **Sensor data visibility**: Sensor readings only shown in Morphing Remix mode (not in Classic or Tetris)
- **No sensor auto-detection**: Must manually configure sensor type in config.h before compile
- **Status bar limitations**: Status bar only shown in Classic/Tetris modes (hidden in Morphing Remix for full-screen)

### Security Considerations
- ⚠️ Default OTA password is "change-me" - **MUST** be changed before deployment
- No HTTPS support (HTTP only) - traffic is unencrypted
- No authentication on web interface - anyone on network can access
- No rate limiting on API endpoints
- WiFi credentials stored in NVS (secure, but device should be physically secured)

## TODO

### High Priority
- [ ] Update README.md version badge to 2.6.0
- [ ] Add API endpoint to query MORPH_PITCH values (eliminate manual sync)
- [ ] Change default OTA_PASSWORD to something more secure
- [ ] Add runtime sensor auto-detection (eliminate compile-time selection)

### Medium Priority
- [ ] Implement battery voltage monitoring and display
- [ ] Add alarm/timer functionality using buzzer
- [ ] Support multiple sensors simultaneously
- [ ] Add HTTPS support for web interface
- [ ] Implement web UI authentication

### Low Priority / Future Enhancements
- [ ] Additional clock modes (analog, binary, word clock)
- [ ] MicroSD logging support (time changes, sensor readings)
- [ ] Color schemes and theme presets
- [ ] MQTT integration for home automation
- [ ] Weather display mode using API
- [ ] Text message display mode
- [ ] Animation speed presets (slow/medium/fast)
- [ ] Expose GPIO breakout pins for extensions via web UI

### Nice to Have
- [ ] Split-screen mode (show multiple clocks)
- [ ] Customizable startup splash screen
- [ ] Sound effects via buzzer (tick, chime, alarm)
- [ ] Battery percentage display in status bar
- [ ] Gesture support (swipe to change modes)
- [ ] Scene/profile switching (work/sleep/party presets)

---

## Quick Start for AI Assistants

**When helping with this project:**

1. **Main code is in**: `src/main.cpp` (3,873 lines)
2. **Configuration**: `include/config.h` (version, features, pins)
3. **Web UI**: `data/index.html` and `data/app.js`
4. **Current version**: 2.6.0 (update in config.h and README)
5. **Build**: `pio run` (PlatformIO)
6. **Upload**: `pio run -t upload` (firmware) + `pio run -t uploadfs` (web UI)

**Common tasks:**
- Version bump: Edit `include/config.h` FIRMWARE_VERSION and CHANGELOG.md
- Add feature: Check if it needs NVS storage, web UI, and API endpoint
- Pin change: Edit `include/User_Setup.h` or `include/config.h`
- Display tuning: Adjust MORPH_PITCH_X/Y in config.h AND app.js
- Debug: Serial @ 115200 baud, adjust debugLevel via web UI

**Architecture to respect:**
- Framebuffer abstraction (don't bypass with direct TFT calls in clock modes)
- Config save/load pattern (use NVS Preferences, not files)
- Mode switching via cfg.clockMode (don't add mode-specific globals)
- Touch state machine (idle → info pages → back to idle)

**Credits & Licensing:**
- Hardware: ESP32 Touchdown by Dustin Watts
- Morphing Classic: Hari Wiguna (HariFun)
- Tetris Animation: Tobias Blum (toblum)
- Morphing Remix: lmirel
- Integration & Features: Anthony Clarke
- AI Assistant: Claude (Anthropic)
- License: MIT

---

## Document Revision History

**2026-01-16**: Initial CLAUDE.md creation and accuracy review
- Created comprehensive project context document
- Verified against actual codebase (main.cpp 3,873 lines)
- Corrections made:
  - Framebuffer format: RGB565 (not 8-bit intensity)
  - Mirror API size: 4096 bytes (not 2048)
  - MorphingDigit.cpp exists (2,394 bytes)
  - Splash screen details added (~350 lines, 5 phases)
  - Auto-rotation implementation documented
  - Colon blinking behavior clarified (all modes)
  - AppConfig struct fully documented (18 NVS keys)
  - Constants expanded with defaults
  - Known issues enhanced with specifics
- Verified accuracy: Pin mappings, library versions, feature completeness
- Document is now complete and accurate for v2.6.0
