/**
 * @file concurrency_test.h
 * @brief Concurrency testing utilities for NIMCP brain operations
 *
 * WHAT: Utilities for launching concurrent operations and detecting race conditions
 * WHY:  NIMCP brain operations must be thread-safe for multi-threaded environments
 * HOW:  Provide thread barriers, concurrent operation launchers, and race detectors
 *
 * USAGE:
 *   // Launch concurrent operations
 *   ConcurrentRunner runner(8);  // 8 threads
 *   runner.run([&brain](int thread_id) {
 *       brain_process(brain, input);
 *   });
 *
 *   // With barrier synchronization
 *   runner.run_with_barrier([&brain](int thread_id, Barrier& barrier) {
 *       // Setup phase
 *       auto data = prepare_data(thread_id);
 *       barrier.wait();  // All threads start processing together
 *       brain_process(brain, data);
 *   });
 *
 * NIMCP STANDARDS:
 * - Thread-safe utilities
 * - RAII-based resource management
 * - Compatible with ThreadSanitizer
 */

#ifndef NIMCP_TEST_CONCURRENCY_TEST_H
#define NIMCP_TEST_CONCURRENCY_TEST_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace nimcp {
namespace test {

// ============================================================================
// Barrier - Thread synchronization primitive
// ============================================================================

/**
 * @brief Reusable thread barrier for synchronized concurrent tests
 *
 * All threads must call wait() before any can proceed.
 * Barrier resets automatically for reuse.
 */
class Barrier {
public:
    explicit Barrier(uint32_t thread_count)
        : threshold_(thread_count)
        , count_(thread_count)
        , generation_(0) {}

    /**
     * @brief Wait at barrier until all threads arrive
     */
    void wait() {
        std::unique_lock<std::mutex> lock(mutex_);
        auto gen = generation_;

        if (--count_ == 0) {
            // Last thread to arrive
            generation_++;
            count_ = threshold_;
            cv_.notify_all();
        } else {
            // Wait for other threads
            cv_.wait(lock, [this, gen] { return gen != generation_; });
        }
    }

    /**
     * @brief Reset barrier for new thread count
     * @param thread_count New number of threads
     */
    void reset(uint32_t thread_count) {
        std::lock_guard<std::mutex> lock(mutex_);
        threshold_ = thread_count;
        count_ = thread_count;
        generation_ = 0;
    }

private:
    std::mutex mutex_;
    std::condition_variable cv_;
    uint32_t threshold_;
    uint32_t count_;
    uint32_t generation_;
};

// ============================================================================
// ConcurrentRunner - Execute operations across multiple threads
// ============================================================================

/**
 * @brief Execute a function concurrently across multiple threads
 *
 * Provides utilities for testing thread safety of NIMCP operations.
 */
class ConcurrentRunner {
public:
    /**
     * @brief Create runner with specified thread count
     * @param thread_count Number of threads to use
     */
    explicit ConcurrentRunner(uint32_t thread_count)
        : thread_count_(thread_count)
        , barrier_(thread_count) {}

    /**
     * @brief Run function concurrently on all threads
     * @param func Function taking thread_id (0 to thread_count-1)
     *
     * Blocks until all threads complete.
     */
    void run(std::function<void(int)> func) {
        std::vector<std::thread> threads;
        threads.reserve(thread_count_);

        for (uint32_t i = 0; i < thread_count_; ++i) {
            threads.emplace_back(func, static_cast<int>(i));
        }

        for (auto& t : threads) {
            t.join();
        }
    }

    /**
     * @brief Run function with barrier synchronization
     * @param func Function taking thread_id and barrier reference
     *
     * Allows threads to synchronize at specific points during execution.
     */
    void run_with_barrier(std::function<void(int, Barrier&)> func) {
        std::vector<std::thread> threads;
        threads.reserve(thread_count_);

        for (uint32_t i = 0; i < thread_count_; ++i) {
            threads.emplace_back([this, func, i]() {
                func(static_cast<int>(i), barrier_);
            });
        }

        for (auto& t : threads) {
            t.join();
        }
    }

