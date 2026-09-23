// SPIKE S3b — tilt playability drills. Active only when -DSF_PROBE == 4.
//
// T1..T5 run the incremental tilt pipeline (accel in G, median filter, EMA,
// CALIBRATION-1 dead zone / hysteresis, adaptive neutral drift).
// T6 tests absolute angle-to-column steering.
// T7 tests the gyro twist channel for IMU-only rotation.
//
// getGyro is called inside TiltPipeline::update; it returns dps (degrees per
// second) at a +/-2000 dps full scale, NOT rad/s and NOT raw ADC.
//
// Summary lines emitted by this probe:
// [S3B] T6: exact=<n>/20 median=<ms>ms p95=<ms>ms near=<ms>ms far=<ms>ms ratio=<f> <OK|BAD>
// [S3B] T7: dir=<n>/20 median=<ms>ms colbleed=<n> rotbleed_centre=<n> rotbleed_edge=<n> <OK|BAD>
#if SF_PROBE == 4

#include "s3b_drills.h"
#include "s3b_pipeline.h"
#include "s3b_t6.h"
#include "s3b_t7.h"
#include "s3b_render.h"
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

// TILT-SPECIFIC repeat constants. The button constants 167/33 assume a release
// measured in milliseconds; a wrist returning from a tilt takes about 200 ms,
// so a 33 ms repeat emits six moves during the return. The press edge still
// fires exactly one move, unchanged.
constexpr uint32_t kTiltDasMs = 400;
constexpr uint32_t kTiltArrMs = 250;

// ABSOLUTE-AIM path: accelerometer reading is in G and the Y axis reads
// approximately sin(roll). 0.09 G is about 5.2 degrees per column, so the full
// ten-column span is about +/-24 degrees of wrist roll. Hysteresis is a
// Schmitt band on the quantiser boundary, about 22 percent of a column.
constexpr float kGPerColumn = 0.09f;
constexpr float kColHysteresisG = 0.02f;

// Drill timing.
constexpr int64_t kT1RestUs = 60 * 1000 * 1000LL;
constexpr int64_t kT2PromptMinUs = 1500 * 1000LL;
constexpr int64_t kT2PromptMaxUs = 3000 * 1000LL;
constexpr uint32_t kT2LatencyTimeoutMs = 3000;
constexpr uint32_t kT3NeutralStableMs = 800;
constexpr uint32_t kT3TrialTimeoutMs = 5000;
constexpr int kT2PromptCount = 20;
constexpr int kT3PromptCount = 20;
constexpr int kT4MoveCount = 20;
constexpr int64_t kT5DurationUs = 3 * 60 * 1000 * 1000LL;
constexpr int64_t kT5ReportIntervalUs = 30 * 1000 * 1000LL;

static_assert(kGridCols * kGridCellSize == 100, "ten-column grid must span 100 px");

TiltPipeline s_tilt(kTiltDasMs, kTiltArrMs);
TinyRng s_rng(0x7F4A3C2Du);
Drill s_drill = Drill::BootWait;
int s_drillSub = 0;
bool s_t1Finished = false;

// Saved per-drill results for final verdict.
uint32_t s_t1Moves = 0, s_t1Soft = 0;
int s_t2Correct = 0, s_t2LatencyCount = 0;
uint32_t s_t2LatenciesMs[kT2PromptCount] = {0};
bool s_t2ExpectedRight[kT2PromptCount] = {false};
int s_t3Exact = 0;
int s_t4Unsoft = 0, s_t4Unmove = 0;
int s_t5Markers = 0;
T6State s_t6;
T7State s_t7;

// Transient drill state.
uint32_t s_t2PromptTimeMs = 0;
bool s_t2AwaitingMove = false;
uint32_t s_t2MovesLeftAtPrompt = 0, s_t2MovesRightAtPrompt = 0;
int s_t3StartCol = 5;
int s_t3TargetCol = -1;
int s_t3SignedTarget = 0;
bool s_t3MoveRight = false;
int s_t3MoveCount = 0;
uint32_t s_t3MoveStartMs = 0, s_t3LastMoveMs = 0;
bool s_t3TrialActive = false;
uint32_t s_t3MovesLeftAtPrompt = 0, s_t3MovesRightAtPrompt = 0;
int64_t s_drillStartUs = 0, s_t5ReportUs = 0;
bool s_verdictPrinted = false, s_summaryPrinted = false;
bool s_t2CurrentExpectedRight = false;
int s_t3CurrentTargetCols = 0;

