#pragma once

#include <cstdint>

#include "stackfall/input/action.h"

namespace sf::input {

inline constexpr float kDipEngageG = 0.25f;
inline constexpr float kDipReleaseG = 0.15f;
inline constexpr uint32_t kDipNeutralDwellMs = 90;
inline constexpr uint32_t kDipSoftDropDwellMs = 120;

// In the upright playing grip +X points down the stick and +Z points out of
// the screen toward the player. When the IR end (-X) tips away, +Z tips upward;
// the measured accelerometer convention therefore makes raw Z increase.
// Corrected-positive always means "IR end away from the face" below.
inline constexpr int8_t kDipSignAway = 1;

// Signed front-dip detector for raw accelerometer Z samples in G. The caller
// supplies sample timing and action timestamps; this class reads no clock.
//
// The 0.25 G engage threshold is 14.5 degrees. A natural 45 degree dip puts
// about 0.707 G on Z (2.8x above engage), while steering to a 21.1 degree board
// edge bleeds only 0.067 G into this plane (3.7x below engage). The larger
// engage and smaller 0.15 G release thresholds form the deliberate Schmitt
// band that prevents a held gesture from chattering.
class Dip {
 public:
  void reset() {
    filteredG_ = 0.0f;
    neutralG_ = 0.0f;
    direction_ = Direction::Neutral;
    engaged_ = false;
    filterReady_ = false;
    neutralDwellRequired_ = false;
    neutralDwellActive_ = false;
    towardDwellActive_ = false;
  }

  // Capture is an outright assignment. It also snaps the EMA to the sample so
  // entering play or explicitly re-zeroing takes effect immediately.
  void captureNeutral(float zG) {
    const float correctedG = correctSign(zG);
    filteredG_ = correctedG;
    neutralG_ = correctedG;
    direction_ = Direction::Neutral;
    engaged_ = false;
    filterReady_ = true;
    neutralDwellRequired_ = false;
    neutralDwellActive_ = false;
    towardDwellActive_ = false;
  }

  void update(float zG, uint32_t dtMs, uint32_t nowMs, ActionQueue& out) {
    if (!filterReady_) {
      captureNeutral(zG);
      return;
    }

    const uint32_t clampedDtMs = dtMs > kMaxDtMs ? kMaxDtMs : dtMs;
    const float dt = static_cast<float>(clampedDtMs);
    const float alpha = dt / (kFilterRcMs + dt);
    const float correctedG = correctSign(zG);
    filteredG_ += alpha * (correctedG - filteredG_);

    const float signalG = filteredG_ - neutralG_;
    // Steering freezes as soon as either direction is beyond the release
    // threshold, including the approach through the Schmitt band.
    engaged_ = signalG > kDipReleaseG || signalG < -kDipReleaseG;

    switch (direction_) {
      case Direction::Neutral:
        updateNeutral(signalG, nowMs, out);
        break;

      case Direction::Away:
        // Away is edge-triggered. It cannot fire again until the signal has
        // stayed inside the release band long enough to re-arm. A return-stroke
        // overshoot outside the opposite edge is still the same away gesture.
        if (insideReleaseBand(signalG)) {
          direction_ = Direction::Neutral;
          beginNeutralDwell(nowMs);
          return;
        }
        break;

      case Direction::Toward:
        // Toward is level-triggered: SoftDropOn remains owed until the signal
        // returns inside the release band. It cannot change direction on the
        // same sample, which keeps a return-stroke overshoot paired correctly.
        if (insideReleaseBand(signalG)) {
          direction_ = Direction::Neutral;
          out.push(ActionKind::SoftDropOff, nowMs);
          beginNeutralDwell(nowMs);
          return;
        }
        break;
    }
  }

  // True in either physical direction once Z is outside the release band. The
  // steering controller consumes this directly to freeze left/right motion.
  bool engaged() const { return engaged_; }

  bool softDropActive() const { return direction_ == Direction::Toward; }
  float signalG() const { return filteredG_ - neutralG_; }

 private:
  enum class Direction : uint8_t { Neutral, Away, Toward };

  static constexpr float kFilterRcMs = 80.0f;
  static constexpr uint32_t kMaxDtMs = 40;

  static float correctSign(float zG) {
    return zG * static_cast<float>(kDipSignAway);
  }

  static bool insideReleaseBand(float signalG) {
    return signalG >= -kDipReleaseG && signalG <= kDipReleaseG;
  }

  void beginNeutralDwell(uint32_t nowMs) {
    neutralDwellRequired_ = true;
    neutralDwellActive_ = true;
    neutralSinceMs_ = nowMs;
    towardDwellActive_ = false;
  }

  void updateNeutral(float signalG, uint32_t nowMs, ActionQueue& out) {
    if (neutralDwellRequired_) {
      if (!insideReleaseBand(signalG)) {
        neutralDwellActive_ = false;
        towardDwellActive_ = false;
        return;
      }
      if (!neutralDwellActive_) {
        neutralDwellActive_ = true;
        neutralSinceMs_ = nowMs;
        return;
      }
      if (nowMs - neutralSinceMs_ < kDipNeutralDwellMs) {
        return;
      }
      neutralDwellRequired_ = false;
      neutralDwellActive_ = false;
    }

    if (signalG >= kDipEngageG) {
      towardDwellActive_ = false;
      engageFromNeutral(signalG, nowMs, out);
      return;
    }

    if (signalG > -kDipEngageG) {
      towardDwellActive_ = false;
      return;
    }

    // If 120 ms proves insufficient, raising the engage threshold for the
    // toward direction only is the second lever. Keep the shared 0.25 G
    // threshold unchanged until hands-on evidence asks for that adjustment.
    if (!towardDwellActive_) {
      towardDwellActive_ = true;
      towardSinceMs_ = nowMs;
      return;
    }
    if (nowMs - towardSinceMs_ >= kDipSoftDropDwellMs) {
      towardDwellActive_ = false;
      engageFromNeutral(signalG, nowMs, out);
    }
  }

  // This helper performs a qualified transition. Neutral samples reach it only
  // after the direction-specific dwell policy in updateNeutral().
  void engageFromNeutral(float signalG, uint32_t nowMs, ActionQueue& out) {
    if (signalG >= kDipEngageG) {
      direction_ = Direction::Away;
      out.push(ActionKind::RotateCw, nowMs);
    } else if (signalG <= -kDipEngageG) {
      direction_ = Direction::Toward;
      out.push(ActionKind::SoftDropOn, nowMs);
    }
  }

  float filteredG_ = 0.0f;
  float neutralG_ = 0.0f;
  Direction direction_ = Direction::Neutral;
  bool engaged_ = false;
  bool filterReady_ = false;
  bool neutralDwellRequired_ = false;
  bool neutralDwellActive_ = false;
  bool towardDwellActive_ = false;
  uint32_t neutralSinceMs_ = 0;
  uint32_t towardSinceMs_ = 0;
};

}  // namespace sf::input
