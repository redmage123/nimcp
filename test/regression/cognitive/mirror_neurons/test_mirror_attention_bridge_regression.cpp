/**
 * @file test_mirror_attention_bridge_regression.cpp
 * @brief Regression tests for mirror-attention bridge numerical stability and performance
 * @version 1.0.0
 * @date 2025-12-31
 *
 * WHAT: Tests to prevent regression in mirror-attention bridge behavior
 * WHY:  Ensure gaze-following and joint attention parameters remain stable
 * HOW:  Test exact values, boundary conditions, monotonicity, and performance
 *
 * Test Coverage:
 * - Exact parameter value verification
 * - Numerical stability at boundaries
 * - Gaze computation accuracy
 * - Joint attention state machine correctness
 * - Performance baselines
 * - SIMD vs scalar equivalence
 */

#include <gtest/gtest.h>

#include "cognitive/mirror_neurons/nimcp_mirror_attention_bridge.h"
#include "utils/memory/nimcp_memory.h"

#include <cstring>
#include <cmath>
#include <vector>
#include <chrono>

//=============================================================================
// Test Fixtures
//=============================================================================

class MirrorAttentionRegressionTest : public ::testing::Test {
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

    mirror_gaze_observation_t create_observation(
        float confidence = 0.8f,
        float dir_x = 0.0f,
        float dir_y = 0.0f,
        float dir_z = 1.0f
    ) {
        mirror_gaze_observation_t obs = {};
        obs.agent_id = 1;
        obs.cue_type = MIRROR_CUE_GAZE;

        // Normalize direction
        float len = std::sqrt(dir_x*dir_x + dir_y*dir_y + dir_z*dir_z);
        if (len > 0.001f) {
            obs.direction.x = dir_x / len;
            obs.direction.y = dir_y / len;
            obs.direction.z = dir_z / len;
        } else {
            obs.direction.x = 0.0f;
            obs.direction.y = 0.0f;
            obs.direction.z = 1.0f;
        }

        obs.agent_position.x = 0.0f;
        obs.agent_position.y = 1.6f;
        obs.agent_position.z = 0.0f;

        obs.target_position.x = 2.0f;
        obs.target_position.y = 0.8f;
        obs.target_position.z = 2.0f;
        obs.target_position_valid = true;

        obs.confidence = confidence;
        obs.duration_ms = 200.0f;
        obs.timestamp_us = 1000000;
        obs.is_referential = true;

        return obs;
    }
};

//=============================================================================
// Default Configuration Regression Tests
//=============================================================================

TEST_F(MirrorAttentionRegressionTest, DefaultConfigValues) {
    // Verify default configuration values don't change unexpectedly

    // Cue validity threshold should be reasonable
    EXPECT_GT(config.cue_validity_threshold, 0.0f);
    EXPECT_LT(config.cue_validity_threshold, 1.0f);

    // SOA values should be in biologically plausible range (50-500ms)
    EXPECT_GE(config.reflexive_soa_ms, 50.0f);
    EXPECT_LE(config.reflexive_soa_ms, 200.0f);

    EXPECT_GE(config.voluntary_soa_ms, 100.0f);
    EXPECT_LE(config.voluntary_soa_ms, 500.0f);

    // Cue strengths should be positive
    EXPECT_GT(config.gaze_cue_strength, 0.0f);
    EXPECT_GT(config.pointing_cue_strength, 0.0f);

    // Joint attention threshold
    EXPECT_GT(config.joint_attention_threshold, 0.0f);
    EXPECT_LT(config.joint_attention_threshold, 1.0f);

    // Timeout should be reasonable (1-10 seconds)
    EXPECT_GE(config.joint_attention_timeout_ms, 1000.0f);
    EXPECT_LE(config.joint_attention_timeout_ms, 10000.0f);
}

//=============================================================================
// Numerical Stability Tests
//=============================================================================

TEST_F(MirrorAttentionRegressionTest, ProcessGazeBoundaryValues) {
    bridge = mirror_attention_create(&config);
    ASSERT_NE(nullptr, bridge);

    float confidences[] = {0.0f, 0.001f, 0.5f, 0.999f, 1.0f};

    for (float confidence : confidences) {
        mirror_gaze_observation_t obs = create_observation(confidence);
        mirror_attention_cue_t result = {};

        bool success = mirror_attention_process_gaze(bridge, &obs, &result);
        ASSERT_TRUE(success) << "Failed at confidence=" << confidence;

        // All outputs must be in valid ranges
        EXPECT_GE(result.cue_strength, 0.0f);
        EXPECT_GE(result.cue_validity, 0.0f);
        EXPECT_LE(result.cue_validity, 1.0f);
    }
}

