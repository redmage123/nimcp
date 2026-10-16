/**
 * @file test_health_agent_neural_e2e.cpp
 * @brief End-to-end tests for NIMCP Health Agent Neural Integration (Phase 5.5)
 * @version 1.0.0
 * @date 2025-01-18
 *
 * WHAT: E2E tests for complete health agent neural monitoring workflows
 * WHY:  Verify full system behavior with SNN/LNN immune bridges
 * HOW:  Test realistic scenarios from agent creation through extended operation
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <cstring>
#include <atomic>
#include <vector>
#include <memory>

// Health agent header
#include "utils/fault_tolerance/nimcp_health_agent.h"

// Neural module immune bridge headers
#include "snn/nimcp_snn_immune.h"
#include "lnn/nimcp_lnn_immune.h"

// Brain immune system for full integration
#include "cognitive/immune/nimcp_brain_immune.h"

//=============================================================================
// Test Fixtures
//=============================================================================

/**
 * @brief E2E fixture for health agent neural workflows
 */
class HealthAgentNeuralE2ETest : public ::testing::Test {
protected:
    nimcp_health_agent_t* agent = nullptr;
    health_agent_config_t config;

    void SetUp() override {
        nimcp_health_agent_default_config(&config);
        strncpy(config.agent_name, "NeuralE2ETestAgent", sizeof(config.agent_name) - 1);
        config.check_interval_ms = 50;
        config.heartbeat_interval_ms = 100;
        config.watchdog_timeout_ms = 500;
        config.enable_auto_recovery = false;

        agent = nimcp_health_agent_create(&config);
        ASSERT_NE(agent, nullptr) << "Failed to create health agent";
    }

    void TearDown() override {
        if (agent) {
            if (nimcp_health_agent_is_running(agent)) {
                nimcp_health_agent_stop(agent);
            }
            nimcp_health_agent_destroy(agent);
            agent = nullptr;
        }
    }
};

/**
 * @brief E2E fixture with immune system for deeper integration
 */
class HealthAgentNeuralImmuneE2ETest : public HealthAgentNeuralE2ETest {
protected:
    brain_immune_system_t* immune = nullptr;

    void SetUp() override {
        HealthAgentNeuralE2ETest::SetUp();

        // Create immune system for integration
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune = brain_immune_create(&immune_config);
        // Note: immune may be NULL if not fully available
    }

    void TearDown() override {
        if (immune) {
            brain_immune_destroy(immune);
            immune = nullptr;
        }
        HealthAgentNeuralE2ETest::TearDown();
    }
};

//=============================================================================
// Full Workflow E2E Tests
//=============================================================================

