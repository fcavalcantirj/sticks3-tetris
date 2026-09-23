#pragma once

#include "control_profile.h"
#include "stackfall/input/action.h"

#include <cstdint>

namespace sf::input {

// This state machine deliberately consumes only the two isPressed() booleans
// sampled by the HAL and a caller-supplied timestamp. M5Unified Button_Class's
// hold-threshold setter and its hold, single-click, and double-click consuming
// queries are forbidden: those helpers can consume the press before our hold
// rule observes it. No clock or hardware API belongs in this pure input layer.
class ButtonGesture {
 public:
  void setRotateCcw(bool ccw) { rotateCcw_ = ccw; }
  void setProfile(ControlProfile profile) { profile_ = profile; }

  void update(bool bluePressed, bool sidePressed, uint32_t nowMs, ActionQueue& out) {
    const bool bluePressEdge = bluePressed && !blue_.down;
    const bool sidePressEdge = sidePressed && !side_.down;

    // Blue acts on its press edge with no deliberate delay. Do not move this
    // rotation behind the hold decision window: when the gesture becomes Hold,
    // the swapped-in piece respawns in spawn orientation and absorbs it.
    if (bluePressEdge) {
      beginPress(blue_, nowMs);
      out.push(rotateCcw_ ? ActionKind::RotateCcw : ActionKind::RotateCw, nowMs);
    }

    if (sidePressEdge) {
      beginPress(side_, nowMs);
      sidePressWasModifier_ = blue_.down;
      if (!sidePressWasModifier_) {
        if (profile_ == ControlProfile::BUTTONS_ONLY) {
          out.push(ActionKind::MoveRight, nowMs);
          side_.nextRepeatMs = nowMs + kSideDasMs;
        } else {
          out.push(ActionKind::HardDrop, nowMs);
        }
      }
      // When blue is already down, nextRepeatMs deliberately remains equal to
      // downAtMs. That marks the side press as a pause modifier which owes no
      // side-key action if the chord is aborted.
    }

    const bool bothPressed = bluePressed && sidePressed && blue_.down && side_.down;

    // If a per-key threshold passes while the chord owns both keys, consume the
    // threshold without emitting it. This prevents releasing one key later from
    // turning an aborted chord into a delayed Hold or HardDrop. Scheduled right
    // repeats are advanced, not accumulated into a burst after the suspension.
    if (bothPressed) {
      if (!blue_.holdFired && elapsed(nowMs, blue_.downAtMs, kHoldMs)) {
        blue_.holdFired = true;
      }
      if (profile_ == ControlProfile::BUTTONS_ONLY && sideTimerArmed()) {
        skipDueSideRepeats(nowMs);
        if (!side_.holdFired && elapsed(nowMs, side_.downAtMs, kSideWrapDropMs)) {
          side_.holdFired = true;
        }
      }
    }

    if (bluePressed && blue_.down && !sidePressed && !blue_.holdFired &&
        elapsed(nowMs, blue_.downAtMs, kHoldMs)) {
      out.push(ActionKind::Hold, nowMs);
      blue_.holdFired = true;
    }

    // BUTTONS_ONLY repeats advance from their scheduled deadline, not from the
    // poll that notices it. Both-key ownership suspends every side timer.
    if (profile_ == ControlProfile::BUTTONS_ONLY && sidePressed && side_.down &&
        !bluePressed && sideTimerArmed()) {
      while (deadlineReached(nowMs, side_.nextRepeatMs)) {
        out.push(ActionKind::MoveRight, nowMs);
        side_.nextRepeatMs += kSideArrMs;
      }
      if (!side_.holdFired && elapsed(nowMs, side_.downAtMs, kSideWrapDropMs)) {
        out.push(ActionKind::HardDrop, nowMs);
        side_.holdFired = true;
      }
    }

    if (bothPressed && !chordFired_) {
      const uint32_t laterEdge = laterPressMs();
      if (nowMs >= laterEdge && nowMs - laterEdge >= kChordMs) {
        out.push(ActionKind::Pause, nowMs);
        chordFired_ = true;
        blue_.holdFired = true;
        side_.holdFired = true;
        side_.nextRepeatMs = side_.downAtMs;
      }
    }

    if (!bluePressed && blue_.down) {
      blue_ = Btn{};
    }
    if (!sidePressed && side_.down) {
      side_ = Btn{};
      sidePressWasModifier_ = false;
    }
    if (!blue_.down && !side_.down) {
      chordFired_ = false;
    }
  }

  void reset() {
    blue_ = Btn{};
    side_ = Btn{};
    chordFired_ = false;
    sidePressWasModifier_ = false;
  }

  uint32_t chordHeldMs(uint32_t nowMs) const {
    if (!blue_.down || !side_.down) {
      return 0;
    }
    const uint32_t laterEdge = laterPressMs();
    if (nowMs < laterEdge) {
      return 0;
    }
    return nowMs - laterEdge;
  }

  uint32_t sideHeldMs(uint32_t nowMs) const {
    return side_.down ? nowMs - side_.downAtMs : 0u;
  }

  // A SIDE press that began while BLUE was already down belongs to the pause
  // modifier until SIDE is released, even if BLUE is released first.
  bool sideHoldEligible() const {
    return side_.down && !sidePressWasModifier_;
  }

 private:
  struct Btn {
    bool down = false;
    uint32_t downAtMs = 0;
    bool holdFired = false;
    uint32_t nextRepeatMs = 0;
  };

  static constexpr uint32_t kHoldMs = 900;
  static constexpr uint32_t kChordMs = 800;
  static constexpr uint32_t kSideWrapDropMs = 900;
  static constexpr uint32_t kSideDasMs = 167;
  static constexpr uint32_t kSideArrMs = 33;

  static void beginPress(Btn& button, uint32_t nowMs) {
    button.down = true;
    button.downAtMs = nowMs;
    button.holdFired = false;
    button.nextRepeatMs = nowMs;
  }

  static bool elapsed(uint32_t nowMs, uint32_t sinceMs, uint32_t durationMs) {
    return nowMs - sinceMs >= durationMs;
  }

  static bool deadlineReached(uint32_t nowMs, uint32_t deadlineMs) {
    return static_cast<int32_t>(nowMs - deadlineMs) >= 0;
  }

  void skipDueSideRepeats(uint32_t nowMs) {
    if (!deadlineReached(nowMs, side_.nextRepeatMs)) {
      return;
    }
    const uint32_t due = (nowMs - side_.nextRepeatMs) / kSideArrMs + 1u;
    side_.nextRepeatMs += due * kSideArrMs;
  }

  bool sideTimerArmed() const { return side_.nextRepeatMs != side_.downAtMs; }

  uint32_t laterPressMs() const {
    return blue_.downAtMs > side_.downAtMs ? blue_.downAtMs : side_.downAtMs;
  }

  Btn blue_;
  Btn side_;
  bool chordFired_ = false;
  bool sidePressWasModifier_ = false;
  bool rotateCcw_ = false;
  ControlProfile profile_ = ControlProfile::TILT_DIP;
};

}  // namespace sf::input
