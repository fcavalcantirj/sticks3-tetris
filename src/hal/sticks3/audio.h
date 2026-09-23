#pragma once

#include <cstdint>

#include "stackfall/audio/sfx.h"

namespace sf_hal {

class Audio {
 public:
  bool begin();
  void setVolumeCap(uint8_t v);
  void play(sf::SfxId id, uint32_t nowMs);
  void update(uint32_t nowMs);
  uint32_t dropped() const { return dropped_; }

 private:
  static constexpr uint32_t kMinTriggerGapMs = 40;

  void startNote(const sf::Note& note, uint32_t nowMs);

  sf::SfxId cueId_ = sf::SfxId::Move;
  uint32_t noteStartMs_ = 0;
  uint32_t lastTriggerMs_ = 0;
  uint32_t dropped_ = 0;
  uint8_t noteIndex_ = 0;
  bool active_ = false;
  bool hasTriggered_ = false;
};

}  // namespace sf_hal