// Static drill helpers.
int s_lastT1Second = -1;
int64_t s_t2NextPromptUs = 0;

// Debounced buttons.
bool s_bluePrev = false, s_sidePrev = false;
uint32_t s_blueLastChangeMs = 0, s_sideLastChangeMs = 0;

struct ButtonEvent { bool bluePress = false, sidePress = false; };

ButtonEvent pollButtons(uint32_t nowMs) {
    ButtonEvent ev;
    const sf_hal::RawButtons buttons = sf_hal::sampleButtons(nowMs);
    bool blueRaw = buttons.blue;
    bool sideRaw = buttons.side;
    if (blueRaw != s_bluePrev && (uint32_t)(nowMs - s_blueLastChangeMs) >= 8u) {
        s_bluePrev = blueRaw; s_blueLastChangeMs = nowMs;
        if (blueRaw) ev.bluePress = true;
    }
    if (sideRaw != s_sidePrev && (uint32_t)(nowMs - s_sideLastChangeMs) >= 8u) {
        s_sidePrev = sideRaw; s_sideLastChangeMs = nowMs;
        if (sideRaw) ev.sidePress = true;
    }
    return ev;
}

void printPrompt(const char* msg) { Serial.printf("[S3B] %s\n", msg); }

uint32_t medianOf(uint32_t* arr, int n) {
    if (n <= 0) return 0;
    for (int i = 1; i < n; ++i) {
        uint32_t v = arr[i]; int j = i;
        while (j > 0 && arr[j-1] > v) { arr[j] = arr[j-1]; --j; }
        arr[j] = v;
    }
    return arr[n / 2];
}

uint32_t p95Of(uint32_t* arr, int n) {
    if (n <= 0) return 0;
    for (int i = 1; i < n; ++i) {
        uint32_t v = arr[i]; int j = i;
        while (j > 0 && arr[j-1] > v) { arr[j] = arr[j-1]; --j; }
        arr[j] = v;
    }
    uint32_t idx = (uint32_t)n * 95u / 100u;
    if (idx >= (uint32_t)n) idx = n - 1;
    return arr[idx];
}

int clampCol(int c) {
    if (c < 0) return 0;
    if (c >= kGridCols) return kGridCols - 1;
    return c;
}

bool t2Pass() {
    return s_t2LatencyCount == kT2PromptCount && s_t2Correct == kT2PromptCount &&
           medianOf(s_t2LatenciesMs, s_t2LatencyCount) <= 400 &&
           p95Of(s_t2LatenciesMs, s_t2LatencyCount) <= 700;
}
bool t3Pass() { return s_t3Exact >= 18; }
bool t4Pass() { return s_t4Unsoft == 0 && s_t4Unmove <= 1; }
bool t5Pass() { return s_t5Markers <= 6; }

const char* verdictString() {
    if (s_t1Moves != 0 || s_t1Soft != 0) return "FAIL:T1";
    if (!t2Pass()) return "FAIL:T2";
    if (!t3Pass()) return "FAIL:T3";
    if (!t4Pass()) return "FAIL:T4";
    if (!t5Pass()) return "FAIL:T5";
    if (!t6Pass(s_t6)) return "FAIL:T6";
    if (!t7Pass(s_t7)) return "FAIL:T7";
    return "PASS";
}