    /**
     * @brief Run function repeatedly for duration
     * @param func Function to execute
     * @param duration How long to run
     * @return Total iterations completed across all threads
     */
    uint64_t run_for_duration(
        std::function<void(int)> func,
        std::chrono::milliseconds duration) {

        std::atomic<bool> running{true};
        std::atomic<uint64_t> total_iterations{0};
        std::vector<std::thread> threads;
        threads.reserve(thread_count_);

        auto start = std::chrono::steady_clock::now();

        for (uint32_t i = 0; i < thread_count_; ++i) {
            threads.emplace_back([&, i]() {
                uint64_t local_iterations = 0;
                while (running.load(std::memory_order_relaxed)) {
                    func(static_cast<int>(i));
                    local_iterations++;
                }
                total_iterations.fetch_add(local_iterations,
                                          std::memory_order_relaxed);
            });
        }

        std::this_thread::sleep_for(duration);
        running.store(false, std::memory_order_relaxed);

        for (auto& t : threads) {
            t.join();
        }

        return total_iterations.load();
    }

    /**
     * @brief Run function with staggered start times
     * @param func Function to execute
     * @param stagger_ms Milliseconds between thread starts
     */
    void run_staggered(std::function<void(int)> func, uint32_t stagger_ms) {
        std::vector<std::thread> threads;
        threads.reserve(thread_count_);

        for (uint32_t i = 0; i < thread_count_; ++i) {
            threads.emplace_back(func, static_cast<int>(i));
            if (i < thread_count_ - 1) {
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(stagger_ms));
            }
        }

        for (auto& t : threads) {
            t.join();
        }
    }

    /**
     * @brief Get thread count
     * @return Number of threads
     */
    uint32_t thread_count() const {
        return thread_count_;
    }

private:
    uint32_t thread_count_;
    Barrier barrier_;
};

// ============================================================================
// Race Condition Detectors
// ============================================================================

/**
 * @brief Detect potential data races on a shared counter
 *
 * Uses compare-and-swap to detect if multiple threads are racing.
 */
class RaceDetector {
public:
    RaceDetector() : value_(0), race_detected_(false) {}

    /**
     * @brief Increment counter (call from each thread)
     * @return true if race detected
     */
    bool increment() {
        uint64_t expected = value_.load(std::memory_order_relaxed);
        uint64_t desired = expected + 1;

        // If CAS fails, another thread modified concurrently
        if (!value_.compare_exchange_strong(expected, desired,
                                           std::memory_order_seq_cst)) {
            race_detected_.store(true, std::memory_order_relaxed);
            return true;
        }
        return false;
    }

    /**
     * @brief Check if any race was detected
     * @return true if race detected
     */
    bool race_detected() const {
        return race_detected_.load(std::memory_order_relaxed);
    }

    /**
     * @brief Reset detector
     */
    void reset() {
        value_.store(0, std::memory_order_relaxed);
        race_detected_.store(false, std::memory_order_relaxed);
    }

    /**
     * @brief Get current value
     * @return Counter value
     */
    uint64_t value() const {
        return value_.load(std::memory_order_relaxed);
    }

private:
    std::atomic<uint64_t> value_;
    std::atomic<bool> race_detected_;
};

/**
 * @brief Verify atomic operations are properly ordered
 *
 * Detects improper memory ordering in lock-free code.
 */
class OrderingChecker {
public:
    OrderingChecker() : x_(0), y_(0), r1_(0), r2_(0) {}

    /**
     * @brief Thread 1 operation
     */
    void thread1_write() {
        x_.store(1, std::memory_order_seq_cst);
        r1_ = y_.load(std::memory_order_seq_cst);
    }

    /**
     * @brief Thread 2 operation
     */
    void thread2_write() {
        y_.store(1, std::memory_order_seq_cst);
        r2_ = x_.load(std::memory_order_seq_cst);
    }

