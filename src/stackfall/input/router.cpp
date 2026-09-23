#include "stackfall/input/router.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace sf::input {
namespace {

const char* actionName(ActionKind action) {
  const std::size_t index = static_cast<std::size_t>(action);
  return index < (sizeof(kTraceActionNames) / sizeof(kTraceActionNames[0]))
             ? kTraceActionNames[index]
             : kTraceActionNames[0];
}

const char* screenName(Screen screen) {
  const std::size_t index = static_cast<std::size_t>(screen);
  return index < (sizeof(kTraceScreenNames) / sizeof(kTraceScreenNames[0]))
             ? kTraceScreenNames[index]
             : kTraceScreenNames[0];
}

const char* sourceName(ActionKind action) {
  const uint8_t index = static_cast<uint8_t>(action);
  // GameAction intentionally has no source byte. The pinned trace contract
  // therefore classifies the four movement/soft-drop values as TILT and the
  // remaining button-compatible values as BTN.
  return index >= 1u && index <= 4u ? "TILT" : "BTN";
}

int zeroReferencedColumn(float correctedG) {
  const float edgeG = 4.0f * kGPerColumn;
  if (correctedG <= -edgeG) return 0;
  if (correctedG >= edgeG) return 9;
  int column = static_cast<int>(
      std::floor(correctedG / kGPerColumn + 5.0f));
  if (column < 0) return 0;
  if (column > 9) return 9;
  return column;
}

}  // namespace

int formatTrace(char* out, std::size_t n, const GameAction& action,
                Screen screen, uint8_t depth, uint16_t dropped) {
  if (out == nullptr) {
    n = 0;
  }
  return std::snprintf(
      out, n, "[TRACE] t=%u scr=%s act=%s src=%s q=%u/16 drop=%u",
      static_cast<unsigned>(action.tMs), screenName(screen),
      actionName(action.kind), sourceName(action.kind),
      static_cast<unsigned>(depth), static_cast<unsigned>(dropped));
}

InputRouter::InputRouter() {
  buttons_.setProfile(profile_);
  buildMazeCourse();
}

void InputRouter::setBasis(const TiltBasis& basis) {
  basis_ = basis;
  dipOffsetG_ = 0.0f;
  reportedDipOffsetG_ = 0.0f;
}

const TiltBasis& InputRouter::basis() const { return basis_; }

void InputRouter::clearBasis() {
  basis_ = identityBasis();
  dipOffsetG_ = 0.0f;
  reportedDipOffsetG_ = 0.0f;
}

void InputRouter::setTiltSpeed(uint8_t level) {
  if (level >= 5u) level = 4u;
  tiltGain_ = kTiltSpeedGain[level];
}

float InputRouter::tiltGain() const { return tiltGain_; }

void InputRouter::beginCalibration(bool full) {
  resetImu();
  calibrationFull_ = full;
  calibrationStep_ = full ? CalibrationStep::Intro : CalibrationStep::Still;
  calibrationRestless_ = false;
  calibrationRotationCount_ = 0;
  calibrationRotationState_ = 0;
  rotateStartedMs_ = 0;
  poseStartedMs_ = 0;
  poseWindowEntryMs_ = 0;
  poseWindowRunning_ = false;
  stillStepStartedMs_ = 0;
  poseStartedMs_ = 0;
  stillStepStartedMs_ = 0;
  resetPoseRing();
  calibrationRight_ = Vec3{};
  calibrationLeft_ = Vec3{};
  calibrationAway_ = Vec3{};
  calibrationToward_ = Vec3{};
  poseDeflectionG_ = 0.0f;
  poseTooSmall_ = false;
  poseBestG_ = 0.0f;
  poseBestSample_ = Vec3{};
  poseRetry_ = 0;
  boxBallCol_ = 4;
  boxBallRow_ = 10;
  boxTargetCol_ = 4;
  boxTargetRow_ = 10;
  boxReached_ = false;
  rightDefaulted_ = false;
  leftDefaulted_ = false;
  dipDefaulted_ = false;
  calibrationSampleSkip_ = false;
  calibrationButtonsOnlyConfirmed_ = false;
  neutralWindowPpX_ = 0.0f;
  neutralWindowPpYG_ = 0.0f;
  neutralWindowPpZG_ = 0.0f;
}

void InputRouter::buildMazeCourse() {
  bool visited[kMazeRows][kMazeCols]{};
  int col = 4;
  int row = 10;
  for (int index = 0; index < kMazeCourseLength; ++index) {
    mazeCourse_[index] = MazeCell{static_cast<int8_t>(col),
                                  static_cast<int8_t>(row)};
    visited[row][col] = true;
    if (index + 1 == kMazeCourseLength) break;

    // The two A cells are one shared start marker in the 42-index course.
    if (index == 0 && col == 4 && row == 10) {
      visited[10][5] = true;
      col = 6;
      continue;
    }

    constexpr int kDirections[4][2] = {
        {1, 0}, {0, 1}, {-1, 0}, {0, -1}};
    int nextCol = col;
    int nextRow = row;
    for (const auto& direction : kDirections) {
      const int candidateCol = col + direction[0];
      const int candidateRow = row + direction[1];
      if (candidateCol < 0 || candidateCol >= kMazeCols ||
          candidateRow < 0 || candidateRow >= kMazeRows ||
          visited[candidateRow][candidateCol] ||
          kMazeLayout[candidateRow][candidateCol] == '#') {
        continue;
      }
      nextCol = candidateCol;
      nextRow = candidateRow;
      break;
    }
    col = nextCol;
    row = nextRow;
  }
}

void InputRouter::setProfile(ControlProfile profile) {
  buttons_.setProfile(profile);
  if (profile_ != profile) {
    profile_ = profile;
    resetImu();
  }
}

void InputRouter::setRotateCcw(bool ccw) {
  rotateCcw_ = ccw;
  buttons_.setRotateCcw(ccw);
}

void InputRouter::setActivePiece(const ActivePiece& piece, bool valid) {
  if (!valid) {
    pieceKnown_ = false;
    haveEmittedTarget_ = false;
    return;
  }
  const bool changed =
      !pieceKnown_ || piece_.id != piece.id || piece_.state != piece.state ||
      piece_.col != piece.col || piece_.row != piece.row;
  piece_ = piece;
  pieceKnown_ = true;
  if (changed) {
    haveEmittedTarget_ = false;
    lastEmittedTarget_ = 0;
    lastEmittedPiece_ = ActivePiece{};
  }
}

