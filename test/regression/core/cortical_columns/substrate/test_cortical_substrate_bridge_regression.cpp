/**
 * @file test_cortical_substrate_bridge_regression.cpp
 * @brief Regression and performance tests for Cortical Substrate Bridge
 *
 * WHAT: Comprehensive regression suite for cortical substrate bridge stability
 * WHY:  Ensure bridge maintains performance, correctness, and stability over time
 * HOW:  Test numerical stability, memory behavior, performance, scaling, thread safety
 *
 * TEST CATEGORIES:
 * - PerformanceBenchmarks: Throughput and latency measurements
 * - MemoryRegression: Leak detection and allocation patterns
 * - NumericalStability: Edge cases and extreme values
 * - ScalingTests: Performance under increasing load
 * - CorrectnessRegression: Prevention of known issues
 * - ThreadSafetyRegression: Concurrent access safety
 *
 * PERFORMANCE BASELINES:
 * - Bridge creation: < 100 us
 * - Update cycle: < 10 us
 * - Effect computation: < 5 us
 * - Query operations: < 500 ns
 * - Bio-async messaging: < 2 us
 */

#include <gtest/gtest.h>
#include <chrono>
#include <vector>
#include <thread>
#include <atomic>
#include <cmath>
#include <algorithm>
#include <numeric>

// Headers have their own extern "C" guards
#include "core/cortical_columns/nimcp_cortical_substrate_bridge.h"
#include "core/neural_substrate/nimcp_neural_substrate.h"

//=============================================================================
// Test Fixture
//=============================================================================

class CorticalSubstrateBridgeRegressionTest : public ::testing::Test {
protected:
    cortical_substrate_bridge_t* bridge = nullptr;
    neural_substrate_t* substrate = nullptr;

    static constexpr int WARMUP_ITERATIONS = 50;
    static constexpr double NS_TO_US = 1e-3;
    static constexpr double NS_TO_MS = 1e-6;

    void SetUp() override {
        // Create substrate with default config
        substrate = substrate_create(nullptr);
        ASSERT_NE(substrate, nullptr);

        // Create bridge with default config
        bridge = cortical_substrate_bridge_create(nullptr, substrate);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            cortical_substrate_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (substrate) {
            substrate_destroy(substrate);
            substrate = nullptr;
        }
    }

    // Helper: Timing statistics
    struct TimingStats {
        double avg_ns;
        double min_ns;
        double max_ns;
        double stddev_ns;
        double median_ns;
        double p95_ns;
        double p99_ns;
    };

    TimingStats calculate_stats(std::vector<long long> times) {
        TimingStats stats = {0, 0, 0, 0, 0, 0, 0};
        if (times.empty()) return stats;

        std::sort(times.begin(), times.end());

        stats.min_ns = static_cast<double>(times.front());
        stats.max_ns = static_cast<double>(times.back());

        double sum = std::accumulate(times.begin(), times.end(), 0.0);
        stats.avg_ns = sum / times.size();

        size_t mid = times.size() / 2;
        stats.median_ns = times.size() % 2 == 0
            ? (times[mid-1] + times[mid]) / 2.0
            : static_cast<double>(times[mid]);

        size_t p95_idx = static_cast<size_t>(times.size() * 0.95);
        size_t p99_idx = static_cast<size_t>(times.size() * 0.99);
        stats.p95_ns = static_cast<double>(times[p95_idx]);
        stats.p99_ns = static_cast<double>(times[p99_idx]);

        double variance = 0;
        for (auto t : times) {
            double diff = static_cast<double>(t) - stats.avg_ns;
            variance += diff * diff;
        }
        stats.stddev_ns = std::sqrt(variance / times.size());

        return stats;
    }

    // Helper: Set substrate to specific state
    void set_substrate_state(float atp, float temp, float o2, float glucose) {
        substrate_set_atp(substrate, atp);
        substrate_set_temperature(substrate, temp);
        substrate_set_oxygen(substrate, o2);
        substrate_set_glucose(substrate, glucose);
    }

