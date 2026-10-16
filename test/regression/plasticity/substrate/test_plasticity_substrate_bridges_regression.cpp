/**
 * @file test_plasticity_substrate_bridges_regression.cpp
 * @brief Regression tests for plasticity and neuromodulator substrate bridges
 *
 * WHAT: Test for regression in bridge behavior, performance, and numerical stability
 * WHY:  Ensure consistent behavior across updates and detect regressions early
 * HOW:  Test performance benchmarks, memory usage, numerical stability, thread safety
 *
 * REGRESSION TEST CATEGORIES:
 * - Performance overhead
 * - Memory usage and leaks
 * - Numerical stability and precision
 * - Thread safety
 * - Historical bug reproductions
 *
 * @author NIMCP Development Team
 * @date 2025-12-19
 */

#include <gtest/gtest.h>
#include <cmath>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>

#include "plasticity/nimcp_plasticity_substrate_bridge.h"
#include "plasticity/neuromodulators/nimcp_neuromod_substrate_bridge.h"
#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class PlasticitySubstrateBridgesRegressionTest : public ::testing::Test {
protected:
    neural_substrate_t* substrate = nullptr;
    plasticity_substrate_bridge_t* plasticity_bridge = nullptr;
    neuromod_substrate_bridge_t* neuromod_bridge = nullptr;
    neuromodulator_system_t neuromod_system;

    void SetUp() override {
        substrate_config_t sub_config = {};
        substrate_config_init(&sub_config);
        substrate = substrate_create(&sub_config);
        ASSERT_NE(substrate, nullptr);

        neuromodulator_config_t nm_config;
        neuromodulator_default_config(&nm_config);
        neuromod_system = neuromodulator_system_create(&nm_config);
        ASSERT_NE(neuromod_system, nullptr);

        plasticity_substrate_config_t plast_config;
        plasticity_substrate_default_config(&plast_config);
        plasticity_bridge = plasticity_substrate_bridge_create(&plast_config, substrate);
        ASSERT_NE(plasticity_bridge, nullptr);

        neuromod_substrate_config_t nm_sub_config;
        neuromod_substrate_default_config(&nm_sub_config);
        neuromod_bridge = neuromod_substrate_bridge_create(&nm_sub_config, substrate, neuromod_system);
        ASSERT_NE(neuromod_bridge, nullptr);
    }

    void TearDown() override {
        neuromod_substrate_bridge_destroy(neuromod_bridge);
        plasticity_substrate_bridge_destroy(plasticity_bridge);
        neuromodulator_system_destroy(neuromod_system);
        substrate_destroy(substrate);
    }
};

//=============================================================================
// Performance Regression Tests
//=============================================================================

TEST_F(PlasticitySubstrateBridgesRegressionTest, PlasticityUpdatePerformance) {
    // WHAT: Measure plasticity bridge update performance
    // WHY:  Detect performance regressions

    const int iterations = 10000;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; i++) {
        plasticity_substrate_update_all(plasticity_bridge);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    double us_per_update = static_cast<double>(duration.count()) / iterations;

    // Should be < 10 microseconds per update (baseline)
    EXPECT_LT(us_per_update, 10.0);

    std::cout << "Plasticity update: " << us_per_update << " us/update" << std::endl;
}

TEST_F(PlasticitySubstrateBridgesRegressionTest, NeurommodulatorUpdatePerformance) {
    // WHAT: Measure neuromodulator bridge update performance
    // WHY:  Detect performance regressions

    const int iterations = 10000;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; i++) {
        neuromod_substrate_update_effects(neuromod_bridge);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    double us_per_update = static_cast<double>(duration.count()) / iterations;

    // Should be < 15 microseconds per update (more computation)
    EXPECT_LT(us_per_update, 15.0);

    std::cout << "Neuromod update: " << us_per_update << " us/update" << std::endl;
}

TEST_F(PlasticitySubstrateBridgesRegressionTest, MetabolicFeedbackPerformance) {
    // WHAT: Measure metabolic feedback recording performance
    // WHY:  Verify low overhead for feedback

    const int iterations = 100000;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; i++) {
        neuromod_substrate_record_synthesis(neuromod_bridge, NEUROMOD_BRIDGE_DOPAMINE);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    double us_per_record = static_cast<double>(duration.count()) / iterations;

    // Should be < 1 microsecond per record
    EXPECT_LT(us_per_record, 1.0);

    std::cout << "Metabolic feedback: " << us_per_record << " us/record" << std::endl;
}

//=============================================================================
// Memory Regression Tests
//=============================================================================

