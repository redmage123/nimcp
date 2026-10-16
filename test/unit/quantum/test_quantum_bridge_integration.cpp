/**
 * @file test_quantum_bridge_integration.cpp
 * @brief Integration tests for quantum bridges across cognitive modules
 *
 * Tests cover:
 * - Cross-bridge communication
 * - Quantum state sharing between modules
 * - Combined quantum operations
 * - Performance under integrated workloads
 * - Error propagation across bridges
 * - Statistics aggregation
 *
 * @version Phase C2: Quantum Reasoning Integration
 * @date 2025-12-30
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>

/* Implementation defines before including headers */
#define NIMCP_ATTENTION_QUANTUM_BRIDGE_IMPLEMENTATION
#define NIMCP_BCM_QUANTUM_BRIDGE_IMPLEMENTATION
#define NIMCP_THALAMIC_QUANTUM_BRIDGE_IMPLEMENTATION
#define NIMCP_STDP_QUANTUM_BRIDGE_IMPLEMENTATION
#define NIMCP_EXECUTIVE_QUANTUM_BRIDGE_IMPLEMENTATION

// Headers have their own extern "C" guards
#include "plasticity/attention/nimcp_attention_quantum_bridge.h"
#include "plasticity/bcm/nimcp_bcm_quantum_bridge.h"
#include "middleware/routing/nimcp_thalamic_quantum_bridge.h"
#include "plasticity/stdp/nimcp_stdp_quantum_bridge.h"
#include "cognitive/executive/nimcp_executive_quantum_bridge.h"
#include "cognitive/reasoning/nimcp_quantum_reasoning.h"

//=============================================================================
// Test Fixture
//=============================================================================

class QuantumBridgeIntegrationTest : public ::testing::Test {
protected:
    attention_quantum_bridge_t* attention_bridge = nullptr;
    bcm_quantum_bridge_t* bcm_bridge = nullptr;
    thalamic_quantum_bridge_t* thalamic_bridge = nullptr;
    stdp_quantum_bridge_t* stdp_bridge = nullptr;
    executive_quantum_bridge_t* executive_bridge = nullptr;
    qreason_t reasoner = nullptr;

    void SetUp() override {
        attention_quantum_config_t attn_cfg = attention_quantum_default_config();
        attention_bridge = attention_quantum_bridge_create(&attn_cfg);

        bcm_quantum_config_t bcm_cfg = bcm_quantum_default_config();
        bcm_bridge = bcm_quantum_bridge_create(&bcm_cfg);

        thalamic_quantum_config_t thal_cfg = thalamic_quantum_default_config();
        thalamic_bridge = thalamic_quantum_bridge_create(&thal_cfg);

        stdp_quantum_config_t stdp_cfg = stdp_quantum_default_config();
        stdp_bridge = stdp_quantum_bridge_create(&stdp_cfg);

        executive_quantum_config_t exec_cfg = executive_quantum_default_config();
        executive_bridge = executive_quantum_bridge_create(&exec_cfg);

        qreason_config_t qr_cfg = qreason_default_config();
        reasoner = qreason_create(&qr_cfg);
    }

    void TearDown() override {
        if (attention_bridge) attention_quantum_bridge_destroy(attention_bridge);
        if (bcm_bridge) bcm_quantum_bridge_destroy(bcm_bridge);
        if (thalamic_bridge) thalamic_quantum_bridge_destroy(thalamic_bridge);
        if (stdp_bridge) stdp_quantum_bridge_destroy(stdp_bridge);
        if (executive_bridge) executive_quantum_bridge_destroy(executive_bridge);
        if (reasoner) qreason_destroy(reasoner);
    }
};

//=============================================================================
// Bridge Creation Tests
//=============================================================================

