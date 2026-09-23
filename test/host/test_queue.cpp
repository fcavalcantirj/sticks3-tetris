#include <string>

#include "framework.h"
#include "stackfall/core/queue.h"

namespace {

std::string drain(sf::NextQueue& q, int n) {
  std::string s;
  s.reserve(static_cast<size_t>(n));
  for (int i = 0; i < n; ++i) {
    s.push_back(sf::letterFor(q.pop()));
  }
  return s;
}

std::string takeBag(uint32_t seed, int n) {
  sf::Bag b(seed);
  std::string s;
  s.reserve(static_cast<size_t>(n));
  for (int i = 0; i < n; ++i) {
    s.push_back(sf::letterFor(b.next()));
  }
  return s;
}

}  // namespace

SF_TEST(queue_prefix_after_reset) {
  sf::NextQueue q;
  q.reset(1);
  ASSERT_TRUE(q.size() >= 7);
  std::string s;
  for (int i = 0; i < 7; ++i) {
    s.push_back(sf::letterFor(q.peek(i)));
  }
  ASSERT_STR_EQ(s, "SILOTZJ");
  ASSERT_FALSE(q.misuse());
}

SF_TEST(queue_matches_bag_70) {
  sf::NextQueue q;
  q.reset(1);
  ASSERT_STR_EQ(drain(q, 70), takeBag(1, 70));
  ASSERT_FALSE(q.misuse());
}

SF_TEST(queue_size_bounds_1000) {
  sf::NextQueue q;
  q.reset(1);
  for (int i = 0; i < 1000; ++i) {
    (void)q.pop();
    ASSERT_TRUE(q.size() >= 7);
    ASSERT_TRUE(q.size() <= 14);
  }
  ASSERT_FALSE(q.misuse());
}

SF_TEST(queue_same_seed_same_200) {
  sf::NextQueue a;
  sf::NextQueue b;
  a.reset(9876);
  b.reset(9876);
  ASSERT_STR_EQ(drain(a, 200), drain(b, 200));
}

SF_TEST(queue_peek_out_of_range_sets_misuse) {
  sf::NextQueue q;
  q.reset(1);
  ASSERT_FALSE(q.misuse());
  ASSERT_TRUE(q.peek(7) == sf::PieceId::I);
  ASSERT_TRUE(q.misuse());
}

SF_TEST(queue_peek_negative_sets_misuse) {
  sf::NextQueue q;
  q.reset(1);
  ASSERT_FALSE(q.misuse());
  ASSERT_TRUE(q.peek(-1) == sf::PieceId::I);
  ASSERT_TRUE(q.misuse());
}

SF_TEST(queue_reset_clears_misuse) {
  sf::NextQueue q;
  q.reset(1);
  (void)drain(q, 50);
  (void)q.peek(100);
  ASSERT_TRUE(q.misuse());
  q.reset(1);
  ASSERT_FALSE(q.misuse());
  ASSERT_TRUE(q.peek(0) == sf::PieceId::S);
}

SF_TEST(queue_pop_matches_peek) {
  sf::NextQueue q;
  q.reset(1);
  for (int i = 0; i < 30; ++i) {
    sf::PieceId front = q.peek(0);
    sf::PieceId second = q.peek(1);
    sf::PieceId got = q.pop();
    ASSERT_TRUE(got == front);
    ASSERT_TRUE(q.peek(0) == second);
    ASSERT_TRUE(q.size() >= 7);
  }
  ASSERT_FALSE(q.misuse());
}
