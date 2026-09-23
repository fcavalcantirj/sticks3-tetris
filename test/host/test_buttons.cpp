#include "framework.h"
#include "stackfall/input/buttons.h"

#include <cstddef>
#include <cstdint>
#include <vector>

using sf::ControlProfile;
using sf::input::ActionKind;
using sf::input::ActionQueue;
using sf::input::ButtonGesture;
using sf::input::GameAction;

namespace {

struct Capture {
  void update(ButtonGesture& buttons, bool blue, bool side, uint32_t nowMs) {
    buttons.update(blue, side, nowMs, queue);
    GameAction action{};
    while (queue.pop(action)) {
      actions.push_back(action);
    }
  }

  int count(ActionKind kind) const {
    int found = 0;
    for (const GameAction& action : actions) {
      if (action.kind == kind) {
        ++found;
      }
    }
    return found;
  }

  ActionQueue queue;
  std::vector<GameAction> actions;
};

void assertAction(const std::vector<GameAction>& actions, std::size_t index, ActionKind kind,
                  uint32_t atMs) {
  ASSERT_TRUE(index < actions.size());
  ASSERT_EQ(actions[index].kind, kind);
  ASSERT_EQ(actions[index].tMs, atMs);
}

void driveBlueHold(ButtonGesture& buttons, Capture& capture, uint32_t downAt, uint32_t upAt) {
  for (uint32_t nowMs = downAt; nowMs < upAt; nowMs += 5) {
    capture.update(buttons, true, false, nowMs);
  }
  capture.update(buttons, false, false, upAt);
}

}  // namespace

SF_TEST(button_short_blue_press_is_immediate) {
  ButtonGesture buttons;
  Capture capture;

  for (uint32_t nowMs = 10; nowMs < 60; nowMs += 5) {
    capture.update(buttons, true, false, nowMs);
  }
  capture.update(buttons, false, false, 60);

  ASSERT_EQ(capture.actions.size(), 1u);
  assertAction(capture.actions, 0, ActionKind::RotateCw, 10);
}

SF_TEST(button_blue_hold_boundary_and_idempotence) {
  ButtonGesture buttons;
  Capture capture;
  driveBlueHold(buttons, capture, 100, 1100);

  ASSERT_EQ(capture.actions.size(), 2u);
  assertAction(capture.actions, 0, ActionKind::RotateCw, 100);
  assertAction(capture.actions, 1, ActionKind::Hold, 1000);

  ButtonGesture boundary;
  ActionQueue queue;
  boundary.update(true, false, 0, queue);
  boundary.update(true, false, 899, queue);
  ASSERT_EQ(queue.size(), 1u);
  boundary.update(true, false, 900, queue);
  ASSERT_EQ(queue.size(), 2u);

  GameAction action{};
  ASSERT_TRUE(queue.pop(action));
  ASSERT_EQ(action.kind, ActionKind::RotateCw);
  ASSERT_TRUE(queue.pop(action));
  ASSERT_EQ(action.kind, ActionKind::Hold);
  ASSERT_EQ(action.tMs, 900u);

  for (int i = 0; i < 100; ++i) {
    boundary.update(true, false, 1300, queue);
  }
  ASSERT_EQ(queue.size(), 0u);
}

SF_TEST(button_blue_hold_requires_900ms) {
  ButtonGesture releasedAt700;
  Capture shortCapture;
  driveBlueHold(releasedAt700, shortCapture, 0, 700);
  ASSERT_EQ(shortCapture.actions.size(), 1u);
  assertAction(shortCapture.actions, 0, ActionKind::RotateCw, 0);
  ASSERT_EQ(shortCapture.count(ActionKind::Hold), 0);

  ButtonGesture heldTo950;
  Capture longCapture;
  driveBlueHold(heldTo950, longCapture, 0, 950);
  ASSERT_EQ(longCapture.actions.size(), 2u);
  assertAction(longCapture.actions, 0, ActionKind::RotateCw, 0);
  assertAction(longCapture.actions, 1, ActionKind::Hold, 900);
}

SF_TEST(button_tilt_dip_side_press_hard_drops_once) {
  ButtonGesture buttons;
  Capture capture;

  for (uint32_t nowMs = 0; nowMs <= 2000; nowMs += 5) {
    capture.update(buttons, false, true, nowMs);
  }
  capture.update(buttons, false, false, 2005);

  ASSERT_EQ(capture.actions.size(), 1u);
  assertAction(capture.actions, 0, ActionKind::HardDrop, 0);
}

