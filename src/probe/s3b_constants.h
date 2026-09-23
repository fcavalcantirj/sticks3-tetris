#pragma once
#include <cstdint>

namespace sf {

// Shared probe constants. The tilt-specific repeat constants and absolute-aim
// geometry are intentionally left in s3b_drills.cpp because later verify steps
// grep for them there.

inline constexpr float kDeadZoneG = 0.20f;
inline constexpr float kHysteresisG = 0.07f;
inline constexpr uint32_t kDwellMs = 40;
inline constexpr int kMedianWindow = 3;

inline constexpr int64_t kEmaRcUs = 80 * 1000;
inline constexpr int64_t kDtMaxUs = 40 * 1000;
inline constexpr uint32_t kMaxMovesPerUpdate = 10;
inline constexpr uint32_t kNeutralCalibMs = 500;
inline constexpr int64_t kNeutralDriftTauUs = 2000 * 1000;
inline constexpr float kNeutralLogThresholdG = 0.05f;
inline constexpr uint32_t kNeutralLogMinIntervalMs = 10000;

inline constexpr int kGridCols = 10;
inline constexpr int kGridCellSize = 10;
inline constexpr int kGridX = 1;

} // namespace sf
