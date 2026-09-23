#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace sf::ui {

enum class BattLevel : uint8_t { Ok, Warn, Low, Charging };

constexpr BattLevel battColour(uint8_t pct, bool charging) {
  if (charging) return BattLevel::Charging;
  if (pct >= 40) return BattLevel::Ok;
  if (pct >= 20) return BattLevel::Warn;
  return BattLevel::Low;
}

struct HudModel {
  uint32_t score;
  uint16_t level;
  uint16_t lines;
  uint32_t elapsedMs;
  uint8_t batteryPct;
  bool charging;
};

struct HudText {
  char left[16];
  uint8_t leftSize;
  char mid[10];
  char right[8];
};

namespace hud {

enum class Priority { Head, Tail };

namespace detail {

constexpr std::size_t textLength(const char* s) {
  if (s == nullptr) return 0;
  std::size_t length = 0;
  while (s[length] != '\0') ++length;
  return length;
}

inline void copyText(char* out, std::size_t n, const char* s) {
  if (out == nullptr || n == 0) return;
  std::size_t written = 0;
  if (s != nullptr) {
    while (written + 1 < n && s[written] != '\0') {
      out[written] = s[written];
      ++written;
    }
  }
  out[written] = '\0';
}

}  // namespace detail

constexpr int textWidthPx(const char* s, uint8_t size) {
  return static_cast<int>(detail::textLength(s)) * 6 * static_cast<int>(size);
}

inline int fitText(char* out, std::size_t n, const char* s, int maxPx, uint8_t size,
                   Priority priority) {
  if (out == nullptr || n == 0) return 0;
  out[0] = '\0';
  if (s == nullptr || maxPx <= 0 || size == 0) return 0;

  const std::size_t sourceLength = detail::textLength(s);
  const int glyphWidth = textWidthPx(".", size);
  const std::size_t widthCapacity = static_cast<std::size_t>(maxPx / glyphWidth);
  const std::size_t bufferCapacity = n - 1;
  const std::size_t outputCapacity =
      widthCapacity < bufferCapacity ? widthCapacity : bufferCapacity;

  if (sourceLength <= outputCapacity) {
    detail::copyText(out, n, s);
    return textWidthPx(out, size);
  }
  if (outputCapacity == 0) return 0;

  const std::size_t kept = outputCapacity - 1;
  if (priority == Priority::Head) {
    for (std::size_t i = 0; i < kept; ++i) out[i] = s[i];
    out[kept] = '.';
  } else {
    out[0] = '.';
    const std::size_t sourceStart = sourceLength - kept;
    for (std::size_t i = 0; i < kept; ++i) out[i + 1] = s[sourceStart + i];
  }
  out[outputCapacity] = '\0';
  return textWidthPx(out, size);
}

inline bool formatScore(char* out, std::size_t n, uint32_t score, int maxPx,
                        uint8_t& sizeOut) {
  constexpr uint32_t kMaxScore = 9999999;
  const bool exact = score <= kMaxScore;
  const uint32_t shown = exact ? score : kMaxScore;

  char full[16]{};
  std::snprintf(full, sizeof(full), "SCORE %u", static_cast<unsigned>(shown));

  sizeOut = 2;
  if (textWidthPx(full, sizeOut) <= maxPx) {
    detail::copyText(out, n, full);
    return exact;
  }

  if (shown >= 1000000) {
    char abbreviated[16]{};
    const unsigned millions = static_cast<unsigned>(shown / 1000000);
    const unsigned hundredths = static_cast<unsigned>((shown % 1000000) / 10000);
    std::snprintf(abbreviated, sizeof(abbreviated), "SCORE %u.%02uM", millions, hundredths);
    if (textWidthPx(abbreviated, sizeOut) <= maxPx) {
      detail::copyText(out, n, abbreviated);
      return exact;
    }
  }

  sizeOut = 1;
  if (textWidthPx(full, sizeOut) <= maxPx) {
    detail::copyText(out, n, full);
  } else {
    fitText(out, n, full, maxPx, sizeOut, Priority::Head);
  }
  return exact;
}

inline int formatBattery(char* out, std::size_t n, uint8_t pct) {
  const unsigned shown = pct > 100 ? 100 : pct;
  char battery[5]{};
  std::snprintf(battery, sizeof(battery), "%u%%", shown);
  detail::copyText(out, n, battery);
  return out == nullptr || n == 0 ? 0 : textWidthPx(out, 1);
}

static_assert(4 * 6 * 1 <= 26);

}  // namespace hud
}  // namespace sf::ui
