#pragma once
#include "s3b_twist.h"
#include <cstdint>

namespace sf {

class TinyRng;
struct TiltPipeline;

struct T7State {
    TwistDetector twist;
    int block = 0; // 1=A, 2=B, 3=C centre, 4=C edge
    int trial = 0;
    int dirCorrect = 0;
    int timeCount = 0;
    uint32_t timesMs[20] = {0};
    int colBleed = 0;
    int rotBleedCentre = 0;
    int rotBleedEdge = 0;
    bool promptCw = false;
    uint32_t promptMs = 0;
    bool awaiting = false;
    int holdCol = 4;
    int lastSelectedCol = 4;
    int edgePass = 0;
};

void t7Reset(T7State& st);
// Returns true when the whole T7 drill is complete (block 4 done).
// gPerColumn and hystG are the absolute-aim geometry constants.
bool t7Update(T7State& st, uint32_t nowMs, const TiltPipeline& tilt, TinyRng& rng,
              bool bluePress, float gPerColumn, float hystG);
void t7PrintSummary(const T7State& st);
// Accessor so the single-line S3B: summary can carry T7's numbers.
uint32_t t7MedianMs(const T7State& st);
bool t7Pass(const T7State& st);
const char* t7CueText(const T7State& st);

} // namespace sf
