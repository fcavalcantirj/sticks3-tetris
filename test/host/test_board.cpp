#include <string>

#include "framework.h"
#include "stackfall/core/board.h"

SF_TEST(board_default_empty) {
  sf::Board b;
  ASSERT_TRUE(b.isEmpty());
  for (int r = 0; r < sf::Board::kTotalRows; ++r) {
    for (int c = 0; c < sf::Board::kWidth; ++c) {
      ASSERT_TRUE(b.at(c, r) == sf::Cell::Empty);
    }
  }
  std::string r0(b.rowString(0).data());
  ASSERT_STR_EQ(r0, "..........");
}

SF_TEST(board_set_single_cell) {
  sf::Board b;
  b.set(0, 39, sf::Cell::T);
  std::string r39(b.rowString(39).data());
  ASSERT_STR_EQ(r39, "T.........");
  ASSERT_EQ(b.filledCount(39), 1);
  ASSERT_FALSE(b.rowFull(39));
}

SF_TEST(board_full_row) {
  sf::Board b;
  b.setRowFromString(39, "IIIIIIIIII");
  ASSERT_TRUE(b.rowFull(39));
  ASSERT_EQ(b.filledCount(39), 10);
  ASSERT_FALSE(b.isEmpty());
}

SF_TEST(board_mixed_row) {
  sf::Board b;
  b.setRowFromString(38, "ZZZ.OOO..T");
  std::string r38(b.rowString(38).data());
  ASSERT_STR_EQ(r38, "ZZZ.OOO..T");
  ASSERT_EQ(b.filledCount(38), 7);
}

SF_TEST(board_row_string_oob) {
  sf::Board b;
  std::string a(b.rowString(-1).data());
  std::string c(b.rowString(40).data());
  ASSERT_STR_EQ(a, "??????????");
  ASSERT_STR_EQ(c, "??????????");
}

SF_TEST(board_set_oob_noop) {
  sf::Board b;
  sf::Board fresh;
  b.set(-1, 5, sf::Cell::I);
  b.set(10, 5, sf::Cell::I);
  b.set(0, -1, sf::Cell::I);
  b.set(0, 40, sf::Cell::I);
  ASSERT_TRUE(b == fresh);
  ASSERT_TRUE(b.at(-1, 5) == sf::Cell::Empty);
}

SF_TEST(board_blocked_edges) {
  sf::Board b;
  ASSERT_TRUE(b.isBlocked(-1, 20));
  ASSERT_TRUE(b.isBlocked(10, 20));
  ASSERT_TRUE(b.isBlocked(0, 40));
  ASSERT_TRUE(b.isBlocked(0, -1));
  ASSERT_FALSE(b.isBlocked(0, 20));
}

SF_TEST(board_set_row_ignores) {
  sf::Board b;
  b.setRowFromString(20, "abcdefghij");
  ASSERT_EQ(b.filledCount(20), 0);
  ASSERT_TRUE(b.rowEmpty(20));
  std::string r20(b.rowString(20).data());
  ASSERT_STR_EQ(r20, "..........");
  sf::Board before = b;
  b.setRowFromString(20, "III");
  ASSERT_TRUE(b == before);
}

SF_TEST(board_equality) {
  sf::Board a;
  sf::Board b;
  a.set(0, 39, sf::Cell::T);
  a.set(1, 39, sf::Cell::T);
  b.set(1, 39, sf::Cell::T);
  b.set(0, 39, sf::Cell::T);
  ASSERT_TRUE(a == b);
  b.set(2, 39, sf::Cell::Z);
  ASSERT_FALSE(a == b);
  ASSERT_TRUE(a != b);
}
