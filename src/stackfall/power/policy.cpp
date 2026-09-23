#include "stackfall/power/policy.h"

#include "stackfall/audio/sfx.h"

namespace sf {
namespace {

constexpr uint8_t kSettingCount = 5;
constexpr uint8_t kBrightnessSteps[kSettingCount] = {32, 64, 96, 160, 255};
constexpr uint8_t kVolumeSteps[kSettingCount] = {0, 64, 128, 192, 255};
constexpr uint16_t kUsbPresentMv = 4000;
constexpr uint8_t kUsbAgreementSamples = 3;
constexpr uint8_t kLowBatteryLatchPct = 15;
constexpr uint8_t kLowBatteryClearPct = 20;
constexpr uint32_t kIdleDimMs = 30000;
constexpr uint8_t kMinimumBrightness = 8;

uint8_t settingIndex(uint8_t setting) {
  return setting < kSettingCount ? setting
                                 : static_cast<uint8_t>(kSettingCount - 1);
}

}  // namespace

bool isOnUsb(PowerState& state, uint16_t vbusMv) {
  const bool sampleOnUsb = vbusMv >= kUsbPresentMv;
  if (sampleOnUsb == state.onUsb) {
    state.usbAgreement = 0;
    return state.onUsb;
  }

  if (state.usbAgreement < kUsbAgreementSamples) {
    ++state.usbAgreement;
  }
  if (state.usbAgreement >= kUsbAgreementSamples) {
    state.onUsb = sampleOnUsb;
    state.usbAgreement = 0;
  }
  return state.onUsb;
}

uint8_t volumeCapFor(bool onUsb) {
  return onUsb ? kVolumeCapUsb : kVolumeCapBattery;
}

PowerOut powerStep(PowerState& state, const PowerIn& in) {
  const bool onUsb = isOnUsb(state, in.vbusMv);
  if (in.batteryPct <= kLowBatteryLatchPct) {
    state.lowBattery = true;
  } else if (in.batteryPct >= kLowBatteryClearPct) {
    state.lowBattery = false;
  }

  const bool dimmed = !in.playing &&
                       in.nowMs - in.lastInputMs >= kIdleDimMs;
  uint8_t brightness = kBrightnessSteps[settingIndex(in.brightnessSetting)];
  if (dimmed) {
    const uint8_t quarter = static_cast<uint8_t>(brightness / 4u);
    brightness = quarter < kMinimumBrightness ? kMinimumBrightness : quarter;
  }

  const uint8_t volumeCap =
      state.lowBattery ? kVolumeCapLow : volumeCapFor(onUsb);
  const uint8_t requestedVolume = kVolumeSteps[settingIndex(in.volumeSetting)];
  const uint8_t volume =
      requestedVolume < volumeCap ? requestedVolume : volumeCap;
  return PowerOut{brightness, volumeCap, volume, state.lowBattery, dimmed,
                  onUsb};
}

}  // namespace sf
