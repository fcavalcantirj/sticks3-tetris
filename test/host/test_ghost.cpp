#include "framework.h"
#include "stackfall/core/ghost.h"

static sf::ActivePiece makePiece(sf::PieceId id, int state, int col, int row) {
  sf::ActivePiece p;
  p.id = id;
  p.state = static_cast<uint8_t>(state);
  p.col = static_cast<int8_t>(col);
  p.row = static_cast<int8_t>(row);
  return p;
}

SF_TEST(ghost_empty_t_spawn) {
  sf::Board b;
  sf::ActivePiece p = makePiece(sf::PieceId::T, 0, 3, 20);
  ASSERT_EQ(sf::ghostRow(b, p), 38);
  ASSERT_EQ(sf::dropDistance(b, p), 18);
}

SF_TEST(ghost_empty_i_horizontal) {
  sf::Board b;
  ASSERT_EQ(sf::ghostRow(b, makePiece(sf::PieceId::I, 0, 3, 20)), 38);
}

SF_TEST(ghost_empty_i_vertical) {
  sf::Board b;
  ASSERT_EQ(sf::ghostRow(b, makePiece(sf::PieceId::I, 1, 3, 20)), 36);
}

SF_TEST(ghost_empty_o_spawn) {
  sf::Board b;
  ASSERT_EQ(sf::ghostRow(b, makePiece(sf::PieceId::O, 0, 4, 20)), 38);
}

SF_TEST(ghost_resting_has_zero_drop) {
  sf::Board b;
  sf::ActivePiece p = makePiece(sf::PieceId::T, 0, 3, 38);
  ASSERT_EQ(sf::ghostRow(b, p), 38);
  ASSERT_EQ(sf::dropDistance(b, p), 0);
}

SF_TEST(ghost_column_sensitive_stack) {
  sf::Board b;
  b.setRowFromString(30, "....T.....");
  ASSERT_EQ(sf::ghostRow(b, makePiece(sf::PieceId::T, 0, 3, 20)), 28);
  ASSERT_EQ(sf::ghostRow(b, makePiece(sf::PieceId::T, 0, 6, 20)), 38);
}

SF_TEST(ghost_full_floor_row) {
  sf::Board b;
  b.setRowFromString(39, "IIIIIIIIII");
  ASSERT_EQ(sf::ghostRow(b, makePiece(sf::PieceId::I, 0, 0, 20)), 37);
}

SF_TEST(ghost_is_pure) {
  sf::Board b;
  b.setRowFromString(30, "....T.....");
  sf::ActivePiece p = makePiece(sf::PieceId::T, 0, 3, 20);
  sf::Board before = b;
  int first = sf::ghostRow(b, p);
  int second = sf::ghostRow(b, p);
  ASSERT_EQ(first, second);
  ASSERT_TRUE(b == before);
}

SF_TEST(ghost_colliding_returns_own_row) {
  sf::Board b;
  b.setRowFromString(21, "...TTT....");
  sf::ActivePiece p = makePiece(sf::PieceId::T, 0, 3, 20);
  ASSERT_TRUE(b.collides(p.id, 0, 3, 20));
  ASSERT_EQ(sf::ghostRow(b, p), 20);
  ASSERT_EQ(sf::dropDistance(b, p), 0);
}