void InputRouter::resetImu() {
  tilt_.reset();
  dip_.reset();
  imuReady_ = false;
  imuTracking_ = false;
  imuTrackingStartedMs_ = 0;
  previousMs_ = 0;
  previousColumn_ = 4;
  pieceKnown_ = false;
  haveEmittedTarget_ = false;
  for (std::size_t i = 0; i < kReCalibrateTaps; ++i) {
    blueTapMs_[i] = 0;
  }
  blueTapCount_ = 0;
  resetNeutralWindow();
  lastDipActionMs_ = 0;
}

void InputRouter::resetNeutralWindow() {
  neutralWindowIndex_ = 0;
  neutralWindowCount_ = 0;
  lastNeutralSampleMs_ = 0;
  haveNeutralSampleTime_ = false;
}

void InputRouter::resetPoseRing() {
  poseDeflectionRingIndex_ = 0;
  poseDeflectionRingCount_ = 0;
  poseSampleRingIndex_ = 0;
  poseSampleRingCount_ = 0;
}

void InputRouter::captureNeutral(float xG, float yG, float zG,
                                 uint32_t nowMs, NeutralCaptureSource source) {
  tilt_.captureNeutral(yG);
  dip_.captureNeutral(zG);
  previousColumn_ = tilt_.column();
  previousMs_ = nowMs;
  imuReady_ = true;
  for (std::size_t i = 0; i < kReCalibrateTaps; ++i) {
    blueTapMs_[i] = 0;
  }
  blueTapCount_ = 0;

  neutralXG_ = xG;
  neutralYG_ = yG;
  neutralZG_ = zG;
  basis_.g0 = Vec3{xG, yG, zG};
  dipOffsetG_ = 0.0f;
  reportedDipOffsetG_ = 0.0f;
  neutralCapturedAtMs_ = nowMs;
  ++neutralCaptureCount_;
  if (source == NeutralCaptureSource::Manual) ++reZeroCount_;
  lastCaptureWasManual_ = source == NeutralCaptureSource::Manual;
  lastCaptureSource_ = source;
  neutralWindowPpX_ = neutralWindowPeakToPeak(neutralWindowX_);
  neutralWindowPpYG_ = neutralWindowPeakToPeak(neutralWindowY_);
  neutralWindowPpZG_ = neutralWindowPeakToPeak(neutralWindowZ_);

  const Vec3 g0n = scale(basis_.g0, 1.0f / norm(basis_.g0));
  const Vec3 yAxis = Vec3{0.0f, static_cast<float>(kTiltSignLeftRight),
                           0.0f};
  Vec3 e1 = sub(yAxis, scale(g0n, dot(yAxis, g0n)));
  const float e1Len = norm(e1);
  if (e1Len > 0.0f) {
    boxE1_ = scale(e1, 1.0f / e1Len);
  } else {
    boxE1_ = Vec3{0.0f, static_cast<float>(kTiltSignLeftRight), 0.0f};
  }
  const Vec3 cross = Vec3{
      g0n.y * boxE1_.z - g0n.z * boxE1_.y,
      g0n.z * boxE1_.x - g0n.x * boxE1_.z,
      g0n.x * boxE1_.y - g0n.y * boxE1_.x};
  const float crossLen = norm(cross);
  boxE2_ = crossLen > 0.0f ? scale(cross, 1.0f / crossLen) : Vec3{0, 0, 1};

  const float correctedG =
      yG * static_cast<float>(kTiltSignLeftRight);
  neutralColumn_ = zeroReferencedColumn(correctedG);
  const float edgeG = 4.0f * kGPerColumn;
  lowEdgeMagnitudeG_ = std::fabs(correctedG - edgeG);
  highEdgeMagnitudeG_ = std::fabs(correctedG + edgeG);
}

float InputRouter::neutralWindowPeakToPeak(const float* samples) const {
  if (neutralWindowCount_ < 2) return 0.0f;
  float minimum = samples[0];
  float maximum = samples[0];
  for (std::size_t i = 1; i < neutralWindowCount_; ++i) {
    if (samples[i] < minimum) minimum = samples[i];
    if (samples[i] > maximum) maximum = samples[i];
  }
  return maximum - minimum;
}

bool InputRouter::neutralWindowIsStill() const {
  return neutralWindowCount_ == kNeutralQuietSamples &&
         neutralWindowPeakToPeak(neutralWindowY_) < kNeutralQuietPpG &&
         neutralWindowPeakToPeak(neutralWindowZ_) < kNeutralQuietPpG;
}

Vec3 InputRouter::neutralWindowMean() const {
  if (neutralWindowCount_ == 0) return basis_.g0;
  Vec3 total{};
  for (std::size_t i = 0; i < neutralWindowCount_; ++i) {
    total.x += neutralWindowX_[i];
    total.y += neutralWindowY_[i];
    total.z += neutralWindowZ_[i];
  }
  const float divisor = static_cast<float>(neutralWindowCount_);
  return Vec3{total.x / divisor, total.y / divisor, total.z / divisor};
}

bool InputRouter::calibrationPoseStep() const {
  return calibrationStep_ == CalibrationStep::TiltRight ||
         calibrationStep_ == CalibrationStep::TiltLeft ||
         calibrationStep_ == CalibrationStep::DipAway ||
         calibrationStep_ == CalibrationStep::DipToward;
}

