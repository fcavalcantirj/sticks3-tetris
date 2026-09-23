#include <string>

#include "framework.h"
#include "stackfall/core/board.h"

SF_TEST(board_ops_collides_empty_and_walls) {
  sf::Board b;
  ASSERT_FALSE(b.collides(sf::PieceId::I, 0, 3, 20));
  ASSERT_TRUE(b.collides(sf::PieceId::I, 0, -1, 20));
  ASSERT_TRUE(b.collides(sf::PieceId::I, 0, 7, 20));
}

SF_TEST(board_ops_collides_floor) {
  sf::Board b;
  ASSERT_FALSE(b.collides(sf::PieceId::I, 0, 3, 38));
  ASSERT_TRUE(b.collides(sf::PieceId::I, 0, 3, 39));
}

SF_TEST(board_ops_collides_stack) {
  sf::Board b;
  b.setRowFromString(30, "....T.....");
  ASSERT_TRUE(b.collides(sf::PieceId::O, 0, 4, 29));
  ASSERT_FALSE(b.collides(sf::PieceId::O, 0, 6, 29));
}

SF_TEST(board_ops_place_t) {
  sf::Board b;
  ASSERT_EQ(b.place(sf::PieceId::T, 0, 3, 20), 4);
  std::string r20(b.rowString(20).data());
  std::string r21(b.rowString(21).data());
  ASSERT_STR_EQ(r20, "....T.....");
  ASSERT_STR_EQ(r21, "...TTT....");
}

SF_TEST(board_ops_place_partial_off_edge) {
  sf::Board b;
  ASSERT_EQ(b.place(sf::PieceId::I, 0, 8, 20), 2);
}

SF_TEST(board_ops_clear_four) {
  sf::Board b;
  b.setRowFromString(36, "IIIIIIIIII");
  b.setRowFromString(37, "JJJJJJJJJJ");
  b.setRowFromString(38, "LLLLLLLLLL");
  b.setRowFromString(39, "OOOOOOOOOO");
  int out[4] = {-1, -1, -1, -1};
  ASSERT_EQ(b.clearLines(out), 4);
  ASSERT_EQ(out[0], 39);
  ASSERT_EQ(out[1], 38);
  ASSERT_EQ(out[2], 37);
  ASSERT_EQ(out[3], 36);
  ASSERT_TRUE(b.isEmpty());
}

SF_TEST(board_ops_clear_two_with_carry) {
  sf::Board b;
  b.setRowFromString(37, "IIIIIIIIII");
  b.setRowFromString(38, "T.........");
  b.setRowFromString(39, "OOOOOOOOOO");
  int out[4] = {-1, -1, -1, -1};
  ASSERT_EQ(b.clearLines(out), 2);
  ASSERT_EQ(out[0], 39);
  ASSERT_EQ(out[1], 37);
  std::string r39(b.rowString(39).data());
  std::string r38(b.rowString(38).data());
  ASSERT_STR_EQ(r39, "T.........");
  ASSERT_STR_EQ(r38, "..........");
  ASSERT_EQ(b.filledCount(39), 1);
}

SF_TEST(board_ops_clear_shift_single) {
  sf::Board b;
  b.set(0, 20, sf::Cell::T);
  b.setRowFromString(39, "IIIIIIIIII");
  int out[4] = {-1, -1, -1, -1};
  ASSERT_EQ(b.clearLines(out), 1);
  ASSERT_EQ(out[0], 39);
  ASSERT_TRUE(b.at(0, 21) == sf::Cell::T);
  ASSERT_TRUE(b.at(0, 20) == sf::Cell::Empty);
}

SF_TEST(board_ops_clear_empty) {
  sf::Board b;
  sf::Board fresh;
  int out[4] = {-1, -1, -1, -1};
  ASSERT_EQ(b.clearLines(out), 0);
  ASSERT_TRUE(b == fresh);
}

SF_TEST(board_ops_clear_twice) {
  sf::Board b;
  b.setRowFromString(36, "IIIIIIIIII");
  b.setRowFromString(37, "JJJJJJJJJJ");
  b.setRowFromString(38, "LLLLLLLLLL");
  b.setRowFromString(39, "OOOOOOOOOO");
  int out[4] = {-1, -1, -1, -1};
  ASSERT_EQ(b.clearLines(out), 4);
  int out2[4] = {-1, -1, -1, -1};
  ASSERT_EQ(b.clearLines(out2), 0);
}
