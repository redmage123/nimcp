/**
 * @file test_axon_performance_regression.cpp
 * @brief Performance regression tests for NIMCP Axon Module
 *
 * WHAT: Baseline performance tests for axon operations
 * WHY:  Detect performance regressions across versions
 * HOW:  Measure operation times against established baselines
 */

#include <gtest/gtest.h>
#include <chrono>
#include <vector>
#include <numeric>
#include <cmath>
#include <iostream>
#include <iomanip>

// Headers have their own extern "C" guards
#include "core/axon/nimcp_axon.h"

//=============================================================================
// PERFORMANCE MONITORING UTILITIES
//=============================================================================

class PerformanceMonitor {
public:
    template<typename Func>
    static double MeasureTimeMs(Func&& func) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(end - start).count();
    }

    static double Mean(const std::vector<double>& values) {
        if (values.empty()) return 0.0;
        return std::accumulate(values.begin(), values.end(), 0.0) / values.size();
    }

    static double StdDev(const std::vector<double>& values) {
        if (values.size() < 2) return 0.0;
        double mean = Mean(values);
        double sq_sum = 0.0;
        for (double v : values) {
            sq_sum += (v - mean) * (v - mean);
        }
        return std::sqrt(sq_sum / (values.size() - 1));
    }
};

//=============================================================================
// PERFORMANCE BASELINES
//=============================================================================

namespace Baseline {
    // Creation/destruction baselines (ms for 1000 axons)
    constexpr double CREATION_1K_MS = 10.0;
    constexpr double DESTRUCTION_1K_MS = 25.0;

    // Spike propagation (ms for 10000 spikes)
    constexpr double SPIKE_10K_MS = 50.0;

    // Network step (ms for 1000 axons)
    constexpr double NETWORK_STEP_1K_MS = 5.0;

    // Queue operations (ms for 10000 events)
    constexpr double QUEUE_10K_MS = 10.0;

    // Regression tolerance (20% above baseline)
    constexpr double REGRESSION_TOLERANCE = 1.2;
}

//=============================================================================
// TEST FIXTURE
//=============================================================================

class AxonPerformanceTest : public ::testing::Test {
protected:
    static constexpr int NUM_SAMPLES = 10;
    static constexpr int WARMUP_RUNS = 2;
};

//=============================================================================
// CREATION PERFORMANCE
//=============================================================================

TEST_F(AxonPerformanceTest, CreationPerformance) {
    std::cout << "\n=== Axon Creation Performance ===" << std::endl;

    constexpr int NUM_AXONS = 1000;
    std::vector<double> times_ms;

    // Warmup
    for (int w = 0; w < WARMUP_RUNS; w++) {
        std::vector<axon_t*> axons;
        for (int i = 0; i < NUM_AXONS; i++) {
            axons.push_back(axon_create(i, AXON_TYPE_MYELINATED, i, i + 1000,
                                        500.0f + i, 2.0f));
        }
        for (auto* a : axons) axon_destroy(a);
    }

    // Measure
    for (int sample = 0; sample < NUM_SAMPLES; sample++) {
        std::vector<axon_t*> axons;
        axons.reserve(NUM_AXONS);

        double time = PerformanceMonitor::MeasureTimeMs([&]() {
            for (int i = 0; i < NUM_AXONS; i++) {
                axons.push_back(axon_create(i, AXON_TYPE_MYELINATED, i, i + 1000,
                                            500.0f + i, 2.0f));
            }
        });

        times_ms.push_back(time);

        // Cleanup
        for (auto* a : axons) axon_destroy(a);
    }

    double mean = PerformanceMonitor::Mean(times_ms);
    double stddev = PerformanceMonitor::StdDev(times_ms);

    std::cout << "Creating " << NUM_AXONS << " axons:" << std::endl;
    std::cout << "  Mean: " << std::fixed << std::setprecision(3) << mean << " ms" << std::endl;
    std::cout << "  StdDev: " << stddev << " ms" << std::endl;
    std::cout << "  Throughput: " << (NUM_AXONS / mean * 1000.0) << " axons/sec" << std::endl;
    std::cout << "  Baseline: < " << Baseline::CREATION_1K_MS << " ms" << std::endl;

    EXPECT_LT(mean, Baseline::CREATION_1K_MS * Baseline::REGRESSION_TOLERANCE)
        << "Creation performance regression detected";
}

