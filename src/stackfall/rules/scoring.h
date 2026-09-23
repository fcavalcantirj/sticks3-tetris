#pragma once

#include <cstdint>

namespace sf {

// Scoring: line-clear base values scaled by level, plus combo,
// perfect-clear and drop points. Integer arithmetic only (int32_t);
// the back-to-back multiplier is written (v * 3) / 2, never v * 1.5f,
// because rounding differs between clang-on-macOS and xtensa-gcc and
// would split the host-versus-device parity hash.

enum class ClearKind : uint8_t { None, Single, Double, Triple, Quad };
enum class SpinKind : uint8_t { None, Mini, Full };

struct ClearEvent {
  ClearKind clear;
  SpinKind spin;
  int16_t comboCount;
  bool backToBack;
  bool perfectClear;
  uint16_t softDropCells;
  uint16_t hardDropCells;
  uint8_t level;
};

struct ScoreBreakdown {
  int32_t base;
  int32_t b2bBonus;
  int32_t combo;
  int32_t perfect;
  int32_t drop;
  int32_t total;
};

// Base table indexed [spin][clear]; the row index IS the SpinKind value:
// row 0 = None, row 1 = Mini, row 2 = Full. All fifteen entries written
// out, none left implicit. Mini triple and mini quad are geometrically
// unreachable and score 0; full quad is unreachable and scores 0.
constexpr int16_t kBase[3][5] = {
    {0, 100, 300, 500, 800},
    {100, 200, 400, 0, 0},
    {400, 800, 1200, 1600, 0},
};

static_assert(kBase[0][4] == 800 && kBase[1][0] == 100 && kBase[2][0] == 400,
              "scoring base table transposed");

constexpr int16_t kPerfect[5] = {0, 800, 1200, 1800, 2000};

// Single definition of "difficult": a Quad, or any clear of one or more
// lines with Mini or Full spin. A Single, Double or Triple with no spin
// is not difficult.
constexpr bool isDifficult(ClearKind clear, SpinKind spin) {
  if (clear == ClearKind::Quad) {
    return true;
  }
  if (clear == ClearKind::None) {
    return false;
  }
  return spin == SpinKind::Mini || spin == SpinKind::Full;
}

constexpr ScoreBreakdown scoreFor(const ClearEvent& ev) {
  const int spinIdx = static_cast<int>(ev.spin);
  const int clearIdx = static_cast<int>(ev.clear);
  const int32_t level = static_cast<int32_t>(ev.level);
  const int32_t base =
      static_cast<int32_t>(kBase[spinIdx][clearIdx]) * level;
  // Every base value is a multiple of 100, so base * 3 is even and the
  // truncating integer division below is exact.
  const bool difficult = isDifficult(ev.clear, ev.spin);
  const int32_t b2bBonus =
      (ev.backToBack && ev.clear != ClearKind::None && difficult)
          ? (base * 3) / 2 - base
          : 0;
  const int32_t combo = (ev.comboCount >= 1)
                            ? 50 * static_cast<int32_t>(ev.comboCount) * level
                            : 0;
  const int32_t perfect = ev.perfectClear
                              ? static_cast<int32_t>(kPerfect[clearIdx]) * level
                              : 0;
  const int32_t drop = static_cast<int32_t>(ev.softDropCells) +
                       static_cast<int32_t>(ev.hardDropCells) * 2;
  ScoreBreakdown out{base, b2bBonus, combo, perfect, drop,
                     base + b2bBonus + combo + perfect + drop};
  return out;
}

}  // namespace sf
