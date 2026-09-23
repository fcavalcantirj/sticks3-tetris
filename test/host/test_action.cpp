#include "framework.h"
#include "stackfall/input/action.h"

using sf::input::ActionKind;
using sf::input::ActionQueue;
using sf::input::GameAction;

SF_TEST(action_queue_fresh_state) {
  ActionQueue q;
  GameAction out{};

  ASSERT_EQ(q.size(), 0u);
  ASSERT_EQ(q.dropped(), 0u);
  ASSERT_EQ(q.capacity(), 16u);
  ASSERT_FALSE(q.pop(out));
}

SF_TEST(action_queue_preserves_order_timestamp_and_sequence) {
  ActionQueue q;
  ASSERT_TRUE(q.push(ActionKind::MoveLeft, 100));
  ASSERT_TRUE(q.push(ActionKind::HardDrop, 120));

  GameAction out{};
  ASSERT_TRUE(q.pop(out));
  ASSERT_EQ(out.kind, ActionKind::MoveLeft);
  ASSERT_EQ(out.tMs, 100u);
  ASSERT_EQ(out.seq, 0u);

  ASSERT_TRUE(q.pop(out));
  ASSERT_EQ(out.kind, ActionKind::HardDrop);
  ASSERT_EQ(out.tMs, 120u);
  ASSERT_EQ(out.seq, 1u);
  ASSERT_FALSE(q.pop(out));
}

SF_TEST(action_queue_full_drops_oldest_and_keeps_newest) {
  ActionQueue q;
  for (uint32_t i = 0; i < 16; ++i) {
    ASSERT_TRUE(q.push(ActionKind::MoveLeft, 100u + i));
  }
  ASSERT_EQ(q.size(), 16u);
  ASSERT_EQ(q.dropped(), 0u);

  ASSERT_TRUE(q.push(ActionKind::RotateCw, 999));
  ASSERT_EQ(q.size(), 16u);
  ASSERT_EQ(q.dropped(), 1u);

  GameAction out{};
  ASSERT_TRUE(q.pop(out));
  ASSERT_EQ(out.kind, ActionKind::MoveLeft);
  ASSERT_EQ(out.tMs, 101u);
  ASSERT_EQ(out.seq, 1u);

  for (uint8_t i = 0; i < 14; ++i) {
    ASSERT_TRUE(q.pop(out));
  }
  ASSERT_TRUE(q.pop(out));
  ASSERT_EQ(out.kind, ActionKind::RotateCw);
  ASSERT_EQ(out.tMs, 999u);
  ASSERT_EQ(out.seq, 16u);
  ASSERT_FALSE(q.pop(out));

  for (uint32_t i = 0; i < 32; ++i) {
    ASSERT_TRUE(q.push(ActionKind::MoveRight, 2000u + i));
  }
  ASSERT_EQ(q.size(), 16u);
  ASSERT_EQ(q.dropped(), 17u);
}

SF_TEST(action_queue_clear_keeps_dropped_and_none_touches_nothing) {
  ActionQueue q;
  for (uint32_t i = 0; i < 17; ++i) {
    ASSERT_TRUE(q.push(ActionKind::MenuDown, i));
  }
  ASSERT_EQ(q.dropped(), 1u);

  q.clear();
  ASSERT_EQ(q.size(), 0u);
  ASSERT_EQ(q.dropped(), 1u);

  ASSERT_FALSE(q.push(ActionKind::None, 5));
  ASSERT_EQ(q.size(), 0u);
  ASSERT_EQ(q.dropped(), 1u);

  ASSERT_TRUE(q.push(ActionKind::Confirm, 6));
  GameAction out{};
  ASSERT_TRUE(q.pop(out));
  ASSERT_EQ(out.kind, ActionKind::Confirm);
  ASSERT_EQ(out.tMs, 6u);
  ASSERT_EQ(out.seq, 17u);
}

SF_TEST(action_queue_sequence_wraps_from_255_to_zero) {
  ActionQueue q;
  GameAction out{};

  for (uint32_t i = 0; i <= 256; ++i) {
    ASSERT_TRUE(q.push(ActionKind::Diag, i));
    ASSERT_TRUE(q.pop(out));
    ASSERT_EQ(out.tMs, i);
    if (i == 255) {
      ASSERT_EQ(out.seq, 255u);
    }
    if (i == 256) {
      ASSERT_EQ(out.seq, 0u);
    }
  }
}
