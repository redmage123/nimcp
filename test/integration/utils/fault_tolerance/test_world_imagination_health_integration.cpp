/**
 * @file test_world_imagination_health_integration.cpp
 * @brief Integration tests for Phase 5.14 World Model & Imagination Health
 * @date 2026-01-19
 *
 * Tests integration between health agent and world model/imagination systems.
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>

extern "C" {
#include "utils/fault_tolerance/nimcp_health_agent.h"
#include "utils/logging/nimcp_logging.h"
}

/**
 * @brief Integration test fixture for world model/imagination health
 */
class WorldImaginationHealthIntegrationTest : public ::testing::Test {
protected:
    nimcp_health_agent_t* agent = nullptr;

    void SetUp() override {
        health_agent_config_t config = {};
        config.heartbeat_interval_ms = 100;
        config.message_queue_depth = 256;
        config.watchdog_timeout_ms = 5000;
        config.enable_auto_recovery = true;

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
 * Multi-Component Integration Tests
 * ============================================================================ */

TEST_F(WorldImaginationHealthIntegrationTest, JepaWorldModelIntegration) {
    /* Connect both JEPA and world model */
    jepa_predictor_t* fake_jepa = reinterpret_cast<jepa_predictor_t*>(0x1234);
    omni_world_model_t* fake_wm = reinterpret_cast<omni_world_model_t*>(0x5678);

    EXPECT_EQ(nimcp_health_agent_connect_jepa(agent, fake_jepa, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_world_model(agent, fake_wm, nullptr), 0);

    /* Run multiple health checks */
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(nimcp_health_agent_check_world_model_now(agent), 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    /* Verify combined metrics */
    world_imagination_health_t health = {};
    EXPECT_EQ(nimcp_health_agent_get_world_imagination_health(agent, &health), 0);
    EXPECT_GE(health.check_count, 5u);
}

TEST_F(WorldImaginationHealthIntegrationTest, FullSystemIntegration) {
    /* Connect all three components */
    jepa_predictor_t* fake_jepa = reinterpret_cast<jepa_predictor_t*>(0x1234);
    omni_world_model_t* fake_wm = reinterpret_cast<omni_world_model_t*>(0x5678);
    imagination_engine_t* fake_imag = reinterpret_cast<imagination_engine_t*>(0x9ABC);

    EXPECT_EQ(nimcp_health_agent_connect_jepa(agent, fake_jepa, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_world_model(agent, fake_wm, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_imagination(agent, fake_imag, nullptr), 0);

    /* Run health checks */
    EXPECT_EQ(nimcp_health_agent_check_world_model_now(agent), 0);
    EXPECT_EQ(nimcp_health_agent_check_imagination_now(agent), 0);

    /* Get combined health */
    world_imagination_health_t health = {};
    EXPECT_EQ(nimcp_health_agent_get_world_imagination_health(agent, &health), 0);

    /* Verify cross-system metrics are computed */
    EXPECT_GE(health.wm_imagination_alignment, 0.0f);
    EXPECT_LE(health.wm_imagination_alignment, 1.0f);
}

/* ============================================================================
 * Configuration Propagation Tests
 * ============================================================================ */

TEST_F(WorldImaginationHealthIntegrationTest, ConfigPropagationOnConnect) {
    health_agent_wm_imagination_config_t config = {};
    nimcp_health_agent_wm_imagination_config_default(&config);
    config.jepa_error_warning = 0.25f;
    config.coherence_warning = 0.55f;

    /* Connect with custom config */
    jepa_predictor_t* fake_jepa = reinterpret_cast<jepa_predictor_t*>(0x1234);
    EXPECT_EQ(nimcp_health_agent_connect_jepa(agent, fake_jepa, &config), 0);

    /* Subsequent connections should use the same config */
    omni_world_model_t* fake_wm = reinterpret_cast<omni_world_model_t*>(0x5678);
    EXPECT_EQ(nimcp_health_agent_connect_world_model(agent, fake_wm, nullptr), 0);

    /* Run check and verify */
    EXPECT_EQ(nimcp_health_agent_check_world_model_now(agent), 0);
}

/* ============================================================================
 * Recovery Coordination Tests
 * ============================================================================ */

TEST_F(WorldImaginationHealthIntegrationTest, RecoverySequence) {
    /* Connect systems */
    jepa_predictor_t* fake_jepa = reinterpret_cast<jepa_predictor_t*>(0x1234);
    imagination_engine_t* fake_imag = reinterpret_cast<imagination_engine_t*>(0x9ABC);

    EXPECT_EQ(nimcp_health_agent_connect_jepa(agent, fake_jepa, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_imagination(agent, fake_imag, nullptr), 0);

    /* Simulate a recovery sequence */
    EXPECT_EQ(nimcp_health_agent_world_model_recovery(
        agent, WM_RECOVERY_RESET_PREDICTOR, "integration test"), 0);
    EXPECT_EQ(nimcp_health_agent_world_model_recovery(
        agent, WM_RECOVERY_CLEAR_WORKSPACE, "clear imagination"), 0);

    /* Check that health checks still work after recovery */
    EXPECT_EQ(nimcp_health_agent_check_world_model_now(agent), 0);
    EXPECT_EQ(nimcp_health_agent_check_imagination_now(agent), 0);
}

/* ============================================================================
 * Health Score Correlation Tests
 * ============================================================================ */

TEST_F(WorldImaginationHealthIntegrationTest, HealthScoreCorrelation) {
    /* Connect all systems */
    jepa_predictor_t* fake_jepa = reinterpret_cast<jepa_predictor_t*>(0x1234);
    omni_world_model_t* fake_wm = reinterpret_cast<omni_world_model_t*>(0x5678);
    imagination_engine_t* fake_imag = reinterpret_cast<imagination_engine_t*>(0x9ABC);

    EXPECT_EQ(nimcp_health_agent_connect_jepa(agent, fake_jepa, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_world_model(agent, fake_wm, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_imagination(agent, fake_imag, nullptr), 0);

    /* Run checks */
    EXPECT_EQ(nimcp_health_agent_check_world_model_now(agent), 0);
    EXPECT_EQ(nimcp_health_agent_check_imagination_now(agent), 0);

    /* Get individual scores */
    float wm_score = nimcp_health_agent_get_world_model_health_score(agent);
    float imag_score = nimcp_health_agent_get_imagination_health_score(agent);

    /* Both should be valid */
    EXPECT_GE(wm_score, 0.0f);
    EXPECT_LE(wm_score, 1.0f);
    EXPECT_GE(imag_score, 0.0f);
    EXPECT_LE(imag_score, 1.0f);

    /* Combined health should reflect both */
    world_imagination_health_t health = {};
    EXPECT_EQ(nimcp_health_agent_get_world_imagination_health(agent, &health), 0);

    /* Alignment should be average of scores */
    float expected_alignment = (wm_score + imag_score) / 2.0f;
    EXPECT_NEAR(health.wm_imagination_alignment, expected_alignment, 0.01f);
}

/* ============================================================================
 * Connect/Disconnect Cycle Tests
 * ============================================================================ */

TEST_F(WorldImaginationHealthIntegrationTest, ConnectDisconnectCycle) {
    jepa_predictor_t* fake_jepa = reinterpret_cast<jepa_predictor_t*>(0x1234);
    omni_world_model_t* fake_wm = reinterpret_cast<omni_world_model_t*>(0x5678);
    imagination_engine_t* fake_imag = reinterpret_cast<imagination_engine_t*>(0x9ABC);

    /* Multiple connect/disconnect cycles */
    for (int i = 0; i < 3; i++) {
        EXPECT_EQ(nimcp_health_agent_connect_jepa(agent, fake_jepa, nullptr), 0);
        EXPECT_EQ(nimcp_health_agent_connect_world_model(agent, fake_wm, nullptr), 0);
        EXPECT_EQ(nimcp_health_agent_connect_imagination(agent, fake_imag, nullptr), 0);

        EXPECT_EQ(nimcp_health_agent_check_world_model_now(agent), 0);
        EXPECT_EQ(nimcp_health_agent_check_imagination_now(agent), 0);

        EXPECT_EQ(nimcp_health_agent_disconnect_imagination(agent), 0);
        EXPECT_EQ(nimcp_health_agent_disconnect_world_model(agent), 0);
        EXPECT_EQ(nimcp_health_agent_disconnect_jepa(agent), 0);
    }
}

/* ============================================================================
 * Anomaly Detection Integration Tests
 * ============================================================================ */

TEST_F(WorldImaginationHealthIntegrationTest, NoAnomaliesOnHealthySystem) {
    jepa_predictor_t* fake_jepa = reinterpret_cast<jepa_predictor_t*>(0x1234);
    omni_world_model_t* fake_wm = reinterpret_cast<omni_world_model_t*>(0x5678);
    imagination_engine_t* fake_imag = reinterpret_cast<imagination_engine_t*>(0x9ABC);

    EXPECT_EQ(nimcp_health_agent_connect_jepa(agent, fake_jepa, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_world_model(agent, fake_wm, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_imagination(agent, fake_imag, nullptr), 0);

    /* Initial state should not need attention */
    EXPECT_FALSE(nimcp_health_agent_world_model_needs_attention(agent));
    EXPECT_FALSE(nimcp_health_agent_imagination_needs_attention(agent));

    /* Get health - should have no anomalies initially */
    world_imagination_health_t health = {};
    EXPECT_EQ(nimcp_health_agent_get_world_imagination_health(agent, &health), 0);
    EXPECT_EQ(health.active_anomalies, 0u);
    EXPECT_EQ(health.recommended_action, WM_RECOVERY_NONE);
}
