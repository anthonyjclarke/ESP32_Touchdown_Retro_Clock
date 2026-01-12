# CYD ESP32 Clock - Porting Implementation Guide

**Target Device:** CYD ESP32 (320×240 display)
**Source Project:** ESP32 Touchdown Retro Clock v2.1.0
**Date:** January 12, 2026

---

## 🎯 Quick Reference: Key Changes Required

### 1. Display Configuration (User_Setup.h)

**BEFORE (Touchdown - ILI9488 480×320):**
```cpp
#define ILI9488_DRIVER
#define TFT_MOSI  23
#define TFT_SCLK  18
#define TFT_CS    15
#define TFT_DC     2
#define TFT_RST    4
#define TFT_BL    32
```

**AFTER (CYD - ST7789 320×240):**
```cpp
#define ST7789_DRIVER  // Update display driver for CYD
#define TFT_MOSI  23   // Verify against actual CYD pinout
#define TFT_SCLK  18   // Verify against actual CYD pinout
#define TFT_CS    15   // Verify against actual CYD pinout
#define TFT_DC     2   // Verify against actual CYD pinout
#define TFT_RST    4   // Verify against actual CYD pinout
#define TFT_BL    32   // Verify backlight pin on CYD
// ... add any CYD-specific pins or configurations
```

**Action Items:**
1. Check CYD documentation for exact GPIO pins
2. Identify display driver (likely ST7789 or ILI9341)
3. Verify SPI frequency compatibility
4. Test display initialization with new config

---

### 2. Configuration Constants (config.h)

**BEFORE (Touchdown 480×320):**
```cpp
#define DEFAULT_LED_DIAMETER 7     // Larger screen allows bigger LEDs
#define DEFAULT_LED_GAP      0
#define STATUS_BAR_H 70            // 70px for status bar

// Build flags in platformio.ini
-DLED_MATRIX_W=64
-DLED_MATRIX_H=32
```

**AFTER (CYD 320×240):**
```cpp
#define DEFAULT_LED_DIAMETER 5     // Smaller screen, smaller LEDs
#define DEFAULT_LED_GAP      0     // Keep gap minimal
#define STATUS_BAR_H 40            // Reduce status bar to 40px

// Build flags in platformio.ini
-DLED_MATRIX_W=64
-DLED_MATRIX_H=32  // Keep LED matrix size same!
```

**Calculation for CYD:**
```
Display: 320×240
Available for matrix: 320×(240-40) = 320×200

fbPitch = min(320/64, 200/32) = min(5, 6.25) = 5
Result: 64*5 = 320px wide, 32*5 = 160px tall
Status bar: 40px (leaves 240-160-40 = 40px margin)
```

---

### 3. PlatformIO Configuration (platformio.ini)

**BEFORE:**
```ini
[env:esp32_touchdown]
platform = espressif32
board = esp32dev
```

**AFTER (for CYD):**
```ini
[env:cyd_esp32]
platform = espressif32
board = esp32dev  # CYD typically uses generic esp32dev
# OR check if specific CYD board is available
# board = esp32-cyd  # (if available)
```

---

### 4. Status Bar Content Adjustment

**Current (Touchdown 70px):**
- Line 1: "Temp: 22°C  Humidity: 45%"
- Line 2: "2024-01-11  Sydney, Australia"

**For CYD (40px):**
- Option A: Single line with truncated info
- Option B: Rotate/scroll content
- Option C: Reduce font size (current is reasonable)

**Code Change in src/main.cpp (~580-620):**
```cpp
// Current layout calculation
static void drawStatusBar() {
  int barY = tft.height() - STATUS_BAR_H;  // Now 200 instead of 250
  
  // Two-line display with smaller font if needed
  char line1[64];
  snprintf(line1, sizeof(line1), "%.20°C  %d%%", displayTemp, humidity);  // Shortened
  
  char line2[64];
  snprintf(line2, sizeof(line2), "%s", currDate);  // Timezone optional
}
```

---

### 5. Touch Controller Configuration

**CYD Touch Controller (likely different from Touchdown):**

