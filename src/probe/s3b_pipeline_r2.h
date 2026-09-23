#pragma once
#include "s3b_constants_r2.h"
#include <imu_axes.h>
#include <M5Unified.h>
#include <cmath>
#include <cstdint>
#include <cstdio>

#include "hal/sticks3/clock.h"
#include "hal/sticks3/imu.h"

namespace sf {

inline float median3R2(float a, float b, float c) {
    if (a > b) { float t = a; a = b; b = t; }
    if (b > c) { float t = b; b = c; c = t; }
    if (a > b) { float t = a; a = b; b = t; }
    return b;
}

inline int countDistinct3(float a, float b, float c) {
    int d = 1;
    if (b != a) ++d;
    if (c != a && c != b) ++d;
    return d;
}

struct BurstSampleR2 {
    uint32_t t;
    float sig;
    int col;
};

struct TiltPipelineR2 {
    float ema[3] = {0,0,0};
    bool emaInit = false;
    int64_t lastSampleUs = 0;

    float rawHistory[3][kMedianWindowR2] = {};
    int rawIndex = 0;
    bool rawHistoryFull = false;

    float neutralLr = 0.0f, neutralSd = 0.0f;
    bool neutralValid = false, calibrating = false;
    uint32_t calibStartMs = 0;
    float calibSumLr = 0.0f, calibSumSd = 0.0f;
    int calibSamples = 0;

    bool snapFrozen = false;
    uint32_t snapMs = 0;

    // Quiescence window for the one-time neutral snap.
    float quietWindow[kQuietWindowSamplesR2] = {};
    int quietIndex = 0;
    bool quietWindowFull = false;
    uint32_t lastQuietSampleMs = 0;

    // T1 rest-noise peak-to-peak windows.
    float ppWindow[kT1PpWindowSamplesR2] = {};
    int ppIndex = 0;
    bool ppWindowFull = false;
    uint32_t lastPpSampleMs = 0;

    // Logging cadence.
    uint32_t lastRawLogMs = 0;
    uint32_t lastQuietLogMs = 0;
    uint32_t lastMedian3LogMs = 0;

    // Burst ring.
    BurstSampleR2 burst[kBurstRingSizeR2];
    int burstHead = 0;
    bool burstFull = false;
    uint32_t lastBurstSampleMs = 0;

    // Gyro.
    float gyro[3] = {0,0,0};

    // T1 accumulators.
    int t1CurrentCol = 4;
    int t1WanderMax = 0;
    int t1ColMin = 99;
    int t1ColMax = -99;
    int t1ColChanges = 0;
    float ppSamples[128] = {};
    int ppSampleCount = 0;

    void reset() {
        emaInit = false; lastSampleUs = 0;
        rawIndex = 0; rawHistoryFull = false;
        neutralValid = false; calibrating = false;
        calibSumLr = calibSumSd = 0.0f; calibSamples = 0;
        snapFrozen = false; snapMs = 0;
        quietIndex = 0; quietWindowFull = false; lastQuietSampleMs = 0;
        ppIndex = 0; ppWindowFull = false; lastPpSampleMs = 0;
        lastRawLogMs = 0; lastQuietLogMs = 0; lastMedian3LogMs = 0;
        burstHead = 0; burstFull = false; lastBurstSampleMs = 0;
        gyro[0] = gyro[1] = gyro[2] = 0.0f;
        t1CurrentCol = 4; t1WanderMax = 0; t1ColMin = 99; t1ColMax = -99; t1ColChanges = 0; ppSampleCount = 0;
        for (int i = 0; i < 3; ++i) {
            ema[i] = 0.0f;
            for (int j = 0; j < kMedianWindowR2; ++j) rawHistory[i][j] = 0.0f;
        }
        for (int i = 0; i < kQuietWindowSamplesR2; ++i) quietWindow[i] = 0.0f;
        for (int i = 0; i < kT1PpWindowSamplesR2; ++i) ppWindow[i] = 0.0f;
        for (int i = 0; i < kBurstRingSizeR2; ++i) burst[i] = {0, 0.0f, 0};
        for (int i = 0; i < 128; ++i) ppSamples[i] = 0.0f;
    }