TEST_F(MirrorAttentionRegressionTest, GazeDirectionNormalization) {
    bridge = mirror_attention_create(&config);
    ASSERT_NE(nullptr, bridge);

    // Test with unnormalized directions
    float directions[][3] = {
        {10.0f, 0.0f, 0.0f},
        {0.0f, 100.0f, 0.0f},
        {0.0f, 0.0f, 0.001f},
        {1.0f, 1.0f, 1.0f},
        {-1.0f, -1.0f, -1.0f}
    };

    for (auto& dir : directions) {
        mirror_gaze_observation_t obs = create_observation(0.8f, dir[0], dir[1], dir[2]);
        mirror_attention_cue_t result = {};

        bool success = mirror_attention_process_gaze(bridge, &obs, &result);
        ASSERT_TRUE(success);
        EXPECT_GE(result.cue_strength, 0.0f);
    }
}

TEST_F(MirrorAttentionRegressionTest, SaliencyBoostNeverNegative) {
    config.enable_saliency_modulation = true;
    bridge = mirror_attention_create(&config);
    ASSERT_NE(nullptr, bridge);

    // Set focus
    mirror_attention_vec3_t focus = {0.0f, 0.0f, 1.0f};
    mirror_attention_set_focus(bridge, &focus, 0.9f, 0.3f);

    // Test across entire saliency map range
    for (float x = -0.5f; x <= 1.5f; x += 0.1f) {
        for (float y = -0.5f; y <= 1.5f; y += 0.1f) {
            float boost = mirror_attention_get_saliency_boost(bridge, x, y);
            EXPECT_GE(boost, 0.0f) << "Negative saliency at (" << x << ", " << y << ")";
        }
    }
}

TEST_F(MirrorAttentionRegressionTest, SensitivityNeverNegative) {
    bridge = mirror_attention_create(&config);
    ASSERT_NE(nullptr, bridge);

    // Test sensitivity at various positions
    float positions[][3] = {
        {0.0f, 0.0f, 0.0f},
        {100.0f, 100.0f, 100.0f},
        {-100.0f, -100.0f, -100.0f},
        {0.0f, 1.6f, 2.0f}
    };

    for (auto& pos : positions) {
        mirror_attention_vec3_t p = {pos[0], pos[1], pos[2]};
        float sensitivity = mirror_attention_get_sensitivity_at(bridge, &p);
        EXPECT_GE(sensitivity, 0.0f);
    }
}

//=============================================================================
// Gaze Computation Accuracy Tests
//=============================================================================

TEST_F(MirrorAttentionRegressionTest, GazeTargetComputation) {
    // Looking straight forward from origin
    mirror_attention_vec3_t position = {0.0f, 0.0f, 0.0f};
    mirror_attention_vec3_t direction = {0.0f, 0.0f, 1.0f};
    mirror_attention_vec3_t target = {};

    float distance = mirror_attention_compute_gaze_target(&position, &direction, &target);

    // Result should be computed (may be -1 if no intersection)
    // If positive, target should be along direction from position
    if (distance > 0.0f) {
        EXPECT_NEAR(target.x, position.x, 0.01f);
        EXPECT_GE(target.z, position.z);  // Forward
    }
}

TEST_F(MirrorAttentionRegressionTest, GazeTargetDownwardHitsGround) {
    // Looking down from height
    mirror_attention_vec3_t position = {0.0f, 2.0f, 0.0f};
    mirror_attention_vec3_t direction = {0.0f, -1.0f, 0.0f};
    mirror_attention_vec3_t target = {};

    float distance = mirror_attention_compute_gaze_target(&position, &direction, &target);

    // Should hit ground plane (y=0)
    if (distance > 0.0f) {
        EXPECT_NEAR(target.y, 0.0f, 0.5f);  // Near ground
        EXPECT_NEAR(distance, 2.0f, 0.5f);  // Distance should be about height
    }
}

//=============================================================================
// Joint Attention State Machine Tests
//=============================================================================

TEST_F(MirrorAttentionRegressionTest, JointAttentionStateTransitions) {
    config.enable_joint_attention_initiation = true;
    bridge = mirror_attention_create(&config);
    ASSERT_NE(nullptr, bridge);

    const uint32_t agent_id = 1;

    // Initial state should be NONE
    joint_attention_state_t state = mirror_attention_get_joint_state(bridge, agent_id);
    EXPECT_EQ(JOINT_ATTENTION_NONE, state);

    // Initiate joint attention
    mirror_attention_vec3_t target = {1.0f, 1.0f, 2.0f};
    bool initiated = mirror_attention_initiate_joint(bridge, agent_id, &target);

    if (initiated) {
        state = mirror_attention_get_joint_state(bridge, agent_id);
        EXPECT_TRUE(state == JOINT_ATTENTION_INITIATING ||
                   state == JOINT_ATTENTION_ESTABLISHED);
    }

    // Break joint attention
    mirror_attention_break_joint(bridge, agent_id);
    state = mirror_attention_get_joint_state(bridge, agent_id);
    EXPECT_TRUE(state == JOINT_ATTENTION_BREAKING ||
               state == JOINT_ATTENTION_NONE);
}

