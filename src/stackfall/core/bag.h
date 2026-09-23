#pragma once
#include <array>
#include <cstdint>
#include <utility>

#include "stackfall/core/piece.h"

namespace sf {

class XorShift32 {
 public:
  explicit XorShift32(uint32_t seed) : s_(seed == 0u ? 0x9E3779B9u : seed) {}
  uint32_t next() {
    s_ ^= s_ << 13;
    s_ ^= s_ >> 17;
    s_ ^= s_ << 5;
    return s_;
  }
  uint32_t state() const { return s_; }
  void reseed(uint32_t seed) { s_ = (seed == 0u ? 0x9E3779B9u : seed); }

 private:
  uint32_t s_;
};

class Bag {
 public:
  explicit Bag(uint32_t seed) : rng_(seed), index_(7) { refill(); }
  void reset(uint32_t seed) {
    rng_.reseed(seed);
    refill();
  }
  PieceId next() {
    if (index_ == 7) {
      refill();
    }
    return bag_[index_++];
  }

 private:
  XorShift32 rng_;
  std::array<PieceId, 7> bag_;
  uint8_t index_;
  void refill() {
    bag_[0] = PieceId::I;
    bag_[1] = PieceId::J;
    bag_[2] = PieceId::L;
    bag_[3] = PieceId::O;
    bag_[4] = PieceId::S;
    bag_[5] = PieceId::T;
    bag_[6] = PieceId::Z;
    for (int i = 6; i >= 1; --i) {
      uint32_t j = rng_.next() % static_cast<uint32_t>(i + 1);
      std::swap(bag_[static_cast<size_t>(i)], bag_[static_cast<size_t>(j)]);
    }
    index_ = 0;
  }
};

}  // namespace sf
