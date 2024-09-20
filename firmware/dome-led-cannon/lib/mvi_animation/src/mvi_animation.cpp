#include "mvi_animation.h"

#include <cmath>
#include <cstdint>

AnimationManager::AnimationManager() : start_time_(std::chrono::steady_clock::time_point::min()) {}

void AnimationManager::update() {
  const auto now = std::chrono::steady_clock::now();
  const auto elapsed_time_us =
    std::chrono::duration_cast<std::chrono::microseconds>(now - start_time_).count();

  // Convert microseconds to float milliseconds
  const float elapsed_time_ms = elapsed_time_us / 1000.0f;
  current_animation_(elapsed_time_ms);
}

void AnimationManager::setAnimation(AnimationFunction animation) {
  current_animation_ = animation;
  start_time_ = std::chrono::steady_clock::now();
}

void AnimationManager::stop() {
  current_animation_ = [](time_ms) {};
}

void FillColor(Lights& leds, const CRGB& color) {
  for (uint16_t i = 0; i < leds.size(); i++) {
    leds.setPixelColor(i, color);
  }
}

void SlowBluePulse(time_ms elapsed_time, time_ms duration_ms, Lights& leds) {
  // Normalize the phase between 0 and 1
  const float phase = fmod(elapsed_time, duration_ms) / duration_ms;
  // Sine wave for smooth pulse; scale to 0-1
  const float brightness = (sin(phase * 2 * PI) + 1) / 2;

  const uint8_t scaled_brightness = brightness * 255;
  CRGB color = Lights::RoyalBlue;
  color.nscale8_video(scaled_brightness);
  FillColor(leds, color);
}

void GreenSpinner(time_ms elapsed_time, time_ms duration_ms, Lights& leds) {
  constexpr float SPIN_SPEED = 0.5; // Revolutions per second
  constexpr float TAIL_FRACTION = 0.75;
  const CRGB COLOR = Lights::Green;

  const uint16_t num_leds = leds.size();
  const uint32_t tail_length = uint32_t(float(num_leds) * TAIL_FRACTION);

  // Calculate the current position of the spinner based on the elapsed time
  const float phase = fmod(elapsed_time, duration_ms) / duration_ms;
  const uint32_t position = uint32_t(phase * num_leds * SPIN_SPEED) % num_leds;

  // Clear all LEDs first
  FillColor(leds, Lights::Black);

  // Light up the current position LED and create a fading tail
  for (uint32_t i = 0; i <= tail_length; i++) {
    // Ensure the index wraps around correctly
    const uint8_t led_index = uint8_t((position - i + num_leds) % num_leds);
    // Decrease brightness for the tail
    const uint8_t brightness = 255 * (tail_length - i) / tail_length;
    CRGB color = COLOR;
    color.nscale8_video(brightness);
    leds.setPixelColor(led_index, color);
  }
}

void BlinkThenThrob(time_ms elapsed_time, time_ms duration_ms, Lights& leds) {
  // Blink the LEDs on and off for two seconds then a dim slow pulse
  constexpr time_ms BLINK_DURATION_MS = 1800.0f;
  constexpr uint8_t THROB_MIN_BRIGHTNESS = 10;
  constexpr uint8_t THROB_BRIGHTNESS_SCALE = 32;
  const CRGB BLINK_COLOR = CRGB{0xea, 0xff, 0xaf};
  const CRGB THROB_COLOR = CRGB{0xea, 0xff, 0xaf};

  assert(duration_ms > BLINK_DURATION_MS);

  if (elapsed_time < BLINK_DURATION_MS) {
    // Blink phase. Calculate the number of complete intervals that have passed
    const uint32_t intervals = uint32_t(elapsed_time / 600.0f); // Toggle on/off every 600 ms
    const uint8_t blink_state = intervals % 2;
    const CRGB color = (blink_state == 0) ? BLINK_COLOR : Lights::Black;
    FillColor(leds, color);
    return;
  }

  // Throb phase
  const time_ms current_elapsed_time = elapsed_time - BLINK_DURATION_MS;
  const float phase = fmod(elapsed_time, duration_ms) / duration_ms;
  const float brightness = (sin(phase * 2 * PI) + 1) / 2;
  const uint8_t scaled_brightness = THROB_MIN_BRIGHTNESS + brightness * THROB_BRIGHTNESS_SCALE;
  CRGB color = THROB_COLOR;
  color.nscale8_video(scaled_brightness);
  FillColor(leds, color);
}
