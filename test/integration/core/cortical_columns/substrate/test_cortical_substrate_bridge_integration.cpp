//=============================================================================
// test_cortical_substrate_bridge_integration.cpp
// Integration tests for cortical substrate bridge
//=============================================================================
/**
 * @file test_cortical_substrate_bridge_integration.cpp
 * @brief Integration tests for cortical substrate bridge with neural substrate
 *
 * WHAT: Complete integration testing of cortical substrate bridge with:
 *       - Neural substrate module (ATP, temperature, metabolic state)
 *       - Bio-async messaging system
 *       - Multi-module coordination
 *       - State propagation across systems
 *
 * WHY:  Verify bidirectional integration between metabolic substrate and
 *       cortical processing under realistic scenarios (energy depletion,
 *       hyperthermia, recovery, concurrent access)
 *
 * HOW:  GTest framework with comprehensive scenarios covering:
 *       - Brain-level integration
 *       - Cross-module communication via bio-async
 *       - State propagation (ATP/temperature → cortical effects)
 *       - Concurrent access from multiple modules
 *       - Recovery scenarios (ATP replenishment, cooling)
 *       - End-to-end sensory processing with substrate effects
 *
 * TEST COVERAGE:
 * 1. Brain-level integration (substrate + bridge)
 * 2. Cross-module communication (bio-async messaging)
 * 3. State propagation (substrate → effects → cortical)
 * 4. Concurrent access from multiple threads
 * 5. Recovery scenarios (ATP replenishment, thermal regulation)
 * 6. End-to-end processing with substrate modulation
 *
 * @author NIMCP Development Team
 * @date 2025-12-19
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <cstring>

// Headers have their own extern "C" guards
#include "core/cortical_columns/nimcp_cortical_substrate_bridge.h"
#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

//=============================================================================
// Test Fixtures
//=============================================================================

/**
 * Base fixture for cortical substrate integration tests
 */
class CorticalSubstrateBridgeIntegrationTest : public ::testing::Test {
protected:
    neural_substrate_t* substrate;
    cortical_substrate_bridge_t* bridge;

