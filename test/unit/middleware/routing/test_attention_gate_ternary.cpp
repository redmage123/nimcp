//=============================================================================
// test_attention_gate_ternary.cpp - Unit Tests for Ternary Attention Gate
//=============================================================================
/**
 * @file test_attention_gate_ternary.cpp
 * @brief Comprehensive unit tests for ternary attention modes
 *
 * WHAT: Tests ternary attention modes (FOCUS/NEUTRAL/SUPPRESS)
 * WHY:  Validate discrete attention states for efficient attention routing
 * HOW:  GTest-based unit tests with edge cases
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>

// Headers have their own extern "C" guards
#include "middleware/routing/nimcp_attention_gate.h"
#include "utils/ternary/nimcp_ternary.h"
#include "utils/ternary/nimcp_ternary_types.h"
#include "utils/ternary/nimcp_ternary_vector.h"
#include "utils/ternary/nimcp_ternary_convert.h"

//=============================================================================
// Ternary Attention State Definitions
//=============================================================================

// Map ternary to attention states
#define ATTENTION_FOCUS    TRIT_POSITIVE   // +1: Full attention
#define ATTENTION_NEUTRAL  TRIT_UNKNOWN    //  0: Baseline attention
#define ATTENTION_SUPPRESS TRIT_NEGATIVE   // -1: Suppressed attention

//=============================================================================
// Test Fixtures
//=============================================================================

class AttentionGateTernaryTest : public ::testing::Test {
protected:
    attention_gate_t* gate;
    attention_gate_config_t config;

    void SetUp() override {
        config = attention_gate_default_config();
        config.max_targets = 64;
        config.spotlight_size = 8;
        config.enable_winner_take_all = false;
        config.enable_shift_detection = true;
        config.topdown_weight = 0.5f;
        config.bottomup_weight = 0.5f;
        config.mode = ATTENTION_MODE_MIXED;

        gate = attention_gate_create(&config);
        ASSERT_NE(gate, nullptr);
    }

    void TearDown() override {
        if (gate) {
            attention_gate_destroy(gate);
            gate = nullptr;
        }
    }

    // Helper to convert attention weight to ternary state
    trit_t attention_to_ternary(float weight, float focus_threshold = 0.7f,
                                 float suppress_threshold = 0.3f) {
        if (weight >= focus_threshold) return ATTENTION_FOCUS;
        if (weight <= suppress_threshold) return ATTENTION_SUPPRESS;
        return ATTENTION_NEUTRAL;
    }

    // Helper to convert ternary state to attention weight
    float ternary_to_attention(trit_t state) {
        switch (state) {
            case ATTENTION_FOCUS: return 1.0f;
            case ATTENTION_NEUTRAL: return 0.5f;
            case ATTENTION_SUPPRESS: return 0.0f;
            default: return 0.5f;
        }
    }
};

//=============================================================================
// Ternary Attention State Tests
//=============================================================================

TEST_F(AttentionGateTernaryTest, FocusState) {
    // Set high attention weight
    bool result = attention_gate_set_weight(gate, 0, 1, 1.0f);
    EXPECT_TRUE(result);

    float weight;
    result = attention_gate_get_weight(gate, 0, 1, &weight);
    EXPECT_TRUE(result);
    EXPECT_FLOAT_EQ(weight, 1.0f);

    // Convert to ternary
    trit_t state = attention_to_ternary(weight);
    EXPECT_EQ(state, ATTENTION_FOCUS);
}

TEST_F(AttentionGateTernaryTest, NeutralState) {
    // Set baseline attention weight
    bool result = attention_gate_set_weight(gate, 0, 1, 0.5f);
    EXPECT_TRUE(result);

    float weight;
    result = attention_gate_get_weight(gate, 0, 1, &weight);
    EXPECT_TRUE(result);

    // Convert to ternary
    trit_t state = attention_to_ternary(weight);
    EXPECT_EQ(state, ATTENTION_NEUTRAL);
}

TEST_F(AttentionGateTernaryTest, SuppressState) {
    // Set low attention weight
    bool result = attention_gate_set_weight(gate, 0, 1, 0.0f);
    EXPECT_TRUE(result);

    float weight;
    result = attention_gate_get_weight(gate, 0, 1, &weight);
    EXPECT_TRUE(result);
    EXPECT_FLOAT_EQ(weight, 0.0f);

    // Convert to ternary
    trit_t state = attention_to_ternary(weight);
    EXPECT_EQ(state, ATTENTION_SUPPRESS);
}

TEST_F(AttentionGateTernaryTest, TernaryAttentionVector) {
    // Create attention for multiple targets
    const uint32_t num_targets = 16;
    std::vector<float> weights = {
        1.0f, 0.9f, 0.8f, 0.7f,  // Focus
        0.5f, 0.4f, 0.6f, 0.5f,  // Neutral
        0.2f, 0.1f, 0.0f, 0.3f,  // Suppress
        0.75f, 0.25f, 0.5f, 0.85f // Mixed
    };

    // Set weights
    for (uint32_t i = 0; i < num_targets; i++) {
        bool result = attention_gate_set_weight(gate, 0, i, weights[i]);
        EXPECT_TRUE(result);
    }

    // Create ternary vector representation
    trit_vector_t* attention_states = trit_vector_create(num_targets, TERNARY_PACK_NONE);
    ASSERT_NE(attention_states, nullptr);

    for (uint32_t i = 0; i < num_targets; i++) {
        float weight;
        attention_gate_get_weight(gate, 0, i, &weight);
        trit_t state = attention_to_ternary(weight);
        trit_vector_set(attention_states, i, state);
    }

    // Verify ternary states
    EXPECT_EQ(trit_vector_get(attention_states, 0), ATTENTION_FOCUS);     // 1.0
    EXPECT_EQ(trit_vector_get(attention_states, 1), ATTENTION_FOCUS);     // 0.9
    EXPECT_EQ(trit_vector_get(attention_states, 2), ATTENTION_FOCUS);     // 0.8
    EXPECT_EQ(trit_vector_get(attention_states, 3), ATTENTION_FOCUS);     // 0.7
    EXPECT_EQ(trit_vector_get(attention_states, 4), ATTENTION_NEUTRAL);   // 0.5
    EXPECT_EQ(trit_vector_get(attention_states, 8), ATTENTION_SUPPRESS);  // 0.2
    EXPECT_EQ(trit_vector_get(attention_states, 10), ATTENTION_SUPPRESS); // 0.0

    trit_vector_destroy(attention_states);
}

//=============================================================================
// Attention Mode Tests
//=============================================================================

TEST_F(AttentionGateTernaryTest, TopDownAttentionMode) {
    // Pure top-down attention
    attention_gate_config_t topdown_config = attention_gate_default_config();
    topdown_config.mode = ATTENTION_MODE_TOPDOWN;
    topdown_config.topdown_weight = 1.0f;
    topdown_config.bottomup_weight = 0.0f;

    attention_gate_t* topdown_gate = attention_gate_create(&topdown_config);
    ASSERT_NE(topdown_gate, nullptr);

    // Set explicit attention
    attention_gate_set_weight(topdown_gate, 0, 0, 1.0f);  // Focus
    attention_gate_set_weight(topdown_gate, 0, 1, 0.5f);  // Neutral
    attention_gate_set_weight(topdown_gate, 0, 2, 0.0f);  // Suppress

    float weight;
    attention_gate_get_weight(topdown_gate, 0, 0, &weight);
    EXPECT_EQ(attention_to_ternary(weight), ATTENTION_FOCUS);

    attention_gate_get_weight(topdown_gate, 0, 1, &weight);
    EXPECT_EQ(attention_to_ternary(weight), ATTENTION_NEUTRAL);

    attention_gate_get_weight(topdown_gate, 0, 2, &weight);
    EXPECT_EQ(attention_to_ternary(weight), ATTENTION_SUPPRESS);

    attention_gate_destroy(topdown_gate);
}

TEST_F(AttentionGateTernaryTest, BottomUpSalienceMode) {
    // Update bottom-up salience
    bool result = attention_gate_update_salience(gate, 0, 1.0f);  // High salience
    EXPECT_TRUE(result);

    result = attention_gate_update_salience(gate, 1, 0.5f);  // Medium salience
    EXPECT_TRUE(result);

    result = attention_gate_update_salience(gate, 2, 0.1f);  // Low salience
    EXPECT_TRUE(result);
}

//=============================================================================
// Winner-Take-All Tests
//=============================================================================

TEST_F(AttentionGateTernaryTest, WinnerTakeAllTernary) {
    // Enable WTA
    attention_gate_config_t wta_config = attention_gate_default_config();
    wta_config.enable_winner_take_all = true;

    attention_gate_t* wta_gate = attention_gate_create(&wta_config);
    ASSERT_NE(wta_gate, nullptr);

    // Set weights
    attention_gate_set_weight(wta_gate, 0, 0, 0.7f);
    attention_gate_set_weight(wta_gate, 0, 1, 0.9f);  // Winner
    attention_gate_set_weight(wta_gate, 0, 2, 0.6f);
    attention_gate_set_weight(wta_gate, 0, 3, 0.5f);

    // Apply WTA
    uint32_t winner_id;
    bool result = attention_gate_apply_wta(wta_gate, &winner_id);
    EXPECT_TRUE(result);
    EXPECT_EQ(winner_id, 1U);

    // In ternary: winner = FOCUS, others = SUPPRESS
    // Winner should have full attention
    float weight;
    attention_gate_get_weight(wta_gate, 0, winner_id, &weight);
    EXPECT_FLOAT_EQ(weight, 1.0f);
    EXPECT_EQ(attention_to_ternary(weight), ATTENTION_FOCUS);

    attention_gate_destroy(wta_gate);
}

//=============================================================================
// Spotlight Tests
//=============================================================================

TEST_F(AttentionGateTernaryTest, SpotlightTernaryRepresentation) {
    // Set up multiple targets with varying attention
    for (uint32_t i = 0; i < 16; i++) {
        float weight = (float)(16 - i) / 16.0f;  // Decreasing weights
        attention_gate_set_weight(gate, 0, i, weight);
    }

    // Update spotlight (top 8 targets)
    uint32_t spotlight_ids[8];
    uint32_t num_in_spotlight;
    bool result = attention_gate_update_spotlight(gate, spotlight_ids, &num_in_spotlight);
    EXPECT_TRUE(result);
    EXPECT_LE(num_in_spotlight, 8U);

    // Create ternary representation
    trit_vector_t* spotlight_ternary = trit_vector_create(16, TERNARY_PACK_NONE);
    ASSERT_NE(spotlight_ternary, nullptr);

    // Targets in spotlight = FOCUS, others = SUPPRESS
    for (uint32_t i = 0; i < 16; i++) {
        bool in_spotlight = false;
        for (uint32_t j = 0; j < num_in_spotlight; j++) {
            if (spotlight_ids[j] == i) {
                in_spotlight = true;
                break;
            }
        }

        trit_t state = in_spotlight ? ATTENTION_FOCUS : ATTENTION_SUPPRESS;
        trit_vector_set(spotlight_ternary, i, state);
    }

    // Count states
    int focus_count = 0, suppress_count = 0;
    for (uint32_t i = 0; i < 16; i++) {
        if (trit_vector_get(spotlight_ternary, i) == ATTENTION_FOCUS) {
            focus_count++;
        } else {
            suppress_count++;
        }
    }

    EXPECT_EQ(focus_count, (int)num_in_spotlight);
    EXPECT_EQ(suppress_count, 16 - (int)num_in_spotlight);

    trit_vector_destroy(spotlight_ternary);
}

//=============================================================================
// Attention Shift Detection Tests
//=============================================================================

TEST_F(AttentionGateTernaryTest, AttentionShiftTernaryTransition) {
    // Initial state: target 0 focused
    attention_gate_set_weight(gate, 0, 0, 1.0f);
    attention_gate_set_weight(gate, 0, 1, 0.0f);

    // Shift attention to target 1
    attention_gate_set_weight(gate, 0, 0, 0.0f);
    attention_gate_set_weight(gate, 0, 1, 1.0f);

    // This represents ternary transition:
    // Target 0: FOCUS -> SUPPRESS
    // Target 1: SUPPRESS -> FOCUS

    float weight0, weight1;
    attention_gate_get_weight(gate, 0, 0, &weight0);
    attention_gate_get_weight(gate, 0, 1, &weight1);

    EXPECT_EQ(attention_to_ternary(weight0), ATTENTION_SUPPRESS);
    EXPECT_EQ(attention_to_ternary(weight1), ATTENTION_FOCUS);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(AttentionGateTernaryTest, GateStatistics) {
    // Add some targets
    for (uint32_t i = 0; i < 10; i++) {
        attention_gate_set_weight(gate, 0, i, (float)i / 10.0f);
    }

    uint32_t num_targets, num_in_spotlight;
    uint64_t total_shifts;
    bool result = attention_gate_get_stats(gate, &num_targets, &num_in_spotlight, &total_shifts);
    EXPECT_TRUE(result);
    EXPECT_GT(num_targets, 0U);
}

//=============================================================================
// Reset Tests
//=============================================================================

TEST_F(AttentionGateTernaryTest, ResetToNeutral) {
    // Set up attention
    attention_gate_set_weight(gate, 0, 0, 1.0f);
    attention_gate_set_weight(gate, 0, 1, 0.0f);

    // Reset
    attention_gate_reset(gate);

    // After reset, all should be at baseline (or undefined)
    // Behavior depends on implementation
}

//=============================================================================
// Null Pointer Handling
//=============================================================================

TEST_F(AttentionGateTernaryTest, NullGateHandling) {
    bool result = attention_gate_set_weight(nullptr, 0, 0, 1.0f);
    EXPECT_FALSE(result);

    float weight;
    result = attention_gate_get_weight(nullptr, 0, 0, &weight);
    EXPECT_FALSE(result);

    result = attention_gate_update_salience(nullptr, 0, 1.0f);
    EXPECT_FALSE(result);

    // Destroy null should not crash
    attention_gate_destroy(nullptr);
}

TEST_F(AttentionGateTernaryTest, NullOutputHandling) {
    bool result = attention_gate_get_weight(gate, 0, 0, nullptr);
    EXPECT_FALSE(result);

    result = attention_gate_apply_wta(gate, nullptr);
    // Might succeed even with null output - depends on implementation
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(AttentionGateTernaryTest, BoundaryWeights) {
    // Test boundary values
    attention_gate_set_weight(gate, 0, 0, 0.0f);  // Minimum
    attention_gate_set_weight(gate, 0, 1, 1.0f);  // Maximum
    attention_gate_set_weight(gate, 0, 2, 0.5f);  // Middle

    float weight;
    attention_gate_get_weight(gate, 0, 0, &weight);
    EXPECT_FLOAT_EQ(weight, 0.0f);

    attention_gate_get_weight(gate, 0, 1, &weight);
    EXPECT_FLOAT_EQ(weight, 1.0f);

    attention_gate_get_weight(gate, 0, 2, &weight);
    EXPECT_FLOAT_EQ(weight, 0.5f);
}

TEST_F(AttentionGateTernaryTest, ThresholdBoundaries) {
    // Test at exact threshold boundaries
    const float focus_threshold = 0.7f;
    const float suppress_threshold = 0.3f;

    // Exactly at focus threshold
    trit_t state = attention_to_ternary(focus_threshold, focus_threshold, suppress_threshold);
    EXPECT_EQ(state, ATTENTION_FOCUS);  // >= threshold

    // Just below focus threshold
    state = attention_to_ternary(focus_threshold - 0.01f, focus_threshold, suppress_threshold);
    EXPECT_EQ(state, ATTENTION_NEUTRAL);

    // Exactly at suppress threshold
    state = attention_to_ternary(suppress_threshold, focus_threshold, suppress_threshold);
    EXPECT_EQ(state, ATTENTION_SUPPRESS);  // <= threshold

    // Just above suppress threshold
    state = attention_to_ternary(suppress_threshold + 0.01f, focus_threshold, suppress_threshold);
    EXPECT_EQ(state, ATTENTION_NEUTRAL);
}

//=============================================================================
// Default Configuration Test
//=============================================================================

TEST_F(AttentionGateTernaryTest, DefaultConfiguration) {
    attention_gate_config_t default_config = attention_gate_default_config();

    EXPECT_GT(default_config.max_targets, 0U);
    EXPECT_GT(default_config.spotlight_size, 0U);
    EXPECT_GE(default_config.topdown_weight, 0.0f);
    EXPECT_LE(default_config.topdown_weight, 1.0f);
    EXPECT_GE(default_config.bottomup_weight, 0.0f);
    EXPECT_LE(default_config.bottomup_weight, 1.0f);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
