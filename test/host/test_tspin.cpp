#include "framework.h"
#include "stackfall/core/board.h"
#include "stackfall/core/piece.h"
#include "stackfall/rules/tspin.h"

using sf::Board;
using sf::PieceId;
using sf::SpinKind;
using sf::SpinQuery;

static SpinQuery q(PieceId p, uint8_t state, int boxCol, int boxRow, int kick, bool rot) {
  SpinQuery s{};
  s.piece = p;
  s.state = state;
  s.boxCol = static_cast<int8_t>(boxCol);
  s.boxRow = static_cast<int8_t>(boxRow);
  s.kickIndex = static_cast<int8_t>(kick);
  s.lastMoveWasRotation = rot;
  return s;
}

static Board boardWith() { return Board{}; }

SF_TEST(tspin_tsd_notch_full) {
  Board b = boardWith();
  b.setRowFromString(30, "....J.J...");
  b.setRowFromString(32, "....J.....");
  ASSERT_TRUE(sf::classifySpin(b, q(PieceId::T, 0, 4, 30, 0, true)) == SpinKind::Full);
}

SF_TEST(tspin_tss_notch_full) {
  Board b = boardWith();
  b.setRowFromString(30, "....J.J...");
  b.setRowFromString(32, "......J...");
  ASSERT_TRUE(sf::classifySpin(b, q(PieceId::T, 0, 4, 30, 1, true)) == SpinKind::Full);
}

SF_TEST(tspin_tst_column_full) {
  Board b = boardWith();
  b.setRowFromString(30, "....J.J...");
  b.setRowFromString(32, "....J.J...");
  ASSERT_TRUE(sf::classifySpin(b, q(PieceId::T, 0, 4, 30, 2, true)) == SpinKind::Full);
}

SF_TEST(tspin_mini_overhang) {
  Board b = boardWith();
  b.setRowFromString(30, "....J.....");
  b.setRowFromString(32, "....J.J...");
  ASSERT_TRUE(sf::classifySpin(b, q(PieceId::T, 0, 4, 30, 0, true)) == SpinKind::Mini);
}

SF_TEST(tspin_mini_promoted_kick4) {
  Board b = boardWith();
  b.setRowFromString(30, "....J.....");
  b.setRowFromString(32, "....J.J...");
  ASSERT_TRUE(sf::classifySpin(b, q(PieceId::T, 0, 4, 30, 4, true)) == SpinKind::Full);
}

SF_TEST(tspin_mini_kick0_stays_mini) {
  Board b = boardWith();
  b.setRowFromString(30, "....J.....");
  b.setRowFromString(32, "....J.J...");
  ASSERT_TRUE(sf::classifySpin(b, q(PieceId::T, 0, 4, 30, 0, true)) == SpinKind::Mini);
  ASSERT_TRUE(sf::classifySpin(b, q(PieceId::T, 0, 4, 30, 3, true)) == SpinKind::Mini);
}

SF_TEST(tspin_two_corners_1100_none) {
  Board b = boardWith();
  b.setRowFromString(30, "....J.J...");
  ASSERT_TRUE(sf::classifySpin(b, q(PieceId::T, 0, 4, 30, 0, true)) == SpinKind::None);
}

SF_TEST(tspin_two_corners_1010_none) {
  Board b = boardWith();
  b.setRowFromString(30, "....J.....");
  b.setRowFromString(32, "....J.....");
  ASSERT_TRUE(sf::classifySpin(b, q(PieceId::T, 0, 4, 30, 0, true)) == SpinKind::None);
}

SF_TEST(tspin_two_corners_0101_none) {
  Board b = boardWith();
  b.setRowFromString(30, "......J...");
  b.setRowFromString(32, "......J...");
  ASSERT_TRUE(sf::classifySpin(b, q(PieceId::T, 0, 4, 30, 0, true)) == SpinKind::None);
}

SF_TEST(tspin_two_corners_0011_none) {
  Board b = boardWith();
  b.setRowFromString(32, "....J.J...");
  ASSERT_TRUE(sf::classifySpin(b, q(PieceId::T, 0, 4, 30, 0, true)) == SpinKind::None);
}

SF_TEST(tspin_left_wall_counts_occupied) {
  Board b = boardWith();
  b.setRowFromString(30, ".J........");
  b.setRowFromString(32, ".J........");
  ASSERT_TRUE(sf::classifySpin(b, q(PieceId::T, 0, -1, 30, 0, true)) == SpinKind::Full);
}

SF_TEST(tspin_open_sky_counts_empty) {
  Board b = boardWith();
  b.setRowFromString(0, "....J.J...");
  ASSERT_TRUE(sf::classifySpin(b, q(PieceId::T, 0, 4, -2, 0, true)) == SpinKind::None);
}

SF_TEST(tspin_no_rotation_scores_nothing) {
  Board b = boardWith();
  b.setRowFromString(30, "....J.J...");
  b.setRowFromString(32, "....J.....");
  ASSERT_TRUE(sf::classifySpin(b, q(PieceId::T, 0, 4, 30, 0, false)) == SpinKind::None);
}

SF_TEST(tspin_failed_kick_scores_nothing) {
  Board b = boardWith();
  b.setRowFromString(30, "....J.J...");
  b.setRowFromString(32, "....J.....");
  ASSERT_TRUE(sf::classifySpin(b, q(PieceId::T, 0, 4, 30, -1, true)) == SpinKind::None);
}

SF_TEST(tspin_bad_kick_index_scores_nothing) {
  Board b = boardWith();
  b.setRowFromString(30, "....J.J...");
  b.setRowFromString(32, "....J.J...");
  ASSERT_TRUE(sf::classifySpin(b, q(PieceId::T, 0, 4, 30, 5, true)) == SpinKind::None);
}

SF_TEST(tspin_s_piece_scores_nothing) {
  Board b = boardWith();
  b.setRowFromString(30, "....J.J...");
  b.setRowFromString(32, "....J.J...");
  ASSERT_TRUE(sf::classifySpin(b, q(PieceId::S, 0, 4, 30, 0, true)) == SpinKind::None);
}

SF_TEST(tspin_state1_right_front_full) {
  // State 1 (nub right): front = TR,BR. Fill TR,BR,BL -> F1 F2 B1 B2 = 1 1 0 1.
  Board b = boardWith();
  b.setRowFromString(30, "......J...");
  b.setRowFromString(32, "....J.J...");
  ASSERT_TRUE(sf::classifySpin(b, q(PieceId::T, 1, 4, 30, 0, true)) == SpinKind::Full);
}

SF_TEST(tspin_state2_bottom_front_full) {
  // State 2 (nub down): front = BL,BR. Fill TL,BL,BR -> 1 1 0 1 in F1 F2 B1 B2.
  Board b = boardWith();
  b.setRowFromString(30, "....J.....");
  b.setRowFromString(32, "....J.J...");
  ASSERT_TRUE(sf::classifySpin(b, q(PieceId::T, 2, 4, 30, 0, true)) == SpinKind::Full);
}

SF_TEST(tspin_state3_left_front_mini) {
  // State 3 (nub left): front = TL,BL; back = TR,BR. TL + TR + BR = 1 0 1 1.
  Board b = boardWith();
  b.setRowFromString(30, "....J.J...");
  b.setRowFromString(32, "......J...");
  ASSERT_TRUE(sf::classifySpin(b, q(PieceId::T, 3, 4, 30, 0, true)) == SpinKind::Mini);
}
