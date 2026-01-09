#pragma once

// -----------------------------
// Project: ESP32 Touchdown LED Matrix Retro Clock
// Hardware: ESP32 Touchdown by Dustin Watts
// Board Info: https://github.com/DustinWatts/esp32-touchdown
// Display: ILI9488 480x320 TFT with FT62x6 capacitive touch
// Features: WiFiManager + NTP + WebUI + TFT "LED Matrix" emulation + morphing animations
// -----------------------------

// ===== FIRMWARE VERSION =====
#define FIRMWARE_VERSION "2.0.0"

// ===== LED MATRIX EMULATION =====
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

// Rendering mode: 0 = sprite (flickery during morph), 1 = direct TFT (smooth)
#define DISABLE_SPRITE_RENDERING 1  // Use direct TFT rendering to eliminate morph flashing

// Default LED color (RGB565). Start with red.
#define DEFAULT_LED_COLOR_565 0xF800

// ===== TIME / NTP =====
#define DEFAULT_TZ "Sydney, Australia"    // Timezone name from timezones.h
#define DEFAULT_NTP "pool.ntp.org"
#define DEFAULT_24H true

// ===== SENSOR CONFIGURATION =====
// Choose your sensor type by uncommenting ONE of the following:
// #define USE_BME280        // BME280: Temperature, Humidity, Pressure sensor
// #define USE_SHT3X         // SHT3X: Temperature and Humidity sensor (no pressure)
#define USE_HTU21D           // HTU21D: Temperature and Humidity sensor (no pressure) - DEFAULT

// I2C pins for sensor
// Note: GPIO 21/22 are used by touch controller (FT62x6) on Touchdown
// Can share I2C bus with touch or use alternate pins
#define SENSOR_SDA_PIN    21    // I2C Data (shared with touch controller)
#define SENSOR_SCL_PIN    22    // I2C Clock (shared with touch controller)

// Sensor update interval
#define SENSOR_UPDATE_INTERVAL 60000  // Update sensor every 60 seconds

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
