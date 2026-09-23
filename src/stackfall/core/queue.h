#pragma once

#include <array>
#include <cstdint>

#include "stackfall/core/bag.h"

namespace sf {

// The queue owns the only Bag instance in the running game; nothing else may
// call Bag::next() once the game is playing.
class NextQueue {
 public:
  static constexpr int kMinBuffered = 7;
  static constexpr int kCapacity = 14;

  NextQueue()
      : bag_(0u),
        head_(0),
        count_(0),
        misuse_(false),
        minBuffered_(kMinBuffered),
        previewCount_(0) {}

  void configure(uint8_t minBuffered, uint8_t previewCount) {
    minBuffered_ = minBuffered;
    previewCount_ = previewCount;
  }

  void reset(uint32_t seed) {
    bag_.reset(seed);
    head_ = 0;
    count_ = 0;
    misuse_ = false;
    while (count_ < minBuffered_) {
      ring_[static_cast<size_t>((static_cast<int>(head_) + static_cast<int>(count_)) %
                                kCapacity)] = bag_.next();
      ++count_;
    }
  }

  PieceId pop() {
    PieceId front = ring_[head_];
    head_ = static_cast<uint8_t>((static_cast<int>(head_) + 1) % kCapacity);
    --count_;
    while (count_ < minBuffered_) {
      ring_[static_cast<size_t>((static_cast<int>(head_) + static_cast<int>(count_)) %
                                kCapacity)] = bag_.next();
      ++count_;
    }
    return front;
  }

  PieceId peek(int i) const {
    if (i < 0 || i >= static_cast<int>(count_)) {
      misuse_ = true;
      return PieceId::I;
    }
    return ring_[static_cast<size_t>((static_cast<int>(head_) + i) % kCapacity)];
  }

  int size() const { return static_cast<int>(count_); }
  uint8_t previewCount() const { return previewCount_; }
  bool misuse() const { return misuse_; }

 private:
  Bag bag_;
  std::array<PieceId, kCapacity> ring_;
  uint8_t head_;
  uint8_t count_;
  mutable bool misuse_;
  uint8_t minBuffered_;
  uint8_t previewCount_;
};

static_assert(NextQueue::kCapacity >= 2 * NextQueue::kMinBuffered, "queue holds two bags");

}  // namespace sf
