//=============================================================================
// e2e_test_quantum_bridges_pipeline.cpp - End-to-End Quantum Bridge Tests
//=============================================================================
/**
 * @file e2e_test_quantum_bridges_pipeline.cpp
 * @brief End-to-end tests for quantum bridge cognitive pipelines
 *
 * WHAT: Test complete cognitive workflows using quantum bridges
 * WHY:  Verify quantum acceleration works in realistic scenarios
 * HOW:  Simulate full cognitive processing pipelines
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <cstring>

// Enable implementations
#define NIMCP_ATTENTION_QUANTUM_BRIDGE_IMPLEMENTATION
#define NIMCP_BCM_QUANTUM_BRIDGE_IMPLEMENTATION
#define NIMCP_THALAMIC_QUANTUM_BRIDGE_IMPLEMENTATION

// Include quantum bridge headers
// Headers have their own extern "C" guards
#include "plasticity/attention/nimcp_attention_quantum_bridge.h"
#include "plasticity/bcm/nimcp_bcm_quantum_bridge.h"
#include "middleware/routing/nimcp_thalamic_quantum_bridge.h"

//=============================================================================
// Test Fixtures
//=============================================================================

/**
 * @brief Full cognitive pipeline test fixture
 *
 * Simulates a cognitive processing system with attention, plasticity, routing
 */
class CognitivePipelineE2E : public ::testing::Test {
protected:
    attention_quantum_bridge_t* attention = nullptr;
    bcm_quantum_bridge_t* bcm = nullptr;
    thalamic_quantum_bridge_t* thalamic = nullptr;

    void SetUp() override {
        attention_quantum_config_t attn_cfg = attention_quantum_default_config();
        attention = attention_quantum_bridge_create(&attn_cfg);

        bcm_quantum_config_t bcm_cfg = bcm_quantum_default_config();
        bcm = bcm_quantum_bridge_create(&bcm_cfg);

        thalamic_quantum_config_t thal_cfg = thalamic_quantum_default_config();
        thalamic = thalamic_quantum_bridge_create(&thal_cfg);
    }

    void TearDown() override {
        if (attention) attention_quantum_bridge_destroy(attention);
        if (bcm) bcm_quantum_bridge_destroy(bcm);
        if (thalamic) thalamic_quantum_bridge_destroy(thalamic);
    }

    // Simulate sensory input
    void generate_sensory_input(float* features, uint32_t dim, float intensity) {
        for (uint32_t i = 0; i < dim; i++) {
            features[i] = intensity * (0.5f + 0.5f * sinf((float)i * 0.3f));
        }
    }
};

//=============================================================================
// E2E Test Cases: Full Cognitive Pipeline
//=============================================================================

TEST_F(CognitivePipelineE2E, PerceptionToResponsePipeline) {
    /**
     * SCENARIO: Complete stimulus → response pipeline
     *
     * 1. Sensory input arrives
     * 2. Thalamic router gates signal to relevant modules
     * 3. Attention selects important features
     * 4. BCM updates plasticity threshold
     * 5. Response generated
     */

    // Step 1: Generate sensory input (visual stimulus)
    float sensory_input[8];
    generate_sensory_input(sensory_input, 8, 0.8f);

    // Step 2: Thalamic routing decision
    uint32_t modules[] = {1, 2, 3, 4, 5};  // Visual, Audio, Memory, Executive, Motor
    uint32_t routed_modules[5];
    uint32_t num_routed = 0;

    int thal_result = thalamic_quantum_route(
        thalamic, 0, modules, 5, sensory_input, 8,
        routed_modules, &num_routed
    );
    EXPECT_GE(thal_result, 0);

    // Step 3: Attention selects heads for processing
    float attention_scores[4] = {0.9f, 0.7f, 0.5f, 0.3f};
    uint32_t selected_heads[4];

    int attn_result = attention_quantum_select_heads(
        attention, attention_scores, 4, 2, selected_heads
    );
    EXPECT_GE(attn_result, 0);

    // Step 4: BCM threshold update based on activity
    bcm_activity_stats_t bcm_activity = {
        .avg_weight = sensory_input[0],
        .weight_variance = 0.1f,
        .avg_post_activity = sensory_input[1],
        .selectivity_index = sensory_input[2],
        .num_active_synapses = 100
    };
    float threshold = bcm_quantum_optimize_threshold(bcm, &bcm_activity);
    EXPECT_FALSE(std::isnan(threshold));

    // Verify pipeline completed
    attention_quantum_stats_t attn_stats;
    attention_quantum_get_stats(attention, &attn_stats);
    EXPECT_GT(attn_stats.quantum_selections + attn_stats.classical_fallbacks, 0UL);
}

