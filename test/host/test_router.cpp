#include "framework.h"
#include "stackfall/input/bindings.h"
#include "stackfall/input/router.h"

#include <cstddef>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <vector>

using sf::ControlProfile;
using sf::Screen;
using sf::input::ActionKind;
using sf::input::ActionQueue;
using sf::input::GameAction;
using sf::input::Gesture;
using sf::input::InputRouter;
using sf::input::TiltBasis;
using sf::input::Vec3;
using sf::input::dot;
using sf::input::formatTrace;
using sf::input::kBindingCount;
using sf::input::kBindings;
using sf::input::kMaxBindingsPerScreen;
using sf::input::norm;
using sf::input::scale;

namespace {

float rawY(float correctedSignal) {
  return correctedSignal * static_cast<float>(kTiltSignLeftRight);
}

struct Capture {
  void update(InputRouter& router, bool blue, bool side, float yG, float zG,
              Screen screen, uint32_t nowMs) {
    router.update(blue, side, yG, zG, screen, nowMs, queue);
    GameAction action{};
    while (queue.pop(action)) {
      actions.push_back(action);
    }
  }

  int count(ActionKind kind) const {
    int result = 0;
    for (const GameAction& action : actions) {
      if (action.kind == kind) {
        ++result;
      }
    }
    return result;
  }

  void clear() { actions.clear(); }

  ActionQueue queue;
  std::vector<GameAction> actions;
};

void assertOnlyAction(const Capture& capture, ActionKind kind, uint32_t atMs) {
  ASSERT_EQ(capture.actions.size(), 1u);
  ASSERT_EQ(capture.actions[0].kind, kind);
  ASSERT_EQ(capture.actions[0].tMs, atMs);
}

uint32_t settleNeutral(InputRouter& router, Capture& capture, Screen screen,
                       uint32_t nowMs, float yG = 0.0f,
                       float zG = 0.0f) {
  for (std::size_t i = 0; i < sf::input::kNeutralQuietSamples; ++i) {
    capture.update(
        router, false, false, yG, zG, screen,
        nowMs + static_cast<uint32_t>(i) * sf::input::kNeutralQuietSampleMs);
  }
  ASSERT_TRUE(router.neutralReady());
  ASSERT_EQ(capture.actions.size(), 0u);
  return nowMs + static_cast<uint32_t>(sf::input::kNeutralQuietSamples - 1u) *
                     sf::input::kNeutralQuietSampleMs;
}

uint32_t enterPlay(InputRouter& router, Capture& capture,
                   uint32_t nowMs = 0, float yG = 0.0f,
                   float zG = 0.0f) {
  return settleNeutral(router, capture, Screen::Playing, nowMs, yG, zG);
}

}  // namespace

SF_TEST(router_blocked_move_steers_from_the_real_piece) {
  InputRouter router;
  Capture capture;
  const uint32_t readyMs = enterPlay(router, capture);

  router.setActivePiece(
      sf::ActivePiece{sf::PieceId::I, 0, static_cast<int8_t>(3), 21}, true);
  for (uint32_t nowMs = readyMs + 40; nowMs <= readyMs + 200;
       nowMs += 40) {
    capture.update(router, false, false, rawY(0.75f), 0.0f,
                   Screen::Playing, nowMs);
  }
  ASSERT_EQ(capture.count(ActionKind::MoveRight), 3);

  capture.clear();
  router.setActivePiece(
      sf::ActivePiece{sf::PieceId::I, 0, static_cast<int8_t>(5), 21}, true);
  capture.update(router, false, false, rawY(0.75f), 0.0f, Screen::Playing,
                 readyMs + 240);
  capture.clear();
  capture.update(router, false, false, rawY(0.75f), 0.0f, Screen::Playing,
                 readyMs + 280);
  ASSERT_EQ(capture.actions.size(), 0u);

  capture.clear();
  for (uint32_t nowMs = readyMs + 320; nowMs <= readyMs + 600;
       nowMs += 40) {
    capture.update(router, false, false, rawY(-0.05f), 0.0f,
                   Screen::Playing, nowMs);
  }
  ASSERT_EQ(capture.count(ActionKind::MoveLeft), 1);
}

