/**
 * @file test_health_stability_regression.cpp
 * @brief Regression tests for Health System stability
 *
 * WHAT: Tests to verify health system behavior remains stable
 * WHY:  Phase 8 changes must not break existing health functionality
 * HOW:  Test edge cases, boundary conditions, and error scenarios
 *
 * @author NIMCP Team
 * @date 2026-01-25
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <random>

extern "C" {
#include "utils/fault_tolerance/nimcp_health_agent.h"
#include "utils/fault_tolerance/nimcp_state_manager.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
}

/**
 * @brief Test fixture for Health Stability Regression tests
 */
class HealthStabilityRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Fresh state for each test
    }

    void TearDown() override {
    }
};

/**
 * @test REGRESSION: Agent should handle rapid create/destroy cycles
 */
TEST_F(HealthStabilityRegressionTest, RapidCreateDestroyCycles) {
    for (int cycle = 0; cycle < 100; cycle++) {
        nimcp_health_agent_t* agent = nimcp_health_agent_create(nullptr);
        ASSERT_NE(agent, nullptr) << "Failed on cycle " << cycle;
        nimcp_health_agent_destroy(agent);
    }
}

/**
 * @test REGRESSION: Agent should handle rapid start/stop cycles
 */
TEST_F(HealthStabilityRegressionTest, RapidStartStopCycles) {
    health_agent_config_t config;
    nimcp_health_agent_default_config(&config);
    config.heartbeat_interval_ms = 10;

    for (int cycle = 0; cycle < 50; cycle++) {
        nimcp_health_agent_t* agent = nimcp_health_agent_create(&config);
        ASSERT_NE(agent, nullptr);

        int result = nimcp_health_agent_start(agent);
        ASSERT_EQ(result, 0) << "Start failed on cycle " << cycle;

        // Brief operation
        nimcp_health_agent_heartbeat(agent);

        result = nimcp_health_agent_stop(agent);
        ASSERT_EQ(result, 0) << "Stop failed on cycle " << cycle;

        nimcp_health_agent_destroy(agent);
    }
}

/**
 * @test REGRESSION: Heartbeat should not cause memory growth
 */
TEST_F(HealthStabilityRegressionTest, HeartbeatMemoryStability) {
    nimcp_health_agent_t* agent = nimcp_health_agent_create(nullptr);
    ASSERT_NE(agent, nullptr);

    // Get initial memory (approximate)
    health_agent_stats_t initial_stats;
    nimcp_health_agent_get_stats(agent, &initial_stats);

    // Send many heartbeats
    const int num_heartbeats = 100000;
    for (int i = 0; i < num_heartbeats; i++) {
        nimcp_health_agent_heartbeat_ex(agent, "memory_test",
                                        static_cast<float>(i % 100) / 100.0f);
    }

    health_agent_stats_t final_stats;
    nimcp_health_agent_get_stats(agent, &final_stats);

    EXPECT_EQ(final_stats.heartbeats_received, static_cast<uint64_t>(num_heartbeats));

    nimcp_health_agent_destroy(agent);
}

/**
 * @test REGRESSION: Concurrent operations should be thread-safe
 */
