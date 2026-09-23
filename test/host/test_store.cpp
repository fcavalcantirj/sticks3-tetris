#include <cstddef>
#include <cstdint>
#include <cstring>

#include "framework.h"
#include "stackfall/app/blob.h"

namespace {

struct ScoreTable {
  sf::ScoreEntry rows[sf::kScoreEntryCount];
};

sf::Settings variedSettings() {
  sf::Settings settings;
  settings.controlProfile = 1;
  settings.rotateCw = false;
  settings.tiltSensitivity = 4;
  settings.brightness = 1;
  settings.volume = 3;
  settings.ghostEnabled = false;
  settings.startLevel = 12;
  settings.mode = 2;
  return settings;
}

bool sameSettings(const sf::Settings& lhs, const sf::Settings& rhs) {
  return lhs.controlProfile == rhs.controlProfile &&
         lhs.rotateCw == rhs.rotateCw &&
         lhs.tiltSensitivity == rhs.tiltSensitivity &&
         lhs.brightness == rhs.brightness && lhs.volume == rhs.volume &&
         lhs.ghostEnabled == rhs.ghostEnabled &&
         lhs.startLevel == rhs.startLevel && lhs.mode == rhs.mode;
}

ScoreTable variedScores() {
  ScoreTable table{};
  for (size_t i = 0; i < sf::kScoreEntryCount; ++i) {
    table.rows[i] = sf::ScoreEntry{
        static_cast<uint32_t>(100003u + i * 7919u),
        static_cast<uint16_t>(17u + i * 29u),
        static_cast<uint8_t>(1u + i),
        static_cast<uint8_t>(i / 5u),
    };
  }
  return table;
}

bool sameScore(const sf::ScoreEntry& lhs, const sf::ScoreEntry& rhs) {
  return lhs.score == rhs.score && lhs.lines == rhs.lines &&
         lhs.level == rhs.level && lhs.mode == rhs.mode;
}

void putU16(uint8_t* out, size_t& pos, uint16_t value) {
  out[pos++] = static_cast<uint8_t>(value);
  out[pos++] = static_cast<uint8_t>(value >> 8u);
}

void putU32(uint8_t* out, size_t& pos, uint32_t value) {
  out[pos++] = static_cast<uint8_t>(value);
  out[pos++] = static_cast<uint8_t>(value >> 8u);
  out[pos++] = static_cast<uint8_t>(value >> 16u);
  out[pos++] = static_cast<uint8_t>(value >> 24u);
}

void assertDefaultSettings(const sf::Settings& settings) {
  const sf::Settings defaults;
  ASSERT_TRUE(sameSettings(settings, defaults));
  ASSERT_TRUE(settings.rotateCw);
  ASSERT_EQ(settings.tiltSensitivity, 2u);
  ASSERT_EQ(settings.brightness, 3u);
  ASSERT_EQ(settings.volume, 2u);
  ASSERT_TRUE(settings.ghostEnabled);
  ASSERT_EQ(settings.startLevel, 1u);
  ASSERT_EQ(settings.mode, 0u);
}

}  // namespace