SF_TEST(router_new_piece_follows_the_held_tilt) {
  InputRouter router;
  Capture capture;
  const uint32_t readyMs = enterPlay(router, capture);

  router.setActivePiece(
      sf::ActivePiece{sf::PieceId::I, 0, static_cast<int8_t>(6), 21}, true);
  for (uint32_t nowMs = readyMs + 40; nowMs <= readyMs + 200;
       nowMs += 40) {
    capture.update(router, false, false, rawY(0.75f), 0.0f,
                   Screen::Playing, nowMs);
  }
  capture.clear();
  capture.update(router, false, false, rawY(0.75f), 0.0f, Screen::Playing,
                 readyMs + 240);
  ASSERT_EQ(capture.actions.size(), 0u);

  router.setActivePiece(
      sf::ActivePiece{sf::PieceId::I, 0, static_cast<int8_t>(3), 21}, true);
  capture.update(router, false, false, rawY(0.75f), 0.0f, Screen::Playing,
                 readyMs + 280);
  ASSERT_EQ(capture.count(ActionKind::MoveRight), 3);
}

SF_TEST(router_target_is_clamped_to_the_shape) {
  InputRouter router;
  Capture capture;
  const uint32_t readyMs = enterPlay(router, capture);
  router.setActivePiece(
      sf::ActivePiece{sf::PieceId::I, 0, static_cast<int8_t>(3), 21}, true);
  for (uint32_t nowMs = readyMs + 40; nowMs <= readyMs + 200;
       nowMs += 40) {
    capture.update(router, false, false, rawY(0.75f), 0.0f,
                   Screen::Playing, nowMs);
  }
  ASSERT_EQ(capture.count(ActionKind::MoveRight), 3);

  InputRouter verticalRouter;
  Capture vertical;
  const uint32_t verticalReadyMs = enterPlay(verticalRouter, vertical);
  for (uint32_t nowMs = verticalReadyMs + 40;
       nowMs <= verticalReadyMs + 200; nowMs += 40) {
    vertical.update(verticalRouter, false, false, rawY(-0.75f), 0.0f,
                    Screen::Playing, nowMs);
  }
  vertical.clear();
  verticalRouter.setActivePiece(
      sf::ActivePiece{sf::PieceId::I, 1, static_cast<int8_t>(3), 21}, true);
  vertical.update(verticalRouter, false, false, rawY(-0.75f), 0.0f,
                  Screen::Playing, verticalReadyMs + 240);
  ASSERT_EQ(vertical.count(ActionKind::MoveLeft), 5);
}

SF_TEST(router_without_piece_feedback_keeps_legacy_deltas) {
  InputRouter router;
  Capture capture;
  const uint32_t readyMs = enterPlay(router, capture);

  for (uint32_t nowMs = readyMs + 40; nowMs <= readyMs + 200;
       nowMs += 40) {
    capture.update(router, false, false, rawY(0.75f), 0.0f,
                   Screen::Playing, nowMs);
  }
  ASSERT_EQ(capture.count(ActionKind::MoveRight), 5);

  capture.clear();
  for (uint32_t nowMs = readyMs + 240; nowMs <= readyMs + 560;
       nowMs += 40) {
    capture.update(router, false, false, rawY(-0.05f), 0.0f,
                   Screen::Playing, nowMs);
  }
  ASSERT_EQ(capture.count(ActionKind::MoveLeft), 5);
}

SF_TEST(router_remaps_the_same_blue_edge_by_screen) {
  InputRouter router;
  Capture capture;

  capture.update(router, false, false, 0.0f, 0.0f, Screen::Title, 0);
  capture.update(router, true, false, 0.0f, 0.0f, Screen::Title, 10);
  assertOnlyAction(capture, ActionKind::Confirm, 10);

  capture.update(router, false, false, 0.0f, 0.0f, Screen::Title, 20);
  capture.clear();
  const uint32_t readyMs = enterPlay(router, capture, 30);
  capture.update(router, true, false, 0.0f, 0.0f, Screen::Playing,
                 readyMs + 10);
  assertOnlyAction(capture, ActionKind::RotateCw, readyMs + 10);
}

SF_TEST(router_pause_modifier_suppresses_side_on_play_and_title) {
  const Screen screens[] = {Screen::Playing, Screen::Title};
  for (Screen screen : screens) {
    InputRouter router;
    Capture capture;
    capture.update(router, true, false, 0.0f, 0.0f, screen, 0);
    capture.clear();

    capture.update(router, true, true, 0.0f, 0.0f, screen, 100);
    ASSERT_EQ(capture.actions.size(), 0u);
    ASSERT_EQ(capture.count(ActionKind::HardDrop), 0);
    ASSERT_EQ(capture.count(ActionKind::MenuDown), 0);
  }
}

