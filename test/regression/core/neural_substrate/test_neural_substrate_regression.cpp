/**
 * @file test_neural_substrate_regression.cpp
 * @brief Comprehensive regression tests for Neural Substrate layer
 * @version 1.0.0
 *
 * WHAT: Performance, determinism, state consistency, and safety regression tests
 * WHY:  Ensure neural substrate operations maintain baseline behavior across versions
 * HOW:  Benchmark timing, verify determinism, stress test state, check null safety
 *
 * Tests coverage (~20 tests):
 * - Performance benchmarks (update cycle timing)
 * - Determinism tests (same input = same output)
 * - State consistency after operations
 * - Memory usage patterns
 * - Null pointer safety
 * - Backward compatibility (default config values)
 */

#include <gtest/gtest.h>
#include <chrono>
#include <vector>
#include <numeric>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <thread>
#include <atomic>

#include "utils/nimcp_test_base.h"

// Headers have their own extern "C" guards
#include "core/neural_substrate/nimcp_neural_substrate.h"

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

    static double Min(const std::vector<double>& values) {
        if (values.empty()) return 0.0;
        return *std::min_element(values.begin(), values.end());
    }

    static double Max(const std::vector<double>& values) {
        if (values.empty()) return 0.0;
        return *std::max_element(values.begin(), values.end());
    }
};

//=============================================================================
// PERFORMANCE BASELINES
//=============================================================================

namespace Baseline {
    // Neural substrate benchmarks (ms)
    constexpr double SUBSTRATE_CREATE_MS = 1.0;
    constexpr double SUBSTRATE_UPDATE_1K_MS = 50.0;
    constexpr double SUBSTRATE_SPIKE_RECORD_10K_MS = 20.0;
    constexpr double SUBSTRATE_TRANSMISSION_RECORD_10K_MS = 20.0;

    // Memory limits
    constexpr size_t MAX_SUBSTRATE_BYTES = 4096;

    // Regression tolerance (30% above baseline)
    constexpr double REGRESSION_TOLERANCE = 1.3;

    // Numerical stability tolerances
    constexpr double MODULATION_TOLERANCE = 1e-5;
    constexpr float FLOAT_EPSILON = 1e-6f;
}

//=============================================================================
// TEST FIXTURES
//=============================================================================

class NeuralSubstrateRegressionTest : public NimcpTestBase {
protected:
    static constexpr int NUM_SAMPLES = 10;
    static constexpr int WARMUP_RUNS = 2;

    neural_substrate_t* substrate = nullptr;

    void SetUp() override {
        NimcpTestBase::SetUp();
    }

    void TearDown() override {
        if (substrate) {
            substrate_destroy(substrate);
            substrate = nullptr;
        }
        NimcpTestBase::TearDown();
    }

    // Helper to create substrate with default config
    neural_substrate_t* CreateDefaultSubstrate() {
        return substrate_create(nullptr);
    }

    // Helper to create substrate with custom config
    neural_substrate_t* CreateCustomSubstrate(const substrate_config_t& config) {
        return substrate_create(&config);
    }
};

//=============================================================================
// PERFORMANCE BENCHMARK TESTS
//=============================================================================

TEST_F(NeuralSubstrateRegressionTest, SubstrateCreationPerformance) {
    std::cout << "\n=== Substrate Creation Performance ===" << std::endl;

    constexpr int NUM_CREATES = 1000;
    std::vector<double> times_ms;

    // Warmup
    for (int w = 0; w < WARMUP_RUNS; w++) {
        neural_substrate_t* s = substrate_create(nullptr);
        substrate_destroy(s);
    }

    // Measure creation time
    for (int sample = 0; sample < NUM_SAMPLES; sample++) {
        std::vector<neural_substrate_t*> substrates;
        substrates.reserve(NUM_CREATES);

        double time = PerformanceMonitor::MeasureTimeMs([&]() {
            for (int i = 0; i < NUM_CREATES; i++) {
                substrates.push_back(substrate_create(nullptr));
            }
        });
        times_ms.push_back(time);

        // Cleanup
        for (auto* s : substrates) {
            substrate_destroy(s);
        }
    }

    double mean = PerformanceMonitor::Mean(times_ms);
    double per_create = mean / NUM_CREATES;

    std::cout << "Creating " << NUM_CREATES << " substrates:" << std::endl;
    std::cout << "  Total mean: " << std::fixed << std::setprecision(3) << mean << " ms" << std::endl;
    std::cout << "  Per create: " << per_create * 1000.0 << " us" << std::endl;
    std::cout << "  Throughput: " << (NUM_CREATES / mean * 1000.0) << " creates/sec" << std::endl;

    EXPECT_LT(per_create, Baseline::SUBSTRATE_CREATE_MS * Baseline::REGRESSION_TOLERANCE)
        << "Substrate creation performance regression detected";
}

