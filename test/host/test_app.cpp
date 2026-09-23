#include <cstddef>
#include <cstdint>
#include <cstring>

#include "framework.h"
#include "stackfall/app/app.h"

using sf::ActivePiece;
using sf::App;
using sf::AppAction;
using sf::EventQueue;
using sf::EventType;
using sf::GameEvent;
using sf::GameStatus;
using sf::ScoreEntry;
using sf::Screen;
using sf::ScreenTransition;
using sf::Settings;

namespace {

struct ScoreTable {
  ScoreEntry rows[15];
};

ScoreTable emptyScores() {
  ScoreTable table{};
  for (uint8_t mode = 0; mode < 3; ++mode) {
    for (uint8_t rank = 0; rank < 5; ++rank) {
      ScoreEntry& row = table.rows[static_cast<size_t>(mode) * 5u + rank];
      row.score = mode == 1 ? UINT32_MAX : 0u;
      row.lines = 0;
      row.level = 0;
      row.mode = mode;
    }
  }
  return table;
}

void beginEmpty(App& app, uint32_t nowMs = 0) {
  Settings settings;
  ScoreTable table = emptyScores();
  app.begin(settings, table.rows, nowMs);
}

void startGame(App& app, uint32_t nowMs) {
  ASSERT_EQ(app.screen(), Screen::Title);
  ASSERT_EQ(app.menuIndex(), 0u);
  app.onAction(AppAction::Confirm, nowMs);
  ASSERT_EQ(app.screen(), Screen::Calibrating);
  app.onAction(AppAction::Confirm, nowMs + 1u);
  ASSERT_EQ(app.screen(), Screen::Playing);
}

void finishByHardDrops(App& app, uint32_t nowMs) {
  int drops = 0;
  while (app.game().status() == GameStatus::Playing && drops < 100) {
    app.onAction(AppAction::HardDrop, nowMs);
    ++drops;
  }
  ASSERT_TRUE(drops < 100);
  ASSERT_EQ(app.game().status(), GameStatus::GameOver);
  app.step(nowMs);
  ASSERT_EQ(app.screen(), Screen::GameOver);
}

void reachScreen(App& app, Screen target, uint32_t nowMs = 100) {
  beginEmpty(app, 0);
  switch (target) {
    case Screen::Title:
      return;
    case Screen::Playing:
      startGame(app, nowMs);
      return;
    case Screen::Paused:
      startGame(app, nowMs);
      app.onAction(AppAction::Pause, nowMs);
      return;
    case Screen::GameOver:
      startGame(app, nowMs);
      finishByHardDrops(app, nowMs);
      return;
    case Screen::HighScores:
      app.onAction(AppAction::MenuDown, nowMs);
      app.onAction(AppAction::MenuDown, nowMs);
      app.onAction(AppAction::Confirm, nowMs);
      return;
    case Screen::Settings:
      app.onAction(AppAction::MenuDown, nowMs);
      app.onAction(AppAction::MenuDown, nowMs);
      app.onAction(AppAction::MenuDown, nowMs);
      app.onAction(AppAction::Confirm, nowMs);
      return;
    case Screen::Instructions:
      startGame(app, nowMs);
      app.onAction(AppAction::Pause, nowMs);
      app.onAction(AppAction::MenuDown, nowMs);
      app.onAction(AppAction::MenuDown, nowMs);
      app.onAction(AppAction::Confirm, nowMs);
      return;
    case Screen::Diagnostics:
      app.onAction(AppAction::Diag, nowMs);
      return;
    case Screen::Calibrating:
      startGame(app, nowMs);
      app.onAction(AppAction::Calibrate, nowMs + 1u);
      return;
  }
}

uint8_t itemCount(Screen screen) {
  switch (screen) {
    case Screen::Title:
    case Screen::Paused:
      return 4;
    case Screen::GameOver:
      return 2;
    case Screen::Settings:
      return 8;
    case Screen::Playing:
    case Screen::HighScores:
    case Screen::Instructions:
    case Screen::Diagnostics:
      return 1;
    case Screen::Calibrating:
      return 0;
  }
  return 1;
}

void reachScreenAtIndex(App& app, Screen screen, uint8_t menuIndex) {
  reachScreen(app, screen);
  for (uint8_t i = 0; i < menuIndex; ++i) {
    app.onAction(AppAction::MenuDown, 101u + i);
  }
  ASSERT_EQ(app.screen(), screen);
  ASSERT_EQ(app.menuIndex(), menuIndex);
}

constexpr uint8_t kAnyIndex = 0xFF;
constexpr ScreenTransition kExpectedTransitions[] = {
    {Screen::Title, AppAction::Confirm, 0, Screen::Calibrating},
    {Screen::Title, AppAction::Confirm, 2, Screen::HighScores},
    {Screen::Title, AppAction::Confirm, 3, Screen::Settings},
    {Screen::Title, AppAction::Diag, kAnyIndex, Screen::Diagnostics},
    {Screen::HighScores, AppAction::Confirm, kAnyIndex, Screen::Title},
    {Screen::HighScores, AppAction::Back, kAnyIndex, Screen::Title},
    {Screen::Settings, AppAction::Back, kAnyIndex, Screen::Title},
    {Screen::Settings, AppAction::Confirm, 7, Screen::Title},
    {Screen::Diagnostics, AppAction::Back, kAnyIndex, Screen::Title},
    {Screen::Playing, AppAction::Pause, kAnyIndex, Screen::Paused},
    {Screen::Paused, AppAction::Pause, kAnyIndex, Screen::Playing},
    {Screen::Paused, AppAction::Back, kAnyIndex, Screen::Playing},
    {Screen::Paused, AppAction::Confirm, 0, Screen::Playing},
    {Screen::Paused, AppAction::Confirm, 1, Screen::Calibrating},
    {Screen::Paused, AppAction::Confirm, 2, Screen::Instructions},
    {Screen::Paused, AppAction::Confirm, 3, Screen::Title},
    {Screen::Instructions, AppAction::Confirm, kAnyIndex, Screen::Paused},
    {Screen::Instructions, AppAction::Back, kAnyIndex, Screen::Paused},
    {Screen::GameOver, AppAction::Confirm, 0, Screen::Calibrating},
    {Screen::GameOver, AppAction::Confirm, 1, Screen::Title},
    {Screen::GameOver, AppAction::Back, kAnyIndex, Screen::Title},
    {Screen::Calibrating, AppAction::Confirm, kAnyIndex, Screen::Playing},
    {Screen::Calibrating, AppAction::Back, kAnyIndex, Screen::Title},
    {Screen::Playing, AppAction::Calibrate, kAnyIndex, Screen::Calibrating},
};

Screen expectedScreen(Screen from, AppAction action, uint8_t menuIndex) {
  for (const ScreenTransition& transition : kExpectedTransitions) {
    if (transition.from == from && transition.trigger == action &&
        (transition.menuIndex == kAnyIndex ||
         transition.menuIndex == menuIndex)) {
      return transition.to;
    }
  }
  return from;
}

bool hasEvent(EventQueue events, EventType wanted) {
  GameEvent event;
  while (events.pop(event)) {
    if (event.type == wanted) {
      return true;
    }
  }
  return false;
}

}  // namespace