SF_TEST(router_never_feeds_imu_off_play_and_captures_entry_neutral) {
  InputRouter router;
  Capture capture;
  const float heldY = rawY(0.30f);

  for (uint32_t nowMs = 0; nowMs <= 2000; nowMs += 5) {
    capture.update(router, false, false, heldY, 0.30f, Screen::Paused,
                   nowMs);
  }
  ASSERT_EQ(capture.actions.size(), 0u);

  const uint32_t readyMs = enterPlay(router, capture, 2005, heldY, 0.30f);

  for (uint32_t nowMs = readyMs + 40; nowMs <= readyMs + 320;
       nowMs += 40) {
    capture.update(router, false, false, rawY(0.75f), 0.30f,
                   Screen::Playing, nowMs);
  }
  ASSERT_TRUE(capture.count(ActionKind::MoveRight) > 0);
}

SF_TEST(router_steering_follows_tilt_during_a_dip) {
  InputRouter router;
  Capture capture;
  const uint32_t readyMs = enterPlay(router, capture);

  // While an away dip is engaged, a one-column Y change emits the move exactly
  // as without the dip.
  router.setActivePiece(
      sf::ActivePiece{sf::PieceId::I, 0, static_cast<int8_t>(4), 20}, true);
  for (uint32_t nowMs = readyMs + 40; nowMs <= readyMs + 120; nowMs += 40) {
    capture.update(router, false, false, rawY(0.75f), 0.78f,
                   Screen::Playing, nowMs);
  }
  ASSERT_EQ(capture.count(ActionKind::RotateCw), 1);
  ASSERT_EQ(capture.count(ActionKind::MoveLeft), 0);
  ASSERT_TRUE(capture.count(ActionKind::MoveRight) > 0);

  // While a toward soft drop is held, the same is true.
  InputRouter towardRouter;
  Capture toward;
  const uint32_t towardReadyMs = enterPlay(towardRouter, toward);
  towardRouter.setActivePiece(
      sf::ActivePiece{sf::PieceId::I, 0, static_cast<int8_t>(4), 20}, true);
  for (uint32_t nowMs = towardReadyMs + 40;
       nowMs <= towardReadyMs + 120; nowMs += 40) {
    toward.update(towardRouter, false, false, 0.0f, -0.78f,
                  Screen::Playing, nowMs);
  }
  for (uint32_t nowMs = towardReadyMs + 160;
       nowMs <= towardReadyMs + 280; nowMs += 40) {
    toward.update(towardRouter, false, false, 0.0f, 0.0f,
                  Screen::Playing, nowMs);
  }

  ASSERT_EQ(toward.count(ActionKind::SoftDropOn), 1);
  ASSERT_EQ(toward.count(ActionKind::SoftDropOff), 1);
}

SF_TEST(router_buttons_only_ignores_both_imu_axes_but_keeps_side) {
  InputRouter router;
  router.setProfile(ControlProfile::BUTTONS_ONLY);
  Capture capture;

  for (uint32_t nowMs = 0; nowMs <= 2000; nowMs += 5) {
    capture.update(router, false, false, rawY(0.75f),
                   (nowMs & 8u) == 0 ? 0.78f : -0.78f, Screen::Playing,
                   nowMs);
  }
  ASSERT_EQ(capture.actions.size(), 0u);

  capture.update(router, false, true, rawY(0.75f), 0.78f,
                 Screen::Playing, 2005);
  assertOnlyAction(capture, ActionKind::MoveRight, 2005);
}

SF_TEST(router_rotation_setting_flips_blue_and_away_dip) {
  InputRouter router;
  router.setRotateCcw(true);
  Capture capture;
  const uint32_t readyMs = enterPlay(router, capture);

  capture.update(router, true, false, 0.0f, 0.0f, Screen::Playing,
                 readyMs + 10);
  ASSERT_EQ(capture.count(ActionKind::RotateCcw), 1);
  capture.update(router, false, false, 0.0f, 0.0f, Screen::Playing,
                 readyMs + 20);
  for (uint32_t nowMs = readyMs + 60; nowMs <= readyMs + 140;
       nowMs += 40) {
    capture.update(router, false, false, 0.0f, 0.78f, Screen::Playing,
                   nowMs);
  }

  ASSERT_EQ(capture.count(ActionKind::RotateCcw), 2);
  ASSERT_EQ(capture.count(ActionKind::RotateCw), 0);
}