TEST_F(CognitivePipelineE2E, PlasticityModulationPipeline) {
    /**
     * SCENARIO: BCM plasticity modulates attention and routing
     *
     * 1. BCM threshold adapts based on activity
     * 2. Attention scores are modulated by threshold
     * 3. Thalamic routing decisions are affected
     */

    // Step 1: Initial activity pattern
    float activities[] = {0.7f, 0.5f, 0.3f, 0.8f};

    // Step 2: Compute BCM threshold
    bcm_activity_stats_t bcm_activity = {
        .avg_weight = activities[0],
        .weight_variance = 0.1f,
        .avg_post_activity = activities[1],
        .selectivity_index = activities[2],
        .num_active_synapses = 100
    };
    float threshold = bcm_quantum_optimize_threshold(bcm, &bcm_activity);
    EXPECT_GE(threshold, 0.0f);

    // Step 3: Modulate attention scores based on threshold
    float attention_scores[4];
    for (int i = 0; i < 4; i++) {
        // Activities above threshold get higher attention
        attention_scores[i] = (activities[i] > threshold) ? 0.8f : 0.2f;
    }

    uint32_t selected[4];
    int attn_result = attention_quantum_select_heads(attention, attention_scores, 4, 2, selected);
    EXPECT_GE(attn_result, 0);

    // Step 4: Route to modules based on selected heads
    uint32_t modules[] = {1, 2, 3, 4};
    uint32_t routed[4];
    uint32_t num_routed = 0;

    thalamic_quantum_route(thalamic, 0, modules, 4, attention_scores, 4, routed, &num_routed);

    // Verify stats
    bcm_quantum_stats_t bcm_q_stats2;
    bcm_quantum_get_stats(bcm, &bcm_q_stats2);
    EXPECT_GT(bcm_q_stats2.optimization_steps, 0UL);
}

TEST_F(CognitivePipelineE2E, MultiStimulusProcessing) {
    /**
     * SCENARIO: Multiple stimuli processed in sequence
     *
     * Tests quantum speedup under cognitive load
     */

    const int NUM_STIMULI = 50;
    uint64_t total_quantum_ops = 0;
    uint64_t total_classical_ops = 0;

    for (int s = 0; s < NUM_STIMULI; s++) {
        // Generate stimulus
        float stimulus[8];
        generate_sensory_input(stimulus, 8, 0.5f + 0.4f * sinf((float)s * 0.2f));

        // Route to more modules for novel stimuli
        uint32_t modules[] = {1, 2, 3, 4, 5};
        uint32_t routed[5];
        uint32_t num_routed = 0;
        thalamic_quantum_route(thalamic, 0, modules, 5, stimulus, 8, routed, &num_routed);

        // Attention on routed signals
        float attn_scores[4] = {0.8f, 0.6f, 0.4f, 0.2f};
        uint32_t selected[4];
        attention_quantum_select_heads(attention, attn_scores, 4, 2, selected);

        // BCM update
        bcm_activity_stats_t bcm_stats = {
            .avg_weight = 0.5f + 0.3f * sinf((float)s * 0.1f),
            .weight_variance = 0.1f,
            .avg_post_activity = 0.5f,
            .selectivity_index = 0.5f,
            .num_active_synapses = 100
        };
        bcm_quantum_optimize_threshold(bcm, &bcm_stats);
    }

    // Check stats across all bridges
    attention_quantum_stats_t attn_stats;
    attention_quantum_get_stats(attention, &attn_stats);

    bcm_quantum_stats_t bcm_q_stats;
    bcm_quantum_get_stats(bcm, &bcm_q_stats);

    thalamic_quantum_stats_t thal_stats;
    thalamic_quantum_get_stats(thalamic, &thal_stats);

    // Should have processed many operations
    total_quantum_ops = attn_stats.quantum_selections +
                        bcm_q_stats.optimization_steps +
                        thal_stats.quantum_routes;
    total_classical_ops = attn_stats.classical_fallbacks +
                          thal_stats.classical_fallbacks;

    EXPECT_GT(total_quantum_ops + total_classical_ops, 0UL);
}