SF_TEST(app_defaults_and_title_confirm_start) {
  App app;
  beginEmpty(app);

  ASSERT_EQ(app.screen(), Screen::Title);
  ASSERT_EQ(app.menuIndex(), 0u);
  ASSERT_EQ(app.settings().controlProfile,
            uint8_t(sf::kActiveControlProfile));
  ASSERT_TRUE(app.settings().rotateCw);
  ASSERT_EQ(app.settings().tiltSensitivity, 2u);
  ASSERT_EQ(app.settings().brightness, 3u);
  ASSERT_EQ(app.settings().volume, 2u);
  ASSERT_TRUE(app.settings().ghostEnabled);
  ASSERT_EQ(app.settings().startLevel, 1u);
  ASSERT_EQ(app.settings().mode, 0u);

  app.onAction(AppAction::Confirm, 10);
  ASSERT_EQ(app.screen(), Screen::Calibrating);
  app.onAction(AppAction::Confirm, 11);
  ASSERT_EQ(app.screen(), Screen::Playing);
  ActivePiece active = app.game().active();
  app.onAction(AppAction::Confirm, 11);
  ASSERT_EQ(app.screen(), Screen::Playing);
  ASSERT_EQ(app.game().active().id, active.id);
  ASSERT_EQ(app.game().active().state, active.state);
  ASSERT_EQ(app.game().active().col, active.col);
  ASSERT_EQ(app.game().active().row, active.row);
}

