#include "framework.h"
#include "stackfall/core/bag.h"
#include "stackfall/input/router.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <vector>

using sf::ControlProfile;
using sf::Screen;
using sf::XorShift32;
using sf::input::ActionKind;
using sf::input::ActionQueue;
using sf::input::GameAction;
using sf::input::InputRouter;

namespace {

constexpr uint32_t kPollMs = 5;
constexpr uint32_t kTickMs = 8;
constexpr uint32_t kEventCount = 10000;
constexpr uint32_t kP95Ticks = 2;
constexpr uint32_t kMaxTicks = 3;
constexpr uint32_t kMeanTicksX100 = 120;
constexpr uint32_t kSeed = 0x9E3779B9u;
// Source of truth: src/stackfall/input/buttons.h.
constexpr uint32_t kHoldMs = 900;
constexpr uint32_t kChordMs = 800;
constexpr uint32_t kSideHardDropMs = 900;
constexpr uint32_t kNoTime = std::numeric_limits<uint32_t>::max();

enum class Pattern : uint8_t { Blue, Side, BlueFirst, SideFirst };

struct Stimulus {
  Pattern pattern = Pattern::Blue;
  Screen screen = Screen::Playing;
  uint32_t startMs = 0;
  uint32_t blueDownMs = kNoTime;
  uint32_t blueUpMs = kNoTime;
  uint32_t sideDownMs = kNoTime;
  uint32_t sideUpMs = kNoTime;

  bool blueAt(uint32_t nowMs) const {
    return blueDownMs != kNoTime && nowMs >= blueDownMs && nowMs < blueUpMs;
  }

  bool sideAt(uint32_t nowMs) const {
    return sideDownMs != kNoTime && nowMs >= sideDownMs && nowMs < sideUpMs;
  }

