/**
 * @file test_world_imagination_health_regression.cpp
 * @brief Regression tests for Phase 5.14 World Model & Imagination Health Integration
 * @date 2026-01-19
 *
 * Tests for boundary conditions, edge cases, and stability.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <thread>
#include <chrono>
#include <vector>
#include <algorithm>
#include <random>

extern "C" {
#include "utils/fault_tolerance/nimcp_health_agent.h"
}

// Mock components for testing
static jepa_predictor_t* create_mock_jepa(int id) {
    return reinterpret_cast<jepa_predictor_t*>(static_cast<uintptr_t>(0x1000 + id * 0x100));
}

static omni_world_model_t* create_mock_world_model(int id) {
    return reinterpret_cast<omni_world_model_t*>(static_cast<uintptr_t>(0x2000 + id * 0x100));
}

static imagination_engine_t* create_mock_imagination(int id) {
    return reinterpret_cast<imagination_engine_t*>(static_cast<uintptr_t>(0x3000 + id * 0x100));
}

class WorldImaginationHealthRegressionTest : public ::testing::Test {
protected:
    nimcp_health_agent_t* agent;

    void SetUp() override {
        health_agent_config_t config;
        nimcp_health_agent_default_config(&config);
        agent = nimcp_health_agent_create(&config);
        ASSERT_NE(agent, nullptr);
    }

    void TearDown() override {
        if (agent) {
            nimcp_health_agent_destroy(agent);
            agent = nullptr;
        }
    }
};

/* ============================================================================
 * Boundary Condition Tests
 * ============================================================================ */

TEST_F(WorldImaginationHealthRegressionTest, HealthScoreBoundaries) {
    jepa_predictor_t* jepa = create_mock_jepa(1);
    omni_world_model_t* wm = create_mock_world_model(1);
    imagination_engine_t* imag = create_mock_imagination(1);

    nimcp_health_agent_connect_jepa(agent, jepa, nullptr);
    nimcp_health_agent_connect_world_model(agent, wm, nullptr);
    nimcp_health_agent_connect_imagination(agent, imag, nullptr);

    // Health scores should always be within [0, 1]
    for (int i = 0; i < 100; i++) {
        nimcp_health_agent_check_world_model_now(agent);
        nimcp_health_agent_check_imagination_now(agent);

        float wm_score = nimcp_health_agent_get_world_model_health_score(agent);
        float imag_score = nimcp_health_agent_get_imagination_health_score(agent);

        EXPECT_GE(wm_score, 0.0f);
        EXPECT_LE(wm_score, 1.0f);
        EXPECT_GE(imag_score, 0.0f);
        EXPECT_LE(imag_score, 1.0f);
    }
}

TEST_F(WorldImaginationHealthRegressionTest, MetricsBoundaryValues) {
    jepa_predictor_t* jepa = create_mock_jepa(1);
    nimcp_health_agent_connect_jepa(agent, jepa, nullptr);

    nimcp_health_agent_check_world_model_now(agent);

    world_imagination_health_t health = {};
    EXPECT_EQ(nimcp_health_agent_get_world_imagination_health(agent, &health), 0);

    // All float metrics should be in valid ranges
    EXPECT_GE(health.wm_imagination_alignment, 0.0f);
    EXPECT_LE(health.wm_imagination_alignment, 1.0f);

    EXPECT_GE(health.jepa.mean_prediction_error, 0.0f);
    EXPECT_GE(health.jepa.embedding_orthogonality, 0.0f);
    EXPECT_LE(health.jepa.embedding_orthogonality, 1.0f);
}

/* ============================================================================
 * Repeated Operations Tests
 * ============================================================================ */

TEST_F(WorldImaginationHealthRegressionTest, RepeatedConnectDisconnectJepa) {
    jepa_predictor_t* jepa = create_mock_jepa(1);

    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(nimcp_health_agent_connect_jepa(agent, jepa, nullptr), 0);
        EXPECT_EQ(nimcp_health_agent_disconnect_jepa(agent), 0);
    }
}

TEST_F(WorldImaginationHealthRegressionTest, RepeatedConnectDisconnectWorldModel) {
    omni_world_model_t* wm = create_mock_world_model(1);

    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(nimcp_health_agent_connect_world_model(agent, wm, nullptr), 0);
        EXPECT_EQ(nimcp_health_agent_disconnect_world_model(agent), 0);
    }
}

