// SPIKE S3b twist detector, round 2. Gyro units are degrees per second (dps).
#include "s3b_twist_r2.h"
#include <cmath>

namespace sf {

namespace {
// All numeric settings below are STARTING GUESSES to be measured by the drill,
// not derived constants.
constexpr uint32_t kWindowMs = 300;
constexpr uint32_t kResampleMs = 5;
constexpr int kWindowSamples = kWindowMs / kResampleMs; // 60
constexpr float kFireThresholdDeg = 25.0f;
constexpr float kRearmDps = 20.0f;
constexpr uint32_t kRearmHoldMs = 100;
constexpr float kRestDps = 5.0f;
constexpr uint32_t kBiasTauMs = 2000;
} // namespace

TwistDetectorR2::TwistDetectorR2()
    : head_(0), count_(0), integral_(0.0f), bias_(0.0f),
      fired_(false), lastFiredDir_(0), calmSinceMs_(0), lastSampleMs_(0),
      lastResampleMs_(0), gxAccumulator_(0.0f), accSamples_(0) {
    for (int i = 0; i < kWindowSamples; ++i) {
        samples_[i] = {0, 0.0f};
    }
}

void TwistDetectorR2::resetWindow() {
    head_ = 0;
    count_ = 0;
    integral_ = 0.0f;
    for (int i = 0; i < kWindowSamples; ++i) samples_[i] = {0, 0.0f};
}

void TwistDetectorR2::push(float gx, uint32_t nowMs) {
    // Evict oldest sample before overwriting if the ring is full.
    if (count_ == kWindowSamples) {
        int tail = (head_ - count_ + kWindowSamples) % kWindowSamples;
        integral_ -= samples_[tail].gx * (float)kResampleMs / 1000.0f;
    }
    samples_[head_] = {nowMs, gx};
    head_ = (head_ + 1) % kWindowSamples;
    if (count_ < kWindowSamples) ++count_;
    integral_ += gx * (float)kResampleMs / 1000.0f;
}

int8_t TwistDetectorR2::update(float gxDps, uint32_t nowMs) {
    if (lastSampleMs_ != 0 && nowMs > lastSampleMs_) {
        uint32_t dt = nowMs - lastSampleMs_;
        if (dt > 50) dt = 50;
        gxAccumulator_ += gxDps * (float)dt;
        accSamples_ += (int)dt;
    }
    lastSampleMs_ = nowMs;

    // Update zero-rate bias while the device is at rest. This is a refinement,
    // not a correctness issue: a few dps of BMI270 bias integrates to only about
    // a degree over a 300 ms window.
    float corrected = gxDps - bias_;
    if (std::fabs(corrected) < kRestDps) {
        uint32_t dtMs = (lastSampleMs_ == 0) ? kResampleMs : 1;
        float alpha = (float)dtMs / ((float)kBiasTauMs + (float)dtMs);
        bias_ += (gxDps - bias_) * alpha;
    }

    // Resample at a fixed 5 ms cadence so the window is a wall-clock window.
    if (nowMs - lastResampleMs_ >= kResampleMs) {
        lastResampleMs_ = nowMs;
        float avgGx = (accSamples_ > 0) ? (gxAccumulator_ / (float)accSamples_) : corrected;
        push(avgGx, nowMs);
        gxAccumulator_ = 0.0f;
        accSamples_ = 0;
    }

    // Re-arm after the wrist has been still long enough.
    if (fired_) {
        if (std::fabs(corrected) < kRearmDps) {
            if (calmSinceMs_ == 0) calmSinceMs_ = nowMs;
            else if ((uint32_t)(nowMs - calmSinceMs_) >= kRearmHoldMs) {
                fired_ = false;
                lastFiredDir_ = 0;
                calmSinceMs_ = 0;
            }
        } else {
            calmSinceMs_ = 0;
        }
    }

    if (!fired_) {
        if (integral_ > kFireThresholdDeg) {
            fired_ = true;
            lastFiredDir_ = 1;
            calmSinceMs_ = 0;
            resetWindow();
            return 1;
        }
        if (integral_ < -kFireThresholdDeg) {
            fired_ = true;
            lastFiredDir_ = -1;
            calmSinceMs_ = 0;
            resetWindow();
            return -1;
        }
    }
    return 0;
}

} // namespace sf
