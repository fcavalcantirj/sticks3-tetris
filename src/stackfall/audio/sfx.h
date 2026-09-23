#pragma once

#include <cstddef>
#include <cstdint>

namespace sf {

enum class SfxId : uint8_t {
  Move,
  Rotate,
  Hold,
  Denied,
  Lock,
  HardDrop,
  Single,
  Double,
  Triple,
  Quad,
  TSpin,
  LevelUp,
  GameOver,
};

constexpr uint8_t kVolumeCapBattery = 96;
constexpr uint8_t kVolumeCapUsb = 128;

struct Note {
  uint16_t freqHz;
  uint16_t ms;
};

struct Cue {
  Note notes[4];
  uint8_t count;
};

namespace detail {

constexpr Cue makeCue(Note first, Note second = Note{0, 0},
                      Note third = Note{0, 0}, Note fourth = Note{0, 0}) {
  const uint8_t count = fourth.ms != 0 ? 4 : third.ms != 0 ? 3 : second.ms != 0 ? 2 : 1;
  return Cue{{first, second, third, fourth}, count};
}

}  // namespace detail

inline constexpr Cue kSfxCues[] = {
    detail::makeCue({880, 12}),
    detail::makeCue({1175, 14}),
    detail::makeCue({660, 20}),
    detail::makeCue({180, 60}),
    detail::makeCue({440, 18}),
    detail::makeCue({220, 22}),
    detail::makeCue({988, 40}),
    detail::makeCue({1175, 45}),
    detail::makeCue({1319, 50}),
    detail::makeCue({1568, 60}, {2093, 70}),
    detail::makeCue({740, 40}, {1480, 60}),
    detail::makeCue({1319, 40}, {1760, 60}),
    detail::makeCue({587, 90}, {440, 110}, {294, 180}),
};

inline constexpr std::size_t kSfxCueCount = sizeof(kSfxCues) / sizeof(kSfxCues[0]);

constexpr bool sfxTableValid() {
  for (const Cue& cue : kSfxCues) {
    if (cue.count == 0 || cue.count > 4) {
      return false;
    }
    for (std::size_t i = 0; i < 4; ++i) {
      const bool populated = cue.notes[i].freqHz != 0 && cue.notes[i].ms != 0;
      if (populated != (i < cue.count)) {
        return false;
      }
    }
  }
  return true;
}

static_assert(kSfxCueCount == static_cast<std::size_t>(SfxId::GameOver) + 1,
              "every SfxId must have exactly one cue");
static_assert(sfxTableValid(), "SFX cues must contain one to four complete notes");

}  // namespace sf
