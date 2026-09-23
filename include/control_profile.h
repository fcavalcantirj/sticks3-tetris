#pragma once
#include <cstdint>

// Control profile decision recorded 2026-09-08.
// Source: owner's hands-on testing recorded in docs/TILT-LOG.md ROUND 3.
// Locked map: tilt left/right moves the piece; dip the front down (IR away) rotates CW;
// dip toward yourself soft-drops while held; left/right is frozen while either dip is engaged.

namespace sf {
enum class ControlProfile : uint8_t { TILT_DIP = 0, BUTTONS_ONLY = 1 };
constexpr ControlProfile kActiveControlProfile = ControlProfile::TILT_DIP;
constexpr bool kEnableInternalImu = true;
}
