#pragma once

#include <Adafruit_GFX.h>
#include <TetrisMatrixDraw.h>
#include "config.h"

/**
 * FramebufferGFX - Adafruit GFX wrapper for our LED matrix framebuffer
 *
 * This class allows TetrisMatrixDraw (which expects an Adafruit_GFX display)
 * to write to our custom RGB565 framebuffer instead of a physical display.
 *
 * Stores full RGB565 colors directly for authentic Tetris block colors.
 */
class FramebufferGFX : public Adafruit_GFX {
public:
  FramebufferGFX(uint16_t (*framebuffer)[LED_MATRIX_W], uint8_t w, uint8_t h)
    : Adafruit_GFX(w, h), fb(framebuffer) {}

  /**
   * Draw a single pixel to the framebuffer
   * @param x X coordinate (0 to LED_MATRIX_W-1)
   * @param y Y coordinate (0 to LED_MATRIX_H-1)
   * @param color RGB565 color from Tetris library
   */
  void drawPixel(int16_t x, int16_t y, uint16_t color) override {
    if (x < 0 || x >= LED_MATRIX_W || y < 0 || y >= LED_MATRIX_H) return;

    // Store full RGB565 color directly
    fb[y][x] = color;
  }

  /**
   * Fill the entire display with a color
   * @param color RGB565 color to fill with
   */
  void fillScreen(uint16_t color) {
    for (int y = 0; y < LED_MATRIX_H; y++) {
      for (int x = 0; x < LED_MATRIX_W; x++) {
        fb[y][x] = color;
      }
    }
  }

private:
  uint16_t (*fb)[LED_MATRIX_W];  // Pointer to our RGB565 framebuffer
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
   * @param framebuffer Pointer to LED matrix RGB565 framebuffer
   */
  TetrisClock(uint16_t (*framebuffer)[LED_MATRIX_W])
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
   * @param isPM True if PM, false if AM (only used when use24h=false)
   * @return True if animation is complete, false if still animating
   */
  bool update(const String& timeStr, bool use24h, bool showColon, bool isPM = false) {
    // Store current time for comparison
    if (lastTimeStr != timeStr) {
      lastTimeStr = timeStr;

      // Set the new time (this triggers animation)
      tetrisTime.setTime(timeStr, true);  // forceRefresh=true
      animating = true;

      // Handle AM/PM for 12-hour format
      if (!use24h) {
        // Use the isPM parameter to determine AM/PM
        String ampm = isPM ? "PM" : "AM";

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

  /**
   * Reset the Tetris clock to force all digits to rebuild from scratch
   * This clears internal state so all digits will animate in fresh
   */
  void reset() {
    lastTimeStr = "";  // Force time refresh
    lastAMPM = "";     // Force AM/PM refresh
    animating = false;
    display.fillScreen(0);  // Clear display
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
