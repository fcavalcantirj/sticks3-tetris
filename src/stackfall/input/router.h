#pragma once

#include "control_profile.h"
#include "stackfall/app/app.h"
#include "stackfall/input/action.h"
#include "stackfall/input/basis.h"
#include "stackfall/input/bindings.h"
#include "stackfall/input/buttons.h"
#include "stackfall/input/dip.h"
#include "stackfall/input/tilt.h"

#include <cstddef>
#include <cstdint>

namespace sf::input {

inline constexpr uint32_t kDiagChordMs = 2000;
inline constexpr uint32_t kReZeroHoldMs = 700;
// Four BLUE press edges from first to fourth within this window re-capture the
// neutrals. Each tap is released before the 900 ms Hold threshold, the chord
// requires SIDE, and the four retained rotations cancel for every piece.
inline constexpr std::size_t kReCalibrateTaps = 4;
inline constexpr uint32_t kReCalibrateTapWindowMs = 600;
inline constexpr uint32_t kNeutralQuietSampleMs = 25;
inline constexpr std::size_t kNeutralQuietSamples = 20;
inline constexpr float kNeutralQuietPpG = 0.040f;
// A resting pitch offset is absorbed continuously while the player is not
// gesturing, in play and in the maze, but never during a gesture (the gate
// below still blocks a held dip). The 2 s time constant reaches 86 % of a
// resting offset in 4 s and 95 % in 6 s; a 250 ms rotate dip moves it by at
// most 0.03 G. The earlier 1 s quiet-window gate was removed: it fired on only
// ~8 % of rest windows and never kept up with the owner's +0.2..+0.5 G drift.
inline constexpr uint32_t kDipTrackTauMs = 2000;
inline constexpr uint32_t kDipTrackHoldOffMs = 500;
inline constexpr float kDipTrackReportG = 0.05f;
// The still band the maze-entry seed watches (dipN peak-to-peak over
// kMazeSeedStillMs). Also the smallest offset change worth a re-seed.
inline constexpr float kDipQuietBandG = 0.05f;
inline constexpr uint32_t kNeutralTimeoutMs = 1200;
inline constexpr float kNeutralRestMaxG = 1.15f;
inline constexpr uint32_t kNeutralTimeoutHardMs = 3000;
// Measured tutorial captures take 2-8 s; these backstops keep HOLD STILL from
// blocking forever without shortening the still window.
inline constexpr uint32_t kStillStepTimeoutMs = 6000;
inline constexpr uint32_t kStillStepForcedMs = 10000;
inline constexpr uint32_t kRotateTimeoutMs = 12000;
inline constexpr uint32_t kPoseWindowMs = 5000;
inline constexpr uint32_t kPoseHoldMs = 300;
inline constexpr float kPoseStartG = 0.05f;
inline constexpr uint32_t kPoseWaitCeilingMs = 8000;
inline constexpr uint8_t kPoseRetries = 1;
inline constexpr std::size_t kPoseRingSamples = 30;
inline constexpr int kMazeCols = 10;
inline constexpr int kMazeRows = 20;
inline constexpr char kMazeLayout[kMazeRows][kMazeCols + 1] = {
    "##########", "##########", "##########", "..........",
    ".########.", ".########.", ".########.", ".########.",
    ".########.", ".########.", ".###AA....", ".#########",
    ".#########", ".####B####", ".####.####", ".####.####",
    "......####", "##########", "##########", "##########",
};
inline constexpr int kMazeCourseLength = 42;
inline constexpr uint32_t kMazeGiveUpMs = 12000;
inline constexpr uint32_t kMazeTimeoutMs = 90000;
inline constexpr uint32_t kMazeGoalPauseMs = 500;
inline constexpr uint32_t kMazeArmHoldMs = 500;
// The maze always enters with dipOffsetG_ zeroed (captureNeutral/setBasis reset
// it), and the |dipN - offset| >= kDipEngageG gate cannot ratchet up from zero
// against a resting drift larger than the engage band. So the maze seeds the
// offset once from this much stillness (dipN span < kDipQuietBandG), banner
// HOLD STILL, and re-seeds until the maze arms. No ceiling: never seed a moving
// hand. The seed carries forward into play.
inline constexpr uint32_t kMazeSeedStillMs = 500;
inline constexpr float kMazeDefaultReachG = 4.0f * kGPerColumn;
// The ball leaves a row only 0.85 rows from its centre, about 2.6 degrees of
// pitch at the default vertical scale, above the measured resting noise.
inline constexpr float kMazeRowHysteresis = 0.35f;
inline constexpr int kMazeSkipAhead = 3;
inline constexpr float kTiltSpeedGain[5] = {0.5f, 0.75f, 1.0f, 1.25f,
                                             1.5f};

enum class NeutralCaptureSource : uint8_t {
  Still,
  Timeout,
  Forced,
  Manual,
  Drift,
  Seed,
};

enum class CalibrationStep : uint8_t {
  Idle,
  Intro,
  Still,
  TiltRight,
  TiltLeft,
  DipAway,
  DipToward,
  Maze,
  Targets = Maze,
  Rotate,
  Done,
};

constexpr const char* neutralCaptureSourceName(NeutralCaptureSource source) {
  switch (source) {
    case NeutralCaptureSource::Still:
      return "still";
    case NeutralCaptureSource::Timeout:
      return "timeout";
    case NeutralCaptureSource::Forced:
      return "forced";
    case NeutralCaptureSource::Manual:
      return "manual";
    case NeutralCaptureSource::Drift:
      return "drift";
    case NeutralCaptureSource::Seed:
      return "seed";
  }
  return "timeout";
}

inline constexpr const char* kTraceActionNames[] = {
    "NONE",       "LEFT",      "RIGHT",       "SOFT_DROP",
    "SOFT_DROP_OFF", "ROTATE_CW", "ROTATE_CCW", "HARD_DROP",
    "HOLD",       "PAUSE",     "CONFIRM",     "BACK",
    "MENU_UP",    "MENU_DOWN", "DIAG",        "CALIBRATE",
    "TILT_SPEED",
};

inline constexpr const char* kTraceScreenNames[] = {
    "TITLE",      "PLAYING",     "PAUSED",       "GAMEOVER",
    "HIGHSCORES", "SETTINGS",    "INSTRUCTIONS", "DIAGNOSTICS",
    "CALIBRATING",
};

static_assert(sizeof(kTraceActionNames) / sizeof(kTraceActionNames[0]) == 17,
              "trace action vocabulary must match ActionKind");
static_assert(sizeof(kTraceScreenNames) / sizeof(kTraceScreenNames[0]) == 9,
              "trace screen vocabulary must match Screen");

int formatTrace(char* out, std::size_t n, const GameAction& action,
                Screen screen, uint8_t depth, uint16_t dropped);

// Pure per-screen arbitration over caller-supplied button states, raw
// accelerometer samples in G, and time. yG is the measured steering axis and
// zG is the signed face-normal dip axis; xG is also used by the calibrated
// tutorial's stillness and learned basis.
class InputRouter {
 public:
  InputRouter();

