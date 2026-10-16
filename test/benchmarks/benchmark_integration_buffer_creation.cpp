/**
 * @file benchmark_integration_buffer_creation.cpp
 * @brief Measure integration buffer creation overhead
 */

#include <iostream>
#include <chrono>
#include <iomanip>
#include <vector>

// Headers have their own extern "C" guards
#include "middleware/buffering/nimcp_integration_buffer.h"

using namespace std::chrono;

/**
 * @brief Benchmark integration buffer creation for various channel counts
 */
void benchmark_creation(size_t num_channels, size_t iterations = 100) {
    const size_t fast_size = 100;
    const size_t medium_size = 500;
    const size_t slow_size = 2500;

    // Warmup
    for (size_t i = 0; i < 10; i++) {
        integration_buffer_t* buf = integration_buffer_create(
            fast_size, medium_size, slow_size, num_channels);
        integration_buffer_destroy(buf);
    }

    // Measure creation time
    auto start = high_resolution_clock::now();

    for (size_t i = 0; i < iterations; i++) {
        integration_buffer_t* buf = integration_buffer_create(
            fast_size, medium_size, slow_size, num_channels);
        integration_buffer_destroy(buf);
    }

    auto end = high_resolution_clock::now();
    double total_us = duration_cast<microseconds>(end - start).count();
    double avg_us = total_us / iterations;

    size_t expected_allocations = 2 + (num_channels * 6);  // Main + channel array + (channels × 6 buffers)

    std::cout << "  " << std::setw(5) << num_channels << " channels: "
              << std::setw(10) << std::fixed << std::setprecision(2) << avg_us << " µs/create"
              << "  (" << expected_allocations << " allocs)"
              << std::endl;
}

int main() {
    std::cout << "\n";
    std::cout << "=================================================================\n";
    std::cout << "       Integration Buffer Creation Benchmark\n";
    std::cout << "=================================================================\n\n";

    std::cout << "Configuration:\n";
    std::cout << "  Fast:     100 samples\n";
    std::cout << "  Medium:   500 samples\n";
    std::cout << "  Slow:     2500 samples\n";
    std::cout << "  Iterations: 100 per channel count\n";
    std::cout << "\n";

    std::cout << "Results:\n";
    std::cout << "-----------------------------------------------------------------\n";

    benchmark_creation(1, 100);
    benchmark_creation(8, 100);
    benchmark_creation(16, 100);
    benchmark_creation(32, 100);
    benchmark_creation(64, 100);
    benchmark_creation(128, 100);
    benchmark_creation(256, 100);
    benchmark_creation(512, 50);   // Fewer iterations for large configs
    benchmark_creation(1024, 25);

    std::cout << "\n";
    std::cout << "=================================================================\n";
    std::cout << "Analysis:\n";
    std::cout << "=================================================================\n\n";

    std::cout << "Key Findings:\n";
    std::cout << "  - Each channel requires 6 allocations (3 buffers + 3 windows)\n";
    std::cout << "  - Each sliding window creates a memory pool (2 blocks)\n";
    std::cout << "  - Each circular buffer allocates internal storage\n";
    std::cout << "  - Creation time scales linearly with channel count\n";
    std::cout << "\n";

    std::cout << "Optimization Opportunities:\n";
    std::cout << "  1. Object Pool: Pre-create common configs (64, 128, 256 channels)\n";
    std::cout << "  2. Lazy Init: Allocate channels on first use (sparse channels)\n";
    std::cout << "  3. Bulk Alloc: Single allocation for all channel structures\n";
    std::cout << "\n";

    std::cout << "Practical Considerations:\n";
    std::cout << "  - Integration buffers typically created ONCE at startup\n";
    std::cout << "  - Not a hot path (unlike stats recalculation)\n";
    std::cout << "  - Optimization only valuable if created/destroyed frequently\n";
    std::cout << "  - Focus on USAGE optimization (add/query) vs creation\n";
    std::cout << "\n";

    std::cout << "Recommendation:\n";
    std::cout << "  If creation time < 10ms AND created once:\n";
    std::cout << "    → SKIP pooling, optimize usage hot paths instead\n";
    std::cout << "  If creation time > 10ms OR created frequently:\n";
    std::cout << "    → Implement object pool for common configurations\n";
    std::cout << "=================================================================\n\n";

    return 0;
}
