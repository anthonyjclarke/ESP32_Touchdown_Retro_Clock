// TFT_eSPI User Setup for ESP32 Touchdown (ILI9488 480x320 TFT Display)
// Hardware: ESP32 Touchdown by Dustin Watts
// GitHub: https://github.com/DustinWatts/esp32-touchdown
//
// PlatformIO uses this via:
//   -DUSER_SETUP_LOADED -include include/User_Setup.h

#define ILI9488_DRIVER

// Display dimensions are defined by ILI9488_Defines.h as 320x480 (portrait)
// We use setRotation(1) to get landscape mode which swaps to 480x320

// SPI pins for ESP32 Touchdown
#define TFT_MISO  -1   // ILI9488 in 4-wire SPI mode (no MISO)
#define TFT_MOSI  23   // SDI (MOSI)
#define TFT_SCLK  18   // SPI Clock
#define TFT_CS    15   // Chip Select
#define TFT_DC     2   // Data/Command (RS)
#define TFT_RST    4   // Reset

// Backlight control on ESP32 Touchdown
// Default: GPIO32 for PWM brightness control
#define TFT_BL    32
#define TFT_BACKLIGHT_ON HIGH

// Touch controller (FT62x6 on I2C)
// ESP32 Touchdown uses I2C touch (FT62x6), not SPI touch
// Define TOUCH_CS as -1 to disable SPI touch warnings
#define TOUCH_CS   -1  // Not used (I2C touch controller)
#define TOUCH_IRQ  27  // Interrupt pin (optional, not currently used)

#define SPI_FREQUENCY  40000000
#define SPI_READ_FREQUENCY  20000000
#define SPI_TOUCH_FREQUENCY  2500000

// Performance optimizations to reduce flashing
// Use DMA for faster SPI transfers
#define USE_DMA_TO_TFT

// Enable SPI write mode for batch operations
#define SPI_32BIT_WRITES 1

// Fonts
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF
#define SMOOTH_FONT
