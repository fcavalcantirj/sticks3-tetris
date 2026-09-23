#pragma once
#include <cstdint>

namespace sf {

enum class Rot : uint8_t { Spawn = 0, R = 1, Two = 2, L = 3 };
enum class Turn : int8_t { CW = 1, CCW = -1 };

// There is deliberately no turn of two quarter-turns in the core: the locked
// control map makes counter-clockwise a SETTING that flips what the blue key
// does, and a CCW turn is one Turn::CCW, never three CW turns applied inside
// the engine.
constexpr Rot rotate(Rot from, Turn t) {
  return static_cast<Rot>((static_cast<int>(from) + 4 + static_cast<int>(t)) & 3);
}

constexpr char rotChar(Rot r) {
  switch (r) {
    case Rot::Spawn:
      return '0';
    case Rot::R:
      return 'R';
    case Rot::Two:
      return '2';
    case Rot::L:
      return 'L';
  }
  return '?';
}

constexpr int kTransitionCount = 8;

// Fixed indices for the eight legal single-step transitions:
// 0 = 0>>R, 1 = R>>0, 2 = R>>2, 3 = 2>>R, 4 = 2>>L, 5 = L>>2, 6 = L>>0,
// 7 = 0>>L. Returns -1 for the eight illegal pairs (four identities and the
// four two-step pairs) so a caller that forgets to check cannot silently
// apply the 0>>R kicks to a two-step turn.
constexpr int transitionIndex(Rot from, Rot to) {
  if (from == Rot::Spawn && to == Rot::R) return 0;
  if (from == Rot::R && to == Rot::Spawn) return 1;
  if (from == Rot::R && to == Rot::Two) return 2;
  if (from == Rot::Two && to == Rot::R) return 3;
  if (from == Rot::Two && to == Rot::L) return 4;
  if (from == Rot::L && to == Rot::Two) return 5;
  if (from == Rot::L && to == Rot::Spawn) return 6;
  if (from == Rot::Spawn && to == Rot::L) return 7;
  return -1;
}

constexpr const char* transitionName(int idx) {
  switch (idx) {
    case 0:
      return "0>>R";
    case 1:
      return "R>>0";
    case 2:
      return "R>>2";
    case 3:
      return "2>>R";
    case 4:
      return "2>>L";
    case 5:
      return "L>>2";
    case 6:
      return "L>>0";
    case 7:
      return "0>>L";
    default:
      return "?";
  }
}

}  // namespace sf