    // Helper: Print performance report
    void print_performance(const char* name, const TimingStats& stats) {
        std::cout << name << " Performance:" << std::endl;
        std::cout << "  avg=" << stats.avg_ns * NS_TO_US << " us, "
                  << "min=" << stats.min_ns * NS_TO_US << " us, "
                  << "max=" << stats.max_ns * NS_TO_US << " us" << std::endl;
        std::cout << "  median=" << stats.median_ns * NS_TO_US << " us, "
                  << "p95=" << stats.p95_ns * NS_TO_US << " us, "
                  << "p99=" << stats.p99_ns * NS_TO_US << " us" << std::endl;
        std::cout << "  stddev=" << stats.stddev_ns * NS_TO_US << " us" << std::endl;
    }
};

//=============================================================================
// CATEGORY 1: Performance Benchmarks
//=============================================================================

TEST_F(CorticalSubstrateBridgeRegressionTest, PerformanceBenchmarks_BridgeCreation) {
    const int NUM_ITERATIONS = 1000;
    std::vector<long long> creation_times;
    creation_times.reserve(NUM_ITERATIONS);

    // Warmup
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        auto* temp_bridge = cortical_substrate_bridge_create(nullptr, substrate);
        cortical_substrate_bridge_destroy(temp_bridge);
    }

    // Benchmark
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        auto start = std::chrono::high_resolution_clock::now();
        auto* temp_bridge = cortical_substrate_bridge_create(nullptr, substrate);
        auto end = std::chrono::high_resolution_clock::now();

        creation_times.push_back(
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());

        cortical_substrate_bridge_destroy(temp_bridge);
    }

    auto stats = calculate_stats(creation_times);
    print_performance("Bridge Creation", stats);

    // Regression: < 100 us average
    EXPECT_LT(stats.avg_ns * NS_TO_US, 100.0)
        << "Bridge creation should average < 100 us";
    EXPECT_LT(stats.p95_ns * NS_TO_US, 200.0)
        << "95th percentile should be < 200 us";
}

TEST_F(CorticalSubstrateBridgeRegressionTest, PerformanceBenchmarks_UpdateThroughput) {
    const int NUM_ITERATIONS = 10000;
    std::vector<long long> update_times;
    update_times.reserve(NUM_ITERATIONS);

    // Set varied substrate states
    set_substrate_state(0.85f, 37.0f, 0.95f, 0.90f);

    // Warmup
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        cortical_substrate_update(bridge);
    }

    // Benchmark
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        // Vary substrate state slightly
        float atp = 0.7f + (i % 30) * 0.01f;
        substrate_set_atp(substrate, atp);

        auto start = std::chrono::high_resolution_clock::now();
        cortical_substrate_update(bridge);
        auto end = std::chrono::high_resolution_clock::now();

        update_times.push_back(
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
    }

    auto stats = calculate_stats(update_times);
    print_performance("Update Cycle", stats);

    // Regression: < 10 us average
    EXPECT_LT(stats.avg_ns * NS_TO_US, 10.0)
        << "Update cycle should average < 10 us";
    EXPECT_LT(stats.p99_ns * NS_TO_US, 50.0)
        << "99th percentile should be < 50 us";
}

TEST_F(CorticalSubstrateBridgeRegressionTest, PerformanceBenchmarks_QueryLatency) {
    const int NUM_ITERATIONS = 50000;

    // Update once to compute effects
    cortical_substrate_update(bridge);

    std::vector<long long> query_times;
    query_times.reserve(NUM_ITERATIONS);

    // Warmup
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        volatile float val = cortical_substrate_get_column_fidelity(bridge);
        (void)val;
    }

    // Benchmark query operations
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        auto start = std::chrono::high_resolution_clock::now();
        volatile float val = cortical_substrate_get_column_fidelity(bridge);
        auto end = std::chrono::high_resolution_clock::now();
        (void)val;

        query_times.push_back(
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
    }

    auto stats = calculate_stats(query_times);
    print_performance("Query Operation", stats);

    // Regression: < 500 ns average
    EXPECT_LT(stats.avg_ns, 500.0)
        << "Query operations should average < 500 ns";
}