**BEFORE (Touchdown - FT6236/FT6206 @ I2C 0x38):**
```cpp
// include/config.h
#define TOUCH_SDA_PIN     21    // I2C Data (shared with sensor)
#define TOUCH_SCL_PIN     22    // I2C Clock (shared with sensor)
#define TOUCH_IRQ_PIN     27    // Touch interrupt pin
#define TOUCH_I2C_ADDR    0x38  // FT6236/FT6206 I2C address

#define ENABLE_TOUCH         1     // Enable touch support
#define TOUCH_DEBOUNCE_MS    300   // Debounce time
#define TOUCH_LONG_PRESS_MS  3000  // Long press (3 seconds)
#define TOUCH_INFO_PAGES     2     // Number of info pages
```

**AFTER (CYD - check actual controller):**
```cpp
// CYD may use different touch IC (check datasheet)
// Common options: FT6236, CST816, GT911, XPT2046 (resistive), etc.
#define TOUCH_SDA_PIN  21  // Verify on CYD (I2C capacitive)
#define TOUCH_SCL_PIN  22  // Verify on CYD (I2C capacitive)
#define TOUCH_I2C_ADDR 0x38  // Verify I2C address
#define TOUCH_IRQ      27  // Verify interrupt pin

// CYD with resistive touch (XPT2046) uses SPI instead:
// #define TOUCH_CS    33
// #define TOUCH_IRQ   36
// In this case, you'll need to replace Adafruit_FT6206 with XPT2046_Touchscreen library
```

**⚠️ Important Note for v2.1.0:**
The Touchdown v2.1.0 now includes **full touch interface implementation**:
- Touch-based info pages (User Settings & System Diagnostics)
- Interactive buttons (Flip Display, Reset WiFi, Reboot)
- Rotation-aware touch mapping
- Touch calibration support (touchOffsetX/Y)

If CYD uses **resistive touch** (XPT2046), you'll need to:
1. Replace `Adafruit_FT6206` library with `XPT2046_Touchscreen`
2. Update touch initialization code in `src/main.cpp`
3. Adjust coordinate mapping for XPT2046 output format
4. Test and calibrate touch coordinates

If CYD uses **capacitive touch** (FT6236/FT6206), the code should work with minimal changes!

---

## 🖐️ Touch Interface Features (New in v2.1.0)

### Touch Coordinate Mapping

**Important:** Touch coordinate mapping must be adjusted for CYD's display resolution.

**Touchdown (480×320) Mapping:**
```cpp
// For rotation 1 (USB on right):
touchX = map(point.y, 0, 480, 0, 479);  // Touch Y -> Screen X
touchY = map(point.x, 0, 320, 319, 0);  // Touch X -> Screen Y (inverted)

// For rotation 3 (flipped 180°):
touchX = map(point.y, 0, 480, 479, 0);  // Touch Y -> Screen X (inverted)
touchY = map(point.x, 0, 320, 0, 319);  // Touch X -> Screen Y (normal)
```

**CYD (320×240) Mapping:**
```cpp
// Adjust based on CYD's touch controller orientation
// For ST7789 with typical portrait touch:
// For rotation 1:
touchX = map(point.y, 0, 320, 0, 319);  // Adjust based on testing
touchY = map(point.x, 0, 240, 239, 0);  // Adjust based on testing

// For rotation 3 (flipped):
touchX = map(point.y, 0, 320, 319, 0);  // Inverted
touchY = map(point.x, 0, 240, 0, 239);  // Normal
```

**Testing Touch Mapping:**
1. Enable debug output: `DBG_INFO` statements already in code
2. Touch corners of screen and note raw coordinates
3. Adjust mapping formula to match screen coordinates
4. Use `cfg.touchOffsetX` and `cfg.touchOffsetY` for fine-tuning

### Info Page Layout Considerations

**Touchdown Layout (480×320):**
- Content area: 0-320px (left side)
- Button area: 320-480px (right side)
- Navigation buttons: Top right (330-475, 5-35)
- Action buttons: Right side (330-470, 80-180)

**CYD Layout (320×240) - Will Need Adjustment:**