SF_TEST(router_rotation_setting_only_flips_playing_bindings) {
  InputRouter router;
  router.setRotateCcw(true);
  Capture capture;

  capture.update(router, false, false, 0.0f, 0.0f, Screen::Title, 0);
  capture.update(router, true, false, 0.0f, 0.0f, Screen::Title, 10);
  ASSERT_EQ(capture.count(ActionKind::Confirm), 1);
  ASSERT_EQ(capture.count(ActionKind::RotateCcw), 0);
  capture.update(router, false, false, 0.0f, 0.0f, Screen::Title, 20);
  capture.clear();

  capture.update(router, true, false, 0.0f, 0.0f, Screen::Settings, 30);
  ASSERT_EQ(capture.count(ActionKind::Confirm), 1);
  ASSERT_EQ(capture.count(ActionKind::RotateCcw), 0);
  capture.update(router, false, false, 0.0f, 0.0f, Screen::Settings, 40);
  capture.clear();

  capture.update(router, true, false, 0.0f, 0.0f, Screen::Diagnostics, 50);
  ASSERT_EQ(capture.count(ActionKind::Back), 1);
  ASSERT_EQ(capture.count(ActionKind::RotateCcw), 0);
  capture.update(router, false, false, 0.0f, 0.0f, Screen::Diagnostics, 60);
  capture.clear();

  const uint32_t readyMs = enterPlay(router, capture, 70);
  capture.update(router, true, false, 0.0f, 0.0f, Screen::Playing,
                 readyMs + 10);
  ASSERT_EQ(capture.count(ActionKind::RotateCcw), 1);
  capture.update(router, false, false, 0.0f, 0.0f, Screen::Playing,
                 readyMs + 20);
  capture.clear();
  for (uint32_t nowMs = readyMs + 60; nowMs <= readyMs + 140;
       nowMs += 40) {
    capture.update(router, false, false, 0.0f, 0.78f, Screen::Playing,
                   nowMs);
  }
  ASSERT_EQ(capture.count(ActionKind::RotateCcw), 1);
}

SF_TEST(router_stale_imu_samples_do_not_advance_filters) {
  InputRouter router;
  ActionQueue queue;

  for (std::size_t i = 0; i < sf::input::kNeutralQuietSamples; ++i) {
    router.update(
        false, false, 0.0f, 0.0f, true, Screen::Playing,
        static_cast<uint32_t>(i) * sf::input::kNeutralQuietSampleMs, queue);
  }
  ASSERT_TRUE(router.neutralReady());
  const uint32_t readyMs =
      static_cast<uint32_t>(sf::input::kNeutralQuietSamples - 1u) *
      sf::input::kNeutralQuietSampleMs;
  for (uint32_t nowMs = readyMs + 40; nowMs <= readyMs + 400;
       nowMs += 40) {
    router.update(false, false, rawY(0.75f), 0.0f, false,
                  Screen::Playing, nowMs, queue);
  }
  ASSERT_EQ(queue.size(), 0u);

  router.update(false, false, rawY(0.75f), 0.0f, true,
                Screen::Playing, readyMs + 440, queue);
  ASSERT_TRUE(queue.size() > 0u);
}

SF_TEST(router_bindings_are_unique_bounded_and_cover_all_screens) {
  bool seenScreen[9]{};
  for (std::size_t i = 0; i < kBindingCount; ++i) {
    const std::size_t screen = static_cast<std::size_t>(kBindings[i].screen);
    ASSERT_TRUE(screen < 9u);
    seenScreen[screen] = true;

    std::size_t count = 0;
    for (std::size_t j = 0; j < kBindingCount; ++j) {
      if (kBindings[j].screen == kBindings[i].screen) {
        ++count;
      }
      if (j > i) {
        ASSERT_TRUE(kBindings[j].screen != kBindings[i].screen ||
                    kBindings[j].gesture != kBindings[i].gesture);
      }
    }
    ASSERT_TRUE(count <= kMaxBindingsPerScreen);
  }

  for (bool present : seenScreen) {
    ASSERT_TRUE(present);
  }

  bool orderedChordLabel = false;
  for (std::size_t i = 0; i < kBindingCount; ++i) {
    if (kBindings[i].screen == Screen::Playing &&
        kBindings[i].gesture == Gesture::Chord) {
      ASSERT_STR_EQ(kBindings[i].gestureLabel, "BLUE 1ST+SIDE");
      orderedChordLabel = true;
    }
  }
  ASSERT_TRUE(orderedChordLabel);
}

