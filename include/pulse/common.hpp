#pragma once

#include <pulse/pulseaudio.h>
#include <tl/expected.hpp>

#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace pulse {

// Forward declarations
struct Sink;
struct Source;

struct PulseError {
  int errorCode;
  std::string errorMessage;

  explicit PulseError(int code,
    std::optional<std::string> message = std::nullopt,
    std::optional<std::string> filename = std::nullopt,
    std::optional<int> line = std::nullopt);
};

struct Session {
  pa_threaded_mainloop* mainloop;
  pa_context* context;
};

inline std::ostream& operator<<(std::ostream& os, const PulseError& err) {
  return os << err.errorMessage;
}

tl::expected<Session, pulse::PulseError> CreateSession(
  const std::string_view appName, const std::string_view appId, const std::string_view appVersion);

std::unordered_map<std::string, std::string> ProplistToMap(pa_proplist* proplist);

std::vector<Sink> GetAudioSinks(pa_threaded_mainloop* mainloop, pa_context* context);

std::vector<Source> GetRecordingSources(pa_threaded_mainloop* mainloop, pa_context* context);

bool WaitForStreamReady(pa_stream* stream, pa_threaded_mainloop* mainloop);

void DestroyMainloop(pa_threaded_mainloop* mainloop);

void DestroyContext(pa_context* context);

void DestroyPulse(pa_threaded_mainloop* mainloop, pa_context* context);

} // namespace pulse