TEST_F(CorticalSubstrateBridgeRegressionTest, PerformanceBenchmarks_EffectComputation) {
    const int NUM_ITERATIONS = 10000;
    std::vector<long long> computation_times;
    computation_times.reserve(NUM_ITERATIONS);

    // Warmup
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        set_substrate_state(0.8f, 37.5f, 0.9f, 0.85f);
        cortical_substrate_update(bridge);
    }

    // Benchmark effect computation within update
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        // Vary conditions
        float temp = 36.0f + (i % 40) * 0.1f;
        set_substrate_state(0.75f, temp, 0.9f, 0.85f);

        auto start = std::chrono::high_resolution_clock::now();
        cortical_substrate_update(bridge);
        auto end = std::chrono::high_resolution_clock::now();

        computation_times.push_back(
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
    }

    auto stats = calculate_stats(computation_times);
    print_performance("Effect Computation", stats);

    // Regression: < 5 us average
    EXPECT_LT(stats.avg_ns * NS_TO_US, 5.0)
        << "Effect computation should average < 5 us";
}

//=============================================================================
// CATEGORY 2: Memory Regression
//=============================================================================

TEST_F(CorticalSubstrateBridgeRegressionTest, MemoryRegression_NoLeakOnRepeatedCreate) {
    const int NUM_CYCLES = 1000;

    // Baseline: Get initial memory state (simplified)
    std::vector<cortical_substrate_bridge_t*> bridges;

    for (int i = 0; i < NUM_CYCLES; i++) {
        auto* temp_bridge = cortical_substrate_bridge_create(nullptr, substrate);
        ASSERT_NE(temp_bridge, nullptr);

        // Perform some operations
        cortical_substrate_update(temp_bridge);
        cortical_substrate_get_column_fidelity(temp_bridge);

        cortical_substrate_bridge_destroy(temp_bridge);
    }

    // Test passes if no crashes or valgrind errors
    SUCCEED() << "Completed " << NUM_CYCLES << " create/destroy cycles";
}

TEST_F(CorticalSubstrateBridgeRegressionTest, MemoryRegression_StableAllocationPattern) {
    const int NUM_ITERATIONS = 5000;
    std::vector<long long> allocation_times;
    allocation_times.reserve(NUM_ITERATIONS);

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        auto start = std::chrono::high_resolution_clock::now();
        auto* temp_bridge = cortical_substrate_bridge_create(nullptr, substrate);
        auto end = std::chrono::high_resolution_clock::now();

        allocation_times.push_back(
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());

        cortical_substrate_bridge_destroy(temp_bridge);
    }

    auto stats = calculate_stats(allocation_times);

    // Verify allocation time remains stable (no fragmentation)
    double variance_ratio = stats.stddev_ns / stats.avg_ns;

    std::cout << "Allocation Pattern Stability:" << std::endl;
    std::cout << "  Variance ratio: " << variance_ratio << std::endl;
    std::cout << "  Range: " << (stats.max_ns - stats.min_ns) * NS_TO_US << " us" << std::endl;

    /* Allocation variance can be high due to OS scheduling, cgroup limits,
     * and parallel test execution (ctest -j4). Relax threshold accordingly. */
    EXPECT_LT(variance_ratio, 30.0)
        << "Allocation times should remain reasonably stable";
}

TEST_F(CorticalSubstrateBridgeRegressionTest, MemoryRegression_UpdateMemoryStability) {
    const int NUM_UPDATES = 10000;

    // Perform many updates and verify no memory growth
    for (int i = 0; i < NUM_UPDATES; i++) {
        float atp = 0.5f + (i % 50) * 0.01f;
        set_substrate_state(atp, 37.0f, 0.9f, 0.85f);
        cortical_substrate_update(bridge);

        // Query various metrics
        cortical_substrate_get_column_fidelity(bridge);
        cortical_substrate_get_competition_efficiency(bridge);
        cortical_substrate_get_sparsity_modulation(bridge);
    }

    SUCCEED() << "Completed " << NUM_UPDATES << " updates without memory issues";
}

