#pragma once

#include <cstdint>

#include "stackfall/app/blob.h"

namespace sf_hal {

class Store {
 public:
  bool begin();
  sf::DecodeStatus load(sf::Settings& out);
  sf::DecodeStatus load(sf::ScoreEntry out[sf::kScoreEntryCount]);
  bool save(const sf::Settings& settings);
  bool save(const sf::ScoreEntry scores[sf::kScoreEntryCount]);
  void wipe();
  uint32_t writes() const { return writes_; }

 private:
  void remember(const sf::Settings& settings);
  void remember(const sf::ScoreEntry scores[sf::kScoreEntryCount]);
  void defaultScores(sf::ScoreEntry out[sf::kScoreEntryCount]);

  sf::Settings memorySettings_{};
  sf::ScoreEntry memoryScores_[sf::kScoreEntryCount]{};
  uint32_t writes_ = 0;
  bool ready_ = false;
  bool haveSettings_ = false;
  bool haveScores_ = false;
};

}  // namespace sf_hal