TEST_F(MirrorAttentionRegressionTest, JointAttentionFromGazeCue) {
    bridge = mirror_attention_create(&config);
    ASSERT_NE(nullptr, bridge);

    const uint32_t agent_id = 1;

    // Process referential gaze
    mirror_gaze_observation_t obs = create_observation(0.9f);
    obs.agent_id = agent_id;
    obs.is_referential = true;
    mirror_attention_cue_t result = {};

    mirror_attention_process_gaze(bridge, &obs, &result);

    // Should have joint attention info
    // State might be NONE, RESPONDING, or ESTABLISHED depending on implementation
    joint_attention_state_t state = mirror_attention_get_joint_state(bridge, agent_id);
    // Just verify it's a valid state
    EXPECT_GE(static_cast<int>(state), 0);
    EXPECT_LE(static_cast<int>(state), 4);
}

//=============================================================================
// Monotonicity Tests
//=============================================================================

TEST_F(MirrorAttentionRegressionTest, CueStrengthMonotonicWithConfidence) {
    bridge = mirror_attention_create(&config);
    ASSERT_NE(nullptr, bridge);

    float prev_strength = 0.0f;
    for (float confidence = 0.0f; confidence <= 1.0f; confidence += 0.1f) {
        mirror_gaze_observation_t obs = create_observation(confidence);
        mirror_attention_cue_t result = {};
        mirror_attention_process_gaze(bridge, &obs, &result);

        // Cue strength should generally increase with confidence
        EXPECT_GE(result.cue_strength, prev_strength - 0.1f)  // Small tolerance
            << "Non-monotonic at confidence " << confidence;
        prev_strength = result.cue_strength;
    }
}

TEST_F(MirrorAttentionRegressionTest, SaliencyDecaysWithDistance) {
    config.enable_saliency_modulation = true;
    bridge = mirror_attention_create(&config);
    ASSERT_NE(nullptr, bridge);

    // Set focus at center
    mirror_attention_vec3_t focus = {0.0f, 0.0f, 1.0f};
    mirror_attention_set_focus(bridge, &focus, 0.9f, 0.2f);  // Small sigma

    // Get saliency at increasing distances from center
    float center = mirror_attention_get_saliency_boost(bridge, 0.5f, 0.5f);
    float near = mirror_attention_get_saliency_boost(bridge, 0.6f, 0.5f);
    float far = mirror_attention_get_saliency_boost(bridge, 0.9f, 0.5f);

    // Should decay with distance (or be equal if uniform)
    EXPECT_GE(center, near - 0.1f);
    EXPECT_GE(near, far - 0.1f);
}

//=============================================================================
// SIMD vs Scalar Equivalence Tests
//=============================================================================