void InputRouter::updateBoxView(const Vec3& sample) {
  const float dx = dot(sub(sample, basis_.g0), boxE1_);
  const float dy = dot(sub(sample, basis_.g0), boxE2_);
  const float boxScale = kMazeDefaultReachG;

  float colF = 4.5f + dx / boxScale * 4.5f;
  int candidateCol = static_cast<int>(std::lround(colF));
  if (candidateCol < 0) candidateCol = 0;
  if (candidateCol > 9) candidateCol = 9;
  if (candidateCol != boxBallCol_ &&
      std::fabs(colF - static_cast<float>(boxBallCol_)) >= kMazeRowHysteresis) {
    boxBallCol_ = static_cast<int8_t>(candidateCol);
  }

  float rowF = 9.5f - dy / boxScale * 9.5f;
  int candidateRow = static_cast<int>(std::lround(rowF));
  if (candidateRow < 0) candidateRow = 0;
  if (candidateRow > 19) candidateRow = 19;
  if (candidateRow != boxBallRow_ &&
      std::fabs(rowF - static_cast<float>(boxBallRow_)) >= kMazeRowHysteresis) {
    boxBallRow_ = static_cast<int8_t>(candidateRow);
  }

  switch (calibrationStep_) {
    case CalibrationStep::TiltRight:
      boxTargetCol_ = 8;
      boxTargetRow_ = 9;
      break;
    case CalibrationStep::TiltLeft:
      boxTargetCol_ = 0;
      boxTargetRow_ = 9;
      break;
    case CalibrationStep::DipAway:
      boxTargetCol_ = 4;
      boxTargetRow_ = 1;
      break;
    case CalibrationStep::DipToward:
      boxTargetCol_ = 4;
      boxTargetRow_ = 18;
      break;
    default:
      break;
  }

  boxReached_ = boxBallCol_ >= boxTargetCol_ && boxBallCol_ <= boxTargetCol_ + 1 &&
                boxBallRow_ >= boxTargetRow_ && boxBallRow_ <= boxTargetRow_ + 1;
}

float InputRouter::calibrationPoseDeflection(const Vec3& sample) const {
  const Vec3 delta = sub(sample, basis_.g0);
  if (calibrationStep_ == CalibrationStep::TiltRight) {
    return std::max(0.0f, dot(delta, identityBasis().s));
  }
  if (calibrationStep_ == CalibrationStep::TiltLeft) {
    Vec3 direction = sub(calibrationRight_, basis_.g0);
    const float length = norm(direction);
    if (length <= 0.0f) direction = identityBasis().s;
    else direction = scale(direction, 1.0f / length);
    return std::max(0.0f, -dot(delta, direction));
  }
  if (calibrationStep_ == CalibrationStep::DipToward) {
    Vec3 direction = basis_.s;
    const float sLength = norm(direction);
    if (sLength <= 0.0f) direction = identityBasis().s;
    else direction = scale(direction, 1.0f / sLength);
    Vec3 awayDelta = sub(calibrationAway_, basis_.g0);
    Vec3 orthogonalAway =
        sub(awayDelta, scale(direction, dot(awayDelta, direction)));
    const float awayLen = norm(orthogonalAway);
    if (awayLen >= 0.10f) {
      Vec3 dirAway = scale(orthogonalAway, 1.0f / awayLen);
      Vec3 orthogonalDelta =
          sub(delta, scale(direction, dot(delta, direction)));
      return std::max(0.0f, -dot(orthogonalDelta, dirAway));
    }
    const Vec3 orthogonalDelta =
        sub(delta, scale(direction, dot(delta, direction)));
    return norm(orthogonalDelta);
  }

  Vec3 direction = basis_.s;
  const float length = norm(direction);
  if (length <= 0.0f) direction = identityBasis().s;
  else direction = scale(direction, 1.0f / length);
  const Vec3 orthogonal = sub(delta, scale(direction, dot(delta, direction)));
  return norm(orthogonal);
}

void InputRouter::finishCalibrationPose(const Vec3& pose, bool defaulted,
                                        uint32_t nowMs) {
  switch (calibrationStep_) {
    case CalibrationStep::TiltRight:
      calibrationRight_ = pose;
      rightDefaulted_ = defaulted;
      calibrationStep_ = CalibrationStep::TiltLeft;
      break;

    case CalibrationStep::TiltLeft: {
      calibrationLeft_ = pose;
      leftDefaulted_ = defaulted;
      TiltBasis candidate = identityBasis();
      candidate.g0 = basis_.g0;
      if (!deriveSteer(candidate.g0, calibrationRight_, calibrationLeft_,
                       candidate)) {
        candidate = identityBasis();
        candidate.g0 = basis_.g0;
        calibrationRight_ =
            candidate.g0 + scale(candidate.s, kMazeDefaultReachG);
        calibrationLeft_ =
            sub(candidate.g0, scale(candidate.s, kMazeDefaultReachG));
        deriveSteer(candidate.g0, calibrationRight_, calibrationLeft_,
                    candidate);
      }
      basis_ = candidate;
      calibrationStep_ = CalibrationStep::DipAway;
      break;
    }

    case CalibrationStep::DipAway:
      calibrationAway_ = pose;
      calibrationStep_ = CalibrationStep::DipToward;
      break;

    case CalibrationStep::DipToward: {
      calibrationToward_ = pose;
      dipDefaulted_ = defaulted;
      TiltBasis candidate = basis_;
      if (!deriveDip(calibrationToward_, candidate)) {
        dipDefaulted_ = true;
        calibrationToward_ = sub(candidate.g0,
                                 scale(identityBasis().f, kDipReferenceG));
        candidate.f = identityBasis().f;
        candidate.dipScaleG = kDipReferenceG;
        candidate.calibrated = true;
      }
      setBasis(candidate);
      startMaze(nowMs);
      calibrationStep_ = CalibrationStep::Maze;
      calibrationSampleSkip_ = true;
      break;
    }

    case CalibrationStep::Idle:
    case CalibrationStep::Intro:
    case CalibrationStep::Still:
    case CalibrationStep::Maze:
    case CalibrationStep::Rotate:
    case CalibrationStep::Done:
      return;
  }

  poseDeflectionG_ = 0.0f;
  poseTooSmall_ = false;
  poseBestG_ = 0.0f;
  poseRetry_ = 0;
  resetPoseRing();
  poseStartedMs_ = nowMs;
  // The pose window does not start until the player begins moving, or until
  // the 8 s ceiling elapses; reset both flags on entry to a pose step.
  poseWindowRunning_ = false;
  poseWindowEntryMs_ = nowMs;
}

void InputRouter::finishCalibrationStill(const Vec3& pose, uint32_t nowMs,
                                          NeutralCaptureSource source) {
  captureNeutral(pose.x, pose.y, pose.z, nowMs, source);
  calibrationRestless_ = false;
  resetNeutralWindow();
  if (!calibrationFull_) {
    calibrationStep_ = CalibrationStep::Done;
    return;
  }
  TiltBasis working = identityBasis();
  working.g0 = basis_.g0;
  basis_ = working;
  calibrationStep_ = CalibrationStep::TiltRight;
  poseStartedMs_ = nowMs;
  poseWindowRunning_ = false;
  poseWindowEntryMs_ = nowMs;
  poseDeflectionG_ = 0.0f;
  poseTooSmall_ = false;
  poseBestG_ = 0.0f;
  poseRetry_ = 0;
  resetPoseRing();
}

