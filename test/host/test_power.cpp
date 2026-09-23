#include <cstdint>

#include "framework.h"
#include "stackfall/power/policy.h"

namespace {

sf::PowerIn normalInput() {
  sf::PowerIn in{};
  in.batteryPct = 100;
  in.vbatMv = 4100;
  in.vbusMv = 0;
  in.charging = false;
  in.lastInputMs = 0;
  in.nowMs = 0;
  in.brightnessSetting = 3;
  in.volumeSetting = 4;
  in.playing = false;
  return in;
}

sf::PowerOut stepWithVbus(sf::PowerState& state, sf::PowerIn& in,
                          uint16_t vbusMv) {
  in.vbusMv = vbusMv;
  ++in.nowMs;
  return sf::powerStep(state, in);
}

}  // namespace

SF_TEST(test_power_usb_requires_three_consecutive_samples) {
  sf::PowerState state{};
  sf::PowerIn in = normalInput();
  constexpr uint16_t kNoTransitionSamples[] = {0, 0, 4900, 0, 0, 0};

  for (uint16_t vbusMv : kNoTransitionSamples) {
    const sf::PowerOut out = stepWithVbus(state, in, vbusMv);
    ASSERT_FALSE(out.onUsb);
    ASSERT_EQ(out.volumeCap, 96u);
  }

  uint8_t transitions = 0;
  bool previous = false;
  for (uint8_t sample = 0; sample < 3; ++sample) {
    const sf::PowerOut out = stepWithVbus(state, in, 4900);
    transitions += out.onUsb != previous ? 1u : 0u;
    previous = out.onUsb;
    ASSERT_EQ(out.onUsb, sample == 2);
    ASSERT_EQ(out.volumeCap, sample == 2 ? 128u : 96u);
  }
  ASSERT_EQ(transitions, 1u);

  ASSERT_TRUE(stepWithVbus(state, in, 0).onUsb);
  ASSERT_TRUE(stepWithVbus(state, in, 0).onUsb);
  ASSERT_FALSE(stepWithVbus(state, in, 0).onUsb);
}

SF_TEST(test_power_volume_caps_and_applied_volume) {
  ASSERT_EQ(sf::volumeCapFor(false), 96u);
  ASSERT_EQ(sf::volumeCapFor(true), 128u);

  sf::PowerState state{};
  sf::PowerIn in = normalInput();
  sf::PowerOut out = sf::powerStep(state, in);
  ASSERT_FALSE(out.onUsb);
  ASSERT_EQ(out.volumeCap, 96u);
  ASSERT_EQ(out.volume, 96u);

  stepWithVbus(state, in, 4900);
  stepWithVbus(state, in, 4900);
  out = stepWithVbus(state, in, 4900);
  ASSERT_TRUE(out.onUsb);
  ASSERT_EQ(out.volumeCap, 128u);
  ASSERT_EQ(out.volume, 128u);
}

SF_TEST(test_power_low_battery_latches_until_twenty_percent) {
  sf::PowerState state{};
  sf::PowerIn in = normalInput();
  stepWithVbus(state, in, 4900);
  stepWithVbus(state, in, 4900);
  stepWithVbus(state, in, 4900);

  in.batteryPct = 15;
  sf::PowerOut out = sf::powerStep(state, in);
  ASSERT_TRUE(out.lowBattery);
  ASSERT_TRUE(out.onUsb);
  ASSERT_EQ(out.volumeCap, 64u);
  ASSERT_EQ(out.volume, 64u);

  for (uint8_t pct = 16; pct < 20; ++pct) {
    in.batteryPct = pct;
    out = sf::powerStep(state, in);
    ASSERT_TRUE(out.lowBattery);
    ASSERT_EQ(out.volumeCap, 64u);
  }

  in.batteryPct = 20;
  out = sf::powerStep(state, in);
  ASSERT_FALSE(out.lowBattery);
  ASSERT_EQ(out.volumeCap, 128u);
  ASSERT_EQ(out.volume, 128u);
}

SF_TEST(test_power_idle_dim_boundary_and_input_restore) {
  sf::PowerState state{};
  sf::PowerIn in = normalInput();

  in.nowMs = 29999;
  sf::PowerOut out = sf::powerStep(state, in);
  ASSERT_FALSE(out.dimmed);
  ASSERT_EQ(out.brightness, 160u);

  in.nowMs = 30000;
  out = sf::powerStep(state, in);
  ASSERT_TRUE(out.dimmed);
  ASSERT_EQ(out.brightness, 40u);

  in.nowMs = 30001;
  in.lastInputMs = 30001;
  out = sf::powerStep(state, in);
  ASSERT_FALSE(out.dimmed);
  ASSERT_EQ(out.brightness, 160u);
}

SF_TEST(test_power_playing_never_dims) {
  sf::PowerState state{};
  sf::PowerIn in = normalInput();
  in.playing = true;
  in.nowMs = 600000;

  const sf::PowerOut out = sf::powerStep(state, in);
  ASSERT_FALSE(out.dimmed);
  ASSERT_EQ(out.brightness, 160u);
}

SF_TEST(test_power_brightness_floor_for_all_settings) {
  constexpr uint8_t kExpectedDimmed[] = {8, 16, 24, 40, 63};
  for (uint8_t setting = 0; setting < 5; ++setting) {
    sf::PowerState state{};
    sf::PowerIn in = normalInput();
    in.brightnessSetting = setting;
    in.nowMs = 30000;

    const sf::PowerOut out = sf::powerStep(state, in);
    ASSERT_TRUE(out.dimmed);
    ASSERT_EQ(out.brightness, kExpectedDimmed[setting]);
    ASSERT_TRUE(out.brightness >= 8u);
  }
}
