#pragma once
#include <cstdint>

#include "stackfall/core/board.h"

namespace sf {

struct ActivePiece {
  PieceId id;
  uint8_t state;
  int8_t col;
  int8_t row;
};

static_assert(sizeof(ActivePiece) == 4, "active piece is 4 bytes");

// Origin row 20 places the J/L/S/T/Z nub on board row 20 and its three-cell
// body on row 21, places the I bar on row 21, and places the O square across
// rows 20 and 21 -- all seven pieces therefore appear on the top two VISIBLE
// rows, and rows 0..19 stay free for kicks and lock-out.
constexpr ActivePiece spawnOf(PieceId p) {
  return ActivePiece{p, 0, static_cast<int8_t>(p == PieceId::O ? 4 : 3), 20};
}

enum class TopOut : uint8_t { None = 0, BlockOut = 1, LockOut = 2 };

inline TopOut classifySpawn(const Board& b, PieceId p) {
  ActivePiece s = spawnOf(p);
  if (b.collides(p, 0, s.col, s.row)) return TopOut::BlockOut;
  return TopOut::None;
}

inline bool trySpawn(const Board& b, PieceId p, ActivePiece& out) {
  if (classifySpawn(b, p) != TopOut::None) return false;
  out = spawnOf(p);
  return true;
}

inline TopOut classifyLock(const ActivePiece& piece) {
  const Shape& s = shapeOf(piece.id, piece.state);
  for (int i = 0; i < 4; ++i) {
    int r = static_cast<int>(piece.row) + static_cast<int>(s[static_cast<size_t>(i)].row);
    if (r >= Board::kFirstVisibleRow) return TopOut::None;
  }
  return TopOut::LockOut;
}

}  // namespace sf
