#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "framework.h"
#include "stackfall/ui/plan.h"

namespace {

using sf::ui::DrawPlan;
using sf::ui::PlayingModel;
using sf::ui::Rect;

PlayingModel blankModel() {
  PlayingModel model{};
  model.banner = "";
  return model;
}

bool sameRect(Rect a, Rect b) {
  return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

bool planCovers(const DrawPlan& plan, Rect cell) {
  if (plan.full) return true;
  for (uint8_t i = 0; i < plan.count; ++i) {
    if (sf::ui::contains(plan.rects[i], cell)) return true;
  }
  return false;
}

void assertGhostCellsCovered(const DrawPlan& plan, const PlayingModel& before,
                             const PlayingModel& after) {
  for (int i = 0; i < 4; ++i) {
    ASSERT_TRUE(planCovers(plan, before.ghost[i]));
    ASSERT_TRUE(planCovers(plan, after.ghost[i]));
  }
}

void setFlatI(PlayingModel& model, int visibleRow) {
  for (int i = 0; i < 4; ++i) {
    model.active[i] = sf::ui::cellRect(i + 3, visibleRow + sf::ui::kHiddenRows);
  }
}

}  // namespace

SF_TEST(plan_piece_move_and_rotation_cover_old_and_new_ghost_cells) {
  sf::Game game;
  game.reset(sf::RuleProfile{}, 7, 0);
  const sf::Board board;
  constexpr sf::ActivePiece kTSpawn{sf::PieceId::T, 0, 3, 20};

  game.injectForTest(board, kTSpawn, sf::PieceId::I, true, 0);
  PlayingModel before = sf::ui::buildPlaying(game, 0);
  ASSERT_TRUE(game.move(1));
  PlayingModel after = sf::ui::buildPlaying(game, 0);
  assertGhostCellsCovered(sf::ui::diff(before, after), before, after);

  game.injectForTest(board, kTSpawn, sf::PieceId::I, true, 0);
  before = sf::ui::buildPlaying(game, 0);
  ASSERT_TRUE(game.rotate(sf::Turn::CW));
  after = sf::ui::buildPlaying(game, 0);
  assertGhostCellsCovered(sf::ui::diff(before, after), before, after);
}

SF_TEST(plan_gravity_and_rotation_use_one_active_row_band) {
  PlayingModel before = blankModel();
  setFlatI(before, 4);
  PlayingModel after = before;
  setFlatI(after, 5);

  const DrawPlan gravity = sf::ui::diff(before, after);
  ASSERT_FALSE(gravity.full);
  ASSERT_EQ(gravity.count, 1u);
  ASSERT_TRUE(sameRect(gravity.rects[0], Rect{1, 58, 100, 20}));
  ASSERT_EQ(gravity.estUs, 800u);

  before = blankModel();
  before.active[0] = sf::ui::cellRect(4, 25);
  before.active[1] = sf::ui::cellRect(3, 26);
  before.active[2] = sf::ui::cellRect(4, 26);
  before.active[3] = sf::ui::cellRect(5, 26);
  after = before;
  after.active[0] = sf::ui::cellRect(4, 25);
  after.active[1] = sf::ui::cellRect(4, 26);
  after.active[2] = sf::ui::cellRect(5, 26);
  after.active[3] = sf::ui::cellRect(4, 27);

  const DrawPlan rotation = sf::ui::diff(before, after);
  ASSERT_FALSE(rotation.full);
  ASSERT_EQ(rotation.count, 1u);
  ASSERT_TRUE(sameRect(rotation.rects[0], Rect{1, 68, 100, 30}));
  ASSERT_TRUE(rotation.estUs <= 1200u);
}

SF_TEST(plan_line_clear_uses_full_frame_cost) {
  PlayingModel before = blankModel();
  before.field[17][0] = 1;
  before.field[18][2] = 2;
  for (int col = 0; col < sf::ui::kCols; ++col) before.field[19][col] = 3;

  PlayingModel after = before;
  for (int col = 0; col < sf::ui::kCols; ++col) after.field[19][col] = before.field[18][col];
  for (int col = 0; col < sf::ui::kCols; ++col) after.field[18][col] = before.field[17][col];
  for (int col = 0; col < sf::ui::kCols; ++col) after.field[17][col] = 0;

  const DrawPlan plan = sf::ui::diff(before, after);
  ASSERT_TRUE(plan.full);
  ASSERT_EQ(plan.estUs, 12960u);
  ASSERT_EQ(plan.collapses, 0u);
}

SF_TEST(plan_hud_only_uses_top_bar_and_semantic_idle_is_empty) {
  PlayingModel before = blankModel();
  std::strcpy(before.hud.left, "SCORE 100");
  PlayingModel after = before;
  std::strcpy(after.hud.left, "SCORE 1234");

  const DrawPlan score = sf::ui::diff(before, after);
  ASSERT_FALSE(score.full);
  ASSERT_EQ(score.count, 1u);
  ASSERT_TRUE(sameRect(score.rects[0], sf::ui::topBarRect()));
  ASSERT_EQ(score.estUs, 972u);

  char firstBanner[] = "QUAD";
  char secondBanner[] = "QUAD";
  before.banner = firstBanner;
  after = before;
  after.banner = secondBanner;
  ASSERT_TRUE(before == after);

  const DrawPlan idle = sf::ui::diff(before, after);
  ASSERT_FALSE(idle.full);
  ASSERT_EQ(idle.count, 0u);
  ASSERT_EQ(idle.estUs, 0u);
}

SF_TEST(plan_sidebar_and_footer_changes_use_exact_boxes) {
  PlayingModel before = blankModel();
  PlayingModel after = before;
  after.hold = 2;
  DrawPlan plan = sf::ui::diff(before, after);
  ASSERT_EQ(plan.count, 1u);
  ASSERT_TRUE(sameRect(plan.rects[0], sf::ui::holdBoxRect()));

  after = before;
  after.next[1] = 4;
  plan = sf::ui::diff(before, after);
  ASSERT_EQ(plan.count, 1u);
  ASSERT_TRUE(sameRect(plan.rects[0], sf::ui::nextBoxRect(1)));

  after = before;
  after.banner = "T-SPIN";
  after.bannerMsLeft = 1200;
  plan = sf::ui::diff(before, after);
  ASSERT_EQ(plan.count, 1u);
  ASSERT_TRUE(sameRect(plan.rects[0], sf::ui::footerRect()));
}

SF_TEST(plan_ninth_disjoint_region_collapses_once) {
  PlayingModel before = blankModel();
  PlayingModel after = before;
  constexpr std::array<int, 9> kColsAt{{0, 2, 4, 6, 8, 1, 3, 5, 7}};
  constexpr std::array<int, 9> kRowsAt{{0, 0, 0, 0, 0, 2, 2, 2, 2}};
  for (std::size_t i = 0; i < kColsAt.size(); ++i) {
    after.field[kRowsAt[i]][kColsAt[i]] = 1;
  }

  const DrawPlan plan = sf::ui::diff(before, after);
  ASSERT_TRUE(plan.full);
  ASSERT_EQ(plan.collapses, 1u);
  ASSERT_EQ(plan.estUs, 12960u);
}

SF_TEST(plan_recorded_1000_frame_budget) {
  constexpr std::array<uint8_t, 36> kRecordedRows{{
      0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11,
      12, 13, 14, 15, 16, 17, 18, 17, 16, 15, 14, 13,
      12, 11, 10, 9,  8,  7,  6,  5,  4,  3,  2,  1,
  }};
  constexpr uint32_t kFrames = 1000;
  constexpr uint32_t kRecordedMeanBudgetUs = 2000;

  PlayingModel before = blankModel();
  setFlatI(before, kRecordedRows[0]);
  uint64_t totalUs = 0;
  uint32_t fullFrames = 0;
  for (uint32_t frame = 0; frame < kFrames; ++frame) {
    PlayingModel after = before;
    setFlatI(after, kRecordedRows[(frame + 1u) % kRecordedRows.size()]);
    if ((frame + 1u) % 100u == 0u) {
      std::snprintf(after.hud.left, sizeof(after.hud.left), "SCORE %u",
                    static_cast<unsigned>(frame + 1u));
    }

    const DrawPlan plan = sf::ui::diff(before, after);
    totalUs += plan.estUs;
    if (plan.full) ++fullFrames;
    before = after;
  }

  const uint32_t meanUs = static_cast<uint32_t>(totalUs / kFrames);
  std::printf("PLAN frames=1000 mean_us=%u full_frames=%u\n",
              static_cast<unsigned>(meanUs),
              static_cast<unsigned>(fullFrames));
  ASSERT_TRUE(meanUs < kRecordedMeanBudgetUs);
  ASSERT_TRUE(meanUs < 16670u);
  ASSERT_TRUE(fullFrames < 50u);
}
