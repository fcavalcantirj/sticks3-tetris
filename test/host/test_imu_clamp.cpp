#include <cstdint>

#include "framework.h"
#include "stackfall/input/tilt.h"

SF_TEST(test_imu_clamp) {
  ASSERT_EQ(sf::clampDtMs(0u, 1u, 50u), 1u);
  ASSERT_EQ(sf::clampDtMs(37u, 1u, 50u), 37u);
  ASSERT_EQ(sf::clampDtMs(400u, 1u, 50u), 50u);

  const uint32_t nowMs = 5u;
  const uint32_t lastFreshMs = 0xFFFFFFF0u;
  const uint32_t elapsedMs = nowMs - lastFreshMs;
  ASSERT_EQ(elapsedMs, 21u);
  ASSERT_EQ(sf::clampDtMs(elapsedMs, 1u, 50u), 21u);
}