SF_TEST(store_crc32_iso_hdlc_reference) {
  const uint8_t text[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
  ASSERT_EQ(sf::crc32(text, sizeof(text)), 0xCBF43926u);
}

SF_TEST(store_settings_round_trip_is_byte_stable) {
  const sf::Settings settings = variedSettings();
  uint8_t first[sf::kMaxBlobBytes]{};
  const size_t firstLen = sf::encode(settings, first, sizeof(first));
  ASSERT_EQ(firstLen, sf::kSettingsBlobBytes);
  ASSERT_EQ(first[4], sf::kBlobVersion);
  ASSERT_EQ(first[5], settings.controlProfile);
  ASSERT_EQ(first[6], 0u);
  ASSERT_EQ(first[7], settings.tiltSensitivity);
  ASSERT_EQ(first[8], settings.brightness);
  ASSERT_EQ(first[9], settings.volume);
  ASSERT_EQ(first[10], 0u);
  ASSERT_EQ(first[11], settings.startLevel);
  ASSERT_EQ(first[12], settings.mode);

  sf::Settings decoded;
  ASSERT_EQ(sf::decode(first, firstLen, decoded), sf::DecodeStatus::Ok);
  ASSERT_TRUE(sameSettings(decoded, settings));

  uint8_t second[sf::kMaxBlobBytes]{};
  const size_t secondLen = sf::encode(decoded, second, sizeof(second));
  ASSERT_EQ(secondLen, firstLen);
  ASSERT_EQ(std::memcmp(first, second, firstLen), 0);
}

SF_TEST(store_scores_round_trip_is_byte_stable) {
  const ScoreTable scores = variedScores();
  uint8_t first[sf::kMaxBlobBytes]{};
  const size_t firstLen = sf::encode(scores.rows, first, sizeof(first));
  ASSERT_EQ(firstLen, sf::kScoresBlobBytes);
  ASSERT_EQ(first[4], sf::kBlobVersion);

  sf::ScoreEntry decoded[sf::kScoreEntryCount]{};
  ASSERT_EQ(sf::decode(first, firstLen, decoded), sf::DecodeStatus::Ok);
  for (size_t i = 0; i < sf::kScoreEntryCount; ++i) {
    ASSERT_TRUE(sameScore(decoded[i], scores.rows[i]));
  }

  uint8_t second[sf::kMaxBlobBytes]{};
  const size_t secondLen = sf::encode(decoded, second, sizeof(second));
  ASSERT_EQ(secondLen, firstLen);
  ASSERT_EQ(std::memcmp(first, second, firstLen), 0);
}

SF_TEST(store_wrong_magic_returns_bad_magic_and_defaults_output) {
  uint8_t bytes[sf::kMaxBlobBytes]{};
  ASSERT_EQ(sf::encode(variedSettings(), bytes, sizeof(bytes)),
            sf::kSettingsBlobBytes);
  bytes[0] ^= 0x80u;

  sf::Settings decoded = variedSettings();
  ASSERT_EQ(sf::decode(bytes, sf::kSettingsBlobBytes, decoded),
            sf::DecodeStatus::BadMagic);
  assertDefaultSettings(decoded);
}

SF_TEST(store_flipped_payload_returns_bad_crc_and_defaults_output) {
  uint8_t bytes[sf::kMaxBlobBytes]{};
  ASSERT_EQ(sf::encode(variedSettings(), bytes, sizeof(bytes)),
            sf::kSettingsBlobBytes);
  bytes[7] ^= 0x01u;

  sf::Settings decoded = variedSettings();
  ASSERT_EQ(sf::decode(bytes, sf::kSettingsBlobBytes, decoded),
            sf::DecodeStatus::BadCrc);
  assertDefaultSettings(decoded);
}

SF_TEST(store_four_byte_buffer_returns_short_and_defaults_output) {
  uint8_t bytes[sf::kMaxBlobBytes]{};
  ASSERT_EQ(sf::encode(variedSettings(), bytes, sizeof(bytes)),
            sf::kSettingsBlobBytes);

  sf::Settings decoded = variedSettings();
  ASSERT_EQ(sf::decode(bytes, 4, decoded), sf::DecodeStatus::Short);
  assertDefaultSettings(decoded);
}

SF_TEST(store_legacy_scores_migrate_all_fifteen_rows) {
  constexpr size_t kLegacyBytes =
      4u + sf::kScoreEntryCount * (4u + 2u + 1u + 1u) + 4u;
  static_assert(kLegacyBytes == 128, "legacy score layout changed");

  const ScoreTable legacy = variedScores();
  uint8_t bytes[kLegacyBytes]{};
  size_t pos = 0;
  putU32(bytes, pos, sf::kBlobMagic);
  for (const sf::ScoreEntry& entry : legacy.rows) {
    putU32(bytes, pos, entry.score);
    putU16(bytes, pos, entry.lines);
    bytes[pos++] = entry.level;
    bytes[pos++] = entry.mode;
  }
  ASSERT_EQ(pos, kLegacyBytes - 4u);
  putU32(bytes, pos, sf::crc32(bytes, pos));
  ASSERT_EQ(pos, kLegacyBytes);

  sf::ScoreEntry decoded[sf::kScoreEntryCount];
  for (sf::ScoreEntry& entry : decoded) {
    entry = sf::ScoreEntry{UINT32_MAX, UINT16_MAX, UINT8_MAX, UINT8_MAX};
  }
  ASSERT_EQ(sf::decode(bytes, sizeof(bytes), decoded),
            sf::DecodeStatus::Migrated);
  for (size_t i = 0; i < sf::kScoreEntryCount; ++i) {
    ASSERT_TRUE(sameScore(decoded[i], legacy.rows[i]));
  }

  uint8_t migrated[sf::kMaxBlobBytes]{};
  ASSERT_EQ(sf::encode(decoded, migrated, sizeof(migrated)),
            sf::kScoresBlobBytes);
  ASSERT_EQ(migrated[4], sf::kBlobVersion);
  sf::ScoreEntry roundTrip[sf::kScoreEntryCount]{};
  ASSERT_EQ(sf::decode(migrated, sf::kScoresBlobBytes, roundTrip),
            sf::DecodeStatus::Ok);
  for (size_t i = 0; i < sf::kScoreEntryCount; ++i) {
    ASSERT_TRUE(sameScore(roundTrip[i], legacy.rows[i]));
  }

  assertDefaultSettings(sf::Settings{});
}
