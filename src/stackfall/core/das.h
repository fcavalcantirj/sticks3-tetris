#pragma once

#include <cstdint>

namespace sf {

constexpr uint16_t kDasMs = 167;
constexpr uint16_t kArrMs = 33;
constexpr int kMaxShiftsPerUpdate = 10;

// Horizontal auto-shift: DAS charge then ARR repeat.
// All time arrives as nowMs; this class never reads a clock.
// Caller applies the sign of the held direction; update returns
// the number of columns to shift on this call.
class AutoShift {
 public:
  enum class Dir : int8_t { None = 0, Left = -1, Right = 1 };

  AutoShift(uint16_t dasMs = kDasMs, uint16_t arrMs = kArrMs)
      : das_(dasMs), arr_(arrMs == 0u ? 1u : arrMs), dir_(Dir::None), charged_(false), mark_(0) {}

  void reset() {
    dir_ = Dir::None;
    charged_ = false;
    mark_ = 0;
  }

  int update(Dir held, uint32_t nowMs) {
    if (held == Dir::None) {
      dir_ = Dir::None;
      return 0;
    }
    if (held != dir_) {
      dir_ = held;
      charged_ = false;
      mark_ = nowMs;
      return 1;
    }
    if (!charged_) {
      if ((nowMs - mark_) >= das_) {
        charged_ = true;
        mark_ = mark_ + static_cast<uint32_t>(das_);
        return 1;
      }
      return 0;
    }
    int count = 0;
    while ((nowMs - mark_) >= arr_) {
      ++count;
      mark_ = mark_ + arr_;
      if (count >= kMaxShiftsPerUpdate) {
        mark_ = nowMs;
        break;
      }
    }
    return count;
  }

  Dir dir() const { return dir_; }

 private:
  uint16_t das_;
  uint16_t arr_;
  Dir dir_;
  bool charged_;
  uint32_t mark_;
};

}  // namespace sf
