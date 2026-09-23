#pragma once

#include "stackfall/input/tilt.h"
#include "stackfall/input/dip.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace sf::input {

struct Vec3 {
  float x;
  float y;
  float z;
};

struct TiltBasis {
  Vec3 g0;
  Vec3 s;
  Vec3 f;
  float steerScaleG;
  float steerScaleRightG;
  float steerScaleLeftG;
  float dipScaleG;
  float steerAsymmetryG;
  bool calibrated;
};

// [UNVERIFIED] Five times the 0.040 G still-window span; replace this with the
// first calibration logs when the interactive calibration task measures it.
inline constexpr float kMinCalibrationDeflectionG = 0.20f;

// The upright 45 degree dip reference. Calibration may attenuate this
// reference for a larger comfortable dip, but it never amplifies a smaller
// one, so the detector's 0.25/0.15 G band keeps its upright feel.
inline constexpr float kDipReferenceG = 0.707f;

// Limit steering calibration to a two-times gain so a small measured range
// cannot make ordinary hand noise select a distant column.
inline constexpr float kSteerMaxGain = 2.0f;

// Engage at a fraction of the player's comfortable dip so the gesture has
// room before the detector's fixed 0.25 G cap. The floor rejects hand noise.
inline constexpr float kDipEngageFraction = 0.6f;
inline constexpr float kDipEngageFloorG = 0.12f;
static_assert(kDipEngageG == 0.25f,
              "basis dip band must match the detector engage threshold");

// Identity keeps the shipping upright geometry byte-for-byte compatible. A
// toward dip makes raw Z decrease, so f is +Z: (0, 0, -d) dot (0, 0, 1) is
// -d. The dip detector's kDipSignAway is +1, hence a negative projection is
// the existing toward signal.
constexpr TiltBasis identityBasis() {
  return TiltBasis{{0.0f, 0.0f, 0.0f},
                     {0.0f, static_cast<float>(kTiltSignLeftRight), 0.0f},
                     {0.0f, 0.0f, 1.0f},
                     4.0f * kGPerColumn,
                     4.0f * kGPerColumn,
                     4.0f * kGPerColumn,
                     kDipReferenceG,
                     0.0f,
                     false};
}

inline constexpr Vec3 sub(const Vec3& a, const Vec3& b) {
  return Vec3{a.x - b.x, a.y - b.y, a.z - b.z};
}

inline constexpr Vec3 operator+(const Vec3& a, const Vec3& b) {
  return Vec3{a.x + b.x, a.y + b.y, a.z + b.z};
}

inline constexpr Vec3 scale(const Vec3& value, float factor) {
  return Vec3{value.x * factor, value.y * factor, value.z * factor};
}

inline constexpr float dot(const Vec3& a, const Vec3& b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline float norm(const Vec3& value) {
  return std::sqrt(dot(value, value));
}

inline bool deriveSteer(const Vec3& g0, const Vec3& gRight,
                        const Vec3& gLeft, TiltBasis& out) {
  const Vec3 raw = sub(gRight, gLeft);
  const float length = norm(raw);
  if (length <= 0.0f) return false;
  const Vec3 direction = scale(raw, 1.0f / length);
  const float rightReach = dot(sub(gRight, g0), direction);
  const float leftReach = dot(sub(g0, gLeft), direction);
  if (rightReach < kMinCalibrationDeflectionG ||
      leftReach < kMinCalibrationDeflectionG) {
    return false;
  }
  out.g0 = g0;
  out.s = direction;
  out.steerScaleG = std::min(rightReach, leftReach);
  out.steerScaleRightG = rightReach;
  out.steerScaleLeftG = leftReach;
  out.steerAsymmetryG = rightReach - leftReach;
  out.calibrated = true;
  return true;
}

inline bool deriveDip(const Vec3& gToward, TiltBasis& inout) {
  const Vec3 delta = sub(gToward, inout.g0);
  const Vec3 raw = sub(delta, scale(inout.s, dot(delta, inout.s)));
  const float length = norm(raw);
  if (length < kMinCalibrationDeflectionG) return false;
  Vec3 direction = scale(raw, 1.0f / length);
  if (dot(delta, direction) > 0.0f) direction = scale(direction, -1.0f);
  inout.f = direction;
  inout.dipScaleG = length;
  inout.calibrated = true;
  return true;
}

inline float projectSteer(const TiltBasis& basis, const Vec3& sample) {
  return dot(sub(sample, basis.g0), basis.s);
}
inline float projectDip(const TiltBasis& basis, const Vec3& sample) {
  return dot(sub(sample, basis.g0), basis.f);
}

// A full calibrated deflection reaches the existing edge plus two hysteresis
// bands: 0.36 G + 2 * 0.02 G = 0.40 G. This gives the unchanged Tilt filter
// enough margin to select both endpoint columns without snapping the signal.
inline constexpr float kSteerReachG =
    4.0f * kGPerColumn + 2.0f * kColHysteresis;

inline float effectiveSteerFactorForScale(const TiltBasis& basis,
                                          float scaleG) {
  const float reachG = basis.calibrated ? kSteerReachG
                                        : 4.0f * kGPerColumn;
  if (scaleG <= 0.0f) return 0.0f;
  const float factor = reachG / scaleG;
  return factor > kSteerMaxGain ? kSteerMaxGain : factor;
}

inline float effectiveSteerFactorRight(const TiltBasis& basis) {
  return effectiveSteerFactorForScale(basis, basis.steerScaleRightG);
}

inline float effectiveSteerFactorLeft(const TiltBasis& basis) {
  return effectiveSteerFactorForScale(basis, basis.steerScaleLeftG);
}

// Compatibility telemetry keeps the historical single factor as the right
// side; steering itself selects the factor for the sign being played.
inline float effectiveSteerFactor(const TiltBasis& basis) {
  return effectiveSteerFactorRight(basis);
}

inline float normalisedSteer(const TiltBasis& basis, const Vec3& sample,
                             float gain) {
  const float projected = projectSteer(basis, sample);
  const float factor = projected >= 0.0f ? effectiveSteerFactorRight(basis)
                                        : effectiveSteerFactorLeft(basis);
  return projected * factor * gain;
}

inline float effectiveDipFactor(const TiltBasis& basis) {
  if (basis.dipScaleG <= 0.0f) return 0.0f;
  const float engageRaw = std::clamp(kDipEngageFraction * basis.dipScaleG,
                                     kDipEngageFloorG, kDipEngageG);
  return kDipEngageG / engageRaw;
}

inline float normalisedDip(const TiltBasis& basis, const Vec3& sample) {
  return projectDip(basis, sample) * effectiveDipFactor(basis);
}

}  // namespace sf::input
