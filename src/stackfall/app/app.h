#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "control_profile.h"
#include "stackfall/engine/game.h"

namespace sf {

enum class Screen : uint8_t {
  Title,
  Playing,
  Paused,
  GameOver,
  HighScores,
  Settings,
  Instructions,
  Diagnostics,
  Calibrating,
};

// Kept name-for-name and order-for-order with input::ActionKind, which is
// introduced by the later input task. App deliberately does not include input/.
enum class AppAction : uint8_t {
  None,
  MoveLeft,
  MoveRight,
  SoftDropOn,
  SoftDropOff,
  RotateCw,
  RotateCcw,
  HardDrop,
  Hold,
  Pause,
  Confirm,
  Back,
  MenuUp,
  MenuDown,
  Diag,
  Calibrate,
  TiltSpeed,
};

struct ScreenTransition {
  Screen from;
  AppAction trigger;
  uint8_t menuIndex;
  Screen to;
};

constexpr uint8_t kAnyMenuIndex = 0xFF;

// The complete input-driven screen graph. Playing -> GameOver is intentionally
// absent because step() performs that transition from Game::status().
inline constexpr ScreenTransition kScreenTransitions[] = {
    {Screen::Title, AppAction::Confirm, 0, Screen::Calibrating},
    {Screen::Title, AppAction::Confirm, 2, Screen::HighScores},
    {Screen::Title, AppAction::Confirm, 3, Screen::Settings},
    {Screen::Title, AppAction::Diag, kAnyMenuIndex, Screen::Diagnostics},
    {Screen::HighScores, AppAction::Confirm, kAnyMenuIndex, Screen::Title},
    {Screen::HighScores, AppAction::Back, kAnyMenuIndex, Screen::Title},
    {Screen::Settings, AppAction::Back, kAnyMenuIndex, Screen::Title},
    {Screen::Settings, AppAction::Confirm, 7, Screen::Title},
    {Screen::Diagnostics, AppAction::Back, kAnyMenuIndex, Screen::Title},
    {Screen::Playing, AppAction::Pause, kAnyMenuIndex, Screen::Paused},
    {Screen::Paused, AppAction::Pause, kAnyMenuIndex, Screen::Playing},
    {Screen::Paused, AppAction::Back, kAnyMenuIndex, Screen::Playing},
    {Screen::Paused, AppAction::Confirm, 0, Screen::Playing},
    {Screen::Paused, AppAction::Confirm, 1, Screen::Calibrating},
    {Screen::Paused, AppAction::Confirm, 2, Screen::Instructions},
    {Screen::Paused, AppAction::Confirm, 3, Screen::Title},
    {Screen::Instructions, AppAction::Confirm, kAnyMenuIndex, Screen::Paused},
    {Screen::Instructions, AppAction::Back, kAnyMenuIndex, Screen::Paused},
    {Screen::GameOver, AppAction::Confirm, 0, Screen::Calibrating},
    {Screen::GameOver, AppAction::Confirm, 1, Screen::Title},
    {Screen::GameOver, AppAction::Back, kAnyMenuIndex, Screen::Title},
    {Screen::Calibrating, AppAction::Confirm, kAnyMenuIndex, Screen::Playing},
    {Screen::Calibrating, AppAction::Back, kAnyMenuIndex, Screen::Title},
    {Screen::Playing, AppAction::Calibrate, kAnyMenuIndex,
     Screen::Calibrating},
};

struct Settings {
  uint8_t controlProfile = uint8_t(kActiveControlProfile);
  bool rotateCw = true;
  uint8_t tiltSensitivity = 2;
  uint8_t brightness = 3;
  uint8_t volume = 2;
  bool ghostEnabled = true;
  uint8_t startLevel = 1;
  uint8_t mode = 0;
};

struct ScoreEntry {
  uint32_t score;
  uint16_t lines;
  uint8_t level;
  uint8_t mode;
};

static_assert(std::is_trivially_copyable<Settings>::value,
              "Settings must stay persistence-safe");
static_assert(std::is_trivially_copyable<ScoreEntry>::value,
              "ScoreEntry must stay persistence-safe");
static_assert(sizeof(ScoreEntry) == 8, "ScoreEntry wire payload is eight bytes");

constexpr uint16_t kFlushDeadlineMs = 500;
constexpr uint32_t kIdleFlushMs = 30000;

class App {
 public:
  App() = default;

  void begin(const Settings& settings, const ScoreEntry (&scores)[15],
             uint32_t nowMs);
  void setNextGameSeed(uint32_t seed) { nextGameSeed_ = seed; }
  void onAction(AppAction action, uint32_t nowMs);
  void step(uint32_t nowMs);

  Screen screen() const { return screen_; }
  uint8_t menuIndex() const { return menuIndex_; }
  const Game& game() const { return game_; }
  const Settings& settings() const { return settings_; }
  const ScoreEntry* scores() const { return scores_; }

  int8_t insertScore(uint8_t mode, uint32_t value, uint16_t lines,
                     uint8_t level);

  // Dirty persistence is requested immediately, with kFlushDeadlineMs an
  // upper bound rather than a delay. A player may double-click the hardware
  // power key immediately after GameOver; there is no shutdown callback in
  // which to recover an unwritten score. NVS work is nevertheless suppressed
  // throughout Playing so a blocking commit cannot consume lock delay.
  bool wantsFlush(uint32_t nowMs) const;
  void markFlushed(uint32_t nowMs);

 private:
  static uint8_t itemCount(Screen screen);
  RuleProfile rulesForSettings() const;
  void startGame(uint32_t nowMs);
  void editSetting(uint32_t nowMs);
  void markSettingsDirty(uint32_t nowMs);
  void markScoresDirty(uint32_t nowMs);

  Game game_;
  Settings settings_;
  ScoreEntry scores_[15]{};
  Screen screen_ = Screen::Title;
  uint8_t menuIndex_ = 0;

  bool settingsDirty_ = false;
  bool scoresDirty_ = false;
  uint32_t dirtySinceMs_ = 0;
  uint32_t lastInputMs_ = 0;
  uint32_t currentNowMs_ = 0;
  uint32_t nextGameSeed_ = 0x9E3779B9u;
  bool calibratingStartsGame_ = false;
};

}  // namespace sf
