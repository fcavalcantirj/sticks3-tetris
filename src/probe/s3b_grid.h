#pragma once
#include <cstdint>

namespace sf {

// Round-1 grid rendering. The layout constants live in s3b_constants.h so the
// renderer cannot shadow them.
void initGrid();
void setGrid(bool active, bool targetVisible, int markerCol, int targetCol,
             bool targetFilled = false, bool selectedOutlined = false,
             int selectedCol = -1);

} // namespace sf
