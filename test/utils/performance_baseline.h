/**
 * @file performance_baseline.h
 * @brief Performance baseline tracking and timing assertions for NIMCP tests
 *
 * WHAT: Utilities for tracking, storing, and asserting performance baselines
 * WHY:  Detect performance regressions in critical NIMCP operations
 * HOW:  Store baselines in JSON, compare against thresholds, report regressions
 *
 * USAGE:
 *   // Simple timing assertion
 *   EXPECT_TIMING("brain_create", std::chrono::milliseconds(100), {
 *       brain = brain_create("test", BRAIN_SIZE_TINY, ...);
 *   });
 *
 *   // With baseline tracking
 *   PerformanceTracker tracker("test_results.json");
 *   auto result = tracker.measure("brain_process", [&]() {
 *       brain_process(brain, input);
 *   });
 *   EXPECT_TRUE(result.within_baseline(1.2f));  // Within 20% of baseline
 *
 * NIMCP STANDARDS:
 * - Minimal overhead when not actively measuring
 * - JSON storage for baseline persistence
 * - Statistical analysis for stable baselines
 */

#ifndef NIMCP_TEST_PERFORMANCE_BASELINE_H
#define NIMCP_TEST_PERFORMANCE_BASELINE_H

#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iomanip>
#include <map>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

namespace nimcp {
namespace test {

// ============================================================================
// Timing Utilities
// ============================================================================

/**
 * @brief High-resolution timer for performance measurements
 */
class Timer {
public:
    Timer() : running_(false), elapsed_ns_(0) {}

    void start() {
        start_time_ = std::chrono::high_resolution_clock::now();
        running_ = true;
    }

    void stop() {
        if (running_) {
            auto end = std::chrono::high_resolution_clock::now();
            elapsed_ns_ = std::chrono::duration_cast<std::chrono::nanoseconds>(
                end - start_time_).count();
            running_ = false;
        }
    }

    void reset() {
        running_ = false;
        elapsed_ns_ = 0;
    }

    int64_t elapsed_ns() const { return elapsed_ns_; }
    int64_t elapsed_us() const { return elapsed_ns_ / 1000; }
    int64_t elapsed_ms() const { return elapsed_ns_ / 1000000; }
    double elapsed_seconds() const { return elapsed_ns_ / 1e9; }

    bool running() const { return running_; }

private:
    std::chrono::high_resolution_clock::time_point start_time_;
    bool running_;
    int64_t elapsed_ns_;
};

/**
 * @brief RAII timer that measures scope duration
 */
class ScopedTimer {
public:
    explicit ScopedTimer(int64_t* output_ns) : output_(output_ns) {
        timer_.start();
    }

    ~ScopedTimer() {
        timer_.stop();
        if (output_) {
            *output_ = timer_.elapsed_ns();
        }
    }

    // Non-copyable
    ScopedTimer(const ScopedTimer&) = delete;
    ScopedTimer& operator=(const ScopedTimer&) = delete;

private:
    Timer timer_;
    int64_t* output_;
};

// ============================================================================
// Measurement Result
// ============================================================================

/**
 * @brief Result of a performance measurement
 */
struct MeasurementResult {
    std::string name;
    int64_t elapsed_ns = 0;
    int64_t baseline_ns = 0;
    bool has_baseline = false;

    double elapsed_ms() const { return elapsed_ns / 1e6; }
    double baseline_ms() const { return baseline_ns / 1e6; }

    /**
     * @brief Check if measurement is within tolerance of baseline
     * @param tolerance_factor e.g., 1.2 means within 20% slower
     */
    bool within_baseline(float tolerance_factor = 1.2f) const {
        if (!has_baseline) return true;  // No baseline to compare
        return elapsed_ns <= static_cast<int64_t>(baseline_ns * tolerance_factor);
    }

    /**
     * @brief Get ratio of elapsed to baseline
     */
    double ratio() const {
        if (!has_baseline || baseline_ns == 0) return 0.0;
        return static_cast<double>(elapsed_ns) / baseline_ns;
    }

    /**
     * @brief Get human-readable result string
     */
    std::string to_string() const {
        std::ostringstream oss;
        oss << name << ": " << std::fixed << std::setprecision(2)
            << elapsed_ms() << "ms";
        if (has_baseline) {
            oss << " (baseline: " << baseline_ms() << "ms, ratio: "
                << std::setprecision(2) << ratio() << "x)";
        }
        return oss.str();
    }
};

// ============================================================================
// Statistical Analysis
// ============================================================================

/**
 * @brief Statistics for a series of measurements
 */
struct MeasurementStats {
    double mean_ns = 0;
    double stddev_ns = 0;
    double median_ns = 0;
    double min_ns = 0;
    double max_ns = 0;
    double p95_ns = 0;
    double p99_ns = 0;
    size_t sample_count = 0;

