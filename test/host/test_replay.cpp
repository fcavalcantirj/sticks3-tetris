#include <cstdint>
#include <sstream>

#include "framework.h"
#include "replay_fixture.h"
#include "stackfall/engine/statehash.h"

namespace {

constexpr const char* kGolden =
    "test/host/fixtures/replays/golden_endless_v1.replay";
constexpr const char* kRollover =
    "test/host/fixtures/replays/golden_endless_rollover_v1.replay";

sf::test::replay::ReplayFile load(const char* path) {
  sf::test::replay::ReplayFile replay;
  std::string error;
  if (!sf::test::replay::loadReplay(path, replay, error, true)) {
    sf::test::failWith(__FILE__, __LINE__, error.c_str());
  }
  return replay;
}

}  // namespace

SF_TEST(statehash_fnv1a_and_little_endian_helpers) {
  const uint8_t hello[] = {'h', 'e', 'l', 'l', 'o'};
  ASSERT_EQ(sf::fnv1a(hello, sizeof(hello)), 0x4F9F2CABu);

  const uint8_t bytes[] = {0xAB, 0x34, 0x12, 0xEF, 0xCD, 0xAB, 0x89};
  sf::StateHash hash;
  hash.u8(0xAB);
  hash.u16(0x1234);
  hash.u32(0x89ABCDEFu);
  ASSERT_EQ(hash.value(), sf::fnv1a(bytes, sizeof(bytes)));
}

SF_TEST(replay_rejects_unknown_version_header) {
  std::istringstream input("# stackfall-replay v2\n");
  sf::test::replay::ReplayFile replay;
  std::string error;
  ASSERT_FALSE(sf::test::replay::parseReplayStream(
      input, "bad_version.replay", replay, error, false));
  ASSERT_TRUE(error.find("bad_version.replay") != std::string::npos);
}

SF_TEST(replay_golden_repeats_in_one_process) {
  const auto replay = load(kGolden);
  const uint32_t first = sf::test::replay::runFinalHash(replay, replay.seed);
  const uint32_t second = sf::test::replay::runFinalHash(replay, replay.seed);
  ASSERT_EQ(first, second);
}

SF_TEST(replay_committed_trace_and_final_match) {
  const auto replay = load(kGolden);
  const auto result = sf::test::replay::compareReplay(replay);
  ASSERT_TRUE(result.ok);
  ASSERT_EQ(result.actual, replay.finalHash);
}

SF_TEST(replay_seed_changes_final_hash) {
  const auto replay = load(kGolden);
  const uint32_t seeded = sf::test::replay::runFinalHash(replay, replay.seed);
  const uint32_t other =
      sf::test::replay::runFinalHash(replay, 0x12345678u);
  ASSERT_NEQ(seeded, other);
}

SF_TEST(replay_rollover_matches_epoch_zero) {
  const auto base = load(kGolden);
  const auto rollover = load(kRollover);
  ASSERT_TRUE(base.totalTicks >= 3000u);
  ASSERT_EQ(base.totalTicks, rollover.totalTicks);
  ASSERT_EQ(base.seed, rollover.seed);
  ASSERT_EQ(base.mode, rollover.mode);
  ASSERT_EQ(base.wrap, rollover.wrap);
  ASSERT_EQ(base.tickMs, rollover.tickMs);
  ASSERT_EQ(base.epochMs, 0u);
  ASSERT_EQ(rollover.epochMs, 0xFFFFF000u);
  ASSERT_EQ(base.actions.size(), rollover.actions.size());
  for (size_t i = 0; i < base.actions.size(); ++i) {
    ASSERT_EQ(base.actions[i].tick, rollover.actions[i].tick);
    ASSERT_EQ(base.actions[i].action, rollover.actions[i].action);
  }
  const auto baseResult = sf::test::replay::compareReplay(base);
  const auto rolloverResult = sf::test::replay::compareReplay(rollover);
  ASSERT_TRUE(baseResult.ok);
  ASSERT_TRUE(rolloverResult.ok);
  ASSERT_EQ(baseResult.actual, rolloverResult.actual);
}
