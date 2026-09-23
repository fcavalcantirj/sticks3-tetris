#pragma once

#include "stackfall/core/board.h"
#include "stackfall/core/spawn.h"

namespace sf {

// The ghost is recomputed from state, never cached across frames.
inline int ghostRow(const Board& b, const ActivePiece& p) {
  if (b.collides(p.id, static_cast<int>(p.state), static_cast<int>(p.col),
                 static_cast<int>(p.row))) {
    return static_cast<int>(p.row);
  }
  int row = static_cast<int>(p.row);
  for (int step = 0; step < Board::kTotalRows; ++step) {
    if (b.collides(p.id, static_cast<int>(p.state), static_cast<int>(p.col), row + 1)) {
      break;
    }
    ++row;
  }
  return row;
}

inline int dropDistance(const Board& b, const ActivePiece& p) {
  return ghostRow(b, p) - static_cast<int>(p.row);
}

}  // namespace sf
