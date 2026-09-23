#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "stackfall/input/action.h"
#include "stackfall/input/dip.h"
#include "stackfall/input/tilt.h"

namespace {

using sf::input::ActionKind;
using sf::input::ActionQueue;
using sf::input::Dip;
using sf::input::GameAction;
using sf::input::Tilt;

constexpr const char* kCsvHeader = "t_ms,t_us,ax_g,ay_g,az_g";
constexpr const char* kLabelsSuffix = ".labels.json";

struct Expectation {
  std::string counter;
  std::string op;
  uint32_t value = 0;
};

struct Labels {
  std::string stage;
  bool synthetic = false;
  std::vector<Expectation> expectations;
};

struct Counts {
  std::array<uint32_t, 15> actions{};
  uint32_t moveWhileDip = 0;
  uint32_t dropped = 0;
  uint32_t samples = 0;
};

std::string readText(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) return {};
  std::ostringstream text;
  text << input.rdbuf();
  return text.str();
}

void skipSpace(const std::string& text, size_t& pos) {
  while (pos < text.size() &&
         (text[pos] == ' ' || text[pos] == '\n' || text[pos] == '\r' ||
          text[pos] == '\t')) {
    ++pos;
  }
}

bool parseString(const std::string& text, size_t& pos, std::string& value) {
  skipSpace(text, pos);
  if (pos >= text.size() || text[pos++] != '"') return false;
  value.clear();
  while (pos < text.size()) {
    const char ch = text[pos++];
    if (ch == '"') return true;
    if (ch == '\\') {
      if (pos >= text.size()) return false;
      const char escaped = text[pos++];
      if (escaped != '"' && escaped != '\\' && escaped != '/') return false;
      value.push_back(escaped);
    } else {
      value.push_back(ch);
    }
  }
  return false;
}

bool keyValuePosition(const std::string& text, const std::string& key,
                      size_t& pos) {
  const std::string needle = "\"" + key + "\"";
  pos = text.find(needle);
  if (pos == std::string::npos) return false;
  pos += needle.size();
  skipSpace(text, pos);
  if (pos >= text.size() || text[pos++] != ':') return false;
  skipSpace(text, pos);
  return true;
}

bool parseStringField(const std::string& text, const std::string& key,
                      std::string& value) {
  size_t pos = 0;
  return keyValuePosition(text, key, pos) && parseString(text, pos, value);
}

bool parseBoolField(const std::string& text, const std::string& key,
                    bool& value) {
  size_t pos = 0;
  if (!keyValuePosition(text, key, pos)) return false;
  if (text.compare(pos, 4, "true") == 0) {
    value = true;
    return true;
  }
  if (text.compare(pos, 5, "false") == 0) {
    value = false;
    return true;
  }
  return false;
}

bool parseU32(const std::string& text, size_t& pos, uint32_t& value) {
  skipSpace(text, pos);
  const size_t start = pos;
  uint64_t parsed = 0;
  while (pos < text.size() && text[pos] >= '0' && text[pos] <= '9') {
    parsed = parsed * 10u + static_cast<unsigned>(text[pos++] - '0');
    if (parsed > std::numeric_limits<uint32_t>::max()) return false;
  }
  if (pos == start) return false;
  value = static_cast<uint32_t>(parsed);
  return true;
}

bool parseExpectations(const std::string& text,
                       std::vector<Expectation>& out) {
  size_t pos = 0;
  if (!keyValuePosition(text, "expect", pos) || pos >= text.size() ||
      text[pos++] != '{') {
    return false;
  }
  for (;;) {
    skipSpace(text, pos);
    if (pos >= text.size()) return false;
    if (text[pos] == '}') {
      ++pos;
      return !out.empty();
    }
    std::string counter;
    if (!parseString(text, pos, counter)) return false;
    skipSpace(text, pos);
    if (pos >= text.size() || text[pos++] != ':') return false;
    skipSpace(text, pos);
    if (pos >= text.size() || text[pos++] != '{') return false;
    for (;;) {
      skipSpace(text, pos);
      std::string op;
      if (!parseString(text, pos, op) ||
          (op != "eq" && op != "ge" && op != "le")) {
        return false;
      }
      skipSpace(text, pos);
      if (pos >= text.size() || text[pos++] != ':') return false;
      uint32_t value = 0;
      if (!parseU32(text, pos, value)) return false;
      out.push_back(Expectation{counter, op, value});
      skipSpace(text, pos);
      if (pos >= text.size()) return false;
      if (text[pos] == '}') {
        ++pos;
        break;
      }
      if (text[pos++] != ',') return false;
    }
    skipSpace(text, pos);
    if (pos >= text.size()) return false;
    if (text[pos] == '}') {
      ++pos;
      return !out.empty();
    }
    if (text[pos++] != ',') return false;
  }
}