SF_TEST(router_trace_formatter_is_exact_and_truncates_safely) {
  const GameAction action{1234, ActionKind::RotateCw, 0};
  char full[96]{};
  const int fullLength =
      formatTrace(full, sizeof(full), action, Screen::Playing, 1, 0);
  ASSERT_EQ(fullLength, 62);
  ASSERT_STR_EQ(
      full,
      "[TRACE] t=1234 scr=PLAYING act=ROTATE_CW src=BTN q=1/16 drop=0");

  char shortBuffer[8] = {'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x'};
  const int shortLength = formatTrace(shortBuffer, sizeof(shortBuffer), action,
                                      Screen::Playing, 1, 0);
  ASSERT_EQ(shortLength, 62);
  ASSERT_EQ(std::strlen(shortBuffer), 7u);
  ASSERT_STR_EQ(shortBuffer, "[TRACE]");
}

SF_TEST(router_title_diagnostics_requires_2000ms_and_latches) {
  InputRouter router;
  Capture capture;

  capture.update(router, true, false, 0.0f, 0.0f, Screen::Title, 10);
  ASSERT_EQ(capture.count(ActionKind::Confirm), 1);
  capture.clear();

  capture.update(router, true, true, 0.0f, 0.0f, Screen::Title, 20);
  capture.update(router, true, true, 0.0f, 0.0f, Screen::Title, 819);
  capture.update(router, true, true, 0.0f, 0.0f, Screen::Title, 820);
  capture.update(router, true, true, 0.0f, 0.0f, Screen::Title, 2019);
  ASSERT_EQ(capture.count(ActionKind::Diag), 0);

  capture.update(router, true, true, 0.0f, 0.0f, Screen::Title, 2020);
  assertOnlyAction(capture, ActionKind::Diag, 2020);
  capture.update(router, true, true, 0.0f, 0.0f, Screen::Title, 10020);
  ASSERT_EQ(capture.count(ActionKind::Diag), 1);

  capture.update(router, false, false, 0.0f, 0.0f, Screen::Title, 10030);
  capture.clear();
  capture.update(router, true, false, 0.0f, 0.0f, Screen::Title, 10040);
  capture.clear();
  capture.update(router, true, true, 0.0f, 0.0f, Screen::Title, 10050);
  capture.update(router, true, true, 0.0f, 0.0f, Screen::Title, 12050);
  assertOnlyAction(capture, ActionKind::Diag, 12050);
}

SF_TEST(router_diagnostics_observes_imu_without_emitting_gameplay) {
  InputRouter router;
  Capture capture;
  const uint32_t readyMs =
      settleNeutral(router, capture, Screen::Diagnostics, 0);

  for (uint32_t nowMs = readyMs + 40; nowMs <= readyMs + 240;
       nowMs += 40) {
    capture.update(router, false, false, rawY(0.75f), 0.0f,
                   Screen::Diagnostics, nowMs);
  }
  ASSERT_TRUE(router.selectedColumn() > 4);
  ASSERT_TRUE(router.tiltSignalG() > 0.0f);
  ASSERT_EQ(capture.actions.size(), 0u);

  for (uint32_t nowMs = readyMs + 280; nowMs <= readyMs + 400;
       nowMs += 40) {
    capture.update(router, false, false, rawY(0.75f), 0.78f,
                   Screen::Diagnostics, nowMs);
  }
  ASSERT_TRUE(router.dipEngaged());
  ASSERT_TRUE(router.dipSignalG() > 0.0f);
  ASSERT_EQ(capture.count(ActionKind::RotateCw), 0);
  ASSERT_EQ(capture.count(ActionKind::MoveRight), 0);
}

