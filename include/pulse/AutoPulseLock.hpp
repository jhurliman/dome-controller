#pragma once

#include <pulse/pulseaudio.h>

namespace pulse {

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

} // namespace pulse
