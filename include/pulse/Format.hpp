#pragma once

#include "common.hpp"

#include <pulse/format.h>

#include <string>
#include <unordered_map>

namespace pulse {

struct Format {
  pa_encoding_t encoding;
  std::unordered_map<std::string, std::string> properties;

  explicit Format(const pa_format_info* info) : encoding(info->encoding) {
    if (info->plist) { properties = ProplistToMap(info->plist); }
  }
};

} // namespace pulse
