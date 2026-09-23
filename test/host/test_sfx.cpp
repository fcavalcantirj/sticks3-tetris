#include <cstddef>
#include <cstdint>

#include "framework.h"
#include "stackfall/audio/sfx.h"

namespace {

using sf::Note;

constexpr Note kExpectedNotes[13][4] = {
    {{880, 12}, {0, 0}, {0, 0}, {0, 0}},
    {{1175, 14}, {0, 0}, {0, 0}, {0, 0}},
    {{660, 20}, {0, 0}, {0, 0}, {0, 0}},
    {{180, 60}, {0, 0}, {0, 0}, {0, 0}},
    {{440, 18}, {0, 0}, {0, 0}, {0, 0}},
    {{220, 22}, {0, 0}, {0, 0}, {0, 0}},
    {{988, 40}, {0, 0}, {0, 0}, {0, 0}},
    {{1175, 45}, {0, 0}, {0, 0}, {0, 0}},
    {{1319, 50}, {0, 0}, {0, 0}, {0, 0}},
    {{1568, 60}, {2093, 70}, {0, 0}, {0, 0}},
    {{740, 40}, {1480, 60}, {0, 0}, {0, 0}},
    {{1319, 40}, {1760, 60}, {0, 0}, {0, 0}},
    {{587, 90}, {440, 110}, {294, 180}, {0, 0}},
};

constexpr uint8_t kExpectedCounts[13] = {1, 1, 1, 1, 1, 1, 1,
                                         1, 1, 2, 2, 2, 3};

}  // namespace

SF_TEST(sfx_vocabulary_and_volume_caps) {
  ASSERT_EQ(static_cast<uint8_t>(sf::SfxId::Move), 0u);
  ASSERT_EQ(static_cast<uint8_t>(sf::SfxId::GameOver), 12u);
  ASSERT_EQ(sf::kSfxCueCount, 13u);
  ASSERT_EQ(sf::kVolumeCapBattery, 96u);
  ASSERT_EQ(sf::kVolumeCapUsb, 128u);
}

SF_TEST(sfx_note_table_is_exact_and_bounded) {
  for (std::size_t cueIndex = 0; cueIndex < sf::kSfxCueCount; ++cueIndex) {
    const sf::Cue& cue = sf::kSfxCues[cueIndex];
    ASSERT_EQ(cue.count, kExpectedCounts[cueIndex]);
    ASSERT_TRUE(cue.count <= 4u);
    for (std::size_t noteIndex = 0; noteIndex < 4u; ++noteIndex) {
      ASSERT_EQ(cue.notes[noteIndex].freqHz,
                kExpectedNotes[cueIndex][noteIndex].freqHz);
      ASSERT_EQ(cue.notes[noteIndex].ms,
                kExpectedNotes[cueIndex][noteIndex].ms);
    }
  }
}