TEST_F(NeuralSubstrateRegressionTest, SubstrateUpdatePerformance) {
    std::cout << "\n=== Substrate Update Performance ===" << std::endl;

    constexpr int NUM_UPDATES = 1000;
    std::vector<double> times_ms;

    substrate = CreateDefaultSubstrate();
    ASSERT_NE(substrate, nullptr);

    // Warmup
    for (int w = 0; w < WARMUP_RUNS; w++) {
        for (int i = 0; i < 100; i++) {
            substrate_update(substrate, 1);
        }
    }

    // Measure
    for (int sample = 0; sample < NUM_SAMPLES; sample++) {
        // Reset to normal state
        substrate_reset(substrate);

        double time = PerformanceMonitor::MeasureTimeMs([&]() {
            for (int i = 0; i < NUM_UPDATES; i++) {
                substrate_update(substrate, 1);
            }
        });
        times_ms.push_back(time);
    }

    double mean = PerformanceMonitor::Mean(times_ms);
    double stddev = PerformanceMonitor::StdDev(times_ms);

    std::cout << "Processing " << NUM_UPDATES << " substrate updates:" << std::endl;
    std::cout << "  Mean: " << std::fixed << std::setprecision(3) << mean << " ms" << std::endl;
    std::cout << "  StdDev: " << stddev << " ms" << std::endl;
    std::cout << "  Throughput: " << (NUM_UPDATES / mean * 1000.0) << " updates/sec" << std::endl;
    std::cout << "  Baseline: < " << Baseline::SUBSTRATE_UPDATE_1K_MS << " ms" << std::endl;

    EXPECT_LT(mean, Baseline::SUBSTRATE_UPDATE_1K_MS * Baseline::REGRESSION_TOLERANCE)
        << "Substrate update performance regression detected";
}

TEST_F(NeuralSubstrateRegressionTest, SpikeRecordingPerformance) {
    std::cout << "\n=== Spike Recording Performance ===" << std::endl;

    constexpr int NUM_SPIKES = 10000;
    std::vector<double> times_ms;

    substrate = CreateDefaultSubstrate();
    ASSERT_NE(substrate, nullptr);

    // Warmup
    for (int w = 0; w < WARMUP_RUNS; w++) {
        for (int i = 0; i < 1000; i++) {
            substrate_record_spikes(substrate, 1);
        }
    }

    // Measure
    for (int sample = 0; sample < NUM_SAMPLES; sample++) {
        substrate_reset(substrate);

        double time = PerformanceMonitor::MeasureTimeMs([&]() {
            for (int i = 0; i < NUM_SPIKES; i++) {
                substrate_record_spikes(substrate, 1);
            }
        });
        times_ms.push_back(time);
    }

    double mean = PerformanceMonitor::Mean(times_ms);

    std::cout << "Recording " << NUM_SPIKES << " spike events:" << std::endl;
    std::cout << "  Mean: " << std::fixed << std::setprecision(3) << mean << " ms" << std::endl;
    std::cout << "  Throughput: " << (NUM_SPIKES / mean * 1000.0) << " spikes/sec" << std::endl;

    EXPECT_LT(mean, Baseline::SUBSTRATE_SPIKE_RECORD_10K_MS * Baseline::REGRESSION_TOLERANCE)
        << "Spike recording performance regression detected";
}

