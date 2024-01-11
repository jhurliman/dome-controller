#pragma once

#include <pulse/pulseaudio.h>
#include <tl/expected.hpp>

#include <memory>
#include <optional>
#include <string>

struct PulseError {
  int errorCode;
  std::string errorMessage;

  PulseError(int code,
    std::string message,
    std::optional<std::string> filename = std::nullopt,
    std::optional<int> line = std::nullopt)
    : errorCode(code),
      errorMessage(message) {
    if (filename && line) {
      errorMessage = *filename + ":" + std::to_string(*line) + ": " + errorMessage;
    }
  }
};

class ScopedPropertyList {
public:
  ScopedPropertyList() : propertyList_(pa_proplist_new()) {}

  ScopedPropertyList(const ScopedPropertyList&) = delete;
  ScopedPropertyList& operator=(const ScopedPropertyList&) = delete;

  pa_proplist* get() const { return propertyList_.get(); }

  int setValue(const char* key, const char* value) {
    return pa_proplist_sets(propertyList_.get(), key, value);
  }

private:
  using deleter = std::integral_constant<decltype(pa_proplist_free)*, pa_proplist_free>;
  std::unique_ptr<pa_proplist, deleter> propertyList_;
};

class AutoPulseLock {
public:
  explicit AutoPulseLock(pa_threaded_mainloop* paMainloop) : paMainloop_(paMainloop) {
    pa_threaded_mainloop_lock(paMainloop_);
  }

  AutoPulseLock(const AutoPulseLock&) = delete;
  AutoPulseLock& operator=(const AutoPulseLock&) = delete;

  ~AutoPulseLock() { pa_threaded_mainloop_unlock(paMainloop_); }

private:
  pa_threaded_mainloop* paMainloop_;
};

void DestroyMainloop(pa_threaded_mainloop* mainloop) {
  pa_threaded_mainloop_stop(mainloop);
  pa_threaded_mainloop_free(mainloop);
}

void DestroyContext(pa_context* context) {
  pa_context_set_state_callback(context, nullptr, nullptr);
  pa_context_disconnect(context);
  pa_context_unref(context);
}

void DestroyPulse(pa_threaded_mainloop* mainloop, pa_context* context) {
  {
    AutoPulseLock lock(mainloop);
    DestroyContext(context);
  }
  DestroyMainloop(mainloop);
}