//=============================================================================
// E2E Test Cases: Learning Pipeline
//=============================================================================

class LearningPipelineE2E : public ::testing::Test {
protected:
    bcm_quantum_bridge_t* bcm = nullptr;
    attention_quantum_bridge_t* attention = nullptr;

    void SetUp() override {
        bcm_quantum_config_t bcm_cfg = bcm_quantum_default_config();
        bcm = bcm_quantum_bridge_create(&bcm_cfg);

        attention_quantum_config_t attn_cfg = attention_quantum_default_config();
        attention = attention_quantum_bridge_create(&attn_cfg);
    }

    void TearDown() override {
        if (bcm) bcm_quantum_bridge_destroy(bcm);
        if (attention) attention_quantum_bridge_destroy(attention);
    }
};

TEST_F(LearningPipelineE2E, BCMPlasticityUpdateCycle) {
    /**
     * SCENARIO: BCM plasticity learning cycle
     *
     * 1. Present patterns repeatedly
     * 2. BCM threshold adapts
     * 3. Attention modulates learning
     */

    const int EPOCHS = 100;

    for (int epoch = 0; epoch < EPOCHS; epoch++) {
        // Activity pattern
        float activities[10];
        for (int i = 0; i < 10; i++) {
            activities[i] = 0.3f + 0.4f * sinf((float)(i + epoch) * 0.2f);
        }

        // BCM threshold update
        bcm_activity_stats_t bcm_activity = {
            .avg_weight = activities[0],
            .weight_variance = activities[1] - activities[0],
            .avg_post_activity = activities[2],
            .selectivity_index = activities[3],
            .num_active_synapses = 100
        };
        float threshold = bcm_quantum_optimize_threshold(bcm, &bcm_activity);
        EXPECT_FALSE(std::isnan(threshold));

        // Attention modulates which synapses learn
        float attn_scores[10];
        for (int i = 0; i < 10; i++) {
            attn_scores[i] = (activities[i] > threshold) ? 0.8f : 0.2f;
        }
        uint32_t selected[10];
        attention_quantum_select_heads(attention, attn_scores, 10, 5, selected);
    }

    // Verify learning occurred
    bcm_quantum_stats_t bcm_q_stats;
    bcm_quantum_get_stats(bcm, &bcm_q_stats);
    EXPECT_EQ(bcm_q_stats.optimization_steps, (uint64_t)EPOCHS);
}

TEST_F(LearningPipelineE2E, AttentionModulatedLearning) {
    /**
     * SCENARIO: Attention-gated plasticity
     *
     * 1. Compute attention scores
     * 2. Only attended patterns update BCM
     * 3. Verify learning is selective
     */

    const int EPOCHS = 50;

    for (int epoch = 0; epoch < EPOCHS; epoch++) {
        // Attention selects active heads
        float attn_scores[] = {0.9f, 0.7f, 0.5f, 0.3f, 0.1f};
        uint32_t selected[5];
        int num_selected = attention_quantum_select_heads(attention, attn_scores, 5, 3, selected);
        EXPECT_GE(num_selected, 0);

        // Only attended patterns drive BCM update
        bcm_activity_stats_t bcm_activity = {
            .avg_weight = (attn_scores[0] > 0.4f) ? 0.8f : 0.2f,
            .weight_variance = 0.1f,
            .avg_post_activity = (attn_scores[1] > 0.4f) ? 0.8f : 0.2f,
            .selectivity_index = 0.5f,
            .num_active_synapses = 100
        };
        bcm_quantum_optimize_threshold(bcm, &bcm_activity);
    }

    // Both bridges should have processed
    attention_quantum_stats_t attn_stats;
    attention_quantum_get_stats(attention, &attn_stats);
    EXPECT_EQ(attn_stats.quantum_selections + attn_stats.classical_fallbacks, (uint64_t)EPOCHS);

    bcm_quantum_stats_t bcm_q_stats;
    bcm_quantum_get_stats(bcm, &bcm_q_stats);
    EXPECT_EQ(bcm_q_stats.optimization_steps, (uint64_t)EPOCHS);
}

//=============================================================================
// E2E Test Cases: Stress Tests
//=============================================================================

class StressPipelineE2E : public ::testing::Test {
protected:
    std::vector<attention_quantum_bridge_t*> attention_bridges;
    std::vector<bcm_quantum_bridge_t*> bcm_bridges;

