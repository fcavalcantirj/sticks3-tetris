#pragma once

#include <cstdint>

namespace sf_hal::palette {

constexpr uint16_t kBg = 0x0000;
constexpr uint16_t kFrame = 0x4208;
constexpr uint16_t kText = 0xFFFF;
constexpr uint16_t kTextDim = 0x8410;
constexpr uint16_t kBanner = 0xFD20;
constexpr uint16_t kGhost = 0x39C7;
constexpr uint16_t kWarn = 0xFEA0;
constexpr uint16_t kBattOk = 0x07E0;
constexpr uint16_t kBattWarn = 0xFEA0;
constexpr uint16_t kBattLow = 0xF800;
constexpr uint16_t kBattChg = 0x07FF;

}  // namespace sf_hal::palette
