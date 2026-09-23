#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

#include "framework.h"
#include "stackfall/app/app.h"
#include "stackfall/input/bindings.h"
#include "stackfall/ui/hud.h"
#include "stackfall/ui/screens.h"

namespace {

using sf::App;
using sf::ControlProfile;
using sf::ScoreEntry;
using sf::Screen;
using sf::Settings;
using sf::input::Binding;
using sf::input::kBindingCount;
using sf::input::kBindings;
using sf::ui::GameOverModel;
using sf::ui::HighScoresModel;
using sf::ui::HudModel;
using sf::ui::InstructionsModel;
using sf::ui::SettingsModel;
using sf::input::CalibrationStep;

struct ScoreTable {
  ScoreEntry rows[15];
};

ScoreTable emptyScores() {
  ScoreTable table{};
  for (uint8_t mode = 0; mode < 3; ++mode) {
    for (uint8_t rank = 0; rank < 5; ++rank) {
      ScoreEntry& row = table.rows[static_cast<std::size_t>(mode) * 5u + rank];
      row.score = mode == 1 ? std::numeric_limits<uint32_t>::max() : 0u;
      row.mode = mode;
    }
  }
  return table;
}

}  // namespace

SF_TEST(screens2_playing_instructions_follow_binding_order) {
  constexpr const char* kExpected[] = {
      "BLUE PRESS  ROTATE",       "BLUE HOLD  HOLD/SWAP",
      "SIDE PRESS  HARD DROP",    "SIDE HOLD  RE-ZERO",
      "BLUE x4  CALIBRATE",       "BLUE 1ST+SIDE  PAUSE",
      "TILT LEFT  MOVE LEFT",     "TILT RIGHT  MOVE RIGHT",
      "DIP AWAY  ROTATE",        "DIP TOWARD  SOFT DROP",
  };

  const InstructionsModel model = sf::ui::buildInstructions(Screen::Playing);

  ASSERT_EQ(model.count, std::size(kExpected));
  for (std::size_t i = 0; i < std::size(kExpected); ++i) {
    ASSERT_STR_EQ(model.lines[i], kExpected[i]);
  }
  ASSERT_STR_EQ(model.lines[0], "BLUE PRESS  ROTATE");
}

SF_TEST(screens2_instruction_copy_tracks_binding_drift) {
  std::array<Binding, kBindingCount> changed{};
  for (std::size_t i = 0; i < changed.size(); ++i) {
    changed[i] = kBindings[i];
  }
  changed[0].actionLabel = "SPIN";

  const InstructionsModel model =
      sf::ui::buildInstructions(Screen::Playing, changed.data(), changed.size());

  ASSERT_STR_EQ(model.lines[0], "BLUE PRESS  SPIN");
}

SF_TEST(screens2_title_instructions_count_and_width_are_bounded) {
  std::size_t titleRows = 0;
  for (const Binding& binding : kBindings) {
    if (binding.screen == Screen::Title) {
      ++titleRows;
    }
  }

  const InstructionsModel model = sf::ui::buildInstructions(Screen::Title);

  ASSERT_EQ(model.count, titleRows);
  for (uint8_t i = 0; i < model.count; ++i) {
    ASSERT_TRUE(std::strlen(model.lines[i]) <= 22u);
    ASSERT_TRUE(sf::ui::hud::textWidthPx(model.lines[i], 1) <= 133);
  }
}

SF_TEST(screens2_game_over_seven_digit_score_fits_content_width) {
  const HudModel run{1234567u, 19u, 42u, 98765u, 100u, false};
  const GameOverModel model = sf::ui::buildGameOver(run, 2, 1);
  char score[16]{};
  uint8_t size = 0;

  ASSERT_TRUE(sf::ui::hud::formatScore(score, sizeof(score), model.score, 133,
                                       size));
  ASSERT_STR_EQ(score, "SCORE 1.23M");
  ASSERT_TRUE(sf::ui::hud::textWidthPx(score, size) <= 133);
  ASSERT_EQ(model.lines, 42u);
  ASSERT_EQ(model.level, 19u);
  ASSERT_EQ(model.elapsedMs, 98765u);
  ASSERT_TRUE(model.isHighScore);
  ASSERT_EQ(model.rank, 2u);
  ASSERT_STR_EQ(model.items[0], "RETRY");
  ASSERT_STR_EQ(model.items[1], "TITLE");
  ASSERT_EQ(model.index, 1u);
}