**Option A: Bottom Button Bar (Recommended)**
```
┌──────────────────────────┐
│ USER SETTINGS            │ ← Header (y: 0-40)
│ Content Area             │
│ - Display: Tetris        │ ← Content (y: 40-190)
│ - Time: 12-hour          │
│ - LED: 5px, Red          │
├──────────────────────────┤
│ [<] [>] [X] [Flip]       │ ← Buttons (y: 190-240)
└──────────────────────────┘
```

**Option B: Compact Right-Side (Tight Fit)**
```
┌─────────────────────┬────┐
│ USER SETTINGS       │[<] │ ← y: 0-30
│                     │[>] │
│ Content Area        │[X] │ ← y: 30-200
│ - Display: Tetris   │    │
│ - Time Format       │    │
│                     │[F] │ ← Action button
└─────────────────────┴────┘
```

**Code Changes Required:**
```cpp
// src/main.cpp - Button definitions (~line 750)
#if defined(CYD_320x240)
  // Bottom bar layout for CYD
  static Button btnPrev = {10, 200, 70, 35, "<", TFT_DARKGREY};
  static Button btnNext = {85, 200, 70, 35, ">", TFT_DARKGREY};
  static Button btnClose = {160, 200, 70, 35, "X", TFT_RED};
  static Button btnFlipDisplay = {235, 200, 80, 35, "Flip", TFT_ORANGE};
  static Button btnResetWiFi = {10, 200, 150, 35, "Reset WiFi", TFT_RED};
  static Button btnReboot = {165, 200, 150, 35, "Reboot", TFT_ORANGE};
#else
  // Original Touchdown layout (right-side buttons)
  // ... existing code ...
#endif
```

### Touch Features to Test

1. **Single Tap** - Switch clock modes (Morphing ↔ Tetris)
2. **Long Press (3s)** - Show info pages
3. **Navigation Buttons** - < > X buttons for page control
4. **Action Buttons** - Flip Display, Reset WiFi, Reboot
5. **Rotation Awareness** - Touch works correctly when display flipped

---

## 📐 Display Rendering Changes

### LED Pitch Calculation (Auto-Adjusted)

**Current Code (works for both):**
```cpp
static int computeRenderPitch() {
  int matrixAreaH = tft.height() - STATUS_BAR_H;
  int maxPitch = min(tft.width() / LED_MATRIX_W, matrixAreaH / LED_MATRIX_H);
  return maxPitch < 1 ? 1 : maxPitch;
}
```

**For Touchdown (480×320):**
```
fbPitch = min(480/64, (320-70)/32)
        = min(7, 250/32)
        = min(7, 7.8)
        = 7
```

**For CYD (320×240):**
```
fbPitch = min(320/64, (240-40)/32)
        = min(5, 200/32)
        = min(5, 6.25)
        = 5
```

**Good News:** This calculation is **already in the code** and will auto-adjust! ✓

---

## 🔌 I2C Device Addresses

**Verify these on CYD hardware:**

| Device | Default Address | Alternative | Notes |
|--------|-----------------|-------------|-------|
| Touch Controller | 0x38 | 0x5D, 0x14 | Check CYD schematic |
| HTU21D (Temp/Humidity) | 0x40 | - | Adafruit standard |
| BME280 (Sensor) | 0x77 | 0x76 | Adafruit standard |
| SHT31 (Sensor) | 0x44 | 0x45 | Adafruit standard |

**Action:** Scan I2C bus on CYD to verify addresses
```cpp
// Add temporary I2C scan in setup()
void scanI2C() {
  Wire.begin(21, 22);  // SDA, SCL
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("Found device at 0x%02X\n", addr);
    }
  }
}
```

---

## 📊 Memory Impact Analysis

### Sprite Buffer Sizes

**Touchdown (480×320):**
```cpp
int sprW = 64 * 7 = 448 pixels
int sprH = 32 * 7 = 224 pixels
Size = 448 * 224 * 2 bytes = 200,704 bytes (~196 KB)
```

**CYD (320×240):**
```cpp
int sprW = 64 * 5 = 320 pixels
int sprH = 32 * 5 = 160 pixels
Size = 320 * 160 * 2 bytes = 102,400 bytes (~100 KB)
```

**Result:** CYD uses **49% LESS memory** for sprite buffer! ✓

