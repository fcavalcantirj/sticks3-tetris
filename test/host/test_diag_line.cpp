#include "framework.h"
#include "stackfall/diag/diagline.h"

#include <cstring>

SF_TEST(test_diag_line) {
  sf::DiagValues values{};
  values.axMilliG = -1020;
  values.ayMilliG = 50;
  values.azMilliG = 1000;
  std::memcpy(values.tilt, "RIGHT", sizeof(values.tilt));
  values.blue = 1;
  values.side = 0;
  values.heap = 123;
  values.minHeap = 100;
  values.fps = 30;
  values.sfxDrop = 4;
  values.imuStale = 7;

  char line[120]{};
  const int length = sf::formatDiagLine(values, line, sizeof(line));
  ASSERT_STR_EQ(
      line,
      "[DIAG] ax=-1.02 ay=0.05 az=1.00 tilt=RIGHT blue=1 side=0 "
      "heap=123 minheap=100 fps=30 sfxdrop=4 imustale=7");
  ASSERT_EQ(length, static_cast<int>(std::strlen(line)));
  ASSERT_TRUE(length < 120);
}
