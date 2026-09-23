#include <array>
#include <cstddef>
#include <cstdio>

#include "framework.h"
#include "stackfall/core/ghost.h"
#include "stackfall/core/spawn.h"
#include "stackfall/ui/layout.h"

namespace {

using sf::ui::Rect;

template <std::size_t N>
bool appendVisibleCell(std::array<Rect, N>& drawList, int& count, int col, int boardRow) {
  if (!sf::ui::cellVisible(col, boardRow)) return false;
  drawList[static_cast<std::size_t>(count++)] = sf::ui::cellRect(col, boardRow);
  return true;
}

void assertRect(Rect actual, int x, int y, int w, int h) {
  ASSERT_EQ(actual.x, x);
  ASSERT_EQ(actual.y, y);
  ASSERT_EQ(actual.w, w);
  ASSERT_EQ(actual.h, h);
}

}  // namespace

SF_TEST(layout_geometry_constants_and_boxes) {
  ASSERT_EQ(sf::ui::kScreenW, 135);
  ASSERT_EQ(sf::ui::kScreenH, 240);
  ASSERT_EQ(sf::ui::kCols, 10);
  ASSERT_EQ(sf::ui::kRows, 20);
  ASSERT_EQ(sf::ui::kCell, 10);
  ASSERT_EQ(sf::ui::kHiddenRows, 20);

  assertRect(sf::ui::topBarRect(), 0, 0, 135, 18);
  assertRect(sf::ui::playfieldRect(), 1, 18, 100, 200);
  assertRect(sf::ui::footerRect(), 0, 218, 135, 22);
  assertRect(sf::ui::sidebarRect(), 102, 18, 33, 200);
  assertRect(sf::ui::holdBoxRect(), 104, 22, 28, 28);
  assertRect(sf::ui::nextCaptionRect(), 104, 51, 24, 8);
  assertRect(sf::ui::nextBoxRect(0), 104, 60, 28, 28);
  assertRect(sf::ui::nextBoxRect(1), 104, 104, 28, 28);
  assertRect(sf::ui::nextBoxRect(2), 104, 148, 28, 28);

  ASSERT_TRUE(sf::ui::cellVisible(0, 20));
  ASSERT_TRUE(sf::ui::cellVisible(9, 39));
  ASSERT_FALSE(sf::ui::cellVisible(-1, 20));
  ASSERT_FALSE(sf::ui::cellVisible(10, 20));
  ASSERT_FALSE(sf::ui::cellVisible(0, 19));
  ASSERT_FALSE(sf::ui::cellVisible(0, 40));
  assertRect(sf::ui::cellRect(0, 20), 1, 18, 10, 10);
  assertRect(sf::ui::cellRect(9, 39), 91, 208, 10, 10);
}

SF_TEST(layout_spawn_bounds_proof) {
  constexpr int kPlacementCount = sf::kPieceCount * 4 * 10;
  constexpr int kMinoCount = kPlacementCount * 4;
  std::array<Rect, kMinoCount> drawList{};
  int placements = 0;
  int minos = 0;
  int drawn = 0;
  int clipped = 0;

  for (int piece = 0; piece < sf::kPieceCount; ++piece) {
    const sf::PieceId id = static_cast<sf::PieceId>(piece);
    const int spawnRow = sf::spawnOf(id).row;
    for (int state = 0; state < 4; ++state) {
      const sf::Shape& shape = sf::shapeOf(id, state);
      for (int spawnCol = 0; spawnCol < sf::ui::kCols; ++spawnCol) {
        ++placements;
        for (const sf::Offset mino : shape) {
          ++minos;
          const int col = spawnCol + mino.col;
          const int row = spawnRow + mino.row;
          const bool visible = sf::ui::cellVisible(col, row);
          const int drawCountBefore = drawn;
          const bool appended = appendVisibleCell(drawList, drawn, col, row);

          ASSERT_EQ(appended, visible);
          if (!visible) {
            ++clipped;
            ASSERT_EQ(drawn, drawCountBefore);
            continue;
          }

          const Rect rect = drawList[static_cast<std::size_t>(drawn - 1)];
          ASSERT_TRUE(sf::ui::contains(sf::ui::playfieldRect(), rect));
          ASSERT_TRUE(rect.y >= sf::ui::kFieldY);
          ASSERT_TRUE(rect.y + rect.h <= sf::ui::kFooterY);
          ASSERT_TRUE(rect.x >= sf::ui::kFieldX);
          ASSERT_TRUE(rect.x + rect.w <= sf::ui::kFrameRightX);
        }
      }
    }
  }

  ASSERT_EQ(placements, 280);
  ASSERT_EQ(minos, 1120);
  ASSERT_EQ(drawn + clipped, 1120);
  ASSERT_TRUE(clipped > 0);
  std::printf("LAYOUT placements=%d minos=%d drawn=%d clipped=%d\n", placements, minos,
              drawn, clipped);
}