SF_TEST(screens2_empty_high_scores_render_five_placeholders) {
  Settings settings;
  ScoreTable table = emptyScores();
  App app;
  app.begin(settings, table.rows, 0);

  const HighScoresModel model = sf::ui::buildHighScores(app);

  for (uint8_t i = 0; i < 5; ++i) {
    ASSERT_STR_EQ(model.rows[i].text, "-- EMPTY --");
  }
}

SF_TEST(screens2_high_score_row_keeps_seven_digits_within_133px) {
  ScoreTable table = emptyScores();
  table.rows[0] = ScoreEntry{1234567u, 65535u, 20u, 0u};

  const HighScoresModel model = sf::ui::buildHighScores(table.rows, 0);

  ASSERT_EQ(model.rows[0].rank, 1u);
  ASSERT_EQ(model.rows[0].score, 1234567u);
  ASSERT_EQ(model.rows[0].lines, 65535u);
  ASSERT_EQ(model.rows[0].mode, 0u);
  ASSERT_TRUE(std::strstr(model.rows[0].text, "1234567") != nullptr);
  ASSERT_TRUE(sf::ui::hud::textWidthPx(model.rows[0].text, 1) <= 133);
}

SF_TEST(screens2_settings_rotation_and_locked_profiles) {
  Settings settings;
  settings.rotateCw = false;

  SettingsModel model = sf::ui::buildSettings(settings, 0);
  ASSERT_STR_EQ(model.rows[0].label, "ROTATION");
  ASSERT_STR_EQ(model.rows[0].value, "CCW");
  ASSERT_STR_EQ(model.rows[1].label, "PROFILE");
  ASSERT_STR_EQ(model.rows[1].value, "TILT DIP");
  ASSERT_STR_EQ(model.rows[2].label, "TILT SPEED");
  ASSERT_STR_EQ(model.rows[2].value, "NORMAL");
  settings.tiltSensitivity = 4;
  model = sf::ui::buildSettings(settings, 2);
  ASSERT_STR_EQ(model.rows[2].value, "FAST");
  ASSERT_TRUE(model.rows[6].destructive);
  ASSERT_STR_EQ(model.rows[6].label, "RESET SCORES");
  ASSERT_STR_EQ(model.rows[7].label, "BACK");
  ASSERT_STR_EQ(model.rows[7].value, "");
  ASSERT_FALSE(model.rows[7].destructive);

  settings.controlProfile = static_cast<uint8_t>(ControlProfile::BUTTONS_ONLY);
  model = sf::ui::buildSettings(settings, 1);
  ASSERT_STR_EQ(model.rows[1].value, "BUTTONS");
  ASSERT_EQ(model.index, 1u);
}

