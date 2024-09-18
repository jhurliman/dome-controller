#pragma once

// mvi_lights.h - Library for controlling RGB(W) LED lights

// #include "internal/colors/RgbwwColor.h"

#include <NeoPixelBus.h>

#include <cstdint>

// Simple RGB struct replacing the CRGB struct from FastLED. Supports conversion to RgbwwColor
struct CRGB {
  uint8_t r;
  uint8_t g;
  uint8_t b;

  CRGB(uint8_t r, uint8_t g, uint8_t b) : r(r), g(g), b(b) {}

  void nscale8_video(uint8_t scale);

  RgbwwColor toRgbww() const;
};

class Lights {
public:
  // Constructor: number of LEDs, GPIO pin number
  Lights(uint16_t num_leds, uint8_t data_pin);

  // Set the color of a single LED
  void setPixelColor(uint16_t index, const RgbwwColor& color);

  // Set the color of a single LED, converting the RGB color to RGBWW
  void setPixelColor(uint16_t index, const CRGB& color);

  // Fill the entire LED strip with a single color
  void fillColor(const RgbwwColor& color);

  void fillColor(const CRGB& color);

  // Update the LED strip
  void show();

  // Convert a color temperature from [0-10000K] to an RGBWW color
  static RgbwwColor ColorFromTemperature(uint16_t color_temp);

  static const CRGB Black;
  static const CRGB Red;
  static const CRGB Green;
  static const CRGB RoyalBlue;

private:
  // WS2805 RGB+CCT LED strip, ESP32 I2S method
  NeoPixelBus<NeoGrbwwFeature, NeoWs2805Method> leds_;
  uint16_t num_leds_;
};
