// SPIKE S3a — six-pose IMU axis measurement.
// Active only when -DSF_PROBE == 3. Owned by probe_main.cpp's setup()/loop().
//
// Measures the sign and axis mapping of the BMI270 accelerometer by asking the
// operator to hold the device in six static poses. Outputs [G] lines at 10 Hz
// during each 3-second capture window and one [PROBE] s=3 summary per pose.
// The derived mapping is written to include/imu_axes.h (placeholder values when
// no device is attached for the build).
#if SF_PROBE == 3

#include <M5Unified.h>
#include <cmath>
#include <cstdint>

#include "hal/sticks3/buttons.h"
#include "hal/sticks3/clock.h"
#include "hal/sticks3/imu.h"
#include "hal/sticks3/panel.h"

namespace {

constexpr int64_t kWindowUs = (int64_t)3 * 1000 * 1000;
constexpr int64_t kGPeriodUs = 100 * 1000; // 10 Hz
constexpr int kMinSamples = 150;

const char* poseInstruction(int pose)
{
    switch (pose)
    {
        case 1:
            return "hold it flat and level, SCREEN FACING THE CEILING. Hold steady for 3 s, then press BLUE.";
        case 2:
            return "turn it over, still flat and level, SCREEN FACING THE FLOOR. Hold steady for 3 s, then press BLUE.";
        case 3:
            return "hold it upright like a phone, USB-C PORT POINTING AT THE FLOOR (let the cable hang down). Hold steady for 3 s, then press BLUE.";
        case 4:
            return "flip it end over end, USB-C PORT POINTING AT THE CEILING (cable now points up). Hold steady for 3 s, then press BLUE.";
        case 5:
            return "hold it on edge with the side that HAS THE SMALL RESET BUTTON pointing at the floor; that long side is POINTING AT THE FLOOR. Hold steady for 3 s, then press BLUE.";
        case 6:
            return "flip it over, so the side WITHOUT the reset button (the opposite long side) is POINTING AT THE FLOOR. Hold steady for 3 s, then press BLUE.";
        default:
            return "";
    }
}

struct PoseResult
{
    int pose = 0;
    float mean[3] = {0.0f, 0.0f, 0.0f};
    float mag = 0.0f;
    int dominantAxis = -1;
    int8_t dominantSign = 0;
    int samples = 0;
    bool ok = false;
    const char* reason = "";
};

PoseResult s_results[6];
int s_currentPose = 1;
bool s_windowActive = false;
bool s_drillComplete = false;
bool s_verdictPrinted = false;

int64_t s_windowStartUs = 0;
int64_t s_lastGUs = 0;
int64_t s_lastHoldSecondUs = 0;
int s_holdSecond = 0;

int s_samples = 0;
float s_sum[3] = {0.0f, 0.0f, 0.0f};
float s_min[3] = {0.0f, 0.0f, 0.0f};
float s_max[3] = {0.0f, 0.0f, 0.0f};
float s_lastAx = 0.0f;
float s_lastAy = 0.0f;
float s_lastAz = 0.0f;

bool s_bluePrev = false;
uint32_t s_blueLastChangeMs = 0;

void readAccel()
{
    sf_hal::probe::readAccel(&s_lastAx, &s_lastAy, &s_lastAz);
}

void resetWindow()
{
    s_windowActive = false;
    s_samples = 0;
    s_sum[0] = s_sum[1] = s_sum[2] = 0.0f;
    s_min[0] = s_min[1] = s_min[2] = 0.0f;
    s_max[0] = s_max[1] = s_max[2] = 0.0f;
}

void printPrompt()
{
    Serial.printf("[S3A] pose %d/6: %s\n", s_currentPose, poseInstruction(s_currentPose));
}

void printDrillStart()
{
    Serial.printf("[S3A] drill start: 6 poses. Follow the prompts. BLUE key advances.\n");
    Serial.printf("[S3A] hold each pose steady in your hand for 3 s — no need to set it down; the cable can hang.\n");
    printPrompt();
}

void startWindow(uint32_t nowMs)
{
    s_windowActive = true;
    s_windowStartUs = static_cast<int64_t>(sf_hal::nowUs());
    s_samples = 0;
    s_sum[0] = s_sum[1] = s_sum[2] = 0.0f;
    s_min[0] = s_min[1] = s_min[2] = 0.0f;
    s_max[0] = s_max[1] = s_max[2] = 0.0f;
    s_lastGUs = s_windowStartUs;
    s_lastHoldSecondUs = s_windowStartUs;
    s_holdSecond = 0;
    (void)nowMs;
    Serial.printf("[S3A] hold... 0s\n");
}

void accumulateSample()
{
    if (s_samples == 0)
    {
        s_min[0] = s_max[0] = s_lastAx;
        s_min[1] = s_max[1] = s_lastAy;
        s_min[2] = s_max[2] = s_lastAz;
    }
    else
    {
        if (s_lastAx < s_min[0]) { s_min[0] = s_lastAx; }
        if (s_lastAy < s_min[1]) { s_min[1] = s_lastAy; }
        if (s_lastAz < s_min[2]) { s_min[2] = s_lastAz; }
        if (s_lastAx > s_max[0]) { s_max[0] = s_lastAx; }
        if (s_lastAy > s_max[1]) { s_max[1] = s_lastAy; }
        if (s_lastAz > s_max[2]) { s_max[2] = s_lastAz; }
    }
    s_sum[0] += s_lastAx;
    s_sum[1] += s_lastAy;
    s_sum[2] += s_lastAz;
    ++s_samples;
}

void printG(int64_t nowUs)
{
    uint32_t ms = (uint32_t)(nowUs / 1000);
    Serial.printf("[G] t=%lu ax=%.3f ay=%.3f az=%.3f\n",
                  (unsigned long)ms,
                  (double)s_lastAx,
                  (double)s_lastAy,
                  (double)s_lastAz);
}

const char* axisName(int axis)
{
    switch (axis)
    {
        case 0: return "x";
        case 1: return "y";
        case 2: return "z";
        default: return "?";
    }
}

const char* signName(int8_t sign)
{
    if (sign > 0) { return "+"; }
    if (sign < 0) { return "-"; }
    return "?";
}

PoseResult computeResult()
{
    PoseResult r;
    r.pose = s_currentPose;
    r.samples = s_samples;
    if (s_samples == 0)
    {
        r.reason = "no samples";
        return r;
    }
    for (int i = 0; i < 3; ++i)
    {
        r.mean[i] = s_sum[i] / s_samples;
    }
    r.mag = std::sqrt(r.mean[0] * r.mean[0] +
                      r.mean[1] * r.mean[1] +
                      r.mean[2] * r.mean[2]);
    if (r.mag < 0.95f || r.mag > 1.05f)
    {
        r.reason = "magnitude not 1g";
        return r;
    }
    int dominant = -1;
    for (int i = 0; i < 3; ++i)
    {
        float a = std::fabs(r.mean[i]);
        if (a >= 0.95f)
        {
            if (dominant >= 0)
            {
                r.reason = "no single dominant axis";
                return r;
            }
            dominant = i;
        }
        else if (a >= 0.30f)
        {
            r.reason = "no single dominant axis";
            return r;
        }
    }
    if (dominant < 0)
    {
        r.reason = "no single dominant axis";
        return r;
    }
    if (s_samples < kMinSamples)
    {
        r.reason = "too few samples";
        return r;
    }
    r.dominantAxis = dominant;
    r.dominantSign = (r.mean[dominant] >= 0.0f) ? 1 : -1;
    r.ok = true;
    return r;
}

void printProbeLine(const PoseResult& r)
{
    Serial.printf("[PROBE] s=3 pose=%d mean_ax=%.3f mean_ay=%.3f mean_az=%.3f mag=%.3f dominant=%s sign=%s n=%d\n",
                  r.pose,
                  (double)r.mean[0],
                  (double)r.mean[1],
                  (double)r.mean[2],
                  (double)r.mag,
                  axisName(r.dominantAxis),
                  signName(r.dominantSign),
                  r.samples);
}

void finishPose()
{
    s_windowActive = false;
    PoseResult r = computeResult();
    s_results[s_currentPose - 1] = r;
    printProbeLine(r);
    if (r.ok)
    {
        Serial.printf("[S3A] pose %d: OK\n", s_currentPose);
        if (s_currentPose < 6)
        {
            ++s_currentPose;
            printPrompt();
        }
        else
        {
            s_drillComplete = true;
        }
    }
    else
    {
        Serial.printf("[S3A] pose %d: BAD — %s\n", s_currentPose, r.reason);
        Serial.printf("[S3A] repeat pose %d. %s\n", s_currentPose, poseInstruction(s_currentPose));
    }
}

bool checkAxisCoverage()
{
    int signCount[3][2] = {{0, 0}, {0, 0}, {0, 0}};
    for (int i = 0; i < 6; ++i)
    {
        const PoseResult& r = s_results[i];
        if (!r.ok) { return false; }
        int axis = r.dominantAxis;
        int sign = (r.dominantSign > 0) ? 1 : 0;
        if (axis < 0 || axis > 2) { return false; }
        ++signCount[axis][sign];
    }
    for (int a = 0; a < 3; ++a)
    {
        if (signCount[a][0] != 1 || signCount[a][1] != 1) { return false; }
    }
    return true;
}

void printSummary()
{
    Serial.printf("[S3A] drill complete\n");
    for (int i = 0; i < 6; ++i)
    {
        const PoseResult& r = s_results[i];
        Serial.printf("[S3A] pose %d mean=(%.3f,%.3f,%.3f) mag=%.3f dom=%s sign=%s n=%d ok=%s\n",
                      i + 1,
                      (double)r.mean[0],
                      (double)r.mean[1],
                      (double)r.mean[2],
                      (double)r.mag,
                      axisName(r.dominantAxis),
                      signName(r.dominantSign),
                      r.samples,
                      r.ok ? "yes" : "no");
    }
    bool allOk = true;
    for (int i = 0; i < 6; ++i)
    {
        if (!s_results[i].ok) { allOk = false; }
    }
    if (allOk && checkAxisCoverage())
    {
        Serial.printf("[S3A] VERDICT: PASS\n");
    }
    else if (!allOk)
    {
        Serial.printf("[S3A] VERDICT: FAIL — one or more poses did not satisfy the criteria\n");
    }
    else
    {
        Serial.printf("[S3A] VERDICT: FAIL — the six poses did not cover all three axes once positive and once negative\n");
    }
}

} // namespace

