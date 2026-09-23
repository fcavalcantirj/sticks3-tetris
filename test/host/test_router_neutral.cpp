#include "framework.h"
#include "stackfall/input/router.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

using sf::Screen;
using sf::input::ActionKind;
using sf::input::ActionQueue;
using sf::input::CalibrationStep;
using sf::input::GameAction;
using sf::input::InputRouter;
using sf::input::TiltBasis;
using sf::input::Vec3;
using sf::input::kColHysteresis;
using sf::input::kGPerColumn;
using sf::input::kMazeGiveUpMs;
using sf::input::kMazeGoalPauseMs;
using sf::input::kNeutralQuietSampleMs;
using sf::input::kNeutralQuietSamples;
using sf::input::kNeutralTimeoutHardMs;
using sf::input::kNeutralTimeoutMs;
using sf::input::kPoseWaitCeilingMs;
using sf::input::kPoseWindowMs;
using sf::input::kMazeDefaultReachG;
using sf::input::kMazeSeedStillMs;
using sf::input::kMazeTimeoutMs;
using sf::input::kReZeroHoldMs;
using sf::input::kRotateTimeoutMs;
using sf::input::NeutralCaptureSource;
using sf::input::effectiveDipFactor;
using sf::input::identityBasis;
using sf::input::scale;
using sf::input::sub;

namespace {

constexpr float kPi = 3.14159265358979323846f;

float rawY(float correctedSignal) {
  return correctedSignal * static_cast<float>(kTiltSignLeftRight);
}

struct Capture {
  void update(InputRouter& router, bool blue, bool side, float yG, float zG,
              Screen screen, uint32_t nowMs) {
    router.update(blue, side, yG, zG, screen, nowMs, queue);
    GameAction action{};
    while (queue.pop(action)) actions.push_back(action);
  }

  int count(ActionKind kind) const {
    int result = 0;
    for (const GameAction& action : actions) {
      if (action.kind == kind) ++result;
    }
    return result;
  }

  void clear() { actions.clear(); }

  ActionQueue queue;
  std::vector<GameAction> actions;
};

uint32_t settleNeutral(InputRouter& router, Capture& capture,
                       uint32_t nowMs = 0, float yG = 0.0f,
                       float zG = 0.0f) {
  for (std::size_t i = 0; i < kNeutralQuietSamples; ++i) {
    capture.update(router, false, false, yG, zG, Screen::Playing,
                   nowMs + static_cast<uint32_t>(i) *
                               kNeutralQuietSampleMs);
  }
  ASSERT_TRUE(router.neutralReady());
  ASSERT_EQ(capture.actions.size(), 0u);
  return nowMs + static_cast<uint32_t>(kNeutralQuietSamples - 1u) *
                     kNeutralQuietSampleMs;
}

void updateNine(InputRouter& router, Capture& capture, bool blue, bool side,
                Vec3 sample, Screen screen, uint32_t nowMs);

uint32_t settleNeutralNine(InputRouter& router, Capture& capture, Vec3 sample,
                           uint32_t nowMs = 0) {
  for (std::size_t i = 0; i < kNeutralQuietSamples; ++i) {
    updateNine(router, capture, false, false, sample, Screen::Playing,
               nowMs + static_cast<uint32_t>(i) * kNeutralQuietSampleMs);
  }
  ASSERT_TRUE(router.neutralReady());
  ASSERT_EQ(capture.actions.size(), 0u);
  return nowMs + static_cast<uint32_t>(kNeutralQuietSamples - 1u) *
                     kNeutralQuietSampleMs;
}

TiltBasis driftBasis(Vec3 neutral) {
  const Vec3 rawSteer{-0.264f, -0.958f, 0.107f};
  const Vec3 rawDip{0.403f, -0.009f, 0.915f};
  TiltBasis basis = sf::input::identityBasis();
  basis.g0 = neutral;
  basis.s = sf::input::scale(rawSteer, 1.0f / sf::input::norm(rawSteer));
  basis.f = sf::input::scale(rawDip, 1.0f / sf::input::norm(rawDip));
  basis.steerScaleG = 0.243f;
  basis.steerScaleRightG = 0.243f;
  basis.steerScaleLeftG = 0.685f;
  basis.dipScaleG = 1.272f;
  basis.steerAsymmetryG = -0.442f;
  basis.calibrated = true;
  return basis;
}

void updateNine(InputRouter& router, Capture& capture, bool blue, bool side,
                Vec3 sample, Screen screen, uint32_t nowMs) {
  router.update(blue, side, sample.x, sample.y, sample.z, true, screen, nowMs,
                capture.queue);
  GameAction action{};
  while (capture.queue.pop(action)) capture.actions.push_back(action);
}

Vec3 rotateAround(Vec3 value, Vec3 axis, float radians) {
  const float axisNorm = sf::input::norm(axis);
  axis = sf::input::scale(axis, 1.0f / axisNorm);
  const float c = std::cos(radians);
  const float s = std::sin(radians);
  return sf::input::scale(value, c) +
         sf::input::scale(
             Vec3{axis.y * value.z - axis.z * value.y,
                  axis.z * value.x - axis.x * value.z,
                  axis.x * value.y - axis.y * value.x},
                s) +
         sf::input::scale(axis, sf::input::dot(axis, value) * (1.0f - c));
}

Vec3 pitchShift(Vec3 neutral, float degrees) {
  constexpr float kPi = 3.14159265358979323846f;
  const float pitch0 = std::atan2(neutral.x, neutral.z);
  const float radius = std::sqrt(neutral.x * neutral.x + neutral.z * neutral.z);
  const float pitch = pitch0 + degrees * kPi / 180.0f;
  return Vec3{radius * std::sin(pitch), neutral.y, radius * std::cos(pitch)};
}

struct CalibrationPose {
  Vec3 neutral;
  Vec3 right;
  Vec3 left;
  Vec3 toward;
  Vec3 away;
};

CalibrationPose reclinedPose() {
  constexpr float kPi = 3.14159265358979323846f;
  const float angle = 25.0f * kPi / 180.0f;
  CalibrationPose pose{};
  pose.neutral = Vec3{0.519f, 0.423f, 0.743f};
  pose.right = pose.neutral + Vec3{0.0f, -0.30f, 0.0f};
  pose.left = pose.neutral + Vec3{0.0f, 0.30f, 0.0f};
  TiltBasis basis = sf::input::identityBasis();
  ASSERT_TRUE(sf::input::deriveSteer(pose.neutral, pose.right, pose.left,
                                     basis));
  pose.toward = rotateAround(pose.neutral, basis.s, angle);
  pose.away = rotateAround(pose.neutral, basis.s, -angle);
  return pose;
}

uint32_t feedCalibrationWindow(InputRouter& router, Capture& capture,
                               Vec3 sample, uint32_t firstMs) {
  for (std::size_t i = 0; i < sf::input::kNeutralQuietSamples; ++i) {
    updateNine(router, capture, false, false, sample, Screen::Calibrating,
               firstMs + static_cast<uint32_t>(i) * kNeutralQuietSampleMs);
  }
  return firstMs + static_cast<uint32_t>(sf::input::kNeutralQuietSamples - 1u) *
                      kNeutralQuietSampleMs;
}

uint32_t feedCalibrationPose(InputRouter& router, Capture& capture,
                             Vec3 sample, uint32_t firstMs,
                             uint32_t durationMs = sf::input::kPoseWindowMs) {
  uint32_t nowMs = firstMs;
  for (uint32_t elapsed = 0; elapsed < durationMs; elapsed += 10) {
    updateNine(router, capture, false, false, sample, Screen::Calibrating,
               nowMs + elapsed);
  }
  nowMs += durationMs;
  updateNine(router, capture, false, false, sample, Screen::Calibrating,
             nowMs);
  return nowMs;
}

uint32_t enterMaze(InputRouter& router, Capture& capture,
                   const CalibrationPose& pose) {
  router.beginCalibration(true);
  updateNine(router, capture, false, false, pose.neutral, Screen::Calibrating,
             0);
  updateNine(router, capture, true, false, pose.neutral, Screen::Calibrating,
             10);
  uint32_t nowMs = feedCalibrationWindow(router, capture, pose.neutral, 35);
  ASSERT_EQ(router.calibrationStep(), CalibrationStep::TiltRight);
  nowMs = feedCalibrationPose(router, capture, pose.right, nowMs);
  ASSERT_EQ(router.calibrationStep(), CalibrationStep::TiltLeft);
  nowMs = feedCalibrationPose(router, capture, pose.left, nowMs);
  ASSERT_EQ(router.calibrationStep(), CalibrationStep::DipAway);
  nowMs = feedCalibrationPose(router, capture, pose.away, nowMs, 13000);
  ASSERT_EQ(router.calibrationStep(), CalibrationStep::DipToward);
  nowMs = feedCalibrationPose(router, capture, pose.toward, nowMs);
  ASSERT_EQ(router.calibrationStep(), CalibrationStep::Maze);
  return nowMs;
}

uint32_t enterFirstPose(InputRouter& router, Capture& capture, Vec3 neutral) {
  router.beginCalibration(true);
  updateNine(router, capture, false, false, neutral, Screen::Calibrating, 0);
  updateNine(router, capture, true, false, neutral, Screen::Calibrating, 10);
  const uint32_t nowMs = feedCalibrationWindow(router, capture, neutral, 35);
  ASSERT_EQ(router.calibrationStep(), CalibrationStep::TiltRight);
  return nowMs;
}

uint32_t feedMaze(InputRouter& router, Capture& capture, Vec3 sample,
                  uint32_t durationMs, uint32_t nowMs) {
  for (uint32_t elapsed = 0; elapsed < durationMs;
       elapsed += kNeutralQuietSampleMs) {
    nowMs += kNeutralQuietSampleMs;
    updateNine(router, capture, false, false, sample, Screen::Calibrating,
               nowMs);
  }
  return nowMs;
}

}  // namespace
SF_TEST(router_full_calibration_derives_and_activates) {
  InputRouter router;
  Capture capture;
  CalibrationPose pose{};
  pose.neutral = Vec3{-0.375f, 0.0f, 0.927f};
  const Vec3 rightRaw{-0.722f, -0.589f, 0.354f};
  const Vec3 leftRaw{-0.058f, 0.832f, 0.554f};
  pose.right = scale(rightRaw, 1.0f / norm(rightRaw));
  pose.left = scale(leftRaw, 1.0f / norm(leftRaw));
  pose.away = pose.neutral;
  pose.toward = rotateAround(pose.neutral, Vec3{1.0f, 0.0f, 0.0f},
                             35.0f * kPi / 180.0f);
  TiltBasis expected = identityBasis();
  ASSERT_TRUE(sf::input::deriveSteer(pose.neutral, pose.right, pose.left,
                                     expected));
  ASSERT_TRUE(sf::input::deriveDip(pose.toward, expected));
  uint32_t nowMs = enterMaze(router, capture, pose);
  ASSERT_NEAR(router.basis().s.x, expected.s.x, 1e-3f);
  ASSERT_NEAR(router.basis().s.y, expected.s.y, 1e-3f);
  ASSERT_NEAR(router.basis().s.z, expected.s.z, 1e-3f);
  ASSERT_NEAR(router.basis().f.x, expected.f.x, 1e-3f);
  ASSERT_NEAR(router.basis().f.y, expected.f.y, 1e-3f);
  ASSERT_NEAR(router.basis().f.z, expected.f.z, 1e-3f);

  const TiltBasis learned = router.basis();
  auto sampleAt = [&](int col, int row) {
    const float signal =
        (static_cast<float>(col) - 4.5f) * kGPerColumn;
    const float factor = signal >= 0.0f
                             ? sf::input::effectiveSteerFactorRight(learned)
                             : sf::input::effectiveSteerFactorLeft(learned);
    const float steer = factor <= 0.0f ? 0.0f : signal / factor;
    const float dip =
        (10.0f - static_cast<float>(row)) / 9.5f * learned.dipScaleG;
    return learned.g0 + scale(learned.s, steer) + scale(learned.f, dip);
  };
  auto holdCell = [&](int col, int row) {
    nowMs = feedMaze(router, capture, sampleAt(col, row), 300, nowMs);
  };

  for (int col = 4; col <= 9; ++col) holdCell(col, 10);
  for (int row = 10; row >= 3; --row) holdCell(9, row);
  for (int col = 9; col >= 0; --col) holdCell(col, 3);
  for (int row = 3; row <= 16; ++row) holdCell(0, row);
  for (int col = 0; col <= 5; ++col) holdCell(col, 16);
  for (int row = 16; row >= 13; --row) holdCell(5, row);
  holdCell(5, 13);
  nowMs = feedMaze(router, capture, sampleAt(5, 13),
                   kMazeGoalPauseMs + 100u, nowMs);

  ASSERT_TRUE(router.mazeSolved());
  ASSERT_EQ(router.mazeStrays(), 0u);
  ASSERT_EQ(router.mazeProgress(), 41u);
  ASSERT_EQ(router.calibrationStep(), CalibrationStep::Rotate);
  ASSERT_TRUE(router.basis().calibrated);

  const Vec3 away = pose.neutral + scale(learned.f, 0.50f);
  nowMs = feedMaze(router, capture, away, 500, nowMs);
  nowMs = feedMaze(router, capture, pose.neutral, 500, nowMs);
  nowMs = feedMaze(router, capture, away, 500, nowMs);
  ASSERT_EQ(router.rotationCount(), 2u);
  ASSERT_EQ(router.calibrationStep(), CalibrationStep::Done);
  capture.clear();
  updateNine(router, capture, true, false, pose.neutral, Screen::Calibrating,
             nowMs + 40);
  ASSERT_EQ(capture.count(ActionKind::Confirm), 1);

  capture.clear();
  nowMs += 80;
  for (int i = 0; i < 8; ++i) {
    nowMs += 40;
    updateNine(router, capture, false, false, pose.neutral,
               Screen::Playing, nowMs);
  }
  for (int i = 0; i < 12; ++i) {
    nowMs += 40;
    updateNine(router, capture, false, false,
               learned.g0 + scale(learned.s, learned.steerScaleRightG),
               Screen::Playing, nowMs);
  }
  ASSERT_EQ(router.selectedColumn(), 9);
  ASSERT_TRUE(capture.count(ActionKind::MoveRight) > 0);

  capture.clear();
  for (int i = 0; i < 12; ++i) {
    nowMs += 40;
    updateNine(router, capture, false, false,
               sub(learned.g0, scale(learned.f, learned.dipScaleG)),
               Screen::Playing, nowMs);
  }
  ASSERT_EQ(capture.count(ActionKind::SoftDropOn), 1);
}

