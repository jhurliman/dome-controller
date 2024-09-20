#include "mvi_lights.h"

#include "internal/features/NeoRgbwwFeatures.h"
#include "internal/methods/NeoEsp32I2sMethod.h"
#include "sys/_stdint.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <tuple>

constexpr uint16_t kMinColorTemp_K = 1000;
constexpr uint16_t kMaxColorTemp_K = 10000;

constexpr uint16_t kCoolWhiteTemp_K = 7000; // WS2805 cool white temperature (approx.)
constexpr uint16_t kWarmWhiteTemp_K = 3000; // WS2805 warm white temperature (approx.)

using HSI = std::tuple<double, double, double>;
using RGBW = std::tuple<int, int, int, int>;

template<typename T> constexpr const T& clamp(const T& value, const T& low, const T& high) {
  return (value < low) ? low : (value > high ? high : value);
}

// Function to convert RGB to HSI color space
static HSI rgb_to_hsi(int r_in, int g_in, int b_in) {
  const double r = clamp(double(r_in) / 255.0, 0.0, 1.0);
  const double g = clamp(double(g_in) / 255.0, 0.0, 1.0);
  const double b = clamp(double(b_in) / 255.0, 0.0, 1.0);

  const double intensity = (r + g + b) / 3.0;

  const double M = std::max({r, g, b});
  const double m = std::min({r, g, b});
  const double C = M - m;

  double saturation = 0.0;
  if (intensity == 0.0) {
    saturation = 0.0;
  } else {
    saturation = 1.0 - (m / intensity);
  }

  double hue = 0.0;
  if (M == m) {
    hue = 0.0;
  } else if (M == r) {
    hue = 60.0 * (0.0 + (g - b) / (M - m));
  } else if (M == g) {
    hue = 60.0 * (2.0 + (b - r) / (M - m));
  } else if (M == b) {
    hue = 60.0 * (4.0 + (r - g) / (M - m));
  }

  if (hue < 0.0) { hue += 360.0; }

  return std::make_tuple(hue, std::abs(saturation), intensity);
}

// Function to convert HSI to RGBW color space
static RGBW hsi_to_rgbw(double H, double S, double I) {
  double r = 0.0, g = 0.0, b = 0.0, w = 0.0;
  double cos_h = 0.0, cos_1047_h = 0.0;

  H = std::fmod(H, 360.0); // Cycle H around to 0-360 degrees
  H = 3.14159 * H / 180.0; // Convert to radians
  S = clamp(S, 0.0, 1.0);
  I = clamp(I, 0.0, 1.0);

  if (H < 2.09439) {
    cos_h = std::cos(H);
    cos_1047_h = std::cos(1.047196667 - H);
    r = S * 255.0 * I / 3.0 * (1.0 + cos_h / cos_1047_h);
    g = S * 255.0 * I / 3.0 * (1.0 + (1.0 - cos_h / cos_1047_h));
    b = 0.0;
  } else if (H < 4.188787) {
    H -= 2.09439;
    cos_h = std::cos(H);
    cos_1047_h = std::cos(1.047196667 - H);
    g = S * 255.0 * I / 3.0 * (1.0 + cos_h / cos_1047_h);
    b = S * 255.0 * I / 3.0 * (1.0 + (1.0 - cos_h / cos_1047_h));
    r = 0.0;
  } else {
    H -= 4.188787;
    cos_h = std::cos(H);
    cos_1047_h = std::cos(1.047196667 - H);
    b = S * 255.0 * I / 3.0 * (1.0 + cos_h / cos_1047_h);
    r = S * 255.0 * I / 3.0 * (1.0 + (1.0 - cos_h / cos_1047_h));
    g = 0.0;
  }
  w = 255.0 * (1.0 - S) * I;

  // Adjusting RGB values as per the original Python code
  const int ri = int(clamp(r * 3.0, 0.0, 255.0));
  const int gi = int(clamp(g * 3.0, 0.0, 255.0));
  const int bi = int(clamp(b * 3.0, 0.0, 255.0));
  const int wi = int(clamp(w, 0.0, 255.0));

  return std::make_tuple(ri, gi, bi, wi);
}

// Function to convert RGB to HSI and then to RGBW color space. Adapted from
// <https://github.com/iamh2o/rgbw_colorspace_converter/blob/5dfbf9fd3d519939191d7e7d3213eb173dcce828/src/rgbw_colorspace_converter/colors/converters.py>
static std::tuple<int, int, int, int> rgb_to_rgbw(int r, int g, int b) {
  const HSI hsi = rgb_to_hsi(r, g, b);
  const double hue = std::get<0>(hsi);
  const double saturation = std::get<1>(hsi);
  const double intensity = std::get<2>(hsi);
  return hsi_to_rgbw(hue, saturation, intensity);
}

void CRGB::nscale8_video(uint8_t scale) {
  const uint8_t nonzeroscale = (scale != 0) ? 1 : 0;
  r = uint8_t((r == 0) ? 0 : ((int(r) * int(scale)) >> 8) + nonzeroscale);
  g = uint8_t((g == 0) ? 0 : ((int(g) * int(scale)) >> 8) + nonzeroscale);
  b = uint8_t((b == 0) ? 0 : ((int(b) * int(scale)) >> 8) + nonzeroscale);
}

RgbwwColor CRGB::toRgbww() const {
  // Convert RGB to RGBW
  const RGBW rgbw = rgb_to_rgbw(r, g, b);
  const int r_out = std::get<0>(rgbw);
  const int g_out = std::get<1>(rgbw);
  const int b_out = std::get<2>(rgbw);
  const int w_out = std::get<3>(rgbw);
  return RgbwwColor(r_out, g_out, b_out, w_out, w_out);
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
