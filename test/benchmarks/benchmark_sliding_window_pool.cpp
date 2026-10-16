/**
 * @file benchmark_sliding_window_pool.cpp
 * @brief Benchmark sliding window stats with and without memory pool
 */

#include <iostream>
#include <chrono>
#include <vector>
#include <iomanip>
#include <cstring>

// Headers have their own extern "C" guards
#include "middleware/buffering/nimcp_sliding_window.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/memory/nimcp_memory_pool.h"

using namespace std::chrono;

constexpr size_t WINDOW_SIZE = 1024;
constexpr uint32_t NUM_ITERATIONS = 1000;

/**
 * @brief Simulate OLD stats recalculation (malloc/free)
 */
double benchmark_old_stats_recalc() {
    // Create test data
    std::vector<float> samples(WINDOW_SIZE);
    for (size_t i = 0; i < WINDOW_SIZE; i++) {
        samples[i] = static_cast<float>(i) * 0.1f;
    }

    auto start = high_resolution_clock::now();

    // Simulate repeated stats recalculation (old way: malloc + free each time)
    for (uint32_t iter = 0; iter < NUM_ITERATIONS; iter++) {
        // Allocate temp buffer (OLD WAY)
        float* temp = (float*)nimcp_malloc(WINDOW_SIZE * sizeof(float));

        // Simulate stats calculation
        std::memcpy(temp, samples.data(), WINDOW_SIZE * sizeof(float));

        volatile double sum = 0.0;
        for (size_t i = 0; i < WINDOW_SIZE; i++) {
            sum += temp[i];
        }

        // Free temp buffer
        nimcp_free(temp);
    }

    auto end = high_resolution_clock::now();
    return duration_cast<microseconds>(end - start).count();
}

/**
 * @brief Simulate NEW stats recalculation (memory pool)
 */
double benchmark_new_stats_recalc() {
    // Create test data
    std::vector<float> samples(WINDOW_SIZE);
    for (size_t i = 0; i < WINDOW_SIZE; i++) {
        samples[i] = static_cast<float>(i) * 0.1f;
    }

    // Create memory pool (NEW WAY)
    memory_pool_config_t pool_config = {
        .block_size = WINDOW_SIZE * sizeof(float),
        .num_blocks = 2,
        .alignment = 16,
        .enable_tracking = false,
        .enable_guard_pages = false
    };
    memory_pool_t pool = memory_pool_create(&pool_config);

    auto start = high_resolution_clock::now();

    // Simulate repeated stats recalculation (new way: pool acquire/release)
    for (uint32_t iter = 0; iter < NUM_ITERATIONS; iter++) {
        // Acquire from pool (NEW WAY)
        float* temp = (float*)memory_pool_acquire(pool);

        // Simulate stats calculation
        std::memcpy(temp, samples.data(), WINDOW_SIZE * sizeof(float));

        volatile double sum = 0.0;
        for (size_t i = 0; i < WINDOW_SIZE; i++) {
            sum += temp[i];
        }

        // Release to pool
        memory_pool_release(pool, temp);
    }

    auto end = high_resolution_clock::now();

    memory_pool_destroy(pool);

    return duration_cast<microseconds>(end - start).count();
}

/**
 * @brief Test actual sliding window with integrated memory pool
 */
double benchmark_real_sliding_window() {
    // Create sliding window with memory pool integrated
    sliding_window_t* window = sliding_window_create(WINDOW_SIZE, 0);

    // Fill window
    for (size_t i = 0; i < WINDOW_SIZE; i++) {
        sliding_window_add(window, static_cast<float>(i) * 0.1f);
    }

    auto start = high_resolution_clock::now();

    // Trigger stats recalculation multiple times
    // (normally happens every 1000 adds, we force it)
    for (uint32_t iter = 0; iter < NUM_ITERATIONS; iter++) {
        sliding_window_reset_stats(window);  // Forces recalculation
    }

    auto end = high_resolution_clock::now();

    sliding_window_destroy(window);

    return duration_cast<microseconds>(end - start).count();
}

