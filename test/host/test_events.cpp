#include "framework.h"
#include "stackfall/engine/events.h"

using sf::EventQueue;
using sf::EventType;
using sf::GameEvent;

SF_TEST(events_sixteen_pushes_fill_queue) {
  EventQueue q;
  for (int i = 0; i < 16; ++i) {
    q.push(GameEvent{EventType::Move, static_cast<uint8_t>(i), 0, 0, 0});
  }
  ASSERT_EQ(q.size(), 16u);
  ASSERT_EQ(q.dropped(), 0u);
}

SF_TEST(events_seventeenth_push_drops_oldest_not_newest) {
  EventQueue q;
  for (int i = 0; i < 17; ++i) {
    q.push(GameEvent{EventType::Move, static_cast<uint8_t>(i), 0, 0, 0});
  }
  ASSERT_EQ(q.size(), 16u);
  ASSERT_EQ(q.dropped(), 1u);

  GameEvent out;
  ASSERT_TRUE(q.pop(out));
  // The first event (a == 0) was overwritten; the first pop returns a == 1.
  ASSERT_EQ(out.a, 1u);
}

SF_TEST(events_forty_pushes_report_twenty_four_dropped) {
  EventQueue q;
  for (int i = 0; i < 40; ++i) {
    q.push(GameEvent{EventType::Move, static_cast<uint8_t>(i), 0, 0, 0});
  }
  ASSERT_EQ(q.size(), 16u);
  ASSERT_EQ(q.dropped(), 24u);
}

SF_TEST(events_reset_dropped_zeroes_counter_without_touching_entries) {
  EventQueue q;
  q.push(GameEvent{EventType::Move, 7, 0, 0, 0});
  q.push(GameEvent{EventType::Move, 8, 0, 0, 0});
  for (int i = 0; i < 16; ++i) {
    q.push(GameEvent{EventType::Rotate, 0, 0, 0, 0});
  }
  ASSERT_EQ(q.dropped(), 2u);

  q.resetDropped();
  ASSERT_EQ(q.dropped(), 0u);
  ASSERT_EQ(q.size(), 16u);

  GameEvent out;
  ASSERT_TRUE(q.pop(out));
  ASSERT_EQ(out.type, EventType::Rotate);
}
