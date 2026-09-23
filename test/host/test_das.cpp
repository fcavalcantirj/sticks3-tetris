#include "framework.h"
#include "stackfall/core/das.h"

using sf::AutoShift;
using Dir = sf::AutoShift::Dir;

SF_TEST(das_constants) {
  ASSERT_EQ(sf::kDasMs, 167);
  ASSERT_EQ(sf::kArrMs, 33);
  ASSERT_EQ(sf::kMaxShiftsPerUpdate, 10);
  AutoShift a;
  ASSERT_TRUE(a.dir() == Dir::None);
}

SF_TEST(das_headline_exact_boundaries) {
  AutoShift a;
  uint32_t fired[16];
  int count = 0;
  int ret[301];
  for (uint32_t t = 0; t <= 300; ++t) {
    int n = a.update(Dir::Right, t);
    ret[t] = n;
    for (int i = 0; i < n && count < 16; ++i) {
      fired[count++] = t;
    }
  }
  ASSERT_EQ(count, 6);
  ASSERT_EQ(fired[0], 0u);
  ASSERT_EQ(fired[1], 167u);
  ASSERT_EQ(fired[2], 200u);
  ASSERT_EQ(fired[3], 233u);
  ASSERT_EQ(fired[4], 266u);
  ASSERT_EQ(fired[5], 299u);
  ASSERT_EQ(ret[166], 0);
  ASSERT_EQ(ret[167], 1);
  ASSERT_EQ(ret[168], 0);
  ASSERT_EQ(ret[199], 0);
  ASSERT_EQ(ret[200], 1);
  ASSERT_EQ(ret[201], 0);
}

SF_TEST(das_release_restarts_charge) {
  AutoShift a;
  for (uint32_t t = 0; t <= 180; ++t) {
    a.update(Dir::Right, t);
  }
  ASSERT_EQ(a.update(Dir::None, 180), 0);
  uint32_t fired[16];
  int count = 0;
  int retAt200 = -1;
  for (uint32_t t = 181; t <= 360; ++t) {
    int n = a.update(Dir::Right, t);
    if (t == 200) {
      retAt200 = n;
    }
    for (int i = 0; i < n && count < 16; ++i) {
      fired[count++] = t;
    }
  }
  ASSERT_EQ(fired[0], 181u);
  ASSERT_EQ(fired[1], 348u);
  ASSERT_EQ(retAt200, 0);
}

SF_TEST(das_reversal_restarts_charge) {
  AutoShift a;
  uint32_t first[16];
  int nFirst = 0;
  for (uint32_t t = 0; t <= 250; ++t) {
    int n = a.update(Dir::Right, t);
    for (int i = 0; i < n && nFirst < 16; ++i) {
      first[nFirst++] = t;
    }
  }
  ASSERT_EQ(nFirst, 4);
  ASSERT_EQ(first[0], 0u);
  ASSERT_EQ(first[1], 167u);
  ASSERT_EQ(first[2], 200u);
  ASSERT_EQ(first[3], 233u);
  uint32_t second[16];
  int nSecond = 0;
  for (uint32_t t = 251; t <= 450; ++t) {
    int n = a.update(Dir::Left, t);
    for (int i = 0; i < n && nSecond < 16; ++i) {
      second[nSecond++] = t;
    }
  }
  ASSERT_TRUE(a.dir() == Dir::Left);
  ASSERT_EQ(second[0], 251u);
  ASSERT_EQ(second[1], 418u);
}

SF_TEST(das_same_millisecond_fires_once) {
  AutoShift a;
  ASSERT_EQ(a.update(Dir::Right, 0), 1);
  ASSERT_EQ(a.update(Dir::Right, 0), 0);
  for (uint32_t t = 1; t <= 166; ++t) {
    ASSERT_EQ(a.update(Dir::Right, t), 0);
  }
  ASSERT_EQ(a.update(Dir::Right, 167), 1);
  ASSERT_EQ(a.update(Dir::Right, 167), 0);
}

SF_TEST(das_arr_clamp_and_cap_discards_remainder) {
  AutoShift b(167, 0);
  ASSERT_EQ(b.update(Dir::Right, 0), 1);
  ASSERT_EQ(b.update(Dir::Right, 167), 1);
  ASSERT_EQ(b.update(Dir::Right, 5000), 10);
  ASSERT_EQ(b.update(Dir::Right, 5001), 1);
}

SF_TEST(das_wrap_around_charge) {
  AutoShift a;
  ASSERT_EQ(a.update(Dir::Right, 0xFFFFFFC0u), 1);
  ASSERT_EQ(a.update(Dir::Right, 0x00000066u), 0);
  ASSERT_EQ(a.update(Dir::Right, 0x00000067u), 1);
}

SF_TEST(das_reset_clears_dir_and_charge) {
  AutoShift a;
  ASSERT_EQ(a.update(Dir::Right, 0), 1);
  ASSERT_EQ(a.update(Dir::Right, 10), 0);
  a.reset();
  ASSERT_TRUE(a.dir() == Dir::None);
  ASSERT_EQ(a.update(Dir::Right, 20), 1);
}

SF_TEST(das_coarse_tick_keeps_repeats) {
  AutoShift a;
  int total = 0;
  for (uint32_t t = 0; t <= 320; t += 16) {
    total += a.update(Dir::Right, t);
  }
  ASSERT_EQ(total, 6);
}
