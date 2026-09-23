#pragma once
#include "s3b_constants.h"
#include <imu_axes.h>
#include <M5Unified.h>
#include <cmath>
#include <cstdint>
#include <cstdio>

#include "hal/sticks3/clock.h"
#include "hal/sticks3/imu.h"

namespace sf {

enum class LrState { Neutral, Left, Right };
enum class SdState { Off, On };
enum class ActionKind : uint8_t { MoveLeft, MoveRight, SoftDropOn };

inline const char* actionName(ActionKind a) {
    switch (a) {
        case ActionKind::MoveLeft: return "MOVE_LEFT";
        case ActionKind::MoveRight: return "MOVE_RIGHT";
        case ActionKind::SoftDropOn: return "SOFT_DROP_ON";
    }
    return "UNKNOWN";
}

inline float median3(float a, float b, float c) {
    if (a > b) { float t = a; a = b; b = t; }
    if (b > c) { float t = b; b = c; c = t; }
    if (a > b) { float t = a; a = b; b = t; }
    return b;
}

class ProbeAutoShift {
 public:
    enum class Dir : int8_t { None = 0, Left = -1, Right = 1 };
    ProbeAutoShift(uint32_t dasMs, uint32_t arrMs)
        : das_(dasMs), arr_(arrMs == 0u ? 1u : arrMs), dir_(Dir::None),
          charged_(false), mark_(0) {}
    void reset() { dir_ = Dir::None; charged_ = false; mark_ = 0; }
    int update(Dir held, uint32_t nowMs) {
        if (held == Dir::None) { dir_ = Dir::None; return 0; }
        if (held != dir_) { dir_ = held; charged_ = false; mark_ = nowMs; return 1; }
        if (!charged_) {
            if ((uint32_t)(nowMs - mark_) >= das_) {
                charged_ = true; mark_ = mark_ + das_; return 1;
            }
            return 0;
        }
        int count = 0;
        while ((uint32_t)(nowMs - mark_) >= arr_) {
            ++count; mark_ = mark_ + arr_;
            if (count >= (int)kMaxMovesPerUpdate) { mark_ = nowMs; break; }
        }
        return count;
    }
 private:
    uint32_t das_, arr_;
    Dir dir_;
    bool charged_;
    uint32_t mark_;
};

class TinyRng {
 public:
    explicit TinyRng(uint32_t seed = 0x9E3779B9u) { x_ = seed ? seed : 0x9E3779B9u; }
    uint32_t next() { x_ ^= x_ << 13; x_ ^= x_ >> 17; x_ ^= x_ << 5; return x_; }
    uint32_t range(uint32_t lo, uint32_t hi) {
        if (hi <= lo) return lo;
        return lo + (next() % (hi - lo + 1));
    }
 private:
    uint32_t x_;
};

struct TiltPipeline {
    float ema[3] = {0,0,0};
    bool emaInit = false;
    int64_t lastSampleUs = 0;
    int64_t lastDtUs_ = 0;
    LrState lrState = LrState::Neutral;
    SdState sdState = SdState::Off;
    bool lrPending = false, sdPending = false;
    uint32_t lrDwellMarkMs = 0, sdDwellMarkMs = 0;
    ProbeAutoShift autoShift;
    uint32_t movesLeft = 0, movesRight = 0, softDrops = 0;
    uint32_t lrChanges = 0, repeats = 0;

    float rawHistory[3][kMedianWindow] = {};
    int rawIndex = 0;
    bool rawHistoryFull = false;

    float neutralLr = 0.0f, neutralSd = 0.0f;
    bool neutralValid = false, calibrating = false;
    uint32_t calibStartMs = 0;
    float calibSumLr = 0.0f, calibSumSd = 0.0f;
    int calibSamples = 0;
    float loggedNeutralLr = 0.0f, loggedNeutralSd = 0.0f;
    uint32_t neutralLogMarkMs = 0;

    float gyro[3] = {0,0,0};

    TiltPipeline(uint32_t dasMs, uint32_t arrMs) : autoShift(dasMs, arrMs) {}

