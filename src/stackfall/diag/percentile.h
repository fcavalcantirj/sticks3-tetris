#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace sf {

class PercentileRing {
 public:
  static constexpr std::size_t kCapacity = 256;

  void push(uint32_t us) {
    samples_[next_] = us;
    next_ = (next_ + 1u) % kCapacity;
    if (count_ < kCapacity) {
      ++count_;
    }
  }

  uint32_t percentile(uint8_t pct) const {
    if (count_ == 0) {
      return 0;
    }

    uint32_t sorted[kCapacity]{};
    for (std::size_t i = 0; i < count_; ++i) {
      sorted[i] = samples_[i];
    }
    std::sort(sorted, sorted + count_);

    const uint8_t bounded = pct > 100u ? 100u : pct;
    const std::size_t index = (count_ - 1u) * bounded / 100u;
    return sorted[index];
  }

 private:
  uint32_t samples_[kCapacity]{};
  std::size_t next_ = 0;
  std::size_t count_ = 0;
};

}  // namespace sf