void printSummary() {
    if (s_summaryPrinted) return;
    s_summaryPrinted = true;
    Serial.printf("[S3B] drill complete\n");
    Serial.printf("[PROBE] s=4 T1 moves=%lu repeats=%lu softdrops=%lu\n",
                  (unsigned long)s_tilt.lrChanges, (unsigned long)s_tilt.repeats,
                  (unsigned long)s_tilt.softDrops);
    Serial.printf("[PROBE] s=4 T2 median=%lums p95=%lums correct=%d/%d\n",
                  (unsigned long)medianOf(s_t2LatenciesMs, s_t2LatencyCount),
                  (unsigned long)p95Of(s_t2LatenciesMs, s_t2LatencyCount), s_t2Correct, kT2PromptCount);
    Serial.printf("[PROBE] s=4 T3 exact=%d/%d\n", s_t3Exact, kT3PromptCount);
    Serial.printf("[PROBE] s=4 T4 unsoft=%d unmove=%d\n", s_t4Unsoft, s_t4Unmove);
    Serial.printf("[PROBE] s=4 T5 markers=%d minutes=3\n", s_t5Markers);
    t6PrintSummary(s_t6);
    t7PrintSummary(s_t7);
    Serial.printf("[S3B] VERDICT: %s\n", verdictString());
    // UAT-04 tells the owner to copy THIS one line, and task 14 parses its
    // fields, so every drill's numbers must appear here - not only in the
    // separate [S3B] T6:/T7: lines.
    Serial.printf("S3B: T1 moves=%lu softdrops=%lu T2 median=%lums p95=%lums correct=%d/%d T3 exact=%d/%d T4 unsoft=%d unmove=%d T5 markers=%d minutes=3 T6 exact=%d/20 median=%lums ratio=%.2f T7 dir=%d/20 median=%lums colbleed=%d rotbleed_centre=%d rotbleed_edge=%d verdict=%s\n",
                  (unsigned long)s_t1Moves, (unsigned long)s_t1Soft,
                  (unsigned long)medianOf(s_t2LatenciesMs, s_t2LatencyCount),
                  (unsigned long)p95Of(s_t2LatenciesMs, s_t2LatencyCount),
                  s_t2Correct, kT2PromptCount, s_t3Exact, kT3PromptCount,
                  s_t4Unsoft, s_t4Unmove, s_t5Markers,
                  s_t6.exact, (unsigned long)t6MedianMs(s_t6), t6Ratio(s_t6),
                  s_t7.dirCorrect, (unsigned long)t7MedianMs(s_t7),
                  s_t7.colBleed, s_t7.rotBleedCentre, s_t7.rotBleedEdge,
                  verdictString());
}

void issueT2Prompt(uint32_t nowMs) {
    bool right = (s_rng.next() & 1u) != 0u;
    if (s_drillSub < kT2PromptCount) s_t2ExpectedRight[s_drillSub] = right;
    s_t2CurrentExpectedRight = right;
    s_t2PromptTimeMs = nowMs;
    s_t2AwaitingMove = true;
    s_t2MovesLeftAtPrompt = s_tilt.movesLeft;
    s_t2MovesRightAtPrompt = s_tilt.movesRight;
    Serial.printf("[PROMPT] n=%d dir=%s\n", s_drillSub + 1, right ? "R" : "L");
}

void issueT3Prompt(uint32_t nowMs) {
    int cols = (int)s_rng.range(1, 4);
    bool right = (s_rng.next() & 1u) != 0u;
    int signedTarget = right ? cols : -cols;
    int target = s_t3StartCol + signedTarget;
    if (target < 0) { target = 0; signedTarget = target - s_t3StartCol; }
    if (target >= kGridCols) { target = kGridCols - 1; signedTarget = target - s_t3StartCol; }
    s_t3TargetCol = target;
    s_t3SignedTarget = signedTarget;
    s_t3MoveRight = right;
    s_t3CurrentTargetCols = std::abs(signedTarget);
    s_t3MoveCount = 0;
    s_t3TrialActive = true;
    s_t3MoveStartMs = nowMs;
    s_t3LastMoveMs = nowMs;
    s_t3MovesLeftAtPrompt = s_tilt.movesLeft;
    s_t3MovesRightAtPrompt = s_tilt.movesRight;
    Serial.printf("[PROMPT] n=%d dir=%s cols=%d\n", s_drillSub + 1, right ? "R" : "L", s_t3CurrentTargetCols);
}

void confirmT3Trial(uint32_t nowMs, bool timeout) {
    if (!s_t3TrialActive) return;
    int landed = clampCol(s_t3StartCol +
                          (int)(s_tilt.movesRight - s_t3MovesRightAtPrompt) -
                          (int)(s_tilt.movesLeft - s_t3MovesLeftAtPrompt));
    bool ok = (landed == s_t3TargetCol);
    if (ok) ++s_t3Exact;
    Serial.printf("[S3B] T3 n=%d target=%d landed=%d %s%s\n",
                  s_drillSub + 1, s_t3TargetCol, landed, ok ? "OK" : "BAD",
                  timeout ? " (timeout)" : "");
    s_t3TrialActive = false;
    ++s_drillSub;
}

