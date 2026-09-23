#include <cstdint>

#include "framework.h"

SF_TEST(framework_registers_tests) {
  ASSERT_EQ(1 + 1, 2);
  ASSERT_EQ(uint32_t(3), 3);
  ASSERT_TRUE(true);
}