TEST_F(QuantumBridgeIntegrationTest, AllBridgesCreated) {
    ASSERT_NE(attention_bridge, nullptr);
    ASSERT_NE(bcm_bridge, nullptr);
    ASSERT_NE(thalamic_bridge, nullptr);
    ASSERT_NE(stdp_bridge, nullptr);
    ASSERT_NE(executive_bridge, nullptr);
    ASSERT_NE(reasoner, nullptr);
}

TEST_F(QuantumBridgeIntegrationTest, BridgesIndependent) {
    /* Enable/disable operations on one bridge shouldn't affect others */
    attention_quantum_bridge_set_enabled(attention_bridge, false);

    /* Other bridges should still be enabled */
    EXPECT_TRUE(bcm_quantum_is_enabled(bcm_bridge));
    EXPECT_TRUE(thalamic_quantum_bridge_is_enabled(thalamic_bridge));
    EXPECT_TRUE(stdp_quantum_bridge_is_enabled(stdp_bridge));
    EXPECT_TRUE(executive_quantum_bridge_is_enabled(executive_bridge));

    attention_quantum_bridge_set_enabled(attention_bridge, true);
}

//=============================================================================
// Cross-Bridge Workflow Tests
//=============================================================================

TEST_F(QuantumBridgeIntegrationTest, AttentionToBCMWorkflow) {
    /**
     * SCENARIO: Attention selection modulates BCM plasticity
     *
     * 1. Attention selects active heads based on scores
     * 2. Selected heads drive activity for BCM threshold update
     * 3. BCM threshold affects plasticity
     */

    /* Step 1: Attention selection */
    float attention_scores[] = {0.9f, 0.7f, 0.5f, 0.3f, 0.1f};
    uint32_t selected_heads[5];

    int n_selected = attention_quantum_select_heads(
        attention_bridge, attention_scores, 5, 3, selected_heads
    );
    EXPECT_GE(n_selected, 0);

    /* Step 2: Use selection to drive BCM activity */
    bcm_activity_stats_t bcm_activity = {
        .avg_weight = 0.0f,
        .weight_variance = 0.1f,
        .avg_post_activity = 0.0f,
        .selectivity_index = 0.5f,
        .num_active_synapses = 0
    };

    /* Accumulate activity from selected heads */
    for (int i = 0; i < n_selected && i < 5; i++) {
        uint32_t head = selected_heads[i];
        if (head < 5) {
            bcm_activity.avg_weight += attention_scores[head] / (float)n_selected;
            bcm_activity.avg_post_activity += attention_scores[head] / (float)n_selected;
            bcm_activity.num_active_synapses += 10;
        }
    }

    /* Step 3: BCM threshold optimization */
    float threshold = bcm_quantum_optimize_threshold(bcm_bridge, &bcm_activity);
    EXPECT_FALSE(std::isnan(threshold));
    EXPECT_GE(threshold, 0.0f);

    /* Verify operations were tracked */
    attention_quantum_stats_t attn_stats;
    attention_quantum_get_stats(attention_bridge, &attn_stats);
    EXPECT_GT(attn_stats.quantum_selections + attn_stats.classical_fallbacks, 0UL);

    bcm_quantum_stats_t bcm_stats;
    bcm_quantum_get_stats(bcm_bridge, &bcm_stats);
    EXPECT_GT(bcm_stats.optimization_steps, 0UL);
}