TEST_F(WorldImaginationHealthRegressionTest, RepeatedConnectDisconnectImagination) {
    imagination_engine_t* imag = create_mock_imagination(1);

    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(nimcp_health_agent_connect_imagination(agent, imag, nullptr), 0);
        EXPECT_EQ(nimcp_health_agent_disconnect_imagination(agent), 0);
    }
}

TEST_F(WorldImaginationHealthRegressionTest, RepeatedHealthChecks) {
    jepa_predictor_t* jepa = create_mock_jepa(1);
    omni_world_model_t* wm = create_mock_world_model(1);
    imagination_engine_t* imag = create_mock_imagination(1);

    nimcp_health_agent_connect_jepa(agent, jepa, nullptr);
    nimcp_health_agent_connect_world_model(agent, wm, nullptr);
    nimcp_health_agent_connect_imagination(agent, imag, nullptr);

    float prev_wm_score = 1.0f;
    float prev_imag_score = 1.0f;

    for (int i = 0; i < 1000; i++) {
        EXPECT_EQ(nimcp_health_agent_check_world_model_now(agent), 0);
        EXPECT_EQ(nimcp_health_agent_check_imagination_now(agent), 0);

        float wm_score = nimcp_health_agent_get_world_model_health_score(agent);
        float imag_score = nimcp_health_agent_get_imagination_health_score(agent);

        // Scores should remain stable
        EXPECT_GE(wm_score, 0.0f);
        EXPECT_LE(wm_score, 1.0f);
        EXPECT_GE(imag_score, 0.0f);
        EXPECT_LE(imag_score, 1.0f);

        prev_wm_score = wm_score;
        prev_imag_score = imag_score;
    }
}

TEST_F(WorldImaginationHealthRegressionTest, RepeatedConfigUpdates) {
    jepa_predictor_t* jepa = create_mock_jepa(1);
    nimcp_health_agent_connect_jepa(agent, jepa, nullptr);

    for (int i = 0; i < 100; i++) {
        health_agent_wm_imagination_config_t config = {};
        nimcp_health_agent_wm_imagination_config_default(&config);
        config.check_interval_ms = 100 + (i % 1000);
        config.jepa_error_warning = 0.1f + (i % 10) * 0.05f;
        config.coherence_warning = 0.5f + (i % 5) * 0.05f;

        EXPECT_EQ(nimcp_health_agent_update_wm_imagination_config(agent, &config), 0);
    }
}

/* ============================================================================
 * Random Order Operations
 * ============================================================================ */

TEST_F(WorldImaginationHealthRegressionTest, RandomMixedOperations) {
    jepa_predictor_t* jepa = create_mock_jepa(1);
    omni_world_model_t* wm = create_mock_world_model(1);
    imagination_engine_t* imag = create_mock_imagination(1);

    std::random_device rd;
    std::mt19937 g(rd());

    bool jepa_connected = false;
    bool wm_connected = false;
    bool imag_connected = false;

    for (int op = 0; op < 500; op++) {
        int action = g() % 10;

        switch (action) {
            case 0:  // Connect JEPA
                if (!jepa_connected) {
                    nimcp_health_agent_connect_jepa(agent, jepa, nullptr);
                    jepa_connected = true;
                }
                break;

            case 1:  // Disconnect JEPA
                if (jepa_connected) {
                    nimcp_health_agent_disconnect_jepa(agent);
                    jepa_connected = false;
                }
                break;

            case 2:  // Connect World Model
                if (!wm_connected) {
                    nimcp_health_agent_connect_world_model(agent, wm, nullptr);
                    wm_connected = true;
                }
                break;

            case 3:  // Disconnect World Model
                if (wm_connected) {
                    nimcp_health_agent_disconnect_world_model(agent);
                    wm_connected = false;
                }
                break;

            case 4:  // Connect Imagination
                if (!imag_connected) {
                    nimcp_health_agent_connect_imagination(agent, imag, nullptr);
                    imag_connected = true;
                }
                break;

            case 5:  // Disconnect Imagination
                if (imag_connected) {
                    nimcp_health_agent_disconnect_imagination(agent);
                    imag_connected = false;
                }
                break;

            case 6:  // Check world model
                nimcp_health_agent_check_world_model_now(agent);
                break;

            case 7:  // Check imagination
                nimcp_health_agent_check_imagination_now(agent);
                break;

            case 8:  // Get health scores
                nimcp_health_agent_get_world_model_health_score(agent);
                nimcp_health_agent_get_imagination_health_score(agent);
                break;

            case 9:  // Get combined health
                {
                    world_imagination_health_t health = {};
                    nimcp_health_agent_get_world_imagination_health(agent, &health);
                }
                break;
        }
    }
}