    void SetUp() override {
        // Create neural substrate
        substrate_config_t sub_config;
        substrate_default_config(&sub_config);
        substrate = substrate_create(&sub_config);
        ASSERT_NE(substrate, nullptr);

        // Create cortical substrate bridge
        cortical_substrate_config_t bridge_config;
        cortical_substrate_default_config(&bridge_config);
        bridge_config.enable_bio_async = true;
        bridge = cortical_substrate_bridge_create(&bridge_config, substrate);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            cortical_substrate_bridge_destroy(bridge);
        }
        if (substrate) {
            substrate_destroy(substrate);
        }
    }

    // Helper: Simulate ATP depletion by recording many spike events
    void deplete_atp(float target_level) {
        substrate_metabolic_state_t met_state;
        substrate_get_metabolic_state(substrate, &met_state);

        while (met_state.atp_level > target_level) {
            // Record many spikes to consume ATP
            substrate_record_spikes(substrate, 1000);
            substrate_update(substrate, 1);
            substrate_get_metabolic_state(substrate, &met_state);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    // Helper: Simulate hyperthermia by setting temperature directly
    void increase_temperature(float target_temp) {
        substrate_set_temperature(substrate, target_temp);
        substrate_update(substrate, 1);
    }

    // Helper: Verify layer gains are affected by substrate state
    // Note: Gains can range from 0.3 to 1.5 based on temperature Q10 effects
    bool verify_layer_gains_modulated() {
        for (int layer = 0; layer < CORTICAL_SUBSTRATE_NUM_LAYERS; layer++) {
            float gain = cortical_substrate_get_layer_gain(bridge, layer);
            if (gain < 0.3f || gain > 1.5f) {
                return false;
            }
        }
        return true;
    }
};

/**
 * Multi-threaded fixture for concurrent access tests
 */
class CorticalSubstrateConcurrentTest : public CorticalSubstrateBridgeIntegrationTest {
protected:
    std::atomic<uint32_t> error_count{0};
    std::atomic<bool> stop_threads{false};
};

//=============================================================================
// 1. BRAIN-LEVEL INTEGRATION TESTS
//=============================================================================

TEST_F(CorticalSubstrateBridgeIntegrationTest, SubstrateAndBridgeIntegration) {
    // Update substrate state
    substrate_update(substrate, 1);

    // Update bridge to compute effects
    int ret = cortical_substrate_update(bridge);
    EXPECT_EQ(ret, 0);

    // Verify effects are computed
    cortical_substrate_effects_t effects;
    ret = cortical_substrate_get_effects(bridge, &effects);
    EXPECT_EQ(ret, 0);

    // Under normal conditions, should have high fidelity
    EXPECT_GT(effects.column_fidelity, 0.8f);
    EXPECT_GT(effects.competition_efficiency, 0.8f);
    EXPECT_FALSE(effects.is_impaired);
}

TEST_F(CorticalSubstrateBridgeIntegrationTest, ATPDepletionAffectsCorticalProcessing) {
    // Deplete ATP to critical level
    deplete_atp(CORTICAL_SUBSTRATE_ATP_CRITICAL);

    // Update substrate and bridge
    substrate_update(substrate, 1);
    cortical_substrate_update(bridge);

    // Verify cortical processing is impaired
    EXPECT_TRUE(cortical_substrate_is_impaired(bridge));

    float fidelity = cortical_substrate_get_column_fidelity(bridge);
    EXPECT_LT(fidelity, 0.5f) << "Column fidelity should be severely reduced";

    float competition = cortical_substrate_get_competition_efficiency(bridge);
    EXPECT_LT(competition, 0.5f) << "Competition efficiency should be impaired";
}

TEST_F(CorticalSubstrateBridgeIntegrationTest, TemperatureAffectsLayerGains) {
    // Increase temperature to hyperthermic range
    increase_temperature(39.0f);

    // Update substrate and bridge
    substrate_update(substrate, 1);
    cortical_substrate_update(bridge);

    // Verify layer gains are modulated based on Q10 coefficients
    EXPECT_TRUE(verify_layer_gains_modulated());

    // Layers with higher Q10 should be MORE affected by temperature
    // Layer II/III has highest Q10 (2.8), should show greater deviation from 1.0
    // At 39°C (above 37°C reference), Q10 effect INCREASES gains
    float layer_2_3_gain = cortical_substrate_get_layer_gain(bridge, 1);
    float layer_1_gain = cortical_substrate_get_layer_gain(bridge, 0);

    // Layer II/III (higher Q10=2.8) should deviate more from 1.0 than Layer I (Q10=2.0)
    // At hyperthermia, both gains increase, but Layer II/III increases more
    float layer_2_3_deviation = std::abs(layer_2_3_gain - 1.0f);
    float layer_1_deviation = std::abs(layer_1_gain - 1.0f);
    EXPECT_GT(layer_2_3_deviation, layer_1_deviation)
        << "Layer II/III (higher Q10) should be more temperature-sensitive";
}

TEST_F(CorticalSubstrateBridgeIntegrationTest, HierarchicalDepthDegradation) {
    // Hierarchical depth degrades with hyperthermia (per implementation)
    // Set ATP low and temperature high to cause significant impairment
    deplete_atp(CORTICAL_SUBSTRATE_ATP_CRITICAL);  // 0.3f
    increase_temperature(41.0f);  // Significant hyperthermia

    // Update systems
    substrate_update(substrate, 1);
    cortical_substrate_update(bridge);

    // Verify hierarchical depth is reduced under hyperthermia
    float depth = cortical_substrate_get_hierarchical_depth(bridge);
    EXPECT_LT(depth, 1.0f) << "Hierarchical processing should be degraded";

    // Verify statistics track degradation
    cortical_substrate_stats_t stats;
    cortical_substrate_get_stats(bridge, &stats);
    EXPECT_GT(stats.impairment_events, 0);
}

//=============================================================================
// 2. CROSS-MODULE COMMUNICATION (BIO-ASYNC)
//=============================================================================

TEST_F(CorticalSubstrateBridgeIntegrationTest, BioAsyncConnection) {
    // Connect to bio-async router
    // Note: Bio-async router may not be available in test environment
    int ret = cortical_substrate_connect_bio_async(bridge);
    EXPECT_EQ(ret, 0);  // Should not return error even if router unavailable

    // Connection status depends on router availability
    // In test environment, router is typically not initialized
    bool connected = cortical_substrate_is_bio_async_connected(bridge);

    // Disconnect (safe even if not connected)
    ret = cortical_substrate_disconnect_bio_async(bridge);
    EXPECT_EQ(ret, 0);
    EXPECT_FALSE(cortical_substrate_is_bio_async_connected(bridge));

    // Test passes regardless of router availability
    SUCCEED();
}

TEST_F(CorticalSubstrateBridgeIntegrationTest, BioAsyncMessaging) {
    // Connect to bio-async
    cortical_substrate_connect_bio_async(bridge);

    // Trigger substrate state change
    deplete_atp(CORTICAL_SUBSTRATE_ATP_CRITICAL);
    substrate_update(substrate, 1);
    cortical_substrate_update(bridge);

    // Bridge should be able to send/receive messages via bio-async
    // (Actual message handling would be verified in unit tests)
    EXPECT_TRUE(cortical_substrate_is_impaired(bridge));
}

TEST_F(CorticalSubstrateBridgeIntegrationTest, MultiModuleCoordination) {
    // Connect to bio-async
    cortical_substrate_connect_bio_async(bridge);

    // Simulate multiple updates with varying substrate states
    for (int i = 0; i < 10; i++) {
        // Vary ATP level
        float atp = 0.5f + 0.4f * sinf(i * 0.5f);
        substrate_set_atp(substrate, atp);

        // Update systems
        substrate_update(substrate, 1);
        cortical_substrate_update(bridge);

        // Verify effects update correspondingly
        float fidelity = cortical_substrate_get_column_fidelity(bridge);
        EXPECT_GE(fidelity, 0.0f);
        EXPECT_LE(fidelity, 1.0f);

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Verify statistics accumulated
    cortical_substrate_stats_t stats;
    cortical_substrate_get_stats(bridge, &stats);
    EXPECT_GE(stats.update_count, 10);
}

//=============================================================================
// 3. STATE PROPAGATION TESTS
//=============================================================================

TEST_F(CorticalSubstrateBridgeIntegrationTest, ATPToCorticalPropagation) {
    // Test ATP level → cortical effects propagation
    std::vector<float> atp_levels = {1.0f, 0.8f, 0.5f, 0.3f, 0.1f};
    std::vector<float> fidelities;

    for (float atp : atp_levels) {
        substrate_set_atp(substrate, atp);
        substrate_update(substrate, 1);
        cortical_substrate_update(bridge);

        float fidelity = cortical_substrate_get_column_fidelity(bridge);
        fidelities.push_back(fidelity);
    }

    // Verify monotonic decrease in fidelity as ATP depletes
    for (size_t i = 1; i < fidelities.size(); i++) {
        EXPECT_LE(fidelities[i], fidelities[i-1])
            << "Fidelity should decrease with ATP depletion";
    }
}

TEST_F(CorticalSubstrateBridgeIntegrationTest, TemperatureToCorticalPropagation) {
    // Test temperature → layer gain propagation
    std::vector<float> temperatures = {37.0f, 38.0f, 39.0f, 40.0f, 41.0f};
    std::vector<float> layer_gains;

    for (float temp : temperatures) {
        substrate_set_temperature(substrate, temp);
        substrate_update(substrate, 1);
        cortical_substrate_update(bridge);

        // Check Layer II/III (most temperature-sensitive)
        float gain = cortical_substrate_get_layer_gain(bridge, 1);
        layer_gains.push_back(gain);
    }

    // Verify gains vary with temperature (Q10 effect)
    EXPECT_NE(layer_gains[0], layer_gains[4])
        << "Layer gains should change with temperature";
}

TEST_F(CorticalSubstrateBridgeIntegrationTest, SparsityModulation) {
    // Normal ATP → high sparsity
    substrate_set_atp(substrate, 0.9f);
    substrate_update(substrate, 1);
    cortical_substrate_update(bridge);

    float high_atp_sparsity = cortical_substrate_get_sparsity_modulation(bridge);

    // Low ATP → reduced sparsity (less competitive inhibition)
    substrate_set_atp(substrate, 0.2f);
    substrate_update(substrate, 1);
    cortical_substrate_update(bridge);

    float low_atp_sparsity = cortical_substrate_get_sparsity_modulation(bridge);

    // Sparsity should be affected by ATP (competition requires energy)
    EXPECT_NE(high_atp_sparsity, low_atp_sparsity)
        << "Sparsity modulation should vary with ATP";
}

//=============================================================================
// 4. CONCURRENT ACCESS TESTS
//=============================================================================

TEST_F(CorticalSubstrateConcurrentTest, ConcurrentSubstrateUpdates) {
    // Multiple threads updating substrate simultaneously
    std::vector<std::thread> threads;
    constexpr int NUM_THREADS = 4;
    constexpr int UPDATES_PER_THREAD = 50;

    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([this, t]() {
            for (int i = 0; i < UPDATES_PER_THREAD; i++) {
                // Each thread modifies substrate differently
                float atp = 0.5f + 0.3f * sinf(t + i * 0.1f);
                substrate_set_atp(substrate, atp);
                substrate_update(substrate, 1);

                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Verify substrate is still valid
    substrate_metabolic_state_t met_state;
    substrate_get_metabolic_state(substrate, &met_state);
    EXPECT_GE(met_state.atp_level, 0.0f);
    EXPECT_LE(met_state.atp_level, 1.0f);
}

TEST_F(CorticalSubstrateConcurrentTest, ConcurrentBridgeUpdates) {
    // Multiple threads updating bridge simultaneously
    std::vector<std::thread> threads;
    constexpr int NUM_THREADS = 4;
    constexpr int UPDATES_PER_THREAD = 50;

    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([this]() {
            for (int i = 0; i < UPDATES_PER_THREAD; i++) {
                int ret = cortical_substrate_update(bridge);
                if (ret != 0) {
                    error_count++;
                }

                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Verify no errors occurred
    EXPECT_EQ(error_count.load(), 0) << "Concurrent updates caused errors";

    // Verify bridge is still operational
    cortical_substrate_stats_t stats;
    int ret = cortical_substrate_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(stats.update_count, 0);
}

TEST_F(CorticalSubstrateConcurrentTest, ConcurrentReadsAndWrites) {
    // Mix of reader and writer threads
    std::atomic<uint32_t> read_count{0};
    std::atomic<uint32_t> write_count{0};
    std::vector<std::thread> threads;

    // Writer threads
    for (int t = 0; t < 2; t++) {
        threads.emplace_back([this, &write_count]() {
            for (int i = 0; i < 50; i++) {
                substrate_update(substrate, 1);
                cortical_substrate_update(bridge);
                write_count++;
                std::this_thread::sleep_for(std::chrono::microseconds(200));
            }
        });
    }

    // Reader threads
    for (int t = 0; t < 2; t++) {
        threads.emplace_back([this, &read_count]() {
            for (int i = 0; i < 100; i++) {
                float fidelity = cortical_substrate_get_column_fidelity(bridge);
                float competition = cortical_substrate_get_competition_efficiency(bridge);
                (void)fidelity;  // Suppress unused warning
                (void)competition;
                read_count++;
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Verify operations completed
    EXPECT_EQ(write_count.load(), 100);
    EXPECT_EQ(read_count.load(), 200);
}

//=============================================================================
// 5. RECOVERY SCENARIOS
//=============================================================================

TEST_F(CorticalSubstrateBridgeIntegrationTest, ATPReplenishmentRecovery) {
    // Deplete ATP
    deplete_atp(0.2f);
    substrate_update(substrate, 1);
    cortical_substrate_update(bridge);

    float impaired_fidelity = cortical_substrate_get_column_fidelity(bridge);
    EXPECT_LT(impaired_fidelity, 0.5f);
    EXPECT_TRUE(cortical_substrate_is_impaired(bridge));

    // Replenish ATP to full level (not just incremental)
    substrate_set_atp(substrate, 1.0f);
    substrate_update(substrate, 1);
    cortical_substrate_update(bridge);

    float recovered_fidelity = cortical_substrate_get_column_fidelity(bridge);
    EXPECT_GT(recovered_fidelity, impaired_fidelity)
        << "Fidelity should recover with ATP replenishment";

    // With full ATP, should no longer be impaired
    // Note: Impairment depends on fidelity >= 0.5 AND competition >= 0.3
    EXPECT_FALSE(cortical_substrate_is_impaired(bridge))
        << "Should recover from impaired state with full ATP";
}

TEST_F(CorticalSubstrateBridgeIntegrationTest, ThermalRegulationRecovery) {
    // Start at normal temperature
    substrate_set_temperature(substrate, 37.0f);
    substrate_update(substrate, 1);
    cortical_substrate_update(bridge);

    float normal_gain = cortical_substrate_get_layer_gain(bridge, 1);

    // Induce hyperthermia - Q10 effect increases gain at higher temperatures
    increase_temperature(40.0f);
    substrate_update(substrate, 1);
    cortical_substrate_update(bridge);

    float hyperthermic_gain = cortical_substrate_get_layer_gain(bridge, 1);

    // At 40°C, Q10 effect should increase layer gain above normal
    EXPECT_GT(hyperthermic_gain, normal_gain)
        << "Q10 effect should increase gain at elevated temperature";

    // Apply cooling by setting temperature back to normal
    substrate_set_temperature(substrate, 37.0f);
    substrate_update(substrate, 1);
    cortical_substrate_update(bridge);

    float recovered_gain = cortical_substrate_get_layer_gain(bridge, 1);

    // Gain should return to near-normal value after cooling
    EXPECT_NEAR(recovered_gain, normal_gain, 0.05f)
        << "Layer gain should return to normal after cooling";
}

TEST_F(CorticalSubstrateBridgeIntegrationTest, GradualRecoveryDynamics) {
    // Severe impairment
    deplete_atp(0.1f);
    increase_temperature(41.0f);
    substrate_update(substrate, 1);
    cortical_substrate_update(bridge);

    // Gradual recovery
    std::vector<float> fidelities;
    for (int i = 0; i < 10; i++) {
        // Gradually increase ATP
        substrate_metabolic_state_t met_state;
        substrate_get_metabolic_state(substrate, &met_state);
        substrate_set_atp(substrate, met_state.atp_level + 0.08f);

        // Gradually decrease temperature
        substrate_physical_state_t phys_state;
        substrate_get_physical_state(substrate, &phys_state);
        substrate_set_temperature(substrate, phys_state.temperature - 0.4f);

        substrate_update(substrate, 1);
        cortical_substrate_update(bridge);

        float fidelity = cortical_substrate_get_column_fidelity(bridge);
        fidelities.push_back(fidelity);

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Verify monotonic recovery
    for (size_t i = 1; i < fidelities.size(); i++) {
        EXPECT_GE(fidelities[i], fidelities[i-1])
            << "Fidelity should gradually recover";
    }
}

//=============================================================================
// 6. END-TO-END SENSORY PROCESSING
//=============================================================================

TEST_F(CorticalSubstrateBridgeIntegrationTest, NormalProcessingPipeline) {
    // Simulate normal cortical processing with healthy substrate
    // Use full ATP for best fidelity
    substrate_set_atp(substrate, 1.0f);
    substrate_set_temperature(substrate, 37.0f);

    // Process multiple cycles
    for (int cycle = 0; cycle < 20; cycle++) {
        // Simulate sensory input → cortical processing
        substrate_update(substrate, 1);
        cortical_substrate_update(bridge);

        // Verify high-fidelity processing
        // Fidelity = ATP * metabolic_capacity * sensitivity, clamped to [0.2, 1.0]
        float fidelity = cortical_substrate_get_column_fidelity(bridge);
        EXPECT_GT(fidelity, 0.8f) << "Should maintain high fidelity";

        // Competition should be full at ATP > 0.5
        float competition = cortical_substrate_get_competition_efficiency(bridge);
        EXPECT_GE(competition, 0.9f) << "Should maintain competition at full ATP";

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

TEST_F(CorticalSubstrateBridgeIntegrationTest, ImpairedProcessingPipeline) {
    // Simulate cortical processing under metabolic stress
    substrate_set_atp(substrate, 0.3f);
    substrate_set_temperature(substrate, 39.5f);

    // Process multiple cycles
    for (int cycle = 0; cycle < 20; cycle++) {
        // Consume ATP via spike recording
        substrate_record_spikes(substrate, 100);
        substrate_update(substrate, 1);
        cortical_substrate_update(bridge);

        // Verify degraded processing
        EXPECT_TRUE(cortical_substrate_is_impaired(bridge));

        float fidelity = cortical_substrate_get_column_fidelity(bridge);
        EXPECT_LT(fidelity, 0.7f) << "Fidelity should be degraded";

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    // Verify impairment events tracked
    cortical_substrate_stats_t stats;
    cortical_substrate_get_stats(bridge, &stats);
    EXPECT_GT(stats.impairment_events, 0);
}

TEST_F(CorticalSubstrateBridgeIntegrationTest, AdaptiveProcessingUnderStress) {
    // Simulate adaptive response to substrate stress
    std::vector<float> sparsity_values;

    // Gradually deplete ATP
    for (int step = 0; step < 10; step++) {
        float atp = 0.9f - step * 0.07f;
        substrate_set_atp(substrate, atp);
        substrate_update(substrate, 1);
        cortical_substrate_update(bridge);

        float sparsity = cortical_substrate_get_sparsity_modulation(bridge);
        sparsity_values.push_back(sparsity);

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Verify sparsity modulation adapts to energy constraints
    // Lower ATP should reduce competitive inhibition → less sparse
    EXPECT_NE(sparsity_values.front(), sparsity_values.back())
        << "Sparsity should adapt to metabolic state";
}

TEST_F(CorticalSubstrateBridgeIntegrationTest, LongRunningStabilityTest) {
    // Verify bridge remains stable over extended operation
    cortical_substrate_connect_bio_async(bridge);

    for (int iteration = 0; iteration < 100; iteration++) {
        // Vary substrate state
        float atp = 0.6f + 0.3f * sinf(iteration * 0.1f);
        float temp = 37.0f + 1.5f * sinf(iteration * 0.15f);

        substrate_set_atp(substrate, atp);
        substrate_set_temperature(substrate, temp);
        substrate_update(substrate, 1);

        int ret = cortical_substrate_update(bridge);
        EXPECT_EQ(ret, 0) << "Update failed at iteration " << iteration;

        // Verify bridge remains operational
        cortical_substrate_effects_t effects;
        ret = cortical_substrate_get_effects(bridge, &effects);
        EXPECT_EQ(ret, 0);

        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    // Verify statistics are reasonable
    cortical_substrate_stats_t stats;
    cortical_substrate_get_stats(bridge, &stats);
    EXPECT_EQ(stats.update_count, 100);
    EXPECT_GE(stats.avg_column_fidelity, 0.0f);
    EXPECT_LE(stats.avg_column_fidelity, 1.0f);
}

//=============================================================================
// Statistics and Monitoring Tests
//=============================================================================

TEST_F(CorticalSubstrateBridgeIntegrationTest, StatisticsAccumulation) {
    // Trigger various events
    for (int i = 0; i < 5; i++) {
        // Cause impairment
        deplete_atp(0.2f);
        substrate_update(substrate, 1);
        cortical_substrate_update(bridge);

        // Recover by setting ATP to high level
        substrate_set_atp(substrate, 0.9f);
        substrate_update(substrate, 1);
        cortical_substrate_update(bridge);
    }

    // Verify statistics tracked
    cortical_substrate_stats_t stats;
    cortical_substrate_get_stats(bridge, &stats);

    EXPECT_GT(stats.update_count, 0);
    EXPECT_GT(stats.impairment_events, 0);
    EXPECT_GE(stats.min_fidelity_observed, 0.0f);
    EXPECT_LE(stats.max_fidelity_observed, 1.0f);
    EXPECT_GE(stats.avg_column_fidelity, 0.0f);
    EXPECT_LE(stats.avg_column_fidelity, 1.0f);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