SF_TEST(button_taught_pause_and_aborted_pause_are_non_destructive) {
  ButtonGesture taught;
  Capture complete;
  for (uint32_t nowMs = 0; nowMs < 100; nowMs += 5) {
    complete.update(taught, true, false, nowMs);
  }
  for (uint32_t nowMs = 100; nowMs <= 1500; nowMs += 5) {
    complete.update(taught, true, true, nowMs);
  }
  complete.update(taught, false, false, 1505);

  ASSERT_EQ(complete.actions.size(), 2u);
  assertAction(complete.actions, 0, ActionKind::RotateCw, 0);
  assertAction(complete.actions, 1, ActionKind::Pause, 900);
  ASSERT_EQ(complete.count(ActionKind::HardDrop), 0);
  ASSERT_EQ(complete.count(ActionKind::Hold), 0);

  ButtonGesture aborted;
  Capture partial;
  for (uint32_t nowMs = 0; nowMs < 100; nowMs += 5) {
    partial.update(aborted, true, false, nowMs);
  }
  for (uint32_t nowMs = 100; nowMs < 700; nowMs += 5) {
    partial.update(aborted, true, true, nowMs);
  }
  partial.update(aborted, false, false, 700);

  ASSERT_EQ(partial.actions.size(), 1u);
  assertAction(partial.actions, 0, ActionKind::RotateCw, 0);
}

SF_TEST(button_aborted_modifier_never_releases_deferred_actions) {
  ButtonGesture keepBlue;
  Capture noLateHold;
  for (uint32_t nowMs = 0; nowMs < 200; nowMs += 5) {
    noLateHold.update(keepBlue, true, false, nowMs);
  }
  for (uint32_t nowMs = 200; nowMs < 950; nowMs += 5) {
    noLateHold.update(keepBlue, true, true, nowMs);
  }
  for (uint32_t nowMs = 950; nowMs <= 1200; nowMs += 5) {
    noLateHold.update(keepBlue, true, false, nowMs);
  }
  noLateHold.update(keepBlue, false, false, 1205);

  ASSERT_EQ(noLateHold.actions.size(), 1u);
  assertAction(noLateHold.actions, 0, ActionKind::RotateCw, 0);

  ButtonGesture keepSide;
  keepSide.setProfile(ControlProfile::BUTTONS_ONLY);
  Capture noLateSideAction;
  for (uint32_t nowMs = 0; nowMs < 200; nowMs += 5) {
    noLateSideAction.update(keepSide, true, false, nowMs);
  }
  for (uint32_t nowMs = 200; nowMs < 950; nowMs += 5) {
    noLateSideAction.update(keepSide, true, true, nowMs);
  }
  for (uint32_t nowMs = 950; nowMs <= 1200; nowMs += 5) {
    noLateSideAction.update(keepSide, false, true, nowMs);
  }
  noLateSideAction.update(keepSide, false, false, 1205);

  ASSERT_EQ(noLateSideAction.actions.size(), 1u);
  assertAction(noLateSideAction.actions, 0, ActionKind::RotateCw, 0);
}

SF_TEST(button_pause_order_asymmetry_and_dawdled_hold_are_explicit) {
  ButtonGesture sideFirst;
  Capture asymmetric;
  for (uint32_t nowMs = 0; nowMs < 100; nowMs += 5) {
    asymmetric.update(sideFirst, false, true, nowMs);
  }
  for (uint32_t nowMs = 100; nowMs <= 1500; nowMs += 5) {
    asymmetric.update(sideFirst, true, true, nowMs);
  }
  asymmetric.update(sideFirst, false, false, 1505);

  ASSERT_EQ(asymmetric.actions.size(), 3u);
  assertAction(asymmetric.actions, 0, ActionKind::HardDrop, 0);
  assertAction(asymmetric.actions, 1, ActionKind::RotateCw, 100);
  assertAction(asymmetric.actions, 2, ActionKind::Pause, 900);

  ButtonGesture dawdled;
  Capture delayed;
  for (uint32_t nowMs = 0; nowMs < 1000; nowMs += 5) {
    delayed.update(dawdled, true, false, nowMs);
  }
  for (uint32_t nowMs = 1000; nowMs <= 2000; nowMs += 5) {
    delayed.update(dawdled, true, true, nowMs);
  }
  delayed.update(dawdled, false, false, 2005);

  ASSERT_EQ(delayed.actions.size(), 3u);
  assertAction(delayed.actions, 0, ActionKind::RotateCw, 0);
  assertAction(delayed.actions, 1, ActionKind::Hold, 900);
  assertAction(delayed.actions, 2, ActionKind::Pause, 1800);
}

SF_TEST(button_release_rearms_and_rotation_setting_flips_blue) {
  ButtonGesture buttons;
  Capture capture;

  capture.update(buttons, true, false, 0);
  capture.update(buttons, false, false, 10);
  capture.update(buttons, true, false, 20);
  capture.update(buttons, false, false, 30);
  buttons.setRotateCcw(true);
  capture.update(buttons, true, false, 40);

  ASSERT_EQ(capture.actions.size(), 3u);
  assertAction(capture.actions, 0, ActionKind::RotateCw, 0);
  assertAction(capture.actions, 1, ActionKind::RotateCw, 20);
  assertAction(capture.actions, 2, ActionKind::RotateCcw, 40);

  buttons.reset();
  capture.update(buttons, true, false, 50);
  ASSERT_EQ(capture.actions.size(), 4u);
  assertAction(capture.actions, 3, ActionKind::RotateCcw, 50);
}

