#pragma once

#include <cstdint>

namespace sf {

constexpr uint16_t kLockDelayMs = 500;
constexpr uint8_t kMaxMoveResets = 15;

// Lock delay with capped move resets. All time arrives as nowMs;
// this class never reads a clock.
//
// kLockDelayMs and kMaxMoveResets are the DEFAULT values carried by
// RuleProfile; Game calls configure() from the active profile so custom
// timings are honored.
class LockDelay {
 public:
  void configure(uint16_t lockDelayMs, uint8_t lockResetLimit) {
    lockDelayMs_ = lockDelayMs;
    lockResetLimit_ = lockResetLimit;
    reset();
  }

  void reset() {
    deadlineStart_ = 0;
    grounded_ = false;
    resets_ = 0;
  }

  void onLocked() { reset(); }

  void onGrounded(uint32_t nowMs) {
    if (grounded_) {
      return;
    }
    grounded_ = true;
    deadlineStart_ = nowMs;
  }

  void onAirborne() { grounded_ = false; }

  bool onMoveReset(uint32_t nowMs) {
    if (!grounded_) {
      return false;
    }
    if (resets_ >= lockResetLimit_) {
      return false;
    }
    deadlineStart_ = nowMs;
    ++resets_;
    return true;
  }

  bool expired(uint32_t nowMs) const {
    if (!grounded_) {
      return false;
    }
    return (nowMs - deadlineStart_) >= lockDelayMs_;
  }

  bool grounded() const { return grounded_; }
  uint8_t resets() const { return resets_; }
  uint32_t deadlineStart() const { return deadlineStart_; }
  uint16_t lockDelayMs() const { return lockDelayMs_; }
  uint8_t lockResetLimit() const { return lockResetLimit_; }

 private:
  uint32_t deadlineStart_ = 0;
  bool grounded_ = false;
  uint8_t resets_ = 0;
  uint16_t lockDelayMs_ = kLockDelayMs;
  uint8_t lockResetLimit_ = kMaxMoveResets;
};

}  // namespace sf