void InputRouter::startMaze(uint32_t nowMs) {
  mazeReachRightG_ = basis_.steerScaleRightG;
  mazeReachLeftG_ = basis_.steerScaleLeftG;
  mazeBallCol_ = static_cast<int8_t>(tilt_.column() <= 4 ? 4 : 5);
  mazeBallRow_ = 10;
  mazeProgress_ = 0;
  for (int i = 0; i < kMazeCourseLength; ++i) {
    if (mazeCourse_[i].col == mazeBallCol_ && mazeCourse_[i].row == 10) {
      mazeProgress_ = static_cast<uint8_t>(i);
      break;
    }
  }
  mazeOnCourse_ = true;
  mazeWasOnCourse_ = true;
  mazeStrays_ = 0;
  mazeSolved_ = false;
  mazeStartedMs_ = nowMs;
  mazeLastProgressMs_ = nowMs;
  mazeGoalReachedMs_ = 0;
  mazeArmed_ = false;
  mazeArmedMs_ = 0;
  mazeSeedPending_ = true;
  mazeSeedStillStartMs_ = nowMs;
  mazeSeedStillMin_ = 1e9f;
  mazeSeedStillMax_ = -1e9f;
  previousColumn_ = mazeBallCol_;
}

void InputRouter::seedMazeDipOffset(float dipN, uint32_t nowMs) {
  // Once the maze arms the continuous tracker keeps the offset; stop seeding.
  if (mazeArmed_) {
    mazeSeedPending_ = false;
    return;
  }
  // Track the running span of dipN. A span beyond the still band means the hand
  // moved: restart the window (no ceiling, so a moving hand is never seeded).
  if (dipN < mazeSeedStillMin_) mazeSeedStillMin_ = dipN;
  if (dipN > mazeSeedStillMax_) mazeSeedStillMax_ = dipN;
  if (mazeSeedStillMax_ - mazeSeedStillMin_ >= kDipQuietBandG) {
    mazeSeedStillMin_ = dipN;
    mazeSeedStillMax_ = dipN;
    mazeSeedStillStartMs_ = nowMs;
    return;
  }
  if (nowMs - mazeSeedStillStartMs_ < kMazeSeedStillMs) {
    return;
  }
  // 500 ms of stillness: seed the offset to this resting dip when it would move
  // by at least the still band, then re-arm the window so a still hand in a new
  // pose re-seeds until the maze arms. Report it as [NEU] src=seed.
  if (std::fabs(dipN - dipOffsetG_) >= kDipQuietBandG) {
    dipOffsetG_ = dipN;
    reportedDipOffsetG_ = dipN;
    neutralCapturedAtMs_ = nowMs;
    lastCaptureSource_ = NeutralCaptureSource::Seed;
    lastCaptureWasManual_ = false;
    ++neutralCaptureCount_;
  }
  mazeSeedPending_ = false;
  mazeSeedStillMin_ = dipN;
  mazeSeedStillMax_ = dipN;
  mazeSeedStillStartMs_ = nowMs;
}

void InputRouter::finishMazeToRotate(uint32_t nowMs) {
  calibrationRotationCount_ = 0;
  calibrationRotationState_ = 0;
  calibrationStep_ = CalibrationStep::Rotate;
  rotateStartedMs_ = nowMs;
}

void InputRouter::updateMaze(int selectedColumn, float dipRaw,
                             uint32_t nowMs) {
  mazeBallCol_ = static_cast<int8_t>(selectedColumn);
  float rowPosition = 10.0f - dipRaw / basis_.dipScaleG * 9.5f;
  rowPosition = std::clamp(rowPosition, 0.0f,
                           static_cast<float>(kMazeRows - 1));
  int candidateRow = static_cast<int>(std::lround(rowPosition));
  if (candidateRow < 0) candidateRow = 0;
  if (candidateRow >= kMazeRows) candidateRow = kMazeRows - 1;
  const float rowDelta = rowPosition - static_cast<float>(mazeBallRow_);
  if (candidateRow != mazeBallRow_ &&
      std::fabs(rowDelta) >= 0.5f + kMazeRowHysteresis) {
    mazeBallRow_ = static_cast<int8_t>(candidateRow);
  }

  // Arm only once the ball has sat on a start cell for 500 ms. Before that,
  // the player is still recovering from the last box pose: no strays, no
  // give-up timer, no progress.
  const bool onStartCell =
      (mazeBallCol_ == 4 || mazeBallCol_ == 5) && mazeBallRow_ == 10;
  if (!mazeArmed_) {
    if (onStartCell) {
      if (mazeArmedMs_ == 0) mazeArmedMs_ = nowMs;
      if (nowMs - mazeArmedMs_ >= kMazeArmHoldMs) {
        mazeArmed_ = true;
      }
    } else {
      mazeArmedMs_ = 0;
    }
  }

  const bool onCourse = kMazeLayout[mazeBallRow_][mazeBallCol_] != '#';
  if (mazeArmed_) {
    if (mazeWasOnCourse_ && !onCourse && mazeStrays_ < 255u) ++mazeStrays_;
    if (!mazeOnCourse_ && !onCourse) {
      // still off-course: timer already started
    } else if (mazeOnCourse_ && !onCourse) {
      mazeOffCourseSinceMs_ = nowMs;
    }
  }
  mazeWasOnCourse_ = onCourse;
  mazeOnCourse_ = onCourse;

  if (mazeArmed_ && onCourse) {
    const int first = static_cast<int>(mazeProgress_) + 1;
    const int last = std::min(kMazeCourseLength - 1,
                              first + kMazeSkipAhead - 1);
    for (int index = first; index <= last; ++index) {
      if (mazeCourse_[index].col != mazeBallCol_ ||
          mazeCourse_[index].row != mazeBallRow_) {
        continue;
      }
      mazeProgress_ = static_cast<uint8_t>(index);
      mazeLastProgressMs_ = nowMs;
      if (index == kMazeCourseLength - 1) {
        mazeSolved_ = true;
        mazeGoalReachedMs_ = nowMs;
      }
      break;
    }
  }

  // The 90 s maze timeout always counts from step entry, regardless of
  // arming. Give-up and goal pause still require arming.
  if (nowMs - mazeStartedMs_ >= kMazeTimeoutMs) {
    finishMazeToRotate(nowMs);
  } else if (mazeArmed_ &&
      ((mazeSolved_ && nowMs - mazeGoalReachedMs_ >= kMazeGoalPauseMs) ||
       (!mazeSolved_ &&
        (nowMs - mazeLastProgressMs_ >= kMazeGiveUpMs)))) {
    finishMazeToRotate(nowMs);
  }
}