SF_TEST(router_maze_ball_starts_in_the_centre_on_course) {
  InputRouter router;
  Capture capture;
  const CalibrationPose pose = reclinedPose();
  uint32_t nowMs = enterMaze(router, capture, pose);

  ASSERT_TRUE(router.mazeBallCol() == 4 || router.mazeBallCol() == 5);
  ASSERT_EQ(router.mazeBallRow(), 10);
  ASSERT_TRUE(router.mazeOnCourse());
  const uint8_t progress = router.mazeProgress();

  for (int i = 0; i < 120; ++i) {
    Vec3 sample = pitchShift(pose.neutral, (i & 1) == 0 ? 1.9f : -1.9f);
    sample.y = pose.neutral.y + ((i & 1) == 0 ? 0.02f : -0.02f);
    nowMs += kNeutralQuietSampleMs;
    updateNine(router, capture, false, false, sample, Screen::Calibrating,
               nowMs);
    ASSERT_EQ(router.mazeBallRow(), 10);
    ASSERT_TRUE(router.mazeOnCourse());
  }
  ASSERT_TRUE(router.mazeBallCol() == 4 || router.mazeBallCol() == 5);
  ASSERT_EQ(router.mazeBallRow(), 10);
  ASSERT_TRUE(router.mazeOnCourse());
  ASSERT_EQ(router.mazeStrays(), 0u);
  ASSERT_EQ(router.mazeProgress(), progress);

  // A maze entered with the hand still tilted toward should not count a stray
  // or start the give-up timer until the ball has sat on a start cell for 500 ms.
  InputRouter armedRouter;
  Capture armedCapture;
  const CalibrationPose armedPose = reclinedPose();
  uint32_t armedMs = enterMaze(armedRouter, armedCapture, armedPose);
  ASSERT_EQ(armedRouter.calibrationStep(), CalibrationStep::Maze);

  // Push the ball off the start cells with a MOVING dip (a hand held still
  // would be taken as the resting pose and seeded, centring the ball). The ball
  // moves off the start cells; until it returns and sits for 500 ms, no strays
  // are counted and the maze clock does not advance.
  const Vec3 hardToward =
      armedPose.neutral + scale(armedRouter.basis().f, armedRouter.basis().dipScaleG);
  const Vec3 halfToward =
      armedPose.neutral +
      scale(armedRouter.basis().f, 0.5f * armedRouter.basis().dipScaleG);
  for (uint32_t elapsed = 0; elapsed <= 1500u;
       elapsed += kNeutralQuietSampleMs) {
    armedMs += kNeutralQuietSampleMs;
    const Vec3& sample = (elapsed / kNeutralQuietSampleMs) % 2u == 0u
                             ? hardToward
                             : halfToward;
    updateNine(armedRouter, armedCapture, false, false, sample,
               Screen::Calibrating, armedMs);
  }
  ASSERT_EQ(armedRouter.mazeStrays(), 0u);

  // Level the stick so the ball returns to row 10. It must sit on the start cell
  // for 500 ms before the maze clock/strays/progress begin counting.
  for (uint32_t elapsed = 0; elapsed <= 600u;
       elapsed += kNeutralQuietSampleMs) {
    armedMs += kNeutralQuietSampleMs;
    updateNine(armedRouter, armedCapture, false, false, armedPose.neutral,
               Screen::Calibrating, armedMs);
  }
  ASSERT_TRUE(armedRouter.mazeBallCol() == 4 ||
              armedRouter.mazeBallCol() == 5);
  ASSERT_EQ(armedRouter.mazeBallRow(), 10);
  ASSERT_TRUE(armedRouter.mazeOnCourse());
  ASSERT_EQ(armedRouter.mazeStrays(), 0u);
  ASSERT_EQ(armedRouter.mazeProgress(), 0u);

  // Continue holding neutral: no strays while armed and stationary.
  for (int i = 0; i < 120; ++i) {
    Vec3 sample =
        pitchShift(armedPose.neutral, (i & 1) == 0 ? 1.9f : -1.9f);
    sample.y = armedPose.neutral.y + ((i & 1) == 0 ? 0.02f : -0.02f);
    armedMs += kNeutralQuietSampleMs;
    updateNine(armedRouter, armedCapture, false, false, sample,
               Screen::Calibrating, armedMs);
    ASSERT_EQ(armedRouter.mazeBallRow(), 10);
  }
  ASSERT_EQ(armedRouter.mazeStrays(), 0u);
  ASSERT_EQ(armedRouter.mazeProgress(), 0u);

  nowMs = feedMaze(router, capture, pitchShift(pose.neutral, 3.5f), 500,
                   nowMs);
  ASSERT_EQ(router.mazeBallRow(), 9);
}

SF_TEST(router_tutorial_still_waits_for_the_window) {
  const CalibrationPose pose = reclinedPose();

  InputRouter stillRouter;
  Capture stillCapture;
  stillRouter.beginCalibration(true);
  updateNine(stillRouter, stillCapture, false, false, pose.neutral,
             Screen::Calibrating, 0);
  updateNine(stillRouter, stillCapture, true, false, pose.neutral,
             Screen::Calibrating, 3000);
  const uint32_t capturesBefore = stillRouter.neutralCaptureCount();
  for (uint32_t elapsed = 25; elapsed <= 5000; elapsed += 25) {
    Vec3 moving = pose.neutral;
    moving.y += (elapsed / 25u) % 2u == 0u ? 0.06f : -0.06f;
    updateNine(stillRouter, stillCapture, false, false, moving,
               Screen::Calibrating, 3000u + elapsed);
  }
  ASSERT_EQ(stillRouter.calibrationStep(), CalibrationStep::Still);
  ASSERT_EQ(stillRouter.neutralCaptureCount(), capturesBefore);

  for (uint32_t elapsed = 5025; elapsed <= 5600; elapsed += 25) {
    updateNine(stillRouter, stillCapture, false, false, pose.neutral,
               Screen::Calibrating, 3000u + elapsed);
  }
   ASSERT_EQ(stillRouter.calibrationStep(), CalibrationStep::TiltRight);
  ASSERT_EQ(stillRouter.lastCaptureSource(), NeutralCaptureSource::Still);

  InputRouter timeoutRouter;
  Capture timeoutCapture;
  timeoutRouter.beginCalibration(true);
  updateNine(timeoutRouter, timeoutCapture, false, false, pose.neutral,
             Screen::Calibrating, 0);
  updateNine(timeoutRouter, timeoutCapture, true, false, pose.neutral,
             Screen::Calibrating, 3000);
  for (uint32_t elapsed = 25; elapsed <= 6500; elapsed += 25) {
    Vec3 moving = pose.neutral;
    moving.y += (elapsed / 25u) % 2u == 0u ? 0.06f : -0.06f;
    updateNine(timeoutRouter, timeoutCapture, false, false, moving,
               Screen::Calibrating, 3000u + elapsed);
  }
   ASSERT_EQ(timeoutRouter.calibrationStep(), CalibrationStep::TiltRight);
  ASSERT_EQ(timeoutRouter.lastCaptureSource(), NeutralCaptureSource::Timeout);
  ASSERT_NEAR(timeoutRouter.neutralYG(), pose.neutral.y, 0.01f);

  InputRouter forcedRouter;
  Capture forcedCapture;
  forcedRouter.beginCalibration(true);
  updateNine(forcedRouter, forcedCapture, false, false, pose.neutral,
             Screen::Calibrating, 0);
  updateNine(forcedRouter, forcedCapture, true, false, pose.neutral,
             Screen::Calibrating, 3000);
  for (uint32_t elapsed = 25; elapsed <= 10500; elapsed += 25) {
    const float movingY = (elapsed / 25u) % 2u == 0u ? 0.06f : -0.06f;
    updateNine(forcedRouter, forcedCapture, false, false,
               Vec3{0.0f, movingY, 1.4f}, Screen::Calibrating,
               3000u + elapsed);
  }
   ASSERT_EQ(forcedRouter.calibrationStep(), CalibrationStep::TiltRight);
  ASSERT_EQ(forcedRouter.lastCaptureSource(), NeutralCaptureSource::Forced);
}