SF_TEST(screens2_calibrating_pages_are_bounded) {
  auto makeView = [](CalibrationStep step) {
    sf::ui::CalibratingView view{};
    view.step = step;
    view.stillCount = 12;
    view.ppY = 0.01f;
    view.ppZ = 0.02f;
    view.poseDeflectionG = 0.30f;
    view.poseRemainingS = 15;
    view.steerScaleG = 0.36f;
    view.dipScaleG = 0.50f;
    view.reachRightG = 0.36f;
    view.reachLeftG = 0.36f;
    view.tiltGain = 1.0f;
    view.elapsedS = 12;
    view.rotationCount = 2;
    view.rotationState = 2;
    return view;
  };
  const sf::ui::CalibratingView views[] = {
      makeView(CalibrationStep::Intro), makeView(CalibrationStep::Still),
      makeView(CalibrationStep::TiltRight),
      makeView(CalibrationStep::DipToward),
      makeView(CalibrationStep::Maze), makeView(CalibrationStep::Done),
  };

  for (const sf::ui::CalibratingView& view : views) {
    const InstructionsModel model = sf::ui::buildCalibrating(view);
    ASSERT_TRUE(model.count <= 12u);
    for (uint8_t i = 0; i < model.count; ++i) {
      ASSERT_TRUE(std::strlen(model.lines[i]) <= 22u);
      ASSERT_TRUE(sf::ui::hud::textWidthPx(model.lines[i], 1) <= 133);
      ASSERT_TRUE(model.emphasis[i] <= 2u);
    }
  }

  const InstructionsModel intro = sf::ui::buildCalibrating(views[0]);
  ASSERT_EQ(intro.emphasis[0], 1u);
  ASSERT_EQ(intro.emphasis[intro.count - 1u], 1u);
  ASSERT_STR_EQ(intro.lines[0], "HOW TO PLAY");
  ASSERT_STR_EQ(intro.lines[1], "TILT = MOVE");
  ASSERT_STR_EQ(intro.lines[2], "TOP AWAY = ROTATE");
  ASSERT_STR_EQ(intro.lines[3], "TOP TOWARD = DROP");
  ASSERT_STR_EQ(intro.lines[4], "SIDE = SPEED");
  ASSERT_EQ(intro.emphasis[1], 1u);
  const InstructionsModel still = sf::ui::buildCalibrating(views[1]);
  ASSERT_EQ(still.emphasis[4], 2u);
  // Pose steps render through the practice path, not the text path.
  const InstructionsModel pose = sf::ui::buildCalibrating(views[2]);
  ASSERT_EQ(pose.count, 0u);
  const InstructionsModel done = sf::ui::buildCalibrating(views[5]);
  ASSERT_EQ(done.emphasis[0], 1u);
  ASSERT_EQ(done.emphasis[done.count - 1u], 1u);
  ASSERT_STR_EQ(done.lines[0], "DONE");
  ASSERT_STR_EQ(done.lines[3], "STRAYS 0  TIME 12s");
  ASSERT_STR_EQ(done.lines[4], "UP=ROTATE DOWN=DROP");
  ASSERT_STR_EQ(done.lines[5], "TOP AWAY = ROTATE");
  ASSERT_STR_EQ(done.lines[6], "TOP TOWARD = DROP");
  ASSERT_STR_EQ(done.lines[8], "BLUE = PLAY");

  sf::ui::CalibratingView doneView = views[5];
  doneView.strays = 2;
  doneView.elapsedS = 34;
  doneView.tiltGain = 1.5f;
  const InstructionsModel summary = sf::ui::buildCalibrating(doneView);
  ASSERT_STR_EQ(summary.lines[2], "DIP 30 deg");
  ASSERT_STR_EQ(summary.lines[7], "SPEED FAST");
  doneView.dipDefaulted = true;
  const InstructionsModel defaulted = sf::ui::buildCalibrating(doneView);
  ASSERT_STR_EQ(defaulted.lines[2], "DIP DEFAULT");
}

SF_TEST(screens2_maze_and_rotate_banners_guide) {
  sf::ui::PracticeView maze{};
  maze.step = CalibrationStep::Maze;
  maze.ballCol = 4;
  maze.ballRow = 10;
  maze.onCourse = true;
  maze.progress = 0;
  maze.elapsedS = 1;
  maze.tiltGain = 1.0f;
  maze.offCourseMs = 0;
  maze.mazeArmed = true;
  sf::ui::PlayingModel model = sf::ui::buildCalibratePractice(maze);
  ASSERT_STR_EQ(model.banner, "FOLLOW THE GREEN PATH");
  ASSERT_EQ(model.bannerMsLeft, sf::ui::kBannerDurationMs);

  maze.offCourseMs = 500;
  maze.onCourse = false;
  model = sf::ui::buildCalibratePractice(maze);
  ASSERT_STR_EQ(model.banner, "RED = OFF THE PATH");
  ASSERT_EQ(model.bannerMsLeft, sf::ui::kBannerDurationMs);

  maze.offCourseMs = 0;
  maze.onCourse = true;
  maze.progress = sf::input::kMazeCourseLength - 1;
  maze.solved = true;
  model = sf::ui::buildCalibratePractice(maze);
  ASSERT_STR_EQ(model.banner, "DONE");
  ASSERT_EQ(model.bannerMsLeft, sf::ui::kBannerDurationMs);

  sf::ui::PracticeView rotate0{};
  rotate0.step = CalibrationStep::Rotate;
  rotate0.rotationCount = 0;
  rotate0.rotationState = 0;
  model = sf::ui::buildCalibratePractice(rotate0);
  ASSERT_STR_EQ(model.banner, "TOP AWAY = ROTATE");
  ASSERT_STR_EQ(model.hud.right, "BLUE");

  sf::ui::PracticeView rotate1{};
  rotate1.step = CalibrationStep::Rotate;
  rotate1.rotationCount = 1;
  rotate1.rotationState = 1;
  model = sf::ui::buildCalibratePractice(rotate1);
  ASSERT_STR_EQ(model.banner, "ONCE MORE");
}

