// SPIKE S3b round-2 rendering helper.
#include "s3b_render_r2.h"
#include "s3b_constants_r2.h"
#include <M5Unified.h>
#include <cstring>
#include <cstdio>

#include "hal/sticks3/clock.h"
#include "hal/sticks3/panel.h"

namespace sf {

namespace {

constexpr uint16_t kOutlineColor = 0xFFFF; // white
constexpr uint16_t kMarkerColor = 0x07E0;  // green
constexpr uint16_t kTargetColor = 0xF800;  // red

struct CueStateR2 {
    Drill drill = Drill::BootWait;
    char text[16] = "";
    char status[56] = "";
};
CueStateR2 s_cue;

struct GridStateR2 {
    bool drawn = false;
    bool active = false;
    int targetCol = -1;
    int selectedCol = -1;
};
GridStateR2 s_grid;

void renderCueR2(const char* text, const char* status, Drill d) {
    if (s_cue.drill != d) {
        // Clear ONLY the cue and status regions. The grid owns its own rows and clears
        // itself in drawGridR2; a fillScreen here would wipe it on every drill change.
        sf_hal::probe::display().fillRect(0, kCueYR2, kPanelWR2, kCueHR2, TFT_BLACK);
        sf_hal::probe::display().fillRect(0, kStatusYR2, kPanelWR2, kStatusHR2, TFT_BLACK);
        s_cue.drill = d;
        s_cue.text[0] = '\0';
        s_cue.status[0] = '\0';
    }
    if (std::strcmp(s_cue.text, text) != 0) {
        sf_hal::probe::display().setTextSize(6);
        sf_hal::probe::display().setTextColor(TFT_WHITE, TFT_BLACK);
        sf_hal::probe::display().setTextDatum(MC_DATUM);
        // Confined to the cue region. This was fillRect(0, 32, 135, 96), which reached
        // into the grid's rows - and because the T6 cue text is "selected:target" it
        // changed on EVERY column move, so the grid vanished and reappeared constantly.
        sf_hal::probe::display().fillRect(0, kCueYR2, kPanelWR2, kCueHR2, TFT_BLACK);
        sf_hal::probe::display().drawString(text, kPanelWR2 / 2, kCueYR2 + kCueHR2 / 2);
        std::strncpy(s_cue.text, text, sizeof(s_cue.text) - 1);
        s_cue.text[sizeof(s_cue.text) - 1] = '\0';
    }
    if (std::strcmp(s_cue.status, status) != 0) {
        sf_hal::probe::display().setTextSize(1);
        sf_hal::probe::display().setTextColor(TFT_WHITE, TFT_BLACK);
        sf_hal::probe::display().setTextDatum(TL_DATUM);
        sf_hal::probe::display().fillRect(0, kStatusYR2, kPanelWR2, kStatusHR2, TFT_BLACK);
        sf_hal::probe::display().setCursor(2, kStatusYR2 + 4);
        sf_hal::probe::display().print(status);
        std::strncpy(s_cue.status, status, sizeof(s_cue.status) - 1);
        s_cue.status[sizeof(s_cue.status) - 1] = '\0';
    }
}

void drawGridR2(const GridStateR2& g) {
    // Owns rows kGridYR2 .. kGridYR2+kGridHR2-1 exclusively, and runs EDGE TO EDGE,
    // x=0..134. Cells are 13 or 14 px because 135 does not divide by 10 - a uniform
    // cell size always leaves dead space on the right, which is what the owner kept
    // reporting as "doesn't finish on total right".
    sf_hal::probe::display().fillRect(0, kGridYR2, kPanelWR2, kGridHR2, TFT_BLACK);
    if (!g.active) return;
    for (int c = 0; c < kGridColsR2; ++c) {
        sf_hal::probe::display().drawRect(gridColXR2(c), kGridYR2, gridColWR2(c), kGridHR2, kOutlineColor);
    }
    if (g.targetCol >= 0) {
        sf_hal::probe::display().fillRect(gridColXR2(g.targetCol) + 2, kGridYR2 + 2,
                            gridColWR2(g.targetCol) - 4, kGridHR2 - 4, kTargetColor);
    }
    if (g.selectedCol >= 0) {
        int x = gridColXR2(g.selectedCol), w = gridColWR2(g.selectedCol);
        sf_hal::probe::display().drawRect(x, kGridYR2, w, kGridHR2, kMarkerColor);
        sf_hal::probe::display().drawRect(x + 1, kGridYR2 + 1, w - 2, kGridHR2 - 2, kMarkerColor);
        sf_hal::probe::display().drawRect(x + 2, kGridYR2 + 2, w - 4, kGridHR2 - 4, kMarkerColor);
    }
}

void setGridR2(bool active, int targetCol, int selectedCol) {
    if (s_grid.drawn && s_grid.active == active && s_grid.targetCol == targetCol &&
        s_grid.selectedCol == selectedCol) {
        return;
    }
    s_grid.drawn = true;
    s_grid.active = active; s_grid.targetCol = targetCol; s_grid.selectedCol = selectedCol;
    drawGridR2(s_grid);
}

} // namespace

void initRenderR2() {
    sf_hal::probe::display().fillScreen(TFT_BLACK);
    s_cue = CueStateR2{};
    s_grid = GridStateR2{};
}

void renderDrillCueR2(uint32_t nowMs, const RenderInputsR2& in) {
    (void)nowMs;
    switch (in.drill) {
        case Drill::BootWait:
            setGridR2(false, -1, -1);
            renderCueR2("S3B", "BLUE=advance", Drill::BootWait);
            break;
        case Drill::T1Rest: {
            setGridR2(false, -1, -1);
            int64_t elapsedUs =
                static_cast<int64_t>(sf_hal::nowUs()) - in.drillStartUs;
            int sec = (int)(elapsedUs / (1000 * 1000));
            int remain = 60 - sec;
            if (remain < 0) remain = 0;
            char text[8]; snprintf(text, sizeof(text), "%d", remain);
            char status[48];
            if (in.t1Finished) snprintf(status, sizeof(status), "REST done BLUE=next");
            else snprintf(status, sizeof(status), "REST hold still %ds", remain);
            renderCueR2(text, status, Drill::T1Rest);
            break;
        }
        case Drill::T9PressJolt: {
            setGridR2(false, -1, -1);
            char text[8]; snprintf(text, sizeof(text), "%d", in.t9Presses);
            char status[48];
            snprintf(status, sizeof(status), "press BLUE 20x done=%d", in.t9Presses);
            renderCueR2(text, status, Drill::T9PressJolt);
            break;
        }
        case Drill::T6Absolute: {
            char text[8];
            // Selected column as a large digit beside the target, e.g. "4:6".
            snprintf(text, sizeof(text), "%d:%d", in.t6SelectedCol, in.t6TargetCol);
            char status[48];
            snprintf(status, sizeof(status), "col=%d target=%d BLUE=confirm",
                     in.t6SelectedCol, in.t6TargetCol);
            setGridR2(true, in.t6TargetCol, in.t6SelectedCol);
            renderCueR2(text, status, Drill::T6Absolute);
            break;
        }
        case Drill::T7Twist: {
            char text[16];
            if (in.t7Block == 1) {
                snprintf(text, sizeof(text), "%s", in.t7Cue);
            } else if (in.t7Block == 2) {
                snprintf(text, sizeof(text), "C%d", in.t7Trial);
            } else if (in.t7Block == 3) {
                snprintf(text, sizeof(text), "E%d", in.t7Trial);
            } else {
                text[0] = '\0';
            }
            char status[56];
            snprintf(status, sizeof(status), "%s BLUE=next", in.t7Cue);
            setGridR2(false, -1, -1);
            renderCueR2(text, status, Drill::T7Twist);
            break;
        }
        case Drill::Done:
            setGridR2(false, -1, -1);
            renderCueR2("END", "BLUE=restart", Drill::Done);
            break;
        default:
            setGridR2(false, -1, -1);
            renderCueR2("S3B", "BLUE=advance", Drill::BootWait);
            break;
    }
}

} // namespace sf