SF_TEST(router_maze_straying_turns_red_and_counts) {
  InputRouter router;
  Capture capture;
  const CalibrationPose pose = reclinedPose();
  uint32_t nowMs = enterMaze(router, capture, pose);
  const uint8_t progress = router.mazeProgress();

  // Arm the maze: the ball must sit on a start cell for 500 ms.
  nowMs = feedMaze(router, capture, pose.neutral, 600, nowMs);

  Vec3 stray = pitchShift(pose.neutral, 10.0f);
  stray.y = pose.neutral.y - 0.20f;
  nowMs = feedMaze(router, capture, stray, 1000, nowMs);
  ASSERT_FALSE(router.mazeOnCourse());
  ASSERT_EQ(router.mazeStrays(), 1u);
  ASSERT_EQ(router.mazeProgress(), progress);

  Vec3 centredOffCourse = pitchShift(pose.neutral, 10.0f);
  centredOffCourse.y = pose.neutral.y;
  nowMs = feedMaze(router, capture, centredOffCourse, 1000, nowMs);
  nowMs = feedMaze(router, capture, pose.neutral, 500, nowMs);
  ASSERT_TRUE(router.mazeOnCourse());
  ASSERT_EQ(router.mazeStrays(), 1u);
  ASSERT_EQ(router.mazeProgress(), progress);
}

SF_TEST(router_maze_progress_needs_the_course) {
  InputRouter router;
  Capture capture;
  const CalibrationPose pose = reclinedPose();
  uint32_t nowMs = enterMaze(router, capture, pose);

  Vec3 goal = pitchShift(pose.neutral, -11.0f);
  goal.y = pose.neutral.y - 0.05f;
  nowMs = feedMaze(router, capture, goal, 1000, nowMs);
  ASSERT_TRUE(router.mazeOnCourse());
  ASSERT_FALSE(router.mazeSolved());
  ASSERT_EQ(router.mazeProgress(), 0u);
}

SF_TEST(router_maze_no_freeze_while_dipped) {
  InputRouter router;
  Capture capture;
  const CalibrationPose pose = reclinedPose();
  uint32_t nowMs = enterMaze(router, capture, pose);

  Vec3 rightAway = pitchShift(pose.neutral, 25.0f);
  rightAway.y = pose.neutral.y - 0.40f;
  nowMs = feedMaze(router, capture, rightAway, 1500, nowMs);
  ASSERT_EQ(router.mazeBallCol(), 9);
}

SF_TEST(router_maze_blue_skips) {
  InputRouter router;
  Capture capture;
  const CalibrationPose pose = reclinedPose();
  const uint32_t nowMs = enterMaze(router, capture, pose);

  updateNine(router, capture, true, false, pose.neutral, Screen::Calibrating,
             nowMs + 25);
  ASSERT_EQ(router.calibrationStep(), CalibrationStep::Rotate);
  ASSERT_NEAR(router.mazeReachRightG(), router.basis().steerScaleRightG,
              1e-3f);
  ASSERT_NEAR(router.mazeReachLeftG(), router.basis().steerScaleLeftG,
              1e-3f);
  ASSERT_EQ(router.rotationCount(), 0u);
}

SF_TEST(router_still_only_recalibration_keeps_the_basis) {
  InputRouter router;
  Capture capture;
  const CalibrationPose pose = reclinedPose();
  TiltBasis basis = sf::input::identityBasis();
  ASSERT_TRUE(sf::input::deriveSteer(pose.neutral, pose.right, pose.left,
                                     basis));
  ASSERT_TRUE(sf::input::deriveDip(pose.toward, basis));
  router.setBasis(basis);
  router.beginCalibration(false);
  feedCalibrationWindow(router, capture, pose.neutral, 0);

  ASSERT_EQ(router.calibrationStep(), CalibrationStep::Done);
  ASSERT_TRUE(router.basis().calibrated);
  ASSERT_NEAR(router.basis().s.x, basis.s.x, 1e-5f);
  ASSERT_NEAR(router.basis().s.y, basis.s.y, 1e-5f);
  ASSERT_NEAR(router.basis().s.z, basis.s.z, 1e-5f);
  ASSERT_NEAR(router.basis().f.x, basis.f.x, 1e-5f);
  ASSERT_NEAR(router.basis().f.y, basis.f.y, 1e-5f);
  ASSERT_NEAR(router.basis().f.z, basis.f.z, 1e-5f);
  ASSERT_NEAR(router.basis().steerScaleG, basis.steerScaleG, 1e-5f);
  ASSERT_NEAR(router.basis().dipScaleG, basis.dipScaleG, 1e-5f);
}
SF_TEST(router_blue_taps_run_the_full_tutorial) {
  InputRouter router;
  Capture capture;
  const CalibrationPose pose = reclinedPose();
  TiltBasis basis = sf::input::identityBasis();
  ASSERT_TRUE(sf::input::deriveSteer(pose.neutral, pose.right, pose.left,
                                     basis));
  ASSERT_TRUE(sf::input::deriveDip(pose.toward, basis));
  router.setBasis(basis);
  settleNeutral(router, capture);
  capture.clear();

  for (uint32_t tap = 0; tap < 4; ++tap) {
    const uint32_t pressedAtMs = 500u + tap * 120u;
    capture.update(router, true, false, 0.0f, 0.0f, Screen::Playing,
                   pressedAtMs);
    capture.update(router, false, false, 0.0f, 0.0f, Screen::Playing,
                   pressedAtMs + 40u);
  }

  ASSERT_EQ(capture.count(ActionKind::Calibrate), 1);
  ASSERT_TRUE(router.basis().calibrated);
  router.beginCalibration(true);
  ASSERT_EQ(router.calibrationStep(), CalibrationStep::Intro);
  ASSERT_TRUE(router.basis().calibrated);
}
SF_TEST(router_calibrating_intro_then_still_captures_and_confirms) {
  InputRouter router;
  Capture capture;
  router.beginCalibration(false);

  for (std::size_t i = 0; i < kNeutralQuietSamples; ++i) {
    capture.update(router, false, false, 0.0f, 0.0f, Screen::Calibrating,
                   static_cast<uint32_t>(i) * kNeutralQuietSampleMs);
  }
  capture.update(router, true, false, 0.0f, 0.0f, Screen::Calibrating, 600);

  ASSERT_EQ(capture.count(ActionKind::Confirm), 1);
  ASSERT_EQ(router.calibrationStep(), CalibrationStep::Done);
  ASSERT_EQ(router.lastCaptureSource(), NeutralCaptureSource::Still);
}
SF_TEST(router_calibrating_blue_now_only_when_resting) {
  InputRouter router;
  Capture capture;
  router.beginCalibration(false);

  capture.update(router, false, false, 0.0f, 0.0f, Screen::Calibrating, 0);
  capture.update(router, false, false, 0.90f, 0.90f, Screen::Calibrating,
                 35);
  capture.update(router, true, false, 0.90f, 0.90f, Screen::Calibrating,
                 50);
  ASSERT_EQ(router.calibrationStep(), CalibrationStep::Still);
  ASSERT_TRUE(router.restless());
  ASSERT_FALSE(router.neutralReady());

  capture.update(router, false, false, 0.0f, 0.0f, Screen::Calibrating, 75);
  capture.update(router, true, false, 0.0f, 0.0f, Screen::Calibrating, 100);
  ASSERT_EQ(router.calibrationStep(), CalibrationStep::Done);
  ASSERT_EQ(router.lastCaptureSource(), NeutralCaptureSource::Manual);
}
SF_TEST(router_buttons_only_skips_calibration) {
  InputRouter router;
  router.setProfile(sf::ControlProfile::BUTTONS_ONLY);
  Capture capture;
  router.beginCalibration(true);

  capture.update(router, false, false, 0.75f, 0.75f, Screen::Calibrating, 0);
  ASSERT_EQ(capture.count(ActionKind::Confirm), 1);
  ASSERT_FALSE(router.neutralReady());
  capture.clear();
  capture.update(router, false, false, 0.75f, 0.75f, Screen::Calibrating,
                 25);
  ASSERT_EQ(capture.actions.size(), 0u);
}

SF_TEST(router_deferred_neutral_uses_first_still_window) {
  InputRouter router;
  Capture capture;

  capture.update(router, false, false, rawY(0.10f), 0.10f,
                 Screen::Playing, 0);
  ASSERT_FALSE(router.neutralReady());
  for (uint32_t nowMs = 25; nowMs <= 475; nowMs += 25) {
    capture.update(router, false, false, rawY(0.0f), 0.0f,
                   Screen::Playing, nowMs);
  }
  ASSERT_FALSE(router.neutralReady());

  capture.update(router, false, false, rawY(0.0f), 0.0f,
                 Screen::Playing, 500);
  ASSERT_TRUE(router.neutralReady());
  ASSERT_TRUE(std::fabs(router.neutralYG()) < 0.001f);
  ASSERT_TRUE(std::fabs(router.neutralZG()) < 0.001f);
}

SF_TEST(router_deferred_neutral_waits_for_button_release) {
  InputRouter router;
  Capture capture;

  for (std::size_t i = 0; i < kNeutralQuietSamples; ++i) {
    capture.update(router, true, false, 0.0f, 0.0f, Screen::Playing,
                   static_cast<uint32_t>(i) * kNeutralQuietSampleMs);
  }
  ASSERT_FALSE(router.neutralReady());

  const uint32_t releasedAtMs =
      static_cast<uint32_t>(kNeutralQuietSamples) * kNeutralQuietSampleMs;
  for (std::size_t i = 0; i < kNeutralQuietSamples; ++i) {
    capture.update(router, false, false, 0.0f, 0.0f, Screen::Playing,
                   releasedAtMs +
                       static_cast<uint32_t>(i) * kNeutralQuietSampleMs);
  }
  ASSERT_TRUE(router.neutralReady());
  ASSERT_EQ(router.neutralCapturedAtMs(),
            releasedAtMs +
                static_cast<uint32_t>(kNeutralQuietSamples - 1u) *
                    kNeutralQuietSampleMs);
}

SF_TEST(router_button_press_on_stale_imu_resets_quiet_window) {
  InputRouter router;
  ActionQueue queue;

  for (std::size_t i = 0; i < 10; ++i) {
    router.update(false, false, 0.0f, 0.0f, true, Screen::Playing,
                  static_cast<uint32_t>(i) * kNeutralQuietSampleMs, queue);
  }
  router.update(true, false, 0.0f, 0.0f, false, Screen::Playing, 230,
                queue);
  router.update(false, false, 0.0f, 0.0f, false, Screen::Playing, 235,
                queue);

  for (std::size_t i = 0; i < 10; ++i) {
    router.update(false, false, 0.0f, 0.0f, true, Screen::Playing,
                  250u + static_cast<uint32_t>(i) * kNeutralQuietSampleMs,
                  queue);
  }
  ASSERT_FALSE(router.neutralReady());

  for (std::size_t i = 0; i < 10; ++i) {
    router.update(false, false, 0.0f, 0.0f, true, Screen::Playing,
                  500u + static_cast<uint32_t>(i) * kNeutralQuietSampleMs,
                  queue);
  }
  ASSERT_TRUE(router.neutralReady());
  ASSERT_EQ(router.neutralCapturedAtMs(), 725u);
}

