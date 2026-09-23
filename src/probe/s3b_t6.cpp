// SPIKE S3b T6 — absolute angle-to-column steering drill.
// Round 2 raises this to 40 scored trials plus 3 warm-ups and prints:
// [S3B] T6: exact=%d/40 median=%lums p95=%lums near=%lums far=%lums ratio=%.2f <OK|BAD>
#include "s3b_t6.h"
#include "s3b_pipeline.h"
#include <M5Unified.h>
#include <cmath>
#include <cstdio>

namespace sf {

namespace {

int clampCol(int c) {
    if (c < 0) return 0;
    if (c >= kGridCols) return kGridCols - 1;
    return c;
}

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

int absoluteColFromSignal(float signedLr, int currentCol, float gPerColumn, float hystG) {
    int raw = (int)(signedLr >= 0.0f ? (signedLr / gPerColumn + 0.5f)
                                     : (signedLr / gPerColumn - 0.5f));
    int candidate = clampCol(raw + 4);
    if (candidate == currentCol) return currentCol;
    float boundary = ((float)(currentCol + candidate) / 2.0f - 4.0f) * gPerColumn;
    if (candidate > currentCol) {
        if (signedLr > boundary + hystG) return candidate;
    } else {
        if (signedLr < boundary - hystG) return candidate;
    }
    return currentCol;
}


void t6Reset(T6State& st) {
    st = T6State{};
}

void issueT6Prompt(T6State& st, uint32_t nowMs, TinyRng& rng) {
    int target;
    do {
        target = (int)rng.range(0, 9);
    } while (target == st.selectedCol);
    st.startCol = st.selectedCol;
    st.targetCol = target;
    st.promptMs = nowMs;
    st.active = true;
    Serial.printf("[PROMPT] n=%d target=%d start=%d\n", st.trial + 1, target, st.startCol);
}

bool t6Update(T6State& st, uint32_t nowMs, const TiltPipeline& tilt, TinyRng& rng,
              bool bluePress, float gPerColumn, float hystG) {
    float sig = tilt.lrSignal();
    int newCol = absoluteColFromSignal(sig, st.selectedCol, gPerColumn, hystG);
    if (newCol != st.selectedCol) st.selectedCol = newCol;

    if (!st.active && st.trial < 20) {
        issueT6Prompt(st, nowMs, rng);
    } else if (st.active && bluePress) {
        uint32_t dt = nowMs - st.promptMs;
        bool ok = (st.selectedCol == st.targetCol);
        if (ok) ++st.exact;
        int dist = std::abs(st.targetCol - st.startCol);
        if (st.trial < 20) {
            st.timesMs[st.trial] = dt;
            st.distances[st.trial] = dist;
        }
        Serial.printf("[TRACE] t=%lu scr=PLAYING act=ABSOLUTE_AIM src=TILT q=0/16 drop=0 col=%d target=%d dist=%d %s\n",
                      (unsigned long)nowMs, st.selectedCol, st.targetCol, dist,
                      ok ? "OK" : "BAD");
        st.active = false;
        ++st.trial;
    }
    return st.trial >= 20 && !st.active;
}

uint32_t t6MedianMs(const T6State& st) {
    uint32_t tmp[20];
    for (int i = 0; i < 20; ++i) tmp[i] = st.timesMs[i];
    return medianOf(tmp, 20);
}

float t6Ratio(const T6State& st) {
    uint32_t nearArr[20], farArr[20];
    int nearN = 0, farN = 0;
    for (int i = 0; i < 20; ++i) {
        if (st.distances[i] <= 2) nearArr[nearN++] = st.timesMs[i];
        if (st.distances[i] >= 5) farArr[farN++] = st.timesMs[i];
    }
    uint32_t med = t6MedianMs(st);
    uint32_t nearMed = (nearN > 0) ? medianOf(nearArr, nearN) : med;
    uint32_t farMed = (farN > 0) ? medianOf(farArr, farN) : med;
    return (nearMed == 0) ? 0.0f : ((float)farMed / (float)nearMed);
}

void t6PrintSummary(const T6State& st) {
    uint32_t tmp[20];
    for (int i = 0; i < 20; ++i) tmp[i] = st.timesMs[i];
    uint32_t med = medianOf(tmp, 20);
    uint32_t p95 = p95Of(tmp, 20);
    uint32_t nearArr[20], farArr[20];
    int nearN = 0, farN = 0;
    for (int i = 0; i < 20; ++i) {
        if (st.distances[i] <= 2) nearArr[nearN++] = st.timesMs[i];
        if (st.distances[i] >= 5) farArr[farN++] = st.timesMs[i];
    }
    uint32_t nearMed = (nearN > 0) ? medianOf(nearArr, nearN) : med;
    uint32_t farMed = (farN > 0) ? medianOf(farArr, farN) : med;
    float ratio = (nearMed == 0) ? 0.0f : ((float)farMed / (float)nearMed);
    Serial.printf("[S3B] T6: exact=%d/%d median=%lums p95=%lums near=%lums far=%lums ratio=%.2f %s\n",
                  st.exact, 20,
                  (unsigned long)med, (unsigned long)p95,
                  (unsigned long)nearMed, (unsigned long)farMed,
                  ratio, t6Pass(st) ? "OK" : "BAD");
}

bool t6Pass(const T6State& st) {
    if (st.trial < 20) return false;
    if (st.exact < 18) return false;
    uint32_t tmp[20];
    for (int i = 0; i < 20; ++i) tmp[i] = st.timesMs[i];
    uint32_t med = medianOf(tmp, 20);
    if (med > 900) return false;
    uint32_t nearArr[20], farArr[20];
    int nearN = 0, farN = 0;
    for (int i = 0; i < 20; ++i) {
        if (st.distances[i] <= 2) nearArr[nearN++] = st.timesMs[i];
        if (st.distances[i] >= 5) farArr[farN++] = st.timesMs[i];
    }
    uint32_t nearMed = (nearN > 0) ? medianOf(nearArr, nearN) : med;
    uint32_t farMed = (farN > 0) ? medianOf(farArr, farN) : med;
    if (nearMed == 0) return false;
    float ratio = (float)farMed / (float)nearMed;
    return ratio <= 1.5f;
}

} // namespace sf