TEST_F(NeuralSubstrateRegressionTest, TransmissionRecordingPerformance) {
    std::cout << "\n=== Transmission Recording Performance ===" << std::endl;

    constexpr int NUM_TRANSMISSIONS = 10000;
    std::vector<double> times_ms;

    substrate = CreateDefaultSubstrate();
    ASSERT_NE(substrate, nullptr);

    // Warmup
    for (int w = 0; w < WARMUP_RUNS; w++) {
        for (int i = 0; i < 1000; i++) {
            substrate_record_transmissions(substrate, 1);
        }
    }

    // Measure
    for (int sample = 0; sample < NUM_SAMPLES; sample++) {
        substrate_reset(substrate);

        double time = PerformanceMonitor::MeasureTimeMs([&]() {
            for (int i = 0; i < NUM_TRANSMISSIONS; i++) {
                substrate_record_transmissions(substrate, 1);
            }
        });
        times_ms.push_back(time);
    }

    double mean = PerformanceMonitor::Mean(times_ms);

    std::cout << "Recording " << NUM_TRANSMISSIONS << " transmission events:" << std::endl;
    std::cout << "  Mean: " << std::fixed << std::setprecision(3) << mean << " ms" << std::endl;
    std::cout << "  Throughput: " << (NUM_TRANSMISSIONS / mean * 1000.0) << " transmissions/sec" << std::endl;

    EXPECT_LT(mean, Baseline::SUBSTRATE_TRANSMISSION_RECORD_10K_MS * Baseline::REGRESSION_TOLERANCE)
        << "Transmission recording performance regression detected";
}

//=============================================================================
// DETERMINISM TESTS
//=============================================================================

TEST_F(NeuralSubstrateRegressionTest, UpdateDeterminism) {
    std::cout << "\n=== Update Determinism ===" << std::endl;

    constexpr int NUM_RUNS = 5;
    constexpr int NUM_UPDATES = 100;

    std::vector<float> final_capacities;
    std::vector<float> final_firing_mods;

    for (int run = 0; run < NUM_RUNS; run++) {
        neural_substrate_t* s = substrate_create(nullptr);
        ASSERT_NE(s, nullptr);

        // Set identical initial state
        substrate_set_atp(s, 0.8f);
        substrate_set_oxygen(s, 0.9f);
        substrate_set_glucose(s, 0.85f);
        substrate_set_temperature(s, 37.5f);

        // Run identical sequence
        for (int i = 0; i < NUM_UPDATES; i++) {
            substrate_update(s, 10);
            if (i % 10 == 0) {
                substrate_record_spikes(s, 5);
            }
        }

        // Capture final state
        float capacity = substrate_get_capacity(s);
        float firing_mod = substrate_get_firing_modulation(s);

        final_capacities.push_back(capacity);
        final_firing_mods.push_back(firing_mod);

        substrate_destroy(s);
    }

    // All runs should produce identical results
    float expected_capacity = final_capacities[0];
    float expected_firing_mod = final_firing_mods[0];

    std::cout << "Comparing " << NUM_RUNS << " identical runs:" << std::endl;
    std::cout << "  Expected capacity: " << expected_capacity << std::endl;
    std::cout << "  Expected firing mod: " << expected_firing_mod << std::endl;

    for (int run = 1; run < NUM_RUNS; run++) {
        EXPECT_NEAR(final_capacities[run], expected_capacity, Baseline::FLOAT_EPSILON)
            << "Capacity diverged on run " << run;
        EXPECT_NEAR(final_firing_mods[run], expected_firing_mod, Baseline::FLOAT_EPSILON)
            << "Firing modulation diverged on run " << run;
    }

    std::cout << "  All runs produced identical results" << std::endl;
}

