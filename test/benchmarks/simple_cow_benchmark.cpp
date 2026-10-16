/**
 * @file simple_cow_benchmark.cpp
 * @brief Simple CoW performance benchmark
 */

#include <iostream>
#include <chrono>
#include <vector>
#include <iomanip>
#include <cstring>
#include "middleware/routing/nimcp_signal_wrapper.h"

using namespace std::chrono;

constexpr uint32_t ITERATIONS = 100000;
constexpr uint32_t SIGNAL_SIZE = 100;
constexpr uint32_t NUM_DESTS = 4;

int main() {
    std::cout << "Simple CoW Benchmark\n";
    std::cout << "====================\n\n";

    // Prepare test data
    std::vector<uint32_t> dest_ids(NUM_DESTS);
    std::vector<float> signal_data(SIGNAL_SIZE);

    for (uint32_t i = 0; i < NUM_DESTS; i++) dest_ids[i] = i;
    for (uint32_t i = 0; i < SIGNAL_SIZE; i++) signal_data[i] = i * 0.1f;

    // Test 1: Deep copy (old way)
    {
        auto start = high_resolution_clock::now();

        for (uint32_t i = 0; i < ITERATIONS; i++) {
            uint32_t* dc = (uint32_t*)malloc(NUM_DESTS * sizeof(uint32_t));
            std::memcpy(dc, dest_ids.data(), NUM_DESTS * sizeof(uint32_t));

            float* sd = (float*)malloc(SIGNAL_SIZE * sizeof(float));
            std::memcpy(sd, signal_data.data(), SIGNAL_SIZE * sizeof(float));

            free(dc);
            free(sd);
        }

        auto end = high_resolution_clock::now();
        auto duration = duration_cast<nanoseconds>(end - start);
        double avg_ns = static_cast<double>(duration.count()) / ITERATIONS;

        std::cout << "1. Deep Copy (malloc + memcpy x2):\n";
        std::cout << "   " << std::fixed << std::setprecision(2) << avg_ns << " ns per operation\n\n";
    }

    // Test 2: CoW wrapper (new way)
    {
        signal_wrapper_t original = signal_wrapper_create(
            dest_ids.data(), NUM_DESTS,
            signal_data.data(), SIGNAL_SIZE);

        auto start = high_resolution_clock::now();

        for (uint32_t i = 0; i < ITERATIONS; i++) {
            signal_wrapper_t ref = signal_wrapper_acquire(original);
            signal_wrapper_release(ref);
        }

        auto end = high_resolution_clock::now();
        signal_wrapper_release(original);

        auto duration = duration_cast<nanoseconds>(end - start);
        double avg_ns = static_cast<double>(duration.count()) / ITERATIONS;

        std::cout << "2. CoW Wrapper (acquire + release):\n";
        std::cout << "   " << std::fixed << std::setprecision(2) << avg_ns << " ns per operation\n\n";
    }

    // Test 3: Read from CoW wrapper
    {
        signal_wrapper_t wrapper = signal_wrapper_create(
            dest_ids.data(), NUM_DESTS,
            signal_data.data(), SIGNAL_SIZE);

        auto start = high_resolution_clock::now();

        volatile float sum = 0.0f;
        for (uint32_t i = 0; i < ITERATIONS; i++) {
            const float* data = signal_wrapper_read_data(wrapper, nullptr);
            sum += data[0];  // Touch the data to prevent optimization
        }

        auto end = high_resolution_clock::now();
        signal_wrapper_release(wrapper);

        auto duration = duration_cast<nanoseconds>(end - start);
        double avg_ns = static_cast<double>(duration.count()) / ITERATIONS;

        std::cout << "3. CoW Read (zero-copy pointer access):\n";
        std::cout << "   " << std::fixed << std::setprecision(2) << avg_ns << " ns per operation\n\n";
    }

    std::cout << "====================\n";
    std::cout << "Benchmark complete!\n";

    return 0;
}