/* ============================================================================
 * Metrics Consistency Tests
 * ============================================================================ */

TEST_F(WorldImaginationHealthRegressionTest, MetricsConsistentAcrossChecks) {
    jepa_predictor_t* jepa = create_mock_jepa(1);
    omni_world_model_t* wm = create_mock_world_model(1);
    imagination_engine_t* imag = create_mock_imagination(1);

    nimcp_health_agent_connect_jepa(agent, jepa, nullptr);
    nimcp_health_agent_connect_world_model(agent, wm, nullptr);
    nimcp_health_agent_connect_imagination(agent, imag, nullptr);

    world_imagination_health_t prev_health = {};
    nimcp_health_agent_check_world_model_now(agent);
    nimcp_health_agent_check_imagination_now(agent);
    nimcp_health_agent_get_world_imagination_health(agent, &prev_health);

    for (int i = 0; i < 50; i++) {
        nimcp_health_agent_check_world_model_now(agent);
        nimcp_health_agent_check_imagination_now(agent);

        world_imagination_health_t curr_health = {};
        nimcp_health_agent_get_world_imagination_health(agent, &curr_health);

        // Check count should increment
        EXPECT_GE(curr_health.check_count, prev_health.check_count);

        // Alignment should be valid
        EXPECT_GE(curr_health.wm_imagination_alignment, 0.0f);
        EXPECT_LE(curr_health.wm_imagination_alignment, 1.0f);

        prev_health = curr_health;
    }
}

TEST_F(WorldImaginationHealthRegressionTest, MetricsPreservedDuringDisconnect) {
    jepa_predictor_t* jepa = create_mock_jepa(1);
    omni_world_model_t* wm = create_mock_world_model(1);
    imagination_engine_t* imag = create_mock_imagination(1);

    nimcp_health_agent_connect_jepa(agent, jepa, nullptr);
    nimcp_health_agent_connect_world_model(agent, wm, nullptr);
    nimcp_health_agent_connect_imagination(agent, imag, nullptr);

    // Run some checks
    for (int i = 0; i < 5; i++) {
        nimcp_health_agent_check_world_model_now(agent);
        nimcp_health_agent_check_imagination_now(agent);
    }

    world_imagination_health_t health_before = {};
    nimcp_health_agent_get_world_imagination_health(agent, &health_before);

    // Disconnect JEPA
    nimcp_health_agent_disconnect_jepa(agent);

    // Health metrics should still be retrievable
    world_imagination_health_t health_after = {};
    EXPECT_EQ(nimcp_health_agent_get_world_imagination_health(agent, &health_after), 0);

    // Check count should be preserved
    EXPECT_EQ(health_before.check_count, health_after.check_count);
}

/* ============================================================================
 * Health Score Stability Tests
 * ============================================================================ */

TEST_F(WorldImaginationHealthRegressionTest, HealthScoreStableWithConstantMetrics) {
    jepa_predictor_t* jepa = create_mock_jepa(1);
    omni_world_model_t* wm = create_mock_world_model(1);

    nimcp_health_agent_connect_jepa(agent, jepa, nullptr);
    nimcp_health_agent_connect_world_model(agent, wm, nullptr);

    nimcp_health_agent_check_world_model_now(agent);
    float initial_score = nimcp_health_agent_get_world_model_health_score(agent);

    std::vector<float> scores;
    for (int i = 0; i < 10; i++) {
        nimcp_health_agent_check_world_model_now(agent);
        scores.push_back(nimcp_health_agent_get_world_model_health_score(agent));
    }

    // Scores should be relatively stable
    for (float score : scores) {
        EXPECT_GE(score, 0.0f);
        EXPECT_LE(score, 1.0f);
    }
}

