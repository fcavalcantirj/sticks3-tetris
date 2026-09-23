#pragma once

#include <cstddef>
#include <cstdint>

#include "stackfall/ui/screens.h"

namespace sf::ui {

struct DrawPlan {
  bool full;
  Rect rects[8];
  uint8_t count;
  uint32_t estUs;
  uint16_t collapses;
};

constexpr uint32_t estUs(Rect r) {
  return static_cast<uint32_t>(r.w) * static_cast<uint32_t>(r.h) * 16u /
         40u;
}

static_assert(estUs({0, 0, 135, 240}) == 12960);
static_assert(estUs({1, 18, 100, 200}) == 8000);
static_assert(estUs({0, 0, 10, 10}) == 40);

namespace plan_detail {

constexpr bool sameRect(Rect a, Rect b) {
  return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

template <std::size_t N>
constexpr bool sameChars(const char (&a)[N], const char (&b)[N]) {
  for (std::size_t i = 0; i < N; ++i) {
    if (a[i] != b[i]) return false;
  }
  return true;
}

constexpr bool sameText(const char* a, const char* b) {
  if (a == b) return true;
  if (a == nullptr || b == nullptr) return false;
  std::size_t i = 0;
  while (a[i] != '\0' && b[i] != '\0') {
    if (a[i] != b[i]) return false;
    ++i;
  }
  return a[i] == b[i];
}

constexpr bool sameHud(const HudText& a, const HudText& b) {
  return sameChars(a.left, b.left) && a.leftSize == b.leftSize &&
         sameChars(a.mid, b.mid) && sameChars(a.right, b.right);
}

constexpr bool hasArea(Rect r) { return r.w > 0 && r.h > 0; }

constexpr int32_t right(Rect r) {
  return static_cast<int32_t>(r.x) + r.w;
}

constexpr int32_t bottom(Rect r) {
  return static_cast<int32_t>(r.y) + r.h;
}

constexpr bool adjacentOrOverlapping(Rect a, Rect b) {
  return hasArea(a) && hasArea(b) && static_cast<int32_t>(a.x) <= right(b) &&
         static_cast<int32_t>(b.x) <= right(a) &&
         static_cast<int32_t>(a.y) <= bottom(b) &&
         static_cast<int32_t>(b.y) <= bottom(a);
}

constexpr int32_t min32(int32_t a, int32_t b) { return a < b ? a : b; }

constexpr int32_t max32(int32_t a, int32_t b) { return a > b ? a : b; }

constexpr Rect united(Rect a, Rect b) {
  const int32_t x = min32(a.x, b.x);
  const int32_t y = min32(a.y, b.y);
  const int32_t r = max32(right(a), right(b));
  const int32_t d = max32(bottom(a), bottom(b));
  return Rect{static_cast<int16_t>(x), static_cast<int16_t>(y),
              static_cast<int16_t>(r - x), static_cast<int16_t>(d - y)};
}

inline void updateCost(DrawPlan& plan) {
  plan.estUs = 0;
  for (uint8_t i = 0; i < plan.count; ++i) {
    plan.estUs += sf::ui::estUs(plan.rects[i]);
  }
}

inline void useFullFrame(DrawPlan& plan, bool collapsed) {
  plan.full = true;
  plan.count = 0;
  plan.estUs = sf::ui::estUs(Rect{0, 0, kScreenW, kScreenH});
  if (collapsed) ++plan.collapses;
}

inline void addRect(DrawPlan& plan, Rect rect) {
  if (plan.full || !hasArea(rect)) return;

  bool merged = true;
  while (merged) {
    merged = false;
    for (uint8_t i = 0; i < plan.count; ++i) {
      if (!adjacentOrOverlapping(rect, plan.rects[i])) continue;
      rect = united(rect, plan.rects[i]);
      --plan.count;
      plan.rects[i] = plan.rects[plan.count];
      merged = true;
      break;
    }
  }

  if (plan.count == 8) {
    useFullFrame(plan, true);
    return;
  }
  plan.rects[plan.count++] = rect;
  updateCost(plan);
}

constexpr bool sameField(const PlayingModel& a, const PlayingModel& b) {
  for (int row = 0; row < kRows; ++row) {
    for (int col = 0; col < kCols; ++col) {
      if (a.field[row][col] != b.field[row][col]) return false;
    }
  }
  return true;
}

constexpr bool sameRects(const Rect (&a)[4], const Rect (&b)[4]) {
  for (int i = 0; i < 4; ++i) {
    if (!sameRect(a[i], b[i])) return false;
  }
  return true;
}

constexpr bool sameNext(const uint8_t (&a)[3], const uint8_t (&b)[3]) {
  for (int i = 0; i < 3; ++i) {
    if (a[i] != b[i]) return false;
  }
  return true;
}

inline bool boardNeedsFullFrame(const PlayingModel& prev,
                                const PlayingModel& next) {
  for (int row = 0; row < kRows; ++row) {
    for (int col = 0; col < kCols; ++col) {
      const uint8_t oldCell = prev.field[row][col];
      const uint8_t newCell = next.field[row][col];
      if (oldCell != 0u && oldCell != newCell) return true;
    }
  }
  return false;
}

inline Rect activeRowBand(const PlayingModel& prev,
                          const PlayingModel& next) {
  int32_t top = kFieldY + kFieldH;
  int32_t lower = kFieldY;
  const Rect field = playfieldRect();
  const Rect* groups[] = {prev.active, next.active};
  for (const Rect* group : groups) {
    for (int i = 0; i < 4; ++i) {
      const Rect rect = group[i];
      if (!hasArea(rect)) continue;
      const int32_t clippedTop = max32(rect.y, field.y);
      const int32_t clippedBottom = min32(bottom(rect), bottom(field));
      if (clippedTop >= clippedBottom) continue;
      top = min32(top, clippedTop);
      lower = max32(lower, clippedBottom);
    }
  }
  if (top >= lower) return Rect{0, 0, 0, 0};

  const int32_t firstRow = (top - kFieldY) / kCell;
  const int32_t lastRow = (lower - 1 - kFieldY) / kCell;
  return Rect{kFieldX,
              static_cast<int16_t>(kFieldY + firstRow * kCell), kFieldW,
              static_cast<int16_t>((lastRow - firstRow + 1) * kCell)};
}

inline void addGhostCells(DrawPlan& plan, const PlayingModel& prev,
                          const PlayingModel& next) {
  for (int i = 0; i < 4 && !plan.full; ++i) {
    if (!prev.ghostHidden) addRect(plan, prev.ghost[i]);
    if (!next.ghostHidden) addRect(plan, next.ghost[i]);
  }
}

}  // namespace plan_detail

inline bool operator==(const PlayingModel& a, const PlayingModel& b) {
  if (!plan_detail::sameField(a, b) ||
      !plan_detail::sameRects(a.active, b.active) ||
      !plan_detail::sameRects(a.ghost, b.ghost) ||
      a.ghostHidden != b.ghostHidden || a.hold != b.hold ||
      a.holdUsed != b.holdUsed || !plan_detail::sameNext(a.next, b.next) ||
      !plan_detail::sameHud(a.hud, b.hud) ||
      !plan_detail::sameText(a.banner, b.banner) ||
      a.bannerMsLeft != b.bannerMsLeft) {
    return false;
  }
  return true;
}

inline bool operator!=(const PlayingModel& a, const PlayingModel& b) {
  return !(a == b);
}

inline DrawPlan diff(const PlayingModel& prev, const PlayingModel& next) {
  DrawPlan plan{};
  if (prev == next) return plan;

  const bool boardChanged = !plan_detail::sameField(prev, next);
  if (boardChanged && plan_detail::boardNeedsFullFrame(prev, next)) {
    plan_detail::useFullFrame(plan, false);
    return plan;
  }

  if (boardChanged) {
    for (int row = 0; row < kRows && !plan.full; ++row) {
      for (int col = 0; col < kCols && !plan.full; ++col) {
        if (prev.field[row][col] != next.field[row][col]) {
          plan_detail::addRect(
              plan, cellRect(col, row + static_cast<int>(kHiddenRows)));
        }
      }
    }
  }

  const bool activeChanged =
      !plan_detail::sameRects(prev.active, next.active);
  if (activeChanged) {
    plan_detail::addRect(plan, plan_detail::activeRowBand(prev, next));
  }
  if (!plan_detail::sameRects(prev.ghost, next.ghost) ||
      prev.ghostHidden != next.ghostHidden) {
    // Piece motion and its projection can change in the same frame. The active
    // row band does not reach the old or new ghost near the stack.
    plan_detail::addGhostCells(plan, prev, next);
  }

  if (!plan_detail::sameHud(prev.hud, next.hud)) {
    plan_detail::addRect(plan, topBarRect());
  }
  if (prev.hold != next.hold || prev.holdUsed != next.holdUsed) {
    plan_detail::addRect(plan, holdBoxRect());
  }
  for (int i = 0; i < 3; ++i) {
    if (prev.next[i] != next.next[i]) {
      plan_detail::addRect(plan, nextBoxRect(i));
    }
  }
  if (!plan_detail::sameText(prev.banner, next.banner) ||
      prev.bannerMsLeft != next.bannerMsLeft) {
    plan_detail::addRect(plan, footerRect());
  }

  return plan;
}

}  // namespace sf::ui
