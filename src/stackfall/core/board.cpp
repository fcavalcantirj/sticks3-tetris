#include "stackfall/core/board.h"

namespace sf {

bool Board::collides(PieceId p, int state, int col, int row) const {
  const Shape& s = shapeOf(p, state);
  for (int i = 0; i < 4; ++i) {
    int c = col + s[static_cast<size_t>(i)].col;
    int r = row + s[static_cast<size_t>(i)].row;
    if (isBlocked(c, r)) return true;
  }
  return false;
}

int Board::place(PieceId p, int state, int col, int row) {
  const Shape& s = shapeOf(p, state);
  Cell c = cellFor(p);
  int landed = 0;
  for (int i = 0; i < 4; ++i) {
    int bc = col + s[static_cast<size_t>(i)].col;
    int br = row + s[static_cast<size_t>(i)].row;
    if (inBounds(bc, br)) {
      rows_[static_cast<size_t>(br)][static_cast<size_t>(bc)] = c;
      ++landed;
    }
  }
  return landed;
}

int Board::clearLines(int (&outRows)[4]) {
  int count = 0;
  int write = kTotalRows - 1;
  for (int read = kTotalRows - 1; read >= 0; --read) {
    if (rowFull(read)) {
      if (count < 4) outRows[count] = read;
      ++count;
      continue;
    }
    if (write != read) rows_[static_cast<size_t>(write)] = rows_[static_cast<size_t>(read)];
    --write;
  }
  for (int r = write; r >= 0; --r) {
    for (int c = 0; c < kWidth; ++c) {
      rows_[static_cast<size_t>(r)][static_cast<size_t>(c)] = Cell::Empty;
    }
  }
  return count > 4 ? 4 : count;
}

}  // namespace sf
