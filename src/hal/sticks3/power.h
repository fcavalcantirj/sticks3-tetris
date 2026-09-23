#pragma once

#include <cstdint>

#include "stackfall/power/policy.h"

namespace sf_hal {

sf::PowerIn readPower(uint32_t nowMs);

}  // namespace sf_hal
