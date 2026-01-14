#pragma once

#include <Arduino.h>

// Seven-segment digit representation
// Segments labeled:
//     aaa
//    f   b
//    f   b
//     ggg
//    e   c
//    e   c
//     ddd

// Segment bit positions
#define SEG_A 0b00000001
#define SEG_B 0b00000010
#define SEG_C 0b00000100
#define SEG_D 0b00001000
#define SEG_E 0b00010000
#define SEG_F 0b00100000
#define SEG_G 0b01000000

// Digit to segment mapping (which segments are ON for each digit 0-9)
// NOTE: Since we mirrored the segment coordinates (B↔F, C↔E), we keep the
// original logical mapping - the physical positions are swapped in SEGMENT_COORDS
const uint8_t DIGIT_SEGMENTS[10] = {
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,           // 0
    SEG_B | SEG_C,                                           // 1
    SEG_A | SEG_B | SEG_D | SEG_E | SEG_G,                  // 2
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_G,                  // 3
    SEG_B | SEG_C | SEG_F | SEG_G,                          // 4
    SEG_A | SEG_C | SEG_D | SEG_F | SEG_G,                  // 5
    SEG_A | SEG_C | SEG_D | SEG_E | SEG_F | SEG_G,          // 6
    SEG_A | SEG_B | SEG_C,                                   // 7
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F | SEG_G,  // 8
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_F | SEG_G           // 9
};

// Segment coordinates for 12x20 pixel digit
struct SegmentCoords {
    int x1, y1;  // Start point
    int x2, y2;  // End point
    int thickness;  // Segment thickness
};

// LED-style 7-segment digit definition for MorphingClockRemix
// Each segment consists of multiple LED dots arranged in a line
// This creates the classic LED display look with individual light points

// Number of LEDs per segment for each segment type
// Horizontal segments (A, D, G): more dots across
// Vertical segments (B, C, E, F): dots along the length
#define SEG_A_LEDS 4   // Top horizontal
#define SEG_B_LEDS 5   // Upper right vertical
#define SEG_C_LEDS 5   // Lower right vertical
#define SEG_D_LEDS 4   // Bottom horizontal
#define SEG_E_LEDS 5   // Lower left vertical
#define SEG_F_LEDS 5   // Upper left vertical
#define SEG_G_LEDS 4   // Middle horizontal

// LED dot size (diameter in pixels)
#define LED_DOT_SIZE 2

// Digit colors (RGB565) - Each digit has a distinct color like the original
#define DIGIT_COLOR_0  0xF800  // Red
#define DIGIT_COLOR_1  0x07E0  // Green
#define DIGIT_COLOR_2  0x001F  // Blue
#define DIGIT_COLOR_3  0x07FF  // Cyan
#define DIGIT_COLOR_4  0xF81F  // Magenta
#define DIGIT_COLOR_5  0xFFE0  // Yellow
#define DIGIT_COLOR_6  0xFD20  // Orange
#define DIGIT_COLOR_7  0xA000  // Purple
#define DIGIT_COLOR_8  0xFFFF  // White
#define DIGIT_COLOR_9  0x07E0  // Green (like 1)

const uint16_t DIGIT_COLORS[10] = {
    DIGIT_COLOR_0, DIGIT_COLOR_1, DIGIT_COLOR_2, DIGIT_COLOR_3,
    DIGIT_COLOR_4, DIGIT_COLOR_5, DIGIT_COLOR_6, DIGIT_COLOR_7,
    DIGIT_COLOR_8, DIGIT_COLOR_9
};

// 7-segment digit coordinates for 64x32 LED matrix
// Compact height (18 rows) to fit between sensor (y=0-4) and date (y=27-31)
// Each digit fits in 7x18 pixel slot with proper spacing
const SegmentCoords SEGMENT_COORDS[7] = {
    {6, 1, 1, 1, SEG_A_LEDS},  // Segment A (top horizontal) - 4 LEDs
    {6, 2, 6, 8, SEG_B_LEDS},  // Segment B (upper right vertical) - 5 LEDs
    {6, 11, 6, 17, SEG_C_LEDS}, // Segment C (lower right vertical) - 5 LEDs
    {6, 18, 1, 18, SEG_D_LEDS}, // Segment D (bottom horizontal) - 4 LEDs
    {1, 11, 1, 17, SEG_E_LEDS}, // Segment E (lower left vertical) - 5 LEDs
    {1, 2, 1, 8, SEG_F_LEDS},   // Segment F (upper left vertical) - 5 LEDs
    {6, 9, 1, 9, SEG_G_LEDS}    // Segment G (middle horizontal) - 4 LEDs
};

class MorphingDigit {
public:
    MorphingDigit();

    // Set the target digit to morph to
    void setTarget(uint8_t digit);

    // Update morphing animation (call every frame)
    void update(unsigned long deltaMs);

    // Check if currently morphing
    bool isMorphing() const { return _isMorphing; }

    // Get current morph progress (0.0 = current digit, 1.0 = target digit)
    float getProgress() const { return _progress; }

    // Get segment brightness for rendering (0-255)
    uint8_t getSegmentBrightness(uint8_t segment) const;

    // Force set current digit (no morphing)
    void setCurrent(uint8_t digit);

    // Get current and target digits
    uint8_t getCurrent() const { return _currentDigit; }
    uint8_t getTarget() const { return _targetDigit; }

    // Get the digit color (for rendering)
    uint16_t getColor() const { return DIGIT_COLORS[_currentDigit]; }

private:
    uint8_t _currentDigit;     // Current displayed digit (0-9)
    uint8_t _targetDigit;      // Target digit to morph to (0-9)
    float _progress;           // Morph progress (0.0 to 1.0)
    bool _isMorphing;          // Animation in progress flag
    unsigned long _morphTime;  // Total time for morph (ms)
    unsigned long _elapsed;    // Elapsed time (ms)

    // Check if a segment is active for a given digit
    bool isSegmentActive(uint8_t digit, uint8_t segment) const;

    // Apply easing function for smooth animation
    float easeInOutCubic(float t) const;
};
