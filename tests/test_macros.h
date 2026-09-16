#ifndef TEST_MACROS_H
#define TEST_MACROS_H

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace test_detail
{

// Counts failed expectations so the test binary can still exit non-zero.
// 'inline' makes this a single variable shared across translation units.
inline int expect_failures = 0;

// Tells GCC and Clang that parameter 3 is a printf-style format string and the
// variadic arguments start at position 4. The compiler then checks call sites
// exactly as it does for printf.
[[noreturn, gnu::cold, gnu::format(printf, 3, 4)]]
inline void require_fail(const char *expr, std::source_location loc,
                         const char *fmt, ...)
{
    std::fprintf(stderr,
                 "%s:%u: assertion '%s' failed in '%s': ", loc.file_name(),
                 static_cast<unsigned>(loc.line()), expr, loc.function_name());
    va_list args;
    va_start(args, fmt);
    std::vfprintf(stderr, fmt, args);
    va_end(args);
    std::fputc('\n', stderr);
    std::exit(1);
}

[[gnu::cold, gnu::format(printf, 3, 4)]]
inline void expect_fail(const char *expr, std::source_location loc,
                        const char *fmt, ...)
{
    std::fprintf(stdout,
                 "%s:%u: expectation '%s' failed in '%s': ", loc.file_name(),
                 static_cast<unsigned>(loc.line()), expr, loc.function_name());
    va_list args;
    va_start(args, fmt);
    std::vfprintf(stderr, fmt, args);
    va_end(args);
    std::fputc('\n', stderr);
    ++expect_failures;
}

inline void assert_fail() {}

template <typename Fn> void run(const char *name, Fn fn)
{
    std::printf("[ RUN      ] %s\n", name);
    fn();
    std::printf("[       OK ] %s\n", name);
}

} // namespace test_detail

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define REQUIRE(cond, ...)                                                     \
    (static_cast<bool>(cond)                                                   \
         ? void(0)                                                             \
         : ::test_detail::require_fail(#cond, std::source_location::current(), \
                                       __VA_ARGS__))

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define EXPECT(cond, ...)                                                      \
    (static_cast<bool>(cond)                                                   \
         ? void(0)                                                             \
         : ::test_detail::expect_fail(                                 \
               #cond, std::source_location::current(), __VA_ARGS__))

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define RUN_TEST(fn) ::test_detail::run(#fn, fn)

#endif