SF_TEST(app_tilt_speed_action_cycles_the_setting) {
  App app;
  beginEmpty(app);
  app.onAction(AppAction::Confirm, 10);
  ASSERT_EQ(app.screen(), Screen::Calibrating);

  app.onAction(AppAction::TiltSpeed, 20);
  ASSERT_EQ(app.settings().tiltSensitivity, 3u);
  ASSERT_EQ(app.screen(), Screen::Calibrating);
  app.onAction(AppAction::TiltSpeed, 30);
  ASSERT_EQ(app.settings().tiltSensitivity, 4u);
  app.onAction(AppAction::TiltSpeed, 40);
  ASSERT_EQ(app.settings().tiltSensitivity, 0u);
  ASSERT_TRUE(app.wantsFlush(40));
}

SF_TEST(app_calibrating_starts_the_game_on_exit_not_entry) {
  App app;
  beginEmpty(app);

  app.onAction(AppAction::Confirm, 10);
  ASSERT_EQ(app.screen(), Screen::Calibrating);
  ASSERT_EQ(app.game().wallMs(), 0u);

  app.onAction(AppAction::Confirm, 5010);
  ASSERT_EQ(app.screen(), Screen::Playing);
  ASSERT_EQ(app.game().wallMs(), 0u);
  ASSERT_EQ(app.game().active().row,
            sf::spawnOf(app.game().active().id).row);
}

SF_TEST(app_blue_taps_calibration_resumes_the_run) {
  App app;
  beginEmpty(app);
  startGame(app, 0);
  app.onAction(AppAction::HardDrop, 1);
  const uint32_t scoreBefore = static_cast<uint32_t>(app.game().score());
  const sf::Board boardBefore = app.game().board();

  app.onAction(AppAction::Calibrate, 20);
  ASSERT_EQ(app.screen(), Screen::Calibrating);
  app.onAction(AppAction::Confirm, 30);

  ASSERT_EQ(app.screen(), Screen::Playing);
  ASSERT_EQ(app.game().score(), scoreBefore);
  ASSERT_TRUE(app.game().board() == boardBefore);
}

SF_TEST(app_screen_graph_is_exact_and_exhaustive) {
  constexpr size_t expectedCount =
      sizeof(kExpectedTransitions) / sizeof(kExpectedTransitions[0]);
  constexpr size_t actualCount =
      sizeof(sf::kScreenTransitions) / sizeof(sf::kScreenTransitions[0]);
  ASSERT_EQ(actualCount, expectedCount);
  for (size_t i = 0; i < expectedCount; ++i) {
    ASSERT_EQ(sf::kScreenTransitions[i].from, kExpectedTransitions[i].from);
    ASSERT_EQ(sf::kScreenTransitions[i].trigger,
              kExpectedTransitions[i].trigger);
    ASSERT_EQ(sf::kScreenTransitions[i].menuIndex,
              kExpectedTransitions[i].menuIndex);
    ASSERT_EQ(sf::kScreenTransitions[i].to, kExpectedTransitions[i].to);
  }

  for (uint8_t rawScreen = uint8_t(Screen::Title);
       rawScreen <= uint8_t(Screen::Calibrating); ++rawScreen) {
    Screen from = static_cast<Screen>(rawScreen);
    for (uint8_t index = 0; index < itemCount(from); ++index) {
      for (uint8_t rawAction = uint8_t(AppAction::None);
            rawAction <= uint8_t(AppAction::Calibrate); ++rawAction) {
        App app;
        reachScreenAtIndex(app, from, index);
        AppAction action = static_cast<AppAction>(rawAction);
        Screen expected = expectedScreen(from, action, index);
        app.onAction(action, 200);
        ASSERT_EQ(app.screen(), expected);
      }
    }
  }
}

SF_TEST(app_title_menu_wraps_and_mode_cycles) {
  App app;
  beginEmpty(app);
  for (int i = 0; i < 4; ++i) {
    app.onAction(AppAction::MenuDown, static_cast<uint32_t>(i + 1));
  }
  ASSERT_EQ(app.menuIndex(), 0u);

  app.onAction(AppAction::MenuDown, 10);
  ASSERT_EQ(app.menuIndex(), 1u);
  app.onAction(AppAction::Confirm, 11);
  ASSERT_EQ(app.settings().mode, 1u);
  ASSERT_EQ(app.screen(), Screen::Title);
  app.onAction(AppAction::Confirm, 12);
  ASSERT_EQ(app.settings().mode, 2u);
  app.onAction(AppAction::Confirm, 13);
  ASSERT_EQ(app.settings().mode, 0u);
}