TEST_F(QuantumBridgeIntegrationTest, ThalamicToAttentionWorkflow) {
    /**
     * SCENARIO: Thalamic routing feeds attention mechanism
     *
     * 1. Thalamic router decides which modules receive signal
     * 2. Routed signals generate attention scores
     * 3. Attention selects from available heads
     */

    /* Step 1: Generate signal features */
    float signal_features[] = {0.8f, 0.6f, 0.4f, 0.7f, 0.5f, 0.3f, 0.9f, 0.2f};

    /* Step 2: Route to modules */
    uint32_t modules[] = {1, 2, 3, 4, 5};
    uint32_t routed_modules[5];
    uint32_t num_routed = 0;

    int thal_result = thalamic_quantum_route(
        thalamic_bridge, 0, modules, 5, signal_features, 8,
        routed_modules, &num_routed
    );
    EXPECT_GE(thal_result, 0);

    /* Step 3: Generate attention scores from routing */
    float attention_scores[5];
    for (uint32_t i = 0; i < 5; i++) {
        attention_scores[i] = 0.1f;  /* Base score */
    }

    /* Boost scores for routed modules */
    for (uint32_t i = 0; i < num_routed && i < 5; i++) {
        uint32_t mod = routed_modules[i];
        for (uint32_t j = 0; j < 5; j++) {
            if (modules[j] == mod) {
                attention_scores[j] = 0.9f;
                break;
            }
        }
    }

    /* Step 4: Attention selection */
    uint32_t selected[5];
    int n_selected = attention_quantum_select_heads(
        attention_bridge, attention_scores, 5, 2, selected
    );
    EXPECT_GE(n_selected, 0);
}

TEST_F(QuantumBridgeIntegrationTest, ExecutiveDecisionWithReasoning) {
    /**
     * SCENARIO: Executive function uses quantum reasoning
     *
     * 1. Set up knowledge base with decision constraints
     * 2. Use forward chaining for logical deduction
     * 3. Executive evaluates options based on inferred facts
     */

    /* Step 1: Knowledge base setup */
    /* Fact: Resource available (variable 0) */
    qreason_set_fact(reasoner, 0, QREASON_TRUE, 0.9f);

    /* Fact: Time constraint satisfied (variable 1) */
    qreason_set_fact(reasoner, 1, QREASON_TRUE, 0.8f);

    /* Rule: If resource AND time -> proceed (variable 2) */
    uint32_t antecedents[] = {0, 1};
    qreason_add_rule(reasoner, antecedents, 2, 2, 0.95f);

    /* Step 2: Forward chaining */
    qreason_result_t reason_result;
    uint32_t inferences = qreason_forward_chain(reasoner, &reason_result);
    EXPECT_GE(inferences, 1u);

    /* Step 3: Use inference for decision */
    float proceed_confidence = 0.0f;
    qreason_truth_t proceed = qreason_get_fact(reasoner, 2, &proceed_confidence);

    decision_option_t options[2] = {
        {.option_id = 0, .expected_reward = 0.3f, .risk_level = 0.1f},
        {.option_id = 1, .expected_reward = 0.0f, .risk_level = 0.0f}
    };
    strncpy(options[0].description, "Proceed", sizeof(options[0].description));
    strncpy(options[1].description, "Wait", sizeof(options[1].description));

    /* Adjust reward based on reasoning */
    if (proceed == QREASON_TRUE) {
        options[0].expected_reward += proceed_confidence;
    } else {
        options[1].expected_reward += 0.5f;
    }

    quantum_decision_result_t decision;
    int ret = executive_quantum_evaluate_options(executive_bridge, options, 2, &decision);
    EXPECT_EQ(ret, 0);

    /* If reasoning says proceed, that option should be selected */
    if (proceed == QREASON_TRUE && proceed_confidence > 0.5f) {
        EXPECT_EQ(decision.selected_option_id, 0u);
    }
}

