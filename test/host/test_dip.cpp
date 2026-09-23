#include "framework.h"
#include "stackfall/input/action.h"
#include "stackfall/input/dip.h"

#include <cstddef>
#include <cstdint>

using sf::input::ActionKind;
using sf::input::ActionQueue;
using sf::input::Dip;
using sf::input::GameAction;
using sf::input::kDipSignAway;

namespace {

constexpr uint32_t kTestDtMs = 40;
constexpr float kFilterRcMs = 80.0f;

float rawZForAwaySignal(float awaySignal) {
  return awaySignal * static_cast<float>(kDipSignAway);
}

// Drives the production EMA to an exact post-filter signal. Threshold tests
// can therefore exercise the specified Schmitt boundaries without depending
// on an arbitrary warm-up duration.
class DipDriver {
 public:
  DipDriver() { capture(0.0f); }

  void capture(float awaySignal) {
    dip.captureNeutral(rawZForAwaySignal(awaySignal));
    neutral_ = awaySignal;
    filtered_ = awaySignal;
  }

  void setSignal(float signal, uint32_t nowMs, ActionQueue& out,
                 uint32_t dtMs = kTestDtMs) {
    const uint32_t clampedDt = dtMs > kTestDtMs ? kTestDtMs : dtMs;
    const float alpha = static_cast<float>(clampedDt) /
                        (kFilterRcMs + static_cast<float>(clampedDt));
    const float target = neutral_ + signal;
    const float rawAway = filtered_ + (target - filtered_) / alpha;
    filtered_ = target;
    dip.update(rawZForAwaySignal(rawAway), dtMs, nowMs, out);
  }

  Dip dip;

 private:
  float neutral_ = 0.0f;
  float filtered_ = 0.0f;
};

struct TraceSample {
  uint32_t tMs;
  float zG;
};

template <std::size_t N>
void replayDipTrace(const TraceSample (&samples)[N], Dip& dip,
                    ActionQueue& out) {
  static_assert(N > 0, "a trace needs a neutral sample");
  dip.captureNeutral(samples[0].zG);
  uint32_t previousMs = samples[0].tMs;
  for (std::size_t i = 1; i < N; ++i) {
    const uint32_t dtMs = samples[i].tMs - previousMs;
    dip.update(samples[i].zG, dtMs, samples[i].tMs, out);
    previousMs = samples[i].tMs;
  }
}

}  // namespace

SF_TEST(dip_away_crossing_rotates_once_and_holding_is_silent) {
  DipDriver driver;
  ActionQueue out;

  driver.setSignal(0.25f, 10, out);
  ASSERT_TRUE(driver.dip.engaged());
  ASSERT_TRUE(!driver.dip.softDropActive());

  for (uint32_t tMs = 15; tMs <= 5010; tMs += 5) {
    driver.setSignal(0.50f, tMs, out, 5);
  }

  GameAction action{};
  ASSERT_TRUE(out.pop(action));
  ASSERT_EQ(action.kind, ActionKind::RotateCw);
  ASSERT_EQ(action.tMs, 10u);
  ASSERT_TRUE(!out.pop(action));
}

SF_TEST(dip_away_return_overshoot_does_not_soft_drop) {
  DipDriver driver;
  ActionQueue out;

  driver.setSignal(0.70f, 10, out);
  // One return-stroke sample skips past neutral into the opposite direction.
  driver.setSignal(-0.40f, 20, out);

  int rotates = 0;
  int softDropOns = 0;
  GameAction action{};
  while (out.pop(action)) {
    if (action.kind == ActionKind::RotateCw) {
      ++rotates;
    } else if (action.kind == ActionKind::SoftDropOn) {
      ++softDropOns;
    }
  }
  ASSERT_EQ(rotates, 1);
  ASSERT_EQ(softDropOns, 0);
}

