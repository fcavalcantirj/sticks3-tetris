// SPIKE S3b round 2 — four-drill build: T1 rest, T9 press-jolt, T7 twist, T6 absolute.
// Emits [RAW] and [BURST] streams for offline neutral-policy analysis.
// Historical round-2 build. SF_PROBE was moved to 99 when SPIKE S4 took over
// SF_PROBE=5; the source is kept verbatim as the record of the closed tilt spike.
#if SF_PROBE == 99

#include "s3b_constants_r2.h"
#include "s3b_pipeline_r2.h"
#include "s3b_twist_r2.h"
#include "s3b_t6_r2.h"
#include "s3b_t7_r2.h"
#include "s3b_render_r2.h"
#include "s3b_pipeline.h"  // TinyRng
#include <M5Unified.h>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdio>

#include "hal/sticks3/buttons.h"
#include "hal/sticks3/clock.h"
#include "hal/sticks3/panel.h"

namespace sf {

namespace {

TinyRng s_rng(0x7F4A3C2Du);
TiltPipelineR2 s_tilt;
T6StateR2 s_t6;
T7StateR2 s_t7;

Drill s_drill = Drill::BootWait;
int64_t s_drillStartUs = 0;
bool s_verdictPrinted = false;
bool s_summaryPrinted = false;

// T1 result.
int s_t1WanderMax = 0;
int s_t1ColChanges = 0;
float s_t1PpMed = 0.0f;
float s_t1PpP95 = 0.0f;
bool s_t1Ok = false;
bool s_t1Finished = false;
int s_lastT1Second = -1;

// T9 state.
struct T9StateR2 {
    int trial = 0;
    bool evaluating = false;
    uint32_t pressMs = 0;
    uint32_t evalEndMs = 0;
    float pressSig = 0.0f;
    int pressCol = 4;
    bool colChanged = false;
    float peakDelta = 0.0f;
    float peakDeltas[20] = {0};
    bool jolt[20] = {false};
};
T9StateR2 s_t9;
int s_t9Jolt = 0;
float s_t9PeakMed = 0.0f;
float s_t9PeakMax = 0.0f;
bool s_t9Finished = false;

// Buttons.
bool s_bluePrev = false, s_sidePrev = false;
uint32_t s_blueLastChangeMs = 0, s_sideLastChangeMs = 0;
uint32_t s_sideDownMs = 0;
bool s_sideHeldEmitted = false;

struct ButtonEventR2 { bool bluePress = false; bool sideHeld500 = false; };

ButtonEventR2 pollButtonsR2(uint32_t nowMs) {
    ButtonEventR2 ev;
    const sf_hal::RawButtons buttons = sf_hal::sampleButtons(nowMs);
    bool blueRaw = buttons.blue;
    bool sideRaw = buttons.side;
    if (blueRaw != s_bluePrev && (uint32_t)(nowMs - s_blueLastChangeMs) >= 8u) {
        s_bluePrev = blueRaw; s_blueLastChangeMs = nowMs;
        if (blueRaw) ev.bluePress = true;
    }
    if (sideRaw != s_sidePrev && (uint32_t)(nowMs - s_sideLastChangeMs) >= 8u) {
        s_sidePrev = sideRaw; s_sideLastChangeMs = nowMs;
        if (sideRaw) { s_sideDownMs = nowMs; s_sideHeldEmitted = false; }
        else { s_sideDownMs = 0; s_sideHeldEmitted = false; }
    }
    if (s_sidePrev && !s_sideHeldEmitted && s_sideDownMs != 0 &&
        (uint32_t)(nowMs - s_sideDownMs) >= kManualRezeroHoldMsR2) {
        s_sideHeldEmitted = true;
        ev.sideHeld500 = true;
    }
    return ev;
}

void printS3bLineR2(const char* stage);

void startDrillR2(Drill d) {
    s_drill = d;
    s_drillStartUs = static_cast<int64_t>(sf_hal::nowUs());
    uint32_t nowMs = (uint32_t)(s_drillStartUs / 1000);
    s_tilt.reset();
    s_tilt.startCalibration(nowMs);
    switch (d) {
        case Drill::T1Rest:
            s_t1Finished = false; s_lastT1Second = -1;
            Serial.printf("[S3B] T1/4 REST: hold neutral, do nothing for 60 s.\n");
            break;
        case Drill::T9PressJolt:
            s_t9 = T9StateR2{};
            s_t9Finished = false;
            Serial.printf("[S3B] T2/4 PRESS-JOLT: hold still, press BLUE 20 times.\n");
            break;
        case Drill::T7Twist:
            t7r2Reset(s_t7);
            Serial.printf("[S3B] T3/4 TWIST: gyro rotation channel. BLUE advances.\n");
            break;
        case Drill::T6Absolute:
            t6r2Reset(s_t6);
            Serial.printf("[S3B] T4/4 ABSOLUTE: 40 scored trials. Roll, then press BLUE.\n");
            break;
        default:
            break;
    }
}

void advanceDrillR2() {
    // Stage the pasteable line at every boundary: if the session is cut mid-drill the
    // owner still has one line carrying everything that completed.
    switch (s_drill) {
        case Drill::T1Rest:      printS3bLineR2("T1"); break;
        case Drill::T9PressJolt: printS3bLineR2("T9"); break;
        case Drill::T7Twist:     printS3bLineR2("T7"); break;
        case Drill::T6Absolute:  printS3bLineR2("T6"); break;
        default: break;
    }
    switch (s_drill) {
        case Drill::BootWait: startDrillR2(Drill::T1Rest); break;
        case Drill::T1Rest: startDrillR2(Drill::T9PressJolt); break;
        case Drill::T9PressJolt: startDrillR2(Drill::T7Twist); break;
        case Drill::T7Twist: startDrillR2(Drill::T6Absolute); break;
        case Drill::T6Absolute: s_drill = Drill::Done; break;
        case Drill::Done: startDrillR2(Drill::BootWait); break;
        default: break;
    }
}

float medianOfFloat(float* arr, int n) {
    if (n <= 0) return 0.0f;
    for (int i = 1; i < n; ++i) {
        float v = arr[i]; int j = i;
        while (j > 0 && arr[j-1] > v) { arr[j] = arr[j-1]; --j; }
        arr[j] = v;
    }
    return arr[n / 2];
}

void finalizeT9Trial(uint32_t nowMs) {
    if (!s_t9.evaluating) return;
    bool jolt = s_t9.colChanged;
    if (s_t9.trial > 0 && s_t9.trial <= 20) {
        s_t9.jolt[s_t9.trial - 1] = jolt;
        s_t9.peakDeltas[s_t9.trial - 1] = s_t9.peakDelta;
    }
    Serial.printf("[TRACE] t=%lu scr=PLAYING act=PRESS_JOLT src=BTN q=0/16 drop=0 trial=%d jolt=%d peak=%.4f\n",
                  (unsigned long)nowMs, s_t9.trial, jolt ? 1 : 0, s_t9.peakDelta);
    s_t9.evaluating = false;
    if (s_t9.trial >= 20) {
        s_t9Finished = true;
    }
}

void startT9Trial(uint32_t nowMs, int col) {
    if (s_t9.trial >= 20) return;
    ++s_t9.trial;
    s_t9.pressMs = nowMs;
    s_t9.evalEndMs = nowMs + 200;
    s_t9.pressSig = s_tilt.lrSignal();
    s_t9.pressCol = col;
    s_t9.colChanged = false;
    s_t9.peakDelta = 0.0f;
    s_t9.evaluating = true;
}

void updateT9(uint32_t nowMs, int col, bool bluePress) {
    if (s_t9Finished) return;
    if (bluePress) {
        finalizeT9Trial(nowMs);
        if (!s_t9Finished) {
            startT9Trial(nowMs, col);
            s_tilt.dumpBurst(s_t9.trial);
        }
    } else if (s_t9.evaluating) {
        float sig = s_tilt.lrSignal();
        float d = std::fabs(sig - s_t9.pressSig);
        if (d > s_t9.peakDelta) s_t9.peakDelta = d;
        if (col != s_t9.pressCol) s_t9.colChanged = true;
        if (nowMs >= s_t9.evalEndMs) {
            finalizeT9Trial(nowMs);
        }
    }
}

void printT9Summary() {
    int jolt = 0;
    float mx = 0.0f;
    float tmp[20];
    for (int i = 0; i < 20; ++i) {
        if (s_t9.jolt[i]) ++jolt;
        tmp[i] = s_t9.peakDeltas[i];
        if (s_t9.peakDeltas[i] > mx) mx = s_t9.peakDeltas[i];
    }
    float med = medianOfFloat(tmp, 20);
    bool ok = (jolt == 0);
    s_t9Jolt = jolt; s_t9PeakMed = med; s_t9PeakMax = mx;
    Serial.printf("[S3B] T9: jolt=%d/20 peak_med=%.4f peak_max=%.4f %s\n",
                  jolt, med, mx, ok ? "OK" : "BAD");
}

const char* verdictStringR2() {
    if (!s_t1Ok) return "FAIL:T1";
    if (s_t9Jolt != 0) return "FAIL:T9";
    if (!t7r2Pass(s_t7)) return "FAIL:T7";
    if (!t6r2Pass(s_t6)) return "FAIL:T6";
    return "PASS";
}

// One pasteable line carrying every drill's numbers. UAT-04 tells the owner to copy
// THIS line and spec.json task 14 parses its fields, so it must exist even when the
// session is cut early - hence `stage=`, which names the last drill that completed.
// ROUND 1 lost T1/T2/T4p2/T5 because every summary was gated behind Drill::Done.
void printS3bLineR2(const char* stage) {
    Serial.printf("S3B: stage=%s "
                  "T1 wander_max=%d colchanges=%d pp_med=%.4f pp_p95=%.4f "
                  "T9 jolt=%d/20 peak_med=%.4f peak_max=%.4f "
                  "T7 dir=%d/%d median=%lums colbleed=%d rotbleed_centre=%d rotbleed_edge=%d "
                  "T6 exact=%d/%d median=%lums ratio=%.2f "
                  "verdict=%s\n",
                  stage,
                  s_t1WanderMax, s_t1ColChanges, s_t1PpMed, s_t1PpP95,
                  s_t9Jolt, s_t9PeakMed, s_t9PeakMax,
                  s_t7.dirCorrect, kT7DirTrialsR2, (unsigned long)t7r2MedianMs(s_t7),
                  s_t7.colBleed, s_t7.rotBleedCentre, s_t7.rotBleedEdge,
                  s_t6.exact, kT6ScoredR2, (unsigned long)t6r2MedianMs(s_t6), t6r2Ratio(s_t6),
                  verdictStringR2());
}

void printSummaryR2() {
    if (s_summaryPrinted) return;
    s_summaryPrinted = true;
    Serial.printf("[S3B] drill complete\n");
    Serial.printf("[S3B] T1: wander_max=%d colchanges=%d pp_med=%.4f pp_p95=%.4f verdict=%s\n",
                  s_t1WanderMax, s_t1ColChanges, s_t1PpMed, s_t1PpP95,
                  s_t1Ok ? "OK" : "BAD");
    printT9Summary();
    t7r2PrintSummary(s_t7);
    t6r2PrintSummary(s_t6);
    Serial.printf("[S3B] VERDICT: %s\n", verdictStringR2());
    printS3bLineR2("DONE");
}

} // namespace

void sfProbeSetup() {
    sf_hal::probe::display().setRotation(0);
    sf_hal::probe::display().fillScreen(TFT_BLACK);
    initRenderR2();
    s_tilt.reset();
    s_drill = Drill::BootWait;
    s_verdictPrinted = false; s_summaryPrinted = false;
    t6r2Reset(s_t6); t7r2Reset(s_t7); s_t9 = T9StateR2{};
    s_t1Finished = false; s_t9Finished = false;
    Serial.printf("[S3B] grid x0=%d x9end=%d cols=%d w0=%d w9=%d panel=%d\n",
                  gridColXR2(0), gridColXR2(kGridColsR2), kGridColsR2,
                  gridColWR2(0), gridColWR2(kGridColsR2 - 1), kPanelWR2);
    Serial.printf("[S3B] round-2 drills: T1 REST -> T9 PRESS-JOLT -> T7 TWIST -> T6 ABSOLUTE. BLUE advances. SIDE held = rezero.\n");
    RenderInputsR2 in;
    in.drill = Drill::BootWait;
    renderDrillCueR2(0, in);
}

void sfProbeReport() {
    if (s_drill == Drill::Done && !s_verdictPrinted) {
        printSummaryR2(); s_verdictPrinted = true;
    } else {
        Serial.printf("[S3B] round-2 drills: T1 REST -> T9 PRESS-JOLT -> T7 TWIST -> T6 ABSOLUTE. BLUE advances. SIDE held = rezero.\n");
    }
}

void sfProbeLoop() {
    int64_t nowUs = static_cast<int64_t>(sf_hal::nowUs());
    uint32_t nowMs = (uint32_t)(nowUs / 1000);

    ButtonEventR2 ev = pollButtonsR2(nowMs);
    bool inT1 = (s_drill == Drill::T1Rest);
    int col = s_tilt.update(nowMs, ev.sideHeld500, inT1);

    if (ev.bluePress) {
        if (s_drill == Drill::T1Rest && !s_t1Finished) {
            // ignored: rest measurement must run full duration
        } else if (s_drill == Drill::T9PressJolt) {
            updateT9(nowMs, col, true);
        } else if (s_drill == Drill::T7Twist) {
            if (t7r2Update(s_t7, nowMs, s_tilt, s_rng, true)) {
                t7r2PrintSummary(s_t7);
                advanceDrillR2();
            }
        } else if (s_drill == Drill::T6Absolute) {
            // The +/-150 ms burst matters most HERE: T6's presses are where the
            // round-1 "+1 overshoot" lives, and the 10 Hz [RAW] stream is far too
            // coarse to resolve a press event.
            s_tilt.dumpBurst(s_t6.trial);
            if (t6r2Update(s_t6, nowMs, s_tilt, s_rng, true)) {
                t6r2PrintSummary(s_t6);
                advanceDrillR2();
            }
        } else {
            advanceDrillR2();
        }
    }

    switch (s_drill) {
        case Drill::T1Rest: {
            int64_t elapsedUs = nowUs - s_drillStartUs;
            int sec = (int)(elapsedUs / (1000 * 1000));
            if (sec != s_lastT1Second && sec <= 60) {
                s_lastT1Second = sec;
                if (sec % 10 == 0 || sec == 60) {
                    Serial.printf("[S3B] T1 hold... %ds\n", sec);
                }
            }
            if (!s_t1Finished && elapsedUs >= kT1RestUsR2) {
                s_tilt.t1Stats(s_t1WanderMax, s_t1ColChanges, s_t1PpMed, s_t1PpP95);
                s_t1Ok = (s_t1WanderMax == 0 && s_t1ColChanges == 0);
                Serial.printf("[S3B] T1: wander_max=%d colchanges=%d pp_med=%.4f pp_p95=%.4f verdict=%s\n",
                              s_t1WanderMax, s_t1ColChanges, s_t1PpMed, s_t1PpP95,
                              s_t1Ok ? "OK" : "BAD");
                s_t1Finished = true;
            }
            break;
        }
        case Drill::T9PressJolt:
            updateT9(nowMs, col, false);
            if (s_t9Finished) {
                printT9Summary();
                advanceDrillR2();
            }
            break;
        case Drill::T7Twist:
            t7r2Update(s_t7, nowMs, s_tilt, s_rng, false);
            break;
        case Drill::T6Absolute:
            t6r2Update(s_t6, nowMs, s_tilt, s_rng, false);
            break;
        default:
            break;
    }

    if (s_drill == Drill::Done && !s_verdictPrinted) {
        printSummaryR2(); s_verdictPrinted = true;
    }

    RenderInputsR2 in;
    in.drill = s_drill;
    in.drillStartUs = s_drillStartUs;
    in.t1Finished = s_t1Finished;
    in.t9Presses = s_t9.trial;
    in.t6TargetCol = s_t6.targetCol;
    in.t6SelectedCol = s_t6.selectedCol;
    in.t7Block = s_t7.block;
    in.t7Trial = s_t7.trial;
    in.t7Cue = t7r2CueText(s_t7);
    renderDrillCueR2(nowMs, in);
}

} // namespace sf

void sfProbeSetup() { sf::sfProbeSetup(); }
void sfProbeLoop() { sf::sfProbeLoop(); }
void sfProbeReport() { sf::sfProbeReport(); }

#endif