    double mean_ms() const { return mean_ns / 1e6; }
    double stddev_ms() const { return stddev_ns / 1e6; }
    double median_ms() const { return median_ns / 1e6; }
    double min_ms() const { return min_ns / 1e6; }
    double max_ms() const { return max_ns / 1e6; }
    double p95_ms() const { return p95_ns / 1e6; }
    double p99_ms() const { return p99_ns / 1e6; }

    /**
     * @brief Calculate coefficient of variation (stddev/mean)
     */
    double cv() const {
        return mean_ns > 0 ? stddev_ns / mean_ns : 0;
    }

    std::string to_string() const {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2);
        oss << "mean=" << mean_ms() << "ms, "
            << "stddev=" << stddev_ms() << "ms, "
            << "median=" << median_ms() << "ms, "
            << "min=" << min_ms() << "ms, "
            << "max=" << max_ms() << "ms, "
            << "p95=" << p95_ms() << "ms, "
            << "p99=" << p99_ms() << "ms, "
            << "n=" << sample_count;
        return oss.str();
    }
};

/**
 * @brief Calculate statistics from samples
 */
inline MeasurementStats calculate_stats(std::vector<int64_t> samples) {
    MeasurementStats stats;

    if (samples.empty()) return stats;

    stats.sample_count = samples.size();

    // Sort for percentiles
    std::sort(samples.begin(), samples.end());

    stats.min_ns = static_cast<double>(samples.front());
    stats.max_ns = static_cast<double>(samples.back());

    // Median
    size_t mid = samples.size() / 2;
    stats.median_ns = (samples.size() % 2 == 0)
        ? (samples[mid - 1] + samples[mid]) / 2.0
        : static_cast<double>(samples[mid]);

    // Mean
    double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
    stats.mean_ns = sum / samples.size();

    // Standard deviation
    double sq_sum = std::accumulate(samples.begin(), samples.end(), 0.0,
        [&](double acc, int64_t val) {
            double diff = val - stats.mean_ns;
            return acc + diff * diff;
        });
    stats.stddev_ns = std::sqrt(sq_sum / samples.size());

    // Percentiles
    size_t p95_idx = static_cast<size_t>(samples.size() * 0.95);
    size_t p99_idx = static_cast<size_t>(samples.size() * 0.99);
    stats.p95_ns = static_cast<double>(samples[std::min(p95_idx, samples.size() - 1)]);
    stats.p99_ns = static_cast<double>(samples[std::min(p99_idx, samples.size() - 1)]);

    return stats;
}

// ============================================================================
// Performance Tracker
// ============================================================================

/**
 * @brief Track and store performance baselines
 */
class PerformanceTracker {
public:
    /**
     * @brief Create tracker with baseline file
     * @param baseline_file Path to JSON baseline file
     */
    explicit PerformanceTracker(const std::string& baseline_file = "")
        : baseline_file_(baseline_file) {
        if (!baseline_file_.empty()) {
            load_baselines();
        }
    }

    ~PerformanceTracker() {
        if (!baseline_file_.empty() && !measurements_.empty()) {
            save_baselines();
        }
    }

    /**
     * @brief Measure a single operation
     * @param name Operation name
     * @param func Function to measure
     * @return Measurement result
     */
    template<typename Func>
    MeasurementResult measure(const std::string& name, Func&& func) {
        Timer timer;
        timer.start();
        func();
        timer.stop();

        MeasurementResult result;
        result.name = name;
        result.elapsed_ns = timer.elapsed_ns();

        auto it = baselines_.find(name);
        if (it != baselines_.end()) {
            result.baseline_ns = it->second;
            result.has_baseline = true;
        }

        measurements_[name].push_back(result.elapsed_ns);
        return result;
    }

    /**
     * @brief Measure operation multiple times and return statistics
     * @param name Operation name
     * @param iterations Number of iterations
     * @param func Function to measure
     * @return Statistics from all iterations
     */
    template<typename Func>
    MeasurementStats measure_stats(const std::string& name,
                                   size_t iterations,
                                   Func&& func) {
        std::vector<int64_t> samples;
        samples.reserve(iterations);

        for (size_t i = 0; i < iterations; ++i) {
            Timer timer;
            timer.start();
            func();
            timer.stop();
            samples.push_back(timer.elapsed_ns());
        }

        measurements_[name].insert(measurements_[name].end(),
                                   samples.begin(), samples.end());

        return calculate_stats(samples);
    }

    /**
     * @brief Set baseline for an operation
     * @param name Operation name
     * @param baseline_ns Baseline time in nanoseconds
     */
    void set_baseline(const std::string& name, int64_t baseline_ns) {
        baselines_[name] = baseline_ns;
    }

    /**
     * @brief Get baseline for an operation
     * @param name Operation name
     * @return Baseline in nanoseconds, or 0 if not found
     */
    int64_t get_baseline(const std::string& name) const {
        auto it = baselines_.find(name);
        return (it != baselines_.end()) ? it->second : 0;
    }

