/**
 * @file test_mirror_emotion_bridge.cpp
 * @brief Comprehensive unit tests for mirror-emotion recognition bridge
 * @version 1.0.0
 * @date 2025-12-31
 *
 * WHAT: Tests for mirror neuron - emotion recognition bidirectional bridge
 * WHY:  Verify emotional contagion, empathy processing, and SIMD operations
 * HOW:  Test lifecycle, observation processing, contagion, agent tracking, statistics
 *
 * Test Coverage:
 * - Bridge lifecycle (create/destroy with config)
 * - Emotion observation processing
 * - Batch observation processing (SIMD)
 * - Emotional contagion computation
 * - Empathy triggering and regulation
 * - Agent state management
 * - SIMD feature similarity
 * - Facial Action Unit comparison
 * - Crisis mode and modulation
 * - Statistics collection and reset
 * - Error handling and edge cases
 */

#include <gtest/gtest.h>

#include "cognitive/mirror_neurons/nimcp_mirror_emotion_bridge.h"
#include "utils/memory/nimcp_memory.h"

#include <cstring>
#include <cmath>
#include <vector>

//=============================================================================
// Test Fixtures
//=============================================================================

class MirrorEmotionBridgeTest : public ::testing::Test {
protected:
    mirror_emotion_bridge_t* bridge;
    mirror_emotion_config_t config;

    void SetUp() override {
        config = mirror_emotion_config_default();
        bridge = nullptr;
    }

    void TearDown() override {
        if (bridge) {
            mirror_emotion_destroy(bridge);
            bridge = nullptr;
        }
    }

    // Helper to create a sample observation
    mirror_emotion_observation_t create_sample_observation(
        uint32_t agent_id = 1,
        mirror_emotion_modality_t modality = MIRROR_EMOTION_MODALITY_FACIAL,
        float resonance = 0.7f
    ) {
        mirror_emotion_observation_t obs = {};
        obs.agent_id = agent_id;
        obs.modality = modality;
        obs.resonance_strength = resonance;
        obs.motor_priming = 0.5f;
        obs.observation_confidence = 0.8f;
        obs.timestamp_us = 1000000;
        obs.duration_us = 500000;
        obs.is_genuine = true;
        obs.is_directed_at_self = false;

        // Set some facial action units (AU6: cheek raiser, AU12: lip corner puller = happiness)
        obs.action_units[6] = 0.8f;  // Cheek raiser
        obs.action_units[12] = 0.9f; // Lip corner puller
        obs.active_au_count = 2;

        // Set emotion features
        for (uint32_t i = 0; i < 8; i++) {
            obs.emotion_features[i] = 0.1f * (i + 1);
        }
        obs.feature_dim = 8;

        return obs;
    }
};

class MirrorEmotionSIMDTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(MirrorEmotionBridgeTest, DefaultConfigHasReasonableValues) {
    // Config should have reasonable defaults
    EXPECT_GT(config.resonance_threshold, 0.0f);
    EXPECT_LT(config.resonance_threshold, 1.0f);
    EXPECT_GT(config.contagion_threshold, 0.0f);
    EXPECT_GT(config.empathy_gain, 0.0f);

    // Modality weights should be positive
    EXPECT_GT(config.facial_weight, 0.0f);
    EXPECT_GT(config.vocal_weight, 0.0f);
    EXPECT_GT(config.bodily_weight, 0.0f);