bool loadLabels(const std::filesystem::path& path, Labels& labels,
                std::string& error) {
  const std::string text = readText(path);
  if (text.empty()) {
    error = "cannot read labels";
    return false;
  }
  std::string schema;
  if (!parseStringField(text, "schema", schema) ||
      schema != "stackfall-trace-labels/1" ||
      !parseStringField(text, "stage", labels.stage) ||
      !parseBoolField(text, "synthetic", labels.synthetic) ||
      !parseExpectations(text, labels.expectations)) {
    error = "malformed stackfall-trace-labels/1 document";
    return false;
  }
  return true;
}

std::vector<std::string> splitCsv(const std::string& line) {
  std::vector<std::string> fields;
  size_t start = 0;
  for (;;) {
    const size_t comma = line.find(',', start);
    fields.push_back(line.substr(start, comma - start));
    if (comma == std::string::npos) return fields;
    start = comma + 1;
  }
}

bool parseUnsignedField(const std::string& token, uint32_t& value) {
  if (token.empty()) return false;
  size_t used = 0;
  try {
    const unsigned long long parsed = std::stoull(token, &used, 10);
    if (used != token.size() ||
        parsed > std::numeric_limits<uint32_t>::max()) {
      return false;
    }
    value = static_cast<uint32_t>(parsed);
    return true;
  } catch (...) {
    return false;
  }
}

bool parseFloatField(const std::string& token, float& value) {
  if (token.empty()) return false;
  size_t used = 0;
  try {
    value = std::stof(token, &used);
    return used == token.size() && std::isfinite(value);
  } catch (...) {
    return false;
  }
}

struct Sample {
  uint32_t tMs = 0;
  uint32_t tUs = 0;
  float ax = 0.0f;
  float ay = 0.0f;
  float az = 0.0f;
};

bool parseSample(const std::string& line, Sample& sample) {
  const std::vector<std::string> fields = splitCsv(line);
  return fields.size() == 5 &&
         parseUnsignedField(fields[0], sample.tMs) &&
         parseUnsignedField(fields[1], sample.tUs) &&
         parseFloatField(fields[2], sample.ax) &&
         parseFloatField(fields[3], sample.ay) &&
         parseFloatField(fields[4], sample.az);
}

void countAction(const GameAction& action, Counts& counts, bool dipEngaged) {
  const size_t index = static_cast<size_t>(action.kind);
  if (index < counts.actions.size()) ++counts.actions[index];
  if (dipEngaged &&
      (action.kind == ActionKind::MoveLeft ||
       action.kind == ActionKind::MoveRight)) {
    ++counts.moveWhileDip;
  }
}

void drain(ActionQueue& queue, Counts& counts, bool dipEngaged) {
  GameAction action{};
  while (queue.pop(action)) countAction(action, counts, dipEngaged);
}

bool replayCsv(const std::filesystem::path& path, Counts& counts,
               std::string& error) {
  std::ifstream input(path);
  std::string line;
  if (!input || !std::getline(input, line) || line != kCsvHeader) {
    error = "missing exact CSV header";
    return false;
  }
  Sample first;
  if (!std::getline(input, line) || !parseSample(line, first)) {
    error = "missing first sample";
    return false;
  }
  counts.samples = 1;
  Tilt tilt;
  Dip dip;
  ActionQueue queue;
  tilt.captureNeutral(first.ay);
  dip.captureNeutral(first.az);
  int previousColumn = tilt.column();
  uint32_t previousMs = first.tMs;

  while (std::getline(input, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty()) continue;
    Sample sample;
    if (!parseSample(line, sample) || sample.tMs < previousMs) {
      error = "malformed sample or decreasing t_ms at row " +
              std::to_string(counts.samples + 2u);
      return false;
    }
    const uint32_t dtMs = sample.tMs - previousMs;
    dip.update(sample.az, dtMs, sample.tMs, queue);
    const bool engaged = dip.engaged();
    const int column = tilt.update(sample.ay, dtMs, engaged);
    const ActionKind move =
        column < previousColumn ? ActionKind::MoveLeft : ActionKind::MoveRight;
    for (int i = 0; i < std::abs(column - previousColumn); ++i) {
      queue.push(move, sample.tMs);
    }
    previousColumn = column;
    drain(queue, counts, engaged);
    previousMs = sample.tMs;
    ++counts.samples;
    (void)sample.tUs;
    (void)sample.ax;
  }
  counts.dropped = queue.dropped();
  if (counts.samples < 2) {
    error = "fixture has fewer than two samples";
    return false;
  }
  return true;
}

uint32_t actionCount(const Counts& counts, ActionKind kind) {
  return counts.actions[static_cast<size_t>(kind)];
}