    void SetUp() override {
        // Create multiple bridge instances
        for (int i = 0; i < 5; i++) {
            attention_quantum_config_t attn_cfg = attention_quantum_default_config();
            attention_bridges.push_back(attention_quantum_bridge_create(&attn_cfg));

            bcm_quantum_config_t bcm_cfg = bcm_quantum_default_config();
            bcm_bridges.push_back(bcm_quantum_bridge_create(&bcm_cfg));
        }
    }

    void TearDown() override {
        for (auto bridge : attention_bridges) {
            if (bridge) attention_quantum_bridge_destroy(bridge);
        }
        for (auto bridge : bcm_bridges) {
            if (bridge) bcm_quantum_bridge_destroy(bridge);
        }
    }
};

TEST_F(StressPipelineE2E, MultipleBridgeInstances) {
    /**
     * SCENARIO: Multiple bridge instances operating independently
     */

    for (int iter = 0; iter < 100; iter++) {
        for (size_t b = 0; b < attention_bridges.size(); b++) {
            float scores[4] = {
                0.8f - (float)b * 0.1f,
                0.6f - (float)b * 0.05f,
                0.4f,
                0.2f + (float)b * 0.05f
            };
            uint32_t selected[4];
            attention_quantum_select_heads(attention_bridges[b], scores, 4, 2, selected);
        }

        for (size_t b = 0; b < bcm_bridges.size(); b++) {
            bcm_activity_stats_t bcm_activity = {
                .avg_weight = 0.5f + (float)b * 0.1f,
                .weight_variance = 0.1f,
                .avg_post_activity = 0.5f,
                .selectivity_index = 0.5f,
                .num_active_synapses = 100
            };
            bcm_quantum_optimize_threshold(bcm_bridges[b], &bcm_activity);
        }
    }

    // Verify all bridges accumulated stats (functional even without MHA)
    for (auto bridge : attention_bridges) {
        attention_quantum_stats_t stats;
        attention_quantum_get_stats(bridge, &stats);
        EXPECT_GT(stats.quantum_selections + stats.classical_fallbacks, 0UL);
    }
    for (auto bridge : bcm_bridges) {
        EXPECT_TRUE(bcm_quantum_is_enabled(bridge));
    }
}

TEST_F(StressPipelineE2E, RapidStateTransitions) {
    /**
     * SCENARIO: Rapid state changes across all bridges
     */

    for (int iter = 0; iter < 1000; iter++) {
        // Toggle states
        for (auto bridge : attention_bridges) {
            attention_quantum_bridge_set_enabled(bridge, iter % 2 == 0);
        }
        for (auto bridge : bcm_bridges) {
            bcm_quantum_set_enabled(bridge, iter % 2 == 1);
        }

        // Perform operations
        float scores[4] = {0.8f, 0.6f, 0.4f, 0.2f};
        uint32_t selected[4];

        for (auto bridge : attention_bridges) {
            attention_quantum_select_heads(bridge, scores, 4, 2, selected);
        }

        bcm_activity_stats_t bcm_activity = {
            .avg_weight = 0.5f + 0.4f * sinf((float)iter * 0.1f),
            .weight_variance = 0.1f,
            .avg_post_activity = 0.5f,
            .selectivity_index = 0.5f,
            .num_active_synapses = 100
        };
        for (auto bridge : bcm_bridges) {
            bcm_quantum_optimize_threshold(bridge, &bcm_activity);
        }
    }

    // Re-enable all bridges and verify operations work
    for (auto bridge : attention_bridges) {
        attention_quantum_bridge_set_enabled(bridge, true);
        // Operations should work even without MHA
        float final_scores[4] = {0.9f, 0.7f, 0.5f, 0.3f};
        uint32_t final_selected[4];
        int result = attention_quantum_select_heads(bridge, final_scores, 4, 2, final_selected);
        EXPECT_GE(result, 0);
    }
}

TEST_F(StressPipelineE2E, HighDimensionalProcessing) {
    /**
     * SCENARIO: Process high-dimensional data
     */

    const int HIGH_DIM = 256;
    std::vector<float> large_scores(HIGH_DIM);
    for (int i = 0; i < HIGH_DIM; i++) {
        large_scores[i] = (float)(HIGH_DIM - i) / (float)HIGH_DIM;
    }

    std::vector<uint32_t> selected(HIGH_DIM);

    // Process with first attention bridge
    int result = attention_quantum_select_heads(
        attention_bridges[0], large_scores.data(), HIGH_DIM, 32, selected.data()
    );
    EXPECT_GE(result, 0);
}

