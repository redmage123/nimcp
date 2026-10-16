/**
 * @file test_substrate_health_integration.cpp
 * @brief Integration tests for Phase 5.10 Neural Substrate Health Integration
 * @date 2026-01-19
 *
 * Tests the integration of neural substrate health monitoring with the
 * health agent and immune system.
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>

extern "C" {
#include "utils/fault_tolerance/nimcp_health_agent.h"
#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "utils/logging/nimcp_logging.h"
}

/**
 * @brief Test fixture for neural substrate health integration tests
 */
class SubstrateHealthIntegrationTest : public ::testing::Test {
protected:
    nimcp_health_agent_t* agent = nullptr;
    std::vector<neural_substrate_t*> substrates;

    void SetUp() override {
        health_agent_config_t config = {};
        config.heartbeat_interval_ms = 100;
        config.message_queue_depth = 64;
        config.watchdog_timeout_ms = 5000;

        agent = nimcp_health_agent_create(&config);
        ASSERT_NE(agent, nullptr);
    }

    void TearDown() override {
        /* Disconnect and destroy all substrates */
        for (auto& sub : substrates) {
            if (sub) {
                nimcp_health_agent_disconnect_substrate(agent, sub);
                substrate_destroy(sub);
            }
        }
        substrates.clear();

        if (agent) {
            nimcp_health_agent_destroy(agent);
            agent = nullptr;
        }
    }

    neural_substrate_t* createSubstrate() {
        substrate_config_t cfg = {};
        substrate_default_config(&cfg);

        neural_substrate_t* sub = substrate_create(&cfg);
        if (sub) {
            substrates.push_back(sub);
        }
        return sub;
    }
};

/* ============================================================================
 * Multi-Substrate Integration Tests
 * ============================================================================ */

TEST_F(SubstrateHealthIntegrationTest, MultipleSubstratesMonitoring) {
    /* Create multiple substrates */
    const int NUM_SUBSTRATES = 4;
    int connected = 0;

    for (int i = 0; i < NUM_SUBSTRATES; i++) {
        neural_substrate_t* sub = createSubstrate();
        if (sub) {
            int result = nimcp_health_agent_connect_substrate(agent, sub, nullptr);
            if (result == 0) connected++;
        }
    }

    if (connected == 0) {
        GTEST_SKIP() << "Could not create any neural substrates";
    }

    /* Get metrics for all substrates */
    substrate_health_metrics_t metrics = {};
    ASSERT_EQ(nimcp_health_agent_get_substrate_metrics(agent, &metrics), 0);
    EXPECT_EQ(metrics.num_substrates, (uint32_t)connected);
    EXPECT_GE(metrics.overall_substrate_health, 0.0f);
    EXPECT_LE(metrics.overall_substrate_health, 100.0f);
}