SF_TEST(button_buttons_only_repeat_and_hard_drop_schedule) {
  ButtonGesture shortPress;
  shortPress.setProfile(ControlProfile::BUTTONS_ONLY);
  Capture shortCapture;
  for (uint32_t nowMs = 10; nowMs < 60; nowMs += 5) {
    shortCapture.update(shortPress, false, true, nowMs);
  }
  shortCapture.update(shortPress, false, false, 60);

  ASSERT_EQ(shortCapture.actions.size(), 1u);
  assertAction(shortCapture.actions, 0, ActionKind::MoveRight, 10);

  ButtonGesture held;
  held.setProfile(ControlProfile::BUTTONS_ONLY);
  Capture capture;
  for (uint32_t nowMs = 0; nowMs <= 1000; nowMs += 5) {
    capture.update(held, false, true, nowMs);
  }
  const std::size_t beforeSamePoll = capture.actions.size();
  capture.update(held, false, true, 1000);
  ASSERT_EQ(capture.actions.size(), beforeSamePoll);
  capture.update(held, false, false, 1005);

  ASSERT_EQ(capture.actions.size(), 28u);
  ASSERT_EQ(capture.count(ActionKind::MoveRight), 27);
  ASSERT_EQ(capture.count(ActionKind::HardDrop), 1);
  ASSERT_EQ(capture.count(ActionKind::SoftDropOn), 0);
  ASSERT_EQ(capture.count(ActionKind::SoftDropOff), 0);

  std::vector<uint32_t> moveTimes;
  for (const GameAction& action : capture.actions) {
    if (action.kind == ActionKind::MoveRight) {
      moveTimes.push_back(action.tMs);
    } else {
      ASSERT_EQ(action.kind, ActionKind::HardDrop);
      ASSERT_EQ(action.tMs, 900u);
    }
  }
  ASSERT_EQ(moveTimes.size(), 27u);
  ASSERT_EQ(moveTimes[0], 0u);
  for (uint32_t repeat = 0; repeat < 26; ++repeat) {
    const uint32_t scheduled = 167u + 33u * repeat;
    const uint32_t firstFiveMsPoll = ((scheduled + 4u) / 5u) * 5u;
    ASSERT_EQ(moveTimes[repeat + 1u], firstFiveMsPoll);
  }
}

SF_TEST(button_pause_modifier_suspends_buttons_only_side_actions) {
  ButtonGesture buttons;
  buttons.setProfile(ControlProfile::BUTTONS_ONLY);
  Capture capture;

  for (uint32_t nowMs = 0; nowMs < 100; nowMs += 5) {
    capture.update(buttons, true, false, nowMs);
  }
  for (uint32_t nowMs = 100; nowMs <= 1500; nowMs += 5) {
    capture.update(buttons, true, true, nowMs);
  }
  capture.update(buttons, false, false, 1505);

  ASSERT_EQ(capture.actions.size(), 2u);
  assertAction(capture.actions, 0, ActionKind::RotateCw, 0);
  assertAction(capture.actions, 1, ActionKind::Pause, 900);
  ASSERT_EQ(capture.count(ActionKind::MoveRight), 0);
  ASSERT_EQ(capture.count(ActionKind::HardDrop), 0);
  ASSERT_EQ(capture.count(ActionKind::SoftDropOn), 0);
}

SF_TEST(button_chord_held_ms_is_read_only_and_restarts) {
  ButtonGesture buttons;
  ActionQueue queue;

  auto assertRead = [&](uint32_t nowMs, uint32_t expected) {
    const uint8_t sizeBefore = queue.size();
    ASSERT_EQ(buttons.chordHeldMs(nowMs), expected);
    ASSERT_EQ(queue.size(), sizeBefore);
  };

  buttons.update(false, false, 0, queue);
  assertRead(0, 0);
  buttons.update(true, false, 300, queue);
  assertRead(300, 0);

  buttons.reset();
  queue.clear();
  buttons.update(false, true, 300, queue);
  assertRead(300, 0);

  buttons.reset();
  queue.clear();
  buttons.update(true, false, 0, queue);
  buttons.update(true, true, 100, queue);
  assertRead(100, 0);
  buttons.update(true, true, 800, queue);
  assertRead(800, 700);
  buttons.update(true, true, 900, queue);
  assertRead(900, 800);
  assertRead(2000, 1900);
  assertRead(2500, 2400);

  buttons.update(true, false, 2500, queue);
  assertRead(2500, 0);
  buttons.update(true, true, 2600, queue);
  assertRead(2600, 0);
  assertRead(2700, 100);
}
