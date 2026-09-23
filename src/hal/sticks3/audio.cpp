#include "hal/sticks3/audio.h"

#include <M5Unified.h>

#include <cstddef>

namespace sf_hal {

bool Audio::begin() {
  const bool ok = M5.Speaker.begin();
  setVolumeCap(sf::kVolumeCapBattery);
  return ok;
}

void Audio::setVolumeCap(uint8_t v) {
  M5.Speaker.setVolume(v);
}

void Audio::play(sf::SfxId id, uint32_t nowMs) {
  const std::size_t cueIndex = static_cast<std::size_t>(id);
  if (cueIndex >= sf::kSfxCueCount) {
    ++dropped_;
    return;
  }
  if (hasTriggered_ && nowMs - lastTriggerMs_ < kMinTriggerGapMs) {
    ++dropped_;
    return;
  }

  cueId_ = id;
  noteIndex_ = 0;
  active_ = true;
  hasTriggered_ = true;
  lastTriggerMs_ = nowMs;
  startNote(sf::kSfxCues[cueIndex].notes[0], nowMs);
}

void Audio::update(uint32_t nowMs) {
  if (!active_) {
    return;
  }

  const sf::Cue& cue = sf::kSfxCues[static_cast<std::size_t>(cueId_)];
  const sf::Note& note = cue.notes[noteIndex_];
  if (nowMs - noteStartMs_ < note.ms) {
    return;
  }

  ++noteIndex_;
  if (noteIndex_ >= cue.count) {
    active_ = false;
    return;
  }
  startNote(cue.notes[noteIndex_], nowMs);
}

void Audio::startNote(const sf::Note& note, uint32_t nowMs) {
  noteStartMs_ = nowMs;
  M5.Speaker.tone(note.freqHz, note.ms, 0, true);
}

}  // namespace sf_hal
