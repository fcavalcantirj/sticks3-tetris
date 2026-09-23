#include "hal/sticks3/panel.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>

#include "hal/sticks3/palette.h"
#include "hal/sticks3/sysinfo.h"
#include "stackfall/core/piece.h"
#include "stackfall/ui/layout.h"

namespace sf_hal {
namespace {

constexpr uint32_t kSpriteBytes = 64800;
constexpr uint16_t kPieceColours[] = {
    palette::kBg, 0x07FF, 0x001F, 0xFD20,
    0xFFE0,       0x07E0, 0xF81F, 0xF800,
};

static_assert(sf::ui::kScreenW == 135);
static_assert(sf::ui::kScreenH == 240);
static_assert(kSpriteBytes ==
              static_cast<uint32_t>(sf::ui::kScreenW) *
                  static_cast<uint32_t>(sf::ui::kScreenH) * 2u);
static_assert(sizeof(kPieceColours) / sizeof(kPieceColours[0]) == 8);

struct Point {
  int8_t col;
  int8_t row;
};

constexpr bool hasArea(sf::ui::Rect rect) {
  return rect.w > 0 && rect.h > 0;
}

uint16_t darker(uint16_t colour) {
  const uint16_t red = static_cast<uint16_t>(((colour >> 11u) & 0x1Fu) / 2u);
  const uint16_t green =
      static_cast<uint16_t>(((colour >> 5u) & 0x3Fu) / 2u);
  const uint16_t blue = static_cast<uint16_t>((colour & 0x1Fu) / 2u);
  return static_cast<uint16_t>((red << 11u) | (green << 5u) | blue);
}

uint16_t pieceColour(uint8_t paletteIndex) {
  return paletteIndex < sizeof(kPieceColours) / sizeof(kPieceColours[0])
             ? kPieceColours[paletteIndex]
             : palette::kText;
}

void sortPoints(Point (&points)[4]) {
  for (int i = 1; i < 4; ++i) {
    const Point value = points[i];
    int j = i;
    while (j > 0 &&
           (points[j - 1].row > value.row ||
            (points[j - 1].row == value.row &&
             points[j - 1].col > value.col))) {
      points[j] = points[j - 1];
      --j;
    }
    points[j] = value;
  }
}

void normalizePoints(Point (&points)[4]) {
  int8_t minCol = points[0].col;
  int8_t minRow = points[0].row;
  for (int i = 1; i < 4; ++i) {
    minCol = std::min(minCol, points[i].col);
    minRow = std::min(minRow, points[i].row);
  }
  for (Point& point : points) {
    point.col = static_cast<int8_t>(point.col - minCol);
    point.row = static_cast<int8_t>(point.row - minRow);
  }
  sortPoints(points);
}

bool samePoints(const Point (&left)[4], const Point (&right)[4]) {
  for (int i = 0; i < 4; ++i) {
    if (left[i].col != right[i].col || left[i].row != right[i].row) {
      return false;
    }
  }
  return true;
}

uint8_t inferActivePalette(const sf::ui::PlayingModel& model) {
  Point actual[4]{};
  for (int i = 0; i < 4; ++i) {
    const sf::ui::Rect rect = model.active[i];
    if (rect.w != sf::ui::kCell || rect.h != sf::ui::kCell ||
        !sf::ui::contains(sf::ui::playfieldRect(), rect)) {
      return 0;
    }
    const int x = rect.x - sf::ui::kFieldX;
    const int y = rect.y - sf::ui::kFieldY;
    if (x % sf::ui::kCell != 0 || y % sf::ui::kCell != 0) {
      return 0;
    }
    actual[i] = Point{static_cast<int8_t>(x / sf::ui::kCell),
                      static_cast<int8_t>(y / sf::ui::kCell)};
  }
  normalizePoints(actual);

  for (int piece = 0; piece < sf::kPieceCount; ++piece) {
    const sf::PieceId id = static_cast<sf::PieceId>(piece);
    for (int state = 0; state < 4; ++state) {
      Point candidate[4]{};
      const sf::Shape& shape = sf::shapeOf(id, state);
      for (int i = 0; i < 4; ++i) {
        candidate[i] = Point{shape[static_cast<std::size_t>(i)].col,
                             shape[static_cast<std::size_t>(i)].row};
      }
      normalizePoints(candidate);
      if (samePoints(actual, candidate)) {
        return static_cast<uint8_t>(piece + 1);
      }
    }
  }
  return 0;
}

void drawMino(LovyanGFX& graphics, sf::ui::Rect rect, uint16_t colour) {
  if (!hasArea(rect)) return;
  graphics.fillRect(rect.x, rect.y, rect.w, rect.h, colour);
  if (rect.w > 2 && rect.h > 2) {
    const uint16_t edge = darker(colour);
    graphics.drawFastHLine(rect.x + 1, rect.y + rect.h - 2, rect.w - 2,
                           edge);
    graphics.drawFastVLine(rect.x + rect.w - 2, rect.y + 1, rect.h - 2,
                           edge);
  }
}

void drawPreview(LovyanGFX& graphics, sf::ui::Rect box, uint8_t palette,
                 bool dimmed) {
  if (palette == 0 || palette > sf::kPieceCount) return;

  const sf::PieceId id = static_cast<sf::PieceId>(palette - 1u);
  const sf::Shape& shape = sf::shapeOf(id, 0);
  int8_t minCol = shape[0].col;
  int8_t maxCol = shape[0].col;
  int8_t minRow = shape[0].row;
  int8_t maxRow = shape[0].row;
  for (const sf::Offset offset : shape) {
    minCol = std::min(minCol, offset.col);
    maxCol = std::max(maxCol, offset.col);
    minRow = std::min(minRow, offset.row);
    maxRow = std::max(maxRow, offset.row);
  }

  const int width = maxCol - minCol + 1;
  const int height = maxRow - minRow + 1;
  const int xShift = (4 - width) * sf::ui::kMiniCell / 2;
  const int yShift = (4 - height) * sf::ui::kMiniCell / 2;
  uint16_t colour = pieceColour(palette);
  if (dimmed) colour = darker(colour);

  for (const sf::Offset offset : shape) {
    sf::ui::Rect rect = sf::ui::miniCellRect(
        box, offset.col - minCol, offset.row - minRow);
    rect.x = static_cast<int16_t>(rect.x + xShift);
    rect.y = static_cast<int16_t>(rect.y + yShift);
    drawMino(graphics, rect, colour);
  }
}

void drawPlayfield(LovyanGFX& graphics,
                   const sf::ui::PlayingModel& model,
                   uint8_t activePalette) {
  graphics.fillRect(sf::ui::kFrameLeftX, sf::ui::kFieldY, 1,
                    sf::ui::kFieldH, palette::kFrame);
  graphics.fillRect(sf::ui::kFrameRightX, sf::ui::kFieldY, 1,
                    sf::ui::kFieldH, palette::kFrame);

  for (int row = 0; row < sf::ui::kRows; ++row) {
    const int boardRow = row + sf::ui::kHiddenRows;
    for (int col = 0; col < sf::ui::kCols; ++col) {
      if (!sf::ui::cellVisible(col, boardRow)) continue;
      const uint8_t palette = model.field[row][col];
      if (palette == 0) continue;
      drawMino(graphics, sf::ui::cellRect(col, boardRow),
               pieceColour(palette));
    }
  }

  const uint16_t activeColour = pieceColour(activePalette);
  if (!model.ghostHidden) {
    for (const sf::ui::Rect rect : model.ghost) {
      if (!hasArea(rect) ||
          !sf::ui::contains(sf::ui::playfieldRect(), rect)) {
        continue;
      }
      graphics.drawRect(rect.x, rect.y, rect.w, rect.h, palette::kGhost);
    }
  }
  for (const sf::ui::Rect rect : model.active) {
    if (!hasArea(rect) ||
        !sf::ui::contains(sf::ui::playfieldRect(), rect)) {
      continue;
    }
    drawMino(graphics, rect, activeColour);
  }
}

void drawSidebar(LovyanGFX& graphics, const sf::ui::PlayingModel& model,
                 sf::ui::Rect region) {
  const sf::ui::Rect hold = sf::ui::holdBoxRect();
  if (sf::ui::intersects(region, hold)) {
    graphics.drawRect(hold.x, hold.y, hold.w, hold.h, palette::kFrame);
    drawPreview(graphics, hold, model.hold, model.holdUsed);
  }

  const sf::ui::Rect caption = sf::ui::nextCaptionRect();
  if (sf::ui::intersects(region, caption)) {
    graphics.setFont(&fonts::Font0);
    graphics.setTextSize(1);
    graphics.setTextDatum(textdatum_t::top_left);
    graphics.setTextColor(palette::kText, palette::kBg);
    graphics.setCursor(caption.x, caption.y);
    graphics.print("NEXT");
  }

  for (int i = 0; i < 3; ++i) {
    const sf::ui::Rect box = sf::ui::nextBoxRect(i);
    if (sf::ui::intersects(region, box)) {
      drawPreview(graphics, box, model.next[i], false);
    }
  }
}

sf::ui::Rect clippedToScreen(sf::ui::Rect rect) {
  const int left = std::max<int>(rect.x, 0);
  const int top = std::max<int>(rect.y, 0);
  const int right = std::min<int>(rect.x + rect.w, sf::ui::kScreenW);
  const int bottom = std::min<int>(rect.y + rect.h, sf::ui::kScreenH);
  if (left >= right || top >= bottom) return sf::ui::Rect{0, 0, 0, 0};
  return sf::ui::Rect{static_cast<int16_t>(left),
                      static_cast<int16_t>(top),
                      static_cast<int16_t>(right - left),
                      static_cast<int16_t>(bottom - top)};
}

void drawRegion(LovyanGFX& graphics, const sf::ui::PlayingModel& model,
                sf::ui::Rect region, uint8_t activePalette) {
  region = clippedToScreen(region);
  if (!hasArea(region)) return;
  graphics.setClipRect(region.x, region.y, region.w, region.h);
  graphics.fillRect(region.x, region.y, region.w, region.h, palette::kBg);
  drawPlayfield(graphics, model, activePalette);
  drawSidebar(graphics, model, region);
  graphics.clearClipRect();
}

}  // namespace

Panel::Panel() : canvas_(&M5.Display) {}

bool Panel::begin() {
  M5.Display.setRotation(0);
  if (width() != sf::ui::kScreenW || height() != sf::ui::kScreenH) {
    return false;
  }

  heapBefore_ = freeHeap();
  canvas_.setColorDepth(16);
  // The parent-aware canvas constructor opts into PSRAM; Stackfall keeps its
  // only full-screen buffer in internal DRAM, matching the measured S1 result.
  canvas_.setPsram(false);
  sprite_ = (canvas_.createSprite(135, 240) != nullptr);
  // LGFXBase.cpp:2413-2421 wraps print() against _clip_l/_clip_r, so text
  // placed outside a dirty clip can wrap back into it. Stackfall's fixed-layout
  // strings are explicitly positioned and fitted; overflow must clip, not wrap.
  canvas_.setTextWrap(false, false);
  M5.Display.setTextWrap(false, false);
  heapAfter_ = freeHeap();
  if (sprite_) canvas_.fillScreen(palette::kBg);
  return true;
}

LovyanGFX& Panel::target() {
  return sprite_ ? static_cast<LovyanGFX&>(canvas_)
                 : static_cast<LovyanGFX&>(M5.Display);
}

int Panel::width() const { return M5.Display.width(); }

int Panel::height() const { return M5.Display.height(); }

void Panel::setBrightness(uint8_t value) {
  M5.Display.setBrightness(std::max<uint8_t>(8u, value));
}

uint32_t Panel::spriteBytes() const { return sprite_ ? kSpriteBytes : 0u; }

void Panel::drawPlaying(const sf::ui::PlayingModel& model,
                        const sf::ui::DrawPlan& plan) {
  if (!plan.full && plan.count == 0) return;

  const uint8_t inferred = inferActivePalette(model);
  if (inferred != 0) activePalette_ = inferred;
  LovyanGFX& graphics = target();
  if (plan.full) {
    const sf::ui::Rect full{0, 0, sf::ui::kScreenW, sf::ui::kScreenH};
    drawRegion(graphics, model, full, activePalette_);
    drawPlayingHudRegion(model, full);
    return;
  }
  for (uint8_t i = 0; i < plan.count; ++i) {
    drawRegion(graphics, model, plan.rects[i], activePalette_);
    drawPlayingHudRegion(model, plan.rects[i]);
  }
}

void Panel::pushFrame(const sf::ui::DrawPlan& plan) {
  if (!plan.full && plan.count == 0) return;
  if (sprite_) canvas_.pushSprite(0, 0);
}

namespace probe {

M5GFX& display() { return M5.Display; }

}  // namespace probe

}  // namespace sf_hal