void InputRouter::collectNeutralSample(float xG, float yG, float zG,
                                       uint32_t nowMs) {
  if (haveNeutralSampleTime_ &&
      nowMs - lastNeutralSampleMs_ < kNeutralQuietSampleMs) {
    return;
  }
  haveNeutralSampleTime_ = true;
  lastNeutralSampleMs_ = nowMs;
  neutralWindowX_[neutralWindowIndex_] = xG;
  neutralWindowY_[neutralWindowIndex_] = yG;
  neutralWindowZ_[neutralWindowIndex_] = zG;
  neutralWindowIndex_ =
      (neutralWindowIndex_ + 1u) % kNeutralQuietSamples;
  if (neutralWindowCount_ < kNeutralQuietSamples) ++neutralWindowCount_;

  neutralWindowPpX_ = neutralWindowPeakToPeak(neutralWindowX_);
  neutralWindowPpYG_ = neutralWindowPeakToPeak(neutralWindowY_);
  neutralWindowPpZG_ = neutralWindowPeakToPeak(neutralWindowZ_);

}

bool InputRouter::trackDipNeutral(float dipN, bool bluePressed,
                                  bool sidePressed, Screen screen,
                                  uint32_t dtMs, uint32_t nowMs) {
  // The resting-dip offset is corrected continuously in play while the player
  // is not gesturing. Each gate below the screen check must hold or the offset
  // does not move; the old 1 s quiet-window gate is gone (it fired on only
  // ~8 % of rest windows and never kept up with the drift). The maze does NOT
  // track continuously: it centres on a one-shot entry seed instead, because a
  // continuous tracker would re-centre a held sub-engage dip and fight the
  // maze's fine dip-navigation (there the row IS the dip).
  if (!basis_.calibrated || screen != Screen::Playing ||
      bluePressed || sidePressed || reCalibrateRequested_ ||
      (blueTapCount_ != 0 &&
       nowMs - blueTapMs_[blueTapCount_ - 1] <= kReCalibrateTapWindowMs) ||
      dip_.softDropActive() || std::fabs(dipN - dipOffsetG_) >= kDipEngageG ||
      nowMs - lastDipActionMs_ < kDipTrackHoldOffMs) {
    return false;
  }

  const uint32_t limitedDtMs = dtMs > 40u ? 40u : dtMs;
  const float trackingFraction =
      static_cast<float>(limitedDtMs) / static_cast<float>(kDipTrackTauMs);
  dipOffsetG_ += (dipN - dipOffsetG_) * trackingFraction;

  if (std::fabs(dipOffsetG_ - reportedDipOffsetG_) >= kDipTrackReportG &&
      nowMs - lastDriftReportMs_ >= 1000u) {
    reportedDipOffsetG_ = dipOffsetG_;
    lastDriftReportMs_ = nowMs;
    neutralCapturedAtMs_ = nowMs;
    lastCaptureSource_ = NeutralCaptureSource::Drift;
    lastCaptureWasManual_ = false;
    ++neutralCaptureCount_;
  }
  return true;
}

bool InputRouter::tryManualReZero(bool bluePressed, bool sidePressed,
                                  float xG, float yG, float zG, Screen screen,
                                  uint32_t nowMs) {
  const Binding* binding = findBinding(screen, Gesture::SideHold);
  if (screen != Screen::Playing || profile_ != ControlProfile::TILT_DIP ||
      binding == nullptr || binding->action != ActionKind::None ||
      !sidePressed || bluePressed || !buttons_.sideHoldEligible() ||
      reZeroLatched_ || dip_.engaged() ||
      buttons_.sideHeldMs(nowMs) < kReZeroHoldMs) {
    return false;
  }

  captureNeutral(xG, yG, zG, nowMs, NeutralCaptureSource::Manual);
  reZeroLatched_ = true;
  return true;
}

void InputRouter::emit(Screen screen, Gesture gesture, ActionKind emitted,
                       uint32_t nowMs, ActionQueue& out) const {
  const Binding* binding = findBinding(screen, gesture);
  if (binding != nullptr) {
    out.push(resolveBindingAction(*binding, emitted, profile_, rotateCcw_),
             nowMs);
  }
}

void InputRouter::routeCalibrationBluePress(float xG, float yG, float zG,
                                            bool imuFresh, uint32_t nowMs,
                                            ActionQueue& out) {
  switch (calibrationStep_) {
    case CalibrationStep::Intro:
      resetNeutralWindow();
      neutralWindowPpX_ = 0.0f;
      neutralWindowPpYG_ = 0.0f;
      neutralWindowPpZG_ = 0.0f;
      calibrationRestless_ = false;
      calibrationStep_ = CalibrationStep::Still;
      stillStepStartedMs_ = nowMs;
      return;
    case CalibrationStep::Still: {
      if (!imuFresh) return;
      const float restMaxSquared = kNeutralRestMaxG * kNeutralRestMaxG;
      if (yG * yG + zG * zG > restMaxSquared) {
        calibrationRestless_ = true;
        return;
      }
      finishCalibrationStill(Vec3{xG, yG, zG}, nowMs,
                             NeutralCaptureSource::Manual);
      return;
    }
    case CalibrationStep::TiltRight:
    case CalibrationStep::TiltLeft:
    case CalibrationStep::DipAway:
    case CalibrationStep::DipToward: {
      if (!imuFresh) return;
      if (poseBestG_ >= kMinCalibrationDeflectionG ||
          calibrationStep_ == CalibrationStep::DipAway) {
        finishCalibrationPose(poseBestSample_, false, nowMs);
      } else {
        poseTooSmall_ = true;
      }
      return;
    }
    case CalibrationStep::Maze:
      finishMazeToRotate(nowMs);
      return;
    case CalibrationStep::Rotate:
      calibrationStep_ = CalibrationStep::Done;
      return;
    case CalibrationStep::Done:
      out.push(ActionKind::Confirm, nowMs);
      return;
    case CalibrationStep::Idle:
      return;
  }
}