void startDrill(Drill d) {
    s_drill = d; s_drillSub = 0;
    s_drillStartUs = static_cast<int64_t>(sf_hal::nowUs());
    s_tilt.reset();
    s_t1Finished = false;
    s_t2AwaitingMove = false; s_t3TrialActive = false;
    s_lastT1Second = -1; s_t2NextPromptUs = 0;
    uint32_t nowMs = (uint32_t)(s_drillStartUs / 1000);
    s_tilt.startCalibration(nowMs);
    switch (d) {
        case Drill::T1Rest:
            printPrompt("T1/7 REST: hold the device in a neutral grip and deliberately do nothing for 60 s.");
            printPrompt("T1: BLUE advances between drills; SIDE marks unintended moves in T5 only.");
            printPrompt("T1: hold... 0s");
            break;
        case Drill::T2Intent:
            s_t2Correct = 0; s_t2LatencyCount = 0;
            printPrompt("T2/7 INTENT: 20 prompts. Tilt the way [PROMPT] says, then return to neutral.");
            break;
        case Drill::T3Precision:
            s_t3Exact = 0;
            printPrompt("T3/7 PRECISION: 20 prompts. Tilt to move the marker to the target column, then press BLUE.");
            break;
        case Drill::T4CrossH:
            s_t4Unsoft = 0;
            printPrompt("T4/7 CROSS-AXIS phase 1: tilt left/right 20 times with NO soft drop.");
            break;
        case Drill::T4CrossV:
            s_t4Unmove = 0;
            printPrompt("T4/7 CROSS-AXIS phase 2: tip the top away 20 times with NO horizontal movement.");
            break;
        case Drill::T5Sustained:
            s_t5Markers = 0; s_t5ReportUs = s_drillStartUs;
            printPrompt("T5/7 SUSTAINED: play-like tilting for 3 min. Press SIDE every time the device does something unintended.");
            break;
        case Drill::T6Absolute:
            t6Reset(s_t6);
            printPrompt("T6/7 ABSOLUTE: 20 trials. Roll until the outline covers the filled cell, then press BLUE.");
            break;
        case Drill::T7Twist:
            t7Reset(s_t7);
            printPrompt("T7/7 TWIST: gyro rotation channel. Three blocks, BLUE advances.");
            break;
        default: break;
    }
}

void advanceDrill() {
    switch (s_drill) {
        case Drill::BootWait: startDrill(Drill::T1Rest); break;
        case Drill::T1Rest: startDrill(Drill::T2Intent); break;
        case Drill::T2Intent: startDrill(Drill::T3Precision); break;
        case Drill::T3Precision: startDrill(Drill::T4CrossH); break;
        case Drill::T4CrossH: startDrill(Drill::T4CrossV); break;
        case Drill::T4CrossV: startDrill(Drill::T5Sustained); break;
        case Drill::T5Sustained: startDrill(Drill::T6Absolute); break;
        case Drill::T6Absolute: startDrill(Drill::T7Twist); break;
        case Drill::T7Twist: s_drill = Drill::Done; break;
        case Drill::Done: startDrill(Drill::BootWait); break;
    }
}

int t2MarkerCol() {
    int delta = (int)(s_tilt.movesRight - s_t2MovesRightAtPrompt) -
                (int)(s_tilt.movesLeft - s_t2MovesLeftAtPrompt);
    return clampCol(s_t3StartCol + delta);
}

int t3MarkerCol() {
    int delta = (int)(s_tilt.movesRight - s_t3MovesRightAtPrompt) -
                (int)(s_tilt.movesLeft - s_t3MovesLeftAtPrompt);
    return clampCol(s_t3StartCol + delta);
}

} // namespace

