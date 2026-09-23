#pragma once
#include <cstdint>

namespace sf {

// Twist gesture detector for SPIKE S3b T7 round 2.
// Integrates the X gyro (degrees per second) over a sliding 300 ms window
// resampled at a fixed 5 ms cadence (60 taps). Fires when the integral crosses
// +/-25 degrees. Re-arms only after |gx| stays below 20 dps for 100 ms.
// Maintains a slow rolling zero-rate bias estimate while the device is at rest.
// The constants are starting guesses to be measured; they are labelled as such
// in the implementation.
class TwistDetectorR2 {
 public:
    TwistDetectorR2();
    // Call with gx in degrees per second and a monotonic millisecond clock.
    // Returns +1 on a detected CW twist, -1 on CCW, 0 when no gesture fires.
    int8_t update(float gxDps, uint32_t nowMs);
    float bias() const { return bias_; }
 private:
    static constexpr int kWindowMs = 300;       // starting guess
    static constexpr int kResampleMs = 5;       // fixed cadence
    static constexpr int kWindowSamples = kWindowMs / kResampleMs; // 60 taps
    static constexpr float kFireThresholdDeg = 25.0f; // starting guess
    static constexpr float kRearmDps = 20.0f;   // starting guess
    static constexpr uint32_t kRearmHoldMs = 100;
    static constexpr float kRestDps = 5.0f;
    static constexpr uint32_t kBiasTauMs = 2000;

    struct Sample {
        uint32_t t;
        float gx;
    };
    Sample samples_[kWindowSamples];
    int head_;
    int count_;
    float integral_;
    float bias_;
    bool fired_;
    int8_t lastFiredDir_;
    uint32_t calmSinceMs_;
    uint32_t lastSampleMs_;
    uint32_t lastResampleMs_;
    float gxAccumulator_;
    int accSamples_;

    void resetWindow();
    void push(float gx, uint32_t nowMs);
};

} // namespace sf