void InputRouter::routeButtons(Screen screen, ActionQueue& source,
                               ActionQueue& out, float xG, float yG,
                               float zG, bool imuFresh) {
  GameAction action{};
  while (source.pop(action)) {
    Gesture gesture = Gesture::BluePress;
    if (buttonGestureFor(action.kind, gesture)) {
      bool opensCalibration = false;
      // ButtonGesture owns the ordinary 800 ms chord. Title's diagnostics
      // binding has its longer router-level threshold below, so the ordinary
      // chord action means nothing on that one screen.
      if (screen == Screen::Title && gesture == Gesture::Chord) {
        continue;
      }
      if (screen == Screen::Calibrating && gesture == Gesture::BluePress) {
        routeCalibrationBluePress(xG, yG, zG, imuFresh, action.tMs, out);
        continue;
      }
      const Binding* reCalibrate =
          findBinding(Screen::Playing, Gesture::BlueTaps);
      if (gesture == Gesture::BluePress && screen == Screen::Playing &&
           profile_ == ControlProfile::TILT_DIP && imuReady_ &&
           reCalibrate != nullptr) {
        if (blueTapCount_ == kReCalibrateTaps) {
          for (std::size_t i = 1; i < kReCalibrateTaps; ++i) {
            blueTapMs_[i - 1u] = blueTapMs_[i];
          }
          --blueTapCount_;
        }
        blueTapMs_[blueTapCount_++] = action.tMs;
        if (blueTapCount_ == kReCalibrateTaps &&
            action.tMs - blueTapMs_[0] <= kReCalibrateTapWindowMs) {
          const Binding* calibration =
              findBinding(Screen::Calibrating, Gesture::BluePress);
          if (reCalibrate->action == ActionKind::None ||
              calibration == nullptr) {
            reCalibrateRequested_ = true;
          } else {
            opensCalibration = true;
          }
          for (std::size_t i = 0; i < kReCalibrateTaps; ++i) {
            blueTapMs_[i] = 0;
          }
          blueTapCount_ = 0;
        }
      }
      emit(screen, gesture, action.kind, action.tMs, out);
      if (opensCalibration) {
        const Binding* reCalibrate =
            findBinding(Screen::Playing, Gesture::BlueTaps);
        out.push(resolveBindingAction(*reCalibrate, ActionKind::Calibrate,
                                      profile_, rotateCcw_), action.tMs);
      }
    }
  }
}

void InputRouter::routeDiagChord(Screen screen, uint32_t nowMs,
                                 ActionQueue& out) {
  const uint32_t heldMs = buttons_.chordHeldMs(nowMs);
  if (heldMs == 0) {
    diagChordTracking_ = false;
    diagChordEligible_ = false;
    diagChordLatched_ = false;
    return;
  }
  if (!diagChordTracking_) {
    diagChordTracking_ = true;
    diagChordEligible_ = screen == Screen::Title;
  }
  if (diagChordEligible_ && screen == Screen::Title &&
      !diagChordLatched_ && heldMs >= kDiagChordMs) {
    emit(screen, Gesture::Chord, ActionKind::Diag, nowMs, out);
    diagChordLatched_ = true;
  }
}

void InputRouter::routeDip(Screen screen, ActionQueue& source,
                           ActionQueue& out) {
  GameAction action{};
  while (source.pop(action)) {
    Gesture gesture = Gesture::DipAway;
    if (dipGestureFor(action.kind, gesture)) {
      emit(screen, gesture, action.kind, action.tMs, out);
      lastDipActionMs_ = action.tMs;
    }
  }
}

void InputRouter::update(bool bluePressed, bool sidePressed, float yG,
                         float zG, Screen screen, uint32_t nowMs,
                         ActionQueue& out) {
  update(bluePressed, sidePressed, 0.0f, yG, zG, true, screen, nowMs, out);
}

void InputRouter::update(bool bluePressed, bool sidePressed, float yG,
                         float zG, bool imuFresh, Screen screen,
                         uint32_t nowMs, ActionQueue& out) {
  update(bluePressed, sidePressed, 0.0f, yG, zG, imuFresh, screen, nowMs,
         out);
}