  uint32_t endMs() const {
    if (blueUpMs == kNoTime) {
      return sideUpMs;
    }
    if (sideUpMs == kNoTime) {
      return blueUpMs;
    }
    return std::max(blueUpMs, sideUpMs);
  }
};

struct Stream {
  std::vector<Stimulus> stimuli;
  uint32_t blueFirstCount = 0;
};

struct Metrics {
  std::vector<uint32_t> latencyTicks;
  uint32_t actions = 0;
  uint32_t p50 = 0;
  uint32_t p95 = 0;
  uint32_t max = 0;
  uint32_t meanX100 = 0;
  uint16_t dropped = 0;
};

uint32_t quantizeToPoll(uint32_t valueMs) {
  return ((valueMs + kPollMs - 1u) / kPollMs) * kPollMs;
}

uint32_t uniformMs(XorShift32& rng, uint32_t first, uint32_t last) {
  return first + rng.next() % (last - first + 1u);
}

uint32_t pressDuration(XorShift32& rng) {
  return quantizeToPoll(uniformMs(rng, 1, 1200));
}

uint32_t interEventGap(XorShift32& rng) {
  return quantizeToPoll(uniformMs(rng, 5, 500));
}

Stream makeStream() {
  Stream stream;
  stream.stimuli.reserve(kEventCount);
  XorShift32 rng(kSeed);
  uint32_t cursorMs = 0;

  for (uint32_t index = 0; index < kEventCount; ++index) {
    Stimulus stimulus;
    stimulus.startMs = cursorMs + interEventGap(rng);

    const uint32_t chance = rng.next() % 100u;
    if (chance < 12u || (chance >= 12u && chance < 15u)) {
      const bool blueFirst = chance < 12u;
      stimulus.pattern = blueFirst ? Pattern::BlueFirst : Pattern::SideFirst;
      stimulus.screen = Screen::Playing;

      const uint32_t leadMs = (1u + rng.next() % 20u) * kPollMs;
      uint32_t firstDurationMs = 0;
      uint32_t secondDurationMs = 0;
      do {
        firstDurationMs = pressDuration(rng);
        secondDurationMs = pressDuration(rng);
      } while (firstDurationMs < leadMs + kChordMs + kPollMs ||
               secondDurationMs < kChordMs + kPollMs);

      if (blueFirst) {
        stimulus.blueDownMs = stimulus.startMs;
        stimulus.blueUpMs = stimulus.startMs + firstDurationMs;
        stimulus.sideDownMs = stimulus.startMs + leadMs;
        stimulus.sideUpMs = stimulus.sideDownMs + secondDurationMs;
        ++stream.blueFirstCount;
      } else {
        stimulus.sideDownMs = stimulus.startMs;
        stimulus.sideUpMs = stimulus.startMs + firstDurationMs;
        stimulus.blueDownMs = stimulus.startMs + leadMs;
        stimulus.blueUpMs = stimulus.blueDownMs + secondDurationMs;
      }
    } else {
      const bool blue = (rng.next() & 1u) != 0;
      stimulus.pattern = blue ? Pattern::Blue : Pattern::Side;
      // A small set of blue-only presses exercises the Confirm edge without
      // turning BUTTONS_ONLY side repeats into menu repeats.
      stimulus.screen = blue && rng.next() % 20u == 0u ? Screen::Title
                                                       : Screen::Playing;
      const uint32_t durationMs = pressDuration(rng);
      if (blue) {
        stimulus.blueDownMs = stimulus.startMs;
        stimulus.blueUpMs = stimulus.startMs + durationMs;
      } else {
        stimulus.sideDownMs = stimulus.startMs;
        stimulus.sideUpMs = stimulus.startMs + durationMs;
      }
    }

    cursorMs = stimulus.endMs();
    stream.stimuli.push_back(stimulus);
  }
  return stream;
}

const Stimulus* stimulusAt(const Stream& stream, uint32_t nowMs) {
  const auto after = std::upper_bound(
      stream.stimuli.begin(), stream.stimuli.end(), nowMs,
      [](uint32_t time, const Stimulus& stimulus) {
        return time < stimulus.startMs;
      });
  if (after == stream.stimuli.begin()) {
    return nullptr;
  }
  const Stimulus& candidate = *(after - 1);
  return nowMs < candidate.endMs() ? &candidate : nullptr;
}

uint32_t referenceMs(const Stimulus& stimulus, const GameAction& action,
                     ControlProfile profile) {
  switch (action.kind) {
    case ActionKind::RotateCw:
    case ActionKind::RotateCcw:
    case ActionKind::Confirm:
      ASSERT_TRUE(stimulus.blueDownMs != kNoTime);
      return stimulus.blueDownMs;
    case ActionKind::HardDrop:
      ASSERT_TRUE(stimulus.sideDownMs != kNoTime);
      if (profile == ControlProfile::TILT_DIP ||
          action.tMs == stimulus.sideDownMs) {
        return stimulus.sideDownMs;
      }
      return stimulus.sideDownMs + kSideHardDropMs;
    case ActionKind::MoveRight:
    case ActionKind::MenuDown:
      ASSERT_TRUE(stimulus.sideDownMs != kNoTime);
      // Repeats have no physical edge or threshold reference in the contract;
      // their first observed poll is the reference for the engine-consumer
      // budget. The press-edge MoveRight still uses the physical edge.
      return action.tMs == stimulus.sideDownMs ? stimulus.sideDownMs
                                               : action.tMs;
    case ActionKind::Hold:
      ASSERT_TRUE(stimulus.blueDownMs != kNoTime);
      return stimulus.blueDownMs + kHoldMs;
    case ActionKind::Pause:
    case ActionKind::Back:
    case ActionKind::Diag:
      ASSERT_TRUE(stimulus.blueDownMs != kNoTime);
      ASSERT_TRUE(stimulus.sideDownMs != kNoTime);
      return std::max(stimulus.blueDownMs, stimulus.sideDownMs) + kChordMs;
    default:
      ::sf::test::failWith(__FILE__, __LINE__,
                           "latency stream emitted an unexpected action");
      return action.tMs;
  }
}

uint32_t originatingEdgeMs(const Stimulus& stimulus,
                           const GameAction& action) {
  switch (action.kind) {
    case ActionKind::RotateCw:
    case ActionKind::RotateCcw:
    case ActionKind::Confirm:
    case ActionKind::Hold:
      return stimulus.blueDownMs;
    case ActionKind::HardDrop:
    case ActionKind::MoveRight:
    case ActionKind::MenuDown:
      return stimulus.sideDownMs;
    case ActionKind::Pause:
    case ActionKind::Back:
    case ActionKind::Diag:
      return std::max(stimulus.blueDownMs, stimulus.sideDownMs);
    default:
      ::sf::test::failWith(__FILE__, __LINE__,
                           "latency stream emitted an unexpected action");
      return action.tMs;
  }
}

void finishMetrics(Metrics& metrics) {
  ASSERT_TRUE(!metrics.latencyTicks.empty());
  std::sort(metrics.latencyTicks.begin(), metrics.latencyTicks.end());
  const std::size_t count = metrics.latencyTicks.size();
  metrics.p50 = metrics.latencyTicks[(count * 50u) / 100u];
  metrics.p95 = metrics.latencyTicks[(count * 95u) / 100u];
  metrics.max = metrics.latencyTicks.back();

  uint64_t total = 0;
  for (uint32_t ticks : metrics.latencyTicks) {
    total += ticks;
  }
  metrics.meanX100 =
      static_cast<uint32_t>((total * 100u + count - 1u) / count);
}

Metrics runProfile(const Stream& stream, ControlProfile profile) {
  InputRouter router;
  router.setProfile(profile);
  ActionQueue queue;
  Metrics metrics;
  metrics.latencyTicks.reserve(kEventCount * 4u);
  std::vector<uint32_t> suppressedSideEdges;
  suppressedSideEdges.reserve(stream.blueFirstCount);

  std::size_t stimulusIndex = 0;
  bool previousBlue = false;
  bool previousSide = false;
  uint32_t rotateCount = 0;
  uint32_t hardDropCount = 0;
  uint32_t moveRightCount = 0;
  uint32_t confirmCount = 0;
  uint32_t holdCount = 0;
  uint32_t pauseCount = 0;
  uint32_t suppressedHardDrops = 0;
  const uint32_t finalMs = stream.stimuli.back().endMs() + kTickMs;

  for (uint32_t nowMs = 0; nowMs <= finalMs; ++nowMs) {
    if (nowMs % kPollMs == 0u) {
      while (stimulusIndex < stream.stimuli.size() &&
             nowMs >= stream.stimuli[stimulusIndex].endMs()) {
        ++stimulusIndex;
      }

      const Stimulus* active = nullptr;
      if (stimulusIndex < stream.stimuli.size() &&
          nowMs >= stream.stimuli[stimulusIndex].startMs) {
        active = &stream.stimuli[stimulusIndex];
      }
      const bool blue = active != nullptr && active->blueAt(nowMs);
      const bool side = active != nullptr && active->sideAt(nowMs);
      const Screen screen = active == nullptr ? Screen::Playing : active->screen;

      if (side && !previousSide && previousBlue) {
        suppressedSideEdges.push_back(nowMs);
      }

      router.update(blue, side, 0.0f, 0.0f, screen, nowMs, queue);
      previousBlue = blue;
      previousSide = side;
    }

    // Input is produced on 5 ms polls and observed by the 8 ms engine tick.
    // Polling happens first when the schedules coincide, allowing zero ticks.
    if (nowMs % kTickMs == 0u) {
      GameAction action{};
      while (queue.pop(action)) {
        const Stimulus* source = stimulusAt(stream, action.tMs);
        ASSERT_TRUE(source != nullptr);
        const uint32_t reference = referenceMs(*source, action, profile);
        const uint32_t edge = originatingEdgeMs(*source, action);
        ASSERT_TRUE(edge != kNoTime);
        ASSERT_TRUE(action.tMs >= edge);
        ASSERT_TRUE(action.tMs >= reference);
        ASSERT_TRUE(nowMs >= action.tMs);
        const uint32_t latencyMs = nowMs - reference;
        metrics.latencyTicks.push_back(
            (latencyMs + kPollMs - 1u) / kPollMs);
        ++metrics.actions;

        switch (action.kind) {
          case ActionKind::RotateCw:
          case ActionKind::RotateCcw:
            ++rotateCount;
            break;
          case ActionKind::HardDrop:
            ++hardDropCount;
            if (std::binary_search(suppressedSideEdges.begin(),
                                   suppressedSideEdges.end(), action.tMs)) {
              ++suppressedHardDrops;
            }
            break;
          case ActionKind::MoveRight:
            ++moveRightCount;
            break;
          case ActionKind::Confirm:
            ++confirmCount;
            break;
          case ActionKind::Hold:
            ++holdCount;
            break;
          case ActionKind::Pause:
            ++pauseCount;
            break;
          default:
            break;
        }
      }
    }
  }

  metrics.dropped = queue.dropped();
  finishMetrics(metrics);

  ASSERT_TRUE(metrics.actions >= kEventCount);
  ASSERT_EQ(suppressedSideEdges.size(),
            static_cast<std::size_t>(stream.blueFirstCount));
  ASSERT_EQ(suppressedHardDrops, 0u);
  ASSERT_EQ(metrics.dropped, 0u);
  ASSERT_TRUE(rotateCount > 0u);
  ASSERT_TRUE(hardDropCount > 0u);
  ASSERT_TRUE(confirmCount > 0u);
  ASSERT_TRUE(holdCount > 0u);
  ASSERT_TRUE(pauseCount > 0u);
  if (profile == ControlProfile::BUTTONS_ONLY) {
    ASSERT_TRUE(moveRightCount > 0u);
  } else {
    ASSERT_EQ(moveRightCount, 0u);
  }
  ASSERT_TRUE(metrics.p95 <= kP95Ticks);
  ASSERT_TRUE(metrics.max <= kMaxTicks);
  ASSERT_TRUE(metrics.meanX100 <= kMeanTicksX100);
  return metrics;
}

void printMetrics(const char* profile, const Metrics& metrics) {
  std::printf(
      "LATENCY profile=%s events=%u actions=%u p50=%u p95=%u max=%u "
      "dropped=%u\n",
      profile, static_cast<unsigned>(kEventCount),
      static_cast<unsigned>(metrics.actions), static_cast<unsigned>(metrics.p50),
      static_cast<unsigned>(metrics.p95), static_cast<unsigned>(metrics.max),
      static_cast<unsigned>(metrics.dropped));
}

}  // namespace

