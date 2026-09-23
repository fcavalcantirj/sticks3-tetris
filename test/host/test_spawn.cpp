#include <string>

#include "framework.h"
#include "stackfall/core/spawn.h"

namespace {
bool samePiece(const sf::ActivePiece& a, const sf::ActivePiece& b) {
  return a.id == b.id && a.state == b.state && a.col == b.col && a.row == b.row;
}

void checkCells(const sf::Board& b, sf::PieceId p, int col, int row,
                const char* r20, const char* r21) {
  sf::Board c;
  int n = c.place(p, 0, col, row);
  (void)n;
  // Overlay onto b comparison via strings is done by caller; here just check rows.
  std::string s20(c.rowString(20).data());
  std::string s21(c.rowString(21).data());
  ASSERT_STR_EQ(s20, r20);
  ASSERT_STR_EQ(s21, r21);
  (void)b;
}
}  // namespace

SF_TEST(spawn_cells_exact) {
  sf::ActivePiece si = sf::spawnOf(sf::PieceId::I);
  ASSERT_TRUE(si.id == sf::PieceId::I);
  ASSERT_EQ(si.state, 0);
  ASSERT_EQ(si.col, 3);
  ASSERT_EQ(si.row, 20);
  sf::ActivePiece so = sf::spawnOf(sf::PieceId::O);
  ASSERT_TRUE(so.id == sf::PieceId::O);
  ASSERT_EQ(so.state, 0);
  ASSERT_EQ(so.col, 4);
  ASSERT_EQ(so.row, 20);
  sf::ActivePiece st = sf::spawnOf(sf::PieceId::T);
  ASSERT_TRUE(st.id == sf::PieceId::T);
  ASSERT_EQ(st.state, 0);
  ASSERT_EQ(st.col, 3);
  ASSERT_EQ(st.row, 20);

  // Occupancy via placement on an empty board.
  {
    sf::Board b;
    ASSERT_EQ(b.place(sf::PieceId::T, 0, 3, 20), 4);
    ASSERT_STR_EQ(std::string(b.rowString(20).data()), "....T.....");
    ASSERT_STR_EQ(std::string(b.rowString(21).data()), "...TTT....");
  }
  {
    sf::Board b;
    ASSERT_EQ(b.place(sf::PieceId::O, 0, 4, 20), 4);
    ASSERT_STR_EQ(std::string(b.rowString(20).data()), "....OO....");
    ASSERT_STR_EQ(std::string(b.rowString(21).data()), "....OO....");
  }
  {
    sf::Board b;
    ASSERT_EQ(b.place(sf::PieceId::I, 0, 3, 20), 4);
    ASSERT_STR_EQ(std::string(b.rowString(20).data()), "..........");
    ASSERT_STR_EQ(std::string(b.rowString(21).data()), "...IIII...");
  }
  // All spawn origins enumerated.
  ASSERT_EQ(static_cast<int>(sf::spawnOf(sf::PieceId::J).col), 3);
  ASSERT_EQ(static_cast<int>(sf::spawnOf(sf::PieceId::L).col), 3);
  ASSERT_EQ(static_cast<int>(sf::spawnOf(sf::PieceId::S).col), 3);
  ASSERT_EQ(static_cast<int>(sf::spawnOf(sf::PieceId::Z).col), 3);
  (void)checkCells;
}

SF_TEST(spawn_block_out) {
  sf::Board b;
  b.setRowFromString(21, "...ZZZ....");
  ASSERT_TRUE(sf::classifySpawn(b, sf::PieceId::T) == sf::TopOut::BlockOut);
  ASSERT_TRUE(sf::classifySpawn(b, sf::PieceId::O) == sf::TopOut::BlockOut);
}

SF_TEST(spawn_clear_allows_all) {
  sf::Board b;
  b.setRowFromString(21, "ZZZ.......");
  const sf::PieceId all[7] = {sf::PieceId::I, sf::PieceId::J, sf::PieceId::L,
                              sf::PieceId::O, sf::PieceId::S, sf::PieceId::T,
                              sf::PieceId::Z};
  for (int i = 0; i < 7; ++i) {
    ASSERT_TRUE(sf::classifySpawn(b, all[i]) == sf::TopOut::None);
    sf::ActivePiece out{sf::PieceId::I, 0, 0, 0};
    ASSERT_TRUE(sf::trySpawn(b, all[i], out));
    sf::ActivePiece want = sf::spawnOf(all[i]);
    ASSERT_TRUE(samePiece(out, want));
  }
}

SF_TEST(spawn_refused_leaves_out_untouched) {
  sf::Board b;
  b.setRowFromString(21, "...ZZZ....");
  sf::ActivePiece out{sf::PieceId::Z, 3, 0, 0};
  ASSERT_FALSE(sf::trySpawn(b, sf::PieceId::T, out));
  sf::ActivePiece want{sf::PieceId::Z, 3, 0, 0};
  ASSERT_TRUE(samePiece(out, want));
}

SF_TEST(spawn_row22_full_still_spawns) {
  sf::Board b;
  b.setRowFromString(22, "IIIIIIIIII");
  const sf::PieceId all[7] = {sf::PieceId::I, sf::PieceId::J, sf::PieceId::L,
                              sf::PieceId::O, sf::PieceId::S, sf::PieceId::T,
                              sf::PieceId::Z};
  for (int i = 0; i < 7; ++i) {
    ASSERT_TRUE(sf::classifySpawn(b, all[i]) == sf::TopOut::None);
  }
}

SF_TEST(spawn_classify_lock_cases) {
  ASSERT_TRUE(sf::classifyLock(sf::ActivePiece{sf::PieceId::T, 0, 3, 17}) ==
              sf::TopOut::LockOut);
  ASSERT_TRUE(sf::classifyLock(sf::ActivePiece{sf::PieceId::T, 0, 3, 19}) ==
              sf::TopOut::None);
  ASSERT_TRUE(sf::classifyLock(sf::ActivePiece{sf::PieceId::I, 0, 3, 18}) ==
              sf::TopOut::LockOut);
  ASSERT_TRUE(sf::classifyLock(sf::ActivePiece{sf::PieceId::I, 0, 3, 19}) ==
              sf::TopOut::None);
  ASSERT_TRUE(sf::TopOut::BlockOut != sf::TopOut::LockOut);
}

SF_TEST(spawn_classify_lock_sweep) {
  const sf::PieceId all[7] = {sf::PieceId::I, sf::PieceId::J, sf::PieceId::L,
                              sf::PieceId::O, sf::PieceId::S, sf::PieceId::T,
                              sf::PieceId::Z};
  for (int i = 0; i < 7; ++i) {
    for (int s = 0; s < 4; ++s) {
      sf::ActivePiece hi{all[i], static_cast<uint8_t>(s), 3, 0};
      sf::TopOut h = sf::classifyLock(hi);
      ASSERT_TRUE(h == sf::TopOut::LockOut);
      ASSERT_FALSE(h == sf::TopOut::BlockOut);
      sf::ActivePiece lo{all[i], static_cast<uint8_t>(s), 3, 30};
      sf::TopOut l = sf::classifyLock(lo);
      ASSERT_TRUE(l == sf::TopOut::None);
      ASSERT_FALSE(l == sf::TopOut::BlockOut);
    }
  }
}
