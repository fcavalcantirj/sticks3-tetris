#pragma once

#include <cstdint>

#include "stackfall/core/piece.h"

namespace sf {

// Hold keeps a bare PieceId only; the swapped-in piece always respawns
// in its spawn orientation, so a turn already fired on the press edge
// is absorbed and harmless when the press turns out to be a long hold.
class Hold {
 public:
  enum class Result : uint8_t { Filled, Swapped, Refused };

  void reset() {
    held_ = PieceId::I;
    has_ = false;
    used_ = false;
  }

  bool empty() const { return !has_; }
  PieceId piece() const { return held_; }
  bool used() const { return used_; }

  Result swap(PieceId incoming, PieceId& outgoing) {
    if (used_) {
      return Result::Refused;
    }
    if (!has_) {
      held_ = incoming;
      has_ = true;
      used_ = true;
      return Result::Filled;
    }
    outgoing = held_;
    held_ = incoming;
    used_ = true;
    return Result::Swapped;
  }

  void onPieceLocked() { used_ = false; }

 private:
  PieceId held_ = PieceId::I;
  bool has_ = false;
  bool used_ = false;
};

static_assert(sizeof(Hold) <= 4, "hold fits in 4 bytes");

}  // namespace sf
