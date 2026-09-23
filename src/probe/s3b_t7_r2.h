#pragma once
#include "s3b_constants_r2.h"
#include "s3b_twist_r2.h"
#include <cstdint>

namespace sf {

class TinyRng;
struct TiltPipelineR2;

struct T7StateR2 {
    TwistDetectorR2 twist;
    int block = 0; // 1=A, 2=B, 3=C centre, 4=C edge
    int trial = 0;
    int dirCorrect = 0;
    int timeCount = 0;
    uint32_t timesMs[kT7DirTrialsR2] = {0};
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

void t7r2Reset(T7StateR2& st);
bool t7r2Update(T7StateR2& st, uint32_t nowMs, const TiltPipelineR2& tilt, TinyRng& rng,
                bool bluePress);
void t7r2PrintSummary(const T7StateR2& st);
uint32_t t7r2MedianMs(const T7StateR2& st);
bool t7r2Pass(const T7StateR2& st);
const char* t7r2CueText(const T7StateR2& st);

} // namespace sf
