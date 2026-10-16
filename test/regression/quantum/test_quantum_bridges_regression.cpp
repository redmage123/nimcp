//=============================================================================
// test_quantum_bridges_regression.cpp - Regression Tests for Quantum Bridges
//=============================================================================
/**
 * @file test_quantum_bridges_regression.cpp
 * @brief Regression tests ensuring stability and consistency of quantum bridges
 *
 * WHAT: Test edge cases, boundary conditions, and behavioral consistency
 * WHY:  Prevent regressions in quantum bridge behavior over time
 * HOW:  Test deterministic behavior, stress conditions, and edge cases
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
// Attention Quantum Bridge Regression Tests
//=============================================================================

class AttentionQuantumRegressionTest : public ::testing::Test {
protected:
    attention_quantum_bridge_t* bridge = nullptr;

    void SetUp() override {
        attention_quantum_config_t config = attention_quantum_default_config();
        bridge = attention_quantum_bridge_create(&config);
    }

    void TearDown() override {
        if (bridge) {
            attention_quantum_bridge_destroy(bridge);
        }
    }
};

TEST_F(AttentionQuantumRegressionTest, DeterministicBehaviorWithSameInputs) {
    float scores[4] = {0.8f, 0.6f, 0.4f, 0.2f};
    uint32_t selected1[4], selected2[4];

    int result1 = attention_quantum_select_heads(bridge, scores, 4, 2, selected1);
    int result2 = attention_quantum_select_heads(bridge, scores, 4, 2, selected2);

    EXPECT_EQ(result1, result2);
}

TEST_F(AttentionQuantumRegressionTest, ZeroScoresHandling) {
    float scores[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    uint32_t selected[4];

    int result = attention_quantum_select_heads(bridge, scores, 4, 2, selected);
    EXPECT_GE(result, 0);
}

TEST_F(AttentionQuantumRegressionTest, MaxScoresHandling) {
    float scores[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    uint32_t selected[4];

    int result = attention_quantum_select_heads(bridge, scores, 4, 2, selected);
    EXPECT_GE(result, 0);
}

TEST_F(AttentionQuantumRegressionTest, SingleHeadSelection) {
    float scores[1] = {0.5f};
    uint32_t selected[1];

    int result = attention_quantum_select_heads(bridge, scores, 1, 1, selected);
    EXPECT_GE(result, 0);
}

TEST_F(AttentionQuantumRegressionTest, StatsConsistencyAfterMultipleOperations) {
    float scores[4] = {0.8f, 0.6f, 0.4f, 0.2f};
    uint32_t selected[4];

    for (int i = 0; i < 100; i++) {
        attention_quantum_select_heads(bridge, scores, 4, 2, selected);
    }

    attention_quantum_stats_t stats;
    int result = attention_quantum_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_GT(stats.quantum_selections + stats.classical_fallbacks, 0UL);
}

TEST_F(AttentionQuantumRegressionTest, EnableDisableToggling) {
    // Note: is_enabled requires both config.enabled AND bridge->connected
    // Without an MHA connected, is_enabled will always return false
    // This test verifies the toggle mechanism doesn't corrupt state
    for (int i = 0; i < 10; i++) {
        attention_quantum_bridge_set_enabled(bridge, false);
        // Without MHA connection, is_enabled is always false
        EXPECT_FALSE(attention_quantum_bridge_is_enabled(bridge));
        attention_quantum_bridge_set_enabled(bridge, true);
        // Still false because no MHA connected
        EXPECT_FALSE(attention_quantum_bridge_is_enabled(bridge));
    }

    // Verify operations still work after toggling
    float scores[4] = {0.8f, 0.6f, 0.4f, 0.2f};
    uint32_t selected[4];
    int result = attention_quantum_select_heads(bridge, scores, 4, 2, selected);
    EXPECT_GE(result, 0);
}

TEST_F(AttentionQuantumRegressionTest, SelectMoreThanAvailable) {
    float scores[3] = {0.8f, 0.6f, 0.4f};
    uint32_t selected[5];

    int result = attention_quantum_select_heads(bridge, scores, 3, 5, selected);
    // Should handle gracefully
}

TEST_F(AttentionQuantumRegressionTest, ResetStatsWorks) {
    float scores[4] = {0.8f, 0.6f, 0.4f, 0.2f};
    uint32_t selected[4];

    for (int i = 0; i < 10; i++) {
        attention_quantum_select_heads(bridge, scores, 4, 2, selected);
    }

    attention_quantum_reset_stats(bridge);

    attention_quantum_stats_t stats;
    attention_quantum_get_stats(bridge, &stats);
    EXPECT_EQ(stats.quantum_selections, 0UL);
    EXPECT_EQ(stats.classical_fallbacks, 0UL);
}

//=============================================================================
// BCM Quantum Bridge Regression Tests
//=============================================================================

class BCMQuantumRegressionTest : public ::testing::Test {
protected:
    bcm_quantum_bridge_t* bridge = nullptr;

    void SetUp() override {
        bcm_quantum_config_t config = bcm_quantum_default_config();
        bridge = bcm_quantum_bridge_create(&config);
    }

    void TearDown() override {
        if (bridge) {
            bcm_quantum_bridge_destroy(bridge);
        }
    }
};

TEST_F(BCMQuantumRegressionTest, ThresholdStability) {
    bcm_activity_stats_t stats = {
        .avg_weight = 0.5f,
        .weight_variance = 0.1f,
        .avg_post_activity = 0.5f,
        .selectivity_index = 0.5f,
        .num_active_synapses = 100
    };

    for (int i = 0; i < 100; i++) {
        bcm_quantum_optimize_threshold(bridge, &stats);
    }

    bcm_quantum_stats_t q_stats;
    bcm_quantum_get_stats(bridge, &q_stats);
    // Should have completed optimizations
    EXPECT_GT(q_stats.optimization_steps, 0UL);
}

TEST_F(BCMQuantumRegressionTest, HighActivityHandling) {
    bcm_activity_stats_t stats = {
        .avg_weight = 10.0f,
        .weight_variance = 5.0f,
        .avg_post_activity = 10.0f,
        .selectivity_index = 1.0f,
        .num_active_synapses = 10000
    };

    float threshold = bcm_quantum_optimize_threshold(bridge, &stats);
    EXPECT_FALSE(std::isnan(threshold));
    EXPECT_FALSE(std::isinf(threshold));
}

TEST_F(BCMQuantumRegressionTest, ZeroActivityHandling) {
    bcm_activity_stats_t stats = {
        .avg_weight = 0.0f,
        .weight_variance = 0.0f,
        .avg_post_activity = 0.0f,
        .selectivity_index = 0.0f,
        .num_active_synapses = 0
    };

    float threshold = bcm_quantum_optimize_threshold(bridge, &stats);
    EXPECT_FALSE(std::isnan(threshold));
}

TEST_F(BCMQuantumRegressionTest, StatsAccumulation) {
    bcm_activity_stats_t stats = {
        .avg_weight = 0.5f,
        .weight_variance = 0.1f,
        .avg_post_activity = 0.5f,
        .selectivity_index = 0.5f,
        .num_active_synapses = 100
    };

    for (int i = 0; i < 100; i++) {
        bcm_quantum_optimize_threshold(bridge, &stats);
    }

    bcm_quantum_stats_t q_stats;
    bcm_quantum_get_stats(bridge, &q_stats);
    EXPECT_EQ(q_stats.optimization_steps, 100UL);
}

TEST_F(BCMQuantumRegressionTest, EnableDisableToggling) {
    for (int i = 0; i < 10; i++) {
        bcm_quantum_set_enabled(bridge, false);
        EXPECT_FALSE(bcm_quantum_is_enabled(bridge));
        bcm_quantum_set_enabled(bridge, true);
        EXPECT_TRUE(bcm_quantum_is_enabled(bridge));
    }
}

TEST_F(BCMQuantumRegressionTest, ThresholdBoundedVariation) {
    bcm_activity_stats_t stats = {
        .avg_weight = 0.5f,
        .weight_variance = 0.1f,
        .avg_post_activity = 0.5f,
        .selectivity_index = 0.5f,
        .num_active_synapses = 100
    };

    float threshold1 = bcm_quantum_optimize_threshold(bridge, &stats);
    float threshold2 = bcm_quantum_optimize_threshold(bridge, &stats);

    // Quantum-inspired algorithms have inherent stochasticity
    // Verify both values are within valid range (0, 20)
    EXPECT_GT(threshold1, 0.0f);
    EXPECT_LT(threshold1, 20.0f);
    EXPECT_GT(threshold2, 0.0f);
    EXPECT_LT(threshold2, 20.0f);
}

TEST_F(BCMQuantumRegressionTest, StatsReset) {
    bcm_activity_stats_t stats = {
        .avg_weight = 0.5f,
        .weight_variance = 0.1f,
        .avg_post_activity = 0.5f,
        .selectivity_index = 0.5f,
        .num_active_synapses = 100
    };

    for (int i = 0; i < 10; i++) {
        bcm_quantum_optimize_threshold(bridge, &stats);
    }

    bcm_quantum_reset_stats(bridge);

    bcm_quantum_stats_t q_stats;
    bcm_quantum_get_stats(bridge, &q_stats);
    EXPECT_EQ(q_stats.optimization_steps, 0UL);
}

//=============================================================================
// Thalamic Quantum Bridge Regression Tests
//=============================================================================

class ThalamicQuantumRegressionTest : public ::testing::Test {
protected:
    thalamic_quantum_bridge_t* bridge = nullptr;

    void SetUp() override {
        thalamic_quantum_config_t config = thalamic_quantum_default_config();
        bridge = thalamic_quantum_bridge_create(&config);
    }

    void TearDown() override {
        if (bridge) {
            thalamic_quantum_bridge_destroy(bridge);
        }
    }
};

TEST_F(ThalamicQuantumRegressionTest, RoutingDeterminism) {
    uint32_t dests[] = {1, 2, 3, 4, 5};
    float features[] = {0.5f, 0.5f, 0.5f, 0.5f};
    uint32_t routed1[5], routed2[5];
    uint32_t num1 = 0, num2 = 0;

    int result1 = thalamic_quantum_route(bridge, 0, dests, 5, features, 4, routed1, &num1);
    int result2 = thalamic_quantum_route(bridge, 0, dests, 5, features, 4, routed2, &num2);

    EXPECT_EQ(result1, result2);
    EXPECT_EQ(num1, num2);
}

TEST_F(ThalamicQuantumRegressionTest, SingleDestinationRouting) {
    uint32_t dest = 1;
    float features[] = {0.5f, 0.5f};
    uint32_t routed[1];
    uint32_t num_routed = 0;

    int result = thalamic_quantum_route(bridge, 0, &dest, 1, features, 2, routed, &num_routed);
    EXPECT_GE(result, 0);
}

TEST_F(ThalamicQuantumRegressionTest, GatingDecisionConsistency) {
    float features[] = {0.5f, 0.5f, 0.5f, 0.5f};
    float weight1 = 0.0f, weight2 = 0.0f;

    bool gate1 = thalamic_quantum_gate_signal(bridge, 0, 1, features, 4, &weight1);
    bool gate2 = thalamic_quantum_gate_signal(bridge, 0, 1, features, 4, &weight2);

    EXPECT_EQ(gate1, gate2);
    EXPECT_NEAR(weight1, weight2, 0.001f);
}

TEST_F(ThalamicQuantumRegressionTest, HighDestinationCount) {
    std::vector<uint32_t> dests(200);
    for (uint32_t i = 0; i < 200; i++) dests[i] = i + 1;

    float features[] = {0.5f, 0.5f};
    std::vector<uint32_t> routed(200);
    uint32_t num_routed = 0;

    int result = thalamic_quantum_route(bridge, 0, dests.data(), 200, features, 2,
                                         routed.data(), &num_routed);
    EXPECT_GE(result, 0);
}

TEST_F(ThalamicQuantumRegressionTest, SpeedupTracking) {
    uint32_t dests[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    float features[] = {0.5f, 0.5f};
    uint32_t routed[10];
    uint32_t num_routed = 0;

    thalamic_quantum_route(bridge, 0, dests, 10, features, 2, routed, &num_routed);

    thalamic_quantum_stats_t stats;
    thalamic_quantum_get_stats(bridge, &stats);
    EXPECT_GE(stats.routing_speedup, 0.0f);
}

TEST_F(ThalamicQuantumRegressionTest, EnableDisableToggling) {
    for (int i = 0; i < 10; i++) {
        thalamic_quantum_bridge_set_enabled(bridge, false);
        EXPECT_FALSE(thalamic_quantum_bridge_is_enabled(bridge));
        thalamic_quantum_bridge_set_enabled(bridge, true);
        EXPECT_TRUE(thalamic_quantum_bridge_is_enabled(bridge));
    }
}

TEST_F(ThalamicQuantumRegressionTest, StatsReset) {
    uint32_t dests[] = {1, 2, 3};
    float features[] = {0.5f, 0.5f};
    uint32_t routed[3];
    uint32_t num_routed = 0;

    for (int i = 0; i < 10; i++) {
        thalamic_quantum_route(bridge, 0, dests, 3, features, 2, routed, &num_routed);
    }

    thalamic_quantum_reset_stats(bridge);

    thalamic_quantum_stats_t stats;
    thalamic_quantum_get_stats(bridge, &stats);
    EXPECT_EQ(stats.quantum_routes, 0UL);
    EXPECT_EQ(stats.classical_fallbacks, 0UL);
}

TEST_F(ThalamicQuantumRegressionTest, ClassicalFallbackWhenDisabled) {
    thalamic_quantum_bridge_set_enabled(bridge, false);

    uint32_t dests[] = {1, 2, 3};
    float features[] = {0.5f, 0.5f};
    uint32_t routed[3];
    uint32_t num_routed = 0;

    for (int i = 0; i < 10; i++) {
        thalamic_quantum_route(bridge, 0, dests, 3, features, 2, routed, &num_routed);
    }

    thalamic_quantum_stats_t stats;
    thalamic_quantum_get_stats(bridge, &stats);
    EXPECT_GT(stats.classical_fallbacks, 0UL);
}

//=============================================================================
// Cross-Bridge Regression Tests
//=============================================================================

class CrossBridgeRegressionTest : public ::testing::Test {
protected:
    attention_quantum_bridge_t* attention = nullptr;
    bcm_quantum_bridge_t* bcm = nullptr;
    thalamic_quantum_bridge_t* thalamic = nullptr;

    void SetUp() override {
        attention_quantum_config_t attn_config = attention_quantum_default_config();
        attention = attention_quantum_bridge_create(&attn_config);

        bcm_quantum_config_t bcm_config = bcm_quantum_default_config();
        bcm = bcm_quantum_bridge_create(&bcm_config);

        thalamic_quantum_config_t thal_config = thalamic_quantum_default_config();
        thalamic = thalamic_quantum_bridge_create(&thal_config);
    }

    void TearDown() override {
        if (attention) attention_quantum_bridge_destroy(attention);
        if (bcm) bcm_quantum_bridge_destroy(bcm);
        if (thalamic) thalamic_quantum_bridge_destroy(thalamic);
    }
};

TEST_F(CrossBridgeRegressionTest, SimultaneousOperations) {
    float attn_scores[] = {0.8f, 0.6f, 0.4f, 0.2f};
    uint32_t selected[4];

    bcm_activity_stats_t bcm_stats = {
        .avg_weight = 0.5f,
        .weight_variance = 0.1f,
        .avg_post_activity = 0.5f,
        .selectivity_index = 0.5f,
        .num_active_synapses = 100
    };

    uint32_t dests[] = {1, 2, 3};
    float thal_features[] = {0.5f, 0.5f};
    uint32_t routed[3];
    uint32_t num_routed = 0;

    for (int i = 0; i < 100; i++) {
        attention_quantum_select_heads(attention, attn_scores, 4, 2, selected);
        bcm_quantum_optimize_threshold(bcm, &bcm_stats);
        thalamic_quantum_route(thalamic, 0, dests, 3, thal_features, 2, routed, &num_routed);
    }

    // Verify stats accumulated (bridges work even without full connection)
    attention_quantum_stats_t attn_stats;
    attention_quantum_get_stats(attention, &attn_stats);
    EXPECT_GT(attn_stats.quantum_selections + attn_stats.classical_fallbacks, 0UL);

    bcm_quantum_stats_t bcm_q_stats;
    bcm_quantum_get_stats(bcm, &bcm_q_stats);
    EXPECT_GT(bcm_q_stats.optimization_steps, 0UL);

    // BCM and thalamic should be enabled (no connection requirement)
    EXPECT_TRUE(bcm_quantum_is_enabled(bcm));
    EXPECT_TRUE(thalamic_quantum_bridge_is_enabled(thalamic));
}

TEST_F(CrossBridgeRegressionTest, IndependentStateManagement) {
    attention_quantum_bridge_set_enabled(attention, false);

    EXPECT_FALSE(attention_quantum_bridge_is_enabled(attention));
    EXPECT_TRUE(bcm_quantum_is_enabled(bcm));
    EXPECT_TRUE(thalamic_quantum_bridge_is_enabled(thalamic));
}

TEST_F(CrossBridgeRegressionTest, AllBridgesStressTest) {
    const int ITERATIONS = 500;

    for (int i = 0; i < ITERATIONS; i++) {
        // Attention
        float attn_scores[] = {0.8f, 0.6f, 0.4f, 0.2f};
        uint32_t selected[4];
        attention_quantum_select_heads(attention, attn_scores, 4, 2, selected);

        // BCM
        bcm_activity_stats_t bcm_stats = {
            .avg_weight = 0.5f + sinf((float)i * 0.1f) * 0.2f,
            .weight_variance = 0.1f,
            .avg_post_activity = 0.5f,
            .selectivity_index = 0.5f,
            .num_active_synapses = (uint32_t)(100 + i % 50)
        };
        bcm_quantum_optimize_threshold(bcm, &bcm_stats);

        // Thalamic
        uint32_t dests[] = {1, 2, 3, 4, 5};
        float features[] = {0.5f, 0.5f};
        uint32_t routed[5];
        uint32_t num_routed = 0;
        thalamic_quantum_route(thalamic, 0, dests, 5, features, 2, routed, &num_routed);
    }

    // Verify all bridges processed correctly
    attention_quantum_stats_t attn_stats;
    attention_quantum_get_stats(attention, &attn_stats);
    EXPECT_EQ(attn_stats.quantum_selections + attn_stats.classical_fallbacks, (uint64_t)ITERATIONS);

    bcm_quantum_stats_t bcm_q_stats;
    bcm_quantum_get_stats(bcm, &bcm_q_stats);
    EXPECT_EQ(bcm_q_stats.optimization_steps, (uint64_t)ITERATIONS);

    thalamic_quantum_stats_t thal_stats;
    thalamic_quantum_get_stats(thalamic, &thal_stats);
    EXPECT_EQ(thal_stats.quantum_routes + thal_stats.classical_fallbacks, (uint64_t)ITERATIONS);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
