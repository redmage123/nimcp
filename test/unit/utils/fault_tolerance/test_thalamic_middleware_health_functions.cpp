/**
 * @file test_thalamic_middleware_health_functions.cpp
 * @brief Unit tests for Phase 5.11: Thalamic/Middleware Health Integration
 *
 * Tests the health agent's thalamic bridge and middleware training monitoring APIs.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

extern "C" {
#include "utils/fault_tolerance/nimcp_health_agent.h"
#include "cognitive/omni/bridges/nimcp_omni_wm_thalamic_bridge.h"
#include "middleware/training/nimcp_brain_training_integration.h"
}

/**
 * @brief Test fixture for Thalamic/Middleware Health tests
 */
class ThalamicMiddlewareHealthTest : public ::testing::Test {
protected:
    nimcp_health_agent_t* agent = nullptr;

    void SetUp() override {
        health_agent_config_t config;
        nimcp_health_agent_default_config(&config);
        config.check_interval_ms = 50;
        agent = nimcp_health_agent_create(&config);
    }

    void TearDown() override {
        if (agent) {
            nimcp_health_agent_destroy(agent);
            agent = nullptr;
        }
    }
};

/* ============================================================================
 * Thalamic Health Configuration Tests
 * ============================================================================ */

TEST_F(ThalamicMiddlewareHealthTest, ThalamicConfigDefault) {
    health_agent_thalamic_config_t config;
    nimcp_health_agent_thalamic_config_default(&config);

    // Gating monitoring defaults
    EXPECT_TRUE(config.enable_gating_monitoring);
    EXPECT_FLOAT_EQ(config.min_gating_efficiency, 0.1f);
    EXPECT_FLOAT_EQ(config.attention_imbalance_threshold, 0.5f);
    EXPECT_EQ(config.max_blocked_ratio, 0.9f);

    // Prediction monitoring defaults
    EXPECT_TRUE(config.enable_prediction_monitoring);
    EXPECT_FLOAT_EQ(config.min_bias_confidence, 0.3f);
    EXPECT_FLOAT_EQ(config.max_prediction_error, 0.8f);

    // TRN monitoring defaults
    EXPECT_TRUE(config.enable_trn_monitoring);
    EXPECT_FLOAT_EQ(config.trn_imbalance_threshold, 0.6f);
    EXPECT_FLOAT_EQ(config.max_inhibition_duration_ms, 1000.0f);

    // Timing monitoring defaults
    EXPECT_TRUE(config.enable_timing_monitoring);
    EXPECT_DOUBLE_EQ(config.max_update_time_ms, 10.0);

    // Auto-recovery defaults
    EXPECT_TRUE(config.enable_auto_recovery);
    EXPECT_TRUE(config.enable_attention_rebalance);
    EXPECT_TRUE(config.enable_trn_release);
    EXPECT_TRUE(config.enable_arousal_adjustment);

    // Check intervals
    EXPECT_EQ(config.health_check_interval_ms, 100u);
}

TEST_F(ThalamicMiddlewareHealthTest, ThalamicConfigDefaultNullSafe) {
    // Should not crash with NULL
    nimcp_health_agent_thalamic_config_default(nullptr);
}

/* ============================================================================
 * Middleware Health Configuration Tests
 * ============================================================================ */

