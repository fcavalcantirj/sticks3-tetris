#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace sf {

struct DiagValues {
  int16_t axMilliG = 0;
  int16_t ayMilliG = 0;
  int16_t azMilliG = 0;
  char tilt[5]{};
  uint8_t blue = 0;
  uint8_t side = 0;
  uint32_t heap = 0;
  uint32_t minHeap = 0;
  uint16_t fps = 0;
  uint32_t sfxDrop = 0;
  uint32_t imuStale = 0;
};

inline int formatMilliG(int16_t milliG, char* out, std::size_t cap) {
  if (out == nullptr) {
    cap = 0;
  }
  const int32_t signedValue = milliG;
  const bool negative = signedValue < 0;
  const uint32_t magnitude = static_cast<uint32_t>(
      negative ? -signedValue : signedValue);
  return std::snprintf(out, cap, "%s%u.%02u", negative ? "-" : "",
                       static_cast<unsigned>(magnitude / 1000u),
                       static_cast<unsigned>((magnitude % 1000u) / 10u));
}

inline int formatDiagLine(const DiagValues& values, char* out,
                          std::size_t cap) {
  if (out == nullptr) {
    cap = 0;
  }
  char ax[8]{};
  char ay[8]{};
  char az[8]{};
  formatMilliG(values.axMilliG, ax, sizeof(ax));
  formatMilliG(values.ayMilliG, ay, sizeof(ay));
  formatMilliG(values.azMilliG, az, sizeof(az));
  return std::snprintf(
      out, cap,
      "[DIAG] ax=%s ay=%s az=%s tilt=%.5s blue=%u side=%u heap=%u "
      "minheap=%u fps=%u sfxdrop=%u imustale=%u",
      ax, ay, az, values.tilt, static_cast<unsigned>(values.blue),
      static_cast<unsigned>(values.side),
      static_cast<unsigned>(values.heap),
      static_cast<unsigned>(values.minHeap),
      static_cast<unsigned>(values.fps),
      static_cast<unsigned>(values.sfxDrop),
      static_cast<unsigned>(values.imuStale));
}

}  // namespace sf