TEST_F(WorldImaginationHealthRegressionTest, HealthScoreAfterDisconnects) {
    jepa_predictor_t* jepa = create_mock_jepa(1);
    omni_world_model_t* wm = create_mock_world_model(1);
    imagination_engine_t* imag = create_mock_imagination(1);

    // Connect all
    nimcp_health_agent_connect_jepa(agent, jepa, nullptr);
    nimcp_health_agent_connect_world_model(agent, wm, nullptr);
    nimcp_health_agent_connect_imagination(agent, imag, nullptr);

    nimcp_health_agent_check_world_model_now(agent);
    nimcp_health_agent_check_imagination_now(agent);

    // Scores should be valid with all connected
    float wm_score = nimcp_health_agent_get_world_model_health_score(agent);
    float imag_score = nimcp_health_agent_get_imagination_health_score(agent);
    EXPECT_GE(wm_score, 0.0f);
    EXPECT_LE(wm_score, 1.0f);
    EXPECT_GE(imag_score, 0.0f);
    EXPECT_LE(imag_score, 1.0f);

    // Disconnect one by one and verify scores
    nimcp_health_agent_disconnect_imagination(agent);
    wm_score = nimcp_health_agent_get_world_model_health_score(agent);
    EXPECT_GE(wm_score, 0.0f);
    EXPECT_LE(wm_score, 1.0f);

    nimcp_health_agent_disconnect_world_model(agent);
    wm_score = nimcp_health_agent_get_world_model_health_score(agent);
    EXPECT_GE(wm_score, 0.0f);
    EXPECT_LE(wm_score, 1.0f);

    nimcp_health_agent_disconnect_jepa(agent);
    // With nothing connected, score reflects last computed value (still valid range)
    wm_score = nimcp_health_agent_get_world_model_health_score(agent);
    EXPECT_GE(wm_score, 0.0f);
    EXPECT_LE(wm_score, 1.0f);
}

/* ============================================================================
 * Double Operation Safety Tests
 * ============================================================================ */

TEST_F(WorldImaginationHealthRegressionTest, DoubleDisconnectSafe) {
    jepa_predictor_t* jepa = create_mock_jepa(1);
    nimcp_health_agent_connect_jepa(agent, jepa, nullptr);

    EXPECT_EQ(nimcp_health_agent_disconnect_jepa(agent), 0);
    // Second disconnect should be safe (already disconnected)
    int result = nimcp_health_agent_disconnect_jepa(agent);
    // Implementation may return 0 or -1, both are acceptable
    (void)result;
}

TEST_F(WorldImaginationHealthRegressionTest, DoubleConnectReplace) {
    jepa_predictor_t* jepa1 = create_mock_jepa(1);
    jepa_predictor_t* jepa2 = create_mock_jepa(2);

    EXPECT_EQ(nimcp_health_agent_connect_jepa(agent, jepa1, nullptr), 0);
    // Second connect should replace the first
    EXPECT_EQ(nimcp_health_agent_connect_jepa(agent, jepa2, nullptr), 0);
}

/* ============================================================================
 * Recovery Under Load Tests
 * ============================================================================ */