TEST_F(QuantumBridgeIntegrationTest, STDPWithAttentionModulation) {
    /**
     * SCENARIO: STDP learning rate modulated by attention
     *
     * 1. Attention scores determine which synapses are active
     * 2. STDP updates learning rate based on activity
     */

    /* Step 1: Attention selection */
    float scores[] = {0.8f, 0.6f, 0.4f, 0.2f};
    uint32_t selected[4];
    int n_selected = attention_quantum_select_heads(
        attention_bridge, scores, 4, 2, selected
    );
    EXPECT_GE(n_selected, 0);

    /* Step 2: Compute activity stats based on selection */
    qstdp_activity_stats_t stdp_activity = {0};
    stdp_activity.mean_weight = 0.5f;
    stdp_activity.weight_variance = 0.1f;

    /* Higher firing for selected heads */
    float total_activity = 0.0f;
    for (int i = 0; i < n_selected && i < 4; i++) {
        if (selected[i] < 4) {
            total_activity += scores[selected[i]];
        }
    }
    stdp_activity.firing_rate = total_activity * 20.0f;
    stdp_activity.ltp_rate = stdp_activity.firing_rate * 0.3f;
    stdp_activity.ltd_rate = stdp_activity.firing_rate * 0.1f;
    stdp_activity.ltp_ltd_ratio = 3.0f;

    /* Step 3: STDP learning rate update */
    float lr = stdp_quantum_step(stdp_bridge, &stdp_activity);
    EXPECT_GT(lr, 0.0f);
    EXPECT_LE(lr, 1.0f);
}

//=============================================================================
// Full Pipeline Tests
//=============================================================================

TEST_F(QuantumBridgeIntegrationTest, FullCognitiveLoop) {
    /**
     * SCENARIO: Complete cognitive processing loop
     *
     * 1. Thalamic routes sensory input
     * 2. Attention selects features
     * 3. Executive makes decision
     * 4. BCM updates plasticity threshold
     * 5. STDP adjusts learning rate
     */

    const int NUM_ITERATIONS = 10;

    for (int iter = 0; iter < NUM_ITERATIONS; iter++) {
        float intensity = 0.5f + 0.4f * sinf((float)iter * 0.3f);

        /* Generate sensory features */
        float features[8];
        for (int i = 0; i < 8; i++) {
            features[i] = intensity * (0.5f + 0.5f * sinf((float)i * 0.2f + (float)iter));
        }

        /* 1. Thalamic routing */
        uint32_t modules[] = {1, 2, 3, 4};
        uint32_t routed[4];
        uint32_t num_routed = 0;
        thalamic_quantum_route(thalamic_bridge, 0, modules, 4, features, 8, routed, &num_routed);

        /* 2. Attention selection */
        float attn_scores[4] = {features[0], features[2], features[4], features[6]};
        uint32_t selected[4];
        attention_quantum_select_heads(attention_bridge, attn_scores, 4, 2, selected);

        /* 3. Executive decision */
        decision_option_t options[2] = {
            {.option_id = 0, .expected_reward = intensity, .risk_level = 1.0f - intensity},
            {.option_id = 1, .expected_reward = 0.5f, .risk_level = 0.3f}
        };
        strncpy(options[0].description, "Act", sizeof(options[0].description));
        strncpy(options[1].description, "Wait", sizeof(options[1].description));

        quantum_decision_result_t decision;
        executive_quantum_evaluate_options(executive_bridge, options, 2, &decision);

        /* 4. BCM threshold */
        bcm_activity_stats_t bcm_activity = {
            .avg_weight = intensity,
            .weight_variance = 0.1f,
            .avg_post_activity = intensity * 0.8f,
            .selectivity_index = 0.6f,
            .num_active_synapses = 100
        };
        bcm_quantum_optimize_threshold(bcm_bridge, &bcm_activity);

        /* 5. STDP learning */
        qstdp_activity_stats_t stdp_activity = {
            .mean_weight = intensity,
            .weight_variance = 0.1f,
            .ltp_rate = 5.0f,
            .ltd_rate = 2.0f,
            .ltp_ltd_ratio = 2.5f,
            .firing_rate = intensity * 30.0f,
            .sparsity = 0.0f
        };
        stdp_quantum_step(stdp_bridge, &stdp_activity);
    }

    /* Verify all bridges processed operations */
    thalamic_quantum_stats_t thal_stats;
    thalamic_quantum_get_stats(thalamic_bridge, &thal_stats);
    EXPECT_EQ(thal_stats.quantum_routes + thal_stats.classical_fallbacks, (uint64_t)NUM_ITERATIONS);

    attention_quantum_stats_t attn_stats;
    attention_quantum_get_stats(attention_bridge, &attn_stats);
    EXPECT_EQ(attn_stats.quantum_selections + attn_stats.classical_fallbacks, (uint64_t)NUM_ITERATIONS);

    bcm_quantum_stats_t bcm_stats;
    bcm_quantum_get_stats(bcm_bridge, &bcm_stats);
    EXPECT_EQ(bcm_stats.optimization_steps, (uint64_t)NUM_ITERATIONS);

    stdp_quantum_stats_t stdp_stats;
    stdp_quantum_get_stats(stdp_bridge, &stdp_stats);
    EXPECT_EQ(stdp_stats.optimization_steps, (uint64_t)NUM_ITERATIONS);
}

