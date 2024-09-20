#include "mvi_lights.h"

#include "internal/features/NeoRgbwwFeatures.h"
#include "internal/methods/NeoEsp32I2sMethod.h"
#include "sys/_stdint.h"

#include <cstdint>

constexpr uint16_t kMinColorTemp_K = 1000;
constexpr uint16_t kMaxColorTemp_K = 10000;

constexpr uint16_t kCoolWhiteTemp_K = 7000; // WS2805 cool white temperature (approx.)
constexpr uint16_t kWarmWhiteTemp_K = 3000; // WS2805 warm white temperature (approx.)

template<typename T> constexpr const T& clamp(const T& value, const T& low, const T& high) {
  return (value < low) ? low : (value > high ? high : value);
}

void CRGB::nscale8_video(uint8_t scale) {
  const uint8_t nonzeroscale = (scale != 0) ? 1 : 0;
  r = uint8_t((r == 0) ? 0 : (((int)r * (int)(scale)) >> 8) + nonzeroscale);
  g = uint8_t((g == 0) ? 0 : (((int)g * (int)(scale)) >> 8) + nonzeroscale);
  b = uint8_t((b == 0) ? 0 : (((int)b * (int)(scale)) >> 8) + nonzeroscale);
}

RgbwwColor CRGB::toRgbww() const {
  // FIXME: This is a very naive conversion from RGB to RGBWW
  return RgbwwColor(r, g, b);
}

const CRGB Lights::Black{0, 0, 0};
const CRGB Lights::Red{255, 0, 0};
const CRGB Lights::Green{0, 128, 0};
const CRGB Lights::RoyalBlue{65, 105, 225};

Lights::Lights(uint16_t num_leds, uint8_t data_pin)
  : leds_(num_leds, data_pin),
    num_leds_(num_leds) {}

void Lights::setup() {
  leds_.Begin();
  leds_.ClearTo(RgbwwColor{});
}

void Lights::setPixelColor(uint16_t index, const RgbwwColor& color) {
  leds_.SetPixelColor(index, color);
}

void Lights::setPixelColor(uint16_t index, const CRGB& color) {
  const RgbwwColor rgbww_color = color.toRgbww();
  setPixelColor(index, rgbww_color);
}

void Lights::fillColor(const RgbwwColor& color) {
  leds_.ClearTo(color);
}

void Lights::fillColor(const CRGB& color) {
  const RgbwwColor rgbww_color = color.toRgbww();
  fillColor(rgbww_color);
}

void Lights::show() {
  leds_.Show();
}

// Map a color temperature to a corresponding RgbwwColor with Red and Blue LEDs
RgbwwColor Lights::ColorFromTemperature(uint16_t color_temp) {
  // Special case for color temperatures at or below 1000K
  if (color_temp <= kMinColorTemp_K) {
    // Scale red intensity from 0 to 255 with color temperature 0-1000K
    const uint8_t red_value = uint8_t(UINT8_MAX * (float(color_temp) / kMinColorTemp_K));

    // Scale warm white intensity from 0 to 255 with color temperature 400-1000K
    constexpr uint16_t kMinWarmWhiteValue = 400;
    uint8_t warm_white_value = 0;
    if (color_temp >= kMinWarmWhiteValue) {
      warm_white_value = uint8_t(UINT8_MAX *
        ((float(color_temp) - kMinWarmWhiteValue) / (kMinColorTemp_K - kMinWarmWhiteValue)));
    }

    return RgbwwColor(red_value, 0, 0, warm_white_value, 0);
  }

  // Clip the color temperature to stay within the range
  color_temp = clamp(color_temp, kMinColorTemp_K, kMaxColorTemp_K);

  // Calculate Red and Blue levels based on temperature extremes
  uint8_t red_level = 0;
  uint8_t blue_level = 0;

  // If temperature is below 3000K, increase red intensity
  if (color_temp <= kWarmWhiteTemp_K) {
    const float red_ratio =
      float(kWarmWhiteTemp_K - color_temp) / (kWarmWhiteTemp_K - kMinColorTemp_K);
    red_level = uint8_t(red_ratio * UINT8_MAX);
  }

  // If temperature is above 7000K, increase blue intensity
  if (color_temp >= kCoolWhiteTemp_K) {
    const float blue_ratio =
      float(color_temp - kCoolWhiteTemp_K) / (kMaxColorTemp_K - kCoolWhiteTemp_K);
    blue_level = uint8_t(blue_ratio * UINT8_MAX);
  }

  // Calculate the warm and cool white levels based on temperature in the range 3000K to 7000K
  float cool_ratio = 0.0f;
  float warm_ratio = 0.0f;

  if (color_temp >= kWarmWhiteTemp_K && color_temp <= kCoolWhiteTemp_K) {
    cool_ratio = float(color_temp - kWarmWhiteTemp_K) / (kCoolWhiteTemp_K - kWarmWhiteTemp_K);
    warm_ratio = 1.0f - cool_ratio;
  } else if (color_temp < kWarmWhiteTemp_K) {
    warm_ratio = 1.0f; // Below 3000K, warm white dominates
  } else if (color_temp > kCoolWhiteTemp_K) {
    cool_ratio = 1.0f; // Above 7000K, cool white dominates
  }

  const uint8_t cool_white_level = uint8_t(cool_ratio * UINT8_MAX);
  const uint8_t warm_white_level = uint8_t(warm_ratio * UINT8_MAX);
  return RgbwwColor(red_level, 0, blue_level, warm_white_level, cool_white_level);
}
