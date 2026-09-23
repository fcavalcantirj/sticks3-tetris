#pragma once
#include <array>
#include <cstdint>

#include "stackfall/core/piece.h"

namespace sf {

class Board {
 public:
  static constexpr int kWidth = 10;
  static constexpr int kVisibleRows = 20;
  static constexpr int kBufferRows = 20;
  static constexpr int kTotalRows = 40;
  static constexpr int kFirstVisibleRow = 20;

  // screenRow = boardRow - 20

  Board() { clear(); }

  static constexpr bool inBounds(int col, int row) {
    return col >= 0 && col < kWidth && row >= 0 && row < kTotalRows;
  }

  constexpr Cell at(int col, int row) const {
    if (!inBounds(col, row)) return Cell::Empty;
    return rows_[static_cast<size_t>(row)][static_cast<size_t>(col)];
  }

  void set(int col, int row, Cell c) {
    if (!inBounds(col, row)) return;
    rows_[static_cast<size_t>(row)][static_cast<size_t>(col)] = c;
  }

  bool isBlocked(int col, int row) const {
    if (!inBounds(col, row)) return true;
    return at(col, row) != Cell::Empty;
  }

  void clear() {
    for (size_t r = 0; r < static_cast<size_t>(kTotalRows); ++r) {
      for (size_t c = 0; c < static_cast<size_t>(kWidth); ++c) {
        rows_[r][c] = Cell::Empty;
      }
    }
  }

  bool rowFull(int row) const {
    if (row < 0 || row >= kTotalRows) return false;
    for (int c = 0; c < kWidth; ++c) {
      if (rows_[static_cast<size_t>(row)][static_cast<size_t>(c)] == Cell::Empty) return false;
    }
    return true;
  }

  bool rowEmpty(int row) const {
    if (row < 0 || row >= kTotalRows) return false;
    for (int c = 0; c < kWidth; ++c) {
      if (rows_[static_cast<size_t>(row)][static_cast<size_t>(c)] != Cell::Empty) return false;
    }
    return true;
  }

  int filledCount(int row) const {
    if (row < 0 || row >= kTotalRows) return 0;
    int n = 0;
    for (int c = 0; c < kWidth; ++c) {
      if (rows_[static_cast<size_t>(row)][static_cast<size_t>(c)] != Cell::Empty) ++n;
    }
    return n;
  }

  bool isEmpty() const {
    for (size_t r = 0; r < static_cast<size_t>(kTotalRows); ++r) {
      for (size_t c = 0; c < static_cast<size_t>(kWidth); ++c) {
        if (rows_[r][c] != Cell::Empty) return false;
      }
    }
    return true;
  }

  bool operator==(const Board& other) const { return rows_ == other.rows_; }
  bool operator!=(const Board& other) const { return !(*this == other); }

  bool collides(PieceId p, int state, int col, int row) const;
  int place(PieceId p, int state, int col, int row);
  int clearLines(int (&outRows)[4]);

  std::array<char, kWidth + 1> rowString(int row) const {
    std::array<char, kWidth + 1> out{};
    if (row < 0 || row >= kTotalRows) {
      for (int i = 0; i < kWidth; ++i) out[static_cast<size_t>(i)] = '?';
      out[static_cast<size_t>(kWidth)] = '\0';
      return out;
    }
    for (int i = 0; i < kWidth; ++i) {
      out[static_cast<size_t>(i)] = letterForCell(rows_[static_cast<size_t>(row)][static_cast<size_t>(i)]);
    }
    out[static_cast<size_t>(kWidth)] = '\0';
    return out;
  }

  void setRowFromString(int row, const char* s) {    if (row < 0 || row >= kTotalRows) return;
    if (s == nullptr) return;
    for (int i = 0; i < kWidth; ++i) {
      if (s[i] == '\0') return;
    }
    for (int i = 0; i < kWidth; ++i) {
      char ch = s[i];
      Cell c = Cell::Empty;
      switch (ch) {
        case 'I': c = Cell::I; break;
        case 'J': c = Cell::J; break;
        case 'L': c = Cell::L; break;
        case 'O': c = Cell::O; break;
        case 'S': c = Cell::S; break;
        case 'T': c = Cell::T; break;
        case 'Z': c = Cell::Z; break;
        case '.':
        case ' ':
        default: c = Cell::Empty; break;
      }
      rows_[static_cast<size_t>(row)][static_cast<size_t>(i)] = c;
    }
  }

 private:
  std::array<std::array<Cell, kWidth>, kTotalRows> rows_;
};

static_assert(Board::kBufferRows + Board::kVisibleRows == Board::kTotalRows, "row split");
static_assert(Board::kFirstVisibleRow == Board::kBufferRows, "visible starts after buffer");
static_assert(sizeof(Board) == 400, "board is 400 bytes");

}  // namespace sf
