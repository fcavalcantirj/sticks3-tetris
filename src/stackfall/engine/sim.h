#pragma once

#include <cstdint>

#include "stackfall/engine/game.h"

namespace sf {

enum class SimAction : uint8_t { L, R, CW, CCW, HD, SD, SU, HOLD };

// Pure deterministic driver shared by the host replay tool and the firmware
// parity check. It owns no filesystem, stream or dynamically allocated state.
class Sim {
 public:
  static constexpr uint16_t kTickMs = RuleProfile{}.tickMs;

  void begin(const RuleProfile& rules, uint32_t seed, uint32_t epochMs);
  void applyAction(SimAction action, uint32_t atTick);
  void stepTo(uint32_t tick);
  uint32_t hash() const;
  uint32_t tick() const { return tick_; }

 private:
  Game game_;
  uint32_t epochMs_ = 0;
  uint32_t tick_ = 0;
  uint16_t tickMs_ = kTickMs;
};

static_assert(Sim::kTickMs == 8, "replay v1 uses an 8 ms tick");

}  // namespace sf
