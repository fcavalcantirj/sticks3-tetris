#include "stackfall/engine/sim.h"

#include "stackfall/engine/statehash.h"

namespace sf {

void Sim::begin(const RuleProfile& rules, uint32_t seed, uint32_t epochMs) {
  epochMs_ = epochMs;
  tick_ = 0;
  tickMs_ = rules.tickMs == 0 ? kTickMs : rules.tickMs;
  game_.reset(rules, seed, epochMs_);
}

void Sim::applyAction(SimAction action, uint32_t atTick) {
  stepTo(atTick);
  switch (action) {
    case SimAction::L:
      game_.move(-1);
      break;
    case SimAction::R:
      game_.move(1);
      break;
    case SimAction::CW:
      game_.rotate(Turn::CW);
      break;
    case SimAction::CCW:
      game_.rotate(Turn::CCW);
      break;
    case SimAction::HD:
      game_.hardDrop();
      break;
    case SimAction::SD:
      game_.setSoftDrop(true);
      break;
    case SimAction::SU:
      game_.setSoftDrop(false);
      break;
    case SimAction::HOLD:
      game_.hold();
      break;
  }
}

void Sim::stepTo(uint32_t targetTick) {
  while (tick_ < targetTick) {
    ++tick_;
    const uint32_t nowMs =
        epochMs_ + tick_ * static_cast<uint32_t>(tickMs_);
    game_.update(nowMs);
  }
}

uint32_t Sim::hash() const { return stateHash(game_); }

}  // namespace sf
