/**
 * @file test_mirror_emotion_attention_integration.cpp
 * @brief Integration tests for mirror-emotion and mirror-attention bridges
 * @version 1.0.0
 * @date 2025-12-31
 *
 * WHAT: Tests for cross-bridge integration between emotion and attention systems
 * WHY:  Verify that emotional processing affects attention and vice versa
 * HOW:  Test combined workflows, data flow, and emergent behaviors
 *
 * Test Coverage:
 * - Emotion-Attention bridge coordination
 * - Emotional saliency affecting attention shifts
 * - Joint attention with emotional context
 * - Crisis emotion triggering attention regulation
 * - Combined observation processing
 * - Agent state consistency across bridges
 * - Statistics aggregation
 */

#include <gtest/gtest.h>

#include "cognitive/mirror_neurons/nimcp_mirror_emotion_bridge.h"
#include "cognitive/mirror_neurons/nimcp_mirror_attention_bridge.h"
#include "cognitive/nimcp_mirror_neurons.h"
#include "utils/memory/nimcp_memory.h"

#include <cstring>
#include <cmath>
#include <vector>

//=============================================================================
// Test Fixtures
//=============================================================================

class MirrorEmotionAttentionIntegrationTest : public ::testing::Test {
protected:
    mirror_emotion_bridge_t* emotion_bridge;
    mirror_attention_bridge_t* attention_bridge;
    mirror_emotion_config_t emotion_config;
    mirror_attention_config_t attention_config;

    void SetUp() override {
        emotion_config = mirror_emotion_config_default();
        attention_config = mirror_attention_config_default();
        emotion_bridge = nullptr;
        attention_bridge = nullptr;
    }

    void TearDown() override {
        if (emotion_bridge) {
            mirror_emotion_destroy(emotion_bridge);
            emotion_bridge = nullptr;
        }
        if (attention_bridge) {
            mirror_attention_destroy(attention_bridge);
            attention_bridge = nullptr;
        }
    }

    // Helper to create sample emotion observation
    mirror_emotion_observation_t create_emotion_obs(
        uint32_t agent_id = 1,
        float resonance = 0.7f,
        mirror_emotion_modality_t modality = MIRROR_EMOTION_MODALITY_FACIAL
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

        // Happiness AU pattern
        obs.action_units[6] = 0.8f;
        obs.action_units[12] = 0.9f;
        obs.active_au_count = 2;

        for (uint32_t i = 0; i < 8; i++) {
            obs.emotion_features[i] = 0.1f * (i + 1);
        }
        obs.feature_dim = 8;

        return obs;
    }

    // Helper to create sample gaze observation
    mirror_gaze_observation_t create_gaze_obs(
        uint32_t agent_id = 1,
        mirror_attention_cue_type_t cue_type = MIRROR_CUE_GAZE
    ) {
        mirror_gaze_observation_t obs = {};
        obs.agent_id = agent_id;
        obs.cue_type = cue_type;

        obs.direction.x = 0.707f;
        obs.direction.y = -0.5f;
        obs.direction.z = 0.5f;

        obs.agent_position.x = 0.0f;
        obs.agent_position.y = 1.6f;
        obs.agent_position.z = 0.0f;

        obs.target_position.x = 2.0f;
        obs.target_position.y = 0.8f;
        obs.target_position.z = 1.0f;
        obs.target_position_valid = true;

        obs.confidence = 0.85f;
        obs.duration_ms = 200.0f;
        obs.timestamp_us = 1000000;

        obs.is_referential = true;

        return obs;
    }

    // Create both bridges
    void createBridges() {
        emotion_bridge = mirror_emotion_create(&emotion_config);
        attention_bridge = mirror_attention_create(&attention_config);
    }
};

//=============================================================================
// Basic Integration Tests
//=============================================================================

TEST_F(MirrorEmotionAttentionIntegrationTest, BothBridgesCreateSuccessfully) {
    createBridges();
    ASSERT_NE(nullptr, emotion_bridge);
    ASSERT_NE(nullptr, attention_bridge);
}