void sfProbeSetup() {
    sf_hal::probe::display().setRotation(0);
    sf_hal::probe::display().fillScreen(TFT_BLACK);
    initRender();
    s_tilt.reset();
    s_drill = Drill::BootWait;
    s_verdictPrinted = false; s_summaryPrinted = false;
    s_t1Finished = false;
    s_t1Moves = s_t1Soft = 0;
    s_t2Correct = s_t2LatencyCount = s_t3Exact = 0;
    s_t4Unsoft = s_t4Unmove = s_t5Markers = 0;
    t6Reset(s_t6);
    t7Reset(s_t7);
    Serial.printf("[S3B] grid x0=%d cellw=%d cells=%d x1=%d\n",
                  kGridX, kGridCellSize, kGridCols, kGridX + kGridCols * kGridCellSize);
    printPrompt("drill start: 7 tilt drills. BLUE key advances; SIDE key marks unintended moves in T5 only.");
    printPrompt("Boot: press BLUE to begin T1 REST.");
    RenderInputs in;
    in.drill = Drill::BootWait;
    renderDrillCue(0, in);
}

void sfProbeReport() {
    if (s_drill == Drill::Done && !s_verdictPrinted) {
        printSummary(); s_verdictPrinted = true;
    } else {
        printPrompt("drill start: 7 tilt drills. BLUE key advances; SIDE key marks unintended moves in T5 only.");
        if (s_drill == Drill::BootWait) printPrompt("Boot: press BLUE to begin T1 REST.");
    }
}

