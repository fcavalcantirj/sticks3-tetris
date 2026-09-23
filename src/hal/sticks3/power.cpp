#include "hal/sticks3/power.h"

#include <M5Unified.h>

#include <cstdint>

namespace sf_hal {
namespace {

uint8_t batteryPercent(int32_t value) {
  if (value <= 0) return 0;
  if (value >= 100) return 100;
  return static_cast<uint8_t>(value);
}

uint16_t millivolts(int16_t value) {
  return value > 0 ? static_cast<uint16_t>(value) : 0u;
}

}  // namespace

sf::PowerIn readPower(uint32_t nowMs) {
  sf::PowerIn in{};
  in.batteryPct = batteryPercent(M5.Power.getBatteryLevel());
  in.vbatMv = millivolts(M5.Power.getBatteryVoltage());
  in.vbusMv = millivolts(M5.Power.getVBUSVoltage());
  in.charging =
      M5.Power.isCharging() == m5::Power_Class::is_charging;
  in.nowMs = nowMs;
  return in;
}

}  // namespace sf_hal