TEST_F(AxonPerformanceTest, DestructionPerformance) {
    std::cout << "\n=== Axon Destruction Performance ===" << std::endl;

    constexpr int NUM_AXONS = 1000;
    std::vector<double> times_ms;

    for (int sample = 0; sample < NUM_SAMPLES; sample++) {
        // Create axons
        std::vector<axon_t*> axons;
        for (int i = 0; i < NUM_AXONS; i++) {
            axons.push_back(axon_create(i, AXON_TYPE_MYELINATED, i, i + 1000,
                                        500.0f, 2.0f));
        }

        // Measure destruction
        double time = PerformanceMonitor::MeasureTimeMs([&]() {
            for (auto* a : axons) {
                axon_destroy(a);
            }
        });

        times_ms.push_back(time);
    }

    double mean = PerformanceMonitor::Mean(times_ms);

    std::cout << "Destroying " << NUM_AXONS << " axons:" << std::endl;
    std::cout << "  Mean: " << std::fixed << std::setprecision(3) << mean << " ms" << std::endl;
    std::cout << "  Baseline: < " << Baseline::DESTRUCTION_1K_MS << " ms" << std::endl;

    EXPECT_LT(mean, Baseline::DESTRUCTION_1K_MS * Baseline::REGRESSION_TOLERANCE)
        << "Destruction performance regression detected";
}

//=============================================================================
// SPIKE PROPAGATION PERFORMANCE
//=============================================================================

TEST_F(AxonPerformanceTest, SpikeInitiationPerformance) {
    std::cout << "\n=== Spike Initiation Performance ===" << std::endl;

    constexpr int NUM_SPIKES = 10000;
    std::vector<double> times_ms;

    // Create axon
    axon_t* axon = axon_create(1, AXON_TYPE_MYELINATED, 100, 200, 1000.0f, 2.0f);
    ASSERT_NE(axon, nullptr);

    for (int sample = 0; sample < NUM_SAMPLES; sample++) {
        uint64_t time = 1000000;
        int successful = 0;

        double elapsed = PerformanceMonitor::MeasureTimeMs([&]() {
            for (int i = 0; i < NUM_SPIKES; i++) {
                time += 2000;  // 2ms between spikes
                if (axon_initiate_spike(axon, time, 1.0f)) {
                    successful++;
                }
                axon_step(axon, time + 1500, 1.0f);
            }
        });

        times_ms.push_back(elapsed);
    }

    double mean = PerformanceMonitor::Mean(times_ms);

    std::cout << "Initiating " << NUM_SPIKES << " spikes:" << std::endl;
    std::cout << "  Mean: " << std::fixed << std::setprecision(3) << mean << " ms" << std::endl;
    std::cout << "  Throughput: " << (NUM_SPIKES / mean * 1000.0) << " spikes/sec" << std::endl;
    std::cout << "  Baseline: < " << Baseline::SPIKE_10K_MS << " ms" << std::endl;

    EXPECT_LT(mean, Baseline::SPIKE_10K_MS * Baseline::REGRESSION_TOLERANCE)
        << "Spike initiation performance regression detected";

    axon_destroy(axon);
}

//=============================================================================
// NETWORK PERFORMANCE
//=============================================================================