SF_TEST(dip_away_rearms_only_at_release_and_rotates_again) {
  DipDriver driver;
  ActionQueue out;

  driver.setSignal(0.25f, 10, out);
  driver.setSignal(0.20f, 20, out);
  driver.setSignal(0.16f, 30, out);
  driver.setSignal(0.24f, 40, out);
  ASSERT_TRUE(driver.dip.engaged());
  ASSERT_EQ(out.size(), 1u);

  driver.setSignal(0.15f, 50, out);
  ASSERT_TRUE(!driver.dip.engaged());
  driver.setSignal(0.0f, 139, out);
  ASSERT_EQ(out.size(), 1u);
  driver.setSignal(0.0f, 140, out);
  driver.setSignal(0.25f, 150, out);

  GameAction action{};
  ASSERT_TRUE(out.pop(action));
  ASSERT_EQ(action.kind, ActionKind::RotateCw);
  ASSERT_EQ(action.tMs, 10u);
  ASSERT_TRUE(out.pop(action));
  ASSERT_EQ(action.kind, ActionKind::RotateCw);
  ASSERT_EQ(action.tMs, 150u);
  ASSERT_TRUE(!out.pop(action));
}

SF_TEST(dip_toward_reports_soft_drop_until_release) {
  DipDriver driver;
  ActionQueue out;

  driver.setSignal(-0.25f, 100, out);
  ASSERT_TRUE(driver.dip.engaged());
  ASSERT_TRUE(!driver.dip.softDropActive());

  driver.setSignal(-0.25f, 219, out);
  ASSERT_TRUE(!driver.dip.softDropActive());
  driver.setSignal(-0.25f, 220, out);
  ASSERT_TRUE(driver.dip.softDropActive());
  driver.setSignal(-0.20f, 300, out);
  driver.setSignal(-0.16f, 350, out);
  ASSERT_TRUE(driver.dip.softDropActive());

  driver.setSignal(-0.15f, 400, out);
  ASSERT_TRUE(!driver.dip.engaged());
  ASSERT_TRUE(!driver.dip.softDropActive());

  GameAction action{};
  ASSERT_TRUE(out.pop(action));
  ASSERT_EQ(action.kind, ActionKind::SoftDropOn);
  ASSERT_EQ(action.tMs, 220u);
  ASSERT_TRUE(out.pop(action));
  ASSERT_EQ(action.kind, ActionKind::SoftDropOff);
  ASSERT_EQ(action.tMs, 400u);
  ASSERT_TRUE(!out.pop(action));
}

SF_TEST(dip_toward_requires_120ms_continuous_dwell) {
  DipDriver driver;
  ActionQueue out;

  driver.setSignal(-0.30f, 100, out);
  driver.setSignal(-0.30f, 200, out);
  ASSERT_EQ(out.size(), 0u);

  driver.setSignal(-0.30f, 300, out);
  ASSERT_EQ(out.size(), 1u);
  GameAction action{};
  ASSERT_TRUE(out.pop(action));
  ASSERT_EQ(action.kind, ActionKind::SoftDropOn);
  ASSERT_EQ(action.tMs, 300u);

  driver.setSignal(-0.30f, 400, out);
  ASSERT_TRUE(!out.pop(action));
}

SF_TEST(dip_mixed_sequence_pairs_every_soft_drop) {
  DipDriver driver;
  ActionQueue out;

  driver.setSignal(-0.30f, 0, out);
  driver.setSignal(-0.30f, 120, out);
  driver.setSignal(0.0f, 130, out);
  driver.setSignal(0.0f, 220, out);
  driver.setSignal(0.70f, 230, out);
  driver.setSignal(-0.40f, 240, out);

  bool softDropActive = false;
  int softDropOns = 0;
  int softDropOffs = 0;
  GameAction action{};
  while (out.pop(action)) {
    if (action.kind == ActionKind::SoftDropOn) {
      ASSERT_TRUE(!softDropActive);
      softDropActive = true;
      ++softDropOns;
    } else if (action.kind == ActionKind::SoftDropOff) {
      ASSERT_TRUE(softDropActive);
      softDropActive = false;
      ++softDropOffs;
    }
  }

  ASSERT_TRUE(!softDropActive);
  ASSERT_EQ(softDropOns, 1);
  ASSERT_EQ(softDropOffs, 1);
}

