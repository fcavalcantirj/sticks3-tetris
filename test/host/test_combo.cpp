#include "framework.h"
#include "stackfall/rules/combo.h"

using sf::ChainResult;
using sf::ChainState;
using sf::ClearKind;
using sf::SpinKind;
using sf::advanceChain;

SF_TEST(combo_three_quads_pay_twice) {
  ChainState s{-1, false};
  ChainResult r0 = advanceChain(s, ClearKind::Quad, SpinKind::None);
  ASSERT_EQ(r0.next.comboCount, 0);
  ASSERT_TRUE(r0.next.b2b);
  ASSERT_FALSE(r0.payB2b);
  ASSERT_EQ(r0.comboForScoring, 0);
  ChainResult r1 = advanceChain(r0.next, ClearKind::Quad, SpinKind::None);
  ASSERT_EQ(r1.next.comboCount, 1);
  ASSERT_TRUE(r1.next.b2b);
  ASSERT_TRUE(r1.payB2b);
  ASSERT_EQ(r1.comboForScoring, 1);
  ChainResult r2 = advanceChain(r1.next, ClearKind::Quad, SpinKind::None);
  ASSERT_EQ(r2.next.comboCount, 2);
  ASSERT_TRUE(r2.next.b2b);
  ASSERT_TRUE(r2.payB2b);
  ASSERT_EQ(r2.comboForScoring, 2);
}

SF_TEST(combo_broken_by_single) {
  ChainState s{-1, false};
  ChainResult r0 = advanceChain(s, ClearKind::Quad, SpinKind::None);
  ASSERT_EQ(r0.next.comboCount, 0);
  ASSERT_TRUE(r0.next.b2b);
  ASSERT_FALSE(r0.payB2b);
  ChainResult r1 = advanceChain(r0.next, ClearKind::Single, SpinKind::None);
  ASSERT_EQ(r1.next.comboCount, 1);
  ASSERT_FALSE(r1.next.b2b);
  ASSERT_FALSE(r1.payB2b);
  ASSERT_EQ(r1.comboForScoring, 1);
  ChainResult r2 = advanceChain(r1.next, ClearKind::Quad, SpinKind::None);
  ASSERT_EQ(r2.next.comboCount, 2);
  ASSERT_TRUE(r2.next.b2b);
  ASSERT_FALSE(r2.payB2b);
  ASSERT_EQ(r2.comboForScoring, 2);
}

SF_TEST(combo_survives_zero_line_lock) {
  ChainState s{-1, false};
  ChainResult r0 = advanceChain(s, ClearKind::Quad, SpinKind::None);
  ASSERT_EQ(r0.next.comboCount, 0);
  ASSERT_TRUE(r0.next.b2b);
  ASSERT_FALSE(r0.payB2b);
  ChainResult r1 = advanceChain(r0.next, ClearKind::None, SpinKind::None);
  ASSERT_EQ(r1.next.comboCount, -1);
  ASSERT_TRUE(r1.next.b2b);
  ASSERT_FALSE(r1.payB2b);
  ChainResult r2 = advanceChain(r1.next, ClearKind::Quad, SpinKind::None);
  ASSERT_EQ(r2.next.comboCount, 0);
  ASSERT_TRUE(r2.next.b2b);
  ASSERT_TRUE(r2.payB2b);
  ASSERT_EQ(r2.comboForScoring, 0);
}

SF_TEST(combo_zero_line_keeps_b2b_for_tspin) {
  ChainState s{-1, false};
  ChainResult r0 = advanceChain(s, ClearKind::Single, SpinKind::Full);
  ASSERT_EQ(r0.next.comboCount, 0);
  ASSERT_TRUE(r0.next.b2b);
  ASSERT_FALSE(r0.payB2b);
  // Zero-line T-spin: combo resets, b2b carried through unchanged.
  ChainResult r1 = advanceChain(r0.next, ClearKind::None, SpinKind::Full);
  ASSERT_EQ(r1.next.comboCount, -1);
  ASSERT_TRUE(r1.next.b2b);
  ASSERT_FALSE(r1.payB2b);
  ASSERT_EQ(r1.comboForScoring, -1);
}

SF_TEST(combo_tspin_single_then_double) {
  ChainState s{-1, false};
  ChainResult r0 = advanceChain(s, ClearKind::Single, SpinKind::Full);
  ASSERT_EQ(r0.next.comboCount, 0);
  ASSERT_TRUE(r0.next.b2b);
  ASSERT_FALSE(r0.payB2b);
  ChainResult r1 = advanceChain(r0.next, ClearKind::Double, SpinKind::Full);
  ASSERT_EQ(r1.next.comboCount, 1);
  ASSERT_TRUE(r1.next.b2b);
  ASSERT_TRUE(r1.payB2b);
  ASSERT_EQ(r1.comboForScoring, 1);
}

SF_TEST(combo_for_scoring_counts_up) {
  ChainState s{-1, false};
  for (int i = 0; i < 5; ++i) {
    ChainResult r = advanceChain(s, ClearKind::Single, SpinKind::None);
    ASSERT_EQ(r.comboForScoring, i);
    ASSERT_EQ(r.next.comboCount, i);
    ASSERT_FALSE(r.next.b2b);
    ASSERT_FALSE(r.payB2b);
    s = r.next;
  }
}

SF_TEST(combo_saturates_at_twenty) {
  ChainState s{-1, false};
  ChainState cur = s;
  for (int i = 0; i < 25; ++i) {
    ChainResult r = advanceChain(cur, ClearKind::Single, SpinKind::None);
    cur = r.next;
  }
  ASSERT_EQ(cur.comboCount, 20);
  // Stays there: one more clearing piece keeps it at 20.
  ChainResult r = advanceChain(cur, ClearKind::Single, SpinKind::None);
  ASSERT_EQ(r.next.comboCount, 20);
  ASSERT_EQ(r.comboForScoring, 20);
}

SF_TEST(combo_fresh_double_no_b2b) {
  ChainState s{-1, false};
  ChainResult r = advanceChain(s, ClearKind::Double, SpinKind::None);
  ASSERT_EQ(r.next.comboCount, 0);
  ASSERT_FALSE(r.next.b2b);
  ASSERT_FALSE(r.payB2b);
  ASSERT_EQ(r.comboForScoring, 0);
}