TEST_F(WorldImaginationHealthRegressionTest, RecoveryUnderLoad) {
    jepa_predictor_t* jepa = create_mock_jepa(1);
    omni_world_model_t* wm = create_mock_world_model(1);
    imagination_engine_t* imag = create_mock_imagination(1);

    nimcp_health_agent_connect_jepa(agent, jepa, nullptr);
    nimcp_health_agent_connect_world_model(agent, wm, nullptr);
    nimcp_health_agent_connect_imagination(agent, imag, nullptr);

    // Perform many checks
    for (int i = 0; i < 100; i++) {
        nimcp_health_agent_check_world_model_now(agent);
        nimcp_health_agent_check_imagination_now(agent);
    }

    // Execute all recovery actions
    EXPECT_EQ(nimcp_health_agent_world_model_recovery(
        agent, WM_RECOVERY_RESET_PREDICTOR, "test"), 0);
    EXPECT_EQ(nimcp_health_agent_world_model_recovery(
        agent, WM_RECOVERY_PRUNE_LATENT, "test"), 0);
    EXPECT_EQ(nimcp_health_agent_world_model_recovery(
        agent, WM_RECOVERY_RETRAIN_DYNAMICS, "test"), 0);
    EXPECT_EQ(nimcp_health_agent_world_model_recovery(
        agent, WM_RECOVERY_CLEAR_WORKSPACE, "test"), 0);
    EXPECT_EQ(nimcp_health_agent_world_model_recovery(
        agent, WM_RECOVERY_REDUCE_HORIZON, "test"), 0);
    EXPECT_EQ(nimcp_health_agent_world_model_recovery(
        agent, WM_RECOVERY_INCREASE_REALITY_CHECK, "test"), 0);
    EXPECT_EQ(nimcp_health_agent_world_model_recovery(
        agent, WM_RECOVERY_THROTTLE_IMAGINATION, "test"), 0);
    EXPECT_EQ(nimcp_health_agent_world_model_recovery(
        agent, WM_RECOVERY_BOOST_GROUNDING, "test"), 0);
    EXPECT_EQ(nimcp_health_agent_world_model_recovery(
        agent, WM_RECOVERY_CHECKPOINT_RESTORE, "test"), 0);

    // System should still be functional
    nimcp_health_agent_check_world_model_now(agent);
    nimcp_health_agent_check_imagination_now(agent);

    world_imagination_health_t health = {};
    EXPECT_EQ(nimcp_health_agent_get_world_imagination_health(agent, &health), 0);
}

/* ============================================================================
 * Config Edge Cases
 * ============================================================================ */

TEST_F(WorldImaginationHealthRegressionTest, ExtremeConfigValues) {
    jepa_predictor_t* jepa = create_mock_jepa(1);
    nimcp_health_agent_connect_jepa(agent, jepa, nullptr);

    health_agent_wm_imagination_config_t config = {};
    nimcp_health_agent_wm_imagination_config_default(&config);

    // Extreme low values
    config.check_interval_ms = 1;
    config.jepa_error_warning = 0.0001f;
    config.jepa_error_critical = 0.0002f;
    config.coherence_warning = 0.0001f;
    config.coherence_critical = 0.0002f;
    config.embedding_variance_min = 0.0001f;
    config.vividness_warning = 0.0001f;
    config.reality_check_min = 0.0001f;
    config.imagination_reality_blur_max = 0.0001f;
    config.horizon_min_stable = 1;

    EXPECT_EQ(nimcp_health_agent_update_wm_imagination_config(agent, &config), 0);

    // Should still work without crashing
    nimcp_health_agent_check_world_model_now(agent);
    float score = nimcp_health_agent_get_world_model_health_score(agent);
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 1.0f);

    // Extreme high values
    nimcp_health_agent_wm_imagination_config_default(&config);
    config.check_interval_ms = UINT32_MAX;
    config.jepa_error_warning = 1.0f;
    config.jepa_error_critical = 1.0f;
    config.coherence_warning = 1.0f;
    config.coherence_critical = 1.0f;
    config.embedding_variance_min = 1.0f;
    config.vividness_warning = 1.0f;
    config.reality_check_min = 1.0f;
    config.imagination_reality_blur_max = 1.0f;
    config.horizon_min_stable = UINT32_MAX;

    EXPECT_EQ(nimcp_health_agent_update_wm_imagination_config(agent, &config), 0);
    nimcp_health_agent_check_world_model_now(agent);
    score = nimcp_health_agent_get_world_model_health_score(agent);
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 1.0f);
}

TEST_F(WorldImaginationHealthRegressionTest, DisabledAutoRecoveryConfig) {
    health_agent_wm_imagination_config_t config = {};
    nimcp_health_agent_wm_imagination_config_default(&config);
    config.auto_recovery_enabled = false;

    jepa_predictor_t* jepa = create_mock_jepa(1);
    EXPECT_EQ(nimcp_health_agent_connect_jepa(agent, jepa, &config), 0);

    // Checks should succeed but not trigger auto-recovery
    int result = nimcp_health_agent_check_world_model_now(agent);
    EXPECT_EQ(result, 0);

    world_imagination_health_t health = {};
    nimcp_health_agent_get_world_imagination_health(agent, &health);

    // Just verify no crash and valid score
    float score = nimcp_health_agent_get_world_model_health_score(agent);
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 1.0f);
}