    void startCalibration(uint32_t nowMs) {
        neutralValid = false; calibrating = true;
        calibStartMs = nowMs;
        calibSumLr = 0.0f; calibSumSd = 0.0f; calibSamples = 0;
        snapFrozen = false; snapMs = 0;
    }

    void readAccel(float out[3]) { sf_hal::probe::readAccel(&out[0], &out[1], &out[2]); }
    // getGyro returns degrees per second (dps) at +/-2000 dps full scale.
    void readGyro(float out[3]) { sf_hal::probe::readGyro(&out[0], &out[1], &out[2]); }

    float lrEma() const { return ema[kTiltAxisLeftRight] * (float)kTiltSignLeftRight; }
    float sdEma() const { return ema[kTiltAxisSoftDrop] * (float)kTiltSignSoftDrop; }
    float lrSignal() const { return lrEma() - neutralLr; }
    float sdSignal() const { return sdEma() - neutralSd; }
    float gxDps() const { return gyro[0]; }

    void updateEma(float raw[3], int64_t nowUs) {
        for (int i = 0; i < 3; ++i) rawHistory[i][rawIndex] = raw[i];
        rawIndex = (rawIndex + 1) % kMedianWindowR2;
        if (!rawHistoryFull && rawIndex == 0) rawHistoryFull = true;

        float median[3];
        int axis = kTiltAxisLeftRight;
        if (rawHistoryFull) {
            for (int i = 0; i < 3; ++i) {
                median[i] = median3R2(rawHistory[i][0], rawHistory[i][1], rawHistory[i][2]);
            }
        } else {
            for (int i = 0; i < 3; ++i) median[i] = raw[i];
        }

        int64_t dtUs = nowUs - lastSampleUs;
        if (dtUs <= 0) dtUs = 1;
        if (dtUs > kDtMaxUsR2) dtUs = kDtMaxUsR2;
        float dt = (float)dtUs;
        float alpha = dt / ((float)kEmaRcUsR2 + dt);
        if (!emaInit) {
            for (int i = 0; i < 3; ++i) ema[i] = median[i];
            emaInit = true;
        } else {
            for (int i = 0; i < 3; ++i)
                ema[i] = alpha * median[i] + (1.0f - alpha) * ema[i];
        }
        lastSampleUs = nowUs;

        if ((uint32_t)(nowUs / 1000) - lastMedian3LogMs >= kMedian3LogIntervalMsR2) {
            lastMedian3LogMs = (uint32_t)(nowUs / 1000);
            int distinct = countDistinct3(rawHistory[axis][0], rawHistory[axis][1], rawHistory[axis][2]);
            Serial.printf("[S3B] median3 distinct=%d/3\n", distinct);
        }
    }

    void pushQuiet(float lr, uint32_t nowMs) {
        if (lastQuietSampleMs == 0 || (uint32_t)(nowMs - lastQuietSampleMs) >= kQuietSampleMsR2) {
            lastQuietSampleMs = nowMs;
            quietWindow[quietIndex] = lr;
            quietIndex = (quietIndex + 1) % kQuietWindowSamplesR2;
            if (!quietWindowFull && quietIndex == 0) quietWindowFull = true;
        }
    }

    float quietPp() const {
        int n = quietWindowFull ? kQuietWindowSamplesR2 : quietIndex;
        if (n <= 0) return 999.0f;
        float mn = quietWindow[0], mx = quietWindow[0];
        for (int i = 1; i < n; ++i) {
            if (quietWindow[i] < mn) mn = quietWindow[i];
            if (quietWindow[i] > mx) mx = quietWindow[i];
        }
        return mx - mn;
    }

