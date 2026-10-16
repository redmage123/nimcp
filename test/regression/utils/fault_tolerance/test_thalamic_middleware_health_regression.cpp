/**
 * @file test_thalamic_middleware_health_regression.cpp
 * @brief Regression tests for Phase 5.11: Thalamic/Middleware Health Integration
 *
 * Verifies API stability, struct completeness, and backward compatibility.
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "utils/fault_tolerance/nimcp_health_agent.h"
}

/**
 * @brief Regression test fixture for Thalamic/Middleware Health
 */
class ThalamicMiddlewareHealthRegressionTest : public ::testing::Test {
protected:
    nimcp_health_agent_t* agent = nullptr;

    void SetUp() override {
        health_agent_config_t config;
        nimcp_health_agent_default_config(&config);
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
 * API Function Signature Tests
 * ============================================================================ */

TEST_F(ThalamicMiddlewareHealthRegressionTest, ThalamicConfigDefaultSignature) {
    // Verify function signature hasn't changed
    void (*func_ptr)(health_agent_thalamic_config_t*) = &nimcp_health_agent_thalamic_config_default;
    EXPECT_NE(func_ptr, nullptr);
}

TEST_F(ThalamicMiddlewareHealthRegressionTest, MiddlewareConfigDefaultSignature) {
    void (*func_ptr)(health_agent_middleware_config_t*) = &nimcp_health_agent_middleware_config_default;
    EXPECT_NE(func_ptr, nullptr);
}

TEST_F(ThalamicMiddlewareHealthRegressionTest, ConnectThalamicSignature) {
    int (*func_ptr)(nimcp_health_agent_t*, omni_wm_thalamic_bridge_t*, const health_agent_thalamic_config_t*) =
        &nimcp_health_agent_connect_thalamic;
    EXPECT_NE(func_ptr, nullptr);
}

TEST_F(ThalamicMiddlewareHealthRegressionTest, DisconnectThalamicSignature) {
    int (*func_ptr)(nimcp_health_agent_t*, omni_wm_thalamic_bridge_t*) =
        &nimcp_health_agent_disconnect_thalamic;
    EXPECT_NE(func_ptr, nullptr);
}

TEST_F(ThalamicMiddlewareHealthRegressionTest, GetThalamicMetricsSignature) {
    int (*func_ptr)(const nimcp_health_agent_t*, thalamic_health_metrics_t*) =
        &nimcp_health_agent_get_thalamic_metrics;
    EXPECT_NE(func_ptr, nullptr);
}

TEST_F(ThalamicMiddlewareHealthRegressionTest, ThalamicRecoverySignature) {
    int (*func_ptr)(nimcp_health_agent_t*, thalamic_recovery_action_t, int) =
        &nimcp_health_agent_thalamic_recovery;
    EXPECT_NE(func_ptr, nullptr);
}

TEST_F(ThalamicMiddlewareHealthRegressionTest, ThalamicNeedsAttentionSignature) {
    bool (*func_ptr)(const nimcp_health_agent_t*) =
        &nimcp_health_agent_thalamic_needs_attention;
    EXPECT_NE(func_ptr, nullptr);
}

TEST_F(ThalamicMiddlewareHealthRegressionTest, GetThalamicHealthScoreSignature) {
    float (*func_ptr)(const nimcp_health_agent_t*) =
        &nimcp_health_agent_get_thalamic_health_score;
    EXPECT_NE(func_ptr, nullptr);
}

TEST_F(ThalamicMiddlewareHealthRegressionTest, UpdateThalamicConfigSignature) {
    int (*func_ptr)(nimcp_health_agent_t*, const health_agent_thalamic_config_t*) =
        &nimcp_health_agent_update_thalamic_config;
    EXPECT_NE(func_ptr, nullptr);
}

TEST_F(ThalamicMiddlewareHealthRegressionTest, ConnectMiddlewareSignature) {
    int (*func_ptr)(nimcp_health_agent_t*, nimcp_brain_training_ctx_t*, const health_agent_middleware_config_t*) =
        &nimcp_health_agent_connect_middleware;
    EXPECT_NE(func_ptr, nullptr);
}

TEST_F(ThalamicMiddlewareHealthRegressionTest, DisconnectMiddlewareSignature) {
    int (*func_ptr)(nimcp_health_agent_t*, nimcp_brain_training_ctx_t*) =
        &nimcp_health_agent_disconnect_middleware;
    EXPECT_NE(func_ptr, nullptr);
}

TEST_F(ThalamicMiddlewareHealthRegressionTest, GetMiddlewareMetricsSignature) {
    int (*func_ptr)(const nimcp_health_agent_t*, middleware_health_metrics_t*) =
        &nimcp_health_agent_get_middleware_metrics;
    EXPECT_NE(func_ptr, nullptr);
}

TEST_F(ThalamicMiddlewareHealthRegressionTest, MiddlewareRecoverySignature) {
    int (*func_ptr)(nimcp_health_agent_t*, middleware_recovery_action_t, int) =
        &nimcp_health_agent_middleware_recovery;
    EXPECT_NE(func_ptr, nullptr);
}

TEST_F(ThalamicMiddlewareHealthRegressionTest, MiddlewareNeedsAttentionSignature) {
    bool (*func_ptr)(const nimcp_health_agent_t*) =
        &nimcp_health_agent_middleware_needs_attention;
    EXPECT_NE(func_ptr, nullptr);
}

TEST_F(ThalamicMiddlewareHealthRegressionTest, GetMiddlewareHealthScoreSignature) {
    float (*func_ptr)(const nimcp_health_agent_t*) =
        &nimcp_health_agent_get_middleware_health_score;
    EXPECT_NE(func_ptr, nullptr);
}

TEST_F(ThalamicMiddlewareHealthRegressionTest, UpdateMiddlewareConfigSignature) {
    int (*func_ptr)(nimcp_health_agent_t*, const health_agent_middleware_config_t*) =
        &nimcp_health_agent_update_middleware_config;
    EXPECT_NE(func_ptr, nullptr);
}

/* ============================================================================
 * Thalamic Config Struct Field Tests
 * ============================================================================ */

TEST_F(ThalamicMiddlewareHealthRegressionTest, ThalamicConfigGatingFields) {
    health_agent_thalamic_config_t config;
    nimcp_health_agent_thalamic_config_default(&config);

    // Gating monitoring fields must exist
    EXPECT_TRUE(config.enable_gating_monitoring);
    EXPECT_FLOAT_EQ(config.min_gating_efficiency, 0.1f);
    EXPECT_FLOAT_EQ(config.attention_imbalance_threshold, 0.5f);
}

TEST_F(ThalamicMiddlewareHealthRegressionTest, ThalamicConfigPredictionFields) {
    health_agent_thalamic_config_t config;
    nimcp_health_agent_thalamic_config_default(&config);

    // Prediction monitoring fields must exist
    EXPECT_TRUE(config.enable_prediction_monitoring);
    EXPECT_FLOAT_EQ(config.min_bias_confidence, 0.3f);
    EXPECT_FLOAT_EQ(config.max_prediction_error, 0.8f);
}

TEST_F(ThalamicMiddlewareHealthRegressionTest, ThalamicConfigTRNFields) {
    health_agent_thalamic_config_t config;
    nimcp_health_agent_thalamic_config_default(&config);

    // TRN monitoring fields must exist
    EXPECT_TRUE(config.enable_trn_monitoring);
    EXPECT_FLOAT_EQ(config.trn_imbalance_threshold, 0.6f);
    EXPECT_FLOAT_EQ(config.max_inhibition_duration_ms, 1000.0f);
}

TEST_F(ThalamicMiddlewareHealthRegressionTest, ThalamicConfigTimingFields) {
    health_agent_thalamic_config_t config;
    nimcp_health_agent_thalamic_config_default(&config);

    // Timing monitoring fields must exist
    EXPECT_TRUE(config.enable_timing_monitoring);
    EXPECT_DOUBLE_EQ(config.max_update_time_ms, 10.0);
}

TEST_F(ThalamicMiddlewareHealthRegressionTest, ThalamicConfigAutoRecoveryFields) {
    health_agent_thalamic_config_t config;
    nimcp_health_agent_thalamic_config_default(&config);

    // Auto-recovery fields must exist
    EXPECT_TRUE(config.enable_auto_recovery);
    EXPECT_TRUE(config.enable_attention_rebalance);
    EXPECT_TRUE(config.enable_trn_release);
    EXPECT_TRUE(config.enable_arousal_adjustment);
}

/* ============================================================================
 * Middleware Config Struct Field Tests
 * ============================================================================ */

TEST_F(ThalamicMiddlewareHealthRegressionTest, MiddlewareConfigLossFields) {
    health_agent_middleware_config_t config;
    nimcp_health_agent_middleware_config_default(&config);

    // Loss monitoring fields must exist
    EXPECT_TRUE(config.enable_loss_monitoring);
    EXPECT_FLOAT_EQ(config.loss_explosion_threshold, 10.0f);
    EXPECT_FLOAT_EQ(config.loss_plateau_threshold, 0.001f);
    EXPECT_EQ(config.plateau_patience, 100u);
}

TEST_F(ThalamicMiddlewareHealthRegressionTest, MiddlewareConfigGradientFields) {
    health_agent_middleware_config_t config;
    nimcp_health_agent_middleware_config_default(&config);

    // Gradient monitoring fields must exist
    EXPECT_TRUE(config.enable_gradient_monitoring);
    EXPECT_FLOAT_EQ(config.max_gradient_norm, 10.0f);
    EXPECT_EQ(config.max_nan_count, 5u);
    EXPECT_FLOAT_EQ(config.high_clip_ratio_threshold, 0.5f);
}

TEST_F(ThalamicMiddlewareHealthRegressionTest, MiddlewareConfigLRFields) {
    health_agent_middleware_config_t config;
    nimcp_health_agent_middleware_config_default(&config);

    // Learning rate monitoring fields must exist
    EXPECT_TRUE(config.enable_lr_monitoring);
    EXPECT_FLOAT_EQ(config.lr_too_high_threshold, 0.1f);
    EXPECT_NEAR(config.lr_too_low_threshold, 1e-8f, 1e-10f);
}

TEST_F(ThalamicMiddlewareHealthRegressionTest, MiddlewareConfigTimingFields) {
    health_agent_middleware_config_t config;
    nimcp_health_agent_middleware_config_default(&config);

    // Timing monitoring fields must exist
    EXPECT_TRUE(config.enable_timing_monitoring);
    EXPECT_DOUBLE_EQ(config.max_batch_time_ms, 1000.0);
    EXPECT_FLOAT_EQ(config.timing_variance_threshold, 0.5f);
}

TEST_F(ThalamicMiddlewareHealthRegressionTest, MiddlewareConfigAutoRecoveryFields) {
    health_agent_middleware_config_t config;
    nimcp_health_agent_middleware_config_default(&config);

    // Auto-recovery fields must exist
    EXPECT_TRUE(config.enable_auto_recovery);
    EXPECT_TRUE(config.enable_lr_reduction);
    EXPECT_TRUE(config.enable_gradient_reset);
    EXPECT_TRUE(config.enable_auto_pause);
    EXPECT_TRUE(config.enable_auto_checkpoint);
}

/* ============================================================================
 * Thalamic Metrics Struct Field Tests
 * ============================================================================ */

TEST_F(ThalamicMiddlewareHealthRegressionTest, ThalamicMetricsConnectionFields) {
    thalamic_health_metrics_t metrics;
    memset(&metrics, 0xFF, sizeof(metrics));

    // Connection fields must be accessible
    metrics.num_bridges = 5;
    metrics.any_bridge_unhealthy = true;
    metrics.any_connection_lost = false;

    EXPECT_EQ(metrics.num_bridges, 5u);
    EXPECT_TRUE(metrics.any_bridge_unhealthy);
    EXPECT_FALSE(metrics.any_connection_lost);
}

TEST_F(ThalamicMiddlewareHealthRegressionTest, ThalamicMetricsGatingFields) {
    thalamic_health_metrics_t metrics;
    memset(&metrics, 0, sizeof(metrics));

    // Gating fields must be accessible
    metrics.total_inputs_gated = 1000;
    metrics.total_inputs_passed = 800;
    metrics.total_inputs_blocked = 200;
    metrics.avg_gating_attention = 0.75f;
    metrics.gating_efficiency = 0.8f;

    EXPECT_EQ(metrics.total_inputs_gated, 1000u);
    EXPECT_EQ(metrics.total_inputs_passed, 800u);
    EXPECT_EQ(metrics.total_inputs_blocked, 200u);
    EXPECT_FLOAT_EQ(metrics.avg_gating_attention, 0.75f);
    EXPECT_FLOAT_EQ(metrics.gating_efficiency, 0.8f);
}

TEST_F(ThalamicMiddlewareHealthRegressionTest, ThalamicMetricsNucleusFields) {
    thalamic_health_metrics_t metrics;
    memset(&metrics, 0, sizeof(metrics));

    // Nucleus attention fields must be accessible
    metrics.avg_lgn_attention = 0.5f;
    metrics.avg_mgn_attention = 0.6f;
    metrics.avg_pulvinar_attention = 0.7f;
    metrics.avg_md_attention = 0.4f;
    metrics.avg_trn_inhibition = 0.3f;
    metrics.avg_va_vl_attention = 0.55f;

    EXPECT_FLOAT_EQ(metrics.avg_lgn_attention, 0.5f);
    EXPECT_FLOAT_EQ(metrics.avg_mgn_attention, 0.6f);
    EXPECT_FLOAT_EQ(metrics.avg_pulvinar_attention, 0.7f);
    EXPECT_FLOAT_EQ(metrics.avg_md_attention, 0.4f);
    EXPECT_FLOAT_EQ(metrics.avg_trn_inhibition, 0.3f);
    EXPECT_FLOAT_EQ(metrics.avg_va_vl_attention, 0.55f);
}

TEST_F(ThalamicMiddlewareHealthRegressionTest, ThalamicMetricsAggregateFields) {
    thalamic_health_metrics_t metrics;
    memset(&metrics, 0, sizeof(metrics));

    // Aggregate fields must be accessible
    metrics.overall_thalamic_health = 85.5f;
    metrics.total_critical_events = 2;
    metrics.total_recoveries = 1;
    metrics.last_check_timestamp_us = 123456789;

    EXPECT_FLOAT_EQ(metrics.overall_thalamic_health, 85.5f);
    EXPECT_EQ(metrics.total_critical_events, 2u);
    EXPECT_EQ(metrics.total_recoveries, 1u);
    EXPECT_EQ(metrics.last_check_timestamp_us, 123456789u);
}

/* ============================================================================
 * Middleware Metrics Struct Field Tests
 * ============================================================================ */

TEST_F(ThalamicMiddlewareHealthRegressionTest, MiddlewareMetricsConnectionFields) {
    middleware_health_metrics_t metrics;
    memset(&metrics, 0xFF, sizeof(metrics));

    // Connection fields must be accessible
    metrics.num_contexts = 3;
    metrics.any_context_unhealthy = true;
    metrics.any_training_active = true;

    EXPECT_EQ(metrics.num_contexts, 3u);
    EXPECT_TRUE(metrics.any_context_unhealthy);
    EXPECT_TRUE(metrics.any_training_active);
}

TEST_F(ThalamicMiddlewareHealthRegressionTest, MiddlewareMetricsProgressFields) {
    middleware_health_metrics_t metrics;
    memset(&metrics, 0, sizeof(metrics));

    // Progress fields must be accessible
    metrics.total_epochs_completed = 100;
    metrics.total_batches_processed = 10000;
    metrics.total_samples_trained = 320000;
    metrics.total_weight_updates = 9500;

    EXPECT_EQ(metrics.total_epochs_completed, 100u);
    EXPECT_EQ(metrics.total_batches_processed, 10000u);
    EXPECT_EQ(metrics.total_samples_trained, 320000u);
    EXPECT_EQ(metrics.total_weight_updates, 9500u);
}

TEST_F(ThalamicMiddlewareHealthRegressionTest, MiddlewareMetricsLossFields) {
    middleware_health_metrics_t metrics;
    memset(&metrics, 0, sizeof(metrics));

    // Loss fields must be accessible
    metrics.avg_loss = 0.25f;
    metrics.min_loss = 0.1f;
    metrics.max_loss = 0.5f;
    metrics.loss_explosion_detected = false;
    metrics.loss_plateau_detected = true;

    EXPECT_FLOAT_EQ(metrics.avg_loss, 0.25f);
    EXPECT_FLOAT_EQ(metrics.min_loss, 0.1f);
    EXPECT_FLOAT_EQ(metrics.max_loss, 0.5f);
    EXPECT_FALSE(metrics.loss_explosion_detected);
    EXPECT_TRUE(metrics.loss_plateau_detected);
}

TEST_F(ThalamicMiddlewareHealthRegressionTest, MiddlewareMetricsGradientFields) {
    middleware_health_metrics_t metrics;
    memset(&metrics, 0, sizeof(metrics));

    // Gradient fields must be accessible
    metrics.total_gradient_clips = 50;
    metrics.total_nan_gradients = 2;
    metrics.total_inf_gradients = 1;
    metrics.gradient_health_critical = false;

    EXPECT_EQ(metrics.total_gradient_clips, 50u);
    EXPECT_EQ(metrics.total_nan_gradients, 2u);
    EXPECT_EQ(metrics.total_inf_gradients, 1u);
    EXPECT_FALSE(metrics.gradient_health_critical);
}

/* ============================================================================
 * Recovery Action Enum Stability Tests
 * ============================================================================ */

TEST_F(ThalamicMiddlewareHealthRegressionTest, ThalamicRecoveryEnumValues) {
    // Enum values must remain stable
    EXPECT_EQ(static_cast<int>(THALAMIC_RECOVERY_NONE), 0);
    EXPECT_EQ(static_cast<int>(THALAMIC_RECOVERY_RESET_ATTENTION), 1);
    EXPECT_EQ(static_cast<int>(THALAMIC_RECOVERY_REBALANCE_NUCLEI), 2);
    EXPECT_EQ(static_cast<int>(THALAMIC_RECOVERY_RELEASE_TRN), 3);
    EXPECT_EQ(static_cast<int>(THALAMIC_RECOVERY_BOOST_AROUSAL), 4);
    EXPECT_EQ(static_cast<int>(THALAMIC_RECOVERY_REDUCE_AROUSAL), 5);
    EXPECT_EQ(static_cast<int>(THALAMIC_RECOVERY_CLEAR_BIAS), 6);
    EXPECT_EQ(static_cast<int>(THALAMIC_RECOVERY_RESET_PULVINAR), 7);
    EXPECT_EQ(static_cast<int>(THALAMIC_RECOVERY_FORCE_TONIC), 8);
    EXPECT_EQ(static_cast<int>(THALAMIC_RECOVERY_RESET_STATS), 9);
    EXPECT_EQ(static_cast<int>(THALAMIC_RECOVERY_SOFT_RESET), 10);
    EXPECT_EQ(static_cast<int>(THALAMIC_RECOVERY_FULL_RESET), 11);
}

TEST_F(ThalamicMiddlewareHealthRegressionTest, MiddlewareRecoveryEnumValues) {
    // Enum values must remain stable
    EXPECT_EQ(static_cast<int>(MIDDLEWARE_RECOVERY_NONE), 0);
    EXPECT_EQ(static_cast<int>(MIDDLEWARE_RECOVERY_REDUCE_LR), 1);
    EXPECT_EQ(static_cast<int>(MIDDLEWARE_RECOVERY_INCREASE_LR), 2);
    EXPECT_EQ(static_cast<int>(MIDDLEWARE_RECOVERY_RESET_GRADIENTS), 3);
    EXPECT_EQ(static_cast<int>(MIDDLEWARE_RECOVERY_CLEAR_NAM), 4);
    EXPECT_EQ(static_cast<int>(MIDDLEWARE_RECOVERY_PAUSE_TRAINING), 5);
    EXPECT_EQ(static_cast<int>(MIDDLEWARE_RECOVERY_RESUME_TRAINING), 6);
    EXPECT_EQ(static_cast<int>(MIDDLEWARE_RECOVERY_SAVE_CHECKPOINT), 7);
    EXPECT_EQ(static_cast<int>(MIDDLEWARE_RECOVERY_LOAD_CHECKPOINT), 8);
    EXPECT_EQ(static_cast<int>(MIDDLEWARE_RECOVERY_RESET_EARLY_STOP), 9);
    EXPECT_EQ(static_cast<int>(MIDDLEWARE_RECOVERY_RESET_STATS), 10);
    EXPECT_EQ(static_cast<int>(MIDDLEWARE_RECOVERY_SOFT_RESET), 11);
    EXPECT_EQ(static_cast<int>(MIDDLEWARE_RECOVERY_FULL_RESET), 12);
}

/* ============================================================================
 * Constants Stability Tests
 * ============================================================================ */

TEST_F(ThalamicMiddlewareHealthRegressionTest, ThalamicMaxBridgesConstant) {
    // Constant must remain at least 8
    EXPECT_GE(HEALTH_AGENT_MAX_THALAMIC_BRIDGES, 8);
}

TEST_F(ThalamicMiddlewareHealthRegressionTest, MiddlewareMaxContextsConstant) {
    // Constant must remain at least 4
    EXPECT_GE(HEALTH_AGENT_MAX_TRAINING_CONTEXTS, 4);
}

/* ============================================================================
 * Error Handling Consistency Tests
 * ============================================================================ */

TEST_F(ThalamicMiddlewareHealthRegressionTest, ThalamicNullErrorHandling) {
    // All functions should handle NULL consistently
    EXPECT_EQ(nimcp_health_agent_connect_thalamic(nullptr, nullptr, nullptr), -1);
    EXPECT_EQ(nimcp_health_agent_disconnect_thalamic(nullptr, nullptr), -1);
    EXPECT_EQ(nimcp_health_agent_get_thalamic_metrics(nullptr, nullptr), -1);
    EXPECT_EQ(nimcp_health_agent_thalamic_recovery(nullptr, THALAMIC_RECOVERY_NONE, 0), -1);
    EXPECT_FALSE(nimcp_health_agent_thalamic_needs_attention(nullptr));
    EXPECT_FLOAT_EQ(nimcp_health_agent_get_thalamic_health_score(nullptr), 100.0f);
    EXPECT_EQ(nimcp_health_agent_update_thalamic_config(nullptr, nullptr), -1);
}

TEST_F(ThalamicMiddlewareHealthRegressionTest, MiddlewareNullErrorHandling) {
    // All functions should handle NULL consistently
    EXPECT_EQ(nimcp_health_agent_connect_middleware(nullptr, nullptr, nullptr), -1);
    EXPECT_EQ(nimcp_health_agent_disconnect_middleware(nullptr, nullptr), -1);
    EXPECT_EQ(nimcp_health_agent_get_middleware_metrics(nullptr, nullptr), -1);
    EXPECT_EQ(nimcp_health_agent_middleware_recovery(nullptr, MIDDLEWARE_RECOVERY_NONE, 0), -1);
    EXPECT_FALSE(nimcp_health_agent_middleware_needs_attention(nullptr));
    EXPECT_FLOAT_EQ(nimcp_health_agent_get_middleware_health_score(nullptr), 100.0f);
    EXPECT_EQ(nimcp_health_agent_update_middleware_config(nullptr, nullptr), -1);
}
