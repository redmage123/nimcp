/**
 * @file test_nimcp_brainstem_quantum_bridge.cpp
 * @brief Unit tests for nimcp_brainstem_quantum_bridge.c
 *
 * WHAT: Unit tests for the brainstem quantum bridge
 * WHY:  Ensure correct quantum-accelerated processing for reflexes and arousal
 * HOW:  Use Google Test framework to test quantum operations and fallbacks
 *
 * COVERAGE TARGET: 100%
 */

#include <gtest/gtest.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

// Headers have their own extern "C" guards
#include "core/brain/regions/brainstem/nimcp_brainstem_adapter.h"
#include "core/brain/regions/brainstem/nimcp_brainstem_quantum_bridge.h"

// ============================================================================
// TEST FIXTURE
// ============================================================================

class BrainstemQuantumBridgeTest : public ::testing::Test {
protected:
    brainstem_adapter_t* adapter;
    brainstem_quantum_bridge_t* bridge;
    brainstem_config_t config;
    brainstem_quantum_config_t qconfig;

    void SetUp() override {
        config = brainstem_default_config();
        config.enable_bio_async = false;
        adapter = brainstem_create(&config, NULL);
        ASSERT_NE(nullptr, adapter) << "Failed to create Brainstem adapter";

        qconfig = brainstem_quantum_default_config();
        bridge = brainstem_quantum_bridge_create(adapter, &qconfig);
        ASSERT_NE(nullptr, bridge) << "Failed to create Quantum bridge";
    }