    void trySnap(float lrRaw, uint32_t nowMs) {
        if (snapFrozen) return;
        pushQuiet(lrRaw, nowMs);
        if (!quietWindowFull) return;
        if (quietPp() < kQuietPpGR2) {
            neutralLr = lrRaw;
            snapFrozen = true;
            snapMs = nowMs;
            Serial.printf("[S3B] snap neutral=%.4f\n", neutralLr);
        }
    }

    void recaptureNeutral(uint32_t nowMs) {
        neutralLr = lrEma();
        neutralSd = sdEma();
        neutralValid = true;
        snapFrozen = true;
        snapMs = nowMs;
        // Reset quiescence window so a later accidental un-snap cannot drift it.
        quietIndex = 0; quietWindowFull = false; lastQuietSampleMs = 0;
        Serial.printf("[S3B] rezero neutral=%.4f\n", neutralLr);
    }

    void logQuiet(uint32_t nowMs) {
        if (lastQuietLogMs == 0 || (uint32_t)(nowMs - lastQuietLogMs) >= kQuietLogIntervalMsR2) {
            lastQuietLogMs = nowMs;
            int n = quietWindowFull ? kQuietWindowSamplesR2 : quietIndex;
            uint32_t age = snapMs ? (nowMs - snapMs) : 0;
            Serial.printf("[S3B] quiet n=%d age=%lums pp=%.4f neutral=%.4f\n",
                          n, (unsigned long)age, quietPp(), neutralLr);
        }
    }

    void pushPp(float lr, uint32_t nowMs) {
        if (lastPpSampleMs == 0 || (uint32_t)(nowMs - lastPpSampleMs) >= kT1PpSampleMsR2) {
            lastPpSampleMs = nowMs;
            ppWindow[ppIndex] = lr;
            ppIndex = (ppIndex + 1) % kT1PpWindowSamplesR2;
            if (!ppWindowFull && ppIndex == 0) ppWindowFull = true;
        }
    }

    float windowPp() const {
        int n = ppWindowFull ? kT1PpWindowSamplesR2 : ppIndex;
        if (n <= 0) return 0.0f;
        float mn = ppWindow[0], mx = ppWindow[0];
        for (int i = 1; i < n; ++i) {
            if (ppWindow[i] < mn) mn = ppWindow[i];
            if (ppWindow[i] > mx) mx = ppWindow[i];
        }
        return mx - mn;
    }

    void recordT1(int col, uint32_t nowMs) {
        // Reference-free: the symmetric mapping has no unique home column (rest sits on
        // the 4/5 boundary), so wander is the spread of columns visited, not a distance
        // from a hardcoded centre. Bar is still 0.
        if (col < t1ColMin) t1ColMin = col;
        if (col > t1ColMax) t1ColMax = col;
        int wander = t1ColMax - t1ColMin;
        if (wander > t1WanderMax) t1WanderMax = wander;
        if (col != t1CurrentCol) { t1CurrentCol = col; ++t1ColChanges; }
        pushPp(lrSignal(), nowMs);
        int n = ppWindowFull ? kT1PpWindowSamplesR2 : ppIndex;
        if (ppWindowFull) {
            if (ppSampleCount < 128) ppSamples[ppSampleCount++] = windowPp();
        }
    }

    void pushBurst(uint32_t nowMs) {
        if (lastBurstSampleMs == 0 || (uint32_t)(nowMs - lastBurstSampleMs) >= kBurstSampleMsR2) {
            lastBurstSampleMs = nowMs;
            BurstSampleR2 s = {nowMs, lrSignal(), t1CurrentCol};
            burst[burstHead] = s;
            burstHead = (burstHead + 1) % kBurstRingSizeR2;
            if (!burstFull && burstHead == 0) burstFull = true;
        }
    }