    /**
     * @brief Check if operation has a baseline
     */
    bool has_baseline(const std::string& name) const {
        return baselines_.find(name) != baselines_.end();
    }

    /**
     * @brief Update baselines from current measurements
     *
     * Uses median of measurements as new baseline.
     */
    void update_baselines_from_measurements() {
        for (const auto& [name, samples] : measurements_) {
            if (!samples.empty()) {
                auto stats = calculate_stats(
                    std::vector<int64_t>(samples.begin(), samples.end()));
                baselines_[name] = static_cast<int64_t>(stats.median_ns);
            }
        }
    }

    /**
     * @brief Generate report of all measurements
     */
    std::string generate_report() const {
        std::ostringstream oss;
        oss << "=== Performance Report ===\n\n";

        for (const auto& [name, samples] : measurements_) {
            auto stats = calculate_stats(
                std::vector<int64_t>(samples.begin(), samples.end()));

            oss << name << ":\n";
            oss << "  " << stats.to_string() << "\n";

            auto baseline_it = baselines_.find(name);
            if (baseline_it != baselines_.end()) {
                double ratio = stats.mean_ns / baseline_it->second;
                oss << "  baseline: " << (baseline_it->second / 1e6)
                    << "ms, ratio: " << std::fixed << std::setprecision(2)
                    << ratio << "x\n";
            }
            oss << "\n";
        }

        return oss.str();
    }

    /**
     * @brief Clear all measurements (keeps baselines)
     */
    void clear_measurements() {
        measurements_.clear();
    }

private:
    void load_baselines() {
        std::ifstream file(baseline_file_);
        if (!file.is_open()) return;

        // Simple JSON-like parsing (name: value format)
        std::string line;
        while (std::getline(file, line)) {
            // Skip comments and empty lines
            if (line.empty() || line[0] == '#') continue;

            auto colon_pos = line.find(':');
            if (colon_pos != std::string::npos) {
                std::string name = line.substr(0, colon_pos);
                std::string value_str = line.substr(colon_pos + 1);

                // Trim whitespace
                name.erase(0, name.find_first_not_of(" \t\""));
                name.erase(name.find_last_not_of(" \t\"") + 1);
                value_str.erase(0, value_str.find_first_not_of(" \t"));
                value_str.erase(value_str.find_last_not_of(" \t,") + 1);

                try {
                    baselines_[name] = std::stoll(value_str);
                } catch (...) {
                    // Skip invalid entries
                }
            }
        }
    }

    void save_baselines() {
        std::ofstream file(baseline_file_);
        if (!file.is_open()) return;

        file << "# NIMCP Performance Baselines\n";
        file << "# Generated: " << __DATE__ << " " << __TIME__ << "\n";
        file << "# Values in nanoseconds\n\n";

        for (const auto& [name, value] : baselines_) {
            file << "\"" << name << "\": " << value << ",\n";
        }
    }

    std::string baseline_file_;
    std::map<std::string, int64_t> baselines_;
    std::map<std::string, std::vector<int64_t>> measurements_;
};

// ============================================================================
// Timing Assertion Macros
// ============================================================================

/**
 * @brief Assert that code block completes within time limit
 *
 * USAGE:
 *   EXPECT_WITHIN_MS(100, {
 *       brain_create(...);
 *   });
 */
#define EXPECT_WITHIN_MS(max_ms, code_block) \
    do { \
        ::nimcp::test::Timer _timer; \
        _timer.start(); \
        code_block; \
        _timer.stop(); \
        EXPECT_LE(_timer.elapsed_ms(), (max_ms)) \
            << "Operation took " << _timer.elapsed_ms() << "ms, " \
            << "expected <= " << (max_ms) << "ms"; \
    } while(0)

#define ASSERT_WITHIN_MS(max_ms, code_block) \
    do { \
        ::nimcp::test::Timer _timer; \
        _timer.start(); \
        code_block; \
        _timer.stop(); \
        ASSERT_LE(_timer.elapsed_ms(), (max_ms)) \
            << "Operation took " << _timer.elapsed_ms() << "ms, " \
            << "expected <= " << (max_ms) << "ms"; \
    } while(0)

/**
 * @brief Named timing assertion with baseline comparison
 *
 * USAGE:
 *   EXPECT_TIMING("brain_create", 100, {
 *       brain = brain_create(...);
 *   });
 */
#define EXPECT_TIMING(name, max_ms, code_block) \
    do { \
        ::nimcp::test::Timer _timer; \
        _timer.start(); \
        code_block; \
        _timer.stop(); \
        EXPECT_LE(_timer.elapsed_ms(), (max_ms)) \
            << name << " took " << _timer.elapsed_ms() << "ms, " \
            << "expected <= " << (max_ms) << "ms"; \
    } while(0)

}  // namespace test
}  // namespace nimcp

#endif  // NIMCP_TEST_PERFORMANCE_BASELINE_H
