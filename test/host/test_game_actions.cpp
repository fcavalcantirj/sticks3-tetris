#include "framework.h"
#include "stackfall/engine/events.h"
#include "stackfall/engine/game.h"

using sf::ActivePiece;
using sf::Board;
using sf::Cell;
using sf::EventQueue;
using sf::EventType;
using sf::Game;
using sf::GameEvent;
using sf::GameStatus;
using sf::PieceId;
using sf::RuleProfile;
using sf::Turn;

namespace {

// Drain the actual event queue so subsequent checks see only new events.
void drainEvents(Game& g) {
  GameEvent ev;
  while (const_cast<EventQueue*>(&g.events())->pop(ev)) {
  }
}

bool popEventOfType(EventQueue events, EventType type, GameEvent& out) {
  GameEvent ev;
  while (events.pop(ev)) {
    if (ev.type == type) {
      out = ev;
      return true;
    }
  }
  return false;
}

bool hasNoEvents(EventQueue events) {
  GameEvent ev;
  return !events.pop(ev);
}

void advanceTo(Game& g, uint32_t targetMs) {
  while (g.elapsedMs() < targetMs && g.status() == GameStatus::Playing) {
    g.update(g.lastNowMs() + g.tickMs());
  }
}

}  // namespace

SF_TEST(game_action_rotate_kicks_on_candidate_one) {
  Game g;
  g.reset(RuleProfile{}, 0, 0);

  Board b;
  // T at state 0, col 4, row 30. Candidate 0 of 0>>R needs cell (6,31),
  // so block that cell to force candidate 1 ({-1,0}).
  b.set(6, 31, Cell::O);
  ActivePiece active{PieceId::T, 0, 4, 30};
  g.injectForTest(b, active, PieceId::I, true, 0);

  drainEvents(g);
  ASSERT_TRUE(g.rotate(Turn::CW));
  ASSERT_EQ(g.active().state, 1u);
  ASSERT_EQ(g.active().col, 3);
  ASSERT_EQ(g.active().row, 30);

  GameEvent rot;
  ASSERT_TRUE(popEventOfType(g.events(), EventType::Rotate, rot));
  ASSERT_EQ(rot.a, 1u);  // new state

  GameEvent kick;
  ASSERT_TRUE(popEventOfType(g.events(), EventType::Kick, kick));
  ASSERT_EQ(kick.a, 1u);  // kick index
}

SF_TEST(game_action_rotate_no_kick_pushes_nothing) {
  Game g;
  g.reset(RuleProfile{}, 0, 0);

  Board b;
  ActivePiece active{PieceId::T, 0, 4, 30};
  g.injectForTest(b, active, PieceId::I, true, 0);

  ActivePiece before = g.active();
  // CCW from state 0 on an empty board succeeds at candidate 0, so use a
  // blocked rotation: fill cells that every candidate of 0>>L needs.
  // State L cells for box (c,r) are (c+1,r),(c,r+1),(c+1,r+1),(c+1,r+2).
  b.set(4, 31, Cell::O);  // blocks candidate 0
  b.set(6, 31, Cell::O);  // blocks candidate 1
  b.set(6, 29, Cell::O);  // blocks candidate 2
  b.set(5, 32, Cell::O);  // blocks candidate 3
  b.set(6, 32, Cell::O);  // blocks candidate 4
  g.injectForTest(b, active, PieceId::I, true, 0);

  drainEvents(g);
  ASSERT_FALSE(g.rotate(Turn::CCW));
  ASSERT_EQ(g.active().id, before.id);
  ASSERT_EQ(g.active().state, before.state);
  ASSERT_EQ(g.active().col, before.col);
  ASSERT_EQ(g.active().row, before.row);

  ASSERT_TRUE(hasNoEvents(g.events()));
}

SF_TEST(game_action_hold_then_hold_denied_board_unchanged) {
  Game g;
  g.reset(RuleProfile{}, 0, 0);

  Board b;
  ActivePiece active{PieceId::T, 0, 4, 30};
  g.injectForTest(b, active, PieceId::I, true, 0);

  drainEvents(g);
  ASSERT_TRUE(g.hold());
  ASSERT_TRUE(g.holdUsed());

  GameEvent hold;
  ASSERT_TRUE(popEventOfType(g.events(), EventType::Hold, hold));

  // The board must still be empty; the active piece was swapped out, not placed.
  ASSERT_TRUE(g.board().isEmpty());

  ASSERT_FALSE(g.hold());
  GameEvent denied;
  ASSERT_TRUE(popEventOfType(g.events(), EventType::HoldDenied, denied));
  ASSERT_TRUE(g.board().isEmpty());
}

SF_TEST(game_action_hold_fills_empty_slot_with_next_piece) {
  Game g;
  g.reset(RuleProfile{}, 0, 0);

  Board b;
  ActivePiece active{PieceId::T, 0, 4, 30};
  g.injectForTest(b, active, PieceId::I, true, 0);

  PieceId firstActive = g.active().id;
  PieceId firstQueue0 = g.queue().peek(0);
  drainEvents(g);
  ASSERT_TRUE(g.hold());
  ASSERT_TRUE(g.holdEmpty() == false);
  ASSERT_EQ(g.heldPiece(), firstActive);
  ASSERT_EQ(g.active().id, firstQueue0);
}