SF_TEST(app_game_over_requests_flush_immediately_and_play_suppresses_it) {
  App app;
  beginEmpty(app);

  // Changing the mode dirties settings before the run begins.
  app.onAction(AppAction::MenuDown, 1);
  app.onAction(AppAction::Confirm, 2);
  for (int i = 0; i < 3; ++i) {
    app.onAction(AppAction::MenuDown, 3u + static_cast<uint32_t>(i));
  }
  app.onAction(AppAction::Confirm, 6);
  ASSERT_EQ(app.screen(), Screen::Calibrating);
  app.onAction(AppAction::Confirm, 7);
  ASSERT_EQ(app.screen(), Screen::Playing);
  ASSERT_FALSE(app.wantsFlush(6));
  ASSERT_FALSE(app.wantsFlush(9999));

  finishByHardDrops(app, 10000);
  const uint32_t gameOverMs = 10000;
  const uint32_t flushRequestedAtMs = 10000;
  ASSERT_TRUE(app.wantsFlush(10000));
  ASSERT_TRUE(app.wantsFlush(10500));
  ASSERT_TRUE(flushRequestedAtMs - gameOverMs <= sf::kFlushDeadlineMs);

  // A retry hides the pending NVS request throughout active play, even with
  // both the settings and scores dirty.
  app.onAction(AppAction::Confirm, 10001);
  ASSERT_EQ(app.screen(), Screen::Calibrating);
  app.onAction(AppAction::Confirm, 10002);
  ASSERT_EQ(app.screen(), Screen::Playing);
  ASSERT_FALSE(app.wantsFlush(10002));
  ASSERT_FALSE(app.wantsFlush(10003));
  app.onAction(AppAction::Pause, 10004);
  ASSERT_TRUE(app.wantsFlush(10004));

  app.markFlushed(10005);
  ASSERT_FALSE(app.wantsFlush(10005));
}

SF_TEST(app_settings_change_and_exit_each_arm_flush) {
  App app;
  reachScreenAtIndex(app, Screen::Settings, 3);
  uint8_t oldBrightness = app.settings().brightness;
  app.onAction(AppAction::Confirm, 20);
  ASSERT_NEQ(app.settings().brightness, oldBrightness);
  ASSERT_TRUE(app.wantsFlush(20));

  app.markFlushed(21);
  ASSERT_FALSE(app.wantsFlush(21));
  app.onAction(AppAction::Back, 22);
  ASSERT_EQ(app.screen(), Screen::Title);
  ASSERT_TRUE(app.wantsFlush(22));
}

SF_TEST(app_every_settings_row_edits_its_own_value) {
  for (uint8_t index = 0; index < 8; ++index) {
    App app;
    beginEmpty(app);
    ScoreTable defaults = emptyScores();
    if (index == 6) {
      app.insertScore(0, 1234, 7, 2);
      app.insertScore(1, 9876, 40, 3);
    }

    const Settings before = app.settings();
    ScoreEntry scoresBefore[15]{};
    for (uint8_t i = 0; i < 15; ++i) {
      scoresBefore[i] = app.scores()[i];
    }

    reachScreen(app, Screen::Settings, 10);
    for (uint8_t i = 0; i < index; ++i) {
      app.onAction(AppAction::MenuDown, 20u + i);
    }
    ASSERT_EQ(app.menuIndex(), index);
    app.onAction(AppAction::Confirm, 40);

    if (index == 7) {
      ASSERT_EQ(app.screen(), Screen::Title);
      ASSERT_TRUE(app.wantsFlush(40));
      continue;
    }

    ASSERT_EQ(app.settings().rotateCw,
              index == 0 ? !before.rotateCw : before.rotateCw);
    ASSERT_EQ(app.settings().controlProfile,
              index == 1
                  ? (before.controlProfile ==
                             uint8_t(sf::ControlProfile::BUTTONS_ONLY)
                         ? uint8_t(sf::ControlProfile::TILT_DIP)
                         : uint8_t(sf::ControlProfile::BUTTONS_ONLY))
                  : before.controlProfile);
    ASSERT_EQ(app.settings().tiltSensitivity,
              index == 2 ? uint8_t((before.tiltSensitivity + 1u) % 5u)
                         : before.tiltSensitivity);
    ASSERT_EQ(app.settings().brightness,
              index == 3 ? uint8_t((before.brightness + 1u) % 5u)
                         : before.brightness);
    ASSERT_EQ(app.settings().volume,
              index == 4 ? uint8_t((before.volume + 1u) % 5u)
                         : before.volume);
    ASSERT_EQ(app.settings().ghostEnabled,
              index == 5 ? !before.ghostEnabled : before.ghostEnabled);

    for (uint8_t i = 0; i < 15; ++i) {
      const ScoreEntry& actual = app.scores()[i];
      const ScoreEntry& expected = index == 6 ? defaults.rows[i] : scoresBefore[i];
      ASSERT_EQ(actual.score, expected.score);
      ASSERT_EQ(actual.lines, expected.lines);
      ASSERT_EQ(actual.level, expected.level);
      ASSERT_EQ(actual.mode, expected.mode);
    }
    ASSERT_TRUE(app.wantsFlush(40));
  }
}

