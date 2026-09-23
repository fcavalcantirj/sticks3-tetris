#pragma once

#include <cstdint>

namespace sf_hal {

// nowMs() wraps after 49.71 days. Every elapsed-time consumer must compare by
// unsigned subtraction: now - last >= period, never now >= last + period.
uint32_t nowMs();
uint64_t nowUs();

}  // namespace sf_hal
