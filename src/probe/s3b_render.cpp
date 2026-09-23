// SPIKE S3b rendering helper. Pure display code: no serial protocol, no game logic.
// The grid layout constants live in s3b_constants.h and are used by s3b_grid.cpp;
// this file does not shadow them.
#include "s3b_render.h"
#include "s3b_grid.h"
#include <M5Unified.h>
#include <cstring>
#include <cstdio>

#include "hal/sticks3/clock.h"
#include "hal/sticks3/panel.h"

namespace sf {

namespace {

// Cue state: redraw only when the displayed values change.
struct CueState {
    Drill drill = Drill::BootWait;
    char text[12] = "";
    char status[48] = "";
};
CueState s_cue;

void renderCue(const char* text, const char* status, Drill d) {
    if (s_cue.drill != d) {
        sf_hal::probe::display().fillScreen(TFT_BLACK);
        s_cue.drill = d;
        s_cue.text[0] = '\0';
        s_cue.status[0] = '\0';
    }
    if (std::strcmp(s_cue.text, text) != 0) {
        sf_hal::probe::display().setTextSize(6);
        sf_hal::probe::display().setTextColor(TFT_WHITE, TFT_BLACK);
        sf_hal::probe::display().setTextDatum(MC_DATUM);
        // Clear the large-cue box before drawing so a shorter string cannot
        // leave fragments of the previous glyphs (e.g. CCW drawn over CW).
        sf_hal::probe::display().fillRect(0, 32, 135, 96, TFT_BLACK);
        sf_hal::probe::display().drawString(text, 67, 80);
        std::strncpy(s_cue.text, text, sizeof(s_cue.text) - 1);
        s_cue.text[sizeof(s_cue.text) - 1] = '\0';
    }
    if (std::strcmp(s_cue.status, status) != 0) {
        sf_hal::probe::display().setTextSize(1);
        sf_hal::probe::display().setTextColor(TFT_WHITE, TFT_BLACK);
        sf_hal::probe::display().setTextDatum(TL_DATUM);
        sf_hal::probe::display().fillRect(0, 160, 135, 80, TFT_BLACK);
        sf_hal::probe::display().setCursor(4, 168);
        sf_hal::probe::display().print(status);
        std::strncpy(s_cue.status, status, sizeof(s_cue.status) - 1);
        s_cue.status[sizeof(s_cue.status) - 1] = '\0';
    }
}

} // namespace

void initRender() {
    s_cue = CueState{};
    initGrid();
}

void renderDrillCue(uint32_t nowMs, const RenderInputs& in) {
    (void)nowMs;
    switch (in.drill) {
        case Drill::BootWait:
            setGrid(false, false, -1, -1);
            renderCue("S3B", "BLUE=advance", Drill::BootWait);
            break;
        case Drill::T1Rest: {
            setGrid(false, false, -1, -1);
            int64_t elapsedUs =
                static_cast<int64_t>(sf_hal::nowUs()) - in.drillStartUs;
            int sec = (int)(elapsedUs / (1000 * 1000));
            int remain = 60 - sec;
            if (remain < 0) remain = 0;
            char text[8]; snprintf(text, sizeof(text), "%d", remain);
            char status[40];
            if (in.t1Finished) {
                snprintf(status, sizeof(status), "REST done BLUE=next");
            } else {
                snprintf(status, sizeof(status), "REST hold still %ds", remain);
            }
            renderCue(text, status, Drill::T1Rest);
            break;
        }
        case Drill::T2Intent: {
            const char* txt = in.t2AwaitingMove ? (in.t2ExpectedRight ? ">" : "<") : "-";
            char status[40];
            snprintf(status, sizeof(status), "col=%d TILT then neutral", in.t2MarkerCol);
            setGrid(true, false, in.t2MarkerCol, -1);
            renderCue(txt, status, Drill::T2Intent);
            break;
        }
        case Drill::T3Precision: {
            char text[8];
            const char* dir = in.t3MoveRight ? ">" : "<";
            snprintf(text, sizeof(text), "%s%d", dir, in.t3CurrentTargetCols);
            char status[40];
            snprintf(status, sizeof(status), "col=%d target=%d BLUE=confirm", in.t3MarkerCol, in.t3TargetCol);
            setGrid(true, true, in.t3MarkerCol, in.t3TargetCol);
            renderCue(text, status, Drill::T3Precision);
            break;
        }
        case Drill::T4CrossH:
            setGrid(false, false, -1, -1);
            renderCue("<>", "left/right no soft", Drill::T4CrossH);
            break;
        case Drill::T4CrossV:
            setGrid(false, false, -1, -1);
            renderCue("v", "top away no move", Drill::T4CrossV);
            break;
        case Drill::T5Sustained: {
            setGrid(false, false, -1, -1);
            int64_t elapsedUs =
                static_cast<int64_t>(sf_hal::nowUs()) - in.drillStartUs;
            int sec = (int)(elapsedUs / (1000 * 1000));
            if (sec > 180) sec = 180;
            char text[8]; snprintf(text, sizeof(text), "%d", sec);
            char status[40];
            snprintf(status, sizeof(status), "markers=%d SIDE=mark", in.t5Markers);
            renderCue(text, status, Drill::T5Sustained);
            break;
        }
        case Drill::T6Absolute: {
            char text[8];
            snprintf(text, sizeof(text), "%d", in.t6TargetCol);
            char status[40];
            snprintf(status, sizeof(status), "col=%d target=%d BLUE=confirm", in.t6SelectedCol, in.t6TargetCol);
            setGrid(true, true, -1, in.t6TargetCol, true, true, in.t6SelectedCol);
            renderCue(text, status, Drill::T6Absolute);
            break;
        }
        case Drill::T7Twist: {
            char text[12];
            if (in.t7Block == 1) {
                snprintf(text, sizeof(text), "%s", in.t7Cue);
            } else if (in.t7Block == 2) {
                snprintf(text, sizeof(text), "C%d", in.t7Trial);
            } else if (in.t7Block == 3) {
                snprintf(text, sizeof(text), "E%d", in.t7Trial);
            } else {
                text[0] = '\0';
            }
            char status[48];
            snprintf(status, sizeof(status), "%s BLUE=next", in.t7Cue);
            setGrid(false, false, -1, -1);
            renderCue(text, status, Drill::T7Twist);
            break;
        }
        case Drill::Done:
            setGrid(false, false, -1, -1);
            renderCue("END", "BLUE=restart", Drill::Done);
            break;
    }
}

} // namespace sf
