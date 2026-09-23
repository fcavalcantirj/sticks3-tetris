#include "framework.h"
#include "kick_fixture.h"
#include "stackfall/core/kicks.h"

namespace {

const char* kKicksPath = "test/host/fixtures/kicks/jlstz_T.txt";

void checkKickInvariants(const sf::test::KickCase& c, const char* file,
                         int line) {
  sf::Rot from = static_cast<sf::Rot>(c.fromState);
  sf::Rot to = sf::rotate(from, c.turn);
  int idx = sf::transitionIndex(from, to);
  if (idx < 0 || idx >= sf::kTransitionCount) {
    sf::test::kickFail(file, line, "case '" + c.name + "' has no transition");
  }
  sf::ActivePiece piece{c.piece, static_cast<uint8_t>(c.fromState),
                        static_cast<int8_t>(c.originCol),
                        static_cast<int8_t>(c.originRow)};
  int kick = -99;
  bool ok = sf::tryRotate(c.board, piece, c.turn, kick);
  if (ok != c.expectOk) {
    sf::test::kickFail(file, line, "case '" + c.name + "' ok mismatch");
  }
  if (!ok) {
    if (kick != -1) {
      sf::test::kickFail(file, line, "case '" + c.name + "' fail kick != -1");
    }
    if (piece.id != c.piece || piece.state != c.fromState ||
        piece.col != c.originCol || piece.row != c.originRow) {
      sf::test::kickFail(file, line,
                         "case '" + c.name + "' failed turn moved the piece");
    }
    return;
  }
  int toState = static_cast<int>(to);
  // Accepted position is free.
  if (c.board.collides(c.piece, toState, piece.col, piece.row)) {
    sf::test::kickFail(file, line,
                       "case '" + c.name + "' accepted position collides");
  }
  // Applied offset is exactly the table entry.
  sf::Kick applied{static_cast<int8_t>(piece.col - c.originCol),
                   static_cast<int8_t>(piece.row - c.originRow)};
  if (applied != sf::kKicksJLSTZ[idx][kick]) {
    sf::test::kickFail(file, line,
                       "case '" + c.name + "' offset is not the table entry");
  }
  if (piece.state != toState) {
    sf::test::kickFail(file, line, "case '" + c.name + "' wrong end state");
  }
  // Every earlier candidate genuinely collides.
  for (int j = 0; j < kick; ++j) {
    int cc = c.originCol + sf::kKicksJLSTZ[idx][j].dCol;
    int rr = c.originRow + sf::kKicksJLSTZ[idx][j].dRow;
    if (!c.board.collides(c.piece, toState, cc, rr)) {
      sf::test::kickFail(file, line, "case '" + c.name + "' candidate " +
                                       std::to_string(j) + " was actually free");
    }
  }
}

}  // namespace

SF_TEST(kicks_table_shape) {
  for (int i = 0; i < 8; ++i) {
    for (int k = 0; k < 5; ++k) {
      ASSERT_TRUE(sf::kKicksJLSTZ[i][k].dCol >= -1 &&
                  sf::kKicksJLSTZ[i][k].dCol <= 1);
      ASSERT_TRUE(sf::kKicksJLSTZ[i][k].dRow >= -2 &&
                  sf::kKicksJLSTZ[i][k].dRow <= 2);
    }
    ASSERT_TRUE(sf::kKicksJLSTZ[i][0] == (sf::Kick{0, 0}));
  }
  // Symmetry of the published table: 0>>R rows equal 2>>R, R>>0 equals R>>2.
  for (int k = 0; k < 5; ++k) {
    ASSERT_TRUE(sf::kKicksJLSTZ[0][k] == sf::kKicksJLSTZ[3][k]);
    ASSERT_TRUE(sf::kKicksJLSTZ[1][k] == sf::kKicksJLSTZ[2][k]);
  }
  for (int i = 0; i < 8; ++i) {
    for (int k = 0; k < 5; ++k) {
      ASSERT_TRUE(sf::kKicksNone[i][k] == (sf::Kick{0, 0}));
    }
  }
}

SF_TEST(kicks_fixture_count) {
  auto cases =
      sf::test::loadKickFixtures(kKicksPath, __FILE__, __LINE__);
  ASSERT_EQ(cases.size(), 32u);
}

