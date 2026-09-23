#include <cstdio>
#include <cstring>
#include <string>

#include "framework.h"
#include "golden.h"
#include "replay_fixture.h"

int main(int argc, char** argv) {
  if (argc == 3 && std::strcmp(argv[1], "--replay") == 0) {
    return ::sf::test::replay::compareReplayFile(argv[2], stdout);
  }

  std::string filter;
  bool bless = false;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--filter") == 0 && i + 1 < argc) {
      filter = argv[i + 1];
      ++i;
    } else if (std::strcmp(argv[i], "--bless") == 0) {
      bless = true;
    }
  }
  ::sf::test::golden::setBlessMode(bless);
  int passed = 0;
  int failed = 0;
  for (const auto& tc : ::sf::test::registry()) {
    // Blessing always exercises the complete non-golden suite. A filter may
    // narrow an ordinary run, but can never weaken the precondition to write.
    if (!bless && !filter.empty() &&
        std::string(tc.name).find(filter) == std::string::npos) {
      continue;
    }
    const bool golden = std::strncmp(tc.name, "golden_", 7) == 0;
    try {
      tc.fn();
      if (bless && golden) {
        std::printf("SKIP %s (bless)\n", tc.name);
      } else {
        std::printf("PASS %s\n", tc.name);
        ++passed;
      }
    } catch (const ::sf::test::Failure& f) {
      std::printf("FAIL %s: %s\n", tc.name, f.msg);
      ++failed;
    }
  }
  int total = passed + failed;
  if (bless && failed != 0) {
    std::printf("BLESS REFUSED %d tests failed\n", failed);
    std::printf("Total %d Passed %d Failed %d\n", total, passed, failed);
    return 1;
  }
  if (bless) {
    const int blessed = ::sf::test::golden::blessAll();
    if (blessed != ::sf::test::golden::kGoldenCount) {
      std::printf("Total %d Passed %d Failed 1\n", total + 1, passed);
      return 1;
    }
    std::printf("BLESSED %d goldens\n", blessed);
  }
  std::printf("Total %d Passed %d Failed %d\n", total, passed, failed);
  return failed == 0 ? 0 : 1;
}
