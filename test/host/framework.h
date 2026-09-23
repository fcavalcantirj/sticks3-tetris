#pragma once

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <type_traits>
#include <vector>

namespace sf {
namespace test {

struct Failure {
  char msg[512];
};

struct TestCase {
  const char* name;
  void (*fn)();
};

inline std::vector<TestCase>& registry() {
  static std::vector<TestCase> cases;
  return cases;
}

struct Registrar {
  Registrar(const char* name, void (*fn)()) { registry().push_back(TestCase{name, fn}); }
};

inline void failWith(const char* file, int line, const char* text) {
  Failure f;
  std::snprintf(f.msg, sizeof(f.msg), "%s:%d: %s", file, line, text);
  throw f;
}

inline void assertTrue(bool c, const char* file, int line, const char* expr) {
  if (!c) {
    Failure f;
    std::snprintf(f.msg, sizeof(f.msg), "%s:%d: ASSERT_TRUE failed: %s", file, line, expr);
    throw f;
  }
}

inline void assertFalse(bool c, const char* file, int line, const char* expr) {
  if (c) {
    Failure f;
    std::snprintf(f.msg, sizeof(f.msg), "%s:%d: ASSERT_FALSE failed: %s", file, line, expr);
    throw f;
  }
}

template <typename A, typename B>
inline void assertEq(const A& a, const B& b, const char* file, int line, const char* exprA,
                     const char* exprB) {
  if constexpr (std::is_integral_v<A> && std::is_integral_v<B>) {
    long long av = static_cast<long long>(a);
    long long bv = static_cast<long long>(b);
    if (av != bv) {
      Failure f;
      std::snprintf(f.msg, sizeof(f.msg), "%s:%d: ASSERT_EQ failed: %s (%lld) != %s (%lld)",
                    file, line, exprA, av, exprB, bv);
      throw f;
    }
  } else {
    if (!(a == b)) {
      Failure f;
      std::snprintf(f.msg, sizeof(f.msg), "%s:%d: ASSERT_EQ failed: %s != %s", file, line,
                    exprA, exprB);
      throw f;
    }
  }
}

template <typename A, typename B>
inline void assertNeq(const A& a, const B& b, const char* file, int line, const char* exprA,
                      const char* exprB) {
  if constexpr (std::is_integral_v<A> && std::is_integral_v<B>) {
    long long av = static_cast<long long>(a);
    long long bv = static_cast<long long>(b);
    if (av == bv) {
      Failure f;
      std::snprintf(f.msg, sizeof(f.msg), "%s:%d: ASSERT_NEQ failed: %s (%lld) == %s (%lld)",
                    file, line, exprA, av, exprB, bv);
      throw f;
    }
  } else {
    if (a == b) {
      Failure f;
      std::snprintf(f.msg, sizeof(f.msg), "%s:%d: ASSERT_NEQ failed: %s == %s", file, line,
                    exprA, exprB);
      throw f;
    }
  }
}

template <typename A, typename B>
inline void assertStrEq(const A& a, const B& b, const char* file, int line, const char* exprA,
                        const char* exprB) {
  std::string sa(a);
  std::string sb(b);
  if (sa != sb) {
    Failure f;
    std::snprintf(f.msg, sizeof(f.msg), "%s:%d: ASSERT_STR_EQ failed: %s (\"%s\") != %s (\"%s\")",
                  file, line, exprA, sa.c_str(), exprB, sb.c_str());
    throw f;
  }
}

template <typename A, typename B, typename T>
inline void assertNear(const A& a, const B& b, const T& tol, const char* file, int line,
                       const char* exprA, const char* exprB, const char* exprTol) {
  double da = static_cast<double>(a);
  double db = static_cast<double>(b);
  double dt = static_cast<double>(tol);
  if (std::fabs(da - db) > dt) {
    Failure f;
    std::snprintf(f.msg, sizeof(f.msg),
                  "%s:%d: ASSERT_NEAR failed: %s (%f) != %s (%f) tol %s (%f)", file, line, exprA,
                  da, exprB, db, exprTol, dt);
    throw f;
  }
}

}  // namespace test
}  // namespace sf

#define SF_TEST(name)                                     \
  static void name();                                     \
  static ::sf::test::Registrar registrar_##name(#name, name); \
  static void name()

#define ASSERT_TRUE(c) ::sf::test::assertTrue((c), __FILE__, __LINE__, #c)
#define ASSERT_FALSE(c) ::sf::test::assertFalse((c), __FILE__, __LINE__, #c)
#define ASSERT_EQ(a, b) ::sf::test::assertEq((a), (b), __FILE__, __LINE__, #a, #b)
#define ASSERT_NEQ(a, b) ::sf::test::assertNeq((a), (b), __FILE__, __LINE__, #a, #b)
#define ASSERT_STR_EQ(a, b) ::sf::test::assertStrEq((a), (b), __FILE__, __LINE__, #a, #b)
#define ASSERT_NEAR(a, b, tol) \
  ::sf::test::assertNear((a), (b), (tol), __FILE__, __LINE__, #a, #b, #tol)