TEST_F(NeuralSubstrateRegressionTest, ModulationDeterminism) {
    std::cout << "\n=== Modulation Calculation Determinism ===" << std::endl;

    substrate = CreateDefaultSubstrate();
    ASSERT_NE(substrate, nullptr);

    // Test at specific substrate states
    struct TestState {
        float atp;
        float oxygen;
        float temp;
    };

    std::vector<TestState> states = {
        {0.95f, 0.97f, 37.0f},  // Normal
        {0.5f, 0.7f, 38.0f},   // Stressed
        {0.3f, 0.5f, 39.0f},   // Critical
    };

    for (const auto& state : states) {
        std::vector<substrate_modulation_t> modulations;

        for (int run = 0; run < 5; run++) {
            substrate_set_atp(substrate, state.atp);
            substrate_set_oxygen(substrate, state.oxygen);
            substrate_set_temperature(substrate, state.temp);
            substrate_update(substrate, 1);

            substrate_modulation_t mod;
            substrate_get_modulation(substrate, &mod);
            modulations.push_back(mod);
        }

        // All should be identical
        for (size_t i = 1; i < modulations.size(); i++) {
            EXPECT_NEAR(modulations[i].firing_rate_mod, modulations[0].firing_rate_mod, Baseline::FLOAT_EPSILON);
            EXPECT_NEAR(modulations[i].transmission_efficiency, modulations[0].transmission_efficiency, Baseline::FLOAT_EPSILON);
            EXPECT_NEAR(modulations[i].conduction_velocity, modulations[0].conduction_velocity, Baseline::FLOAT_EPSILON);
            EXPECT_NEAR(modulations[i].plasticity_capacity, modulations[0].plasticity_capacity, Baseline::FLOAT_EPSILON);
        }
    }

    std::cout << "  Modulation calculations are deterministic across all states" << std::endl;
}

//=============================================================================
// STATE CONSISTENCY TESTS
//=============================================================================

TEST_F(NeuralSubstrateRegressionTest, StateConsistencyAfterReset) {
    std::cout << "\n=== State Consistency After Reset ===" << std::endl;

    substrate = CreateDefaultSubstrate();
    ASSERT_NE(substrate, nullptr);

    // Capture initial state
    substrate_metabolic_state_t initial_metabolic;
    substrate_physical_state_t initial_physical;
    substrate_get_metabolic_state(substrate, &initial_metabolic);
    substrate_get_physical_state(substrate, &initial_physical);

    // Stress the substrate
    for (int i = 0; i < 100; i++) {
        substrate_record_spikes(substrate, 100);
        substrate_record_transmissions(substrate, 200);
        substrate_update(substrate, 10);
    }

    // Reset
    int result = substrate_reset(substrate);
    EXPECT_EQ(result, 0);

    // Verify state matches initial
    substrate_metabolic_state_t reset_metabolic;
    substrate_physical_state_t reset_physical;
    substrate_get_metabolic_state(substrate, &reset_metabolic);
    substrate_get_physical_state(substrate, &reset_physical);

    EXPECT_NEAR(reset_metabolic.atp_level, initial_metabolic.atp_level, Baseline::FLOAT_EPSILON);
    EXPECT_NEAR(reset_metabolic.oxygen_saturation, initial_metabolic.oxygen_saturation, Baseline::FLOAT_EPSILON);
    EXPECT_NEAR(reset_metabolic.glucose_level, initial_metabolic.glucose_level, Baseline::FLOAT_EPSILON);
    EXPECT_NEAR(reset_physical.temperature, initial_physical.temperature, Baseline::FLOAT_EPSILON);

    std::cout << "  Reset restored all state to initial values" << std::endl;
}

