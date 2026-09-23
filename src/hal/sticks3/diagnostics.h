#pragma once

#include <cstddef>
#include <cstdint>

#include "stackfall/diag/diagline.h"
#include "stackfall/diag/percentile.h"

namespace sf::input {
class InputRouter;
}

namespace sf_hal {

class Panel;
struct RawButtons;

class Diagnostics {
 public:
  void setAccel(float ax, float ay, float az);
  void refresh(const sf::input::InputRouter& router,
               const RawButtons& buttons, uint32_t nowMs,
               uint32_t lastBlueEdgeMs, uint32_t lastSideEdgeMs,
               uint16_t fps, uint32_t sfxDrop, uint32_t imuStale);
  void draw(Panel& panel, const char* version, const char* buildId) const;
  int formatLine(char* out, std::size_t cap) const;

  void recordLoopUs(uint32_t us) { loopTimes_.push(us); }
  void recordFrameUs(uint32_t us) { frameTimes_.push(us); }
  uint32_t loopPercentile(uint8_t pct) const {
    return loopTimes_.percentile(pct);
  }
  uint32_t framePercentile(uint8_t pct) const {
    return frameTimes_.percentile(pct);
  }

 private:
  sf::DiagValues values_{};
  sf::PercentileRing loopTimes_{};
  sf::PercentileRing frameTimes_{};
  int16_t marginMilliG_ = 0;
  uint32_t blueAgeMs_ = 0;
  uint32_t sideAgeMs_ = 0;
  int8_t column_ = 4;
  char dip_[7] = "NEUT";
};

}  // namespace sf_hal