SF_TEST(router_skewed_neutral_edges_are_asymmetric_until_manual_rezero) {
  InputRouter router;
  Capture capture;
  settleNeutral(router, capture, 0, rawY(0.10f));

  ASSERT_EQ(router.neutralColumn(), 6);
  ASSERT_NEAR(router.lowEdgeMagnitudeG(), 0.26f, 0.001f);
  ASSERT_NEAR(router.highEdgeMagnitudeG(), 0.46f, 0.001f);
  ASSERT_TRUE(std::fabs(router.lowEdgeMagnitudeG() -
                        router.highEdgeMagnitudeG()) > kColHysteresis);
  const uint32_t capturesBefore = router.neutralCaptureCount();

  for (uint32_t nowMs = 500; nowMs <= 1200; nowMs += 25) {
    capture.update(router, false, true, rawY(0.0f), 0.0f,
                   Screen::Playing, nowMs);
  }
  ASSERT_EQ(capture.count(ActionKind::HardDrop), 1);
  ASSERT_EQ(router.neutralCaptureCount(), capturesBefore + 1u);
  ASSERT_EQ(router.reZeroCount(), 1u);
  ASSERT_TRUE(router.lastCaptureWasManual());
  ASSERT_NEAR(router.lowEdgeMagnitudeG(), 0.36f, 0.001f);
  ASSERT_NEAR(router.highEdgeMagnitudeG(), 0.36f, 0.001f);
  ASSERT_TRUE(std::fabs(router.lowEdgeMagnitudeG() -
                        router.highEdgeMagnitudeG()) <= kColHysteresis);
}

SF_TEST(router_side_short_press_hard_drops_without_rezero) {
  InputRouter router;
  Capture capture;
  settleNeutral(router, capture);
  const uint32_t capturesBefore = router.neutralCaptureCount();

  capture.update(router, false, true, 0.0f, 0.0f, Screen::Playing, 500);
  capture.update(router, false, true, 0.0f, 0.0f, Screen::Playing, 1100);
  capture.update(router, false, false, 0.0f, 0.0f, Screen::Playing, 1150);

  ASSERT_EQ(capture.count(ActionKind::HardDrop), 1);
  ASSERT_EQ(router.neutralCaptureCount(), capturesBefore);
  ASSERT_EQ(router.reZeroCount(), 0u);
}

SF_TEST(router_side_hold_700ms_rezeros_once) {
  InputRouter router;
  Capture capture;
  settleNeutral(router, capture);
  const uint32_t capturesBefore = router.neutralCaptureCount();

  for (uint32_t nowMs = 500; nowMs <= 1500; nowMs += 25) {
    capture.update(router, false, true, rawY(0.08f), 0.0f,
                   Screen::Playing, nowMs);
  }

  ASSERT_EQ(capture.count(ActionKind::HardDrop), 1);
  ASSERT_EQ(router.neutralCaptureCount(), capturesBefore + 1u);
  ASSERT_EQ(router.reZeroCount(), 1u);
}

SF_TEST(router_side_hold_does_not_rezero_mid_dip) {
  InputRouter router;
  Capture capture;
  settleNeutral(router, capture);

  for (uint32_t nowMs = 500; nowMs <= 700; nowMs += 25) {
    capture.update(router, false, false, 0.0f, 0.80f,
                   Screen::Playing, nowMs);
  }
  ASSERT_TRUE(router.dipEngaged());

  for (uint32_t nowMs = 725; nowMs <= 1425; nowMs += 25) {
    capture.update(router, false, true, 0.0f, 0.80f,
                   Screen::Playing, nowMs);
  }
  ASSERT_EQ(router.reZeroCount(), 0u);

  for (uint32_t nowMs = 1450; nowMs <= 2000; nowMs += 25) {
    capture.update(router, false, true, 0.0f, 0.0f,
                   Screen::Playing, nowMs);
  }
  ASSERT_EQ(router.reZeroCount(), 1u);
}

SF_TEST(router_aborted_blue_first_chord_never_rezeros) {
  InputRouter router;
  Capture capture;
  const uint32_t readyMs = settleNeutral(router, capture);

  capture.update(router, true, false, 0.0f, 0.0f, Screen::Playing,
                 readyMs + 25);
  capture.update(router, true, true, 0.0f, 0.0f, Screen::Playing,
                 readyMs + 50);
  capture.update(router, true, true, 0.0f, 0.0f, Screen::Playing,
                 readyMs + 725);
  capture.update(router, false, true, 0.0f, 0.0f, Screen::Playing,
                 readyMs + 775);
  capture.update(router, false, true, 0.0f, 0.0f, Screen::Playing,
                 readyMs + 1000);

  ASSERT_EQ(capture.count(ActionKind::HardDrop), 0);
  ASSERT_EQ(capture.count(ActionKind::Pause), 0);
  ASSERT_EQ(router.reZeroCount(), 0u);
}

SF_TEST(router_manual_rezero_stays_latched_until_side_release) {
  InputRouter router;
  Capture capture;
  const uint32_t readyMs = settleNeutral(router, capture);

  for (uint32_t nowMs = readyMs + 25; nowMs <= readyMs + 1025;
       nowMs += 25) {
    capture.update(router, false, true, 0.0f, 0.0f,
                   Screen::Playing, nowMs);
  }
  ASSERT_EQ(router.reZeroCount(), 1u);

  capture.update(router, false, true, 0.0f, 0.0f, Screen::Paused,
                 readyMs + 1050);
  capture.clear();
  for (std::size_t i = 0; i < kNeutralQuietSamples; ++i) {
    capture.update(router, false, true, 0.0f, 0.0f, Screen::Playing,
                   readyMs + 1075 +
                       static_cast<uint32_t>(i) * kNeutralQuietSampleMs);
  }
  ASSERT_FALSE(router.neutralReady());
  ASSERT_EQ(capture.actions.size(), 0u);
  ASSERT_EQ(router.reZeroCount(), 1u);

  const uint32_t releasedAtMs = readyMs + 1575;
  capture.update(router, false, false, 0.0f, 0.0f, Screen::Playing,
                 releasedAtMs);
  for (std::size_t i = 1; i < kNeutralQuietSamples; ++i) {
    capture.update(router, false, false, 0.0f, 0.0f, Screen::Playing,
                   releasedAtMs +
                       static_cast<uint32_t>(i) * kNeutralQuietSampleMs);
  }
  ASSERT_TRUE(router.neutralReady());
  const uint32_t recapturedAtMs =
      releasedAtMs + static_cast<uint32_t>(kNeutralQuietSamples - 1u) *
                         kNeutralQuietSampleMs;

  for (uint32_t nowMs = recapturedAtMs + 25;
       nowMs <= recapturedAtMs + 725; nowMs += 25) {
    capture.update(router, false, true, 0.0f, 0.0f,
                   Screen::Playing, nowMs);
  }
  ASSERT_EQ(router.reZeroCount(), 2u);
}

SF_TEST(router_neutral_telemetry_preserves_outward_edge_ties) {
  InputRouter left;
  Capture leftCapture;
  settleNeutral(left, leftCapture, 0, rawY(-4.0f * kGPerColumn));
  ASSERT_EQ(left.neutralColumn(), 0);

  InputRouter right;
  Capture rightCapture;
  settleNeutral(right, rightCapture, 0, rawY(4.0f * kGPerColumn));
  ASSERT_EQ(right.neutralColumn(), 9);
}

SF_TEST(router_never_still_hand_times_out_and_tilt_works) {
  InputRouter router;
  Capture capture;

  for (uint32_t nowMs = 0; nowMs <= 5000 && !router.neutralReady();
       nowMs += kNeutralQuietSampleMs) {
    const float restlessG =
        ((nowMs / kNeutralQuietSampleMs) & 1u) == 0u ? 0.12f : -0.12f;
    capture.update(router, false, false, rawY(restlessG), restlessG,
                   Screen::Playing, nowMs);
  }

  ASSERT_TRUE(router.neutralReady());
  ASSERT_TRUE(router.neutralCapturedAtMs() <= kNeutralTimeoutMs);
  ASSERT_EQ(router.lastCaptureSource(), NeutralCaptureSource::Timeout);
  ASSERT_NEAR(router.neutralWindowPpYG(), 0.240f, 0.001f);
  ASSERT_NEAR(router.neutralWindowPpZG(), 0.240f, 0.001f);

  capture.clear();
  uint32_t nowMs = router.neutralCapturedAtMs();
  for (int i = 0; i < 10; ++i) {
    nowMs += kNeutralQuietSampleMs;
    capture.update(router, false, false, rawY(-0.80f), router.neutralZG(),
                   Screen::Playing, nowMs);
  }
  for (int i = 0; i < 20; ++i) {
    nowMs += kNeutralQuietSampleMs;
    capture.update(router, false, false, rawY(0.80f), router.neutralZG(),
                   Screen::Playing, nowMs);
  }

  ASSERT_TRUE(capture.count(ActionKind::MoveLeft) > 0);
  ASSERT_TRUE(capture.count(ActionKind::MoveRight) > 0);
}

SF_TEST(router_button_masher_cannot_defer_neutral_timeout) {
  InputRouter router;
  Capture capture;

  for (uint32_t nowMs = 0; nowMs <= kNeutralTimeoutMs;
       nowMs += kNeutralQuietSampleMs) {
    const bool bluePressed = nowMs % 100u == 0u;
    const float restlessG =
        ((nowMs / kNeutralQuietSampleMs) & 1u) == 0u ? 0.12f : -0.12f;
    capture.update(router, bluePressed, false, rawY(restlessG), restlessG,
                   Screen::Playing, nowMs);
  }

  ASSERT_TRUE(router.neutralReady());
  ASSERT_TRUE(router.neutralCapturedAtMs() <= kNeutralTimeoutMs);
  ASSERT_EQ(router.lastCaptureSource(), NeutralCaptureSource::Timeout);
}

SF_TEST(router_still_window_precedes_timeout_and_reports_source) {
  InputRouter router;
  Capture capture;

  for (std::size_t i = 0; i < kNeutralQuietSamples; ++i) {
    const float yG = (i & 1u) == 0u ? 0.015f : -0.015f;
    const float zG = (i & 1u) == 0u ? 0.010f : -0.010f;
    capture.update(router, false, false, rawY(yG), zG, Screen::Playing,
                   static_cast<uint32_t>(i) * kNeutralQuietSampleMs);
  }

  ASSERT_TRUE(router.neutralReady());
  ASSERT_TRUE(router.neutralCapturedAtMs() < kNeutralTimeoutMs);
  ASSERT_EQ(router.lastCaptureSource(), NeutralCaptureSource::Still);
  ASSERT_FALSE(router.lastCaptureWasManual());
  ASSERT_NEAR(router.neutralWindowPpYG(), 0.030f, 0.001f);
  ASSERT_NEAR(router.neutralWindowPpZG(), 0.020f, 0.001f);
  ASSERT_STR_EQ(
      sf::input::neutralCaptureSourceName(NeutralCaptureSource::Still),
      "still");
  ASSERT_STR_EQ(
      sf::input::neutralCaptureSourceName(NeutralCaptureSource::Timeout),
      "timeout");
  ASSERT_STR_EQ(
      sf::input::neutralCaptureSourceName(NeutralCaptureSource::Forced),
      "forced");
  ASSERT_STR_EQ(
      sf::input::neutralCaptureSourceName(NeutralCaptureSource::Manual),
      "manual");
}

