#pragma once
#include "s3b_drills.h"
#include <cstdint>

namespace sf {

struct RenderInputsR2 {
    Drill drill = Drill::BootWait;
    int64_t drillStartUs = 0;
    bool t1Finished = false;
    int t9Presses = 0;
    int t6TargetCol = -1;
    int t6SelectedCol = -1;
    int t7Block = 0;
    int t7Trial = 0;
    const char* t7Cue = "";
};

void initRenderR2();
void renderDrillCueR2(uint32_t nowMs, const RenderInputsR2& in);

} // namespace sf
