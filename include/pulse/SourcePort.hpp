#pragma once

#include <pulse/introspect.h>

#include <string>

namespace pulse {

struct SourcePort {
  std::string name;
  std::string description;
  uint32_t priority;
  bool available;
  std::string availabilityGroup;
  pa_device_port_type_t type;

  explicit SourcePort(const pa_source_port_info* info)
    : name(info->name),
      description(info->description ? info->description : ""),
      priority(info->priority),
      available(bool(info->available)),
      availabilityGroup(info->availability_group ? info->availability_group : ""),
      type(pa_device_port_type_t(info->type)) {}
};

} // namespace pulse