SF_TEST(app_settings_menu_has_eight_items) {
  App app;
  reachScreen(app, Screen::Settings, 10);
  for (uint8_t i = 0; i < 8; ++i) {
    app.onAction(AppAction::MenuDown, 20u + i);
  }
  ASSERT_EQ(app.menuIndex(), 0u);
}

SF_TEST(app_settings_back_row_returns_to_title_and_flushes) {
  App app;
  reachScreenAtIndex(app, Screen::Settings, 0);
  for (uint8_t i = 0; i < 7; ++i) {
    app.onAction(AppAction::MenuDown, 10u + i);
  }
  ASSERT_EQ(app.menuIndex(), 7u);
  app.onAction(AppAction::Confirm, 20);
  ASSERT_EQ(app.screen(), Screen::Title);
  ASSERT_TRUE(app.wantsFlush(20));
}

SF_TEST(app_idle_flush_arms_at_exactly_thirty_seconds) {
  App app;
  beginEmpty(app, 100);
  ASSERT_FALSE(app.wantsFlush(30099));
  ASSERT_TRUE(app.wantsFlush(30100));
  app.markFlushed(30100);
  ASSERT_FALSE(app.wantsFlush(30100));
  ASSERT_FALSE(app.wantsFlush(60099));
  ASSERT_TRUE(app.wantsFlush(60100));
}

SF_TEST(app_endless_scores_insert_descending_without_displacing_ties) {
  App app;
  Settings settings;
  ScoreTable table = emptyScores();
  const uint32_t values[5] = {900, 800, 700, 600, 500};
  for (uint8_t i = 0; i < 5; ++i) {
    table.rows[i] = ScoreEntry{values[i], uint16_t(i), 1, 0};
  }
  app.begin(settings, table.rows, 0);

  ASSERT_EQ(app.insertScore(0, 850, 12, 2), 1);
  ASSERT_EQ(app.scores()[0].score, 900u);
  ASSERT_EQ(app.scores()[1].score, 850u);
  ASSERT_EQ(app.scores()[2].score, 800u);
  ASSERT_EQ(app.scores()[4].score, 600u);
  ASSERT_EQ(app.scores()[1].lines, 12u);
  ASSERT_EQ(app.scores()[1].level, 2u);
  ASSERT_EQ(app.scores()[1].mode, 0u);

  ASSERT_EQ(app.insertScore(0, 950, 20, 3), 0);
  ASSERT_EQ(app.scores()[0].score, 950u);
  ASSERT_EQ(app.scores()[1].score, 900u);
  ASSERT_EQ(app.insertScore(0, 400, 0, 1), -1);
  ASSERT_EQ(app.insertScore(0, 700, 0, 1), -1);
}