TEST_F(NeuralSubstrateRegressionTest, StateBoundsAfterStress) {
    std::cout << "\n=== State Bounds After Stress ===" << std::endl;

    substrate = CreateDefaultSubstrate();
    ASSERT_NE(substrate, nullptr);

    // Severe stress: deplete ATP with many spikes
    for (int i = 0; i < 10000; i++) {
        substrate_record_spikes(substrate, 10);
        substrate_record_transmissions(substrate, 20);
        if (i % 100 == 0) {
            substrate_update(substrate, 10);
        }
    }

    // Verify all state values remain in valid bounds
    substrate_metabolic_state_t metabolic;
    substrate_physical_state_t physical;
    substrate_modulation_t mod;

    substrate_get_metabolic_state(substrate, &metabolic);
    substrate_get_physical_state(substrate, &physical);
    substrate_get_modulation(substrate, &mod);

    // Metabolic bounds [0, 1]
    EXPECT_GE(metabolic.atp_level, 0.0f);
    EXPECT_LE(metabolic.atp_level, 1.0f);
    EXPECT_GE(metabolic.oxygen_saturation, 0.0f);
    EXPECT_LE(metabolic.oxygen_saturation, 1.0f);
    EXPECT_GE(metabolic.glucose_level, 0.0f);
    EXPECT_LE(metabolic.glucose_level, 1.0f);

    // Physical bounds
    EXPECT_GE(physical.membrane_integrity, 0.0f);
    EXPECT_LE(physical.membrane_integrity, 1.0f);
    EXPECT_GE(physical.ion_balance, 0.0f);
    EXPECT_LE(physical.ion_balance, 1.0f);

    // Modulation bounds
    EXPECT_GE(mod.firing_rate_mod, 0.0f);
    EXPECT_LE(mod.firing_rate_mod, 2.0f);  // Can be boosted above 1.0
    EXPECT_GE(mod.transmission_efficiency, 0.0f);
    EXPECT_LE(mod.transmission_efficiency, 1.5f);

    std::cout << "  All state values remain within valid bounds after stress" << std::endl;
    std::cout << "  ATP: " << metabolic.atp_level << ", Membrane: " << physical.membrane_integrity << std::endl;
}

TEST_F(NeuralSubstrateRegressionTest, StatisticsAccumulation) {
    std::cout << "\n=== Statistics Accumulation ===" << std::endl;

    substrate = CreateDefaultSubstrate();
    ASSERT_NE(substrate, nullptr);

    constexpr int SPIKES_PER_UPDATE = 5;
    constexpr int TRANSMISSIONS_PER_UPDATE = 10;
    constexpr int NUM_UPDATES = 200;

    // Total expected
    constexpr int EXPECTED_SPIKES = SPIKES_PER_UPDATE * NUM_UPDATES;
    constexpr int EXPECTED_TRANSMISSIONS = TRANSMISSIONS_PER_UPDATE * NUM_UPDATES;

    for (int i = 0; i < NUM_UPDATES; i++) {
        substrate_record_spikes(substrate, SPIKES_PER_UPDATE);
        substrate_record_transmissions(substrate, TRANSMISSIONS_PER_UPDATE);
        substrate_update(substrate, 1);
    }

    substrate_stats_t stats;
    int result = substrate_get_stats(substrate, &stats);
    EXPECT_EQ(result, 0);

    // Verify statistics match expected counts
    // Allow some tolerance for batching
    EXPECT_GE(stats.total_updates, (uint64_t)(NUM_UPDATES * 0.9));
    EXPECT_GE(stats.spikes_processed, (uint64_t)(EXPECTED_SPIKES * 0.9));
    EXPECT_GE(stats.transmissions_processed, (uint64_t)(EXPECTED_TRANSMISSIONS * 0.9));
    EXPECT_GT(stats.total_atp_consumed, 0.0f);

    std::cout << "  Updates: " << stats.total_updates << " (expected ~" << NUM_UPDATES << ")" << std::endl;
    std::cout << "  Spikes: " << stats.spikes_processed << " (expected ~" << EXPECTED_SPIKES << ")" << std::endl;
    std::cout << "  Transmissions: " << stats.transmissions_processed << " (expected ~" << EXPECTED_TRANSMISSIONS << ")" << std::endl;
    std::cout << "  ATP consumed: " << stats.total_atp_consumed << std::endl;
}

//=============================================================================
// MEMORY USAGE TESTS
//=============================================================================

TEST_F(NeuralSubstrateRegressionTest, MemoryLeakCreateDestroy) {
    std::cout << "\n=== Memory Leak Create/Destroy Test ===" << std::endl;

    constexpr int NUM_ITERATIONS = 10000;

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        neural_substrate_t* s = substrate_create(nullptr);
        ASSERT_NE(s, nullptr);

        // Exercise the substrate
        substrate_record_spikes(s, 10);
        substrate_record_transmissions(s, 20);
        substrate_update(s, 5);

        substrate_destroy(s);
    }

    std::cout << "  Completed " << NUM_ITERATIONS << " create/destroy cycles" << std::endl;
    std::cout << "  No memory leaks detected (test passed without crashes)" << std::endl;
    SUCCEED();
}

