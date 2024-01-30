#include "cuda/CudaBufferUnified.hpp"
#include "cuda/arithmetic.hpp"
#include "cuda/stream.hpp"
#include "pulse/AutoPulseLock.hpp"
#include "pulse/MainloopSync.hpp"
#include "pulse/ScopedPropertyList.hpp"
#include "pulse/Sink.hpp"
#include "pulse/Source.hpp"
#include "pulse/common.hpp"

#include <fmt/format.h>
#include <pulse/pulseaudio.h>
#include <tl/expected.hpp>

#include <iostream>
#include <string_view>
#include <thread>

constexpr std::string_view APP_NAME = "dome-controller";
constexpr std::string_view APP_ID = "org.jhurliman.dome-controller";
constexpr std::string_view APP_VERSION = "0.0.1";

constexpr float DURATION_SEC = 5.0f;

using namespace std::chrono_literals;

std::vector<float> GenerateSineWave(float frequency, float durationSeconds, uint32_t sampleRate) {
  size_t totalSamples = size_t(durationSeconds * float(sampleRate));
  std::vector<float> wave(totalSamples);

  for (size_t i = 0; i < totalSamples; i++) {
    wave[i] = std::sin(float(2 * M_PI) * frequency * float(i) / float(sampleRate));
  }

  return wave;
}

std::vector<uint8_t> FloatToS24LE(const std::vector<float>& input) {
  const size_t outputSize = input.size() * 3; // 3 bytes per sample
  std::vector<uint8_t> output(outputSize); // Resize the vector to the required size

  constexpr float scale = float(0x7FFFFF); // 2^23 - 1
  size_t index = 0;

  for (const float& sample : input) {
    // Clamp and scale in a single step
    const int32_t scaled = int32_t(std::max(-1.0f, std::min(1.0f, sample)) * scale);

    // Assign bytes directly using indexing
    output[index++] = uint8_t(scaled & 0xFF);
    output[index++] = uint8_t((scaled >> 8) & 0xFF);
    output[index++] = uint8_t((scaled >> 16) & 0xFF);
  }

  return output;
}

template<typename T>
void InterleaveChannels(std::vector<T>& output, const std::vector<std::vector<T>>& channels) {
  // Calculate total number of channels
  const size_t numChannels = channels.size();
  assert(numChannels > 0);

  // Check that all channels have the same number of samples
  const size_t numSamples = channels[0].size();
  for (size_t i = 1; i < numChannels; ++i) {
    assert(channels[i].size() == numSamples);
  }

  // Resize output vector
  output.resize(numSamples * numChannels);

  // Interleave channels
  for (size_t i = 0; i < numSamples; ++i) {
    for (size_t j = 0; j < numChannels; ++j) {
      output[i * numChannels + j] = channels[j][i];
    }
  }
}

void PulseContextStateCallback(pa_context* context, void* userData) {
  (void)context;
  (void)userData;
}

void PulseStreamWriteCallback(pa_stream* stream, size_t requestedBytes, void* userData) {
  (void)stream;
  (void)requestedBytes;
  (void)userData;
}