SF_TEST(game_action_wrap_true_o_at_eight_moves_right_to_zero) {
  RuleProfile p;
  p.horizontalWrap = true;
  Game g;
  g.reset(p, 0, 0);

  Board b;
  ActivePiece active{PieceId::O, 0, 8, 30};
  g.injectForTest(b, active, PieceId::I, true, 0);

  ASSERT_TRUE(g.move(1));
  ASSERT_EQ(g.active().col, 0);
}

SF_TEST(game_action_wrap_false_o_at_eight_moves_right_fails) {
  RuleProfile p;
  p.horizontalWrap = false;
  Game g;
  g.reset(p, 0, 0);

  Board b;
  ActivePiece active{PieceId::O, 0, 8, 30};
  g.injectForTest(b, active, PieceId::I, true, 0);

  ASSERT_FALSE(g.move(1));
  ASSERT_EQ(g.active().col, 8);
}

SF_TEST(game_action_hard_drop_travels_18_cells_and_adds_36_score) {
  Game g;
  g.reset(RuleProfile{}, 0, 0);

  Board b;
  ActivePiece active{PieceId::T, 0, 3, 20};
  g.injectForTest(b, active, PieceId::I, true, 0);

  ASSERT_EQ(g.ghostRow(), 38);

  drainEvents(g);
  g.hardDrop();

  // hardDrop locks and spawns the next piece; the score proves the 18-cell drop.
  ASSERT_EQ(g.score(), 36);

  GameEvent hd;
  ASSERT_TRUE(popEventOfType(g.events(), EventType::HardDrop, hd));
  ASSERT_EQ(hd.a, 18u);
  ASSERT_EQ(hd.value, 36);
}

SF_TEST(game_action_soft_drop_latch_non_locking) {
  Game g;
  g.reset(RuleProfile{}, 0, 0);

  Board b;
  ActivePiece active{PieceId::O, 0, 4, 20};
  g.injectForTest(b, active, PieceId::I, true, 0);

  drainEvents(g);
  g.setSoftDrop(true);
  ASSERT_TRUE(g.softDropHeld());

  GameEvent sd;
  ASSERT_TRUE(popEventOfType(g.events(), EventType::SoftDrop, sd));
  ASSERT_EQ(sd.a, 1u);

  // Repeating the same latch produces no event.
  drainEvents(g);
  g.setSoftDrop(true);
  ASSERT_FALSE(popEventOfType(g.events(), EventType::SoftDrop, sd));

  g.setSoftDrop(false);
  ASSERT_FALSE(g.softDropHeld());
  ASSERT_TRUE(popEventOfType(g.events(), EventType::SoftDrop, sd));
  ASSERT_EQ(sd.a, 0u);
}

SF_TEST(game_action_ghost_disabled_returns_own_row) {
  RuleProfile p;
  p.ghostEnabled = false;
  Game g;
  g.reset(p, 0, 0);

  Board b;
  ActivePiece active{PieceId::T, 0, 3, 20};
  g.injectForTest(b, active, PieceId::I, true, 0);

  ASSERT_EQ(g.ghostRow(), 20);
}

SF_TEST(game_action_ghost_enabled_recomputes_landing_row) {
  Game g;
  g.reset(RuleProfile{}, 0, 0);

  Board b;
  ActivePiece active{PieceId::T, 0, 3, 20};
  g.injectForTest(b, active, PieceId::I, true, 0);

  ASSERT_EQ(g.ghostRow(), 38);
}

SF_TEST(game_action_hold_disabled_pushes_hold_denied) {
  RuleProfile p;
  p.holdEnabled = false;
  Game g;
  g.reset(p, 0, 0);

  Board b;
  ActivePiece active{PieceId::T, 0, 4, 30};
  g.injectForTest(b, active, PieceId::I, true, 0);

  drainEvents(g);
  ASSERT_FALSE(g.hold());
  GameEvent denied;
  ASSERT_TRUE(popEventOfType(g.events(), EventType::HoldDenied, denied));
}

SF_TEST(game_action_move_pushes_move_event_with_new_column) {
  Game g;
  g.reset(RuleProfile{}, 0, 0);

  Board b;
  ActivePiece active{PieceId::O, 0, 4, 30};
  g.injectForTest(b, active, PieceId::I, true, 0);

  drainEvents(g);
  ASSERT_TRUE(g.move(1));

  GameEvent mv;
  ASSERT_TRUE(popEventOfType(g.events(), EventType::Move, mv));
  ASSERT_EQ(mv.a, 5u);
}

SF_TEST(game_action_single_clear_pushes_line_clear_event) {
  Game g;
  g.reset(RuleProfile{}, 0, 0);

  Board b;
  b.setRowFromString(39, "OOOOOOOOOO");
  ActivePiece active{PieceId::O, 0, 4, 38};
  g.injectForTest(b, active, PieceId::I, true, 0);

  advanceTo(g, 504);

  ASSERT_EQ(g.lines(), 1u);

  GameEvent lc;
  ASSERT_TRUE(popEventOfType(g.events(), EventType::LineClear, lc));
  ASSERT_EQ(lc.a, 1u);
}

SF_TEST(game_reset_discards_events_from_the_previous_run) {
  Game game;
  game.reset(sf::endless(), 1, 0);
  ASSERT_TRUE(game.move(1));
  ASSERT_TRUE(game.events().size() > 1u);

  game.reset(sf::endless(), 2, 100);
  EventQueue events = game.events();
  ASSERT_EQ(events.size(), 1u);
  GameEvent event{};
  ASSERT_TRUE(events.pop(event));
  ASSERT_EQ(event.type, EventType::Spawn);
  ASSERT_FALSE(events.pop(event));
}