TEST_F(ThalamicMiddlewareHealthTest, MiddlewareConfigDefault) {
    health_agent_middleware_config_t config;
    nimcp_health_agent_middleware_config_default(&config);

    // Loss monitoring defaults
    EXPECT_TRUE(config.enable_loss_monitoring);
    EXPECT_FLOAT_EQ(config.loss_explosion_threshold, 10.0f);
    EXPECT_FLOAT_EQ(config.loss_plateau_threshold, 0.001f);
    EXPECT_EQ(config.plateau_patience, 100u);

    // Gradient monitoring defaults
    EXPECT_TRUE(config.enable_gradient_monitoring);
    EXPECT_FLOAT_EQ(config.max_gradient_norm, 10.0f);
    EXPECT_EQ(config.max_nan_count, 5u);
    EXPECT_FLOAT_EQ(config.high_clip_ratio_threshold, 0.5f);

    // Learning rate monitoring defaults
    EXPECT_TRUE(config.enable_lr_monitoring);
    EXPECT_FLOAT_EQ(config.lr_too_high_threshold, 0.1f);
    EXPECT_NEAR(config.lr_too_low_threshold, 1e-8f, 1e-10f);

    // Timing monitoring defaults
    EXPECT_TRUE(config.enable_timing_monitoring);
    EXPECT_DOUBLE_EQ(config.max_batch_time_ms, 1000.0);
    EXPECT_FLOAT_EQ(config.timing_variance_threshold, 0.5f);

    // Auto-recovery defaults
    EXPECT_TRUE(config.enable_auto_recovery);
    EXPECT_TRUE(config.enable_lr_reduction);
    EXPECT_TRUE(config.enable_gradient_reset);
    EXPECT_TRUE(config.enable_auto_pause);
    EXPECT_TRUE(config.enable_auto_checkpoint);

    // Check intervals
    EXPECT_EQ(config.health_check_interval_ms, 100u);
}

TEST_F(ThalamicMiddlewareHealthTest, MiddlewareConfigDefaultNullSafe) {
    // Should not crash with NULL
    nimcp_health_agent_middleware_config_default(nullptr);
}

/* ============================================================================
 * Thalamic Connection Tests
 * ============================================================================ */

TEST_F(ThalamicMiddlewareHealthTest, ConnectThalamicNullAgent) {
    omni_wm_thalamic_bridge_t bridge;
    EXPECT_EQ(nimcp_health_agent_connect_thalamic(nullptr, &bridge, nullptr), -1);
}

TEST_F(ThalamicMiddlewareHealthTest, ConnectThalamicNullBridge) {
    ASSERT_NE(agent, nullptr);
    EXPECT_EQ(nimcp_health_agent_connect_thalamic(agent, nullptr, nullptr), -1);
}

TEST_F(ThalamicMiddlewareHealthTest, DisconnectThalamicNullAgent) {
    omni_wm_thalamic_bridge_t bridge;
    EXPECT_EQ(nimcp_health_agent_disconnect_thalamic(nullptr, &bridge), -1);
}

TEST_F(ThalamicMiddlewareHealthTest, DisconnectThalamicNullBridge) {
    ASSERT_NE(agent, nullptr);
    EXPECT_EQ(nimcp_health_agent_disconnect_thalamic(agent, nullptr), -1);
}

TEST_F(ThalamicMiddlewareHealthTest, DisconnectThalamicNotFound) {
    ASSERT_NE(agent, nullptr);
    omni_wm_thalamic_bridge_t bridge;
    // Disconnect without prior connect should return -1
    EXPECT_EQ(nimcp_health_agent_disconnect_thalamic(agent, &bridge), -1);
}

/* ============================================================================
 * Middleware Connection Tests
 * ============================================================================ */

TEST_F(ThalamicMiddlewareHealthTest, ConnectMiddlewareNullAgent) {
    // Use a mock pointer since the actual type is opaque
    nimcp_brain_training_ctx_t* ctx = (nimcp_brain_training_ctx_t*)0x1234;
    EXPECT_EQ(nimcp_health_agent_connect_middleware(nullptr, ctx, nullptr), -1);
}

TEST_F(ThalamicMiddlewareHealthTest, ConnectMiddlewareNullContext) {
    ASSERT_NE(agent, nullptr);
    EXPECT_EQ(nimcp_health_agent_connect_middleware(agent, nullptr, nullptr), -1);
}

TEST_F(ThalamicMiddlewareHealthTest, DisconnectMiddlewareNullAgent) {
    nimcp_brain_training_ctx_t* ctx = (nimcp_brain_training_ctx_t*)0x1234;
    EXPECT_EQ(nimcp_health_agent_disconnect_middleware(nullptr, ctx), -1);
}

TEST_F(ThalamicMiddlewareHealthTest, DisconnectMiddlewareNullContext) {
    ASSERT_NE(agent, nullptr);
    EXPECT_EQ(nimcp_health_agent_disconnect_middleware(agent, nullptr), -1);
}

