#pragma once
#include <cstdint>

namespace sf {

// Twist gesture detector for SPIKE S3b T7. Integrates the X gyro (degrees per
// second) over a sliding ~300 ms window and fires when the integral crosses
// +/-25 degrees. Re-arms only after |gx| stays below 20 dps for 100 ms.
// Maintains a slow rolling zero-rate bias estimate while the device is at rest.
class TwistDetector {
 public:
    TwistDetector();
    // Call with gx in degrees per second and a monotonic millisecond clock.
    // Returns +1 on a detected CW twist, -1 on CCW, 0 when no gesture fires.
    int8_t update(float gxDps, uint32_t nowMs);
    float bias() const { return bias_; }
 private:
    static constexpr int kWindowSamples = 64;
    struct Sample {
        uint32_t t;
        float gx;
        uint32_t dt; // ms since previous sample
    };
    Sample samples_[kWindowSamples];
    int head_;
    int count_;
    float integral_;     // degrees, sum over window
    float bias_;
    bool fired_;
    int8_t lastFiredDir_;
    uint32_t calmSinceMs_;
    uint32_t lastSampleMs_;
    void popOld(uint32_t nowMs);
    void push(float gx, uint32_t dtMs, uint32_t nowMs);
};

} // namespace sf
