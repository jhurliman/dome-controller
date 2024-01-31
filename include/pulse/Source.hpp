#pragma once

#include "Format.hpp"
#include "SourcePort.hpp"
#include "common.hpp"

#include <pulse/pulseaudio.h>

#include <optional>
#include <unordered_map>
#include <vector>

namespace pulse {

struct Source {
  uint32_t index;
  uint32_t cardIndex;
  std::string name;
  std::string description;
  pa_sample_spec sampleSpec;
  pa_cvolume volume;
  pa_volume_t baseVolume;
  bool mute;
  pa_usec_t latency;
  pa_usec_t configuredLatency;
  std::string driver;
  pa_source_flags_t flags;
  std::unordered_map<std::string, std::string> properties;
  pa_source_state_t state;
  uint32_t numVolumeSteps;
  std::vector<SourcePort> ports;
  std::optional<SourcePort> activePort;
  std::vector<Format> formats;

  explicit Source(const pa_source_info* info)
    : index(info->index),
      cardIndex(info->card),
      name(info->name),
      description(info->description),
      sampleSpec(info->sample_spec),
      volume(info->volume),
      baseVolume(info->base_volume),
      mute(bool(info->mute)),
      latency(info->latency),
      configuredLatency(info->configured_latency),
      driver(info->driver),
      flags(info->flags),
      properties(ProplistToMap(info->proplist)),
      state(info->state),
      numVolumeSteps(info->n_volume_steps) {
    if (info->n_ports > 0) {
      ports.reserve(info->n_ports);
      for (uint32_t i = 0; i < info->n_ports; ++i) {
        ports.emplace_back(SourcePort{info->ports[i]});
      }
    }

    if (info->active_port) { activePort = SourcePort{info->active_port}; }

    if (info->n_formats > 0) {
      formats.reserve(info->n_formats);
      for (uint8_t i = 0; i < info->n_formats; ++i) {
        formats.emplace_back(Format{info->formats[i]});
      }
    }
  }
};

} // namespace pulse