  void setProfile(ControlProfile profile);
  void setRotateCcw(bool ccw);
  void update(bool bluePressed, bool sidePressed, float yG, float zG,
              Screen screen, uint32_t nowMs, ActionQueue& out);
  void update(bool bluePressed, bool sidePressed, float yG, float zG,
              bool imuFresh, Screen screen, uint32_t nowMs,
              ActionQueue& out);
  void update(bool bluePressed, bool sidePressed, float xG, float yG,
              float zG, bool imuFresh, Screen screen, uint32_t nowMs,
              ActionQueue& out);
  void setBasis(const TiltBasis& basis);
  const TiltBasis& basis() const;
  void clearBasis();
  void setTiltSpeed(uint8_t level);
  float tiltGain() const;
  void beginCalibration(bool full);
  CalibrationStep calibrationStep() const { return calibrationStep_; }
  uint8_t stillCount() const {
    return neutralWindowCount_ > kNeutralQuietSamples
               ? static_cast<uint8_t>(kNeutralQuietSamples)
               : static_cast<uint8_t>(neutralWindowCount_);
  }
  float stillPpX() const { return neutralWindowPpX_; }
  float stillPpY() const { return neutralWindowPpYG_; }
  float stillPpZ() const { return neutralWindowPpZG_; }
  bool restless() const { return calibrationRestless_; }
  float poseDeflectionG() const { return poseDeflectionG_; }
  bool poseTooSmall() const { return poseTooSmall_; }
  bool poseWindowRunning() const { return poseWindowRunning_; }
  uint16_t poseRemainingS(uint32_t nowMs) const {
    if (!calibrationPoseStep()) return 0;
    const uint32_t elapsed = nowMs - poseStartedMs_;
    if (elapsed >= kPoseWindowMs) return 0;
    const uint32_t remaining = kPoseWindowMs - elapsed;
    return remaining > 999u
               ? static_cast<uint16_t>((remaining + 999u) / 1000u)
               : 1u;
  }
  int boxBallCol() const { return boxBallCol_; }
  int boxBallRow() const { return boxBallRow_; }
  int boxTargetCol() const { return boxTargetCol_; }
  int boxTargetRow() const { return boxTargetRow_; }
  bool boxReached() const { return boxReached_; }
  float poseBestG() const { return poseBestG_; }
  uint8_t poseRetry() const { return poseRetry_; }
  Vec3 calibrationRight() const { return calibrationRight_; }
  Vec3 calibrationLeft() const { return calibrationLeft_; }
  bool rightDefaulted() const { return rightDefaulted_; }
  bool leftDefaulted() const { return leftDefaulted_; }
  bool dipDefaulted() const { return dipDefaulted_; }
  uint8_t rotationCount() const { return calibrationRotationCount_; }
  uint8_t calibrationRotationState() const { return calibrationRotationState_; }
  int mazeBallCol() const { return mazeBallCol_; }
  int mazeBallRow() const { return mazeBallRow_; }
  bool mazeOnCourse() const { return mazeOnCourse_; }
  uint32_t mazeOffCourseMs(uint32_t nowMs) const {
    return mazeOnCourse_ ? 0u : nowMs - mazeOffCourseSinceMs_;
  }
  uint8_t mazeProgress() const { return mazeProgress_; }
  uint8_t mazeStrays() const { return mazeStrays_; }
  bool mazeSolved() const { return mazeSolved_; }
  bool mazeArmed() const { return mazeArmed_; }
  float mazeReachRightG() const { return mazeReachRightG_; }
  float mazeReachLeftG() const { return mazeReachLeftG_; }
  uint32_t mazeElapsedMs(uint32_t nowMs) const {
    return calibrationFull_ ? nowMs - mazeStartedMs_ : 0u;
  }
  void setActivePiece(const ActivePiece& piece, bool valid);
  uint32_t chordHeldMs(uint32_t nowMs) const {
    return buttons_.chordHeldMs(nowMs);
  }
  int selectedColumn() const { return previousColumn_; }
  int pieceColumn() const {
    return pieceKnown_ ? static_cast<int>(piece_.col) : -1;
  }
  float tiltSignalG() const { return tilt_.signalG(); }
  float dipSignalG() const { return dip_.signalG(); }
  bool dipEngaged() const { return dip_.engaged(); }
  bool softDropActive() const { return dip_.softDropActive(); }
  bool neutralReady() const { return imuReady_; }
  uint32_t neutralCaptureCount() const { return neutralCaptureCount_; }
  uint32_t neutralCapturedAtMs() const { return neutralCapturedAtMs_; }
  float neutralYG() const { return neutralYG_; }
  float neutralZG() const { return neutralZG_; }
  float neutralXG() const { return neutralXG_; }
  int neutralColumn() const { return neutralColumn_; }
  float lowEdgeMagnitudeG() const { return lowEdgeMagnitudeG_; }
  float highEdgeMagnitudeG() const { return highEdgeMagnitudeG_; }
  uint32_t reZeroCount() const { return reZeroCount_; }
  uint32_t reCalibrateCount() const { return reCalibrateCount_; }
  bool lastCaptureWasManual() const { return lastCaptureWasManual_; }
  NeutralCaptureSource lastCaptureSource() const {
    return lastCaptureSource_;
  }
  float neutralWindowPpYG() const { return neutralWindowPpYG_; }
  float neutralWindowPpZG() const { return neutralWindowPpZG_; }
  float dipOffsetG() const { return dipOffsetG_; }
  bool mazeSeedPending() const { return mazeSeedPending_; }
  float rawXG() const { return rawXG_; }
  float rawYG() const { return rawYG_; }
  float rawZG() const { return rawZG_; }