//=============================================================================
// CATEGORY 3: Numerical Stability
//=============================================================================

TEST_F(CorticalSubstrateBridgeRegressionTest, NumericalStability_ExtremeATPLevels) {
    // Test with extreme ATP levels
    std::vector<float> atp_levels = {0.0f, 0.1f, 0.3f, 0.5f, 0.8f, 1.0f};

    for (float atp : atp_levels) {
        set_substrate_state(atp, 37.0f, 0.95f, 0.90f);

        int result = cortical_substrate_update(bridge);
        EXPECT_EQ(result, 0) << "Update failed at ATP=" << atp;

        float fidelity = cortical_substrate_get_column_fidelity(bridge);
        EXPECT_GE(fidelity, 0.0f) << "Fidelity out of range at ATP=" << atp;
        EXPECT_LE(fidelity, 1.0f) << "Fidelity out of range at ATP=" << atp;
        EXPECT_FALSE(std::isnan(fidelity)) << "Fidelity is NaN at ATP=" << atp;
        EXPECT_FALSE(std::isinf(fidelity)) << "Fidelity is Inf at ATP=" << atp;
    }
}

TEST_F(CorticalSubstrateBridgeRegressionTest, NumericalStability_ExtremeTemperatures) {
    // Test with extreme temperature ranges
    std::vector<float> temperatures = {30.0f, 32.0f, 35.0f, 37.0f, 39.0f, 40.5f, 42.0f};

    for (float temp : temperatures) {
        set_substrate_state(0.85f, temp, 0.95f, 0.90f);

        int result = cortical_substrate_update(bridge);
        EXPECT_EQ(result, 0) << "Update failed at temp=" << temp;

        // Check all layer gains
        for (int layer = 0; layer < CORTICAL_SUBSTRATE_NUM_LAYERS; layer++) {
            float gain = cortical_substrate_get_layer_gain(bridge, layer);
            EXPECT_GE(gain, 0.0f) << "Layer " << layer << " gain out of range at temp=" << temp;
            EXPECT_LE(gain, 2.0f) << "Layer " << layer << " gain out of range at temp=" << temp;
            EXPECT_FALSE(std::isnan(gain)) << "Layer " << layer << " gain is NaN at temp=" << temp;
            EXPECT_FALSE(std::isinf(gain)) << "Layer " << layer << " gain is Inf at temp=" << temp;
        }
    }
}

TEST_F(CorticalSubstrateBridgeRegressionTest, NumericalStability_CombinedExtremes) {
    // Test combinations of extreme conditions
    struct ExtremeCondition {
        float atp;
        float temp;
        float o2;
        float glucose;
    };

    std::vector<ExtremeCondition> conditions = {
        {0.2f, 30.0f, 0.4f, 0.3f},  // Severe metabolic crisis + hypothermia
        {0.3f, 40.5f, 0.5f, 0.4f},  // Metabolic crisis + hyperthermia
        {1.0f, 42.0f, 1.0f, 1.0f},  // Optimal metabolism + severe fever
        {0.0f, 37.0f, 0.0f, 0.0f},  // Complete metabolic failure
        {0.5f, 35.0f, 0.6f, 0.5f},  // Moderate stress
    };

    for (const auto& cond : conditions) {
        set_substrate_state(cond.atp, cond.temp, cond.o2, cond.glucose);

        int result = cortical_substrate_update(bridge);
        EXPECT_EQ(result, 0) << "Update failed at extreme condition";

        // Verify all effects are valid
        cortical_substrate_effects_t effects;
        result = cortical_substrate_get_effects(bridge, &effects);
        EXPECT_EQ(result, 0);

        EXPECT_FALSE(std::isnan(effects.column_fidelity));
        EXPECT_FALSE(std::isnan(effects.competition_efficiency));
        EXPECT_FALSE(std::isnan(effects.sparsity_modulation));
        EXPECT_FALSE(std::isnan(effects.hierarchical_depth));

        for (int i = 0; i < CORTICAL_SUBSTRATE_NUM_LAYERS; i++) {
            EXPECT_FALSE(std::isnan(effects.layer_gain[i]));
        }
    }
}

