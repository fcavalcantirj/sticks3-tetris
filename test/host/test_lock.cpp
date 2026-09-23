#include "framework.h"
#include "stackfall/core/lock.h"

SF_TEST(lock_boundary_exact_500ms) {
  sf::LockDelay l;
  l.onGrounded(1000);
  ASSERT_FALSE(l.expired(1499));
  ASSERT_TRUE(l.expired(1500));
  ASSERT_TRUE(l.expired(1501));
}

SF_TEST(lock_reground_does_not_restart_timer) {
  sf::LockDelay l;
  l.onGrounded(1000);
  l.onGrounded(1200);
  ASSERT_TRUE(l.expired(1500));
}

SF_TEST(lock_single_move_reset_moves_deadline) {
  sf::LockDelay l;
  l.onGrounded(1000);
  ASSERT_TRUE(l.onMoveReset(1400));
  ASSERT_EQ(l.resets(), 1);
  ASSERT_FALSE(l.expired(1899));
  ASSERT_TRUE(l.expired(1900));
}

SF_TEST(lock_reset_cap_fifteen_then_refuse) {
  sf::LockDelay l;
  l.onGrounded(1000);
  for (int i = 1; i <= 15; ++i) {
    ASSERT_TRUE(l.onMoveReset(1000u + static_cast<uint32_t>(i)));
  }
  ASSERT_EQ(l.resets(), 15);
  ASSERT_FALSE(l.onMoveReset(1016));
  ASSERT_EQ(l.resets(), 15);
  ASSERT_FALSE(l.expired(1514));
  ASSERT_TRUE(l.expired(1515));
}

SF_TEST(lock_cap_survives_airborne_and_reground) {
  sf::LockDelay l;
  l.onGrounded(1000);
  for (int i = 1; i <= 15; ++i) {
    ASSERT_TRUE(l.onMoveReset(1000u + static_cast<uint32_t>(i)));
  }
  l.onAirborne();
  ASSERT_FALSE(l.expired(1515));
  l.onGrounded(2000);
  ASSERT_EQ(l.resets(), 15);
  ASSERT_FALSE(l.expired(2499));
  ASSERT_TRUE(l.expired(2500));
  ASSERT_FALSE(l.onMoveReset(2100));
  ASSERT_EQ(l.resets(), 15);
}

SF_TEST(lock_wrap_around_unsigned_deadline) {
  sf::LockDelay l;
  l.reset();
  l.onGrounded(0xFFFFFF00u);
  ASSERT_FALSE(l.expired(0xFFFFFF00u + 499u));
  ASSERT_TRUE(l.expired(0x000000F4u));
}

SF_TEST(lock_expired_false_when_not_grounded) {
  sf::LockDelay l;
  ASSERT_FALSE(l.expired(0));
  ASSERT_FALSE(l.expired(100000));
  l.onGrounded(500);
  l.onAirborne();
  ASSERT_FALSE(l.expired(500));
  ASSERT_FALSE(l.expired(500 + 500));
  ASSERT_FALSE(l.expired(0xFFFFFFFFu));
}

SF_TEST(lock_on_locked_clears_all) {
  sf::LockDelay l;
  l.onGrounded(1000);
  ASSERT_TRUE(l.onMoveReset(1100));
  l.onLocked();
  ASSERT_EQ(l.resets(), 0);
  ASSERT_FALSE(l.grounded());
  ASSERT_FALSE(l.expired(0xFFFFFFFFu));
}

SF_TEST(lock_move_reset_while_airborne_refused) {
  sf::LockDelay l;
  ASSERT_FALSE(l.onMoveReset(1000));
  ASSERT_EQ(l.resets(), 0);
  l.onGrounded(2000);
  l.onAirborne();
  ASSERT_FALSE(l.onMoveReset(2500));
  ASSERT_EQ(l.resets(), 0);
}