SF_TEST(kicks_fixture_run) {
  auto cases =
      sf::test::loadKickFixtures(kKicksPath, __FILE__, __LINE__);
  ASSERT_EQ(cases.size(), 32u);
  for (const auto& c : cases) {
    sf::test::runKickCase(c, __FILE__, __LINE__);
    checkKickInvariants(c, __FILE__, __LINE__);
  }
}

SF_TEST(kicks_famous_wall_case_is_nonzero) {
  // T in state R against the left wall rotating to 2 must kick, not sit.
  auto cases =
      sf::test::loadKickFixtures(kKicksPath, __FILE__, __LINE__);
  bool found = false;
  for (const auto& c : cases) {
    if (c.name == "T_R2_left") {
      found = true;
      ASSERT_TRUE(c.expectOk);
      ASSERT_TRUE(c.expectKick != 0);
      ASSERT_EQ(c.expectKick, 1);
      ASSERT_EQ(c.expectCol, 0);
      ASSERT_EQ(c.expectRow, 30);
    }
    if (c.name == "T_0R_stack") {
      // Three-deep well: must reach candidate 4, the downward kick.
      ASSERT_TRUE(c.expectOk);
      ASSERT_EQ(c.expectKick, 4);
      ASSERT_EQ(c.expectCol, 3);
      ASSERT_EQ(c.expectRow, 37);
    }
  }
  ASSERT_TRUE(found);
}

SF_TEST(kicks_rotate_fails_unchanged) {
  // Garbage seals all five 0>>R footprints of a T at (4,30); nothing fits.
  sf::Board b;
  b.set(4, 29, sf::Cell::Z);
  b.set(4, 30, sf::Cell::Z);
  b.set(5, 32, sf::Cell::Z);
  b.set(4, 32, sf::Cell::Z);
  b.set(5, 33, sf::Cell::Z);
  b.set(4, 34, sf::Cell::Z);
  sf::ActivePiece piece{sf::PieceId::T, 0, 4, 30};
  sf::ActivePiece before = piece;
  int kick = -99;
  ASSERT_FALSE(sf::tryRotate(b, piece, sf::Turn::CW, kick));
  ASSERT_EQ(kick, -1);
  ASSERT_TRUE(piece.id == before.id);
  ASSERT_EQ(piece.state, before.state);
  ASSERT_EQ(piece.col, before.col);
  ASSERT_EQ(piece.row, before.row);
}

SF_TEST(kicks_i_piece_uses_own_table) {
  // The I table is wired in: a flat I on an empty board turns freely at
  // candidate 0, and its candidates are the I table, not JLSTZ.
  sf::Board b;
  sf::ActivePiece piece{sf::PieceId::I, 0, 3, 20};
  int kick = -99;
  ASSERT_TRUE(sf::tryRotate(b, piece, sf::Turn::CW, kick));
  ASSERT_EQ(kick, 0);
  ASSERT_EQ(piece.state, 1);
  ASSERT_EQ(piece.col, 3);
  ASSERT_EQ(piece.row, 20);
  ASSERT_TRUE(sf::kickCandidates(sf::PieceId::I, 0) != nullptr);
  ASSERT_TRUE(sf::kickCandidates(sf::PieceId::I, 0)[1] == (sf::Kick{-2, 0}));
  ASSERT_TRUE(sf::kickCandidates(sf::PieceId::I, 7) != nullptr);
}

SF_TEST(kicks_o_piece_noop_at_candidate_zero) {
  sf::Board b;
  sf::ActivePiece piece{sf::PieceId::O, 0, 4, 20};
  int kick = -99;
  ASSERT_TRUE(sf::tryRotate(b, piece, sf::Turn::CW, kick));
  ASSERT_EQ(kick, 0);
  ASSERT_EQ(piece.state, 1);
  ASSERT_EQ(piece.col, 4);
  ASSERT_EQ(piece.row, 20);
  ASSERT_TRUE(sf::kickCandidates(sf::PieceId::O, 3) != nullptr);
  ASSERT_TRUE(sf::kickCandidates(sf::PieceId::O, 3)[2] == (sf::Kick{0, 0}));
}

SF_TEST(kicks_bad_transition_is_null) {
  ASSERT_TRUE(sf::kickCandidates(sf::PieceId::T, -1) == nullptr);
  ASSERT_TRUE(sf::kickCandidates(sf::PieceId::T, 8) == nullptr);
  ASSERT_TRUE(sf::kickCandidates(sf::PieceId::T, 0) != nullptr);
}