TEST_F(PlasticitySubstrateBridgesRegressionTest, PlasticityBridgeMemoryFootprint) {
    // WHAT: Verify plasticity bridge memory footprint
    // WHY:  Detect memory bloat

    size_t initial_allocated = nimcp_get_allocated_memory();

    plasticity_substrate_config_t config;
    plasticity_substrate_default_config(&config);
    plasticity_substrate_bridge_t* bridge = plasticity_substrate_bridge_create(&config, substrate);
    ASSERT_NE(bridge, nullptr);

    size_t after_create = nimcp_get_allocated_memory();
    size_t footprint = after_create - initial_allocated;

    plasticity_substrate_bridge_destroy(bridge);

    // Should be < 10 KB (reasonable for state + config + stats)
    EXPECT_LT(footprint, 10240);

    std::cout << "Plasticity bridge footprint: " << footprint << " bytes" << std::endl;
}

TEST_F(PlasticitySubstrateBridgesRegressionTest, NeurommodulatorBridgeMemoryFootprint) {
    // WHAT: Verify neuromodulator bridge memory footprint
    // WHY:  Detect memory bloat

    size_t initial_allocated = nimcp_get_allocated_memory();

    neuromod_substrate_config_t config;
    neuromod_substrate_default_config(&config);
    neuromod_substrate_bridge_t* bridge = neuromod_substrate_bridge_create(&config, substrate, neuromod_system);
    ASSERT_NE(bridge, nullptr);

    size_t after_create = nimcp_get_allocated_memory();
    size_t footprint = after_create - initial_allocated;

    neuromod_substrate_bridge_destroy(bridge);

    // Should be < 12 KB (more state for 4 neuromodulators)
    EXPECT_LT(footprint, 12288);

    std::cout << "Neuromod bridge footprint: " << footprint << " bytes" << std::endl;
}

TEST_F(PlasticitySubstrateBridgesRegressionTest, NoMemoryLeaks) {
    // WHAT: Test for memory leaks
    // WHY:  Verify proper resource cleanup

    size_t initial = nimcp_get_allocated_memory();

    for (int i = 0; i < 100; i++) {
        plasticity_substrate_config_t config;
        plasticity_substrate_default_config(&config);
        plasticity_substrate_bridge_t* bridge = plasticity_substrate_bridge_create(&config, substrate);

        // Use the bridge
        plasticity_substrate_update_all(bridge);

        plasticity_substrate_bridge_destroy(bridge);
    }

    size_t final = nimcp_get_allocated_memory();

    // Should return to initial (within tolerance)
    EXPECT_NEAR(final, initial, 1024); // Allow 1KB variance
}

//=============================================================================
// Numerical Stability Tests
//=============================================================================

TEST_F(PlasticitySubstrateBridgesRegressionTest, LearningRateModulationStability) {
    // WHAT: Test learning rate modulation is stable
    // WHY:  Verify no numerical drift or NaN

    for (int i = 0; i < 1000; i++) {
        float atp = 0.5f + 0.3f * sinf(i * 0.1f);
        substrate_set_atp_level(substrate, atp);

        plasticity_substrate_update_all(plasticity_bridge);

        float lr_mod = plasticity_substrate_get_learning_rate_mod(plasticity_bridge);

        EXPECT_FALSE(std::isnan(lr_mod));
        EXPECT_FALSE(std::isinf(lr_mod));
        EXPECT_GE(lr_mod, 0.0f);
        EXPECT_LE(lr_mod, 2.0f);
    }
}

TEST_F(PlasticitySubstrateBridgesRegressionTest, Q10TemperatureScalingStability) {
    // WHAT: Test Q10 scaling doesn't overflow
    // WHY:  Verify temperature extremes handled

    float test_temps[] = {25.0f, 30.0f, 35.0f, 37.0f, 39.0f, 40.0f, 42.0f};

    for (float temp : test_temps) {
        substrate_set_temperature(substrate, temp);

        plasticity_substrate_update_stdp(plasticity_bridge);
        neuromod_substrate_compute_temperature_effects(neuromod_bridge);

        float stdp_window = plasticity_substrate_get_stdp_window_mod(plasticity_bridge);

        EXPECT_FALSE(std::isnan(stdp_window));
        EXPECT_FALSE(std::isinf(stdp_window));
        EXPECT_GT(stdp_window, 0.0f);
        EXPECT_LT(stdp_window, 5.0f); // Reasonable upper bound
    }
}

