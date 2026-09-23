#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "ascii.h"
#include "framework.h"
#include "golden.h"
#include "stackfall/app/app.h"
#include "stackfall/engine/game.h"
#include "stackfall/rules/profile.h"
#include "stackfall/ui/hud.h"
#include "stackfall/ui/screens.h"

namespace {

using sf::App;
using sf::AppAction;
using sf::Game;
using sf::GameStatus;
using sf::PieceId;
using sf::RuleProfile;
using sf::ScoreEntry;
using sf::Screen;
using sf::Settings;
using sf::Turn;
using sf::ui::GameOverModel;
using sf::ui::HighScoresModel;
using sf::ui::InstructionsModel;
using sf::ui::PausedModel;
using sf::ui::PlayingModel;
using sf::ui::SettingsModel;
using sf::ui::TitleModel;

constexpr uint32_t kGoldenSeed = 0x9E3779B9u;
constexpr uint32_t kSnapshotNowMs = 4096u;
constexpr std::size_t kRenderCapacity = 8192;

enum class ScriptAction : uint8_t {
  Left,
  Right,
  Cw,
  Ccw,
  Hold,
  HardDrop,
};

struct ScriptStep {
  ScriptAction action;
  uint8_t repeat;
};

// The queue inside Game is fed by XorShift32 with this task's pinned seed.
// These literal actions, rather than a second random player, make every
// snapshot state byte-stable.
constexpr ScriptStep kEmptyScript[] = {
    {ScriptAction::Left, 1},
    {ScriptAction::Cw, 1},
    {ScriptAction::Right, 1},
};

constexpr ScriptStep kMidgameScript[] = {
    {ScriptAction::Hold, 1},     {ScriptAction::Left, 3},
    {ScriptAction::HardDrop, 1}, {ScriptAction::Right, 4},
    {ScriptAction::HardDrop, 1}, {ScriptAction::Left, 1},
    {ScriptAction::Cw, 1},       {ScriptAction::HardDrop, 1},
    {ScriptAction::Right, 2},    {ScriptAction::HardDrop, 1},
    {ScriptAction::Cw, 1},       {ScriptAction::Left, 3},
    {ScriptAction::HardDrop, 1}, {ScriptAction::Right, 4},
    {ScriptAction::HardDrop, 1}, {ScriptAction::Left, 2},
    {ScriptAction::HardDrop, 1}, {ScriptAction::Right, 4},
    {ScriptAction::HardDrop, 1}, {ScriptAction::Right, 1},
    {ScriptAction::Ccw, 1},      {ScriptAction::HardDrop, 1},
};

void apply(Game& game, ScriptAction action) {
  switch (action) {
    case ScriptAction::Left: game.move(-1); break;
    case ScriptAction::Right: game.move(1); break;
    case ScriptAction::Cw: game.rotate(Turn::CW); break;
    case ScriptAction::Ccw: game.rotate(Turn::CCW); break;
    case ScriptAction::Hold: game.hold(); break;
    case ScriptAction::HardDrop: game.hardDrop(); break;
  }
}

template <std::size_t N>
Game scriptedGame(const ScriptStep (&script)[N]) {
  Game game;
  RuleProfile rules = sf::endless();
  game.reset(rules, kGoldenSeed, 0);
  for (const ScriptStep& step : script) {
    for (uint8_t i = 0; i < step.repeat; ++i) {
      apply(game, step.action);
    }
  }
  if (game.status() != GameStatus::Playing) {
    sf::test::failWith(__FILE__, __LINE__, "golden action script topped out");
  }
  return game;
}

std::string rendered(const TitleModel& model) {
  std::array<char, kRenderCapacity> out{};
  const int length = sf::test::renderAscii(out.data(), out.size(), model);
  ASSERT_TRUE(length >= 0);
  ASSERT_TRUE(static_cast<std::size_t>(length) < out.size());
  return std::string(out.data(), static_cast<std::size_t>(length));
}

template <typename Model>
std::string rendered(const Model& model) {
  std::array<char, kRenderCapacity> out{};
  const int length = sf::test::renderAscii(out.data(), out.size(), model);
  ASSERT_TRUE(length >= 0);
  ASSERT_TRUE(static_cast<std::size_t>(length) < out.size());
  return std::string(out.data(), static_cast<std::size_t>(length));
}

std::string makeTitle() {
  Settings settings;
  ScoreEntry scores[15]{};
  App app;
  app.begin(settings, scores, kGoldenSeed);
  app.onAction(AppAction::MenuDown, kGoldenSeed + 1u);
  return rendered(sf::ui::buildTitle(app));
}

std::string makePlayingEmpty() {
  Game game = scriptedGame(kEmptyScript);
  return rendered(sf::ui::buildPlaying(game, kSnapshotNowMs));
}

PlayingModel midgameModel() {
  Game game = scriptedGame(kMidgameScript);
  return sf::ui::buildPlaying(game, kSnapshotNowMs);
}

std::string makePlayingMidgame() { return rendered(midgameModel()); }

std::string makePlayingSevenDigit() {
  PlayingModel model = midgameModel();
  ASSERT_TRUE(sf::ui::hud::formatScore(model.hud.left,
                                       sizeof(model.hud.left), 1234567u, 85,
                                       model.hud.leftSize));
  return rendered(model);
}

std::string makePaused() {
  Game game = scriptedGame(kMidgameScript);
  const PausedModel model =
      sf::ui::buildPaused(game, 1, kSnapshotNowMs);
  return rendered(model);
}

std::string makeGameOver() {
  const sf::ui::HudModel run{7654321u, 12u, 34u, 98765u, 88u, false};
  return rendered(sf::ui::buildGameOver(run, 2, 1));
}

std::string makeHighScores() {
  ScoreEntry scores[15]{};
  scores[0] = ScoreEntry{1234567u, 87u, 15u, 0u};
  scores[1] = ScoreEntry{765432u, 64u, 12u, 0u};
  scores[2] = ScoreEntry{54321u, 40u, 9u, 0u};
  return rendered(sf::ui::buildHighScores(scores, 0));
}

std::string makeSettingsInstructions() {
  Settings settings;
  settings.rotateCw = false;
  settings.brightness = 4;
  settings.volume = 1;
  const SettingsModel settingsModel = sf::ui::buildSettings(settings, 1);
  const InstructionsModel instructions =
      sf::ui::buildInstructions(Screen::Playing);
  return rendered(settingsModel) + "\n" + rendered(instructions);
}

using MakeGolden = std::string (*)();

struct GoldenSpec {
  const char* name;
  const char* path;
  MakeGolden make;
};

constexpr GoldenSpec kGoldens[] = {
    {"golden_title", "test/host/golden/title.txt", makeTitle},
    {"golden_playing_empty", "test/host/golden/playing_empty.txt",
     makePlayingEmpty},
    {"golden_playing_midgame", "test/host/golden/playing_midgame.txt",
     makePlayingMidgame},
    {"golden_playing_sevendigit",
     "test/host/golden/playing_sevendigit.txt", makePlayingSevenDigit},
    {"golden_paused", "test/host/golden/paused.txt", makePaused},
    {"golden_gameover", "test/host/golden/gameover.txt", makeGameOver},
    {"golden_highscores", "test/host/golden/highscores.txt", makeHighScores},
    {"golden_settings_instructions",
     "test/host/golden/settings_instructions.txt", makeSettingsInstructions},
};

static_assert(std::size(kGoldens) == sf::test::golden::kGoldenCount);

std::string readFile(const char* path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    const std::string message =
        std::string("missing ") + path +
        "; review with: ./build/sf_tests --bless";
    sf::test::failWith(__FILE__, __LINE__, message.c_str());
  }
  return std::string(std::istreambuf_iterator<char>(input),
                     std::istreambuf_iterator<char>());
}