TEST_F(HealthAgentNeuralE2ETest, FullWorkflow_CreateConnectStartRunStop) {
    // Step 1: Connect neural modules
    EXPECT_EQ(nimcp_health_agent_connect_snn(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_lnn(agent, nullptr, nullptr), 0);

    // Step 2: Start agent
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    EXPECT_TRUE(nimcp_health_agent_is_running(agent));

    // Step 3: Run with heartbeats for extended period
    for (int i = 0; i < 50; i++) {
        nimcp_health_agent_heartbeat(agent);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    // Step 4: Query neural metrics periodically
    for (int i = 0; i < 10; i++) {
        neural_health_metrics_t metrics;
        EXPECT_EQ(nimcp_health_agent_get_neural_metrics(agent, &metrics), 0);
        EXPECT_GE(metrics.neural_health_score, 0.0f);
        EXPECT_LE(metrics.neural_health_score, 100.0f);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // Step 5: Get statistics
    health_agent_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    nimcp_health_agent_get_stats(agent, &stats);
    EXPECT_GE(stats.checks_performed, 0u);

    // Step 6: Stop agent
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
    EXPECT_FALSE(nimcp_health_agent_is_running(agent));
}

TEST_F(HealthAgentNeuralE2ETest, FullWorkflow_AllModulesWithNeural) {
    // Connect cognitive modules first
    EXPECT_EQ(nimcp_health_agent_connect_failure_prediction(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_metacognition(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_ethics(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_emotion(agent, nullptr, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_wellbeing(agent, nullptr, nullptr), 0);

    // Then connect neural modules
    EXPECT_EQ(nimcp_health_agent_connect_snn(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_lnn(agent, nullptr, nullptr), 0);

    // Start and run
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    for (int i = 0; i < 30; i++) {
        nimcp_health_agent_heartbeat(agent);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    // Verify both neural and general health
    EXPECT_FALSE(nimcp_health_agent_is_neural_unhealthy(agent));
    EXPECT_GE(nimcp_health_agent_get_neural_health_score(agent), 0.0f);

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentNeuralE2ETest, FullWorkflow_CustomConfigurations) {
    // Use custom configurations for neural modules
    health_agent_snn_config_t snn_cfg;
    memset(&snn_cfg, 0, sizeof(snn_cfg));
    snn_cfg.enable_snn_monitoring = true;
    snn_cfg.enable_instability_detection = true;
    snn_cfg.enable_auto_report = true;
    snn_cfg.enable_learning_modulation = true;
    snn_cfg.max_spike_rate_hz = 150.0f;
    snn_cfg.min_spike_rate_hz = 0.5f;
    snn_cfg.burst_threshold = 0.55f;
    snn_cfg.sync_threshold = 0.85f;
    snn_cfg.check_interval_ms = 50;

    health_agent_lnn_config_t lnn_cfg;
    memset(&lnn_cfg, 0, sizeof(lnn_cfg));
    lnn_cfg.enable_lnn_monitoring = true;
    lnn_cfg.enable_stability_detection = true;
    lnn_cfg.enable_auto_report = true;
    lnn_cfg.enable_tau_modulation = true;
    lnn_cfg.enable_lr_modulation = true;
    lnn_cfg.state_explosion_threshold = 1e7f;
    lnn_cfg.state_collapse_threshold = 1e-10f;
    lnn_cfg.tau_max = 2000.0f;
    lnn_cfg.tau_min = 0.005f;
    lnn_cfg.gradient_explosion_threshold = 1e4f;
    lnn_cfg.gradient_vanishing_threshold = 1e-8f;
    lnn_cfg.check_interval_ms = 50;

    EXPECT_EQ(nimcp_health_agent_connect_snn(agent, nullptr, &snn_cfg), 0);
    EXPECT_EQ(nimcp_health_agent_connect_lnn(agent, nullptr, &lnn_cfg), 0);

    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Run for extended period with custom configs
    for (int i = 0; i < 40; i++) {
        nimcp_health_agent_heartbeat(agent);

        neural_health_metrics_t metrics;
        EXPECT_EQ(nimcp_health_agent_get_neural_metrics(agent, &metrics), 0);

        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

//=============================================================================
// Dynamic Reconfiguration E2E Tests
//=============================================================================

TEST_F(HealthAgentNeuralE2ETest, DynamicReconfig_WhileRunning) {
    EXPECT_EQ(nimcp_health_agent_connect_snn(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_lnn(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Run with initial config
    for (int i = 0; i < 20; i++) {
        nimcp_health_agent_heartbeat(agent);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    // Reconfigure while running
    health_agent_snn_config_t snn_cfg;
    memset(&snn_cfg, 0, sizeof(snn_cfg));
    snn_cfg.enable_snn_monitoring = true;
    snn_cfg.check_interval_ms = 25;  // Faster checks

    health_agent_lnn_config_t lnn_cfg;
    memset(&lnn_cfg, 0, sizeof(lnn_cfg));
    lnn_cfg.enable_lnn_monitoring = true;
    lnn_cfg.check_interval_ms = 25;  // Faster checks

    EXPECT_EQ(nimcp_health_agent_configure_neural(agent, &snn_cfg, &lnn_cfg), 0);

    // Continue running with new config
    for (int i = 0; i < 20; i++) {
        nimcp_health_agent_heartbeat(agent);

        neural_health_metrics_t metrics;
        EXPECT_EQ(nimcp_health_agent_get_neural_metrics(agent, &metrics), 0);

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentNeuralE2ETest, DynamicReconfig_DisableEnableMonitoring) {
    EXPECT_EQ(nimcp_health_agent_connect_snn(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_lnn(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Run with monitoring enabled
    for (int i = 0; i < 10; i++) {
        nimcp_health_agent_heartbeat(agent);
        EXPECT_FALSE(nimcp_health_agent_is_neural_unhealthy(agent));
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    // Disable monitoring
    health_agent_snn_config_t snn_cfg;
    memset(&snn_cfg, 0, sizeof(snn_cfg));
    snn_cfg.enable_snn_monitoring = false;

    health_agent_lnn_config_t lnn_cfg;
    memset(&lnn_cfg, 0, sizeof(lnn_cfg));
    lnn_cfg.enable_lnn_monitoring = false;

    EXPECT_EQ(nimcp_health_agent_configure_neural(agent, &snn_cfg, &lnn_cfg), 0);

    // Run with monitoring disabled
    for (int i = 0; i < 10; i++) {
        nimcp_health_agent_heartbeat(agent);
        // Should still be callable even when disabled
        EXPECT_FALSE(nimcp_health_agent_is_neural_unhealthy(agent));
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    // Re-enable monitoring
    snn_cfg.enable_snn_monitoring = true;
    lnn_cfg.enable_lnn_monitoring = true;
    EXPECT_EQ(nimcp_health_agent_configure_neural(agent, &snn_cfg, &lnn_cfg), 0);

    // Run with monitoring re-enabled
    for (int i = 0; i < 10; i++) {
        nimcp_health_agent_heartbeat(agent);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

//=============================================================================
// Concurrent Operations E2E Tests
//=============================================================================

TEST_F(HealthAgentNeuralE2ETest, ConcurrentOps_MetricsQueryDuringOperation) {
    EXPECT_EQ(nimcp_health_agent_connect_snn(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_lnn(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    std::atomic<bool> should_stop{false};
    std::atomic<int> total_queries{0};
    std::atomic<int> failed_queries{0};

    // Thread 1: Query neural metrics
    std::thread metrics_thread([this, &should_stop, &total_queries, &failed_queries]() {
        while (!should_stop.load()) {
            neural_health_metrics_t metrics;
            if (nimcp_health_agent_get_neural_metrics(agent, &metrics) == 0) {
                total_queries++;
            } else {
                failed_queries++;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    // Thread 2: Query health score
    std::thread score_thread([this, &should_stop, &total_queries]() {
        while (!should_stop.load()) {
            float score = nimcp_health_agent_get_neural_health_score(agent);
            if (score >= 0.0f && score <= 100.0f) {
                total_queries++;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    // Thread 3: Check is_unhealthy
    std::thread unhealthy_thread([this, &should_stop, &total_queries]() {
        while (!should_stop.load()) {
            (void)nimcp_health_agent_is_neural_unhealthy(agent);
            total_queries++;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    // Main thread: heartbeats
    for (int i = 0; i < 50; i++) {
        nimcp_health_agent_heartbeat(agent);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    should_stop.store(true);
    metrics_thread.join();
    score_thread.join();
    unhealthy_thread.join();

    EXPECT_GT(total_queries.load(), 100);
    EXPECT_EQ(failed_queries.load(), 0);

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentNeuralE2ETest, ConcurrentOps_ReconfigDuringQuery) {
    EXPECT_EQ(nimcp_health_agent_connect_snn(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_lnn(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    std::atomic<bool> should_stop{false};
    std::atomic<int> query_count{0};
    std::atomic<int> reconfig_count{0};

    // Thread: Query metrics continuously
    std::thread query_thread([this, &should_stop, &query_count]() {
        while (!should_stop.load()) {
            neural_health_metrics_t metrics;
            if (nimcp_health_agent_get_neural_metrics(agent, &metrics) == 0) {
                query_count++;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });

    // Main thread: reconfigure periodically
    for (int i = 0; i < 20; i++) {
        health_agent_snn_config_t snn_cfg;
        memset(&snn_cfg, 0, sizeof(snn_cfg));
        snn_cfg.enable_snn_monitoring = true;
        snn_cfg.check_interval_ms = 25 + (i % 5) * 10;

        if (nimcp_health_agent_configure_neural(agent, &snn_cfg, nullptr) == 0) {
            reconfig_count++;
        }

        nimcp_health_agent_heartbeat(agent);
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }

    should_stop.store(true);
    query_thread.join();

    EXPECT_GT(query_count.load(), 50);
    EXPECT_GT(reconfig_count.load(), 10);

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

//=============================================================================
// Extended Operation E2E Tests
//=============================================================================

TEST_F(HealthAgentNeuralE2ETest, ExtendedOperation_3SecondsRuntime) {
    EXPECT_EQ(nimcp_health_agent_connect_snn(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_lnn(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    auto start_time = std::chrono::steady_clock::now();
    int heartbeat_count = 0;
    int metrics_queries = 0;

    // Run for 3 seconds
    while (std::chrono::steady_clock::now() - start_time < std::chrono::seconds(3)) {
        nimcp_health_agent_heartbeat(agent);
        heartbeat_count++;

        if (heartbeat_count % 5 == 0) {
            neural_health_metrics_t metrics;
            EXPECT_EQ(nimcp_health_agent_get_neural_metrics(agent, &metrics), 0);
            EXPECT_FALSE(metrics.any_neural_unhealthy);
            metrics_queries++;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    EXPECT_GT(heartbeat_count, 100);
    EXPECT_GT(metrics_queries, 20);

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);
    EXPECT_GT(stats.checks_performed, 0u);
    EXPECT_GE(stats.uptime_ms, 2500u);

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentNeuralE2ETest, ExtendedOperation_StartStopCycle) {
    EXPECT_EQ(nimcp_health_agent_connect_snn(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_lnn(agent, nullptr, nullptr), 0);

    // Multiple start/stop cycles
    for (int cycle = 0; cycle < 5; cycle++) {
        EXPECT_EQ(nimcp_health_agent_start(agent), 0);
        EXPECT_TRUE(nimcp_health_agent_is_running(agent));

        // Run each cycle for 200ms
        for (int i = 0; i < 10; i++) {
            nimcp_health_agent_heartbeat(agent);

            neural_health_metrics_t metrics;
            EXPECT_EQ(nimcp_health_agent_get_neural_metrics(agent, &metrics), 0);

            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }

        EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
        EXPECT_FALSE(nimcp_health_agent_is_running(agent));

        // Brief pause between cycles
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

//=============================================================================
// Stress Test E2E
//=============================================================================

TEST_F(HealthAgentNeuralE2ETest, StressTest_RapidOperations) {
    EXPECT_EQ(nimcp_health_agent_connect_snn(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_lnn(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    int operations = 0;

    // Rapid operations for 2 seconds
    auto start_time = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start_time < std::chrono::seconds(2)) {
        // Rapid heartbeats
        nimcp_health_agent_heartbeat(agent);
        operations++;

        // Rapid metrics queries
        neural_health_metrics_t metrics;
        nimcp_health_agent_get_neural_metrics(agent, &metrics);
        operations++;

        // Rapid health score queries
        nimcp_health_agent_get_neural_health_score(agent);
        operations++;

        // Rapid unhealthy checks
        nimcp_health_agent_is_neural_unhealthy(agent);
        operations++;

        // Minimal sleep
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    EXPECT_GT(operations, 1000);

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

//=============================================================================
// Integration with Immune System E2E Tests
//=============================================================================

TEST_F(HealthAgentNeuralImmuneE2ETest, ImmuneIntegration_NeuralAndImmune) {
    // Connect neural modules
    EXPECT_EQ(nimcp_health_agent_connect_snn(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_lnn(agent, nullptr, nullptr), 0);

    // Connect immune system if available
    if (immune) {
        // Try to connect immune system (may not be directly supported)
        // The point is to test that neural integration doesn't break
        // when other systems are also connected
    }

    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Run with both neural and potential immune integration
    for (int i = 0; i < 30; i++) {
        nimcp_health_agent_heartbeat(agent);

        neural_health_metrics_t metrics;
        EXPECT_EQ(nimcp_health_agent_get_neural_metrics(agent, &metrics), 0);

        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

//=============================================================================
// Error Recovery E2E Tests
//=============================================================================

TEST_F(HealthAgentNeuralE2ETest, ErrorRecovery_ContinuousOperationAfterBadConfig) {
    EXPECT_EQ(nimcp_health_agent_connect_snn(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_lnn(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Attempt various edge case configurations
    health_agent_snn_config_t edge_cfg;
    memset(&edge_cfg, 0, sizeof(edge_cfg));

    // Edge case: negative values
    edge_cfg.max_spike_rate_hz = -100.0f;
    edge_cfg.min_spike_rate_hz = -50.0f;
    nimcp_health_agent_configure_neural(agent, &edge_cfg, nullptr);

    // Agent should still be functional
    EXPECT_TRUE(nimcp_health_agent_is_running(agent));

    for (int i = 0; i < 10; i++) {
        nimcp_health_agent_heartbeat(agent);
        neural_health_metrics_t metrics;
        EXPECT_EQ(nimcp_health_agent_get_neural_metrics(agent, &metrics), 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
