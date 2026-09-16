#ifndef ARCHON_BENCHMARK_MACROS_H
#define ARCHON_BENCHMARK_MACROS_H

// A small homegrown micro-benchmark harness, replacing Catch2's
// BENCHMARK_ADVANCED()/Chronometer. Each BENCHMARK() block runs a short
// untimed warm-up, calibrates how many calls are needed to fill a target
// batch duration (so results aren't dominated by clock resolution), then
// times a number of such batches and reports mean/median/min/max per call.

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

namespace benchmark_detail
{

#if defined(_MSC_VER) && !defined(__clang__)
template <typename T> inline void do_not_optimize(T const &value)
{
    // MSVC has no equivalent to the asm volatile trick below; round-trip
    // through a volatile static to stop the optimizer from proving the
    // computation is dead.
    static volatile T sink;
    sink = value;
}
#else
template <typename T> inline void do_not_optimize(T const &value)
{
    asm volatile("" : : "g"(value) : "memory");
}
#endif

inline std::string format_duration_ns(double ns)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);
    if (ns < 1000.0) {
        oss << ns << " ns";
    } else if (ns < 1'000'000.0) {
        oss << (ns / 1000.0) << " us";
    } else {
        oss << (ns / 1'000'000.0) << " ms";
    }
    return oss.str();
}

// Passed by reference into each BENCHMARK() block as `meter`; call
// meter.measure(fn) once with the code to be timed, mirroring Catch2's
// Chronometer::measure(). `fn` may return a value (as the existing
// benchmarks already do via a "dummy accumulator") which is fed through
// do_not_optimize() on every call to prevent the optimizer from eliminating
// the measured work.
class Meter {
public:
    template <typename Fn> void measure(Fn fn)
    {
        using clock = std::chrono::steady_clock;
        constexpr double target_batch_ns = 1'000'000.0; // ~1ms per batch
        constexpr int num_samples = 30;

        // Untimed warm-up (first-call effects: page faults, cold caches, ...)
        do_not_optimize(fn());

        // Calibrate how many calls make up one ~1ms batch.
        auto calib_start = clock::now();
        do_not_optimize(fn());
        double single_call_ns =
            std::chrono::duration<double, std::nano>(clock::now() - calib_start)
                .count();

        std::size_t runs_per_batch = 1;
        if (single_call_ns > 0.0 && single_call_ns < target_batch_ns) {
            runs_per_batch =
                static_cast<std::size_t>(target_batch_ns / single_call_ns) + 1;
        }

        samples_ns_.clear();
        samples_ns_.reserve(num_samples);
        for (int s = 0; s < num_samples; ++s) {
            auto start = clock::now();
            for (std::size_t r = 0; r < runs_per_batch; ++r) {
                do_not_optimize(fn());
            }
            double elapsed_ns =
                std::chrono::duration<double, std::nano>(clock::now() - start)
                    .count();
            samples_ns_.push_back(elapsed_ns /
                                  static_cast<double>(runs_per_batch));
        }
    }

    const std::vector<double> &samples_ns() const { return samples_ns_; }

private:
    std::vector<double> samples_ns_;
};

inline void report(const char *name, std::vector<double> samples_ns)
{
    std::sort(samples_ns.begin(), samples_ns.end());
    double sum = std::accumulate(samples_ns.begin(), samples_ns.end(), 0.0);
    double mean = sum / static_cast<double>(samples_ns.size());
    double median = samples_ns[samples_ns.size() / 2];
    double min = samples_ns.front();
    double max = samples_ns.back();

    std::cout << std::left << std::setw(58) << name << std::right
              << " mean " << std::setw(10) << format_duration_ns(mean)
              << "  median " << std::setw(10) << format_duration_ns(median)
              << "  min " << std::setw(10) << format_duration_ns(min)
              << "  max " << std::setw(10) << format_duration_ns(max)
              << std::endl;
}

template <typename Fn> void run_benchmark(const char *name, Fn fn)
{
    Meter meter;
    fn(meter);
    report(name, meter.samples_ns());
}

} // namespace benchmark_detail

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define BENCHMARK(name, ...)                                                  \
    ::benchmark_detail::run_benchmark(                                        \
        name, [&](::benchmark_detail::Meter &meter) __VA_ARGS__)

#endif
