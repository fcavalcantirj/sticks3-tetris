#include "framework.h"
#include "scenario_fixture.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace {

using sf::test::scenario::FixtureAction;
using sf::test::scenario::FixtureActionKind;
using sf::test::scenario::Scenario;
using sf::test::scenario::fixtureFail;
using sf::test::scenario::kFixtureRows;
using sf::test::scenario::parseScenario;

constexpr uint32_t kScenarioStartMs = 0;
const char* kScenarioDir = "test/host/fixtures/scenarios";

const char* eventName(sf::EventType type) {
  switch (type) {
    case sf::EventType::Spawn: return "Spawn";
    case sf::EventType::Move: return "Move";
    case sf::EventType::Rotate: return "Rotate";
    case sf::EventType::Kick: return "Kick";
    case sf::EventType::SoftDrop: return "SoftDrop";
    case sf::EventType::HardDrop: return "HardDrop";
    case sf::EventType::Hold: return "Hold";
    case sf::EventType::HoldDenied: return "HoldDenied";
    case sf::EventType::Lock: return "Lock";
    case sf::EventType::LineClear: return "LineClear";
    case sf::EventType::TSpin: return "TSpin";
    case sf::EventType::Combo: return "Combo";
    case sf::EventType::BackToBack: return "BackToBack";
    case sf::EventType::PerfectClear: return "PerfectClear";
    case sf::EventType::LevelUp: return "LevelUp";
    case sf::EventType::GameOver: return "GameOver";
  }
  return "?";
}

sf::RuleProfile profileFor(const Scenario& scenario) {
  sf::RuleProfile profile;
  switch (scenario.mode) {
    case sf::GameMode::Endless: profile = sf::endless(); break;
    case sf::GameMode::FortyLine: profile = sf::fortyLine(); break;
    case sf::GameMode::ThreeMinute: profile = sf::threeMinute(); break;
  }
  profile.startLevel = scenario.level;
  profile.horizontalWrap = scenario.wrap;
  return profile;
}

sf::Board boardFromRows(const Scenario& scenario) {
  sf::Board board;
  for (int row = 0; row < kFixtureRows; ++row) {
    for (int col = 0; col < sf::Board::kWidth; ++col) {
      if (scenario.board[static_cast<size_t>(row)][static_cast<size_t>(col)] !=
          '.') {
        board.set(col, sf::Board::kFirstVisibleRow + row, sf::Cell::O);
      }
    }
  }
  int firstHiddenRow = sf::Board::kFirstVisibleRow -
                       static_cast<int>(scenario.hidden.size());
  for (size_t row = 0; row < scenario.hidden.size(); ++row) {
    for (int col = 0; col < sf::Board::kWidth; ++col) {
      if (scenario.hidden[row][static_cast<size_t>(col)] != '.') {
        board.set(col, firstHiddenRow + static_cast<int>(row), sf::Cell::O);
      }
    }
  }
  return board;
}

void drainEvents(sf::Game& game) {
  sf::GameEvent event;
  sf::EventQueue& queue = const_cast<sf::EventQueue&>(game.events());
  while (queue.pop(event)) {
  }
}

void applyAction(sf::Game& game, const FixtureAction& action,
                 uint32_t& nowMs) {
  switch (action.kind) {
    case FixtureActionKind::L: game.move(-1); break;
    case FixtureActionKind::R: game.move(1); break;
    case FixtureActionKind::CW: game.rotate(sf::Turn::CW); break;
    case FixtureActionKind::CCW: game.rotate(sf::Turn::CCW); break;
    case FixtureActionKind::HD: game.hardDrop(); break;
    case FixtureActionKind::SD: game.setSoftDrop(true); break;
    case FixtureActionKind::SU: game.setSoftDrop(false); break;
    case FixtureActionKind::Hold: game.hold(); break;
    case FixtureActionKind::Tick:
      for (uint32_t tick = 0; tick < action.ticks; ++tick) {
        nowMs += game.tickMs();
        game.update(nowMs);
      }
      break;
  }
}

std::vector<sf::GameEvent> copiedEvents(const sf::Game& game) {
  sf::EventQueue queue = game.events();
  std::vector<sf::GameEvent> events;
  sf::GameEvent event;
  while (queue.pop(event)) {
    events.push_back(event);
  }
  return events;
}

bool expectedFilled(const Scenario& scenario, int col, int row) {
  if (row < sf::Board::kFirstVisibleRow) {
    // expectedHidden comes from the optional `expect_hidden:` fixture section.
    int firstHiddenRow = sf::Board::kFirstVisibleRow -
                         static_cast<int>(scenario.expectedHidden.size());
    if (row < firstHiddenRow) {
      return false;
    }
    return scenario.expectedHidden[static_cast<size_t>(row - firstHiddenRow)]
                                  [static_cast<size_t>(col)] != '.';
  }
  return scenario.expectedBoard[static_cast<size_t>(
      row - sf::Board::kFirstVisibleRow)][static_cast<size_t>(col)] != '.';
}

bool boardMatches(const Scenario& scenario, const sf::Board& actual,
                  int& firstCol, int& firstRow) {
  for (int row = 0; row < sf::Board::kTotalRows; ++row) {
    for (int col = 0; col < sf::Board::kWidth; ++col) {
      bool filled = actual.at(col, row) != sf::Cell::Empty;
      if (filled != expectedFilled(scenario, col, row)) {
        firstCol = col;
        firstRow = row;
        return false;
      }
    }
  }
  firstCol = -1;
  firstRow = -1;
  return true;
}

