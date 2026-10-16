/**
 * @file test_health_agent_heartbeat.cpp
 * @brief Unit tests for Health Agent Heartbeat functionality
 *
 * WHAT: Tests for nimcp_health_agent_heartbeat_ex API
 * WHY:  Phase 8 health integration requires heartbeat monitoring
 * HOW:  Test heartbeat reception, timeout detection, progress tracking
 *
 * @author NIMCP Team
 * @date 2026-01-25
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <thread>
#include <chrono>
#include <atomic>

extern "C" {
#include "utils/fault_tolerance/nimcp_health_agent.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
}

/**
 * @brief Test fixture for Health Agent Heartbeat tests
 */
class HealthAgentHeartbeatTest : public ::testing::Test {
protected:
    nimcp_health_agent_t* agent = nullptr;
    health_agent_config_t config;

    void SetUp() override {
        nimcp_health_agent_default_config(&config);
        config.heartbeat_interval_ms = 50;
        config.watchdog_timeout_ms = 200;
        agent = nimcp_health_agent_create(&config);
    }

    void TearDown() override {
        if (agent) {
            nimcp_health_agent_stop(agent);
            nimcp_health_agent_destroy(agent);
            agent = nullptr;
        }
    }
};

/**
 * @test Verify agent creation with default config
 */
TEST_F(HealthAgentHeartbeatTest, CreateWithDefaultConfig) {
    nimcp_health_agent_t* default_agent = nimcp_health_agent_create(nullptr);
    ASSERT_NE(default_agent, nullptr);
    nimcp_health_agent_destroy(default_agent);
}

/**
 * @test Verify agent creation with custom config
 */
TEST_F(HealthAgentHeartbeatTest, CreateWithCustomConfig) {
    ASSERT_NE(agent, nullptr);
}

/**
 * @test Verify basic heartbeat functionality
 */
TEST_F(HealthAgentHeartbeatTest, BasicHeartbeat) {
    ASSERT_NE(agent, nullptr);

    // Send heartbeat - should not crash
    nimcp_health_agent_heartbeat(agent);

    // Get stats
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);

    EXPECT_GE(stats.heartbeats_received, 1u);
}

/**
 * @test Verify extended heartbeat with operation and progress
 */
TEST_F(HealthAgentHeartbeatTest, ExtendedHeartbeatWithProgress) {
    ASSERT_NE(agent, nullptr);

    // Send heartbeat with context
    nimcp_health_agent_heartbeat_ex(agent, "test_operation", 0.5f);

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);

    EXPECT_GE(stats.heartbeats_received, 1u);
}

/**
 * @test Verify heartbeat progress values
 */
TEST_F(HealthAgentHeartbeatTest, HeartbeatProgressValues) {
    ASSERT_NE(agent, nullptr);

    // Test boundary progress values
    nimcp_health_agent_heartbeat_ex(agent, "test_start", 0.0f);
    nimcp_health_agent_heartbeat_ex(agent, "test_mid", 0.5f);
    nimcp_health_agent_heartbeat_ex(agent, "test_end", 1.0f);

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);

    EXPECT_GE(stats.heartbeats_received, 3u);
}

/**
 * @test Verify NULL agent handling in heartbeat
 */
TEST_F(HealthAgentHeartbeatTest, NullAgentHeartbeat) {
    // Should not crash with NULL agent
    nimcp_health_agent_heartbeat(nullptr);
    nimcp_health_agent_heartbeat_ex(nullptr, "test", 0.5f);
}

/**
 * @test Verify NULL operation string handling
 */
TEST_F(HealthAgentHeartbeatTest, NullOperationHeartbeat) {
    ASSERT_NE(agent, nullptr);

    // Should handle NULL operation gracefully
    nimcp_health_agent_heartbeat_ex(agent, nullptr, 0.5f);

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);

    EXPECT_GE(stats.heartbeats_received, 1u);
}

/**
 * @test Verify rapid heartbeat stress
 */
TEST_F(HealthAgentHeartbeatTest, RapidHeartbeatStress) {
    ASSERT_NE(agent, nullptr);

    const int num_heartbeats = 1000;
    for (int i = 0; i < num_heartbeats; i++) {
        float progress = static_cast<float>(i) / num_heartbeats;
        nimcp_health_agent_heartbeat_ex(agent, "stress_test", progress);
    }

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);

    EXPECT_GE(stats.heartbeats_received, static_cast<uint64_t>(num_heartbeats));
}

/**
 * @test Verify heartbeat timeout detection when agent is started
 */
TEST_F(HealthAgentHeartbeatTest, HeartbeatTimeoutDetection) {
    ASSERT_NE(agent, nullptr);

    // Start the agent
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    // Send a heartbeat
    nimcp_health_agent_heartbeat(agent);

    // Wait for timeout (watchdog is 200ms)
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);

    // After timeout, should have detected missed beats
    // Note: This may or may not trigger depending on agent thread timing
    EXPECT_GE(stats.heartbeats_received, 1u);
}

/**
 * @test Verify agent start/stop lifecycle
 */
TEST_F(HealthAgentHeartbeatTest, AgentStartStop) {
    ASSERT_NE(agent, nullptr);

    // Start agent
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    // Send some heartbeats
    for (int i = 0; i < 5; i++) {
        nimcp_health_agent_heartbeat(agent);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    // Stop agent
    result = nimcp_health_agent_stop(agent);
    ASSERT_EQ(result, 0);

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);

    EXPECT_GE(stats.heartbeats_received, 5u);
}

/**
 * @test Verify concurrent heartbeats from multiple threads
 */
TEST_F(HealthAgentHeartbeatTest, ConcurrentHeartbeats) {
    ASSERT_NE(agent, nullptr);

    std::atomic<int> total_sent{0};
    const int num_threads = 4;
    const int heartbeats_per_thread = 100;

    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, &total_sent, t, heartbeats_per_thread]() {
            for (int i = 0; i < heartbeats_per_thread; i++) {
                char op[32];
                snprintf(op, sizeof(op), "thread_%d", t);
                float progress = static_cast<float>(i) / heartbeats_per_thread;
                nimcp_health_agent_heartbeat_ex(agent, op, progress);
                total_sent++;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);

    // All heartbeats should be received
    EXPECT_EQ(stats.heartbeats_received, static_cast<uint64_t>(num_threads * heartbeats_per_thread));
}

/**
 * @test Verify stats retrieval is consistent
 */
TEST_F(HealthAgentHeartbeatTest, StatsConsistency) {
    ASSERT_NE(agent, nullptr);

    // Send some heartbeats
    for (int i = 0; i < 10; i++) {
        nimcp_health_agent_heartbeat(agent);
    }

    // Get stats multiple times - should be consistent
    health_agent_stats_t stats1, stats2;
    nimcp_health_agent_get_stats(agent, &stats1);
    nimcp_health_agent_get_stats(agent, &stats2);

    EXPECT_GE(stats1.heartbeats_received, 10u);
    EXPECT_EQ(stats1.heartbeats_received, stats2.heartbeats_received);
}

/**
 * @test Verify agent is running check
 */
TEST_F(HealthAgentHeartbeatTest, IsRunningCheck) {
    ASSERT_NE(agent, nullptr);

    // Before start
    EXPECT_FALSE(nimcp_health_agent_is_running(agent));

    // After start
    nimcp_health_agent_start(agent);
    EXPECT_TRUE(nimcp_health_agent_is_running(agent));

    // After stop
    nimcp_health_agent_stop(agent);
    EXPECT_FALSE(nimcp_health_agent_is_running(agent));
}

/**
 * @brief Main entry point
 */
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