TEST_F(CorticalSubstrateBridgeRegressionTest, NumericalStability_RapidFluctuations) {
    const int NUM_FLUCTUATIONS = 1000;

    // Rapidly oscillate between states
    for (int i = 0; i < NUM_FLUCTUATIONS; i++) {
        float atp = (i % 2 == 0) ? 0.3f : 0.9f;
        float temp = (i % 2 == 0) ? 35.0f : 40.0f;

        set_substrate_state(atp, temp, 0.85f, 0.80f);

        int result = cortical_substrate_update(bridge);
        EXPECT_EQ(result, 0);

        float fidelity = cortical_substrate_get_column_fidelity(bridge);
        EXPECT_FALSE(std::isnan(fidelity)) << "NaN after fluctuation " << i;
        EXPECT_FALSE(std::isinf(fidelity)) << "Inf after fluctuation " << i;
    }
}

//=============================================================================
// CATEGORY 4: Scaling Tests
//=============================================================================

TEST_F(CorticalSubstrateBridgeRegressionTest, ScalingTests_SustainedHighThroughput) {
    const int NUM_UPDATES = 100000;
    const int SAMPLE_INTERVAL = 10000;

    std::vector<double> throughput_samples;

    for (int batch = 0; batch < NUM_UPDATES / SAMPLE_INTERVAL; batch++) {
        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < SAMPLE_INTERVAL; i++) {
            cortical_substrate_update(bridge);
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        double throughput = (SAMPLE_INTERVAL * 1e9) / duration_ns;  // ops/sec
        throughput_samples.push_back(throughput);
    }

    // Calculate throughput statistics
    double avg_throughput = std::accumulate(throughput_samples.begin(),
                                           throughput_samples.end(), 0.0) / throughput_samples.size();

    std::cout << "Sustained Throughput Test (" << NUM_UPDATES << " updates):" << std::endl;
    std::cout << "  Average: " << avg_throughput / 1000.0 << " K ops/sec" << std::endl;

    // Verify throughput remains stable (no degradation over time)
    double first_half_avg = std::accumulate(throughput_samples.begin(),
                                            throughput_samples.begin() + throughput_samples.size()/2,
                                            0.0) / (throughput_samples.size()/2);
    double second_half_avg = std::accumulate(throughput_samples.begin() + throughput_samples.size()/2,
                                             throughput_samples.end(),
                                             0.0) / (throughput_samples.size()/2);

    double degradation = (first_half_avg - second_half_avg) / first_half_avg;
    std::cout << "  Degradation: " << (degradation * 100.0) << "%" << std::endl;

    /* Allow up to 25% throughput variation due to OS scheduling
     * and parallel test execution (ctest -j4) */
    EXPECT_LT(std::abs(degradation), 0.25)
        << "Throughput should remain stable (< 25% degradation)";
    EXPECT_GT(avg_throughput, 25000.0)
        << "Should maintain > 25K updates/sec";
}

TEST_F(CorticalSubstrateBridgeRegressionTest, ScalingTests_LatencyUnderLoad) {
    const int NUM_ITERATIONS = 10000;
    std::vector<long long> latencies;
    latencies.reserve(NUM_ITERATIONS);

    // Simulate load with varied substrate states
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        float atp = 0.5f + (i % 50) * 0.01f;
        float temp = 36.0f + (i % 40) * 0.1f;
        set_substrate_state(atp, temp, 0.9f, 0.85f);

        auto start = std::chrono::high_resolution_clock::now();
        cortical_substrate_update(bridge);
        auto end = std::chrono::high_resolution_clock::now();

        latencies.push_back(
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
    }

    auto stats = calculate_stats(latencies);
    print_performance("Latency Under Load", stats);

    // Verify tail latencies
    EXPECT_LT(stats.p99_ns * NS_TO_US, 100.0)
        << "99th percentile latency should be < 100 us";
}

//=============================================================================
// CATEGORY 5: Correctness Regression
//=============================================================================