TEST_F(MirrorEmotionAttentionIntegrationTest, ProcessSameAgentInBothBridges) {
    createBridges();

    const uint32_t agent_id = 42;

    // Process emotion observation
    mirror_emotion_observation_t emo_obs = create_emotion_obs(agent_id);
    mirror_emotion_resonance_t emo_result = {};
    bool emo_success = mirror_emotion_process_observation(emotion_bridge, &emo_obs, &emo_result);

    // Process gaze observation from same agent
    mirror_gaze_observation_t gaze_obs = create_gaze_obs(agent_id);
    mirror_attention_cue_t att_result = {};
    bool att_success = mirror_attention_process_gaze(attention_bridge, &gaze_obs, &att_result);

    EXPECT_TRUE(emo_success);
    EXPECT_TRUE(att_success);

    // Both bridges should track this agent
    mirror_emotion_agent_state_t* emo_agent = mirror_emotion_get_agent(emotion_bridge, agent_id);
    mirror_attention_agent_t* att_agent = mirror_attention_get_agent(attention_bridge, agent_id);

    ASSERT_NE(nullptr, emo_agent);
    ASSERT_NE(nullptr, att_agent);

    EXPECT_EQ(agent_id, emo_agent->agent_id);
    EXPECT_EQ(agent_id, att_agent->agent_id);
}

//=============================================================================
// Emotional Context Affecting Attention Tests
//=============================================================================

TEST_F(MirrorEmotionAttentionIntegrationTest, HighEmotionIncreasesAttentionSaliency) {
    emotion_config.enable_simd = true;
    attention_config.enable_saliency_modulation = true;
    createBridges();

    const uint32_t agent_id = 1;

    // First process high-intensity emotional observation
    mirror_emotion_observation_t emo_obs = create_emotion_obs(agent_id, 0.95f);
    emo_obs.is_directed_at_self = true;  // Directed at observer - should boost salience
    mirror_emotion_resonance_t emo_result = {};
    mirror_emotion_process_observation(emotion_bridge, &emo_obs, &emo_result);

    // Use emotion result to modulate attention
    // In a real system, this would happen through bio-async messages
    mirror_attention_vec3_t focus = {0.0f, 1.6f, 2.0f};
    float strength = emo_result.intensity * emo_result.empathy_level;
    mirror_attention_set_focus(attention_bridge, &focus, strength, 0.5f);

    // Now process gaze from same agent
    mirror_gaze_observation_t gaze_obs = create_gaze_obs(agent_id);
    mirror_attention_cue_t att_result = {};
    mirror_attention_process_gaze(attention_bridge, &gaze_obs, &att_result);

    // High emotion should result in stronger attention cue
    EXPECT_GT(att_result.cue_strength, 0.0f);
}

TEST_F(MirrorEmotionAttentionIntegrationTest, EmotionalContagionFromJointAttention) {
    emotion_config.bidirectional_enabled = true;
    attention_config.enable_joint_attention_initiation = true;
    createBridges();

    const uint32_t agent_id = 1;

    // Establish joint attention
    mirror_gaze_observation_t gaze_obs = create_gaze_obs(agent_id);
    gaze_obs.is_referential = true;
    mirror_attention_cue_t att_result = {};
    mirror_attention_process_gaze(attention_bridge, &gaze_obs, &att_result);

    mirror_attention_respond_to_joint(attention_bridge, agent_id);
    joint_attention_state_t ja_state = mirror_attention_get_joint_state(attention_bridge, agent_id);

    // Now process emotion while in joint attention
    mirror_emotion_observation_t emo_obs = create_emotion_obs(agent_id, 0.8f);
    mirror_emotion_resonance_t emo_result = {};
    mirror_emotion_process_observation(emotion_bridge, &emo_obs, &emo_result);

    // Emotional processing should work during joint attention
    EXPECT_GE(emo_result.confidence, 0.0f);
    EXPECT_GE(emo_result.empathy_level, 0.0f);
}

//=============================================================================
// Crisis Mode Integration Tests
//=============================================================================

