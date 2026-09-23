#include "framework.h"
#include "stackfall/engine/game.h"

using sf::ActivePiece;
using sf::Board;
using sf::Cell;
using sf::Game;
using sf::GameStatus;
using sf::OverReason;
using sf::PieceId;
using sf::RuleProfile;
using sf::threeMinute;

namespace {

void advanceTo(Game& g, uint32_t targetMs) {
  while (g.elapsedMs() < targetMs && g.status() == GameStatus::Playing) {
    g.update(g.lastNowMs() + g.tickMs());
  }
}

}  // namespace

SF_TEST(game_time_accumulator_clamped_over_many_large_frames) {
  Game g;
  g.reset(RuleProfile{}, 0, 0);

  for (int i = 0; i < 100; ++i) {
    uint32_t now = static_cast<uint32_t>((i + 1) * 900);
    g.update(now);
    // 900 ms clamps to the 64 ms catch-up budget and drains exactly, leaving
    // the accumulator at zero. The real invariant is that it never wrapped.
    ASSERT_EQ(g.tickAccumMs(), 0u);
    ASSERT_TRUE(g.tickAccumMs() <=
                static_cast<uint16_t>(Game::kMaxCatchUpTicks * g.tickMs()));
  }

  // 100 frames of 900 ms raw wall time, but only 8 ticks/frame were executed.
  ASSERT_EQ(g.wallMs(), 90000u);
  ASSERT_EQ(g.elapsedMs(), 6400u);
  ASSERT_TRUE(g.discardedMs() > 0u);
}

SF_TEST(game_time_wall_mode_ends_at_time_limit_with_large_steps) {
  Game g;
  g.reset(threeMinute(), 0, 0);

  uint32_t now = 0;
  while (g.status() == GameStatus::Playing && now < 200000) {
    now += 900;
    g.update(now);
  }

  ASSERT_EQ(g.status(), GameStatus::GameOver);
  ASSERT_EQ(g.overReason(), OverReason::ModeComplete);
  ASSERT_EQ(g.wallMs(), 180000u);
}

SF_TEST(game_time_wall_matches_sum_of_raw_steps) {
  Game g;
  g.reset(RuleProfile{}, 0, 0);

  uint32_t now = 0;
  for (int i = 0; i < 10; ++i) {
    now += 5000;
    g.update(now);
  }

  // wallMs_ sees the raw 50 seconds even though each 5-second step was
  // clamped to a single tick in the engine clock.
  ASSERT_EQ(g.wallMs(), 50000u);
  ASSERT_EQ(g.stallCount(), 10u);
}

SF_TEST(game_time_spawn_resets_gravity_for_next_piece) {
  Game g;
  g.reset(RuleProfile{}, 0, 0);

  Board b;
  // Lock an O-piece on the floor so the next piece inherits its lock-delay
  // dwell if the bug is present.
  ActivePiece o{PieceId::O, 0, 4, 38};
  g.injectForTest(b, o, PieceId::I, true, 0);
  advanceTo(g, 504);
  ASSERT_NEQ(g.active().id, PieceId::O);
  uint32_t afterLock = g.elapsedMs();

  // Inject a falling I-piece and confirm it starts with a zeroed accumulator.
  Board empty;
  ActivePiece i{PieceId::I, 0, 3, 20};
  g.injectForTest(empty, i, PieceId::I, true, afterLock);
  ASSERT_EQ(g.gravity().accumulatorMs(), 0u);

  // Level 1 gravity period is 1000 ms. The piece must take the full period
  // for its first row, not the ~496 ms the old accumulator-inheritance bug
  // would have produced.
  advanceTo(g, afterLock + 1000);
  ASSERT_EQ(g.active().row, 21);
}

SF_TEST(game_time_inject_gives_fresh_lock_delay) {
  Game g;
  g.reset(RuleProfile{}, 0, 0);

  Board b;
  ActivePiece o{PieceId::O, 0, 4, 38};
  g.injectForTest(b, o, PieceId::I, true, 0);

  // 40 ticks = 320 ms; the O-piece is still active and has been consuming
  // lock delay the whole time.
  for (int i = 0; i < 40; ++i) {
    g.update(g.lastNowMs() + g.tickMs());
  }
  ASSERT_EQ(g.active().id, PieceId::O);
  ASSERT_EQ(g.elapsedMs(), 320u);

  // Inject a fresh grounded T-piece at the current engine time.
  ActivePiece t{PieceId::T, 0, 4, 38};
  g.injectForTest(b, t, PieceId::I, true, g.elapsedMs());
  ASSERT_TRUE(g.lock().grounded());

  // The T must receive a full 504 ms lock delay from the injection point,
  // not inherit the O-piece's earlier deadline.
  for (int i = 0; i < 62; ++i) {
    g.update(g.lastNowMs() + g.tickMs());
  }
  ASSERT_EQ(g.active().id, PieceId::T);
  ASSERT_EQ(g.elapsedMs(), 320u + 496u);

  g.update(g.lastNowMs() + g.tickMs());
  ASSERT_NEQ(g.active().id, PieceId::T);
}

SF_TEST(game_time_move_from_air_keeps_first_reset) {
  Game g;
  g.reset(RuleProfile{}, 0, 0);

  Board b;
  // Floor at col 5 row 38; an O-piece at col 3 row 36 is airborne.
  b.set(5, 38, Cell::O);
  ActivePiece o{PieceId::O, 0, 3, 36};
  g.injectForTest(b, o, PieceId::I, true, 0);
  ASSERT_FALSE(g.lock().grounded());

  // Move right so the piece lands on the floor. The first grounding must not
  // burn a reset from the budget.
  ASSERT_TRUE(g.move(1));
  ASSERT_TRUE(g.lock().grounded());
  ASSERT_EQ(g.lock().resets(), 0);

  // A subsequent move while already grounded burns one reset.
  ASSERT_TRUE(g.move(1));
  ASSERT_EQ(g.lock().resets(), 1);
}
