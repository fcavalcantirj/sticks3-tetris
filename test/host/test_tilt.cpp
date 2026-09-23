#include "framework.h"
#include "imu_axes.h"
#include "stackfall/input/tilt.h"

#include <cstdint>

using sf::input::Tilt;

namespace {

constexpr uint32_t kTestDtMs = 40;
constexpr float kFilterRcMs = 80.0f;

float rawYForSigned(float signedY) {
  return signedY * static_cast<float>(kTiltSignLeftRight);
}

// Drives the production EMA to an exact requested post-filter signal. This
// keeps the quantiser tests about its specified boundaries instead of about an
// arbitrary number of warm-up samples.
class TiltDriver {
 public:
  TiltDriver() { capture(0.0f); }

  void capture(float signedY) {
    tilt.captureNeutral(rawYForSigned(signedY));
    neutral_ = signedY;
    filtered_ = signedY;
  }

  int setSignal(float signal, bool dipEngaged = false, uint32_t dtMs = kTestDtMs) {
    const uint32_t clampedDt = dtMs > kTestDtMs ? kTestDtMs : dtMs;
    const float alpha = static_cast<float>(clampedDt) /
                        (kFilterRcMs + static_cast<float>(clampedDt));
    const float target = neutral_ + signal;
    const float rawSigned = filtered_ + (target - filtered_) / alpha;
    filtered_ = target;
    return tilt.update(rawYForSigned(rawSigned), dtMs, dipEngaged);
  }

  Tilt tilt;

 private:
  float neutral_ = 0.0f;
  float filtered_ = 0.0f;
};

}  // namespace

SF_TEST(tilt_quantiser_has_symmetric_edges_and_centre_boundary) {
  TiltDriver centred;
  ASSERT_EQ(centred.setSignal(0.0f), 4);

  TiltDriver leftEdge;
  ASSERT_EQ(leftEdge.setSignal(-0.36f), 0);

  TiltDriver rightEdge;
  ASSERT_EQ(rightEdge.setSignal(0.36f), 9);

  ASSERT_EQ(-(-0.36f), 0.36f);
}

SF_TEST(tilt_hysteresis_crosses_once_without_chatter) {
  TiltDriver driver;
  int transitions = 0;
  int previous = driver.tilt.column();

  const float signals[] = {0.019f, 0.021f, 0.019f, 0.001f, -0.019f,
                           0.001f, 0.019f};
  for (float signal : signals) {
    const int column = driver.setSignal(signal);
    if (column != previous) {
      ++transitions;
      previous = column;
    }
  }

  ASSERT_EQ(transitions, 1);
  ASSERT_EQ(previous, 5);
  ASSERT_EQ(driver.setSignal(-0.021f), 4);
}

SF_TEST(tilt_five_column_jump_is_one_absolute_selection) {
  TiltDriver driver;
  ASSERT_EQ(driver.tilt.column(), 4);

  const int selected = driver.setSignal(0.36f);
  ASSERT_EQ(selected, 9);
  ASSERT_EQ(driver.tilt.column(), 9);
}

SF_TEST(tilt_dip_freezes_and_then_releases_selection) {
  TiltDriver driver;
  ASSERT_EQ(driver.setSignal(-0.20f), 2);

  ASSERT_EQ(driver.setSignal(0.36f, true), 2);
  ASSERT_EQ(driver.setSignal(-0.45f, true), 2);
  ASSERT_EQ(driver.setSignal(0.36f, true), 2);

  ASSERT_EQ(driver.setSignal(0.36f, false), 9);
}

SF_TEST(tilt_capture_neutral_recentres_immediately) {
  TiltDriver driver;
  ASSERT_EQ(driver.setSignal(0.36f), 9);

  driver.capture(0.36f);
  ASSERT_EQ(driver.tilt.column(), 5);
  ASSERT_EQ(driver.setSignal(0.0f), 5);
}

SF_TEST(tilt_neutral_never_drifts_off_centre) {
  TiltDriver driver;
  ASSERT_EQ(driver.setSignal(0.18f), 7);

  for (int i = 0; i < 10000; ++i) {
    ASSERT_EQ(driver.tilt.update(rawYForSigned(0.18f), 6, false), 7);
  }
}

SF_TEST(tilt_ema_uses_measured_sign_and_clamps_stalled_dt) {
  Tilt tilt;
  tilt.captureNeutral(0.0f);

  ASSERT_EQ(kTiltSignLeftRight, -1);
  // With dt clamped to 40 ms, alpha is 1/3 and this raw sample becomes
  // +0.010 G: still inside the +0.020 G hysteresis threshold. An unclamped
  // 1000 ms frame would incorrectly jump to column 5.
  ASSERT_EQ(tilt.update(rawYForSigned(0.030f), 1000, false), 4);
}