void sfProbeLoopImpl() {
    int64_t nowUs = static_cast<int64_t>(sf_hal::nowUs());
    uint32_t nowMs = (uint32_t)(nowUs / 1000);

    // Allow neutral drift only while T6 selection is centred (column 4).
    // Neutral drift must be frozen for BOTH absolute drills. In the absolute
    // scheme the neutral IS column 4, so a drifting neutral is a drifting board.
    // T7 needs this as much as T6: every T7 hold column (2..6) sits inside the
    // 0.20 G incremental dead zone, so driftNeutral's lrState guard never fires,
    // and with a 2000 ms tau the neutral absorbs ~92% of a held tilt within 5 s.
    // Block B runs far longer than that, so the selection would slide back to 4
    // and count as colbleed with no involvement from the twist at all - a false
    // failure on the one bar that must be exactly zero.
    bool allowDrift = true;
    if (s_drill == Drill::T6Absolute) {
        allowDrift = (s_t6.selectedCol == 4);
    } else if (s_drill == Drill::T7Twist) {
        // Recomputed live rather than read from s_t7.lastSelectedCol, which is
        // only updated inside Block B and is stale during Blocks A, C-centre
        // and C-edge.
        allowDrift = (absoluteColFromSignal(s_tilt.lrSignal(), 4,
                                            kGPerColumn, kColHysteresisG) == 4);
    }
    s_tilt.update(nowMs, allowDrift);
    ButtonEvent ev = pollButtons(nowMs);

    if (ev.sidePress && s_drill == Drill::T5Sustained) {
        ++s_t5Markers;
        Serial.printf("[S3B] marker %d\n", s_t5Markers);
    }

    if (ev.bluePress) {
        if (s_drill == Drill::T1Rest && !s_t1Finished) {
            // ignored: rest measurement must run full duration
        } else if (s_drill == Drill::T3Precision && s_t3TrialActive) {
            confirmT3Trial(nowMs, false);
        } else if (s_drill == Drill::T6Absolute) {
            if (t6Update(s_t6, nowMs, s_tilt, s_rng, true, kGPerColumn, kColHysteresisG)) {
                advanceDrill();
            }
        } else if (s_drill == Drill::T7Twist) {
            if (t7Update(s_t7, nowMs, s_tilt, s_rng, true, kGPerColumn, kColHysteresisG)) {
                s_drill = Drill::Done;
            }
        } else {
            advanceDrill();
        }
    }

    switch (s_drill) {
        case Drill::T1Rest: {
            int64_t elapsedUs = nowUs - s_drillStartUs;
            int sec = (int)(elapsedUs / (1000 * 1000));
            if (sec != s_lastT1Second && sec <= 60) {
                s_lastT1Second = sec;
                if (sec % 10 == 0 || sec == 60) Serial.printf("[S3B] T1 hold... %ds\n", sec);
            }
            if (!s_t1Finished && elapsedUs >= kT1RestUs) {
                s_t1Moves = s_tilt.movesLeft + s_tilt.movesRight;
                s_t1Soft = s_tilt.softDrops;
                bool ok = (s_t1Moves == 0 && s_t1Soft == 0);
                Serial.printf("[S3B] T1: moves=%lu repeats=%lu softdrops=%lu %s\n",
                              (unsigned long)s_tilt.lrChanges, (unsigned long)s_tilt.repeats,
                              (unsigned long)s_tilt.softDrops, ok ? "OK" : "BAD");
                s_t1Finished = true;
            }
            break;
        }
        case Drill::T2Intent: {
            if (!s_t2AwaitingMove && s_drillSub < kT2PromptCount) {
                if (s_t2NextPromptUs == 0) {
                    int64_t delayUs = kT2PromptMinUs +
                        (int64_t)(s_rng.next() % (uint32_t)(kT2PromptMaxUs - kT2PromptMinUs + 1));
                    s_t2NextPromptUs = nowUs + delayUs;
                }
                if (nowUs >= s_t2NextPromptUs) {
                    issueT2Prompt(nowMs);
                    s_t2NextPromptUs = 0;
                }
            } else if (s_t2AwaitingMove) {
                bool leftMoved = s_tilt.movesLeft != s_t2MovesLeftAtPrompt;
                bool rightMoved = s_tilt.movesRight != s_t2MovesRightAtPrompt;
                if (leftMoved || rightMoved) {
                    bool movedRight = rightMoved && !leftMoved;
                    uint32_t lat = (uint32_t)(nowMs - s_t2PromptTimeMs);
                    if (s_t2LatencyCount < kT2PromptCount) {
                        s_t2LatenciesMs[s_t2LatencyCount++] = lat;
                    }
                    if (movedRight == s_t2ExpectedRight[s_drillSub]) ++s_t2Correct;
                    s_t2AwaitingMove = false;
                    ++s_drillSub;
                } else if ((uint32_t)(nowMs - s_t2PromptTimeMs) >= kT2LatencyTimeoutMs) {
                    if (s_t2LatencyCount < kT2PromptCount) {
                        s_t2LatenciesMs[s_t2LatencyCount++] = kT2LatencyTimeoutMs;
                    }
                    s_t2AwaitingMove = false;
                    ++s_drillSub;
                }
            } else if (s_drillSub >= kT2PromptCount) {
                bool ok = t2Pass();
                Serial.printf("[S3B] T2: %s\n", ok ? "OK" : "BAD");
                startDrill(Drill::T3Precision);
            }
            break;
        }
        case Drill::T3Precision: {
            if (!s_t3TrialActive && s_drillSub < kT3PromptCount) {
                issueT3Prompt(nowMs);
            } else if (s_t3TrialActive) {
                uint32_t leftDelta = s_tilt.movesLeft - s_t3MovesLeftAtPrompt;
                uint32_t rightDelta = s_tilt.movesRight - s_t3MovesRightAtPrompt;
                bool moved = (leftDelta != 0 || rightDelta != 0);
                if (moved) {
                    s_t3LastMoveMs = nowMs;
                    s_t3MoveCount = (int)(leftDelta + rightDelta);
                }
                bool neutralStable = !moved &&
                    ((uint32_t)(nowMs - s_t3LastMoveMs) >= kT3NeutralStableMs);
                bool timeout = ((uint32_t)(nowMs - s_t3MoveStartMs) >= kT3TrialTimeoutMs);
                if (neutralStable || timeout) {
                    confirmT3Trial(nowMs, timeout);
                }
            } else if (s_drillSub >= kT3PromptCount) {
                bool ok = t3Pass();
                Serial.printf("[S3B] T3: %s\n", ok ? "OK" : "BAD");
                startDrill(Drill::T4CrossH);
            }
            break;
        }
        case Drill::T4CrossH: {
            int totalMoves = (int)(s_tilt.movesLeft + s_tilt.movesRight);
            int totalSoft = (int)s_tilt.softDrops;
            s_t4Unsoft = totalSoft;
            if (totalMoves >= kT4MoveCount) {
                Serial.printf("[S3B] T4 phase 1: OK (moves=%d unsoft=%d)\n", totalMoves, totalSoft);
                startDrill(Drill::T4CrossV);
            }
            break;
        }
        case Drill::T4CrossV: {
            int totalSoft = (int)s_tilt.softDrops;
            int totalMoves = (int)(s_tilt.movesLeft + s_tilt.movesRight);
            s_t4Unmove = totalMoves;
            if (totalSoft >= kT4MoveCount) {
                Serial.printf("[S3B] T4 phase 2: OK (soft=%d unmove=%d)\n", totalSoft, totalMoves);
                startDrill(Drill::T5Sustained);
            }
            break;
        }
        case Drill::T5Sustained: {
            int64_t elapsedUs = nowUs - s_drillStartUs;
            if (nowUs - s_t5ReportUs >= kT5ReportIntervalUs) {
                s_t5ReportUs = nowUs;
                Serial.printf("[S3B] T5 %lds markers=%d\n",
                              (long)(elapsedUs / (1000 * 1000)), s_t5Markers);
            }
            if (elapsedUs >= kT5DurationUs) {
                bool ok = t5Pass();
                Serial.printf("[S3B] T5: %s\n", ok ? "OK" : "BAD");
                s_drill = Drill::Done;
            }
            break;
        }
        case Drill::T6Absolute: {
            t6Update(s_t6, nowMs, s_tilt, s_rng, false, kGPerColumn, kColHysteresisG);
            if (s_t6.trial >= 20 && !s_t6.active) {
                t6PrintSummary(s_t6);
                advanceDrill();
            }
            break;
        }
        case Drill::T7Twist: {
            t7Update(s_t7, nowMs, s_tilt, s_rng, false, kGPerColumn, kColHysteresisG);
            if (s_t7.block == 4 && s_t7.edgePass >= 5) {
                // Completed on last BLUE press; summary printed in Done state.
            }
            break;
        }
        default: break;
    }

    if (s_drill == Drill::Done && !s_verdictPrinted) {
        printSummary(); s_verdictPrinted = true;
    }

    RenderInputs in;
    in.drill = s_drill;
    in.t1Finished = s_t1Finished;
    in.t2AwaitingMove = s_t2AwaitingMove;
    in.t2ExpectedRight = s_t2CurrentExpectedRight;
    in.t3TargetCol = s_t3TargetCol;
    in.t3CurrentTargetCols = s_t3CurrentTargetCols;
    in.t3MoveRight = s_t3MoveRight;
    in.t5Markers = s_t5Markers;
    in.drillStartUs = s_drillStartUs;
    in.t2MarkerCol = t2MarkerCol();
    in.t3MarkerCol = t3MarkerCol();
    in.t6SelectedCol = s_t6.selectedCol;
    in.t6TargetCol = s_t6.targetCol;
    in.t7Block = s_t7.block;
    in.t7Trial = s_t7.trial;
    in.t7Cue = t7CueText(s_t7);
    renderDrillCue(nowMs, in);
}

} // namespace sf

