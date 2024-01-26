#pragma once

#include <pulse/introspect.h>

#include <string>

namespace pulse {

struct SinkPort {
  std::string name;
  std::string description;
  uint32_t priority;
  bool available;

  explicit SinkPort(const pa_sink_port_info* info)
    : name(info->name),
      description(info->description ? info->description : ""),
      priority(info->priority),
      available(bool(info->available)) {}
};

} // namespace pulse
