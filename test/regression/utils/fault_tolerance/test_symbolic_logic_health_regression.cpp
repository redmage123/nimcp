/**
 * @file test_symbolic_logic_health_regression.cpp
 * @brief Regression tests for Phase 5.9 Symbolic Logic Health Integration
 * @date 2026-01-18
 *
 * Ensures backward compatibility and prevents regressions in
 * symbolic logic health monitoring functionality.
 */

#include <gtest/gtest.h>

extern "C" {
#include "utils/fault_tolerance/nimcp_health_agent.h"
#include "cognitive/nimcp_symbolic_logic.h"
#include "utils/logging/nimcp_logging.h"
}

/**
 * @brief Test fixture for symbolic logic health regression tests
 */
class SymbolicLogicHealthRegressionTest : public ::testing::Test {
protected:
    nimcp_health_agent_t* agent = nullptr;
    symbolic_logic_t* logic = nullptr;

    void SetUp() override {
        health_agent_config_t config = {};
        config.heartbeat_interval_ms = 100;
        config.message_queue_depth = 64;
        config.watchdog_timeout_ms = 5000;

        agent = nimcp_health_agent_create(&config);
        ASSERT_NE(agent, nullptr);

        logic_config_t logic_cfg = {};
        logic_cfg.max_predicates = 100;
        logic_cfg.max_rules = 50;
        logic_cfg.max_kb_size = 200;
        logic = symbolic_logic_create(&logic_cfg);
    }

    void TearDown() override {
        if (logic) {
            if (agent) {
                nimcp_health_agent_disconnect_symbolic_logic(agent, logic);
            }
            symbolic_logic_destroy(logic);
            logic = nullptr;
        }
        if (agent) {
            nimcp_health_agent_destroy(agent);
            agent = nullptr;
        }
    }
};

/* ============================================================================
 * API Stability Regression Tests
 * ============================================================================ */

TEST_F(SymbolicLogicHealthRegressionTest, ConfigDefaultAPIStable) {
    health_agent_symbolic_logic_config_t config = {};
    nimcp_health_agent_symbolic_logic_config_default(&config);

    /* Verify specific default values haven't changed */
    EXPECT_FLOAT_EQ(config.inference_timeout_ms, 100.0f);
    EXPECT_EQ(config.max_inference_depth, 1000u);
    EXPECT_FLOAT_EQ(config.loop_detection_threshold, 10000.0f);
    EXPECT_FLOAT_EQ(config.kb_utilization_warning, 0.8f);
    EXPECT_FLOAT_EQ(config.kb_utilization_critical, 0.95f);
    EXPECT_FLOAT_EQ(config.unification_success_min, 0.5f);
    EXPECT_FLOAT_EQ(config.reasoning_accuracy_min, 0.7f);
    EXPECT_FLOAT_EQ(config.memory_warning_threshold, 0.8f);
    EXPECT_FLOAT_EQ(config.memory_critical_threshold, 0.95f);
    EXPECT_EQ(config.stack_depth_warning, 500u);
    EXPECT_EQ(config.health_check_interval_ms, 100u);
}