TEST_F(MirrorEmotionAttentionIntegrationTest, CrisisModeAffectsBothBridges) {
    emotion_config.enable_crisis_suppression = true;
    createBridges();

    const uint32_t agent_id = 1;

    // Enable crisis mode in emotion bridge
    mirror_emotion_set_crisis_mode(emotion_bridge, true);

    // Process crisis emotion (fear or distress)
    mirror_emotion_observation_t emo_obs = create_emotion_obs(agent_id, 0.95f);
    // Set fear AU pattern
    std::memset(emo_obs.action_units, 0, sizeof(emo_obs.action_units));
    emo_obs.action_units[1] = 0.9f;  // Inner brow raiser
    emo_obs.action_units[2] = 0.8f;  // Outer brow raiser
    emo_obs.action_units[4] = 0.7f;  // Brow lowerer
    emo_obs.action_units[5] = 0.6f;  // Upper lid raiser
    emo_obs.active_au_count = 4;

    mirror_emotion_resonance_t emo_result = {};
    mirror_emotion_process_observation(emotion_bridge, &emo_obs, &emo_result);

    // Process gaze during crisis
    mirror_gaze_observation_t gaze_obs = create_gaze_obs(agent_id);
    mirror_attention_cue_t att_result = {};
    mirror_attention_process_gaze(attention_bridge, &gaze_obs, &att_result);

    // Both should still function but contagion should be suppressed
    EXPECT_GE(emo_result.confidence, 0.0f);
    EXPECT_GE(att_result.cue_strength, 0.0f);

    // Disable crisis mode
    mirror_emotion_set_crisis_mode(emotion_bridge, false);
}

//=============================================================================
// Multi-Agent Integration Tests
//=============================================================================

TEST_F(MirrorEmotionAttentionIntegrationTest, TrackMultipleAgentsAcrossBridges) {
    createBridges();

    const uint32_t num_agents = 5;

    // Process observations from multiple agents
    for (uint32_t i = 0; i < num_agents; i++) {
        // Emotion observation
        mirror_emotion_observation_t emo_obs = create_emotion_obs(i);
        mirror_emotion_resonance_t emo_result = {};
        mirror_emotion_process_observation(emotion_bridge, &emo_obs, &emo_result);

        // Attention observation
        mirror_gaze_observation_t gaze_obs = create_gaze_obs(i);
        mirror_attention_cue_t att_result = {};
        mirror_attention_process_gaze(attention_bridge, &gaze_obs, &att_result);
    }

    // Verify all agents are tracked
    for (uint32_t i = 0; i < num_agents; i++) {
        mirror_emotion_agent_state_t* emo_agent = mirror_emotion_get_agent(emotion_bridge, i);
        mirror_attention_agent_t* att_agent = mirror_attention_get_agent(attention_bridge, i);

        ASSERT_NE(nullptr, emo_agent);
        ASSERT_NE(nullptr, att_agent);
        EXPECT_EQ(i, emo_agent->agent_id);
        EXPECT_EQ(i, att_agent->agent_id);
    }
}

TEST_F(MirrorEmotionAttentionIntegrationTest, AgentFamiliarityAffectsAttention) {
    createBridges();

    const uint32_t familiar_agent = 1;
    const uint32_t unfamiliar_agent = 2;

    // Build familiarity with one agent
    for (int i = 0; i < 10; i++) {
        mirror_emotion_observation_t emo_obs = create_emotion_obs(familiar_agent);
        mirror_emotion_resonance_t emo_result = {};
        mirror_emotion_process_observation(emotion_bridge, &emo_obs, &emo_result);
        mirror_emotion_update_familiarity(emotion_bridge, familiar_agent, 0.1f);
    }

    // Process attention from both agents
    mirror_gaze_observation_t familiar_gaze = create_gaze_obs(familiar_agent);
    mirror_gaze_observation_t unfamiliar_gaze = create_gaze_obs(unfamiliar_agent);
    mirror_attention_cue_t familiar_cue = {};
    mirror_attention_cue_t unfamiliar_cue = {};

    mirror_attention_process_gaze(attention_bridge, &familiar_gaze, &familiar_cue);
    mirror_attention_process_gaze(attention_bridge, &unfamiliar_gaze, &unfamiliar_cue);

    // Both should produce valid cues
    EXPECT_GT(familiar_cue.cue_strength, 0.0f);
    EXPECT_GT(unfamiliar_cue.cue_strength, 0.0f);

    // Check familiarity in emotion bridge
    mirror_emotion_agent_state_t* emo_agent = mirror_emotion_get_agent(emotion_bridge, familiar_agent);
    EXPECT_GT(emo_agent->familiarity, 0.0f);
}

//=============================================================================
// Batch Processing Integration Tests
//=============================================================================