### Heap Allocation Summary
```cpp
// Global allocations (both devices)
fb[32][64]        = 4,096 bytes    (framebuffer)
fbPrev[32][64]    = 4,096 bytes    (prev frame)
AppConfig         = ~200 bytes
Digit bitmaps     = ~2,000 bytes
Timezone data     = ~3,000 bytes (read-only)

// Dynamic allocation (TFT_eSprite)
Touchdown: ~200 KB
CYD:       ~100 KB   ← Better!
```

---

## 🎨 Web UI Responsive Design

### HTML Adjustments (data/index.html)

**Current viewport settings:**
```html
<meta name="viewport" content="width=device-width,initial-scale=1" />
```

**No changes needed** - Already responsive!

### CSS Adjustments (data/style.css)

**Check these media queries:**
```css
/* Add CYD-specific responsive layout */
@media (max-width: 480px) {
  /* Stack layout vertically for mobile */
  .grid { grid-template-columns: 1fr; }
  
  /* Reduce canvas size for display mirror */
  canvas { max-width: 100%; height: auto; }
  
  /* Adjust font sizes */
  h1 { font-size: 18px; }
  h2 { font-size: 16px; }
}
```

### JavaScript Frontend (data/app.js)

**Display mirror canvas upscaling:**
```javascript
// Current code automatically upscales 64×32 to canvas size
// No changes needed - responsive by default ✓

// Example: 64×32 framebuffer displayed as 480×320 on Touchdown
//          64×32 framebuffer displayed as 320×240 on CYD
```

---

## 🧪 Testing Checklist

### Phase 1: Hardware Setup
- [ ] Flash firmware to CYD
- [ ] Serial monitor shows initialization steps
- [ ] Startup display appears on TFT
- [ ] Display orientation is correct
- [ ] No garbled text or colors

### Phase 2: LED Matrix Rendering
- [ ] 7-segment digits render correctly
- [ ] LED size/gap looks reasonable at new scale
- [ ] Color rendering is accurate
- [ ] No flicker or glitches
- [ ] Brightness control works

### Phase 3: Animations
- [ ] 7-segment morphing transitions smoothly
- [ ] Tetris falling blocks animate correctly
- [ ] No stuttering or lag
- [ ] Animation speed is appropriate
- [ ] Mode switching transitions work

### Phase 4: WiFi & Web Interface
- [ ] WiFiManager captive portal works
- [ ] Web interface loads correctly
- [ ] Display mirror shows framebuffer
- [ ] All configuration options responsive
- [ ] Changes apply instantly

### Phase 5: Time & Sensors
- [ ] NTP time synchronization works
- [ ] Timezone changes take effect
- [ ] Sensor data displays (if available)
- [ ] Temperature/humidity updates
- [ ] Date formatting options work

### Phase 6: Touch Interface (New in v2.1.0)
- [ ] Touch controller detected at I2C address
- [ ] Single tap switches clock modes
- [ ] Long press (3s) shows info pages
- [ ] Navigation buttons (< > X) respond correctly
- [ ] Action buttons work (Flip, Reset WiFi, Reboot)
- [ ] Touch coordinates accurate (no offset issues)
- [ ] Touch works after display flip
- [ ] Text displays correctly (not truncated)
- [ ] Info pages navigate smoothly
- [ ] Exit button returns to clock display

### Phase 7: Performance
- [ ] No memory leaks (monitor heap)
- [ ] CPU frequency stable
- [ ] Stable frame rate (monitor via debug output)
- [ ] OTA updates work
- [ ] Extended runtime (24+ hours) stable

---

## 🐛 Troubleshooting Guide

### Issue: Display Not Initializing
**Solution:**
1. Check User_Setup.h display driver matches hardware
2. Verify GPIO pins (MOSI, SCLK, CS, DC, RST)
3. Enable serial debugging: set `debugLevel = 4`
4. Check SPI frequency isn't too high for CYD
5. Verify RST pin pulse during init

### Issue: Display Upside Down or Rotated
**Solution:**
1. Try adjusting rotation: `tft.setRotation(0)` to `tft.setRotation(3)`
2. Check CYD specifications for default orientation
3. Use web UI "Flip Display" button to save preference

