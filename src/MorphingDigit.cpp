#include "MorphingDigit.h"

// Fast morph duration for smooth but quick transitions (100ms)
#define MORPH_DURATION_MS 100

MorphingDigit::MorphingDigit()
    : _currentDigit(0)
    , _targetDigit(0)
    , _progress(0.0f)
    , _isMorphing(false)
    , _morphTime(MORPH_DURATION_MS)
    , _elapsed(0)
{
}

void MorphingDigit::setTarget(uint8_t digit) {
    if (digit > 9) digit = 0;  // Clamp to valid range

    if (digit != _currentDigit) {
        _targetDigit = digit;
        _isMorphing = true;
        _progress = 0.0f;
        _elapsed = 0;
    }
}

void MorphingDigit::update(unsigned long deltaMs) {
    if (!_isMorphing) return;

    _elapsed += deltaMs;

    if (_elapsed >= _morphTime) {
        // Morphing complete
        _currentDigit = _targetDigit;
        _progress = 1.0f;
        _isMorphing = false;
        _elapsed = 0;
    } else {
        // Calculate progress with easing
        float t = (float)_elapsed / (float)_morphTime;
        _progress = easeInOutCubic(t);
    }
}

uint8_t MorphingDigit::getSegmentBrightness(uint8_t segment) const {
    bool currentActive = isSegmentActive(_currentDigit, segment);
    bool targetActive = isSegmentActive(_targetDigit, segment);

    if (!_isMorphing) {
        // Not morphing - segment is either fully on or off
        return currentActive ? 255 : 0;
    }

    // Morphing in progress
    if (currentActive && targetActive) {
        // Segment stays on - full brightness
        return 255;
    }
    else if (currentActive && !targetActive) {
        // Segment turning off - fade out
        return (uint8_t)(255.0f * (1.0f - _progress));
    }
    else if (!currentActive && targetActive) {
        // Segment turning on - fade in
        return (uint8_t)(255.0f * _progress);
    }
    else {
        // Segment stays off
        return 0;
    }
}

void MorphingDigit::setCurrent(uint8_t digit) {
    if (digit > 9) digit = 0;
    _currentDigit = digit;
    _targetDigit = digit;
    _progress = 0.0f;
    _isMorphing = false;
    _elapsed = 0;
}

bool MorphingDigit::isSegmentActive(uint8_t digit, uint8_t segment) const {
    if (digit > 9) return false;
    uint8_t segmentMask = (1 << segment);
    return (DIGIT_SEGMENTS[digit] & segmentMask) != 0;
}

float MorphingDigit::easeInOutCubic(float t) const {
    return t < 0.5f
        ? 4.0f * t * t * t
        : 1.0f - powf(-2.0f * t + 2.0f, 3.0f) / 2.0f;
}