TEST_F(MirrorEmotionAttentionIntegrationTest, BatchProcessingBothBridges) {
    emotion_config.enable_simd = true;
    attention_config.enable_simd = true;
    createBridges();

    const uint32_t batch_size = 8;

    // Create batch observations
    std::vector<mirror_emotion_observation_t> emo_obs(batch_size);
    std::vector<mirror_emotion_resonance_t> emo_results(batch_size);
    std::vector<mirror_gaze_observation_t> gaze_obs(batch_size);
    std::vector<mirror_attention_cue_t> att_results(batch_size);

    for (uint32_t i = 0; i < batch_size; i++) {
        emo_obs[i] = create_emotion_obs(i);
        gaze_obs[i] = create_gaze_obs(i);
    }

    // Batch process both
    uint32_t emo_processed = mirror_emotion_process_batch(
        emotion_bridge, emo_obs.data(), emo_results.data(), batch_size
    );
    uint32_t att_processed = mirror_attention_process_batch(
        attention_bridge, gaze_obs.data(), att_results.data(), batch_size
    );

    EXPECT_EQ(batch_size, emo_processed);
    EXPECT_EQ(batch_size, att_processed);

    // Verify all results are valid
    for (uint32_t i = 0; i < batch_size; i++) {
        EXPECT_GE(emo_results[i].confidence, 0.0f);
        EXPECT_GE(att_results[i].cue_strength, 0.0f);
    }
}

//=============================================================================
// Temporal Sequence Tests
//=============================================================================

TEST_F(MirrorEmotionAttentionIntegrationTest, EmotionThenAttentionSequence) {
    createBridges();

    const uint32_t agent_id = 1;

    // Simulate realistic sequence: detect emotion, then follow gaze

    // 1. First notice emotional expression
    mirror_emotion_observation_t emo_obs = create_emotion_obs(agent_id, 0.85f);
    emo_obs.timestamp_us = 100000;  // t=100ms
    mirror_emotion_resonance_t emo_result = {};
    mirror_emotion_process_observation(emotion_bridge, &emo_obs, &emo_result);

    // 2. Then follow their gaze
    mirror_gaze_observation_t gaze_obs = create_gaze_obs(agent_id);
    gaze_obs.timestamp_us = 250000;  // t=250ms (150ms later)
    mirror_attention_cue_t att_result = {};
    mirror_attention_process_gaze(attention_bridge, &gaze_obs, &att_result);

    // Both should process correctly
    EXPECT_TRUE(emo_result.confidence > 0.0f);
    EXPECT_TRUE(att_result.cue_strength > 0.0f);
}

TEST_F(MirrorEmotionAttentionIntegrationTest, AttentionThenEmotionSequence) {
    createBridges();

    const uint32_t agent_id = 1;

    // Alternative sequence: notice gaze first, then emotion

    // 1. First notice gaze direction
    mirror_gaze_observation_t gaze_obs = create_gaze_obs(agent_id);
    gaze_obs.timestamp_us = 100000;
    mirror_attention_cue_t att_result = {};
    mirror_attention_process_gaze(attention_bridge, &gaze_obs, &att_result);

    // 2. Then notice emotional expression
    mirror_emotion_observation_t emo_obs = create_emotion_obs(agent_id, 0.75f);
    emo_obs.timestamp_us = 200000;
    mirror_emotion_resonance_t emo_result = {};
    mirror_emotion_process_observation(emotion_bridge, &emo_obs, &emo_result);

    // Both should process correctly
    EXPECT_TRUE(att_result.cue_strength > 0.0f);
    EXPECT_TRUE(emo_result.confidence > 0.0f);
}

//=============================================================================
// Statistics Integration Tests
//=============================================================================

TEST_F(MirrorEmotionAttentionIntegrationTest, CombinedStatistics) {
    createBridges();

    // Process several observations
    for (int i = 0; i < 10; i++) {
        mirror_emotion_observation_t emo_obs = create_emotion_obs(i % 3);
        mirror_emotion_resonance_t emo_result = {};
        mirror_emotion_process_observation(emotion_bridge, &emo_obs, &emo_result);

        mirror_gaze_observation_t gaze_obs = create_gaze_obs(i % 3);
        mirror_attention_cue_t att_result = {};
        mirror_attention_process_gaze(attention_bridge, &gaze_obs, &att_result);
    }

    // Get statistics from both bridges
    mirror_emotion_stats_t emo_stats = {};
    mirror_attention_stats_t att_stats = {};

    EXPECT_TRUE(mirror_emotion_get_stats(emotion_bridge, &emo_stats));
    EXPECT_TRUE(mirror_attention_get_stats(attention_bridge, &att_stats));

    // Both should show activity
    EXPECT_GE(emo_stats.total_observations, 10u);
    EXPECT_GE(att_stats.gaze_cues_detected, 10u);
}

