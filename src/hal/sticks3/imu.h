#pragma once

#include <cstdint>

namespace sf_hal {

struct AccelSample {
  uint32_t tMs = 0;
  float ax = 0.0f;
  float ay = 0.0f;
  float az = 0.0f;
  uint16_t dtMs = 0;
  bool fresh = false;
};

struct Stats {
  uint32_t polls = 0;
  uint32_t fresh = 0;
  uint32_t stale = 0;
  uint16_t maxGapMs = 0;
};

class Imu {
 public:
  bool begin();
  bool present() const { return present_; }
  AccelSample poll(uint32_t nowMs);
  const Stats& stats() const { return stats_; }

 private:
  AccelSample sample_{};
  Stats stats_{};
  uint32_t lastFreshMs_ = 0;
  bool present_ = false;
};

// Compatibility bridge for the archived S3 spike firmware. Gameplay samples
// through Imu; these functions keep all direct sensor access in imu.cpp while
// preserving the old accelerometer/gyroscope capture tools.
namespace probe {

void readAccel(float* ax, float* ay, float* az);
void readGyro(float* gx, float* gy, float* gz);

}  // namespace probe

}  // namespace sf_hal
