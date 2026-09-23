#include "hal/sticks3/store.h"

#include <Arduino.h>
#include <Preferences.h>

namespace {

Preferences prefs_;

constexpr char kStoreNamespace[] = "stackfall";
constexpr char kSettingsKey[] = "settings";
constexpr char kScoresKey[] = "scores";

void printStore(const char* operation, uint8_t schema, size_t bytes,
                bool ok) {
  Serial.printf("[NVS] op=%s schema=%u bytes=%u ok=%u\n", operation,
                static_cast<unsigned>(schema),
                static_cast<unsigned>(bytes), ok ? 1u : 0u);
}

}  // namespace

namespace sf_hal {

bool Store::begin() {
  ready_ = prefs_.begin(kStoreNamespace, false);
  if (!ready_) {
    Serial.printf("[ERR] where=store_begin code=1\n");
  }
  return ready_;
}

sf::DecodeStatus Store::load(sf::Settings& out) {
  if (!ready_) {
    if (haveSettings_) {
      out = memorySettings_;
      printStore("load", sf::kBlobVersion, sf::kSettingsBlobBytes, true);
    } else {
      out = sf::Settings{};
      remember(out);
      printStore("default", sf::kBlobVersion, 0, true);
    }
    return sf::DecodeStatus::Ok;
  }

  const size_t storedBytes = prefs_.getBytesLength(kSettingsKey);
  if (storedBytes == 0) {
    out = sf::Settings{};
    remember(out);
    printStore("default", sf::kBlobVersion, 0, true);
    return sf::DecodeStatus::Ok;
  }
  if (storedBytes > sf::kMaxBlobBytes) {
    out = sf::Settings{};
    remember(out);
    printStore("load", sf::kBlobVersion, storedBytes, false);
    printStore("default", sf::kBlobVersion, 0, true);
    return sf::DecodeStatus::Short;
  }

  uint8_t bytes[sf::kMaxBlobBytes]{};
  const size_t readBytes =
      prefs_.getBytes(kSettingsKey, bytes, sizeof(bytes));
  const sf::DecodeStatus status = sf::decode(bytes, readBytes, out);
  if (status == sf::DecodeStatus::Ok) {
    remember(out);
    printStore("load", sf::kBlobVersion, readBytes, true);
    return status;
  }
  if (status == sf::DecodeStatus::Migrated) {
    remember(out);
    printStore("migrate", 0, readBytes, true);
    save(out);
    return status;
  }

  remember(out);
  printStore("load", sf::kBlobVersion, readBytes, false);
  printStore("default", sf::kBlobVersion, 0, true);
  return status;
}

sf::DecodeStatus Store::load(
    sf::ScoreEntry out[sf::kScoreEntryCount]) {
  if (!ready_) {
    if (haveScores_) {
      for (size_t i = 0; i < sf::kScoreEntryCount; ++i) {
        out[i] = memoryScores_[i];
      }
      printStore("load", sf::kBlobVersion, sf::kScoresBlobBytes, true);
    } else {
      defaultScores(out);
      remember(out);
      printStore("default", sf::kBlobVersion, 0, true);
    }
    return sf::DecodeStatus::Ok;
  }

  const size_t storedBytes = prefs_.getBytesLength(kScoresKey);
  if (storedBytes == 0) {
    defaultScores(out);
    remember(out);
    printStore("default", sf::kBlobVersion, 0, true);
    return sf::DecodeStatus::Ok;
  }
  if (storedBytes > sf::kMaxBlobBytes) {
    defaultScores(out);
    remember(out);
    printStore("load", sf::kBlobVersion, storedBytes, false);
    printStore("default", sf::kBlobVersion, 0, true);
    return sf::DecodeStatus::Short;
  }

  uint8_t bytes[sf::kMaxBlobBytes]{};
  const size_t readBytes = prefs_.getBytes(kScoresKey, bytes, sizeof(bytes));
  const sf::DecodeStatus status = sf::decode(bytes, readBytes, out);
  if (status == sf::DecodeStatus::Ok) {
    remember(out);
    printStore("load", sf::kBlobVersion, readBytes, true);
    return status;
  }
  if (status == sf::DecodeStatus::Migrated) {
    remember(out);
    printStore("migrate", 0, readBytes, true);
    save(out);
    return status;
  }

  remember(out);
  printStore("load", sf::kBlobVersion, readBytes, false);
  printStore("default", sf::kBlobVersion, 0, true);
  return status;
}

bool Store::save(const sf::Settings& settings) {
  uint8_t bytes[sf::kMaxBlobBytes]{};
  const size_t encoded = sf::encode(settings, bytes, sizeof(bytes));
  remember(settings);

  bool ok = encoded == sf::kSettingsBlobBytes;
  if (ok && ready_) {
    ok = prefs_.putBytes(kSettingsKey, bytes, encoded) == encoded;
  }
  if (ok) {
    ++writes_;
  }
  printStore("save", sf::kBlobVersion, encoded, ok);
  return ok;
}

bool Store::save(const sf::ScoreEntry scores[sf::kScoreEntryCount]) {
  uint8_t bytes[sf::kMaxBlobBytes]{};
  const size_t encoded = sf::encode(scores, bytes, sizeof(bytes));
  remember(scores);

  bool ok = encoded == sf::kScoresBlobBytes;
  if (ok && ready_) {
    ok = prefs_.putBytes(kScoresKey, bytes, encoded) == encoded;
  }
  if (ok) {
    ++writes_;
  }
  printStore("save", sf::kBlobVersion, encoded, ok);
  return ok;
}

void Store::wipe() {
  if (ready_) {
    prefs_.clear();
  }
  memorySettings_ = sf::Settings{};
  defaultScores(memoryScores_);
  haveSettings_ = false;
  haveScores_ = false;
}

void Store::remember(const sf::Settings& settings) {
  memorySettings_ = settings;
  haveSettings_ = true;
}

void Store::remember(const sf::ScoreEntry scores[sf::kScoreEntryCount]) {
  for (size_t i = 0; i < sf::kScoreEntryCount; ++i) {
    memoryScores_[i] = scores[i];
  }
  haveScores_ = true;
}

void Store::defaultScores(sf::ScoreEntry out[sf::kScoreEntryCount]) {
  for (size_t i = 0; i < sf::kScoreEntryCount; ++i) {
    out[i] = sf::ScoreEntry{};
  }
}

}  // namespace sf_hal