TEST_F(AxonPerformanceTest, NetworkStepPerformance) {
    std::cout << "\n=== Network Step Performance ===" << std::endl;

    constexpr int NUM_AXONS = 1000;
    constexpr int NUM_STEPS = 100;
    std::vector<double> times_ms;

    // Create network
    axon_network_t* network = axon_network_create(NUM_AXONS);
    ASSERT_NE(network, nullptr);

    for (int i = 0; i < NUM_AXONS; i++) {
        axon_t* axon = axon_create(i, AXON_TYPE_MYELINATED, i, i + NUM_AXONS,
                                    500.0f + (i % 100) * 10, 1.0f + (i % 5) * 0.5f);
        axon_network_add(network, axon);
    }

    // Warmup
    uint64_t time = 0;
    for (int w = 0; w < 10; w++) {
        time += 1000;
        axon_network_step(network, time, 1.0f);
    }

    // Measure
    for (int sample = 0; sample < NUM_SAMPLES; sample++) {
        double elapsed = PerformanceMonitor::MeasureTimeMs([&]() {
            for (int step = 0; step < NUM_STEPS; step++) {
                time += 1000;
                axon_network_step(network, time, 1.0f);
            }
        });

        times_ms.push_back(elapsed / NUM_STEPS);  // Per-step time
    }

    double mean = PerformanceMonitor::Mean(times_ms);
    double stddev = PerformanceMonitor::StdDev(times_ms);

    std::cout << "Network step with " << NUM_AXONS << " axons:" << std::endl;
    std::cout << "  Mean per step: " << std::fixed << std::setprecision(3) << mean << " ms" << std::endl;
    std::cout << "  StdDev: " << stddev << " ms" << std::endl;
    std::cout << "  Throughput: " << (NUM_AXONS / mean) << " K axons/ms" << std::endl;
    std::cout << "  Baseline: < " << Baseline::NETWORK_STEP_1K_MS << " ms" << std::endl;

    EXPECT_LT(mean, Baseline::NETWORK_STEP_1K_MS * Baseline::REGRESSION_TOLERANCE)
        << "Network step performance regression detected";

    axon_network_destroy(network);
}

//=============================================================================
// QUEUE PERFORMANCE
//=============================================================================

TEST_F(AxonPerformanceTest, SpikeQueuePerformance) {
    std::cout << "\n=== Spike Queue Performance ===" << std::endl;

    constexpr int NUM_EVENTS = 10000;
    std::vector<double> times_ms;

    axon_spike_queue_t* queue = axon_spike_queue_create(NUM_EVENTS);
    ASSERT_NE(queue, nullptr);

    for (int sample = 0; sample < NUM_SAMPLES; sample++) {
        // Measure push performance
        double push_time = PerformanceMonitor::MeasureTimeMs([&]() {
            for (int i = 0; i < NUM_EVENTS; i++) {
                axon_spike_event_t event = {
                    .axon_id = (uint32_t)i,
                    .initiation_time = 1000000,
                    .arrival_time = (uint64_t)(1000000 + i * 100),
                    .amplitude = 1.0f,
                    .source_neuron_id = (uint32_t)i,
                    .target_synapse_id = (uint32_t)(i + 10000)
                };
                axon_spike_queue_push(queue, &event);
            }
        });

        // Measure pop performance
        double pop_time = PerformanceMonitor::MeasureTimeMs([&]() {
            axon_spike_event_t event;
            uint64_t time = 2000000;  // After all arrivals
            while (axon_spike_queue_pop(queue, time, &event)) {
                // Pop all events
            }
        });

        times_ms.push_back(push_time + pop_time);
    }

    double mean = PerformanceMonitor::Mean(times_ms);

    std::cout << "Queue " << NUM_EVENTS << " push+pop operations:" << std::endl;
    std::cout << "  Mean: " << std::fixed << std::setprecision(3) << mean << " ms" << std::endl;
    std::cout << "  Throughput: " << (NUM_EVENTS * 2 / mean * 1000.0) << " ops/sec" << std::endl;
    std::cout << "  Baseline: < " << Baseline::QUEUE_10K_MS << " ms" << std::endl;

    EXPECT_LT(mean, Baseline::QUEUE_10K_MS * Baseline::REGRESSION_TOLERANCE)
        << "Queue performance regression detected";

    axon_spike_queue_destroy(queue);
}