SF_TEST(router_manual_rezero_is_reachable_before_neutral_ready) {
  InputRouter heldRouter;
  Capture heldCapture;
  heldCapture.update(heldRouter, false, false, rawY(0.12f), 0.12f,
                     Screen::Playing, 0);
  ASSERT_FALSE(heldRouter.neutralReady());

  for (uint32_t nowMs = kNeutralQuietSampleMs;
       nowMs <= kNeutralQuietSampleMs + kReZeroHoldMs;
       nowMs += kNeutralQuietSampleMs) {
    heldCapture.update(heldRouter, false, true, rawY(0.08f), 0.0f,
                       Screen::Playing, nowMs);
  }

  ASSERT_TRUE(heldRouter.neutralReady());
  ASSERT_TRUE(heldRouter.lastCaptureWasManual());
  ASSERT_EQ(heldRouter.lastCaptureSource(), NeutralCaptureSource::Manual);
  ASSERT_EQ(heldRouter.reZeroCount(), 1u);
  ASSERT_EQ(heldCapture.count(ActionKind::HardDrop), 1);

  InputRouter briefRouter;
  Capture briefCapture;
  briefCapture.update(briefRouter, false, false, 0.0f, 0.0f,
                      Screen::Playing, 0);
  briefCapture.update(briefRouter, false, true, 0.0f, 0.0f,
                      Screen::Playing, kNeutralQuietSampleMs);
  briefCapture.update(briefRouter, false, true, 0.0f, 0.0f,
                      Screen::Playing, kReZeroHoldMs);
  briefCapture.update(briefRouter, false, false, 0.0f, 0.0f,
                      Screen::Playing,
                      kReZeroHoldMs + kNeutralQuietSampleMs);

  ASSERT_FALSE(briefRouter.neutralReady());
  ASSERT_EQ(briefRouter.reZeroCount(), 0u);
  ASSERT_EQ(briefCapture.count(ActionKind::HardDrop), 1);
}

SF_TEST(router_timeout_capture_in_motion_does_not_lock_steering) {
  InputRouter router;
  Capture capture;

  for (uint32_t nowMs = 0; nowMs <= 1275;
       nowMs += kNeutralQuietSampleMs) {
    const bool first =
        ((nowMs / kNeutralQuietSampleMs) & 1u) == 0u;
    capture.update(router, false, false, first ? 0.494f : -0.40f,
                   first ? 1.211f : 1.15f, Screen::Playing, nowMs);
  }
  ASSERT_FALSE(router.neutralReady());

  capture.update(router, false, false, 0.0f, 0.0f, Screen::Playing, 1300);
  ASSERT_TRUE(router.neutralReady());
  ASSERT_EQ(router.lastCaptureSource(), NeutralCaptureSource::Timeout);
  ASSERT_EQ(router.neutralCapturedAtMs(), 1300u);

  for (uint32_t nowMs = 1325; nowMs <= 1625;
       nowMs += kNeutralQuietSampleMs) {
    capture.update(router, false, false, rawY(-0.30f), 0.0f,
                   Screen::Playing, nowMs);
  }
  for (uint32_t nowMs = 1650; nowMs <= 1950;
       nowMs += kNeutralQuietSampleMs) {
    capture.update(router, false, false, rawY(0.30f), 0.0f,
                   Screen::Playing, nowMs);
  }

  ASSERT_TRUE(capture.count(ActionKind::MoveLeft) > 0);
  ASSERT_TRUE(capture.count(ActionKind::MoveRight) > 0);
  ASSERT_EQ(capture.count(ActionKind::SoftDropOn), 0);
}

SF_TEST(router_forced_capture_then_blue_taps_open_calibration) {
  InputRouter router;
  Capture capture;

  for (uint32_t nowMs = 0; nowMs <= 3500;
       nowMs += kNeutralQuietSampleMs) {
    const bool first =
        ((nowMs / kNeutralQuietSampleMs) & 1u) == 0u;
    capture.update(router, false, false, first ? 0.494f : -0.40f,
                   first ? 1.211f : 1.15f, Screen::Playing, nowMs);
    if (nowMs < kNeutralTimeoutHardMs) {
      ASSERT_FALSE(router.neutralReady());
    } else if (nowMs == kNeutralTimeoutHardMs) {
      ASSERT_TRUE(router.neutralReady());
      ASSERT_EQ(router.neutralCapturedAtMs(), kNeutralTimeoutHardMs);
      ASSERT_EQ(router.lastCaptureSource(), NeutralCaptureSource::Forced);
    }
  }

  ASSERT_TRUE(router.neutralReady());
  ASSERT_EQ(router.lastCaptureSource(), NeutralCaptureSource::Forced);
  capture.clear();

  constexpr uint32_t kTapsStartMs = 3520;
  for (uint32_t nowMs = kTapsStartMs; nowMs <= kTapsStartMs + 400;
       nowMs += 20) {
    const uint32_t phase = (nowMs - kTapsStartMs) % 120u;
    capture.update(router, phase < 40u, false, 0.0f, 0.0f,
                   Screen::Playing, nowMs);
  }

  ASSERT_EQ(router.reCalibrateCount(), 0u);
  ASSERT_EQ(capture.count(ActionKind::RotateCw), 4);
  ASSERT_EQ(capture.count(ActionKind::Calibrate), 1);
  ASSERT_EQ(capture.actions.back().kind, ActionKind::Calibrate);
  ASSERT_EQ(capture.count(ActionKind::Hold), 0);
  ASSERT_EQ(capture.count(ActionKind::Pause), 0);
  ASSERT_EQ(router.lastCaptureSource(), NeutralCaptureSource::Forced);
}

SF_TEST(router_blue_taps_emit_calibrate_even_mid_dip) {
  InputRouter router;
  Capture capture;
  settleNeutral(router, capture);

  for (uint32_t nowMs = 500; nowMs <= 700; nowMs += 25) {
    capture.update(router, false, false, 0.0f, -0.78f,
                   Screen::Playing, nowMs);
  }
  ASSERT_EQ(capture.count(ActionKind::SoftDropOn), 1);
  ASSERT_TRUE(router.dipEngaged());
  const uint32_t capturesBefore = router.neutralCaptureCount();

  constexpr uint32_t kTapsStartMs = 720;
  for (uint32_t nowMs = kTapsStartMs; nowMs <= kTapsStartMs + 400;
       nowMs += 20) {
    const uint32_t phase = (nowMs - kTapsStartMs) % 120u;
    capture.update(router, phase < 40u, false, 0.0f, -0.78f,
                   Screen::Playing, nowMs);
  }

  ASSERT_EQ(router.neutralCaptureCount(), capturesBefore);
  ASSERT_EQ(capture.count(ActionKind::SoftDropOff), 0);
  ASSERT_EQ(capture.count(ActionKind::RotateCw), 4);
  ASSERT_EQ(capture.count(ActionKind::Calibrate), 1);
  ASSERT_EQ(capture.actions.back().kind, ActionKind::Calibrate);
  ASSERT_EQ(capture.count(ActionKind::Hold), 0);
  ASSERT_EQ(router.reCalibrateCount(), 0u);
}

SF_TEST(router_three_taps_or_slow_taps_do_not_recalibrate) {
  InputRouter threeTapRouter;
  Capture threeTapCapture;
  settleNeutral(threeTapRouter, threeTapCapture);

  for (uint32_t tap = 0; tap < 3; ++tap) {
    const uint32_t pressedAtMs = 500u + tap * 100u;
    threeTapCapture.update(threeTapRouter, true, false, 0.0f, 0.0f,
                           Screen::Playing, pressedAtMs);
    threeTapCapture.update(threeTapRouter, false, false, 0.0f, 0.0f,
                           Screen::Playing, pressedAtMs + 40u);
  }

  ASSERT_EQ(threeTapRouter.reCalibrateCount(), 0u);
  ASSERT_EQ(threeTapCapture.count(ActionKind::RotateCw), 3);

  InputRouter slowTapRouter;
  Capture slowTapCapture;
  settleNeutral(slowTapRouter, slowTapCapture);

  for (uint32_t tap = 0; tap < 4; ++tap) {
    const uint32_t pressedAtMs = 500u + tap * 400u;
    slowTapCapture.update(slowTapRouter, true, false, 0.0f, 0.0f,
                          Screen::Playing, pressedAtMs);
    slowTapCapture.update(slowTapRouter, false, false, 0.0f, 0.0f,
                          Screen::Playing, pressedAtMs + 40u);
  }

  ASSERT_EQ(slowTapRouter.reCalibrateCount(), 0u);
  ASSERT_EQ(slowTapCapture.count(ActionKind::RotateCw), 4);

  InputRouter masherRouter;
  Capture masherCapture;
  for (uint32_t nowMs = 0; nowMs <= kNeutralTimeoutMs;
       nowMs += kNeutralQuietSampleMs) {
    const bool bluePressed =
        nowMs <= 300u && nowMs % 100u == 0u;
    const float restlessG =
        ((nowMs / kNeutralQuietSampleMs) & 1u) == 0u ? 0.12f : -0.12f;
    masherCapture.update(masherRouter, bluePressed, false, rawY(restlessG),
                         restlessG, Screen::Playing, nowMs);
  }

  ASSERT_TRUE(masherRouter.neutralReady());
  ASSERT_EQ(masherRouter.reCalibrateCount(), 0u);
  ASSERT_EQ(masherRouter.lastCaptureSource(),
            NeutralCaptureSource::Timeout);
}

SF_TEST(router_dip_neutral_tracks_a_resting_offset) {
  InputRouter router;
  Capture capture;
  const Vec3 neutral{-0.456f, -0.023f, 0.899f};
  const TiltBasis basis = driftBasis(neutral);
  uint32_t nowMs = settleNeutralNine(router, capture, neutral);
  router.setBasis(basis);
  capture.clear();

  const Vec3 posture = pitchShift(neutral, 12.0f);
  for (uint32_t elapsed = kNeutralQuietSampleMs; elapsed <= 8000;
       elapsed += kNeutralQuietSampleMs) {
    nowMs += kNeutralQuietSampleMs;
    updateNine(router, capture, false, false, posture, Screen::Playing, nowMs);
  }

  ASSERT_TRUE(std::fabs(router.dipSignalG()) < 0.05f);
  ASSERT_FALSE(router.dipEngaged());
  ASSERT_EQ(router.lastCaptureSource(), NeutralCaptureSource::Drift);
  ASSERT_NEAR(router.basis().g0.x, neutral.x, 1e-6f);
  ASSERT_NEAR(router.basis().g0.y, neutral.y, 1e-6f);
  ASSERT_NEAR(router.basis().g0.z, neutral.z, 1e-6f);

  capture.clear();
  for (int i = 0; i < 12; ++i) {
    nowMs += 40;
    Vec3 steering = posture;
    steering.y += rawY(0.30f);
    updateNine(router, capture, false, false, steering, Screen::Playing, nowMs);
  }
  ASSERT_TRUE(capture.count(ActionKind::MoveRight) > 0);
}

SF_TEST(router_dip_neutral_holds_during_gestures) {
  InputRouter router;
  Capture capture;
  const Vec3 neutral{-0.456f, -0.023f, 0.899f};
  const TiltBasis basis = driftBasis(neutral);
  uint32_t nowMs = settleNeutralNine(router, capture, neutral);
  router.setBasis(basis);
  capture.clear();

  const float awayDegrees = std::asin(0.60f) * 180.0f / 3.14159265358979323846f;
  for (uint32_t elapsed = kNeutralQuietSampleMs; elapsed <= 250;
       elapsed += kNeutralQuietSampleMs) {
    nowMs += kNeutralQuietSampleMs;
    updateNine(router, capture, false, false,
               pitchShift(neutral, awayDegrees * elapsed / 250.0f),
               Screen::Playing, nowMs);
  }
  ASSERT_EQ(capture.count(ActionKind::RotateCw), 1);

  for (uint32_t elapsed = 0; elapsed < 500; elapsed += 25) {
    nowMs += kNeutralQuietSampleMs;
    updateNine(router, capture, false, false, neutral, Screen::Playing, nowMs);
  }

  const uint32_t capturesBefore = router.neutralCaptureCount();
  const Vec3 toward = pitchShift(neutral, -30.0f);
  for (uint32_t elapsed = kNeutralQuietSampleMs; elapsed <= 2000;
       elapsed += kNeutralQuietSampleMs) {
    nowMs += kNeutralQuietSampleMs;
    updateNine(router, capture, false, false, toward, Screen::Playing, nowMs);
  }
  ASSERT_EQ(router.neutralCaptureCount(), capturesBefore);
  ASSERT_TRUE(router.softDropActive());

  for (uint32_t elapsed = 0; elapsed < 700; elapsed += 25) {
    nowMs += kNeutralQuietSampleMs;
    updateNine(router, capture, false, false, neutral, Screen::Playing, nowMs);
  }
  ASSERT_EQ(capture.count(ActionKind::SoftDropOff), 1);

  const Vec3 restingOffset = pitchShift(neutral, 12.0f);
  for (uint32_t elapsed = 0; elapsed < 8000; elapsed += 25) {
    nowMs += kNeutralQuietSampleMs;
    updateNine(router, capture, false, false, restingOffset, Screen::Playing,
               nowMs);
  }
  ASSERT_EQ(router.lastCaptureSource(), NeutralCaptureSource::Drift);
}

