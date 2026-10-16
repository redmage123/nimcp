/**
 * @file test_nimcp_insula_quantum_bridge.cpp
 * @brief Unit tests for nimcp_insula_quantum_bridge.c
 *
 * WHAT: Comprehensive unit tests for the Insula quantum bridge
 * WHY:  Ensure correct quantum-accelerated interoception and emotional processing
 * HOW:  Use Google Test framework to test lifecycle, interoceptive integration,
 *       emotional evaluation, somatic marker search, and statistics.
 *
 * COVERAGE TARGET: 100%
 */

#include <gtest/gtest.h>
#include <string.h>
#include <stdlib.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "core/brain/regions/insula/nimcp_insula_quantum_bridge.h"

// Test Fixture for Insula Quantum Bridge
class InsulaQuantumBridgeTest : public ::testing::Test {
protected:
    insula_quantum_bridge_t* bridge;
    insula_quantum_config_t config;

    void SetUp() override {
        config = insula_quantum_default_config();
        bridge = insula_quantum_bridge_create(NULL, &config);  // Insula handle can be NULL for testing
        ASSERT_NE(nullptr, bridge) << "Failed to create Insula quantum bridge";
    }

    void TearDown() override {
        insula_quantum_bridge_destroy(bridge);
        bridge = nullptr;
    }
};

// ============================================================================
// CONFIGURATION TESTS
// ============================================================================

TEST_F(InsulaQuantumBridgeTest, DefaultConfigHasReasonableValues) {
    insula_quantum_config_t default_config = insula_quantum_default_config();

    EXPECT_TRUE(default_config.enabled);
    EXPECT_EQ(default_config.intero_channels, 16u);
    EXPECT_EQ(default_config.emotion_superposition_size, 8u);
    EXPECT_GT(default_config.max_grover_iterations, 0u);
    EXPECT_GT(default_config.min_confidence_threshold, 0.0f);
    EXPECT_LE(default_config.min_confidence_threshold, 1.0f);
    EXPECT_TRUE(default_config.enable_interference);
    EXPECT_TRUE(default_config.use_superposition);
}

TEST_F(InsulaQuantumBridgeTest, GetConfigSuccess) {
    insula_quantum_config_t retrieved;
    EXPECT_EQ(insula_quantum_get_config(bridge, &retrieved), 0);
    EXPECT_EQ(retrieved.intero_channels, config.intero_channels);
    EXPECT_EQ(retrieved.enabled, config.enabled);
}

TEST_F(InsulaQuantumBridgeTest, GetConfigNullFails) {
    insula_quantum_config_t retrieved;
    EXPECT_EQ(insula_quantum_get_config(NULL, &retrieved), -1);
    EXPECT_EQ(insula_quantum_get_config(bridge, NULL), -1);
}

// ============================================================================
// LIFECYCLE TESTS
// ============================================================================

TEST_F(InsulaQuantumBridgeTest, CreateWithNullConfigUsesDefaults) {
    insula_quantum_bridge_t* bridge_null = insula_quantum_bridge_create(NULL, NULL);
    ASSERT_NE(nullptr, bridge_null);

    insula_quantum_config_t retrieved;
    EXPECT_EQ(insula_quantum_get_config(bridge_null, &retrieved), 0);
    EXPECT_TRUE(retrieved.enabled);

    insula_quantum_bridge_destroy(bridge_null);
}

TEST_F(InsulaQuantumBridgeTest, DestroyNullDoesNotCrash) {
    insula_quantum_bridge_destroy(NULL);
    // Should not crash
}

TEST_F(InsulaQuantumBridgeTest, IsEnabledSuccess) {
    EXPECT_TRUE(insula_quantum_bridge_is_enabled(bridge));
}

TEST_F(InsulaQuantumBridgeTest, IsEnabledNullReturnsFalse) {
    EXPECT_FALSE(insula_quantum_bridge_is_enabled(NULL));
}

