#include "framework.h"
#include "stackfall/core/kicks.h"

SF_TEST(kicks_i_table_shape) {
  for (int i = 0; i < 8; ++i) {
    for (int k = 0; k < 5; ++k) {
      ASSERT_TRUE(sf::kKicksI[i][k].dCol >= -2 &&
                  sf::kKicksI[i][k].dCol <= 2);
      ASSERT_TRUE(sf::kKicksI[i][k].dRow >= -2 &&
                  sf::kKicksI[i][k].dRow <= 2);
    }
    ASSERT_TRUE(sf::kKicksI[i][0] == (sf::Kick{0, 0}));
  }
  // The I table's own symmetry: 0>>R rows equal L>>2, R>>0 equals 2>>L,
  // R>>2 equals 0>>L, 2>>R equals L>>0.
  for (int k = 0; k < 5; ++k) {
    ASSERT_TRUE(sf::kKicksI[0][k] == sf::kKicksI[5][k]);
    ASSERT_TRUE(sf::kKicksI[1][k] == sf::kKicksI[4][k]);
    ASSERT_TRUE(sf::kKicksI[2][k] == sf::kKicksI[7][k]);
    ASSERT_TRUE(sf::kKicksI[3][k] == sf::kKicksI[6][k]);
  }
}

SF_TEST(kicks_i_table_is_not_jlstz) {
  // A copy-paste of the JLSTZ table for I is the classic defect; count
  // differing entries so it cannot pass.
  int diff = 0;
  for (int i = 0; i < 8; ++i) {
    for (int k = 0; k < 5; ++k) {
      if (sf::kKicksI[i][k] != sf::kKicksJLSTZ[i][k]) ++diff;
    }
  }
  ASSERT_TRUE(diff >= 24);
}

SF_TEST(kicks_i_candidates_wired) {
  for (int i = 0; i < 8; ++i) {
    const sf::Kick* got = sf::kickCandidates(sf::PieceId::I, i);
    ASSERT_TRUE(got != nullptr);
    // Content check, not pointer identity: the table has internal
    // linkage, so each translation unit holds its own copy.
    for (int k = 0; k < 5; ++k) {
      ASSERT_TRUE(got[k] == sf::kKicksI[i][k]);
    }
  }
  ASSERT_TRUE(sf::kickCandidates(sf::PieceId::I, -1) == nullptr);
  ASSERT_TRUE(sf::kickCandidates(sf::PieceId::I, 8) == nullptr);
}
