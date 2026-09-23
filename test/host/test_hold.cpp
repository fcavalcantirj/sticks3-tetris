#include "framework.h"
#include "stackfall/core/hold.h"

SF_TEST(hold_fresh_is_empty_unused) {
  sf::Hold h;
  h.reset();
  ASSERT_TRUE(h.empty());
  ASSERT_FALSE(h.used());
}

SF_TEST(hold_first_swap_fills) {
  sf::Hold h;
  h.reset();
  sf::PieceId outgoing = sf::PieceId::Z;
  ASSERT_TRUE(h.swap(sf::PieceId::T, outgoing) == sf::Hold::Result::Filled);
  ASSERT_TRUE(h.piece() == sf::PieceId::T);
  ASSERT_TRUE(h.used());
  ASSERT_FALSE(h.empty());
  ASSERT_TRUE(outgoing == sf::PieceId::Z);
}

SF_TEST(hold_second_swap_refused) {
  sf::Hold h;
  h.reset();
  sf::PieceId outgoing = sf::PieceId::Z;
  ASSERT_TRUE(h.swap(sf::PieceId::T, outgoing) == sf::Hold::Result::Filled);
  ASSERT_TRUE(h.swap(sf::PieceId::I, outgoing) == sf::Hold::Result::Refused);
  ASSERT_TRUE(h.piece() == sf::PieceId::T);
  ASSERT_TRUE(outgoing == sf::PieceId::Z);
}

SF_TEST(hold_lock_then_swap_hands_back) {
  sf::Hold h;
  h.reset();
  sf::PieceId outgoing = sf::PieceId::Z;
  ASSERT_TRUE(h.swap(sf::PieceId::T, outgoing) == sf::Hold::Result::Filled);
  h.onPieceLocked();
  ASSERT_TRUE(h.swap(sf::PieceId::I, outgoing) == sf::Hold::Result::Swapped);
  ASSERT_TRUE(outgoing == sf::PieceId::T);
  ASSERT_TRUE(h.piece() == sf::PieceId::I);
}

SF_TEST(hold_lock_keeps_slot) {
  sf::Hold h;
  h.reset();
  sf::PieceId outgoing = sf::PieceId::Z;
  (void)h.swap(sf::PieceId::T, outgoing);
  h.onPieceLocked();
  (void)h.swap(sf::PieceId::I, outgoing);
  h.onPieceLocked();
  ASSERT_FALSE(h.empty());
  ASSERT_TRUE(h.piece() == sf::PieceId::I);
}

SF_TEST(hold_three_swaps_hand_back_in_order) {
  sf::Hold h;
  h.reset();
  sf::PieceId outgoing = sf::PieceId::Z;
  ASSERT_TRUE(h.swap(sf::PieceId::T, outgoing) == sf::Hold::Result::Filled);
  h.onPieceLocked();
  ASSERT_TRUE(h.swap(sf::PieceId::I, outgoing) == sf::Hold::Result::Swapped);
  ASSERT_TRUE(outgoing == sf::PieceId::T);
  h.onPieceLocked();
  ASSERT_TRUE(h.swap(sf::PieceId::O, outgoing) == sf::Hold::Result::Swapped);
  ASSERT_TRUE(outgoing == sf::PieceId::I);
  ASSERT_TRUE(h.piece() == sf::PieceId::O);
}

SF_TEST(hold_reset_empties) {
  sf::Hold h;
  h.reset();
  sf::PieceId outgoing = sf::PieceId::Z;
  (void)h.swap(sf::PieceId::T, outgoing);
  h.reset();
  ASSERT_TRUE(h.empty());
  ASSERT_FALSE(h.used());
}

SF_TEST(hold_alternating_never_refused) {
  sf::Hold h;
  h.reset();
  sf::PieceId outgoing = sf::PieceId::Z;
  for (int i = 0; i < 100; ++i) {
    ASSERT_TRUE(h.swap(sf::PieceId::T, outgoing) != sf::Hold::Result::Refused);
    h.onPieceLocked();
  }
}

SF_TEST(hold_repeated_without_lock_refused) {
  sf::Hold h;
  h.reset();
  sf::PieceId outgoing = sf::PieceId::Z;
  ASSERT_TRUE(h.swap(sf::PieceId::T, outgoing) == sf::Hold::Result::Filled);
  int refused = 0;
  for (int i = 0; i < 99; ++i) {
    if (h.swap(sf::PieceId::I, outgoing) == sf::Hold::Result::Refused) {
      ++refused;
    }
  }
  ASSERT_EQ(refused, 99);
}
