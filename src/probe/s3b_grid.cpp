// SPIKE S3b round-1 grid rendering. Layout is read from s3b_constants.h.
#include "s3b_grid.h"
#include "s3b_constants.h"
#include <M5Unified.h>

#include "hal/sticks3/panel.h"

namespace sf {

namespace {

constexpr int kGridY = 110;
constexpr uint16_t kGridOutlineColor = 0xFFFF; // white
constexpr int kGridMarkerColor = 0x07E0;  // green
constexpr int kGridTargetColor = 0xF800;  // red

struct GridState {
    bool active = false;
    bool targetVisible = false;
    int markerCol = -1;
    int targetCol = -1;
    bool targetFilled = false;
    bool selectedOutlined = false;
    int selectedCol = -1;
};
GridState s_grid;

void drawGrid(const GridState& g) {
    sf_hal::probe::display().fillRect(kGridX, kGridY, kGridCols * kGridCellSize, kGridCellSize, TFT_BLACK);
    if (!g.active) return;
    for (int c = 0; c < kGridCols; ++c) {
        int x = kGridX + c * kGridCellSize;
        sf_hal::probe::display().drawRect(x, kGridY, kGridCellSize, kGridCellSize, kGridOutlineColor);
    }
    if (g.targetVisible && g.targetCol >= 0) {
        int x = kGridX + g.targetCol * kGridCellSize;
        if (g.targetFilled) {
            sf_hal::probe::display().fillRect(x + 2, kGridY + 2, kGridCellSize - 4, kGridCellSize - 4, kGridTargetColor);
        } else {
            sf_hal::probe::display().drawRect(x, kGridY, kGridCellSize, kGridCellSize, kGridTargetColor);
            sf_hal::probe::display().drawRect(x + 1, kGridY + 1, kGridCellSize - 2, kGridCellSize - 2, kGridTargetColor);
        }
    }
    if (g.selectedOutlined && g.selectedCol >= 0) {
        int x = kGridX + g.selectedCol * kGridCellSize;
        sf_hal::probe::display().drawRect(x, kGridY, kGridCellSize, kGridCellSize, kGridMarkerColor);
        sf_hal::probe::display().drawRect(x + 1, kGridY + 1, kGridCellSize - 2, kGridCellSize - 2, kGridMarkerColor);
    }
    if (g.markerCol >= 0) {
        int x = kGridX + g.markerCol * kGridCellSize;
        sf_hal::probe::display().fillRect(x + 2, kGridY + 2, kGridCellSize - 4, kGridCellSize - 4, kGridMarkerColor);
    }
}

} // namespace

void initGrid() { s_grid = GridState{}; }

void setGrid(bool active, bool targetVisible, int markerCol, int targetCol,
             bool targetFilled, bool selectedOutlined, int selectedCol) {
    if (s_grid.active == active && s_grid.targetVisible == targetVisible &&
        s_grid.markerCol == markerCol && s_grid.targetCol == targetCol &&
        s_grid.targetFilled == targetFilled && s_grid.selectedOutlined == selectedOutlined &&
        s_grid.selectedCol == selectedCol) {
        return;
    }
    s_grid = {active, targetVisible, markerCol, targetCol, targetFilled, selectedOutlined, selectedCol};
    drawGrid(s_grid);
}

} // namespace sf
