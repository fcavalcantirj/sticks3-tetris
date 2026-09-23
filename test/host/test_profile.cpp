#include "framework.h"
#include "stackfall/rules/profile.h"

using sf::Board;
using sf::ControlProfile;
using sf::GameMode;
using sf::ModeStatus;
using sf::RuleProfile;
using sf::fortyLine;
using sf::isPerfectClear;
using sf::kMaxCombo;
using sf::levelFor;
using sf::modeStatus;
using sf::threeMinute;
using sf::withControlProfile;

SF_TEST(profile_level_progression) {
  RuleProfile p;
  ASSERT_EQ(levelFor(p, 0), 1);
  ASSERT_EQ(levelFor(p, 9), 1);
  ASSERT_EQ(levelFor(p, 10), 2);
  ASSERT_EQ(levelFor(p, 130), 14);
  ASSERT_EQ(levelFor(p, 140), 15);
  ASSERT_EQ(levelFor(p, 10000), 15);
}

SF_TEST(profile_mode_status_forty_line) {
  RuleProfile p = fortyLine();
  ASSERT_EQ(modeStatus(p, 39, 0), ModeStatus::Running);
  ASSERT_EQ(modeStatus(p, 40, 0), ModeStatus::Complete);
  ASSERT_EQ(modeStatus(p, 41, 0), ModeStatus::Complete);
}

SF_TEST(profile_mode_status_endless) {
  RuleProfile p;
  ASSERT_EQ(p.mode, GameMode::Endless);
  ASSERT_EQ(modeStatus(p, 40, 0), ModeStatus::Running);
  ASSERT_EQ(modeStatus(p, 4000, 0), ModeStatus::Running);
}

SF_TEST(profile_mode_status_three_minute) {
  RuleProfile p = threeMinute();
  ASSERT_EQ(modeStatus(p, 0, 179999), ModeStatus::Running);
  ASSERT_EQ(modeStatus(p, 0, 180000), ModeStatus::Complete);
}

SF_TEST(profile_with_control_profile_buttons_only_wraps) {
  RuleProfile base;
  base.lockDelayMs = 500;
  base.dasMs = 167;

  RuleProfile tilt = withControlProfile(base, ControlProfile::TILT_DIP);
  ASSERT_FALSE(tilt.horizontalWrap);
  ASSERT_EQ(tilt.lockDelayMs, 500);
  ASSERT_EQ(tilt.dasMs, 167);

  RuleProfile buttons = withControlProfile(base, ControlProfile::BUTTONS_ONLY);
  ASSERT_TRUE(buttons.horizontalWrap);
  ASSERT_EQ(buttons.lockDelayMs, 500);
  ASSERT_EQ(buttons.dasMs, 167);
}

SF_TEST(profile_perfect_clear_scans_buffer) {
  Board empty;
  ASSERT_TRUE(isPerfectClear(empty));

  Board hidden;
  hidden.set(0, 3, sf::Cell::I);
  ASSERT_FALSE(isPerfectClear(hidden));

  Board visible;
  visible.set(9, 39, sf::Cell::T);
  ASSERT_FALSE(isPerfectClear(visible));
}

SF_TEST(profile_defaults) {
  RuleProfile p;
  ASSERT_EQ(p.maxCombo, kMaxCombo);
  ASSERT_EQ(p.maxLevel, 15);
  ASSERT_EQ(p.startLevel, 1);
  ASSERT_EQ(p.linesPerLevel, 10);
}

SF_TEST(profile_trivially_copyable_and_size) {
  ASSERT_TRUE(std::is_trivially_copyable<RuleProfile>::value);
  ASSERT_TRUE(sizeof(RuleProfile) <= 40);
}
