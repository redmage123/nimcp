/**
 * @file test_health_agent_world_imagination_functions.cpp
 * @brief Unit tests for Phase 5.14 World Model & Imagination Health Integration
 * @date 2026-01-19
 *
 * Tests for JEPA, world model, and imagination engine health monitoring
 * integration with the health agent.
 */

#include <gtest/gtest.h>

extern "C" {
#include "utils/fault_tolerance/nimcp_health_agent.h"
#include "utils/logging/nimcp_logging.h"
}

/**
 * @brief Test fixture for world model/imagination health integration tests
 */
class HealthAgentWorldImaginationTest : public ::testing::Test {
protected:
    nimcp_health_agent_t* agent = nullptr;

    void SetUp() override {
        health_agent_config_t config = {};
        config.heartbeat_interval_ms = 1000;
        config.message_queue_depth = 64;
        config.watchdog_timeout_ms = 5000;
        config.enable_auto_recovery = false;

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
 * Configuration Default Tests
 * ============================================================================ */

TEST_F(HealthAgentWorldImaginationTest, WmImaginationConfigDefault) {
    health_agent_wm_imagination_config_t config = {};

    nimcp_health_agent_wm_imagination_config_default(&config);

    /* Check intervals */
    EXPECT_EQ(config.check_interval_ms, 500u);
    EXPECT_EQ(config.trend_window_ms, 10000u);

    /* JEPA thresholds */
    EXPECT_FLOAT_EQ(config.jepa_error_warning, 0.3f);
    EXPECT_FLOAT_EQ(config.jepa_error_critical, 0.6f);
    EXPECT_FLOAT_EQ(config.embedding_variance_min, 0.01f);
    EXPECT_FLOAT_EQ(config.gradient_norm_max, 100.0f);
    EXPECT_FLOAT_EQ(config.gradient_norm_min, 1e-7f);

    /* World model thresholds */
    EXPECT_FLOAT_EQ(config.forward_accuracy_warning, 0.7f);
    EXPECT_FLOAT_EQ(config.forward_accuracy_critical, 0.5f);
    EXPECT_EQ(config.horizon_min_stable, 5u);
    EXPECT_FLOAT_EQ(config.counterfactual_validity_min, 0.8f);

    /* Imagination thresholds */
    EXPECT_FLOAT_EQ(config.coherence_warning, 0.6f);
    EXPECT_FLOAT_EQ(config.coherence_critical, 0.4f);
    EXPECT_FLOAT_EQ(config.vividness_warning, 0.4f);
    EXPECT_FLOAT_EQ(config.reality_check_min, 0.9f);
    EXPECT_FLOAT_EQ(config.imagination_reality_blur_max, 0.3f);

    /* Recovery settings */
    EXPECT_TRUE(config.auto_recovery_enabled);
    EXPECT_EQ(config.recovery_cooldown_ms, 5000u);
    EXPECT_EQ(config.max_recoveries_per_hour, 10u);

    /* Immune integration */
    EXPECT_TRUE(config.report_to_immune);
    EXPECT_EQ(config.immune_severity_base, 6);
}

TEST_F(HealthAgentWorldImaginationTest, WmImaginationConfigDefaultNullSafe) {
    /* Should not crash with NULL */
    nimcp_health_agent_wm_imagination_config_default(nullptr);
}

/* ============================================================================
 * JEPA Connection Tests
 * ============================================================================ */

TEST_F(HealthAgentWorldImaginationTest, ConnectJepaNullAgent) {
    /* Fake JEPA pointer for testing */
    jepa_predictor_t* fake_jepa = reinterpret_cast<jepa_predictor_t*>(0x1234);

    int result = nimcp_health_agent_connect_jepa(nullptr, fake_jepa, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(HealthAgentWorldImaginationTest, ConnectJepaNullJepa) {
    int result = nimcp_health_agent_connect_jepa(agent, nullptr, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(HealthAgentWorldImaginationTest, ConnectJepaSuccess) {
    jepa_predictor_t* fake_jepa = reinterpret_cast<jepa_predictor_t*>(0x1234);

    int result = nimcp_health_agent_connect_jepa(agent, fake_jepa, nullptr);
    EXPECT_EQ(result, 0);
}

TEST_F(HealthAgentWorldImaginationTest, ConnectJepaWithConfig) {
    jepa_predictor_t* fake_jepa = reinterpret_cast<jepa_predictor_t*>(0x1234);
    health_agent_wm_imagination_config_t config = {};
    nimcp_health_agent_wm_imagination_config_default(&config);
    config.jepa_error_warning = 0.4f;

    int result = nimcp_health_agent_connect_jepa(agent, fake_jepa, &config);
    EXPECT_EQ(result, 0);
}

TEST_F(HealthAgentWorldImaginationTest, DisconnectJepaNullAgent) {
    int result = nimcp_health_agent_disconnect_jepa(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(HealthAgentWorldImaginationTest, DisconnectJepaSuccess) {
    jepa_predictor_t* fake_jepa = reinterpret_cast<jepa_predictor_t*>(0x1234);
    nimcp_health_agent_connect_jepa(agent, fake_jepa, nullptr);

    int result = nimcp_health_agent_disconnect_jepa(agent);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * World Model Connection Tests
 * ============================================================================ */

TEST_F(HealthAgentWorldImaginationTest, ConnectWorldModelNullAgent) {
    omni_world_model_t* fake_wm = reinterpret_cast<omni_world_model_t*>(0x5678);

    int result = nimcp_health_agent_connect_world_model(nullptr, fake_wm, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(HealthAgentWorldImaginationTest, ConnectWorldModelNullModel) {
    int result = nimcp_health_agent_connect_world_model(agent, nullptr, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(HealthAgentWorldImaginationTest, ConnectWorldModelSuccess) {
    omni_world_model_t* fake_wm = reinterpret_cast<omni_world_model_t*>(0x5678);

    int result = nimcp_health_agent_connect_world_model(agent, fake_wm, nullptr);
    EXPECT_EQ(result, 0);
}

TEST_F(HealthAgentWorldImaginationTest, DisconnectWorldModelNullAgent) {
    int result = nimcp_health_agent_disconnect_world_model(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(HealthAgentWorldImaginationTest, DisconnectWorldModelSuccess) {
    omni_world_model_t* fake_wm = reinterpret_cast<omni_world_model_t*>(0x5678);
    nimcp_health_agent_connect_world_model(agent, fake_wm, nullptr);

    int result = nimcp_health_agent_disconnect_world_model(agent);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * Imagination Connection Tests
 * ============================================================================ */

TEST_F(HealthAgentWorldImaginationTest, ConnectImaginationNullAgent) {
    imagination_engine_t* fake_imag = reinterpret_cast<imagination_engine_t*>(0x9ABC);

    int result = nimcp_health_agent_connect_imagination(nullptr, fake_imag, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(HealthAgentWorldImaginationTest, ConnectImaginationNullEngine) {
    int result = nimcp_health_agent_connect_imagination(agent, nullptr, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(HealthAgentWorldImaginationTest, ConnectImaginationSuccess) {
    imagination_engine_t* fake_imag = reinterpret_cast<imagination_engine_t*>(0x9ABC);

    int result = nimcp_health_agent_connect_imagination(agent, fake_imag, nullptr);
    EXPECT_EQ(result, 0);
}

TEST_F(HealthAgentWorldImaginationTest, DisconnectImaginationNullAgent) {
    int result = nimcp_health_agent_disconnect_imagination(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(HealthAgentWorldImaginationTest, DisconnectImaginationSuccess) {
    imagination_engine_t* fake_imag = reinterpret_cast<imagination_engine_t*>(0x9ABC);
    nimcp_health_agent_connect_imagination(agent, fake_imag, nullptr);

    int result = nimcp_health_agent_disconnect_imagination(agent);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * Metrics Retrieval Tests
 * ============================================================================ */

TEST_F(HealthAgentWorldImaginationTest, GetJepaMetricsNullAgent) {
    jepa_health_metrics_t metrics = {};
    int result = nimcp_health_agent_get_jepa_metrics(nullptr, &metrics);
    EXPECT_EQ(result, -1);
}

TEST_F(HealthAgentWorldImaginationTest, GetJepaMetricsNullMetrics) {
    int result = nimcp_health_agent_get_jepa_metrics(agent, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(HealthAgentWorldImaginationTest, GetJepaMetricsSuccess) {
    jepa_predictor_t* fake_jepa = reinterpret_cast<jepa_predictor_t*>(0x1234);
    nimcp_health_agent_connect_jepa(agent, fake_jepa, nullptr);

    jepa_health_metrics_t metrics = {};
    int result = nimcp_health_agent_get_jepa_metrics(agent, &metrics);
    EXPECT_EQ(result, 0);
}

TEST_F(HealthAgentWorldImaginationTest, GetWorldModelMetricsNullAgent) {
    omni_wm_health_metrics_t metrics = {};
    int result = nimcp_health_agent_get_world_model_metrics(nullptr, &metrics);
    EXPECT_EQ(result, -1);
}

TEST_F(HealthAgentWorldImaginationTest, GetWorldModelMetricsNullMetrics) {
    int result = nimcp_health_agent_get_world_model_metrics(agent, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(HealthAgentWorldImaginationTest, GetWorldModelMetricsSuccess) {
    omni_world_model_t* fake_wm = reinterpret_cast<omni_world_model_t*>(0x5678);
    nimcp_health_agent_connect_world_model(agent, fake_wm, nullptr);

    omni_wm_health_metrics_t metrics = {};
    int result = nimcp_health_agent_get_world_model_metrics(agent, &metrics);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(metrics.health_state, WM_HEALTH_OPTIMAL);
    EXPECT_FLOAT_EQ(metrics.health_score, 1.0f);
}

TEST_F(HealthAgentWorldImaginationTest, GetImaginationMetricsNullAgent) {
    imagination_health_metrics_t metrics = {};
    int result = nimcp_health_agent_get_imagination_metrics(nullptr, &metrics);
    EXPECT_EQ(result, -1);
}

TEST_F(HealthAgentWorldImaginationTest, GetImaginationMetricsNullMetrics) {
    int result = nimcp_health_agent_get_imagination_metrics(agent, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(HealthAgentWorldImaginationTest, GetImaginationMetricsSuccess) {
    imagination_engine_t* fake_imag = reinterpret_cast<imagination_engine_t*>(0x9ABC);
    nimcp_health_agent_connect_imagination(agent, fake_imag, nullptr);

    imagination_health_metrics_t metrics = {};
    int result = nimcp_health_agent_get_imagination_metrics(agent, &metrics);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(metrics.health_state, IMAG_HEALTH_VIVID);
    EXPECT_FLOAT_EQ(metrics.health_score, 1.0f);
}

TEST_F(HealthAgentWorldImaginationTest, GetWorldImaginationHealthNullAgent) {
    world_imagination_health_t health = {};
    int result = nimcp_health_agent_get_world_imagination_health(nullptr, &health);
    EXPECT_EQ(result, -1);
}

TEST_F(HealthAgentWorldImaginationTest, GetWorldImaginationHealthNullHealth) {
    int result = nimcp_health_agent_get_world_imagination_health(agent, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(HealthAgentWorldImaginationTest, GetWorldImaginationHealthSuccess) {
    jepa_predictor_t* fake_jepa = reinterpret_cast<jepa_predictor_t*>(0x1234);
    omni_world_model_t* fake_wm = reinterpret_cast<omni_world_model_t*>(0x5678);
    imagination_engine_t* fake_imag = reinterpret_cast<imagination_engine_t*>(0x9ABC);

    nimcp_health_agent_connect_jepa(agent, fake_jepa, nullptr);
    nimcp_health_agent_connect_world_model(agent, fake_wm, nullptr);
    nimcp_health_agent_connect_imagination(agent, fake_imag, nullptr);

    world_imagination_health_t health = {};
    int result = nimcp_health_agent_get_world_imagination_health(agent, &health);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(health.world_model.health_state, WM_HEALTH_OPTIMAL);
    EXPECT_EQ(health.imagination.health_state, IMAG_HEALTH_VIVID);
}

/* ============================================================================
 * Recovery Action Tests
 * ============================================================================ */

TEST_F(HealthAgentWorldImaginationTest, WorldModelRecoveryNullAgent) {
    int result = nimcp_health_agent_world_model_recovery(
        nullptr, WM_RECOVERY_RESET_PREDICTOR, "test");
    EXPECT_EQ(result, -1);
}

TEST_F(HealthAgentWorldImaginationTest, WorldModelRecoveryNone) {
    int result = nimcp_health_agent_world_model_recovery(
        agent, WM_RECOVERY_NONE, "no action");
    EXPECT_EQ(result, 0);
}

TEST_F(HealthAgentWorldImaginationTest, WorldModelRecoveryResetPredictor) {
    int result = nimcp_health_agent_world_model_recovery(
        agent, WM_RECOVERY_RESET_PREDICTOR, "test reset");
    EXPECT_EQ(result, 0);
}

TEST_F(HealthAgentWorldImaginationTest, WorldModelRecoveryPruneLatent) {
    int result = nimcp_health_agent_world_model_recovery(
        agent, WM_RECOVERY_PRUNE_LATENT, "embedding collapse");
    EXPECT_EQ(result, 0);
}

TEST_F(HealthAgentWorldImaginationTest, WorldModelRecoveryClearWorkspace) {
    int result = nimcp_health_agent_world_model_recovery(
        agent, WM_RECOVERY_CLEAR_WORKSPACE, "imagination stuck");
    EXPECT_EQ(result, 0);
}

TEST_F(HealthAgentWorldImaginationTest, WorldModelRecoveryIncreaseRealityCheck) {
    int result = nimcp_health_agent_world_model_recovery(
        agent, WM_RECOVERY_INCREASE_REALITY_CHECK, "confabulation detected");
    EXPECT_EQ(result, 0);
}

TEST_F(HealthAgentWorldImaginationTest, WorldModelRecoveryAllActions) {
    /* Test all recovery actions */
    for (int action = WM_RECOVERY_NONE; action <= WM_RECOVERY_CHECKPOINT_RESTORE; action++) {
        int result = nimcp_health_agent_world_model_recovery(
            agent, static_cast<world_model_recovery_action_t>(action), "test");
        EXPECT_EQ(result, 0);
    }
}

/* ============================================================================
 * Health Status Query Tests
 * ============================================================================ */

TEST_F(HealthAgentWorldImaginationTest, WorldModelNeedsAttentionNullAgent) {
    bool result = nimcp_health_agent_world_model_needs_attention(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(HealthAgentWorldImaginationTest, WorldModelNeedsAttentionNoConnections) {
    /* No connections means no attention needed */
    bool result = nimcp_health_agent_world_model_needs_attention(agent);
    EXPECT_FALSE(result);
}

TEST_F(HealthAgentWorldImaginationTest, ImaginationNeedsAttentionNullAgent) {
    bool result = nimcp_health_agent_imagination_needs_attention(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(HealthAgentWorldImaginationTest, ImaginationNeedsAttentionNoConnections) {
    bool result = nimcp_health_agent_imagination_needs_attention(agent);
    EXPECT_FALSE(result);
}

TEST_F(HealthAgentWorldImaginationTest, GetWorldModelHealthScoreNullAgent) {
    float score = nimcp_health_agent_get_world_model_health_score(nullptr);
    EXPECT_FLOAT_EQ(score, -1.0f);
}

TEST_F(HealthAgentWorldImaginationTest, GetWorldModelHealthScoreNoConnections) {
    float score = nimcp_health_agent_get_world_model_health_score(agent);
    /* Default score should be 0 when nothing connected */
    EXPECT_GE(score, 0.0f);
}

TEST_F(HealthAgentWorldImaginationTest, GetImaginationHealthScoreNullAgent) {
    float score = nimcp_health_agent_get_imagination_health_score(nullptr);
    EXPECT_FLOAT_EQ(score, -1.0f);
}

TEST_F(HealthAgentWorldImaginationTest, GetImaginationHealthScoreNoConnections) {
    float score = nimcp_health_agent_get_imagination_health_score(agent);
    EXPECT_GE(score, 0.0f);
}

/* ============================================================================
 * Configuration Update Tests
 * ============================================================================ */

TEST_F(HealthAgentWorldImaginationTest, UpdateConfigNullAgent) {
    health_agent_wm_imagination_config_t config = {};
    int result = nimcp_health_agent_update_wm_imagination_config(nullptr, &config);
    EXPECT_EQ(result, -1);
}

TEST_F(HealthAgentWorldImaginationTest, UpdateConfigNullConfig) {
    int result = nimcp_health_agent_update_wm_imagination_config(agent, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(HealthAgentWorldImaginationTest, UpdateConfigSuccess) {
    health_agent_wm_imagination_config_t config = {};
    nimcp_health_agent_wm_imagination_config_default(&config);
    config.jepa_error_warning = 0.5f;
    config.coherence_warning = 0.7f;

    int result = nimcp_health_agent_update_wm_imagination_config(agent, &config);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * Immediate Health Check Tests
 * ============================================================================ */

TEST_F(HealthAgentWorldImaginationTest, CheckWorldModelNowNullAgent) {
    int result = nimcp_health_agent_check_world_model_now(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(HealthAgentWorldImaginationTest, CheckWorldModelNowNoConnections) {
    int result = nimcp_health_agent_check_world_model_now(agent);
    EXPECT_EQ(result, 0);
}

TEST_F(HealthAgentWorldImaginationTest, CheckWorldModelNowWithJepa) {
    jepa_predictor_t* fake_jepa = reinterpret_cast<jepa_predictor_t*>(0x1234);
    nimcp_health_agent_connect_jepa(agent, fake_jepa, nullptr);

    int result = nimcp_health_agent_check_world_model_now(agent);
    EXPECT_EQ(result, 0);
}

TEST_F(HealthAgentWorldImaginationTest, CheckWorldModelNowWithWorldModel) {
    omni_world_model_t* fake_wm = reinterpret_cast<omni_world_model_t*>(0x5678);
    nimcp_health_agent_connect_world_model(agent, fake_wm, nullptr);

    int result = nimcp_health_agent_check_world_model_now(agent);
    EXPECT_EQ(result, 0);
}

TEST_F(HealthAgentWorldImaginationTest, CheckImaginationNowNullAgent) {
    int result = nimcp_health_agent_check_imagination_now(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(HealthAgentWorldImaginationTest, CheckImaginationNowNoConnections) {
    int result = nimcp_health_agent_check_imagination_now(agent);
    EXPECT_EQ(result, 0);
}

TEST_F(HealthAgentWorldImaginationTest, CheckImaginationNowWithImagination) {
    imagination_engine_t* fake_imag = reinterpret_cast<imagination_engine_t*>(0x9ABC);
    nimcp_health_agent_connect_imagination(agent, fake_imag, nullptr);

    int result = nimcp_health_agent_check_imagination_now(agent);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * Combined System Tests
 * ============================================================================ */

TEST_F(HealthAgentWorldImaginationTest, FullIntegrationCycle) {
    /* Connect all components */
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

    /* Check health scores are valid */
    float wm_score = nimcp_health_agent_get_world_model_health_score(agent);
    float imag_score = nimcp_health_agent_get_imagination_health_score(agent);
    EXPECT_GE(wm_score, 0.0f);
    EXPECT_LE(wm_score, 1.0f);
    EXPECT_GE(imag_score, 0.0f);
    EXPECT_LE(imag_score, 1.0f);

    /* Disconnect all */
    EXPECT_EQ(nimcp_health_agent_disconnect_jepa(agent), 0);
    EXPECT_EQ(nimcp_health_agent_disconnect_world_model(agent), 0);
    EXPECT_EQ(nimcp_health_agent_disconnect_imagination(agent), 0);
}

/* ============================================================================
 * Enumeration Tests
 * ============================================================================ */

TEST_F(HealthAgentWorldImaginationTest, WorldModelHealthStates) {
    /* Verify all health states are distinct */
    EXPECT_NE(WM_HEALTH_OPTIMAL, WM_HEALTH_DEGRADED);
    EXPECT_NE(WM_HEALTH_DEGRADED, WM_HEALTH_PREDICTION_DRIFT);
    EXPECT_NE(WM_HEALTH_PREDICTION_DRIFT, WM_HEALTH_EMBEDDING_COLLAPSE);
    EXPECT_NE(WM_HEALTH_EMBEDDING_COLLAPSE, WM_HEALTH_DYNAMICS_UNSTABLE);
    EXPECT_NE(WM_HEALTH_DYNAMICS_UNSTABLE, WM_HEALTH_HALLUCINATING);
    EXPECT_NE(WM_HEALTH_HALLUCINATING, WM_HEALTH_CRITICAL);
}

TEST_F(HealthAgentWorldImaginationTest, ImaginationHealthStates) {
    EXPECT_NE(IMAG_HEALTH_VIVID, IMAG_HEALTH_HAZY);
    EXPECT_NE(IMAG_HEALTH_HAZY, IMAG_HEALTH_FRAGMENTED);
    EXPECT_NE(IMAG_HEALTH_FRAGMENTED, IMAG_HEALTH_STUCK);
    EXPECT_NE(IMAG_HEALTH_STUCK, IMAG_HEALTH_CONFABULATING);
    EXPECT_NE(IMAG_HEALTH_CONFABULATING, IMAG_HEALTH_OVERACTIVE);
    EXPECT_NE(IMAG_HEALTH_OVERACTIVE, IMAG_HEALTH_IMPAIRED);
}

TEST_F(HealthAgentWorldImaginationTest, RecoveryActions) {
    EXPECT_EQ(WM_RECOVERY_NONE, 0);
    EXPECT_NE(WM_RECOVERY_RESET_PREDICTOR, WM_RECOVERY_NONE);
    EXPECT_NE(WM_RECOVERY_PRUNE_LATENT, WM_RECOVERY_RESET_PREDICTOR);
    EXPECT_NE(WM_RECOVERY_RETRAIN_DYNAMICS, WM_RECOVERY_PRUNE_LATENT);
    EXPECT_NE(WM_RECOVERY_CLEAR_WORKSPACE, WM_RECOVERY_RETRAIN_DYNAMICS);
}