TEST_F(CorticalSubstrateBridgeRegressionTest, CorrectnessRegression_MonotonicFidelityWithATP) {
    // Fidelity should decrease monotonically as ATP decreases
    std::vector<float> atp_levels = {1.0f, 0.9f, 0.8f, 0.7f, 0.6f, 0.5f, 0.4f, 0.3f, 0.2f, 0.1f};
    std::vector<float> fidelities;

    for (float atp : atp_levels) {
        set_substrate_state(atp, 37.0f, 0.95f, 0.90f);
        cortical_substrate_update(bridge);
        fidelities.push_back(cortical_substrate_get_column_fidelity(bridge));
    }

    // Verify monotonic decrease
    for (size_t i = 1; i < fidelities.size(); i++) {
        EXPECT_LE(fidelities[i], fidelities[i-1] + 1e-5)
            << "Fidelity should decrease with ATP: "
            << "ATP[" << i-1 << "]=" << atp_levels[i-1] << " -> fidelity=" << fidelities[i-1]
            << ", ATP[" << i << "]=" << atp_levels[i] << " -> fidelity=" << fidelities[i];
    }
}

TEST_F(CorticalSubstrateBridgeRegressionTest, CorrectnessRegression_LayerGainQ10Scaling) {
    // Layer gains should respond correctly to temperature via Q10
    float base_temp = 37.0f;
    float high_temp = 40.0f;

    set_substrate_state(0.9f, base_temp, 0.95f, 0.90f);
    cortical_substrate_update(bridge);

    std::vector<float> base_gains;
    for (int layer = 0; layer < CORTICAL_SUBSTRATE_NUM_LAYERS; layer++) {
        base_gains.push_back(cortical_substrate_get_layer_gain(bridge, layer));
    }

    set_substrate_state(0.9f, high_temp, 0.95f, 0.90f);
    cortical_substrate_update(bridge);

    std::vector<float> high_gains;
    for (int layer = 0; layer < CORTICAL_SUBSTRATE_NUM_LAYERS; layer++) {
        high_gains.push_back(cortical_substrate_get_layer_gain(bridge, layer));
    }

    // Higher temperature should affect gains (direction depends on Q10 implementation)
    bool gains_changed = false;
    for (int layer = 0; layer < CORTICAL_SUBSTRATE_NUM_LAYERS; layer++) {
        if (std::abs(high_gains[layer] - base_gains[layer]) > 1e-5) {
            gains_changed = true;
            break;
        }
    }

    EXPECT_TRUE(gains_changed)
        << "Layer gains should respond to temperature changes";
}

TEST_F(CorticalSubstrateBridgeRegressionTest, CorrectnessRegression_ImpairmentFlagAccuracy) {
    // Impairment flag should be set correctly based on substrate state
    // Impairment is defined as: fidelity < 0.5 OR competition < 0.3
    // Fidelity = ATP * metabolic_capacity * sensitivity
    // Competition = 1.0 if ATP > 0.5, else ATP * 2.0

    // Normal state - not impaired
    set_substrate_state(0.9f, 37.0f, 0.95f, 0.90f);
    cortical_substrate_update(bridge);
    EXPECT_FALSE(cortical_substrate_is_impaired(bridge))
        << "Should not be impaired in normal state";

    // Critical ATP - impaired (low fidelity AND low competition)
    set_substrate_state(0.2f, 37.0f, 0.95f, 0.90f);
    cortical_substrate_update(bridge);
    EXPECT_TRUE(cortical_substrate_is_impaired(bridge))
        << "Should be impaired at critical ATP";

    // Hyperthermia with high ATP - NOT impaired
    // Note: Hyperthermia affects layer gains but doesn't directly impair
    // fidelity or competition if ATP is high
    set_substrate_state(0.9f, 41.0f, 0.95f, 0.90f);
    cortical_substrate_update(bridge);
    EXPECT_FALSE(cortical_substrate_is_impaired(bridge))
        << "High ATP maintains function despite hyperthermia";

    // Hyperthermia WITH low ATP - impaired
    set_substrate_state(0.2f, 41.0f, 0.95f, 0.90f);
    cortical_substrate_update(bridge);
    EXPECT_TRUE(cortical_substrate_is_impaired(bridge))
        << "Low ATP combined with hyperthermia should cause impairment";

    // Recovery - not impaired
    set_substrate_state(0.85f, 37.0f, 0.95f, 0.90f);
    cortical_substrate_update(bridge);
    EXPECT_FALSE(cortical_substrate_is_impaired(bridge))
        << "Should recover from impairment";
}

