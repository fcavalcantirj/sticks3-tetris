#include "framework.h"
#include "stackfall/input/basis.h"

#include <cmath>

using sf::input::TiltBasis;
using sf::input::Vec3;
using sf::input::deriveDip;
using sf::input::deriveSteer;
using sf::input::dot;
using sf::input::effectiveDipFactor;
using sf::input::effectiveSteerFactor;
using sf::input::effectiveSteerFactorLeft;
using sf::input::effectiveSteerFactorRight;
using sf::input::identityBasis;
using sf::input::kMinCalibrationDeflectionG;
using sf::input::kSteerReachG;
using sf::input::norm;
using sf::input::normalisedDip;
using sf::input::normalisedSteer;
using sf::input::projectDip;
using sf::input::projectSteer;
using sf::input::scale;
using sf::input::sub;

namespace {

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

}  // namespace

SF_TEST(basis_identity_projects_raw_deltas) {
  const TiltBasis basis = identityBasis();
  const Vec3 sample{0.30f, -0.20f, 0.40f};

  ASSERT_NEAR(projectSteer(basis, sample), 0.20f, 1e-5f);
  ASSERT_NEAR(projectDip(basis, sample), 0.40f, 1e-5f);
  ASSERT_NEAR(normalisedSteer(basis, sample, 1.0f), 0.20f, 1e-5f);
  ASSERT_NEAR(normalisedDip(basis, sample), 0.40f, 1e-5f);
}

SF_TEST(basis_steer_scale_is_symmetric) {
  const float angle = 25.0f * kPi / 180.0f;
  const Vec3 g0{1.0f, 0.0f, 0.0f};
  const Vec3 right{std::cos(angle), -std::sin(angle), 0.0f};
  const Vec3 left{std::cos(angle), std::sin(angle), 0.0f};
  TiltBasis basis = identityBasis();

  ASSERT_TRUE(deriveSteer(g0, right, left, basis));
  ASSERT_NEAR(norm(basis.s), 1.0f, 1e-4f);
  ASSERT_NEAR(basis.steerScaleG, std::sin(angle), 1e-3f);
  ASSERT_NEAR(basis.steerAsymmetryG, 0.0f, 1e-4f);
}

SF_TEST(basis_steer_uses_the_smaller_wall_and_reports_asymmetry) {
  const float rightAngle = 25.0f * kPi / 180.0f;
  const float leftAngle = 15.0f * kPi / 180.0f;
  const Vec3 g0{1.0f, 0.0f, 0.0f};
  const Vec3 right{1.0f, -std::sin(rightAngle), 0.0f};
  const Vec3 left{1.0f, std::sin(leftAngle), 0.0f};
  TiltBasis basis = identityBasis();

  ASSERT_TRUE(deriveSteer(g0, right, left, basis));
  ASSERT_NEAR(basis.steerScaleG, std::sin(leftAngle), 1e-3f);
  ASSERT_TRUE(basis.steerAsymmetryG > 0.0f);
}

SF_TEST(basis_dip_is_orthogonal_to_steer) {
  const Vec3 g0{1.0f, 0.0f, 0.0f};
  TiltBasis basis = identityBasis();
  ASSERT_TRUE(deriveSteer(g0, Vec3{1.0f, -0.3f, 0.0f},
                          Vec3{1.0f, 0.3f, 0.0f}, basis));

  ASSERT_TRUE(deriveDip(Vec3{1.0f, 0.0f, 0.5f}, basis));
  ASSERT_NEAR(dot(basis.s, basis.f), 0.0f, 1e-4f);
  ASSERT_NEAR(norm(basis.f), 1.0f, 1e-4f);
  ASSERT_TRUE(projectDip(basis, Vec3{1.0f, 0.0f, 0.5f}) < 0.0f);
}

SF_TEST(basis_projection_round_trip_returns_both_components) {
  const Vec3 g0{1.0f, 0.0f, 0.0f};
  TiltBasis basis = identityBasis();
  ASSERT_TRUE(deriveSteer(g0, Vec3{1.0f, -0.3f, 0.0f},
                          Vec3{1.0f, 0.3f, 0.0f}, basis));
  ASSERT_TRUE(deriveDip(Vec3{1.0f, 0.0f, 0.5f}, basis));

  const float dipRadians = std::asin(0.31f);
  const Vec3 sample{std::cos(dipRadians), 0.17f, std::sin(dipRadians)};
  ASSERT_NEAR(projectSteer(basis, sample), -0.17f, 1e-4f);
  ASSERT_NEAR(projectDip(basis, sample), -0.31f, 1e-4f);
}

