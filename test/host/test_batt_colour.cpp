#include <cstdint>

#include "framework.h"
#include "stackfall/ui/hud.h"

SF_TEST(test_batt_colour) {
  constexpr uint8_t kPct[] = {100, 41, 40, 20, 19, 0};
  constexpr sf::ui::BattLevel kExpected[] = {
      sf::ui::BattLevel::Ok,   sf::ui::BattLevel::Ok,
      sf::ui::BattLevel::Ok,   sf::ui::BattLevel::Warn,
      sf::ui::BattLevel::Low,  sf::ui::BattLevel::Low,
  };

  for (uint8_t i = 0; i < 6; ++i) {
    ASSERT_EQ(sf::ui::battColour(kPct[i], false), kExpected[i]);
    ASSERT_EQ(sf::ui::battColour(kPct[i], true),
              sf::ui::BattLevel::Charging);
  }
}