TEST_F(ThalamicMiddlewareHealthTest, DisconnectMiddlewareNotFound) {
    ASSERT_NE(agent, nullptr);
    nimcp_brain_training_ctx_t* ctx = (nimcp_brain_training_ctx_t*)0x1234;
    // Disconnect without prior connect should return -1
    EXPECT_EQ(nimcp_health_agent_disconnect_middleware(agent, ctx), -1);
}

/* ============================================================================
 * Thalamic Metrics Tests
 * ============================================================================ */

TEST_F(ThalamicMiddlewareHealthTest, GetThalamicMetricsNullAgent) {
    thalamic_health_metrics_t metrics;
    EXPECT_EQ(nimcp_health_agent_get_thalamic_metrics(nullptr, &metrics), -1);
}

TEST_F(ThalamicMiddlewareHealthTest, GetThalamicMetricsNullMetrics) {
    ASSERT_NE(agent, nullptr);
    EXPECT_EQ(nimcp_health_agent_get_thalamic_metrics(agent, nullptr), -1);
}

TEST_F(ThalamicMiddlewareHealthTest, GetThalamicMetricsNoBridges) {
    ASSERT_NE(agent, nullptr);
    thalamic_health_metrics_t metrics;
    EXPECT_EQ(nimcp_health_agent_get_thalamic_metrics(agent, &metrics), 0);
    EXPECT_EQ(metrics.num_bridges, 0u);
    EXPECT_FLOAT_EQ(metrics.overall_thalamic_health, 100.0f);
}

/* ============================================================================
 * Middleware Metrics Tests
 * ============================================================================ */

TEST_F(ThalamicMiddlewareHealthTest, GetMiddlewareMetricsNullAgent) {
    middleware_health_metrics_t metrics;
    EXPECT_EQ(nimcp_health_agent_get_middleware_metrics(nullptr, &metrics), -1);
}

TEST_F(ThalamicMiddlewareHealthTest, GetMiddlewareMetricsNullMetrics) {
    ASSERT_NE(agent, nullptr);
    EXPECT_EQ(nimcp_health_agent_get_middleware_metrics(agent, nullptr), -1);
}

TEST_F(ThalamicMiddlewareHealthTest, GetMiddlewareMetricsNoContexts) {
    ASSERT_NE(agent, nullptr);
    middleware_health_metrics_t metrics;
    EXPECT_EQ(nimcp_health_agent_get_middleware_metrics(agent, &metrics), 0);
    EXPECT_EQ(metrics.num_contexts, 0u);
    EXPECT_FLOAT_EQ(metrics.overall_middleware_health, 100.0f);
}

/* ============================================================================
 * Thalamic Health Score Tests
 * ============================================================================ */

TEST_F(ThalamicMiddlewareHealthTest, GetThalamicHealthScoreNullAgent) {
    float score = nimcp_health_agent_get_thalamic_health_score(nullptr);
    EXPECT_FLOAT_EQ(score, 100.0f);
}

TEST_F(ThalamicMiddlewareHealthTest, GetThalamicHealthScoreNoBridges) {
    ASSERT_NE(agent, nullptr);
    float score = nimcp_health_agent_get_thalamic_health_score(agent);
    EXPECT_FLOAT_EQ(score, 100.0f);
}

/* ============================================================================
 * Middleware Health Score Tests
 * ============================================================================ */

TEST_F(ThalamicMiddlewareHealthTest, GetMiddlewareHealthScoreNullAgent) {
    float score = nimcp_health_agent_get_middleware_health_score(nullptr);
    EXPECT_FLOAT_EQ(score, 100.0f);
}

TEST_F(ThalamicMiddlewareHealthTest, GetMiddlewareHealthScoreNoContexts) {
    ASSERT_NE(agent, nullptr);
    float score = nimcp_health_agent_get_middleware_health_score(agent);
    EXPECT_FLOAT_EQ(score, 100.0f);
}

/* ============================================================================
 * Thalamic Needs Attention Tests
 * ============================================================================ */

TEST_F(ThalamicMiddlewareHealthTest, ThalamicNeedsAttentionNullAgent) {
    EXPECT_FALSE(nimcp_health_agent_thalamic_needs_attention(nullptr));
}

