#pragma once

#include <pulse/proplist.h>

#include <memory>

namespace pulse {

class ScopedPropertyList {
public:
  ScopedPropertyList() : propertyList_(pa_proplist_new()) {}

  ScopedPropertyList(const ScopedPropertyList&) = delete;
  ScopedPropertyList& operator=(const ScopedPropertyList&) = delete;

  pa_proplist* get() const { return propertyList_.get(); }

  int setValue(const char* key, const char* value) {
    return pa_proplist_sets(propertyList_.get(), key, value);
  }

private:
  using deleter = std::integral_constant<decltype(pa_proplist_free)*, pa_proplist_free>;
  std::unique_ptr<pa_proplist, deleter> propertyList_;
};

} // namespace pulse