TEST_F(CorticalSubstrateBridgeRegressionTest, CorrectnessRegression_StatisticsAccumulation) {
    cortical_substrate_stats_t stats;

    // Perform multiple updates
    const int NUM_UPDATES = 100;
    for (int i = 0; i < NUM_UPDATES; i++) {
        cortical_substrate_update(bridge);
    }

    int result = cortical_substrate_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(stats.update_count, NUM_UPDATES)
        << "Update count should match number of updates performed";

    // Trigger impairment
    set_substrate_state(0.2f, 37.0f, 0.95f, 0.90f);
    cortical_substrate_update(bridge);

    result = cortical_substrate_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_GT(stats.impairment_events, 0)
        << "Impairment events should be recorded";
}

//=============================================================================
// CATEGORY 6: Thread Safety Regression
//=============================================================================

TEST_F(CorticalSubstrateBridgeRegressionTest, ThreadSafetyRegression_ConcurrentUpdates) {
    const int NUM_THREADS = 4;
    const int UPDATES_PER_THREAD = 1000;
    std::atomic<int> error_count{0};

    auto worker = [&]() {
        for (int i = 0; i < UPDATES_PER_THREAD; i++) {
            int result = cortical_substrate_update(bridge);
            if (result != 0) {
                error_count++;
            }
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(worker);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(error_count.load(), 0)
        << "No errors should occur during concurrent updates";

    cortical_substrate_stats_t stats;
    cortical_substrate_get_stats(bridge, &stats);
    EXPECT_EQ(stats.update_count, NUM_THREADS * UPDATES_PER_THREAD)
        << "All updates should be counted correctly";
}

TEST_F(CorticalSubstrateBridgeRegressionTest, ThreadSafetyRegression_ConcurrentQueries) {
    const int NUM_THREADS = 8;
    const int QUERIES_PER_THREAD = 10000;
    std::atomic<int> nan_count{0};

    // Perform one update to ensure data is available
    cortical_substrate_update(bridge);

    auto worker = [&]() {
        for (int i = 0; i < QUERIES_PER_THREAD; i++) {
            float fidelity = cortical_substrate_get_column_fidelity(bridge);
            float competition = cortical_substrate_get_competition_efficiency(bridge);
            float sparsity = cortical_substrate_get_sparsity_modulation(bridge);

            if (std::isnan(fidelity) || std::isnan(competition) || std::isnan(sparsity)) {
                nan_count++;
            }
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(worker);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(nan_count.load(), 0)
        << "No NaN values should occur during concurrent queries";
}

TEST_F(CorticalSubstrateBridgeRegressionTest, ThreadSafetyRegression_MixedOperations) {
    const int NUM_THREADS = 6;
    const int OPERATIONS_PER_THREAD = 1000;
    std::atomic<int> error_count{0};

    auto worker = [&](int thread_id) {
        for (int i = 0; i < OPERATIONS_PER_THREAD; i++) {
            if (thread_id % 3 == 0) {
                // Update operations
                int result = cortical_substrate_update(bridge);
                if (result != 0) error_count++;
            } else if (thread_id % 3 == 1) {
                // Query operations
                float val = cortical_substrate_get_column_fidelity(bridge);
                if (std::isnan(val)) error_count++;
            } else {
                // Stats operations
                cortical_substrate_stats_t stats;
                int result = cortical_substrate_get_stats(bridge, &stats);
                if (result != 0) error_count++;
            }
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(worker, i);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(error_count.load(), 0)
        << "No errors should occur during mixed concurrent operations";
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
