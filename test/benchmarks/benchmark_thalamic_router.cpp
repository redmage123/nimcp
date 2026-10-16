/**
 * @file benchmark_thalamic_router.cpp
 * @brief Performance benchmark for thalamic router CoW integration
 *
 * Measures the performance impact of CoW-based signal routing compared to
 * the theoretical deep copy approach.
 */

#include <iostream>
#include <chrono>
#include <vector>
#include <iomanip>
#include <cstring>
#include "middleware/routing/nimcp_thalamic_router.h"
#include "middleware/routing/nimcp_signal_wrapper.h"

using namespace std::chrono;

// Benchmark parameters
constexpr uint32_t NUM_SIGNALS = 10000;
constexpr uint32_t SIGNAL_SIZE = 100;  // floats per signal
constexpr uint32_t NUM_DESTS = 4;       // destinations per signal

// Test data
struct TestData {
    std::vector<uint32_t> dest_ids;
    std::vector<float> signal_data;
};

TestData create_test_data() {
    TestData data;
    data.dest_ids.resize(NUM_DESTS);
    data.signal_data.resize(SIGNAL_SIZE);

    for (uint32_t i = 0; i < NUM_DESTS; i++) {
        data.dest_ids[i] = i;
    }

    for (uint32_t i = 0; i < SIGNAL_SIZE; i++) {
        data.signal_data[i] = static_cast<float>(i) * 0.1f;
    }

    return data;
}

// Callback for signal delivery
void delivery_callback(uint32_t dest_id, const float* signal, uint32_t size,
                       float attention, void* user_data) {
    // Minimal processing to avoid dominating benchmark
    volatile float sum = 0.0f;
    for (uint32_t i = 0; i < size; i++) {
        sum += signal[i];
    }
    (void)sum;
}

// Benchmark thalamic router with CoW
double benchmark_router_cow() {
    thalamic_router_config_t config = thalamic_router_default_config();
    config.max_queue_size = NUM_SIGNALS * 2;
    thalamic_router_t* router = thalamic_router_create(&config);

    // Register callbacks
    for (uint32_t i = 0; i < NUM_DESTS; i++) {
        thalamic_router_set_callback(router, i, delivery_callback, nullptr);
    }

    TestData data = create_test_data();

    auto start = high_resolution_clock::now();

    // Route signals (uses CoW internally)
    for (uint32_t i = 0; i < NUM_SIGNALS; i++) {
        routed_signal_t* signal = thalamic_router_create_signal(
            0,  // source_id
            data.dest_ids.data(),
            NUM_DESTS,
            data.signal_data.data(),
            SIGNAL_SIZE,
            SIGNAL_PRIORITY_NORMAL);

        if (signal) {
            thalamic_router_route_signal(router, signal);
            thalamic_router_free_signal(signal);
        }
    }

    // Process all queued signals
    uint32_t processed = 0;
    thalamic_router_process_queue(router, NUM_SIGNALS, &processed);

    auto end = high_resolution_clock::now();

    thalamic_router_destroy(router);

    auto duration = duration_cast<nanoseconds>(end - start);
    return static_cast<double>(duration.count()) / NUM_SIGNALS;
}

// Benchmark signal wrapper acquire/release directly
double benchmark_wrapper_operations() {
    TestData data = create_test_data();

    // Create initial wrapper
    signal_wrapper_t original = signal_wrapper_create(
        data.dest_ids.data(), NUM_DESTS,
        data.signal_data.data(), SIGNAL_SIZE);

    auto start = high_resolution_clock::now();

    // Measure acquire + release (what happens during enqueue)
    for (uint32_t i = 0; i < NUM_SIGNALS; i++) {
        signal_wrapper_t ref = signal_wrapper_acquire(original);
        signal_wrapper_release(ref);
    }

    auto end = high_resolution_clock::now();

    signal_wrapper_release(original);

    auto duration = duration_cast<nanoseconds>(end - start);
    return static_cast<double>(duration.count()) / NUM_SIGNALS;
}

