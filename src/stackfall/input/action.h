#pragma once

#include <array>
#include <cstdint>

namespace sf::input {

enum class ActionKind : uint8_t {
  None,
  MoveLeft,
  MoveRight,
  SoftDropOn,
  SoftDropOff,
  RotateCw,
  RotateCcw,
  HardDrop,
  Hold,
  Pause,
  Confirm,
  Back,
  MenuUp,
  MenuDown,
  Diag,
  Calibrate,
  TiltSpeed,
};

struct GameAction {
  uint32_t tMs;
  ActionKind kind;
  uint8_t seq;
};

static_assert(sizeof(GameAction) == 8, "GameAction wire payload is eight bytes");

class ActionQueue {
 public:
  bool push(ActionKind kind, uint32_t tMs) {
    if (kind == ActionKind::None) {
      return false;
    }

    // The queue is drained every 5 ms poll, so fullness means the consumer
    // stalled. Drop the oldest action and preserve the newest player intent;
    // a fresh HardDrop must not lose to a stale MoveLeft.
    if (count_ == kCapacity) {
      head_ = advance(head_);
      ++dropped_;
    } else {
      ++count_;
    }

    buf_[tail_] = GameAction{tMs, kind, seq_++};
    tail_ = advance(tail_);
    return true;
  }

  bool pop(GameAction& out) {
    if (count_ == 0) {
      return false;
    }

    out = buf_[head_];
    head_ = advance(head_);
    --count_;
    return true;
  }

  uint8_t size() const { return count_; }
  uint8_t capacity() const { return kCapacity; }
  uint16_t dropped() const { return dropped_; }

  void clear() {
    head_ = 0;
    tail_ = 0;
    count_ = 0;
  }

 private:
  static constexpr uint8_t kCapacity = 16;

  static constexpr uint8_t advance(uint8_t index) {
    return static_cast<uint8_t>((index + 1u) % kCapacity);
  }

  std::array<GameAction, kCapacity> buf_{};
  uint8_t head_ = 0;
  uint8_t tail_ = 0;
  uint8_t count_ = 0;
  uint8_t seq_ = 0;
  uint16_t dropped_ = 0;
};

}  // namespace sf::input
