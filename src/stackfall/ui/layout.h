#pragma once

#include <cstdint>

namespace sf::ui {

constexpr int16_t kScreenW = 135;
constexpr int16_t kScreenH = 240;

constexpr int16_t kTopBarY = 0;
constexpr int16_t kTopBarH = 18;
constexpr int16_t kFieldY = 18;
constexpr int16_t kFieldH = 200;
constexpr int16_t kFooterY = 218;
constexpr int16_t kFooterH = 22;

constexpr int16_t kFrameLeftX = 0;
constexpr int16_t kFieldX = 1;
constexpr int16_t kFieldW = 100;
constexpr int16_t kFrameRightX = 101;
constexpr int16_t kSidebarX = 102;
constexpr int16_t kSidebarW = 33;

constexpr int16_t kCols = 10;
constexpr int16_t kRows = 20;
constexpr int16_t kCell = 10;
constexpr int16_t kMiniCell = 6;
constexpr int16_t kHiddenRows = 20;

static_assert(kTopBarH + kFieldH + kFooterH == kScreenH);
static_assert(kFieldX + kFieldW + 1 + kSidebarW == kScreenW);
static_assert(kFieldW == kCols * kCell);
static_assert(kFieldH == kRows * kCell);
static_assert(kMiniCell * 4 <= kSidebarW);

struct Rect {
  int16_t x;
  int16_t y;
  int16_t w;
  int16_t h;
};

constexpr Rect cellRect(int col, int boardRow) {
  return Rect{static_cast<int16_t>(kFieldX + kCell * col),
              static_cast<int16_t>(kFieldY + kCell * (boardRow - kHiddenRows)), kCell, kCell};
}

constexpr bool cellVisible(int col, int boardRow) {
  return col >= 0 && col < kCols && boardRow >= kHiddenRows && boardRow < 2 * kHiddenRows;
}

constexpr Rect topBarRect() { return Rect{0, 0, 135, 18}; }

constexpr Rect playfieldRect() { return Rect{1, 18, 100, 200}; }

constexpr Rect footerRect() { return Rect{0, 218, 135, 22}; }

// Shared with panel_hud.cpp so host tests exercise the actual footer bands.
constexpr Rect kStatusRect{1, 218, 133, 12};
constexpr Rect kBannerRect{0, 230, 135, 10};

constexpr Rect sidebarRect() { return Rect{102, 18, 33, 200}; }

constexpr Rect holdBoxRect() { return Rect{104, 22, 28, 28}; }

constexpr Rect nextCaptionRect() { return Rect{104, 51, 24, 8}; }

constexpr Rect nextBoxRect(int i) {
  return Rect{104, static_cast<int16_t>(60 + 44 * i), 28, 28};
}

constexpr Rect miniCellRect(Rect box, int cx, int cy) {
  constexpr int16_t kPreviewGrid = 4 * kMiniCell;
  return Rect{static_cast<int16_t>(box.x + (box.w - kPreviewGrid) / 2 + cx * kMiniCell),
              static_cast<int16_t>(box.y + (box.h - kPreviewGrid) / 2 + cy * kMiniCell),
              kMiniCell, kMiniCell};
}

constexpr bool contains(Rect outer, Rect inner) {
  return inner.x >= outer.x && inner.y >= outer.y && inner.x + inner.w <= outer.x + outer.w &&
         inner.y + inner.h <= outer.y + outer.h;
}

constexpr bool intersects(Rect a, Rect b) {
  return a.w > 0 && a.h > 0 && b.w > 0 && b.h > 0 &&
         a.x < b.x + b.w && b.x < a.x + a.w &&
         a.y < b.y + b.h && b.y < a.y + a.h;
}

static_assert(contains(sidebarRect(), holdBoxRect()));
static_assert(contains(sidebarRect(), nextCaptionRect()));
static_assert(contains(sidebarRect(), nextBoxRect(0)));
static_assert(contains(sidebarRect(), nextBoxRect(1)));
static_assert(contains(sidebarRect(), nextBoxRect(2)));
static_assert(contains(footerRect(), kStatusRect));
static_assert(contains(footerRect(), kBannerRect));
static_assert(kStatusRect.y + kStatusRect.h <= kBannerRect.y);

}  // namespace sf::ui
