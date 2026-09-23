#pragma once

#include "stackfall/core/board.h"
#include "stackfall/core/spawn.h"
#include "stackfall/core/srs.h"

namespace sf {

struct Kick {
  int8_t dCol;
  int8_t dRow;
};

constexpr bool operator==(Kick a, Kick b) {
  return a.dCol == b.dCol && a.dRow == b.dRow;
}

constexpr bool operator!=(Kick a, Kick b) { return !(a == b); }

// Wall and floor kicks for J, L, S, T and Z, indexed by transitionIndex.
// dRow is positive DOWNWARD, matching the Board: each entry below is already
// the vertical negation of the classic y-positive-UPWARD value.
constexpr Kick kKicksJLSTZ[8][5] = {
    // 0>>R
    {Kick{0, 0}, Kick{-1, 0}, Kick{-1, -1}, Kick{0, 2}, Kick{-1, 2}},
    // R>>0
    {Kick{0, 0}, Kick{1, 0}, Kick{1, 1}, Kick{0, -2}, Kick{1, -2}},
    // R>>2
    {Kick{0, 0}, Kick{1, 0}, Kick{1, 1}, Kick{0, -2}, Kick{1, -2}},
    // 2>>R
    {Kick{0, 0}, Kick{-1, 0}, Kick{-1, -1}, Kick{0, 2}, Kick{-1, 2}},
    // 2>>L
    {Kick{0, 0}, Kick{1, 0}, Kick{1, -1}, Kick{0, 2}, Kick{1, 2}},
    // L>>2
    {Kick{0, 0}, Kick{-1, 0}, Kick{-1, 1}, Kick{0, -2}, Kick{-1, -2}},
    // L>>0
    {Kick{0, 0}, Kick{-1, 0}, Kick{-1, 1}, Kick{0, -2}, Kick{-1, -2}},
    // 0>>L
    {Kick{0, 0}, Kick{1, 0}, Kick{1, -1}, Kick{0, 2}, Kick{1, 2}},
};

// The O piece is identical in all four states, so a rotation is a no-op
// that always succeeds at candidate 0.
constexpr Kick kKicksNone[8][5] = {
    {Kick{0, 0}, Kick{0, 0}, Kick{0, 0}, Kick{0, 0}, Kick{0, 0}},
    {Kick{0, 0}, Kick{0, 0}, Kick{0, 0}, Kick{0, 0}, Kick{0, 0}},
    {Kick{0, 0}, Kick{0, 0}, Kick{0, 0}, Kick{0, 0}, Kick{0, 0}},
    {Kick{0, 0}, Kick{0, 0}, Kick{0, 0}, Kick{0, 0}, Kick{0, 0}},
    {Kick{0, 0}, Kick{0, 0}, Kick{0, 0}, Kick{0, 0}, Kick{0, 0}},
    {Kick{0, 0}, Kick{0, 0}, Kick{0, 0}, Kick{0, 0}, Kick{0, 0}},
    {Kick{0, 0}, Kick{0, 0}, Kick{0, 0}, Kick{0, 0}, Kick{0, 0}},
    {Kick{0, 0}, Kick{0, 0}, Kick{0, 0}, Kick{0, 0}, Kick{0, 0}},
};

// I-piece wall and floor kicks, indexed by transitionIndex.
// Same downward-positive dRow convention as kKicksJLSTZ. The two-column
// horizontal kicks are what let a flat I slide into a well.
constexpr Kick kKicksI[8][5] = {
    // 0>>R
    {Kick{0, 0}, Kick{-2, 0}, Kick{1, 0}, Kick{-2, 1}, Kick{1, -2}},
    // R>>0
    {Kick{0, 0}, Kick{2, 0}, Kick{-1, 0}, Kick{2, -1}, Kick{-1, 2}},
    // R>>2
    {Kick{0, 0}, Kick{-1, 0}, Kick{2, 0}, Kick{-1, -2}, Kick{2, 1}},
    // 2>>R
    {Kick{0, 0}, Kick{1, 0}, Kick{-2, 0}, Kick{1, 2}, Kick{-2, -1}},
    // 2>>L
    {Kick{0, 0}, Kick{2, 0}, Kick{-1, 0}, Kick{2, -1}, Kick{-1, 2}},
    // L>>2
    {Kick{0, 0}, Kick{-2, 0}, Kick{1, 0}, Kick{-2, 1}, Kick{1, -2}},
    // L>>0
    {Kick{0, 0}, Kick{1, 0}, Kick{-2, 0}, Kick{1, 2}, Kick{-2, -1}},
    // 0>>L
    {Kick{0, 0}, Kick{-1, 0}, Kick{2, 0}, Kick{-1, -2}, Kick{2, 1}},
};

// Pointer to the five candidates for (piece, transition), or nullptr when
// the transition index is outside 0..7.
inline const Kick* kickCandidates(PieceId p, int transitionIdx) {
  if (transitionIdx < 0 || transitionIdx >= kTransitionCount) return nullptr;
  if (p == PieceId::O) return kKicksNone[transitionIdx];
  if (p == PieceId::I) return kKicksI[transitionIdx];
  return kKicksJLSTZ[transitionIdx];
}

// Try turning the piece one step CW or CCW with wall and floor kicks.
// On success writes the new state, col and row into piece, sets
// kickIndexOut to the winning candidate 0..4 and returns true. On failure
// leaves piece byte-identical, sets kickIndexOut to -1 and returns false.
// kickIndexOut is load-bearing: later rules classify mini versus full turns
// partly from it, so it is the true candidate index, never clamped.
inline bool tryRotate(const Board& b, ActivePiece& piece, Turn t,
                      int& kickIndexOut) {
  Rot from = static_cast<Rot>(piece.state & 3);
  Rot to = rotate(from, t);
  int idx = transitionIndex(from, to);
  const Kick* kicks = kickCandidates(piece.id, idx);
  if (idx < 0 || kicks == nullptr) {
    kickIndexOut = -1;
    return false;
  }
  int toState = static_cast<int>(to);
  for (int k = 0; k < 5; ++k) {
    int c = static_cast<int>(piece.col) + static_cast<int>(kicks[k].dCol);
    int r = static_cast<int>(piece.row) + static_cast<int>(kicks[k].dRow);
    if (!b.collides(piece.id, toState, c, r)) {
      piece.state = static_cast<uint8_t>(toState);
      piece.col = static_cast<int8_t>(c);
      piece.row = static_cast<int8_t>(r);
      kickIndexOut = k;
      return true;
    }
  }
  kickIndexOut = -1;
  return false;
}

}  // namespace sf
