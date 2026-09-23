// SPIKE S3b twist detector. Gyro units are degrees per second (dps).
#include "s3b_twist.h"
#include <cmath>

namespace sf {

namespace {
constexpr uint32_t kWindowMs = 300;
constexpr float kFireThresholdDeg = 25.0f;
constexpr float kRearmDps = 20.0f;
constexpr uint32_t kRearmHoldMs = 100;
constexpr float kRestDps = 5.0f;
constexpr uint32_t kBiasTauMs = 2000;
} // namespace

TwistDetector::TwistDetector()
    : head_(0), count_(0), integral_(0.0f), bias_(0.0f),
      fired_(false), lastFiredDir_(0), calmSinceMs_(0), lastSampleMs_(0) {
    for (int i = 0; i < kWindowSamples; ++i) {
        samples_[i] = {0, 0.0f, 0};
    }
}

void TwistDetector::popOld(uint32_t nowMs) {
    while (count_ > 0) {
        int tail = (head_ - count_ + kWindowSamples) % kWindowSamples;
        if (nowMs - samples_[tail].t <= kWindowMs) break;
        integral_ -= samples_[tail].gx * (float)samples_[tail].dt / 1000.0f;
        --count_;
    }
}

void TwistDetector::push(float gx, uint32_t dtMs, uint32_t nowMs) {
    samples_[head_] = {nowMs, gx, dtMs};
    integral_ += gx * (float)dtMs / 1000.0f;
    head_ = (head_ + 1) % kWindowSamples;
    if (count_ < kWindowSamples) {
        ++count_;
    } else {
        // Overwrote oldest sample; subtract its contribution.
        int tail = head_;
        integral_ -= samples_[tail].gx * (float)samples_[tail].dt / 1000.0f;
    }
}

int8_t TwistDetector::update(float gxDps, uint32_t nowMs) {
    uint32_t dtMs = 0;
    if (lastSampleMs_ != 0 && nowMs > lastSampleMs_) {
        dtMs = nowMs - lastSampleMs_;
    }
    lastSampleMs_ = nowMs;
    if (dtMs == 0) dtMs = 1;
    if (dtMs > 50) dtMs = 50; // clamp single gap

    // Update zero-rate bias while the device is at rest.
    float corrected = gxDps - bias_;
    if (std::fabs(corrected) < kRestDps) {
        float alpha = (float)dtMs / ((float)kBiasTauMs + (float)dtMs);
        bias_ += (gxDps - bias_) * alpha;
    }

    popOld(nowMs);
    push(corrected, dtMs, nowMs);

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
            return 1;
        }
        if (integral_ < -kFireThresholdDeg) {
            fired_ = true;
            lastFiredDir_ = -1;
            calmSinceMs_ = 0;
            return -1;
        }
    }
    return 0;
}

} // namespace sf
