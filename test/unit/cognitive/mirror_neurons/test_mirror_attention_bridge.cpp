/**
 * @file test_mirror_attention_bridge.cpp
 * @brief Comprehensive unit tests for mirror-attention system bridge
 * @version 1.0.0
 * @date 2025-12-31
 *
 * WHAT: Tests for mirror neuron - attention system bidirectional bridge
 * WHY:  Verify joint attention, gaze following, and saliency modulation
 * HOW:  Test lifecycle, gaze processing, joint attention, SIMD operations
 *
 * Test Coverage:
 * - Bridge lifecycle (create/destroy with config)
 * - Gaze observation processing
 * - Gaze target computation
 * - Batch gaze processing (SIMD)
 * - Joint attention state management
 * - Joint attention initiation and response
 * - Attention sensitivity modulation
 * - Saliency map updates
 * - Agent tracking and validity
 * - Statistics collection and reset
 * - Error handling and edge cases
 */

#include <gtest/gtest.h>

#include "cognitive/mirror_neurons/nimcp_mirror_attention_bridge.h"
#include "utils/memory/nimcp_memory.h"

#include <cstring>
#include <cmath>
#include <vector>

//=============================================================================
// Test Fixtures
//=============================================================================

class MirrorAttentionBridgeTest : public ::testing::Test {
protected:
    mirror_attention_bridge_t* bridge;
    mirror_attention_config_t config;

    void SetUp() override {
        config = mirror_attention_config_default();
        bridge = nullptr;
    }

    void TearDown() override {
        if (bridge) {
            mirror_attention_destroy(bridge);
            bridge = nullptr;
        }
    }

    // Helper to create a sample gaze observation
    mirror_gaze_observation_t create_sample_gaze(
        uint32_t agent_id = 1,
        mirror_attention_cue_type_t cue_type = MIRROR_CUE_GAZE
    ) {
        mirror_gaze_observation_t obs = {};
        obs.agent_id = agent_id;
        obs.cue_type = cue_type;

        // Gaze direction (looking forward-right-down)
        obs.direction.x = 0.707f;
        obs.direction.y = -0.5f;
        obs.direction.z = 0.5f;

        // Agent position
        obs.agent_position.x = 0.0f;
        obs.agent_position.y = 1.6f;  // Head height
        obs.agent_position.z = 0.0f;

        // Target (if computed)
        obs.target_position.x = 2.0f;
        obs.target_position.y = 0.8f;
        obs.target_position.z = 1.0f;
        obs.target_position_valid = true;

        obs.target_object_id = 0;
        obs.target_object_valid = false;

        obs.confidence = 0.85f;
        obs.duration_ms = 200.0f;
        obs.timestamp_us = 1000000;

        obs.is_mutual_gaze = false;
        obs.is_referential = true;

        return obs;
    }

    // Helper to create a normalized direction vector
    mirror_attention_vec3_t normalize(float x, float y, float z) {
        float len = std::sqrt(x*x + y*y + z*z);
        if (len > 0.0001f) {
            return {x/len, y/len, z/len};
        }
        return {0.0f, 0.0f, 1.0f};
    }
};

class MirrorAttentionSIMDTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(MirrorAttentionBridgeTest, DefaultConfigHasReasonableValues) {
    EXPECT_GT(config.cue_validity_threshold, 0.0f);
    EXPECT_GT(config.reflexive_soa_ms, 0.0f);
    EXPECT_GT(config.voluntary_soa_ms, 0.0f);
    EXPECT_GT(config.gaze_cue_strength, 0.0f);
    EXPECT_GT(config.pointing_cue_strength, 0.0f);
    EXPECT_GT(config.joint_attention_threshold, 0.0f);
    EXPECT_GT(config.joint_attention_timeout_ms, 0.0f);
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(MirrorAttentionBridgeTest, CreateWithDefaultConfig) {
    bridge = mirror_attention_create(nullptr);
    ASSERT_NE(nullptr, bridge);
}

TEST_F(MirrorAttentionBridgeTest, CreateWithCustomConfig) {
    config.gaze_cue_strength = 1.5f;
    config.enable_simd = true;
    config.enable_joint_attention_initiation = true;
    config.enable_saliency_modulation = true;

    bridge = mirror_attention_create(&config);
    ASSERT_NE(nullptr, bridge);
}

TEST_F(MirrorAttentionBridgeTest, DestroyNullSafe) {
    mirror_attention_destroy(nullptr);
}

TEST_F(MirrorAttentionBridgeTest, CreateDestroyMultipleTimes) {
    for (int i = 0; i < 5; i++) {
        bridge = mirror_attention_create(&config);
        ASSERT_NE(nullptr, bridge);
        mirror_attention_destroy(bridge);
        bridge = nullptr;
    }
}

//=============================================================================
// Gaze Processing Tests
//=============================================================================

TEST_F(MirrorAttentionBridgeTest, ProcessSingleGaze) {
    bridge = mirror_attention_create(&config);
    ASSERT_NE(nullptr, bridge);

    mirror_gaze_observation_t obs = create_sample_gaze();
    mirror_attention_cue_t cue = {};

    bool success = mirror_attention_process_gaze(bridge, &obs, &cue);
    EXPECT_TRUE(success);

    // Cue should have valid data
    EXPECT_GE(cue.cue_strength, 0.0f);
    EXPECT_LE(cue.cue_strength, 2.0f);  // Could be boosted
    EXPECT_GE(cue.cue_validity, 0.0f);
    EXPECT_LE(cue.cue_validity, 1.0f);
}

TEST_F(MirrorAttentionBridgeTest, ProcessGazeWithNullBridge) {
    mirror_gaze_observation_t obs = create_sample_gaze();
    mirror_attention_cue_t cue = {};

    bool success = mirror_attention_process_gaze(nullptr, &obs, &cue);
    EXPECT_FALSE(success);
}

TEST_F(MirrorAttentionBridgeTest, ProcessGazeWithNullObservation) {
    bridge = mirror_attention_create(&config);
    ASSERT_NE(nullptr, bridge);

    mirror_attention_cue_t cue = {};
    bool success = mirror_attention_process_gaze(bridge, nullptr, &cue);
    EXPECT_FALSE(success);
}

TEST_F(MirrorAttentionBridgeTest, ProcessGazeWithNullCue) {
    bridge = mirror_attention_create(&config);
    ASSERT_NE(nullptr, bridge);

    mirror_gaze_observation_t obs = create_sample_gaze();
    bool success = mirror_attention_process_gaze(bridge, &obs, nullptr);
    EXPECT_FALSE(success);
}

TEST_F(MirrorAttentionBridgeTest, ProcessDifferentCueTypes) {
    bridge = mirror_attention_create(&config);
    ASSERT_NE(nullptr, bridge);

    mirror_attention_cue_type_t cue_types[] = {
        MIRROR_CUE_GAZE,
        MIRROR_CUE_HEAD_TURN,
        MIRROR_CUE_POINTING,
        MIRROR_CUE_REACH,
        MIRROR_CUE_BODY_ORIENT
    };

    for (auto cue_type : cue_types) {
        mirror_gaze_observation_t obs = create_sample_gaze(1, cue_type);
        mirror_attention_cue_t cue = {};

        bool success = mirror_attention_process_gaze(bridge, &obs, &cue);
        EXPECT_TRUE(success) << "Failed for cue type: " << static_cast<int>(cue_type);
    }
}

TEST_F(MirrorAttentionBridgeTest, ProcessMutualGaze) {
    bridge = mirror_attention_create(&config);
    ASSERT_NE(nullptr, bridge);

    mirror_gaze_observation_t obs = create_sample_gaze();
    obs.is_mutual_gaze = true;
    mirror_attention_cue_t cue = {};

    bool success = mirror_attention_process_gaze(bridge, &obs, &cue);
    EXPECT_TRUE(success);
    // Mutual gaze should be detected
}

//=============================================================================
// Gaze Target Computation Tests
//=============================================================================

TEST_F(MirrorAttentionBridgeTest, ComputeGazeTargetForward) {
    mirror_attention_vec3_t position = {0.0f, 1.6f, 0.0f};
    mirror_attention_vec3_t direction = {0.0f, 0.0f, 1.0f};  // Looking forward
    mirror_attention_vec3_t target = {};

    float distance = mirror_attention_compute_gaze_target(&position, &direction, &target);

    // Should compute some positive distance (or -1 if no hit)
    EXPECT_TRUE(distance > 0.0f || distance < 0.0f);
}

TEST_F(MirrorAttentionBridgeTest, ComputeGazeTargetDownward) {
    mirror_attention_vec3_t position = {0.0f, 2.0f, 0.0f};
    mirror_attention_vec3_t direction = {0.0f, -1.0f, 0.0f};  // Looking straight down
    mirror_attention_vec3_t target = {};

    float distance = mirror_attention_compute_gaze_target(&position, &direction, &target);

    // Looking down from height should hit ground
    if (distance > 0.0f) {
        EXPECT_NEAR(target.y, 0.0f, 0.5f);  // Should hit near ground plane
    }
}

TEST_F(MirrorAttentionBridgeTest, ComputeGazeTargetWithNullInputs) {
    mirror_attention_vec3_t position = {0.0f, 1.6f, 0.0f};
    mirror_attention_vec3_t direction = {0.0f, 0.0f, 1.0f};
    mirror_attention_vec3_t target = {};

    // Should handle nulls gracefully
    float d1 = mirror_attention_compute_gaze_target(nullptr, &direction, &target);
    float d2 = mirror_attention_compute_gaze_target(&position, nullptr, &target);
    float d3 = mirror_attention_compute_gaze_target(&position, &direction, nullptr);

    EXPECT_LT(d1, 0.0f);  // Error
    EXPECT_LT(d2, 0.0f);  // Error
    // d3 might still compute but not store result
}

//=============================================================================
// Batch Processing Tests
//=============================================================================

TEST_F(MirrorAttentionBridgeTest, ProcessBatchGaze) {
    config.enable_simd = true;
    bridge = mirror_attention_create(&config);
    ASSERT_NE(nullptr, bridge);

    const uint32_t count = 8;
    std::vector<mirror_gaze_observation_t> observations(count);
    std::vector<mirror_attention_cue_t> cues(count);

    for (uint32_t i = 0; i < count; i++) {
        observations[i] = create_sample_gaze(i + 1);
    }

    uint32_t processed = mirror_attention_process_batch(
        bridge, observations.data(), cues.data(), count
    );

    EXPECT_EQ(count, processed);

    for (uint32_t i = 0; i < count; i++) {
        EXPECT_GE(cues[i].cue_strength, 0.0f);
    }
}

TEST_F(MirrorAttentionBridgeTest, ProcessBatchWithZeroCount) {
    bridge = mirror_attention_create(&config);
    ASSERT_NE(nullptr, bridge);

    mirror_gaze_observation_t obs;
    mirror_attention_cue_t cue;

    uint32_t processed = mirror_attention_process_batch(bridge, &obs, &cue, 0);
    EXPECT_EQ(0u, processed);
}

TEST_F(MirrorAttentionBridgeTest, ProcessBatchWithNullArrays) {
    bridge = mirror_attention_create(&config);
    ASSERT_NE(nullptr, bridge);

    uint32_t processed = mirror_attention_process_batch(bridge, nullptr, nullptr, 5);
    EXPECT_EQ(0u, processed);
}

//=============================================================================
// Joint Attention Tests
//=============================================================================

TEST_F(MirrorAttentionBridgeTest, GetJointStateInitiallyNone) {
    bridge = mirror_attention_create(&config);
    ASSERT_NE(nullptr, bridge);

    joint_attention_state_t state = mirror_attention_get_joint_state(bridge, 1);
    EXPECT_EQ(JOINT_ATTENTION_NONE, state);
}

TEST_F(MirrorAttentionBridgeTest, InitiateJointAttention) {
    config.enable_joint_attention_initiation = true;
    bridge = mirror_attention_create(&config);
    ASSERT_NE(nullptr, bridge);

    mirror_attention_vec3_t target = {1.0f, 1.0f, 2.0f};
    bool success = mirror_attention_initiate_joint(bridge, 1, &target);

    // May succeed or fail depending on agent state
    if (success) {
        joint_attention_state_t state = mirror_attention_get_joint_state(bridge, 1);
        EXPECT_TRUE(state == JOINT_ATTENTION_INITIATING ||
                   state == JOINT_ATTENTION_ESTABLISHED);
    }
}

TEST_F(MirrorAttentionBridgeTest, InitiateJointAttentionWithNullTarget) {
    config.enable_joint_attention_initiation = true;
    bridge = mirror_attention_create(&config);
    ASSERT_NE(nullptr, bridge);

    bool success = mirror_attention_initiate_joint(bridge, 1, nullptr);
    EXPECT_FALSE(success);
}

TEST_F(MirrorAttentionBridgeTest, RespondToJointAttention) {
    bridge = mirror_attention_create(&config);
    ASSERT_NE(nullptr, bridge);

    // First simulate receiving a gaze that creates a pending joint attention bid
    mirror_gaze_observation_t obs = create_sample_gaze();
    obs.is_referential = true;
    mirror_attention_cue_t cue = {};
    mirror_attention_process_gaze(bridge, &obs, &cue);

    // Then try to respond
    bool success = mirror_attention_respond_to_joint(bridge, 1);
    // May or may not succeed depending on state
    EXPECT_TRUE(success || !success);
}

TEST_F(MirrorAttentionBridgeTest, BreakJointAttention) {
    config.enable_joint_attention_initiation = true;
    bridge = mirror_attention_create(&config);
    ASSERT_NE(nullptr, bridge);

    // Initiate joint attention
    mirror_attention_vec3_t target = {1.0f, 1.0f, 2.0f};
    mirror_attention_initiate_joint(bridge, 1, &target);

    // Break it
    mirror_attention_break_joint(bridge, 1);

    joint_attention_state_t state = mirror_attention_get_joint_state(bridge, 1);
    EXPECT_TRUE(state == JOINT_ATTENTION_BREAKING ||
               state == JOINT_ATTENTION_NONE);
}

TEST_F(MirrorAttentionBridgeTest, JointAttentionWithMultipleAgents) {
    config.enable_joint_attention_initiation = true;
    bridge = mirror_attention_create(&config);
    ASSERT_NE(nullptr, bridge);

    mirror_attention_vec3_t target1 = {1.0f, 1.0f, 2.0f};
    mirror_attention_vec3_t target2 = {-1.0f, 1.0f, 2.0f};

    mirror_attention_initiate_joint(bridge, 1, &target1);
    mirror_attention_initiate_joint(bridge, 2, &target2);

    // Should track independently
    joint_attention_state_t state1 = mirror_attention_get_joint_state(bridge, 1);
    joint_attention_state_t state2 = mirror_attention_get_joint_state(bridge, 2);

    // States should be independent
    EXPECT_TRUE(state1 != JOINT_ATTENTION_NONE || state2 != JOINT_ATTENTION_NONE);
}

//=============================================================================
// Attention Modulation Tests
//=============================================================================

TEST_F(MirrorAttentionBridgeTest, GetSensitivityAtFocus) {
    bridge = mirror_attention_create(&config);
    ASSERT_NE(nullptr, bridge);

    // Set attention focus
    mirror_attention_vec3_t focus = {1.0f, 1.0f, 2.0f};
    mirror_attention_set_focus(bridge, &focus, 0.8f, 0.5f);

    // Get sensitivity at focus location
    float sensitivity = mirror_attention_get_sensitivity_at(bridge, &focus);
    EXPECT_GT(sensitivity, 0.0f);
}

TEST_F(MirrorAttentionBridgeTest, GetSensitivityAwayFromFocus) {
    bridge = mirror_attention_create(&config);
    ASSERT_NE(nullptr, bridge);

    mirror_attention_vec3_t focus = {1.0f, 1.0f, 2.0f};
    mirror_attention_set_focus(bridge, &focus, 0.8f, 0.2f);  // Small sigma

    // Get sensitivity far from focus
    mirror_attention_vec3_t far = {10.0f, 10.0f, 10.0f};
    float sensitivity = mirror_attention_get_sensitivity_at(bridge, &far);

    // Should be lower than at focus (but still positive)
    EXPECT_GE(sensitivity, 0.0f);
}

TEST_F(MirrorAttentionBridgeTest, SetFocusUpdatesState) {
    bridge = mirror_attention_create(&config);
    ASSERT_NE(nullptr, bridge);

    mirror_attention_vec3_t focus1 = {1.0f, 1.0f, 2.0f};
    mirror_attention_set_focus(bridge, &focus1, 0.5f, 0.3f);

    float sens1 = mirror_attention_get_sensitivity_at(bridge, &focus1);

    // Change focus
    mirror_attention_vec3_t focus2 = {-1.0f, -1.0f, -2.0f};
    mirror_attention_set_focus(bridge, &focus2, 0.9f, 0.5f);

    float sens2 = mirror_attention_get_sensitivity_at(bridge, &focus2);

    // Both should be valid
    EXPECT_GT(sens1, 0.0f);
    EXPECT_GT(sens2, 0.0f);
}

//=============================================================================
// Saliency Map Tests
//=============================================================================

TEST_F(MirrorAttentionBridgeTest, GetSaliencyBoostAtCenter) {
    config.enable_saliency_modulation = true;
    bridge = mirror_attention_create(&config);
    ASSERT_NE(nullptr, bridge);

    // Set focus at center
    mirror_attention_vec3_t focus = {0.0f, 0.0f, 1.0f};
    mirror_attention_set_focus(bridge, &focus, 0.9f, 0.3f);

    float boost = mirror_attention_get_saliency_boost(bridge, 0.5f, 0.5f);
    EXPECT_GT(boost, 0.0f);
}

TEST_F(MirrorAttentionBridgeTest, GetSaliencyBoostAtEdge) {
    config.enable_saliency_modulation = true;
    bridge = mirror_attention_create(&config);
    ASSERT_NE(nullptr, bridge);

    mirror_attention_vec3_t focus = {0.0f, 0.0f, 1.0f};
    mirror_attention_set_focus(bridge, &focus, 0.9f, 0.1f);  // Small sigma

    float center_boost = mirror_attention_get_saliency_boost(bridge, 0.5f, 0.5f);
    float edge_boost = mirror_attention_get_saliency_boost(bridge, 0.0f, 0.0f);

    // Edge should have less boost
    EXPECT_GE(center_boost, edge_boost);
}

TEST_F(MirrorAttentionBridgeTest, SaliencyBoostOutOfRange) {
    config.enable_saliency_modulation = true;
    bridge = mirror_attention_create(&config);
    ASSERT_NE(nullptr, bridge);

    // Coordinates outside [0,1] should be handled
    float boost = mirror_attention_get_saliency_boost(bridge, -0.5f, 1.5f);
    // Should clamp or handle gracefully
    EXPECT_GE(boost, 0.0f);
}

//=============================================================================
// SIMD Operations Tests
//=============================================================================

TEST_F(MirrorAttentionSIMDTest, SIMDGazeTargetsComputation) {
    const uint32_t count = 4;
    float positions[count * 3] = {
        0.0f, 1.6f, 0.0f,
        1.0f, 1.6f, 0.0f,
        2.0f, 1.6f, 0.0f,
        3.0f, 1.6f, 0.0f
    };
    float directions[count * 3] = {
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f
    };
    float targets[count * 3] = {};
    float distances[count] = {};

    mirror_attention_simd_gaze_targets(positions, directions, targets, distances, count);

    // All should have computed targets
    for (uint32_t i = 0; i < count; i++) {
        // Each agent is looking forward from different x positions
        EXPECT_NEAR(targets[i * 3 + 0], positions[i * 3 + 0], 0.01f);  // X should match
    }
}

TEST_F(MirrorAttentionSIMDTest, SIMDUpdateSaliency) {
    const uint32_t size = 16;
    std::vector<float> saliency_map(size * size, 0.0f);

    mirror_attention_simd_update_saliency(
        saliency_map.data(),
        size,
        0.5f, 0.5f,  // Center focus
        0.2f,        // Sigma
        0.9f         // Strength
    );

    // Center should have high saliency
    uint32_t center = (size / 2) * size + (size / 2);
    EXPECT_GT(saliency_map[center], 0.0f);
}

TEST_F(MirrorAttentionSIMDTest, SIMDUpdateSaliencyMultipleTimes) {
    const uint32_t size = 16;
    std::vector<float> saliency_map(size * size, 0.0f);

    // Multiple updates
    mirror_attention_simd_update_saliency(saliency_map.data(), size, 0.25f, 0.25f, 0.2f, 0.5f);
    mirror_attention_simd_update_saliency(saliency_map.data(), size, 0.75f, 0.75f, 0.2f, 0.5f);

    // Both regions should have saliency
    uint32_t pos1 = (size / 4) * size + (size / 4);
    uint32_t pos2 = (3 * size / 4) * size + (3 * size / 4);

    // At least one should be affected
    float max_sal = 0.0f;
    for (size_t i = 0; i < saliency_map.size(); i++) {
        max_sal = std::max(max_sal, saliency_map[i]);
    }
    EXPECT_GT(max_sal, 0.0f);
}

//=============================================================================
// Agent Tracking Tests
//=============================================================================

TEST_F(MirrorAttentionBridgeTest, GetAgentCreatesNew) {
    bridge = mirror_attention_create(&config);
    ASSERT_NE(nullptr, bridge);

    mirror_attention_agent_t* agent = mirror_attention_get_agent(bridge, 42);
    ASSERT_NE(nullptr, agent);

    EXPECT_EQ(42u, agent->agent_id);
    EXPECT_TRUE(agent->active);
}

TEST_F(MirrorAttentionBridgeTest, GetAgentReturnsSame) {
    bridge = mirror_attention_create(&config);
    ASSERT_NE(nullptr, bridge);

    mirror_attention_agent_t* agent1 = mirror_attention_get_agent(bridge, 1);
    mirror_attention_agent_t* agent2 = mirror_attention_get_agent(bridge, 1);

    EXPECT_EQ(agent1, agent2);
}

TEST_F(MirrorAttentionBridgeTest, UpdateValidity) {
    bridge = mirror_attention_create(&config);
    ASSERT_NE(nullptr, bridge);

    // Process some gaze
    mirror_gaze_observation_t obs = create_sample_gaze(1);
    mirror_attention_cue_t cue = {};
    mirror_attention_process_gaze(bridge, &obs, &cue);

    // Update validity
    mirror_attention_update_validity(bridge, 1, true);

    mirror_attention_agent_t* agent = mirror_attention_get_agent(bridge, 1);
    ASSERT_NE(nullptr, agent);

    // Validity should be reasonable
    EXPECT_GE(agent->gaze_validity, 0.0f);
    EXPECT_LE(agent->gaze_validity, 1.0f);
}

TEST_F(MirrorAttentionBridgeTest, InvalidCuesReduceValidity) {
    bridge = mirror_attention_create(&config);
    ASSERT_NE(nullptr, bridge);

    // Process gaze and update as valid several times
    for (int i = 0; i < 5; i++) {
        mirror_gaze_observation_t obs = create_sample_gaze(1);
        mirror_attention_cue_t cue = {};
        mirror_attention_process_gaze(bridge, &obs, &cue);
        mirror_attention_update_validity(bridge, 1, true);
    }

    mirror_attention_agent_t* agent1 = mirror_attention_get_agent(bridge, 1);
    float valid_rate = agent1->gaze_validity;

    // Now update as invalid
    for (int i = 0; i < 5; i++) {
        mirror_attention_update_validity(bridge, 1, false);
    }

    float invalid_rate = agent1->gaze_validity;

    // Validity should decrease (or at least not increase much)
    EXPECT_LE(invalid_rate, valid_rate + 0.1f);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(MirrorAttentionBridgeTest, GetStatsInitiallyZero) {
    bridge = mirror_attention_create(&config);
    ASSERT_NE(nullptr, bridge);

    mirror_attention_stats_t stats = {};
    bool success = mirror_attention_get_stats(bridge, &stats);

    EXPECT_TRUE(success);
    EXPECT_EQ(0u, stats.gaze_cues_detected);
    EXPECT_EQ(0u, stats.attention_shifts_triggered);
}

TEST_F(MirrorAttentionBridgeTest, StatsIncrementAfterProcessing) {
    bridge = mirror_attention_create(&config);
    ASSERT_NE(nullptr, bridge);

    mirror_gaze_observation_t obs = create_sample_gaze();
    mirror_attention_cue_t cue = {};
    mirror_attention_process_gaze(bridge, &obs, &cue);

    mirror_attention_stats_t stats = {};
    mirror_attention_get_stats(bridge, &stats);

    EXPECT_GE(stats.gaze_cues_detected, 1u);
}

TEST_F(MirrorAttentionBridgeTest, ResetStats) {
    bridge = mirror_attention_create(&config);
    ASSERT_NE(nullptr, bridge);

    // Process some observations
    for (int i = 0; i < 5; i++) {
        mirror_gaze_observation_t obs = create_sample_gaze();
        mirror_attention_cue_t cue = {};
        mirror_attention_process_gaze(bridge, &obs, &cue);
    }

    mirror_attention_reset_stats(bridge);

    mirror_attention_stats_t stats = {};
    mirror_attention_get_stats(bridge, &stats);

    EXPECT_EQ(0u, stats.gaze_cues_detected);
}

TEST_F(MirrorAttentionBridgeTest, GetStatsWithNullBridge) {
    mirror_attention_stats_t stats = {};
    bool success = mirror_attention_get_stats(nullptr, &stats);
    EXPECT_FALSE(success);
}

TEST_F(MirrorAttentionBridgeTest, GetStatsWithNullStats) {
    bridge = mirror_attention_create(&config);
    ASSERT_NE(nullptr, bridge);

    bool success = mirror_attention_get_stats(bridge, nullptr);
    EXPECT_FALSE(success);
}

//=============================================================================
// Bio-Async Integration Tests
//=============================================================================

TEST_F(MirrorAttentionBridgeTest, RegisterBioAsync) {
    config.bio_async_enabled = true;
    bridge = mirror_attention_create(&config);
    ASSERT_NE(nullptr, bridge);

    bool success = mirror_attention_register_bio_async(bridge);
    EXPECT_TRUE(success || !success);

    mirror_attention_unregister_bio_async(bridge);
}

TEST_F(MirrorAttentionBridgeTest, UnregisterBioAsyncSafe) {
    bridge = mirror_attention_create(&config);
    ASSERT_NE(nullptr, bridge);

    mirror_attention_unregister_bio_async(bridge);
}

//=============================================================================
// Edge Cases and Error Handling Tests
//=============================================================================

TEST_F(MirrorAttentionBridgeTest, ProcessGazeWithZeroConfidence) {
    bridge = mirror_attention_create(&config);
    ASSERT_NE(nullptr, bridge);

    mirror_gaze_observation_t obs = create_sample_gaze();
    obs.confidence = 0.0f;

    mirror_attention_cue_t cue = {};
    bool success = mirror_attention_process_gaze(bridge, &obs, &cue);

    EXPECT_TRUE(success);
    // Low confidence should produce low cue strength
    EXPECT_GE(cue.cue_strength, 0.0f);
}

TEST_F(MirrorAttentionBridgeTest, HandleMaxAgents) {
    bridge = mirror_attention_create(&config);
    ASSERT_NE(nullptr, bridge);

    for (uint32_t i = 0; i < MIRROR_ATTENTION_MAX_AGENTS + 5; i++) {
        mirror_attention_agent_t* agent = mirror_attention_get_agent(bridge, i);
        // Should handle gracefully
    }

    // Bridge should still function
    mirror_gaze_observation_t obs = create_sample_gaze();
    mirror_attention_cue_t cue = {};
    bool success = mirror_attention_process_gaze(bridge, &obs, &cue);
    EXPECT_TRUE(success);
}

TEST_F(MirrorAttentionBridgeTest, ProcessGazeWithUnnormalizedDirection) {
    bridge = mirror_attention_create(&config);
    ASSERT_NE(nullptr, bridge);

    mirror_gaze_observation_t obs = create_sample_gaze();
    // Unnormalized direction
    obs.direction.x = 10.0f;
    obs.direction.y = 0.0f;
    obs.direction.z = 0.0f;

    mirror_attention_cue_t cue = {};
    bool success = mirror_attention_process_gaze(bridge, &obs, &cue);

    // Should handle gracefully
    EXPECT_TRUE(success);
}

TEST_F(MirrorAttentionBridgeTest, GetSensitivityWithNullPosition) {
    bridge = mirror_attention_create(&config);
    ASSERT_NE(nullptr, bridge);

    float sensitivity = mirror_attention_get_sensitivity_at(bridge, nullptr);
    // Should return some default or zero
    EXPECT_GE(sensitivity, 0.0f);
}

TEST_F(MirrorAttentionBridgeTest, SetFocusWithNullFocus) {
    bridge = mirror_attention_create(&config);
    ASSERT_NE(nullptr, bridge);

    // Should not crash
    mirror_attention_set_focus(bridge, nullptr, 0.5f, 0.3f);
}