TEST_F(PlasticitySubstrateBridgesRegressionTest, ATPGatingNonNegative) {
    // WHAT: Test ATP gating never goes negative
    // WHY:  Verify clamping logic

    float test_atp[] = {0.0f, 0.1f, 0.3f, 0.5f, 0.8f, 1.0f};

    for (float atp : test_atp) {
        substrate_set_atp_level(substrate, atp);

        plasticity_substrate_update_all(plasticity_bridge);
        neuromod_substrate_update_effects(neuromod_bridge);

        plasticity_substrate_effects_t plast_effects;
        plasticity_substrate_get_effects(plasticity_bridge, &plast_effects);

        EXPECT_GE(plast_effects.stdp.atp_gating, 0.0f);
        EXPECT_LE(plast_effects.stdp.atp_gating, 1.0f);

        // Check all neuromodulators
        for (int nm = 0; nm < NEUROMOD_BRIDGE_COUNT; nm++) {
            float capacity = neuromod_substrate_get_capacity(neuromod_bridge, static_cast<neuromod_bridge_type_t>(nm));
            EXPECT_GE(capacity, 0.0f);
            EXPECT_LE(capacity, 1.0f);
        }
    }
}

TEST_F(PlasticitySubstrateBridgesRegressionTest, BCMThresholdBounds) {
    // WHAT: Test BCM threshold shift stays in bounds
    // WHY:  Verify no runaway threshold

    for (int i = 0; i < 100; i++) {
        float atp = 0.2f + 0.7f * (i / 100.0f);
        substrate_set_atp_level(substrate, atp);

        plasticity_substrate_update_bcm(plasticity_bridge);

        float bcm_shift = plasticity_substrate_get_bcm_threshold_shift(plasticity_bridge);

        EXPECT_FALSE(std::isnan(bcm_shift));
        EXPECT_GE(bcm_shift, 0.5f); // Reasonable lower bound
        EXPECT_LE(bcm_shift, 2.0f); // Reasonable upper bound
    }
}

//=============================================================================
// Thread Safety Tests
//=============================================================================

TEST_F(PlasticitySubstrateBridgesRegressionTest, ConcurrentPlasticityUpdates) {
    // WHAT: Test concurrent updates to plasticity bridge
    // WHY:  Verify thread safety

    std::atomic<int> success_count(0);
    const int threads = 4;
    const int iterations_per_thread = 100;

    std::vector<std::thread> workers;

    for (int t = 0; t < threads; t++) {
        workers.emplace_back([this, &success_count, iterations_per_thread]() {
            for (int i = 0; i < iterations_per_thread; i++) {
                if (plasticity_substrate_update_all(plasticity_bridge) == 0) {
                    success_count++;
                }
                std::this_thread::yield();
            }
        });
    }

    for (auto& worker : workers) {
        worker.join();
    }

    EXPECT_EQ(success_count, threads * iterations_per_thread);
}

TEST_F(PlasticitySubstrateBridgesRegressionTest, ConcurrentNeurommodulatorUpdates) {
    // WHAT: Test concurrent updates to neuromodulator bridge
    // WHY:  Verify thread safety

    std::atomic<int> success_count(0);
    const int threads = 4;
    const int iterations_per_thread = 100;

    std::vector<std::thread> workers;

    for (int t = 0; t < threads; t++) {
        workers.emplace_back([this, &success_count, iterations_per_thread]() {
            for (int i = 0; i < iterations_per_thread; i++) {
                if (neuromod_substrate_update_effects(neuromod_bridge) == 0) {
                    success_count++;
                }
                std::this_thread::yield();
            }
        });
    }

    for (auto& worker : workers) {
        worker.join();
    }

    EXPECT_EQ(success_count, threads * iterations_per_thread);
}