    void dumpBurst(int trial) const {
        int n = burstFull ? kBurstRingSizeR2 : burstHead;
        if (n <= 0) return;
        int estimated = n * 55;
        if (Serial.availableForWrite() < estimated) return;
        for (int i = 0; i < n; ++i) {
            int idx = (burstHead - n + i + kBurstRingSizeR2) % kBurstRingSizeR2;
            const BurstSampleR2& s = burst[idx];
            Serial.printf("[BURST] n=%d i=%d t=%lu sig=%.4f col=%d\n",
                          trial, i, (unsigned long)s.t, s.sig, s.col);
        }
    }

    void logRaw(uint32_t nowMs) {
        if (lastRawLogMs == 0 || (uint32_t)(nowMs - lastRawLogMs) >= kRawLogIntervalMsR2) {
            lastRawLogMs = nowMs;
            Serial.printf("[RAW] t=%lu ay=%.4f ax=%.4f gx=%.2f neutral=%.4f sig=%.4f col=%d\n",
                          (unsigned long)nowMs, lrEma(), sdEma(), gxDps(),
                          neutralLr, lrSignal(), t1CurrentCol);
        }
    }

    // Main update. Call once per loop. sideHeld500 is true when the SIDE key
    // has been continuously pressed for kManualRezeroHoldMsR2.
    // inT1 enables the T1 rest-noise accumulators.
    // Returns the absolute column selected by the current signal.
    int update(uint32_t nowMs, bool sideHeld500, bool inT1) {
        float raw[3]; readAccel(raw);
        readGyro(gyro);
        int64_t nowUs = static_cast<int64_t>(sf_hal::nowUs());
        updateEma(raw, nowUs);

        if (calibrating) {
            calibSumLr += lrEma();
            calibSumSd += sdEma();
            ++calibSamples;
            if ((uint32_t)(nowMs - calibStartMs) >= kNeutralCalibMsR2) {
                neutralLr = calibSumLr / calibSamples;
                neutralSd = calibSumSd / calibSamples;
                neutralValid = true; calibrating = false;
                Serial.printf("[S3B] neutral lr=%.4f\n", neutralLr);
            }
            t1CurrentCol = 4;
            pushBurst(nowMs);
            return 4;
        }

        if (sideHeld500) {
            recaptureNeutral(nowMs);
        }

        if (neutralValid) {
            trySnap(lrEma(), nowMs);
            logQuiet(nowMs);
        }

        // Absolute column for steering, crosstalk and logging.
        float sig = lrSignal();
        int col = 4;
        if (neutralValid) {
            col = (int)(sig >= 0.0f ? (sig / kGPerColumnR2 + 0.5f)
                                    : (sig / kGPerColumnR2 - 0.5f));
            if (col < 0) col = 0;
            if (col >= kGridColsR2) col = kGridColsR2 - 1;
        }
        t1CurrentCol = col;

        if (inT1 && neutralValid) {
            recordT1(col, nowMs);
        }

        logRaw(nowMs);
        pushBurst(nowMs);
        return col;
    }

    void t1Stats(int& wanderMax, int& colChanges, float& ppMed, float& ppP95) const {
        wanderMax = t1WanderMax;
        colChanges = t1ColChanges;
        if (ppSampleCount <= 0) { ppMed = 0.0f; ppP95 = 0.0f; return; }
        float tmp[128];
        for (int i = 0; i < ppSampleCount; ++i) tmp[i] = ppSamples[i];
        for (int i = 1; i < ppSampleCount; ++i) {
            float v = tmp[i]; int j = i;
            while (j > 0 && tmp[j-1] > v) { tmp[j] = tmp[j-1]; --j; }
            tmp[j] = v;
        }
        ppMed = tmp[ppSampleCount / 2];
        int idx = ppSampleCount * 95 / 100;
        if (idx >= ppSampleCount) idx = ppSampleCount - 1;
        ppP95 = tmp[idx];
    }
};

} // namespace sf
