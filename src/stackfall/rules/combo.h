#pragma once

#include <cstdint>

#include "stackfall/rules/scoring.h"

namespace sf {

// Combo / back-to-back chain. Depends on nothing but scoring.h —
// no board, no clock, no profile. Profile gating (combosEnabled,
// backToBackEnabled) is applied by the caller, never inside this function.
//
// comboCount == -1 means no chain is running (the value Game::reset
// installs and the value every zero-line lock returns to).
// b2b == true means the previous difficult clear is still standing.

constexpr int16_t kMaxCombo = 20;

struct ChainState {
  int16_t comboCount;
  bool b2b;
};

struct ChainResult {
  ChainState next;
  bool payB2b;
  int16_t comboForScoring;
};

constexpr ChainResult advanceChain(ChainState prev, ClearKind clear, SpinKind spin,
                                   int16_t maxCombo = kMaxCombo) {
  if (clear == ClearKind::None) {
    // A T-spin that clears nothing neither starts nor breaks a
    // back-to-back chain (isDifficult is false for ClearKind::None so no
    // bonus could pay here anyway); an ordinary lock that clears nothing
    // breaks the combo only. b2b is carried through unchanged.
    ChainState next{-1, prev.b2b};
    return ChainResult{next, false, -1};
  }
  int32_t grown = static_cast<int32_t>(prev.comboCount) + 1;
  if (grown > static_cast<int32_t>(maxCombo)) {
    grown = static_cast<int32_t>(maxCombo);
  }
  const int16_t counted = static_cast<int16_t>(grown);
  const bool difficult = isDifficult(clear, spin);
  ChainState next{counted, difficult};
  const bool pay = prev.b2b && isDifficult(clear, spin);
  return ChainResult{next, pay, counted};
}

}  // namespace sf
