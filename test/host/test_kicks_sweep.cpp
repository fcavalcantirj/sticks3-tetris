#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "framework.h"
#include "kick_fixture.h"
#include "stackfall/core/kicks.h"

namespace {

const char* kKicksDir = "test/host/fixtures/kicks";

void checkSweepInvariants(const sf::test::KickCase& c, const char* file,
                          int line) {
  sf::Rot from = static_cast<sf::Rot>(c.fromState);
  sf::Rot to = sf::rotate(from, c.turn);
  int idx = sf::transitionIndex(from, to);
  if (idx < 0 || idx >= sf::kTransitionCount) {
    sf::test::kickFail(file, line, "case '" + c.name + "' has no transition");
  }
  const sf::Kick* table = sf::kickCandidates(c.piece, idx);
  if (table == nullptr) {
    sf::test::kickFail(file, line, "case '" + c.name + "' has no kick table");
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
    // A rejected rotation leaves all four fields unchanged.
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
  // The accepted position does not collide.
  if (c.board.collides(c.piece, toState, piece.col, piece.row)) {
    sf::test::kickFail(file, line,
                       "case '" + c.name + "' accepted position collides");
  }
  // The applied offset is exactly the table entry.
  sf::Kick applied{static_cast<int8_t>(piece.col - c.originCol),
                   static_cast<int8_t>(piece.row - c.originRow)};
  if (applied != table[kick]) {
    sf::test::kickFail(file, line,
                       "case '" + c.name + "' offset is not the table entry");
  }
  if (piece.state != toState) {
    sf::test::kickFail(file, line, "case '" + c.name + "' wrong end state");
  }
  // Every candidate before the accepted one genuinely collides.
  for (int j = 0; j < kick; ++j) {
    int cc = c.originCol + table[j].dCol;
    int rr = c.originRow + table[j].dRow;
    if (!c.board.collides(c.piece, toState, cc, rr)) {
      sf::test::kickFail(file, line, "case '" + c.name + "' candidate " +
                                       std::to_string(j) + " was actually free");
    }
  }
  // No accepted cell leaves the matrix.
  const sf::Shape& s = sf::shapeOf(c.piece, toState);
  for (int i = 0; i < 4; ++i) {
    int cc = piece.col + s[static_cast<size_t>(i)].col;
    int rr = piece.row + s[static_cast<size_t>(i)].row;
    if (cc < 0 || cc >= sf::Board::kWidth || rr < 0 ||
        rr >= sf::Board::kTotalRows) {
      sf::test::kickFail(file, line,
                         "case '" + c.name + "' accepted cell off-matrix");
    }
  }
}

std::vector<std::string> sortedKickFiles(const char* file, int line) {
  namespace fs = std::filesystem;
  std::vector<std::string> paths;
  std::error_code ec;
  fs::directory_iterator it(kKicksDir, ec);
  if (ec) {
    sf::test::kickFail(file, line,
                       std::string("cannot list '") + kKicksDir + "'");
  }
  for (const auto& e : it) {
    if (e.path().extension() == ".txt") paths.push_back(e.path().string());
  }
  std::sort(paths.begin(), paths.end());
  return paths;
}

}  // namespace

SF_TEST(kick_sweep_all_pieces) {
  auto paths = sortedKickFiles(__FILE__, __LINE__);
  ASSERT_EQ(paths.size(), 7u);
  int total = 0;
  bool seen[7][8] = {};
  for (const auto& p : paths) {
    auto cases = sf::test::loadKickFixtures(p.c_str(), __FILE__, __LINE__);
    for (const auto& c : cases) {
      sf::test::runKickCase(c, __FILE__, __LINE__);
      checkSweepInvariants(c, __FILE__, __LINE__);
      sf::Rot from = static_cast<sf::Rot>(c.fromState);
      int idx = sf::transitionIndex(from, sf::rotate(from, c.turn));
      ASSERT_TRUE(idx >= 0 && idx < 8);
      seen[static_cast<int>(c.piece)][idx] = true;
      ++total;
    }
  }
  ASSERT_EQ(total, 224);
  int transitions = 0;
  for (int p = 0; p < 7; ++p) {
    for (int t = 0; t < 8; ++t) {
      ASSERT_TRUE(seen[p][t]);
      ++transitions;
    }
  }
  ASSERT_EQ(transitions, 56);
  std::printf("kick sweep 224 cases 56 transitions\n");
}

SF_TEST(kick_rotate_round_trip) {
  // On an empty board no kick is ever needed, so CW-then-CCW (and the
  // reverse) must restore the exact piece with kick index 0 both ways.
  // A non-zero kick here means the shape and kick tables disagree.
  for (int p = 0; p < 7; ++p) {
    sf::PieceId id = static_cast<sf::PieceId>(p);
    for (int s = 0; s < 4; ++s) {
      sf::Board b;
      sf::ActivePiece start{id, static_cast<uint8_t>(s), 4, 25};
      sf::ActivePiece fwd = start;
      int k1 = -99;
      ASSERT_TRUE(sf::tryRotate(b, fwd, sf::Turn::CW, k1));
      ASSERT_EQ(k1, 0);
      int k2 = -99;
      ASSERT_TRUE(sf::tryRotate(b, fwd, sf::Turn::CCW, k2));
      ASSERT_EQ(k2, 0);
      ASSERT_TRUE(fwd.id == start.id);
      ASSERT_EQ(fwd.state, start.state);
      ASSERT_EQ(fwd.col, start.col);
      ASSERT_EQ(fwd.row, start.row);
      sf::ActivePiece back = start;
      int k3 = -99;
      ASSERT_TRUE(sf::tryRotate(b, back, sf::Turn::CCW, k3));
      ASSERT_EQ(k3, 0);
      int k4 = -99;
      ASSERT_TRUE(sf::tryRotate(b, back, sf::Turn::CW, k4));
      ASSERT_EQ(k4, 0);
      ASSERT_TRUE(back.id == start.id);
      ASSERT_EQ(back.state, start.state);
      ASSERT_EQ(back.col, start.col);
      ASSERT_EQ(back.row, start.row);
    }
  }
}
