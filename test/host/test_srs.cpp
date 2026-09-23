#include "framework.h"
#include "stackfall/core/srs.h"

SF_TEST(srs_rotate_cw_cycle) {
  ASSERT_TRUE(sf::rotate(sf::Rot::Spawn, sf::Turn::CW) == sf::Rot::R);
  ASSERT_TRUE(sf::rotate(sf::Rot::R, sf::Turn::CW) == sf::Rot::Two);
  ASSERT_TRUE(sf::rotate(sf::Rot::Two, sf::Turn::CW) == sf::Rot::L);
  ASSERT_TRUE(sf::rotate(sf::Rot::L, sf::Turn::CW) == sf::Rot::Spawn);
}

SF_TEST(srs_rotate_ccw_from_spawn) {
  ASSERT_TRUE(sf::rotate(sf::Rot::Spawn, sf::Turn::CCW) == sf::Rot::L);
}

SF_TEST(srs_four_cw_returns_to_start) {
  const sf::Rot states[4] = {sf::Rot::Spawn, sf::Rot::R, sf::Rot::Two, sf::Rot::L};
  for (int i = 0; i < 4; ++i) {
    sf::Rot r = states[i];
    r = sf::rotate(r, sf::Turn::CW);
    r = sf::rotate(r, sf::Turn::CW);
    r = sf::rotate(r, sf::Turn::CW);
    r = sf::rotate(r, sf::Turn::CW);
    ASSERT_TRUE(r == states[i]);
  }
}

SF_TEST(srs_three_cw_equals_one_ccw) {
  const sf::Rot states[4] = {sf::Rot::Spawn, sf::Rot::R, sf::Rot::Two, sf::Rot::L};
  for (int i = 0; i < 4; ++i) {
    sf::Rot cw3 = states[i];
    cw3 = sf::rotate(cw3, sf::Turn::CW);
    cw3 = sf::rotate(cw3, sf::Turn::CW);
    cw3 = sf::rotate(cw3, sf::Turn::CW);
    ASSERT_TRUE(cw3 == sf::rotate(states[i], sf::Turn::CCW));
  }
}

SF_TEST(srs_transition_index_sweep) {
  const sf::Rot states[4] = {sf::Rot::Spawn, sf::Rot::R, sf::Rot::Two, sf::Rot::L};
  int legal = 0;
  bool seen[8] = {false, false, false, false, false, false, false, false};
  for (int fi = 0; fi < 4; ++fi) {
    for (int ti = 0; ti < 4; ++ti) {
      int idx = sf::transitionIndex(states[fi], states[ti]);
      if (idx >= 0) {
        ASSERT_TRUE(idx >= 0 && idx < 8);
        ASSERT_FALSE(seen[idx]);
        seen[idx] = true;
        ++legal;
      } else {
        ASSERT_EQ(idx, -1);
      }
    }
  }
  ASSERT_EQ(legal, 8);
  for (int i = 0; i < 8; ++i) ASSERT_TRUE(seen[i]);
  ASSERT_EQ(sf::transitionIndex(sf::Rot::Spawn, sf::Rot::R), 0);
  ASSERT_EQ(sf::transitionIndex(sf::Rot::R, sf::Rot::Spawn), 1);
  ASSERT_EQ(sf::transitionIndex(sf::Rot::L, sf::Rot::Spawn), 6);
  ASSERT_EQ(sf::transitionIndex(sf::Rot::Spawn, sf::Rot::L), 7);
  ASSERT_EQ(sf::transitionIndex(sf::Rot::Spawn, sf::Rot::Spawn), -1);
  ASSERT_EQ(sf::transitionIndex(sf::Rot::R, sf::Rot::L), -1);
}

SF_TEST(srs_transition_names) {
  ASSERT_STR_EQ(sf::transitionName(0), "0>>R");
  ASSERT_STR_EQ(sf::transitionName(7), "0>>L");
  ASSERT_STR_EQ(sf::transitionName(8), "?");
  ASSERT_STR_EQ(sf::transitionName(-1), "?");
}

SF_TEST(srs_rot_char) { ASSERT_EQ(sf::rotChar(sf::Rot::Two), '2'); }
