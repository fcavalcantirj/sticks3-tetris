#pragma once

#include <cstdint>

namespace sf {

struct PowerIn {
  uint8_t batteryPct;
  uint16_t vbatMv;
  uint16_t vbusMv;
  bool charging;
  uint32_t lastInputMs;
  uint32_t nowMs;
  uint8_t brightnessSetting;
  uint8_t volumeSetting;
  bool playing;
};

struct PowerOut {
  uint8_t brightness;
  uint8_t volumeCap;
  uint8_t volume;
  bool lowBattery;
  bool dimmed;
  bool onUsb;
};

struct PowerState {
  bool onUsb = false;
  bool lowBattery = false;
  uint8_t usbAgreement = 0;
};

constexpr uint8_t kVolumeCapLow = 64;

bool isOnUsb(PowerState& state, uint16_t vbusMv);
uint8_t volumeCapFor(bool onUsb);
PowerOut powerStep(PowerState& state, const PowerIn& in);

}  // namespace sf