TEST_F(SymbolicLogicHealthRegressionTest, ConnectAPIStable) {
    if (!logic) {
        GTEST_SKIP() << "Symbolic logic engine not available";
    }

    /* API must accept NULL config (uses defaults) */
    EXPECT_EQ(nimcp_health_agent_connect_symbolic_logic(agent, logic, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_disconnect_symbolic_logic(agent, logic), 0);

    /* API must accept valid config */
    health_agent_symbolic_logic_config_t config = {};
    nimcp_health_agent_symbolic_logic_config_default(&config);
    EXPECT_EQ(nimcp_health_agent_connect_symbolic_logic(agent, logic, &config), 0);
}

TEST_F(SymbolicLogicHealthRegressionTest, DisconnectAPIStable) {
    if (!logic) {
        GTEST_SKIP() << "Symbolic logic engine not available";
    }

    ASSERT_EQ(nimcp_health_agent_connect_symbolic_logic(agent, logic, nullptr), 0);

    /* Disconnect must return 0 on success */
    EXPECT_EQ(nimcp_health_agent_disconnect_symbolic_logic(agent, logic), 0);

    /* Disconnect again must return -1 */
    EXPECT_EQ(nimcp_health_agent_disconnect_symbolic_logic(agent, logic), -1);
}

TEST_F(SymbolicLogicHealthRegressionTest, GetMetricsAPIStable) {
    logic_health_metrics_t metrics = {};

    /* Must succeed with no engines */
    EXPECT_EQ(nimcp_health_agent_get_logic_metrics(agent, &metrics), 0);

    if (logic) {
        ASSERT_EQ(nimcp_health_agent_connect_symbolic_logic(agent, logic, nullptr), 0);

        /* Must succeed with engine */
        EXPECT_EQ(nimcp_health_agent_get_logic_metrics(agent, &metrics), 0);
        EXPECT_EQ(metrics.num_engines, 1u);
    }
}

TEST_F(SymbolicLogicHealthRegressionTest, GetHealthScoreAPIStable) {
    /* Must return 100.0f with no engines */
    EXPECT_FLOAT_EQ(nimcp_health_agent_get_logic_health_score(agent), 100.0f);

    if (logic) {
        ASSERT_EQ(nimcp_health_agent_connect_symbolic_logic(agent, logic, nullptr), 0);

        /* Must return value in [0, 100] with engine */
        float score = nimcp_health_agent_get_logic_health_score(agent);
        EXPECT_GE(score, 0.0f);
        EXPECT_LE(score, 100.0f);
    }
}

TEST_F(SymbolicLogicHealthRegressionTest, NeedsAttentionAPIStable) {
    /* Must return false with no engines */
    EXPECT_FALSE(nimcp_health_agent_logic_needs_attention(agent));

    if (logic) {
        ASSERT_EQ(nimcp_health_agent_connect_symbolic_logic(agent, logic, nullptr), 0);

        /* Must not crash with engine */
        (void)nimcp_health_agent_logic_needs_attention(agent);
    }
}

TEST_F(SymbolicLogicHealthRegressionTest, RecoveryAPIStable) {
    if (!logic) {
        GTEST_SKIP() << "Symbolic logic engine not available";
    }

    ASSERT_EQ(nimcp_health_agent_connect_symbolic_logic(agent, logic, nullptr), 0);

    /* All recovery actions must return 0 on success */
    EXPECT_EQ(nimcp_health_agent_logic_recovery(agent, LOGIC_RECOVERY_NONE, 0), 0);
    EXPECT_EQ(nimcp_health_agent_logic_recovery(agent, LOGIC_RECOVERY_INTERRUPT_INFERENCE, 0), 0);
    EXPECT_EQ(nimcp_health_agent_logic_recovery(agent, LOGIC_RECOVERY_RESET_UNIFIER, 0), 0);
    EXPECT_EQ(nimcp_health_agent_logic_recovery(agent, LOGIC_RECOVERY_GC_KB, 0), 0);
    EXPECT_EQ(nimcp_health_agent_logic_recovery(agent, LOGIC_RECOVERY_COMPACT_KB, 0), 0);
    EXPECT_EQ(nimcp_health_agent_logic_recovery(agent, LOGIC_RECOVERY_CLEAR_CACHE, 0), 0);
    EXPECT_EQ(nimcp_health_agent_logic_recovery(agent, LOGIC_RECOVERY_RESOLVE_INCONSISTENCY, 0), 0);
    EXPECT_EQ(nimcp_health_agent_logic_recovery(agent, LOGIC_RECOVERY_REDUCE_DEPTH, 0), 0);
    EXPECT_EQ(nimcp_health_agent_logic_recovery(agent, LOGIC_RECOVERY_CHECKPOINT_KB, 0), 0);
    EXPECT_EQ(nimcp_health_agent_logic_recovery(agent, LOGIC_RECOVERY_RESTORE_KB, 0), 0);
    EXPECT_EQ(nimcp_health_agent_logic_recovery(agent, LOGIC_RECOVERY_SOFT_RESET, 0), 0);
    EXPECT_EQ(nimcp_health_agent_logic_recovery(agent, LOGIC_RECOVERY_FULL_RESET, 0), 0);
}

TEST_F(SymbolicLogicHealthRegressionTest, UpdateConfigAPIStable) {
    health_agent_symbolic_logic_config_t config = {};
    nimcp_health_agent_symbolic_logic_config_default(&config);
    config.inference_timeout_ms = 200.0f;

    /* Must succeed */
    EXPECT_EQ(nimcp_health_agent_update_logic_config(agent, &config), 0);
}

/* ============================================================================
 * Data Structure Regression Tests
 * ============================================================================ */

TEST_F(SymbolicLogicHealthRegressionTest, ConfigStructureComplete) {
    health_agent_symbolic_logic_config_t config = {};
    nimcp_health_agent_symbolic_logic_config_default(&config);

    /* Verify all expected fields exist */
    (void)config.enable_inference_monitoring;
    (void)config.inference_timeout_ms;
    (void)config.max_inference_depth;
    (void)config.loop_detection_threshold;
    (void)config.enable_kb_monitoring;
    (void)config.kb_utilization_warning;
    (void)config.kb_utilization_critical;
    (void)config.detect_inconsistencies;
    (void)config.enable_performance_monitoring;
    (void)config.unification_success_min;
    (void)config.reasoning_accuracy_min;
    (void)config.enable_resource_monitoring;
    (void)config.memory_warning_threshold;
    (void)config.memory_critical_threshold;
    (void)config.stack_depth_warning;
    (void)config.enable_auto_recovery;
    (void)config.enable_loop_interruption;
    (void)config.enable_gc_on_pressure;
    (void)config.health_check_interval_ms;
}

TEST_F(SymbolicLogicHealthRegressionTest, MetricsStructureComplete) {
    logic_health_metrics_t metrics = {};
    nimcp_health_agent_get_logic_metrics(agent, &metrics);

    /* Verify all expected fields exist */
    (void)metrics.num_engines;
    (void)metrics.any_engine_unhealthy;
    (void)metrics.total_inferences;
    (void)metrics.failed_inferences;
    (void)metrics.infinite_loop_detections;
    (void)metrics.unification_failures;
    (void)metrics.avg_inference_time_ms;
    (void)metrics.max_inference_time_ms;
    (void)metrics.inference_overload;
    (void)metrics.total_facts;
    (void)metrics.total_rules;
    (void)metrics.kb_capacity;
    (void)metrics.kb_utilization;
    (void)metrics.kb_near_capacity;
    (void)metrics.inconsistencies_detected;
    (void)metrics.kb_corruptions;
    (void)metrics.reasoning_accuracy;
    (void)metrics.unification_success_rate;
    (void)metrics.resolution_steps;
    (void)metrics.resolution_timeouts;
    (void)metrics.reasoning_degraded;
    (void)metrics.memory_used_bytes;
    (void)metrics.memory_utilization;
    (void)metrics.stack_depth_max;
    (void)metrics.memory_pressure;
    (void)metrics.overall_logic_health;
    (void)metrics.total_anomalies;
    (void)metrics.total_recoveries;
    (void)metrics.last_check_timestamp_us;
}

/* ============================================================================
 * Enum Value Regression Tests
 * ============================================================================ */

TEST_F(SymbolicLogicHealthRegressionTest, RecoveryActionEnumValuesStable) {
    /* Verify enum values haven't changed */
    EXPECT_EQ(static_cast<int>(LOGIC_RECOVERY_NONE), 0);
    EXPECT_EQ(static_cast<int>(LOGIC_RECOVERY_INTERRUPT_INFERENCE), 1);
    EXPECT_EQ(static_cast<int>(LOGIC_RECOVERY_RESET_UNIFIER), 2);
    EXPECT_EQ(static_cast<int>(LOGIC_RECOVERY_GC_KB), 3);
    EXPECT_EQ(static_cast<int>(LOGIC_RECOVERY_COMPACT_KB), 4);
    EXPECT_EQ(static_cast<int>(LOGIC_RECOVERY_CLEAR_CACHE), 5);
    EXPECT_EQ(static_cast<int>(LOGIC_RECOVERY_RESOLVE_INCONSISTENCY), 6);
    EXPECT_EQ(static_cast<int>(LOGIC_RECOVERY_REDUCE_DEPTH), 7);
    EXPECT_EQ(static_cast<int>(LOGIC_RECOVERY_CHECKPOINT_KB), 8);
    EXPECT_EQ(static_cast<int>(LOGIC_RECOVERY_RESTORE_KB), 9);
    EXPECT_EQ(static_cast<int>(LOGIC_RECOVERY_SOFT_RESET), 10);
    EXPECT_EQ(static_cast<int>(LOGIC_RECOVERY_FULL_RESET), 11);
}

/* ============================================================================
 * Error Handling Regression Tests
 * ============================================================================ */

TEST_F(SymbolicLogicHealthRegressionTest, NullAgentHandling) {
    /* All functions must handle NULL agent gracefully */
    EXPECT_EQ(nimcp_health_agent_connect_symbolic_logic(nullptr, logic, nullptr), -1);
    EXPECT_EQ(nimcp_health_agent_disconnect_symbolic_logic(nullptr, logic), -1);
    EXPECT_EQ(nimcp_health_agent_get_logic_metrics(nullptr, nullptr), -1);
    EXPECT_EQ(nimcp_health_agent_logic_recovery(nullptr, LOGIC_RECOVERY_NONE, 0), -1);
    EXPECT_FALSE(nimcp_health_agent_logic_needs_attention(nullptr));
    EXPECT_FLOAT_EQ(nimcp_health_agent_get_logic_health_score(nullptr), 100.0f);
    EXPECT_EQ(nimcp_health_agent_update_logic_config(nullptr, nullptr), -1);
}

TEST_F(SymbolicLogicHealthRegressionTest, NullParameterHandling) {
    /* Functions must handle NULL parameters gracefully */
    nimcp_health_agent_symbolic_logic_config_default(nullptr);  /* Should not crash */
    EXPECT_EQ(nimcp_health_agent_connect_symbolic_logic(agent, nullptr, nullptr), -1);
    EXPECT_EQ(nimcp_health_agent_disconnect_symbolic_logic(agent, nullptr), -1);
    EXPECT_EQ(nimcp_health_agent_get_logic_metrics(agent, nullptr), -1);
    EXPECT_EQ(nimcp_health_agent_update_logic_config(agent, nullptr), -1);
}

/* ============================================================================
 * Default Value Regression Tests
 * ============================================================================ */

TEST_F(SymbolicLogicHealthRegressionTest, DefaultBooleanFlags) {
    health_agent_symbolic_logic_config_t config = {};
    nimcp_health_agent_symbolic_logic_config_default(&config);

    /* These defaults should remain stable */
    EXPECT_TRUE(config.enable_inference_monitoring);
    EXPECT_TRUE(config.enable_kb_monitoring);
    EXPECT_TRUE(config.detect_inconsistencies);
    EXPECT_TRUE(config.enable_performance_monitoring);
    EXPECT_TRUE(config.enable_resource_monitoring);
    EXPECT_TRUE(config.enable_auto_recovery);
    EXPECT_TRUE(config.enable_loop_interruption);
    EXPECT_TRUE(config.enable_gc_on_pressure);
}

/* ============================================================================
 * Backward Compatibility Tests
 * ============================================================================ */

TEST_F(SymbolicLogicHealthRegressionTest, HealthScoreRangeConsistent) {
    /* Health score should always be in [0, 100] */

    /* No engines case */
    float score = nimcp_health_agent_get_logic_health_score(agent);
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 100.0f);

    if (logic) {
        ASSERT_EQ(nimcp_health_agent_connect_symbolic_logic(agent, logic, nullptr), 0);

        /* With engine */
        score = nimcp_health_agent_get_logic_health_score(agent);
        EXPECT_GE(score, 0.0f);
        EXPECT_LE(score, 100.0f);
    }
}

TEST_F(SymbolicLogicHealthRegressionTest, MetricsTimestampConsistent) {
    logic_health_metrics_t metrics = {};
    ASSERT_EQ(nimcp_health_agent_get_logic_metrics(agent, &metrics), 0);

    /* Timestamp should always be set */
    EXPECT_GT(metrics.last_check_timestamp_us, 0u);
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