void RecordAudio(
  std::vector<float> output, const pulse::Session& session, const pulse::Source& source) {
  output.clear();

  // Create a stream for recording from a source
  std::cout << "Recording from \"" << source.name << "\"\n";
  pulse::ScopedPropertyList streamProps;
  pa_stream* stream = pa_stream_new_with_proplist(
    session.context, "Dome Microphone Array", &source.sampleSpec, nullptr, streamProps.get());
  if (!stream) {
    std::cerr << "Failed to create a new stream for recording from \"" << source.name
              << "\": " << pa_strerror(pa_context_errno(session.context)) << "\n";
    pulse::DestroyPulse(session.mainloop, session.context);
    return;
  }

  // Set the read callback to save to a buffer
  auto readCallback = [](pa_stream* stream, size_t requestedBytes, void* userData) {
    (void)requestedBytes;
    size_t bytesToRead = pa_stream_readable_size(stream);
    if (bytesToRead == size_t(-1)) {
      std::cerr << "Failed to get readable size: "
                << pa_strerror(pa_context_errno(pa_stream_get_context(stream))) << "\n";
      return;
    }
    const size_t floatsToRead = bytesToRead / sizeof(float);
    std::cout << floatsToRead << ".";
    std::cout.flush();

    if (bytesToRead == 0) { return; }

    auto& buffer = *static_cast<std::vector<float>*>(userData);
    buffer.resize(buffer.size() + floatsToRead);

    const void* pulseBuffer = nullptr;
    if (0 != pa_stream_peek(stream, &pulseBuffer, &bytesToRead)) {
      std::cerr << "Failed to peek stream: "
                << pa_strerror(pa_context_errno(pa_stream_get_context(stream))) << "\n";
      return;
    }

    // Copy the data from the pulse buffer to our buffer
    void* data = static_cast<void*>(buffer.data() + buffer.size() - floatsToRead);
    std::memcpy(data, pulseBuffer, bytesToRead);

    if (0 != pa_stream_drop(stream)) {
      std::cerr << "Failed to drop stream: "
                << pa_strerror(pa_context_errno(pa_stream_get_context(stream))) << "\n";
      return;
    }
  };

  pa_stream_set_read_callback(stream, readCallback, &output);

  // Connect to the stream for recording
  const pa_stream_flags_t flags =
    pa_stream_flags_t(PA_STREAM_INTERPOLATE_TIMING | PA_STREAM_AUTO_TIMING_UPDATE);
  const int connectRes = pa_stream_connect_record(stream, source.name.c_str(), nullptr, flags);
  if (0 != connectRes) {
    std::cerr << "Failed to connect record stream to \"" << source.name
              << "\": " << pa_strerror(connectRes) << "\n";
    pa_stream_unref(stream);
    pulse::DestroyPulse(session.mainloop, session.context);
    return;
  }

  if (!pulse::WaitForStreamReady(stream, session.mainloop)) {
    std::cerr << "Timed out waiting for stream to become ready\n";
    pa_stream_unref(stream);
    pulse::DestroyPulse(session.mainloop, session.context);
    return;
  }

  // Start recording
  pa_operation* op = pa_stream_cork(stream, 0, nullptr, nullptr);
  if (!op) {
    std::cerr << "Failed to start recording: "
              << pa_strerror(pa_context_errno(pa_stream_get_context(stream))) << "\n";
    pa_stream_unref(stream);
    pulse::DestroyPulse(session.mainloop, session.context);
    return;
  }

  // Record for five seconds
  std::cout << "Recording for 1 second\n";
  std::this_thread::sleep_for(1s);

  // Stop recording
  pa_operation_unref(op);
  op = pa_stream_cork(stream, 1, nullptr, nullptr);
  if (!op) {
    std::cerr << "Failed to stop recording: "
              << pa_strerror(pa_context_errno(pa_stream_get_context(stream))) << "\n";
    pa_stream_unref(stream);
    pulse::DestroyPulse(session.mainloop, session.context);
    return;
  }
  pa_operation_unref(op);

  float rms = 0;
  for (float sample : output) {
    rms += sample * sample;
  }
  rms = std::sqrt(rms / float(output.size()));
  std::cout << "\nRecorded " << output.size() << " samples, RMS: " << rms << "\n";

  // Cleanup
  std::cout << "Cleaning up recording\n";
  pa_stream_disconnect(stream);
  pa_stream_unref(stream);
}

