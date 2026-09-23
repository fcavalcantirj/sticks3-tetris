#include "stackfall/app/blob.h"

namespace sf {
namespace {

constexpr size_t kMagicBytes = 4;
constexpr size_t kVersionBytes = 1;
constexpr size_t kCrcBytes = 4;
constexpr size_t kSettingsPayloadBytes = 8;
constexpr size_t kScorePayloadBytes = 8;
constexpr size_t kLegacyScoresBlobBytes =
    kMagicBytes + kScoreEntryCount * kScorePayloadBytes + kCrcBytes;

static_assert(kMagicBytes + kVersionBytes + kSettingsPayloadBytes +
                      kCrcBytes ==
                  kSettingsBlobBytes,
              "settings blob byte accounting changed");
static_assert(kMagicBytes + kVersionBytes +
                          kScoreEntryCount * kScorePayloadBytes + kCrcBytes ==
                      kScoresBlobBytes,
              "score blob byte accounting changed");
static_assert(kLegacyScoresBlobBytes == 128,
              "legacy score blob byte accounting changed");

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

uint16_t getU16(const uint8_t* in, size_t& pos) {
  const uint16_t value = static_cast<uint16_t>(in[pos]) |
                         static_cast<uint16_t>(in[pos + 1u]) << 8u;
  pos += 2;
  return value;
}

uint32_t getU32(const uint8_t* in, size_t& pos) {
  const uint32_t value = static_cast<uint32_t>(in[pos]) |
                         static_cast<uint32_t>(in[pos + 1u]) << 8u |
                         static_cast<uint32_t>(in[pos + 2u]) << 16u |
                         static_cast<uint32_t>(in[pos + 3u]) << 24u;
  pos += 4;
  return value;
}

bool magicMatches(const uint8_t* in) {
  size_t pos = 0;
  return getU32(in, pos) == kBlobMagic;
}

bool crcMatches(const uint8_t* in, size_t payloadBytes) {
  size_t crcPos = payloadBytes;
  return getU32(in, crcPos) == crc32(in, payloadBytes);
}

void resetScores(ScoreEntry out[kScoreEntryCount]) {
  if (out == nullptr) {
    return;
  }
  for (size_t i = 0; i < kScoreEntryCount; ++i) {
    out[i] = ScoreEntry{};
  }
}

void readScores(const uint8_t* in, size_t pos,
                ScoreEntry out[kScoreEntryCount]) {
  for (size_t i = 0; i < kScoreEntryCount; ++i) {
    out[i].score = getU32(in, pos);
    out[i].lines = getU16(in, pos);
    out[i].level = in[pos++];
    out[i].mode = in[pos++];
  }
}

}  // namespace

uint32_t crc32(const uint8_t* data, size_t len) {
  uint32_t crc = 0xFFFFFFFFu;
  for (size_t i = 0; i < len; ++i) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      const uint32_t reflected = 0u - (crc & 1u);
      crc = (crc >> 1u) ^ (0xEDB88320u & reflected);
    }
  }
  return crc ^ 0xFFFFFFFFu;
}

size_t encode(const Settings& settings, uint8_t* out, size_t cap) {
  if (out == nullptr || cap < kSettingsBlobBytes) {
    return 0;
  }

  size_t pos = 0;
  putU32(out, pos, kBlobMagic);
  out[pos++] = kBlobVersion;
  out[pos++] = settings.controlProfile;
  out[pos++] = settings.rotateCw ? 1u : 0u;
  out[pos++] = settings.tiltSensitivity;
  out[pos++] = settings.brightness;
  out[pos++] = settings.volume;
  out[pos++] = settings.ghostEnabled ? 1u : 0u;
  out[pos++] = settings.startLevel;
  out[pos++] = settings.mode;
  putU32(out, pos, crc32(out, pos));
  return pos;
}

DecodeStatus decode(const uint8_t* in, size_t len, Settings& out) {
  out = Settings{};
  if (in == nullptr || len < kMagicBytes) {
    return DecodeStatus::Short;
  }
  if (!magicMatches(in)) {
    return DecodeStatus::BadMagic;
  }
  if (len < kSettingsBlobBytes) {
    return DecodeStatus::Short;
  }
  if (in[kMagicBytes] != kBlobVersion) {
    return DecodeStatus::BadMagic;
  }
  if (!crcMatches(in, kSettingsBlobBytes - kCrcBytes)) {
    return DecodeStatus::BadCrc;
  }

  Settings decoded;
  size_t pos = kMagicBytes + kVersionBytes;
  decoded.controlProfile = in[pos++];
  decoded.rotateCw = in[pos++] != 0;
  decoded.tiltSensitivity = in[pos++];
  decoded.brightness = in[pos++];
  decoded.volume = in[pos++];
  decoded.ghostEnabled = in[pos++] != 0;
  decoded.startLevel = in[pos++];
  decoded.mode = in[pos++];
  out = decoded;
  return DecodeStatus::Ok;
}

size_t encode(const ScoreEntry scores[kScoreEntryCount], uint8_t* out,
              size_t cap) {
  if (scores == nullptr || out == nullptr || cap < kScoresBlobBytes) {
    return 0;
  }

  size_t pos = 0;
  putU32(out, pos, kBlobMagic);
  out[pos++] = kBlobVersion;
  for (size_t i = 0; i < kScoreEntryCount; ++i) {
    putU32(out, pos, scores[i].score);
    putU16(out, pos, scores[i].lines);
    out[pos++] = scores[i].level;
    out[pos++] = scores[i].mode;
  }
  putU32(out, pos, crc32(out, pos));
  return pos;
}

DecodeStatus decode(const uint8_t* in, size_t len,
                    ScoreEntry out[kScoreEntryCount]) {
  resetScores(out);
  if (out == nullptr || in == nullptr || len < kMagicBytes) {
    return DecodeStatus::Short;
  }
  if (!magicMatches(in)) {
    return DecodeStatus::BadMagic;
  }

  if (len == kLegacyScoresBlobBytes) {
    if (!crcMatches(in, kLegacyScoresBlobBytes - kCrcBytes)) {
      return DecodeStatus::BadCrc;
    }
    ScoreEntry decoded[kScoreEntryCount]{};
    readScores(in, kMagicBytes, decoded);
    for (size_t i = 0; i < kScoreEntryCount; ++i) {
      out[i] = decoded[i];
    }
    return DecodeStatus::Migrated;
  }

  if (len < kScoresBlobBytes) {
    return DecodeStatus::Short;
  }
  if (in[kMagicBytes] != kBlobVersion) {
    return DecodeStatus::BadMagic;
  }
  if (!crcMatches(in, kScoresBlobBytes - kCrcBytes)) {
    return DecodeStatus::BadCrc;
  }

  ScoreEntry decoded[kScoreEntryCount]{};
  readScores(in, kMagicBytes + kVersionBytes, decoded);
  for (size_t i = 0; i < kScoreEntryCount; ++i) {
    out[i] = decoded[i];
  }
  return DecodeStatus::Ok;
}

}  // namespace sf
