#include <cstdint>
#include <cstring>

#include "framework.h"
#include "stackfall/app/app.h"
#include "stackfall/engine/events.h"
#include "stackfall/ui/screens.h"

namespace {

using sf::ActivePiece;
using sf::App;
using sf::Board;
using sf::Cell;
using sf::EventQueue;
using sf::EventType;
using sf::Game;
using sf::GameEvent;
using sf::PieceId;
using sf::RuleProfile;
using sf::ui::PausedModel;
using sf::ui::PlayingModel;
using sf::ui::Rect;
using sf::ui::TitleModel;

bool sameRect(Rect a, Rect b) {
  return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

void replaceEvents(Game& game, const GameEvent& event) {
  EventQueue& events = const_cast<EventQueue&>(game.events());
  GameEvent discarded{};
  while (events.pop(discarded)) {
  }
  events.push(event);
}

}  // namespace

SF_TEST(screens1_title_fresh_app_uses_static_defaults) {
  TitleModel model{};
  {
    App app;
    model = sf::ui::buildTitle(app);
  }

  ASSERT_STR_EQ(model.title, "STACKFALL");
  ASSERT_STR_EQ(model.version, "1.0.0");
  ASSERT_STR_EQ(model.mode, "ENDLESS");
  ASSERT_EQ(model.highScore, 0u);
  ASSERT_STR_EQ(model.items[0], "PLAY");
  ASSERT_STR_EQ(model.items[1], "MODE");
  ASSERT_STR_EQ(model.items[2], "HIGH SCORES");
  ASSERT_STR_EQ(model.items[3], "SETTINGS");
  ASSERT_EQ(model.menuIndex, 0u);

  // The source is gone; all render strings must still name static storage.
  ASSERT_EQ(model.title[0], 'S');
  ASSERT_EQ(model.version[0], '1');
  ASSERT_EQ(model.mode[0], 'E');
  ASSERT_EQ(model.items[0][0], 'P');
}

SF_TEST(screens1_playing_copies_visible_rows_only) {
  Game game;
  game.reset(RuleProfile{}, 7, 0);

  Board board;
  board.setRowFromString(19, "JJJJJJJJJJ");
  board.setRowFromString(39, "OOOOOOOOOO");
  game.injectForTest(board, ActivePiece{PieceId::T, 0, 3, 20},
                     PieceId::I, true, 0);

  const PlayingModel model = sf::ui::buildPlaying(game, 0);
  for (int col = 0; col < Board::kWidth; ++col) {
    ASSERT_EQ(model.field[0][col], 0u);
    ASSERT_EQ(model.field[19][col], static_cast<uint8_t>(Cell::O));
  }
  ASSERT_EQ(model.hold, 0u);
  ASSERT_FALSE(model.holdUsed);
  for (uint8_t next : model.next) {
    ASSERT_TRUE(next >= static_cast<uint8_t>(Cell::I));
    ASSERT_TRUE(next <= static_cast<uint8_t>(Cell::Z));
  }
}

SF_TEST(screens1_ghost_is_disjoint_until_piece_has_landed) {
  Game game;
  game.reset(RuleProfile{}, 11, 0);
  Board board;

  game.injectForTest(board, ActivePiece{PieceId::T, 0, 3, 20},
                     PieceId::I, true, 0);
  PlayingModel falling = sf::ui::buildPlaying(game, 0);
  ASSERT_FALSE(falling.ghostHidden);
  for (Rect active : falling.active) {
    for (Rect ghost : falling.ghost) {
      ASSERT_FALSE(sf::ui::intersects(active, ghost));
    }
  }

  game.injectForTest(board, ActivePiece{PieceId::T, 0, 3, 38},
                     PieceId::I, true, 0);
  PlayingModel landed = sf::ui::buildPlaying(game, 0);
  ASSERT_TRUE(landed.ghostHidden);
  for (int i = 0; i < 4; ++i) {
    ASSERT_TRUE(sameRect(landed.active[i], landed.ghost[i]));
  }
}

SF_TEST(screens1_combo_banner_and_model_strings_outlive_sources) {
  PlayingModel playing{};
  PausedModel paused{};
  {
    Game game;
    game.reset(RuleProfile{}, 13, 0);
    replaceEvents(game,
                  GameEvent{EventType::Combo, 3, 0, 150, 0});
    playing = sf::ui::buildPlaying(game, 0);
    paused = sf::ui::buildPaused(game, 2, 0);
  }

  ASSERT_STR_EQ(playing.banner, "COMBO x3");
  ASSERT_TRUE(playing.bannerMsLeft > 0);
  ASSERT_STR_EQ(paused.behind.banner, "COMBO x3");
  ASSERT_STR_EQ(paused.items[0], "RESUME");
  ASSERT_STR_EQ(paused.items[1], "RESTART");
  ASSERT_STR_EQ(paused.items[2], "INSTRUCTIONS");
  ASSERT_STR_EQ(paused.items[3], "TITLE");
}

SF_TEST(screens1_paused_model_is_byte_stable_across_wall_time) {
  Game game;
  game.reset(RuleProfile{}, 17, 0);
  replaceEvents(game, GameEvent{EventType::Combo, 3, 0, 150, 0});

  const PausedModel early = sf::ui::buildPaused(game, 1, 1000);
  const PausedModel late = sf::ui::buildPaused(game, 1, 9000);

  ASSERT_EQ(std::memcmp(&early, &late, sizeof(PausedModel)), 0);
  ASSERT_EQ(early.index, 1u);
  ASSERT_TRUE(early.dim);
  ASSERT_EQ(early.behind.bannerMsLeft, 0u);
}

SF_TEST(screens1_calibration_box_page_shows_box_and_banner) {
  sf::ui::PracticeView view{};
  view.step = sf::input::CalibrationStep::TiltRight;
  view.boxTargetCol = 8;
  view.boxTargetRow = 9;
  view.boxBallCol = 4;
  view.boxBallRow = 10;
  view.boxReached = false;
  view.windowRunning = false;

  const sf::ui::PlayingModel model = sf::ui::buildCalibratePractice(view);
  // Box cells at the right edge (cols 8-9, rows 9-10) are palette 5.
  ASSERT_EQ(model.field[9][8], 5u);
  ASSERT_EQ(model.field[9][9], 5u);
  ASSERT_EQ(model.field[10][8], 5u);
  ASSERT_EQ(model.field[10][9], 5u);
  // Ball cell at (4,10) is yellow (palette 4) when not inside the box.
  ASSERT_EQ(model.field[10][4], 4u);
  // HUD: GO before the window runs, countdown once it does.
  ASSERT_STR_EQ(model.hud.left, "BOX 1/4");
  ASSERT_STR_EQ(model.hud.mid, "GO");
  ASSERT_STR_EQ(model.hud.right, "B00");
  // Banner.
  ASSERT_STR_EQ(model.banner, "BALL INTO THE BOX");

  view.windowRunning = true;
  view.elapsedS = 3;
  const sf::ui::PlayingModel running = sf::ui::buildCalibratePractice(view);
  ASSERT_STR_EQ(running.hud.mid, "3s");
}

SF_TEST(screens1_calibration_box_page_red_ball_when_inside) {
  sf::ui::PracticeView view{};
  view.step = sf::input::CalibrationStep::TiltLeft;
  view.boxTargetCol = 0;
  view.boxTargetRow = 9;
  view.boxBallCol = 0;
  view.boxBallRow = 9;
  view.boxReached = true;
  view.boxBestG = 0.30f;

  const sf::ui::PlayingModel model = sf::ui::buildCalibratePractice(view);
  // Ball inside the box (cols 0-1, rows 9-10) is red (palette 7).
  ASSERT_EQ(model.field[9][0], 7u);
  ASSERT_STR_EQ(model.hud.right, "B17");
  ASSERT_STR_EQ(model.banner, "GOT IT");
}

SF_TEST(screens1_calibration_done_marks_default_dip) {
  sf::ui::CalibratingView view{};
  view.step = sf::input::CalibrationStep::Done;
  view.dipDefaulted = true;

  const sf::ui::InstructionsModel model = sf::ui::buildCalibrating(view);
  ASSERT_STR_EQ(model.lines[0], "DONE");
  ASSERT_STR_EQ(model.lines[2], "DIP DEFAULT");
}
