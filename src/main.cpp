#include "cuda/CudaBufferUnified.hpp"
#include "cuda/arithmetic.hpp"
#include "cuda/stream.hpp"
#include "pulse_utils.hpp"

#include <fmt/format.h>
#include <pulse/pulseaudio.h>

#include <condition_variable>
#include <iostream>
#include <mutex>

constexpr char APP_NAME[] = "dome-controller";
constexpr char APP_ID[] = "org.jhurliman.dome-controller";
constexpr char APP_VERSION[] = "0.0.1";

using namespace std::chrono_literals;

template<typename T>
std::unique_ptr<CudaBufferUnified> Vec2Cuda(const std::vector<T>& vec, cudaStream_t stream) {
  auto res = CudaBufferUnified::createFromHostData(vec.data(), vec.size() * sizeof(T), stream);
  if (!res) { throw std::runtime_error(res.error().errorMessage); }
  return std::move(res.value());
}

std::unique_ptr<CudaBufferUnified> CudaBuf(size_t size) {
  auto res = CudaBufferUnified::create(size);
  if (!res) { throw std::runtime_error(res.error().errorMessage); }
  return std::move(res.value());
}

template<typename T> CudaArrayView<T> Buf2View(CudaBufferUnified& buf) {
  const size_t n = buf.size() / sizeof(T);
  auto res = CudaArrayView<T>::fromBuffer(buf, n);
  if (!res) { throw std::runtime_error(res.error().errorMessage); }
  return std::move(res.value());
}

void PulseContextStateCallback(pa_context* context, void* userData);
void PulseStreamWriteCallback(pa_stream* stream, size_t requestedBytes, void* userData);

struct PulseAudioSync {
  pa_threaded_mainloop* mainloop;
  std::mutex mutex;
  std::condition_variable cv;
  bool completed = false;

