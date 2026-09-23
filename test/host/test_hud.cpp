#include <array>
#include <cstdint>
#include <cstring>

#include "framework.h"
#include "stackfall/ui/hud.h"
#include "stackfall/ui/layout.h"
#include "stackfall/ui/screens.h"

namespace {

using sf::ui::hud::Priority;

}  // namespace

SF_TEST(hud_playing_score_fits_85px_and_leaderboard_keeps_133px) {
  sf::Game game;
  game.reset(sf::RuleProfile{}, 11, 0);
  const sf::Board board;
  constexpr sf::ActivePiece kISpawn{sf::PieceId::I, 0, 3, 20};

  game.injectForTest(board, kISpawn, sf::PieceId::T, true, 0);
  game.hardDrop();
  sf::ui::PlayingModel playing = sf::ui::buildPlaying(game, 0);
  ASSERT_TRUE(sf::ui::hud::textWidthPx(playing.hud.left,
                                       playing.hud.leftSize) <= 85);

  uint32_t iterations = 0;
  while (game.score() < 1000000 && iterations < 40000) {
    game.injectForTest(board, kISpawn, sf::PieceId::T, true, 0);
    game.hardDrop();
    ++iterations;
  }
  ASSERT_TRUE(game.score() >= 1000000);
  ASSERT_TRUE(game.score() <= 9999999);
  playing = sf::ui::buildPlaying(game, 0);
  ASSERT_TRUE(sf::ui::hud::textWidthPx(playing.hud.left,
                                       playing.hud.leftSize) <= 85);

  sf::ScoreEntry scores[15]{};
  scores[0] = sf::ScoreEntry{1234567u, 9u, 15u, 0u};
  const sf::ui::HighScoresModel leaderboard =
      sf::ui::buildHighScores(scores, 0);
  ASSERT_TRUE(std::strstr(leaderboard.rows[0].text, "1234567") != nullptr);
  ASSERT_TRUE(
      sf::ui::hud::textWidthPx(leaderboard.rows[0].text, 1) <= 133);
}

SF_TEST(hud_footer_status_and_banner_use_separate_contained_bands) {
  const sf::ui::Rect footer = sf::ui::footerRect();
  const sf::ui::Rect status = sf::ui::kStatusRect;
  const sf::ui::Rect banner = sf::ui::kBannerRect;

  ASSERT_TRUE(sf::ui::contains(footer, status));
  ASSERT_TRUE(sf::ui::contains(footer, banner));
  ASSERT_TRUE(status.y + status.h <= banner.y ||
              banner.y + banner.h <= status.y);
  ASSERT_EQ(status.x, 1);
  ASSERT_EQ(status.y, 218);
  ASSERT_EQ(status.w, 133);
  ASSERT_EQ(status.h, 12);
  ASSERT_EQ(banner.x, 0);
  ASSERT_EQ(banner.y, 230);
  ASSERT_EQ(banner.w, 135);
  ASSERT_EQ(banner.h, 10);
}

SF_TEST(hud_score_width_and_fixed_ladder) {
  ASSERT_EQ(sf::ui::hud::textWidthPx("SCORE 1234567", 2), 156);
  ASSERT_TRUE(sf::ui::hud::textWidthPx("SCORE 1234567", 2) > 135);

  char out[16]{};
  uint8_t size = 0;

  ASSERT_TRUE(sf::ui::hud::formatScore(out, sizeof(out), 9, 131, size));
  ASSERT_STR_EQ(out, "SCORE 9");
  ASSERT_EQ(size, 2);
  ASSERT_EQ(sf::ui::hud::textWidthPx(out, size), 84);

  ASSERT_TRUE(sf::ui::hud::formatScore(out, sizeof(out), 1234567, 131, size));
  ASSERT_STR_EQ(out, "SCORE 1234567");
  ASSERT_EQ(size, 1);
  ASSERT_EQ(sf::ui::hud::textWidthPx(out, size), 78);

  ASSERT_TRUE(sf::ui::hud::formatScore(out, sizeof(out), 1234567, 140, size));
  ASSERT_STR_EQ(out, "SCORE 1.23M");
  ASSERT_EQ(size, 2);
  ASSERT_EQ(sf::ui::hud::textWidthPx(out, size), 132);

  ASSERT_FALSE(sf::ui::hud::formatScore(out, sizeof(out), 12345678, 131, size));
  ASSERT_STR_EQ(out, "SCORE 9999999");
  ASSERT_EQ(size, 1);
}

SF_TEST(hud_battery_width_and_fit_priorities) {
  char battery[8]{};
  for (int pct = 0; pct <= 100; ++pct) {
    const int width = sf::ui::hud::formatBattery(
        battery, sizeof(battery), static_cast<uint8_t>(pct));
    ASSERT_TRUE(width <= 26);
    ASSERT_EQ(width, sf::ui::hud::textWidthPx(battery, 1));
  }
  ASSERT_STR_EQ(battery, "100%");
  ASSERT_EQ(sf::ui::hud::textWidthPx(battery, 1), 24);

  char out[32]{};
  ASSERT_EQ(sf::ui::hud::fitText(out, sizeof(out), "QUAD BACK-TO-BACK", 60, 1,
                                 Priority::Head),
            60);
  ASSERT_STR_EQ(out, "QUAD BACK.");

  ASSERT_EQ(sf::ui::hud::fitText(out, sizeof(out), "QUAD BACK-TO-BACK", 60, 1,
                                 Priority::Tail),
            60);
  ASSERT_STR_EQ(out, ".K-TO-BACK");

  ASSERT_EQ(sf::ui::hud::fitText(out, sizeof(out), "QUAD", 60, 1, Priority::Head), 24);
  ASSERT_STR_EQ(out, "QUAD");
}

SF_TEST(hud_fit_text_small_buffer_is_bounded) {
  std::array<char, 6> guarded = {'L', '?', '?', '?', '?', 'R'};
  char* const out = guarded.data() + 1;

  const int width =
      sf::ui::hud::fitText(out, 4, "OVERFLOW", 120, 1, Priority::Head);

  ASSERT_EQ(guarded.front(), 'L');
  ASSERT_EQ(guarded.back(), 'R');
  ASSERT_EQ(out[3], '\0');
  ASSERT_STR_EQ(out, "OV.");
  ASSERT_EQ(width, 18);
  ASSERT_TRUE(width <= 120);
}
