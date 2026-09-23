// SPIKE S3b T6 round 2 — absolute angle-to-column steering drill.
#include "s3b_t6_r2.h"
#include "s3b_pipeline_r2.h"
#include "s3b_pipeline.h"  // TinyRng
#include <M5Unified.h>
#include <cmath>
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

} // namespace

void t6r2Reset(T6StateR2& st) { st = T6StateR2{}; }

void issueT6r2Prompt(T6StateR2& st, uint32_t nowMs, TinyRng& rng) {
    int target;
    do {
        target = (int)rng.range(0, 9);
    } while (target == st.selectedCol);
    st.startCol = st.selectedCol;
    st.targetCol = target;
    st.promptMs = nowMs;
    st.active = true;
    st.selectedChangedMs = nowMs;
    bool warm = st.trial < kT6WarmupR2;
    st.warmup[st.trial] = warm;
    Serial.printf("[PROMPT] n=%d target=%d start=%d%s\n",
                  st.trial + 1, target, st.startCol, warm ? " WARMUP" : "");
}

bool t6r2Update(T6StateR2& st, uint32_t nowMs, const TiltPipelineR2& tilt, TinyRng& rng,
                bool bluePress) {
    float sig = tilt.lrSignal();
    int prevCol = st.selectedCol;
    int newCol = absoluteColFromSignalR2(sig, st.selectedCol, kGPerColumnR2, kColHysteresisGR2);
    if (newCol != st.selectedCol) {
        st.selectedCol = newCol;
        st.selectedChangedMs = nowMs;
    }

    if (!st.active && st.trial < kT6TotalR2) {
        issueT6r2Prompt(st, nowMs, rng);
    } else if (st.active && bluePress) {
        uint32_t dt = nowMs - st.promptMs;
        bool ok = (st.selectedCol == st.targetCol);
        if (!st.warmup[st.trial] && ok) ++st.exact;
        int dist = std::abs(st.targetCol - st.startCol);
        st.timesMs[st.trial] = dt;
        st.distances[st.trial] = dist;
        uint32_t dwell = nowMs - st.selectedChangedMs;
        if (prevCol != st.selectedCol) dwell = 0; // changed this frame
        const char* tag = st.warmup[st.trial] ? "WARMUP" : (ok ? "OK" : "BAD");
        Serial.printf("[TRACE] t=%lu scr=PLAYING act=ABSOLUTE_AIM src=TILT q=0/16 drop=0 col=%d target=%d dist=%d sig=%.4f neutral=%.4f dwell=%lums %s\n",
                      (unsigned long)nowMs, st.selectedCol, st.targetCol, dist,
                      sig, tilt.neutralLr, (unsigned long)dwell, tag);
        st.active = false;
        ++st.trial;
    }
    return st.trial >= kT6TotalR2 && !st.active;
}

uint32_t t6r2MedianMs(const T6StateR2& st) {
    uint32_t tmp[kT6ScoredR2];
    int n = 0;
    for (int i = 0; i < kT6TotalR2; ++i) {
        if (!st.warmup[i]) tmp[n++] = st.timesMs[i];
    }
    return medianOf(tmp, n);
}

float t6r2Ratio(const T6StateR2& st) {
    uint32_t nearArr[kT6ScoredR2], farArr[kT6ScoredR2];
    int nearN = 0, farN = 0;
    for (int i = 0; i < kT6TotalR2; ++i) {
        if (st.warmup[i]) continue;
        if (st.distances[i] <= 2) nearArr[nearN++] = st.timesMs[i];
        if (st.distances[i] >= 5) farArr[farN++] = st.timesMs[i];
    }
    uint32_t med = t6r2MedianMs(st);
    uint32_t nearMed = (nearN > 0) ? medianOf(nearArr, nearN) : med;
    uint32_t farMed = (farN > 0) ? medianOf(farArr, farN) : med;
    return (nearMed == 0) ? 0.0f : ((float)farMed / (float)nearMed);
}

void t6r2PrintSummary(const T6StateR2& st) {
    uint32_t tmp[kT6ScoredR2];
    int n = 0;
    for (int i = 0; i < kT6TotalR2; ++i) if (!st.warmup[i]) tmp[n++] = st.timesMs[i];
    uint32_t med = medianOf(tmp, n);
    uint32_t p95 = p95Of(tmp, n);
    uint32_t nearArr[kT6ScoredR2], farArr[kT6ScoredR2];
    int nearN = 0, farN = 0;
    for (int i = 0; i < kT6TotalR2; ++i) {
        if (st.warmup[i]) continue;
        if (st.distances[i] <= 2) nearArr[nearN++] = st.timesMs[i];
        if (st.distances[i] >= 5) farArr[farN++] = st.timesMs[i];
    }
    uint32_t nearMed = (nearN > 0) ? medianOf(nearArr, nearN) : med;
    uint32_t farMed = (farN > 0) ? medianOf(farArr, farN) : med;
    float ratio = (nearMed == 0) ? 0.0f : ((float)farMed / (float)nearMed);
    Serial.printf("[S3B] T6: exact=%d/%d median=%lums p95=%lums near=%lums far=%lums ratio=%.2f %s\n",
                  st.exact, kT6ScoredR2,
                  (unsigned long)med, (unsigned long)p95,
                  (unsigned long)nearMed, (unsigned long)farMed,
                  ratio, t6r2Pass(st) ? "OK" : "BAD");
}

bool t6r2Pass(const T6StateR2& st) {
    if (st.trial < kT6TotalR2) return false;
    if (st.exact < 36) return false;
    float ratio = t6r2Ratio(st);
    return ratio <= 1.5f;
}

} // namespace sf