TEST_F(InsulaQuantumBridgeTest, SetEnabledToggles) {
    EXPECT_TRUE(insula_quantum_bridge_is_enabled(bridge));

    insula_quantum_bridge_set_enabled(bridge, false);
    EXPECT_FALSE(insula_quantum_bridge_is_enabled(bridge));

    insula_quantum_bridge_set_enabled(bridge, true);
    EXPECT_TRUE(insula_quantum_bridge_is_enabled(bridge));
}

// ============================================================================
// INTEROCEPTIVE INTEGRATION TESTS
// ============================================================================

TEST_F(InsulaQuantumBridgeTest, InitInteroceptionSuccess) {
    float channels[8] = {0.5f, 0.6f, 0.7f, 0.4f, 0.3f, 0.8f, 0.2f, 0.9f};
    EXPECT_EQ(insula_quantum_init_interoception(bridge, channels, 8), 0);
}

TEST_F(InsulaQuantumBridgeTest, InitInteroceptionNullFails) {
    float channels[8] = {0.5f};
    EXPECT_EQ(insula_quantum_init_interoception(NULL, channels, 8), -1);
    EXPECT_EQ(insula_quantum_init_interoception(bridge, NULL, 8), -1);
}

TEST_F(InsulaQuantumBridgeTest, IntegrateInteroceptionSuccess) {
    // Initialize first
    float channels[8] = {0.5f, 0.6f, 0.7f, 0.4f, 0.3f, 0.8f, 0.2f, 0.9f};
    EXPECT_EQ(insula_quantum_init_interoception(bridge, channels, 8), 0);

    // Integrate
    quantum_intero_result_t result;
    EXPECT_EQ(insula_quantum_integrate_interoception(bridge, &result), 0);

    // Check result
    EXPECT_GT(result.integration_quality, 0.0f);
    EXPECT_GT(result.coherence, 0.0f);
    EXPECT_GT(result.channels_fused, 0u);
    EXPECT_GT(result.speedup, 0.0f);
}

TEST_F(InsulaQuantumBridgeTest, IntegrateInteroceptionNullFails) {
    quantum_intero_result_t result;
    EXPECT_EQ(insula_quantum_integrate_interoception(NULL, &result), -1);
    EXPECT_EQ(insula_quantum_integrate_interoception(bridge, NULL), -1);
}

TEST_F(InsulaQuantumBridgeTest, IntegrateInteroceptionWhenDisabledFails) {
    insula_quantum_bridge_set_enabled(bridge, false);

    float channels[4] = {0.5f, 0.6f, 0.7f, 0.4f};
    EXPECT_EQ(insula_quantum_init_interoception(bridge, channels, 4), -1);

    quantum_intero_result_t result;
    EXPECT_EQ(insula_quantum_integrate_interoception(bridge, &result), -1);
}

TEST_F(InsulaQuantumBridgeTest, CorrectInteroNoiseSuccess) {
    float noisy[4] = {0.5f, 0.3f, 0.8f, 0.2f};
    float corrected[4] = {0.0f};

    EXPECT_EQ(insula_quantum_correct_intero_noise(bridge, noisy, corrected, 4), 0);

    // Corrected values should be valid
    for (int i = 0; i < 4; i++) {
        EXPECT_GE(corrected[i], 0.0f);
        EXPECT_LE(corrected[i], 1.0f);
    }
}

TEST_F(InsulaQuantumBridgeTest, CorrectInteroNoiseNullFails) {
    float noisy[4] = {0.5f};
    float corrected[4] = {0.0f};

    EXPECT_EQ(insula_quantum_correct_intero_noise(NULL, noisy, corrected, 4), -1);
    EXPECT_EQ(insula_quantum_correct_intero_noise(bridge, NULL, corrected, 4), -1);
    EXPECT_EQ(insula_quantum_correct_intero_noise(bridge, noisy, NULL, 4), -1);
}

// ============================================================================
// EMOTIONAL EVALUATION TESTS
// ============================================================================

