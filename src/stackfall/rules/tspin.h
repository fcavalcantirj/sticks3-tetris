#pragma once

#include <cstdint>

#include "stackfall/core/board.h"
#include "stackfall/core/piece.h"
#include "stackfall/rules/scoring.h"

namespace sf {

struct SpinQuery {
  PieceId piece;
  uint8_t state;
  int8_t boxCol;
  int8_t boxRow;
  int8_t kickIndex;
  bool lastMoveWasRotation;
};

// Walls and floor count as occupied; the open sky above row 0 counts as
// empty. That sky rule is the whole point of this predicate: the collision
// primitive treats every out-of-matrix cell as solid, which is right for
// movement and wrong for the air above the well.
constexpr bool cornerOccupied(const Board& b, int col, int row) {
  if (row < 0) {
    return false;
  }
  if (col < 0 || col >= Board::kWidth || row >= Board::kTotalRows) {
    return true;
  }
  return b.at(col, row) != Cell::Empty;
}

// Front is the pair on the side the T nub points at:
// state 0 (nub up) front = top {TL,TR}; state 1 (nub right) front = right
// {TR,BR}; state 2 (nub down) front = bottom {BL,BR}; state 3 (nub left)
// front = left {TL,BL}. Corner indices: 0=TL, 1=TR, 2=BL, 3=BR.
constexpr uint8_t kFront[4][2] = {{0, 1}, {1, 3}, {2, 3}, {0, 2}};
constexpr uint8_t kBack[4][2] = {{2, 3}, {0, 2}, {0, 1}, {1, 3}};

static_assert(kFront[0][0] != kBack[0][0] && kFront[0][0] != kBack[0][1] &&
                  kFront[0][1] != kBack[0][0] && kFront[0][1] != kBack[0][1],
              "state 0 front/back disjoint");
static_assert(kFront[1][0] != kBack[1][0] && kFront[1][0] != kBack[1][1] &&
                  kFront[1][1] != kBack[1][0] && kFront[1][1] != kBack[1][1],
              "state 1 front/back disjoint");
static_assert(kFront[2][0] != kBack[2][0] && kFront[2][0] != kBack[2][1] &&
                  kFront[2][1] != kBack[2][0] && kFront[2][1] != kBack[2][1],
              "state 2 front/back disjoint");
static_assert(kFront[3][0] != kBack[3][0] && kFront[3][0] != kBack[3][1] &&
                  kFront[3][1] != kBack[3][0] && kFront[3][1] != kBack[3][1],
              "state 3 front/back disjoint");
static_assert(kFront[0][0] + kFront[0][1] + kBack[0][0] + kBack[0][1] == 6,
              "state 0 covers 0..3");
static_assert(kFront[1][0] + kFront[1][1] + kBack[1][0] + kBack[1][1] == 6,
              "state 1 covers 0..3");
static_assert(kFront[2][0] + kFront[2][1] + kBack[2][0] + kBack[2][1] == 6,
              "state 2 covers 0..3");
static_assert(kFront[3][0] + kFront[3][1] + kBack[3][0] + kBack[3][1] == 6,
              "state 3 covers 0..3");

// Classification table, order F1 F2 B1 B2 with 1 = occupied:
//   1 1 1 1 -> Full
//   1 1 1 0 -> Full
//   1 1 0 1 -> Full
//   1 1 0 0 -> None (only two corners)
//   1 0 1 1 -> Mini
//   0 1 1 1 -> Mini
//   0 0 1 1 -> None (two corners, both back)
//   1 0 1 0 -> None
//   1 0 0 1 -> None
//   0 1 1 0 -> None
//   0 1 0 1 -> None
//   every pattern with fewer than three occupied corners -> None
// In words: three corners minimum, both fronts give Full, exactly one
// front plus both backs gives Mini.
inline SpinKind classifySpin(const Board& b, const SpinQuery& q) {
  if (q.piece != PieceId::T) {
    return SpinKind::None;
  }
  if (!q.lastMoveWasRotation) {
    return SpinKind::None;
  }
  if (q.kickIndex < 0 || q.kickIndex > 4) {
    return SpinKind::None;
  }
  const int cols[4] = {q.boxCol, q.boxCol + 2, q.boxCol, q.boxCol + 2};
  const int rows[4] = {q.boxRow, q.boxRow, q.boxRow + 2, q.boxRow + 2};
  bool occ[4];
  for (int i = 0; i < 4; ++i) {
    occ[i] = cornerOccupied(b, cols[i], rows[i]);
  }
  const uint8_t s = static_cast<uint8_t>(q.state & 3);
  const bool f1 = occ[kFront[s][0]];
  const bool f2 = occ[kFront[s][1]];
  const bool b1 = occ[kBack[s][0]];
  const bool b2 = occ[kBack[s][1]];
  const int n = (f1 ? 1 : 0) + (f2 ? 1 : 0) + (b1 ? 1 : 0) + (b2 ? 1 : 0);
  if (n < 3) {
    return SpinKind::None;
  }
  SpinKind kind = SpinKind::None;
  if (f1 && f2) {
    kind = SpinKind::Full;
  } else if (b1 && b2 && (f1 != f2)) {
    kind = SpinKind::Mini;
  }
  if (kind == SpinKind::Mini && q.kickIndex == 4) {
    kind = SpinKind::Full;
  }
  return kind;
}

}  // namespace sf