bool counterValue(const Counts& counts, const std::string& name,
                  uint32_t& value) {
  static const std::map<std::string, ActionKind> actions = {
      {"None", ActionKind::None},
      {"MoveLeft", ActionKind::MoveLeft},
      {"MoveRight", ActionKind::MoveRight},
      {"SoftDropOn", ActionKind::SoftDropOn},
      {"SoftDropOff", ActionKind::SoftDropOff},
      {"RotateCw", ActionKind::RotateCw},
      {"RotateCcw", ActionKind::RotateCcw},
      {"HardDrop", ActionKind::HardDrop},
      {"Hold", ActionKind::Hold},
      {"Pause", ActionKind::Pause},
      {"Confirm", ActionKind::Confirm},
      {"Back", ActionKind::Back},
      {"MenuUp", ActionKind::MenuUp},
      {"MenuDown", ActionKind::MenuDown},
      {"Diag", ActionKind::Diag},
  };
  if (name == "dropped") {
    value = counts.dropped;
    return true;
  }
  if (name == "MoveWhileDip") {
    value = counts.moveWhileDip;
    return true;
  }
  const auto found = actions.find(name);
  if (found == actions.end()) return false;
  value = actionCount(counts, found->second);
  return true;
}

bool expectationsPass(const Labels& labels, const Counts& counts,
                      std::string& error) {
  if (counts.moveWhileDip != 0) {
    error = "steering action emitted while a dip was engaged";
    return false;
  }
  for (const Expectation& expected : labels.expectations) {
    uint32_t actual = 0;
    if (!counterValue(counts, expected.counter, actual)) {
      error = "unknown expectation counter " + expected.counter;
      return false;
    }
    const bool pass =
        (expected.op == "eq" && actual == expected.value) ||
        (expected.op == "ge" && actual >= expected.value) ||
        (expected.op == "le" && actual <= expected.value);
    if (!pass) {
      error = "expect " + expected.counter + " " + expected.op + " " +
              std::to_string(expected.value) + " actual " +
              std::to_string(actual);
      return false;
    }
  }
  return true;
}

std::string fixtureName(const std::filesystem::path& labelsPath) {
  std::string name = labelsPath.filename().string();
  name.resize(name.size() - std::char_traits<char>::length(kLabelsSuffix));
  return name;
}

int usage() {
  std::fprintf(stderr,
               "usage: sf_replay <dir>\n"
               "       sf_replay --all-fixtures\n");
  return 2;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) return usage();
  const std::filesystem::path directory =
      std::string(argv[1]) == "--all-fixtures"
          ? std::filesystem::path("traces")
          : std::filesystem::path(argv[1]);
  if (!std::filesystem::exists(directory) ||
      !std::filesystem::is_directory(directory)) {
    std::fprintf(stderr, "TRACE ERROR %s: directory not found\n",
                 directory.string().c_str());
    return 2;
  }

  std::vector<std::filesystem::path> labelPaths;
  for (const auto& entry :
       std::filesystem::recursive_directory_iterator(directory)) {
    const std::string filename = entry.path().filename().string();
    const size_t suffixLength =
        std::char_traits<char>::length(kLabelsSuffix);
    if (entry.is_regular_file() && filename.size() > suffixLength &&
        filename.compare(filename.size() - suffixLength, suffixLength,
                         kLabelsSuffix) == 0) {
      labelPaths.push_back(entry.path());
    }
  }
  std::sort(labelPaths.begin(), labelPaths.end());
  if (labelPaths.empty()) {
    std::fprintf(stderr, "TRACE ERROR %s: zero fixtures found\n",
                 directory.string().c_str());
    return 2;
  }

  uint32_t passed = 0;
  uint32_t failed = 0;
  uint32_t synthetic = 0;
  for (const std::filesystem::path& labelsPath : labelPaths) {
    const std::string name = fixtureName(labelsPath);
    const std::filesystem::path csvPath =
        labelsPath.parent_path() / (name + ".csv");
    Labels labels;
    Counts counts;
    std::string error;
    bool ok = loadLabels(labelsPath, labels, error);
    if (ok) ok = replayCsv(csvPath, counts, error);
    if (ok) ok = expectationsPass(labels, counts, error);
    if (labels.synthetic) ++synthetic;
    if (ok) {
      ++passed;
    } else {
      ++failed;
      std::fprintf(stderr, "TRACE FAIL %s: %s\n", name.c_str(),
                   error.c_str());
    }
    std::printf(
        "FIXTURE %s stage=%s samples=%u MoveLeft=%u MoveRight=%u "
        "SoftDropOn=%u SoftDropOff=%u RotateCw=%u moveWhileDip=%u "
        "dropped=%u RESULT=%s\n",
        name.c_str(), labels.stage.empty() ? "?" : labels.stage.c_str(),
        counts.samples, actionCount(counts, ActionKind::MoveLeft),
        actionCount(counts, ActionKind::MoveRight),
        actionCount(counts, ActionKind::SoftDropOn),
        actionCount(counts, ActionKind::SoftDropOff),
        actionCount(counts, ActionKind::RotateCw), counts.moveWhileDip,
        counts.dropped, ok ? "PASS" : "FAIL");
  }
  std::printf("SUMMARY fixtures=%u synthetic=%u passed=%u failed=%u\n",
              passed + failed, synthetic, passed, failed);
  return failed == 0 ? 0 : 1;
}
