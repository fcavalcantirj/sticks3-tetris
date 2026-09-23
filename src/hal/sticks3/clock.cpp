#include "hal/sticks3/clock.h"

#include <esp_timer.h>

namespace sf_hal {

uint32_t nowMs() {
  return static_cast<uint32_t>(esp_timer_get_time() / 1000);
}

uint64_t nowUs() {
  return static_cast<uint64_t>(esp_timer_get_time());
}

}  // namespace sf_hal