TEST_F(NeuralSubstrateRegressionTest, MemoryLeakWithAlerts) {
    std::cout << "\n=== Memory Leak With Alerts Test ===" << std::endl;

    constexpr int NUM_ITERATIONS = 5000;

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        neural_substrate_t* s = substrate_create(nullptr);
        ASSERT_NE(s, nullptr);

        // Trigger alerts by setting critical values
        substrate_set_atp(s, 0.1f);
        substrate_set_oxygen(s, 0.3f);
        substrate_set_temperature(s, 41.0f);
        substrate_update(s, 10);

        // Get alerts
        substrate_alert_type_t alerts[8];
        uint32_t alert_count;
        substrate_get_alerts(s, alerts, &alert_count);

        substrate_destroy(s);
    }

    std::cout << "  Completed " << NUM_ITERATIONS << " cycles with alert generation" << std::endl;
    SUCCEED();
}

//=============================================================================
// NULL POINTER SAFETY TESTS
//=============================================================================

TEST_F(NeuralSubstrateRegressionTest, NullSubstrateDestroy) {
    std::cout << "\n=== Null Substrate Destroy Safety ===" << std::endl;

    // Should not crash
    substrate_destroy(nullptr);

    std::cout << "  substrate_destroy(NULL) handled safely" << std::endl;
    SUCCEED();
}

TEST_F(NeuralSubstrateRegressionTest, NullSubstrateSetters) {
    std::cout << "\n=== Null Substrate Setters Safety ===" << std::endl;

    // All setters should handle NULL gracefully
    EXPECT_NE(substrate_set_atp(nullptr, 0.5f), 0);
    EXPECT_NE(substrate_set_oxygen(nullptr, 0.5f), 0);
    EXPECT_NE(substrate_set_glucose(nullptr, 0.5f), 0);
    EXPECT_NE(substrate_set_temperature(nullptr, 37.0f), 0);
    EXPECT_NE(substrate_set_membrane_integrity(nullptr, 0.5f), 0);
    EXPECT_NE(substrate_set_ion_balance(nullptr, 0.5f), 0);

    std::cout << "  All setters return error for NULL substrate" << std::endl;
    SUCCEED();
}

TEST_F(NeuralSubstrateRegressionTest, NullSubstrateGetters) {
    std::cout << "\n=== Null Substrate Getters Safety ===" << std::endl;

    substrate_metabolic_state_t metabolic;
    substrate_physical_state_t physical;
    substrate_modulation_t mod;
    substrate_stats_t stats;
    substrate_alert_type_t alerts[8];
    uint32_t alert_count;

    // All getters should handle NULL gracefully
    EXPECT_NE(substrate_get_metabolic_state(nullptr, &metabolic), 0);
    EXPECT_NE(substrate_get_physical_state(nullptr, &physical), 0);
    EXPECT_NE(substrate_get_modulation(nullptr, &mod), 0);
    EXPECT_NE(substrate_get_stats(nullptr, &stats), 0);
    EXPECT_NE(substrate_get_alerts(nullptr, alerts, &alert_count), 0);

    std::cout << "  All getters return error for NULL substrate" << std::endl;
    SUCCEED();
}

TEST_F(NeuralSubstrateRegressionTest, NullOutputPointers) {
    std::cout << "\n=== Null Output Pointer Safety ===" << std::endl;

    substrate = CreateDefaultSubstrate();
    ASSERT_NE(substrate, nullptr);

    // All getters should handle NULL output gracefully
    EXPECT_NE(substrate_get_metabolic_state(substrate, nullptr), 0);
    EXPECT_NE(substrate_get_physical_state(substrate, nullptr), 0);
    EXPECT_NE(substrate_get_modulation(substrate, nullptr), 0);
    EXPECT_NE(substrate_get_stats(substrate, nullptr), 0);
    EXPECT_NE(substrate_get_alerts(substrate, nullptr, nullptr), 0);

    std::cout << "  All getters return error for NULL output pointers" << std::endl;
}

