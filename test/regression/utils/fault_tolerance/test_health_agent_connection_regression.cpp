/**
 * @file test_health_agent_connection_regression.cpp
 * @brief Regression tests for NIMCP Health Agent Connection Functions (Phase 2)
 * @version 1.0.0
 * @date 2025-01-17
 *
 * WHAT: Regression tests to ensure connection functions don't break
 * WHY:  Prevent regressions in connection behavior across releases
 * HOW:  Test specific behaviors that must remain consistent
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <cstring>
#include <atomic>
#include <vector>

// Health agent header
#include "utils/fault_tolerance/nimcp_health_agent.h"

//=============================================================================
// Test Fixtures
//=============================================================================

/**
 * @brief Fixture for health agent connection regression tests
 */
class HealthAgentConnectionRegressionTest : public ::testing::Test {
protected:
    nimcp_health_agent_t* agent = nullptr;
    health_agent_config_t config;

    void SetUp() override {
        nimcp_health_agent_default_config(&config);
        config.check_interval_ms = 100;
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

//=============================================================================
// Return Value Contract Tests
// These tests verify the documented return value contract is maintained
//=============================================================================

TEST_F(HealthAgentConnectionRegressionTest, ReturnValues_NullAgentAlwaysReturnsMinusOne) {
    // All connect functions MUST return -1 for NULL agent
    // This is a documented behavior that must not change

    EXPECT_EQ(nimcp_health_agent_connect_failure_prediction(nullptr, nullptr, nullptr), -1);
    EXPECT_EQ(nimcp_health_agent_connect_metacognition(nullptr, nullptr, nullptr), -1);
    EXPECT_EQ(nimcp_health_agent_connect_ethics(nullptr, nullptr, nullptr), -1);
    EXPECT_EQ(nimcp_health_agent_connect_emotion(nullptr, nullptr, nullptr, nullptr), -1);
    EXPECT_EQ(nimcp_health_agent_connect_wellbeing(nullptr, nullptr, nullptr), -1);
    EXPECT_EQ(nimcp_health_agent_connect_mental_health(nullptr, nullptr), -1);
    EXPECT_EQ(nimcp_health_agent_connect_collective(nullptr, nullptr, nullptr), -1);
    EXPECT_EQ(nimcp_health_agent_connect_rcog(nullptr, nullptr, nullptr), -1);
    EXPECT_EQ(nimcp_health_agent_connect_gpu(nullptr, nullptr, nullptr), -1);
}

TEST_F(HealthAgentConnectionRegressionTest, ReturnValues_ValidAgentReturnsZero) {
    // All connect functions MUST return 0 for valid agent
    // Even with NULL module pointers (allowed for lazy connection)

    EXPECT_EQ(nimcp_health_agent_connect_failure_prediction(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_metacognition(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_ethics(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_emotion(agent, nullptr, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_wellbeing(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_mental_health(agent, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_collective(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_rcog(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_gpu(agent, nullptr, nullptr), 0);
}

//=============================================================================
// Default Configuration Contract Tests
// These tests verify default configurations are applied correctly
//=============================================================================

TEST_F(HealthAgentConnectionRegressionTest, DefaultConfig_FailurePrediction) {
    // When config is NULL, defaults MUST be applied
    // This tests that default configuration behavior is preserved

    EXPECT_EQ(nimcp_health_agent_connect_failure_prediction(agent, nullptr, nullptr), 0);

    // The agent should be functional after connection with defaults
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    EXPECT_TRUE(nimcp_health_agent_is_running(agent));
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentConnectionRegressionTest, DefaultConfig_Metacognition) {
    EXPECT_EQ(nimcp_health_agent_connect_metacognition(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    EXPECT_TRUE(nimcp_health_agent_is_running(agent));
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentConnectionRegressionTest, DefaultConfig_Ethics) {
    EXPECT_EQ(nimcp_health_agent_connect_ethics(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    EXPECT_TRUE(nimcp_health_agent_is_running(agent));
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentConnectionRegressionTest, DefaultConfig_Emotion) {
    EXPECT_EQ(nimcp_health_agent_connect_emotion(agent, nullptr, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    EXPECT_TRUE(nimcp_health_agent_is_running(agent));
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentConnectionRegressionTest, DefaultConfig_Wellbeing) {
    EXPECT_EQ(nimcp_health_agent_connect_wellbeing(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    EXPECT_TRUE(nimcp_health_agent_is_running(agent));
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentConnectionRegressionTest, DefaultConfig_MentalHealth) {
    EXPECT_EQ(nimcp_health_agent_connect_mental_health(agent, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    EXPECT_TRUE(nimcp_health_agent_is_running(agent));
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentConnectionRegressionTest, DefaultConfig_Collective) {
    EXPECT_EQ(nimcp_health_agent_connect_collective(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    EXPECT_TRUE(nimcp_health_agent_is_running(agent));
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentConnectionRegressionTest, DefaultConfig_Rcog) {
    EXPECT_EQ(nimcp_health_agent_connect_rcog(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    EXPECT_TRUE(nimcp_health_agent_is_running(agent));
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentConnectionRegressionTest, DefaultConfig_Gpu) {
    EXPECT_EQ(nimcp_health_agent_connect_gpu(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    EXPECT_TRUE(nimcp_health_agent_is_running(agent));
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

//=============================================================================
// Custom Configuration Contract Tests
// These tests verify custom configurations are applied correctly
//=============================================================================

TEST_F(HealthAgentConnectionRegressionTest, CustomConfig_FailurePrediction) {
    health_agent_prediction_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.enable_failure_prediction = false;  // Explicitly disable
    cfg.prediction_threshold = 0.5f;
    cfg.prediction_horizon_ms = 30000;

    EXPECT_EQ(nimcp_health_agent_connect_failure_prediction(agent, nullptr, &cfg), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentConnectionRegressionTest, CustomConfig_Collective) {
    health_agent_collective_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.enable_collective_monitoring = true;
    cfg.enable_consensus_decisions = false;
    cfg.consensus_threshold = 0.8f;
    cfg.consensus_timeout_ms = 2000;

    EXPECT_EQ(nimcp_health_agent_connect_collective(agent, nullptr, &cfg), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentConnectionRegressionTest, CustomConfig_Gpu) {
    health_agent_gpu_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.enable_gpu_monitoring = true;
    cfg.enable_gpu_acceleration = false;
    cfg.gpu_check_interval_ms = 5000;

    EXPECT_EQ(nimcp_health_agent_connect_gpu(agent, nullptr, &cfg), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

//=============================================================================
// Idempotency Tests
// Multiple calls should have the same effect as a single call
//=============================================================================

TEST_F(HealthAgentConnectionRegressionTest, Idempotency_MultipleConnectsSameModule) {
    // Connecting same module multiple times should succeed
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(nimcp_health_agent_connect_failure_prediction(agent, nullptr, nullptr), 0);
    }

    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentConnectionRegressionTest, Idempotency_AllModulesMultipleTimes) {
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(nimcp_health_agent_connect_failure_prediction(agent, nullptr, nullptr), 0);
        EXPECT_EQ(nimcp_health_agent_connect_metacognition(agent, nullptr, nullptr), 0);
        EXPECT_EQ(nimcp_health_agent_connect_ethics(agent, nullptr, nullptr), 0);
        EXPECT_EQ(nimcp_health_agent_connect_emotion(agent, nullptr, nullptr, nullptr), 0);
        EXPECT_EQ(nimcp_health_agent_connect_wellbeing(agent, nullptr, nullptr), 0);
        EXPECT_EQ(nimcp_health_agent_connect_mental_health(agent, nullptr), 0);
        EXPECT_EQ(nimcp_health_agent_connect_collective(agent, nullptr, nullptr), 0);
        EXPECT_EQ(nimcp_health_agent_connect_rcog(agent, nullptr, nullptr), 0);
        EXPECT_EQ(nimcp_health_agent_connect_gpu(agent, nullptr, nullptr), 0);
    }

    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

//=============================================================================
// Connection Order Independence Tests
// Connections should work regardless of order
//=============================================================================

TEST_F(HealthAgentConnectionRegressionTest, ConnectionOrder_Forward) {
    EXPECT_EQ(nimcp_health_agent_connect_failure_prediction(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_metacognition(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_ethics(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_emotion(agent, nullptr, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_wellbeing(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_mental_health(agent, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_collective(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_rcog(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_gpu(agent, nullptr, nullptr), 0);

    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentConnectionRegressionTest, ConnectionOrder_Reverse) {
    EXPECT_EQ(nimcp_health_agent_connect_gpu(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_rcog(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_collective(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_mental_health(agent, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_wellbeing(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_emotion(agent, nullptr, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_ethics(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_metacognition(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_failure_prediction(agent, nullptr, nullptr), 0);

    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentConnectionRegressionTest, ConnectionOrder_Random) {
    EXPECT_EQ(nimcp_health_agent_connect_rcog(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_emotion(agent, nullptr, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_gpu(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_failure_prediction(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_wellbeing(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_collective(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_mental_health(agent, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_ethics(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_metacognition(agent, nullptr, nullptr), 0);

    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

//=============================================================================
// Lifecycle State Independence Tests
// Connections should work in any agent state
//=============================================================================

TEST_F(HealthAgentConnectionRegressionTest, Lifecycle_ConnectBeforeStart) {
    EXPECT_EQ(nimcp_health_agent_connect_failure_prediction(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_gpu(agent, nullptr, nullptr), 0);

    EXPECT_FALSE(nimcp_health_agent_is_running(agent));
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    EXPECT_TRUE(nimcp_health_agent_is_running(agent));
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentConnectionRegressionTest, Lifecycle_ConnectWhileRunning) {
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    EXPECT_TRUE(nimcp_health_agent_is_running(agent));

    EXPECT_EQ(nimcp_health_agent_connect_failure_prediction(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_gpu(agent, nullptr, nullptr), 0);

    EXPECT_TRUE(nimcp_health_agent_is_running(agent));
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentConnectionRegressionTest, Lifecycle_ConnectAfterStop) {
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
    EXPECT_FALSE(nimcp_health_agent_is_running(agent));

    // Should still be able to connect after stop
    EXPECT_EQ(nimcp_health_agent_connect_failure_prediction(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_gpu(agent, nullptr, nullptr), 0);

    // And restart
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    EXPECT_TRUE(nimcp_health_agent_is_running(agent));
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

//=============================================================================
// Thread Safety Regression Tests
// Concurrent operations must not cause crashes or data corruption
//=============================================================================

TEST_F(HealthAgentConnectionRegressionTest, ThreadSafety_ConcurrentConnectSameModule) {
    const int NUM_THREADS = 8;
    const int ITERATIONS = 100;
    std::atomic<int> success_count{0};
    std::atomic<int> failure_count{0};

    auto worker = [&]() {
        for (int i = 0; i < ITERATIONS; i++) {
            int result = nimcp_health_agent_connect_failure_prediction(agent, nullptr, nullptr);
            if (result == 0) {
                success_count++;
            } else {
                failure_count++;
            }
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(worker);
    }

    for (auto& t : threads) {
        t.join();
    }

    // All should succeed due to mutex protection
    EXPECT_EQ(success_count.load(), NUM_THREADS * ITERATIONS);
    EXPECT_EQ(failure_count.load(), 0);
}

TEST_F(HealthAgentConnectionRegressionTest, ThreadSafety_ConcurrentConnectDifferentModules) {
    const int ITERATIONS = 50;
    std::atomic<bool> any_failure{false};

    auto connect_prediction = [&]() {
        for (int i = 0; i < ITERATIONS; i++) {
            if (nimcp_health_agent_connect_failure_prediction(agent, nullptr, nullptr) != 0) {
                any_failure = true;
            }
        }
    };

    auto connect_gpu = [&]() {
        for (int i = 0; i < ITERATIONS; i++) {
            if (nimcp_health_agent_connect_gpu(agent, nullptr, nullptr) != 0) {
                any_failure = true;
            }
        }
    };

    auto connect_collective = [&]() {
        for (int i = 0; i < ITERATIONS; i++) {
            if (nimcp_health_agent_connect_collective(agent, nullptr, nullptr) != 0) {
                any_failure = true;
            }
        }
    };

    auto connect_rcog = [&]() {
        for (int i = 0; i < ITERATIONS; i++) {
            if (nimcp_health_agent_connect_rcog(agent, nullptr, nullptr) != 0) {
                any_failure = true;
            }
        }
    };

    std::thread t1(connect_prediction);
    std::thread t2(connect_gpu);
    std::thread t3(connect_collective);
    std::thread t4(connect_rcog);

    t1.join();
    t2.join();
    t3.join();
    t4.join();

    EXPECT_FALSE(any_failure.load());
}

TEST_F(HealthAgentConnectionRegressionTest, ThreadSafety_ConnectWhileAgentRunning) {
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    const int ITERATIONS = 50;
    std::atomic<bool> any_failure{false};

    auto worker = [&]() {
        for (int i = 0; i < ITERATIONS; i++) {
            if (nimcp_health_agent_connect_failure_prediction(agent, nullptr, nullptr) != 0 ||
                nimcp_health_agent_connect_metacognition(agent, nullptr, nullptr) != 0 ||
                nimcp_health_agent_connect_ethics(agent, nullptr, nullptr) != 0) {
                any_failure = true;
            }
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < 4; i++) {
        threads.emplace_back(worker);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
    EXPECT_FALSE(any_failure.load());
}

//=============================================================================
// Memory Safety Regression Tests
// Connections should not cause memory leaks or corruption
//=============================================================================

TEST_F(HealthAgentConnectionRegressionTest, MemorySafety_RepeatedConnectDisconnect) {
    // Repeated connect cycles should not leak memory
    for (int cycle = 0; cycle < 100; cycle++) {
        EXPECT_EQ(nimcp_health_agent_connect_failure_prediction(agent, nullptr, nullptr), 0);
        EXPECT_EQ(nimcp_health_agent_connect_metacognition(agent, nullptr, nullptr), 0);
        EXPECT_EQ(nimcp_health_agent_connect_ethics(agent, nullptr, nullptr), 0);
        EXPECT_EQ(nimcp_health_agent_connect_emotion(agent, nullptr, nullptr, nullptr), 0);
        EXPECT_EQ(nimcp_health_agent_connect_wellbeing(agent, nullptr, nullptr), 0);
        EXPECT_EQ(nimcp_health_agent_connect_mental_health(agent, nullptr), 0);
        EXPECT_EQ(nimcp_health_agent_connect_collective(agent, nullptr, nullptr), 0);
        EXPECT_EQ(nimcp_health_agent_connect_rcog(agent, nullptr, nullptr), 0);
        EXPECT_EQ(nimcp_health_agent_connect_gpu(agent, nullptr, nullptr), 0);
    }

    // Agent should still work
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentConnectionRegressionTest, MemorySafety_CreateDestroyWithConnections) {
    // Create-connect-destroy cycle should not leak
    for (int cycle = 0; cycle < 10; cycle++) {
        nimcp_health_agent_t* temp_agent = nimcp_health_agent_create(&config);
        ASSERT_NE(temp_agent, nullptr);

        EXPECT_EQ(nimcp_health_agent_connect_failure_prediction(temp_agent, nullptr, nullptr), 0);
        EXPECT_EQ(nimcp_health_agent_connect_gpu(temp_agent, nullptr, nullptr), 0);
        EXPECT_EQ(nimcp_health_agent_connect_collective(temp_agent, nullptr, nullptr), 0);

        nimcp_health_agent_destroy(temp_agent);
    }
}
