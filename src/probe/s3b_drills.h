#pragma once
#include <cstdint>

namespace sf {

// Drill identifiers shared between s3b_drills.cpp and its rendering helper.
enum class Drill : uint8_t {
    BootWait,
    T1Rest,
    T9PressJolt,
    T2Intent,
    T3Precision,
    T4CrossH,
    T4CrossV,
    T5Sustained,
    T6Absolute,
    T7Twist,
    Done
};

} // namespace sf