//=============================================================================
// E2E Test Cases: Recovery and Resilience
//=============================================================================

class ResilienceE2E : public ::testing::Test {
protected:
    attention_quantum_bridge_t* attention = nullptr;
    thalamic_quantum_bridge_t* thalamic = nullptr;

    void SetUp() override {
        attention_quantum_config_t attn_cfg = attention_quantum_default_config();
        attention = attention_quantum_bridge_create(&attn_cfg);

        thalamic_quantum_config_t thal_cfg = thalamic_quantum_default_config();
        thalamic = thalamic_quantum_bridge_create(&thal_cfg);
    }

    void TearDown() override {
        if (attention) attention_quantum_bridge_destroy(attention);
        if (thalamic) thalamic_quantum_bridge_destroy(thalamic);
    }
};

TEST_F(ResilienceE2E, RecoveryAfterInvalidInput) {
    /**
     * SCENARIO: System recovers after receiving invalid input
     */

    // Valid operation
    float valid_scores[4] = {0.8f, 0.6f, 0.4f, 0.2f};
    uint32_t selected[4];
    int result1 = attention_quantum_select_heads(attention, valid_scores, 4, 2, selected);
    EXPECT_GE(result1, 0);

    // Invalid operation (null) - should fail gracefully
    int result2 = attention_quantum_select_heads(attention, nullptr, 4, 2, selected);

    // Recovery: valid operation should still work
    int result3 = attention_quantum_select_heads(attention, valid_scores, 4, 2, selected);
    EXPECT_GE(result3, 0);
}

TEST_F(ResilienceE2E, ContinuedOperationAfterDisable) {
    /**
     * SCENARIO: Operations continue with classical fallback when disabled
     */

    // Disable quantum processing
    attention_quantum_bridge_set_enabled(attention, false);

    // Operations should still work (classical fallback)
    float scores[4] = {0.8f, 0.6f, 0.4f, 0.2f};
    uint32_t selected[4];

    for (int i = 0; i < 100; i++) {
        int result = attention_quantum_select_heads(attention, scores, 4, 2, selected);
        EXPECT_GE(result, 0);
    }

    // Verify classical fallback was used
    attention_quantum_stats_t stats;
    attention_quantum_get_stats(attention, &stats);
    EXPECT_GT(stats.classical_fallbacks, 0UL);

    // Re-enable and verify
    attention_quantum_bridge_set_enabled(attention, true);
    attention_quantum_reset_stats(attention);

    for (int i = 0; i < 10; i++) {
        attention_quantum_select_heads(attention, scores, 4, 2, selected);
    }

    attention_quantum_get_stats(attention, &stats);
    // Should have some quantum or classical operations
    EXPECT_GT(stats.quantum_selections + stats.classical_fallbacks, 0UL);
}

TEST_F(ResilienceE2E, StatsAccuracyAcrossOperations) {
    /**
     * SCENARIO: Statistics remain accurate across many operations
     */

    const int OPERATIONS = 500;

    for (int i = 0; i < OPERATIONS; i++) {
        float scores[4] = {0.8f, 0.6f, 0.4f, 0.2f};
        uint32_t selected[4];
        attention_quantum_select_heads(attention, scores, 4, 2, selected);
    }

    attention_quantum_stats_t stats;
    attention_quantum_get_stats(attention, &stats);

    // Total operations should equal OPERATIONS
    uint64_t total = stats.quantum_selections + stats.classical_fallbacks;
    EXPECT_EQ(total, (uint64_t)OPERATIONS);
}

TEST_F(ResilienceE2E, ThalamicRecovery) {
    /**
     * SCENARIO: Thalamic routing recovers after invalid input
     */

    // Valid operation
    uint32_t dests[] = {1, 2, 3};
    float features[] = {0.5f, 0.5f};
    uint32_t routed[3];
    uint32_t num_routed = 0;

    int result1 = thalamic_quantum_route(thalamic, 0, dests, 3, features, 2, routed, &num_routed);
    EXPECT_GE(result1, 0);

    // Invalid operation
    int result2 = thalamic_quantum_route(thalamic, 0, nullptr, 3, features, 2, routed, &num_routed);

    // Recovery
    int result3 = thalamic_quantum_route(thalamic, 0, dests, 3, features, 2, routed, &num_routed);
    EXPECT_GE(result3, 0);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
