/**
 * @file test_symbolic_logic_health_e2e.cpp
 * @brief End-to-end tests for Phase 5.9 Symbolic Logic Health Integration
 * @date 2026-01-18
 *
 * Full workflow E2E tests for symbolic logic health monitoring.
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
 * @brief Test fixture for symbolic logic health E2E tests
 */
class SymbolicLogicHealthE2ETest : public ::testing::Test {
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
            nimcp_health_agent_stop(agent);
            nimcp_health_agent_destroy(agent);
            agent = nullptr;
        }
    }

    symbolic_logic_t* createEngine(uint32_t predicates = 100) {
        logic_config_t cfg = {};
        cfg.max_predicates = predicates;
        cfg.max_rules = predicates / 2;
        cfg.max_kb_size = predicates * 2;
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
 * Full Workflow E2E Tests
 * ============================================================================ */

TEST_F(SymbolicLogicHealthE2ETest, FullSymbolicLogicHealthWorkflow) {
    symbolic_logic_t* eng = createEngine();
    if (!eng) {
        GTEST_SKIP() << "Symbolic logic engine not available";
    }

    /* Step 1: Configure health monitoring */
    health_agent_symbolic_logic_config_t config = {};
    nimcp_health_agent_symbolic_logic_config_default(&config);
    config.enable_auto_recovery = true;
    config.inference_timeout_ms = 100.0f;

    /* Step 2: Connect engine */
    ASSERT_EQ(nimcp_health_agent_connect_symbolic_logic(agent, eng, &config), 0);

    /* Step 3: Start health agent */
    ASSERT_EQ(nimcp_health_agent_start(agent), 0);

    /* Step 4: Let it run and monitor */
    for (int i = 0; i < 5; i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        /* Check health status */
        logic_health_metrics_t metrics = {};
        ASSERT_EQ(nimcp_health_agent_get_logic_metrics(agent, &metrics), 0);
        EXPECT_EQ(metrics.num_engines, 1u);
        EXPECT_GE(metrics.overall_logic_health, 0.0f);
        EXPECT_LE(metrics.overall_logic_health, 100.0f);
    }

    /* Step 5: Check if needs attention */
    bool needs = nimcp_health_agent_logic_needs_attention(agent);
    (void)needs;

    /* Step 6: Get health score */
    float score = nimcp_health_agent_get_logic_health_score(agent);
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 100.0f);

    /* Step 7: Stop agent */
    ASSERT_EQ(nimcp_health_agent_stop(agent), 0);

    /* Step 8: Disconnect engine */
    ASSERT_EQ(nimcp_health_agent_disconnect_symbolic_logic(agent, eng), 0);
}

