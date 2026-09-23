#include "hal/sticks3/diagnostics.h"

#include <M5Unified.h>

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "hal/sticks3/buttons.h"
#include "hal/sticks3/palette.h"
#include "hal/sticks3/panel.h"
#include "hal/sticks3/sysinfo.h"
#include "stackfall/input/dip.h"
#include "stackfall/input/router.h"
#include "stackfall/input/tilt.h"
#include "stackfall/ui/layout.h"

namespace sf_hal {
namespace {

constexpr int16_t kLinePitch = 10;

int16_t toMilliG(float value) {
  const float scaled = value * 1000.0f;
  if (scaled >= 32767.0f) return 32767;
  if (scaled <= -32768.0f) return -32768;
  return static_cast<int16_t>(scaled >= 0.0f ? scaled + 0.5f
                                             : scaled - 0.5f);
}

template <std::size_t N>
void copyLabel(char (&out)[N], const char* text) {
  std::memset(out, 0, sizeof(out));
  if (text == nullptr) return;
  std::size_t i = 0;
  while (i < N && text[i] != '\0') {
    out[i] = text[i];
    ++i;
  }
}

float nearestBoundaryMargin(float signalG, int column) {
  const float left =
      (static_cast<float>(column) - 4.5f) * sf::input::kGPerColumn -
      sf::input::kColHysteresis;
  const float right =
      (static_cast<float>(column + 1) - 4.5f) * sf::input::kGPerColumn +
      sf::input::kColHysteresis;
  if (column <= 0) return right - signalG;
  if (column >= 9) return left - signalG;
  const float leftMargin = left - signalG;
  const float rightMargin = right - signalG;
  return std::fabs(leftMargin) < std::fabs(rightMargin) ? leftMargin
                                                        : rightMargin;
}

void formatSignedMilliG(char* out, std::size_t cap, int16_t milliG) {
  const int32_t signedValue = milliG;
  const bool negative = signedValue < 0;
  const uint32_t magnitude = static_cast<uint32_t>(
      negative ? -signedValue : signedValue);
  std::snprintf(out, cap, "%c%u.%03u", negative ? '-' : '+',
                static_cast<unsigned>(magnitude / 1000u),
                static_cast<unsigned>(magnitude % 1000u));
}

void drawLine(LovyanGFX& graphics, uint8_t row, const char* text) {
  const sf::ui::Rect rect{
      0, static_cast<int16_t>(row * kLinePitch), sf::ui::kScreenW,
      kLinePitch};
  const int16_t fontHeight = static_cast<int16_t>(graphics.fontHeight());
  const int16_t y =
      static_cast<int16_t>(rect.y + (rect.h - fontHeight) / 2);
  assert(y >= rect.y);
  assert(y + fontHeight <= rect.y + rect.h);
  graphics.setCursor(rect.x + 1, y);
  graphics.print(text == nullptr ? "" : text);
}

}  // namespace

void Diagnostics::setAccel(float ax, float ay, float az) {
  values_.axMilliG = toMilliG(ax);
  values_.ayMilliG = toMilliG(ay);
  values_.azMilliG = toMilliG(az);
}

void Diagnostics::refresh(const sf::input::InputRouter& router,
                          const RawButtons& buttons, uint32_t nowMs,
                          uint32_t lastBlueEdgeMs,
                          uint32_t lastSideEdgeMs, uint16_t fps,
                          uint32_t sfxDrop, uint32_t imuStale) {
  const float tiltSignalG = router.tiltSignalG();
  const char* intent = "NEUT";
  if (router.softDropActive()) {
    intent = "SOFT";
  } else if (!router.dipEngaged()) {
    if (tiltSignalG < -sf::input::kColHysteresis) {
      intent = "LEFT";
    } else if (tiltSignalG > sf::input::kColHysteresis) {
      intent = "RIGHT";
    }
  }
  copyLabel(values_.tilt, intent);

  const int selected = router.selectedColumn();
  column_ = static_cast<int8_t>(selected < 0 ? 0 : (selected > 9 ? 9 : selected));
  marginMilliG_ =
      toMilliG(nearestBoundaryMargin(tiltSignalG, column_));

  const float dipSignalG = router.dipSignalG();
  if (dipSignalG > sf::input::kDipReleaseG) {
    copyLabel(dip_, "AWAY");
  } else if (dipSignalG < -sf::input::kDipReleaseG) {
    copyLabel(dip_, "TOWARD");
  } else {
    copyLabel(dip_, "NEUT");
  }

  values_.blue = buttons.blue ? 1u : 0u;
  values_.side = buttons.side ? 1u : 0u;
  values_.heap = freeHeap();
  values_.minHeap = minFreeHeap();
  values_.fps = fps;
  values_.sfxDrop = sfxDrop;
  values_.imuStale = imuStale;
  blueAgeMs_ = nowMs - lastBlueEdgeMs;
  sideAgeMs_ = nowMs - lastSideEdgeMs;
}

int Diagnostics::formatLine(char* out, std::size_t cap) const {
  return sf::formatDiagLine(values_, out, cap);
}

void Diagnostics::draw(Panel& panel, const char* version,
                       const char* buildId) const {
  LovyanGFX& graphics = panel.target();
  graphics.fillScreen(palette::kBg);
  graphics.setFont(&fonts::Font0);
  graphics.setTextSize(1, 1);
  graphics.setTextDatum(textdatum_t::top_left);
  graphics.setTextColor(palette::kText, palette::kBg);

  char line[48]{};
  char value[8]{};
  sf::formatMilliG(values_.axMilliG, value, sizeof(value));
  std::snprintf(line, sizeof(line), "ax %s G", value);
  drawLine(graphics, 0, line);
  sf::formatMilliG(values_.ayMilliG, value, sizeof(value));
  std::snprintf(line, sizeof(line), "ay %s G", value);
  drawLine(graphics, 1, line);
  sf::formatMilliG(values_.azMilliG, value, sizeof(value));
  std::snprintf(line, sizeof(line), "az %s G", value);
  drawLine(graphics, 2, line);

  char tilt[6]{};
  std::memcpy(tilt, values_.tilt, sizeof(values_.tilt));
  std::snprintf(line, sizeof(line), "tilt %s", tilt);
  drawLine(graphics, 3, line);
  std::snprintf(line, sizeof(line), "column %u",
                static_cast<unsigned>(column_));
  drawLine(graphics, 4, line);
  formatSignedMilliG(value, sizeof(value), marginMilliG_);
  std::snprintf(line, sizeof(line), "margin %s G", value);
  drawLine(graphics, 5, line);
  std::snprintf(line, sizeof(line), "dip %s", dip_);
  drawLine(graphics, 6, line);

  std::snprintf(line, sizeof(line), "blue %u age %u",
                static_cast<unsigned>(values_.blue),
                static_cast<unsigned>(blueAgeMs_));
  drawLine(graphics, 7, line);
  std::snprintf(line, sizeof(line), "side %u age %u",
                static_cast<unsigned>(values_.side),
                static_cast<unsigned>(sideAgeMs_));
  drawLine(graphics, 8, line);
  std::snprintf(line, sizeof(line), "heap %u",
                static_cast<unsigned>(values_.heap));
  drawLine(graphics, 9, line);
  std::snprintf(line, sizeof(line), "minheap %u",
                static_cast<unsigned>(values_.minHeap));
  drawLine(graphics, 10, line);

  std::snprintf(line, sizeof(line), "loop p50 %u",
                static_cast<unsigned>(loopTimes_.percentile(50)));
  drawLine(graphics, 11, line);
  std::snprintf(line, sizeof(line), "loop p95 %u",
                static_cast<unsigned>(loopTimes_.percentile(95)));
  drawLine(graphics, 12, line);
  std::snprintf(line, sizeof(line), "loop p99 %u",
                static_cast<unsigned>(loopTimes_.percentile(99)));
  drawLine(graphics, 13, line);
  std::snprintf(line, sizeof(line), "loop max %u",
                static_cast<unsigned>(loopTimes_.percentile(100)));
  drawLine(graphics, 14, line);
  std::snprintf(line, sizeof(line), "frame p95 %u",
                static_cast<unsigned>(frameTimes_.percentile(95)));
  drawLine(graphics, 15, line);
  std::snprintf(line, sizeof(line), "fps %u",
                static_cast<unsigned>(values_.fps));
  drawLine(graphics, 16, line);
  std::snprintf(line, sizeof(line), "sfxdrop %u",
                static_cast<unsigned>(values_.sfxDrop));
  drawLine(graphics, 17, line);
  std::snprintf(line, sizeof(line), "imustale %u",
                static_cast<unsigned>(values_.imuStale));
  drawLine(graphics, 18, line);
  std::snprintf(line, sizeof(line), "build %s %s",
                version == nullptr ? "" : version,
                buildId == nullptr ? "" : buildId);
  drawLine(graphics, 19, line);
}

}  // namespace sf_hal
