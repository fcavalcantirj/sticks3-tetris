#pragma once
#include <cstdint>

namespace sf {

class TinyRng;
struct TiltPipeline;

struct T6State {
    int exact = 0;
    int trial = 0;
    int targetCol = -1;
    int startCol = 4;
    int selectedCol = 4;
    bool active = false;
    uint32_t promptMs = 0;
    uint32_t timesMs[20] = {0};
    int distances[20] = {0};
};

void t6Reset(T6State& st);
int absoluteColFromSignal(float signedLr, int currentCol, float gPerColumn, float hystG);
// Update T6 each loop. bluePress confirms the current trial. gPerColumn and
// hystG are the absolute-aim geometry constants defined in s3b_drills.cpp.
// Returns true when all 20 trials are complete and the drill should finish.
bool t6Update(T6State& st, uint32_t nowMs, const TiltPipeline& tilt, TinyRng& rng,
              bool bluePress, float gPerColumn, float hystG);
void t6PrintSummary(const T6State& st);
// Accessors so the single-line S3B: summary can carry T6's numbers.
uint32_t t6MedianMs(const T6State& st);
float t6Ratio(const T6State& st);
bool t6Pass(const T6State& st);

} // namespace sf