//=============================================================================
// Error Handling Tests
//=============================================================================

TEST_F(QuantumBridgeIntegrationTest, NullInputRecovery) {
    /* Bridges should handle null inputs gracefully */

    /* Attention with null scores */
    uint32_t selected[4];
    int result = attention_quantum_select_heads(attention_bridge, nullptr, 4, 2, selected);
    /* Should return error or handle gracefully */

    /* Thalamic with null destinations */
    float features[4] = {0.5f, 0.5f, 0.5f, 0.5f};
    uint32_t routed[4];
    uint32_t num_routed = 0;
    result = thalamic_quantum_route(thalamic_bridge, 0, nullptr, 4, features, 4, routed, &num_routed);
    /* Should return error */

    /* Subsequent operations should still work */
    float valid_scores[] = {0.8f, 0.6f};
    result = attention_quantum_select_heads(attention_bridge, valid_scores, 2, 1, selected);
    EXPECT_GE(result, 0);
}

TEST_F(QuantumBridgeIntegrationTest, DisabledBridgeFallback) {
    /* Disabled bridges should use classical fallback */

    attention_quantum_bridge_set_enabled(attention_bridge, false);

    float scores[] = {0.9f, 0.7f, 0.5f, 0.3f};
    uint32_t selected[4];
    int result = attention_quantum_select_heads(attention_bridge, scores, 4, 2, selected);

    /* Should still work via classical fallback */
    EXPECT_GE(result, 0);

    attention_quantum_stats_t stats;
    attention_quantum_get_stats(attention_bridge, &stats);
    EXPECT_GT(stats.classical_fallbacks, 0UL);

    attention_quantum_bridge_set_enabled(attention_bridge, true);
}

//=============================================================================
// Performance Tests
//=============================================================================

TEST_F(QuantumBridgeIntegrationTest, HighThroughput) {
    const int NUM_OPS = 100;

    for (int i = 0; i < NUM_OPS; i++) {
        /* Rapid-fire operations across all bridges */

        float scores[4] = {0.8f, 0.6f, 0.4f, 0.2f};
        uint32_t selected[4];
        attention_quantum_select_heads(attention_bridge, scores, 4, 2, selected);

        bcm_activity_stats_t bcm_activity = {
            .avg_weight = 0.5f,
            .weight_variance = 0.1f,
            .avg_post_activity = 0.5f,
            .selectivity_index = 0.5f,
            .num_active_synapses = 100
        };
        bcm_quantum_optimize_threshold(bcm_bridge, &bcm_activity);

        float features[4] = {0.5f, 0.5f, 0.5f, 0.5f};
        uint32_t modules[] = {1, 2};
        uint32_t routed[2];
        uint32_t num_routed = 0;
        thalamic_quantum_route(thalamic_bridge, 0, modules, 2, features, 4, routed, &num_routed);

        qstdp_activity_stats_t stdp_activity = {
            .mean_weight = 0.5f,
            .weight_variance = 0.1f,
            .ltp_rate = 5.0f,
            .ltd_rate = 2.0f,
            .ltp_ltd_ratio = 2.5f,
            .firing_rate = 15.0f,
            .sparsity = 0.0f
        };
        stdp_quantum_step(stdp_bridge, &stdp_activity);
    }

    /* All bridges should have processed NUM_OPS operations */
    attention_quantum_stats_t attn_stats;
    attention_quantum_get_stats(attention_bridge, &attn_stats);
    EXPECT_EQ(attn_stats.quantum_selections + attn_stats.classical_fallbacks, (uint64_t)NUM_OPS);
}