### Issue: Sprite Creation Fails
**Solution:**
1. Check available heap memory (should be >150KB)
2. Reduce other allocations if needed
3. Fall back to direct rendering (slower but works)
4. Monitor for memory leaks during runtime

### Issue: I2C Devices Not Detected
**Solution:**
1. Run I2C bus scan (see code above)
2. Verify I2C addresses match device datasheets
3. Check SCL/SDA pin assignments
4. Test with simple I2C scanner sketch
5. Check pull-up resistors (should be present on CYD)

### Issue: WiFi Not Connecting
**Solution:**
1. Check WiFi AP name in config: `"Touchdown-RetroClock-Setup"`
2. Update to CYD-specific name
3. Verify WiFi credentials via reset
4. Check WiFi signal strength
5. Try static IP if DHCP fails

### Issue: OTA Updates Failing
**Solution:**
1. Verify OTA hostname matches: `"Touchdown-RetroClock"`
2. Update to CYD-specific name if needed
3. Ensure device is on same network as upload PC
4. Check password in config.h (default: `"change-me"`)
5. Monitor via serial for OTA progress details

---

## 📝 Step-by-Step Porting Procedure

### Step 1: Prepare CYD Environment
```bash
# Clone this repo to CYD project folder
cp -r ESP32_Touchdown_Retro_Clock/ CYD_Retro_Clock/
cd CYD_Retro_Clock/

# Create new PlatformIO environment
# Update platformio.ini with CYD board settings
```

### Step 2: Update Hardware Configuration
```cpp
// File: include/User_Setup.h
// 1. Change display driver from ILI9488 to ST7789 (or actual CYD driver)
// 2. Update all GPIO pins from Touchdown to CYD
// 3. Verify SPI speed compatibility
// 4. Save and test

// File: include/config.h
// 1. Update DEFAULT_LED_DIAMETER from 7 to 5
// 2. Update STATUS_BAR_H from 70 to 40
// 3. Save configuration
```

### Step 3: Verify Display Initialization
```cpp
// Build and flash firmware
// Monitor serial output for:
// ✓ TFT size (w x h): 320 x 240
// ✓ Sprite OK: 320x160
// ✓ Display ready

// On TFT screen:
// ✓ Startup messages appear
// ✓ Colors render correctly
// ✓ Text is readable
```

### Step 4: Test Clock Display
```cpp
// Wait for WiFi connection
// Display should show:
// ✓ Large LED-style digits (7-segment)
// ✓ Proper scaling at 5px per LED
// ✓ Status bar below (40px)
// ✓ Date and timezone info

// Time should update every second
// Morphing animation should be smooth
```

### Step 5: Test Tetris Mode
```cpp
// Via web UI, switch to Tetris mode
// Should see:
// ✓ Falling blocks forming digits
// ✓ Colorful Tetris pieces
// ✓ Smooth animation
// ✓ Mode transition fade effect

// Verify block animation speed is appropriate
// (default 1800ms - may need adjustment for personal taste)
```

### Step 6: Test Web Interface
```
1. Connect to WiFi from CYD
2. Find IP address on startup display
3. Open browser to http://[IP]:80
4. Verify:
   ✓ Web interface loads
   ✓ Display mirror shows framebuffer
   ✓ Timezone dropdown works
   ✓ LED color picker works
   ✓ All settings apply instantly
   ✓ Configuration persists on reboot
```

### Step 7: Final Testing
```bash
# Monitor serial for 24+ hours
# Watch for:
# - Memory leaks (heap should stabilize)
# - WiFi reconnection if dropped
# - NTP sync periodically
# - Sensor updates every 60s
# - Clean shutdown and restart
```

---

## 🔧 Configuration File: CYD-Specific Defaults

**Recommended defaults for CYD (in config.h):**

