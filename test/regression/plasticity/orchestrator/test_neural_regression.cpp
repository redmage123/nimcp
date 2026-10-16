/**
 * @file test_neural_regression.cpp
 * @brief Regression tests for neural-plasticity bridges and coordinator
 * @version 1.0.0
 * @date 2025-12-19
 *
 * WHAT: Tests long-term stability of neural-plasticity integration
 * WHY:  Ensure system remains stable across extended simulations
 * HOW:  Test parameter bounds, numerical stability, weight convergence
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <numeric>
#include <thread>
#include <atomic>

// Headers have their own extern "C" guards
#include "plasticity/orchestrator/nimcp_neural_plasticity_coordinator.h"
#include "plasticity/orchestrator/nimcp_axon_orchestrator_bridge.h"
#include "plasticity/orchestrator/nimcp_neuron_orchestrator_bridge.h"
#include "plasticity/orchestrator/nimcp_dendrite_orchestrator_bridge.h"
#include "plasticity/nimcp_plasticity_orchestrator.h"

// ============================================================================
// Test Fixtures
// ============================================================================

class NeuralPlasticityRegressionTest : public ::testing::Test {
protected:
    neural_plasticity_coordinator_t* coordinator = nullptr;

    void SetUp() override {
        neural_plasticity_config_t config;
        neural_plasticity_default_config(&config);
        coordinator = neural_plasticity_coordinator_create(&config, nullptr, nullptr);
        ASSERT_NE(coordinator, nullptr);
    }

    void TearDown() override {
        if (coordinator) neural_plasticity_coordinator_destroy(coordinator);
    }
};

// ============================================================================
// Parameter Stability Tests
// ============================================================================

TEST_F(NeuralPlasticityRegressionTest, DefaultConfigValuesValid) {
    neural_plasticity_config_t config;
    neural_plasticity_default_config(&config);

    // dt should be reasonable (0.01 - 10.0 ms typical)
    EXPECT_GE(config.default_dt_ms, 0.001f);
    EXPECT_LE(config.default_dt_ms, 10.0f);
}

TEST_F(NeuralPlasticityRegressionTest, AxonBridgeDefaultConfigValid) {
    axon_orchestrator_config_t config;
    axon_orchestrator_default_config(&config);

    EXPECT_GT(config.activity_ema_tau_ms, 0.0f);
    EXPECT_GT(config.initial_mapping_capacity, 0u);
}

TEST_F(NeuralPlasticityRegressionTest, NeuronBridgeDefaultConfigValid) {
    neuron_orchestrator_config_t config;
    neuron_orchestrator_default_config(&config);

    EXPECT_GT(config.rate_ema_tau_ms, 0.0f);
    EXPECT_GT(config.bap_amplitude, 0.0f);
    EXPECT_GT(config.spike_amplitude, 0.0f);
    EXPECT_GT(config.initial_neuron_capacity, 0u);
}

TEST_F(NeuralPlasticityRegressionTest, DendriteBridgeDefaultConfigValid) {
    dendrite_orchestrator_config_t config;
    dendrite_orchestrator_default_config(&config);

    EXPECT_GT(config.weight_to_volume_scale, 0.0f);
    EXPECT_GT(config.weight_to_ampa_scale, 0.0f);
    EXPECT_GE(config.min_weight_delta_for_sync, 0.0f);
    EXPECT_GT(config.initial_mapping_capacity, 0u);
}

// ============================================================================
// Numerical Stability Tests
// ============================================================================

TEST_F(NeuralPlasticityRegressionTest, WeightsBoundedAfterLongSimulation) {
    // Register synapses
    for (uint32_t i = 0; i < 50; i++) {
        neural_plasticity_register_synapse(coordinator, i, i % 10, (i + 1) % 10, 0, 0.5f);
    }

    // Long simulation with varying inputs
    float inputs[10] = {0};
    for (int t = 0; t < 10000; t++) {
        for (int i = 0; i < 10; i++) {
            inputs[i] = 5.0f + 15.0f * sinf((float)t * 0.01f + i);
        }
        neural_plasticity_step(coordinator, 0.1f, inputs, t * 100);
    }

    // All weights should remain bounded [0, 1]
    for (uint32_t i = 0; i < 50; i++) {
        float weight = neural_plasticity_get_weight(coordinator, i);
        EXPECT_GE(weight, 0.0f) << "Synapse " << i << " weight below 0";
        EXPECT_LE(weight, 1.0f) << "Synapse " << i << " weight above 1";
        EXPECT_FALSE(std::isnan(weight)) << "Synapse " << i << " weight is NaN";
        EXPECT_FALSE(std::isinf(weight)) << "Synapse " << i << " weight is Inf";
    }
}

TEST_F(NeuralPlasticityRegressionTest, StatsCountersNoOverflow) {
    // Run many steps
    float inputs[5] = {10.0f, 10.0f, 10.0f, 10.0f, 10.0f};
    for (int t = 0; t < 10000; t++) {
        neural_plasticity_step(coordinator, 0.1f, inputs, t * 100);
    }

    neural_plasticity_stats_t stats;
    neural_plasticity_get_stats(coordinator, &stats);

    // Counters should be consistent
    EXPECT_EQ(stats.total_steps, 10000u);
}

// ============================================================================
// Long-Term Stability Tests
// ============================================================================

TEST_F(NeuralPlasticityRegressionTest, NoWeightDrift) {
    neural_plasticity_register_synapse(coordinator, 100, 0, 1, 0, 0.5f);

    // Zero input should not drift weights significantly
    float inputs[2] = {0.0f, 0.0f};
    float initial_weight = neural_plasticity_get_weight(coordinator, 100);

    for (int t = 0; t < 5000; t++) {
        neural_plasticity_step(coordinator, 0.1f, inputs, t * 100);
    }

    float final_weight = neural_plasticity_get_weight(coordinator, 100);

    // Weight should not drift significantly with no activity
    EXPECT_NEAR(final_weight, initial_weight, 0.2f);
}

TEST_F(NeuralPlasticityRegressionTest, ConsistentResultsAcrossRuns) {
    std::vector<float> final_weights_run1;
    std::vector<float> final_weights_run2;

    // First run
    {
        neural_plasticity_coordinator_destroy(coordinator);

        neural_plasticity_config_t config;
        neural_plasticity_default_config(&config);
        coordinator = neural_plasticity_coordinator_create(&config, nullptr, nullptr);

        for (uint32_t i = 0; i < 10; i++) {
            neural_plasticity_register_synapse(coordinator, i, i % 5, (i + 1) % 5, 0, 0.5f);
        }

        float inputs[5] = {10.0f, 10.0f, 10.0f, 10.0f, 10.0f};
        for (int t = 0; t < 1000; t++) {
            neural_plasticity_step(coordinator, 0.1f, inputs, t * 100);
        }

        for (uint32_t i = 0; i < 10; i++) {
            final_weights_run1.push_back(neural_plasticity_get_weight(coordinator, i));
        }

        neural_plasticity_coordinator_destroy(coordinator);
        coordinator = nullptr;
    }

    // Second run with identical setup
    {
        neural_plasticity_config_t config;
        neural_plasticity_default_config(&config);
        coordinator = neural_plasticity_coordinator_create(&config, nullptr, nullptr);

        for (uint32_t i = 0; i < 10; i++) {
            neural_plasticity_register_synapse(coordinator, i, i % 5, (i + 1) % 5, 0, 0.5f);
        }

        float inputs[5] = {10.0f, 10.0f, 10.0f, 10.0f, 10.0f};
        for (int t = 0; t < 1000; t++) {
            neural_plasticity_step(coordinator, 0.1f, inputs, t * 100);
        }

        for (uint32_t i = 0; i < 10; i++) {
            final_weights_run2.push_back(neural_plasticity_get_weight(coordinator, i));
        }
    }

    // Weights should be identical between runs
    ASSERT_EQ(final_weights_run1.size(), final_weights_run2.size());
    for (size_t i = 0; i < final_weights_run1.size(); i++) {
        EXPECT_FLOAT_EQ(final_weights_run1[i], final_weights_run2[i])
            << "Weight mismatch at synapse " << i;
    }
}

// ============================================================================
// Memory Stability Tests
// ============================================================================

TEST_F(NeuralPlasticityRegressionTest, RepeatedCreateDestroy) {
    for (int iteration = 0; iteration < 100; iteration++) {
        neural_plasticity_coordinator_destroy(coordinator);

        neural_plasticity_config_t config;
        neural_plasticity_default_config(&config);
        coordinator = neural_plasticity_coordinator_create(&config, nullptr, nullptr);
        ASSERT_NE(coordinator, nullptr) << "Failed at iteration " << iteration;

        float inputs[10] = {5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f};
        for (int t = 0; t < 100; t++) {
            neural_plasticity_step(coordinator, 0.1f, inputs, t * 100);
        }
    }
}

TEST_F(NeuralPlasticityRegressionTest, DynamicSynapseAddRemove) {
    // Repeatedly add and remove synapses
    for (int cycle = 0; cycle < 50; cycle++) {
        // Add synapses
        for (uint32_t i = 0; i < 20; i++) {
            uint32_t syn_id = cycle * 1000 + i;
            neural_plasticity_register_synapse(coordinator, syn_id, i % 10, (i + 1) % 10, 0, 0.5f);
        }

        // Run simulation
        float inputs[10] = {10.0f, 10.0f, 10.0f, 10.0f, 10.0f, 10.0f, 10.0f, 10.0f, 10.0f, 10.0f};
        for (int t = 0; t < 50; t++) {
            neural_plasticity_step(coordinator, 0.1f, inputs, t * 100);
        }

        // Remove synapses
        for (uint32_t i = 0; i < 20; i++) {
            uint32_t syn_id = cycle * 1000 + i;
            neural_plasticity_unregister_synapse(coordinator, syn_id);
        }
    }

    // Should complete without memory issues
    neural_plasticity_stats_t stats;
    neural_plasticity_get_stats(coordinator, &stats);
    EXPECT_GT(stats.total_steps, 0u);
}

// ============================================================================
// Bridge-Specific Regression Tests
// ============================================================================

class AxonBridgeRegressionTest : public ::testing::Test {
protected:
    axon_orchestrator_bridge_t* bridge = nullptr;
    plasticity_orchestrator_t* orchestrator = nullptr;

    void SetUp() override {
        plasticity_orchestrator_config_t orch_config;
        plasticity_orchestrator_default_config(&orch_config);
        orchestrator = plasticity_orchestrator_create(&orch_config);
        ASSERT_NE(orchestrator, nullptr);

        axon_orchestrator_config_t config;
        axon_orchestrator_default_config(&config);
        bridge = axon_orchestrator_bridge_create(&config, orchestrator, nullptr);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) axon_orchestrator_bridge_destroy(bridge);
        if (orchestrator) plasticity_orchestrator_destroy(orchestrator);
    }
};

TEST_F(AxonBridgeRegressionTest, MappingStability) {
    // Add and verify many mappings
    for (uint32_t i = 0; i < 5000; i++) {
        EXPECT_EQ(axon_orchestrator_map_synapse(bridge, i, i % 100), 0);
    }

    EXPECT_EQ(axon_orchestrator_get_mapping_count(bridge), 5000u);
}

TEST_F(AxonBridgeRegressionTest, RepeatedMapUnmap) {
    for (int cycle = 0; cycle < 100; cycle++) {
        // Map synapses
        for (uint32_t i = 0; i < 100; i++) {
            axon_orchestrator_map_synapse(bridge, i, i % 10);
        }

        EXPECT_EQ(axon_orchestrator_get_mapping_count(bridge), 100u);

        // Unmap all
        for (uint32_t i = 0; i < 100; i++) {
            axon_orchestrator_unmap_synapse(bridge, i);
        }

        EXPECT_EQ(axon_orchestrator_get_mapping_count(bridge), 0u);
    }
}

// ============================================================================
// Thread Safety Tests
// ============================================================================

TEST_F(NeuralPlasticityRegressionTest, ConcurrentStatsAccess) {
    std::atomic<bool> stop{false};
    std::atomic<int> stats_calls{0};

    // Thread that runs simulation
    std::thread sim_thread([&]() {
        float inputs[10] = {10.0f, 10.0f, 10.0f, 10.0f, 10.0f,
                            10.0f, 10.0f, 10.0f, 10.0f, 10.0f};
        for (int t = 0; t < 5000 && !stop; t++) {
            neural_plasticity_step(coordinator, 0.1f, inputs, t * 100);
        }
    });

    // Thread that reads stats
    std::thread stats_thread([&]() {
        while (!stop) {
            neural_plasticity_stats_t stats;
            if (neural_plasticity_get_stats(coordinator, &stats) == 0) {
                stats_calls++;
            }
        }
    });

    // Let it run briefly
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    stop = true;

    sim_thread.join();
    stats_thread.join();

    EXPECT_GT(stats_calls.load(), 0);
}

// ============================================================================
// Biological Validity Tests
// ============================================================================

TEST_F(NeuralPlasticityRegressionTest, WeightsRemainValid) {
    for (uint32_t i = 0; i < 20; i++) {
        neural_plasticity_register_synapse(coordinator, i, i % 5, (i + 1) % 5, 0, 0.5f);
    }

    // Drive simulation with varying inputs
    for (int t = 0; t < 10000; t++) {
        float inputs[10];
        for (int i = 0; i < 10; i++) {
            inputs[i] = 5.0f + i * 3.0f;  // 5-35 range
        }
        neural_plasticity_step(coordinator, 0.1f, inputs, t * 100);
    }

    // All weights should be in valid range
    for (uint32_t i = 0; i < 20; i++) {
        float weight = neural_plasticity_get_weight(coordinator, i);
        EXPECT_GE(weight, 0.0f);
        EXPECT_LE(weight, 1.0f);
    }
}

TEST_F(NeuralPlasticityRegressionTest, StatsResetWorks) {
    float inputs[5] = {10.0f, 10.0f, 10.0f, 10.0f, 10.0f};
    for (int t = 0; t < 1000; t++) {
        neural_plasticity_step(coordinator, 0.1f, inputs, t * 100);
    }

    neural_plasticity_stats_t stats;
    neural_plasticity_get_stats(coordinator, &stats);
    EXPECT_EQ(stats.total_steps, 1000u);

    neural_plasticity_reset_stats(coordinator);

    neural_plasticity_get_stats(coordinator, &stats);
    EXPECT_EQ(stats.total_steps, 0u);
}

