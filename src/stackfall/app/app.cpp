#include "stackfall/app/app.h"

namespace sf {

void App::begin(const Settings& settings, const ScoreEntry (&scores)[15],
                uint32_t nowMs) {
  game_ = Game{};
  settings_ = settings;
  for (size_t i = 0; i < 15; ++i) {
    scores_[i] = scores[i];
  }

  screen_ = Screen::Title;
  menuIndex_ = 0;
  settingsDirty_ = false;
  scoresDirty_ = false;
  dirtySinceMs_ = nowMs;
  lastInputMs_ = nowMs;
  currentNowMs_ = nowMs;
  nextGameSeed_ = 0x9E3779B9u;
  calibratingStartsGame_ = false;
}

void App::onAction(AppAction action, uint32_t nowMs) {
  currentNowMs_ = nowMs;
  if (action != AppAction::None) {
    lastInputMs_ = nowMs;
  }

  if (screen_ == Screen::Playing) {
    switch (action) {
      case AppAction::MoveLeft:
        game_.move(-1);
        return;
      case AppAction::MoveRight:
        game_.move(1);
        return;
      case AppAction::SoftDropOn:
        game_.setSoftDrop(true);
        return;
      case AppAction::SoftDropOff:
        game_.setSoftDrop(false);
        return;
      case AppAction::RotateCw:
        game_.rotate(Turn::CW);
        return;
      case AppAction::RotateCcw:
        game_.rotate(Turn::CCW);
        return;
      case AppAction::HardDrop:
        game_.hardDrop();
        return;
      case AppAction::Hold:
        game_.hold();
        return;
      case AppAction::None:
      case AppAction::Pause:
      case AppAction::Confirm:
      case AppAction::Back:
      case AppAction::MenuUp:
      case AppAction::MenuDown:
      case AppAction::Diag:
      case AppAction::Calibrate:
      case AppAction::TiltSpeed:
        break;
    }
  }

  if (action == AppAction::TiltSpeed) {
    settings_.tiltSensitivity =
        static_cast<uint8_t>((settings_.tiltSensitivity + 1u) % 5u);
    markSettingsDirty(nowMs);
    return;
  }

  if (action == AppAction::MenuDown) {
    uint8_t count = itemCount(screen_);
    if (count != 0) {
      menuIndex_ = static_cast<uint8_t>((menuIndex_ + 1u) % count);
    }
    return;
  }

  // MODE is an in-place Title command, not a screen-graph edge.
  if (screen_ == Screen::Title && action == AppAction::Confirm &&
      menuIndex_ == 1) {
    settings_.mode = static_cast<uint8_t>((settings_.mode + 1u) % 3u);
    markSettingsDirty(nowMs);
    return;
  }

  // Settings Confirm edits the selected value without leaving the screen.
  if (screen_ == Screen::Settings && action == AppAction::Confirm &&
      menuIndex_ != 7) {
    editSetting(nowMs);
    return;
  }

  for (const ScreenTransition& transition : kScreenTransitions) {
    if (transition.from != screen_ || transition.trigger != action ||
        (transition.menuIndex != kAnyMenuIndex &&
         transition.menuIndex != menuIndex_)) {
      continue;
    }

    const Screen from = screen_;
    const bool startsCalibration = transition.to == Screen::Calibrating &&
                                   action == AppAction::Confirm;
    if (startsCalibration) calibratingStartsGame_ = true;
    if (from == Screen::Playing && action == AppAction::Calibrate) {
      calibratingStartsGame_ = false;
    }
    if (from == Screen::Calibrating && transition.to == Screen::Title) {
      calibratingStartsGame_ = false;
    }
    if (from == Screen::Calibrating && transition.to == Screen::Playing) {
      if (calibratingStartsGame_) startGame(nowMs);
      calibratingStartsGame_ = false;
    }
    if (from == Screen::Settings && transition.to != Screen::Settings) {
      markSettingsDirty(nowMs);
    }

    screen_ = transition.to;
    menuIndex_ = 0;
    return;
  }
}

void App::step(uint32_t nowMs) {
  currentNowMs_ = nowMs;
  if (screen_ != Screen::Playing) {
    return;
  }

  game_.update(nowMs);
  if (game_.status() == GameStatus::Playing) {
    return;
  }

  const uint8_t mode = static_cast<uint8_t>(game_.rules().mode);
  uint32_t value = game_.score() > 0
                       ? static_cast<uint32_t>(game_.score())
                       : 0u;
  if (game_.rules().mode == GameMode::FortyLine) {
    value = game_.wallMs();
  }
  const uint32_t lines32 = game_.lines();
  const uint16_t lines = lines32 > UINT16_MAX
                             ? UINT16_MAX
                             : static_cast<uint16_t>(lines32);
  insertScore(mode, value, lines, game_.level());

  screen_ = Screen::GameOver;
  menuIndex_ = 0;
}

int8_t App::insertScore(uint8_t mode, uint32_t value, uint16_t lines,
                        uint8_t level) {
  if (mode >= 3) {
    return -1;
  }

  const size_t base = static_cast<size_t>(mode) * 5u;
  for (size_t i = 0; i < 5; ++i) {
    if (scores_[base + i].score == value) {
      return -1;
    }
  }

  int8_t rank = -1;
  for (uint8_t i = 0; i < 5; ++i) {
    const uint32_t existing = scores_[base + i].score;
    const bool better = mode == 1 ? value < existing : value > existing;
    if (better) {
      rank = static_cast<int8_t>(i);
      break;
    }
  }
  if (rank < 0) {
    return -1;
  }

  for (int i = 4; i > rank; --i) {
    scores_[base + static_cast<size_t>(i)] =
        scores_[base + static_cast<size_t>(i - 1)];
  }
  scores_[base + static_cast<size_t>(rank)] =
      ScoreEntry{value, lines, level, mode};
  markScoresDirty(currentNowMs_);
  return rank;
}

bool App::wantsFlush(uint32_t nowMs) const {
  if (screen_ == Screen::Playing) {
    return false;
  }
  if (settingsDirty_ || scoresDirty_) {
    return true;
  }
  return nowMs - lastInputMs_ >= kIdleFlushMs;
}

void App::markFlushed(uint32_t nowMs) {
  settingsDirty_ = false;
  scoresDirty_ = false;
  dirtySinceMs_ = nowMs;
  lastInputMs_ = nowMs;
  currentNowMs_ = nowMs;
}

uint8_t App::itemCount(Screen screen) {
  switch (screen) {
    case Screen::Title:
    case Screen::Paused:
      return 4;
    case Screen::GameOver:
      return 2;
    case Screen::Settings:
      return 8;
    case Screen::Playing:
    case Screen::HighScores:
    case Screen::Instructions:
    case Screen::Diagnostics:
    case Screen::Calibrating:
      return 0;
  }
  return 0;
}

RuleProfile App::rulesForSettings() const {
  RuleProfile rules;
  switch (settings_.mode) {
    case 1:
      rules = fortyLine();
      break;
    case 2:
      rules = threeMinute();
      break;
    case 0:
    default:
      rules = endless();
      break;
  }
  rules.startLevel = settings_.startLevel;
  rules.ghostEnabled = settings_.ghostEnabled;
  return withControlProfile(
      rules, static_cast<ControlProfile>(settings_.controlProfile));
}

void App::startGame(uint32_t nowMs) {
  RuleProfile rules = rulesForSettings();
  game_.reset(rules, nextGameSeed_, nowMs);
}

void App::editSetting(uint32_t nowMs) {
  switch (menuIndex_) {
    case 0:
      settings_.rotateCw = !settings_.rotateCw;
      break;
    case 1:
      settings_.controlProfile =
          settings_.controlProfile == uint8_t(ControlProfile::BUTTONS_ONLY)
              ? uint8_t(ControlProfile::TILT_DIP)
              : uint8_t(ControlProfile::BUTTONS_ONLY);
      break;
    case 2:
      settings_.tiltSensitivity =
          static_cast<uint8_t>((settings_.tiltSensitivity + 1u) % 5u);
      break;
    case 3:
      settings_.brightness =
          static_cast<uint8_t>((settings_.brightness + 1u) % 5u);
      break;
    case 4:
      settings_.volume = static_cast<uint8_t>((settings_.volume + 1u) % 5u);
      break;
    case 5:
      settings_.ghostEnabled = !settings_.ghostEnabled;
      break;
    case 6:
      for (uint8_t mode = 0; mode < 3; ++mode) {
        for (uint8_t rank = 0; rank < 5; ++rank) {
          const size_t index = static_cast<size_t>(mode) * 5u + rank;
          scores_[index] = ScoreEntry{
              mode == 1 ? UINT32_MAX : 0u, 0u, 0u, mode};
        }
      }
      markScoresDirty(nowMs);
      return;
    default:
      return;
  }
  markSettingsDirty(nowMs);
}

void App::markSettingsDirty(uint32_t nowMs) {
  if (!settingsDirty_ && !scoresDirty_) {
    dirtySinceMs_ = nowMs;
  }
  settingsDirty_ = true;
}

void App::markScoresDirty(uint32_t nowMs) {
  if (!settingsDirty_ && !scoresDirty_) {
    dirtySinceMs_ = nowMs;
  }
  scoresDirty_ = true;
}

}  // namespace sf
