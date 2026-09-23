#include "framework.h"
#include "stackfall/engine/game.h"

using sf::ActivePiece;
using sf::Board;
using sf::Cell;
using sf::Game;
using sf::GameMode;
using sf::GameStatus;
using sf::OverReason;
using sf::PieceId;
using sf::RuleProfile;

namespace {

RuleProfile nonDefaultProfile() {
  RuleProfile p;
  // Every numeric field is deliberately different from its default so a
  // regression to hardcoded defaults fails a test.
  p.lockDelayMs = 100;
  p.lockResetLimit = 2;
  p.dasMs = 50;
  p.arrMs = 10;
  p.tickMs = 16;
  p.startLevel = 5;
  p.linesPerLevel = 20;
  p.maxLevel = 10;
  p.nextPreviewCount = 5;
  p.nextQueueMin = 10;
  p.maxCombo = 10;
  p.mode = GameMode::FortyLine;
  p.targetLines = 40;
  p.timeLimitMs = 120000;
  p.seed = 0x12345678;
  return p;
}

void advanceTo(Game& g, uint32_t targetMs) {
  while (g.elapsedMs() < targetMs && g.status() == GameStatus::Playing) {
    uint32_t nextNow = g.lastNowMs() + g.tickMs();
    g.update(nextNow);
  }
}

}  // namespace

SF_TEST(game_profile_uses_custom_tick_ms) {
  Game g;
  g.reset(nonDefaultProfile(), 0, 0);
  ASSERT_EQ(g.tickMs(), 16);
  ASSERT_EQ(g.level(), 5);

  g.update(16);
  ASSERT_EQ(g.elapsedMs(), 16u);
  g.update(32);
  ASSERT_EQ(g.elapsedMs(), 32u);
}

SF_TEST(game_profile_uses_custom_lock_delay) {
  Game g;
  g.reset(nonDefaultProfile(), 0, 0);

  Board b;
  ActivePiece active{PieceId::I, 0, 3, 38};
  g.injectForTest(b, active, PieceId::I, true, 0);
  ASSERT_TRUE(g.lock().grounded());

  // lockDelayMs = 100, tickMs = 16. 6 ticks = 96 ms < 100; 7 ticks = 112 ms.
  advanceTo(g, 96);
  ASSERT_EQ(g.status(), GameStatus::Playing);
  ASSERT_EQ(g.active().id, PieceId::I);

  g.update(112);
  ASSERT_EQ(g.status(), GameStatus::Playing);
  ASSERT_NEQ(g.active().id, PieceId::I);
}

SF_TEST(game_profile_uses_custom_lock_reset_limit) {
  Game g;
  g.reset(nonDefaultProfile(), 0, 0);

  Board b;
  ActivePiece active{PieceId::O, 0, 4, 38};
  g.injectForTest(b, active, PieceId::I, true, 0);
  ASSERT_TRUE(g.lock().grounded());

  ASSERT_TRUE(g.move(-1));
  ASSERT_EQ(g.lock().resets(), 1);
  ASSERT_TRUE(g.move(1));
  ASSERT_EQ(g.lock().resets(), 2);

  // The third move succeeds geometrically but does not reset.
  ASSERT_TRUE(g.move(-1));
  ASSERT_EQ(g.lock().resets(), 2);

  // Lock fires by 112 ms after the last successful reset.
  advanceTo(g, g.elapsedMs() + 112);
  ASSERT_EQ(g.status(), GameStatus::Playing);
  ASSERT_NEQ(g.active().id, PieceId::O);
}

SF_TEST(game_profile_uses_custom_max_combo) {
  Game g;
  g.reset(nonDefaultProfile(), 0, 0);

  // Twelve single-line clears in a row saturate comboCount at the profile's
  // maxCombo of 10. Chain starts at -1, so the sequence is -1,0,1,...,10,10.
  for (int i = 0; i < 12; ++i) {
    Board b;
    b.setRowFromString(39, "OOOOOOOOOO");
    ActivePiece active{PieceId::O, 0, 4, 38};
    g.injectForTest(b, active, PieceId::I, true, g.elapsedMs());
    advanceTo(g, g.elapsedMs() + 200);
  }
  ASSERT_EQ(g.chain().comboCount, 10);
}

SF_TEST(game_profile_uses_custom_next_queue_min) {
  Game g;
  g.reset(nonDefaultProfile(), 0, 0);
  ASSERT_EQ(g.queue().size(), 10);
  ASSERT_EQ(g.queue().previewCount(), 5);
}

SF_TEST(game_profile_mode_and_target_lines) {
  Game g;
  g.reset(nonDefaultProfile(), 0, 0);
  ASSERT_EQ(g.rules().mode, GameMode::FortyLine);

  for (int cleared = 0; cleared < 40 && g.status() == GameStatus::Playing;
       ++cleared) {
    Board b;
    b.setRowFromString(39, "....OOOOOO");
    ActivePiece active{PieceId::I, 0, 0, 38};
    g.injectForTest(b, active, PieceId::I, true, g.elapsedMs());
    advanceTo(g, g.elapsedMs() + 200);
  }
  ASSERT_EQ(g.lines(), 40u);
  g.update(g.elapsedMs() + g.tickMs());
  ASSERT_EQ(g.status(), GameStatus::GameOver);
  ASSERT_EQ(g.overReason(), OverReason::ModeComplete);
}