SF_TEST(basis_rejects_small_deflections_without_mutating) {
  const Vec3 g0{1.0f, 0.0f, 0.0f};
  TiltBasis basis = identityBasis();
  basis.g0 = Vec3{0.4f, 0.5f, 0.6f};
  basis.s = Vec3{0.1f, 0.2f, 0.3f};
  basis.f = Vec3{0.4f, 0.5f, 0.6f};
  basis.steerScaleG = 0.31f;
  basis.steerScaleRightG = 0.31f;
  basis.steerScaleLeftG = 0.31f;
  basis.dipScaleG = 0.72f;
  basis.steerAsymmetryG = 0.09f;
  basis.calibrated = true;
  basis.f = Vec3{0.4f, 0.5f, 0.6f};
  const TiltBasis before = basis;

  ASSERT_FALSE(deriveSteer(g0, Vec3{1.0f, 0.1f, 0.0f},
                           Vec3{1.0f, -0.1f, 0.0f}, basis));
  ASSERT_NEAR(basis.g0.x, before.g0.x, 1e-6f);
  ASSERT_NEAR(basis.s.y, before.s.y, 1e-6f);
  ASSERT_NEAR(basis.f.z, before.f.z, 1e-6f);
  ASSERT_NEAR(basis.steerScaleG, before.steerScaleG, 1e-6f);
  ASSERT_NEAR(basis.steerScaleRightG, before.steerScaleRightG, 1e-6f);
  ASSERT_NEAR(basis.steerScaleLeftG, before.steerScaleLeftG, 1e-6f);
  ASSERT_NEAR(basis.dipScaleG, before.dipScaleG, 1e-6f);
  ASSERT_NEAR(basis.steerAsymmetryG, before.steerAsymmetryG, 1e-6f);
  ASSERT_EQ(basis.calibrated, before.calibrated);

  ASSERT_FALSE(deriveDip(
      Vec3{before.g0.x, before.g0.y, before.g0.z + 0.1f}, basis));
  ASSERT_NEAR(basis.dipScaleG, before.dipScaleG, 1e-6f);
  ASSERT_EQ(basis.calibrated, before.calibrated);
  ASSERT_TRUE(kMinCalibrationDeflectionG > 0.0f);
}

SF_TEST(basis_signs_match_right_and_toward_gestures) {
  const Vec3 g0{1.0f, 0.0f, 0.0f};
  TiltBasis basis = identityBasis();
  ASSERT_TRUE(deriveSteer(g0, Vec3{1.0f, -0.3f, 0.0f},
                          Vec3{1.0f, 0.3f, 0.0f}, basis));
  ASSERT_TRUE(deriveDip(Vec3{1.0f, 0.0f, 0.5f}, basis));

  ASSERT_TRUE(projectSteer(basis, Vec3{1.0f, -0.3f, 0.0f}) > 0.0f);
  ASSERT_TRUE(projectDip(basis, Vec3{1.0f, 0.0f, 0.5f}) < 0.0f);
}

SF_TEST(basis_reclined_pose_normalises_its_full_gestures) {
  const float angle = 25.0f * kPi / 180.0f;
  const Vec3 g0{0.519f, 0.423f, 0.743f};
  const Vec3 screenNormal{0.0f, 0.0f, 1.0f};
  const Vec3 right = rotateAround(g0, screenNormal, angle);
  const Vec3 left = rotateAround(g0, screenNormal, -angle);
  TiltBasis basis = identityBasis();

  ASSERT_TRUE(deriveSteer(g0, right, left, basis));
  const Vec3 perpendicular{
      basis.s.y * g0.z - basis.s.z * g0.y,
      basis.s.z * g0.x - basis.s.x * g0.z,
      basis.s.x * g0.y - basis.s.y * g0.x};
  const Vec3 dipDirection = scale(perpendicular, 1.0f / norm(perpendicular));
  const Vec3 toward = sub(g0, scale(dipDirection, std::sin(angle)));
  ASSERT_TRUE(deriveDip(toward, basis));
  ASSERT_NEAR(normalisedSteer(basis, right, 1.0f), 0.40f, 1e-3f);
   ASSERT_NEAR(normalisedDip(basis, toward), -0.423f, 1e-3f);
  ASSERT_NEAR(normalisedSteer(basis, right, 1.5f), 0.60f, 1e-3f);
  ASSERT_NEAR(kSteerReachG, 0.40f, 1e-6f);
}

SF_TEST(basis_dip_band_is_a_fraction_of_the_comfortable_dip) {
  TiltBasis identity = identityBasis();
  ASSERT_NEAR(effectiveDipFactor(identity), 1.0f, 1e-6f);

  TiltBasis comfortable = identityBasis();
  comfortable.dipScaleG = 0.2222f;
  ASSERT_NEAR(effectiveDipFactor(comfortable), 1.875f, 1e-3f);

  TiltBasis floor = identityBasis();
  floor.dipScaleG = 0.15f;
  ASSERT_NEAR(effectiveDipFactor(floor), 2.083f, 1e-3f);

  TiltBasis capped = identityBasis();
  capped.dipScaleG = 1.20f;
  ASSERT_NEAR(effectiveDipFactor(capped), 1.0f, 1e-6f);
}

