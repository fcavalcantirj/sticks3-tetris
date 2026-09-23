#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#include "stackfall/engine/sim.h"

namespace sf {
namespace test {
namespace replay {

struct ReplayAction {
  uint32_t tick;
  SimAction action;
};

struct HashPoint {
  uint32_t tick;
  uint32_t hash;
};

struct ReplayFile {
  std::string path;
  uint32_t seed = 0;
  GameMode mode = GameMode::Endless;
  bool wrap = false;
  uint32_t epochMs = 0;
  uint16_t tickMs = 0;
  std::vector<ReplayAction> actions;
  std::vector<HashPoint> hashes;
  uint32_t totalTicks = 0;
  uint32_t finalHash = 0;
};

struct ReplayResult {
  bool ok = false;
  uint32_t tick = 0;
  uint32_t expected = 0;
  uint32_t actual = 0;
};

struct RecordedRun {
  std::vector<HashPoint> hashes;
  uint32_t finalHash = 0;
};

inline std::string trim(const std::string& value) {
  const size_t first = value.find_first_not_of(" \t");
  if (first == std::string::npos) {
    return std::string();
  }
  const size_t last = value.find_last_not_of(" \t");
  return value.substr(first, last - first + 1);
}

inline bool parseU32(const std::string& token, uint32_t& value) {
  try {
    size_t used = 0;
    const unsigned long long parsed = std::stoull(token, &used, 0);
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

inline bool parseActionName(const std::string& name, SimAction& action) {
  if (name == "L") action = SimAction::L;
  else if (name == "R") action = SimAction::R;
  else if (name == "CW") action = SimAction::CW;
  else if (name == "CCW") action = SimAction::CCW;
  else if (name == "HD") action = SimAction::HD;
  else if (name == "SD") action = SimAction::SD;
  else if (name == "SU") action = SimAction::SU;
  else if (name == "HOLD") action = SimAction::HOLD;
  else return false;
  return true;
}

inline const char* actionName(SimAction action) {
  switch (action) {
    case SimAction::L: return "L";
    case SimAction::R: return "R";
    case SimAction::CW: return "CW";
    case SimAction::CCW: return "CCW";
    case SimAction::HD: return "HD";
    case SimAction::SD: return "SD";
    case SimAction::SU: return "SU";
    case SimAction::HOLD: return "HOLD";
  }
  return "?";
}

inline const char* modeName(GameMode mode) {
  switch (mode) {
    case GameMode::Endless: return "endless";
    case GameMode::FortyLine: return "fortyLine";
    case GameMode::ThreeMinute: return "threeMinute";
  }
  return "?";
}

inline bool parseReplayStream(std::istream& input, const std::string& path,
                              ReplayFile& replay, std::string& error,
                              bool requireGolden) {
  replay = ReplayFile{};
  replay.path = path;

  std::string line;
  if (!std::getline(input, line)) {
    error = path + ": empty replay";
    return false;
  }
  if (!line.empty() && line.back() == '\r') {
    line.pop_back();
  }
  if (line != "# stackfall-replay v1") {
    error = path + ": first line is not # stackfall-replay v1";
    return false;
  }

  bool haveSeed = false;
  bool haveMode = false;
  bool haveWrap = false;
  bool haveEpoch = false;
  bool haveTick = false;
  bool haveFinal = false;
  uint32_t lineNumber = 1;

  while (std::getline(input, line)) {
    ++lineNumber;
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    line = trim(line);
    if (line.empty() || line[0] == '#') {
      continue;
    }

    std::istringstream fields(line);
    std::string first;
    std::string second;
    std::string extra;
    fields >> first >> second;
    if (first.empty() || second.empty()) {
      error = path + ": malformed line " + std::to_string(lineNumber);
      return false;
    }

    auto noExtra = [&]() {
      fields >> extra;
      return extra.empty();
    };

    if (first == "seed") {
      if (haveSeed || !parseU32(second, replay.seed) || !noExtra()) {
        error = path + ": invalid seed line " + std::to_string(lineNumber);
        return false;
      }
      haveSeed = true;
      continue;
    }
    if (first == "mode") {
      if (haveMode || !noExtra()) {
        error = path + ": invalid mode line " + std::to_string(lineNumber);
        return false;
      }
      if (second == "endless") replay.mode = GameMode::Endless;
      else if (second == "fortyLine") replay.mode = GameMode::FortyLine;
      else if (second == "threeMinute") replay.mode = GameMode::ThreeMinute;
      else {
        error = path + ": unknown mode on line " +
                std::to_string(lineNumber);
        return false;
      }
      haveMode = true;
      continue;
    }
    if (first == "wrap") {
      uint32_t wrap = 0;
      if (haveWrap || !parseU32(second, wrap) || wrap > 1 || !noExtra()) {
        error = path + ": invalid wrap line " + std::to_string(lineNumber);
        return false;
      }
      replay.wrap = wrap != 0;
      haveWrap = true;
      continue;
    }
    if (first == "epoch") {
      if (haveEpoch || !parseU32(second, replay.epochMs) || !noExtra()) {
        error = path + ": invalid epoch line " + std::to_string(lineNumber);
        return false;
      }
      haveEpoch = true;
      continue;
    }
    if (first == "tick") {
      uint32_t tickMs = 0;
      if (haveTick || !parseU32(second, tickMs) ||
          tickMs != Sim::kTickMs || !noExtra()) {
        error = path + ": replay v1 tick must be 8 on line " +
                std::to_string(lineNumber);
        return false;
      }
      replay.tickMs = static_cast<uint16_t>(tickMs);
      haveTick = true;
      continue;
    }
    if (first == "hash") {
      std::string hashText;
      fields >> hashText;
      uint32_t hashTick = 0;
      uint32_t hash = 0;
      if (!parseU32(second, hashTick) || !parseU32(hashText, hash) ||
          !noExtra()) {
        error = path + ": invalid hash line " + std::to_string(lineNumber);
        return false;
      }
      replay.hashes.push_back(HashPoint{hashTick, hash});
      continue;
    }
    if (first == "final") {
      std::string hashText;
      fields >> hashText;
      if (haveFinal || !parseU32(second, replay.totalTicks) ||
          !parseU32(hashText, replay.finalHash) || !noExtra()) {
        error = path + ": invalid final line " + std::to_string(lineNumber);
        return false;
      }
      haveFinal = true;
      continue;
    }

    uint32_t actionTick = 0;
    SimAction action = SimAction::L;
    if (!parseU32(first, actionTick) || !parseActionName(second, action) ||
        !noExtra()) {
      error = path + ": invalid action line " + std::to_string(lineNumber);
      return false;
    }
    if (!replay.actions.empty() &&
        actionTick < replay.actions.back().tick) {
      error = path + ": actions are not sorted on line " +
              std::to_string(lineNumber);
      return false;
    }
    replay.actions.push_back(ReplayAction{actionTick, action});
  }

  if (!haveSeed || !haveMode || !haveWrap || !haveEpoch || !haveTick ||
      !haveFinal) {
    error = path + ": missing replay header or final line";
    return false;
  }
  if (!replay.actions.empty() &&
      replay.actions.back().tick > replay.totalTicks) {
    error = path + ": action occurs after final tick";
    return false;
  }

  if (requireGolden) {
    const size_t expectedPoints = replay.totalTicks / 100u;
    if (replay.hashes.size() != expectedPoints) {
      error = path + ": hash trace is not one line per 100 ticks";
      return false;
    }
    for (size_t i = 0; i < replay.hashes.size(); ++i) {
      const uint32_t expectedTick = static_cast<uint32_t>((i + 1u) * 100u);
      if (replay.hashes[i].tick != expectedTick) {
        error = path + ": hash trace tick sequence is invalid";
        return false;
      }
    }
  }

  return true;
}

inline bool loadReplay(const std::string& path, ReplayFile& replay,
                       std::string& error, bool requireGolden) {
  std::ifstream input(path);
  if (!input) {
    error = path + ": cannot open replay";
    return false;
  }
  return parseReplayStream(input, path, replay, error, requireGolden);
}

inline RuleProfile replayProfile(const ReplayFile& replay) {
  RuleProfile profile;
  switch (replay.mode) {
    case GameMode::Endless: profile = endless(); break;
    case GameMode::FortyLine: profile = fortyLine(); break;
    case GameMode::ThreeMinute: profile = threeMinute(); break;
  }
  profile.horizontalWrap = replay.wrap;
  profile.tickMs = replay.tickMs;
  return profile;
}

inline void applyThrough(Sim& sim, const ReplayFile& replay,
                         size_t& actionIndex, uint32_t targetTick) {
  while (actionIndex < replay.actions.size() &&
         replay.actions[actionIndex].tick <= targetTick) {
    const ReplayAction& action = replay.actions[actionIndex];
    sim.applyAction(action.action, action.tick);
    ++actionIndex;
  }
  sim.stepTo(targetTick);
}

inline RecordedRun recordRun(const ReplayFile& replay, uint32_t seed) {
  RecordedRun run;
  Sim sim;
  sim.begin(replayProfile(replay), seed, replay.epochMs);
  size_t actionIndex = 0;
  for (uint32_t tick = 100; tick <= replay.totalTicks; tick += 100) {
    applyThrough(sim, replay, actionIndex, tick);
    run.hashes.push_back(HashPoint{tick, sim.hash()});
    if (tick > std::numeric_limits<uint32_t>::max() - 100u) {
      break;
    }
  }
  applyThrough(sim, replay, actionIndex, replay.totalTicks);
  run.finalHash = sim.hash();
  return run;
}

inline uint32_t runFinalHash(const ReplayFile& replay, uint32_t seed) {
  Sim sim;
  sim.begin(replayProfile(replay), seed, replay.epochMs);
  size_t actionIndex = 0;
  applyThrough(sim, replay, actionIndex, replay.totalTicks);
  return sim.hash();
}

inline ReplayResult compareReplay(const ReplayFile& replay) {
  Sim sim;
  sim.begin(replayProfile(replay), replay.seed, replay.epochMs);
  size_t actionIndex = 0;
  for (const HashPoint& point : replay.hashes) {
    applyThrough(sim, replay, actionIndex, point.tick);
    const uint32_t actual = sim.hash();
    if (actual != point.hash) {
      return ReplayResult{false, point.tick, point.hash, actual};
    }
  }

  applyThrough(sim, replay, actionIndex, replay.totalTicks);
  const uint32_t actual = sim.hash();
  if (actual != replay.finalHash) {
    return ReplayResult{false, replay.totalTicks, replay.finalHash, actual};
  }
  return ReplayResult{true, replay.totalTicks, replay.finalHash, actual};
}

inline int compareReplayFile(const std::string& path, std::FILE* output) {
  ReplayFile replay;
  std::string error;
  if (!loadReplay(path, replay, error, true)) {
    std::fprintf(output, "REPLAY ERROR %s\n", error.c_str());
    return 2;
  }
  const ReplayResult result = compareReplay(replay);
  if (!result.ok) {
    std::fprintf(output,
                 "SIM MISMATCH %s tick=%u expected=0x%08x actual=0x%08x\n",
                 path.c_str(), static_cast<unsigned>(result.tick),
                 static_cast<unsigned>(result.expected),
                 static_cast<unsigned>(result.actual));
    return 1;
  }
  std::fprintf(output, "SIM OK %s final=0x%08x\n", path.c_str(),
               static_cast<unsigned>(result.actual));
  return 0;
}

}  // namespace replay
}  // namespace test
}  // namespace sf
