#pragma once

#include <cstddef>
#include <cstdint>

#include "stackfall/app/app.h"

namespace sf {

enum class DecodeStatus : uint8_t {
  Ok,
  BadMagic,
  BadCrc,
  Short,
  Migrated,
};

constexpr uint32_t kBlobMagic = 0x53544B31u;
constexpr uint8_t kBlobVersion = 1;
constexpr size_t kScoreEntryCount = 15;
constexpr size_t kSettingsBlobBytes = 17;
constexpr size_t kScoresBlobBytes = 129;
constexpr size_t kMaxBlobBytes = 144;

static_assert(kSettingsBlobBytes == 17,
              "settings persistence layout changed");
static_assert(kScoresBlobBytes == 129,
              "score persistence layout changed");

uint32_t crc32(const uint8_t* data, size_t len);

size_t encode(const Settings& settings, uint8_t* out, size_t cap);
DecodeStatus decode(const uint8_t* in, size_t len, Settings& out);

size_t encode(const ScoreEntry scores[kScoreEntryCount], uint8_t* out,
              size_t cap);
DecodeStatus decode(const uint8_t* in, size_t len,
                    ScoreEntry out[kScoreEntryCount]);

}  // namespace sf
