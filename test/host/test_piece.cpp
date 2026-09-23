#include <array>
#include <string>

#include "framework.h"
#include "stackfall/core/piece.h"

SF_TEST(piece_shapes_bounded_distinct) {
  for (int p = 0; p < sf::kPieceCount; ++p) {
    sf::PieceId id = static_cast<sf::PieceId>(p);
    for (int s = 0; s < 4; ++s) {
      const sf::Shape& sh = sf::shapeOf(id, s);
      ASSERT_EQ(sh.size(), 4u);
      for (int i = 0; i < 4; ++i) {
        ASSERT_TRUE(sh[size_t(i)].col >= 0);
        ASSERT_TRUE(sh[size_t(i)].row >= 0);
        ASSERT_TRUE(sh[size_t(i)].col < sf::kBoxSize[p]);
        ASSERT_TRUE(sh[size_t(i)].row < sf::kBoxSize[p]);
        for (int j = i + 1; j < 4; ++j) {
          ASSERT_TRUE(sh[size_t(i)] != sh[size_t(j)]);
        }
      }
    }
  }
}

SF_TEST(piece_rotation_consistency) {
  for (int p = 0; p < sf::kPieceCount; ++p) {
    sf::PieceId id = static_cast<sf::PieceId>(p);
    int box = sf::kBoxSize[p];
    for (int s = 0; s < 4; ++s) {
      const sf::Shape& from = sf::shapeOf(id, s);
      const sf::Shape& to = sf::shapeOf(id, (s + 1) & 3);
      for (int i = 0; i < 4; ++i) {
        int nc = box - 1 - from[size_t(i)].row;
        int nr = from[size_t(i)].col;
        bool found = false;
        for (int j = 0; j < 4; ++j) {
          if (to[size_t(j)].col == nc && to[size_t(j)].row == nr) {
            found = true;
            break;
          }
        }
        ASSERT_TRUE(found);
      }
    }
  }
}

SF_TEST(piece_o_identical_and_maps) {
  ASSERT_TRUE(sf::kShapes[3][0] == sf::kShapes[3][1]);
  ASSERT_TRUE(sf::kShapes[3][1] == sf::kShapes[3][2]);
  ASSERT_TRUE(sf::kShapes[3][2] == sf::kShapes[3][3]);
  ASSERT_TRUE(sf::cellFor(sf::PieceId::I) == sf::Cell::I);
  ASSERT_TRUE(sf::cellFor(sf::PieceId::Z) == sf::Cell::Z);
  ASSERT_EQ(sf::letterForCell(sf::Cell::Empty), '.');
  ASSERT_EQ(sf::letterFor(sf::PieceId::T), 'T');
  ASSERT_EQ(sf::letterForCell(sf::Cell::T), 'T');
}

SF_TEST(piece_shape_string) {
  std::string t0(sf::shapeString(sf::PieceId::T, 0).data());
  ASSERT_STR_EQ(t0, ".T..TTT.........");
  std::string i1(sf::shapeString(sf::PieceId::I, 1).data());
  ASSERT_STR_EQ(i1, "..I...I...I...I.");
  std::string o3(sf::shapeString(sf::PieceId::O, 3).data());
  ASSERT_STR_EQ(o3, "OO..OO..........");
}

SF_TEST(piece_shape_of_wraps) {
  ASSERT_TRUE(&sf::shapeOf(sf::PieceId::S, 4) == &sf::shapeOf(sf::PieceId::S, 0));
  ASSERT_TRUE(&sf::shapeOf(sf::PieceId::S, -1) == &sf::shapeOf(sf::PieceId::S, 3));
}