TEST_F(SymbolicLogicHealthE2ETest, LongRunningLogicMonitoring) {
    symbolic_logic_t* eng = createEngine();
    if (!eng) {
        GTEST_SKIP() << "Symbolic logic engine not available";
    }

    ASSERT_EQ(nimcp_health_agent_connect_symbolic_logic(agent, eng, nullptr), 0);
    ASSERT_EQ(nimcp_health_agent_start(agent), 0);

    /* Monitor for an extended period */
    float min_score = 100.0f;
    float max_score = 0.0f;

    for (int i = 0; i < 10; i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        float score = nimcp_health_agent_get_logic_health_score(agent);
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
 * Multi-Engine E2E Tests
 * ============================================================================ */

TEST_F(SymbolicLogicHealthE2ETest, MultipleEnginesWorkflow) {
    /* Create multiple engines with different sizes */
    const int NUM_ENGINES = 3;
    int connected = 0;

    for (int i = 0; i < NUM_ENGINES; i++) {
        symbolic_logic_t* eng = createEngine(50 + i * 50);
        if (eng) {
            if (nimcp_health_agent_connect_symbolic_logic(agent, eng, nullptr) == 0) {
                connected++;
            }
        }
    }

    if (connected == 0) {
        GTEST_SKIP() << "Could not create any symbolic logic engines";
    }

    ASSERT_EQ(nimcp_health_agent_start(agent), 0);

    /* Monitor all engines */
    for (int i = 0; i < 5; i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(30));

        logic_health_metrics_t metrics = {};
        ASSERT_EQ(nimcp_health_agent_get_logic_metrics(agent, &metrics), 0);
        EXPECT_EQ(metrics.num_engines, (uint32_t)connected);
    }

    ASSERT_EQ(nimcp_health_agent_stop(agent), 0);
}

/* ============================================================================
 * Concurrent Access E2E Tests
 * ============================================================================ */

TEST_F(SymbolicLogicHealthE2ETest, ConcurrentAccessStress) {
    symbolic_logic_t* eng = createEngine();
    if (!eng) {
        GTEST_SKIP() << "Symbolic logic engine not available";
    }

    ASSERT_EQ(nimcp_health_agent_connect_symbolic_logic(agent, eng, nullptr), 0);
    ASSERT_EQ(nimcp_health_agent_start(agent), 0);

    std::atomic<bool> stop_flag{false};
    std::atomic<int> total_checks{0};
    std::atomic<int> total_errors{0};

    /* Multiple threads querying health concurrently */
    auto worker = [this, &stop_flag, &total_checks, &total_errors]() {
        while (!stop_flag) {
            logic_health_metrics_t metrics = {};
            if (nimcp_health_agent_get_logic_metrics(agent, &metrics) == 0) {
                total_checks++;
                if (metrics.overall_logic_health < 0 || metrics.overall_logic_health > 100) {
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

TEST_F(SymbolicLogicHealthE2ETest, RecoveryWorkflow) {
    symbolic_logic_t* eng = createEngine();
    if (!eng) {
        GTEST_SKIP() << "Symbolic logic engine not available";
    }

    ASSERT_EQ(nimcp_health_agent_connect_symbolic_logic(agent, eng, nullptr), 0);
    ASSERT_EQ(nimcp_health_agent_start(agent), 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    /* Get initial health */
    float initial_score = nimcp_health_agent_get_logic_health_score(agent);

    /* Execute recovery actions */
    EXPECT_EQ(nimcp_health_agent_logic_recovery(agent, LOGIC_RECOVERY_CLEAR_CACHE, 0), 0);
    EXPECT_EQ(nimcp_health_agent_logic_recovery(agent, LOGIC_RECOVERY_GC_KB, 0), 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    /* Check health after recovery */
    float after_score = nimcp_health_agent_get_logic_health_score(agent);
    EXPECT_GE(after_score, 0.0f);
    EXPECT_LE(after_score, 100.0f);

    /* Health should not drastically decrease from recovery */
    EXPECT_GE(after_score, initial_score - 20.0f);

    ASSERT_EQ(nimcp_health_agent_stop(agent), 0);
}

/* ============================================================================
 * Lifecycle E2E Tests
 * ============================================================================ */

TEST_F(SymbolicLogicHealthE2ETest, MultipleStartStopCycles) {
    symbolic_logic_t* eng = createEngine();
    if (!eng) {
        GTEST_SKIP() << "Symbolic logic engine not available";
    }

    ASSERT_EQ(nimcp_health_agent_connect_symbolic_logic(agent, eng, nullptr), 0);

    /* Multiple start/stop cycles */
    for (int cycle = 0; cycle < 3; cycle++) {
        ASSERT_EQ(nimcp_health_agent_start(agent), 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        logic_health_metrics_t metrics = {};
        ASSERT_EQ(nimcp_health_agent_get_logic_metrics(agent, &metrics), 0);
        EXPECT_EQ(metrics.num_engines, 1u);

        ASSERT_EQ(nimcp_health_agent_stop(agent), 0);
    }
}

TEST_F(SymbolicLogicHealthE2ETest, DynamicEngineManagement) {
    /* Start with no engines */
    ASSERT_EQ(nimcp_health_agent_start(agent), 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    /* Dynamically add engines */
    for (int i = 0; i < 3; i++) {
        symbolic_logic_t* eng = createEngine(100);
        if (eng) {
            EXPECT_EQ(nimcp_health_agent_connect_symbolic_logic(agent, eng, nullptr), 0);
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }

    if (engines.empty()) {
        ASSERT_EQ(nimcp_health_agent_stop(agent), 0);
        GTEST_SKIP() << "Could not create any symbolic logic engines";
    }

    /* Verify count */
    logic_health_metrics_t metrics = {};
    ASSERT_EQ(nimcp_health_agent_get_logic_metrics(agent, &metrics), 0);
    EXPECT_EQ(metrics.num_engines, (uint32_t)engines.size());

    /* Dynamically remove engines */
    while (!engines.empty()) {
        symbolic_logic_t* eng = engines.back();
        EXPECT_EQ(nimcp_health_agent_disconnect_symbolic_logic(agent, eng), 0);
        symbolic_logic_destroy(eng);
        engines.pop_back();

        ASSERT_EQ(nimcp_health_agent_get_logic_metrics(agent, &metrics), 0);
        EXPECT_EQ(metrics.num_engines, (uint32_t)engines.size());
    }

    ASSERT_EQ(nimcp_health_agent_stop(agent), 0);
}

/* ============================================================================
 * Configuration E2E Tests
 * ============================================================================ */

TEST_F(SymbolicLogicHealthE2ETest, RuntimeConfigChanges) {
    symbolic_logic_t* eng = createEngine();
    if (!eng) {
        GTEST_SKIP() << "Symbolic logic engine not available";
    }

    health_agent_symbolic_logic_config_t config = {};
    nimcp_health_agent_symbolic_logic_config_default(&config);

    ASSERT_EQ(nimcp_health_agent_connect_symbolic_logic(agent, eng, &config), 0);
    ASSERT_EQ(nimcp_health_agent_start(agent), 0);

    /* Test various config changes while running */
    for (int i = 0; i < 5; i++) {
        config.inference_timeout_ms = 50.0f + i * 20.0f;
        config.kb_utilization_warning = 0.7f + i * 0.05f;

        EXPECT_EQ(nimcp_health_agent_update_logic_config(agent, &config), 0);

        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        float score = nimcp_health_agent_get_logic_health_score(agent);
        EXPECT_GE(score, 0.0f);
        EXPECT_LE(score, 100.0f);
    }

    ASSERT_EQ(nimcp_health_agent_stop(agent), 0);
}

/* ============================================================================
 * Stress E2E Tests
 * ============================================================================ */

TEST_F(SymbolicLogicHealthE2ETest, HighFrequencyMetricsQueries) {
    symbolic_logic_t* eng = createEngine();
    if (!eng) {
        GTEST_SKIP() << "Symbolic logic engine not available";
    }

    ASSERT_EQ(nimcp_health_agent_connect_symbolic_logic(agent, eng, nullptr), 0);
    ASSERT_EQ(nimcp_health_agent_start(agent), 0);

    /* High-frequency metrics queries */
    int success_count = 0;
    int failure_count = 0;

    auto start_time = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start_time < std::chrono::milliseconds(500)) {
        logic_health_metrics_t metrics = {};
        if (nimcp_health_agent_get_logic_metrics(agent, &metrics) == 0) {
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
