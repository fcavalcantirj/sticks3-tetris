#pragma once

#include <cstdint>

namespace sf {

// ms per cell = round(1000 * (0.8 - (level - 1) * 0.007) ^ (level - 1)), levels 1..15.
constexpr uint16_t kGravityMsByLevel[15] = {1000, 793, 618, 473, 355, 262, 190,
                                            135,  94,  64,  43,  28,  18,  11,  7};

constexpr uint16_t kSoftDropMsPerCell = 20;
constexpr int kMaxCellsPerStep = 20;

constexpr uint16_t gravityMsForLevel(int level) {
  if (level <= 1) {
    return kGravityMsByLevel[0];
  }
  if (level >= 15) {
    return kGravityMsByLevel[14];
  }
  return kGravityMsByLevel[level - 1];
}

// Fixed-timestep gravity: the caller passes dtMs, no clock read here.
class Gravity {
 public:
  void reset(int level) {
    level_ = level;
    acc_ = 0;
  }

  void setLevel(int level) { level_ = level; }

  int level() const { return level_; }
  uint32_t accumulatorMs() const { return acc_; }

  int step(uint32_t dtMs, bool softDropHeld) {
    acc_ += dtMs;
    uint16_t natural = gravityMsForLevel(level_);
    uint16_t period = natural;
    if (softDropHeld && kSoftDropMsPerCell < period) {
      period = kSoftDropMsPerCell;
    }
    uint32_t cells = acc_ / static_cast<uint32_t>(period);
    if (cells > static_cast<uint32_t>(kMaxCellsPerStep)) {
      acc_ = 0;
      return kMaxCellsPerStep;
    }
    acc_ = acc_ % static_cast<uint32_t>(period);
    return static_cast<int>(cells);
  }

 private:
  int level_ = 1;
  uint32_t acc_ = 0;
};

}  // namespace sf
