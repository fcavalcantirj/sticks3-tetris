#pragma once

#include <cstdint>

#include "stackfall/ui/screens.h"

namespace sf {
class App;
}

namespace sf::ui {
struct DrawPlan;
}

namespace sf_hal {

class Diagnostics;
class Panel;

class RuntimeRenderer {
 public:
  void invalidate() { needsFullRedraw_ = true; }
  void showReZeroBanner(uint32_t nowMs) {
    reZeroBannerStartedMs_ = nowMs;
    reZeroBannerActive_ = true;
    invalidate();
  }
  void resetPlaying() {
    havePreviousPlaying_ = false;
    reZeroBannerActive_ = false;
    invalidate();
  }
  void render(sf::App& app, Panel& panel, Diagnostics& diagnostics,
              sf::ui::DrawPlan& plan,
              const sf::ui::CalibratingView& calibrating, uint32_t nowMs,
              const char* version, const char* buildId);
  uint32_t takeFrames() {
    const uint32_t result = frames_;
    frames_ = 0;
    return result;
  }

 private:
  void renderPlayingModel(Panel& panel, Diagnostics& diagnostics,
                          sf::ui::DrawPlan& plan,
                          const sf::ui::PlayingModel& model);
  void pushFrame(Panel& panel, Diagnostics& diagnostics,
                 const sf::ui::DrawPlan& plan);

  sf::ui::PlayingModel previousPlaying_{};
  uint32_t lastDiagRenderMs_ = 0;
  uint32_t reZeroBannerStartedMs_ = 0;
  uint32_t frames_ = 0;
  bool havePreviousPlaying_ = false;
  bool needsFullRedraw_ = true;
  bool reZeroBannerActive_ = false;
};

}  // namespace sf_hal
