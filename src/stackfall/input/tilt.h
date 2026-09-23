#pragma once

#include <cstdint>
#include <cmath>

#include "imu_axes.h"

namespace sf {

constexpr uint16_t clampDtMs(uint32_t delta, uint16_t lo, uint16_t hi) {
  if (delta < lo) {
    return lo;
  }
  if (delta > hi) {
    return hi;
  }
  return static_cast<uint16_t>(delta);
}

}  // namespace sf

namespace sf::input {

inline constexpr float kGPerColumn = 0.09f;
inline constexpr float kColHysteresis = 0.02f;

// Absolute left/right steering for the upright playing grip. The caller passes
// the raw accelerometer Y component in G; this pure controller applies the
// measured board sign, filters it, and returns one selected board column.
// There is deliberately no stepping, DAS, ARR, clock read, or neutral drift.
class Tilt {
 public:
  void reset() {
    filteredG_ = 0.0f;
    neutralG_ = 0.0f;
    column_ = 4;
    filterReady_ = false;
  }

  // Capture is an outright assignment, never an adaptive target. It also
  // snaps the EMA to the captured sample so re-zeroing takes effect now rather
  // than one RC later. Neutral is the 4/5 boundary; preserve which side the
  // previous selection approached it from.
  void captureNeutral(float sig) {
    const float correctedG = correctSign(sig);
    filteredG_ = correctedG;
    neutralG_ = correctedG;
    column_ = column_ <= 4 ? 4 : 5;
    filterReady_ = true;
  }

  int update(float yG, uint32_t dtMs, bool dipEngaged) {
    const float correctedG = correctSign(yG);
    if (!filterReady_) {
      captureNeutral(yG);
      return column_;
    }

    const uint32_t clampedDtMs = dtMs > kMaxDtMs ? kMaxDtMs : dtMs;
    const float dt = static_cast<float>(clampedDtMs);
    const float alpha = dt / (kFilterRcMs + dt);
    filteredG_ += alpha * (correctedG - filteredG_);

    // A dip freezes selection while its cross-axis roll passes through this
    // filter. The captured neutral is immutable in both dip and steering paths.
    if (!dipEngaged) {
      select(filteredG_ - neutralG_);
    }
    return column_;
  }

  int column() const { return column_; }
  float signalG() const { return filteredG_ - neutralG_; }

 private:
  static constexpr float kFilterRcMs = 80.0f;
  static constexpr uint32_t kMaxDtMs = 40;
  static constexpr int kColumnCount = 10;
  static constexpr float kEdgeG = 4.0f * kGPerColumn;

  static float correctSign(float yG) {
    return yG * static_cast<float>(kTiltSignLeftRight);
  }

  static int quantise(float sig) {
    int candidate = static_cast<int>(std::floor(sig / kGPerColumn + 5.0f));

    // The endpoint ties belong to the outward columns so both board edges are
    // reachable at the same measured magnitude, exactly -0.36 G and +0.36 G.
    if (sig <= -kEdgeG) {
      candidate = 0;
    } else if (sig >= kEdgeG) {
      candidate = kColumnCount - 1;
    }

    if (candidate < 0) {
      return 0;
    }
    if (candidate >= kColumnCount) {
      return kColumnCount - 1;
    }
    return candidate;
  }

  void select(float sig) {
    const int candidate = quantise(sig);
    if (candidate == column_) {
      return;
    }

    const float boundary =
        ((static_cast<float>(column_ + candidate) / 2.0f) - 4.5f) * kGPerColumn;
    if ((candidate > column_ && sig > boundary + kColHysteresis) ||
        (candidate < column_ && sig < boundary - kColHysteresis)) {
      column_ = candidate;
    }
  }

  float filteredG_ = 0.0f;
  float neutralG_ = 0.0f;
  int column_ = 4;
  bool filterReady_ = false;
};

}  // namespace sf::input