std::string lineAt(const std::string& text, std::size_t position) {
  if (position >= text.size()) return "<EOF>";
  std::size_t begin = position;
  while (begin > 0 && text[begin - 1] != '\n') --begin;
  std::size_t end = text.find('\n', position);
  if (end == std::string::npos) end = text.size();
  return text.substr(begin, end - begin);
}

void compareBytes(const GoldenSpec& spec, const std::string& actual) {
  const std::string expected = readFile(spec.path);
  std::size_t difference = 0;
  const std::size_t common =
      expected.size() < actual.size() ? expected.size() : actual.size();
  while (difference < common && expected[difference] == actual[difference]) {
    ++difference;
  }
  if (difference == expected.size() && difference == actual.size()) return;

  std::size_t line = 1;
  for (std::size_t i = 0; i < difference; ++i) {
    if (actual[i] == '\n') ++line;
  }
  const std::string expectedLine = lineAt(expected, difference);
  const std::string actualLine = lineAt(actual, difference);
  char message[512]{};
  std::snprintf(message, sizeof(message),
                "%s differs at line %zu: expected=\"%s\" actual=\"%s\"; "
                "review with: ./build/sf_tests --bless",
                spec.path, line, expectedLine.c_str(), actualLine.c_str());
  sf::test::failWith(__FILE__, __LINE__, message);
}

