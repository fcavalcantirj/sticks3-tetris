#pragma once

#include <cstddef>
#include <cstdint>

#include "stackfall/app/app.h"
#include "stackfall/input/bindings.h"
#include "stackfall/input/router.h"
#include "stackfall/ui/hud.h"
#include "stackfall/ui/layout.h"

namespace sf::ui {

struct TitleModel {
  const char* title;
  const char* version;
  const char* mode;
  uint32_t highScore;
  const char* items[4];
  uint8_t menuIndex;
};

struct PlayingModel {
  uint8_t field[20][10];
  Rect active[4];
  Rect ghost[4];
  bool ghostHidden;
  uint8_t hold;
  bool holdUsed;
  uint8_t next[3];
  HudText hud;
  const char* banner;
  uint16_t bannerMsLeft;
};

struct PausedModel {
  const char* items[4];
  uint8_t index;
  PlayingModel behind;
  bool dim;
};

struct GameOverModel {
  uint32_t score;
  uint16_t lines;
  uint16_t level;
  uint32_t elapsedMs;
  bool isHighScore;
  uint8_t rank;
  const char* items[2];
  uint8_t index;
};

struct HighScoresModel {
  struct Row {
    uint8_t rank;
    uint32_t score;
    uint16_t lines;
    uint8_t mode;
    char text[23];
  } rows[5];
};

inline constexpr uint8_t kSettingsRows = 8;

struct SettingsModel {
  struct Row {
    const char* label;
    const char* value;
    bool destructive;
  } rows[kSettingsRows];
  uint8_t index;
};

struct InstructionsModel {
  char lines[12][24];
  uint8_t count;
  uint8_t emphasis[12];
};

struct CalibratingView {
  input::CalibrationStep step;
  uint8_t stillCount;
  bool restless;
  float ppY;
  float ppZ;
  float poseDeflectionG;
  bool poseTooSmall;
  uint16_t poseRemainingS;
  int8_t boxBallCol;
  int8_t boxBallRow;
  int8_t boxTargetCol;
  int8_t boxTargetRow;
  bool boxReached;
  float poseBestG;
  uint8_t poseRetry;
  bool rightDefaulted;
  bool leftDefaulted;
  bool dipDefaulted;
  float steerScaleG;
  float dipScaleG;
  float steerAsymmetryG;
  float tiltGain;
  float reachRightG;
  float reachLeftG;
  uint8_t strays;
  uint16_t elapsedS;
  bool mazeSolved;
  uint16_t mazeOffCourseMs;
  uint8_t rotationCount;
  uint8_t rotationState;
  bool mazeArmed = false;
  bool windowRunning = false;
  float dipFactor = 1.0f;
  float steerFactor = 1.0f;
  float steerFactorRight = 1.0f;
  float steerFactorLeft = 1.0f;
  int8_t ballCol = 4;
  int8_t ballRow = 10;
  bool onCourse = true;
  uint8_t mazeProgress = 0;
  int targetColumn = 4;
  int selectedColumn = 10;
  uint8_t hitCount = 0;
};

struct PracticeView {
  PracticeView() = default;
  PracticeView(input::CalibrationStep step, int targetColumn,
               int selectedColumn, uint8_t hitCount, uint8_t rotationCount,
               uint8_t rotationState, uint16_t offCourseMsArg = 0,
               bool solvedArg = false, bool mazeArmedArg = false,
               bool windowRunningArg = false)
      : step(step),
        ballCol(static_cast<int8_t>(targetColumn)),
        ballRow(static_cast<int8_t>(selectedColumn)),
        onCourse((rotationState & 0x80u) != 0),
        progress(hitCount),
        elapsedS(rotationCount),
        tiltGain(input::kTiltSpeedGain[rotationState & 0x7Fu]),
        rotationCount(step == input::CalibrationStep::Rotate ? rotationCount
                                                               : 0),
        rotationState(step == input::CalibrationStep::Rotate ? rotationState
                                                               : 0),
        offCourseMs(offCourseMsArg),
        solved(solvedArg ||
               (step == input::CalibrationStep::Maze &&
                hitCount >= input::kMazeCourseLength - 1)),
        mazeArmed(mazeArmedArg),
        windowRunning(windowRunningArg) {}
  PracticeView(input::CalibrationStep step, int8_t ballCol, int8_t ballRow,
               bool onCourse, uint8_t progress, uint16_t elapsedS,
               float tiltGain, uint8_t rotationCount = 0,
               uint8_t rotationState = 0, int8_t boxBallColArg = 4,
               int8_t boxBallRowArg = 10, int8_t boxTargetColArg = 4,
               int8_t boxTargetRowArg = 10, bool boxReachedArg = false,
               bool poseTooSmallArg = false, float boxBestGArg = 0.0f,
               bool mazeArmedArg = false, bool windowRunningArg = false)
      : step(step),
        ballCol(ballCol),
        ballRow(ballRow),
        onCourse(onCourse),
        progress(progress),
        elapsedS(elapsedS),
        tiltGain(tiltGain),
        rotationCount(rotationCount),
        rotationState(rotationState),
        boxBallCol(boxBallColArg),
        boxBallRow(boxBallRowArg),
        boxTargetCol(boxTargetColArg),
        boxTargetRow(boxTargetRowArg),
        boxReached(boxReachedArg),
        poseTooSmall(poseTooSmallArg),
        boxBestG(boxBestGArg),
        mazeArmed(mazeArmedArg),
        windowRunning(windowRunningArg) {}
  input::CalibrationStep step;
  int8_t ballCol;
  int8_t ballRow;
  bool onCourse;
  uint8_t progress;
  uint16_t elapsedS;
  float tiltGain;
  uint8_t rotationCount;
  uint8_t rotationState;
  int8_t boxBallCol = 4;
  int8_t boxBallRow = 10;
  int8_t boxTargetCol = 4;
  int8_t boxTargetRow = 10;
  bool boxReached = false;
  bool poseTooSmall = false;
  float boxBestG = 0.0f;
  uint16_t offCourseMs = 0;
  bool solved = false;
  bool mazeArmed = false;
  bool windowRunning = false;
};

static_assert(input::kMaxBindingsPerScreen <= 12);

constexpr uint16_t kBannerDurationMs = 1200;

TitleModel buildTitle(const App& app);
PlayingModel buildPlaying(const Game& game, uint32_t nowMs);
PausedModel buildPaused(const Game& game, uint8_t menuIndex,
                        uint32_t nowMs);
PausedModel buildPaused(const Game& game, uint32_t nowMs);
PausedModel buildPaused(const App& app, uint32_t nowMs);
GameOverModel buildGameOver(const HudModel& run, uint8_t rank = 0,
                            uint8_t menuIndex = 0);
GameOverModel buildGameOver(const App& app);
HighScoresModel buildHighScores(const ScoreEntry* scores, uint8_t mode);
HighScoresModel buildHighScores(const App& app);
SettingsModel buildSettings(const Settings& settings, uint8_t menuIndex = 0);
SettingsModel buildSettings(const App& app);
InstructionsModel buildInstructions(
    Screen screen, const input::Binding* table = input::kBindings,
    std::size_t n = input::kBindingCount);
InstructionsModel buildCalibrating(const CalibratingView& view);
PlayingModel buildCalibratePractice(const PracticeView& view);

}  // namespace sf::ui