```cpp
// ===== LED MATRIX PANEL EMULATION =====
#ifndef LED_MATRIX_W
#define LED_MATRIX_W 64              // Keep same virtual grid
#endif
#ifndef LED_MATRIX_H
#define LED_MATRIX_H 32
#endif

// For CYD 320×240 display (fbPitch will be 5)
#define DEFAULT_LED_DIAMETER 5       // Reduced from 7 for Touchdown
#define DEFAULT_LED_GAP      0       // No gap for tighter packing

// Status bar for CYD (reduced from 70px)
#define STATUS_BAR_H 40              // 40px leaves good margin

// ===== CLOCK MODES =====
#define DEFAULT_CLOCK_MODE CLOCK_MODE_7SEG
#define DEFAULT_AUTO_ROTATE false
#define DEFAULT_ROTATE_INTERVAL 5    // Minutes

// ===== TETRIS =====
#define TETRIS_ANIMATION_SPEED 1800  // 1.8s per frame (adjust to taste)

// ===== DEFAULTS =====
#define DEFAULT_LED_COLOR_565 0xF800 // Red
#define DEFAULT_TZ "UTC"             // Change to your region
#define DEFAULT_NTP "pool.ntp.org"
#define DEFAULT_24H true             // Preference
```

---

## 📚 Resources & References

### CYD ESP32 Information
- **Product Page:** https://www.aliexpress.com/ (search "CYD ESP32")
- **GitHub:** Various CYD repositories (search for schematics)
- **Pinout:** Verify from CYD documentation or pinout diagram

### TFT Display Drivers
- **ST7789:** Common on CYD 240×320 displays
- **ILI9341:** Alternative CYD driver
- **TFT_eSPI Supported:** Check docs/User_Setups for examples

### Library Documentation
- **TFT_eSPI:** https://github.com/Bodmer/TFT_eSPI
- **WiFiManager:** https://github.com/tzapu/WiFiManager
- **ArduinoJson:** https://arduinojson.org/
- **TetrisAnimation:** https://github.com/toblum/TetrisAnimation

### Debugging Tools
- **Serial Monitor:** 115200 baud for debug output
- **I2C Scanner:** Identify sensor/touch addresses
- **PlatformIO Monitor:** Real-time serial + upload

---

## ✅ Porting Completion Checklist

### Hardware
- [ ] CYD board identified and documented
- [ ] Pinout mapped to Touchdown equivalents
- [ ] Display driver identified (ST7789, ILI9341, etc.)
- [ ] SPI pins verified
- [ ] I2C devices confirmed
- [ ] Touch controller addressed

### Code Updates
- [ ] User_Setup.h updated for CYD
- [ ] config.h constants adjusted (LED size, status bar height)
- [ ] platformio.ini configured for CYD board
- [ ] Build compiles without errors
- [ ] Firmware size acceptable

### Functional Testing
- [ ] Display initializes correctly
- [ ] Clock displays with proper scaling
- [ ] 7-segment morphing works smoothly
- [ ] Tetris blocks animate properly
- [ ] WiFi connects to network
- [ ] Web UI responsive and functional
- [ ] Sensors detected (if present)
- [ ] NTP time synchronization works
- [ ] OTA firmware updates work

### Performance & Stability
- [ ] No memory leaks (24hr+ runtime)
- [ ] Frame rate stable (monitor via debug)
- [ ] Web interface responsive
- [ ] Configuration persists across reboot
- [ ] Graceful degradation if features unavailable

### Documentation
- [ ] CYD-specific README created
- [ ] Pin configuration documented
- [ ] Any modifications noted
- [ ] Known limitations listed
- [ ] Setup instructions clear

---

## 🎉 Success Criteria

Your CYD port is successful when:

1. ✓ Firmware compiles without errors
2. ✓ Startup sequence completes on TFT
3. ✓ Time displays as large LED digits
4. ✓ Digits morph smoothly when seconds change
5. ✓ Tetris blocks fall and form digits
6. ✓ Web interface accessible via browser
7. ✓ All settings can be adjusted via web UI
8. ✓ WiFi reconnects automatically if needed
9. ✓ Temperature/humidity display (if sensor present)
10. ✓ OTA firmware updates work

---

**Good luck with your CYD port!** 🚀

The codebase is clean, modular, and well-suited for porting. Focus on getting the hardware configuration right (User_Setup.h + GPIO pins), and the rest should "just work."

If you encounter issues, leverage the **comprehensive debug logging system** - set `debugLevel = 4` for detailed output that will help identify problems quickly.

---

*Last Updated: January 11, 2026*