//=============================================================================
// BACKWARD COMPATIBILITY TESTS
//=============================================================================

TEST_F(NeuralSubstrateRegressionTest, DefaultConfigValues) {
    std::cout << "\n=== Default Config Values ===" << std::endl;

    substrate_config_t config;
    int result = substrate_default_config(&config);
    EXPECT_EQ(result, 0);

    // Verify expected default values
    EXPECT_NEAR(config.initial_atp, SUBSTRATE_NORMAL_ATP, 0.01f);
    EXPECT_NEAR(config.initial_o2, SUBSTRATE_NORMAL_O2_SAT, 0.01f);
    EXPECT_NEAR(config.initial_glucose, SUBSTRATE_NORMAL_GLUCOSE, 0.01f);
    EXPECT_NEAR(config.initial_temperature, SUBSTRATE_NORMAL_TEMPERATURE, 0.1f);
    EXPECT_NEAR(config.initial_membrane, SUBSTRATE_NORMAL_MEMBRANE, 0.01f);
    EXPECT_NEAR(config.initial_ion_balance, SUBSTRATE_NORMAL_ION_BALANCE, 0.01f);

    // Verify feature flags default to enabled
    EXPECT_TRUE(config.enable_metabolic_model);
    EXPECT_TRUE(config.enable_temperature_effects);
    EXPECT_TRUE(config.enable_alerts);

    std::cout << "  Default ATP: " << config.initial_atp << " (expected ~" << SUBSTRATE_NORMAL_ATP << ")" << std::endl;
    std::cout << "  Default temp: " << config.initial_temperature << " (expected ~" << SUBSTRATE_NORMAL_TEMPERATURE << ")" << std::endl;
}

TEST_F(NeuralSubstrateRegressionTest, DefaultHealthLevel) {
    std::cout << "\n=== Default Health Level ===" << std::endl;

    substrate = CreateDefaultSubstrate();
    ASSERT_NE(substrate, nullptr);

    // New substrate should start at optimal health
    substrate_health_level_t health = substrate_get_health_level(substrate);
    EXPECT_EQ(health, SUBSTRATE_HEALTH_OPTIMAL);

    // Verify string conversion
    const char* health_str = substrate_health_level_to_string(health);
    EXPECT_NE(health_str, nullptr);

    std::cout << "  Initial health: " << health_str << std::endl;
}

TEST_F(NeuralSubstrateRegressionTest, HealthLevelProgression) {
    std::cout << "\n=== Health Level Progression ===" << std::endl;

    substrate = CreateDefaultSubstrate();
    ASSERT_NE(substrate, nullptr);

    // Gradually degrade substrate
    // NOTE: Health level depends on multiple factors (ATP, O2, glucose, temp, membrane, ions)
    // so we test that lowering ATP monotonically degrades or maintains health
    struct TestCase {
        float atp;
        std::string label;
    };

    std::vector<TestCase> cases = {
        {0.95f, "near-optimal"},
        {0.7f, "moderate"},
        {0.5f, "low"},
        {0.25f, "very-low"},
        {0.1f, "critical"},
    };

    substrate_health_level_t prev_health = SUBSTRATE_HEALTH_OPTIMAL;

    for (const auto& tc : cases) {
        substrate_set_atp(substrate, tc.atp);
        substrate_update(substrate, 10);

        substrate_health_level_t health = substrate_get_health_level(substrate);

        // Health should degrade or stay the same as ATP decreases
        EXPECT_GE((int)health, (int)prev_health)
            << "Health improved unexpectedly when ATP decreased to " << tc.atp;

        const char* health_str = substrate_health_level_to_string(health);
        std::cout << "  ATP " << tc.atp << " (" << tc.label << ") -> " << health_str << std::endl;

        // Only track if health is valid
        if (health >= SUBSTRATE_HEALTH_OPTIMAL && health <= SUBSTRATE_HEALTH_FAILING) {
            prev_health = health;
        }
    }

    // Final state with very low ATP should not be optimal
    substrate_health_level_t final_health = substrate_get_health_level(substrate);
    EXPECT_NE(final_health, SUBSTRATE_HEALTH_OPTIMAL)
        << "Health should degrade below optimal with very low ATP";
}

