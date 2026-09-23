#pragma once

#include <cstdint>
#include <type_traits>

#include "control_profile.h"
#include "stackfall/core/board.h"
#include "stackfall/rules/combo.h"

namespace sf {

// Three neutral game-mode names used everywhere in code, fixtures and UI.
// No trademarked mode name appears in this project.
enum class GameMode : uint8_t { Endless, FortyLine, ThreeMinute };

// Mode status returned by modeStatus(). Top-out is Game's business, not the
// mode's.
enum class ModeStatus : uint8_t { Running, Complete };

// Complete rules/profile bundle. Trivially copyable so the whole struct can be
// copied into the replay header without a heap allocation.
struct RuleProfile {
  uint16_t lockDelayMs = 500;
  uint8_t lockResetLimit = 15;
  uint16_t dasMs = 167;
  uint16_t arrMs = 33;
  uint16_t tickMs = 8;
  uint8_t startLevel = 1;
  uint8_t linesPerLevel = 10;
  uint8_t maxLevel = 15;
  uint8_t nextPreviewCount = 3;
  uint8_t nextQueueMin = 7;
  int16_t maxCombo = kMaxCombo;
  bool holdEnabled = true;
  bool ghostEnabled = true;
  bool tSpinsEnabled = true;
  bool combosEnabled = true;
  bool backToBackEnabled = true;
  bool perfectClearEnabled = true;
  bool horizontalWrap = false;
  GameMode mode = GameMode::Endless;
  uint16_t targetLines = 0;
  uint32_t timeLimitMs = 0;
  uint32_t seed = 0x9E3779B9;
};

static_assert(std::is_trivially_copyable<RuleProfile>::value,
              "RuleProfile must be trivially copyable");
static_assert(sizeof(RuleProfile) <= 40,
              "RuleProfile must fit in the replay header");

// Apply the control-profile-specific rule tweak. BUTTONS_ONLY enables
// horizontal wrap so a single direction key can wrap around the matrix;
// the tilt profile does not.
constexpr RuleProfile withControlProfile(RuleProfile p, ControlProfile c) {
  p.horizontalWrap = (c == ControlProfile::BUTTONS_ONLY);
  return p;
}

// Mode factories.
constexpr RuleProfile endless() {
  RuleProfile p;
  p.mode = GameMode::Endless;
  p.targetLines = 0;
  p.timeLimitMs = 0;
  return p;
}

constexpr RuleProfile fortyLine() {
  RuleProfile p;
  p.mode = GameMode::FortyLine;
  p.targetLines = 40;
  p.timeLimitMs = 0;
  return p;
}

constexpr RuleProfile threeMinute() {
  RuleProfile p;
  p.mode = GameMode::ThreeMinute;
  p.targetLines = 0;
  p.timeLimitMs = 180000;
  return p;
}

// Level progression: computed entirely in uint32_t before the narrowing cast.
constexpr uint8_t levelFor(const RuleProfile& p, uint32_t linesCleared) {
  uint32_t level = static_cast<uint32_t>(p.startLevel) +
                   linesCleared / static_cast<uint32_t>(p.linesPerLevel);
  uint32_t maxLevel = static_cast<uint32_t>(p.maxLevel);
  if (level > maxLevel) {
    level = maxLevel;
  }
  return static_cast<uint8_t>(level);
}

// Mode completion status.
constexpr ModeStatus modeStatus(const RuleProfile& p, uint32_t linesCleared,
                                uint32_t elapsedMs) {
  switch (p.mode) {
    case GameMode::FortyLine:
      if (linesCleared >= static_cast<uint32_t>(p.targetLines)) {
        return ModeStatus::Complete;
      }
      return ModeStatus::Running;
    case GameMode::ThreeMinute:
      if (elapsedMs >= p.timeLimitMs) {
        return ModeStatus::Complete;
      }
      return ModeStatus::Running;
    case GameMode::Endless:
    default:
      return ModeStatus::Running;
  }
}

// Perfect clear: every cell of the 10x40 store must be empty, including the 20
// hidden buffer rows. A stray cell parked in the buffer is precisely the bug
// this predicate exists to catch.
inline bool isPerfectClear(const Board& b) { return b.isEmpty(); }

}  // namespace sf
