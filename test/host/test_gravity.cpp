#include "framework.h"
#include "stackfall/core/gravity.h"

SF_TEST(gravity_table_values) {
  ASSERT_EQ(sf::gravityMsForLevel(1), 1000);
  ASSERT_EQ(sf::gravityMsForLevel(2), 793);
  ASSERT_EQ(sf::gravityMsForLevel(5), 355);
  ASSERT_EQ(sf::gravityMsForLevel(10), 64);
  ASSERT_EQ(sf::gravityMsForLevel(15), 7);
  ASSERT_EQ(sf::gravityMsForLevel(16), 7);
  ASSERT_EQ(sf::gravityMsForLevel(99), 7);
  ASSERT_EQ(sf::gravityMsForLevel(0), 1000);
  ASSERT_EQ(sf::gravityMsForLevel(-3), 1000);
}

SF_TEST(gravity_table_strictly_decreasing) {
  for (int i = 0; i < 14; ++i) {
    ASSERT_TRUE(sf::kGravityMsByLevel[i] > sf::kGravityMsByLevel[i + 1]);
  }
}

SF_TEST(gravity_single_cell_boundary) {
  sf::Gravity g;
  g.reset(1);
  ASSERT_EQ(g.step(999, false), 0);
  ASSERT_EQ(g.accumulatorMs(), 999u);
  ASSERT_EQ(g.step(1, false), 1);
  ASSERT_EQ(g.accumulatorMs(), 0u);
}

SF_TEST(gravity_accumulates_two_cells_over_2500ms) {
  sf::Gravity g;
  g.reset(1);
  int total = 0;
  for (int i = 0; i < 2500; ++i) {
    total += g.step(1, false);
  }
  ASSERT_EQ(total, 2);
  ASSERT_EQ(g.accumulatorMs(), 500u);
}

SF_TEST(gravity_level15_fast_period) {
  sf::Gravity g;
  g.reset(15);
  ASSERT_EQ(g.step(100, false), 14);
  ASSERT_EQ(g.accumulatorMs(), 2u);
}

SF_TEST(gravity_clamp_drains_accumulator) {
  sf::Gravity g;
  g.reset(15);
  ASSERT_EQ(g.step(3000, false), 20);
  ASSERT_EQ(g.accumulatorMs(), 0u);
}

SF_TEST(gravity_soft_drop_level1) {
  sf::Gravity g;
  g.reset(1);
  ASSERT_EQ(g.step(20, true), 1);
}

SF_TEST(gravity_soft_drop_takes_faster_period) {
  sf::Gravity g;
  g.reset(15);
  ASSERT_EQ(g.step(20, true), 2);
  ASSERT_EQ(g.accumulatorMs(), 6u);
}

SF_TEST(gravity_set_level_preserves_accumulator) {
  sf::Gravity g;
  g.reset(1);
  ASSERT_EQ(g.step(300, false), 0);
  g.setLevel(5);
  ASSERT_EQ(g.step(55, false), 1);
}

SF_TEST(gravity_reset_zeroes_accumulator) {
  sf::Gravity g;
  g.reset(1);
  ASSERT_EQ(g.step(300, false), 0);
  g.reset(5);
  ASSERT_EQ(g.step(55, false), 0);
}