SF_TEST(layout_ghost_and_preview_guards) {
  const std::array<Rect, 4> boxes = {sf::ui::holdBoxRect(), sf::ui::nextBoxRect(0),
                                     sf::ui::nextBoxRect(1), sf::ui::nextBoxRect(2)};

  for (int piece = 0; piece < sf::kPieceCount; ++piece) {
    const sf::PieceId id = static_cast<sf::PieceId>(piece);
    const sf::ActivePiece active = sf::spawnOf(id);
    const sf::Board board;
    const int ghostBaseRow = sf::ghostRow(board, active);
    for (const sf::Offset mino : sf::shapeOf(id, active.state)) {
      const int col = active.col + mino.col;
      const int row = ghostBaseRow + mino.row;
      ASSERT_TRUE(sf::ui::cellVisible(col, row));
      ASSERT_TRUE(sf::ui::contains(sf::ui::playfieldRect(), sf::ui::cellRect(col, row)));
    }

    for (int state = 0; state < 4; ++state) {
      for (const sf::Offset mino : sf::shapeOf(id, state)) {
        for (const Rect box : boxes) {
          const Rect preview = sf::ui::miniCellRect(box, mino.col, mino.row);
          ASSERT_TRUE(sf::ui::contains(box, preview));
        }
      }
    }
  }

  assertRect(sf::ui::miniCellRect(sf::ui::holdBoxRect(), 0, 0), 106, 24, 6, 6);
  assertRect(sf::ui::miniCellRect(sf::ui::holdBoxRect(), 3, 3), 124, 42, 6, 6);
}

SF_TEST(layout_next_caption_dirty_region_guards) {
  const Rect caption = sf::ui::nextCaptionRect();
  for (int row = 0; row < sf::ui::kRows; ++row) {
    const Rect playfieldBand{
        sf::ui::kFieldX,
        static_cast<int16_t>(sf::ui::kFieldY + row * sf::ui::kCell),
        sf::ui::kFieldW, sf::ui::kCell};
    ASSERT_FALSE(sf::ui::intersects(caption, playfieldBand));
  }

  // This is precisely why an unguarded print() under a playfield-band clip
  // wrapped NEXT to _clip_l instead of drawing at its explicit x coordinate.
  ASSERT_TRUE(caption.x + caption.w >
              sf::ui::kFieldX + sf::ui::kFieldW);

  const Rect base{10, 10, 10, 10};
  ASSERT_TRUE(sf::ui::intersects(base, Rect{19, 10, 10, 10}));
  ASSERT_FALSE(sf::ui::intersects(base, Rect{20, 10, 10, 10}));
  ASSERT_FALSE(sf::ui::intersects(base, Rect{10, 20, 10, 10}));
  ASSERT_FALSE(sf::ui::intersects(base, Rect{20, 20, 10, 10}));
  ASSERT_FALSE(sf::ui::intersects(base, Rect{21, 10, 10, 10}));
}