  explicit PulseAudioSync(pa_threaded_mainloop* mainloop) : mainloop(mainloop) {}

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

struct PulseSession {
  pa_threaded_mainloop* mainloop;
  pa_context* context;
};

tl::expected<PulseSession, PulseError> CreatePulseAudioSession() {
  ScopedPropertyList props;
  props.setValue(PA_PROP_APPLICATION_NAME, APP_NAME);
  props.setValue(PA_PROP_APPLICATION_ID, APP_ID);
  props.setValue(PA_PROP_APPLICATION_VERSION, APP_VERSION);

  // Create a PulseAudio threaded mainloop API server connection context. The mainloop is the
  // internal asynchronous API event loop
  pa_threaded_mainloop* mainloop = pa_threaded_mainloop_new();
  if (!mainloop) {
    const int err = pa_context_errno(nullptr);
    const std::string msg = err > 0 ? pa_strerror(err) : "pa_threaded_mainloop_new unknown failure";
    return tl::unexpected<PulseError>(PulseError(err, msg));
  }

  pa_context* context =
    pa_context_new_with_proplist(pa_threaded_mainloop_get_api(mainloop), APP_NAME, props.get());
  if (!context) {
    const int err = pa_context_errno(nullptr);
    const std::string msg =
      err > 0 ? pa_strerror(err) : "pa_context_new_with_proplist unknown failure";
    pa_threaded_mainloop_free(mainloop);
    return tl::unexpected<PulseError>(PulseError(err, msg));
  }

  auto signalReadyOrErrorStateCallback = [](pa_context* context, void* userData) {
    pa_context_state_t contextState = pa_context_get_state(context);
    auto* sync = static_cast<PulseAudioSync*>(userData);
    if (!PA_CONTEXT_IS_GOOD(contextState) || contextState == PA_CONTEXT_READY) {
      sync->signalComplete();
    }
    pa_threaded_mainloop_signal(sync->mainloop, 0);
  };

  // Attempt to connect to the PulseAudio server and wait for the connection to be ready. Connection
  // readiness is signaled by calling signalReadyOrErrorStateCallback when the context state
  // changes. When it changes to either ready or an error state, our condition variable is signaled
  PulseAudioSync sync{mainloop};
  pa_context_set_state_callback(context, signalReadyOrErrorStateCallback, &sync);

  if (0 != pa_context_connect(context, nullptr, PA_CONTEXT_NOFLAGS, nullptr)) {
    const int err = pa_context_errno(context);
    DestroyContext(context);
    pa_threaded_mainloop_free(mainloop);
    return tl::unexpected<PulseError>(PulseError(err, pa_strerror(err)));
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
      return tl::unexpected<PulseError>(PulseError(err, pa_strerror(err)));
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

    // Replace our function local state callback with a global one
    pa_context_set_state_callback(context, &PulseContextStateCallback, mainloop);
  }

  return PulseSession{mainloop, context};
};

void PulseContextStateCallback(pa_context* context, void* userData) {
  (void)context;
  (void)userData;
}

void PulseStreamWriteCallback(pa_stream* stream, size_t requestedBytes, void* userData) {
  (void)stream;
  (void)requestedBytes;
  (void)userData;
}

void ListModules(pa_threaded_mainloop* mainloop, pa_context* context) {
  auto moduleInfoCallback =
    [](pa_context* context, const pa_module_info* info, int eol, void* userData) {
      (void)context;

      if (eol > 0) {
        PulseAudioSync* sync = static_cast<PulseAudioSync*>(userData);
        // Work around an apparent bug in PulseAudio where the final module info callback is
        // called with `(nullptr, nullptr, 1, nullptr)` so we can't signal completion and need to
        // let the timeout handle it
        if (sync) { sync->signalComplete(); }
        return;
      }

      std::cout << "Module: " << info->name;
      if (info->argument) { std::cout << " (" << info->argument << ")"; }
      std::cout << "\n";
    };

  PulseAudioSync sync{mainloop};
  auto mainloopLock = std::make_unique<AutoPulseLock>(mainloop);
  pa_operation* op = pa_context_get_module_info_list(context, moduleInfoCallback, nullptr);
  mainloopLock.reset();

  sync.waitWithTimeout(500ms);

  mainloopLock = std::make_unique<AutoPulseLock>(mainloop);
  pa_operation_unref(op);
}

void ListRecordingInputs(pa_threaded_mainloop* mainloop, pa_context* context) {
  auto sourceInfoCallback =
    [](pa_context* context, const pa_source_info* info, int eol, void* userData) {
      (void)context;

      if (eol > 0) {
        PulseAudioSync* sync = static_cast<PulseAudioSync*>(userData);
        sync->signalComplete();
        return;
      }

      std::cout << "Source: " << info->name << "(" << info->description << ")\n";
    };

  PulseAudioSync sync{mainloop};
  auto mainloopLock = std::make_unique<AutoPulseLock>(mainloop);
  pa_operation* op = pa_context_get_source_info_list(context, sourceInfoCallback, &sync);
  mainloopLock.reset();

  if (!sync.waitWithTimeout(2000ms)) {
    std::cerr << "Timed out waiting for source list\n";
    return;
  }

  mainloopLock = std::make_unique<AutoPulseLock>(mainloop);
  pa_operation_unref(op);
}

int main() {
  InstallStackTraceHandler();

  auto sessionRes = CreatePulseAudioSession();
  if (!sessionRes) {
    std::cerr << "CreatePulseAudioSession failed: " << sessionRes.error().errorMessage << "\n";
    return 1;
  }
  auto session = std::move(sessionRes.value());

  std::cout << "PulseAudio session created\n";

  ListModules(session.mainloop, session.context);

  ListRecordingInputs(session.mainloop, session.context);

  DestroyPulse(session.mainloop, session.context);

  // std::cout << "Adding two vectors\n";

  // auto streamRes = cuda::createStream("test", StreamPriority::Normal);
  // if (!streamRes) {
  //   std::cerr << "createdStream failed: " << streamRes.error().errorMessage << "\n";
  //   return 1;
  // }
  // cudaStream_t stream = streamRes.value();

  // const auto bufA = Vec2Cuda<int64_t>({1, 2, 3, 4, 5}, stream);
  // const auto bufB = Vec2Cuda<int64_t>({6, 7, 8, 9, 10}, stream);
  // const auto bufC = CudaBuf(5 * sizeof(int64_t));

  // const auto viewA = Buf2View<int64_t>(*bufA);
  // const auto viewB = Buf2View<int64_t>(*bufB);
  // auto viewC = Buf2View<int64_t>(*bufC);

  // auto err = addVectors(viewA, viewB, viewC, stream);
  // if (err) {
  //   std::cerr << "addVectors failed: " << err.value().errorMessage << "\n";
  //   return 1;
  // }

  // constexpr size_t n = 5;
  // std::vector<int64_t> vecC(n);
  // err = bufC->copyToHost(vecC.data(), 0, vecC.size() * sizeof(int64_t), stream);
  // if (err) {
  //   std::cerr << "copyToHost failed: " << err.value().errorMessage << "\n";
  //   return 1;
  // }

  // err = cuda::destroyStream(stream);
  // if (err) {
  //   std::cerr << "destroyStream failed: " << err.value().errorMessage << "\n";
  //   return 1;
  // }

  // std::cout << "Result:   " << fmt::format("{}", fmt::join(vecC, ", ")) << "\n";
  // std::cout << "Expected: 7, 9, 11, 13, 15\n";
  // return 0;
}
