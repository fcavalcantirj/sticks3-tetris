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
using sf::fortyLine;
using sf::threeMinute;

namespace {

void advanceTo(Game& g, uint32_t targetMs) {
  while (g.elapsedMs() < targetMs && g.status() == GameStatus::Playing) {
    uint32_t nextNow = g.lastNowMs() + g.tickMs();
    g.update(nextNow);
  }
}

}  // namespace

SF_TEST(game_lock_resting_piece_locks_at_504ms) {
  Game g;
  g.reset(RuleProfile{}, 0, 0);

  Board b;
  // I-piece horizontal resting on the floor: cells at row 39 cols 3..6.
  ActivePiece active{PieceId::I, 0, 3, 38};
  g.injectForTest(b, active, PieceId::I, true, 0);

  ASSERT_EQ(g.status(), GameStatus::Playing);
  ASSERT_TRUE(g.lock().grounded());

  // 62 ticks = 496 ms: still active.
  advanceTo(g, 496);
  ASSERT_EQ(g.status(), GameStatus::Playing);
  ASSERT_EQ(g.active().id, PieceId::I);
  ASSERT_EQ(g.active().row, 38);

  // 63rd tick brings elapsed time to 504 ms and the lock fires.
  g.update(504);
  ASSERT_EQ(g.status(), GameStatus::Playing);
  ASSERT_NEQ(g.active().id, PieceId::I);
  ASSERT_EQ(g.lines(), 0u);
}

SF_TEST(game_spawn_clears_soft_drop_for_next_piece) {
  Game g;
  g.reset(RuleProfile{}, 0, 0);

  Board b;
  ActivePiece active{PieceId::I, 0, 3, 38};
  g.injectForTest(b, active, PieceId::I, true, 0);
  g.setSoftDrop(true);
  ASSERT_TRUE(g.softDropHeld());

  advanceTo(g, 504);
  ASSERT_EQ(g.status(), GameStatus::Playing);
  ASSERT_TRUE(!g.softDropHeld());

  const int spawnRow = g.active().row;
  advanceTo(g, 1496);
  ASSERT_EQ(g.active().row, spawnRow);
  g.update(1504);
  ASSERT_EQ(g.active().row, spawnRow + 1);
}

SF_TEST(game_lock_move_reset_cap_at_15) {
  Game g;
  g.reset(RuleProfile{}, 0, 0);

  Board b;
  // O-piece 2x2 resting on the floor: cells at rows 38-39 cols 4-5.
  ActivePiece active{PieceId::O, 0, 4, 38};
  g.injectForTest(b, active, PieceId::I, true, 0);
  ASSERT_TRUE(g.lock().grounded());

  // 15 alternating moves each succeed and reset the lock timer.
  for (int i = 0; i < 15; ++i) {
    bool ok = (i % 2 == 0) ? g.move(-1) : g.move(1);
    ASSERT_TRUE(ok);
    ASSERT_EQ(g.lock().resets(), static_cast<uint8_t>(i + 1));
  }
  ASSERT_EQ(g.lock().resets(), 15);

  // After 62 ticks the timer would still be running if the cap were not hit.
  advanceTo(g, 496);
  ASSERT_EQ(g.status(), GameStatus::Playing);

  // The 16th move is refused a reset, so the piece locks on the next tick.
  bool ok16 = g.move(-1);
  ASSERT_TRUE(ok16);
  ASSERT_EQ(g.lock().resets(), 15);
  g.update(504);
  ASSERT_EQ(g.status(), GameStatus::Playing);
  ASSERT_NEQ(g.active().id, PieceId::O);
}

SF_TEST(game_lock_airborne_clears_deadline_keeps_resets) {
  Game g;
  g.reset(RuleProfile{}, 0, 0);

  Board b;
  // A 2-cell wide stack top at cols 4-5 row 38 supports an O-piece at row 36.
  b.set(4, 38, Cell::O);
  b.set(5, 38, Cell::O);
  ActivePiece active{PieceId::O, 0, 4, 36};
  g.injectForTest(b, active, PieceId::I, true, 0);
  ASSERT_TRUE(g.lock().grounded());

  // Two moves that keep the piece fully over the stack each reset the timer.
  ASSERT_TRUE(g.move(1));   // col 5, covers cols 5-6, col 5 on stack -> grounded
  ASSERT_TRUE(g.move(-1));  // col 4, covers cols 4-5, both on stack -> grounded
  ASSERT_EQ(g.lock().resets(), 2);
  ASSERT_TRUE(g.lock().grounded());

  // Two left moves walk the piece completely off the stack.
  ASSERT_TRUE(g.move(-1));  // col 3, covers cols 3-4, col 4 on stack -> grounded
  ASSERT_TRUE(g.lock().grounded());
  ASSERT_TRUE(g.move(-1));  // col 2, covers cols 2-3, no stack -> airborne
  ASSERT_FALSE(g.lock().grounded());
  ASSERT_EQ(g.lock().resets(), 3);
}

SF_TEST(game_lock_clears_four_lines_and_drops_stack) {
  Game g;
  g.reset(RuleProfile{}, 0, 0);

  Board b;
  // Rows 36-38 full; row 39 empty in cols 0..3, filled in cols 4..9.
  for (int row = 36; row <= 38; ++row) {
    b.setRowFromString(row, "OOOOOOOOOO");
  }
  b.setRowFromString(39, "....OOOOOO");

  // I-piece horizontal at row 38 fills cols 0..3 of row 39 and clears rows 36-39.
  ActivePiece active{PieceId::I, 0, 0, 38};
  g.injectForTest(b, active, PieceId::I, true, 0);

  ASSERT_TRUE(g.lock().grounded());
  advanceTo(g, 504);
  ASSERT_EQ(g.lines(), 4u);
  ASSERT_TRUE(g.board().isEmpty());
}