    void reset() {
        emaInit = false; lastSampleUs = 0; lastDtUs_ = 0;
        lrState = LrState::Neutral; sdState = SdState::Off;
        lrPending = false; sdPending = false;
        autoShift.reset();
        movesLeft = movesRight = softDrops = 0;
        lrChanges = repeats = 0;
        neutralValid = false; calibrating = false;
        calibSumLr = calibSumSd = 0.0f; calibSamples = 0;
        loggedNeutralLr = loggedNeutralSd = 0.0f; neutralLogMarkMs = 0;
        rawIndex = 0; rawHistoryFull = false;
        gyro[0] = gyro[1] = gyro[2] = 0.0f;
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < kMedianWindow; ++j) rawHistory[i][j] = 0.0f;
        }
    }
    void startCalibration(uint32_t nowMs) {
        neutralValid = false; calibrating = true;
        calibStartMs = nowMs;
        calibSumLr = 0.0f; calibSumSd = 0.0f; calibSamples = 0;
    }
    void readAccel(float out[3]) { sf_hal::probe::readAccel(&out[0], &out[1], &out[2]); }
    // getGyro returns degrees per second (dps) at +/-2000 dps full scale.
    void readGyro(float out[3]) { sf_hal::probe::readGyro(&out[0], &out[1], &out[2]); }

    void updateEma(float raw[3], int64_t nowUs) {
        for (int i = 0; i < 3; ++i) rawHistory[i][rawIndex] = raw[i];
        rawIndex = (rawIndex + 1) % kMedianWindow;
        if (!rawHistoryFull && rawIndex == 0) rawHistoryFull = true;

        float median[3];
        if (rawHistoryFull) {
            for (int i = 0; i < 3; ++i) {
                median[i] = median3(rawHistory[i][0], rawHistory[i][1], rawHistory[i][2]);
            }
        } else {
            for (int i = 0; i < 3; ++i) median[i] = raw[i];
        }

        int64_t dtUs = nowUs - lastSampleUs;
        if (dtUs <= 0) dtUs = 1;
        if (dtUs > kDtMaxUs) dtUs = kDtMaxUs;
        lastDtUs_ = dtUs;
        float dt = (float)dtUs;
        float alpha = dt / ((float)kEmaRcUs + dt);
        if (!emaInit) {
            for (int i = 0; i < 3; ++i) ema[i] = median[i];
            emaInit = true;
        } else {
            for (int i = 0; i < 3; ++i)
                ema[i] = alpha * median[i] + (1.0f - alpha) * ema[i];
        }
        lastSampleUs = nowUs;
    }
    void emitTrace(ActionKind a, uint32_t nowMs) {
        Serial.printf("[TRACE] t=%lu scr=PLAYING act=%s src=TILT q=0/16 drop=0\n",
                      (unsigned long)nowMs, actionName(a));
    }
    void recordLrMoves(int count, bool right, uint32_t nowMs, bool stateChange) {
        if (count <= 0) return;
        if (right) { movesRight += count; }
        else { movesLeft += count; }
        if (stateChange) {
            ++lrChanges;
            emitTrace(right ? ActionKind::MoveRight : ActionKind::MoveLeft, nowMs);
            if (count > 1) repeats += (count - 1);
        } else {
            repeats += count;
        }
    }
    void driftNeutral(uint32_t nowMs, float lrRaw, float sdRaw, bool allowDrift) {
        if (!neutralValid || !allowDrift) return;
        if (lrState != LrState::Neutral || sdState != SdState::Off) return;
        float lr = lrRaw - neutralLr;
        float sd = sdRaw - neutralSd;
        if (std::fabs(lr) > kDeadZoneG || std::fabs(sd) > kDeadZoneG) return;
        float alpha = (float)lastDtUs_ / ((float)kNeutralDriftTauUs + (float)lastDtUs_);
        neutralLr += (lrRaw - neutralLr) * alpha;
        neutralSd += (sdRaw - neutralSd) * alpha;
        bool moved = std::fabs(neutralLr - loggedNeutralLr) > kNeutralLogThresholdG ||
                     std::fabs(neutralSd - loggedNeutralSd) > kNeutralLogThresholdG;
        bool due = (neutralLogMarkMs == 0) ||
                   ((uint32_t)(nowMs - neutralLogMarkMs) >= kNeutralLogMinIntervalMs);
        if (moved && due) {
            Serial.printf("[S3B] neutral lr=%.3f sd=%.3f\n", neutralLr, neutralSd);
            loggedNeutralLr = neutralLr;
            loggedNeutralSd = neutralSd;
            neutralLogMarkMs = nowMs;
        }
    }
    float lrSignal() const {
        if (!neutralValid) return 0.0f;
        return ema[kTiltAxisLeftRight] * (float)kTiltSignLeftRight - neutralLr;
    }
    float gxDps() const { return gyro[0]; }

    void update(uint32_t nowMs, bool allowDrift) {
        float raw[3]; readAccel(raw);
        readGyro(gyro);
        int64_t nowUs = static_cast<int64_t>(sf_hal::nowUs());
        updateEma(raw, nowUs);
        float lrRaw = ema[kTiltAxisLeftRight] * (float)kTiltSignLeftRight;
        float sdRaw = ema[kTiltAxisSoftDrop] * (float)kTiltSignSoftDrop;

        if (calibrating) {
            calibSumLr += lrRaw;
            calibSumSd += sdRaw;
            ++calibSamples;
            if ((uint32_t)(nowMs - calibStartMs) >= kNeutralCalibMs) {
                neutralLr = calibSumLr / calibSamples;
                neutralSd = calibSumSd / calibSamples;
                neutralValid = true; calibrating = false;
                loggedNeutralLr = neutralLr;
                loggedNeutralSd = neutralSd;
                Serial.printf("[S3B] neutral lr=%.3f sd=%.3f\n", neutralLr, neutralSd);
            }
            return;
        }

        driftNeutral(nowMs, lrRaw, sdRaw, allowDrift);

        float lr = lrRaw - neutralLr;
        float sd = sdRaw - neutralSd;

        LrState nextLr = lrState;
        if (lrState == LrState::Neutral) {
            if (std::fabs(lr) > kDeadZoneG) {
                if (!lrPending) { lrPending = true; lrDwellMarkMs = nowMs; }
                else if ((uint32_t)(nowMs - lrDwellMarkMs) >= kDwellMs) {
                    bool right = lr > 0.0f;
                    nextLr = right ? LrState::Right : LrState::Left;
                    lrPending = false;
                }
            } else { lrPending = false; }
        } else {
            float th = kDeadZoneG - kHysteresisG;
            bool right = (lrState == LrState::Right);
            bool fallOff = right ? (lr < th) : (lr > -th);
            if (fallOff) { nextLr = LrState::Neutral; lrPending = false; }
        }

        if (nextLr != lrState) {
            if (nextLr != LrState::Neutral) {
                bool right = (nextLr == LrState::Right);
                ProbeAutoShift::Dir d = right ? ProbeAutoShift::Dir::Right : ProbeAutoShift::Dir::Left;
                recordLrMoves(autoShift.update(d, nowMs), right, nowMs, true);
            } else {
                autoShift.reset();
            }
            lrState = nextLr;
        } else if (lrState != LrState::Neutral) {
            bool right = (lrState == LrState::Right);
            ProbeAutoShift::Dir d = right ? ProbeAutoShift::Dir::Right : ProbeAutoShift::Dir::Left;
            recordLrMoves(autoShift.update(d, nowMs), right, nowMs, false);
        }

        SdState nextSd = sdState;
        if (sdState == SdState::Off) {
            if (sd > kDeadZoneG) {
                if (!sdPending) { sdPending = true; sdDwellMarkMs = nowMs; }
                else if ((uint32_t)(nowMs - sdDwellMarkMs) >= kDwellMs) {
                    nextSd = SdState::On; sdPending = false;
                }
            } else { sdPending = false; }
        } else {
            float th = kDeadZoneG - kHysteresisG;
            if (sd < th) { nextSd = SdState::Off; sdPending = false; }
        }

        if (nextSd != sdState) {
            if (nextSd == SdState::On) {
                ++softDrops;
                emitTrace(ActionKind::SoftDropOn, nowMs);
            }
            sdState = nextSd;
        }
    }
};

} // namespace sf