    /**
     * @brief Check if memory ordering violation occurred
     * @return true if both threads saw 0 (impossible with seq_cst)
     */
    bool ordering_violated() const {
        // Under sequential consistency, at least one thread must see 1
        return (r1_ == 0 && r2_ == 0);
    }

    /**
     * @brief Reset for next test
     */
    void reset() {
        x_.store(0, std::memory_order_relaxed);
        y_.store(0, std::memory_order_relaxed);
        r1_ = 0;
        r2_ = 0;
    }

private:
    std::atomic<int> x_, y_;
    int r1_, r2_;
};

// ============================================================================
// Stress Test Utilities
// ============================================================================

/**
 * @brief Run stress test with configurable parameters
 */
struct StressTestConfig {
    uint32_t thread_count = 8;
    uint32_t iterations_per_thread = 1000;
    uint32_t warmup_iterations = 100;
    std::chrono::milliseconds timeout{30000};  // 30 seconds
    bool verbose = false;
};

/**
 * @brief Results from stress test
 */
struct StressTestResult {
    uint64_t total_iterations = 0;
    uint64_t errors = 0;
    std::chrono::microseconds elapsed{0};
    bool timeout_reached = false;
    bool passed = false;
};

/**
 * @brief Run stress test with given function
 * @param config Test configuration
 * @param func Function to stress test (returns true on success)
 * @return Test results
 */
inline StressTestResult run_stress_test(
    const StressTestConfig& config,
    std::function<bool(int thread_id, int iteration)> func) {

    StressTestResult result;
    std::atomic<uint64_t> errors{0};
    std::atomic<uint64_t> iterations{0};
    std::atomic<bool> running{true};

    auto start = std::chrono::steady_clock::now();

    ConcurrentRunner runner(config.thread_count);

    // Start timeout monitor
    std::thread timeout_monitor([&]() {
        std::this_thread::sleep_for(config.timeout);
        running.store(false, std::memory_order_relaxed);
    });

    runner.run([&](int thread_id) {
        // Warmup
        for (uint32_t i = 0; i < config.warmup_iterations; ++i) {
            if (!running.load(std::memory_order_relaxed)) break;
            func(thread_id, -static_cast<int>(i) - 1);
        }

        // Main test
        for (uint32_t i = 0; i < config.iterations_per_thread; ++i) {
            if (!running.load(std::memory_order_relaxed)) break;

            if (!func(thread_id, static_cast<int>(i))) {
                errors.fetch_add(1, std::memory_order_relaxed);
            }
            iterations.fetch_add(1, std::memory_order_relaxed);
        }
    });

    running.store(false, std::memory_order_relaxed);
    timeout_monitor.join();

    auto end = std::chrono::steady_clock::now();

    result.total_iterations = iterations.load();
    result.errors = errors.load();
    result.elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        end - start);
    result.timeout_reached =
        result.total_iterations <
        config.thread_count * config.iterations_per_thread;
    result.passed = (result.errors == 0 && !result.timeout_reached);

    return result;
}

// ============================================================================
// Thread Sanitizer Annotations
// ============================================================================

// These annotations help TSan understand intentional races (if any)
#if defined(__SANITIZE_THREAD__) || defined(__has_feature)
#if defined(__has_feature)
#if __has_feature(thread_sanitizer)
#define NIMCP_TSAN_ENABLED 1
#endif
#else
#define NIMCP_TSAN_ENABLED 1
#endif
#endif

#ifdef NIMCP_TSAN_ENABLED
extern "C" {
void __tsan_acquire(void* addr);
void __tsan_release(void* addr);
}
#define NIMCP_TSAN_ACQUIRE(addr) __tsan_acquire(addr)
#define NIMCP_TSAN_RELEASE(addr) __tsan_release(addr)
#else
#define NIMCP_TSAN_ACQUIRE(addr) ((void)(addr))
#define NIMCP_TSAN_RELEASE(addr) ((void)(addr))
#endif

}  // namespace test
}  // namespace nimcp

#endif  // NIMCP_TEST_CONCURRENCY_TEST_H
