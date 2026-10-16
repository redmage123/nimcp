/**
 * @file test_substrate_health_e2e.cpp
 * @brief End-to-end tests for Phase 5.10 Neural Substrate Health Integration
 * @date 2026-01-19
 *
 * Full workflow E2E tests for neural substrate health monitoring.
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
 * @brief Test fixture for neural substrate health E2E tests
 */
class SubstrateHealthE2ETest : public ::testing::Test {
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
            nimcp_health_agent_stop(agent);
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
 * Full Workflow E2E Tests
 * ============================================================================ */

TEST_F(SubstrateHealthE2ETest, FullSubstrateHealthWorkflow) {
    neural_substrate_t* sub = createSubstrate();
    if (!sub) {
        GTEST_SKIP() << "Neural substrate not available";
    }

    /* Step 1: Configure health monitoring */
    health_agent_substrate_config_t config = {};
    nimcp_health_agent_substrate_config_default(&config);
    config.enable_auto_recovery = true;
    config.atp_warning_threshold = 0.5f;

    /* Step 2: Connect substrate */
    ASSERT_EQ(nimcp_health_agent_connect_substrate(agent, sub, &config), 0);

    /* Step 3: Start health agent */
    ASSERT_EQ(nimcp_health_agent_start(agent), 0);

    /* Step 4: Let it run and monitor */
    for (int i = 0; i < 5; i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        /* Check health status */
        substrate_health_metrics_t metrics = {};
        ASSERT_EQ(nimcp_health_agent_get_substrate_metrics(agent, &metrics), 0);
        EXPECT_EQ(metrics.num_substrates, 1u);
        EXPECT_GE(metrics.overall_substrate_health, 0.0f);
        EXPECT_LE(metrics.overall_substrate_health, 100.0f);
    }

    /* Step 5: Check if needs attention */
    bool needs = nimcp_health_agent_substrate_needs_attention(agent);
    (void)needs;

    /* Step 6: Get health score */
    float score = nimcp_health_agent_get_substrate_health_score(agent);
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 100.0f);

    /* Step 7: Stop agent */
    ASSERT_EQ(nimcp_health_agent_stop(agent), 0);

    /* Step 8: Disconnect substrate */
    ASSERT_EQ(nimcp_health_agent_disconnect_substrate(agent, sub), 0);
}

TEST_F(SubstrateHealthE2ETest, LongRunningSubstrateMonitoring) {
    neural_substrate_t* sub = createSubstrate();
    if (!sub) {
        GTEST_SKIP() << "Neural substrate not available";
    }

    ASSERT_EQ(nimcp_health_agent_connect_substrate(agent, sub, nullptr), 0);
    ASSERT_EQ(nimcp_health_agent_start(agent), 0);

    /* Monitor for an extended period */
    float min_score = 100.0f;
    float max_score = 0.0f;

    for (int i = 0; i < 10; i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        float score = nimcp_health_agent_get_substrate_health_score(agent);
        if (score < min_score) min_score = score;
        if (score > max_score) max_score = score;

        EXPECT_GE(score, 0.0f);
        EXPECT_LE(score, 100.0f);
    }

    /* Health should remain stable */
    EXPECT_LE(max_score - min_score, 50.0f);  /* No more than 50 points variance */

    ASSERT_EQ(nimcp_health_agent_stop(agent), 0);
}

/* ============================================================================
 * Multi-Substrate E2E Tests
 * ============================================================================ */

TEST_F(SubstrateHealthE2ETest, MultipleSubstratesWorkflow) {
    /* Create multiple substrates */
    const int NUM_SUBSTRATES = 3;
    int connected = 0;

    for (int i = 0; i < NUM_SUBSTRATES; i++) {
        neural_substrate_t* sub = createSubstrate();
        if (sub) {
            if (nimcp_health_agent_connect_substrate(agent, sub, nullptr) == 0) {
                connected++;
            }
        }
    }

    if (connected == 0) {
        GTEST_SKIP() << "Could not create any neural substrates";
    }

    ASSERT_EQ(nimcp_health_agent_start(agent), 0);

    /* Monitor all substrates */
    for (int i = 0; i < 5; i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(30));

        substrate_health_metrics_t metrics = {};
        ASSERT_EQ(nimcp_health_agent_get_substrate_metrics(agent, &metrics), 0);
        EXPECT_EQ(metrics.num_substrates, (uint32_t)connected);
    }

    ASSERT_EQ(nimcp_health_agent_stop(agent), 0);
}