TEST_F(InsulaQuantumBridgeTest, EvaluateEmotionSuccess) {
    float body_state[4] = {0.5f, 0.6f, 0.3f, 0.7f};
    quantum_emotion_result_t result;

    EXPECT_EQ(insula_quantum_evaluate_emotion(bridge, body_state, 4, &result), 0);

    // Check result
    EXPECT_NE(result.best_emotion, nullptr);
    EXPECT_GT(result.hypotheses_evaluated, 0u);
    EXPECT_GE(result.emotional_clarity, 0.0f);
    EXPECT_LE(result.emotional_clarity, 1.0f);
}

TEST_F(InsulaQuantumBridgeTest, EvaluateEmotionNullBodyStateOk) {
    quantum_emotion_result_t result;
    // Should work with NULL body state (uses defaults)
    EXPECT_EQ(insula_quantum_evaluate_emotion(bridge, NULL, 0, &result), 0);
}

TEST_F(InsulaQuantumBridgeTest, EvaluateEmotionNullResultFails) {
    float body_state[4] = {0.5f};
    EXPECT_EQ(insula_quantum_evaluate_emotion(bridge, body_state, 4, NULL), -1);
}

TEST_F(InsulaQuantumBridgeTest, EvaluateEmotionBestHasValidFields) {
    float body_state[4] = {0.5f, 0.6f, 0.3f, 0.7f};
    quantum_emotion_result_t result;

    EXPECT_EQ(insula_quantum_evaluate_emotion(bridge, body_state, 4, &result), 0);

    if (result.best_emotion) {
        EXPECT_GE(result.best_emotion->valence, -1.0f);
        EXPECT_LE(result.best_emotion->valence, 1.0f);
        EXPECT_GE(result.best_emotion->arousal, -1.0f);
        EXPECT_LE(result.best_emotion->arousal, 1.0f);
        EXPECT_GT(strlen(result.best_emotion->emotion_label), 0u);
    }
}

TEST_F(InsulaQuantumBridgeTest, ApplyEmotionalInterferenceSuccess) {
    quantum_emotion_hypothesis_t hypotheses[4];
    for (int i = 0; i < 4; i++) {
        hypotheses[i].hypothesis_id = i;
        hypotheses[i].amplitude = 0.5f;
        hypotheses[i].probability = 0.25f;
    }

    // Create simple consistency matrix
    float consistency[16] = {
        1.0f, 0.8f, 0.3f, 0.2f,
        0.8f, 1.0f, 0.4f, 0.3f,
        0.3f, 0.4f, 1.0f, 0.7f,
        0.2f, 0.3f, 0.7f, 1.0f
    };

    EXPECT_EQ(insula_quantum_apply_emotional_interference(bridge, hypotheses, 4, consistency), 0);

    // Amplitudes should have changed
    // (Actual values depend on interference calculation)
}

TEST_F(InsulaQuantumBridgeTest, ApplyEmotionalInterferenceNullMatrixOk) {
    quantum_emotion_hypothesis_t hypotheses[2];
    hypotheses[0].amplitude = 0.5f;
    hypotheses[1].amplitude = 0.5f;

    EXPECT_EQ(insula_quantum_apply_emotional_interference(bridge, hypotheses, 2, NULL), 0);
}

// ============================================================================
// SOMATIC MARKER SEARCH TESTS
// ============================================================================

TEST_F(InsulaQuantumBridgeTest, SearchSomaticMarkerSuccess) {
    float context[4] = {0.5f, 0.6f, 0.7f, 0.8f};
    quantum_somatic_result_t result;

    EXPECT_EQ(insula_quantum_search_somatic_marker(bridge, context, 4, 100, &result), 0);

    EXPECT_NE(result.best_marker, nullptr);
    EXPECT_GT(result.markers_evaluated, 0u);
    EXPECT_GT(result.search_speedup, 0.0f);
}

TEST_F(InsulaQuantumBridgeTest, SearchSomaticMarkerNullFails) {
    float context[4] = {0.5f};
    quantum_somatic_result_t result;

    EXPECT_EQ(insula_quantum_search_somatic_marker(NULL, context, 4, 100, &result), -1);
    EXPECT_EQ(insula_quantum_search_somatic_marker(bridge, context, 4, 100, NULL), -1);
}