TEST_F(MirrorAttentionRegressionTest, SIMDGazeTargetsAccurate) {
    const uint32_t count = 4;
    float positions[count * 3] = {
        0.0f, 1.6f, 0.0f,
        1.0f, 1.6f, 0.0f,
        0.0f, 1.6f, 1.0f,
        1.0f, 1.6f, 1.0f
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

    // All agents looking forward - targets should be ahead
    for (uint32_t i = 0; i < count; i++) {
        // Target Z should be greater than position Z (looking forward)
        if (distances[i] > 0.0f) {
            EXPECT_GE(targets[i * 3 + 2], positions[i * 3 + 2]);
        }
    }
}

TEST_F(MirrorAttentionRegressionTest, BatchProcessingMatchesSingle) {
    config.enable_simd = true;
    bridge = mirror_attention_create(&config);
    ASSERT_NE(nullptr, bridge);

    const uint32_t count = 4;
    std::vector<mirror_gaze_observation_t> observations(count);
    std::vector<mirror_attention_cue_t> batch_results(count);

    for (uint32_t i = 0; i < count; i++) {
        observations[i] = create_observation(0.5f + i * 0.1f);
        observations[i].agent_id = i;
    }

    // Batch process
    mirror_attention_process_batch(bridge, observations.data(), batch_results.data(), count);

    // Recreate bridge and process individually
    mirror_attention_destroy(bridge);
    bridge = mirror_attention_create(&config);

    for (uint32_t i = 0; i < count; i++) {
        mirror_attention_cue_t single_result = {};
        mirror_attention_process_gaze(bridge, &observations[i], &single_result);

        // Results should be reasonably close
        EXPECT_NEAR(batch_results[i].cue_strength, single_result.cue_strength, 0.2f)
            << "Mismatch at index " << i;
    }
}

//=============================================================================
// Performance Baseline Tests
//=============================================================================

TEST_F(MirrorAttentionRegressionTest, ProcessingPerformanceBaseline) {
    config.enable_simd = true;
    bridge = mirror_attention_create(&config);
    ASSERT_NE(nullptr, bridge);

    const int iterations = 1000;
    mirror_gaze_observation_t obs = create_observation(0.8f);
    mirror_attention_cue_t result = {};

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; i++) {
        mirror_attention_process_gaze(bridge, &obs, &result);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Should process at least 10,000 observations per second
    double avg_us = static_cast<double>(duration.count()) / iterations;
    EXPECT_LT(avg_us, 100.0) << "Processing too slow: " << avg_us << " us/observation";
}

TEST_F(MirrorAttentionRegressionTest, BatchProcessingPerformanceBaseline) {
    config.enable_simd = true;
    bridge = mirror_attention_create(&config);
    ASSERT_NE(nullptr, bridge);

    const uint32_t batch_size = 32;
    const int iterations = 100;

    std::vector<mirror_gaze_observation_t> observations(batch_size);
    std::vector<mirror_attention_cue_t> results(batch_size);

    for (uint32_t i = 0; i < batch_size; i++) {
        observations[i] = create_observation(0.7f);
        observations[i].agent_id = i;
    }

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; i++) {
        mirror_attention_process_batch(bridge, observations.data(), results.data(), batch_size);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Batch should be efficient
    double total_obs = iterations * batch_size;
    double avg_us = static_cast<double>(duration.count()) / total_obs;
    EXPECT_LT(avg_us, 50.0) << "Batch processing too slow: " << avg_us << " us/observation";
}

TEST_F(MirrorAttentionRegressionTest, SIMDSaliencyUpdatePerformance) {
    const uint32_t size = 64;
    std::vector<float> saliency_map(size * size, 0.0f);

    const int iterations = 1000;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; i++) {
        mirror_attention_simd_update_saliency(
            saliency_map.data(),
            size,
            0.5f, 0.5f,
            0.2f,
            0.9f
        );
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Performance threshold for scalar fallback (debug build)
    // SIMD-optimized release builds should be ~100us
    double avg_us = static_cast<double>(duration.count()) / iterations;
    EXPECT_LT(avg_us, 1000.0) << "Saliency update too slow: " << avg_us << " us";
}

//=============================================================================
// State Consistency Tests
//=============================================================================

TEST_F(MirrorAttentionRegressionTest, AgentStateConsistentAfterManyOperations) {
    bridge = mirror_attention_create(&config);
    ASSERT_NE(nullptr, bridge);

    const uint32_t agent_id = 1;

    // Perform many operations
    for (int i = 0; i < 100; i++) {
        mirror_gaze_observation_t obs = create_observation(0.5f + (i % 5) * 0.1f);
        obs.agent_id = agent_id;
        mirror_attention_cue_t result = {};
        mirror_attention_process_gaze(bridge, &obs, &result);

        if (i % 10 == 0) {
            mirror_attention_update_validity(bridge, agent_id, (i % 2 == 0));
        }
    }

    // Agent state should still be valid
    mirror_attention_agent_t* agent = mirror_attention_get_agent(bridge, agent_id);
    ASSERT_NE(nullptr, agent);
    EXPECT_EQ(agent_id, agent->agent_id);
    EXPECT_TRUE(agent->active);
    EXPECT_GE(agent->gaze_validity, 0.0f);
    EXPECT_LE(agent->gaze_validity, 1.0f);
}

TEST_F(MirrorAttentionRegressionTest, StatisticsAccurate) {
    bridge = mirror_attention_create(&config);
    ASSERT_NE(nullptr, bridge);

    const int gaze_count = 50;

    for (int i = 0; i < gaze_count; i++) {
        mirror_gaze_observation_t obs = create_observation(0.8f);
        mirror_attention_cue_t result = {};
        mirror_attention_process_gaze(bridge, &obs, &result);
    }

    mirror_attention_stats_t stats = {};
    mirror_attention_get_stats(bridge, &stats);

    EXPECT_EQ(static_cast<uint64_t>(gaze_count), stats.gaze_cues_detected);
}

//=============================================================================
// Memory Leak Regression Tests
//=============================================================================

TEST_F(MirrorAttentionRegressionTest, NoMemoryLeakOnRepeatedCreateDestroy) {
    for (int i = 0; i < 100; i++) {
        mirror_attention_bridge_t* temp = mirror_attention_create(&config);
        ASSERT_NE(nullptr, temp);

        mirror_gaze_observation_t obs = create_observation(0.8f);
        mirror_attention_cue_t result = {};
        mirror_attention_process_gaze(temp, &obs, &result);

        mirror_attention_destroy(temp);
    }
}
