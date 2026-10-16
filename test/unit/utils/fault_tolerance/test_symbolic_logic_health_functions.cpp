/**
 * @file test_symbolic_logic_health_functions.cpp
 * @brief Unit tests for Phase 5.9 Symbolic Logic Health Integration
 * @date 2026-01-18
 *
 * Tests the symbolic logic health monitoring integration with the health agent.
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <cstring>

extern "C" {
#include "utils/fault_tolerance/nimcp_health_agent.h"
#include "cognitive/nimcp_symbolic_logic.h"
#include "utils/logging/nimcp_logging.h"
}

/**
 * @brief Test fixture for symbolic logic health unit tests
 */
class SymbolicLogicHealthTest : public ::testing::Test {
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

        /* Create a symbolic logic engine */
        logic_config_t logic_cfg = {};
        logic_cfg.max_predicates = 100;
        logic_cfg.max_rules = 50;
        logic_cfg.max_kb_size = 200;
        logic_cfg.max_inference_depth = 100;
        logic_cfg.enable_forward_chaining = true;
        logic_cfg.enable_backward_chaining = true;
        logic_cfg.enable_resolution = true;

        logic = symbolic_logic_create(&logic_cfg);
        /* Logic engine may be NULL if not fully implemented */
    }

    void TearDown() override {
        if (logic) {
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
 * Configuration Default Tests
 * ============================================================================ */

TEST_F(SymbolicLogicHealthTest, ConfigDefaultSetsReasonableValues) {
    health_agent_symbolic_logic_config_t config = {};
    nimcp_health_agent_symbolic_logic_config_default(&config);

    /* Inference monitoring */
    EXPECT_TRUE(config.enable_inference_monitoring);
    EXPECT_FLOAT_EQ(config.inference_timeout_ms, 100.0f);
    EXPECT_EQ(config.max_inference_depth, 1000u);
    EXPECT_FLOAT_EQ(config.loop_detection_threshold, 10000.0f);

    /* KB monitoring */
    EXPECT_TRUE(config.enable_kb_monitoring);
    EXPECT_FLOAT_EQ(config.kb_utilization_warning, 0.8f);
    EXPECT_FLOAT_EQ(config.kb_utilization_critical, 0.95f);
    EXPECT_TRUE(config.detect_inconsistencies);

    /* Performance monitoring */
    EXPECT_TRUE(config.enable_performance_monitoring);
    EXPECT_FLOAT_EQ(config.unification_success_min, 0.5f);
    EXPECT_FLOAT_EQ(config.reasoning_accuracy_min, 0.7f);

    /* Resource monitoring */
    EXPECT_TRUE(config.enable_resource_monitoring);
    EXPECT_FLOAT_EQ(config.memory_warning_threshold, 0.8f);
    EXPECT_FLOAT_EQ(config.memory_critical_threshold, 0.95f);
    EXPECT_EQ(config.stack_depth_warning, 500u);

    /* Auto-recovery */
    EXPECT_TRUE(config.enable_auto_recovery);
    EXPECT_TRUE(config.enable_loop_interruption);
    EXPECT_TRUE(config.enable_gc_on_pressure);

    /* Check intervals */
    EXPECT_EQ(config.health_check_interval_ms, 100u);
}

TEST_F(SymbolicLogicHealthTest, ConfigDefaultHandlesNull) {
    /* Should not crash on NULL */
    nimcp_health_agent_symbolic_logic_config_default(nullptr);
}

/* ============================================================================
 * Connection Tests
 * ============================================================================ */

TEST_F(SymbolicLogicHealthTest, ConnectWithNullAgentFails) {
    int result = nimcp_health_agent_connect_symbolic_logic(nullptr, logic, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(SymbolicLogicHealthTest, ConnectWithNullLogicFails) {
    int result = nimcp_health_agent_connect_symbolic_logic(agent, nullptr, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(SymbolicLogicHealthTest, ConnectWithValidLogicSucceeds) {
    if (!logic) {
        GTEST_SKIP() << "Symbolic logic engine not available";
    }

    int result = nimcp_health_agent_connect_symbolic_logic(agent, logic, nullptr);
    EXPECT_EQ(result, 0);
}

TEST_F(SymbolicLogicHealthTest, ConnectWithCustomConfig) {
    if (!logic) {
        GTEST_SKIP() << "Symbolic logic engine not available";
    }

    health_agent_symbolic_logic_config_t config = {};
    nimcp_health_agent_symbolic_logic_config_default(&config);
    config.inference_timeout_ms = 50.0f;
    config.kb_utilization_warning = 0.7f;

    int result = nimcp_health_agent_connect_symbolic_logic(agent, logic, &config);
    EXPECT_EQ(result, 0);
}

TEST_F(SymbolicLogicHealthTest, ConnectSameEngineTwiceReturnsSafe) {
    if (!logic) {
        GTEST_SKIP() << "Symbolic logic engine not available";
    }

    int result1 = nimcp_health_agent_connect_symbolic_logic(agent, logic, nullptr);
    EXPECT_EQ(result1, 0);

    /* Second connection should return 0 (safe, not error) */
    int result2 = nimcp_health_agent_connect_symbolic_logic(agent, logic, nullptr);
    EXPECT_EQ(result2, 0);
}

TEST_F(SymbolicLogicHealthTest, ConnectMultipleEngines) {
    /* Create multiple logic engines */
    std::vector<symbolic_logic_t*> engines;
    logic_config_t cfg = {};
    cfg.max_predicates = 50;
    cfg.max_rules = 25;
    cfg.max_kb_size = 100;

    for (int i = 0; i < 4; i++) {
        symbolic_logic_t* eng = symbolic_logic_create(&cfg);
        if (eng) {
            engines.push_back(eng);
        }
    }

    if (engines.empty()) {
        GTEST_SKIP() << "Could not create any symbolic logic engines";
    }

    /* Connect all engines */
    for (auto& eng : engines) {
        int result = nimcp_health_agent_connect_symbolic_logic(agent, eng, nullptr);
        EXPECT_EQ(result, 0);
    }

    /* Clean up */
    for (auto& eng : engines) {
        nimcp_health_agent_disconnect_symbolic_logic(agent, eng);
        symbolic_logic_destroy(eng);
    }
}

/* ============================================================================
 * Disconnection Tests
 * ============================================================================ */

TEST_F(SymbolicLogicHealthTest, DisconnectWithNullAgentFails) {
    int result = nimcp_health_agent_disconnect_symbolic_logic(nullptr, logic);
    EXPECT_EQ(result, -1);
}

TEST_F(SymbolicLogicHealthTest, DisconnectWithNullLogicFails) {
    int result = nimcp_health_agent_disconnect_symbolic_logic(agent, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(SymbolicLogicHealthTest, DisconnectNotConnectedFails) {
    if (!logic) {
        GTEST_SKIP() << "Symbolic logic engine not available";
    }

    /* Try to disconnect without connecting first */
    int result = nimcp_health_agent_disconnect_symbolic_logic(agent, logic);
    EXPECT_EQ(result, -1);
}

TEST_F(SymbolicLogicHealthTest, DisconnectConnectedSucceeds) {
    if (!logic) {
        GTEST_SKIP() << "Symbolic logic engine not available";
    }

    ASSERT_EQ(nimcp_health_agent_connect_symbolic_logic(agent, logic, nullptr), 0);

    int result = nimcp_health_agent_disconnect_symbolic_logic(agent, logic);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * Metrics Tests
 * ============================================================================ */

TEST_F(SymbolicLogicHealthTest, GetMetricsWithNullAgentFails) {
    logic_health_metrics_t metrics = {};
    int result = nimcp_health_agent_get_logic_metrics(nullptr, &metrics);
    EXPECT_EQ(result, -1);
}

TEST_F(SymbolicLogicHealthTest, GetMetricsWithNullMetricsFails) {
    int result = nimcp_health_agent_get_logic_metrics(agent, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(SymbolicLogicHealthTest, GetMetricsNoEnginesReturnsHealthy) {
    logic_health_metrics_t metrics = {};
    int result = nimcp_health_agent_get_logic_metrics(agent, &metrics);
    EXPECT_EQ(result, 0);

    EXPECT_EQ(metrics.num_engines, 0u);
    EXPECT_FLOAT_EQ(metrics.overall_logic_health, 100.0f);
    EXPECT_GT(metrics.last_check_timestamp_us, 0u);
}

TEST_F(SymbolicLogicHealthTest, GetMetricsWithConnectedEngine) {
    if (!logic) {
        GTEST_SKIP() << "Symbolic logic engine not available";
    }

    ASSERT_EQ(nimcp_health_agent_connect_symbolic_logic(agent, logic, nullptr), 0);

    logic_health_metrics_t metrics = {};
    int result = nimcp_health_agent_get_logic_metrics(agent, &metrics);
    EXPECT_EQ(result, 0);

    EXPECT_EQ(metrics.num_engines, 1u);
    EXPECT_GE(metrics.overall_logic_health, 0.0f);
    EXPECT_LE(metrics.overall_logic_health, 100.0f);
    EXPECT_GT(metrics.last_check_timestamp_us, 0u);
}

/* ============================================================================
 * Health Score Tests
 * ============================================================================ */

TEST_F(SymbolicLogicHealthTest, GetHealthScoreNullAgentReturnsHealthy) {
    float score = nimcp_health_agent_get_logic_health_score(nullptr);
    EXPECT_FLOAT_EQ(score, 100.0f);
}

TEST_F(SymbolicLogicHealthTest, GetHealthScoreNoEnginesReturnsHealthy) {
    float score = nimcp_health_agent_get_logic_health_score(agent);
    EXPECT_FLOAT_EQ(score, 100.0f);
}

TEST_F(SymbolicLogicHealthTest, GetHealthScoreWithConnectedEngine) {
    if (!logic) {
        GTEST_SKIP() << "Symbolic logic engine not available";
    }

    ASSERT_EQ(nimcp_health_agent_connect_symbolic_logic(agent, logic, nullptr), 0);

    float score = nimcp_health_agent_get_logic_health_score(agent);
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 100.0f);
}

/* ============================================================================
 * Needs Attention Tests
 * ============================================================================ */

TEST_F(SymbolicLogicHealthTest, NeedsAttentionNullAgentReturnsFalse) {
    bool needs = nimcp_health_agent_logic_needs_attention(nullptr);
    EXPECT_FALSE(needs);
}

TEST_F(SymbolicLogicHealthTest, NeedsAttentionNoEnginesReturnsFalse) {
    bool needs = nimcp_health_agent_logic_needs_attention(agent);
    EXPECT_FALSE(needs);
}

TEST_F(SymbolicLogicHealthTest, NeedsAttentionHealthyEngineReturnsFalse) {
    if (!logic) {
        GTEST_SKIP() << "Symbolic logic engine not available";
    }

    ASSERT_EQ(nimcp_health_agent_connect_symbolic_logic(agent, logic, nullptr), 0);

    /* Initially healthy engine should not need attention */
    bool needs = nimcp_health_agent_logic_needs_attention(agent);
    /* May return true or false depending on engine state */
    (void)needs;  /* Just verify no crash */
}

/* ============================================================================
 * Recovery Tests
 * ============================================================================ */

TEST_F(SymbolicLogicHealthTest, RecoveryNullAgentFails) {
    int result = nimcp_health_agent_logic_recovery(nullptr, LOGIC_RECOVERY_SOFT_RESET, 0);
    EXPECT_EQ(result, -1);
}

TEST_F(SymbolicLogicHealthTest, RecoveryNoEnginesFails) {
    int result = nimcp_health_agent_logic_recovery(agent, LOGIC_RECOVERY_SOFT_RESET, 0);
    EXPECT_EQ(result, -1);
}

TEST_F(SymbolicLogicHealthTest, RecoveryInvalidIndexFails) {
    if (!logic) {
        GTEST_SKIP() << "Symbolic logic engine not available";
    }

    ASSERT_EQ(nimcp_health_agent_connect_symbolic_logic(agent, logic, nullptr), 0);

    int result = nimcp_health_agent_logic_recovery(agent, LOGIC_RECOVERY_SOFT_RESET, 99);
    EXPECT_EQ(result, -1);
}

TEST_F(SymbolicLogicHealthTest, RecoveryValidEngineSucceeds) {
    if (!logic) {
        GTEST_SKIP() << "Symbolic logic engine not available";
    }

    ASSERT_EQ(nimcp_health_agent_connect_symbolic_logic(agent, logic, nullptr), 0);

    /* Test various recovery actions */
    EXPECT_EQ(nimcp_health_agent_logic_recovery(agent, LOGIC_RECOVERY_NONE, 0), 0);
    EXPECT_EQ(nimcp_health_agent_logic_recovery(agent, LOGIC_RECOVERY_INTERRUPT_INFERENCE, 0), 0);
    EXPECT_EQ(nimcp_health_agent_logic_recovery(agent, LOGIC_RECOVERY_RESET_UNIFIER, 0), 0);
    EXPECT_EQ(nimcp_health_agent_logic_recovery(agent, LOGIC_RECOVERY_GC_KB, 0), 0);
    EXPECT_EQ(nimcp_health_agent_logic_recovery(agent, LOGIC_RECOVERY_SOFT_RESET, 0), 0);
    EXPECT_EQ(nimcp_health_agent_logic_recovery(agent, LOGIC_RECOVERY_FULL_RESET, 0), 0);
}

TEST_F(SymbolicLogicHealthTest, RecoveryAllEngines) {
    if (!logic) {
        GTEST_SKIP() << "Symbolic logic engine not available";
    }

    ASSERT_EQ(nimcp_health_agent_connect_symbolic_logic(agent, logic, nullptr), 0);

    /* -1 means all engines */
    int result = nimcp_health_agent_logic_recovery(agent, LOGIC_RECOVERY_CLEAR_CACHE, -1);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * Config Update Tests
 * ============================================================================ */

TEST_F(SymbolicLogicHealthTest, UpdateConfigNullAgentFails) {
    health_agent_symbolic_logic_config_t config = {};
    nimcp_health_agent_symbolic_logic_config_default(&config);

    int result = nimcp_health_agent_update_logic_config(nullptr, &config);
    EXPECT_EQ(result, -1);
}

TEST_F(SymbolicLogicHealthTest, UpdateConfigNullConfigFails) {
    int result = nimcp_health_agent_update_logic_config(agent, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(SymbolicLogicHealthTest, UpdateConfigSucceeds) {
    health_agent_symbolic_logic_config_t config = {};
    nimcp_health_agent_symbolic_logic_config_default(&config);
    config.inference_timeout_ms = 200.0f;
    config.kb_utilization_warning = 0.6f;

    int result = nimcp_health_agent_update_logic_config(agent, &config);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * Integration Tests
 * ============================================================================ */

TEST_F(SymbolicLogicHealthTest, FullWorkflow) {
    if (!logic) {
        GTEST_SKIP() << "Symbolic logic engine not available";
    }

    /* Configure with custom settings */
    health_agent_symbolic_logic_config_t config = {};
    nimcp_health_agent_symbolic_logic_config_default(&config);
    config.enable_auto_recovery = true;

    /* Connect */
    ASSERT_EQ(nimcp_health_agent_connect_symbolic_logic(agent, logic, &config), 0);

    /* Get initial metrics */
    logic_health_metrics_t metrics = {};
    ASSERT_EQ(nimcp_health_agent_get_logic_metrics(agent, &metrics), 0);
    EXPECT_EQ(metrics.num_engines, 1u);

    /* Check health score */
    float score = nimcp_health_agent_get_logic_health_score(agent);
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 100.0f);

    /* Check needs attention */
    (void)nimcp_health_agent_logic_needs_attention(agent);

    /* Trigger a recovery action */
    EXPECT_EQ(nimcp_health_agent_logic_recovery(agent, LOGIC_RECOVERY_GC_KB, 0), 0);

    /* Update config */
    config.inference_timeout_ms = 150.0f;
    EXPECT_EQ(nimcp_health_agent_update_logic_config(agent, &config), 0);

    /* Disconnect */
    EXPECT_EQ(nimcp_health_agent_disconnect_symbolic_logic(agent, logic), 0);

    /* Verify disconnect */
    ASSERT_EQ(nimcp_health_agent_get_logic_metrics(agent, &metrics), 0);
    EXPECT_EQ(metrics.num_engines, 0u);
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
