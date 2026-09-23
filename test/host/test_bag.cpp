#include <algorithm>
#include <string>

#include "framework.h"
#include "stackfall/core/bag.h"

namespace {

std::string take(sf::Bag& b, int n) {
  std::string s;
  s.reserve(static_cast<size_t>(n));
  for (int i = 0; i < n; ++i) {
    s.push_back(sf::letterFor(b.next()));
  }
  return s;
}

}  // namespace

SF_TEST(bag_rng_pins) {
  sf::XorShift32 g(1);
  ASSERT_EQ(g.next(), 270369u);
  ASSERT_EQ(g.next(), 67634689u);
  ASSERT_EQ(g.next(), 2647435461u);
}

SF_TEST(bag_rng_seed_zero) {
  sf::XorShift32 z(0);
  ASSERT_EQ(z.state(), 0x9E3779B9u);
  sf::XorShift32 a(0);
  sf::XorShift32 b(0x9E3779B9u);
  ASSERT_EQ(a.next(), b.next());
}

SF_TEST(bag_rng_stream_sane) {
  sf::XorShift32 g(1);
  uint32_t prev = g.next();
  ASSERT_TRUE(prev != 0u);
  for (int i = 1; i < 100000; ++i) {
    uint32_t v = g.next();
    ASSERT_TRUE(v != 0u);
    ASSERT_TRUE(v != prev);
    prev = v;
  }
}

SF_TEST(bag_seed1_prefix) {
  sf::Bag b(1);
  ASSERT_STR_EQ(take(b, 21), "SILOTZJLJOTSZITIOJSLZ");
}

SF_TEST(bag_seed0_prefix) {
  sf::Bag b0(0u);
  ASSERT_STR_EQ(take(b0, 21), "TLOJZISTOZSIJLJIZOLST");
  sf::Bag b0b(0x9E3779B9u);
  sf::Bag b0c(0u);
  ASSERT_STR_EQ(take(b0b, 21), take(b0c, 21));
}

SF_TEST(bag_same_seed_same_70) {
  sf::Bag a(12345u);
  sf::Bag b(12345u);
  std::string sa = take(a, 70);
  std::string sb = take(b, 70);
  ASSERT_EQ(sa.size(), 70u);
  ASSERT_STR_EQ(sa, sb);
}

SF_TEST(bag_windows_have_one_of_each) {
  sf::Bag b(1);
  std::string s = take(b, 70);
  ASSERT_EQ(s.size(), 70u);
  for (int w = 0; w < 10; ++w) {
    std::string win = s.substr(static_cast<size_t>(w * 7), 7);
    std::sort(win.begin(), win.end());
    ASSERT_STR_EQ(win, "IJLOSTZ");
  }
}

SF_TEST(bag_boundary_invariants) {
  sf::Bag b(1);
  std::string s = take(b, 70);
  ASSERT_EQ(s[27], 'L');
  ASSERT_EQ(s[28], 'L');
  for (int off = 0; off <= 63; ++off) {
    int counts[256] = {0};
    for (int k = 0; k < 7; ++k) {
      counts[static_cast<unsigned char>(s[static_cast<size_t>(off + k)])]++;
    }
    for (int c = 0; c < 256; ++c) {
      ASSERT_TRUE(counts[c] < 3);
    }
  }
}

SF_TEST(bag_seeds_diverge) {
  sf::Bag a(1);
  sf::Bag b(2);
  ASSERT_STR_EQ(take(a, 70) == take(b, 70) ? "same" : "diff", "diff");
}
