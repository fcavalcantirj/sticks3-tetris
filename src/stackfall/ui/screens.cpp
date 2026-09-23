#include "stackfall/ui/screens.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <limits>

#include "stackfall/core/piece.h"
#include "stackfall/engine/events.h"

namespace sf::ui {
namespace {

// The playing width mirrors panel_hud.cpp's kScoreRect, which is only 85 px.
constexpr int kHudScoreWidthPx = 85;
// The leaderboard has a separate 133 px content row, so it keeps 131 px.
constexpr int kScoreWidthPx = 131;
constexpr int kContentWidthPx = 133;
constexpr uint32_t kMaxHudLines = 999999;
constexpr std::size_t kInstructionChars = 22;
constexpr float kRadiansToDegrees = 57.29577951308232f;

constexpr const char* kModeNames[] = {"ENDLESS", "40 LINE", "3 MIN"};
constexpr const char* kTitleItems[] = {"PLAY", "MODE", "HIGH SCORES",
                                       "SETTINGS"};
constexpr const char* kPauseItems[] = {"RESUME", "RESTART", "INSTRUCTIONS",
                                       "TITLE"};
constexpr const char* kGameOverItems[] = {"RETRY", "TITLE"};
constexpr const char* kSettingLabels[] = {
    "ROTATION", "PROFILE", "TILT SPEED", "BRIGHTNESS", "VOLUME", "GHOST",
    "RESET SCORES", "BACK"};
constexpr const char* kSettingLevels[] = {"0", "1", "2", "3", "4"};
constexpr const char* kTiltSpeedLabels[] = {"SLOW", "EASY", "NORMAL", "QUICK",
                                            "FAST"};
constexpr const char* kComboBanners[] = {
    "COMBO x0",  "COMBO x1",  "COMBO x2",  "COMBO x3",  "COMBO x4",
    "COMBO x5",  "COMBO x6",  "COMBO x7",  "COMBO x8",  "COMBO x9",
    "COMBO x10", "COMBO x11", "COMBO x12", "COMBO x13", "COMBO x14",
    "COMBO x15", "COMBO x16", "COMBO x17", "COMBO x18", "COMBO x19",
    "COMBO x20"};

constexpr uint8_t paletteIndex(PieceId id) {
  return static_cast<uint8_t>(id) + 1u;
}

constexpr Rect emptyRect() { return Rect{0, 0, 0, 0}; }

unsigned poseAngleDegrees(float deflectionG) {
  if (deflectionG < 0.0f) deflectionG = 0.0f;
  if (deflectionG > 1.0f) deflectionG = 1.0f;
  return static_cast<unsigned>(std::lround(
      std::asin(deflectionG) * kRadiansToDegrees));
}

constexpr bool sameRect(Rect a, Rect b) {
  return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

Rect visibleCellRect(int col, int row) {
  return cellVisible(col, row) ? cellRect(col, row) : emptyRect();
}

const char* bannerFor(const GameEvent& event) {
  switch (event.type) {
    case EventType::LineClear:
      return event.a >= 4 ? "QUAD" : nullptr;
    case EventType::TSpin:
      if (event.a == static_cast<uint8_t>(SpinKind::Mini)) {
        return "T-SPIN MINI";
      }
      if (event.a == static_cast<uint8_t>(SpinKind::Full)) {
        return "T-SPIN";
      }
      return nullptr;
    case EventType::Combo:
      if (event.a < sizeof(kComboBanners) / sizeof(kComboBanners[0])) {
        return kComboBanners[event.a];
      }
      return nullptr;
    case EventType::BackToBack:
      return "B2B";
    case EventType::PerfectClear:
      return "PERFECT";
    case EventType::Spawn:
    case EventType::Move:
    case EventType::Rotate:
    case EventType::Kick:
    case EventType::SoftDrop:
    case EventType::HardDrop:
    case EventType::Hold:
    case EventType::HoldDenied:
    case EventType::Lock:
    case EventType::LevelUp:
    case EventType::GameOver:
      return nullptr;
  }
  return nullptr;
}

uint16_t bannerTimeLeft(const Game& game, const GameEvent& event,
                        uint32_t nowMs) {
  const uint64_t eventElapsed64 =
      static_cast<uint64_t>(event.tick) * game.tickMs();
  const uint32_t eventElapsed =
      eventElapsed64 > std::numeric_limits<uint32_t>::max()
          ? std::numeric_limits<uint32_t>::max()
          : static_cast<uint32_t>(eventElapsed64);
  const uint32_t engineAge = game.elapsedMs() >= eventElapsed
                                 ? game.elapsedMs() - eventElapsed
                                 : 0u;
  const uint32_t wallAge = nowMs - game.lastNowMs();
  const uint64_t age = static_cast<uint64_t>(engineAge) + wallAge;
  return age < kBannerDurationMs
             ? static_cast<uint16_t>(kBannerDurationMs - age)
             : 0u;
}

void buildHud(const Game& game, HudText& hud) {
  const uint32_t score =
      game.score() > 0 ? static_cast<uint32_t>(game.score()) : 0u;
  // kHudScoreWidthPx comes from panel_hud.cpp's kScoreRect; using the wider
  // leaderboard allowance here would paint over level and battery fields.
  hud::formatScore(hud.left, sizeof(hud.left), score, kHudScoreWidthPx,
                   hud.leftSize);
  std::snprintf(hud.mid, sizeof(hud.mid), "LV%u",
                static_cast<unsigned>(game.level()));
  const uint32_t lines =
      game.lines() > kMaxHudLines ? kMaxHudLines : game.lines();
  std::snprintf(hud.right, sizeof(hud.right), "L%u",
                static_cast<unsigned>(lines));
}

uint16_t visibleLines(const Game& game) {
  return game.lines() > std::numeric_limits<uint16_t>::max()
             ? std::numeric_limits<uint16_t>::max()
             : static_cast<uint16_t>(game.lines());
}

uint32_t visibleScore(const Game& game) {
  return game.score() > 0 ? static_cast<uint32_t>(game.score()) : 0u;
}

uint8_t gameOverRank(const App& app, const HudModel& run) {
  if (app.screen() != Screen::GameOver) return 0;

  const uint8_t mode = static_cast<uint8_t>(app.game().rules().mode);
  if (mode >= 3) return 0;
  const uint32_t rankedValue =
      app.game().rules().mode == GameMode::FortyLine ? run.elapsedMs : run.score;
  const uint32_t emptyValue =
      mode == 1 ? std::numeric_limits<uint32_t>::max() : 0u;
  if (rankedValue == emptyValue) return 0;

  const std::size_t base = static_cast<std::size_t>(mode) * 5u;
  for (uint8_t i = 0; i < 5; ++i) {
    const ScoreEntry& entry = app.scores()[base + i];
    if (entry.score == rankedValue && entry.lines == run.lines &&
        entry.level == run.level && entry.mode == mode) {
      return static_cast<uint8_t>(i + 1u);
    }
  }
  return 0;
}

bool scoreEntryEmpty(const ScoreEntry& entry, uint8_t mode) {
  return mode == 1
             ? entry.score == std::numeric_limits<uint32_t>::max()
             : entry.score == 0u;
}

void appendBounded(char* out, std::size_t& length, const char* text) {
  if (text == nullptr) return;
  for (std::size_t i = 0;
       text[i] != '\0' && length < kInstructionChars; ++i) {
    out[length++] = text[i];
  }
}

}  // namespace

TitleModel buildTitle(const App& app) {
  TitleModel model{};
  const uint8_t mode = app.settings().mode < 3 ? app.settings().mode : 0u;
  uint32_t highScore = app.scores()[static_cast<size_t>(mode) * 5u].score;
  if (highScore == std::numeric_limits<uint32_t>::max()) {
    highScore = 0;
  }

  model.title = "STACKFALL";
  model.version = SF_VERSION;
  model.mode = kModeNames[mode];
  model.highScore = highScore;
  for (uint8_t i = 0; i < 4; ++i) {
    model.items[i] = kTitleItems[i];
  }
  model.menuIndex = app.menuIndex() < 4 ? app.menuIndex() : 0u;
  return model;
}

PlayingModel buildPlaying(const Game& game, uint32_t nowMs) {
  PlayingModel model{};

  for (int row = 0; row < kRows; ++row) {
    for (int col = 0; col < kCols; ++col) {
      model.field[row][col] = static_cast<uint8_t>(
          game.board().at(col, Board::kFirstVisibleRow + row));
    }
  }

  const ActivePiece active = game.active();
  const Shape& shape = shapeOf(active.id, active.state);
  const int ghostRow = game.ghostRow();
  model.ghostHidden = ghostRow == static_cast<int>(active.row);
  for (int i = 0; i < 4; ++i) {
    const Offset offset = shape[static_cast<size_t>(i)];
    const int activeCol = static_cast<int>(active.col) + offset.col;
    const int activeRow = static_cast<int>(active.row) + offset.row;
    model.active[i] = visibleCellRect(activeCol, activeRow);

    const int projectedRow = ghostRow + offset.row;
    model.ghost[i] = visibleCellRect(activeCol, projectedRow);
    if (!model.ghostHidden) {
      for (int j = 0; j < 4; ++j) {
        const Offset activeOffset = shape[static_cast<size_t>(j)];
        const Rect occupied = visibleCellRect(
            static_cast<int>(active.col) + activeOffset.col,
            static_cast<int>(active.row) + activeOffset.row);
        if (sameRect(model.ghost[i], occupied) && model.ghost[i].w != 0) {
          model.ghost[i] = emptyRect();
          break;
        }
      }
    }
  }

  model.hold = game.holdEmpty() ? 0u : paletteIndex(game.heldPiece());
  model.holdUsed = game.holdUsed();
  for (int i = 0; i < 3; ++i) {
    model.next[i] = i < game.queue().size()
                        ? paletteIndex(game.queue().peek(i))
                        : 0u;
  }
  buildHud(game, model.hud);

  model.banner = "";
  model.bannerMsLeft = 0;
  EventQueue events = game.events();
  GameEvent event{};
  GameEvent latest{};
  bool found = false;
  while (events.pop(event)) {
    const char* banner = bannerFor(event);
    if (banner != nullptr) {
      model.banner = banner;
      latest = event;
      found = true;
    }
  }
  if (found) {
    model.bannerMsLeft = bannerTimeLeft(game, latest, nowMs);
  }

  return model;
}

PausedModel buildPaused(const Game& game, uint8_t menuIndex,
                        uint32_t nowMs) {
  // Value-initialisation is intentional: the host test compares the entire
  // frozen model byte-for-byte, including padding.
  PausedModel model{};
  for (int i = 0; i < 4; ++i) {
    model.items[i] = kPauseItems[i];
  }
  model.index = menuIndex < 4 ? menuIndex : 0u;
  model.behind = buildPlaying(game, nowMs);
  model.behind.bannerMsLeft = 0;
  model.dim = true;
  return model;
}

PausedModel buildPaused(const Game& game, uint32_t nowMs) {
  return buildPaused(game, 0, nowMs);
}

PausedModel buildPaused(const App& app, uint32_t nowMs) {
  return buildPaused(app.game(), app.menuIndex(), nowMs);
}

GameOverModel buildGameOver(const HudModel& run, uint8_t rank,
                            uint8_t menuIndex) {
  GameOverModel model{};
  model.score = run.score;
  model.lines = run.lines;
  model.level = run.level;
  model.elapsedMs = run.elapsedMs;
  model.rank = rank >= 1 && rank <= 5 ? rank : 0u;
  model.isHighScore = model.rank != 0;
  model.items[0] = kGameOverItems[0];
  model.items[1] = kGameOverItems[1];
  model.index = menuIndex < 2 ? menuIndex : 0u;
  return model;
}

GameOverModel buildGameOver(const App& app) {
  const HudModel run{visibleScore(app.game()), app.game().level(),
                     visibleLines(app.game()), app.game().wallMs(), 0, false};
  return buildGameOver(run, gameOverRank(app, run), app.menuIndex());
}

HighScoresModel buildHighScores(const ScoreEntry* scores, uint8_t mode) {
  HighScoresModel model{};
  if (mode >= 3) mode = 0;
  const std::size_t base = static_cast<std::size_t>(mode) * 5u;

  for (uint8_t i = 0; i < 5; ++i) {
    HighScoresModel::Row& row = model.rows[i];
    row.rank = static_cast<uint8_t>(i + 1u);
    row.mode = mode;
    if (scores == nullptr || scoreEntryEmpty(scores[base + i], mode)) {
      hud::detail::copyText(row.text, sizeof(row.text), "-- EMPTY --");
      continue;
    }

    const ScoreEntry& entry = scores[base + i];
    row.score = entry.score;
    row.lines = entry.lines;
    row.mode = entry.mode;

    char score[16]{};
    uint8_t size = 0;
    // Unlike panel_hud.cpp's 85 px kScoreRect, the leaderboard owns a 133 px
    // content row; its 131 px score allowance therefore remains intentional.
    hud::formatScore(score, sizeof(score), row.score, kScoreWidthPx, size);
    char full[40]{};
    std::snprintf(full, sizeof(full), "%u %s L%u",
                  static_cast<unsigned>(row.rank), score,
                  static_cast<unsigned>(row.lines));
    hud::fitText(row.text, sizeof(row.text), full, kContentWidthPx, 1,
                 hud::Priority::Head);
  }
  return model;
}

HighScoresModel buildHighScores(const App& app) {
  const uint8_t mode = app.settings().mode < 3 ? app.settings().mode : 0u;
  return buildHighScores(app.scores(), mode);
}

SettingsModel buildSettings(const Settings& settings, uint8_t menuIndex) {
  SettingsModel model{};
  const uint8_t tiltSensitivity =
      settings.tiltSensitivity < 5 ? settings.tiltSensitivity : 4u;
  const uint8_t brightness = settings.brightness < 5 ? settings.brightness : 4u;
  const uint8_t volume = settings.volume < 5 ? settings.volume : 4u;
  const char* values[] = {
      settings.rotateCw ? "CW" : "CCW",
      settings.controlProfile == static_cast<uint8_t>(ControlProfile::BUTTONS_ONLY)
          ? "BUTTONS"
          : "TILT DIP",
      kTiltSpeedLabels[tiltSensitivity],
      kSettingLevels[brightness],
      kSettingLevels[volume],
      settings.ghostEnabled ? "ON" : "OFF",
      "",
      "",
  };
  for (uint8_t i = 0; i < kSettingsRows; ++i) {
    model.rows[i] = SettingsModel::Row{kSettingLabels[i], values[i], i == 6};
  }
  model.index = menuIndex < kSettingsRows ? menuIndex : 0u;
  return model;
}

SettingsModel buildSettings(const App& app) {
  return buildSettings(app.settings(), app.menuIndex());
}

InstructionsModel buildInstructions(Screen screen, const input::Binding* table,
                                     std::size_t n) {
  InstructionsModel model{};
  if (table == nullptr) return model;

  for (std::size_t i = 0; i < n && model.count < 12; ++i) {
    const input::Binding& binding = table[i];
    if (binding.screen != screen) continue;

    char* line = model.lines[model.count];
    std::size_t length = 0;
    appendBounded(line, length, binding.gestureLabel);
    appendBounded(line, length, "  ");
    appendBounded(line, length, binding.actionLabel);
    line[length] = '\0';
    ++model.count;
  }
  return model;
}

InstructionsModel buildCalibrating(const CalibratingView& view) {
  InstructionsModel model{};
  switch (view.step) {
    case input::CalibrationStep::Idle:
    case input::CalibrationStep::Intro: {
      hud::detail::copyText(model.lines[model.count++], sizeof(model.lines[0]),
                            "HOW TO PLAY");
      model.emphasis[0] = 1;
      hud::detail::copyText(model.lines[model.count++], sizeof(model.lines[0]),
                            "TILT = MOVE");
      model.emphasis[1] = 1;
      hud::detail::copyText(model.lines[model.count++], sizeof(model.lines[0]),
                            "TOP AWAY = ROTATE");
      hud::detail::copyText(model.lines[model.count++], sizeof(model.lines[0]),
                            "TOP TOWARD = DROP");
      hud::detail::copyText(model.lines[model.count++], sizeof(model.lines[0]),
                            "SIDE = SPEED");
      const InstructionsModel playing = buildInstructions(Screen::Playing);
      for (uint8_t i = 0; i < playing.count && model.count < 11; ++i) {
        hud::detail::copyText(model.lines[model.count],
                              sizeof(model.lines[model.count]), playing.lines[i]);
        ++model.count;
      }
      hud::detail::copyText(model.lines[model.count++],
                            sizeof(model.lines[0]), "BLUE = BEGIN");
      model.emphasis[model.count - 1u] = 1;
      break;
    }
    case input::CalibrationStep::Still:
      hud::detail::copyText(model.lines[model.count++], sizeof(model.lines[0]),
                            "HOLD STILL");
      model.emphasis[0] = 1;
      std::snprintf(model.lines[model.count++], sizeof(model.lines[0]),
                    "KEEP STILL  %u/20",
                    static_cast<unsigned>(view.stillCount > 20
                                              ? 20
                                              : view.stillCount));
      hud::detail::copyText(model.lines[model.count++], sizeof(model.lines[0]),
                            view.restless ? "HOLD STILL FIRST"
                                          : "BLUE = USE THIS POSE");
      if (view.restless) model.emphasis[2] = 1;
      model.lines[model.count++][0] = '\0';
      hud::detail::copyText(model.lines[model.count++], sizeof(model.lines[0]),
                            "RE-CALIB = BLUE x4");
      model.emphasis[4] = 2;
      break;
    case input::CalibrationStep::TiltRight:
    case input::CalibrationStep::TiltLeft:
    case input::CalibrationStep::DipAway:
    case input::CalibrationStep::DipToward:
      // Pose steps render through the practice (board) path via
      // buildCalibratePractice, not the text path.
      break;
    case input::CalibrationStep::Maze:
    case input::CalibrationStep::Rotate:
      break;
    case input::CalibrationStep::Done: {
      hud::detail::copyText(model.lines[model.count++], sizeof(model.lines[0]),
                            "DONE");
      model.emphasis[0] = 1;
      if (view.rightDefaulted && view.leftDefaulted) {
        hud::detail::copyText(model.lines[model.count++],
                              sizeof(model.lines[0]), "TILT DEFAULT");
      } else if (view.rightDefaulted) {
        hud::detail::copyText(model.lines[model.count++],
                              sizeof(model.lines[0]), "RIGHT DEFAULT");
      } else if (view.leftDefaulted) {
        hud::detail::copyText(model.lines[model.count++],
                              sizeof(model.lines[0]), "LEFT DEFAULT");
      } else {
        std::snprintf(model.lines[model.count++], sizeof(model.lines[0]),
                      "RIGHT %u deg  LEFT %u",
                      poseAngleDegrees(view.reachRightG),
                      poseAngleDegrees(view.reachLeftG));
      }
      if (view.dipDefaulted) {
        hud::detail::copyText(model.lines[model.count++],
                              sizeof(model.lines[0]), "DIP DEFAULT");
      } else {
        std::snprintf(model.lines[model.count++], sizeof(model.lines[0]),
                      "DIP %u deg", poseAngleDegrees(view.dipScaleG));
      }
      std::snprintf(model.lines[model.count++], sizeof(model.lines[0]),
                    "STRAYS %u  TIME %us",
                    static_cast<unsigned>(view.strays),
                    static_cast<unsigned>(view.elapsedS));
      hud::detail::copyText(model.lines[model.count++], sizeof(model.lines[0]),
                            "UP=ROTATE DOWN=DROP");
      hud::detail::copyText(model.lines[model.count++], sizeof(model.lines[0]),
                            "TOP AWAY = ROTATE");
      hud::detail::copyText(model.lines[model.count++], sizeof(model.lines[0]),
                            "TOP TOWARD = DROP");
      const char* speed = view.tiltGain <= 0.625f
                              ? "SLOW"
                              : view.tiltGain <= 0.875f
                                    ? "EASY"
                                    : view.tiltGain <= 1.125f
                                          ? "NORMAL"
                                          : view.tiltGain <= 1.375f ? "QUICK"
                                                                    : "FAST";
      std::snprintf(model.lines[model.count++], sizeof(model.lines[0]),
                    "SPEED %s", speed);
      hud::detail::copyText(model.lines[model.count++], sizeof(model.lines[0]),
                            "BLUE = PLAY");
      model.emphasis[model.count - 1u] = 1;
      break;
    }
  }
  return model;
}

PlayingModel buildCalibratePractice(const PracticeView& view) {
  PlayingModel model{};
  model.ghostHidden = true;
  if (view.step == input::CalibrationStep::Maze) {
    hud::detail::copyText(model.hud.left, sizeof(model.hud.left),
                          view.solved ? "DONE" : "MAZE");
    std::snprintf(model.hud.mid, sizeof(model.hud.mid), "%us",
                  static_cast<unsigned>(view.elapsedS));
    const char* speed = view.tiltGain <= 0.625f
                            ? "x0.5"
                            : view.tiltGain <= 0.875f
                                  ? "x0.8"
                                  : view.tiltGain <= 1.125f
                                        ? "x1.0"
                                        : view.tiltGain <= 1.375f ? "x1.3"
                                                                  : "x1.5";
    hud::detail::copyText(model.hud.right, sizeof(model.hud.right), speed);
    const int ballCol = std::clamp(static_cast<int>(view.ballCol), 0,
                                   static_cast<int>(kCols - 1));
    const int ballRow = std::clamp(static_cast<int>(view.ballRow), 0,
                                   static_cast<int>(kRows - 1));
    for (int row = 0; row < kRows; ++row) {
      for (int col = 0; col < kCols; ++col) {
        if (input::kMazeLayout[row][col] != '#') {
          model.field[row][col] = 5;
        }
      }
    }
    model.field[ballRow][ballCol] = view.onCourse ? 4 : 7;
    model.bannerMsLeft = kBannerDurationMs;
    if (view.solved) {
      model.banner = "DONE";
    } else if (!view.mazeArmed) {
      // Pre-arm, holding still is always the right instruction: moving prevents
      // the entry seed, and a still hand seeds the resting pose then arms.
      model.banner = "HOLD STILL";
    } else if (!view.onCourse || view.offCourseMs > 300) {
      model.banner = "RED = OFF THE PATH";
    } else {
      model.banner = "FOLLOW THE GREEN PATH";
    }
    return model;
  }

  if (view.step == input::CalibrationStep::TiltRight ||
      view.step == input::CalibrationStep::TiltLeft ||
      view.step == input::CalibrationStep::DipAway ||
      view.step == input::CalibrationStep::DipToward) {
    int boxNum = 0;
    switch (view.step) {
      case input::CalibrationStep::TiltRight: boxNum = 1; break;
      case input::CalibrationStep::TiltLeft: boxNum = 2; break;
      case input::CalibrationStep::DipAway: boxNum = 3; break;
      case input::CalibrationStep::DipToward: boxNum = 4; break;
      default: break;
    }
    std::snprintf(model.hud.left, sizeof(model.hud.left), "BOX %u/4",
                  static_cast<unsigned>(boxNum));
    if (view.windowRunning) {
      std::snprintf(model.hud.mid, sizeof(model.hud.mid), "%us",
                    static_cast<unsigned>(view.elapsedS));
    } else {
      hud::detail::copyText(model.hud.mid, sizeof(model.hud.mid), "GO");
    }
    std::snprintf(model.hud.right, sizeof(model.hud.right), "B%02u",
                  static_cast<unsigned>(
                      poseAngleDegrees(view.boxBestG)));
    model.hud.leftSize = 2;

    const int boxRow0 = std::clamp(static_cast<int>(view.boxTargetRow), 0,
                                   static_cast<int>(kRows - 2));
    const int boxCol0 = std::clamp(static_cast<int>(view.boxTargetCol), 0,
                                   static_cast<int>(kCols - 2));
    const int ballCol = std::clamp(static_cast<int>(view.boxBallCol), 0,
                                   static_cast<int>(kCols - 1));
    const int ballRow = std::clamp(static_cast<int>(view.boxBallRow), 0,
                                   static_cast<int>(kRows - 1));
    model.field[boxRow0][boxCol0] = 5;
    model.field[boxRow0][boxCol0 + 1] = 5;
    model.field[boxRow0 + 1][boxCol0] = 5;
    model.field[boxRow0 + 1][boxCol0 + 1] = 5;
    model.field[ballRow][ballCol] = view.boxReached ? 7 : 4;

    model.bannerMsLeft = kBannerDurationMs;
    if (view.boxReached) {
      model.banner = "GOT IT";
    } else if (view.poseTooSmall) {
      model.banner = "MORE, TRY AGAIN";
    } else {
      model.banner = "BALL INTO THE BOX";
    }
    return model;
  }

  model.hud.leftSize = 2;
  hud::detail::copyText(model.hud.left, sizeof(model.hud.left), "CALIB");
  hud::detail::copyText(model.hud.mid, sizeof(model.hud.mid), "6/6");
  const Shape& shape = shapeOf(PieceId::T, view.rotationState);
  for (int i = 0; i < 4; ++i) {
    const Offset offset = shape[static_cast<std::size_t>(i)];
    model.active[i] = cellRect(4 + offset.col, 20 + 6 + offset.row);
  }
  model.banner =
      view.rotationCount == 0 ? "TOP AWAY = ROTATE" : "ONCE MORE";
  hud::detail::copyText(model.hud.right, sizeof(model.hud.right), "BLUE");
  model.bannerMsLeft = kBannerDurationMs;
  return model;
}

}  // namespace sf::ui