TEST_F(ThalamicMiddlewareHealthTest, ThalamicNeedsAttentionNoBridges) {
    ASSERT_NE(agent, nullptr);
    EXPECT_FALSE(nimcp_health_agent_thalamic_needs_attention(agent));
}

/* ============================================================================
 * Middleware Needs Attention Tests
 * ============================================================================ */

TEST_F(ThalamicMiddlewareHealthTest, MiddlewareNeedsAttentionNullAgent) {
    EXPECT_FALSE(nimcp_health_agent_middleware_needs_attention(nullptr));
}

TEST_F(ThalamicMiddlewareHealthTest, MiddlewareNeedsAttentionNoContexts) {
    ASSERT_NE(agent, nullptr);
    EXPECT_FALSE(nimcp_health_agent_middleware_needs_attention(agent));
}

/* ============================================================================
 * Thalamic Recovery Tests
 * ============================================================================ */

TEST_F(ThalamicMiddlewareHealthTest, ThalamicRecoveryNullAgent) {
    EXPECT_EQ(nimcp_health_agent_thalamic_recovery(nullptr, THALAMIC_RECOVERY_RESET_ATTENTION, 0), -1);
}

TEST_F(ThalamicMiddlewareHealthTest, ThalamicRecoveryNoBridges) {
    ASSERT_NE(agent, nullptr);
    EXPECT_EQ(nimcp_health_agent_thalamic_recovery(agent, THALAMIC_RECOVERY_RESET_ATTENTION, 0), -1);
}

TEST_F(ThalamicMiddlewareHealthTest, ThalamicRecoveryNone) {
    ASSERT_NE(agent, nullptr);
    // NONE action should return 0 (success, nothing to do)
    EXPECT_EQ(nimcp_health_agent_thalamic_recovery(agent, THALAMIC_RECOVERY_NONE, 0), 0);
}

/* ============================================================================
 * Middleware Recovery Tests
 * ============================================================================ */

TEST_F(ThalamicMiddlewareHealthTest, MiddlewareRecoveryNullAgent) {
    EXPECT_EQ(nimcp_health_agent_middleware_recovery(nullptr, MIDDLEWARE_RECOVERY_REDUCE_LR, 0), -1);
}

TEST_F(ThalamicMiddlewareHealthTest, MiddlewareRecoveryNoContexts) {
    ASSERT_NE(agent, nullptr);
    EXPECT_EQ(nimcp_health_agent_middleware_recovery(agent, MIDDLEWARE_RECOVERY_REDUCE_LR, 0), -1);
}

TEST_F(ThalamicMiddlewareHealthTest, MiddlewareRecoveryNone) {
    ASSERT_NE(agent, nullptr);
    // NONE action should return 0 (success, nothing to do)
    EXPECT_EQ(nimcp_health_agent_middleware_recovery(agent, MIDDLEWARE_RECOVERY_NONE, 0), 0);
}

/* ============================================================================
 * Thalamic Config Update Tests
 * ============================================================================ */

TEST_F(ThalamicMiddlewareHealthTest, UpdateThalamicConfigNullAgent) {
    health_agent_thalamic_config_t config;
    nimcp_health_agent_thalamic_config_default(&config);
    EXPECT_EQ(nimcp_health_agent_update_thalamic_config(nullptr, &config), -1);
}

TEST_F(ThalamicMiddlewareHealthTest, UpdateThalamicConfigNullConfig) {
    ASSERT_NE(agent, nullptr);
    EXPECT_EQ(nimcp_health_agent_update_thalamic_config(agent, nullptr), -1);
}

TEST_F(ThalamicMiddlewareHealthTest, UpdateThalamicConfigValid) {
    ASSERT_NE(agent, nullptr);
    health_agent_thalamic_config_t config;
    nimcp_health_agent_thalamic_config_default(&config);
    config.max_update_time_ms = 20.0;
    EXPECT_EQ(nimcp_health_agent_update_thalamic_config(agent, &config), 0);
}

/* ============================================================================
 * Middleware Config Update Tests
 * ============================================================================ */

