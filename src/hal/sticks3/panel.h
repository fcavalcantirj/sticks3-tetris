#pragma once

#include <M5Unified.h>

#include <cstdint>

#include "stackfall/ui/plan.h"

namespace sf_hal {

class Panel {
 public:
  Panel();

  bool begin();
  LovyanGFX& target();
  int width() const;
  int height() const;
  bool spriteOk() const { return sprite_; }
  uint32_t spriteBytes() const;
  uint32_t heapBefore() const { return heapBefore_; }
  uint32_t heapAfter() const { return heapAfter_; }

  void drawPlaying(const sf::ui::PlayingModel& model,
                   const sf::ui::DrawPlan& plan);
  void drawTitle(const sf::ui::TitleModel& model);
  void drawPaused(const sf::ui::PausedModel& model);
  void drawGameOver(const sf::ui::GameOverModel& model);
  void drawHighScores(const sf::ui::HighScoresModel& model);
  void drawSettings(const sf::ui::SettingsModel& model);
  void drawTextPage(const char* heading,
                    const sf::ui::InstructionsModel& model);
  void drawInstructions(const sf::ui::InstructionsModel& model);
  void pushFrame(const sf::ui::DrawPlan& plan);
  void setBattery(uint8_t pct, bool charging, bool lowBattery);
  void setBrightness(uint8_t value);

 private:
  void drawPlayingHudRegion(const sf::ui::PlayingModel& model,
                            sf::ui::Rect region);

  M5Canvas canvas_;
  uint32_t heapBefore_ = 0;
  uint32_t heapAfter_ = 0;
  char batteryText_[8] = "0%";
  uint8_t batteryPct_ = 0;
  uint8_t activePalette_ = 1;
  bool charging_ = false;
  bool lowBattery_ = false;
  bool sprite_ = false;
};

// Compatibility surface for the archived probe firmware. Production drawing
// goes through Panel::target(); probes use this accessor so panel.cpp remains
// the sole owner of the physical display object.
namespace probe {

M5GFX& display();

}  // namespace probe

}  // namespace sf_hal