TEST_F(PlasticitySubstrateBridgesRegressionTest, ConcurrentReadWrite) {
    // WHAT: Test concurrent reads and writes
    // WHY:  Verify no data races

    std::atomic<bool> stop(false);
    std::atomic<int> read_count(0);
    std::atomic<int> write_count(0);

    // Writer thread
    std::thread writer([this, &stop, &write_count]() {
        while (!stop) {
            substrate_set_atp_level(substrate, 0.7f);
            plasticity_substrate_update_all(plasticity_bridge);
            neuromod_substrate_update_effects(neuromod_bridge);
            write_count++;
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    });

    // Reader threads
    std::vector<std::thread> readers;
    for (int i = 0; i < 3; i++) {
        readers.emplace_back([this, &stop, &read_count]() {
            while (!stop) {
                plasticity_substrate_get_learning_rate_mod(plasticity_bridge);
                neuromod_substrate_get_capacity(neuromod_bridge, NEUROMOD_BRIDGE_DOPAMINE);
                read_count++;
                std::this_thread::yield();
            }
        });
    }

    // Run for a short time
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    stop = true;

    writer.join();
    for (auto& reader : readers) {
        reader.join();
    }

    EXPECT_GT(read_count, 0);
    EXPECT_GT(write_count, 0);
}

//=============================================================================
// Historical Bug Reproductions
//=============================================================================

TEST_F(PlasticitySubstrateBridgesRegressionTest, ZeroATPDoesNotCrash) {
    // WHAT: Ensure zero ATP doesn't cause division by zero
    // WHY:  Historical edge case

    substrate_set_atp_level(substrate, 0.0f);

    ASSERT_EQ(plasticity_substrate_update_all(plasticity_bridge), 0);
    ASSERT_EQ(neuromod_substrate_update_effects(neuromod_bridge), 0);

    float lr_mod = plasticity_substrate_get_learning_rate_mod(plasticity_bridge);
    EXPECT_FALSE(std::isnan(lr_mod));
    EXPECT_FALSE(std::isinf(lr_mod));
}

TEST_F(PlasticitySubstrateBridgesRegressionTest, ExtremeTemperatureHandling) {
    // WHAT: Test extreme temperatures are handled gracefully
    // WHY:  Prevent overflow in Q10 calculation

    float extreme_temps[] = {10.0f, 15.0f, 45.0f, 50.0f};

    for (float temp : extreme_temps) {
        substrate_set_temperature(substrate, temp);

        ASSERT_EQ(plasticity_substrate_update_all(plasticity_bridge), 0);
        ASSERT_EQ(neuromod_substrate_update_effects(neuromod_bridge), 0);

        // Should not crash or produce NaN
        float stdp_window = plasticity_substrate_get_stdp_window_mod(plasticity_bridge);
        EXPECT_FALSE(std::isnan(stdp_window));
        EXPECT_FALSE(std::isinf(stdp_window));
    }
}

TEST_F(PlasticitySubstrateBridgesRegressionTest, NullSubstrateHandling) {
    // WHAT: Test NULL substrate pointer handling
    // WHY:  Verify defensive programming

    plasticity_substrate_config_t config;
    plasticity_substrate_default_config(&config);
    plasticity_substrate_bridge_t* bridge = plasticity_substrate_bridge_create(&config, nullptr);

    EXPECT_EQ(bridge, nullptr);
}

TEST_F(PlasticitySubstrateBridgesRegressionTest, RepeatedCreateDestroy) {
    // WHAT: Test repeated create/destroy cycles
    // WHY:  Detect initialization/cleanup bugs

    for (int i = 0; i < 50; i++) {
        plasticity_substrate_config_t config;
        plasticity_substrate_default_config(&config);
        plasticity_substrate_bridge_t* bridge = plasticity_substrate_bridge_create(&config, substrate);
        ASSERT_NE(bridge, nullptr);

        plasticity_substrate_update_all(bridge);

        plasticity_substrate_bridge_destroy(bridge);
    }

    // No crashes = success
}

//=============================================================================
// Consistency Tests
//=============================================================================

TEST_F(PlasticitySubstrateBridgesRegressionTest, IdenticalStateProducesSameResults) {
    // WHAT: Test deterministic behavior
    // WHY:  Verify reproducibility

    substrate_set_atp_level(substrate, 0.6f);
    substrate_set_temperature(substrate, 37.5f);

    plasticity_substrate_update_all(plasticity_bridge);
    float lr_mod1 = plasticity_substrate_get_learning_rate_mod(plasticity_bridge);

    // Update again with same state
    plasticity_substrate_update_all(plasticity_bridge);
    float lr_mod2 = plasticity_substrate_get_learning_rate_mod(plasticity_bridge);

    EXPECT_FLOAT_EQ(lr_mod1, lr_mod2);
}

TEST_F(PlasticitySubstrateBridgesRegressionTest, StatisticsMonotonicity) {
    // WHAT: Test statistics counters are monotonic
    // WHY:  Verify no counter rollback

    plasticity_substrate_stats_t stats1, stats2;
    plasticity_substrate_get_stats(plasticity_bridge, &stats1);

    plasticity_substrate_update_all(plasticity_bridge);

    plasticity_substrate_get_stats(plasticity_bridge, &stats2);

    EXPECT_GE(stats2.total_updates, stats1.total_updates);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