TEST_F(QuantumBridgeIntegrationTest, ConcurrentBridgeOperations) {
    /* Simulate interleaved operations */

    for (int round = 0; round < 20; round++) {
        /* Attention */
        float attn_scores[4] = {0.9f - (float)round * 0.02f, 0.7f, 0.5f, 0.3f};
        uint32_t selected[4];
        attention_quantum_select_heads(attention_bridge, attn_scores, 4, 2, selected);

        /* STDP interleaved */
        qstdp_activity_stats_t stdp_activity = {
            .mean_weight = 0.5f + (float)round * 0.01f,
            .weight_variance = 0.1f,
            .ltp_rate = 5.0f,
            .ltd_rate = 2.0f,
            .ltp_ltd_ratio = 2.5f,
            .firing_rate = 15.0f,
            .sparsity = 0.0f
        };
        stdp_quantum_step(stdp_bridge, &stdp_activity);

        /* BCM */
        bcm_activity_stats_t bcm_activity = {
            .avg_weight = 0.5f,
            .weight_variance = 0.1f + (float)round * 0.005f,
            .avg_post_activity = 0.5f,
            .selectivity_index = 0.5f,
            .num_active_synapses = 100
        };
        bcm_quantum_optimize_threshold(bcm_bridge, &bcm_activity);
    }

    /* Verify stats are accurate */
    attention_quantum_stats_t attn_stats;
    attention_quantum_get_stats(attention_bridge, &attn_stats);
    EXPECT_EQ(attn_stats.quantum_selections + attn_stats.classical_fallbacks, 20UL);

    stdp_quantum_stats_t stdp_stats;
    stdp_quantum_get_stats(stdp_bridge, &stdp_stats);
    EXPECT_EQ(stdp_stats.optimization_steps, 20UL);

    bcm_quantum_stats_t bcm_stats;
    bcm_quantum_get_stats(bcm_bridge, &bcm_stats);
    EXPECT_EQ(bcm_stats.optimization_steps, 20UL);
}

//=============================================================================
// Stats Reset Tests
//=============================================================================

TEST_F(QuantumBridgeIntegrationTest, StatsResetIndependent) {
    /* Operations on all bridges */
    float scores[4] = {0.8f, 0.6f, 0.4f, 0.2f};
    uint32_t selected[4];
    attention_quantum_select_heads(attention_bridge, scores, 4, 2, selected);

    bcm_activity_stats_t bcm_activity = {
        .avg_weight = 0.5f,
        .weight_variance = 0.1f,
        .avg_post_activity = 0.5f,
        .selectivity_index = 0.5f,
        .num_active_synapses = 100
    };
    bcm_quantum_optimize_threshold(bcm_bridge, &bcm_activity);

    /* Reset only attention stats */
    attention_quantum_reset_stats(attention_bridge);

    /* Attention stats should be zero */
    attention_quantum_stats_t attn_stats;
    attention_quantum_get_stats(attention_bridge, &attn_stats);
    EXPECT_EQ(attn_stats.quantum_selections, 0UL);
    EXPECT_EQ(attn_stats.classical_fallbacks, 0UL);

    /* BCM stats should be unchanged */
    bcm_quantum_stats_t bcm_stats;
    bcm_quantum_get_stats(bcm_bridge, &bcm_stats);
    EXPECT_EQ(bcm_stats.optimization_steps, 1UL);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