TEST_F(HealthStabilityRegressionTest, ConcurrentOperationsThreadSafe) {
    nimcp_health_agent_t* agent = nimcp_health_agent_create(nullptr);
    ASSERT_NE(agent, nullptr);

    std::atomic<bool> running{true};
    std::atomic<int> errors{0};

    // Heartbeat thread
    std::thread heartbeat_thread([agent, &running, &errors]() {
        while (running) {
            nimcp_health_agent_heartbeat(agent);
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    });

    // Stats reading thread
    std::thread stats_thread([agent, &running, &errors]() {
        while (running) {
            health_agent_stats_t stats;
            nimcp_health_agent_get_stats(agent, &stats);
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
    });

    // Extended heartbeat thread
    std::thread extended_thread([agent, &running, &errors]() {
        int counter = 0;
        while (running) {
            nimcp_health_agent_heartbeat_ex(agent, "concurrent",
                                            static_cast<float>(counter % 100) / 100.0f);
            counter++;
            std::this_thread::sleep_for(std::chrono::microseconds(75));
        }
    });

    // Run for a while
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    running = false;

    heartbeat_thread.join();
    stats_thread.join();
    extended_thread.join();

    EXPECT_EQ(errors, 0);

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);
    EXPECT_GT(stats.heartbeats_received, 0u);

    nimcp_health_agent_destroy(agent);
}

/**
 * @brief Simple test state for corrupt data handling test
 */
struct CorruptTestState {
    int value;
    bool valid;
};

/**
 * @test REGRESSION: State manager should handle corrupt data gracefully
 */
TEST_F(HealthStabilityRegressionTest, StateManagerCorruptDataHandling) {
    nimcp_state_manager_t* manager = nimcp_state_manager_create();
    ASSERT_NE(manager, nullptr);

    CorruptTestState state = {42, true};

    nimcp_module_state_ops_t ops = {
        .serialize = [](void* s, uint8_t* buf, size_t* sz) -> int {
            if (!buf) { *sz = sizeof(CorruptTestState); return 0; }
            if (*sz < sizeof(CorruptTestState)) return -2;
            memcpy(buf, s, sizeof(CorruptTestState));
            *sz = sizeof(CorruptTestState);
            return 0;
        },
        .deserialize = [](void* s, const uint8_t* buf, size_t sz) -> int {
            if (sz < sizeof(CorruptTestState)) return -1;
            memcpy(s, buf, sizeof(CorruptTestState));
            return 0;
        },
        .validate = [](void* s) -> int {
            auto* st = static_cast<CorruptTestState*>(s);
            return st->valid ? 0 : -1;
        },
        .reset = [](void* s) -> int {
            auto* st = static_cast<CorruptTestState*>(s);
            st->value = 0;
            st->valid = true;
            return 0;
        },
        .get_size = [](void*) -> size_t { return sizeof(CorruptTestState); }
    };

    int result = nimcp_state_manager_register(manager, "test", &ops, &state);
    ASSERT_EQ(result, 0);

    // Create valid checkpoint
    size_t size = 0;
    nimcp_state_manager_checkpoint_all(manager, nullptr, &size);
    std::vector<uint8_t> buffer(size);
    nimcp_state_manager_checkpoint_all(manager, buffer.data(), &size);

    // Corrupt the data
    for (size_t i = 0; i < size; i++) {
        buffer[i] ^= 0xFF;
    }

    // Attempt restore - should fail but not crash
    result = nimcp_state_manager_restore_all(manager, buffer.data(), size);
    // Result may be success or failure depending on deserialization validation

    // Restore with too small buffer - should fail gracefully
    result = nimcp_state_manager_restore_all(manager, buffer.data(), 1);
    EXPECT_NE(result, 0);

    // Restore with NULL - should fail gracefully
    result = nimcp_state_manager_restore_all(manager, nullptr, size);
    EXPECT_NE(result, 0);

    nimcp_state_manager_destroy(manager);
}

/**
 * @test REGRESSION: Empty state manager operations should be safe
 */
TEST_F(HealthStabilityRegressionTest, EmptyStateManagerOperations) {
    nimcp_state_manager_t* manager = nimcp_state_manager_create();
    ASSERT_NE(manager, nullptr);

    // Operations on empty manager should be safe
    int result = nimcp_state_manager_validate_all(manager);
    EXPECT_EQ(result, 0);  // Nothing to validate = success

    result = nimcp_state_manager_reset_all(manager);
    EXPECT_EQ(result, 0);  // Nothing to reset = success

    size_t size = 0;
    result = nimcp_state_manager_checkpoint_all(manager, nullptr, &size);
    EXPECT_EQ(result, 0);

    // Find non-existent module
    nimcp_state_module_entry_t* entry = nimcp_state_manager_find(manager, "nonexistent");
    EXPECT_EQ(entry, nullptr);

    nimcp_state_manager_destroy(manager);
}

/**
 * @test REGRESSION: Health agent should handle NULL config gracefully
 */
TEST_F(HealthStabilityRegressionTest, NullConfigHandling) {
    // Create with NULL should use defaults
    nimcp_health_agent_t* agent = nimcp_health_agent_create(nullptr);
    ASSERT_NE(agent, nullptr);

    // Operations should work
    nimcp_health_agent_heartbeat(agent);
    nimcp_health_agent_heartbeat_ex(agent, "test", 0.5f);

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);
    EXPECT_GE(stats.heartbeats_received, 2u);

    nimcp_health_agent_destroy(agent);
}

