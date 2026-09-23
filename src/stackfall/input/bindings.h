#pragma once

#include "control_profile.h"
#include "stackfall/app/app.h"
#include "stackfall/input/action.h"

#include <cstddef>
#include <cstdint>
#include <iterator>

namespace sf::input {

// This is the project's single vocabulary for turning physical gestures into
// actions and for describing those gestures to the player. TiltForward is the
// ledger's retained name for the toward-face dip; DipAway records the second,
// opposite direction required by the final signed-Z control map.
enum class Gesture : uint8_t {
  BluePress,
  BlueHold,
  SidePress,
  SideHold,
  Chord,
  TiltLeft,
  TiltRight,
  TiltForward,
  DipAway,
  BlueTaps,
};

struct Binding {
  Screen screen;
  Gesture gesture;
  ActionKind action;
  const char* gestureLabel;
  const char* actionLabel;
};

inline constexpr Binding kBindings[] = {
    {Screen::Playing, Gesture::BluePress, ActionKind::RotateCw,
     "BLUE PRESS", "ROTATE"},
    {Screen::Playing, Gesture::BlueHold, ActionKind::Hold,
     "BLUE HOLD", "HOLD/SWAP"},
    {Screen::Playing, Gesture::SidePress, ActionKind::HardDrop,
     "SIDE PRESS", "HARD DROP"},
    // Re-zero is router-local rather than an App action, so this binding is
    // descriptive and ActionKind::None is never placed in the action queue.
    {Screen::Playing, Gesture::SideHold, ActionKind::None,
     "SIDE HOLD", "RE-ZERO"},
    // Re-calibration is also router-local. The four ordinary press actions are
    // retained, and the fourth edge opens the calibration screen.
    {Screen::Playing, Gesture::BlueTaps, ActionKind::Calibrate,
     "BLUE x4", "CALIBRATE"},
    {Screen::Playing, Gesture::Chord, ActionKind::Pause,
     "BLUE 1ST+SIDE", "PAUSE"},
    {Screen::Playing, Gesture::TiltLeft, ActionKind::MoveLeft,
     "TILT LEFT", "MOVE LEFT"},
    {Screen::Playing, Gesture::TiltRight, ActionKind::MoveRight,
     "TILT RIGHT", "MOVE RIGHT"},
    {Screen::Playing, Gesture::DipAway, ActionKind::RotateCw,
     "DIP AWAY", "ROTATE"},
    {Screen::Playing, Gesture::TiltForward, ActionKind::SoftDropOn,
     "DIP TOWARD", "SOFT DROP"},

    {Screen::Title, Gesture::BluePress, ActionKind::Confirm,
     "BLUE PRESS", "SELECT"},
    {Screen::Title, Gesture::SidePress, ActionKind::MenuDown,
     "SIDE PRESS", "NEXT"},
    {Screen::Title, Gesture::Chord, ActionKind::Diag,
     "BLUE 1ST+SIDE", "DIAGNOSTICS"},

    {Screen::Paused, Gesture::BluePress, ActionKind::Confirm,
     "BLUE PRESS", "SELECT"},
    {Screen::Paused, Gesture::SidePress, ActionKind::MenuDown,
     "SIDE PRESS", "NEXT"},
    {Screen::Paused, Gesture::Chord, ActionKind::Back,
     "BLUE 1ST+SIDE", "BACK"},

    {Screen::GameOver, Gesture::BluePress, ActionKind::Confirm,
     "BLUE PRESS", "SELECT"},
    {Screen::GameOver, Gesture::SidePress, ActionKind::MenuDown,
     "SIDE PRESS", "NEXT"},
    {Screen::GameOver, Gesture::Chord, ActionKind::Back,
     "BLUE 1ST+SIDE", "BACK"},

    {Screen::HighScores, Gesture::BluePress, ActionKind::Confirm,
     "BLUE PRESS", "CONTINUE"},
    {Screen::HighScores, Gesture::SidePress, ActionKind::MenuDown,
     "SIDE PRESS", "NEXT"},
    {Screen::HighScores, Gesture::Chord, ActionKind::Back,
     "BLUE 1ST+SIDE", "BACK"},

    {Screen::Settings, Gesture::BluePress, ActionKind::Confirm,
     "BLUE PRESS", "CHANGE"},
    {Screen::Settings, Gesture::SidePress, ActionKind::MenuDown,
     "SIDE PRESS", "NEXT"},
    {Screen::Settings, Gesture::Chord, ActionKind::Back,
     "BLUE 1ST+SIDE", "BACK"},

    {Screen::Instructions, Gesture::BluePress, ActionKind::Confirm,
     "BLUE PRESS", "CONTINUE"},
    {Screen::Instructions, Gesture::SidePress, ActionKind::MenuDown,
     "SIDE PRESS", "NEXT"},
    {Screen::Instructions, Gesture::Chord, ActionKind::Back,
     "BLUE 1ST+SIDE", "BACK"},

    // Diagnostics deliberately replaces the generic blue Confirm binding with
    // Back, so each (screen, gesture) pair remains unique.
    {Screen::Diagnostics, Gesture::BluePress, ActionKind::Back,
     "BLUE PRESS", "BACK"},
    {Screen::Diagnostics, Gesture::SidePress, ActionKind::MenuDown,
     "SIDE PRESS", "NEXT"},
    {Screen::Diagnostics, Gesture::Chord, ActionKind::Back,
     "BLUE 1ST+SIDE", "BACK"},

    {Screen::Calibrating, Gesture::BluePress, ActionKind::None,
     "BLUE PRESS", "NEXT"},
    {Screen::Calibrating, Gesture::SidePress, ActionKind::TiltSpeed,
     "SIDE PRESS", "SPEED"},
    {Screen::Calibrating, Gesture::Chord, ActionKind::Back,
     "BLUE 1ST+SIDE", "BACK"},
};

inline constexpr std::size_t kBindingCount = std::size(kBindings);
inline constexpr std::size_t kMaxBindingsPerScreen = 10;

constexpr const Binding* findBinding(Screen screen, Gesture gesture) {
  for (const Binding& binding : kBindings) {
    if (binding.screen == screen && binding.gesture == gesture) {
      return &binding;
    }
  }
  return nullptr;
}

// ButtonGesture reports semantic actions because it predates screen routing.
// Translate those outputs back to their physical gesture before looking up the
// current screen. The temporary queue given to ButtonGesture contains no IMU
// actions, so MoveRight here unambiguously means the BUTTONS_ONLY side key.
constexpr bool buttonGestureFor(ActionKind emitted, Gesture& gesture) {
  switch (emitted) {
    case ActionKind::RotateCw:
    case ActionKind::RotateCcw:
      gesture = Gesture::BluePress;
      return true;
    case ActionKind::Hold:
      gesture = Gesture::BlueHold;
      return true;
    case ActionKind::MoveRight:
    case ActionKind::HardDrop:
      gesture = Gesture::SidePress;
      return true;
    case ActionKind::Pause:
      gesture = Gesture::Chord;
      return true;
    default:
      return false;
  }
}

constexpr bool dipGestureFor(ActionKind emitted, Gesture& gesture) {
  switch (emitted) {
    case ActionKind::RotateCw:
      gesture = Gesture::DipAway;
      return true;
    case ActionKind::SoftDropOn:
    case ActionKind::SoftDropOff:
      gesture = Gesture::TiltForward;
      return true;
    default:
      return false;
  }
}

// The table describes the default TILT_DIP press action. Two runtime settings
// deliberately refine it without duplicating labels: BUTTONS_ONLY preserves
// the side machine's MoveRight/repeat/held HardDrop grammar, and the rotation
// direction setting flips both physical rotate gestures. SoftDropOff is the
// release half of the table's toward-dip SoftDropOn binding.
constexpr ActionKind resolveBindingAction(const Binding& binding,
                                          ActionKind emitted,
                                          ControlProfile profile,
                                          bool rotateCcw) {
  if (binding.screen == Screen::Playing &&
      binding.gesture == Gesture::SidePress &&
      profile == ControlProfile::BUTTONS_ONLY) {
    return emitted;
  }
  // Rotation direction is a gameplay setting; menu bindings are never rewritten.
  if (binding.screen == Screen::Playing && rotateCcw &&
      (binding.gesture == Gesture::BluePress ||
       binding.gesture == Gesture::DipAway)) {
    return ActionKind::RotateCcw;
  }
  if (binding.gesture == Gesture::TiltForward &&
      emitted == ActionKind::SoftDropOff) {
    return ActionKind::SoftDropOff;
  }
  return binding.action;
}

}  // namespace sf::input
