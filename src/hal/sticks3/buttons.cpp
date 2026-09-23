#include "hal/sticks3/buttons.h"

#include <M5Unified.h>

namespace sf_hal {

void updateDevice() {
  M5.update();
}

RawButtons sampleButtons(uint32_t nowMs) {
  return RawButtons{
      M5.BtnA.isPressed(),
      M5.BtnB.isPressed(),
      nowMs,
  };
}

}  // namespace sf_hal