void sfProbeSetup()
{
    sf_hal::probe::display().setRotation(0);
    sf_hal::probe::display().fillScreen(TFT_BLACK);
    sf_hal::probe::display().setTextSize(2);
    sf_hal::probe::display().setCursor(4, 4);
    sf_hal::probe::display().print("S3A AXES");
    sf_hal::probe::display().setTextSize(1);
    sf_hal::probe::display().setCursor(4, 30);
    sf_hal::probe::display().print("see serial [G]/[PROBE]");
    printDrillStart();
}

void sfProbeReport()
{
    if (s_drillComplete && !s_verdictPrinted)
    {
        printSummary();
        s_verdictPrinted = true;
    }
    else if (!s_drillComplete)
    {
        printDrillStart();
    }
}

void sfProbeLoop()
{
    int64_t nowUs = static_cast<int64_t>(sf_hal::nowUs());
    uint32_t nowMs = (uint32_t)(nowUs / 1000);

    bool blueRaw = sf_hal::sampleButtons(nowMs).blue;
    if (blueRaw != s_bluePrev && (uint32_t)(nowMs - s_blueLastChangeMs) >= 8u)
    {
        s_bluePrev = blueRaw;
        s_blueLastChangeMs = nowMs;
        if (blueRaw && !s_windowActive && !s_drillComplete)
        {
            startWindow(nowMs);
        }
    }

    if (s_windowActive)
    {
        int64_t elapsedUs = nowUs - s_windowStartUs;
        if (elapsedUs < kWindowUs)
        {
            readAccel();
            accumulateSample();
            if (nowUs - s_lastGUs >= kGPeriodUs)
            {
                s_lastGUs = nowUs;
                printG(nowUs);
            }
            if (nowUs - s_lastHoldSecondUs >= 1000 * 1000)
            {
                s_lastHoldSecondUs += 1000 * 1000;
                ++s_holdSecond;
                Serial.printf("[S3A] hold... %ds\n", s_holdSecond);
            }
        }
        else
        {
            finishPose();
        }
    }

    if (s_drillComplete && !s_verdictPrinted)
    {
        printSummary();
        s_verdictPrinted = true;
    }
}

#endif