SF_TEST(router_rotate_step_times_out_and_blue_skips) {
  const CalibrationPose pose = reclinedPose();

  InputRouter timeoutRouter;
  Capture timeoutCapture;
  uint32_t nowMs = enterMaze(timeoutRouter, timeoutCapture, pose);
  nowMs = feedMaze(timeoutRouter, timeoutCapture, pose.neutral,
                   kMazeGiveUpMs + 25u, nowMs);
  ASSERT_EQ(timeoutRouter.calibrationStep(), CalibrationStep::Rotate);
  const bool calibratedBefore = timeoutRouter.basis().calibrated;
  nowMs = feedMaze(timeoutRouter, timeoutCapture, pose.neutral,
                   kRotateTimeoutMs + 25u, nowMs);
  ASSERT_EQ(timeoutRouter.calibrationStep(), CalibrationStep::Done);
  ASSERT_EQ(timeoutRouter.basis().calibrated, calibratedBefore);
  ASSERT_EQ(timeoutRouter.rotationCount(), 0u);

  InputRouter blueRouter;
  Capture blueCapture;
  nowMs = enterMaze(blueRouter, blueCapture, pose);
  updateNine(blueRouter, blueCapture, true, false, pose.neutral,
             Screen::Calibrating, nowMs + 25);
  ASSERT_EQ(blueRouter.calibrationStep(), CalibrationStep::Rotate);
  ASSERT_EQ(blueRouter.rotationCount(), 0u);
}

SF_TEST(router_maze_never_blocks) {
  InputRouter router;
  Capture capture;
  const CalibrationPose pose = reclinedPose();
  uint32_t nowMs = enterFirstPose(router, capture, pose.neutral);

  int feeds = 0;
  while (router.calibrationStep() != CalibrationStep::Maze && feeds < 10) {
    nowMs = feedCalibrationPose(router, capture, pose.neutral, nowMs, 13000);
    ++feeds;
  }
  ASSERT_EQ(router.calibrationStep(), CalibrationStep::Maze);

  nowMs = feedMaze(router, capture, pose.neutral, kMazeGiveUpMs + 25u,
                   nowMs);
  ASSERT_EQ(router.calibrationStep(), CalibrationStep::Rotate);
  nowMs = feedMaze(router, capture, pose.neutral, kRotateTimeoutMs + 25u,
                   nowMs);
  ASSERT_EQ(router.calibrationStep(), CalibrationStep::Done);

  // A maze that never arms: the ball stays off the start cell, so mazeArmed_
  // never becomes true and no give-up timer runs, but the 90 s timeout still
  // counts from step entry and ends the maze by itself.
  InputRouter armedRouter;
  Capture armedCapture;
  const CalibrationPose armedPose = reclinedPose();
  uint32_t armedMs = enterMaze(armedRouter, armedCapture, armedPose);
  ASSERT_EQ(armedRouter.calibrationStep(), CalibrationStep::Maze);

  // Keep the ball off the start cells with a MOVING toward dip so it never
  // returns to arm and the entry seed never fires (a still hand would seed and
  // centre the ball). The 90 s maze timeout fires from step entry regardless.
  const Vec3 hardToward =
      armedPose.neutral +
      scale(armedRouter.basis().f, armedRouter.basis().dipScaleG);
  const Vec3 halfToward =
      armedPose.neutral +
      scale(armedRouter.basis().f, 0.5f * armedRouter.basis().dipScaleG);
  nowMs = armedMs;
  bool armed = false;
  for (uint32_t elapsed = 0; elapsed <= kMazeTimeoutMs + 1000u;
       elapsed += kNeutralQuietSampleMs) {
    nowMs += kNeutralQuietSampleMs;
    const Vec3& sample = (elapsed / kNeutralQuietSampleMs) % 2u == 0u
                             ? hardToward
                             : halfToward;
    updateNine(armedRouter, armedCapture, false, false, sample,
               Screen::Calibrating, nowMs);
    if (armedRouter.calibrationStep() == CalibrationStep::Rotate) {
      armed = true;
      break;
    }
  }
  ASSERT_TRUE(armed);
  ASSERT_FALSE(armedRouter.mazeArmed());
  ASSERT_TRUE(nowMs - armedMs <= kMazeTimeoutMs + 1000u);
}

SF_TEST(router_maze_rows_follow_the_learned_dip) {
  InputRouter router;
  Capture capture;
  CalibrationPose pose{};
  pose.neutral = Vec3{-0.375f, 0.0f, 0.927f};
  pose.right = pose.neutral + scale(identityBasis().s, 0.30f);
  pose.left = sub(pose.neutral, scale(identityBasis().s, 0.30f));
  pose.away = pose.neutral + scale(identityBasis().f, 0.30f);
  pose.toward = sub(pose.neutral, scale(identityBasis().f, 0.30f));
  TiltBasis learned = sf::input::identityBasis();
  const Vec3 rightRaw{-0.722f, -0.589f, 0.354f};
  const Vec3 leftRaw{-0.058f, 0.832f, 0.554f};
  const Vec3 right = sf::input::scale(rightRaw, 1.0f / norm(rightRaw));
  const Vec3 left = sf::input::scale(leftRaw, 1.0f / norm(leftRaw));
  const Vec3 toward = rotateAround(
      pose.neutral, Vec3{0.0f, 1.0f, 0.0f}, -25.0f * 3.14159265358979323846f /
                                               180.0f);
  ASSERT_TRUE(sf::input::deriveSteer(pose.neutral, right, left, learned));
  ASSERT_TRUE(sf::input::deriveDip(toward, learned));
  uint32_t nowMs = enterMaze(router, capture, pose);
  router.setBasis(learned);

  const Vec3 dipSample = sf::input::sub(
      pose.neutral, sf::input::scale(learned.f, 0.5f * learned.dipScaleG));
  nowMs = feedMaze(router, capture, dipSample, 300, nowMs);
  ASSERT_TRUE(router.mazeBallRow() >= 14 && router.mazeBallRow() <= 16);

  nowMs = feedMaze(router, capture, pose.neutral, 300, nowMs);
  const Vec3 sideways = pose.neutral + sf::input::scale(learned.s, 0.30f);
  feedMaze(router, capture, sideways, 300, nowMs);
  ASSERT_EQ(router.mazeBallRow(), 10);
}

SF_TEST(router_box_view_follows_any_grip) {
  const Vec3 neutral{-0.495f, -0.013f, 0.879f};

  {
    InputRouter router;
    Capture capture;
    router.beginCalibration(true);
    updateNine(router, capture, false, false, neutral, Screen::Calibrating, 0);
    updateNine(router, capture, true, false, neutral, Screen::Calibrating, 10);
    uint32_t nowMs = feedCalibrationWindow(router, capture, neutral, 35);
    ASSERT_EQ(router.calibrationStep(), CalibrationStep::TiltRight);

    const float rawY = neutral.y + 0.30f;
    Vec3 sample{neutral.x, rawY, neutral.z};
    for (uint32_t elapsed = 0; elapsed < 500; elapsed += 25) {
      nowMs += 25;
      updateNine(router, capture, false, false, sample, Screen::Calibrating,
                 nowMs);
    }
    ASSERT_TRUE(router.boxBallCol() == 0 || router.boxBallCol() == 1);
    ASSERT_EQ(router.boxBallRow(), 10);
    ASSERT_EQ(router.boxTargetCol(), 8);
  }

  {
    InputRouter router;
    Capture capture;
    router.beginCalibration(true);
    updateNine(router, capture, false, false, neutral, Screen::Calibrating, 0);
    updateNine(router, capture, true, false, neutral, Screen::Calibrating, 10);
    uint32_t nowMs = feedCalibrationWindow(router, capture, neutral, 35);
    ASSERT_EQ(router.calibrationStep(), CalibrationStep::TiltRight);

    const Vec3 pitched = pitchShift(neutral, 20.0f);
    for (uint32_t elapsed = 0; elapsed < 500; elapsed += 25) {
      nowMs += 25;
      updateNine(router, capture, false, false, pitched, Screen::Calibrating,
                 nowMs);
    }
    ASSERT_TRUE(router.boxBallRow() >= 0 && router.boxBallRow() <= 2);
  }

  {
    InputRouter router;
    Capture capture;
    router.beginCalibration(true);
    updateNine(router, capture, false, false, Vec3{-1.0f, 0.0f, 0.0f},
               Screen::Calibrating, 0);
    updateNine(router, capture, true, false, Vec3{-1.0f, 0.0f, 0.0f},
               Screen::Calibrating, 10);
    uint32_t nowMs =
        feedCalibrationWindow(router, capture, Vec3{-1.0f, 0.0f, 0.0f}, 35);
    ASSERT_EQ(router.calibrationStep(), CalibrationStep::TiltRight);

    const Vec3 upSample{-1.0f, 0.0f, 0.30f};
    for (uint32_t elapsed = 0; elapsed < 500; elapsed += 25) {
      nowMs += 25;
      updateNine(router, capture, false, false, upSample,
                 Screen::Calibrating, nowMs);
    }
    ASSERT_TRUE(router.boxBallRow() >= 0 && router.boxBallRow() <= 2);
  }
}

SF_TEST(router_pose_captures_the_peak_held) {
  InputRouter router;
  Capture capture;
  const Vec3 neutral{-0.495f, -0.013f, 0.879f};
  uint32_t nowMs = enterFirstPose(router, capture, neutral);

  uint32_t t = nowMs;
  for (uint32_t elapsed = 0; elapsed < 500; elapsed += 10) {
    const float frac = static_cast<float>(elapsed) / 500.0f;
    const float rawY =
        neutral.y + frac * kMazeDefaultReachG * kTiltSignLeftRight;
    updateNine(router, capture, false, false, Vec3{neutral.x, rawY, neutral.z},
               Screen::Calibrating, t + elapsed);
  }
  for (uint32_t elapsed = 500; elapsed < 900; elapsed += 10) {
    const float rawY =
        neutral.y + 0.35f * static_cast<float>(kTiltSignLeftRight);
    updateNine(router, capture, false, false, Vec3{neutral.x, rawY, neutral.z},
               Screen::Calibrating, t + elapsed);
  }
  for (uint32_t elapsed = 900; elapsed < 5000; elapsed += 10) {
    updateNine(router, capture, false, false,
               Vec3{neutral.x, neutral.y + 0.10f * kTiltSignLeftRight, neutral.z},
               Screen::Calibrating, t + elapsed);
  }
  for (uint32_t elapsed = 4400; elapsed < 4500; elapsed += 10) {
    updateNine(router, capture, false, false,
               Vec3{neutral.x, neutral.y + 0.60f * kTiltSignLeftRight, neutral.z},
               Screen::Calibrating, t + elapsed);
  }
  for (uint32_t elapsed = 4500; elapsed < 5100; elapsed += 10) {
    updateNine(router, capture, false, false,
               Vec3{neutral.x, neutral.y + 0.10f * kTiltSignLeftRight, neutral.z},
               Screen::Calibrating, t + elapsed);
  }
  nowMs = t + 5100;
  updateNine(router, capture, false, false,
             Vec3{neutral.x, neutral.y + 0.10f * kTiltSignLeftRight, neutral.z},
             Screen::Calibrating, nowMs);

  ASSERT_EQ(router.calibrationStep(), CalibrationStep::TiltLeft);
  const Vec3 rightPose = router.calibrationRight();
  ASSERT_NEAR(std::fabs(dot(sub(rightPose, neutral), identityBasis().s)),
              0.35f, 0.02f);
}

