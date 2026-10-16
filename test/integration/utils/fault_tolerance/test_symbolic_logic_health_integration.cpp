/**
 * @file test_symbolic_logic_health_integration.cpp
 * @brief Integration tests for Phase 5.9 Symbolic Logic Health Integration
 * @date 2026-01-18
 *
 * Tests the integration of symbolic logic health monitoring with the
 * health agent and immune system.
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>

extern "C" {
#include "utils/fault_tolerance/nimcp_health_agent.h"
#include "cognitive/nimcp_symbolic_logic.h"
#include "utils/logging/nimcp_logging.h"
}

/**
 * @brief Test fixture for symbolic logic health integration tests
 */
class SymbolicLogicHealthIntegrationTest : public ::testing::Test {
protected:
    nimcp_health_agent_t* agent = nullptr;
    std::vector<symbolic_logic_t*> engines;

    void SetUp() override {
        health_agent_config_t config = {};
        config.heartbeat_interval_ms = 100;
        config.message_queue_depth = 64;
        config.watchdog_timeout_ms = 5000;

        agent = nimcp_health_agent_create(&config);
        ASSERT_NE(agent, nullptr);
    }

    void TearDown() override {
        /* Disconnect and destroy all engines */
        for (auto& eng : engines) {
            if (eng) {
                nimcp_health_agent_disconnect_symbolic_logic(agent, eng);
                symbolic_logic_destroy(eng);
            }
        }
        engines.clear();

        if (agent) {
            nimcp_health_agent_destroy(agent);
            agent = nullptr;
        }
    }

    symbolic_logic_t* createEngine() {
        logic_config_t cfg = {};
        cfg.max_predicates = 100;
        cfg.max_rules = 50;
        cfg.max_kb_size = 200;
        cfg.max_inference_depth = 100;
        cfg.enable_forward_chaining = true;
        cfg.enable_backward_chaining = true;
        cfg.enable_resolution = true;

        symbolic_logic_t* eng = symbolic_logic_create(&cfg);
        if (eng) {
            engines.push_back(eng);
        }
        return eng;
    }
};

/* ============================================================================
 * Multi-Engine Integration Tests
 * ============================================================================ */

TEST_F(SymbolicLogicHealthIntegrationTest, MultipleEnginesMonitoring) {
    /* Create multiple engines */
    const int NUM_ENGINES = 4;
    int connected = 0;

    for (int i = 0; i < NUM_ENGINES; i++) {
        symbolic_logic_t* eng = createEngine();
        if (eng) {
            int result = nimcp_health_agent_connect_symbolic_logic(agent, eng, nullptr);
            if (result == 0) connected++;
        }
    }

    if (connected == 0) {
        GTEST_SKIP() << "Could not create any symbolic logic engines";
    }

    /* Get metrics for all engines */
    logic_health_metrics_t metrics = {};
    ASSERT_EQ(nimcp_health_agent_get_logic_metrics(agent, &metrics), 0);
    EXPECT_EQ(metrics.num_engines, (uint32_t)connected);
    EXPECT_GE(metrics.overall_logic_health, 0.0f);
    EXPECT_LE(metrics.overall_logic_health, 100.0f);
}

