#include "framework.h"
#include "stackfall/rules/scoring.h"

using sf::ClearEvent;
using sf::ClearKind;
using sf::ScoreBreakdown;
using sf::SpinKind;
using sf::isDifficult;
using sf::scoreFor;

static ClearEvent ev(ClearKind c, SpinKind s, uint8_t level, int16_t combo = 0,
                     bool b2b = false, bool pc = false, uint16_t soft = 0,
                     uint16_t hard = 0) {
  ClearEvent e{};
  e.clear = c;
  e.spin = s;
  e.comboCount = combo;
  e.backToBack = b2b;
  e.perfectClear = pc;
  e.softDropCells = soft;
  e.hardDropCells = hard;
  e.level = level;
  return e;
}

SF_TEST(scoring_single_level1) {
  ScoreBreakdown b = scoreFor(ev(ClearKind::Single, SpinKind::None, 1));
  ASSERT_EQ(b.base, 100);
  ASSERT_EQ(b.total, 100);
}

SF_TEST(scoring_triple_level10) {
  ScoreBreakdown b = scoreFor(ev(ClearKind::Triple, SpinKind::None, 10));
  ASSERT_EQ(b.base, 5000);
  ASSERT_EQ(b.total, 5000);
}

SF_TEST(scoring_quad_level5) {
  ScoreBreakdown b = scoreFor(ev(ClearKind::Quad, SpinKind::None, 5));
  ASSERT_EQ(b.base, 4000);
  ASSERT_EQ(b.total, 4000);
}

SF_TEST(scoring_full_double_level3) {
  ScoreBreakdown b = scoreFor(ev(ClearKind::Double, SpinKind::Full, 3));
  ASSERT_EQ(b.base, 3600);
  ASSERT_EQ(b.total, 3600);
}

SF_TEST(scoring_full_none_level4) {
  ScoreBreakdown b = scoreFor(ev(ClearKind::None, SpinKind::Full, 4));
  ASSERT_EQ(b.base, 1600);
  ASSERT_EQ(b.total, 1600);
}

SF_TEST(scoring_mini_none_level1) {
  ScoreBreakdown b = scoreFor(ev(ClearKind::None, SpinKind::Mini, 1));
  ASSERT_EQ(b.base, 100);
  ASSERT_EQ(b.total, 100);
}

SF_TEST(scoring_mini_single_level2) {
  ScoreBreakdown b = scoreFor(ev(ClearKind::Single, SpinKind::Mini, 2));
  ASSERT_EQ(b.base, 400);
  ASSERT_EQ(b.total, 400);
}

SF_TEST(scoring_unreachable_bases_zero) {
  ScoreBreakdown f = scoreFor(ev(ClearKind::Quad, SpinKind::Full, 1));
  ASSERT_EQ(f.base, 0);
  ASSERT_EQ(f.total, 0);
  ScoreBreakdown m = scoreFor(ev(ClearKind::Triple, SpinKind::Mini, 7));
  ASSERT_EQ(m.base, 0);
  ASSERT_EQ(m.total, 0);
}

SF_TEST(scoring_b2b_full_single) {
  ScoreBreakdown b =
      scoreFor(ev(ClearKind::Single, SpinKind::Full, 1, 0, true));
  ASSERT_EQ(b.base, 800);
  ASSERT_EQ(b.b2bBonus, 400);
  ASSERT_EQ(b.total, 1200);
}

SF_TEST(scoring_b2b_quad_perfect) {
  ScoreBreakdown b =
      scoreFor(ev(ClearKind::Quad, SpinKind::None, 1, 0, true, true));
  ASSERT_EQ(b.base, 800);
  ASSERT_EQ(b.b2bBonus, 400);
  ASSERT_EQ(b.perfect, 2000);
  ASSERT_EQ(b.total, 3200);
}

SF_TEST(scoring_combo_pays) {
  ScoreBreakdown b =
      scoreFor(ev(ClearKind::Single, SpinKind::None, 2, 4));
  ASSERT_EQ(b.base, 200);
  ASSERT_EQ(b.combo, 400);
  ASSERT_EQ(b.total, 600);
}

SF_TEST(scoring_combo_zero_pays_nothing) {
  ScoreBreakdown b =
      scoreFor(ev(ClearKind::Single, SpinKind::None, 2, 0));
  ASSERT_EQ(b.combo, 0);
  ASSERT_EQ(b.total, 200);
}

SF_TEST(scoring_drop_ignores_level) {
  ScoreBreakdown b =
      scoreFor(ev(ClearKind::None, SpinKind::None, 9, 0, false, false, 5, 18));
  ASSERT_EQ(b.base, 0);
  ASSERT_EQ(b.drop, 41);
  ASSERT_EQ(b.total, 41);
}

SF_TEST(scoring_b2b_nondifficult_pays_nothing) {
  ScoreBreakdown b =
      scoreFor(ev(ClearKind::Single, SpinKind::None, 3, 0, true));
  ASSERT_EQ(b.base, 300);
  ASSERT_EQ(b.b2bBonus, 0);
  ASSERT_EQ(b.total, 300);
  ASSERT_FALSE(isDifficult(ClearKind::Single, SpinKind::None));
  ASSERT_TRUE(isDifficult(ClearKind::Quad, SpinKind::None));
  ASSERT_TRUE(isDifficult(ClearKind::Single, SpinKind::Full));
}
