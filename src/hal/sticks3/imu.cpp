#include "hal/sticks3/imu.h"

#include <M5Unified.h>

#include <cstdint>
#include <limits>

#include "stackfall/input/tilt.h"

namespace sf_hal {

bool Imu::begin() {
  present_ = M5.Imu.getType() == m5::imu_t::imu_bmi270;
  return present_;
}

AccelSample Imu::poll(uint32_t nowMs) {
  ++stats_.polls;

  AccelSample result = sample_;
  result.fresh = false;
  if (!present_ || M5.Imu.update() == 0) {
    ++stats_.stale;
    return result;
  }

  const uint32_t gapMs = nowMs - lastFreshMs_;
  M5.Imu.getAccel(&sample_.ax, &sample_.ay, &sample_.az);
  sample_.tMs = nowMs;
  sample_.dtMs = sf::clampDtMs(gapMs, 1, 50);
  sample_.fresh = true;
  lastFreshMs_ = nowMs;

  const uint16_t recordedGap =
      gapMs > std::numeric_limits<uint16_t>::max()
          ? std::numeric_limits<uint16_t>::max()
          : static_cast<uint16_t>(gapMs);
  if (recordedGap > stats_.maxGapMs) {
    stats_.maxGapMs = recordedGap;
  }
  ++stats_.fresh;
  return sample_;
}

namespace probe {

void readAccel(float* ax, float* ay, float* az) {
  M5.Imu.getAccel(ax, ay, az);
}

void readGyro(float* gx, float* gy, float* gz) {
  M5.Imu.getGyro(gx, gy, gz);
}

}  // namespace probe

}  // namespace sf_hal
