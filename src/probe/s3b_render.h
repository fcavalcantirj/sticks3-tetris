#pragma once
#include <cstdint>
#include "s3b_drills.h"

namespace sf {

// Inputs needed by the renderer; filled in by the main drill loop.
struct RenderInputs {
    Drill drill = Drill::BootWait;
    bool t1Finished = false;
    bool t2AwaitingMove = false;
    bool t2ExpectedRight = false;
    int t3TargetCol = -1;
    int t3CurrentTargetCols = 0;
    bool t3MoveRight = false;
    int t5Markers = 0;
    int64_t drillStartUs = 0;
    int t2MarkerCol = 5;
    int t3MarkerCol = 5;
    int t6SelectedCol = -1;
    int t6TargetCol = -1;
    int t7Block = 0;
    int t7Trial = 0;
    const char* t7Cue = "";
};

void initRender();
void renderDrillCue(uint32_t nowMs, const RenderInputs& in);

} // namespace sf