/* ============================================================================
 * Concurrent Access E2E Tests
 * ============================================================================ */

TEST_F(SubstrateHealthE2ETest, ConcurrentAccessStress) {
    neural_substrate_t* sub = createSubstrate();
    if (!sub) {
        GTEST_SKIP() << "Neural substrate not available";
    }

    ASSERT_EQ(nimcp_health_agent_connect_substrate(agent, sub, nullptr), 0);
    ASSERT_EQ(nimcp_health_agent_start(agent), 0);

    std::atomic<bool> stop_flag{false};
    std::atomic<int> total_checks{0};
    std::atomic<int> total_errors{0};

    /* Multiple threads querying health concurrently */
    auto worker = [this, &stop_flag, &total_checks, &total_errors]() {
        while (!stop_flag) {
            substrate_health_metrics_t metrics = {};
            if (nimcp_health_agent_get_substrate_metrics(agent, &metrics) == 0) {
                total_checks++;
                if (metrics.overall_substrate_health < 0 || metrics.overall_substrate_health > 100) {
                    total_errors++;
                }
            }
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    };

    std::thread t1(worker);
    std::thread t2(worker);
    std::thread t3(worker);

    /* Run for a short time */
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    stop_flag = true;

    t1.join();
    t2.join();
    t3.join();

    EXPECT_GT(total_checks.load(), 0);
    EXPECT_EQ(total_errors.load(), 0);

    ASSERT_EQ(nimcp_health_agent_stop(agent), 0);
}

/* ============================================================================
 * Recovery Workflow E2E Tests
 * ============================================================================ */

TEST_F(SubstrateHealthE2ETest, RecoveryWorkflow) {
    neural_substrate_t* sub = createSubstrate();
    if (!sub) {
        GTEST_SKIP() << "Neural substrate not available";
    }

    ASSERT_EQ(nimcp_health_agent_connect_substrate(agent, sub, nullptr), 0);
    ASSERT_EQ(nimcp_health_agent_start(agent), 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    /* Get initial health */
    float initial_score = nimcp_health_agent_get_substrate_health_score(agent);

    /* Execute recovery actions */
    EXPECT_EQ(nimcp_health_agent_substrate_recovery(agent, SUBSTRATE_RECOVERY_BOOST_ATP, 0), 0);
    EXPECT_EQ(nimcp_health_agent_substrate_recovery(agent, SUBSTRATE_RECOVERY_BALANCE_IONS, 0), 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    /* Check health after recovery */
    float after_score = nimcp_health_agent_get_substrate_health_score(agent);
    EXPECT_GE(after_score, 0.0f);
    EXPECT_LE(after_score, 100.0f);

    /* Health should not drastically decrease from recovery */
    EXPECT_GE(after_score, initial_score - 20.0f);

    ASSERT_EQ(nimcp_health_agent_stop(agent), 0);
}

/* ============================================================================
 * Lifecycle E2E Tests
 * ============================================================================ */

TEST_F(SubstrateHealthE2ETest, MultipleStartStopCycles) {
    neural_substrate_t* sub = createSubstrate();
    if (!sub) {
        GTEST_SKIP() << "Neural substrate not available";
    }

    ASSERT_EQ(nimcp_health_agent_connect_substrate(agent, sub, nullptr), 0);

    /* Multiple start/stop cycles */
    for (int cycle = 0; cycle < 3; cycle++) {
        ASSERT_EQ(nimcp_health_agent_start(agent), 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        substrate_health_metrics_t metrics = {};
        ASSERT_EQ(nimcp_health_agent_get_substrate_metrics(agent, &metrics), 0);
        EXPECT_EQ(metrics.num_substrates, 1u);

        ASSERT_EQ(nimcp_health_agent_stop(agent), 0);
    }
}

TEST_F(SubstrateHealthE2ETest, DynamicSubstrateManagement) {
    /* Start with no substrates */
    ASSERT_EQ(nimcp_health_agent_start(agent), 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    /* Dynamically add substrates */
    for (int i = 0; i < 3; i++) {
        neural_substrate_t* sub = createSubstrate();
        if (sub) {
            EXPECT_EQ(nimcp_health_agent_connect_substrate(agent, sub, nullptr), 0);
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }

    if (substrates.empty()) {
        ASSERT_EQ(nimcp_health_agent_stop(agent), 0);
        GTEST_SKIP() << "Could not create any neural substrates";
    }

    /* Verify count */
    substrate_health_metrics_t metrics = {};
    ASSERT_EQ(nimcp_health_agent_get_substrate_metrics(agent, &metrics), 0);
    EXPECT_EQ(metrics.num_substrates, (uint32_t)substrates.size());

    /* Dynamically remove substrates */
    while (!substrates.empty()) {
        neural_substrate_t* sub = substrates.back();
        EXPECT_EQ(nimcp_health_agent_disconnect_substrate(agent, sub), 0);
        substrate_destroy(sub);
        substrates.pop_back();

        ASSERT_EQ(nimcp_health_agent_get_substrate_metrics(agent, &metrics), 0);
        EXPECT_EQ(metrics.num_substrates, (uint32_t)substrates.size());
    }

    ASSERT_EQ(nimcp_health_agent_stop(agent), 0);
}

/* ============================================================================
 * Configuration E2E Tests
 * ============================================================================ */

TEST_F(SubstrateHealthE2ETest, RuntimeConfigChanges) {
    neural_substrate_t* sub = createSubstrate();
    if (!sub) {
        GTEST_SKIP() << "Neural substrate not available";
    }

    health_agent_substrate_config_t config = {};
    nimcp_health_agent_substrate_config_default(&config);

    ASSERT_EQ(nimcp_health_agent_connect_substrate(agent, sub, &config), 0);
    ASSERT_EQ(nimcp_health_agent_start(agent), 0);

    /* Test various config changes while running */
    for (int i = 0; i < 5; i++) {
        config.atp_warning_threshold = 0.4f + i * 0.05f;
        config.health_check_interval_ms = 50 + i * 20;

        EXPECT_EQ(nimcp_health_agent_update_substrate_config(agent, &config), 0);

        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        float score = nimcp_health_agent_get_substrate_health_score(agent);
        EXPECT_GE(score, 0.0f);
        EXPECT_LE(score, 100.0f);
    }

    ASSERT_EQ(nimcp_health_agent_stop(agent), 0);
}

/* ============================================================================
 * Stress E2E Tests
 * ============================================================================ */

TEST_F(SubstrateHealthE2ETest, HighFrequencyMetricsQueries) {
    neural_substrate_t* sub = createSubstrate();
    if (!sub) {
        GTEST_SKIP() << "Neural substrate not available";
    }

    ASSERT_EQ(nimcp_health_agent_connect_substrate(agent, sub, nullptr), 0);
    ASSERT_EQ(nimcp_health_agent_start(agent), 0);

    /* High-frequency metrics queries */
    int success_count = 0;
    int failure_count = 0;

    auto start_time = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start_time < std::chrono::milliseconds(500)) {
        substrate_health_metrics_t metrics = {};
        if (nimcp_health_agent_get_substrate_metrics(agent, &metrics) == 0) {
            success_count++;
        } else {
            failure_count++;
        }
    }

    EXPECT_GT(success_count, 0);
    /* Most queries should succeed */
    if (failure_count > 0) {
        EXPECT_GT(success_count, failure_count * 10);
    }

    ASSERT_EQ(nimcp_health_agent_stop(agent), 0);
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
