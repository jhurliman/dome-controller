#pragma once

#include "mvi_lights.h"

#include <chrono>
#include <functional>

using time_ms = float;
using AnimationFunction = std::function<void(time_ms)>;

// Animation system for an LED light strip
class AnimationManager {
private:
  AnimationFunction current_animation_;
  std::chrono::steady_clock::time_point start_time_;

public:
  AnimationManager();

  void update();
  void setAnimation(AnimationFunction animation);
  void stop();
};

void FillColor(Lights& leds, uint32_t num_leds, const CRGB& color);

void SlowBluePulse(time_ms elapsed_time, time_ms duration_ms, Lights& leds, uint32_t num_leds);

void GreenSpinner(time_ms elapsed_time, time_ms duration_ms, Lights& leds, uint32_t num_leds);

void BlinkThenThrob(time_ms elapsed_time, time_ms duration_ms, Lights& leds, uint32_t num_leds);