SF_TEST(router_pose_retries_then_defaults) {
  InputRouter router;
  Capture capture;
  const Vec3 neutral{-0.495f, -0.013f, 0.879f};
  uint32_t nowMs = enterFirstPose(router, capture, neutral);

  const Vec3 small =
      Vec3{neutral.x, neutral.y + 0.10f * kTiltSignLeftRight, neutral.z};
  nowMs = feedCalibrationPose(router, capture, small, nowMs);
  ASSERT_EQ(router.calibrationStep(), CalibrationStep::TiltRight);
  ASSERT_EQ(router.poseRetry(), 1u);
  ASSERT_TRUE(router.poseTooSmall());

  nowMs = feedCalibrationPose(router, capture, small, nowMs);
  ASSERT_EQ(router.calibrationStep(), CalibrationStep::TiltLeft);
  ASSERT_TRUE(router.rightDefaulted());
  ASSERT_NEAR(router.basis().steerScaleRightG, 0.36f, 1e-3f);

  const Vec3 left = Vec3{neutral.x, neutral.y - 0.30f * kTiltSignLeftRight,
                         neutral.z};
  nowMs = feedCalibrationPose(router, capture, left, nowMs);
  ASSERT_EQ(router.calibrationStep(), CalibrationStep::DipAway);
}

SF_TEST(router_dip_offset_tracks_and_resets) {
  InputRouter router;
  Capture capture;
  const Vec3 neutral{-0.456f, -0.023f, 0.899f};
  const TiltBasis basis = driftBasis(neutral);
  uint32_t nowMs = settleNeutralNine(router, capture, neutral);
  router.setBasis(basis);
  capture.clear();

  const Vec3 offset = neutral + scale(basis.f, 0.20f / effectiveDipFactor(basis));
  // With a 5 s time constant and a 1 s quiet ring, the offset tracks slowly.
  for (uint32_t elapsed = kNeutralQuietSampleMs; elapsed <= 12000;
       elapsed += kNeutralQuietSampleMs) {
    nowMs += kNeutralQuietSampleMs;
    updateNine(router, capture, false, false, offset, Screen::Playing, nowMs);
  }
  ASSERT_TRUE(std::fabs(router.dipSignalG()) < 0.05f);
  ASSERT_TRUE(router.dipOffsetG() > 0.10f);
  ASSERT_TRUE(std::fabs(router.dipOffsetG() - 0.20f) < 0.05f);
  ASSERT_EQ(router.lastCaptureSource(), NeutralCaptureSource::Drift);

  // A held toward dip does not move the offset (gate blocks it).
  const Vec3 toward = pitchShift(neutral, -30.0f);
  const float before = router.dipOffsetG();
  for (uint32_t elapsed = 25; elapsed <= 500; elapsed += 25) {
    nowMs += 25;
    updateNine(router, capture, false, false, toward, Screen::Playing, nowMs);
  }
  ASSERT_NEAR(router.dipOffsetG(), before, 0.03f);

  // A neutral capture (calibration re-enter) resets the offset to zero.
  router.beginCalibration(false);
  feedCalibrationWindow(router, capture, neutral, nowMs + 100u);
  ASSERT_NEAR(router.dipOffsetG(), 0.0f, 1e-6f);
}

SF_TEST(router_left_page_ignores_the_residual_right_tilt) {
  InputRouter router;
  Capture capture;
  const CalibrationPose pose = reclinedPose();
  router.beginCalibration(true);
  updateNine(router, capture, false, false, pose.neutral, Screen::Calibrating,
             0);
  updateNine(router, capture, true, false, pose.neutral, Screen::Calibrating,
             10);
  uint32_t nowMs = feedCalibrationWindow(router, capture, pose.neutral, 35);
  ASSERT_EQ(router.calibrationStep(), CalibrationStep::TiltRight);

  // Capture the right pose: +0.32 G raw Y (right on the identity axis).
  Vec3 rightTilt = pose.neutral + Vec3{0.0f, -0.32f, 0.0f};
  nowMs = feedCalibrationPose(router, capture, rightTilt, nowMs);
  ASSERT_EQ(router.calibrationStep(), CalibrationStep::TiltLeft);
  ASSERT_NEAR(sf::input::dot(sub(router.calibrationRight(), pose.neutral),
                             sf::input::identityBasis().s),
              0.32f, 0.02f);

  // On the LEFT page, hold the SAME right tilt for 1.5 s (150 samples at 10 ms),
  // then tilt left 0.30 G raw Y for 1 s (100 samples), then return to neutral.
  // Keep the right tilt alive well past the 300 ms hold window.
  for (int i = 0; i < 150; ++i) {
    updateNine(router, capture, false, false, rightTilt, Screen::Calibrating,
               nowMs + static_cast<uint32_t>(i) * 10u);
  }
  nowMs += 1500u;

  Vec3 leftTilt = pose.neutral + Vec3{0.0f, 0.30f, 0.0f};
  for (int i = 0; i < 100; ++i) {
    updateNine(router, capture, false, false, leftTilt, Screen::Calibrating,
               nowMs + static_cast<uint32_t>(i) * 10u);
  }
  nowMs += 1000u;

  // Now wait out the remaining window so the pose captures.
  for (int i = 0; i < 410; ++i) {
    updateNine(router, capture, false, false, pose.neutral, Screen::Calibrating,
               nowMs + static_cast<uint32_t>(i) * 10u);
  }
  nowMs += 4100u;

  ASSERT_EQ(router.calibrationStep(), CalibrationStep::DipAway);
  ASSERT_NEAR(
      sf::input::dot(sub(router.calibrationLeft(), pose.neutral),
                     sf::input::identityBasis().s),
      -0.30f, 0.02f);
  ASSERT_NEAR(router.basis().steerScaleLeftG, 0.30f, 0.02f);
  ASSERT_NEAR(router.basis().steerScaleRightG, 0.32f, 0.02f);
  ASSERT_TRUE(router.basis().calibrated);
}

SF_TEST(router_tracker_runs_after_a_single_blue_press) {
  InputRouter router;
  Capture capture;
  const CalibrationPose pose = reclinedPose();
  TiltBasis basis = sf::input::identityBasis();
  ASSERT_TRUE(sf::input::deriveSteer(pose.neutral, pose.right, pose.left,
                                     basis));
  ASSERT_TRUE(sf::input::deriveDip(pose.toward, basis));
  router.setBasis(basis);
  const uint32_t startMs = settleNeutralNine(router, capture, pose.neutral);
  capture.clear();
  uint32_t nowMs = startMs;

  // One BLUE press edge in Playing.
  updateNine(router, capture, true, false, pose.neutral, Screen::Playing,
             nowMs + 25u);
  updateNine(router, capture, false, false, pose.neutral, Screen::Playing,
             nowMs + 65u);
  ++nowMs;

  // 1 s later feed a steady +0.20 projected offset for 12 s.
  nowMs += 1000u;
  const Vec3 offsetSample =
      pose.neutral + scale(basis.f, 0.20f);
  for (uint32_t elapsed = 0; elapsed <= 12000u;
       elapsed += kNeutralQuietSampleMs) {
    nowMs += kNeutralQuietSampleMs;
    updateNine(router, capture, false, false, offsetSample, Screen::Playing,
               nowMs);
  }

  ASSERT_TRUE(std::fabs(router.dipSignalG()) < 0.05f);
  ASSERT_EQ(router.lastCaptureSource(), NeutralCaptureSource::Drift);
}

SF_TEST(router_side_press_in_tutorial_emits_tilt_speed) {
  InputRouter router;
  Capture capture;
  router.beginCalibration(true);

  capture.update(router, false, true, 0.0f, 0.0f, Screen::Calibrating, 0);

  ASSERT_EQ(capture.count(ActionKind::TiltSpeed), 1);
  ASSERT_EQ(capture.count(ActionKind::HardDrop), 0);
  ASSERT_EQ(router.reZeroCount(), 0u);
}

SF_TEST(router_no_freeze_with_learned_axes) {
  const CalibrationPose pose = reclinedPose();
  TiltBasis basis = sf::input::identityBasis();
  ASSERT_TRUE(sf::input::deriveSteer(pose.neutral, pose.right, pose.left,
                                     basis));
  ASSERT_TRUE(sf::input::deriveDip(pose.toward, basis));

  InputRouter router;
  Capture capture;
  router.setBasis(basis);
  uint32_t nowMs = settleNeutralNine(router, capture, pose.neutral);
  capture.clear();

  // Hard right tilt while a 25 deg away dip fires -> RotateCw AND the column
  // keeps moving right. With the freeze bug the column stops once the dip
  // engages (~50-100 ms); with no freeze it keeps moving to the wall.
  const Vec3 awaySample =
      pose.neutral + scale(basis.s, 0.55f) + scale(basis.f, 0.40f);
  for (uint32_t elapsed = 0; elapsed <= 400; elapsed += 25) {
    nowMs += 25;
    updateNine(router, capture, false, false, awaySample, Screen::Playing,
               nowMs);
  }
  ASSERT_EQ(capture.count(ActionKind::RotateCw), 1);
  ASSERT_TRUE(capture.count(ActionKind::MoveRight) > 0);
  ASSERT_TRUE(router.dipEngaged());
  ASSERT_TRUE(router.selectedColumn() >= 7);

  // The same with a toward dip held 1 s -> SoftDropOn AND moves continue.
  InputRouter towardRouter;
  Capture towardCapture;
  towardRouter.setBasis(basis);
  uint32_t tNow = settleNeutralNine(towardRouter, towardCapture, pose.neutral);
  towardCapture.clear();

  const Vec3 towardSample =
      pose.neutral + scale(basis.s, 0.55f) + scale(basis.f, -0.40f);
  for (uint32_t elapsed = 0; elapsed <= 1000; elapsed += 25) {
    tNow += 25;
    updateNine(towardRouter, towardCapture, false, false, towardSample,
               Screen::Playing, tNow);
  }
  ASSERT_EQ(towardCapture.count(ActionKind::SoftDropOn), 1);
  ASSERT_TRUE(towardCapture.count(ActionKind::MoveRight) > 0);
  ASSERT_TRUE(towardRouter.dipEngaged() ||
              towardRouter.softDropActive());
  ASSERT_TRUE(towardRouter.selectedColumn() >= 7);
}

