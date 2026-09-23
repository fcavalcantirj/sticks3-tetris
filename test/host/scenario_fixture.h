#pragma once

#include <array>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#include "framework.h"
#include "stackfall/engine/events.h"
#include "stackfall/engine/game.h"

namespace sf::test::scenario {

constexpr int kFixtureRows = 20;

enum class FixtureActionKind : uint8_t {
  L,
  R,
  CW,
  CCW,
  HD,
  SD,
  SU,
  Hold,
  Tick
};

struct FixtureAction {
  FixtureActionKind kind;
  uint32_t ticks;
};

struct ExpectedEvent {
  sf::EventType type;
  uint8_t a;
};

struct Scenario {
  std::filesystem::path path;
  std::string name;
  sf::GameMode mode = sf::GameMode::Endless;
  uint32_t seed = 0;
  uint8_t level = 1;
  bool wrap = false;
  std::array<std::string, kFixtureRows> board{};
  std::vector<std::string> hidden;
  sf::PieceId piece = sf::PieceId::I;
  sf::PieceId hold = sf::PieceId::I;
  bool holdEmpty = true;
  std::vector<FixtureAction> actions;
  std::vector<ExpectedEvent> expectedEvents;
  int32_t expectedScore = 0;
  std::array<std::string, kFixtureRows> expectedBoard{};
  std::vector<std::string> expectedHidden;
};

inline void fixtureFail(const std::filesystem::path& path,
                        const std::string& message) {
  std::string text = path.string() + ": " + message;
  sf::test::failWith(__FILE__, __LINE__, text.c_str());
}

inline std::string trim(const std::string& text) {
  size_t first = 0;
  while (first < text.size() &&
         std::isspace(static_cast<unsigned char>(text[first])) != 0) {
    ++first;
  }
  size_t last = text.size();
  while (last > first &&
         std::isspace(static_cast<unsigned char>(text[last - 1])) != 0) {
    --last;
  }
  return text.substr(first, last - first);
}

inline bool parseUnsigned(const std::string& text, uint32_t base,
                          uint32_t& out) {
  if (text.empty() || (base != 10 && base != 16)) {
    return false;
  }
  uint32_t value = 0;
  for (char ch : text) {
    uint32_t digit = 0;
    if (ch >= '0' && ch <= '9') {
      digit = static_cast<uint32_t>(ch - '0');
    } else if (base == 16 && ch >= 'a' && ch <= 'f') {
      digit = static_cast<uint32_t>(ch - 'a' + 10);
    } else if (base == 16 && ch >= 'A' && ch <= 'F') {
      digit = static_cast<uint32_t>(ch - 'A' + 10);
    } else {
      return false;
    }
    if (digit >= base ||
        value > (std::numeric_limits<uint32_t>::max() - digit) / base) {
      return false;
    }
    value = value * base + digit;
  }
  out = value;
  return true;
}

inline bool parseSigned(const std::string& text, int32_t& out) {
  if (text.empty()) {
    return false;
  }
  bool negative = text[0] == '-';
  std::string magnitude = negative ? text.substr(1) : text;
  uint32_t value = 0;
  if (!parseUnsigned(magnitude, 10, value)) {
    return false;
  }
  const uint32_t maxPositive =
      static_cast<uint32_t>(std::numeric_limits<int32_t>::max());
  if ((!negative && value > maxPositive) ||
      (negative && value > maxPositive + 1u)) {
    return false;
  }
  if (negative && value == maxPositive + 1u) {
    out = std::numeric_limits<int32_t>::min();
  } else {
    int32_t signedValue = static_cast<int32_t>(value);
    out = negative ? -signedValue : signedValue;
  }
  return true;
}

inline bool isSnakeCase(const std::string& name) {
  if (name.empty() || name.front() < 'a' || name.front() > 'z' ||
      name.back() == '_') {
    return false;
  }
  bool previousUnderscore = false;
  for (char ch : name) {
    bool underscore = ch == '_';
    bool allowed = (ch >= 'a' && ch <= 'z') ||
                   (ch >= '0' && ch <= '9') || underscore;
    if (!allowed || (underscore && previousUnderscore)) {
      return false;
    }
    previousUnderscore = underscore;
  }
  return true;
}

inline bool parsePiece(const std::string& text, sf::PieceId& out) {
  if (text.size() != 1) {
    return false;
  }
  switch (text[0]) {
    case 'I': out = sf::PieceId::I; return true;
    case 'J': out = sf::PieceId::J; return true;
    case 'L': out = sf::PieceId::L; return true;
    case 'O': out = sf::PieceId::O; return true;
    case 'S': out = sf::PieceId::S; return true;
    case 'T': out = sf::PieceId::T; return true;
    case 'Z': out = sf::PieceId::Z; return true;
  }
  return false;
}

inline bool parseEventType(const std::string& text, sf::EventType& out) {
#define SF_EVENT_CASE(name)      \
  if (text == #name) {           \
    out = sf::EventType::name;   \
    return true;                 \
  }
  SF_EVENT_CASE(Spawn)
  SF_EVENT_CASE(Move)
  SF_EVENT_CASE(Rotate)
  SF_EVENT_CASE(Kick)
  SF_EVENT_CASE(SoftDrop)
  SF_EVENT_CASE(HardDrop)
  SF_EVENT_CASE(Hold)
  SF_EVENT_CASE(HoldDenied)
  SF_EVENT_CASE(Lock)
  SF_EVENT_CASE(LineClear)
  SF_EVENT_CASE(TSpin)
  SF_EVENT_CASE(Combo)
  SF_EVENT_CASE(BackToBack)
  SF_EVENT_CASE(PerfectClear)
  SF_EVENT_CASE(LevelUp)
  SF_EVENT_CASE(GameOver)
#undef SF_EVENT_CASE
  return false;
}

inline void validateBoardRow(const std::filesystem::path& path,
                             const std::string& row) {
  if (row.size() != static_cast<size_t>(sf::Board::kWidth)) {
    fixtureFail(path, "board row must contain exactly 10 characters");
  }
}

inline Scenario parseScenario(const std::filesystem::path& path) {
  std::ifstream input(path);
  if (!input) {
    fixtureFail(path, "cannot open fixture");
  }

  std::vector<std::string> lines;
  std::string raw;
  while (std::getline(input, raw)) {
    if (!raw.empty() && raw.back() == '\r') {
      raw.pop_back();
    }
    std::string line = trim(raw);
    if (line.empty() || line.front() == '#') {
      continue;
    }
    lines.push_back(line);
  }

  Scenario scenario;
  scenario.path = path;
  size_t cursor = 0;
  auto nextLine = [&]() -> const std::string& {
    if (cursor >= lines.size()) {
      fixtureFail(path, "unexpected end of fixture");
    }
    return lines[cursor++];
  };
  auto valueFor = [&](const char* key) -> std::string {
    const std::string& line = nextLine();
    std::string prefix = std::string(key) + ":";
    if (line.rfind(prefix, 0) != 0) {
      fixtureFail(path, "expected " + prefix + " but found '" + line + "'");
    }
    return trim(line.substr(prefix.size()));
  };
  auto expectMarker = [&](const char* marker) {
    const std::string& line = nextLine();
    if (line != marker) {
      fixtureFail(path, "expected '" + std::string(marker) + "'");
    }
  };

  scenario.name = valueFor("name");
  if (!isSnakeCase(scenario.name) || path.stem().string() != scenario.name) {
    fixtureFail(path, "name must be unique snake_case and match the filename");
  }

  std::string mode = valueFor("mode");
  if (mode == "endless") {
    scenario.mode = sf::GameMode::Endless;
  } else if (mode == "fortyLine") {
    scenario.mode = sf::GameMode::FortyLine;
  } else if (mode == "threeMinute") {
    scenario.mode = sf::GameMode::ThreeMinute;
  } else {
    fixtureFail(path, "unknown mode '" + mode + "'");
  }

  std::string seed = valueFor("seed");
  if (seed.size() <= 2 || seed.rfind("0x", 0) != 0 ||
      !parseUnsigned(seed.substr(2), 16, scenario.seed)) {
    fixtureFail(path, "seed must be 0x followed by hexadecimal digits");
  }

  uint32_t level = 0;
  if (!parseUnsigned(valueFor("level"), 10, level) || level == 0 ||
      level > std::numeric_limits<uint8_t>::max()) {
    fixtureFail(path, "level must be in 1..255");
  }
  scenario.level = static_cast<uint8_t>(level);

  std::string wrap = valueFor("wrap");
  if (wrap != "0" && wrap != "1") {
    fixtureFail(path, "wrap must be 0 or 1");
  }
  scenario.wrap = wrap == "1";

  expectMarker("board:");
  for (std::string& row : scenario.board) {
    row = nextLine();
    validateBoardRow(path, row);
  }

  // Optional hidden blocks are top-to-bottom slices anchored at the visible
  // field: the LAST supplied line is board row kFirstVisibleRow - 1, and each
  // preceding line moves one row upward. Uncovered hidden rows stay empty.
  if (cursor < lines.size() && lines[cursor] == "hidden:") {
    ++cursor;
    while (cursor < lines.size() && lines[cursor].rfind("piece:", 0) != 0) {
      if (scenario.hidden.size() == static_cast<size_t>(kFixtureRows)) {
        fixtureFail(path, "hidden must contain between 1 and 20 rows");
      }
      std::string row = nextLine();
      validateBoardRow(path, row);
      scenario.hidden.push_back(row);
    }
    if (scenario.hidden.empty()) {
      fixtureFail(path, "hidden must contain between 1 and 20 rows");
    }
  }

  if (!parsePiece(valueFor("piece"), scenario.piece)) {
    fixtureFail(path, "piece must be one of I,J,L,O,S,T,Z");
  }
  if (cursor < lines.size() && lines[cursor].rfind("hold:", 0) == 0) {
    scenario.holdEmpty = false;
    if (!parsePiece(valueFor("hold"), scenario.hold)) {
      fixtureFail(path, "hold must be one of I,J,L,O,S,T,Z");
    }
  }

  std::istringstream actionStream(valueFor("actions"));
  std::string token;
  while (actionStream >> token) {
    FixtureAction action{FixtureActionKind::L, 0};
    if (token == "L") action.kind = FixtureActionKind::L;
    else if (token == "R") action.kind = FixtureActionKind::R;
    else if (token == "CW") action.kind = FixtureActionKind::CW;
    else if (token == "CCW") action.kind = FixtureActionKind::CCW;
    else if (token == "HD") action.kind = FixtureActionKind::HD;
    else if (token == "SD") action.kind = FixtureActionKind::SD;
    else if (token == "SU") action.kind = FixtureActionKind::SU;
    else if (token == "HOLD") action.kind = FixtureActionKind::Hold;
    else if (token.rfind("TICK", 0) == 0 && token.size() > 4) {
      action.kind = FixtureActionKind::Tick;
      if (!parseUnsigned(token.substr(4), 10, action.ticks)) {
        fixtureFail(path, "invalid tick action '" + token + "'");
      }
    } else {
      fixtureFail(path, "unknown action '" + token + "'");
    }
    scenario.actions.push_back(action);
  }

  expectMarker("expect_events:");
  while (cursor < lines.size() &&
         lines[cursor].rfind("expect_score:", 0) != 0) {
    std::istringstream eventStream(nextLine());
    std::string typeName;
    uint32_t discriminator = 0;
    std::string extra;
    if (!(eventStream >> typeName >> discriminator) ||
        (eventStream >> extra) || discriminator > 255) {
      fixtureFail(path, "event rows must be '<EventType> <0..255>'");
    }
    sf::EventType type;
    if (!parseEventType(typeName, type)) {
      fixtureFail(path, "unknown event type '" + typeName + "'");
    }
    scenario.expectedEvents.push_back(
        ExpectedEvent{type, static_cast<uint8_t>(discriminator)});
  }

  if (!parseSigned(valueFor("expect_score"), scenario.expectedScore)) {
    fixtureFail(path, "expect_score must be a signed 32-bit integer");
  }
  expectMarker("expect_board:");
  for (std::string& row : scenario.expectedBoard) {
    row = nextLine();
    validateBoardRow(path, row);
  }
  if (cursor < lines.size() && lines[cursor] == "expect_hidden:") {
    ++cursor;
    while (cursor < lines.size()) {
      if (scenario.expectedHidden.size() ==
          static_cast<size_t>(kFixtureRows)) {
        fixtureFail(path, "expect_hidden must contain between 1 and 20 rows");
      }
      std::string row = nextLine();
      validateBoardRow(path, row);
      scenario.expectedHidden.push_back(row);
    }
    if (scenario.expectedHidden.empty()) {
      fixtureFail(path, "expect_hidden must contain between 1 and 20 rows");
    }
  }
  if (cursor != lines.size()) {
    fixtureFail(path, "unexpected trailing content '" + lines[cursor] + "'");
  }
  return scenario;
}

}  // namespace sf::test::scenario