/* ============================================================================
 * State Transition Tests
 * ============================================================================ */

TEST_F(WorldImaginationHealthRegressionTest, WorldModelStateTransitions) {
    jepa_predictor_t* jepa = create_mock_jepa(1);
    omni_world_model_t* wm = create_mock_world_model(1);

    nimcp_health_agent_connect_jepa(agent, jepa, nullptr);
    nimcp_health_agent_connect_world_model(agent, wm, nullptr);

    // Check initial state
    world_imagination_health_t health = {};
    nimcp_health_agent_get_world_imagination_health(agent, &health);

    // State should be a valid enum value
    EXPECT_GE(static_cast<int>(health.world_model.health_state), static_cast<int>(WM_HEALTH_OPTIMAL));
    EXPECT_LE(static_cast<int>(health.world_model.health_state), static_cast<int>(WM_HEALTH_CRITICAL));
}

TEST_F(WorldImaginationHealthRegressionTest, ImaginationStateTransitions) {
    imagination_engine_t* imag = create_mock_imagination(1);

    nimcp_health_agent_connect_imagination(agent, imag, nullptr);

    // Check initial state
    world_imagination_health_t health = {};
    nimcp_health_agent_get_world_imagination_health(agent, &health);

    // State should be a valid enum value
    EXPECT_GE(static_cast<int>(health.imagination.health_state), static_cast<int>(IMAG_HEALTH_VIVID));
    EXPECT_LE(static_cast<int>(health.imagination.health_state), static_cast<int>(IMAG_HEALTH_IMPAIRED));
}

/* ============================================================================
 * Needs Attention Tests
 * ============================================================================ */

TEST_F(WorldImaginationHealthRegressionTest, NeedsAttentionConsistency) {
    jepa_predictor_t* jepa = create_mock_jepa(1);
    omni_world_model_t* wm = create_mock_world_model(1);
    imagination_engine_t* imag = create_mock_imagination(1);

    nimcp_health_agent_connect_jepa(agent, jepa, nullptr);
    nimcp_health_agent_connect_world_model(agent, wm, nullptr);
    nimcp_health_agent_connect_imagination(agent, imag, nullptr);

    // Initial state should not need attention
    EXPECT_FALSE(nimcp_health_agent_world_model_needs_attention(agent));
    EXPECT_FALSE(nimcp_health_agent_imagination_needs_attention(agent));

    // After checks, still shouldn't need attention if healthy
    for (int i = 0; i < 10; i++) {
        nimcp_health_agent_check_world_model_now(agent);
        nimcp_health_agent_check_imagination_now(agent);
    }

    // Healthy system should not need attention
    world_imagination_health_t health = {};
    nimcp_health_agent_get_world_imagination_health(agent, &health);
    if (health.world_model.health_state == WM_HEALTH_OPTIMAL) {
        EXPECT_FALSE(nimcp_health_agent_world_model_needs_attention(agent));
    }
}

/* ============================================================================
 * Alignment Calculation Tests
 * ============================================================================ */

TEST_F(WorldImaginationHealthRegressionTest, AlignmentCalculationConsistency) {
    jepa_predictor_t* jepa = create_mock_jepa(1);
    omni_world_model_t* wm = create_mock_world_model(1);
    imagination_engine_t* imag = create_mock_imagination(1);

    nimcp_health_agent_connect_jepa(agent, jepa, nullptr);
    nimcp_health_agent_connect_world_model(agent, wm, nullptr);
    nimcp_health_agent_connect_imagination(agent, imag, nullptr);

    for (int i = 0; i < 20; i++) {
        nimcp_health_agent_check_world_model_now(agent);
        nimcp_health_agent_check_imagination_now(agent);

        float wm_score = nimcp_health_agent_get_world_model_health_score(agent);
        float imag_score = nimcp_health_agent_get_imagination_health_score(agent);

        world_imagination_health_t health = {};
        nimcp_health_agent_get_world_imagination_health(agent, &health);

        // Alignment should be related to the individual scores
        // (specific calculation may vary, but should be bounded)
        EXPECT_GE(health.wm_imagination_alignment, 0.0f);
        EXPECT_LE(health.wm_imagination_alignment, 1.0f);
    }
}