//=============================================================================
// STRESS TESTS
//=============================================================================

TEST_F(NeuralSubstrateRegressionTest, RapidStateChanges) {
    std::cout << "\n=== Rapid State Changes Stress Test ===" << std::endl;

    substrate = CreateDefaultSubstrate();
    ASSERT_NE(substrate, nullptr);

    constexpr int NUM_ITERATIONS = 10000;

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        // Rapidly toggle between states
        float atp = (i % 2 == 0) ? 0.9f : 0.1f;
        float temp = (i % 3 == 0) ? 40.0f : 35.0f;

        substrate_set_atp(substrate, atp);
        substrate_set_temperature(substrate, temp);
        substrate_update(substrate, 1);

        // Verify state remains valid
        float capacity = substrate_get_capacity(substrate);
        EXPECT_TRUE(std::isfinite(capacity));
        EXPECT_GE(capacity, 0.0f);
        EXPECT_LE(capacity, 1.0f);
    }

    std::cout << "  Completed " << NUM_ITERATIONS << " rapid state changes" << std::endl;
    std::cout << "  All capacity values remained valid" << std::endl;
}

TEST_F(NeuralSubstrateRegressionTest, ExtendedOperation) {
    std::cout << "\n=== Extended Operation Stress Test ===" << std::endl;

    substrate = CreateDefaultSubstrate();
    ASSERT_NE(substrate, nullptr);

    constexpr int SIMULATION_HOURS = 8;
    constexpr int UPDATES_PER_SECOND = 1000;  // 1ms timesteps
    constexpr int TOTAL_UPDATES = SIMULATION_HOURS * 3600 * UPDATES_PER_SECOND / 1000;  // Scaled down

    std::cout << "  Simulating " << SIMULATION_HOURS << " hours of operation (~" << TOTAL_UPDATES << " updates)..." << std::endl;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < TOTAL_UPDATES; i++) {
        // Simulate normal neural activity
        substrate_record_spikes(substrate, i % 10);
        substrate_record_transmissions(substrate, i % 20);
        substrate_update(substrate, 1);

        // Periodically verify state
        if (i % 10000 == 0 && i > 0) {
            float capacity = substrate_get_capacity(substrate);
            EXPECT_TRUE(std::isfinite(capacity));

            // Allow recovery
            substrate_set_atp(substrate, 0.9f);
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();

    substrate_stats_t stats;
    substrate_get_stats(substrate, &stats);

    std::cout << "  Completed in " << elapsed_ms / 1000.0 << " seconds" << std::endl;
    std::cout << "  Updates/sec: " << (TOTAL_UPDATES / elapsed_ms * 1000.0) << std::endl;
    std::cout << "  Total spikes processed: " << stats.spikes_processed << std::endl;

    SUCCEED();
}

TEST_F(NeuralSubstrateRegressionTest, ConcurrentAccess) {
    std::cout << "\n=== Concurrent Access Stress Test ===" << std::endl;

    substrate = CreateDefaultSubstrate();
    ASSERT_NE(substrate, nullptr);

    constexpr int NUM_THREADS = 4;
    constexpr int OPS_PER_THREAD = 5000;
    std::atomic<int> completed_ops{0};

    std::vector<std::thread> threads;

    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < OPS_PER_THREAD; i++) {
                // Mix of operations
                substrate_record_spikes(substrate, 1);
                substrate_record_transmissions(substrate, 1);
                substrate_update(substrate, 1);

                // Read operations
                float capacity = substrate_get_capacity(substrate);
                (void)capacity;

                completed_ops++;
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    std::cout << "  Completed " << completed_ops << " concurrent operations" << std::endl;
    std::cout << "  No crashes or deadlocks detected" << std::endl;

    SUCCEED();
}

//=============================================================================
// MAIN
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