TEST_F(ThalamicMiddlewareHealthTest, UpdateMiddlewareConfigNullAgent) {
    health_agent_middleware_config_t config;
    nimcp_health_agent_middleware_config_default(&config);
    EXPECT_EQ(nimcp_health_agent_update_middleware_config(nullptr, &config), -1);
}

TEST_F(ThalamicMiddlewareHealthTest, UpdateMiddlewareConfigNullConfig) {
    ASSERT_NE(agent, nullptr);
    EXPECT_EQ(nimcp_health_agent_update_middleware_config(agent, nullptr), -1);
}

TEST_F(ThalamicMiddlewareHealthTest, UpdateMiddlewareConfigValid) {
    ASSERT_NE(agent, nullptr);
    health_agent_middleware_config_t config;
    nimcp_health_agent_middleware_config_default(&config);
    config.max_batch_time_ms = 2000.0;
    EXPECT_EQ(nimcp_health_agent_update_middleware_config(agent, &config), 0);
}

/* ============================================================================
 * Metrics Structure Completeness Tests
 * ============================================================================ */

TEST_F(ThalamicMiddlewareHealthTest, ThalamicMetricsStructComplete) {
    thalamic_health_metrics_t metrics;
    memset(&metrics, 0, sizeof(metrics));

    // Connection fields
    EXPECT_EQ(metrics.num_bridges, 0u);
    EXPECT_FALSE(metrics.any_bridge_unhealthy);
    EXPECT_FALSE(metrics.any_connection_lost);

    // Gating fields
    EXPECT_EQ(metrics.total_inputs_gated, 0u);
    EXPECT_EQ(metrics.total_inputs_passed, 0u);
    EXPECT_EQ(metrics.total_inputs_blocked, 0u);
    EXPECT_FLOAT_EQ(metrics.avg_gating_attention, 0.0f);
    EXPECT_FLOAT_EQ(metrics.gating_efficiency, 0.0f);

    // Nucleus fields
    EXPECT_FLOAT_EQ(metrics.avg_lgn_attention, 0.0f);
    EXPECT_FLOAT_EQ(metrics.avg_mgn_attention, 0.0f);
    EXPECT_FLOAT_EQ(metrics.avg_pulvinar_attention, 0.0f);
    EXPECT_FLOAT_EQ(metrics.avg_md_attention, 0.0f);
    EXPECT_FLOAT_EQ(metrics.avg_trn_inhibition, 0.0f);
    EXPECT_FLOAT_EQ(metrics.avg_va_vl_attention, 0.0f);

    // Aggregate fields
    EXPECT_FLOAT_EQ(metrics.overall_thalamic_health, 0.0f);
}

TEST_F(ThalamicMiddlewareHealthTest, MiddlewareMetricsStructComplete) {
    middleware_health_metrics_t metrics;
    memset(&metrics, 0, sizeof(metrics));

    // Connection fields
    EXPECT_EQ(metrics.num_contexts, 0u);
    EXPECT_FALSE(metrics.any_context_unhealthy);
    EXPECT_FALSE(metrics.any_training_active);

    // Training progress fields
    EXPECT_EQ(metrics.total_epochs_completed, 0u);
    EXPECT_EQ(metrics.total_batches_processed, 0u);
    EXPECT_EQ(metrics.total_samples_trained, 0u);
    EXPECT_EQ(metrics.total_weight_updates, 0u);

    // Loss fields
    EXPECT_FLOAT_EQ(metrics.avg_loss, 0.0f);
    EXPECT_FALSE(metrics.loss_explosion_detected);
    EXPECT_FALSE(metrics.loss_plateau_detected);

    // Gradient fields
    EXPECT_EQ(metrics.total_gradient_clips, 0u);
    EXPECT_EQ(metrics.total_nan_gradients, 0u);
    EXPECT_EQ(metrics.total_inf_gradients, 0u);
    EXPECT_FALSE(metrics.gradient_health_critical);

    // Aggregate fields
    EXPECT_FLOAT_EQ(metrics.overall_middleware_health, 0.0f);
}

/* ============================================================================
 * Recovery Action Enum Tests
 * ============================================================================ */

