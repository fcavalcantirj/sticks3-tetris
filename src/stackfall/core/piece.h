#pragma once
#include <array>
#include <cstdint>

namespace sf {

enum class Cell : uint8_t { Empty = 0, I, J, L, O, S, T, Z };
enum class PieceId : uint8_t { I = 0, J, L, O, S, T, Z };

constexpr int kPieceCount = 7;

constexpr Cell cellFor(PieceId p) {
  return static_cast<Cell>(static_cast<uint8_t>(p) + 1);
}

constexpr char letterFor(PieceId p) {
  switch (p) {
    case PieceId::I: return 'I';
    case PieceId::J: return 'J';
    case PieceId::L: return 'L';
    case PieceId::O: return 'O';
    case PieceId::S: return 'S';
    case PieceId::T: return 'T';
    case PieceId::Z: return 'Z';
  }
  return '?';
}

constexpr char letterForCell(Cell c) {
  switch (c) {
    case Cell::Empty: return '.';
    case Cell::I: return 'I';
    case Cell::J: return 'J';
    case Cell::L: return 'L';
    case Cell::O: return 'O';
    case Cell::S: return 'S';
    case Cell::T: return 'T';
    case Cell::Z: return 'Z';
  }
  return '?';
}

struct Offset {
  int8_t col;
  int8_t row;
};

constexpr bool operator==(Offset a, Offset b) {
  return a.col == b.col && a.row == b.row;
}

constexpr bool operator!=(Offset a, Offset b) { return !(a == b); }

using Shape = std::array<Offset, 4>;

// Box edge per piece: I=4, J/L/S/T/Z=3, O=2. Kick tables assume these sizes.
// Grid origin is top-left: col grows right, row grows DOWN (row 0 at top).
constexpr int8_t kBoxSize[kPieceCount] = {4, 3, 3, 2, 3, 3, 3};

constexpr Shape kShapes[kPieceCount][4] = {
    // I
    {Shape{Offset{0, 1}, Offset{1, 1}, Offset{2, 1}, Offset{3, 1}},
     Shape{Offset{2, 0}, Offset{2, 1}, Offset{2, 2}, Offset{2, 3}},
     Shape{Offset{0, 2}, Offset{1, 2}, Offset{2, 2}, Offset{3, 2}},
     Shape{Offset{1, 0}, Offset{1, 1}, Offset{1, 2}, Offset{1, 3}}},
    // J
    {Shape{Offset{0, 0}, Offset{0, 1}, Offset{1, 1}, Offset{2, 1}},
     Shape{Offset{1, 0}, Offset{2, 0}, Offset{1, 1}, Offset{1, 2}},
     Shape{Offset{0, 1}, Offset{1, 1}, Offset{2, 1}, Offset{2, 2}},
     Shape{Offset{1, 0}, Offset{1, 1}, Offset{0, 2}, Offset{1, 2}}},
    // L
    {Shape{Offset{2, 0}, Offset{0, 1}, Offset{1, 1}, Offset{2, 1}},
     Shape{Offset{1, 0}, Offset{1, 1}, Offset{1, 2}, Offset{2, 2}},
     Shape{Offset{0, 1}, Offset{1, 1}, Offset{2, 1}, Offset{0, 2}},
     Shape{Offset{0, 0}, Offset{1, 0}, Offset{1, 1}, Offset{1, 2}}},
    // O (identical in all four states)
    {Shape{Offset{0, 0}, Offset{1, 0}, Offset{0, 1}, Offset{1, 1}},
     Shape{Offset{0, 0}, Offset{1, 0}, Offset{0, 1}, Offset{1, 1}},
     Shape{Offset{0, 0}, Offset{1, 0}, Offset{0, 1}, Offset{1, 1}},
     Shape{Offset{0, 0}, Offset{1, 0}, Offset{0, 1}, Offset{1, 1}}},
    // S
    {Shape{Offset{1, 0}, Offset{2, 0}, Offset{0, 1}, Offset{1, 1}},
     Shape{Offset{1, 0}, Offset{1, 1}, Offset{2, 1}, Offset{2, 2}},
     Shape{Offset{1, 1}, Offset{2, 1}, Offset{0, 2}, Offset{1, 2}},
     Shape{Offset{0, 0}, Offset{0, 1}, Offset{1, 1}, Offset{1, 2}}},
    // T
    {Shape{Offset{1, 0}, Offset{0, 1}, Offset{1, 1}, Offset{2, 1}},
     Shape{Offset{1, 0}, Offset{1, 1}, Offset{2, 1}, Offset{1, 2}},
     Shape{Offset{0, 1}, Offset{1, 1}, Offset{2, 1}, Offset{1, 2}},
     Shape{Offset{1, 0}, Offset{0, 1}, Offset{1, 1}, Offset{1, 2}}},
    // Z
    {Shape{Offset{0, 0}, Offset{1, 0}, Offset{1, 1}, Offset{2, 1}},
     Shape{Offset{2, 0}, Offset{1, 1}, Offset{2, 1}, Offset{1, 2}},
     Shape{Offset{0, 1}, Offset{1, 1}, Offset{1, 2}, Offset{2, 2}},
     Shape{Offset{1, 0}, Offset{0, 1}, Offset{1, 1}, Offset{0, 2}}}};

constexpr const Shape& shapeOf(PieceId p, int state) {
  return kShapes[static_cast<int>(p)][state & 3];
}

inline std::array<char, 17> shapeString(PieceId p, int state) {
  std::array<char, 17> out{};
  for (int i = 0; i < 16; ++i) out[static_cast<size_t>(i)] = '.';
  out[16] = '\0';
  const Shape& s = shapeOf(p, state);
  char mark = letterFor(p);
  for (int i = 0; i < 4; ++i) {
    int c = s[static_cast<size_t>(i)].col;
    int r = s[static_cast<size_t>(i)].row;
    if (c >= 0 && c < 4 && r >= 0 && r < 4) {
      out[static_cast<size_t>(r * 4 + c)] = mark;
    }
  }
  return out;
}

}  // namespace sf