TEST_F(SubstrateHealthIntegrationTest, HealthCheckCycle) {
    neural_substrate_t* sub = createSubstrate();
    if (!sub) {
        GTEST_SKIP() << "Neural substrate not available";
    }

    ASSERT_EQ(nimcp_health_agent_connect_substrate(agent, sub, nullptr), 0);

    /* Run multiple health check cycles */
    for (int i = 0; i < 5; i++) {
        substrate_health_metrics_t metrics = {};
        ASSERT_EQ(nimcp_health_agent_get_substrate_metrics(agent, &metrics), 0);
        EXPECT_EQ(metrics.num_substrates, 1u);
        EXPECT_GE(metrics.overall_substrate_health, 0.0f);
        EXPECT_LE(metrics.overall_substrate_health, 100.0f);

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

/* ============================================================================
 * Health Agent Start/Stop Integration Tests
 * ============================================================================ */

TEST_F(SubstrateHealthIntegrationTest, SubstrateHealthWithRunningAgent) {
    neural_substrate_t* sub = createSubstrate();
    if (!sub) {
        GTEST_SKIP() << "Neural substrate not available";
    }

    ASSERT_EQ(nimcp_health_agent_connect_substrate(agent, sub, nullptr), 0);

    /* Start the health agent */
    ASSERT_EQ(nimcp_health_agent_start(agent), 0);

    /* Let it run for a bit */
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    /* Check metrics while agent is running */
    substrate_health_metrics_t metrics = {};
    ASSERT_EQ(nimcp_health_agent_get_substrate_metrics(agent, &metrics), 0);
    EXPECT_EQ(metrics.num_substrates, 1u);

    /* Check health score */
    float score = nimcp_health_agent_get_substrate_health_score(agent);
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 100.0f);

    /* Stop the agent */
    ASSERT_EQ(nimcp_health_agent_stop(agent), 0);
}

/* ============================================================================
 * Configuration Change Integration Tests
 * ============================================================================ */

TEST_F(SubstrateHealthIntegrationTest, RuntimeConfigUpdate) {
    neural_substrate_t* sub = createSubstrate();
    if (!sub) {
        GTEST_SKIP() << "Neural substrate not available";
    }

    health_agent_substrate_config_t config1 = {};
    nimcp_health_agent_substrate_config_default(&config1);
    config1.atp_warning_threshold = 0.5f;

    ASSERT_EQ(nimcp_health_agent_connect_substrate(agent, sub, &config1), 0);

    /* Get initial metrics */
    substrate_health_metrics_t metrics1 = {};
    ASSERT_EQ(nimcp_health_agent_get_substrate_metrics(agent, &metrics1), 0);

    /* Update config */
    health_agent_substrate_config_t config2 = {};
    nimcp_health_agent_substrate_config_default(&config2);
    config2.atp_warning_threshold = 0.6f;
    config2.health_check_interval_ms = 200;

    ASSERT_EQ(nimcp_health_agent_update_substrate_config(agent, &config2), 0);

    /* Get metrics after config update */
    substrate_health_metrics_t metrics2 = {};
    ASSERT_EQ(nimcp_health_agent_get_substrate_metrics(agent, &metrics2), 0);
    EXPECT_EQ(metrics2.num_substrates, 1u);
}

/* ============================================================================
 * Recovery Action Integration Tests
 * ============================================================================ */

TEST_F(SubstrateHealthIntegrationTest, RecoveryActionsSequence) {
    neural_substrate_t* sub = createSubstrate();
    if (!sub) {
        GTEST_SKIP() << "Neural substrate not available";
    }

    ASSERT_EQ(nimcp_health_agent_connect_substrate(agent, sub, nullptr), 0);

    /* Test a sequence of recovery actions */
    std::vector<substrate_recovery_action_t> actions = {
        SUBSTRATE_RECOVERY_BOOST_ATP,
        SUBSTRATE_RECOVERY_BOOST_OXYGEN,
        SUBSTRATE_RECOVERY_BOOST_GLUCOSE,
        SUBSTRATE_RECOVERY_BALANCE_IONS,
        SUBSTRATE_RECOVERY_REPAIR_MEMBRANE,
        SUBSTRATE_RECOVERY_SOFT_RESET
    };

    for (auto action : actions) {
        int result = nimcp_health_agent_substrate_recovery(agent, action, 0);
        EXPECT_EQ(result, 0);
    }

    /* Verify substrate still functional after recoveries */
    substrate_health_metrics_t metrics = {};
    ASSERT_EQ(nimcp_health_agent_get_substrate_metrics(agent, &metrics), 0);
    EXPECT_EQ(metrics.num_substrates, 1u);
}

TEST_F(SubstrateHealthIntegrationTest, RecoveryAllSubstrates) {
    /* Create multiple substrates */
    const int NUM_SUBSTRATES = 3;
    int connected = 0;

    for (int i = 0; i < NUM_SUBSTRATES; i++) {
        neural_substrate_t* sub = createSubstrate();
        if (sub) {
            int result = nimcp_health_agent_connect_substrate(agent, sub, nullptr);
            if (result == 0) connected++;
        }
    }

    if (connected < 2) {
        GTEST_SKIP() << "Need at least 2 substrates for this test";
    }

    /* Apply recovery to all substrates (-1) */
    int result = nimcp_health_agent_substrate_recovery(agent, SUBSTRATE_RECOVERY_SOFT_RESET, -1);
    EXPECT_EQ(result, 0);

    /* Verify all substrates still tracked */
    substrate_health_metrics_t metrics = {};
    ASSERT_EQ(nimcp_health_agent_get_substrate_metrics(agent, &metrics), 0);
    EXPECT_EQ(metrics.num_substrates, (uint32_t)connected);
}

/* ============================================================================
 * Dynamic Connect/Disconnect Integration Tests
 * ============================================================================ */

TEST_F(SubstrateHealthIntegrationTest, DynamicConnectDisconnect) {
    neural_substrate_t* sub1 = createSubstrate();
    neural_substrate_t* sub2 = createSubstrate();

    if (!sub1) {
        GTEST_SKIP() << "Neural substrate not available";
    }

    /* Connect first substrate */
    ASSERT_EQ(nimcp_health_agent_connect_substrate(agent, sub1, nullptr), 0);

    substrate_health_metrics_t metrics = {};
    ASSERT_EQ(nimcp_health_agent_get_substrate_metrics(agent, &metrics), 0);
    EXPECT_EQ(metrics.num_substrates, 1u);

    /* Connect second substrate if available */
    if (sub2) {
        ASSERT_EQ(nimcp_health_agent_connect_substrate(agent, sub2, nullptr), 0);

        ASSERT_EQ(nimcp_health_agent_get_substrate_metrics(agent, &metrics), 0);
        EXPECT_EQ(metrics.num_substrates, 2u);

        /* Disconnect first substrate */
        ASSERT_EQ(nimcp_health_agent_disconnect_substrate(agent, sub1), 0);

        ASSERT_EQ(nimcp_health_agent_get_substrate_metrics(agent, &metrics), 0);
        EXPECT_EQ(metrics.num_substrates, 1u);

        /* Disconnect second substrate */
        ASSERT_EQ(nimcp_health_agent_disconnect_substrate(agent, sub2), 0);

        ASSERT_EQ(nimcp_health_agent_get_substrate_metrics(agent, &metrics), 0);
        EXPECT_EQ(metrics.num_substrates, 0u);
    }
}

/* ============================================================================
 * Concurrent Access Integration Tests
 * ============================================================================ */

TEST_F(SubstrateHealthIntegrationTest, ConcurrentMetricsQueries) {
    neural_substrate_t* sub = createSubstrate();
    if (!sub) {
        GTEST_SKIP() << "Neural substrate not available";
    }

    ASSERT_EQ(nimcp_health_agent_connect_substrate(agent, sub, nullptr), 0);

    /* Query metrics from multiple threads concurrently */
    std::atomic<int> success_count{0};
    std::atomic<int> failure_count{0};

    auto query_func = [this, &success_count, &failure_count]() {
        for (int i = 0; i < 50; i++) {
            substrate_health_metrics_t metrics = {};
            if (nimcp_health_agent_get_substrate_metrics(agent, &metrics) == 0) {
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
    /* Most queries should succeed */
    EXPECT_GE(success_count.load(), failure_count.load());
}

TEST_F(SubstrateHealthIntegrationTest, ConcurrentHealthScoreQueries) {
    neural_substrate_t* sub = createSubstrate();
    if (!sub) {
        GTEST_SKIP() << "Neural substrate not available";
    }

    ASSERT_EQ(nimcp_health_agent_connect_substrate(agent, sub, nullptr), 0);

    /* Query health score from multiple threads */
    std::atomic<bool> all_valid{true};

    auto score_func = [this, &all_valid]() {
        for (int i = 0; i < 100; i++) {
            float score = nimcp_health_agent_get_substrate_health_score(agent);
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

TEST_F(SubstrateHealthIntegrationTest, NeedsAttentionWorkflow) {
    neural_substrate_t* sub = createSubstrate();
    if (!sub) {
        GTEST_SKIP() << "Neural substrate not available";
    }

    ASSERT_EQ(nimcp_health_agent_connect_substrate(agent, sub, nullptr), 0);

    /* Initial check */
    bool needs = nimcp_health_agent_substrate_needs_attention(agent);
    (void)needs;  /* Just verify no crash */

    /* Get detailed metrics if attention needed */
    substrate_health_metrics_t metrics = {};
    ASSERT_EQ(nimcp_health_agent_get_substrate_metrics(agent, &metrics), 0);

    /* If unhealthy, trigger recovery */
    if (metrics.any_substrate_unhealthy) {
        EXPECT_EQ(nimcp_health_agent_substrate_recovery(agent, SUBSTRATE_RECOVERY_SOFT_RESET, 0), 0);
    }

    /* Verify score is valid */
    float score = nimcp_health_agent_get_substrate_health_score(agent);
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 100.0f);
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
