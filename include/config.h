#pragma once

// -----------------------------
// Project: ESP32 Touchdown LED Matrix Retro Clock
// Hardware: ESP32 Touchdown by Dustin Watts
// Board Info: https://github.com/DustinWatts/esp32-touchdown
// Display: ILI9488 480x320 TFT with FT62x6 capacitive touch
// Features: WiFiManager + NTP + WebUI + TFT "LED Matrix" emulation + morphing animations
// -----------------------------

// ===== FIRMWARE VERSION =====
#define FIRMWARE_VERSION "2.6.0"

// ===== LED MATRIX PANEL EMULATION =====
// Logical LED grid (single 64x32 panel)
#ifndef LED_MATRIX_W
#define LED_MATRIX_W 64
#endif

#ifndef LED_MATRIX_H
#define LED_MATRIX_H 32
#endif

// Default "LED" rendered size (in TFT pixels) and spacing between LEDs (pitch simulation)
// Adjust in Web UI later (stored in config).
// Larger screen (480x320) allows for bigger LED diameter
#define DEFAULT_LED_DIAMETER 7     // pixels (max, fills the pitch completely)
#define DEFAULT_LED_GAP      0     // pixels (no gap for maximum fill)

// Reserve space below the matrix for status/info
#define STATUS_BAR_H 70            // pixels (bottom status bar - increased for larger screen)

// Morphing Remix mode (CLOCK_MODE_MORPH = 2) automatically hides status bar
// to use full display height for digits and date display

// Rendering mode: 0 = sprite (flickery during morph), 1 = direct TFT (smooth)
#define DISABLE_SPRITE_RENDERING 1  // Use direct TFT rendering to eliminate morph flashing

// Default LED color (RGB565). Start with red.
#define DEFAULT_LED_COLOR_565 0xF800

// ===== TIME / NTP =====
#define DEFAULT_TZ "Sydney, Australia"    // Timezone name from timezones.h
#define DEFAULT_NTP "pool.ntp.org"
#define DEFAULT_24H true

// ===== CLOCK DISPLAY MODES =====
// Available clock display modes
#define CLOCK_MODE_7SEG    0    // Morphing (Classic) - LED digits with smooth morphing animations
                                // Based on Morphing Clock by Hari Wiguna (HariFun)
                                // https://github.com/hwiguna/HariFun_166_Morphing_Clock
#define CLOCK_MODE_TETRIS  1    // Tetris - Animated falling blocks form time digits
                                // Uses TetrisAnimation by Tobias Blum (toblum)
                                // https://github.com/toblum/TetrisAnimation
#define CLOCK_MODE_MORPH   2    // Morphing (Remix) - Segment-based morphing with bezier curves
                                // Based on MorphingClockRemix by lmirel
                                // https://github.com/lmirel/MorphingClockRemix
// Future modes: CLOCK_MODE_ANALOG, CLOCK_MODE_BINARY, CLOCK_MODE_WORD, etc.

#define DEFAULT_CLOCK_MODE CLOCK_MODE_MORPH  // Default: Morphing (Remix) mode for testing
#define DEFAULT_AUTO_ROTATE false           // Auto-rotate through clock modes
#define DEFAULT_ROTATE_INTERVAL 5           // Minutes between mode changes (when auto-rotate enabled)

// ===== MORPHING (REMIX) MODE PIXEL PITCH CONFIGURATION =====
// These control how the 64×32 LED matrix is scaled to the 480×320 TFT display
// pitchX: Horizontal pixels per LED (480 / 64 = 7.5, use 7-10)
// pitchY: Vertical pixels per LED (320 / 32 = 10, use 8-10)
// Lower values = smaller display with more margin, Higher values = larger display
//
// ⚠️  CRITICAL: When you change these values, you MUST also update the matching
//    values in data/app.js (lines 375-376) for the WebUI mirror to match!
//    Search for "MORPH_PITCH_X" and "MORPH_PITCH_Y" in app.js
//
#define MORPH_PITCH_X 8    // Horizontal pitch (7-10 recommended, 8 for better spacing)
#define MORPH_PITCH_Y 9    // Vertical pitch (8-10 recommended, 9 for room at top/bottom)