SF_TEST(dip_schmitt_band_cannot_chatter) {
  DipDriver driver;
  ActionQueue out;

  const float signals[] = {0.25f, 0.24f, 0.16f, 0.24f,
                           0.151f, 0.249f, 0.16f};
  uint32_t nowMs = 0;
  for (float signal : signals) {
    driver.setSignal(signal, nowMs, out);
    nowMs += kTestDtMs;
  }

  ASSERT_EQ(out.size(), 1u);
  ASSERT_TRUE(driver.dip.engaged());
}

SF_TEST(dip_engaged_tracks_both_physical_directions) {
  DipDriver driver;
  ActionQueue out;

  ASSERT_TRUE(!driver.dip.engaged());
  driver.setSignal(0.25f, 10, out);
  ASSERT_TRUE(driver.dip.engaged());
  driver.setSignal(0.15f, 20, out);
  ASSERT_TRUE(!driver.dip.engaged());

  driver.setSignal(0.0f, 110, out);
  driver.setSignal(-0.25f, 120, out);
  ASSERT_TRUE(driver.dip.engaged());
  driver.setSignal(-0.25f, 240, out);
  ASSERT_TRUE(driver.dip.softDropActive());
  driver.setSignal(-0.15f, 250, out);
  ASSERT_TRUE(!driver.dip.engaged());
}

SF_TEST(dip_steering_plane_bleed_triggers_nothing) {
  DipDriver driver;
  ActionQueue out;

  for (uint32_t i = 0; i < 1000; ++i) {
    const float bleed = (i & 1u) == 0 ? 0.067f : -0.067f;
    driver.setSignal(bleed, i * 6u, out, 6);
    ASSERT_TRUE(!driver.dip.engaged());
  }

  ASSERT_EQ(out.size(), 0u);
}

SF_TEST(dip_ema_clamps_a_stalled_frame) {
  Dip dip;
  ActionQueue out;
  dip.captureNeutral(0.0f);

  // A clamped 40 ms frame has alpha 1/3, so 0.30 G becomes 0.10 G and
  // remains below engage. Using the raw 1000 ms dt would cross the threshold.
  dip.update(rawZForAwaySignal(0.30f), 1000, 1000, out);
  ASSERT_TRUE(!dip.engaged());
  ASSERT_EQ(out.size(), 0u);
}

SF_TEST(dip_trace_stream_uses_production_detector) {
  ASSERT_EQ(kDipSignAway, 1);

  // A compact raw-G capture: neutral, away dip and return, toward dip and
  // return. Its timestamps supply the real sample intervals to Dip::update.
  const TraceSample samples[] = {
      {0, 0.00f},    {40, 0.78f},  {80, 0.78f},  {120, 0.78f},
      {160, 0.00f},  {200, 0.00f}, {240, 0.00f}, {280, 0.00f},
      {320, 0.00f},  {360, 0.00f}, {400, 0.00f},  {440, -0.78f},
      {480, -0.78f}, {520, -0.78f}, {560, -0.78f}, {600, -0.78f},
      {640, 0.00f},  {680, 0.00f}, {720, 0.00f},  {760, 0.00f},
      {800, 0.00f},
  };

  Dip dip;
  ActionQueue out;
  replayDipTrace(samples, dip, out);

  GameAction action{};
  ASSERT_TRUE(out.pop(action));
  ASSERT_EQ(action.kind, ActionKind::RotateCw);
  ASSERT_EQ(action.tMs, 40u);
  ASSERT_TRUE(out.pop(action));
  ASSERT_EQ(action.kind, ActionKind::SoftDropOn);
  ASSERT_EQ(action.tMs, 600u);
  ASSERT_TRUE(out.pop(action));
  ASSERT_EQ(action.kind, ActionKind::SoftDropOff);
  ASSERT_EQ(action.tMs, 760u);
  ASSERT_TRUE(!out.pop(action));
  ASSERT_TRUE(!dip.engaged());
}
