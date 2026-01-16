# Changelog

All notable changes to the ESP32 Touchdown LED Matrix Retro Clock project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- **Firmware version display on startup**: Version number now displayed on TFT screen during boot sequence (appears after build date/time)
- **Serial debug version output**: Firmware version added to serial console debug header for easier troubleshooting

### Documentation
- Updated README.md version badge from 2.5.0 to 2.6.0 (2026-01-16)
- Corrected colon brightness documentation (50% → 75% for Morphing Remix mode)
- Fixed API `/api/mirror` endpoint specification (2048 bytes → 4096 bytes, RGB565 format)
- Added startup splash screen feature to README display features
- Added independent sensor/date color customization to Morphing Remix features
- Removed outdated "NEW in v2.5.0" label from Morphing Remix mode section
- Updated CLAUDE.md to mark README version update task as completed

## [2.6.0] - 2026-01-15

### Added - Morphing (Remix) Mode Customization
- **Independent color settings for Morphing (Remix) mode**
  - Sensor data color picker in WebUI (default: yellow #FFFF00)
  - Date display color picker in WebUI (default: yellow #FFFF00)
  - Colors stored in NVS and persist across reboots
  - Real-time color updates with instant preview
- **WebUI settings reorganization**
  - Settings now grouped by clock mode for clarity
  - "LED Appearance (All Modes)" section for shared settings
  - "Morphing (Classic) & Tetris Settings" section (LED diameter, gap, morph speed)
  - "Morphing (Remix) Settings" section (show sensor, show date, colors)
  - Mode-specific sections show/hide based on selected clock mode

### Fixed
- **Date truncation in "Mon DD, YYYY" format**
  - Increased currDate buffer from 11 to 14 characters
  - "Jan 15, 2026" now displays fully instead of truncating to "Jan 15  20"

### Removed
- **Fade transition on mode switch**
  - Display now switches instantly at full brightness
  - Eliminated the dim-then-brighten effect when changing clock modes
  - Cleaner, more responsive mode switching experience

### Technical Details
- New config fields: `morphSensorColor`, `morphDateColor` (uint32_t RGB888)
- NVS keys: `mSensCol`, `mDateCol` for color persistence
- API endpoints updated: GET /api/state and POST /api/config support new color fields
- RGB888 to RGB565 conversion using existing `rgb888_to_565()` function
- Removed: `fadeLevel`, `inTransition` variables and `applyFade()` function

## [2.5.0] - 2026-01-14

### Added - Morphing (Remix) Mode Polish
- **Unified color system** for Morphing (Remix) mode
  - All clock digits (HH:MM:SS) now use the user-configured LED color from WebUI
  - Colons display at 50% brightness of the LED color for subtle visual separation
  - Removed rainbow per-digit colors in favor of consistent theming
- **Configurable pixel pitch** for Morphing (Remix) mode display scaling
  - `MORPH_PITCH_X` and `MORPH_PITCH_Y` in config.h control 64×32 LED matrix scaling to 480×320 TFT
  - Independent horizontal and vertical scaling for optimal screen usage
  - WebUI mirror automatically matches TFT display with synchronized pitch values

### Fixed
- **Display layout optimization** for Morphing (Remix) mode
  - Sensor data now centered horizontally at top (was left-aligned with clipping)
  - Clock digits moved up by 1 LED pixel for better balance
  - Date display moved to y=27 to fit within 32-row framebuffer (was clipping at y=28)
  - Proper vertical spacing: Sensor (y=0-4), Gap (y=5), Clock (y=6-24), Gap (y=25-26), Date (y=27-31)
- **Digit spacing improvements**
  - Reduced colon gaps from 3 to 1 LED pixel for tighter, more readable spacing
  - Clock shifted left by 1 LED pixel for better horizontal centering
  - 2-row gap between clock and date for visual separation
- **Segment rendering**
  - Shrunk digit height from 21 to 18 rows to eliminate overlap
  - Updated segment coordinates for compact 7x18 pixel digit slots
  - Colon position adjusted to align with shorter digits

### Changed
- **Morphing (Remix) mode color rendering**
  - `renderMorphingDigit()` now accepts color parameter instead of using per-digit colors
  - RGB565 color dimming algorithm for colons (50% brightness)
- **Sensor data formatting**
  - BMP280/BMP180: Shows "28C 1007HPA" (temperature + pressure with units)
  - BME280: Shows "28C 65% 1007HPA" (temperature + humidity % + pressure)
  - SHT3X/HTU21D: Shows "28C 65%" (temperature + humidity %)
  - Proper percent symbol rendering using `%%` in format string
- **Display pitch configuration**
  - Default MORPH_PITCH_X = 8 (horizontal: 64×8 = 512px width)
  - Default MORPH_PITCH_Y = 9 (vertical: 32×9 = 288px height)
  - Critical synchronization required between config.h and app.js for WebUI mirror

### Technical Details
- Morphing (Remix) mode digit layout: startX=5, startY=6, digitWidth=7, colonGap=1
- Segment coordinates updated in MorphingDigit.h for 18-row tall digits
- Colon rendering uses 2×2 LED dots at 50% brightness
- Date and sensor text centered using `getTextWidth3x5()` calculation
- RGB565 color dimming: Extract components, divide by 2, recombine

## [2.1.0] - 2026-01-12

### Added - Touch Interface Implementation
- **Full capacitive touch support** with FT6236/FT6206 controller
- **Touch-based info pages** accessible via 3-second long press
  - User Settings page: View all configurable settings
  - System Diagnostics page: View network, hardware, and system resources
- **Interactive touch buttons** on info pages
  - Navigation buttons (< > X) in top-right corner for page control
  - Flip Display button on User Settings page
  - Reset WiFi and Reboot buttons on System Diagnostics page
- **Touch mode switching**: Single tap switches between clock display modes (Morphing ↔ Tetris)
- **Rotation-aware touch mapping**: Automatically adjusts touch coordinates when display is flipped
- **Touch calibration support**: Config structure includes touchOffsetX/Y for fine-tuning (infrastructure ready)
- **Text clipping system**: `drawClippedString()` function prevents text overflow with ellipsis truncation

### Fixed
- **Text truncation on info pages**: Text datum was not reset after drawing buttons, causing center-aligned text instead of left-aligned
- **Touch coordinate capture**: Now captures coordinates while finger is down (was returning 0,0 when read after release)
- **Touch coordinate mapping**: Corrected from 0-240 to 0-480 range for proper 1:1 FT6206 portrait mode mapping
- **Display flip touch support**: Touch coordinates now properly invert when display is rotated 180°
- **Delta rendering sync**: fbPrev buffer reset when display is flipped to force full redraw
- **Status bar disappearing**: Status bar cache now resets when display is cleared

### Changed
- Touch debounce time: 300ms to prevent multiple triggers
- Long press threshold: 3 seconds to activate info pages
- Info page layout: Content area (0-320px) separate from button area (320-480px)
- Debug output: Enhanced with touch coordinates, rotation state, and calibration offsets

### Technical Details
- Touch controller initialized on I2C (GPIO21/22) shared with sensors
- Touch coordinates: FT6206 reports portrait mode (0-320 × 0-480)
- Mapping for rotation 1 (USB right): touchX = point.y, touchY = 319 - point.x
- Mapping for rotation 3 (flipped): touchX = 479 - point.y, touchY = point.x
- Calibration offsets applied after mapping, constrained to screen bounds

## [2.0.0] - 2026-01-10

### Major Breaking Changes (Hardware Migration)
⚠️ **This is a major version bump for a complete hardware platform change**

### Changed (Breaking)
- **Hardware platform**: ESP32-2432S028 (CYD) → ESP32 Touchdown
- **Display resolution**: 320×240 ILI9341 → 480×320 ILI9488
- **SPI interface**: 5-wire SPI (with MISO) → 4-wire SPI (no MISO)
- **Build environment**: `cyd_esp32_2432s028` → `esp32_touchdown`
- **All GPIO pins**: Complete refactoring for Touchdown board layout
  - See [User_Setup.h](include/User_Setup.h) for complete new pin mapping
  - See [config.h](include/config.h) for sensor/button/LED pins
- **Status bar height**: 50 pixels → 70 pixels
- **Default LED diameter**: 5 pixels → 7 pixels (larger screen allows bigger LEDs)
- **LED pitch calculation**: 320÷64=5 → 480÷64=7.5 pixels per LED
- **OTA hostname**: `CYD-RetroClock` → `Touchdown-RetroClock`
- **WiFi AP name**: `CYD-RetroClock-Setup` → `Touchdown-RetroClock-Setup`

### Added (New Features)
- **Capacitive touch display** (FT62x6 controller on I2C) - prepared for future UI implementation
- **Battery management** support (MCP73831/SD8016 charging IC)
- **Battery voltage monitoring** (GPIO35 ADC input)
- **Passive buzzer support** (GPIO26) for future alert sounds
- **microSD card slot** available for data logging
- **12 additional GPIO breakout pins** for expansion
- **USB-C connector** for power and programming
- **Higher resolution display** (480×320) for crisper rendering
- **Stemma/JST-PH I2C connector** for easy sensor connection

### Removed
- **RGB LED status indicators** (CYD built-in feature not available on Touchdown)
- **BOOT button** for WiFi reset (use web UI or serial instead)

### Fixed
- Display rendering optimized for larger screen
- LED matrix pitch calculations corrected for 480×320 resolution

### Migration Guide (from v1.x)
If upgrading from CYD version:
1. **Hardware**: Replace ESP32-2432S028 (CYD) with ESP32 Touchdown
2. **Pins**: Complete pin reconfiguration required - see [User_Setup.h](include/User_Setup.h)
3. **Build environment**: Update PlatformIO to use `esp32_touchdown` environment
4. **Firmware upload**: Standard USB-C upload process
5. **Filesystem**: Run `uploadfs` target to update web UI files to LittleFS
6. **Config reset**: Settings from v1.x may need manual reconfiguration due to pin changes
7. **Web UI**: No breaking changes - all existing configurations apply to new hardware

## [1.0.0] - 2026-01-07

### Added
- Initial release of CYD RGB LED Matrix (HUB75) Retro Clock
- 64×32 virtual RGB LED Matrix (HUB75) emulation on 320×240 TFT display
- Large 7-segment style clock digits with spacing for improved readability
- Morphing animations for smooth digit transitions
- WiFi configuration via WiFiManager with AP mode fallback
- NTP time synchronization with IANA timezone support (88 timezones across 13 regions)
- Web-based configuration interface at device IP address
- Live display mirror in web interface showing real-time framebuffer
- Adjustable LED appearance settings:
  - LED diameter (1-10 pixels)
  - LED gap spacing (0-8 pixels)
  - LED color (RGB color picker with instant preview)
  - Backlight brightness (0-255)
- Status bar on TFT display showing:
  - WiFi network name and IP address
  - Current date in selected format
  - Timezone name
- Comprehensive system diagnostics panel in web interface:
  - Time & Network section (current time, date, WiFi status, IP address)
  - Hardware section (board type, display model, sensor status, firmware version, OTA status)
  - System Resources section (uptime, free heap, heap usage percentage, CPU frequency)
  - Debug Settings section (runtime-adjustable debug level with 5 levels)
- Enhanced serial logging with before/after change tracking for all configuration changes
- Timezone dropdown organized by 13 geographic regions with 88 timezones
- NTP server dropdown selector with 9 preset servers:
  - Global Pool (pool.ntp.org)
  - Google (time.google.com)
  - Cloudflare (time.cloudflare.com)
  - Apple (time.apple.com)
  - Microsoft (time.windows.com)
  - Regional pools (Australia, USA, Europe, Asia)
- Date format selection with 5 formats:
  - YYYY-MM-DD (ISO 8601)
  - DD/MM/YYYY (European)
  - MM/DD/YYYY (US)
  - DD.MM.YYYY (German)
  - Mon DD, YYYY (Verbose)
- Debug level selector with runtime control (Off, Error, Warning, Info, Verbose)
- Firmware version display in diagnostic panel (single source from config.h)
- Contact footer with GitHub repository and Bluesky profile links
- Instant auto-apply for all configuration changes (no save button required)
- Display flip/rotation toggle button for 180° orientation (allows mounting in either direction)
- OTA (Over-The-Air) firmware updates via ArduinoOTA
- LittleFS filesystem for serving web UI files
- Persistent configuration storage using Preferences (including debug level)
- Human-readable formatting for uptime (days/hours/minutes) and memory (KB/MB)

### Display Layout
- Top section: Large RGB LED Matrix (HUB75) clock (HH:MM:SS format)
- Bottom section: 50-pixel status bar with system information
- Total display area: 320×240 pixels in landscape orientation
- Emulates physical 64×32 RGB LED Matrix Panel (HUB75 protocol)

### Hardware Support
- ESP32-2432S028 (CYD - Cheap Yellow Display)
- ILI9341 TFT display controller
- Built-in backlight control (GPIO 21)
- SPI interface for display communication

### Web API
- `GET /` - Main web interface (index.html)
- `GET /api/state` - System state JSON (time, date, WiFi, config, diagnostics)
  - Includes: uptime, freeHeap, heapSize, cpuFreq, debugLevel, firmware version
  - Hardware info: board type, display model, sensor status, OTA status
- `GET /api/timezones` - List of 88 timezones grouped by 13 geographic regions
- `POST /api/config` - Update configuration settings with detailed logging
  - Logs before/after values for all changed fields
  - Includes client IP address in logs
- `GET /api/mirror` - Raw framebuffer data (2048 bytes for 64×32 matrix)

### Configuration
- Default timezone: Australia/Sydney
- Default NTP server: pool.ntp.org
- Default LED diameter: 5 pixels
- Default LED gap: 0 pixels (no gap)
- Default LED color: Red (0xFF0000)
- Default brightness: 255 (maximum)
- Status bar height: 50 pixels
- Frame rate: ~30 FPS (33ms per frame)
- Morph animation: 20 steps

### Technical Details
- RGB LED Matrix (HUB75) pitch calculation: min(320/64, 190/32) = 5 pixels per LED
- Framebuffer: 8-bit intensity values (0-255) per pixel
- Sprite-based rendering for flicker-free display updates
- Row-major framebuffer layout: fb[y][x]
- Digit width: 9 pixels (reduced to fit HH:MM:SS with gaps)
- Digit spacing: 1 pixel gap between digits
- Total clock width: 63 pixels (fits centered in 64-pixel width)

### Fixed
- Color picker now properly allows color selection without value being overwritten by auto-refresh
- Timezone dropdown properly displays all 88 timezones organized by geographic region
- Web interface automatically applies changes without requiring manual save button click
- Serial logging now consistently uses [INFO] level for all configuration changes
- All field type checks in API now use consistent `.isNull()` pattern for reliability

### Known Issues
- None at initial release

### Security Notes
- Default OTA password is "change-me" - **CHANGE THIS BEFORE DEPLOYMENT**
- WiFi credentials stored in ESP32 NVS (Non-Volatile Storage)
- No authentication on web interface - suitable for trusted networks only

## [1.1.0] - 2026-01-08

### Added
- **WiFi Reset via BOOT Button**: Hold BOOT button (GPIO 0) for 3 seconds during power-up to reset WiFi credentials
  - Yellow LED indicates button detection
  - Red LED confirms reset after 3-second hold
  - Device automatically restarts in AP mode for reconfiguration
- **Web Interface WiFi Reset**: New `/api/reset-wifi` endpoint for remote WiFi credential reset
  - Returns JSON status message
  - Automatically restarts device in AP mode after reset
- **RGB LED Status Indicators**: Comprehensive visual feedback system using CYD's built-in RGB LED
  - Blue: Device starting up / Connecting to WiFi
  - Green flash: WiFi connected / Sensor detected / NTP configured
  - Yellow: BOOT button detected during startup
  - Yellow flash: No sensor detected (normal operation)
  - Red: WiFi reset confirmed / WiFi connection failed
  - Purple: WiFi config portal active (AP mode)
  - LED turns off when device is fully operational
- **WiFiManager Callback Integration**: Config portal mode triggers purple LED indicator
- **RGB LED Helper Functions**: `setRGBLed()` and `flashRGBLed()` for easy status control

### Changed
- **Display Mirror in Web UI**: Now shows temperature and humidity instead of IP address
  - Exactly matches the physical TFT display layout
  - Includes temperature unit conversion (Celsius/Fahrenheit)
  - Shows "Sensor: Not detected" when no sensor is present
- **Web UI Display Mirror Flip Removed**: Display mirror always shows upright orientation in browser
  - Physical device flip setting no longer affects web UI mirror orientation
  - Improves user experience for web-based monitoring
- **Default Display Orientation**: Rotation defaults to IO ports at top, USB on left
  - Rotation 1 (normal): IO ports top, USB left
  - Rotation 3 (flipped): IO ports bottom, USB right
  - Flip setting properly stored in configuration

### Fixed
- Display orientation now correctly defaults to IO ports at top, USB on left
- Web UI display mirror no longer flips when device flip setting is toggled

### Documentation
- Updated README.md with WiFi reset procedures (BOOT button and web interface)
- Added RGB LED status indicator reference table to README.md
- Updated API documentation with `/api/reset-wifi` endpoint
- Added pin configuration for RGB LED and BOOT button
- Enhanced troubleshooting section with WiFi reset instructions
- Updated version badge to 1.1.0

### Technical Details
- RGB LEDs are active-LOW (LOW = ON, HIGH = OFF)
- BOOT button is active-LOW (LOW = pressed)
- 3-second hold detection with 100ms polling interval
- WiFi reset clears credentials via WiFiManager.resetSettings()
- Device restart triggered after WiFi reset for immediate AP mode activation

## [1.2.0] - Work In Progress

### Added
- **OTA Progress Visualization**: Visual feedback during Over-The-Air firmware updates
  - Animated progress bar on TFT display showing upload percentage (0-100%)
  - Color-coded progress: Red (0-33%), Yellow (33-66%), Green (66-100%)
  - Real-time percentage display centered on progress bar
  - Cyan RGB LED indicator during OTA update process
  - Success screen with "Update Complete!" message and green LED flash
  - Error handling with detailed error messages on TFT display:
    - Auth Failed, Begin Failed, Connect Failed, Receive Failed, End Failed
  - Red LED indicator for failed updates with 3-second error display
  - Automatic screen clear and return to normal operation after update or error

### Technical Details
- OTA progress callback updates display in real-time during firmware upload
- Progress bar dimensions: 280×40 pixels, centered on 320×240 display
- Uses TFT_eSPI Font 4 for title and percentage text, Font 2 for status message
- Static `firstDraw` flag ensures screen clears only once at start of update
- Error messages use detailed switch-case for all ArduinoOTA error types

## [Unreleased]

### Planned Features
- Additional display modes (date, temperature, custom messages)
- Multiple color schemes and themes
- Alarm functionality
- Touch screen support for direct configuration
- MQTT integration for remote control
- Automatic brightness adjustment based on ambient light

---

**Note**: Version 1.0.0 represents the first stable release with core clock functionality,
web configuration, and RGB LED Matrix (HUB75) emulation working correctly.