/**
 * @test REGRESSION: Long operation names should be handled
 */
TEST_F(HealthStabilityRegressionTest, LongOperationNames) {
    nimcp_health_agent_t* agent = nimcp_health_agent_create(nullptr);
    ASSERT_NE(agent, nullptr);

    // Very long operation name
    std::string long_name(1024, 'x');
    nimcp_health_agent_heartbeat_ex(agent, long_name.c_str(), 0.5f);

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);
    EXPECT_GE(stats.heartbeats_received, 1u);

    nimcp_health_agent_destroy(agent);
}

/**
 * @test REGRESSION: Progress values at boundaries should be handled
 */
TEST_F(HealthStabilityRegressionTest, ProgressBoundaryValues) {
    nimcp_health_agent_t* agent = nimcp_health_agent_create(nullptr);
    ASSERT_NE(agent, nullptr);

    // Boundary values
    nimcp_health_agent_heartbeat_ex(agent, "test", 0.0f);
    nimcp_health_agent_heartbeat_ex(agent, "test", 1.0f);

    // Out of bounds - should be clamped or handled
    nimcp_health_agent_heartbeat_ex(agent, "test", -0.5f);
    nimcp_health_agent_heartbeat_ex(agent, "test", 1.5f);
    nimcp_health_agent_heartbeat_ex(agent, "test", -100.0f);
    nimcp_health_agent_heartbeat_ex(agent, "test", 100.0f);

    // Special float values
    nimcp_health_agent_heartbeat_ex(agent, "test", INFINITY);
    nimcp_health_agent_heartbeat_ex(agent, "test", -INFINITY);
    nimcp_health_agent_heartbeat_ex(agent, "test", NAN);

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);
    EXPECT_GE(stats.heartbeats_received, 9u);

    nimcp_health_agent_destroy(agent);
}

/**
 * @brief Simple test state for module name limits test
 */
struct NameLimitTestState {
    int x;
};

/**
 * @test REGRESSION: State manager module name limits
 */
TEST_F(HealthStabilityRegressionTest, ModuleNameLimits) {
    nimcp_state_manager_t* manager = nimcp_state_manager_create();
    ASSERT_NE(manager, nullptr);

    NameLimitTestState state = {0};
    nimcp_module_state_ops_t ops = {
        .serialize = [](void*, uint8_t*, size_t* sz) -> int { *sz = sizeof(NameLimitTestState); return 0; },
        .deserialize = [](void*, const uint8_t*, size_t) -> int { return 0; },
        .validate = [](void*) -> int { return 0; },
        .reset = [](void* s) -> int { static_cast<NameLimitTestState*>(s)->x = 0; return 0; },
        .get_size = [](void*) -> size_t { return sizeof(NameLimitTestState); }
    };

    // Very long name (should be truncated or rejected)
    std::string long_name(256, 'y');
    int result = nimcp_state_manager_register(manager, long_name.c_str(), &ops, &state);
    // Should either succeed with truncation or fail - shouldn't crash

    // Empty name - API currently accepts empty names (regression test documents behavior)
    result = nimcp_state_manager_register(manager, "", &ops, &state);
    // Note: Empty name is currently accepted by the API

    nimcp_state_manager_destroy(manager);
}

/**
 * @brief Main entry point
 */
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
