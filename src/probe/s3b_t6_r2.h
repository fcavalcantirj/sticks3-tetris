#pragma once
#include <cmath>
#include "s3b_constants_r2.h"
#include <cstdint>

namespace sf {

class TinyRng;
struct TiltPipelineR2;

struct T6StateR2 {
    int exact = 0;
    int trial = 0; // 0..kT6WarmupR2+kT6ScoredR2-1
    int targetCol = -1;
    int startCol = 4;
    int selectedCol = 4;
    bool active = false;
    uint32_t promptMs = 0;
    uint32_t selectedChangedMs = 0;
    uint32_t timesMs[kT6TotalR2] = {0};
    int distances[kT6TotalR2] = {0};
    bool warmup[kT6TotalR2] = {false};
};

void t6r2Reset(T6StateR2& st);

// Neutral is centred on board position 4.5 - the true middle of ten columns - NOT on
// column 4. Centring on column 4 leaves 4 columns of travel to the left and 5 to the
// right, so the right edge needs 4.5*gPer while the left needs only 3.5*gPer: 29% more
// wrist rotation to reach column 9 than column 0. The owner felt exactly that and
// reported it as "totally off-center... last 30%". With the 4.5 offset both edges sit at
// 4*gPer = 0.36 G = 21.1 degrees and every column is one gPer wide.
// floor(x + 5.0) == round(x + 4.5), and it needs no sign branch.
// Consequence, deliberate: there is no unique home column at rest - the resting signal
// sits exactly on the 4/5 boundary and the hysteresis band holds whichever side it came
// from. Nothing may assume "column 4 means centred"; T1 measures wander as max-min.
inline int absoluteColFromSignalR2(float signedLr, int currentCol, float gPerColumn, float hystG) {
    int candidate = (int)std::floor(signedLr / gPerColumn + 5.0f);
    if (candidate < 0) candidate = 0;
    if (candidate >= kGridColsR2) candidate = kGridColsR2 - 1;
    if (candidate == currentCol) return currentCol;
    float boundary = ((float)(currentCol + candidate) / 2.0f - 4.5f) * gPerColumn;
    if (candidate > currentCol) {
        if (signedLr > boundary + hystG) return candidate;
    } else {
        if (signedLr < boundary - hystG) return candidate;
    }
    return currentCol;
}

// Update T6 each loop. bluePress confirms the current trial.
// Returns true when all warmups + scored trials are complete.
bool t6r2Update(T6StateR2& st, uint32_t nowMs, const TiltPipelineR2& tilt, TinyRng& rng,
                bool bluePress);
void t6r2PrintSummary(const T6StateR2& st);
uint32_t t6r2MedianMs(const T6StateR2& st);
float t6r2Ratio(const T6StateR2& st);
bool t6r2Pass(const T6StateR2& st);

} // namespace sf