// Probe entry points live in the global namespace because probe_main.cpp
// declares them without a namespace.
void sfProbeSetup() {
    using namespace sf;
    sf_hal::probe::display().setRotation(0);
    sf_hal::probe::display().fillScreen(TFT_BLACK);
    initRender();
    s_tilt.reset();
    s_drill = Drill::BootWait;
    s_verdictPrinted = false; s_summaryPrinted = false;
    s_t1Finished = false;
    s_t1Moves = s_t1Soft = 0;
    s_t2Correct = s_t2LatencyCount = s_t3Exact = 0;
    s_t4Unsoft = s_t4Unmove = s_t5Markers = 0;
    t6Reset(s_t6);
    t7Reset(s_t7);
    Serial.printf("[S3B] grid x0=%d cellw=%d cells=%d x1=%d\n",
                  kGridX, kGridCellSize, kGridCols, kGridX + kGridCols * kGridCellSize);
    printPrompt("drill start: 7 tilt drills. BLUE key advances; SIDE key marks unintended moves in T5 only.");
    printPrompt("Boot: press BLUE to begin T1 REST.");
    RenderInputs in;
    in.drill = Drill::BootWait;
    renderDrillCue(0, in);
}

void sfProbeReport() {
    using namespace sf;
    if (s_drill == Drill::Done && !s_verdictPrinted) {
        printSummary(); s_verdictPrinted = true;
    } else {
        printPrompt("drill start: 7 tilt drills. BLUE key advances; SIDE key marks unintended moves in T5 only.");
        if (s_drill == Drill::BootWait) printPrompt("Boot: press BLUE to begin T1 REST.");
    }
}

void sfProbeLoop() {
    sf::sfProbeLoopImpl();
}

#endif