SF_TEST(app_forty_line_scores_insert_ascending_with_max_empty_sentinel) {
  App app;
  Settings settings;
  ScoreTable table = emptyScores();
  const uint32_t values[5] = {39000, 40000, 42000, 42500, 42999};
  for (uint8_t i = 0; i < 5; ++i) {
    table.rows[5u + i] = ScoreEntry{values[i], 40, 1, 1};
  }
  app.begin(settings, table.rows, 0);

  ASSERT_EQ(app.insertScore(1, 41000, 40, 1), 2);
  ASSERT_EQ(app.scores()[5].score, 39000u);
  ASSERT_EQ(app.scores()[6].score, 40000u);
  ASSERT_EQ(app.scores()[7].score, 41000u);
  ASSERT_EQ(app.scores()[8].score, 42000u);
  ASSERT_EQ(app.insertScore(1, 43000, 40, 1), -1);
  ASSERT_EQ(app.insertScore(1, 42000, 40, 1), -1);
}

SF_TEST(app_gameplay_actions_are_dropped_outside_playing) {
  App app;
  beginEmpty(app);
  const sf::Board beforeBoard = app.game().board();
  const ActivePiece beforeActive = app.game().active();
  const int32_t beforeScore = app.game().score();

  app.onAction(AppAction::HardDrop, 100);

  ASSERT_EQ(app.screen(), Screen::Title);
  ASSERT_EQ(std::memcmp(&app.game().board(), &beforeBoard,
                        sizeof(beforeBoard)),
            0);
  ASSERT_EQ(std::memcmp(&app.game().active(), &beforeActive,
                        sizeof(beforeActive)),
            0);
  ASSERT_TRUE(app.game().board() == beforeBoard);
  ASSERT_EQ(app.game().active().id, beforeActive.id);
  ASSERT_EQ(app.game().active().state, beforeActive.state);
  ASSERT_EQ(app.game().active().col, beforeActive.col);
  ASSERT_EQ(app.game().active().row, beforeActive.row);
  ASSERT_EQ(app.game().score(), beforeScore);
}

SF_TEST(app_pause_sequence_rotates_but_never_hard_drops_or_holds) {
  App app;
  beginEmpty(app);
  startGame(app, 0);
  ActivePiece before = app.game().active();
  ASSERT_TRUE(app.game().board().isEmpty());

  app.onAction(AppAction::RotateCw, 1);
  app.onAction(AppAction::Pause, 2);

  ASSERT_EQ(app.screen(), Screen::Paused);
  ASSERT_EQ(app.game().active().id, before.id);
  ASSERT_TRUE(app.game().board().isEmpty());
  ASSERT_EQ(app.game().score(), 0);
  ASSERT_FALSE(app.game().holdUsed());
  ASSERT_FALSE(hasEvent(app.game().events(), EventType::HardDrop));
}

SF_TEST(app_pause_resume_preserves_run_and_restart_resets_it) {
  App app;
  beginEmpty(app);
  startGame(app, 0);
  app.onAction(AppAction::HardDrop, 1);
  ASSERT_TRUE(app.game().score() > 0);
  int32_t scoreBeforePause = app.game().score();
  sf::Board boardBeforePause = app.game().board();

  app.onAction(AppAction::Pause, 2);
  app.onAction(AppAction::Confirm, 3);  // RESUME at index 0.
  ASSERT_EQ(app.screen(), Screen::Playing);
  ASSERT_EQ(app.game().score(), scoreBeforePause);
  ASSERT_TRUE(app.game().board() == boardBeforePause);

  app.onAction(AppAction::Pause, 4);
  app.onAction(AppAction::MenuDown, 5);
  app.onAction(AppAction::Confirm, 6);  // RESTART at index 1.
  ASSERT_EQ(app.screen(), Screen::Calibrating);
  app.onAction(AppAction::Confirm, 7);
  ASSERT_EQ(app.screen(), Screen::Playing);
  ASSERT_EQ(app.game().score(), 0);
  ASSERT_TRUE(app.game().board().isEmpty());
}

SF_TEST(app_uses_the_explicit_next_game_seed) {
  App app;
  beginEmpty(app);

  app.setNextGameSeed(0x12345678u);
  startGame(app, 10);
  ASSERT_EQ(app.game().rules().seed, 0x12345678u);

  app.onAction(AppAction::Pause, 20);
  app.onAction(AppAction::MenuDown, 21);
  app.setNextGameSeed(0x89ABCDEFu);
  app.onAction(AppAction::Confirm, 22);
  ASSERT_EQ(app.screen(), Screen::Calibrating);
  app.onAction(AppAction::Confirm, 23);
  ASSERT_EQ(app.screen(), Screen::Playing);
  ASSERT_EQ(app.game().rules().seed, 0x89ABCDEFu);
}