SF_TEST(router_tracker_keeps_up_without_a_quiet_window) {
  const Vec3 neutral{-0.456f, -0.023f, 0.899f};
  const TiltBasis basis = driftBasis(neutral);

  // A steady +0.20 offset with a small 0.02 G wobble tracks to +0.20 with the
  // 2 s time constant.
  InputRouter router;
  Capture capture;
  uint32_t nowMs = settleNeutralNine(router, capture, neutral);
  router.setBasis(basis);
  capture.clear();

  const float dipSignal = 0.20f;
  const Vec3 offsetBase =
      neutral + scale(basis.f, dipSignal / effectiveDipFactor(basis));
  for (uint32_t elapsed = kNeutralQuietSampleMs; elapsed <= 6000;
       elapsed += kNeutralQuietSampleMs) {
    nowMs += kNeutralQuietSampleMs;
    // 0.02 G wobble: alternate +/- 0.01 G each sample.
    float wobble = (elapsed / kNeutralQuietSampleMs) % 2u == 0u ? 0.01f : -0.01f;
    Vec3 sample = offsetBase + scale(basis.f, wobble);
    updateNine(router, capture, false, false, sample, Screen::Playing, nowMs);
  }
  ASSERT_TRUE(std::fabs(router.dipOffsetG() - 0.20f) < 0.05f);

  // The same +0.20 offset with a LARGE 0.10 G wobble STILL tracks now that the
  // quiet window is gone: a wobbling but centred rest is a rest. Before the fix
  // the 1 s quiet ring blocked this and the offset stayed near 0.
  InputRouter wobbleRouter;
  Capture wobbleCapture;
  uint32_t wNow = settleNeutralNine(wobbleRouter, wobbleCapture, neutral);
  wobbleRouter.setBasis(basis);
  wobbleCapture.clear();
  for (uint32_t elapsed = kNeutralQuietSampleMs; elapsed <= 6000;
           elapsed += kNeutralQuietSampleMs) {
    wNow += kNeutralQuietSampleMs;
    float wobble = (elapsed / kNeutralQuietSampleMs) % 2u == 0u ? 0.05f : -0.05f;
    Vec3 sample = offsetBase + scale(basis.f, wobble);
    updateNine(wobbleRouter, wobbleCapture, false, false, sample,
               Screen::Playing, wNow);
  }
  ASSERT_TRUE(std::fabs(wobbleRouter.dipOffsetG() - 0.20f) < 0.05f);

  // A held dip PAST the engage band still does NOT move the offset: the
  // |dipN - offset| >= kDipEngageG gate is unchanged, so a real gesture is not
  // mistaken for a resting posture.
  InputRouter heldRouter;
  Capture heldCapture;
  uint32_t hNow = settleNeutralNine(heldRouter, heldCapture, neutral);
  heldRouter.setBasis(basis);
  heldCapture.clear();
  const Vec3 heldDip =
      neutral + scale(basis.f, 0.40f / effectiveDipFactor(basis));
  for (uint32_t elapsed = kNeutralQuietSampleMs; elapsed <= 3000;
       elapsed += kNeutralQuietSampleMs) {
    hNow += kNeutralQuietSampleMs;
    updateNine(heldRouter, heldCapture, false, false, heldDip, Screen::Playing,
               hNow);
  }
  ASSERT_TRUE(std::fabs(heldRouter.dipOffsetG()) < 0.05f);
}

namespace {
// A learned basis + poses whose dip axis is well clear of the engage band, used
// by the maze-seed and maze-row tests below.
CalibrationPose learnedMazePose(TiltBasis& learned) {
  CalibrationPose pose{};
  pose.neutral = Vec3{-0.375f, 0.0f, 0.927f};
  pose.right = pose.neutral + scale(identityBasis().s, 0.30f);
  pose.left = sub(pose.neutral, scale(identityBasis().s, 0.30f));
  pose.away = pose.neutral + scale(identityBasis().f, 0.30f);
  pose.toward = sub(pose.neutral, scale(identityBasis().f, 0.30f));
  learned = identityBasis();
  const Vec3 rightRaw{-0.722f, -0.589f, 0.354f};
  const Vec3 leftRaw{-0.058f, 0.832f, 0.554f};
  const Vec3 right = scale(rightRaw, 1.0f / norm(rightRaw));
  const Vec3 left = scale(leftRaw, 1.0f / norm(leftRaw));
  const Vec3 toward = rotateAround(pose.neutral, Vec3{0.0f, 1.0f, 0.0f},
                                   -25.0f * kPi / 180.0f);
  ASSERT_TRUE(sf::input::deriveSteer(pose.neutral, right, left, learned));
  ASSERT_TRUE(sf::input::deriveDip(toward, learned));
  return pose;
}
}  // namespace

SF_TEST(router_maze_seeds_the_offset_on_entry_and_reseeds_until_armed) {
  InputRouter router;
  Capture capture;
  TiltBasis learned{};
  const CalibrationPose pose = learnedMazePose(learned);
  uint32_t nowMs = enterMaze(router, capture, pose);
  router.setBasis(learned);
  // Every tutorial run enters the maze with the offset zeroed.
  ASSERT_NEAR(router.dipOffsetG(), 0.0f, 1e-6f);

  // A resting AWAY drift of +0.40 (normalised), which pins the ball at row 0
  // with no offset. By construction projectDip(drift)=0.40/factor so dipN=0.40.
  const float factor = effectiveDipFactor(learned);
  const Vec3 drift = pose.neutral + scale(learned.f, 0.40f / factor);

  // Hold still through the seed window: the offset seeds to the resting dip,
  // the ball returns to the start, one [NEU] src=seed is emitted.
  const uint32_t before = router.neutralCaptureCount();
  nowMs = feedMaze(router, capture, drift, kMazeSeedStillMs + 150u, nowMs);
  ASSERT_FALSE(router.mazeSeedPending());
  ASSERT_TRUE(std::fabs(router.dipOffsetG() - 0.40f) < 0.05f);
  ASSERT_EQ(router.neutralCaptureCount(), before + 1u);
  ASSERT_EQ(router.lastCaptureSource(), NeutralCaptureSource::Seed);
  ASSERT_TRUE(router.mazeBallRow() >= 9 && router.mazeBallRow() <= 11);

  // A moving hand never seeds again: alternate two poses so the span exceeds the
  // still band on every sample.
  const uint32_t afterFirst = router.neutralCaptureCount();
  for (int i = 0; i < 40; ++i) {
    nowMs += kNeutralQuietSampleMs;
    const Vec3 wig = (i % 2 == 0)
                         ? pose.neutral + scale(learned.f, 0.10f / factor)
                         : pose.neutral + scale(learned.f, 0.30f / factor);
    updateNine(router, capture, false, false, wig, Screen::Calibrating, nowMs);
  }
  ASSERT_EQ(router.neutralCaptureCount(), afterFirst);

  // A still hand in a NEW pose before the maze arms re-seeds to that pose.
  const Vec3 drift2 = pose.neutral + scale(learned.f, 0.20f / factor);
  nowMs = feedMaze(router, capture, drift2, kMazeSeedStillMs + 150u, nowMs);
  ASSERT_EQ(router.neutralCaptureCount(), afterFirst + 1u);
  ASSERT_TRUE(std::fabs(router.dipOffsetG() - 0.20f) < 0.05f);
}

SF_TEST(router_maze_row_follows_the_tracked_dip) {
  InputRouter router;
  Capture capture;
  TiltBasis learned{};
  const CalibrationPose pose = learnedMazePose(learned);
  uint32_t nowMs = enterMaze(router, capture, pose);
  router.setBasis(learned);
  const float factor = effectiveDipFactor(learned);

  // Seed the offset at a +0.40 away rest, then a resting sample sits at the
  // start row (the row uses the offset-corrected dip, not the raw projection).
  const Vec3 rest = pose.neutral + scale(learned.f, 0.40f / factor);
  nowMs = feedMaze(router, capture, rest, kMazeSeedStillMs + 150u, nowMs);
  ASSERT_TRUE(router.mazeBallRow() >= 9 && router.mazeBallRow() <= 11);

  // A further away dip beyond the seeded rest drives the ball up toward row 0;
  // a toward dip drives it down. Both are measured from the tracked offset.
  const Vec3 moreAway = pose.neutral + scale(learned.f, 0.75f / factor);
  nowMs = feedMaze(router, capture, moreAway, 300, nowMs);
  ASSERT_TRUE(router.mazeBallRow() < 9);
}

SF_TEST(router_box_window_starts_on_first_motion) {
  InputRouter router;
  Capture capture;
  const Vec3 neutral{-0.495f, -0.013f, 0.879f};
  uint32_t nowMs = enterFirstPose(router, capture, neutral);
  // enterFirstPose leaves us on TiltRight with the window not yet running.
  ASSERT_FALSE(router.poseWindowRunning());

  // 3 s of nothing: the window must not start.
  for (uint32_t elapsed = 0; elapsed < 3000; elapsed += 10) {
    updateNine(router, capture, false, false, neutral, Screen::Calibrating,
               nowMs + elapsed);
  }
  ASSERT_FALSE(router.poseWindowRunning());
  ASSERT_EQ(router.calibrationStep(), CalibrationStep::TiltRight);

  // A 0.30 G tilt held 400 ms: the window starts on the first motion.
  const float rawY = neutral.y + 0.30f * static_cast<float>(kTiltSignLeftRight);
  const Vec3 tilt{neutral.x, rawY, neutral.z};
  uint32_t motionStart = nowMs + 3000;
  ASSERT_FALSE(router.poseWindowRunning());
  for (uint32_t elapsed = 0; elapsed < 400; elapsed += 10) {
    updateNine(router, capture, false, false, tilt, Screen::Calibrating,
               motionStart + elapsed);
  }
  ASSERT_TRUE(router.poseWindowRunning());

  // No default and no retry: the pose is captured when the window ends 5 s
  // after the motion began.
  for (uint32_t elapsed = 400; elapsed < 5400; elapsed += 10) {
    updateNine(router, capture, false, false, tilt, Screen::Calibrating,
               motionStart + elapsed);
  }
  ASSERT_EQ(router.calibrationStep(), CalibrationStep::TiltLeft);
  ASSERT_EQ(router.poseRetry(), 0u);
  ASSERT_FALSE(router.rightDefaulted());
  ASSERT_NEAR(std::fabs(dot(sub(router.calibrationRight(), neutral),
                            identityBasis().s)),
              0.30f, 0.02f);

}

SF_TEST(router_pose_window_starts_on_ceiling_with_no_motion) {
  InputRouter router;
  Capture capture;
  const Vec3 neutral{-0.495f, -0.013f, 0.879f};
  uint32_t nowMs = enterFirstPose(router, capture, neutral);
  // The window is not yet running.
  ASSERT_FALSE(router.poseWindowRunning());

  // A page with no motion for 8 s starts its window anyway and then retries
  // and defaults as before. The 8 s ceiling starts the window; the 5 s window
  // ends and retries (poseRetry 1); the ceiling starts again and the window
  // ends again and defaults, advancing the step.
  const uint32_t kCeilingFeedMs =
      kPoseWaitCeilingMs + kPoseWindowMs +
      kPoseWaitCeilingMs + kPoseWindowMs + 100;
  for (uint32_t elapsed = 0; elapsed < kCeilingFeedMs; elapsed += 10) {
    updateNine(router, capture, false, false, neutral, Screen::Calibrating,
               nowMs + elapsed);
  }
  ASSERT_EQ(router.calibrationStep(), CalibrationStep::TiltLeft);
  ASSERT_EQ(router.poseRetry(), 0u);
  ASSERT_TRUE(router.rightDefaulted());
}