int main() {
  InstallStackTraceHandler();

  auto sessionRes = pulse::CreateSession(APP_NAME, APP_ID, APP_VERSION);
  if (!sessionRes) {
    std::cerr << "CreatePulseAudioSession failed: " << sessionRes.error().errorMessage << "\n";
    return 1;
  }
  auto session = std::move(sessionRes.value());
  std::cout << "PulseAudio session created\n";

  const auto sources = pulse::GetRecordingSources(session.mainloop, session.context);
  std::cout << "Recording sources:\n";
  for (const auto& source : sources) {
    std::cout << fmt::format("  {} ({}) - {} channel{}\n",
      source.name,
      source.description,
      source.volume.channels,
      source.volume.channels > 1 ? "s" : "");
  }

  // Record from the source with the most channels
  // std::vector<float> recordingBuffer;
  // if (!sources.empty()) {
  //   // Select the source with the most channels
  //   const auto& source = *std::max_element(sources.begin(),
  //     sources.end(),
  //     [](const auto& a, const auto& b) { return a.volume.channels < b.volume.channels; });

  //   RecordAudio(recordingBuffer, session, source);
  // }

  auto sinks = pulse::GetAudioSinks(session.mainloop, session.context);
  std::cout << "Audio sinks:\n";
  for (const auto& sink : sinks) {
    std::cout << fmt::format("  {} ({}) - {} channel{} {}, {}hz\n",
      sink.name,
      sink.description,
      sink.channelMap.channels,
      sink.channelMap.channels > 1 ? "s" : "",
      pa_sample_format_to_string(sink.sampleSpec.format),
      sink.sampleSpec.rate);
  }

  if (sinks.empty()) {
    std::cerr << "No audio sinks found\n";
    pulse::DestroyPulse(session.mainloop, session.context);
    return 1;
  }

  // Select the sink with the most channels
  auto& sink = *std::max_element(sinks.begin(), sinks.end(), [](const auto& a, const auto& b) {
    return a.channelMap.channels < b.channelMap.channels;
  });

  if (sink.sampleSpec.format != PA_SAMPLE_FLOAT32LE && sink.sampleSpec.format != PA_SAMPLE_S24LE) {
    std::cerr << "Sink \"" << sink.name << "\" does not support float32le or s24le, expects "
              << pa_sample_format_to_string(sink.sampleSpec.format) << "\n";
    pulse::DestroyPulse(session.mainloop, session.context);
    return 1;
  }

  // Create a stream for playback to a sink
  pulse::ScopedPropertyList streamProps;
  pa_stream* stream = pa_stream_new_with_proplist(
    session.context, "Sine Wave Playback", &sink.sampleSpec, nullptr, streamProps.get());
  if (!stream) {
    std::cerr << "Failed to create a new stream for playback to \"" << sink.name
              << "\": " << pa_strerror(pa_context_errno(session.context)) << "\n";
    pulse::DestroyPulse(session.mainloop, session.context);
    return 1;
  }

  // Set the write callback
  pa_stream_set_write_callback(stream, PulseStreamWriteCallback, nullptr);

  // Connect the stream to the default audio output
  const pa_stream_flags_t flags = pa_stream_flags_t(
    PA_STREAM_INTERPOLATE_TIMING | PA_STREAM_AUTO_TIMING_UPDATE | PA_STREAM_START_UNMUTED);
  const int connectRes =
    pa_stream_connect_playback(stream, nullptr, nullptr, flags, nullptr, nullptr);
  if (0 != connectRes) {
    std::cerr << "Failed to connect playback stream to \"" << sink.name
              << "\": " << pa_strerror(connectRes) << "\n";
    pa_stream_unref(stream);
    pulse::DestroyPulse(session.mainloop, session.context);
    return 1;
  }

  if (!pulse::WaitForStreamReady(stream, session.mainloop)) {
    std::cerr << "Timed out waiting for stream to become ready\n";
    pa_stream_unref(stream);
    pulse::DestroyPulse(session.mainloop, session.context);
    return 1;
  }

  // Generate a different sine wave for each channel
  const size_t outputChannels = sink.sampleSpec.channels;
  std::vector<std::vector<float>> sineWaves;
  sineWaves.reserve(outputChannels);
  for (size_t i = 0; i < outputChannels; ++i) {
    const float hz = 440.0f * float(std::pow(2, float(i) / 12));
    sineWaves.emplace_back(GenerateSineWave(hz, DURATION_SEC, sink.sampleSpec.rate));
  }

  // Play the sine waves
  std::cout << "Playing " << outputChannels << " sine waves for " << DURATION_SEC << " seconds\n";
  std::vector<float> interleaved;
  InterleaveChannels(interleaved, sineWaves);

  int writeRes;
  if (sink.sampleSpec.format == PA_SAMPLE_FLOAT32LE) {
    const size_t bytesToWrite = interleaved.size() * sizeof(float);
    writeRes =
      pa_stream_write(stream, interleaved.data(), bytesToWrite, nullptr, 0, PA_SEEK_RELATIVE);
  } else /*if (sink.sampleSpec.format == PA_SAMPLE_S24LE)*/ {
    // Convert to s24le
    const auto interleavedS24LE = FloatToS24LE(interleaved);
    const size_t bytesToWrite = interleavedS24LE.size();
    writeRes =
      pa_stream_write(stream, interleavedS24LE.data(), bytesToWrite, nullptr, 0, PA_SEEK_RELATIVE);
  }

  if (0 != writeRes) {
    std::cerr << "Failed to write to stream: " << pa_strerror(writeRes) << "\n";
    pa_stream_unref(stream);
    pulse::DestroyPulse(session.mainloop, session.context);
    return 1;
  }

  // Wait for the sine wave to finish playing
  pa_operation* op = pa_stream_drain(stream, nullptr, nullptr);
  // pa_threaded_mainloop_wait(session.mainloop);
  std::this_thread::sleep_for(1s);
  pa_operation_unref(op);

  // Cleanup
  std::cout << "Cleaning up\n";
  pa_stream_disconnect(stream);
  pa_stream_unref(stream);

  pulse::DestroyPulse(session.mainloop, session.context);
}