TEST_F(InsulaQuantumBridgeTest, SearchSomaticMarkerSpeedupIsSquareRoot) {
    float context[4] = {0.5f, 0.6f, 0.7f, 0.8f};
    quantum_somatic_result_t result;

    uint32_t database_size = 1000;
    EXPECT_EQ(insula_quantum_search_somatic_marker(bridge, context, 4, database_size, &result), 0);

    // Grover provides sqrt(N) speedup
    float expected_speedup = sqrtf((float)database_size);
    EXPECT_GT(result.search_speedup, expected_speedup * 0.5f);  // Allow some margin
}

// ============================================================================
// SOCIAL EVALUATION TESTS
// ============================================================================

TEST_F(InsulaQuantumBridgeTest, EvaluateSocialSuccess) {
    float cues[4] = {0.7f, 0.6f, 0.8f, 0.5f};
    float trust, fairness;

    EXPECT_EQ(insula_quantum_evaluate_social(bridge, cues, 4, &trust, &fairness), 0);

    EXPECT_GE(trust, 0.0f);
    EXPECT_LE(trust, 1.0f);
    EXPECT_GE(fairness, 0.0f);
    EXPECT_LE(fairness, 1.0f);
}

TEST_F(InsulaQuantumBridgeTest, EvaluateSocialNullCuesOk) {
    float trust, fairness;

    // Should work with NULL cues (uses defaults)
    EXPECT_EQ(insula_quantum_evaluate_social(bridge, NULL, 0, &trust, &fairness), 0);
}

TEST_F(InsulaQuantumBridgeTest, EvaluateSocialNullOutputFails) {
    float cues[4] = {0.5f};
    float trust;

    EXPECT_EQ(insula_quantum_evaluate_social(bridge, cues, 4, NULL, &trust), -1);
    EXPECT_EQ(insula_quantum_evaluate_social(bridge, cues, 4, &trust, NULL), -1);
}

// ============================================================================
// HOMEOSTATIC OPTIMIZATION TESTS
// ============================================================================

TEST_F(InsulaQuantumBridgeTest, OptimizeHomeostasisSuccess) {
    float current[4] = {0.3f, 0.4f, 0.6f, 0.7f};
    float target[4] = {0.5f, 0.5f, 0.5f, 0.5f};
    float action[4] = {0.0f};

    EXPECT_EQ(insula_quantum_optimize_homeostasis(bridge, current, target, 4, action, 4), 0);

    // Action should point toward target
    for (int i = 0; i < 4; i++) {
        float expected_direction = target[i] - current[i];
        // Action should have same sign as error (modulo scaling)
        if (fabsf(expected_direction) > 0.1f) {
            EXPECT_TRUE((action[i] > 0 && expected_direction > 0) ||
                        (action[i] < 0 && expected_direction < 0) ||
                        fabsf(action[i]) < 0.1f)
                << "Action[" << i << "] = " << action[i]
                << ", expected direction = " << expected_direction;
        }
    }
}

TEST_F(InsulaQuantumBridgeTest, OptimizeHomeostasisNullFails) {
    float current[4] = {0.5f};
    float target[4] = {0.5f};
    float action[4] = {0.0f};

    EXPECT_EQ(insula_quantum_optimize_homeostasis(NULL, current, target, 4, action, 4), -1);
    EXPECT_EQ(insula_quantum_optimize_homeostasis(bridge, NULL, target, 4, action, 4), -1);
    EXPECT_EQ(insula_quantum_optimize_homeostasis(bridge, current, NULL, 4, action, 4), -1);
    EXPECT_EQ(insula_quantum_optimize_homeostasis(bridge, current, target, 4, NULL, 4), -1);
}

// ============================================================================
// STATISTICS TESTS
// ============================================================================

