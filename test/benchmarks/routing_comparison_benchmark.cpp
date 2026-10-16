/**
 * @file routing_comparison_benchmark.cpp
 * @brief Compare old vs new routing approach
 */

#include <iostream>
#include <chrono>
#include <vector>
#include <iomanip>
#include <cstring>
#include "middleware/routing/nimcp_signal_wrapper.h"
// Headers have their own extern "C" guards
#include "utils/memory/nimcp_memory.h"

using namespace std::chrono;

constexpr uint32_t NUM_ROUTES = 1000;  // Number of routing operations
constexpr uint32_t SIGNAL_SIZE = 100;
constexpr uint32_t NUM_DESTS = 4;

// Simulate old routing (deep copy per route)
double benchmark_old_routing() {
    std::vector<uint32_t> dest_ids(NUM_DESTS);
    std::vector<float> signal_data(SIGNAL_SIZE);

    for (uint32_t i = 0; i < NUM_DESTS; i++) dest_ids[i] = i;
    for (uint32_t i = 0; i < SIGNAL_SIZE; i++) signal_data[i] = i * 0.1f;

    auto start = high_resolution_clock::now();

    // Simulate routing to multiple destinations (deep copy each time)
    for (uint32_t route = 0; route < NUM_ROUTES; route++) {
        // Allocate and copy for this route
        uint32_t* dc = (uint32_t*)nimcp_malloc(NUM_DESTS * sizeof(uint32_t));
        std::memcpy(dc, dest_ids.data(), NUM_DESTS * sizeof(uint32_t));

        float* sd = (float*)nimcp_malloc(SIGNAL_SIZE * sizeof(float));
        std::memcpy(sd, signal_data.data(), SIGNAL_SIZE * sizeof(float));

        // Simulate some work
        volatile float sum = 0.0f;
        for (uint32_t i = 0; i < SIGNAL_SIZE; i++) {
            sum += sd[i];
        }

        // Free copies
        nimcp_free(dc);
        nimcp_free(sd);
    }

    auto end = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(end - start);
    return duration.count();
}

// Simulate new routing (CoW wrapper, shared until write)
double benchmark_new_routing() {
    std::vector<uint32_t> dest_ids(NUM_DESTS);
    std::vector<float> signal_data(SIGNAL_SIZE);

    for (uint32_t i = 0; i < NUM_DESTS; i++) dest_ids[i] = i;
    for (uint32_t i = 0; i < SIGNAL_SIZE; i++) signal_data[i] = i * 0.1f;

    auto start = high_resolution_clock::now();

    // Create signal wrapper once
    signal_wrapper_t original = signal_wrapper_create(
        dest_ids.data(), NUM_DESTS,
        signal_data.data(), SIGNAL_SIZE);

    // Simulate routing to multiple destinations (acquire reference each time)
    for (uint32_t route = 0; route < NUM_ROUTES; route++) {
        // Just acquire a reference (no copy)
        signal_wrapper_t ref = signal_wrapper_acquire(original);

        // Read data (zero-copy)
        uint32_t size = 0;
        const float* data = signal_wrapper_read_data(ref, &size);

        // Simulate some work
        volatile float sum = 0.0f;
        for (uint32_t i = 0; i < size; i++) {
            sum += data[i];
        }

        // Release reference
        signal_wrapper_release(ref);
    }

    signal_wrapper_release(original);

    auto end = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(end - start);
    return duration.count();
}

int main() {
    std::cout << "\n";
    std::cout << "=================================================================\n";
    std::cout << "        Routing Performance: Deep Copy vs CoW Wrapper\n";
    std::cout << "=================================================================\n\n";

    std::cout << "Configuration:\n";
    std::cout << "  Routes:        " << NUM_ROUTES << "\n";
    std::cout << "  Signal size:   " << SIGNAL_SIZE << " floats (" << (SIGNAL_SIZE * 4) << " bytes)\n";
    std::cout << "  Destinations:  " << NUM_DESTS << " per signal\n";
    std::cout << "\n";

    std::cout << "Running benchmarks (3 runs each)...\n\n";

    // Warmup
    benchmark_old_routing();
    benchmark_new_routing();

    // Run multiple times and average
    double old_total = 0.0, new_total = 0.0;
    const int runs = 3;

    for (int i = 0; i < runs; i++) {
        old_total += benchmark_old_routing();
        new_total += benchmark_new_routing();
    }

    double old_avg = old_total / runs;
    double new_avg = new_total / runs;

    std::cout << std::fixed << std::setprecision(2);

    std::cout << "Results:\n";
    std::cout << "-----------------------------------------------------------------\n";
    std::cout << "  Old (Deep Copy):     " << std::setw(10) << old_avg << " \u00b5s total\n";
    std::cout << "                       " << std::setw(10) << (old_avg * 1000.0 / NUM_ROUTES) << " ns per route\n";
    std::cout << "\n";
    std::cout << "  New (CoW Wrapper):   " << std::setw(10) << new_avg << " \u00b5s total\n";
    std::cout << "                       " << std::setw(10) << (new_avg * 1000.0 / NUM_ROUTES) << " ns per route\n";
    std::cout << "\n";

    double speedup = old_avg / new_avg;
    double improvement_pct = ((old_avg - new_avg) / old_avg) * 100.0;

    std::cout << "=================================================================\n";
    std::cout << "Performance Improvement:\n";
    std::cout << "=================================================================\n";
    std::cout << "  Speedup:              " << speedup << "x faster\n";
    std::cout << "  Latency Reduction:    " << improvement_pct << "%\n";
    std::cout << "  Time Saved:           " << (old_avg - new_avg) << " \u00b5s per " << NUM_ROUTES << " routes\n";
    std::cout << "\n";

    if (speedup >= 2.0) {
        std::cout << "  \u2705 EXCELLENT: CoW provides significant performance improvement!\n";
    } else if (speedup >= 1.5) {
        std::cout << "  \u2705 GOOD: CoW provides meaningful performance improvement\n";
    } else if (speedup >= 1.2) {
        std::cout << "  \u2713 MODERATE: CoW provides some performance improvement\n";
    } else {
        std::cout << "  \u26a0\ufe0f  Speedup is modest - may not justify complexity\n";
    }

    std::cout << "=================================================================\n\n";

    return 0;
}