namespace {

void updateNine(InputRouter& router, Capture& capture, bool blue, bool side,
                float xG, float yG, float zG, Screen screen, uint32_t nowMs) {
  router.update(blue, side, xG, yG, zG, true, screen, nowMs,
                capture.queue);
  GameAction action{};
  while (capture.queue.pop(action)) capture.actions.push_back(action);
}

uint32_t settleNeutralNine(InputRouter& router, Capture& capture,
                           uint32_t nowMs, float xG, float yG, float zG) {
  for (std::size_t i = 0; i < sf::input::kNeutralQuietSamples; ++i) {
    updateNine(router, capture, false, false, xG, yG, zG, Screen::Playing,
               nowMs + static_cast<uint32_t>(i) *
                           sf::input::kNeutralQuietSampleMs);
  }
  ASSERT_TRUE(router.neutralReady());
  ASSERT_EQ(capture.actions.size(), 0u);
  return nowMs + static_cast<uint32_t>(sf::input::kNeutralQuietSamples - 1u) *
                     sf::input::kNeutralQuietSampleMs;
}

constexpr float kPi = 3.14159265358979323846f;

Vec3 rotateAround(Vec3 value, Vec3 axis, float radians) {
  const float axisNorm = norm(axis);
  axis = scale(axis, 1.0f / axisNorm);
  const float c = std::cos(radians);
  const float s = std::sin(radians);
  return scale(value, c) +
         scale(Vec3{axis.y * value.z - axis.z * value.y,
                    axis.z * value.x - axis.x * value.z,
                    axis.x * value.y - axis.y * value.x},
               s) +
         scale(axis, dot(axis, value) * (1.0f - c));
}

TiltBasis reclinedBasis(Vec3& g0, Vec3& right, Vec3& left, Vec3& toward) {
  const float angle = 25.0f * kPi / 180.0f;
  g0 = Vec3{0.519f, 0.423f, 0.743f};
  right = rotateAround(g0, Vec3{0.0f, 0.0f, 1.0f}, -angle);
  left = rotateAround(g0, Vec3{0.0f, 0.0f, 1.0f}, angle);
  TiltBasis basis = sf::input::identityBasis();
  ASSERT_TRUE(sf::input::deriveSteer(g0, right, left, basis));
  toward = rotateAround(g0, basis.s, angle);
  ASSERT_TRUE(sf::input::deriveDip(toward, basis));
  return basis;
}

}  // namespace

SF_TEST(router_nine_arg_identity_matches_legacy) {
  InputRouter legacy;
  InputRouter threeAxis;
  Capture legacyCapture;
  Capture threeAxisCapture;
  constexpr float xG = 0.6f;
  const uint32_t readyMs = settleNeutral(legacy, legacyCapture, Screen::Playing,
                                         0, 0.0f, 0.0f);
  const uint32_t readyNine =
      settleNeutralNine(threeAxis, threeAxisCapture, 0, xG, 0.0f, 0.0f);
  ASSERT_EQ(readyMs, readyNine);

  for (uint32_t nowMs = readyMs + 40; nowMs <= readyMs + 120; nowMs += 40) {
    legacyCapture.update(legacy, false, false, rawY(0.75f), 0.0f,
                         Screen::Playing, nowMs);
    updateNine(threeAxis, threeAxisCapture, false, false, xG, rawY(0.75f),
               0.0f, Screen::Playing, nowMs);
  }
  for (uint32_t nowMs = readyMs + 160; nowMs <= readyMs + 280; nowMs += 40) {
    legacyCapture.update(legacy, false, false, 0.0f, -0.78f,
                         Screen::Playing, nowMs);
    updateNine(threeAxis, threeAxisCapture, false, false, xG, 0.0f, -0.78f,
               Screen::Playing, nowMs);
  }
  for (uint32_t nowMs = readyMs + 320; nowMs <= readyMs + 440; nowMs += 40) {
    legacyCapture.update(legacy, false, false, 0.0f, 0.0f,
                         Screen::Playing, nowMs);
    updateNine(threeAxis, threeAxisCapture, false, false, xG, 0.0f, 0.0f,
               Screen::Playing, nowMs);
  }

  ASSERT_EQ(legacyCapture.actions.size(), threeAxisCapture.actions.size());
  for (std::size_t i = 0; i < legacyCapture.actions.size(); ++i) {
    ASSERT_EQ(legacyCapture.actions[i].kind,
              threeAxisCapture.actions[i].kind);
    ASSERT_EQ(legacyCapture.actions[i].tMs,
              threeAxisCapture.actions[i].tMs);
    ASSERT_EQ(legacyCapture.actions[i].seq,
              threeAxisCapture.actions[i].seq);
  }
}