SF_TEST(basis_steer_gain_is_capped) {
  TiltBasis small = identityBasis();
  small.calibrated = true;
  small.steerScaleG = 0.10f;
  small.steerScaleRightG = 0.10f;
  small.steerScaleLeftG = 0.10f;
  ASSERT_NEAR(effectiveSteerFactor(small), 2.0f, 1e-6f);

  TiltBasis normal = identityBasis();
  normal.calibrated = true;
  normal.steerScaleG = 0.308f;
  normal.steerScaleRightG = 0.308f;
  normal.steerScaleLeftG = 0.308f;
  ASSERT_NEAR(effectiveSteerFactor(normal), 1.30f, 1e-2f);
}

SF_TEST(basis_identity_constants_are_pinned) {
  const TiltBasis basis = identityBasis();
  ASSERT_NEAR(basis.g0.x, 0.0f, 1e-6f);
  ASSERT_NEAR(basis.g0.y, 0.0f, 1e-6f);
  ASSERT_NEAR(basis.g0.z, 0.0f, 1e-6f);
  ASSERT_NEAR(basis.s.x, 0.0f, 1e-6f);
  ASSERT_NEAR(basis.s.y, -1.0f, 1e-6f);
  ASSERT_NEAR(basis.s.z, 0.0f, 1e-6f);
  ASSERT_NEAR(basis.f.x, 0.0f, 1e-6f);
  ASSERT_NEAR(basis.f.y, 0.0f, 1e-6f);
  ASSERT_NEAR(basis.f.z, 1.0f, 1e-6f);
  ASSERT_NEAR(basis.steerScaleG, 0.36f, 1e-6f);
  ASSERT_NEAR(basis.steerScaleRightG, 0.36f, 1e-6f);
  ASSERT_NEAR(basis.steerScaleLeftG, 0.36f, 1e-6f);
  ASSERT_NEAR(basis.dipScaleG, 0.707f, 1e-6f);
  ASSERT_FALSE(basis.calibrated);
}

SF_TEST(basis_steer_scales_are_per_side) {
  const Vec3 g0{0.0f, 0.0f, 0.0f};
  const Vec3 direction = scale(Vec3{-0.264f, -0.958f, 0.107f},
                               1.0f / norm(Vec3{-0.264f, -0.958f, 0.107f}));
  TiltBasis basis = identityBasis();
  const Vec3 right = scale(direction, 0.243f);
  const Vec3 left = scale(direction, -0.685f);

  ASSERT_TRUE(deriveSteer(g0, right, left, basis));
  ASSERT_NEAR(basis.steerScaleRightG, 0.243f, 1e-3f);
  ASSERT_NEAR(basis.steerScaleLeftG, 0.685f, 1e-3f);
  ASSERT_NEAR(normalisedSteer(basis, right, 1.0f), 0.40f, 1e-3f);
  ASSERT_NEAR(normalisedSteer(basis, left, 1.0f), -0.40f, 1e-3f);
  ASSERT_NEAR(effectiveSteerFactorRight(basis), 1.646f, 1e-2f);
  ASSERT_NEAR(effectiveSteerFactorLeft(basis), 0.584f, 1e-2f);
}

SF_TEST(basis_identity_axes_are_orthogonal_unit_vectors) {
  const TiltBasis basis = identityBasis();
  ASSERT_NEAR(dot(basis.s, basis.f), 0.0f, 1e-6f);
  ASSERT_NEAR(norm(basis.s), 1.0f, 1e-6f);
  ASSERT_NEAR(norm(basis.f), 1.0f, 1e-6f);
}

SF_TEST(basis_learned_axes_separate_a_yawed_tilt) {
  const Vec3 g0{-0.375f, 0.0f, 0.927f};
  const Vec3 rightRaw{-0.722f, -0.589f, 0.354f};
  const Vec3 leftRaw{-0.058f, 0.832f, 0.554f};
  const Vec3 gRight = scale(rightRaw, 1.0f / norm(rightRaw));
  const Vec3 gLeft = scale(leftRaw, 1.0f / norm(leftRaw));
  const Vec3 rightDelta = sub(gRight, g0);
  const Vec3 leftDelta = sub(gLeft, g0);
  const Vec3 dipNormal{
      rightDelta.y * leftDelta.z - rightDelta.z * leftDelta.y,
      rightDelta.z * leftDelta.x - rightDelta.x * leftDelta.z,
      rightDelta.x * leftDelta.y - rightDelta.y * leftDelta.x};
  const Vec3 dipDirection = scale(dipNormal, 1.0f / norm(dipNormal));
  const Vec3 gToward = sub(
      g0, scale(dipDirection, std::sin(25.0f * kPi / 180.0f)));
  TiltBasis basis = identityBasis();

  ASSERT_TRUE(deriveSteer(g0, gRight, gLeft, basis));
  ASSERT_TRUE(deriveDip(gToward, basis));
  ASSERT_NEAR(projectDip(basis, gRight), 0.0f, 0.06f);
  ASSERT_NEAR(projectDip(basis, gLeft), 0.0f, 0.06f);
  ASSERT_NEAR(projectSteer(basis, gRight), basis.steerScaleRightG, 1e-3f);
  ASSERT_NEAR(projectSteer(basis, gLeft), -basis.steerScaleLeftG, 1e-3f);
  ASSERT_TRUE(normalisedDip(basis, gToward) <= -0.25f);
}
