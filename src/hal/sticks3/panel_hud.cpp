#include "hal/sticks3/panel.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>

#include "hal/sticks3/palette.h"
#include "stackfall/ui/hud.h"
#include "stackfall/ui/layout.h"

namespace sf_hal {
namespace {

constexpr sf::ui::Rect kScoreRect{1, 3, 85, 12};
constexpr sf::ui::Rect kLevelRect{89, 5, 18, 8};
constexpr sf::ui::Rect kBatteryRect{108, 5, 26, 8};

constexpr sf::ui::Rect menuRowRect(uint8_t i) {
  return sf::ui::Rect{1, static_cast<int16_t>(20 + 18 * i), 133, 18};
}

constexpr sf::ui::Rect instructionRowRect(uint8_t i) {
  return sf::ui::Rect{1, static_cast<int16_t>(20 + 16 * i), 133, 16};
}

const char* menuRectName(uint8_t i) {
  constexpr const char* kNames[] = {"menu0", "menu1", "menu2",
                                     "menu3", "menu4", "menu5", "menu6",
                                     "menu7"};
  return i < 8 ? kNames[i] : "menu";
}

const char* instructionRectName(uint8_t i) {
  constexpr const char* kNames[] = {
      "instruction0",  "instruction1", "instruction2", "instruction3",
      "instruction4",  "instruction5", "instruction6", "instruction7",
      "instruction8",  "instruction9", "instruction10", "instruction11",
  };
  return i < 12 ? kNames[i] : "instruction";
}

bool beginText(LovyanGFX& graphics, sf::ui::Rect rect, uint8_t size,
               uint16_t foreground, uint16_t background,
               const char* rectName, uint8_t reportSlot) {
  graphics.setFont(&fonts::Font0);
  // HudText's size is the horizontal 6 px glyph multiplier. Keeping the
  // vertical scale at one makes the runtime font fit the ledger's 8/12 px
  // rectangles while preserving the pure textWidthPx oracle.
  graphics.setTextSize(size, 1);
  graphics.setTextDatum(textdatum_t::top_left);
  const int16_t fontHeight = static_cast<int16_t>(graphics.fontHeight());
  const int16_t y =
      static_cast<int16_t>(rect.y + (rect.h - fontHeight) / 2);
  const bool fits = y >= rect.y &&
                    y + graphics.fontHeight() <= rect.y + rect.h;
  if (!fits) {
    static uint32_t reported = 0;
    const uint32_t bit = uint32_t{1} << reportSlot;
    if ((reported & bit) == 0) {
      Serial.printf("[ERR] where=hud_%s code=%d\n", rectName,
                    static_cast<int>(y));
      reported |= bit;
    }
  }
  assert(y >= rect.y);
  assert(y + graphics.fontHeight() <= rect.y + rect.h);
  assert(fontHeight == graphics.fontHeight());
  if (!fits) return false;

  graphics.setTextColor(foreground, background);
  graphics.setCursor(rect.x + 2, y);
  return true;
}

void drawText(LovyanGFX& graphics, sf::ui::Rect rect, const char* text,
              uint8_t size, uint16_t foreground, uint16_t background,
              const char* rectName, uint8_t reportSlot) {
  if (!beginText(graphics, rect, size, foreground, background, rectName,
                 reportSlot)) {
    return;
  }
  graphics.print(text == nullptr ? "" : text);
}

sf::ui::Rect centredRect(sf::ui::Rect area, const char* text, uint8_t size) {
  const int width = sf::ui::hud::textWidthPx(text, size);
  return sf::ui::Rect{
      static_cast<int16_t>(area.x + (area.w - width) / 2 - 2), area.y,
      static_cast<int16_t>(width + 4), area.h};
}

void drawCentredText(LovyanGFX& graphics, sf::ui::Rect area,
                     const char* text, uint8_t size, uint16_t foreground,
                     const char* rectName, uint8_t reportSlot) {
  drawText(graphics, centredRect(area, text, size), text, size, foreground,
           palette::kBg, rectName, reportSlot);
}

void drawHeading(LovyanGFX& graphics, const char* text, uint8_t size = 1) {
  const sf::ui::Rect heading = sf::ui::topBarRect();
  graphics.fillRect(heading.x, heading.y, heading.w, heading.h, palette::kBg);
  drawCentredText(graphics, heading, text, size, palette::kText, "heading", 5);
}

bool beginMenuRow(LovyanGFX& graphics, uint8_t row, bool selected,
                  bool destructive) {
  const sf::ui::Rect rect = menuRowRect(row);
  const uint16_t background = selected ? palette::kText : palette::kBg;
  const uint16_t foreground =
      selected ? palette::kBg
               : (destructive ? palette::kWarn : palette::kText);
  graphics.fillRect(rect.x, rect.y, rect.w, rect.h, background);
  return beginText(graphics, rect, 1, foreground, background,
                   menuRectName(row), static_cast<uint8_t>(8 + row));
}

void drawMenuRow(LovyanGFX& graphics, uint8_t row, const char* text,
                 bool selected, bool destructive = false) {
  if (!beginMenuRow(graphics, row, selected, destructive)) return;
  graphics.print(text == nullptr ? "" : text);
}

void drawSettingsRow(LovyanGFX& graphics, uint8_t row,
                     const sf::ui::SettingsModel::Row& model,
                     bool selected) {
  if (!beginMenuRow(graphics, row, selected, model.destructive)) return;
  graphics.print(model.label == nullptr ? "" : model.label);
  if (model.value != nullptr && model.value[0] != '\0') {
    graphics.print(": ");
    graphics.print(model.value);
  }
}

uint16_t batteryColour(uint8_t pct, bool charging) {
  switch (sf::ui::battColour(pct, charging)) {
    case sf::ui::BattLevel::Ok:
      return palette::kBattOk;
    case sf::ui::BattLevel::Warn:
      return palette::kBattWarn;
    case sf::ui::BattLevel::Low:
      return palette::kBattLow;
    case sf::ui::BattLevel::Charging:
      return palette::kBattChg;
  }
  return palette::kBattLow;
}

void formatValue(char (&out)[23], const char* label, uint32_t value) {
  char full[32]{};
  std::snprintf(full, sizeof(full), "%s %u", label,
                static_cast<unsigned>(value));
  sf::ui::hud::fitText(out, sizeof(out), full, 133, 1,
                       sf::ui::hud::Priority::Head);
}

}  // namespace

void Panel::setBattery(uint8_t pct, bool charging, bool lowBattery) {
  batteryPct_ = pct;
  charging_ = charging;
  lowBattery_ = lowBattery;
  sf::ui::hud::formatBattery(batteryText_, sizeof(batteryText_), pct);
}

void Panel::drawPlayingHudRegion(const sf::ui::PlayingModel& model,
                                 sf::ui::Rect region) {
  const bool topDirty = sf::ui::intersects(region, sf::ui::topBarRect());
  const bool footerDirty = sf::ui::intersects(region, sf::ui::footerRect());
  if (!topDirty && !footerDirty) return;

  LovyanGFX& graphics = target();
  graphics.setClipRect(region.x, region.y, region.w, region.h);
  if (topDirty) {
    const sf::ui::Rect top = sf::ui::topBarRect();
    graphics.fillRect(top.x, top.y, top.w, top.h, palette::kBg);
    drawText(graphics, kScoreRect, model.hud.left, model.hud.leftSize,
             palette::kText, palette::kBg, "score", 0);
    drawText(graphics, kLevelRect, model.hud.mid, 1, palette::kText,
             palette::kBg, "level", 1);
    drawText(graphics, kBatteryRect, batteryText_, 1,
             batteryColour(batteryPct_, charging_), palette::kBg, "battery",
             2);
  }
  if (footerDirty) {
    const sf::ui::Rect footer = sf::ui::footerRect();
    graphics.fillRect(footer.x, footer.y, footer.w, footer.h, palette::kBg);
    drawText(graphics, sf::ui::kStatusRect, model.hud.right, 2,
             palette::kText,
             palette::kBg, "status", 3);
    if (model.bannerMsLeft > 0 && model.banner != nullptr &&
        model.banner[0] != '\0') {
      drawCentredText(graphics, sf::ui::kBannerRect, model.banner, 1,
                      palette::kBanner, "banner", 4);
    }
  }
  graphics.clearClipRect();
}

void Panel::drawTitle(const sf::ui::TitleModel& model) {
  LovyanGFX& graphics = target();
  graphics.fillScreen(palette::kBg);
  drawHeading(graphics, model.title, 2);
  for (uint8_t i = 0; i < 4; ++i) {
    drawMenuRow(graphics, i, model.items[i], model.menuIndex == i);
  }

  const sf::ui::Rect modeRect = menuRowRect(4);
  if (beginText(graphics, modeRect, 1, palette::kTextDim, palette::kBg,
                menuRectName(4), 12)) {
    graphics.print("MODE ");
    graphics.print(model.mode == nullptr ? "" : model.mode);
  }

  char score[16]{};
  uint8_t scoreSize = 1;
  sf::ui::hud::formatScore(score, sizeof(score), model.highScore, 133,
                           scoreSize);
  drawText(graphics, menuRowRect(5), score, scoreSize, palette::kTextDim,
           palette::kBg, menuRectName(5), 13);
  drawCentredText(graphics, sf::ui::footerRect(), model.version, 1,
                  palette::kTextDim, "version", 6);
}

void Panel::drawPaused(const sf::ui::PausedModel& model) {
  sf::ui::DrawPlan full{};
  full.full = true;
  drawPlaying(model.behind, full);

  LovyanGFX& graphics = target();
  if (model.dim) {
    for (int16_t y = 0; y < sf::ui::kScreenH; y += 2) {
      graphics.drawFastHLine(0, y, sf::ui::kScreenW, palette::kBg);
    }
  }
  drawHeading(graphics, "PAUSED");
  for (uint8_t i = 0; i < 4; ++i) {
    drawMenuRow(graphics, i, model.items[i], model.index == i);
  }
}

void Panel::drawGameOver(const sf::ui::GameOverModel& model) {
  LovyanGFX& graphics = target();
  graphics.fillScreen(palette::kBg);
  drawHeading(graphics, "GAME OVER");

  char score[16]{};
  uint8_t scoreSize = 1;
  sf::ui::hud::formatScore(score, sizeof(score), model.score, 133,
                           scoreSize);
  drawText(graphics, menuRowRect(0), score, scoreSize, palette::kText,
           palette::kBg, menuRectName(0), 8);

  char value[23]{};
  formatValue(value, "LINES", model.lines);
  drawText(graphics, menuRowRect(1), value, 1, palette::kText,
           palette::kBg, menuRectName(1), 9);
  formatValue(value, "LEVEL", model.level);
  drawText(graphics, menuRowRect(2), value, 1, palette::kText,
           palette::kBg, menuRectName(2), 10);
  formatValue(value, "TIME MS", model.elapsedMs);
  drawText(graphics, menuRowRect(3), value, 1, palette::kText,
           palette::kBg, menuRectName(3), 11);

  for (uint8_t i = 0; i < 2; ++i) {
    drawMenuRow(graphics, static_cast<uint8_t>(i + 4), model.items[i],
                model.index == i);
  }
  if (model.isHighScore) {
    drawCentredText(graphics, sf::ui::footerRect(), "NEW BEST", 1,
                    palette::kBanner, "new_best", 7);
  }
}

void Panel::drawHighScores(const sf::ui::HighScoresModel& model) {
  LovyanGFX& graphics = target();
  graphics.fillScreen(palette::kBg);
  drawHeading(graphics, "HIGH SCORES");
  for (uint8_t i = 0; i < 5; ++i) {
    drawMenuRow(graphics, i, model.rows[i].text, false);
  }
}

void Panel::drawSettings(const sf::ui::SettingsModel& model) {
  LovyanGFX& graphics = target();
  graphics.fillScreen(palette::kBg);
  drawHeading(graphics, "SETTINGS");
  for (uint8_t i = 0; i < sf::ui::kSettingsRows; ++i) {
    drawSettingsRow(graphics, i, model.rows[i], model.index == i);
  }
}

void Panel::drawTextPage(const char* heading,
                         const sf::ui::InstructionsModel& model) {
  LovyanGFX& graphics = target();
  graphics.fillScreen(palette::kBg);
  drawHeading(graphics, heading);
  const uint8_t count = model.count < 12 ? model.count : 12;
  for (uint8_t i = 0; i < count; ++i) {
    const sf::ui::Rect rect = instructionRowRect(i);
    const uint16_t foreground = model.emphasis[i] == 1
                                    ? palette::kWarn
                                    : model.emphasis[i] == 2
                                          ? palette::kTextDim
                                          : palette::kText;
    drawText(graphics, rect, model.lines[i], 1, foreground,
             palette::kBg, instructionRectName(i),
             static_cast<uint8_t>(16 + i));
  }
}

void Panel::drawInstructions(const sf::ui::InstructionsModel& model) {
  drawTextPage("INSTRUCTIONS", model);
}

}  // namespace sf_hal
