#pragma once

#include <pulse/pulseaudio.h>

#include <condition_variable>
#include <mutex>

namespace pulse {

template<typename T> struct MainloopSync {
  pa_threaded_mainloop* mainloop;
  std::mutex mutex;
  std::condition_variable cv;
  T result;
  bool completed = false;

  explicit MainloopSync(pa_threaded_mainloop* mainloop) : mainloop(mainloop), result() {}

  bool waitWithTimeout(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex);
    if (!cv.wait_for(lock, timeout, [&] { return completed; })) { return false; }
    return true;
  }

  void signalComplete() {
    std::lock_guard<std::mutex> lock(mutex);
    completed = true;
    cv.notify_one();
  }
};

} // namespace pulse
