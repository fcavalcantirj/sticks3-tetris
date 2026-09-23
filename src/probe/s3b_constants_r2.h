#pragma once
#include <cstdint>

namespace sf {

// Round-2 probe constants. These are intentionally separate from the round-1
// constants so the SF_PROBE=4 build stays a reproducible record.

// Panel is 135x240 at setRotation(0) (MEASURED, see docs/DEVICES.md).
inline constexpr int kPanelWR2 = 135;
inline constexpr int kPanelHR2 = 240;
inline constexpr int kGridColsR2 = 10;

// The grid spans the panel EDGE TO EDGE. 135/10 is not an integer, so columns are
// 13 or 14 px wide rather than a uniform 13 with dead space on the right - the owner
// reported "starts on total left, BUT DOESNT FINISH ON TOTAL RIGHT" twice, and a
// fixed cell size cannot fix that on a 135 px panel.
inline constexpr int gridColXR2(int c) { return c * kPanelWR2 / kGridColsR2; }
inline constexpr int gridColWR2(int c) { return gridColXR2(c + 1) - gridColXR2(c); }
static_assert(gridColXR2(0) == 0, "grid must start at the left edge");
static_assert(gridColXR2(kGridColsR2) == kPanelWR2, "grid must end at the right edge");

// SCREEN REGIONS - these must NOT overlap. The round-2 render drew the grid over the
// full height and then blanked two full-width bands for the cue text and the status
// line, so every column move erased the grid and it "kept vanishing and reappearing".
// Each region owns its own rows and no draw may cross a boundary.
inline constexpr int kGridYR2 = 0;
inline constexpr int kGridHR2 = 150;          // rows   0..149
inline constexpr int kCueYR2  = 152;
inline constexpr int kCueHR2  = 52;           // rows 152..203
inline constexpr int kStatusYR2 = 206;
inline constexpr int kStatusHR2 = kPanelHR2 - kStatusYR2;  // rows 206..239
static_assert(kGridYR2 + kGridHR2 <= kCueYR2, "grid and cue regions overlap");
static_assert(kCueYR2 + kCueHR2 <= kStatusYR2, "cue and status regions overlap");
static_assert(kStatusYR2 + kStatusHR2 == kPanelHR2, "status must reach the bottom");

// CALIBRATION-1 steering geometry, kept for the absolute mapping.
inline constexpr float kGPerColumnR2 = 0.09f;
inline constexpr float kColHysteresisGR2 = 0.02f;

// Smoothing.
inline constexpr int kMedianWindowR2 = 3;
inline constexpr int64_t kEmaRcUsR2 = 80 * 1000;
inline constexpr int64_t kDtMaxUsR2 = 40 * 1000;
inline constexpr uint32_t kMaxMovesPerUpdateR2 = 10;

// Neutral policy: capture at drill start, then one hard snap when quiescent.
inline constexpr uint32_t kNeutralCalibMsR2 = 500;
inline constexpr float kQuietPpGR2 = 0.010f;
inline constexpr uint32_t kQuietWindowMsR2 = 500;
inline constexpr uint32_t kQuietSampleMsR2 = 25;
inline constexpr int kQuietWindowSamplesR2 = kQuietWindowMsR2 / kQuietSampleMsR2; // 20

// Manual re-zero.
inline constexpr uint32_t kManualRezeroHoldMsR2 = 500;

// T1 rest-noise window for pp_med / pp_p95.
inline constexpr uint32_t kT1PpWindowMsR2 = 400;
inline constexpr uint32_t kT1PpSampleMsR2 = 25;
inline constexpr int kT1PpWindowSamplesR2 = kT1PpWindowMsR2 / kT1PpSampleMsR2; // 16

// Timing.
inline constexpr int64_t kT1RestUsR2 = 60 * 1000 * 1000LL;
inline constexpr int64_t kT9DurationUsR2 = 20 * 1000 * 1000LL;
inline constexpr uint32_t kRawLogIntervalMsR2 = 100;
inline constexpr uint32_t kQuietLogIntervalMsR2 = 10000;
inline constexpr uint32_t kMedian3LogIntervalMsR2 = 1000;

// Burst ring: 30 samples at 5 ms cadence covers the last 150 ms.
inline constexpr int kBurstRingSizeR2 = 30;
inline constexpr uint32_t kBurstSampleMsR2 = 5;

// T6 drill.
inline constexpr int kT6WarmupR2 = 3;
inline constexpr int kT6ScoredR2 = 40;
inline constexpr int kT6TotalR2 = kT6WarmupR2 + kT6ScoredR2;

// T7 drill.
inline constexpr int kT7DirTrialsR2 = 20;
inline constexpr int kT7HoldTwistsR2 = 20;
inline constexpr int kT7SteerPassesR2 = 5;

} // namespace sf