TEST_F(SymbolicLogicHealthIntegrationTest, HealthCheckCycle) {
    symbolic_logic_t* eng = createEngine();
    if (!eng) {
        GTEST_SKIP() << "Symbolic logic engine not available";
    }

    ASSERT_EQ(nimcp_health_agent_connect_symbolic_logic(agent, eng, nullptr), 0);

    /* Run multiple health check cycles */
    for (int i = 0; i < 5; i++) {
        logic_health_metrics_t metrics = {};
        ASSERT_EQ(nimcp_health_agent_get_logic_metrics(agent, &metrics), 0);
        EXPECT_EQ(metrics.num_engines, 1u);
        EXPECT_GE(metrics.overall_logic_health, 0.0f);
        EXPECT_LE(metrics.overall_logic_health, 100.0f);

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

/* ============================================================================
 * Health Agent Start/Stop Integration Tests
 * ============================================================================ */

TEST_F(SymbolicLogicHealthIntegrationTest, LogicHealthWithRunningAgent) {
    symbolic_logic_t* eng = createEngine();
    if (!eng) {
        GTEST_SKIP() << "Symbolic logic engine not available";
    }

    ASSERT_EQ(nimcp_health_agent_connect_symbolic_logic(agent, eng, nullptr), 0);

    /* Start the health agent */
    ASSERT_EQ(nimcp_health_agent_start(agent), 0);

    /* Let it run for a bit */
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    /* Check metrics while agent is running */
    logic_health_metrics_t metrics = {};
    ASSERT_EQ(nimcp_health_agent_get_logic_metrics(agent, &metrics), 0);
    EXPECT_EQ(metrics.num_engines, 1u);

    /* Check health score */
    float score = nimcp_health_agent_get_logic_health_score(agent);
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 100.0f);

    /* Stop the agent */
    ASSERT_EQ(nimcp_health_agent_stop(agent), 0);
}

/* ============================================================================
 * Configuration Change Integration Tests
 * ============================================================================ */

TEST_F(SymbolicLogicHealthIntegrationTest, RuntimeConfigUpdate) {
    symbolic_logic_t* eng = createEngine();
    if (!eng) {
        GTEST_SKIP() << "Symbolic logic engine not available";
    }

    health_agent_symbolic_logic_config_t config1 = {};
    nimcp_health_agent_symbolic_logic_config_default(&config1);
    config1.inference_timeout_ms = 50.0f;

    ASSERT_EQ(nimcp_health_agent_connect_symbolic_logic(agent, eng, &config1), 0);

    /* Get initial metrics */
    logic_health_metrics_t metrics1 = {};
    ASSERT_EQ(nimcp_health_agent_get_logic_metrics(agent, &metrics1), 0);

    /* Update config */
    health_agent_symbolic_logic_config_t config2 = {};
    nimcp_health_agent_symbolic_logic_config_default(&config2);
    config2.inference_timeout_ms = 200.0f;
    config2.kb_utilization_warning = 0.9f;

    ASSERT_EQ(nimcp_health_agent_update_logic_config(agent, &config2), 0);

    /* Get metrics after config update */
    logic_health_metrics_t metrics2 = {};
    ASSERT_EQ(nimcp_health_agent_get_logic_metrics(agent, &metrics2), 0);
    EXPECT_EQ(metrics2.num_engines, 1u);
}

/* ============================================================================
 * Recovery Action Integration Tests
 * ============================================================================ */

TEST_F(SymbolicLogicHealthIntegrationTest, RecoveryActionsSequence) {
    symbolic_logic_t* eng = createEngine();
    if (!eng) {
        GTEST_SKIP() << "Symbolic logic engine not available";
    }

    ASSERT_EQ(nimcp_health_agent_connect_symbolic_logic(agent, eng, nullptr), 0);

    /* Test a sequence of recovery actions */
    std::vector<logic_recovery_action_t> actions = {
        LOGIC_RECOVERY_CLEAR_CACHE,
        LOGIC_RECOVERY_GC_KB,
        LOGIC_RECOVERY_COMPACT_KB,
        LOGIC_RECOVERY_CHECKPOINT_KB,
        LOGIC_RECOVERY_SOFT_RESET
    };

    for (auto action : actions) {
        int result = nimcp_health_agent_logic_recovery(agent, action, 0);
        EXPECT_EQ(result, 0);
    }

    /* Verify engine still functional after recoveries */
    logic_health_metrics_t metrics = {};
    ASSERT_EQ(nimcp_health_agent_get_logic_metrics(agent, &metrics), 0);
    EXPECT_EQ(metrics.num_engines, 1u);
}

TEST_F(SymbolicLogicHealthIntegrationTest, RecoveryAllEngines) {
    /* Create multiple engines */
    const int NUM_ENGINES = 3;
    int connected = 0;

    for (int i = 0; i < NUM_ENGINES; i++) {
        symbolic_logic_t* eng = createEngine();
        if (eng) {
            int result = nimcp_health_agent_connect_symbolic_logic(agent, eng, nullptr);
            if (result == 0) connected++;
        }
    }

    if (connected < 2) {
        GTEST_SKIP() << "Need at least 2 engines for this test";
    }

    /* Apply recovery to all engines (-1) */
    int result = nimcp_health_agent_logic_recovery(agent, LOGIC_RECOVERY_GC_KB, -1);
    EXPECT_EQ(result, 0);

    /* Verify all engines still tracked */
    logic_health_metrics_t metrics = {};
    ASSERT_EQ(nimcp_health_agent_get_logic_metrics(agent, &metrics), 0);
    EXPECT_EQ(metrics.num_engines, (uint32_t)connected);
}

/* ============================================================================
 * Dynamic Connect/Disconnect Integration Tests
 * ============================================================================ */

TEST_F(SymbolicLogicHealthIntegrationTest, DynamicConnectDisconnect) {
    symbolic_logic_t* eng1 = createEngine();
    symbolic_logic_t* eng2 = createEngine();

    if (!eng1) {
        GTEST_SKIP() << "Symbolic logic engine not available";
    }

    /* Connect first engine */
    ASSERT_EQ(nimcp_health_agent_connect_symbolic_logic(agent, eng1, nullptr), 0);

    logic_health_metrics_t metrics = {};
    ASSERT_EQ(nimcp_health_agent_get_logic_metrics(agent, &metrics), 0);
    EXPECT_EQ(metrics.num_engines, 1u);

    /* Connect second engine if available */
    if (eng2) {
        ASSERT_EQ(nimcp_health_agent_connect_symbolic_logic(agent, eng2, nullptr), 0);

        ASSERT_EQ(nimcp_health_agent_get_logic_metrics(agent, &metrics), 0);
        EXPECT_EQ(metrics.num_engines, 2u);

        /* Disconnect first engine */
        ASSERT_EQ(nimcp_health_agent_disconnect_symbolic_logic(agent, eng1), 0);

        ASSERT_EQ(nimcp_health_agent_get_logic_metrics(agent, &metrics), 0);
        EXPECT_EQ(metrics.num_engines, 1u);

        /* Disconnect second engine */
        ASSERT_EQ(nimcp_health_agent_disconnect_symbolic_logic(agent, eng2), 0);

        ASSERT_EQ(nimcp_health_agent_get_logic_metrics(agent, &metrics), 0);
        EXPECT_EQ(metrics.num_engines, 0u);
    }
}

/* ============================================================================
 * Concurrent Access Integration Tests
 * ============================================================================ */

TEST_F(SymbolicLogicHealthIntegrationTest, ConcurrentMetricsQueries) {
    symbolic_logic_t* eng = createEngine();
    if (!eng) {
        GTEST_SKIP() << "Symbolic logic engine not available";
    }

    ASSERT_EQ(nimcp_health_agent_connect_symbolic_logic(agent, eng, nullptr), 0);

    /* Query metrics from multiple threads concurrently */
    std::atomic<int> success_count{0};
    std::atomic<int> failure_count{0};

    auto query_func = [this, &success_count, &failure_count]() {
        for (int i = 0; i < 50; i++) {
            logic_health_metrics_t metrics = {};
            if (nimcp_health_agent_get_logic_metrics(agent, &metrics) == 0) {
                success_count++;
            } else {
                failure_count++;
            }
        }
    };

    std::thread t1(query_func);
    std::thread t2(query_func);
    std::thread t3(query_func);

    t1.join();
    t2.join();
    t3.join();

    EXPECT_GT(success_count.load(), 0);
    /* Some failures might occur, but most should succeed */
    EXPECT_GE(success_count.load(), failure_count.load());
}

TEST_F(SymbolicLogicHealthIntegrationTest, ConcurrentHealthScoreQueries) {
    symbolic_logic_t* eng = createEngine();
    if (!eng) {
        GTEST_SKIP() << "Symbolic logic engine not available";
    }

    ASSERT_EQ(nimcp_health_agent_connect_symbolic_logic(agent, eng, nullptr), 0);

    /* Query health score from multiple threads */
    std::atomic<bool> all_valid{true};

    auto score_func = [this, &all_valid]() {
        for (int i = 0; i < 100; i++) {
            float score = nimcp_health_agent_get_logic_health_score(agent);
            if (score < 0.0f || score > 100.0f) {
                all_valid = false;
            }
        }
    };

    std::thread t1(score_func);
    std::thread t2(score_func);

    t1.join();
    t2.join();

    EXPECT_TRUE(all_valid.load());
}

/* ============================================================================
 * Needs Attention Workflow Integration Tests
 * ============================================================================ */

TEST_F(SymbolicLogicHealthIntegrationTest, NeedsAttentionWorkflow) {
    symbolic_logic_t* eng = createEngine();
    if (!eng) {
        GTEST_SKIP() << "Symbolic logic engine not available";
    }

    ASSERT_EQ(nimcp_health_agent_connect_symbolic_logic(agent, eng, nullptr), 0);

    /* Initial check */
    bool needs = nimcp_health_agent_logic_needs_attention(agent);
    (void)needs;  /* Just verify no crash */

    /* Get detailed metrics if attention needed */
    logic_health_metrics_t metrics = {};
    ASSERT_EQ(nimcp_health_agent_get_logic_metrics(agent, &metrics), 0);

    /* If unhealthy, trigger recovery */
    if (metrics.any_engine_unhealthy) {
        EXPECT_EQ(nimcp_health_agent_logic_recovery(agent, LOGIC_RECOVERY_SOFT_RESET, 0), 0);
    }

    /* Verify score is valid */
    float score = nimcp_health_agent_get_logic_health_score(agent);
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 100.0f);
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