// Tetris animation speed (milliseconds between animation frames)
// Lower = faster falling blocks, Higher = slower, more visible animation
// Recommended: 500-2000ms for dramatic slow-motion effect

#define TETRIS_ANIMATION_SPEED 1800         // Default: 1800ms (1.8s) between frames for cinematic slow-motion

// ===== SENSOR CONFIGURATION =====
// Choose your sensor type by uncommenting ONE of the following:
// #define USE_BME280        // BME280: Temperature, Humidity, Pressure sensor (I2C: 0x76 or 0x77)
#define USE_BMP280        // BMP280: Temperature and Pressure sensor (no humidity) (I2C: 0x76 or 0x77)
// #define USE_BMP180        // BMP180: Temperature and Pressure sensor (no humidity) (I2C: 0x77 only)
// #define USE_SHT3X         // SHT3X: Temperature and Humidity sensor (no pressure) (I2C: 0x44 or 0x45)
// #define USE_HTU21D        // HTU21D: Temperature and Humidity sensor (no pressure) (I2C: 0x40) - DEFAULT

// I2C pins for sensor
// Note: GPIO 21/22 are used by touch controller (FT62x6) on Touchdown
// Can share I2C bus with touch or use alternate pins
#define SENSOR_SDA_PIN    21    // I2C Data (shared with touch controller)
#define SENSOR_SCL_PIN    22    // I2C Clock (shared with touch controller)

// Sensor update interval
#define SENSOR_UPDATE_INTERVAL 60000  // Update sensor every 60 seconds

// ===== CAPACITIVE TOUCH CONTROLLER =====
// FT6236/FT6206 Touch Controller (shares I2C bus with sensors)
#define TOUCH_SDA_PIN     21    // I2C Data (same as sensor)
#define TOUCH_SCL_PIN     22    // I2C Clock (same as sensor)
#define TOUCH_IRQ_PIN     27    // Touch interrupt pin
#define TOUCH_I2C_ADDR    0x38  // FT6236/FT6206 I2C address

// Touch settings
#define ENABLE_TOUCH         1     // Enable touch support (1 = enabled, 0 = disabled)
#define TOUCH_DEBOUNCE_MS    300   // Debounce time in milliseconds to prevent multiple triggers
#define TOUCH_LONG_PRESS_MS  3000  // Long press time (3 seconds) to show info pages
#define TOUCH_INFO_PAGES     2     // Number of info pages (0=User Settings, 1=System Diagnostics)

// Temperature unit
#define DEFAULT_TEMP_C true

// ===== RGB LED STATUS INDICATOR =====
// ESP32 Touchdown does not have built-in RGB LED
// Consider using GPIO breakout or external LED if status indication is needed
// Leaving pin definitions commented for future use
// #define LED_R_PIN     12    // Available GPIO on breakout
// #define LED_G_PIN     13    // Available GPIO on breakout
// #define LED_B_PIN     14    // Available GPIO on breakout

// ===== STATUS BUTTON =====
// ESP32 Touchdown does not have dedicated BOOT button on display
// Use USB serial or web interface for WiFi reset
// Leaving pin definitTouchdown for future GPIO interrupt use
// #define STATUS_BTN_PIN   12    // Available GPIO on breakout

// ===== OTA =====
#define OTA_HOSTNAME "Touchdown-RetroClock"
#define OTA_PASSWORD "change-me"   // Change this before flashing for real use.

// ===== WEB =====
#define HTTP_PORT 80

// ===== RENDER =====
#define FRAME_MS 50   // ~20 FPS - reduced from 33ms to minimize flashing (large 480x320 display is slower to update)
#define MORPH_STEPS 20  // number of frames for morphing transitions
