#include "hal/sticks3/sysinfo.h"

#include <M5Unified.h>

namespace sf_hal {

uint32_t freeHeap() {
  return static_cast<uint32_t>(ESP.getFreeHeap());
}

uint32_t minFreeHeap() {
  return static_cast<uint32_t>(ESP.getMinFreeHeap());
}

uint32_t psramBytes() {
  return static_cast<uint32_t>(ESP.getPsramSize());
}

}  // namespace sf_hal