void assertLinesAtMost40(const std::string& text) {
  std::size_t start = 0;
  while (start < text.size()) {
    const std::size_t newline = text.find('\n', start);
    const std::size_t end =
        newline == std::string::npos ? text.size() : newline;
    ASSERT_TRUE(end - start <= 40u);
    if (newline == std::string::npos) break;
    start = newline + 1;
  }
}

void assertFieldBorders(const std::string& text) {
  std::size_t start = text.find('\n');
  ASSERT_NEQ(start, std::string::npos);
  ++start;
  for (int row = 0; row < 22; ++row) {
    const std::size_t end = text.find('\n', start);
    ASSERT_NEQ(end, std::string::npos);
    ASSERT_TRUE(end - start >= 12u);
    ASSERT_EQ(text[start], '#');
    ASSERT_EQ(text[start + 11u], '#');
    start = end + 1u;
  }
}

void validate(const GoldenSpec& spec, const std::string& actual) {
  assertLinesAtMost40(actual);
  ASSERT_TRUE(actual.find('?') == std::string::npos);
  if (std::string(spec.name) == "golden_title") {
    ASSERT_TRUE(actual.find("STACKFALL") != std::string::npos);
  } else if (std::string(spec.name) == "golden_playing_midgame") {
    ASSERT_TRUE(actual.find('o') != std::string::npos);
    ASSERT_TRUE(actual.find("| HOLD:T NEXT:I,J,L") != std::string::npos);
    assertFieldBorders(actual);
  } else if (std::string(spec.name) == "golden_playing_sevendigit") {
    ASSERT_TRUE(actual.find("SCORE 1234567") != std::string::npos);
  }
}

void check(const GoldenSpec& spec) {
  const std::string actual = spec.make();
  validate(spec, actual);
  if (!sf::test::golden::blessMode()) {
    compareBytes(spec, actual);
  }
}

}  // namespace

namespace sf::test::golden {
namespace {

bool gBlessMode = false;

}  // namespace

bool blessMode() { return gBlessMode; }

void setBlessMode(bool enabled) { gBlessMode = enabled; }

int blessAll() {
  std::array<std::string, kGoldenCount> renderedGoldens;
  for (std::size_t i = 0; i < renderedGoldens.size(); ++i) {
    renderedGoldens[i] = kGoldens[i].make();
    validate(kGoldens[i], renderedGoldens[i]);
  }

  std::error_code error;
  std::filesystem::create_directories("test/host/golden", error);
  if (error) {
    std::fprintf(stderr, "BLESS ERROR test/host/golden: %s\n",
                 error.message().c_str());
    return -1;
  }

  for (std::size_t i = 0; i < renderedGoldens.size(); ++i) {
    std::ofstream output(kGoldens[i].path,
                         std::ios::binary | std::ios::trunc);
    if (!output) {
      std::fprintf(stderr, "BLESS ERROR %s: cannot write\n",
                   kGoldens[i].path);
      return -1;
    }
    output.write(renderedGoldens[i].data(),
                 static_cast<std::streamsize>(renderedGoldens[i].size()));
    output.close();
    if (!output) {
      std::fprintf(stderr, "BLESS ERROR %s: write failed\n",
                   kGoldens[i].path);
      return -1;
    }
  }
  return static_cast<int>(renderedGoldens.size());
}

}  // namespace sf::test::golden

SF_TEST(golden_title) { check(kGoldens[0]); }

SF_TEST(golden_playing_empty) { check(kGoldens[1]); }

SF_TEST(golden_playing_midgame) { check(kGoldens[2]); }

SF_TEST(golden_playing_sevendigit) { check(kGoldens[3]); }

SF_TEST(golden_paused) { check(kGoldens[4]); }

SF_TEST(golden_gameover) { check(kGoldens[5]); }

SF_TEST(golden_highscores) { check(kGoldens[6]); }

SF_TEST(golden_settings_instructions) { check(kGoldens[7]); }
