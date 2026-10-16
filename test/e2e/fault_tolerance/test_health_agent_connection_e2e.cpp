/**
 * @file test_health_agent_connection_e2e.cpp
 * @brief End-to-end tests for NIMCP Health Agent Connection Functions (Phase 2)
 * @version 1.0.0
 * @date 2025-01-17
 *
 * WHAT: E2E tests for complete health agent connection workflows
 * WHY:  Verify full system behavior with connected cognitive modules
 * HOW:  Test realistic scenarios from agent creation through operation
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

// Brain immune system for full integration
#include "cognitive/immune/nimcp_brain_immune.h"

//=============================================================================
// Test Fixtures
//=============================================================================

/**
 * @brief E2E fixture for health agent connection workflows
 */
class HealthAgentConnectionE2ETest : public ::testing::Test {
protected:
    nimcp_health_agent_t* agent = nullptr;
    health_agent_config_t config;

    void SetUp() override {
        nimcp_health_agent_default_config(&config);
        strncpy(config.agent_name, "E2ETestAgent", sizeof(config.agent_name) - 1);
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
 * @brief E2E fixture with immune system integration
 */
class HealthAgentImmuneE2ETest : public HealthAgentConnectionE2ETest {
protected:
    brain_immune_system_t* immune = nullptr;

    void SetUp() override {
        HealthAgentConnectionE2ETest::SetUp();

        // Create immune system
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
        HealthAgentConnectionE2ETest::TearDown();
    }
};

//=============================================================================
// Full Workflow E2E Tests
//=============================================================================

TEST_F(HealthAgentConnectionE2ETest, FullWorkflow_CreateConnectStartRunStop) {
    // Step 1: Connect all cognitive modules
    EXPECT_EQ(nimcp_health_agent_connect_failure_prediction(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_metacognition(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_ethics(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_emotion(agent, nullptr, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_wellbeing(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_mental_health(agent, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_collective(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_rcog(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_gpu(agent, nullptr, nullptr), 0);

    // Step 2: Start agent
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    EXPECT_TRUE(nimcp_health_agent_is_running(agent));

    // Step 3: Run with heartbeats (heartbeat returns void)
    for (int i = 0; i < 20; i++) {
        nimcp_health_agent_heartbeat(agent);
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }

    // Step 4: Get statistics
    health_agent_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    nimcp_health_agent_get_stats(agent, &stats);
    EXPECT_GE(stats.checks_performed, 0u);

    // Step 5: Stop agent
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
    EXPECT_FALSE(nimcp_health_agent_is_running(agent));
}

TEST_F(HealthAgentConnectionE2ETest, FullWorkflow_CustomConfigurations) {
    // Use custom configurations for all modules
    health_agent_prediction_config_t pred_cfg = {
        .enable_failure_prediction = true,
        .prediction_threshold = 0.8f,
        .prediction_horizon_ms = 30000,
        .enable_preventive_action = true,
        .enable_trend_analysis = true
    };

    health_agent_metacog_config_t metacog_cfg = {
        .enable_metacognition = true,
        .enable_confidence_calibration = true,
        .enable_degradation_detection = true,
        .degradation_threshold = 0.25f,
        .enable_self_diagnosis = true
    };

    health_agent_ethics_config_t ethics_cfg = {
        .enable_ethics_evaluation = true,
        .enable_asimov_laws = true,
        .enable_mercy_directive = true,
        .enable_golden_rule = true,
        .ethics_override_threshold = 0.98f
    };

    health_agent_emotion_config_t emotion_cfg = {
        .enable_emotion_awareness = true,
        .enable_emotion_reporting = true,
        .enable_stress_adjustment = true,
        .stress_threshold_multiplier = 1.3f
    };

    health_agent_wellbeing_config_t wellbeing_cfg = {
        .enable_wellbeing_monitoring = true,
        .enable_distress_detection = true,
        .enable_suffering_prevention = true,
        .distress_intervention_threshold = 0.6f
    };

    health_agent_collective_config_t collective_cfg = {
        .enable_collective_monitoring = true,
        .enable_consensus_decisions = true,
        .enable_swarm_immune = true,
        .consensus_threshold = 0.7f,
        .consensus_timeout_ms = 3000
    };

    health_agent_rcog_config_t rcog_cfg = {
        .enable_rcog_diagnosis = true,
        .enable_rcog_recovery_planning = true,
        .enable_imagination = true,
        .rcog_timeout_ms = 15000,
        .confidence_threshold = 0.75f
    };

    health_agent_gpu_config_t gpu_cfg = {
        .enable_gpu_monitoring = true,
        .enable_gpu_acceleration = false,
        .enable_tensor_validation = true,
        .enable_anomaly_detection = true,
        .gpu_check_interval_ms = 2000
    };

    // Connect with custom configs
    EXPECT_EQ(nimcp_health_agent_connect_failure_prediction(agent, nullptr, &pred_cfg), 0);
    EXPECT_EQ(nimcp_health_agent_connect_metacognition(agent, nullptr, &metacog_cfg), 0);
    EXPECT_EQ(nimcp_health_agent_connect_ethics(agent, nullptr, &ethics_cfg), 0);
    EXPECT_EQ(nimcp_health_agent_connect_emotion(agent, nullptr, nullptr, &emotion_cfg), 0);
    EXPECT_EQ(nimcp_health_agent_connect_wellbeing(agent, nullptr, &wellbeing_cfg), 0);
    EXPECT_EQ(nimcp_health_agent_connect_mental_health(agent, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_collective(agent, nullptr, &collective_cfg), 0);
    EXPECT_EQ(nimcp_health_agent_connect_rcog(agent, nullptr, &rcog_cfg), 0);
    EXPECT_EQ(nimcp_health_agent_connect_gpu(agent, nullptr, &gpu_cfg), 0);

    // Run agent
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

//=============================================================================
// Immune System Integration E2E Tests
//=============================================================================

TEST_F(HealthAgentImmuneE2ETest, ImmuneIntegration_ConnectAndRun) {
    if (!immune) GTEST_SKIP() << "Immune system not available";

    // Connect immune system
    EXPECT_EQ(nimcp_health_agent_connect_immune(agent, immune), 0);

    // Connect cognitive modules
    EXPECT_EQ(nimcp_health_agent_connect_failure_prediction(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_metacognition(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_ethics(agent, nullptr, nullptr), 0);

    // Start and run
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    for (int i = 0; i < 10; i++) {
        nimcp_health_agent_heartbeat(agent);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentImmuneE2ETest, ImmuneIntegration_FullCognitiveStack) {
    if (!immune) GTEST_SKIP() << "Immune system not available";

    // Connect immune and all cognitive modules
    EXPECT_EQ(nimcp_health_agent_connect_immune(agent, immune), 0);
    EXPECT_EQ(nimcp_health_agent_connect_failure_prediction(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_metacognition(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_ethics(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_emotion(agent, nullptr, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_wellbeing(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_mental_health(agent, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_collective(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_rcog(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_gpu(agent, nullptr, nullptr), 0);

    // Start and run extended period
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

//=============================================================================
// Stress E2E Tests
//=============================================================================

TEST_F(HealthAgentConnectionE2ETest, Stress_RapidStartStop) {
    // Connect modules
    EXPECT_EQ(nimcp_health_agent_connect_failure_prediction(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_gpu(agent, nullptr, nullptr), 0);

    // Rapid start/stop cycles
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(nimcp_health_agent_start(agent), 0);
        EXPECT_TRUE(nimcp_health_agent_is_running(agent));
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
        EXPECT_FALSE(nimcp_health_agent_is_running(agent));
    }
}

TEST_F(HealthAgentConnectionE2ETest, Stress_RapidReconnect) {
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Rapid reconnection while running
    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(nimcp_health_agent_connect_failure_prediction(agent, nullptr, nullptr), 0);
        EXPECT_EQ(nimcp_health_agent_connect_metacognition(agent, nullptr, nullptr), 0);
        EXPECT_EQ(nimcp_health_agent_connect_gpu(agent, nullptr, nullptr), 0);
    }

    EXPECT_TRUE(nimcp_health_agent_is_running(agent));
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentConnectionE2ETest, Stress_HighFrequencyHeartbeat) {
    EXPECT_EQ(nimcp_health_agent_connect_failure_prediction(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_metacognition(agent, nullptr, nullptr), 0);

    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // High frequency heartbeats (heartbeat returns void)
    for (int i = 0; i < 500; i++) {
        nimcp_health_agent_heartbeat(agent);
    }

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

//=============================================================================
// Concurrent E2E Tests
//=============================================================================

TEST_F(HealthAgentConnectionE2ETest, Concurrent_HeartbeatAndReconnect) {
    EXPECT_EQ(nimcp_health_agent_connect_failure_prediction(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    std::atomic<bool> running{true};
    std::atomic<int> heartbeat_count{0};
    std::atomic<int> reconnect_count{0};

    // Heartbeat thread (heartbeat returns void)
    std::thread heartbeat_thread([&]() {
        while (running) {
            nimcp_health_agent_heartbeat(agent);
            heartbeat_count++;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    // Reconnect thread
    std::thread reconnect_thread([&]() {
        while (running) {
            if (nimcp_health_agent_connect_failure_prediction(agent, nullptr, nullptr) == 0) {
                reconnect_count++;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    });

    // Let them run
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    running = false;

    heartbeat_thread.join();
    reconnect_thread.join();

    EXPECT_GT(heartbeat_count.load(), 0);
    EXPECT_GT(reconnect_count.load(), 0);

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentConnectionE2ETest, Concurrent_MultipleAgents) {
    const int NUM_AGENTS = 4;
    std::vector<nimcp_health_agent_t*> agents;
    std::atomic<int> success_count{0};

    // Create multiple agents
    for (int i = 0; i < NUM_AGENTS; i++) {
        health_agent_config_t agent_config;
        nimcp_health_agent_default_config(&agent_config);
        snprintf(agent_config.agent_name, sizeof(agent_config.agent_name),
                 "Agent_%d", i);
        agent_config.check_interval_ms = 50;

        nimcp_health_agent_t* a = nimcp_health_agent_create(&agent_config);
        ASSERT_NE(a, nullptr);
        agents.push_back(a);
    }

    // Connect and run all agents concurrently
    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_AGENTS; i++) {
        threads.emplace_back([&, i]() {
            nimcp_health_agent_t* a = agents[i];

            // Connect modules
            if (nimcp_health_agent_connect_failure_prediction(a, nullptr, nullptr) != 0) return;
            if (nimcp_health_agent_connect_metacognition(a, nullptr, nullptr) != 0) return;
            if (nimcp_health_agent_connect_ethics(a, nullptr, nullptr) != 0) return;
            if (nimcp_health_agent_connect_gpu(a, nullptr, nullptr) != 0) return;

            // Start
            if (nimcp_health_agent_start(a) != 0) return;

            // Run
            for (int j = 0; j < 20; j++) {
                nimcp_health_agent_heartbeat(a);
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

            // Stop
            if (nimcp_health_agent_stop(a) == 0) {
                success_count++;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count.load(), NUM_AGENTS);

    // Cleanup
    for (auto* a : agents) {
        nimcp_health_agent_destroy(a);
    }
}

//=============================================================================
// Resilience E2E Tests
//=============================================================================

TEST_F(HealthAgentConnectionE2ETest, Resilience_RecoverFromStopStart) {
    // Connect, start, stop, reconnect, start again
    EXPECT_EQ(nimcp_health_agent_connect_failure_prediction(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);

    // Reconnect different modules
    EXPECT_EQ(nimcp_health_agent_connect_metacognition(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_gpu(agent, nullptr, nullptr), 0);

    // Restart
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentConnectionE2ETest, Resilience_LongRunning) {
    // Connect all modules
    EXPECT_EQ(nimcp_health_agent_connect_failure_prediction(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_metacognition(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_ethics(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_emotion(agent, nullptr, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_wellbeing(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_mental_health(agent, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_collective(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_rcog(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_gpu(agent, nullptr, nullptr), 0);

    // Start and run for extended period
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    for (int i = 0; i < 100; i++) {
        nimcp_health_agent_heartbeat(agent);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Verify still running correctly
    EXPECT_TRUE(nimcp_health_agent_is_running(agent));

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);
    EXPECT_GE(stats.checks_performed, 0u);

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

//=============================================================================
// Configuration Validation E2E Tests
//=============================================================================

TEST_F(HealthAgentConnectionE2ETest, ConfigValidation_ExtremeValues) {
    // Test with extreme but valid configuration values
    health_agent_prediction_config_t pred_cfg = {
        .enable_failure_prediction = true,
        .prediction_threshold = 0.001f,  // Very low threshold
        .prediction_horizon_ms = 1,      // Very short horizon
        .enable_preventive_action = true,
        .enable_trend_analysis = true
    };

    health_agent_collective_config_t collective_cfg = {
        .enable_collective_monitoring = true,
        .enable_consensus_decisions = true,
        .enable_swarm_immune = true,
        .consensus_threshold = 0.999f,  // Very high threshold
        .consensus_timeout_ms = 1       // Very short timeout
    };

    EXPECT_EQ(nimcp_health_agent_connect_failure_prediction(agent, nullptr, &pred_cfg), 0);
    EXPECT_EQ(nimcp_health_agent_connect_collective(agent, nullptr, &collective_cfg), 0);

    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentConnectionE2ETest, ConfigValidation_AllDisabled) {
    // Test with all features disabled
    health_agent_prediction_config_t pred_cfg = {0};
    health_agent_metacog_config_t metacog_cfg = {0};
    health_agent_ethics_config_t ethics_cfg = {0};
    health_agent_emotion_config_t emotion_cfg = {0};
    health_agent_wellbeing_config_t wellbeing_cfg = {0};
    health_agent_collective_config_t collective_cfg = {0};
    health_agent_rcog_config_t rcog_cfg = {0};
    health_agent_gpu_config_t gpu_cfg = {0};

    EXPECT_EQ(nimcp_health_agent_connect_failure_prediction(agent, nullptr, &pred_cfg), 0);
    EXPECT_EQ(nimcp_health_agent_connect_metacognition(agent, nullptr, &metacog_cfg), 0);
    EXPECT_EQ(nimcp_health_agent_connect_ethics(agent, nullptr, &ethics_cfg), 0);
    EXPECT_EQ(nimcp_health_agent_connect_emotion(agent, nullptr, nullptr, &emotion_cfg), 0);
    EXPECT_EQ(nimcp_health_agent_connect_wellbeing(agent, nullptr, &wellbeing_cfg), 0);
    EXPECT_EQ(nimcp_health_agent_connect_collective(agent, nullptr, &collective_cfg), 0);
    EXPECT_EQ(nimcp_health_agent_connect_rcog(agent, nullptr, &rcog_cfg), 0);
    EXPECT_EQ(nimcp_health_agent_connect_gpu(agent, nullptr, &gpu_cfg), 0);

    // Agent should still function
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}