SF_TEST(screens2_calibrate_practice_model_is_well_formed) {
  sf::ui::PracticeView maze{};
  maze.step = CalibrationStep::Maze;
  maze.ballCol = 4;
  maze.ballRow = 10;
  maze.onCourse = true;
  maze.progress = 0;
  maze.elapsedS = 7;
  maze.tiltGain = 1.25f;
  maze.mazeArmed = true;
  sf::ui::PlayingModel mazeModel = sf::ui::buildCalibratePractice(maze);
  int courseCells = 0;
  for (int row = 0; row < sf::ui::kRows; ++row) {
    for (int col = 0; col < sf::ui::kCols; ++col) {
      if (sf::input::kMazeLayout[row][col] != '#') {
        ++courseCells;
      }
    }
  }
  ASSERT_EQ(courseCells, 43);
  ASSERT_EQ(mazeModel.field[10][4], 4u);
  ASSERT_EQ(mazeModel.field[10][5], 5u);
  ASSERT_STR_EQ(mazeModel.hud.left, "MAZE");
  ASSERT_STR_EQ(mazeModel.hud.mid, "7s");
  ASSERT_STR_EQ(mazeModel.hud.right, "x1.3");
  ASSERT_STR_EQ(mazeModel.banner, "FOLLOW THE GREEN PATH");
  for (const sf::ui::Rect active : mazeModel.active) {
    ASSERT_EQ(active.x, 0);
    ASSERT_EQ(active.y, 0);
    ASSERT_EQ(active.w, 0);
    ASSERT_EQ(active.h, 0);
  }

  maze.onCourse = false;
  mazeModel = sf::ui::buildCalibratePractice(maze);
  ASSERT_EQ(mazeModel.field[10][4], 7u);

  sf::ui::PracticeView rotate{};
  rotate.step = CalibrationStep::Rotate;
  rotate.rotationCount = 1;
  rotate.rotationState = 1;
  for (int i = 0; i < 4; ++i) {
    ASSERT_EQ(sf::ui::buildCalibratePractice(rotate).active[i].w > 0, true);
  }
  const sf::ui::PlayingModel rotateModel =
      sf::ui::buildCalibratePractice(rotate);
  ASSERT_EQ(rotateModel.bannerMsLeft, sf::ui::kBannerDurationMs);
  ASSERT_STR_EQ(rotateModel.hud.mid, "6/6");
  ASSERT_STR_EQ(rotateModel.banner, "ONCE MORE");
  ASSERT_TRUE(std::strlen(rotateModel.hud.right) <= 4u);
  ASSERT_TRUE(rotateModel.active[0].w > 0);
  ASSERT_TRUE(rotateModel.active[1].w > 0);
  ASSERT_TRUE(rotateModel.active[2].w > 0);
  ASSERT_TRUE(rotateModel.active[3].w > 0);
}

SF_TEST(screens2_maze_back_to_middle_banner) {
  sf::ui::PracticeView maze{};
  maze.step = CalibrationStep::Maze;
  maze.ballCol = 4;
  maze.ballRow = 10;
  maze.onCourse = true;
  maze.progress = 0;
  maze.elapsedS = 0;
  maze.tiltGain = 1.0f;
  maze.offCourseMs = 0;
  maze.mazeArmed = false;
  sf::ui::PlayingModel model = sf::ui::buildCalibratePractice(maze);
  ASSERT_STR_EQ(model.banner, "HOLD STILL");

  maze.mazeArmed = true;
  model = sf::ui::buildCalibratePractice(maze);
  ASSERT_STR_EQ(model.banner, "FOLLOW THE GREEN PATH");
}
