# Migration from CYD (v1.x) to ESP32 Touchdown (v2.0.0)

This document summarizes the refactoring from the Cheap Yellow Display (CYD/ESP32-2432S028) to the ESP32 Touchdown with a larger display.

## Quick Reference

### Firmware Version
- Old: `1.2.0` (CYD)
- New: `2.0.0` (Touchdown)

### Hardware Changes

| Feature | CYD (v1.x) | Touchdown (v2.0) |
|---------|-----------|------------------|
| **Board** | ESP32-2432S028 | ESP32 Touchdown |
| **Display Controller** | ILI9341 | ILI9488 |
| **Resolution** | 320×240 | 480×320 |
| **SPI Mode** | 5-wire (with MISO) | 4-wire (no MISO) |
| **Backlight GPIO** | 21 | 32 |
| **Touch** | Resistive (XPT2046) | Capacitive (FT62x6) |
| **Battery Support** | None | LiPo with charging |
| **Status LED** | Built-in RGB | None (use GPIO breakout) |
| **Reset Button** | BOOT (GPIO0) | None (web/serial) |
| **USB Connector** | Micro-USB | USB-C |

### Display & Rendering Changes

| Aspect | CYD (v1.x) | Touchdown (v2.0) |
|--------|-----------|------------------|
| **LED Pitch** | 5 px per LED | 7.5 px per LED |
| **Default Diameter** | 5 pixels | 7 pixels |
| **Status Bar Height** | 50 pixels | 70 pixels |
| **LED Matrix** | 64×32 (unchanged) | 64×32 (unchanged) |

### Build Configuration Changes

```bash
# Old (CYD)
platformio run -e cyd_esp32_2432s028
platformio run -e cyd_esp32_2432s028 --target uploadfs

# New (Touchdown)
platformio run -e esp32_touchdown
platformio run -e esp32_touchdown --target uploadfs
```

### Pin Configuration Changes

#### SPI Display Pins
```cpp
// CYD (5-wire SPI)
TFT_MISO = 12    →    TFT_MISO = -1  (4-wire, no MISO)
TFT_MOSI = 13    →    TFT_MOSI = 23
TFT_SCLK = 14    →    TFT_SCLK = 18
TFT_CS   = 15    →    TFT_CS   = 15  (unchanged)
TFT_DC   = 2     →    TFT_DC   = 2   (unchanged)
TFT_RST  = -1    →    TFT_RST  = 4
TFT_BL   = 21    →    TFT_BL   = 32
```

#### Sensor/Touch I2C Pins
```cpp
// CYD
SENSOR_SDA = 27  →    SENSOR_SDA = 21  (shared with touch)
SENSOR_SCL = 22  →    SENSOR_SCL = 22  (unchanged)
```

#### Removed (Not Available on Touchdown)
```cpp
// CYD RGB LED (not available on Touchdown)
LED_R_PIN = 4       // Removed
LED_G_PIN = 16      // Removed
LED_B_PIN = 17      // Removed

// CYD BOOT Button (not available on Touchdown)
BOOT_BTN_PIN = 0    // Removed
```

#### Available on Touchdown (New)
```cpp
// Passive Buzzer
GPIO26  // For future alert sounds

// Battery Voltage Monitor
GPIO35  // ADC input for battery level

// Touch Controller IRQ
GPIO27  // Interrupt from FT62x6

// 12 Additional GPIO Breakout Pins
GPIO12, GPIO13, GPIO14, GPIO16, GPIO17, GPIO25
GPIO33, GPIO34 (INPUT only)
```

### Web UI Changes

#### Canvas Dimensions (data/app.js)
```javascript
// CYD (v1.x)
const TFT_W = 320;
const TFT_H = 240;
const STATUS_BAR_H = 50;

// Touchdown (v2.0)
const TFT_W = 480;
const TFT_H = 320;
const STATUS_BAR_H = 70;
```

### OTA Hostname
```
CYD-RetroClock  →  Touchdown-RetroClock
```

### WiFi AP Name
```
CYD-RetroClock-Setup  →  Touchdown-RetroClock-Setup
```

## Files Changed Summary

1. **include/config.h** - Hardware constants, pins, version
2. **include/User_Setup.h** - TFT_eSPI display driver and pin configuration
3. **platformio.ini** - Build environment name change
4. **src/main.cpp** - Header comments and feature descriptions
5. **data/app.js** - Canvas dimensions and status bar height
6. **data/index.html** - Title and header text
7. **README.md** - Hardware specs, purchase links, pin configuration
8. **.github/copilot-instructions.md** - AI agent documentation
9. **CHANGELOG.md** - Version history with migration guide

## All Features Preserved

✅ 64×32 virtual RGB LED Matrix emulation  
✅ 7-segment clock digit rendering  
✅ Morphing animations  
✅ WiFi & NTP time sync  
✅ 88 timezone support  
✅ 9 NTP server presets  
✅ Web-based configuration  
✅ Live display mirror  
✅ System diagnostics  
✅ Debug level control  
✅ OTA firmware updates  
✅ LittleFS web file serving  
✅ Persistent configuration storage  

## New Features Available

✨ Capacitive touch display (FT62x6)  
✨ Battery management (LiPo charging)  
✨ Battery voltage monitoring  
✨ Passive buzzer support  
✨ microSD card slot  
✨ 12 GPIO breakout pins  
✨ Larger display (50% more pixels)  

## Testing Checklist

- [ ] Compile firmware without errors
- [ ] Upload to Touchdown via USB-C
- [ ] Display renders at 480×320
- [ ] LED matrix shows correct pitch (7.5px)
- [ ] Status bar displays in 70px area
- [ ] Web UI loads and mirrors display
- [ ] WiFi AP "Touchdown-RetroClock-Setup" appears
- [ ] Configuration persists
- [ ] OTA hostname is "Touchdown-RetroClock"
- [ ] All web API endpoints respond correctly

## Troubleshooting

### Firmware won't compile
- Ensure TFT_eSPI is installed via PlatformIO
- Check that `include/User_Setup.h` is included in build flags

### Display not working
- Verify USB-C cable connection
- Check that all SPI pins match Touchdown pinout
- Confirm ILI9488 driver is selected (not ILI9341)

### Web UI not displaying
- Verify LittleFS files were uploaded (`uploadfs` target)
- Check that canvas dimensions are 480×320
- Confirm status bar height is 70 pixels

### WiFi issues
- Verify WiFiManager is properly initialized
- Check that GPIO connections are correct
- Try web UI WiFi reset option

## Questions?

Refer to:
- [README.md](README.md) - Hardware and setup details
- [CHANGELOG.md](CHANGELOG.md) - Version history and breaking changes
- [.github/copilot-instructions.md](.github/copilot-instructions.md) - Architecture and patterns
- [ESP32 Touchdown GitHub](https://github.com/DustinWatts/esp32-touchdown) - Official docs

---
**Refactoring Date**: 2026-01-10  
**Status**: ✅ Ready for testing