SF_TEST(game_block_out_on_first_update) {
  Game g;
  g.reset(RuleProfile{}, 0, 0);

  Board b;
  // Partially fill the spawn box so every piece collides, but keep rows
  // non-full so a preceding lock does not clear them.
  b.setRowFromString(20, "...OOOO...");
  b.setRowFromString(21, "...OOOO...");

  // Seed an active piece resting on the floor so it locks quickly and forces a spawn.
  ActivePiece active{PieceId::I, 0, 3, 39};
  g.injectForTest(b, active, PieceId::I, true, 0);

  advanceTo(g, 504);
  ASSERT_EQ(g.status(), GameStatus::GameOver);
  ASSERT_EQ(g.overReason(), OverReason::BlockOut);
}

SF_TEST(game_lock_out_in_buffer) {
  Game g;
  g.reset(RuleProfile{}, 0, 0);

  Board b;
  // A floor inside the hidden buffer catches the I-piece vertical.
  b.set(2, 20, Cell::O);
  ActivePiece active{PieceId::I, 1, 0, 16};
  g.injectForTest(b, active, PieceId::I, true, 0);

  advanceTo(g, 504);
  ASSERT_EQ(g.status(), GameStatus::GameOver);
  ASSERT_EQ(g.overReason(), OverReason::LockOut);
}

SF_TEST(game_update_same_now_advances_zero_ticks) {
  Game g;
  g.reset(RuleProfile{}, 0, 0);
  g.update(1000);
  uint32_t elapsed = g.elapsedMs();
  uint16_t accum = g.tickAccumMs();
  g.update(1000);
  ASSERT_EQ(g.elapsedMs(), elapsed);
  ASSERT_EQ(g.tickAccumMs(), accum);
}

SF_TEST(game_update_stall_clamped_to_one_tick) {
  Game g;
  g.reset(RuleProfile{}, 0, 0);
  g.update(1200);
  ASSERT_EQ(g.elapsedMs(), 8u);
  ASSERT_EQ(g.tickAccumMs(), 0u);
  ASSERT_EQ(g.stallCount(), 1u);
}

SF_TEST(game_update_uint32_wrap_advances_four_ticks) {
  Game g;
  // Seed lastNowMs_ with the starting epoch so the first update has zero dt
  // and records no phantom stall; only the deliberate wrap-around is measured.
  g.reset(RuleProfile{}, 0, 0xFFFFFFF0);
  ASSERT_EQ(g.elapsedMs(), 0u);
  ASSERT_EQ(g.stallCount(), 0u);

  g.update(0xFFFFFFF0);
  ASSERT_EQ(g.elapsedMs(), 0u);
  ASSERT_EQ(g.stallCount(), 0u);

  // The wrap-around delta is exactly 32 ms = 4 ticks.
  g.update(0x00000010);
  ASSERT_EQ(g.elapsedMs(), 32u);
  ASSERT_EQ(g.stallCount(), 0u);
}

SF_TEST(game_forty_line_mode_complete_at_40_lines) {
  Game g;
  g.reset(fortyLine(), 0, 0);

  for (int cleared = 0; cleared < 40 && g.status() == GameStatus::Playing;
       ++cleared) {
    Board b;
    b.setRowFromString(39, "....OOOOOO");
    ActivePiece active{PieceId::I, 0, 0, 38};
    g.injectForTest(b, active, PieceId::I, true, g.elapsedMs());
    advanceTo(g, g.elapsedMs() + 504);
  }

  ASSERT_EQ(g.lines(), 40u);
  ASSERT_EQ(g.status(), GameStatus::Playing);

  // One more tick is required for the mode timer to observe the completed target.
  g.update(g.elapsedMs() + 8);
  ASSERT_EQ(g.status(), GameStatus::GameOver);
  ASSERT_EQ(g.overReason(), OverReason::ModeComplete);
}

SF_TEST(game_three_minute_mode_complete_at_180s) {
  Game g;
  g.reset(threeMinute(), 0, 0);

  // Advance one tick at a time so elapsedMs_ lands exactly on the requested
  // millisecond boundaries. Re-inject a fresh piece each iteration so a
  // slow-falling piece never tops the board out before the mode timer fires.
  auto advanceFresh = [](Game& g, uint32_t targetMs) {
    while (g.elapsedMs() < targetMs && g.status() == GameStatus::Playing) {
      Board empty;
      ActivePiece fresh{PieceId::I, 0, 3, 0};
      g.injectForTest(empty, fresh, g.heldPiece(), g.holdEmpty(), g.elapsedMs());
      g.update(g.lastNowMs() + g.tickMs());
    }
  };

  advanceFresh(g, 179000);
  ASSERT_EQ(g.elapsedMs(), 179000u);
  ASSERT_EQ(g.status(), GameStatus::Playing);

  // The tick that brings elapsed time to 179992 ms is still running.
  advanceFresh(g, 179992);
  ASSERT_EQ(g.elapsedMs(), 179992u);
  ASSERT_EQ(g.status(), GameStatus::Playing);

  // The tick that brings elapsed time to 180000 ms triggers completion.
  advanceFresh(g, 180000);
  ASSERT_EQ(g.elapsedMs(), 180000u);
  ASSERT_EQ(g.status(), GameStatus::GameOver);
  ASSERT_EQ(g.overReason(), OverReason::ModeComplete);
}