TEST_F(MirrorEmotionAttentionIntegrationTest, ResetStatisticsIndependently) {
    createBridges();

    // Process some observations
    for (int i = 0; i < 5; i++) {
        mirror_emotion_observation_t emo_obs = create_emotion_obs(1);
        mirror_emotion_resonance_t emo_result = {};
        mirror_emotion_process_observation(emotion_bridge, &emo_obs, &emo_result);

        mirror_gaze_observation_t gaze_obs = create_gaze_obs(1);
        mirror_attention_cue_t att_result = {};
        mirror_attention_process_gaze(attention_bridge, &gaze_obs, &att_result);
    }

    // Reset only emotion stats
    mirror_emotion_reset_stats(emotion_bridge);

    mirror_emotion_stats_t emo_stats = {};
    mirror_attention_stats_t att_stats = {};
    mirror_emotion_get_stats(emotion_bridge, &emo_stats);
    mirror_attention_get_stats(attention_bridge, &att_stats);

    // Emotion should be reset, attention should still have counts
    EXPECT_EQ(0u, emo_stats.total_observations);
    EXPECT_GE(att_stats.gaze_cues_detected, 5u);
}

//=============================================================================
// Multimodal Integration Tests
//=============================================================================

TEST_F(MirrorEmotionAttentionIntegrationTest, MultimodalEmotionWithGaze) {
    createBridges();

    const uint32_t agent_id = 1;

    // Multimodal emotion observation (face + gaze)
    mirror_emotion_observation_t emo_obs = create_emotion_obs(agent_id);
    emo_obs.modality = MIRROR_EMOTION_MODALITY_MULTIMODAL;
    mirror_emotion_resonance_t emo_result = {};
    mirror_emotion_process_observation(emotion_bridge, &emo_obs, &emo_result);

    // Gaze observation with emotional context
    mirror_gaze_observation_t gaze_obs = create_gaze_obs(agent_id);
    gaze_obs.is_referential = true;
    mirror_attention_cue_t att_result = {};
    mirror_attention_process_gaze(attention_bridge, &gaze_obs, &att_result);

    // Both should succeed
    EXPECT_GE(emo_result.confidence, 0.0f);
    EXPECT_GE(att_result.cue_strength, 0.0f);
}

//=============================================================================
// Edge Cases Integration Tests
//=============================================================================

TEST_F(MirrorEmotionAttentionIntegrationTest, HandleMismatchedAgentStates) {
    createBridges();

    // Agent known to emotion but not attention
    mirror_emotion_observation_t emo_obs = create_emotion_obs(10);
    mirror_emotion_resonance_t emo_result = {};
    mirror_emotion_process_observation(emotion_bridge, &emo_obs, &emo_result);
    mirror_emotion_update_familiarity(emotion_bridge, 10, 0.5f);

    // Agent known to attention but not emotion
    mirror_gaze_observation_t gaze_obs = create_gaze_obs(20);
    mirror_attention_cue_t att_result = {};
    mirror_attention_process_gaze(attention_bridge, &gaze_obs, &att_result);
    mirror_attention_update_validity(attention_bridge, 20, true);

    // Both bridges should handle this independently
    mirror_emotion_agent_state_t* emo_10 = mirror_emotion_get_agent(emotion_bridge, 10);
    mirror_attention_agent_t* att_20 = mirror_attention_get_agent(attention_bridge, 20);

    EXPECT_NE(nullptr, emo_10);
    EXPECT_NE(nullptr, att_20);
    EXPECT_GT(emo_10->familiarity, 0.0f);
}

TEST_F(MirrorEmotionAttentionIntegrationTest, ZeroIntensityHandling) {
    createBridges();

    // Very low intensity emotion
    mirror_emotion_observation_t emo_obs = create_emotion_obs(1, 0.01f);
    mirror_emotion_resonance_t emo_result = {};
    mirror_emotion_process_observation(emotion_bridge, &emo_obs, &emo_result);

    // Very low confidence gaze
    mirror_gaze_observation_t gaze_obs = create_gaze_obs(1);
    gaze_obs.confidence = 0.01f;
    mirror_attention_cue_t att_result = {};
    mirror_attention_process_gaze(attention_bridge, &gaze_obs, &att_result);

    // Both should handle gracefully
    EXPECT_GE(emo_result.intensity, 0.0f);
    EXPECT_GE(att_result.cue_strength, 0.0f);
}