void printBoardDiff(const Scenario& scenario, const sf::Board& actual,
                    int firstCol, int firstRow) {
  std::printf("%s\n", scenario.name.c_str());
  std::printf("expected    actual\n");
  for (int visible = 0; visible < kFixtureRows; ++visible) {
    int boardRow = sf::Board::kFirstVisibleRow + visible;
    for (int col = 0; col < sf::Board::kWidth; ++col) {
      std::putchar(expectedFilled(scenario, col, boardRow) ? 'O' : '.');
    }
    std::printf("  ");
    for (int col = 0; col < sf::Board::kWidth; ++col) {
      std::putchar(actual.at(col, boardRow) == sf::Cell::Empty ? '.' : 'O');
    }
    std::putchar('\n');
    if (boardRow == firstRow) {
      for (int col = 0; col < firstCol; ++col) std::putchar(' ');
      std::printf("^ first board difference\n");
    }
  }
  if (firstRow < 0) {
    std::printf("^ boards match; score or events differ\n");
  } else if (firstRow < sf::Board::kFirstVisibleRow) {
    std::printf("^ first difference is hidden row %d column %d\n", firstRow,
                firstCol);
  }
  std::fflush(stdout);
}

void runScenario(const Scenario& scenario) {
  sf::Game game;
  sf::RuleProfile profile = profileFor(scenario);
  game.reset(profile, scenario.seed, kScenarioStartMs);
  sf::Board board = boardFromRows(scenario);
  game.injectForTest(board, sf::spawnOf(scenario.piece), scenario.hold,
                     scenario.holdEmpty, kScenarioStartMs);
  drainEvents(game);

  const int32_t scoreBefore = game.score();
  uint32_t nowMs = kScenarioStartMs;
  for (const FixtureAction& action : scenario.actions) {
    applyAction(game, action, nowMs);
  }

  std::vector<sf::GameEvent> actualEvents = copiedEvents(game);
  bool eventsMatch = actualEvents.size() == scenario.expectedEvents.size();
  size_t eventDiff = 0;
  size_t common = std::min(actualEvents.size(), scenario.expectedEvents.size());
  while (eventDiff < common &&
         actualEvents[eventDiff].type == scenario.expectedEvents[eventDiff].type &&
         actualEvents[eventDiff].a == scenario.expectedEvents[eventDiff].a) {
    ++eventDiff;
  }
  eventsMatch = eventsMatch && eventDiff == common;

  int32_t scoreDelta = game.score() - scoreBefore;
  bool scoreMatches = scoreDelta == scenario.expectedScore;
  int firstCol = -1;
  int firstRow = -1;
  bool finalBoardMatches =
      boardMatches(scenario, game.board(), firstCol, firstRow);
  if (eventsMatch && scoreMatches && finalBoardMatches) {
    std::printf("PASS %s\n", scenario.name.c_str());
    return;
  }

  printBoardDiff(scenario, game.board(), firstCol, firstRow);
  char detail[384];
  if (!scoreMatches) {
    std::snprintf(detail, sizeof(detail),
                  "record %s score expected %d actual %d", scenario.name.c_str(),
                  scenario.expectedScore, scoreDelta);
  } else if (!eventsMatch) {
    if (eventDiff < common) {
      std::snprintf(detail, sizeof(detail),
                    "record %s event %zu expected %s %u actual %s %u",
                    scenario.name.c_str(), eventDiff,
                    eventName(scenario.expectedEvents[eventDiff].type),
                    static_cast<unsigned>(scenario.expectedEvents[eventDiff].a),
                    eventName(actualEvents[eventDiff].type),
                    static_cast<unsigned>(actualEvents[eventDiff].a));
    } else {
      std::snprintf(detail, sizeof(detail),
                    "record %s event count expected %zu actual %zu",
                    scenario.name.c_str(), scenario.expectedEvents.size(),
                    actualEvents.size());
    }
  } else {
    std::snprintf(detail, sizeof(detail),
                  "record %s board differs at row %d column %d",
                  scenario.name.c_str(), firstRow, firstCol);
  }
  sf::test::failWith(__FILE__, __LINE__, detail);
}

}  // namespace

SF_TEST(scenario_fixture_format_parser_and_loader) {
  std::filesystem::path directory(kScenarioDir);
  if (!std::filesystem::exists(directory) ||
      !std::filesystem::is_directory(directory)) {
    fixtureFail(directory, "missing scenario fixture directory");
  }

  std::vector<std::filesystem::path> paths;
  for (const std::filesystem::directory_entry& entry :
       std::filesystem::directory_iterator(directory)) {
    if (entry.is_regular_file() && entry.path().extension() == ".txt") {
      paths.push_back(entry.path());
    }
  }
  std::sort(paths.begin(), paths.end());
  if (paths.empty()) {
    fixtureFail(directory, "zero scenario records parsed");
  }
  // Deliberately NO target-count guard here. The corpus size is a property of the
  // TASK, asserted by its Verify step (`ls fixtures/scenarios/*.txt | wc -l`), not of
  // the loader: hard-coding the target broke the build every time the corpus was
  // mid-growth — a run that swapped the guard and then stopped before writing the
  // records left the suite red. The empty check above is the loader's real job.

  std::vector<Scenario> scenarios;
  for (const std::filesystem::path& path : paths) {
    scenarios.push_back(parseScenario(path));
  }
  for (size_t i = 0; i < scenarios.size(); ++i) {
    for (size_t j = i + 1; j < scenarios.size(); ++j) {
      if (scenarios[i].name == scenarios[j].name) {
        fixtureFail(scenarios[j].path, "duplicate scenario name");
      }
    }
  }

  for (const Scenario& scenario : scenarios) {
    runScenario(scenario);
  }
}
