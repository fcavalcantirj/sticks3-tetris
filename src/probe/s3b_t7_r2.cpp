// SPIKE S3b T7 round 2 — gyro twist-channel drill.
#include "s3b_t7_r2.h"
#include "s3b_pipeline_r2.h"
#include "s3b_t6_r2.h"
#include "s3b_pipeline.h"  // TinyRng
#include <M5Unified.h>
#include <cstdio>

namespace sf {

namespace {

uint32_t medianOf(uint32_t* arr, int n) {
    if (n <= 0) return 0;
    for (int i = 1; i < n; ++i) {
        uint32_t v = arr[i]; int j = i;
        while (j > 0 && arr[j-1] > v) { arr[j] = arr[j-1]; --j; }
        arr[j] = v;
    }
    return arr[n / 2];
}

} // namespace

void t7r2Reset(T7StateR2& st) { st = T7StateR2{}; }

const char* t7r2CueText(const T7StateR2& st) {
    if (st.block == 1) return st.promptCw ? "CW" : "CCW";
    if (st.block == 2) return "HOLD";
    if (st.block == 3) return "CENTRE";
    if (st.block == 4) return "EDGE";
    return "";
}

void issueT7r2Prompt(T7StateR2& st, uint32_t nowMs, TinyRng& rng) {
    if (st.block == 1) {
        st.promptCw = (rng.next() & 1u) != 0u;
        st.promptMs = nowMs;
        st.awaiting = true;
        Serial.printf("[PROMPT] n=%d dir=%s\n", st.trial + 1, st.promptCw ? "CW" : "CCW");
    } else if (st.block == 2) {
        st.holdCol = (int)rng.range(2, 7);
        st.lastSelectedCol = 4;
        st.trial = 0;
        Serial.printf("[PROMPT] hold col=%d, twist 20 times\n", st.holdCol);
    } else if (st.block == 3) {
        st.edgePass = 1;
        st.trial = 0;
        Serial.printf("[PROMPT] centre pass 1/%d: steer and back\n", kT7SteerPassesR2);
    } else if (st.block == 4) {
        st.edgePass = 1;
        st.trial = 0;
        Serial.printf("[PROMPT] edge pass 1/%d: full span and back\n", kT7SteerPassesR2);
    }
    (void)nowMs;
}

bool t7r2Update(T7StateR2& st, uint32_t nowMs, const TiltPipelineR2& tilt, TinyRng& rng,
                bool bluePress) {
    int8_t twist = st.twist.update(tilt.gxDps(), nowMs);

    if (st.block == 0) {
        st.block = 1;
        issueT7r2Prompt(st, nowMs, rng);
        return false;
    }

    if (st.block == 1) {
        if (twist != 0 && st.awaiting) {
            bool cw = twist > 0;
            uint32_t dt = nowMs - st.promptMs;
            if (st.timeCount < kT7DirTrialsR2) st.timesMs[st.timeCount++] = dt;
            if (cw == st.promptCw) ++st.dirCorrect;
            st.awaiting = false;
            ++st.trial;
            Serial.printf("[TRACE] t=%lu scr=PLAYING act=%s src=GYRO q=0/16 drop=0\n",
                          (unsigned long)nowMs, cw ? "ROTATE_CW" : "ROTATE_CCW");
        }
        if (!st.awaiting && st.trial < kT7DirTrialsR2) {
            issueT7r2Prompt(st, nowMs, rng);
        }
        if (st.trial >= kT7DirTrialsR2 && bluePress) {
            st.block = 2;
            issueT7r2Prompt(st, nowMs, rng);
        }
    } else if (st.block == 2) {
        int col = absoluteColFromSignalR2(tilt.lrSignal(), st.lastSelectedCol,
                                          kGPerColumnR2, kColHysteresisGR2);
        if (col != st.lastSelectedCol) {
            ++st.colBleed;
            st.lastSelectedCol = col;
        }
        if (twist != 0) {
            ++st.trial;
            Serial.printf("[TRACE] t=%lu scr=PLAYING act=%s src=GYRO q=0/16 drop=0\n",
                          (unsigned long)nowMs, twist > 0 ? "ROTATE_CW" : "ROTATE_CCW");
        }
        if (st.trial >= kT7HoldTwistsR2 && bluePress) {
            st.block = 3;
            issueT7r2Prompt(st, nowMs, rng);
        }
    } else if (st.block == 3) {
        if (twist != 0) {
            ++st.rotBleedCentre;
            Serial.printf("[TRACE] t=%lu scr=PLAYING act=%s src=GYRO q=0/16 drop=0\n",
                          (unsigned long)nowMs, twist > 0 ? "ROTATE_CW" : "ROTATE_CCW");
        }
        if (bluePress) {
            if (st.edgePass < kT7SteerPassesR2) {
                ++st.edgePass;
                Serial.printf("[PROMPT] pass %d/%d done\n", st.edgePass, kT7SteerPassesR2);
            } else {
                st.block = 4;
                issueT7r2Prompt(st, nowMs, rng);
            }
        }
    } else if (st.block == 4) {
        if (twist != 0) {
            ++st.rotBleedEdge;
            Serial.printf("[TRACE] t=%lu scr=PLAYING act=%s src=GYRO q=0/16 drop=0\n",
                          (unsigned long)nowMs, twist > 0 ? "ROTATE_CW" : "ROTATE_CCW");
        }
        if (bluePress) {
            if (st.edgePass < kT7SteerPassesR2) {
                ++st.edgePass;
                Serial.printf("[PROMPT] pass %d/%d done\n", st.edgePass, kT7SteerPassesR2);
            } else {
                return true; // T7 complete
            }
        }
    }
    return false;
}

uint32_t t7r2MedianMs(const T7StateR2& st) {
    if (st.timeCount == 0) return 0;
    uint32_t tmp[kT7DirTrialsR2];
    for (int i = 0; i < st.timeCount; ++i) tmp[i] = st.timesMs[i];
    return medianOf(tmp, st.timeCount);
}

void t7r2PrintSummary(const T7StateR2& st) {
    uint32_t med = t7r2MedianMs(st);
    Serial.printf("[S3B] T7: dir=%d/%d median=%lums colbleed=%d rotbleed_centre=%d rotbleed_edge=%d %s\n",
                  st.dirCorrect, kT7DirTrialsR2, (unsigned long)med,
                  st.colBleed, st.rotBleedCentre, st.rotBleedEdge,
                  t7r2Pass(st) ? "OK" : "BAD");
}

bool t7r2Pass(const T7StateR2& st) {
    if (st.block < 4) return false;
    if (st.dirCorrect < 19) return false;
    if (st.timeCount == 0) return false;
    uint32_t tmp[kT7DirTrialsR2];
    for (int i = 0; i < st.timeCount; ++i) tmp[i] = st.timesMs[i];
    if (medianOf(tmp, st.timeCount) > 500) return false;
    if (st.colBleed != 0) return false;
    if (st.rotBleedCentre != 0) return false;
    if (st.rotBleedEdge > 2) return false;
    return true;
}

} // namespace sf
