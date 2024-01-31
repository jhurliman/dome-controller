#include "pulse/common.hpp"

#include "pulse/AutoPulseLock.hpp"
#include "pulse/MainloopSync.hpp"
#include "pulse/ScopedPropertyList.hpp"
#include "pulse/Sink.hpp"
#include "pulse/Source.hpp"

using namespace std::chrono_literals;

namespace pulse {

PulseError::PulseError(int code,
  std::optional<std::string> message,
  std::optional<std::string> filename,
  std::optional<int> line)
  : errorCode(code),
    errorMessage(message ? *message : pa_strerror(code)) {
  if (filename && line) {
    errorMessage = *filename + ":" + std::to_string(*line) + ": " + errorMessage;
  }
}

struct PulseSession {
  pa_threaded_mainloop* mainloop;
  pa_context* context;
};

tl::expected<Session, PulseError> CreateSession(
  const std::string_view appName, const std::string_view appId, const std::string_view appVersion) {
  ScopedPropertyList props;
  props.setValue(PA_PROP_APPLICATION_NAME, appName.data());
  props.setValue(PA_PROP_APPLICATION_ID, appId.data());
  props.setValue(PA_PROP_APPLICATION_VERSION, appVersion.data());

  // Create a PulseAudio threaded mainloop API server connection context. The mainloop is the
  // internal asynchronous API event loop
  pa_threaded_mainloop* mainloop = pa_threaded_mainloop_new();
  if (!mainloop) {
    const int err = pa_context_errno(nullptr);
    const std::string msg = err > 0 ? pa_strerror(err) : "pa_threaded_mainloop_new unknown failure";
    return tl::unexpected<PulseError>(PulseError(err, msg));
  }

  pa_context* context = pa_context_new_with_proplist(
    pa_threaded_mainloop_get_api(mainloop), appName.data(), props.get());
  if (!context) {
    const int err = pa_context_errno(nullptr);
    const std::string msg =
      err > 0 ? pa_strerror(err) : "pa_context_new_with_proplist unknown failure";
    pa_threaded_mainloop_free(mainloop);
    return tl::unexpected<PulseError>(PulseError(err, msg));
  }

  auto signalReadyOrErrorStateCallback = [](pa_context* context, void* userData) {
    pa_context_state_t contextState = pa_context_get_state(context);
    auto* sync = static_cast<MainloopSync<bool>*>(userData);
    if (!PA_CONTEXT_IS_GOOD(contextState) || contextState == PA_CONTEXT_READY) {
      sync->signalComplete();
    }
    pa_threaded_mainloop_signal(sync->mainloop, 0);
  };

  // Attempt to connect to the PulseAudio server and wait for the connection to be ready. Connection
  // readiness is signaled by calling signalReadyOrErrorStateCallback when the context state
  // changes. When it changes to either ready or an error state, our condition variable is signaled
  MainloopSync<bool> sync{mainloop};
  pa_context_set_state_callback(context, signalReadyOrErrorStateCallback, &sync);

  if (0 != pa_context_connect(context, nullptr, PA_CONTEXT_NOFLAGS, nullptr)) {
    const int err = pa_context_errno(context);
    DestroyContext(context);
    pa_threaded_mainloop_free(mainloop);
    return tl::unexpected<PulseError>(PulseError(err));
  }

  {
    // Lock the mainloop before calling pa_threaded_mainloop_start()
    auto mainloopLock = std::make_unique<AutoPulseLock>(mainloop);

    // Start the threaded mainloop
    if (0 != pa_threaded_mainloop_start(mainloop)) {
      const int err = pa_context_errno(context);
      DestroyContext(context);
      mainloopLock.reset();
      DestroyMainloop(mainloop);
      return tl::unexpected<PulseError>(PulseError(err));
    }
  }

  // Wait for the context to be ready or for an error to occur
  constexpr auto STARTUP_TIMEOUT = 2000ms;
  if (!sync.waitWithTimeout(STARTUP_TIMEOUT)) {
    DestroyContext(context);
    DestroyMainloop(mainloop);
    return tl::unexpected<PulseError>(
      PulseError(-1, std::string{"Timed out waiting for PulseAudio connection"}));
  }

  {
    // Lock the mainloop before calling pa_context_get_state()
    auto mainloopLock = std::make_unique<AutoPulseLock>(mainloop);

    pa_context_state_t contextState = pa_context_get_state(context);
    if (contextState != PA_CONTEXT_READY) {
      DestroyContext(context);
      mainloopLock.reset();
      DestroyMainloop(mainloop);
      return tl::unexpected<PulseError>(PulseError(
        -1, std::string{"pa_context_get_state returned "} + std::to_string(contextState)));
    }

    // Clear the state callback
    pa_context_set_state_callback(context, nullptr, nullptr);
  }

  return Session{mainloop, context};
}

std::unordered_map<std::string, std::string> ProplistToMap(pa_proplist* proplist) {
  std::unordered_map<std::string, std::string> properties;
  if (!proplist) { return properties; }
  void* state;
  const char* key;
  for (state = nullptr; (key = pa_proplist_iterate(proplist, &state));) {
    const char* value = pa_proplist_gets(proplist, key);
    properties.emplace(key, value ? value : "");
  }
  return properties;
}

std::vector<Sink> GetAudioSinks(pa_threaded_mainloop* mainloop, pa_context* context) {
  using SinkList = std::vector<Sink>;

  auto sinkInfoCallback =
    [](pa_context* context, const pa_sink_info* info, int eol, void* userData) {
      (void)context;
      MainloopSync<SinkList>* sync = static_cast<MainloopSync<SinkList>*>(userData);

      if (eol > 0) {
        sync->signalComplete();
        return;
      }

      sync->result.emplace_back(Sink{info});
    };

  MainloopSync<SinkList> sync{mainloop};
  auto mainloopLock = std::make_unique<AutoPulseLock>(mainloop);
  pa_operation* op = pa_context_get_sink_info_list(context, sinkInfoCallback, &sync);
  mainloopLock.reset();

  if (!sync.waitWithTimeout(2000ms)) {
    std::cerr << "Timed out waiting for sink list\n";
    return std::move(sync.result);
  }

  mainloopLock = std::make_unique<AutoPulseLock>(mainloop);
  pa_operation_unref(op);

  return std::move(sync.result);
}

std::vector<Source> GetRecordingSources(pa_threaded_mainloop* mainloop, pa_context* context) {
  using SourceList = std::vector<Source>;

  auto sourceInfoCallback =
    [](pa_context* context, const pa_source_info* info, int eol, void* userData) {
      (void)context;
      MainloopSync<SourceList>* sync = static_cast<MainloopSync<SourceList>*>(userData);

      if (eol > 0) {
        sync->signalComplete();
        return;
      }

      if (info->monitor_of_sink != PA_INVALID_INDEX) { return; }
      sync->result.emplace_back(Source{info});
    };

  MainloopSync<SourceList> sync{mainloop};
  auto mainloopLock = std::make_unique<AutoPulseLock>(mainloop);
  pa_operation* op = pa_context_get_source_info_list(context, sourceInfoCallback, &sync);
  mainloopLock.reset();

  if (!sync.waitWithTimeout(2000ms)) {
    std::cerr << "Timed out waiting for source list\n";
    return std::move(sync.result);
  }

  mainloopLock = std::make_unique<AutoPulseLock>(mainloop);
  pa_operation_unref(op);

  return std::move(sync.result);
}

// pa_stream_set_state_callback(stream, [](pa_stream* s, void* userdata) {
//   auto mainloop = static_cast<pa_threaded_mainloop*>(userdata);
//   if (pa_stream_get_state(s) == PA_STREAM_READY) {
//     pa_threaded_mainloop_signal(mainloop, 0);
//   }
// }, session.mainloop);

// // Connect the stream to the audio sink
// // ... existing connection code ...

// // Wait for the stream to become ready
// pa_threaded_mainloop_lock(session.mainloop);
// while (pa_stream_get_state(stream) != PA_STREAM_READY) {
//   pa_threaded_mainloop_wait(session.mainloop);
// }
// pa_threaded_mainloop_unlock(session.mainloop);

bool WaitForStreamReady(pa_stream* stream, pa_threaded_mainloop* mainloop) {
  pa_stream_set_state_callback(
    stream,
    [](pa_stream* s, void* userdata) {
      auto mainloop = static_cast<pa_threaded_mainloop*>(userdata);
      if (pa_stream_get_state(s) == PA_STREAM_READY) { pa_threaded_mainloop_signal(mainloop, 0); }
    },
    mainloop);

  pa_threaded_mainloop_lock(mainloop);
  while (pa_stream_get_state(stream) != PA_STREAM_READY) {
    pa_threaded_mainloop_wait(mainloop);
  }
  pa_threaded_mainloop_unlock(mainloop);

  return true;
}

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

} // namespace pulse
