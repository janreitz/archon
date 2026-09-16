#ifndef TEST_MACROS_H
#define TEST_MACROS_H

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <source_location>

// std::source_location::line() returns uint_least32_t, which happens to be
// exactly `unsigned int` on mainstream platforms (making the cast below a
// no-op there) but isn't guaranteed to be, so the cast stays for portability.
// GCC's -Wuseless-cast doesn't know that and flags it on platforms where it
// is a no-op; Clang has no such warning, and doesn't recognize the pragma
// name, so only GCC gets the suppression.
#if defined(__GNUC__) && !defined(__clang__)
#define TEST_MACROS_SUPPRESS_USELESS_CAST_BEGIN                               \
    _Pragma("GCC diagnostic push")                                            \
        _Pragma("GCC diagnostic ignored \"-Wuseless-cast\"")
#define TEST_MACROS_SUPPRESS_USELESS_CAST_END _Pragma("GCC diagnostic pop")
#else
#define TEST_MACROS_SUPPRESS_USELESS_CAST_BEGIN
#define TEST_MACROS_SUPPRESS_USELESS_CAST_END
#endif

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
    TEST_MACROS_SUPPRESS_USELESS_CAST_BEGIN
    std::fprintf(stderr,
                 "%s:%u: assertion '%s' failed in '%s': ", loc.file_name(),
                 static_cast<unsigned>(loc.line()), expr, loc.function_name());
    TEST_MACROS_SUPPRESS_USELESS_CAST_END
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
    TEST_MACROS_SUPPRESS_USELESS_CAST_BEGIN
    std::fprintf(stdout,
                 "%s:%u: expectation '%s' failed in '%s': ", loc.file_name(),
                 static_cast<unsigned>(loc.line()), expr, loc.function_name());
    TEST_MACROS_SUPPRESS_USELESS_CAST_END
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
    ((cond)                                                                    \
         ? void(0)                                                             \
         : ::test_detail::require_fail(#cond, std::source_location::current(), \
                                       __VA_ARGS__))

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define EXPECT(cond, ...)                                                      \
    ((cond)                                                                    \
         ? void(0)                                                             \
         : ::test_detail::expect_fail(                                 \
               #cond, std::source_location::current(), __VA_ARGS__))

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define RUN_TEST(fn) ::test_detail::run(#fn, fn)

#endif