    void TearDown() override {
        if (bridge) {
            brainstem_quantum_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (adapter) {
            brainstem_destroy(adapter);
            adapter = nullptr;
        }
    }
};

// ============================================================================
// LIFECYCLE TESTS
// ============================================================================

TEST_F(BrainstemQuantumBridgeTest, DefaultConfigHasReasonableValues) {
    brainstem_quantum_config_t default_config = brainstem_quantum_default_config();

    EXPECT_EQ(default_config.reflex_algorithm, BRAINSTEM_QUANTUM_ALG_GROVER);
    EXPECT_EQ(default_config.arousal_algorithm, BRAINSTEM_QUANTUM_ALG_ANNEALING);
    EXPECT_EQ(default_config.sensory_algorithm, BRAINSTEM_QUANTUM_ALG_AMPLITUDE);
    EXPECT_GT(default_config.max_qubits, 0u);
    EXPECT_GT(default_config.grover_iterations, 0u);
    EXPECT_GT(default_config.annealing_steps, 0u);
    EXPECT_GT(default_config.annealing_temperature, 0.0f);
}

TEST_F(BrainstemQuantumBridgeTest, CreateWithNullAdapterFails) {
    brainstem_quantum_bridge_t* bad_bridge = brainstem_quantum_bridge_create(NULL, NULL);
    EXPECT_EQ(nullptr, bad_bridge);
}

TEST_F(BrainstemQuantumBridgeTest, CreateWithNullConfigUsesDefaults) {
    brainstem_quantum_bridge_t* bridge2 = brainstem_quantum_bridge_create(adapter, NULL);
    // Should use defaults, but may or may not succeed depending on implementation
    // At minimum, it shouldn't crash
    if (bridge2) {
        brainstem_quantum_bridge_destroy(bridge2);
    }
}

TEST_F(BrainstemQuantumBridgeTest, DestroyNullDoesNotCrash) {
    brainstem_quantum_bridge_destroy(NULL);
    // Should not crash
}

// ============================================================================
// QUANTUM REFLEX SELECTION TESTS
// ============================================================================

TEST_F(BrainstemQuantumBridgeTest, SelectReflexBasic) {
    float stimulus[] = {0.5f, 0.3f, 0.8f, 0.2f};
    quantum_reflex_result_t result;

    EXPECT_TRUE(brainstem_quantum_select_reflex(bridge, stimulus, 4, 0.7f, &result));

    // Should have valid result (even if classical fallback)
    EXPECT_GE(result.pathways_evaluated, 1u);
    EXPECT_GE(result.execution_time_us, 0.0);
}

TEST_F(BrainstemQuantumBridgeTest, SelectReflexLowUrgency) {
    float stimulus[] = {0.1f, 0.1f, 0.1f, 0.1f};
    quantum_reflex_result_t result;

    EXPECT_TRUE(brainstem_quantum_select_reflex(bridge, stimulus, 4, 0.2f, &result));

    // Low urgency should result in no/weak reflex selection
    EXPECT_EQ(result.selected_reflex_id, 0u);
}

TEST_F(BrainstemQuantumBridgeTest, SelectReflexHighUrgency) {
    float stimulus[] = {0.9f, 0.8f, 0.7f, 0.6f};
    quantum_reflex_result_t result;

    EXPECT_TRUE(brainstem_quantum_select_reflex(bridge, stimulus, 4, 0.9f, &result));

    // High urgency should select a reflex
    EXPECT_GT(result.selected_reflex_id, 0u);
    EXPECT_GT(result.selection_confidence, 0.0f);
}

TEST_F(BrainstemQuantumBridgeTest, SelectReflexNull) {
    quantum_reflex_result_t result;
    EXPECT_FALSE(brainstem_quantum_select_reflex(NULL, NULL, 0, 0, &result));
    EXPECT_FALSE(brainstem_quantum_select_reflex(bridge, NULL, 0, 0, NULL));
}

// ============================================================================
// QUANTUM AROUSAL OPTIMIZATION TESTS
// ============================================================================

TEST_F(BrainstemQuantumBridgeTest, OptimizeArousalBasic) {
    quantum_arousal_result_t result;

    EXPECT_TRUE(brainstem_quantum_optimize_arousal(
        bridge,
        0.5f,   // current_arousal
        0.5f,   // sensory_load
        0.8f,   // metabolic_state
        0.2f,   // threat_level
        &result));

    EXPECT_GE(result.optimal_arousal, 0.0f);
    EXPECT_LE(result.optimal_arousal, 1.0f);
    EXPECT_GE(result.iterations, 1u);
    EXPECT_GE(result.execution_time_us, 0.0);
}

TEST_F(BrainstemQuantumBridgeTest, OptimizeArousalHighThreat) {
    quantum_arousal_result_t result;

    EXPECT_TRUE(brainstem_quantum_optimize_arousal(
        bridge,
        0.5f,   // current_arousal
        0.5f,   // sensory_load
        0.8f,   // metabolic_state
        0.9f,   // HIGH threat_level
        &result));

    // High threat should result in higher optimal arousal
    EXPECT_GT(result.optimal_arousal, 0.5f);
}

TEST_F(BrainstemQuantumBridgeTest, OptimizeArousalLowMetabolic) {
    quantum_arousal_result_t result;

    EXPECT_TRUE(brainstem_quantum_optimize_arousal(
        bridge,
        0.5f,   // current_arousal
        0.5f,   // sensory_load
        0.2f,   // LOW metabolic_state
        0.1f,   // threat_level
        &result));

    // Low metabolic should result in lower optimal arousal
    EXPECT_LT(result.optimal_arousal, 0.5f);
}

TEST_F(BrainstemQuantumBridgeTest, OptimizeArousalNull) {
    quantum_arousal_result_t result;
    EXPECT_FALSE(brainstem_quantum_optimize_arousal(NULL, 0, 0, 0, 0, &result));
    EXPECT_FALSE(brainstem_quantum_optimize_arousal(bridge, 0, 0, 0, 0, NULL));
}

// ============================================================================
// QUANTUM SENSORY INTEGRATION TESTS
// ============================================================================

TEST_F(BrainstemQuantumBridgeTest, IntegrateSensoryVisualOnly) {
    brainstem_sensory_input_t visual = {
        .visual_target_x = 0.5f,
        .visual_target_y = -0.3f,
        .visual_salience = 0.8f
    };

    brainstem_sensory_input_t auditory = {0};

    quantum_sensory_result_t result;
    EXPECT_TRUE(brainstem_quantum_integrate_sensory(bridge, &visual, &auditory, &result));

    EXPECT_GT(result.visual_weight, 0.9f); // Visual dominates
    EXPECT_LT(result.auditory_weight, 0.1f);
    EXPECT_NEAR(result.integrated_salience, 0.8f, 0.1f);
}

TEST_F(BrainstemQuantumBridgeTest, IntegrateSensoryAuditoryOnly) {
    brainstem_sensory_input_t visual = {0};

    brainstem_sensory_input_t auditory = {
        .sound_azimuth = 45.0f,
        .sound_elevation = 10.0f,
        .sound_intensity = 0.7f
    };

    quantum_sensory_result_t result;
    EXPECT_TRUE(brainstem_quantum_integrate_sensory(bridge, &visual, &auditory, &result));

    EXPECT_GT(result.auditory_weight, 0.9f); // Auditory dominates
    EXPECT_LT(result.visual_weight, 0.1f);
}

TEST_F(BrainstemQuantumBridgeTest, IntegrateSensoryBoth) {
    brainstem_sensory_input_t visual = {
        .visual_target_x = 0.3f,
        .visual_target_y = 0.2f,
        .visual_salience = 0.6f
    };

    brainstem_sensory_input_t auditory = {
        .sound_azimuth = -30.0f,
        .sound_intensity = 0.4f
    };

    quantum_sensory_result_t result;
    EXPECT_TRUE(brainstem_quantum_integrate_sensory(bridge, &visual, &auditory, &result));

    // Both should contribute
    EXPECT_GT(result.visual_weight, 0.0f);
    EXPECT_GT(result.auditory_weight, 0.0f);
    EXPECT_NEAR(result.visual_weight + result.auditory_weight, 1.0f, 0.01f);
}

TEST_F(BrainstemQuantumBridgeTest, IntegrateSensoryNull) {
    quantum_sensory_result_t result;
    EXPECT_FALSE(brainstem_quantum_integrate_sensory(NULL, NULL, NULL, &result));
    EXPECT_FALSE(brainstem_quantum_integrate_sensory(bridge, NULL, NULL, NULL));
}

// ============================================================================
// PARALLEL PATHWAY EVALUATION TESTS
// ============================================================================

TEST_F(BrainstemQuantumBridgeTest, EvaluatePathways) {
    uint32_t pathway_ids[10];
    float activations[10];

    uint32_t count = brainstem_quantum_evaluate_pathways(
        bridge, 0.8f, pathway_ids, activations, 10);

    EXPECT_GT(count, 0u);
    EXPECT_LE(count, 10u);

    // All activations should be valid
    for (uint32_t i = 0; i < count; i++) {
        EXPECT_GE(activations[i], 0.0f);
        EXPECT_LE(activations[i], 1.0f);
    }
}

TEST_F(BrainstemQuantumBridgeTest, EvaluatePathwaysLowStimulus) {
    uint32_t pathway_ids[10];
    float activations[10];

    uint32_t count = brainstem_quantum_evaluate_pathways(
        bridge, 0.1f, pathway_ids, activations, 10);

    // Low stimulus should activate fewer pathways
    EXPECT_LE(count, 2u);
}

TEST_F(BrainstemQuantumBridgeTest, EvaluatePathwaysNull) {
    uint32_t pathway_ids[10];
    float activations[10];

    EXPECT_EQ(0u, brainstem_quantum_evaluate_pathways(NULL, 0, NULL, NULL, 0));
    EXPECT_EQ(0u, brainstem_quantum_evaluate_pathways(bridge, 0, NULL, activations, 10));
}

// ============================================================================
// STATE AND UPDATE TESTS
// ============================================================================

TEST_F(BrainstemQuantumBridgeTest, UpdateSuccess) {
    EXPECT_TRUE(brainstem_quantum_bridge_update(bridge, 0.1f));
}

TEST_F(BrainstemQuantumBridgeTest, UpdateNull) {
    EXPECT_FALSE(brainstem_quantum_bridge_update(NULL, 0.1f));
}

// ============================================================================
// STATISTICS TESTS
// ============================================================================

TEST_F(BrainstemQuantumBridgeTest, GetStats) {
    // Do some operations
    float stimulus[] = {0.5f, 0.5f};
    quantum_reflex_result_t reflex_result;
    brainstem_quantum_select_reflex(bridge, stimulus, 2, 0.5f, &reflex_result);

    quantum_arousal_result_t arousal_result;
    brainstem_quantum_optimize_arousal(bridge, 0.5f, 0.5f, 0.5f, 0.5f, &arousal_result);

    brainstem_quantum_stats_t stats;
    EXPECT_TRUE(brainstem_quantum_bridge_get_stats(bridge, &stats));

    EXPECT_EQ(stats.reflex_queries, 1u);
    EXPECT_EQ(stats.arousal_optimizations, 1u);
}

TEST_F(BrainstemQuantumBridgeTest, GetStatsNull) {
    brainstem_quantum_stats_t stats;
    EXPECT_FALSE(brainstem_quantum_bridge_get_stats(NULL, &stats));
    EXPECT_FALSE(brainstem_quantum_bridge_get_stats(bridge, NULL));
}

// ============================================================================
// QUANTUM AVAILABILITY TESTS
// ============================================================================

TEST_F(BrainstemQuantumBridgeTest, IsQuantumAvailable) {
    // Without connected quantum resources, should be false
    EXPECT_FALSE(brainstem_quantum_bridge_is_quantum_available(bridge));
}

TEST_F(BrainstemQuantumBridgeTest, IsQuantumAvailableNull) {
    EXPECT_FALSE(brainstem_quantum_bridge_is_quantum_available(NULL));
}

// ============================================================================
// SPEEDUP ESTIMATION TESTS
// ============================================================================

TEST_F(BrainstemQuantumBridgeTest, EstimateSpeedupGrover) {
    float speedup = brainstem_quantum_bridge_estimate_speedup(
        bridge, 100, BRAINSTEM_QUANTUM_ALG_GROVER);

    // Grover gives sqrt(N) speedup
    EXPECT_NEAR(speedup, 10.0f, 1.0f);
}

TEST_F(BrainstemQuantumBridgeTest, EstimateSpeedupAnnealing) {
    float speedup = brainstem_quantum_bridge_estimate_speedup(
        bridge, 100, BRAINSTEM_QUANTUM_ALG_ANNEALING);

    EXPECT_GT(speedup, 1.0f);
}

TEST_F(BrainstemQuantumBridgeTest, EstimateSpeedupNone) {
    float speedup = brainstem_quantum_bridge_estimate_speedup(
        bridge, 100, BRAINSTEM_QUANTUM_ALG_NONE);

    EXPECT_FLOAT_EQ(speedup, 1.0f);
}

TEST_F(BrainstemQuantumBridgeTest, EstimateSpeedupSmallProblem) {
    float speedup = brainstem_quantum_bridge_estimate_speedup(
        bridge, 4, BRAINSTEM_QUANTUM_ALG_GROVER);

    // Small problems don't benefit from quantum
    EXPECT_LE(speedup, 2.0f);
}

// ============================================================================
// MIX RATIO TESTS
// ============================================================================

TEST_F(BrainstemQuantumBridgeTest, SetMixRatio) {
    EXPECT_TRUE(brainstem_quantum_bridge_set_mix(bridge, 0.7f));
}

TEST_F(BrainstemQuantumBridgeTest, SetMixRatioClamping) {
    // Should clamp values
    EXPECT_TRUE(brainstem_quantum_bridge_set_mix(bridge, -0.5f));
    EXPECT_TRUE(brainstem_quantum_bridge_set_mix(bridge, 1.5f));
}

TEST_F(BrainstemQuantumBridgeTest, SetMixRatioNull) {
    EXPECT_FALSE(brainstem_quantum_bridge_set_mix(NULL, 0.5f));
}

// ============================================================================
// CONNECT TESTS
// ============================================================================

TEST_F(BrainstemQuantumBridgeTest, ConnectReasonerNull) {
    // Connecting NULL should succeed (clears connection)
    EXPECT_TRUE(brainstem_quantum_bridge_connect_reasoner(bridge, NULL));
}

TEST_F(BrainstemQuantumBridgeTest, ConnectAnnealerNull) {
    EXPECT_TRUE(brainstem_quantum_bridge_connect_annealer(bridge, NULL));
}

TEST_F(BrainstemQuantumBridgeTest, ConnectToNullBridge) {
    EXPECT_FALSE(brainstem_quantum_bridge_connect_reasoner(NULL, NULL));
    EXPECT_FALSE(brainstem_quantum_bridge_connect_annealer(NULL, NULL));
}
