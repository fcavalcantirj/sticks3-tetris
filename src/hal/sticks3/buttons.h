#pragma once

#include <cstdint>

namespace sf_hal {

struct RawButtons {
  bool blue;
  bool side;
  uint32_t tMs;
};

// Central M5Unified service poll. Both the shipped game and archived probe
// dispatcher call this so M5.update() has exactly one owner in the tree.
void updateDevice();
RawButtons sampleButtons(uint32_t nowMs);

}  // namespace sf_hal
