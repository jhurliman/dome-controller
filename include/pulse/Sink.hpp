#pragma once

#include "Format.hpp"
#include "SinkPort.hpp"
#include "common.hpp"

#include <pulse/pulseaudio.h>

#include <optional>
#include <unordered_map>
#include <vector>

namespace pulse {

struct Sink {
  uint32_t index;
  uint32_t cardIndex;
  std::string name;
  std::string description;
  pa_sample_spec sampleSpec;
  pa_channel_map channelMap;
  uint32_t ownerModule;
  pa_cvolume volume;
  bool mute;
  uint32_t monitorSource;
  std::string monitorSourceName;
  pa_usec_t latency;
  std::string driver;
  pa_sink_flags_t flags;
  std::unordered_map<std::string, std::string> properties;
  pa_usec_t configuredLatency;
  pa_volume_t baseVolume;
  pa_sink_state_t state;
  uint32_t numVolumeSteps;
  uint32_t card;
  std::vector<SinkPort> ports;
  std::optional<SinkPort> activePort;
  std::vector<Format> formats;

  explicit Sink(const pa_sink_info* info)
    : index(info->index),
      cardIndex(info->card),
      name(info->name),
      description(info->description ? info->description : ""),
      sampleSpec(info->sample_spec),
      channelMap(info->channel_map),
      ownerModule(info->owner_module),
      volume(info->volume),
      mute(bool(info->mute)),
      monitorSource(info->monitor_source),
      monitorSourceName(info->monitor_source_name ? info->monitor_source_name : ""),
      latency(info->latency),
      driver(info->driver),
      flags(info->flags),
      properties(ProplistToMap(info->proplist)),
      configuredLatency(info->configured_latency),
      baseVolume(info->base_volume),
      state(info->state),
      numVolumeSteps(info->n_volume_steps),
      card(info->card) {
    if (info->n_ports > 0) {
      ports.reserve(info->n_ports);
      for (uint32_t i = 0; i < info->n_ports; ++i) {
        ports.emplace_back(SinkPort{info->ports[i]});
      }
    }

    if (info->active_port) { activePort = SinkPort{info->active_port}; }

    if (info->n_formats > 0) {
      formats.reserve(info->n_formats);
      for (uint8_t i = 0; i < info->n_formats; ++i) {
        formats.emplace_back(Format{info->formats[i]});
      }
    }
  }
};

} // namespace pulse
