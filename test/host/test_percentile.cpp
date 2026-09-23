#include "framework.h"
#include "stackfall/diag/percentile.h"

#include <cstdint>

SF_TEST(test_percentile) {
  sf::PercentileRing empty;
  for (uint8_t pct = 0; pct <= 100; ++pct) {
    ASSERT_EQ(empty.percentile(pct), 0u);
  }

  sf::PercentileRing firstHundred;
  for (uint32_t value = 1; value <= 100; ++value) {
    firstHundred.push(value);
  }
  ASSERT_EQ(firstHundred.percentile(50), 50u);
  ASSERT_EQ(firstHundred.percentile(95), 95u);
  ASSERT_EQ(firstHundred.percentile(99), 99u);
  ASSERT_EQ(firstHundred.percentile(100), 100u);

  sf::PercentileRing rolling;
  for (uint32_t value = 1; value <= 300; ++value) {
    rolling.push(value);
  }
  ASSERT_EQ(rolling.percentile(0), 45u);

  sf::PercentileRing identical;
  for (uint16_t i = 0; i < 256; ++i) {
    identical.push(77);
  }
  for (uint8_t pct = 0; pct <= 100; ++pct) {
    ASSERT_EQ(identical.percentile(pct), 77u);
  }
}