SF_TEST(input_latency_budget_randomized_sweep) {
  const Stream stream = makeStream();
  ASSERT_EQ(stream.stimuli.size(), static_cast<std::size_t>(kEventCount));
  ASSERT_TRUE(stream.blueFirstCount > 0u);

  const Metrics tilt = runProfile(stream, ControlProfile::TILT_DIP);
  const Metrics buttons = runProfile(stream, ControlProfile::BUTTONS_ONLY);
  printMetrics("TILT_DIP", tilt);
  printMetrics("BUTTONS_ONLY", buttons);

  // The control-map correction leaves exactly two real profiles. Keep the
  // ledger's three-record grep useful with an explicitly labelled aggregate,
  // rather than reviving either retired profile as a fictional runtime mode.
  Metrics combined;
  combined.actions = tilt.actions + buttons.actions;
  combined.dropped = static_cast<uint16_t>(tilt.dropped + buttons.dropped);
  combined.latencyTicks = tilt.latencyTicks;
  combined.latencyTicks.insert(combined.latencyTicks.end(),
                               buttons.latencyTicks.begin(),
                               buttons.latencyTicks.end());
  finishMetrics(combined);
  std::printf(
      "LATENCY summary=ALL profiles=2 events=%u actions=%u p50=%u p95=%u "
      "max=%u dropped=%u\n",
      static_cast<unsigned>(kEventCount * 2u),
      static_cast<unsigned>(combined.actions),
      static_cast<unsigned>(combined.p50),
      static_cast<unsigned>(combined.p95),
      static_cast<unsigned>(combined.max),
      static_cast<unsigned>(combined.dropped));
}