 private:
  void resetImu();
  void resetNeutralWindow();
  void resetPoseRing();
  void captureNeutral(float xG, float yG, float zG, uint32_t nowMs,
                      NeutralCaptureSource source);
  void collectNeutralSample(float xG, float yG, float zG, uint32_t nowMs);
  float neutralWindowPeakToPeak(const float* samples) const;
  Vec3 neutralWindowMean() const;
  bool neutralWindowIsStill() const;
  bool calibrationPoseStep() const;
  void updateBoxView(const Vec3& sample);
  float calibrationPoseDeflection(const Vec3& sample) const;
  void finishCalibrationPose(const Vec3& pose, bool defaulted,
                             uint32_t nowMs);
  bool trackDipNeutral(float dipN, bool bluePressed,
                       bool sidePressed, Screen screen, uint32_t dtMs,
                       uint32_t nowMs);
  void finishCalibrationStill(const Vec3& pose, uint32_t nowMs,
                              NeutralCaptureSource source);
  void buildMazeCourse();
  void startMaze(uint32_t nowMs);
  void seedMazeDipOffset(float dipN, uint32_t nowMs);
  void finishMazeToRotate(uint32_t nowMs);
  void updateMaze(int selectedColumn, float dipRaw, uint32_t nowMs);
  void routeCalibrationBluePress(float xG, float yG, float zG,
                                 bool imuFresh, uint32_t nowMs,
                                 ActionQueue& out);
  bool tryManualReZero(bool bluePressed, bool sidePressed, float xG,
                       float yG, float zG, Screen screen, uint32_t nowMs);
  void routeButtons(Screen screen, ActionQueue& source, ActionQueue& out,
                    float xG, float yG, float zG, bool imuFresh);
  void routeDiagChord(Screen screen, uint32_t nowMs, ActionQueue& out);
  void routeDip(Screen screen, ActionQueue& source, ActionQueue& out);
  void emit(Screen screen, Gesture gesture, ActionKind emitted,
            uint32_t nowMs, ActionQueue& out) const;