TEST_F(InsulaQuantumBridgeTest, GetStatsSuccess) {
    // Do some processing
    float channels[4] = {0.5f, 0.6f, 0.7f, 0.8f};
    insula_quantum_init_interoception(bridge, channels, 4);

    quantum_intero_result_t intero_result;
    insula_quantum_integrate_interoception(bridge, &intero_result);

    quantum_emotion_result_t emotion_result;
    insula_quantum_evaluate_emotion(bridge, channels, 4, &emotion_result);

    quantum_somatic_result_t somatic_result;
    insula_quantum_search_somatic_marker(bridge, channels, 4, 50, &somatic_result);

    // Get stats
    insula_quantum_stats_t stats;
    EXPECT_EQ(insula_quantum_get_stats(bridge, &stats), 0);

    EXPECT_GE(stats.intero_integrations, 1u);
    EXPECT_GE(stats.emotion_evaluations, 1u);
    EXPECT_GE(stats.somatic_searches, 1u);
}

TEST_F(InsulaQuantumBridgeTest, GetStatsNullFails) {
    insula_quantum_stats_t stats;
    EXPECT_EQ(insula_quantum_get_stats(NULL, &stats), -1);
    EXPECT_EQ(insula_quantum_get_stats(bridge, NULL), -1);
}

TEST_F(InsulaQuantumBridgeTest, ResetStatsSuccess) {
    // Do some processing
    float channels[4] = {0.5f};
    insula_quantum_init_interoception(bridge, channels, 4);

    quantum_intero_result_t result;
    insula_quantum_integrate_interoception(bridge, &result);

    // Reset
    insula_quantum_reset_stats(bridge);

    // Check stats are zero
    insula_quantum_stats_t stats;
    EXPECT_EQ(insula_quantum_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.intero_integrations, 0u);
    EXPECT_EQ(stats.emotion_evaluations, 0u);
    EXPECT_EQ(stats.somatic_searches, 0u);
}

TEST_F(InsulaQuantumBridgeTest, ResetStatsNullDoesNotCrash) {
    insula_quantum_reset_stats(NULL);
    // Should not crash
}

// ============================================================================
// EDGE CASE TESTS
// ============================================================================

TEST_F(InsulaQuantumBridgeTest, LargeChannelCount) {
    // Test with maximum channels
    float channels[32];
    for (int i = 0; i < 32; i++) {
        channels[i] = (float)i / 32.0f;
    }

    // Create bridge with more channels
    insula_quantum_config_t large_config = insula_quantum_default_config();
    large_config.intero_channels = 32;
    insula_quantum_bridge_t* large_bridge = insula_quantum_bridge_create(NULL, &large_config);
    ASSERT_NE(nullptr, large_bridge);

    EXPECT_EQ(insula_quantum_init_interoception(large_bridge, channels, 32), 0);

    quantum_intero_result_t result;
    EXPECT_EQ(insula_quantum_integrate_interoception(large_bridge, &result), 0);

    insula_quantum_bridge_destroy(large_bridge);
}

TEST_F(InsulaQuantumBridgeTest, ExtremeBodyStates) {
    // Test with extreme body state values
    float extreme_high[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    float extreme_low[4] = {0.0f, 0.0f, 0.0f, 0.0f};

    quantum_emotion_result_t result;

    EXPECT_EQ(insula_quantum_evaluate_emotion(bridge, extreme_high, 4, &result), 0);
    EXPECT_EQ(insula_quantum_evaluate_emotion(bridge, extreme_low, 4, &result), 0);
}

TEST_F(InsulaQuantumBridgeTest, RepeatedOperations) {
    // Test repeated operations to check for accumulation errors
    for (int iter = 0; iter < 50; iter++) {
        float channels[4] = {
            (float)(iter % 10) / 10.0f,
            (float)((iter + 3) % 10) / 10.0f,
            (float)((iter + 5) % 10) / 10.0f,
            (float)((iter + 7) % 10) / 10.0f
        };

        EXPECT_EQ(insula_quantum_init_interoception(bridge, channels, 4), 0);

        quantum_intero_result_t result;
        EXPECT_EQ(insula_quantum_integrate_interoception(bridge, &result), 0);

        // Values should remain bounded
        EXPECT_GE(result.integration_quality, 0.0f);
        EXPECT_LE(result.integration_quality, 1.0f);
        EXPECT_GE(result.coherence, 0.0f);
    }
}
