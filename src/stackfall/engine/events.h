#pragma once

#include <cstdint>

namespace sf {

// Game events pushed by the engine and consumed once per frame by audio,
// the UI banner and the serial trace printer. Fixed-size ring buffer: no
// callbacks, no std::function, no heap.
enum class EventType : uint8_t {
  Spawn,
  Move,
  Rotate,
  Kick,
  SoftDrop,
  HardDrop,
  Hold,
  HoldDenied,
  Lock,
  LineClear,
  TSpin,
  Combo,
  BackToBack,
  PerfectClear,
  LevelUp,
  GameOver
};

// POD event. `a` and `b` are discriminators; `value` carries the score
// component or other signed quantity; `tick` is the engine tick count
// (elapsedMs_ / tickMs_) at which the event was raised.
struct GameEvent {
  EventType type;
  uint8_t a;
  uint8_t b;
  int32_t value;
  uint32_t tick;
};

class EventQueue {
 public:
  static constexpr uint8_t kCapacity = 16;

  void push(const GameEvent& ev) {
    if (count_ == kCapacity) {
      // Full: overwrite the oldest entry and keep the newest. The oldest is
      // at head_; advance head_ and reuse that slot.
      head_ = static_cast<uint8_t>((head_ + 1) % kCapacity);
      ++dropped_;
    } else {
      ++count_;
    }
    uint8_t idx = static_cast<uint8_t>((head_ + count_ - 1) % kCapacity);
    buf_[idx] = ev;
  }

  bool pop(GameEvent& out) {
    if (count_ == 0) {
      return false;
    }
    out = buf_[head_];
    head_ = static_cast<uint8_t>((head_ + 1) % kCapacity);
    --count_;
    return true;
  }

  uint8_t size() const { return count_; }
  uint16_t dropped() const { return dropped_; }
  void resetDropped() { dropped_ = 0; }

 private:
  GameEvent buf_[kCapacity];
  uint8_t head_ = 0;
  uint8_t count_ = 0;
  uint16_t dropped_ = 0;
};

}  // namespace sf