int main() {
    std::cout << "\n";
    std::cout << "=================================================================\n";
    std::cout << "   Sliding Window Stats: malloc vs Memory Pool Performance\n";
    std::cout << "=================================================================\n\n";

    std::cout << "Configuration:\n";
    std::cout << "  Window size:   " << WINDOW_SIZE << " floats (" << (WINDOW_SIZE * 4) << " bytes)\n";
    std::cout << "  Iterations:    " << NUM_ITERATIONS << " stats recalculations\n";
    std::cout << "\n";

    std::cout << "Running benchmarks (3 runs each)...\n\n";

    // Warmup
    benchmark_old_stats_recalc();
    benchmark_new_stats_recalc();
    benchmark_real_sliding_window();

    // Run multiple times and average
    double old_total = 0.0, new_total = 0.0, real_total = 0.0;
    const int runs = 3;

    for (int i = 0; i < runs; i++) {
        old_total += benchmark_old_stats_recalc();
        new_total += benchmark_new_stats_recalc();
        real_total += benchmark_real_sliding_window();
    }

    double old_avg = old_total / runs;
    double new_avg = new_total / runs;
    double real_avg = real_total / runs;

    std::cout << std::fixed << std::setprecision(2);

    std::cout << "Results:\n";
    std::cout << "-----------------------------------------------------------------\n";
    std::cout << "  OLD (malloc/free):       " << std::setw(10) << old_avg << " µs total\n";
    std::cout << "                           " << std::setw(10) << (old_avg * 1000.0 / NUM_ITERATIONS) << " ns per recalc\n";
    std::cout << "\n";
    std::cout << "  NEW (memory pool):       " << std::setw(10) << new_avg << " µs total\n";
    std::cout << "                           " << std::setw(10) << (new_avg * 1000.0 / NUM_ITERATIONS) << " ns per recalc\n";
    std::cout << "\n";
    std::cout << "  REAL (sliding window):   " << std::setw(10) << real_avg << " µs total\n";
    std::cout << "                           " << std::setw(10) << (real_avg * 1000.0 / NUM_ITERATIONS) << " ns per recalc\n";
    std::cout << "\n";

    double speedup = old_avg / new_avg;
    double real_speedup = old_avg / real_avg;
    double improvement_pct = ((old_avg - new_avg) / old_avg) * 100.0;
    double time_saved_per_iter_ns = ((old_avg - new_avg) * 1000.0) / NUM_ITERATIONS;

    std::cout << "=================================================================\n";
    std::cout << "Performance Improvement:\n";
    std::cout << "=================================================================\n";
    std::cout << "  Simulated Speedup:    " << speedup << "x faster\n";
    std::cout << "  Real Speedup:         " << real_speedup << "x faster\n";
    std::cout << "  Latency Reduction:    " << improvement_pct << "%\n";
    std::cout << "  Time Saved per Op:    " << time_saved_per_iter_ns << " ns\n";
    std::cout << "\n";

    if (speedup >= 10.0) {
        std::cout << "  ✅ EXCELLENT: Memory pool provides huge performance improvement!\n";
    } else if (speedup >= 3.0) {
        std::cout << "  ✅ GOOD: Memory pool provides significant performance improvement\n";
    } else if (speedup >= 1.5) {
        std::cout << "  ✓ MODERATE: Memory pool provides decent performance improvement\n";
    } else {
        std::cout << "  ⚠️  Speedup is modest - benefits may vary by workload\n";
    }

    std::cout << "\n";
    std::cout << "Analysis:\n";
    std::cout << "-----------------------------------------------------------------\n";
    std::cout << "  malloc overhead:      ~" << (old_avg * 1000.0 / NUM_ITERATIONS) << " ns per allocation\n";
    std::cout << "  pool overhead:        ~" << (new_avg * 1000.0 / NUM_ITERATIONS) << " ns per acquisition\n";
    std::cout << "  Time saved:           ~" << time_saved_per_iter_ns << " ns per operation\n";
    std::cout << "\n";
    std::cout << "  For 1000 channels × 3 timescales × 1000 recalcs:\n";
    std::cout << "  Total time saved:     ~" << (time_saved_per_iter_ns * 3000000 / 1000000.0) << " ms\n";
    std::cout << "=================================================================\n\n";

    return 0;
}