void InputRouter::update(bool bluePressed, bool sidePressed, float xG,
                         float yG, float zG, bool imuFresh, Screen screen,
                         uint32_t nowMs, ActionQueue& out) {
  rawXG_ = xG;
  rawYG_ = yG;
  rawZG_ = zG;
  ActionQueue buttonActions;
  buttons_.update(bluePressed, sidePressed, nowMs, buttonActions);
  const bool playing = screen == Screen::Playing;
  const bool calibrating = screen == Screen::Calibrating;
  if (calibrating && profile_ == ControlProfile::BUTTONS_ONLY) {
    if (!calibrationButtonsOnlyConfirmed_) {
      out.push(ActionKind::Confirm, nowMs);
      calibrationButtonsOnlyConfirmed_ = true;
    }
    return;
  }
  const CalibrationStep stepBeforeButtons = calibrationStep_;
  routeButtons(screen, buttonActions, out, xG, yG, zG, imuFresh);
  if (calibrating && stepBeforeButtons != calibrationStep_ &&
      (stepBeforeButtons == CalibrationStep::TiltRight ||
       stepBeforeButtons == CalibrationStep::TiltLeft ||
       stepBeforeButtons == CalibrationStep::DipAway ||
       stepBeforeButtons == CalibrationStep::DipToward)) {
    return;
  }
  if (calibrationSampleSkip_) {
    calibrationSampleSkip_ = false;
    return;
  }
  routeDiagChord(screen, nowMs, out);
  if (!sidePressed) reZeroLatched_ = false;

  const bool observing = screen == Screen::Diagnostics || calibrating;
  if ((!playing && !observing) ||
      (playing && profile_ == ControlProfile::BUTTONS_ONLY)) {
    if (imuTracking_) {
      resetImu();
    }
    return;
  }

  const bool continuingCalibration =
      imuTracking_ && imuScreen_ == Screen::Calibrating && playing;
  if (!imuTracking_ || (imuScreen_ != screen && !continuingCalibration)) {
    resetImu();
    imuTracking_ = true;
    imuScreen_ = screen;
    imuTrackingStartedMs_ = nowMs;
    if (calibrating && calibrationStep_ == CalibrationStep::Still) {
      stillStepStartedMs_ = nowMs;
    }
  } else if (continuingCalibration) {
    imuScreen_ = screen;
  }

  if (reCalibrateRequested_ && imuFresh) {
    const bool owed = dip_.softDropActive();
    captureNeutral(xG, yG, zG, nowMs, NeutralCaptureSource::Manual);
    ++reCalibrateCount_;
    if (owed) {
      emit(screen, Gesture::TiltForward, ActionKind::SoftDropOff, nowMs, out);
    }
    reCalibrateRequested_ = false;
  }

  if (!imuReady_) {
    const bool pressed = bluePressed || sidePressed;
    if (imuFresh && tryManualReZero(bluePressed, sidePressed, xG, yG, zG,
                                     screen, nowMs)) {
      return;
    }
    if (pressed) {
      // A loaded finger must not qualify as a still neutral window, even when
      // this poll carries a stale IMU sample.
      resetNeutralWindow();
    }
    if (!imuFresh) return;
    if (calibrating) {
      if (calibrationStep_ != CalibrationStep::Still) return;
      if (!pressed) {
        collectNeutralSample(xG, yG, zG, nowMs);
        if (neutralWindowIsStill()) {
          finishCalibrationStill(neutralWindowMean(), nowMs,
                                 NeutralCaptureSource::Still);
        } else if (nowMs - stillStepStartedMs_ >= kStillStepTimeoutMs &&
                   neutralWindowCount_ == kNeutralQuietSamples) {
          const float restMaxSquared = kNeutralRestMaxG * kNeutralRestMaxG;
          if (yG * yG + zG * zG <= restMaxSquared) {
            finishCalibrationStill(neutralWindowMean(), nowMs,
                                   NeutralCaptureSource::Timeout);
          }
        }
        if (calibrationStep_ == CalibrationStep::Still &&
            nowMs - stillStepStartedMs_ >= kStillStepForcedMs) {
          finishCalibrationStill(Vec3{xG, yG, zG}, nowMs,
                                 NeutralCaptureSource::Forced);
        }
      }
      return;
    }

    if (!pressed) {
      collectNeutralSample(xG, yG, zG, nowMs);
      if (!imuReady_ && neutralWindowIsStill()) {
        const Vec3 pose = neutralWindowMean();
        captureNeutral(pose.x, pose.y, pose.z, nowMs,
                       NeutralCaptureSource::Still);
      }
    }
    if (!imuReady_ && nowMs - imuTrackingStartedMs_ >= kNeutralTimeoutMs) {
      const float restMaxSquared = kNeutralRestMaxG * kNeutralRestMaxG;
      if (yG * yG + zG * zG <= restMaxSquared) {
        captureNeutral(xG, yG, zG, nowMs, NeutralCaptureSource::Timeout);
      } else if (nowMs - imuTrackingStartedMs_ >= kNeutralTimeoutHardMs) {
        captureNeutral(xG, yG, zG, nowMs, NeutralCaptureSource::Forced);
      }
    }
    return;
  }

  if (!imuFresh) return;

  if (calibrating && calibrationPoseStep()) {
    if (!bluePressed && !sidePressed) {
      const Vec3 sample{xG, yG, zG};
      const float deflection = calibrationPoseDeflection(sample);
      poseDeflectionRing_[poseDeflectionRingIndex_] =
          Vec3{deflection, 0.0f, 0.0f};
      poseSampleRing_[poseSampleRingIndex_] = sample;
      poseDeflectionRingIndex_ =
          (poseDeflectionRingIndex_ + 1u) % kPoseRingSamples;
      if (poseDeflectionRingCount_ < kPoseRingSamples)
        ++poseDeflectionRingCount_;
      poseSampleRingIndex_ =
          (poseSampleRingIndex_ + 1u) % kPoseRingSamples;
      if (poseSampleRingCount_ < kPoseRingSamples)
        ++poseSampleRingCount_;

      poseDeflectionG_ = deflection;
      updateBoxView(sample);

      // Start the 5 s pose window on the first motion, or on the 8 s ceiling
      // if the player never moves: the window does not begin on page entry.
      if (!poseWindowRunning_) {
        if (deflection >= kPoseStartG) {
          poseWindowRunning_ = true;
          poseStartedMs_ = nowMs;
        } else if (nowMs - poseWindowEntryMs_ >= kPoseWaitCeilingMs) {
          poseWindowRunning_ = true;
          poseStartedMs_ = nowMs;
        }
      }
      if (poseWindowRunning_ &&
          poseDeflectionRingCount_ >= kPoseRingSamples) {
        float heldG = poseDeflectionRing_[0].x;
        for (std::size_t i = 1; i < kPoseRingSamples; ++i) {
          if (poseDeflectionRing_[i].x < heldG)
            heldG = poseDeflectionRing_[i].x;
        }
        if (heldG > poseBestG_) {
          poseBestG_ = heldG;
          Vec3 total{};
          for (std::size_t i = 0; i < kPoseRingSamples; ++i) {
            total = total + poseSampleRing_[i];
          }
          poseBestSample_ =
              scale(total, 1.0f / static_cast<float>(kPoseRingSamples));
        }
      }
    }

    if (poseWindowRunning_ &&
        nowMs - poseStartedMs_ >= kPoseWindowMs) {
      if (calibrationStep() == CalibrationStep::DipAway) {
        if (poseSampleRingCount_ > 0) {
          finishCalibrationPose(poseBestSample_, false, nowMs);
        } else {
          Vec3 defaultPose =
              basis_.g0 + scale(identityBasis().f, kDipReferenceG);
          finishCalibrationPose(defaultPose, true, nowMs);
        }
      } else if (poseBestG_ >= kMinCalibrationDeflectionG) {
        finishCalibrationPose(poseBestSample_, false, nowMs);
      } else if (poseRetry_ < kPoseRetries) {
        ++poseRetry_;
        poseTooSmall_ = true;
        poseStartedMs_ = nowMs;
        poseWindowRunning_ = false;
        poseWindowEntryMs_ = nowMs;
        poseBestG_ = 0.0f;
      } else {
        Vec3 defaultPose = basis_.g0;
        switch (calibrationStep_) {
          case CalibrationStep::TiltRight:
            defaultPose =
                basis_.g0 + scale(identityBasis().s, kMazeDefaultReachG);
            break;
          case CalibrationStep::TiltLeft:
            defaultPose = sub(basis_.g0,
                              scale(identityBasis().s, kMazeDefaultReachG));
            break;
          case CalibrationStep::DipAway:
            defaultPose = basis_.g0 + scale(identityBasis().f, kDipReferenceG);
            break;
          case CalibrationStep::DipToward:
            defaultPose = sub(basis_.g0,
                              scale(identityBasis().f, kDipReferenceG));
            break;
          default:
            break;
        }
        finishCalibrationPose(defaultPose, true, nowMs);
      }
    }
    return;
  }

  if (calibrating && calibrationStep_ != CalibrationStep::Maze &&
       calibrationStep_ != CalibrationStep::Rotate) {
    return;
  }

  const uint32_t dtMs = nowMs - previousMs_;
  previousMs_ = nowMs;

  const Vec3 sample{xG, yG, zG};
  float steerN = normalisedSteer(basis_, sample, tiltGain_);
  float dipN = normalisedDip(basis_, sample);
  const bool mazeActive =
      calibrating && calibrationStep_ == CalibrationStep::Maze;
  if (playing &&
      trackDipNeutral(dipN, bluePressed, sidePressed, screen, dtMs, nowMs)) {
    // The learned steering coordinate and the neutral vector remain fixed;
    // only the one-dimensional dip offset follows a resting posture.
  }
  // The maze always enters with the offset zeroed and a resting drift larger
  // than the tracker's engage gate, so it cannot ratchet from cold; seed the
  // offset once from 500 ms of stillness (re-seeds until the maze arms).
  if (mazeActive) {
    seedMazeDipOffset(dipN, nowMs);
  }
  // The maze row follows the offset-corrected dip. dipN = projectDip *
  // effectiveDipFactor and dipOffsetG_ moves in dipN units, so the
  // offset-corrected projection is (dipN - dipOffsetG_) / effectiveDipFactor.
  float dipRaw = 0.0f;
  if (mazeActive) {
    const float dipFactor = effectiveDipFactor(basis_);
    dipRaw = dipFactor > 0.0f ? (dipN - dipOffsetG_) / dipFactor
                              : projectDip(basis_, sample);
  }
  const float yVirtual =
      neutralYG_ + steerN * static_cast<float>(kTiltSignLeftRight);
  const float zVirtual =
      neutralZG_ + (dipN - dipOffsetG_) * static_cast<float>(kDipSignAway);

  ActionQueue dipActions;
  dip_.update(zVirtual, dtMs, nowMs, dipActions);
  if (playing) {
    routeDip(screen, dipActions, out);
  } else if (calibrating && calibrationStep_ == CalibrationStep::Rotate) {
    if (nowMs - rotateStartedMs_ >= kRotateTimeoutMs) {
      calibrationStep_ = CalibrationStep::Done;
    } else {
      GameAction action{};
      while (dipActions.pop(action)) {
        if (action.kind != ActionKind::RotateCw) continue;
        if (calibrationRotationCount_ < 2u) ++calibrationRotationCount_;
        calibrationRotationState_ = calibrationRotationCount_;
        if (calibrationRotationCount_ >= 2u) {
          calibrationStep_ = CalibrationStep::Done;
        }
      }
    }
  }

  if (tryManualReZero(bluePressed, sidePressed, xG, yG, zG, screen,
                      nowMs)) {
    return;
  }

  int selectedColumn = 0;
  if (calibrating && calibrationStep_ == CalibrationStep::Maze) {
    // The maze must keep following the ball while the dip axis is held.
    selectedColumn = tilt_.update(yVirtual, dtMs, false);
  } else {
    // Owner, 2026-09-17: steering follows the tilt at all times; dips are
    // independent learned axes. The freeze guard against dip bleeding into
    // steering was needed before learning; with orthogonal learned axes a dip
    // no longer moves the steering projection, so the guard only removes
    // control. Pass false (same as the maze branch).
    selectedColumn = tilt_.update(yVirtual, dtMs, false);
  }
  if (calibrating && calibrationStep_ == CalibrationStep::Maze) {
    previousColumn_ = selectedColumn;
    updateMaze(selectedColumn, dipRaw, nowMs);
    return;
  }
  if (observing) {
    previousColumn_ = selectedColumn;
    return;
  }

  if (pieceKnown_) {
    const Shape& shape = shapeOf(piece_.id, piece_.state);
    int minOff = static_cast<int>(shape[0].col);
    int maxOff = minOff;
    for (int i = 1; i < 4; ++i) {
      const int off = static_cast<int>(shape[static_cast<size_t>(i)].col);
      if (off < minOff) minOff = off;
      if (off > maxOff) maxOff = off;
    }
    const int minCol = -minOff;
    const int maxCol = Board::kWidth - 1 - maxOff;
    int target = selectedColumn;
    if (selectedColumn == 0) target = minCol;
    if (selectedColumn == Board::kWidth - 1) target = maxCol;
    if (target < minCol) target = minCol;
    if (target > maxCol) target = maxCol;

    const bool pieceChanged =
        piece_.col != lastEmittedPiece_.col ||
        piece_.id != lastEmittedPiece_.id ||
        piece_.state != lastEmittedPiece_.state;
    if (target != piece_.col &&
        (!haveEmittedTarget_ || target != lastEmittedTarget_ || pieceChanged)) {
      const Gesture direction = target < piece_.col ? Gesture::TiltLeft
                                                    : Gesture::TiltRight;
      const Binding* binding = findBinding(screen, direction);
      if (binding != nullptr) {
        for (int column = static_cast<int>(piece_.col); column != target;
             column += target < column ? -1 : 1) {
          out.push(binding->action, nowMs);
        }
      }
      haveEmittedTarget_ = true;
      lastEmittedTarget_ = target;
      lastEmittedPiece_ = piece_;
    }
    previousColumn_ = selectedColumn;
    return;
  }

  const Gesture direction = selectedColumn < previousColumn_
                                ? Gesture::TiltLeft
                                : Gesture::TiltRight;
  const Binding* binding = findBinding(screen, direction);
  if (binding != nullptr) {
    for (int column = previousColumn_; column != selectedColumn;
         column += selectedColumn < column ? -1 : 1) {
      out.push(binding->action, nowMs);
    }
  }
  previousColumn_ = selectedColumn;
}

}  // namespace sf::input