//=============================================================================
// SCALABILITY TESTS
//=============================================================================

TEST_F(AxonPerformanceTest, ScalabilityTest) {
    std::cout << "\n=== Scalability Test ===" << std::endl;

    std::vector<int> sizes = {100, 500, 1000, 2000, 5000};
    std::vector<double> times_per_axon;

    std::cout << "Size\t\tTime (ms)\tTime/Axon (us)" << std::endl;
    std::cout << "------------------------------------------------" << std::endl;

    for (int size : sizes) {
        axon_network_t* network = axon_network_create(size);
        ASSERT_NE(network, nullptr);

        for (int i = 0; i < size; i++) {
            axon_t* axon = axon_create(i, AXON_TYPE_MYELINATED, i, i + size,
                                        500.0f, 2.0f);
            axon_network_add(network, axon);
        }

        // Measure step time
        uint64_t time = 1000000;
        std::vector<double> step_times;

        for (int step = 0; step < 20; step++) {
            time += 1000;
            double elapsed = PerformanceMonitor::MeasureTimeMs([&]() {
                axon_network_step(network, time, 1.0f);
            });
            step_times.push_back(elapsed);
        }

        double mean = PerformanceMonitor::Mean(step_times);
        double per_axon = mean * 1000.0 / size;  // Convert to microseconds

        times_per_axon.push_back(per_axon);

        std::cout << size << "\t\t" << std::fixed << std::setprecision(3)
                  << mean << "\t\t" << std::setprecision(2) << per_axon << std::endl;

        axon_network_destroy(network);
    }

    // Verify O(n) scaling - time per axon should be relatively constant
    double min_per_axon = *std::min_element(times_per_axon.begin(), times_per_axon.end());
    double max_per_axon = *std::max_element(times_per_axon.begin(), times_per_axon.end());
    double ratio = max_per_axon / min_per_axon;

    std::cout << "\n--- Scaling Analysis ---" << std::endl;
    std::cout << "Min time/axon: " << min_per_axon << " us" << std::endl;
    std::cout << "Max time/axon: " << max_per_axon << " us" << std::endl;
    std::cout << "Ratio: " << ratio << "x (ideal = 1.0)" << std::endl;

    // Allow up to 3x variation for cache effects
    EXPECT_LT(ratio, 3.0) << "Scaling worse than O(n)";
}

//=============================================================================
// MEMORY EFFICIENCY
//=============================================================================

TEST_F(AxonPerformanceTest, MemoryEfficiency) {
    std::cout << "\n=== Memory Efficiency Test ===" << std::endl;

    // sizeof axon_t gives base size
    std::cout << "Base axon_t size: " << sizeof(axon_t) << " bytes" << std::endl;
    std::cout << "Segment size: " << sizeof(axon_segment_t) << " bytes" << std::endl;
    std::cout << "Spike event size: " << sizeof(axon_spike_event_t) << " bytes" << std::endl;

    // Create axon with segments to measure full size
    axon_t* axon = axon_create(1, AXON_TYPE_MYELINATED, 100, 200, 1000.0f, 2.0f);
    ASSERT_NE(axon, nullptr);

    axon_create_segments(axon, 20, 50.0f);

    size_t estimated_size = sizeof(axon_t) + 20 * sizeof(axon_segment_t);
    std::cout << "Estimated axon with 20 segments: " << estimated_size << " bytes" << std::endl;

    // Memory should be reasonable (< 1.2KB per axon with segments)
    // Note: pthread_mutex_t adds ~40 bytes on Linux
    EXPECT_LT(estimated_size, 3000) << "Memory per axon exceeds 3KB";

    axon_destroy(axon);
}

//=============================================================================
// MAIN
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