TEST_F(ThalamicMiddlewareHealthTest, ThalamicRecoveryActionsEnumValid) {
    // Verify all thalamic recovery actions are defined
    EXPECT_EQ(THALAMIC_RECOVERY_NONE, 0);
    EXPECT_EQ(THALAMIC_RECOVERY_RESET_ATTENTION, 1);
    EXPECT_EQ(THALAMIC_RECOVERY_REBALANCE_NUCLEI, 2);
    EXPECT_EQ(THALAMIC_RECOVERY_RELEASE_TRN, 3);
    EXPECT_EQ(THALAMIC_RECOVERY_BOOST_AROUSAL, 4);
    EXPECT_EQ(THALAMIC_RECOVERY_REDUCE_AROUSAL, 5);
    EXPECT_EQ(THALAMIC_RECOVERY_CLEAR_BIAS, 6);
    EXPECT_EQ(THALAMIC_RECOVERY_RESET_PULVINAR, 7);
    EXPECT_EQ(THALAMIC_RECOVERY_FORCE_TONIC, 8);
    EXPECT_EQ(THALAMIC_RECOVERY_RESET_STATS, 9);
    EXPECT_EQ(THALAMIC_RECOVERY_SOFT_RESET, 10);
    EXPECT_EQ(THALAMIC_RECOVERY_FULL_RESET, 11);
}

TEST_F(ThalamicMiddlewareHealthTest, MiddlewareRecoveryActionsEnumValid) {
    // Verify all middleware recovery actions are defined
    EXPECT_EQ(MIDDLEWARE_RECOVERY_NONE, 0);
    EXPECT_EQ(MIDDLEWARE_RECOVERY_REDUCE_LR, 1);
    EXPECT_EQ(MIDDLEWARE_RECOVERY_INCREASE_LR, 2);
    EXPECT_EQ(MIDDLEWARE_RECOVERY_RESET_GRADIENTS, 3);
    EXPECT_EQ(MIDDLEWARE_RECOVERY_CLEAR_NAM, 4);
    EXPECT_EQ(MIDDLEWARE_RECOVERY_PAUSE_TRAINING, 5);
    EXPECT_EQ(MIDDLEWARE_RECOVERY_RESUME_TRAINING, 6);
    EXPECT_EQ(MIDDLEWARE_RECOVERY_SAVE_CHECKPOINT, 7);
    EXPECT_EQ(MIDDLEWARE_RECOVERY_LOAD_CHECKPOINT, 8);
    EXPECT_EQ(MIDDLEWARE_RECOVERY_RESET_EARLY_STOP, 9);
    EXPECT_EQ(MIDDLEWARE_RECOVERY_RESET_STATS, 10);
    EXPECT_EQ(MIDDLEWARE_RECOVERY_SOFT_RESET, 11);
    EXPECT_EQ(MIDDLEWARE_RECOVERY_FULL_RESET, 12);
}

/* ============================================================================
 * Config Struct Size Tests
 * ============================================================================ */

TEST_F(ThalamicMiddlewareHealthTest, ThalamicConfigStructSize) {
    // Ensure the config struct is reasonably sized
    EXPECT_GT(sizeof(health_agent_thalamic_config_t), 0u);
    EXPECT_LT(sizeof(health_agent_thalamic_config_t), 1024u);
}

TEST_F(ThalamicMiddlewareHealthTest, MiddlewareConfigStructSize) {
    // Ensure the config struct is reasonably sized
    EXPECT_GT(sizeof(health_agent_middleware_config_t), 0u);
    EXPECT_LT(sizeof(health_agent_middleware_config_t), 1024u);
}

TEST_F(ThalamicMiddlewareHealthTest, ThalamicMetricsStructSize) {
    // Ensure the metrics struct is reasonably sized
    EXPECT_GT(sizeof(thalamic_health_metrics_t), 0u);
    EXPECT_LT(sizeof(thalamic_health_metrics_t), 4096u);
}

TEST_F(ThalamicMiddlewareHealthTest, MiddlewareMetricsStructSize) {
    // Ensure the metrics struct is reasonably sized
    EXPECT_GT(sizeof(middleware_health_metrics_t), 0u);
    EXPECT_LT(sizeof(middleware_health_metrics_t), 4096u);
}