// Simulate old deep copy approach (for comparison)
double benchmark_deep_copy() {
    TestData data = create_test_data();

    auto start = high_resolution_clock::now();

    // Measure malloc + memcpy (old approach)
    for (uint32_t i = 0; i < NUM_SIGNALS; i++) {
        uint32_t* dest_copy = (uint32_t*)malloc(NUM_DESTS * sizeof(uint32_t));
        memcpy(dest_copy, data.dest_ids.data(), NUM_DESTS * sizeof(uint32_t));

        float* signal_copy = (float*)malloc(SIGNAL_SIZE * sizeof(float));
        memcpy(signal_copy, data.signal_data.data(), SIGNAL_SIZE * sizeof(float));

        free(dest_copy);
        free(signal_copy);
    }

    auto end = high_resolution_clock::now();

    auto duration = duration_cast<nanoseconds>(end - start);
    return static_cast<double>(duration.count()) / NUM_SIGNALS;
}

int main() {
    std::cout << "=============================================================================\n";
    std::cout << "Thalamic Router CoW Performance Benchmark\n";
    std::cout << "=============================================================================\n\n";

    std::cout << "Configuration:\n";
    std::cout << "  Signals routed:    " << NUM_SIGNALS << "\n";
    std::cout << "  Signal size:       " << SIGNAL_SIZE << " floats (" << (SIGNAL_SIZE * sizeof(float)) << " bytes)\n";
    std::cout << "  Destinations:      " << NUM_DESTS << "\n";
    std::cout << "  Total data:        " << ((SIGNAL_SIZE * sizeof(float) + NUM_DESTS * sizeof(uint32_t)) * NUM_SIGNALS / 1024) << " KB\n";
    std::cout << "\n";

    std::cout << "Running benchmarks...\n\n";

    // Warmup
    benchmark_wrapper_operations();
    benchmark_deep_copy();
    benchmark_router_cow();

    // Actual benchmarks
    double wrapper_time = benchmark_wrapper_operations();
    double deep_copy_time = benchmark_deep_copy();
    double router_time = benchmark_router_cow();

    std::cout << std::fixed << std::setprecision(2);

    std::cout << "Results:\n";
    std::cout << "  1. Signal Wrapper Operations (acquire + release):\n";
    std::cout << "     Average: " << wrapper_time << " ns/signal\n";
    std::cout << "\n";

    std::cout << "  2. Deep Copy (malloc + memcpy):\n";
    std::cout << "     Average: " << deep_copy_time << " ns/signal\n";
    std::cout << "\n";

    std::cout << "  3. Full Router with CoW (route + deliver):\n";
    std::cout << "     Average: " << router_time << " ns/signal\n";
    std::cout << "\n";

    double speedup = deep_copy_time / wrapper_time;

    std::cout << "=============================================================================\n";
    std::cout << "Performance Analysis:\n";
    std::cout << "=============================================================================\n";
    std::cout << "  CoW vs Deep Copy Speedup: " << speedup << "x\n";
    std::cout << "  Memory allocation saved:  " << (deep_copy_time - wrapper_time) << " ns/signal\n";
    std::cout << "\n";

    if (speedup > 10.0) {
        std::cout << "  \u2705 EXCELLENT: CoW provides " << speedup << "x speedup!\n";
    } else if (speedup > 5.0) {
        std::cout << "  \u2705 GOOD: CoW provides " << speedup << "x speedup\n";
    } else if (speedup > 2.0) {
        std::cout << "  \u2705 MODERATE: CoW provides " << speedup << "x speedup\n";
    } else {
        std::cout << "  \u26a0\ufe0f  LOW: CoW speedup is only " << speedup << "x\n";
    }

    std::cout << "\n";
    std::cout << "Expected Impact on Middleware:\n";
    std::cout << "  - Routing latency reduction: " << ((1.0 - wrapper_time / deep_copy_time) * 100) << "%\n";
    std::cout << "  - Memory allocation pressure: " << (NUM_SIGNALS * 2) << " fewer mallocs for " << NUM_SIGNALS << " signals\n";
    std::cout << "  - Cache efficiency: Improved (less memcpy thrashing)\n";
    std::cout << "\n";
    std::cout << "=============================================================================\n";

    return 0;
}
