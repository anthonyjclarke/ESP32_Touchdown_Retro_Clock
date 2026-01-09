#pragma once

#include <Adafruit_GFX.h>
#include <TetrisMatrixDraw.h>
#include "config.h"

/**
 * FramebufferGFX - Adafruit GFX wrapper for our LED matrix framebuffer
 *
 * This class allows TetrisMatrixDraw (which expects an Adafruit_GFX display)
 * to write to our custom uint8_t framebuffer instead of a physical display.
 *
 * It converts 16-bit RGB565 colors from the Tetris library into 8-bit
 * intensity values (0-255) for our LED matrix emulation.
 */
class FramebufferGFX : public Adafruit_GFX {
public:
  FramebufferGFX(uint8_t (*framebuffer)[LED_MATRIX_W], uint8_t w, uint8_t h)
    : Adafruit_GFX(w, h), fb(framebuffer) {}

  /**
   * Draw a single pixel to the framebuffer
   * @param x X coordinate (0 to LED_MATRIX_W-1)
   * @param y Y coordinate (0 to LED_MATRIX_H-1)
   * @param color RGB565 color from Tetris library
   */
  void drawPixel(int16_t x, int16_t y, uint16_t color) override {
    if (x < 0 || x >= LED_MATRIX_W || y < 0 || y >= LED_MATRIX_H) return;

    // Convert RGB565 to 8-bit intensity
    uint8_t intensity = colorToIntensity(color);
    fb[y][x] = intensity;
  }

  /**
   * Fill the entire display with a color
   * @param color RGB565 color to fill with
   */
  void fillScreen(uint16_t color) {
    uint8_t intensity = colorToIntensity(color);
    for (int y = 0; y < LED_MATRIX_H; y++) {
      for (int x = 0; x < LED_MATRIX_W; x++) {
        fb[y][x] = intensity;
      }
    }
  }

private:
  uint8_t (*fb)[LED_MATRIX_W];  // Pointer to our framebuffer

  /**
   * Convert RGB565 color to 8-bit intensity (0-255)
   * Uses weighted grayscale conversion based on human perception
   * @param color565 RGB565 color value
   * @return Intensity value 0-255
   */
  uint8_t colorToIntensity(uint16_t color565) {
    // If color is 0 (black), return 0 intensity
    if (color565 == 0) return 0;

    // Extract RGB components from RGB565
    uint8_t r = (color565 >> 11) & 0x1F;  // 5 bits
    uint8_t g = (color565 >> 5) & 0x3F;   // 6 bits
    uint8_t b = color565 & 0x1F;           // 5 bits

    // Scale to 8-bit (0-255)
    uint8_t r8 = (r * 255) / 31;
    uint8_t g8 = (g * 255) / 63;
    uint8_t b8 = (b * 255) / 31;

    // Convert to grayscale using weighted average
    // Human perception: 30% red, 59% green, 11% blue
    uint16_t intensity = (r8 * 30 + g8 * 59 + b8 * 11) / 100;

    return (uint8_t)intensity;
  }
};

/**
 * Tetris Clock Display Mode
 *
 * Manages the Tetris-style animated clock display using falling blocks.
 * Respects 12/24 hour format setting and provides smooth animations.
 */
class TetrisClock {
public:
  /**
   * Initialize the Tetris clock with our framebuffer
   * @param framebuffer Pointer to LED matrix framebuffer
   */
  TetrisClock(uint8_t (*framebuffer)[LED_MATRIX_W])
    : display(framebuffer, LED_MATRIX_W, LED_MATRIX_H),
      tetrisTime(display),
      tetrisAMPM_M(display),
      tetrisAMPM_AP(display) {

    // Set scale for larger digits (2x)
    tetrisTime.scale = 2;
  }

  /**
   * Update the Tetris clock display
   * @param timeStr Time string in format "HH:MM" or "H:MM"
   * @param use24h True for 24-hour format, false for 12-hour with AM/PM
   * @param showColon True to show colon between hours and minutes
   * @return True if animation is complete, false if still animating
   */
  bool update(const String& timeStr, bool use24h, bool showColon) {
    // Store current time for comparison
    if (lastTimeStr != timeStr) {
      lastTimeStr = timeStr;

      // Set the new time (this triggers animation)
      tetrisTime.setTime(timeStr, true);  // forceRefresh=true
      animating = true;

      // Handle AM/PM for 12-hour format
      if (!use24h) {
        // Get current hour to determine AM/PM
        int hour = timeStr.substring(0, timeStr.indexOf(':')).toInt();
        String ampm = (hour >= 12) ? "PM" : "AM";

        if (lastAMPM != ampm) {
          lastAMPM = ampm;
          tetrisAMPM_M.setText("M", true);  // Always "M"
          tetrisAMPM_AP.setText(ampm.substring(0, 1), true);  // "A" or "P"
        }
      }
    }

    // Draw the clock
    bool timeComplete = false;
    bool ampmComplete = true;

    if (use24h) {
      // 24-hour format: centered on display
      timeComplete = tetrisTime.drawNumbers(2, 26, showColon);
    } else {
      // 12-hour format: time on left, AM/PM on right
      timeComplete = tetrisTime.drawNumbers(-6, 26, showColon);

      // Draw AM/PM indicators
      bool mComplete = tetrisAMPM_M.drawText(56, 25);

      // Only draw A/P after M is complete
      if (mComplete) {
        bool apComplete = tetrisAMPM_AP.drawText(56, 15);
        ampmComplete = mComplete && apComplete;
      } else {
        ampmComplete = false;
      }
    }

    animating = !(timeComplete && ampmComplete);
    return !animating;
  }

  /**
   * Check if the Tetris clock is currently animating
   * @return True if animation in progress, false if static
   */
  bool isAnimating() const {
    return animating;
  }

  /**
   * Clear the display
   */
  void clear() {
    display.fillScreen(0);  // Fill with black (0 intensity)
  }

private:
  FramebufferGFX display;           // GFX wrapper for our framebuffer
  TetrisMatrixDraw tetrisTime;      // Main time display
  TetrisMatrixDraw tetrisAMPM_M;    // "M" of AM/PM
  TetrisMatrixDraw tetrisAMPM_AP;   // "A" or "P" of AM/PM

  String lastTimeStr = "";          // Last displayed time
  String lastAMPM = "";             // Last displayed AM/PM
  bool animating = false;           // Animation state
};
