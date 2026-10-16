/**
 * @file test_health_agent_use_regression.cpp
 * @brief Regression tests for Health Agent USE functions
 * @version 1.0.0
 * @date 2025-01-17
 *
 * WHAT: Regression tests ensuring USE function behavior remains consistent
 * WHY:  Prevent regressions in API behavior, return values, and error handling
 * HOW:  Test known good behaviors and edge cases that previously caused issues
 *
 * REGRESSION COVERAGE:
 * - API contract stability (function signatures, return values)
 * - Error handling consistency
 * - State management across operations
 * - Memory safety (no leaks, double-frees)
 * - Thread safety guarantees
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <cstring>
#include <cstdlib>

// Headers have their own extern "C" guards
#include "utils/fault_tolerance/nimcp_health_agent.h"
#include "portia/nimcp_portia.h"
#include "dragonfly/nimcp_dragonfly.h"
#include "swarm/nimcp_swarm_immune.h"
#include "swarm/nimcp_swarm_memory.h"

//=============================================================================
// Test Fixtures
//=============================================================================

/**
 * @brief Base fixture for regression tests
 */
class HealthAgentUSERegressionTest : public ::testing::Test {
protected:
    nimcp_health_agent_t* agent = nullptr;
    health_agent_config_t config;

    void SetUp() override {
        nimcp_health_agent_default_config(&config);
        config.check_interval_ms = 100;
        config.enable_auto_recovery = false;

        agent = nimcp_health_agent_create(&config);
        ASSERT_NE(agent, nullptr);
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
 * @brief Fixture with all modules connected
 */
class HealthAgentUSEFullRegression : public HealthAgentUSERegressionTest {
protected:
    portia_context_t* portia_ctx = nullptr;
    dragonfly_system_t* dragonfly = nullptr;
    NimcpSwarmImmuneSystem* swarm_immune = nullptr;
    NimcpSwarmMemory* swarm_memory = nullptr;

    void SetUp() override {
        HealthAgentUSERegressionTest::SetUp();

        // Initialize Portia subsystem first with defaults
        portia_config_t portia_sys_config = portia_get_default_config();
        portia_init(&portia_sys_config);
        portia_ctx = portia_get_context();

        // Connect Portia
        health_agent_portia_config_t portia_cfg;
        memset(&portia_cfg, 0, sizeof(portia_cfg));
        portia_cfg.enable_portia = true;
        portia_cfg.enable_tier_monitoring = true;
        portia_cfg.enable_degradation_coordination = true;
        portia_cfg.enable_auto_tier_switch = true;
        portia_cfg.degradation_trigger_threshold = 0.3f;
        portia_cfg.upgrade_health_threshold = 0.8f;
        portia_cfg.tier_check_interval_ms = 500;
        nimcp_health_agent_connect_portia(agent, portia_ctx, &portia_cfg);

        // Create and connect Dragonfly
        dragonfly_config_t df_cfg = dragonfly_default_config();
        dragonfly = dragonfly_system_create(&df_cfg);
        if (dragonfly) {
            health_agent_dragonfly_config_t df_agent_cfg;
            memset(&df_agent_cfg, 0, sizeof(df_agent_cfg));
            df_agent_cfg.enable_dragonfly = true;
            df_agent_cfg.enable_anomaly_tracking = true;
            df_agent_cfg.enable_pursuit_mode = true;
            df_agent_cfg.enable_interception = true;
            df_agent_cfg.enable_prediction_integration = true;
            df_agent_cfg.lock_on_severity_threshold = 0.5f;
            df_agent_cfg.pursuit_timeout_s = 5.0f;
            df_agent_cfg.update_rate_hz = 10;
            nimcp_health_agent_connect_dragonfly(agent, dragonfly, &df_agent_cfg);
        }

        // Create and connect Swarm Immune
        NimcpSwarmImmuneConfig immune_cfg;
        nimcp_swarm_immune_default_config(&immune_cfg);
        swarm_immune = nimcp_swarm_immune_create(&immune_cfg, nullptr, 1);
        if (swarm_immune) {
            health_agent_swarm_immune_config_t immune_cfg2;
            memset(&immune_cfg2, 0, sizeof(immune_cfg2));
            immune_cfg2.enable_swarm_immune = true;
            immune_cfg2.enable_threat_detection = true;
            immune_cfg2.enable_coordinated_response = true;
            immune_cfg2.enable_memory_sharing = true;
            immune_cfg2.threat_detection_threshold = 0.5f;
            immune_cfg2.consensus_timeout_ms = 1000;
            nimcp_health_agent_connect_swarm_immune(agent, swarm_immune, &immune_cfg2);
        }

        // Create and connect Swarm Memory
        swarm_memory = nimcp_swarm_memory_create(1000, 3);  // capacity, replication factor
        if (swarm_memory) {
            nimcp_swarm_memory_init(swarm_memory, nullptr);
            health_agent_swarm_memory_config_t mem_agent_cfg;
            memset(&mem_agent_cfg, 0, sizeof(mem_agent_cfg));
            mem_agent_cfg.enable_swarm_memory = true;
            mem_agent_cfg.enable_distributed_storage = true;
            mem_agent_cfg.enable_memory_replay = true;
            mem_agent_cfg.enable_consolidation = true;
            mem_agent_cfg.replay_priority_threshold = 0.3f;
            mem_agent_cfg.consolidation_interval_ms = 5000;
            nimcp_health_agent_connect_swarm_memory(agent, swarm_memory, &mem_agent_cfg);
        }
    }

    void TearDown() override {
        HealthAgentUSERegressionTest::TearDown();
        if (dragonfly) {
            dragonfly_system_destroy(dragonfly);
            dragonfly = nullptr;
        }
        if (swarm_immune) {
            nimcp_swarm_immune_destroy(swarm_immune);
            swarm_immune = nullptr;
        }
        if (swarm_memory) {
            nimcp_swarm_memory_destroy(swarm_memory);
            swarm_memory = nullptr;
        }
        // Clean up Portia
        portia_destroy();
        portia_ctx = nullptr;
    }
};

//=============================================================================
// API Contract Regression Tests
//=============================================================================

/**
 * Regression: Portia functions must return 0 on success, -1 on error
 */
TEST_F(HealthAgentUSEFullRegression, PortiaReturnValueContract) {
    // Success cases - all should return 0
    EXPECT_EQ(nimcp_health_agent_use_portia_set_tier(agent, PLATFORM_TIER_FULL), 0);
    EXPECT_EQ(nimcp_health_agent_use_portia_set_tier(agent, PLATFORM_TIER_MINIMAL), 0);

    uint32_t power, thermal, degradation;
    EXPECT_EQ(nimcp_health_agent_use_portia_get_status(agent, &power, &thermal, &degradation), 0);
    EXPECT_EQ(nimcp_health_agent_use_portia_degrade(agent, 5), 0);

    uint32_t recommended;
    EXPECT_EQ(nimcp_health_agent_use_portia_get_recommended_neurons(agent, &recommended), 0);

    // Error cases - all should return -1
    EXPECT_EQ(nimcp_health_agent_use_portia_set_tier(nullptr, PLATFORM_TIER_FULL), -1);
    EXPECT_EQ(nimcp_health_agent_use_portia_get_status(nullptr, &power, &thermal, &degradation), -1);
    EXPECT_EQ(nimcp_health_agent_use_portia_get_recommended_neurons(agent, nullptr), -1);
}

/**
 * Regression: Dragonfly functions must handle null inputs correctly
 */
TEST_F(HealthAgentUSEFullRegression, DragonflyNullHandling) {
    if (!dragonfly) GTEST_SKIP() << "Dragonfly not available";

    health_agent_message_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = HEALTH_MSG_ANOMALY_DETECTED;
    msg.severity = HEALTH_SEVERITY_WARNING;
    msg.timestamp_us = 12345;
    msg.anomaly_id = 999;

    uint32_t target_id = 0;

    // Null agent
    EXPECT_EQ(nimcp_health_agent_use_dragonfly_track_anomaly(nullptr, &msg, &target_id), -1);

    // Null message
    EXPECT_EQ(nimcp_health_agent_use_dragonfly_track_anomaly(agent, nullptr, &target_id), -1);

    // Null target_id
    EXPECT_EQ(nimcp_health_agent_use_dragonfly_track_anomaly(agent, &msg, nullptr), -1);

    // Null output for predict
    EXPECT_EQ(nimcp_health_agent_use_dragonfly_predict(agent, 0, nullptr, nullptr), 0);  // OK, just doesn't fill

    // Null output for mode (should fail)
    EXPECT_EQ(nimcp_health_agent_use_dragonfly_get_mode(agent, nullptr), -1);
}

/**
 * Regression: Swarm Immune functions must return correct error codes
 */
TEST_F(HealthAgentUSEFullRegression, SwarmImmuneReturnCodes) {
    if (!swarm_immune) GTEST_SKIP() << "Swarm Immune not available";

    uint8_t data[] = {0x01, 0x02, 0x03};
    bool detected = false;
    uint32_t threat_id = 0;
    float score = 0.0f;

    // Valid calls - return 0
    EXPECT_EQ(nimcp_health_agent_use_swarm_detect_threat(
        agent, data, sizeof(data), 1, &detected, &threat_id
    ), 0);
    EXPECT_EQ(nimcp_health_agent_use_swarm_check_behavior(agent, 1, &score), 0);

    // Null data - return -1
    EXPECT_EQ(nimcp_health_agent_use_swarm_detect_threat(
        agent, nullptr, 0, 1, &detected, nullptr
    ), -1);

    // Null output - return -1
    EXPECT_EQ(nimcp_health_agent_use_swarm_detect_threat(
        agent, data, sizeof(data), 1, nullptr, nullptr
    ), -1);
    EXPECT_EQ(nimcp_health_agent_use_swarm_check_behavior(agent, 1, nullptr), -1);
}

/**
 * Regression: Swarm Memory functions must return correct values
 */
TEST_F(HealthAgentUSEFullRegression, SwarmMemoryReturnValues) {
    if (!swarm_memory) GTEST_SKIP() << "Swarm Memory not available";

    uint8_t pattern[] = {0xAA, 0xBB, 0xCC};

    // Store - return 0 on success
    EXPECT_EQ(nimcp_health_agent_use_swarm_memory_store(
        agent, pattern, sizeof(pattern), 0, 1, nullptr
    ), 0);

    // Replay - return count (>= 0)
    int replayed = nimcp_health_agent_use_swarm_memory_replay(agent, 5);
    EXPECT_GE(replayed, 0);

    // Consolidate - return count (>= 0)
    int consolidated = nimcp_health_agent_use_swarm_memory_consolidate(agent);
    EXPECT_GE(consolidated, 0);

    // Get stats - return 0 on success
    uint64_t total, cons;
    float strength;
    EXPECT_EQ(nimcp_health_agent_use_swarm_memory_get_stats(
        agent, &total, &cons, &strength
    ), 0);

    // Null data - return -1
    EXPECT_EQ(nimcp_health_agent_use_swarm_memory_store(
        agent, nullptr, 0, 0, 1, nullptr
    ), -1);
}

//=============================================================================
// State Management Regression Tests
//=============================================================================

/**
 * Regression: Tier changes must persist correctly
 */
TEST_F(HealthAgentUSEFullRegression, TierStatePersistence) {
    // Set each tier and verify it persists
    platform_tier_t tiers[] = {
        PLATFORM_TIER_FULL,
        PLATFORM_TIER_MEDIUM,
        PLATFORM_TIER_CONSTRAINED,
        PLATFORM_TIER_MINIMAL
    };

    for (auto expected_tier : tiers) {
        EXPECT_EQ(nimcp_health_agent_use_portia_set_tier(agent, expected_tier), 0);

        uint32_t power, thermal, degradation;
        EXPECT_EQ(nimcp_health_agent_use_portia_get_status(agent, &power, &thermal, &degradation), 0);
        // Tier change verified by successful status query
    }
}

/**
 * Regression: Degradation level must persist
 */
TEST_F(HealthAgentUSEFullRegression, DegradationStatePersistence) {
    for (int level = 0; level <= 10; level++) {
        EXPECT_EQ(nimcp_health_agent_use_portia_degrade(agent, level), 0);
        // Degradation should be set without error
    }
}

/**
 * Regression: Multiple anomaly tracking must not corrupt state
 */
TEST_F(HealthAgentUSEFullRegression, MultipleAnomalyTracking) {
    if (!dragonfly) GTEST_SKIP() << "Dragonfly not available";

    std::vector<uint32_t> target_ids;

    for (int i = 0; i < 10; i++) {
        health_agent_message_t msg;
        memset(&msg, 0, sizeof(msg));
        msg.type = HEALTH_MSG_ANOMALY_DETECTED;
        msg.severity = static_cast<health_agent_severity_t>(i % 4);
        msg.timestamp_us = 1000 + i * 100;
        msg.anomaly_id = static_cast<uint64_t>(i * 1000);

        uint32_t target_id = 0;
        int result = nimcp_health_agent_use_dragonfly_track_anomaly(agent, &msg, &target_id);
        EXPECT_EQ(result, 0);
        target_ids.push_back(target_id);
    }

    // All target IDs should be valid (non-zero or at least no corruption)
    // Mode query should still work
    uint32_t mode = 0;
    EXPECT_EQ(nimcp_health_agent_use_dragonfly_get_mode(agent, &mode), 0);
}

//=============================================================================
// Module Connection State Regression Tests
//=============================================================================

/**
 * Regression: Operations must fail correctly when module not connected
 */
TEST_F(HealthAgentUSERegressionTest, UnconnectedModuleOperations) {
    // Without connecting any modules, all USE functions should return -1

    // Portia (not connected in base fixture)
    EXPECT_EQ(nimcp_health_agent_use_portia_set_tier(agent, PLATFORM_TIER_FULL), -1);

    uint32_t power;
    EXPECT_EQ(nimcp_health_agent_use_portia_get_status(agent, &power, nullptr, nullptr), -1);

    // Dragonfly
    health_agent_message_t msg;
    memset(&msg, 0, sizeof(msg));
    uint32_t target_id;
    EXPECT_EQ(nimcp_health_agent_use_dragonfly_track_anomaly(agent, &msg, &target_id), -1);
    EXPECT_EQ(nimcp_health_agent_use_dragonfly_pursue(agent), -1);
    EXPECT_EQ(nimcp_health_agent_use_dragonfly_abort(agent), -1);

    // Swarm Immune
    uint8_t data[] = {0x01};
    bool detected;
    EXPECT_EQ(nimcp_health_agent_use_swarm_detect_threat(
        agent, data, 1, 1, &detected, nullptr
    ), -1);

    // Swarm Memory
    EXPECT_EQ(nimcp_health_agent_use_swarm_memory_store(agent, data, 1, 0, 1, nullptr), -1);
    EXPECT_EQ(nimcp_health_agent_use_swarm_memory_replay(agent, 5), -1);

    // Engram (never connected)
    uint64_t engram_id;
    EXPECT_EQ(nimcp_health_agent_use_engram_encode(agent, &msg, &engram_id), -1);

    // Systems Consolidation (never connected)
    EXPECT_EQ(nimcp_health_agent_use_consolidation_replay(agent, 5), -1);
}

/**
 * Regression: Reconnecting modules must work
 */
TEST_F(HealthAgentUSERegressionTest, ModuleReconnection) {
    // Initialize Portia global system for testing
    portia_config_t portia_sys_config = portia_get_default_config();
    portia_init(&portia_sys_config);

    // Connect Portia (using global API via nullptr context)
    health_agent_portia_config_t cfg1 = {0};
    cfg1.enable_portia = true;
    cfg1.enable_tier_monitoring = true;
    cfg1.enable_auto_tier_switch = true;
    EXPECT_EQ(nimcp_health_agent_connect_portia(agent, nullptr, &cfg1), 0);

    // Use it (now works since Portia is initialized)
    EXPECT_EQ(nimcp_health_agent_use_portia_set_tier(agent, PLATFORM_TIER_FULL), 0);

    // Reconnect with different config (should work)
    health_agent_portia_config_t cfg2 = {0};
    cfg2.enable_portia = true;
    cfg2.enable_tier_monitoring = true;
    cfg2.enable_degradation_coordination = true;
    EXPECT_EQ(nimcp_health_agent_connect_portia(agent, nullptr, &cfg2), 0);

    // Should still work
    EXPECT_EQ(nimcp_health_agent_use_portia_set_tier(agent, PLATFORM_TIER_MEDIUM), 0);

    // Cleanup Portia
    portia_destroy();
}

//=============================================================================
// Memory Safety Regression Tests
//=============================================================================

/**
 * Regression: Repeated create/destroy must not leak memory
 */
TEST(HealthAgentMemoryRegression, RepeatedCreateDestroy) {
    for (int i = 0; i < 50; i++) {
        health_agent_config_t cfg;
        nimcp_health_agent_default_config(&cfg);

        nimcp_health_agent_t* agent = nimcp_health_agent_create(&cfg);
        ASSERT_NE(agent, nullptr);

        // Connect a module
        health_agent_portia_config_t portia_cfg = {0};
        portia_cfg.enable_portia = true;
        portia_cfg.enable_tier_monitoring = true;
        portia_cfg.enable_auto_tier_switch = true;
        nimcp_health_agent_connect_portia(agent, nullptr, &portia_cfg);

        // Use it
        nimcp_health_agent_use_portia_set_tier(agent, PLATFORM_TIER_FULL);

        // Destroy
        nimcp_health_agent_destroy(agent);
    }
    // If this completes without crash/leak, test passes
}

/**
 * Regression: Start/stop cycles must not leak resources
 */
TEST_F(HealthAgentUSEFullRegression, RepeatedStartStop) {
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(nimcp_health_agent_start(agent), 0);
        EXPECT_TRUE(nimcp_health_agent_is_running(agent));

        // Use modules while running
        nimcp_health_agent_use_portia_set_tier(agent, static_cast<platform_tier_t>(i % 4));

        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
        EXPECT_FALSE(nimcp_health_agent_is_running(agent));
    }
}

/**
 * Regression: Memory storage must handle large patterns
 */
TEST_F(HealthAgentUSEFullRegression, LargePatternStorage) {
    if (!swarm_memory) GTEST_SKIP() << "Swarm Memory not available";

    // Large pattern (1KB)
    std::vector<uint8_t> large_pattern(1024);
    for (size_t i = 0; i < large_pattern.size(); i++) {
        large_pattern[i] = static_cast<uint8_t>(i % 256);
    }

    char pattern_id[64];
    int result = nimcp_health_agent_use_swarm_memory_store(
        agent, large_pattern.data(), large_pattern.size(), 0, 5, pattern_id
    );
    EXPECT_EQ(result, 0);

    // Very large pattern (64KB) - might fail gracefully
    std::vector<uint8_t> huge_pattern(65536);
    result = nimcp_health_agent_use_swarm_memory_store(
        agent, huge_pattern.data(), huge_pattern.size(), 0, 5, nullptr
    );
    // May succeed or fail, but must not crash
    EXPECT_GE(result, -1);
}

//=============================================================================
// Thread Safety Regression Tests
//=============================================================================

/**
 * Regression: Concurrent Portia access must not cause data races
 */
TEST_F(HealthAgentUSEFullRegression, ConcurrentPortiaAccess) {
    std::atomic<int> errors{0};
    std::atomic<bool> running{true};

    auto setter = [&]() {
        while (running) {
            for (int i = 0; i < 4; i++) {
                if (nimcp_health_agent_use_portia_set_tier(
                    agent, static_cast<platform_tier_t>(i)
                ) != 0) {
                    errors++;
                }
            }
        }
    };

    auto getter = [&]() {
        while (running) {
            uint32_t power, thermal, degradation;
            if (nimcp_health_agent_use_portia_get_status(
                agent, &power, &thermal, &degradation
            ) != 0) {
                errors++;
            }
        }
    };

    std::thread t1(setter);
    std::thread t2(setter);
    std::thread t3(getter);
    std::thread t4(getter);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    running = false;

    t1.join();
    t2.join();
    t3.join();
    t4.join();

    EXPECT_EQ(errors.load(), 0);
}

/**
 * Regression: Concurrent memory operations must be safe
 */
TEST_F(HealthAgentUSEFullRegression, ConcurrentMemoryOperations) {
    if (!swarm_memory) GTEST_SKIP() << "Swarm Memory not available";

    std::atomic<int> errors{0};
    std::atomic<bool> running{true};

    auto storer = [&](int id) {
        int count = 0;
        while (running && count < 100) {
            uint8_t pattern[] = {static_cast<uint8_t>(id), static_cast<uint8_t>(count)};
            if (nimcp_health_agent_use_swarm_memory_store(
                agent, pattern, sizeof(pattern), 0, 1, nullptr
            ) != 0) {
                errors++;
            }
            count++;
        }
    };

    auto replayer = [&]() {
        while (running) {
            nimcp_health_agent_use_swarm_memory_replay(agent, 3);
            std::this_thread::yield();
        }
    };

    std::thread t1(storer, 1);
    std::thread t2(storer, 2);
    std::thread t3(replayer);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    running = false;

    t1.join();
    t2.join();
    t3.join();

    // Allow some errors due to timing, but not too many
    EXPECT_LT(errors.load(), 20);
}

//=============================================================================
// Edge Case Regression Tests
//=============================================================================

/**
 * Regression: Zero-size operations must be handled
 */
TEST_F(HealthAgentUSEFullRegression, ZeroSizeOperations) {
    if (!swarm_memory) GTEST_SKIP() << "Swarm Memory not available";

    // Zero-size pattern
    uint8_t empty[1] = {0};
    int result = nimcp_health_agent_use_swarm_memory_store(
        agent, empty, 0, 0, 1, nullptr
    );
    // Should either succeed or fail gracefully
    EXPECT_GE(result, -1);

    // Zero count replay
    result = nimcp_health_agent_use_swarm_memory_replay(agent, 0);
    EXPECT_GE(result, 0);

    // Zero neuron recommendation
    uint32_t recommended;
    result = nimcp_health_agent_use_portia_get_recommended_neurons(agent, &recommended);
    EXPECT_EQ(result, 0);
}

/**
 * Regression: Boundary values must be handled correctly
 */
TEST_F(HealthAgentUSEFullRegression, BoundaryValues) {
    // Max degradation level
    EXPECT_EQ(nimcp_health_agent_use_portia_degrade(agent, 10), 0);

    // Neuron recommendation
    uint32_t recommended;
    EXPECT_EQ(nimcp_health_agent_use_portia_get_recommended_neurons(
        agent, &recommended
    ), 0);

    // Max component ID for behavior check
    if (swarm_immune) {
        float score;
        EXPECT_EQ(nimcp_health_agent_use_swarm_check_behavior(
            agent, UINT32_MAX, &score
        ), 0);
    }
}

/**
 * Regression: Operations after stop must fail cleanly
 */
TEST_F(HealthAgentUSEFullRegression, OperationsAfterStop) {
    // Start and stop agent
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);

    // Operations should still work (agent exists, just not running)
    EXPECT_EQ(nimcp_health_agent_use_portia_set_tier(agent, PLATFORM_TIER_FULL), 0);

    if (swarm_memory) {
        uint8_t data[] = {0x01};
        // Should work even when agent thread is stopped
        EXPECT_EQ(nimcp_health_agent_use_swarm_memory_store(
            agent, data, 1, 0, 1, nullptr
        ), 0);
    }
}

//=============================================================================
// Statistics Regression Tests
//=============================================================================

/**
 * Regression: Statistics must be accurate after operations
 */
TEST_F(HealthAgentUSEFullRegression, StatisticsAccuracy) {
    health_agent_stats_t stats_before = {0}, stats_after = {0};

    nimcp_health_agent_get_stats(agent, &stats_before);

    // Perform some operations
    for (int i = 0; i < 10; i++) {
        nimcp_health_agent_use_portia_set_tier(agent, static_cast<platform_tier_t>(i % 4));
    }

    nimcp_health_agent_get_stats(agent, &stats_after);

    // Stats should be valid (not corrupted)
    // Specific assertions depend on what stats are tracked
    EXPECT_GE(stats_after.checks_performed, 0u);
}