SF_TEST(router_calibrated_reclined_basis_reaches_the_edges) {
  InputRouter router;
  Capture capture;
  Vec3 g0{};
  Vec3 right{};
  Vec3 left{};
  Vec3 toward{};
  const TiltBasis basis = reclinedBasis(g0, right, left, toward);
  const uint32_t readyMs =
      settleNeutralNine(router, capture, 0, g0.x, g0.y, g0.z);
  router.setBasis(basis);

  for (uint32_t nowMs = readyMs + 20; nowMs <= readyMs + 300;
       nowMs += 20) {
    updateNine(router, capture, false, false, right.x, right.y, right.z,
               Screen::Playing, nowMs);
  }
  ASSERT_EQ(router.selectedColumn(), 9);
  ASSERT_TRUE(capture.count(ActionKind::MoveRight) > 0);

  capture.clear();
  for (uint32_t nowMs = readyMs + 340; nowMs <= readyMs + 640;
       nowMs += 40) {
    updateNine(router, capture, false, false, toward.x, toward.y, toward.z,
               Screen::Playing, nowMs);
  }
  ASSERT_EQ(capture.count(ActionKind::SoftDropOn), 1);

  router.setTiltSpeed(0);
  for (uint32_t nowMs = readyMs + 680; nowMs <= readyMs + 980;
       nowMs += 40) {
    updateNine(router, capture, false, false, g0.x, g0.y, g0.z,
               Screen::Playing, nowMs);
  }
  capture.clear();
  for (uint32_t nowMs = readyMs + 1020; nowMs <= readyMs + 1320;
       nowMs += 40) {
    updateNine(router, capture, false, false, right.x, right.y, right.z,
               Screen::Playing, nowMs);
  }
  ASSERT_TRUE(router.selectedColumn() == 6 || router.selectedColumn() == 7);
}

SF_TEST(router_uncalibrated_reclined_dip_needs_more_pitch) {
  InputRouter router;
  Capture capture;
  const Vec3 g0{0.519f, 0.423f, 0.743f};
  const float angle = 25.0f * kPi / 180.0f;
  const Vec3 toward{g0.x, g0.y, g0.z - g0.x * std::sin(angle)};
  const uint32_t readyMs =
      settleNeutralNine(router, capture, 0, g0.x, g0.y, g0.z);

  for (uint32_t nowMs = readyMs + 40; nowMs <= readyMs + 300;
       nowMs += 40) {
    updateNine(router, capture, false, false, toward.x, toward.y, toward.z,
               Screen::Playing, nowMs);
  }
  ASSERT_EQ(capture.count(ActionKind::SoftDropOn), 0);
}

SF_TEST(router_small_calibration_dip_engages_at_sixty_percent) {
  InputRouter router;
  Capture capture;
  const Vec3 g0{-0.147f, 0.043f, 0.993f};
  const Vec3 fRaw{0.941f, -0.053f, 0.335f};
  const Vec3 sRaw{-0.107f, -0.983f, 0.146f};
  TiltBasis basis = sf::input::identityBasis();
  basis.g0 = g0;
  basis.s = scale(sRaw, 1.0f / norm(sRaw));
  basis.f = scale(fRaw, 1.0f / norm(fRaw));
  basis.steerScaleG = 0.308f;
  basis.dipScaleG = 0.225f;
  basis.steerAsymmetryG = -0.198f;
  basis.calibrated = true;

  const uint32_t readyMs = settleNeutralNine(router, capture, 0, g0.x, g0.y,
                                             g0.z);
  router.setBasis(basis);
  constexpr float kPi = 3.14159265358979323846f;
  const Vec3 pitch4 = sf::input::sub(
      g0, scale(basis.f, std::sin(4.0f * kPi / 180.0f)));
  const Vec3 pitch10 = sf::input::sub(
      g0, scale(basis.f, std::sin(10.0f * kPi / 180.0f)));

  for (uint32_t nowMs = readyMs + 20; nowMs <= readyMs + 300;
       nowMs += 20) {
    updateNine(router, capture, false, false, pitch4.x, pitch4.y, pitch4.z,
               Screen::Playing, nowMs);
  }
  ASSERT_EQ(capture.count(ActionKind::SoftDropOn), 0);
  ASSERT_FALSE(router.dipEngaged());

  capture.clear();
  for (uint32_t nowMs = readyMs + 340; nowMs <= readyMs + 640;
       nowMs += 20) {
    updateNine(router, capture, false, false, pitch10.x, pitch10.y, pitch10.z,
               Screen::Playing, nowMs);
  }
  ASSERT_EQ(capture.count(ActionKind::SoftDropOn), 1);
}