  ButtonGesture buttons_;
  Tilt tilt_;
  Dip dip_;
  ControlProfile profile_ = kActiveControlProfile;
  bool rotateCcw_ = false;
  bool imuReady_ = false;
  bool imuTracking_ = false;
  Screen imuScreen_ = Screen::Title;
  uint32_t imuTrackingStartedMs_ = 0;
  uint32_t stillStepStartedMs_ = 0;
  uint32_t previousMs_ = 0;
  int previousColumn_ = 4;
  uint32_t neutralCaptureCount_ = 0;
  uint32_t neutralCapturedAtMs_ = 0;
  float neutralXG_ = 0.0f;
  float neutralYG_ = 0.0f;
  float neutralZG_ = 0.0f;
  float lowEdgeMagnitudeG_ = 4.0f * kGPerColumn;
  float highEdgeMagnitudeG_ = 4.0f * kGPerColumn;
  int neutralColumn_ = 4;
  uint32_t reZeroCount_ = 0;
  bool lastCaptureWasManual_ = false;
  NeutralCaptureSource lastCaptureSource_ = NeutralCaptureSource::Still;
  float neutralWindowPpYG_ = 0.0f;
  float neutralWindowPpZG_ = 0.0f;
  float neutralWindowPpX_ = 0.0f;
  float neutralWindowX_[kNeutralQuietSamples]{};
  float neutralWindowY_[kNeutralQuietSamples]{};
  float neutralWindowZ_[kNeutralQuietSamples]{};
  std::size_t neutralWindowIndex_ = 0;
  std::size_t neutralWindowCount_ = 0;
  uint32_t lastNeutralSampleMs_ = 0;
  bool haveNeutralSampleTime_ = false;
  float rawXG_ = 0.0f;
  float rawYG_ = 0.0f;
  float rawZG_ = 0.0f;
  uint32_t lastDipActionMs_ = 0;
  float dipOffsetG_ = 0.0f;
  float reportedDipOffsetG_ = 0.0f;
  uint32_t lastDriftReportMs_ = 0;
  bool reZeroLatched_ = false;
  uint32_t blueTapMs_[kReCalibrateTaps]{};
  std::size_t blueTapCount_ = 0;
  bool reCalibrateRequested_ = false;
  uint32_t reCalibrateCount_ = 0;
  CalibrationStep calibrationStep_ = CalibrationStep::Idle;
  bool calibrationFull_ = false;
  bool calibrationRestless_ = false;
  uint8_t calibrationRotationCount_ = 0;
  uint8_t calibrationRotationState_ = 0;
  uint32_t rotateStartedMs_ = 0;
  uint32_t poseStartedMs_ = 0;
  uint32_t poseWindowEntryMs_ = 0;
  bool poseWindowRunning_ = false;
  Vec3 poseDeflectionRing_[kPoseRingSamples]{};
  Vec3 poseSampleRing_[kPoseRingSamples]{};
  std::size_t poseDeflectionRingIndex_ = 0;
  std::size_t poseDeflectionRingCount_ = 0;
  std::size_t poseSampleRingIndex_ = 0;
  std::size_t poseSampleRingCount_ = 0;
  Vec3 calibrationRight_{};
  Vec3 calibrationLeft_{};
  Vec3 calibrationAway_{};
  Vec3 calibrationToward_{};
  float poseDeflectionG_ = 0.0f;
  bool poseTooSmall_ = false;
  float poseBestG_ = 0.0f;
  Vec3 poseBestSample_{};
  uint8_t poseRetry_ = 0;
  Vec3 boxE1_{};
  Vec3 boxE2_{};
  int8_t boxBallCol_ = 4;
  int8_t boxBallRow_ = 10;
  int8_t boxTargetCol_ = 4;
  int8_t boxTargetRow_ = 10;
  bool boxReached_ = false;
  bool rightDefaulted_ = false;
  bool leftDefaulted_ = false;
  bool dipDefaulted_ = false;
  bool calibrationSampleSkip_ = false;
  bool calibrationButtonsOnlyConfirmed_ = false;
  struct MazeCell {
    int8_t col;
    int8_t row;
  };
  MazeCell mazeCourse_[kMazeCourseLength]{};
  int8_t mazeBallCol_ = 4;
  int8_t mazeBallRow_ = 10;
  bool mazeOnCourse_ = true;
  uint32_t mazeOffCourseSinceMs_ = 0;
  uint8_t mazeProgress_ = 0;
  uint8_t mazeStrays_ = 0;
  bool mazeSolved_ = false;
  bool mazeWasOnCourse_ = true;
  uint32_t mazeStartedMs_ = 0;
  uint32_t mazeLastProgressMs_ = 0;
  uint32_t mazeGoalReachedMs_ = 0;
  float mazeReachRightG_ = kMazeDefaultReachG;
  float mazeReachLeftG_ = kMazeDefaultReachG;
  bool mazeArmed_ = false;
  uint32_t mazeArmedMs_ = 0;
  bool mazeSeedPending_ = false;
  uint32_t mazeSeedStillStartMs_ = 0;
  float mazeSeedStillMin_ = 0.0f;
  float mazeSeedStillMax_ = 0.0f;
  bool pieceKnown_ = false;
  ActivePiece piece_{};
  bool haveEmittedTarget_ = false;
  int lastEmittedTarget_ = 0;
  ActivePiece lastEmittedPiece_{};
  bool diagChordTracking_ = false;
  bool diagChordEligible_ = false;
  bool diagChordLatched_ = false;
  TiltBasis basis_ = identityBasis();
  float tiltGain_ = kTiltSpeedGain[2];
};

}  // namespace sf::input