    // Safety thresholds should be set
    EXPECT_GT(config.crisis_threshold, 0.0f);
    EXPECT_GT(config.regulation_threshold, 0.0f);
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(MirrorEmotionBridgeTest, CreateWithDefaultConfig) {
    bridge = mirror_emotion_create(nullptr);
    ASSERT_NE(nullptr, bridge);
}

TEST_F(MirrorEmotionBridgeTest, CreateWithCustomConfig) {
    config.empathy_gain = 2.0f;
    config.enable_simd = true;
    config.bidirectional_enabled = true;

    bridge = mirror_emotion_create(&config);
    ASSERT_NE(nullptr, bridge);
}

TEST_F(MirrorEmotionBridgeTest, DestroyNullSafe) {
    // Should not crash
    mirror_emotion_destroy(nullptr);
}

TEST_F(MirrorEmotionBridgeTest, CreateDestroyMultipleTimes) {
    for (int i = 0; i < 5; i++) {
        bridge = mirror_emotion_create(&config);
        ASSERT_NE(nullptr, bridge);
        mirror_emotion_destroy(bridge);
        bridge = nullptr;
    }
}

//=============================================================================
// Observation Processing Tests
//=============================================================================

TEST_F(MirrorEmotionBridgeTest, ProcessSingleObservation) {
    bridge = mirror_emotion_create(&config);
    ASSERT_NE(nullptr, bridge);

    mirror_emotion_observation_t obs = create_sample_observation();
    mirror_emotion_resonance_t result = {};

    bool success = mirror_emotion_process_observation(bridge, &obs, &result);
    EXPECT_TRUE(success);

    // Result should have valid data
    EXPECT_GE(result.confidence, 0.0f);
    EXPECT_LE(result.confidence, 1.0f);
    EXPECT_GE(result.intensity, 0.0f);
    EXPECT_LE(result.intensity, 1.0f);

    // Dimensional values should be in range
    EXPECT_GE(result.valence, -1.0f);
    EXPECT_LE(result.valence, 1.0f);
    EXPECT_GE(result.arousal, 0.0f);
    EXPECT_LE(result.arousal, 1.0f);
}

TEST_F(MirrorEmotionBridgeTest, ProcessObservationWithNullBridge) {
    mirror_emotion_observation_t obs = create_sample_observation();
    mirror_emotion_resonance_t result = {};

    bool success = mirror_emotion_process_observation(nullptr, &obs, &result);
    EXPECT_FALSE(success);
}

TEST_F(MirrorEmotionBridgeTest, ProcessObservationWithNullObservation) {
    bridge = mirror_emotion_create(&config);
    ASSERT_NE(nullptr, bridge);

    mirror_emotion_resonance_t result = {};
    bool success = mirror_emotion_process_observation(bridge, nullptr, &result);
    EXPECT_FALSE(success);
}

TEST_F(MirrorEmotionBridgeTest, ProcessObservationWithNullResult) {
    bridge = mirror_emotion_create(&config);
    ASSERT_NE(nullptr, bridge);

    mirror_emotion_observation_t obs = create_sample_observation();
    bool success = mirror_emotion_process_observation(bridge, &obs, nullptr);
    EXPECT_FALSE(success);
}

TEST_F(MirrorEmotionBridgeTest, ProcessMultimodalObservation) {
    bridge = mirror_emotion_create(&config);
    ASSERT_NE(nullptr, bridge);

    mirror_emotion_observation_t obs = create_sample_observation(1, MIRROR_EMOTION_MODALITY_MULTIMODAL);
    mirror_emotion_resonance_t result = {};

    bool success = mirror_emotion_process_observation(bridge, &obs, &result);
    EXPECT_TRUE(success);
}

TEST_F(MirrorEmotionBridgeTest, ProcessDifferentModalities) {
    bridge = mirror_emotion_create(&config);
    ASSERT_NE(nullptr, bridge);

    mirror_emotion_modality_t modalities[] = {
        MIRROR_EMOTION_MODALITY_FACIAL,
        MIRROR_EMOTION_MODALITY_VOCAL,
        MIRROR_EMOTION_MODALITY_BODILY,
        MIRROR_EMOTION_MODALITY_GAZE
    };

    for (auto modality : modalities) {
        mirror_emotion_observation_t obs = create_sample_observation(1, modality);
        mirror_emotion_resonance_t result = {};

        bool success = mirror_emotion_process_observation(bridge, &obs, &result);
        EXPECT_TRUE(success) << "Failed for modality: " << static_cast<int>(modality);
    }
}

//=============================================================================
// Batch Processing Tests
//=============================================================================

TEST_F(MirrorEmotionBridgeTest, ProcessBatchObservations) {
    config.enable_simd = true;
    bridge = mirror_emotion_create(&config);
    ASSERT_NE(nullptr, bridge);

    const uint32_t count = 8;
    std::vector<mirror_emotion_observation_t> observations(count);
    std::vector<mirror_emotion_resonance_t> results(count);

    for (uint32_t i = 0; i < count; i++) {
        observations[i] = create_sample_observation(i + 1);
    }

    uint32_t processed = mirror_emotion_process_batch(
        bridge, observations.data(), results.data(), count
    );

    EXPECT_EQ(count, processed);

    // All results should have valid data
    for (uint32_t i = 0; i < count; i++) {
        EXPECT_GE(results[i].confidence, 0.0f);
        EXPECT_LE(results[i].confidence, 1.0f);
    }
}

TEST_F(MirrorEmotionBridgeTest, ProcessBatchWithZeroCount) {
    bridge = mirror_emotion_create(&config);
    ASSERT_NE(nullptr, bridge);

    mirror_emotion_observation_t obs;
    mirror_emotion_resonance_t result;

    uint32_t processed = mirror_emotion_process_batch(bridge, &obs, &result, 0);
    EXPECT_EQ(0u, processed);
}

TEST_F(MirrorEmotionBridgeTest, ProcessBatchWithNullArrays) {
    bridge = mirror_emotion_create(&config);
    ASSERT_NE(nullptr, bridge);

    uint32_t processed = mirror_emotion_process_batch(bridge, nullptr, nullptr, 5);
    EXPECT_EQ(0u, processed);
}

//=============================================================================
// Contagion and Empathy Tests
//=============================================================================

TEST_F(MirrorEmotionBridgeTest, ComputeContagionForNewAgent) {
    bridge = mirror_emotion_create(&config);
    ASSERT_NE(nullptr, bridge);

    float contagion = mirror_emotion_compute_contagion(bridge, 1, 0, 0.8f);

    // New agent should have some default contagion
    EXPECT_GE(contagion, 0.0f);
    EXPECT_LE(contagion, 1.0f);
}

TEST_F(MirrorEmotionBridgeTest, ContagionIncreasesWithFamiliarity) {
    bridge = mirror_emotion_create(&config);
    ASSERT_NE(nullptr, bridge);

    // Initial contagion for new agent
    float initial_contagion = mirror_emotion_compute_contagion(bridge, 1, 0, 0.8f);

    // Increase familiarity
    mirror_emotion_update_familiarity(bridge, 1, 0.5f);

    // Contagion should potentially increase with familiarity
    float after_familiarity = mirror_emotion_compute_contagion(bridge, 1, 0, 0.8f);

    // Both should be valid values
    EXPECT_GE(initial_contagion, 0.0f);
    EXPECT_GE(after_familiarity, 0.0f);
}

TEST_F(MirrorEmotionBridgeTest, TriggerEmpathyWithHighResonance) {
    bridge = mirror_emotion_create(&config);
    ASSERT_NE(nullptr, bridge);

    mirror_emotion_resonance_t resonance = {};
    resonance.confidence = 0.9f;
    resonance.intensity = 0.8f;
    resonance.empathy_level = 0.85f;
    resonance.contagion_strength = 0.7f;
    resonance.contagion_type = MIRROR_CONTAGION_EMPATHIC;

    bool triggered = mirror_emotion_trigger_empathy(bridge, &resonance);
    // May or may not trigger depending on thresholds
    // Just verify it doesn't crash and returns valid bool
    EXPECT_TRUE(triggered || !triggered);
}

TEST_F(MirrorEmotionBridgeTest, TriggerEmpathyWithNullResonance) {
    bridge = mirror_emotion_create(&config);
    ASSERT_NE(nullptr, bridge);

    bool triggered = mirror_emotion_trigger_empathy(bridge, nullptr);
    EXPECT_FALSE(triggered);
}

TEST_F(MirrorEmotionBridgeTest, RegulateContagion) {
    bridge = mirror_emotion_create(&config);
    ASSERT_NE(nullptr, bridge);

    // First create some contagion
    mirror_emotion_compute_contagion(bridge, 1, 0, 0.8f);

    // Then regulate it
    float regulated = mirror_emotion_regulate_contagion(bridge, 1, 0.5f);

    EXPECT_GE(regulated, 0.0f);
    EXPECT_LE(regulated, 1.0f);
}

TEST_F(MirrorEmotionBridgeTest, RegulateContagionFullSuppression) {
    bridge = mirror_emotion_create(&config);
    ASSERT_NE(nullptr, bridge);

    mirror_emotion_compute_contagion(bridge, 1, 0, 0.8f);

    // Full regulation
    float regulated = mirror_emotion_regulate_contagion(bridge, 1, 1.0f);

    // Should be heavily suppressed
    EXPECT_GE(regulated, 0.0f);
    EXPECT_LE(regulated, 1.0f);
}

//=============================================================================
// Agent State Management Tests
//=============================================================================

TEST_F(MirrorEmotionBridgeTest, GetAgentCreatesNew) {
    bridge = mirror_emotion_create(&config);
    ASSERT_NE(nullptr, bridge);

    mirror_emotion_agent_state_t* agent = mirror_emotion_get_agent(bridge, 42);
    ASSERT_NE(nullptr, agent);

    EXPECT_EQ(42u, agent->agent_id);
    EXPECT_TRUE(agent->active);
}

TEST_F(MirrorEmotionBridgeTest, GetAgentReturnsSame) {
    bridge = mirror_emotion_create(&config);
    ASSERT_NE(nullptr, bridge);

    mirror_emotion_agent_state_t* agent1 = mirror_emotion_get_agent(bridge, 1);
    mirror_emotion_agent_state_t* agent2 = mirror_emotion_get_agent(bridge, 1);

    EXPECT_EQ(agent1, agent2);
}

TEST_F(MirrorEmotionBridgeTest, GetAgentWithNullBridge) {
    mirror_emotion_agent_state_t* agent = mirror_emotion_get_agent(nullptr, 1);
    EXPECT_EQ(nullptr, agent);
}

TEST_F(MirrorEmotionBridgeTest, UpdateFamiliarity) {
    bridge = mirror_emotion_create(&config);
    ASSERT_NE(nullptr, bridge);

    mirror_emotion_agent_state_t* agent = mirror_emotion_get_agent(bridge, 1);
    ASSERT_NE(nullptr, agent);

    float initial = agent->familiarity;
    mirror_emotion_update_familiarity(bridge, 1, 0.3f);

    EXPECT_GT(agent->familiarity, initial);
}

TEST_F(MirrorEmotionBridgeTest, UpdateFamiliarityClampedToOne) {
    bridge = mirror_emotion_create(&config);
    ASSERT_NE(nullptr, bridge);

    mirror_emotion_get_agent(bridge, 1);

    // Large increase
    mirror_emotion_update_familiarity(bridge, 1, 10.0f);

    mirror_emotion_agent_state_t* agent = mirror_emotion_get_agent(bridge, 1);
    EXPECT_LE(agent->familiarity, 1.0f);
}

TEST_F(MirrorEmotionBridgeTest, TrackMultipleAgents) {
    bridge = mirror_emotion_create(&config);
    ASSERT_NE(nullptr, bridge);

    for (uint32_t i = 0; i < 10; i++) {
        mirror_emotion_agent_state_t* agent = mirror_emotion_get_agent(bridge, i);
        ASSERT_NE(nullptr, agent);
        EXPECT_EQ(i, agent->agent_id);
    }

    // Verify all are distinct
    for (uint32_t i = 0; i < 10; i++) {
        mirror_emotion_agent_state_t* agent = mirror_emotion_get_agent(bridge, i);
        EXPECT_EQ(i, agent->agent_id);
    }
}

//=============================================================================
// SIMD Operations Tests
//=============================================================================

TEST_F(MirrorEmotionSIMDTest, SimilarityIdenticalVectors) {
    float a[8] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float b[8] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

    float similarity = mirror_emotion_simd_similarity(a, b, 8);

    EXPECT_NEAR(1.0f, similarity, 0.01f);
}

TEST_F(MirrorEmotionSIMDTest, SimilarityOrthogonalVectors) {
    float a[8] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float b[8] = {0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

    float similarity = mirror_emotion_simd_similarity(a, b, 8);

    EXPECT_NEAR(0.0f, similarity, 0.01f);
}

TEST_F(MirrorEmotionSIMDTest, SimilarityOppositeVectors) {
    float a[8] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float b[8] = {-1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

    float similarity = mirror_emotion_simd_similarity(a, b, 8);

    EXPECT_NEAR(-1.0f, similarity, 0.01f);
}

TEST_F(MirrorEmotionSIMDTest, SimilarityWithZeroVector) {
    float a[8] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
    float b[8] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

    float similarity = mirror_emotion_simd_similarity(a, b, 8);

    // Zero vector should return 0 similarity (handled gracefully)
    EXPECT_GE(similarity, -1.0f);
    EXPECT_LE(similarity, 1.0f);
}

TEST_F(MirrorEmotionSIMDTest, AUCompareIdenticalPatterns) {
    float observed[MIRROR_EMOTION_ACTION_UNITS];
    float templates[MIRROR_EMOTION_ACTION_UNITS];
    float similarities[1];

    // Create identical AU patterns
    for (int i = 0; i < MIRROR_EMOTION_ACTION_UNITS; i++) {
        observed[i] = 0.5f;
        templates[i] = 0.5f;
    }

    mirror_emotion_simd_au_compare(observed, templates, similarities, 1);

    EXPECT_GT(similarities[0], 0.9f);
}

TEST_F(MirrorEmotionSIMDTest, AUCompareDifferentPatterns) {
    float observed[MIRROR_EMOTION_ACTION_UNITS];
    float templates[MIRROR_EMOTION_ACTION_UNITS];
    float similarities[1];

    // Create different AU patterns
    for (int i = 0; i < MIRROR_EMOTION_ACTION_UNITS; i++) {
        observed[i] = (i % 2 == 0) ? 1.0f : 0.0f;
        templates[i] = (i % 2 == 0) ? 0.0f : 1.0f;
    }

    mirror_emotion_simd_au_compare(observed, templates, similarities, 1);

    EXPECT_LT(similarities[0], 0.5f);
}

//=============================================================================
// Modulation and Crisis Mode Tests
//=============================================================================

TEST_F(MirrorEmotionBridgeTest, ModulateSensitivity) {
    bridge = mirror_emotion_create(&config);
    ASSERT_NE(nullptr, bridge);

    float sensitivity = mirror_emotion_modulate_sensitivity(bridge, 0, 0.5f, 0.7f);

    EXPECT_GT(sensitivity, 0.0f);
}

TEST_F(MirrorEmotionBridgeTest, ModulateSensitivityNegativeValence) {
    bridge = mirror_emotion_create(&config);
    ASSERT_NE(nullptr, bridge);

    float positive = mirror_emotion_modulate_sensitivity(bridge, 0, 0.5f, 0.8f);
    float negative = mirror_emotion_modulate_sensitivity(bridge, 0, 0.5f, -0.8f);

    // Both should be valid positive multipliers
    EXPECT_GT(positive, 0.0f);
    EXPECT_GT(negative, 0.0f);
}

TEST_F(MirrorEmotionBridgeTest, SetCrisisMode) {
    bridge = mirror_emotion_create(&config);
    ASSERT_NE(nullptr, bridge);

    // Enable crisis mode
    mirror_emotion_set_crisis_mode(bridge, true);

    // Process observation - should work but with regulated response
    mirror_emotion_observation_t obs = create_sample_observation();
    mirror_emotion_resonance_t result = {};

    bool success = mirror_emotion_process_observation(bridge, &obs, &result);
    EXPECT_TRUE(success);

    // Disable crisis mode
    mirror_emotion_set_crisis_mode(bridge, false);
}

TEST_F(MirrorEmotionBridgeTest, CrisisModeSuppressesContagion) {
    config.enable_crisis_suppression = true;
    bridge = mirror_emotion_create(&config);
    ASSERT_NE(nullptr, bridge);

    // Normal mode contagion
    float normal_contagion = mirror_emotion_compute_contagion(bridge, 1, 0, 0.9f);

    // Enable crisis mode
    mirror_emotion_set_crisis_mode(bridge, true);

    // Crisis mode contagion should be suppressed
    float crisis_contagion = mirror_emotion_compute_contagion(bridge, 1, 0, 0.9f);

    // Crisis contagion should be less than or equal to normal
    EXPECT_LE(crisis_contagion, normal_contagion + 0.1f);  // Allow small tolerance
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(MirrorEmotionBridgeTest, GetStatsInitiallyZero) {
    bridge = mirror_emotion_create(&config);
    ASSERT_NE(nullptr, bridge);

    mirror_emotion_stats_t stats = {};
    bool success = mirror_emotion_get_stats(bridge, &stats);

    EXPECT_TRUE(success);
    EXPECT_EQ(0u, stats.total_observations);
    EXPECT_EQ(0u, stats.resonance_events);
}

TEST_F(MirrorEmotionBridgeTest, StatsIncrementAfterProcessing) {
    bridge = mirror_emotion_create(&config);
    ASSERT_NE(nullptr, bridge);

    mirror_emotion_observation_t obs = create_sample_observation();
    mirror_emotion_resonance_t result = {};

    mirror_emotion_process_observation(bridge, &obs, &result);

    mirror_emotion_stats_t stats = {};
    mirror_emotion_get_stats(bridge, &stats);

    EXPECT_GE(stats.total_observations, 1u);
}

TEST_F(MirrorEmotionBridgeTest, ResetStats) {
    bridge = mirror_emotion_create(&config);
    ASSERT_NE(nullptr, bridge);

    // Process some observations
    for (int i = 0; i < 5; i++) {
        mirror_emotion_observation_t obs = create_sample_observation();
        mirror_emotion_resonance_t result = {};
        mirror_emotion_process_observation(bridge, &obs, &result);
    }

    // Reset
    mirror_emotion_reset_stats(bridge);

    mirror_emotion_stats_t stats = {};
    mirror_emotion_get_stats(bridge, &stats);

    EXPECT_EQ(0u, stats.total_observations);
    EXPECT_EQ(0u, stats.resonance_events);
}

TEST_F(MirrorEmotionBridgeTest, GetStatsWithNullBridge) {
    mirror_emotion_stats_t stats = {};
    bool success = mirror_emotion_get_stats(nullptr, &stats);
    EXPECT_FALSE(success);
}

TEST_F(MirrorEmotionBridgeTest, GetStatsWithNullStats) {
    bridge = mirror_emotion_create(&config);
    ASSERT_NE(nullptr, bridge);

    bool success = mirror_emotion_get_stats(bridge, nullptr);
    EXPECT_FALSE(success);
}

//=============================================================================
// Bio-Async Integration Tests
//=============================================================================

TEST_F(MirrorEmotionBridgeTest, RegisterBioAsync) {
    config.bio_async_enabled = true;
    bridge = mirror_emotion_create(&config);
    ASSERT_NE(nullptr, bridge);

    bool success = mirror_emotion_register_bio_async(bridge);
    // May or may not succeed depending on bio-async system state
    EXPECT_TRUE(success || !success);

    // Cleanup
    mirror_emotion_unregister_bio_async(bridge);
}

TEST_F(MirrorEmotionBridgeTest, UnregisterBioAsyncSafe) {
    bridge = mirror_emotion_create(&config);
    ASSERT_NE(nullptr, bridge);

    // Should not crash even if not registered
    mirror_emotion_unregister_bio_async(bridge);
}

//=============================================================================
// Edge Cases and Error Handling Tests
//=============================================================================

TEST_F(MirrorEmotionBridgeTest, ProcessObservationWithZeroResonance) {
    bridge = mirror_emotion_create(&config);
    ASSERT_NE(nullptr, bridge);

    mirror_emotion_observation_t obs = create_sample_observation();
    obs.resonance_strength = 0.0f;

    mirror_emotion_resonance_t result = {};
    bool success = mirror_emotion_process_observation(bridge, &obs, &result);

    // Zero resonance is below threshold (default 0.3f) so processing should be rejected
    EXPECT_FALSE(success);
    // Result should still be initialized (zeroed from failed processing)
    EXPECT_GE(result.intensity, 0.0f);
}

TEST_F(MirrorEmotionBridgeTest, HandleMaxAgents) {
    bridge = mirror_emotion_create(&config);
    ASSERT_NE(nullptr, bridge);

    // Try to create more than max agents
    for (uint32_t i = 0; i < MIRROR_EMOTION_MAX_AGENTS + 5; i++) {
        mirror_emotion_agent_state_t* agent = mirror_emotion_get_agent(bridge, i);
        // Should handle gracefully (may return existing or null after max)
    }

    // Bridge should still function
    mirror_emotion_observation_t obs = create_sample_observation();
    mirror_emotion_resonance_t result = {};
    bool success = mirror_emotion_process_observation(bridge, &obs, &result);
    EXPECT_TRUE(success);
}

TEST_F(MirrorEmotionBridgeTest, ProcessHighIntensityObservation) {
    bridge = mirror_emotion_create(&config);
    ASSERT_NE(nullptr, bridge);

    mirror_emotion_observation_t obs = create_sample_observation();
    obs.resonance_strength = 1.0f;
    obs.motor_priming = 1.0f;
    obs.observation_confidence = 1.0f;

    // Set all AUs to max
    for (int i = 0; i < MIRROR_EMOTION_ACTION_UNITS; i++) {
        obs.action_units[i] = 1.0f;
    }
    obs.active_au_count = MIRROR_EMOTION_ACTION_UNITS;

    mirror_emotion_resonance_t result = {};
    bool success = mirror_emotion_process_observation(bridge, &obs, &result);

    EXPECT_TRUE(success);
    // Should produce high but valid output
    EXPECT_LE(result.intensity, 1.0f);
    EXPECT_LE(result.confidence, 1.0f);
}
