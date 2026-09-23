#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <string>

#include "replay_fixture.h"
#include "stackfall/core/bag.h"
#include "stackfall/engine/game.h"
#include "stackfall/engine/statehash.h"

namespace {

using sf::ControlProfile;
using sf::Game;
using sf::GameStatus;
using sf::RuleProfile;
using sf::Turn;
using sf::XorShift32;
using sf::test::replay::ReplayFile;

int usage() {
  std::fprintf(
      stderr,
      "usage: sf_sim <file>\n"
      "       sf_sim --record <file>\n"
      "       sf_sim --print-tag <file>\n"
      "       sf_sim --soak-minutes <n> --seed <hex> --profile "
      "<TILT_DIP|BUTTONS_ONLY>\n");
  return 2;
}

int recordReplay(const std::string& path) {
  ReplayFile replay;
  std::string error;
  if (!sf::test::replay::loadReplay(path, replay, error, false)) {
    std::fprintf(stderr, "REPLAY ERROR %s\n", error.c_str());
    return 2;
  }

  const sf::test::replay::RecordedRun run =
      sf::test::replay::recordRun(replay, replay.seed);

  // This is deliberately the harness's only .replay write path. Comparison
  // never opens an output stream and can never auto-bless a mismatch.
  std::ofstream out(path, std::ios::trunc);
  if (!out) {
    std::fprintf(stderr, "REPLAY ERROR %s: cannot write replay\n",
                 path.c_str());
    return 2;
  }

  out << "# stackfall-replay v1\n";
  out << "seed 0x" << std::hex << std::nouppercase << std::setfill('0')
      << std::setw(8) << replay.seed << std::dec << "\n";
  out << "mode " << sf::test::replay::modeName(replay.mode) << "\n";
  out << "wrap " << (replay.wrap ? 1 : 0) << "\n";
  if (replay.epochMs == 0) {
    out << "epoch 0\n";
  } else {
    out << "epoch 0x" << std::hex << std::nouppercase << std::setfill('0')
        << std::setw(8) << replay.epochMs << std::dec << "\n";
  }
  out << "tick " << replay.tickMs << "\n\n";
  for (const sf::test::replay::ReplayAction& action : replay.actions) {
    out << action.tick << ' '
        << sf::test::replay::actionName(action.action) << "\n";
  }
  out << "\n";
  for (const sf::test::replay::HashPoint& point : run.hashes) {
    out << "hash " << std::dec << point.tick << " 0x" << std::hex
        << std::nouppercase << std::setfill('0') << std::setw(8) << point.hash
        << std::dec << "\n";
  }
  out << "final " << replay.totalTicks << " 0x" << std::hex
      << std::nouppercase << std::setfill('0') << std::setw(8)
      << run.finalHash << std::dec << "\n";
  out.close();
  if (!out) {
    std::fprintf(stderr, "REPLAY ERROR %s: write failed\n", path.c_str());
    return 2;
  }

  std::printf("RECORDED %s final=0x%08x\n", path.c_str(),
              static_cast<unsigned>(run.finalHash));
  return 0;
}

int printTag(const std::string& path) {
  ReplayFile replay;
  std::string error;
  if (!sf::test::replay::loadReplay(path, replay, error, true)) {
    std::fprintf(stderr, "REPLAY ERROR %s\n", error.c_str());
    return 2;
  }
  const sf::test::replay::ReplayResult result =
      sf::test::replay::compareReplay(replay);
  if (!result.ok) {
    std::fprintf(stderr,
                 "SIM MISMATCH %s tick=%u expected=0x%08x actual=0x%08x\n",
                 path.c_str(), static_cast<unsigned>(result.tick),
                 static_cast<unsigned>(result.expected),
                 static_cast<unsigned>(result.actual));
    return 1;
  }

  std::string base = std::filesystem::path(path).filename().string();
  const size_t underscore = base.find('_');
  if (underscore != std::string::npos) {
    base.resize(underscore);
  } else {
    const size_t dot = base.find('.');
    if (dot != std::string::npos) base.resize(dot);
  }
  std::printf("[REPLAY] name=%s ticks=%u seed=0x%08x hash=0x%08x\n",
              base.c_str(), static_cast<unsigned>(replay.totalTicks),
              static_cast<unsigned>(replay.seed),
              static_cast<unsigned>(result.actual));
  return 0;
}

void collectLines(const Game& game, uint32_t& seen, uint32_t& total) {
  const uint32_t current = game.lines();
  if (current >= seen) {
    total += current - seen;
  }
  seen = current;
}

int soak(uint32_t minutes, uint32_t seed, const std::string& profileName) {
  ControlProfile control;
  if (profileName == "TILT_DIP") {
    control = ControlProfile::TILT_DIP;
  } else if (profileName == "BUTTONS_ONLY") {
    control = ControlProfile::BUTTONS_ONLY;
  } else {
    std::fprintf(stderr, "unknown profile: %s\n", profileName.c_str());
    return 2;
  }

  const uint64_t totalTicks64 =
      static_cast<uint64_t>(minutes) * 60000u / sf::Sim::kTickMs;
  if (totalTicks64 > std::numeric_limits<uint32_t>::max()) {
    std::fprintf(stderr, "soak duration is too large\n");
    return 2;
  }
  const uint32_t totalTicks = static_cast<uint32_t>(totalTicks64);

  RuleProfile profile = sf::withControlProfile(sf::endless(), control);
  Game game;
  game.reset(profile, seed, 0);
  XorShift32 player(seed ^ 0xA5A5A5A5u);
  const uint32_t firstHash = sf::stateHash(game);
  uint32_t pieces = 0;
  uint32_t totalLines = 0;
  uint32_t seenLines = 0;

  for (uint32_t tick = 1; tick <= totalTicks; ++tick) {
    game.update(tick * static_cast<uint32_t>(sf::Sim::kTickMs));
    collectLines(game, seenLines, totalLines);

    if (game.status() != GameStatus::Playing) {
      game.reset(profile, player.next(),
                 tick * static_cast<uint32_t>(sf::Sim::kTickMs));
      seenLines = 0;
      continue;
    }

    if (tick % 25u != 0u) {
      continue;
    }

    const uint32_t choice = player.next();
    const uint8_t turns = static_cast<uint8_t>(choice & 3u);
    for (uint8_t i = 0; i < turns; ++i) {
      game.rotate(Turn::CW);
    }
    const int target = static_cast<int>((choice >> 8) % 10u);
    for (int i = 0; i < 10 && game.active().col < target; ++i) {
      if (!game.move(1)) break;
    }
    for (int i = 0; i < 10 && game.active().col > target; ++i) {
      if (!game.move(-1)) break;
    }
    game.hardDrop();
    ++pieces;
    collectLines(game, seenLines, totalLines);
  }

  const uint32_t lastHash = sf::stateHash(game);
  std::printf(
      "SOAK ticks=%u pieces=%u lines=%u hash_first=0x%08x "
      "hash_last=0x%08x dropped=%u\n",
      static_cast<unsigned>(totalTicks), static_cast<unsigned>(pieces),
      static_cast<unsigned>(totalLines), static_cast<unsigned>(firstHash),
      static_cast<unsigned>(lastHash),
      static_cast<unsigned>(game.events().dropped()));
  return 0;
}

int soakFromArgs(int argc, char** argv) {
  uint32_t minutes = 0;
  uint32_t seed = 0;
  std::string profile;
  bool haveMinutes = false;
  bool haveSeed = false;
  bool haveProfile = false;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--soak-minutes") == 0 && i + 1 < argc) {
      haveMinutes = sf::test::replay::parseU32(argv[++i], minutes);
    } else if (std::strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
      haveSeed = sf::test::replay::parseU32(argv[++i], seed);
    } else if (std::strcmp(argv[i], "--profile") == 0 && i + 1 < argc) {
      profile = argv[++i];
      haveProfile = true;
    } else {
      return usage();
    }
  }
  if (!haveMinutes || !haveSeed || !haveProfile || minutes == 0) {
    return usage();
  }
  return soak(minutes, seed, profile);
}

}  // namespace

int main(int argc, char** argv) {
  if (argc == 2 && argv[1][0] != '-') {
    return sf::test::replay::compareReplayFile(argv[1], stdout);
  }
  if (argc == 3 && std::strcmp(argv[1], "--record") == 0) {
    return recordReplay(argv[2]);
  }
  if (argc == 3 && std::strcmp(argv[1], "--print-tag") == 0) {
    return printTag(argv[2]);
  }
  if (argc >= 2 && std::strcmp(argv[1], "--soak-minutes") == 0) {
    return soakFromArgs(argc, argv);
  }
  return usage();
}
