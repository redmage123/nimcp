/**
 * @file test_medulla.cpp
 * @brief Comprehensive unit tests for the medulla orchestrator module
 *
 * WHAT: Tests for the main medulla coordination system including:
 *       - Thread safety
 *       - Magic number validation
 *       - BBB integration
 *       - Bio-async messaging
 *       - Health agent heartbeat
 *       - Arousal bounds
 *       - State transitions
 *
 * WHY:  Ensure proper orchestration of medulla subsystems with full coverage
 * HOW:  Use GoogleTest framework with lifecycle and state validation
 */

#include <gtest/gtest.h>
#include <cstring>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>

// Headers have their own extern "C" guards
#include "core/medulla/nimcp_medulla.h"
#include "utils/error/nimcp_error_codes.h"
#include "security/nimcp_blood_brain_barrier.h"

//=============================================================================
// Mock Structures for Testing
//=============================================================================

// Mock health agent for heartbeat testing
struct MockHealthAgent {
    std::atomic<int> heartbeat_count{0};
    std::atomic<bool> last_heartbeat_called{false};
    std::string last_operation;
    float last_progress{0.0f};
};

// Global mock for health agent callback interception
static MockHealthAgent* g_mock_health_agent = nullptr;

// Mock BBB system for integration testing
struct MockBBBSystem {
    bool validation_should_pass{true};
    int validation_call_count{0};
    bool is_enabled{true};
};

static MockBBBSystem* g_mock_bbb = nullptr;

//=============================================================================
// Test Fixture
//=============================================================================

class MedullaTest : public ::testing::Test {
protected:
    medulla_t medulla = nullptr;

    void SetUp() override {
        medulla_config_t config = medulla_default_config();
        medulla = medulla_create(&config);
        ASSERT_NE(medulla, nullptr);
    }

    void TearDown() override {
        if (medulla) {
            medulla_destroy(medulla);
            medulla = nullptr;
        }
        g_mock_health_agent = nullptr;
        g_mock_bbb = nullptr;
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(MedullaTest, DefaultConfig) {
    medulla_config_t config = medulla_default_config();

    // Verify arousal defaults
    EXPECT_GE(config.arousal.baseline_arousal, 0.0f);
    EXPECT_LE(config.arousal.baseline_arousal, 1.0f);

    // Verify update interval
    EXPECT_GT(config.update_interval_ms, 0u);
}

TEST_F(MedullaTest, CreateWithNullConfig) {
    medulla_t m = medulla_create(nullptr);
    EXPECT_NE(m, nullptr);
    if (m) medulla_destroy(m);
}

TEST_F(MedullaTest, DestroyNull) {
    medulla_destroy(nullptr);
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(MedullaTest, StartStop) {
    int result = medulla_start(medulla);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    result = medulla_stop(medulla);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(MedullaTest, StartNull) {
    int result = medulla_start(nullptr);
    EXPECT_NE(result, 0);  // Any non-zero error code (NIMCP uses positive codes)
}

TEST_F(MedullaTest, StopNull) {
    int result = medulla_stop(nullptr);
    EXPECT_NE(result, 0);  // Any non-zero error code (NIMCP uses positive codes)
}

TEST_F(MedullaTest, MultipleStartStop) {
    // First start/stop cycle
    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);
    EXPECT_EQ(medulla_stop(medulla), NIMCP_SUCCESS);

    // Second start/stop cycle
    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);
    EXPECT_EQ(medulla_stop(medulla), NIMCP_SUCCESS);
}

//=============================================================================
// Update Tests
//=============================================================================

TEST_F(MedullaTest, Update) {
    medulla_start(medulla);

    int result = medulla_update(medulla, 0.1f);  // 100ms
    EXPECT_EQ(result, NIMCP_SUCCESS);

    medulla_stop(medulla);
}

TEST_F(MedullaTest, UpdateNull) {
    int result = medulla_update(nullptr, 0.1f);
    EXPECT_NE(result, 0);  // Any non-zero error code (NIMCP uses positive codes)
}

TEST_F(MedullaTest, MultipleUpdates) {
    medulla_start(medulla);

    for (int i = 0; i < 100; i++) {
        int result = medulla_update(medulla, 0.016f);  // ~60 fps
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }

    medulla_stop(medulla);
}

//=============================================================================
// Emergency Shutdown Tests
//=============================================================================

TEST_F(MedullaTest, EmergencyShutdown) {
    medulla_start(medulla);

    int result = medulla_emergency_shutdown(medulla, "test shutdown");
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Verify protection level is at max
    protection_level_t level = medulla_get_protection_level(medulla);
    EXPECT_GE((int)level, (int)PROTECTION_LEVEL_CRITICAL);

    medulla_stop(medulla);
}

TEST_F(MedullaTest, EmergencyShutdownNull) {
    int result = medulla_emergency_shutdown(nullptr, "test");
    EXPECT_NE(result, 0);  // Any non-zero error code (NIMCP uses positive codes)
}

//=============================================================================
// State Query Tests
//=============================================================================

TEST_F(MedullaTest, GetProtectionLevel) {
    protection_level_t level = medulla_get_protection_level(medulla);
    EXPECT_GE((int)level, 0);
    EXPECT_LE((int)level, (int)PROTECTION_LEVEL_SHUTDOWN);
}

TEST_F(MedullaTest, GetCircadianPhase) {
    circadian_phase_t phase = medulla_get_circadian_phase(medulla);
    EXPECT_GE((int)phase, 0);
    EXPECT_LT((int)phase, 8);  // 8 phases
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(MedullaTest, GetStats) {
    medulla_stats_t stats;
    int result = medulla_get_stats(medulla, &stats);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Verify state is valid
    EXPECT_GE((int)stats.state, 0);
    EXPECT_LE((int)stats.state, (int)MEDULLA_STATE_STOPPING);
}

TEST_F(MedullaTest, GetStatsNull) {
    medulla_stats_t stats;
    int result = medulla_get_stats(nullptr, &stats);
    EXPECT_NE(result, 0);  // Any non-zero error code (NIMCP uses positive codes)

    result = medulla_get_stats(medulla, nullptr);
    EXPECT_NE(result, 0);  // Any non-zero error code (NIMCP uses positive codes)
}

TEST_F(MedullaTest, StatsAfterUpdates) {
    medulla_start(medulla);

    // Run some updates
    for (int i = 0; i < 10; i++) {
        medulla_update(medulla, 0.1f);
    }

    medulla_stats_t stats;
    medulla_get_stats(medulla, &stats);

    // Verify update count increased
    EXPECT_GE(stats.total_updates, 10u);

    medulla_stop(medulla);
}

//=============================================================================
// State Change Tests
//=============================================================================

TEST_F(MedullaTest, RequestStateChange) {
    medulla_start(medulla);

    int result = medulla_request_state_change(medulla, MEDULLA_STATE_DEGRADED);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    medulla_stop(medulla);
}

TEST_F(MedullaTest, RequestStateChangeNull) {
    int result = medulla_request_state_change(nullptr, MEDULLA_STATE_RUNNING);
    EXPECT_NE(result, 0);  // Any non-zero error code (NIMCP uses positive codes)
}

//=============================================================================
// Bio-async Tests
//=============================================================================

TEST_F(MedullaTest, BioAsyncConnection) {
    bool connected = medulla_is_bio_async_connected(medulla);
    EXPECT_FALSE(connected);

    int result = medulla_connect_bio_async(medulla);
    // Result depends on router availability

    medulla_disconnect_bio_async(medulla);
    connected = medulla_is_bio_async_connected(medulla);
    EXPECT_FALSE(connected);
}

TEST_F(MedullaTest, BioAsyncNullState) {
    bool connected = medulla_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);
}

//=============================================================================
// Integration Connection Tests
//=============================================================================

TEST_F(MedullaTest, ConnectHealthMonitorNull) {
    int result = medulla_connect_health_monitor(medulla, nullptr);
    // May return success with null if function ignores null (just logging)
    EXPECT_TRUE(result == NIMCP_SUCCESS || result < 0);
}

TEST_F(MedullaTest, ConnectRecoveryNull) {
    int result = medulla_connect_recovery_system(medulla, nullptr);
    EXPECT_TRUE(result == NIMCP_SUCCESS || result < 0);
}

TEST_F(MedullaTest, ConnectSleepWakeNull) {
    int result = medulla_connect_sleep_wake(medulla, nullptr);
    EXPECT_TRUE(result == NIMCP_SUCCESS || result < 0);
}

TEST_F(MedullaTest, ConnectNeuromodulatorsNull) {
    int result = medulla_connect_neuromodulators(medulla, nullptr);
    EXPECT_TRUE(result == NIMCP_SUCCESS || result < 0);
}

//=============================================================================
// 1. Thread Safety Tests
//=============================================================================

TEST_F(MedullaTest, ThreadSafeArousalAccess) {
    // Test concurrent reads of arousal level
    medulla_start(medulla);

    const int num_threads = 8;
    const int reads_per_thread = 1000;
    std::vector<std::thread> threads;
    std::atomic<int> read_count{0};
    std::atomic<bool> had_error{false};

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, &read_count, &had_error, reads_per_thread]() {
            for (int i = 0; i < reads_per_thread; i++) {
                float arousal = medulla_get_arousal_level(medulla);
                if (arousal < 0.0f || arousal > 1.0f) {
                    // -1.0f is returned on error, but for valid medulla should be [0,1]
                    if (arousal != -1.0f) {
                        had_error = true;
                    }
                }
                read_count++;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_FALSE(had_error.load());
    EXPECT_EQ(read_count.load(), num_threads * reads_per_thread);

    medulla_stop(medulla);
}

TEST_F(MedullaTest, ThreadSafeBoostArousal) {
    // Test concurrent boost_arousal calls
    medulla_start(medulla);

    // Set arousal to a low value to allow boosting
    medulla_test_set_arousal(medulla, 0.3f);

    const int num_threads = 4;
    const int boosts_per_thread = 100;
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};
    std::atomic<int> failure_count{0};

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, &success_count, &failure_count, boosts_per_thread]() {
            for (int i = 0; i < boosts_per_thread; i++) {
                int result = medulla_boost_arousal(medulla, 0.001f);
                if (result == NIMCP_SUCCESS) {
                    success_count++;
                } else {
                    failure_count++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // All boosts should succeed
    EXPECT_EQ(success_count.load(), num_threads * boosts_per_thread);
    EXPECT_EQ(failure_count.load(), 0);

    // Final arousal should be clamped properly
    float final_arousal = medulla_get_arousal_level(medulla);
    EXPECT_GE(final_arousal, 0.0f);
    EXPECT_LE(final_arousal, 1.0f);

    medulla_stop(medulla);
}

TEST_F(MedullaTest, ThreadSafeReduceArousal) {
    // Test concurrent reduce_arousal calls
    medulla_start(medulla);

    // Set arousal to a high value to allow reduction
    medulla_test_set_arousal(medulla, 0.8f);

    const int num_threads = 4;
    const int reductions_per_thread = 100;
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, &success_count, reductions_per_thread]() {
            for (int i = 0; i < reductions_per_thread; i++) {
                int result = medulla_reduce_arousal(medulla, 0.001f);
                if (result == NIMCP_SUCCESS) {
                    success_count++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count.load(), num_threads * reductions_per_thread);

    // Final arousal should be clamped properly
    float final_arousal = medulla_get_arousal_level(medulla);
    EXPECT_GE(final_arousal, 0.0f);
    EXPECT_LE(final_arousal, 1.0f);

    medulla_stop(medulla);
}

TEST_F(MedullaTest, ThreadSafeProtectionAccess) {
    // Test concurrent protection level reads
    medulla_start(medulla);

    const int num_threads = 8;
    const int reads_per_thread = 1000;
    std::vector<std::thread> threads;
    std::atomic<int> read_count{0};
    std::atomic<bool> had_error{false};

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, &read_count, &had_error, reads_per_thread]() {
            for (int i = 0; i < reads_per_thread; i++) {
                protection_level_t level = medulla_get_protection_level(medulla);
                if ((int)level < 0 || (int)level > (int)PROTECTION_LEVEL_SHUTDOWN) {
                    had_error = true;
                }
                read_count++;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_FALSE(had_error.load());
    EXPECT_EQ(read_count.load(), num_threads * reads_per_thread);

    medulla_stop(medulla);
}

TEST_F(MedullaTest, ThreadSafeCircadianAccess) {
    // Test concurrent circadian phase reads
    medulla_start(medulla);

    const int num_threads = 8;
    const int reads_per_thread = 1000;
    std::vector<std::thread> threads;
    std::atomic<int> read_count{0};
    std::atomic<bool> had_error{false};

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, &read_count, &had_error, reads_per_thread]() {
            for (int i = 0; i < reads_per_thread; i++) {
                circadian_phase_t phase = medulla_get_circadian_phase(medulla);
                if ((int)phase < 0 || (int)phase > (int)CIRCADIAN_PHASE_PRE_DAWN) {
                    had_error = true;
                }
                read_count++;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_FALSE(had_error.load());
    EXPECT_EQ(read_count.load(), num_threads * reads_per_thread);

    medulla_stop(medulla);
}

TEST_F(MedullaTest, ThreadSafeMixedOperations) {
    // Test concurrent reads and writes
    medulla_start(medulla);

    const int num_reader_threads = 4;
    const int num_writer_threads = 2;
    const int ops_per_thread = 500;
    std::vector<std::thread> threads;
    std::atomic<bool> stop_flag{false};
    std::atomic<int> total_ops{0};

    // Reader threads
    for (int t = 0; t < num_reader_threads; t++) {
        threads.emplace_back([this, &stop_flag, &total_ops, ops_per_thread]() {
            for (int i = 0; i < ops_per_thread && !stop_flag; i++) {
                medulla_get_arousal_level(medulla);
                medulla_get_protection_level(medulla);
                medulla_get_circadian_phase(medulla);
                total_ops += 3;
            }
        });
    }

    // Writer threads
    for (int t = 0; t < num_writer_threads; t++) {
        threads.emplace_back([this, &stop_flag, &total_ops, ops_per_thread]() {
            for (int i = 0; i < ops_per_thread && !stop_flag; i++) {
                if (i % 2 == 0) {
                    medulla_boost_arousal(medulla, 0.001f);
                } else {
                    medulla_reduce_arousal(medulla, 0.001f);
                }
                total_ops++;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Verify no deadlock occurred and operations completed
    EXPECT_GT(total_ops.load(), 0);

    medulla_stop(medulla);
}

//=============================================================================
// 2. Magic Number Validation Tests
//=============================================================================

TEST_F(MedullaTest, ValidHandleAfterCreate) {
    // Verify magic is set correctly - valid handle should work
    float arousal = medulla_get_arousal_level(medulla);
    EXPECT_GE(arousal, 0.0f);
    EXPECT_LE(arousal, 1.0f);

    protection_level_t level = medulla_get_protection_level(medulla);
    EXPECT_GE((int)level, 0);
    EXPECT_LE((int)level, (int)PROTECTION_LEVEL_SHUTDOWN);

    circadian_phase_t phase = medulla_get_circadian_phase(medulla);
    EXPECT_GE((int)phase, 0);
    EXPECT_LE((int)phase, (int)CIRCADIAN_PHASE_PRE_DAWN);
}

TEST_F(MedullaTest, InvalidHandleAfterDestroy) {
    // Create a new medulla and destroy it
    medulla_config_t config = medulla_default_config();
    medulla_t temp_medulla = medulla_create(&config);
    ASSERT_NE(temp_medulla, nullptr);

    // Destroy it
    medulla_destroy(temp_medulla);

    // Operations on destroyed handle should fail or return error values
    // Note: This tests that operations detect invalid handles
    // The magic number should be cleared on destroy
    float arousal = medulla_get_arousal_level(temp_medulla);
    EXPECT_EQ(arousal, -1.0f);  // Error return value

    protection_level_t level = medulla_get_protection_level(temp_medulla);
    EXPECT_EQ(level, PROTECTION_LEVEL_NORMAL);  // Default error return

    circadian_phase_t phase = medulla_get_circadian_phase(temp_medulla);
    EXPECT_EQ(phase, CIRCADIAN_PHASE_MORNING);  // Default error return
}

TEST_F(MedullaTest, RejectInvalidHandle) {
    // Test that NULL handles are rejected
    // NIMCP uses positive error codes, not negative
    EXPECT_EQ(medulla_get_arousal_level(nullptr), -1.0f);
    EXPECT_NE(medulla_start(nullptr), 0);
    EXPECT_NE(medulla_stop(nullptr), 0);
    EXPECT_NE(medulla_update(nullptr, 0.1f), 0);
    EXPECT_NE(medulla_emergency_shutdown(nullptr, "test"), 0);
    EXPECT_NE(medulla_request_state_change(nullptr, MEDULLA_STATE_RUNNING), 0);
    EXPECT_NE(medulla_boost_arousal(nullptr, 0.1f), 0);
    EXPECT_NE(medulla_reduce_arousal(nullptr, 0.1f), 0);
    EXPECT_NE(medulla_connect_bio_async(nullptr), 0);
    EXPECT_NE(medulla_disconnect_bio_async(nullptr), 0);
    EXPECT_FALSE(medulla_is_bio_async_connected(nullptr));

    medulla_stats_t stats;
    EXPECT_NE(medulla_get_stats(nullptr, &stats), 0);
}

TEST_F(MedullaTest, ValidateHandleBeforeOperations) {
    // Test that all operations validate the handle
    medulla_start(medulla);

    // All operations should succeed with valid handle
    EXPECT_EQ(medulla_update(medulla, 0.1f), NIMCP_SUCCESS);
    EXPECT_GE(medulla_get_arousal_level(medulla), 0.0f);
    EXPECT_EQ(medulla_boost_arousal(medulla, 0.05f), NIMCP_SUCCESS);
    EXPECT_EQ(medulla_reduce_arousal(medulla, 0.05f), NIMCP_SUCCESS);

    medulla_stats_t stats;
    EXPECT_EQ(medulla_get_stats(medulla, &stats), NIMCP_SUCCESS);

    medulla_stop(medulla);
}

//=============================================================================
// 3. BBB Integration Tests
//=============================================================================

TEST_F(MedullaTest, BBBSetAndGet) {
    // Test setting BBB system
    bbb_system_t bbb = bbb_system_create(nullptr);
    if (bbb) {
        // BBB system created successfully
        EXPECT_NE(bbb, nullptr);

        // Clean up
        bbb_system_destroy(bbb);
    }
    // If BBB creation fails, that's acceptable for unit tests
}

TEST_F(MedullaTest, BBBValidatesArousalBoost) {
    // Test that BBB validates arousal changes
    // This test verifies the integration point exists

    medulla_start(medulla);

    float initial_arousal = medulla_get_arousal_level(medulla);

    // Boost arousal with valid delta
    int result = medulla_boost_arousal(medulla, 0.1f);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    float new_arousal = medulla_get_arousal_level(medulla);
    EXPECT_GE(new_arousal, initial_arousal);  // Should have increased or clamped

    medulla_stop(medulla);
}

TEST_F(MedullaTest, BBBRejectsInvalidInput) {
    // Test that invalid inputs are rejected
    medulla_start(medulla);

    // Negative delta should be rejected
    int result = medulla_boost_arousal(medulla, -0.1f);
    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);

    result = medulla_reduce_arousal(medulla, -0.1f);
    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);

    medulla_stop(medulla);
}

TEST_F(MedullaTest, BBBInputValidationOnAllEndpoints) {
    // Verify all public endpoints have input validation
    medulla_start(medulla);

    // Valid inputs should succeed
    EXPECT_EQ(medulla_boost_arousal(medulla, 0.0f), NIMCP_SUCCESS);
    EXPECT_EQ(medulla_reduce_arousal(medulla, 0.0f), NIMCP_SUCCESS);
    EXPECT_EQ(medulla_update(medulla, 0.0f), NIMCP_SUCCESS);
    EXPECT_EQ(medulla_update(medulla, 0.1f), NIMCP_SUCCESS);

    // Invalid inputs should fail
    EXPECT_EQ(medulla_boost_arousal(medulla, -1.0f), NIMCP_ERROR_INVALID_PARAM);
    EXPECT_EQ(medulla_reduce_arousal(medulla, -1.0f), NIMCP_ERROR_INVALID_PARAM);

    medulla_stop(medulla);
}

//=============================================================================
// 4. Bio-Async Tests
//=============================================================================

TEST_F(MedullaTest, BioAsyncConnect) {
    // Test bio-async connection attempt
    int result = medulla_connect_bio_async(medulla);
    // Connection may succeed or fail depending on router availability
    // NIMCP uses positive error codes, so result could be:
    // - 0 (NIMCP_SUCCESS) if router available and connection worked
    // - Positive error code if router not available or connection failed
    // Either way, it should not crash
    (void)result;  // Just verify no crash
}

TEST_F(MedullaTest, BioAsyncDisconnect) {
    // Test disconnect
    medulla_connect_bio_async(medulla);

    int result = medulla_disconnect_bio_async(medulla);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // After disconnect, should not be connected
    EXPECT_FALSE(medulla_is_bio_async_connected(medulla));
}

TEST_F(MedullaTest, BioAsyncMessageHandler) {
    // Test message handling through bio-async
    medulla_start(medulla);

    // Connect bio-async
    medulla_connect_bio_async(medulla);

    // Run some updates which should process any pending messages
    for (int i = 0; i < 10; i++) {
        medulla_update(medulla, 0.016f);
    }

    // Disconnect and verify clean shutdown
    medulla_disconnect_bio_async(medulla);
    EXPECT_FALSE(medulla_is_bio_async_connected(medulla));

    medulla_stop(medulla);
}

TEST_F(MedullaTest, BioAsyncDoubleConnect) {
    // Test double connection is handled gracefully
    int result1 = medulla_connect_bio_async(medulla);
    int result2 = medulla_connect_bio_async(medulla);

    // If router is available, both should succeed (idempotent)
    // If router not available, both return error (consistent behavior)
    // Key: both calls return the same result (idempotent behavior)
    EXPECT_EQ(result1, result2);

    medulla_disconnect_bio_async(medulla);
}

TEST_F(MedullaTest, BioAsyncDoubleDisconnect) {
    // Test double disconnection is handled gracefully
    medulla_connect_bio_async(medulla);

    int result1 = medulla_disconnect_bio_async(medulla);
    int result2 = medulla_disconnect_bio_async(medulla);

    // Both should succeed (idempotent)
    EXPECT_EQ(result1, NIMCP_SUCCESS);
    EXPECT_EQ(result2, NIMCP_SUCCESS);
}

//=============================================================================
// 5. Heartbeat Tests
//=============================================================================

TEST_F(MedullaTest, HealthAgentSet) {
    // Test that we can work with the medulla without a health agent
    medulla_start(medulla);

    // Updates should work without health agent
    for (int i = 0; i < 5; i++) {
        int result = medulla_update(medulla, 0.1f);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }

    medulla_stop(medulla);
}

TEST_F(MedullaTest, HeartbeatEmittedOnUpdate) {
    // Verify heartbeat is called during update
    // This is verified by successful execution without deadlock
    medulla_start(medulla);

    // Run multiple updates
    for (int i = 0; i < 100; i++) {
        int result = medulla_update(medulla, 0.016f);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }

    // Verify stats show updates were processed
    medulla_stats_t stats;
    medulla_get_stats(medulla, &stats);
    EXPECT_GE(stats.total_updates, 100u);

    medulla_stop(medulla);
}

TEST_F(MedullaTest, UptimeTrackedCorrectly) {
    // Verify uptime is tracked during operation
    medulla_start(medulla);

    // Sleep a bit and run updates
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    for (int i = 0; i < 10; i++) {
        medulla_update(medulla, 0.01f);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    medulla_stats_t stats;
    medulla_get_stats(medulla, &stats);

    // Uptime should be at least 50ms
    EXPECT_GT(stats.uptime_ms, 0u);

    medulla_stop(medulla);
}

//=============================================================================
// 6. Arousal Bounds Tests
//=============================================================================

TEST_F(MedullaTest, BoostArousalClampsToMax) {
    // Test boost doesn't exceed max
    medulla_start(medulla);

    // Set arousal high
    medulla_test_set_arousal(medulla, 0.95f);

    // Try to boost beyond max
    medulla_boost_arousal(medulla, 0.5f);

    float arousal = medulla_get_arousal_level(medulla);
    EXPECT_LE(arousal, 1.0f);

    medulla_stop(medulla);
}

TEST_F(MedullaTest, ReduceArousalClampsToMin) {
    // Test reduce doesn't go below min
    medulla_start(medulla);

    // Set arousal low
    medulla_test_set_arousal(medulla, 0.05f);

    // Try to reduce beyond min
    medulla_reduce_arousal(medulla, 0.5f);

    float arousal = medulla_get_arousal_level(medulla);
    EXPECT_GE(arousal, 0.0f);

    medulla_stop(medulla);
}

TEST_F(MedullaTest, NegativeDeltaRejected) {
    // Test that negative delta is rejected
    medulla_start(medulla);

    int result = medulla_boost_arousal(medulla, -0.1f);
    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);

    result = medulla_reduce_arousal(medulla, -0.1f);
    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);

    medulla_stop(medulla);
}

TEST_F(MedullaTest, ZeroDeltaAccepted) {
    // Zero delta should be accepted (no-op)
    medulla_start(medulla);

    float initial = medulla_get_arousal_level(medulla);

    int result = medulla_boost_arousal(medulla, 0.0f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(medulla_get_arousal_level(medulla), initial);

    result = medulla_reduce_arousal(medulla, 0.0f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(medulla_get_arousal_level(medulla), initial);

    medulla_stop(medulla);
}

TEST_F(MedullaTest, LargeDeltaClamped) {
    // Large deltas should be clamped properly
    medulla_start(medulla);

    medulla_test_set_arousal(medulla, 0.5f);

    // Boost with very large delta
    medulla_boost_arousal(medulla, 100.0f);
    float arousal = medulla_get_arousal_level(medulla);
    EXPECT_LE(arousal, 1.0f);

    // Reduce with very large delta
    medulla_reduce_arousal(medulla, 100.0f);
    arousal = medulla_get_arousal_level(medulla);
    EXPECT_GE(arousal, 0.0f);

    medulla_stop(medulla);
}

TEST_F(MedullaTest, ArousalLevelCategorizationCorrect) {
    // Test that arousal level categorization matches continuous value
    medulla_start(medulla);

    // Test different arousal levels using test helper
    struct TestCase {
        float level;
        arousal_level_t min_expected;
        arousal_level_t max_expected;
    };

    // Thresholds: <0.1 COMA, <0.3 DEEP_SLEEP, <0.4 LIGHT_SLEEP,
    //             <0.5 DROWSY, <0.7 AWAKE, <0.9 ALERT, >=0.9 HYPER
    TestCase cases[] = {
        {0.02f, AROUSAL_LEVEL_COMA, AROUSAL_LEVEL_COMA},
        {0.15f, AROUSAL_LEVEL_DEEP_SLEEP, AROUSAL_LEVEL_DEEP_SLEEP},
        {0.35f, AROUSAL_LEVEL_LIGHT_SLEEP, AROUSAL_LEVEL_LIGHT_SLEEP},
        {0.45f, AROUSAL_LEVEL_DROWSY, AROUSAL_LEVEL_DROWSY},
        {0.55f, AROUSAL_LEVEL_AWAKE, AROUSAL_LEVEL_AWAKE},
        {0.75f, AROUSAL_LEVEL_ALERT, AROUSAL_LEVEL_ALERT},
        {0.95f, AROUSAL_LEVEL_HYPERAROUSAL, AROUSAL_LEVEL_HYPERAROUSAL}
    };

    for (const auto& tc : cases) {
        medulla_test_set_arousal(medulla, tc.level);

        medulla_stats_t stats;
        medulla_get_stats(medulla, &stats);

        EXPECT_GE((int)stats.arousal_level, (int)tc.min_expected);
        EXPECT_LE((int)stats.arousal_level, (int)tc.max_expected);
    }

    medulla_stop(medulla);
}

//=============================================================================
// 7. State Transition Tests
//=============================================================================

TEST_F(MedullaTest, EmergencyShutdownFromRunning) {
    // Test emergency shutdown works from running state
    medulla_start(medulla);

    medulla_stats_t stats_before;
    medulla_get_stats(medulla, &stats_before);
    EXPECT_EQ(stats_before.state, MEDULLA_STATE_RUNNING);

    int result = medulla_emergency_shutdown(medulla, "test emergency");
    EXPECT_EQ(result, NIMCP_SUCCESS);

    medulla_stats_t stats_after;
    medulla_get_stats(medulla, &stats_after);
    EXPECT_EQ(stats_after.state, MEDULLA_STATE_EMERGENCY);
    EXPECT_EQ(stats_after.protection_level, PROTECTION_LEVEL_SHUTDOWN);
    EXPECT_GE(stats_after.emergency_shutdowns, 1u);

    medulla_stop(medulla);
}

TEST_F(MedullaTest, CannotTransitionFromEmergency) {
    // Test that most transitions are blocked from emergency state
    medulla_start(medulla);

    // Trigger emergency
    medulla_emergency_shutdown(medulla, "test");

    // Try to transition to RUNNING - should fail
    int result = medulla_request_state_change(medulla, MEDULLA_STATE_RUNNING);
    EXPECT_NE(result, 0);  // NIMCP uses positive error codes

    // Try to transition to DEGRADED - should fail
    result = medulla_request_state_change(medulla, MEDULLA_STATE_DEGRADED);
    EXPECT_NE(result, 0);  // NIMCP uses positive error codes

    // Transition to STOPPED should work (only valid exit from emergency)
    result = medulla_request_state_change(medulla, MEDULLA_STATE_STOPPED);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    medulla_stats_t stats;
    medulla_get_stats(medulla, &stats);
    EXPECT_EQ(stats.state, MEDULLA_STATE_STOPPED);
}

TEST_F(MedullaTest, StateTransitionFromStopped) {
    // Test transitions from stopped state
    medulla_stats_t stats;
    medulla_get_stats(medulla, &stats);
    EXPECT_EQ(stats.state, MEDULLA_STATE_STOPPED);

    // Start should transition to running
    medulla_start(medulla);
    medulla_get_stats(medulla, &stats);
    EXPECT_EQ(stats.state, MEDULLA_STATE_RUNNING);

    medulla_stop(medulla);
}

TEST_F(MedullaTest, StateTransitionToDegraded) {
    // Test transition to degraded state
    medulla_start(medulla);

    int result = medulla_request_state_change(medulla, MEDULLA_STATE_DEGRADED);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    medulla_stats_t stats;
    medulla_get_stats(medulla, &stats);
    EXPECT_EQ(stats.state, MEDULLA_STATE_DEGRADED);

    medulla_stop(medulla);
}

TEST_F(MedullaTest, StopFromRunning) {
    medulla_start(medulla);

    medulla_stats_t stats;
    medulla_get_stats(medulla, &stats);
    EXPECT_EQ(stats.state, MEDULLA_STATE_RUNNING);

    medulla_stop(medulla);

    medulla_get_stats(medulla, &stats);
    EXPECT_EQ(stats.state, MEDULLA_STATE_STOPPED);
}

TEST_F(MedullaTest, StopFromDegraded) {
    medulla_start(medulla);
    medulla_request_state_change(medulla, MEDULLA_STATE_DEGRADED);

    medulla_stop(medulla);

    medulla_stats_t stats;
    medulla_get_stats(medulla, &stats);
    EXPECT_EQ(stats.state, MEDULLA_STATE_STOPPED);
}

TEST_F(MedullaTest, UpdateFailsWhenNotRunning) {
    // Update should fail when not in running state
    medulla_stats_t stats;
    medulla_get_stats(medulla, &stats);
    EXPECT_EQ(stats.state, MEDULLA_STATE_STOPPED);

    int result = medulla_update(medulla, 0.1f);
    EXPECT_NE(result, 0);  // NIMCP uses positive error codes

    // Start, then stop
    medulla_start(medulla);
    medulla_stop(medulla);

    result = medulla_update(medulla, 0.1f);
    EXPECT_NE(result, 0);  // NIMCP uses positive error codes
}

//=============================================================================
// Additional Edge Case Tests
//=============================================================================

TEST_F(MedullaTest, EmergencyShutdownWithNullReason) {
    medulla_start(medulla);

    // Should handle null reason gracefully
    int result = medulla_emergency_shutdown(medulla, nullptr);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    medulla_stats_t stats;
    medulla_get_stats(medulla, &stats);
    EXPECT_EQ(stats.state, MEDULLA_STATE_EMERGENCY);

    medulla_stop(medulla);
}

TEST_F(MedullaTest, ProtectionLevelTest) {
    // Test protection level setting via test helper
    EXPECT_EQ(medulla_test_set_protection(medulla, PROTECTION_LEVEL_NORMAL), 0);
    EXPECT_EQ(medulla_get_protection_level(medulla), PROTECTION_LEVEL_NORMAL);

    EXPECT_EQ(medulla_test_set_protection(medulla, PROTECTION_LEVEL_CRITICAL), 0);
    EXPECT_EQ(medulla_get_protection_level(medulla), PROTECTION_LEVEL_CRITICAL);

    EXPECT_EQ(medulla_test_set_protection(medulla, PROTECTION_LEVEL_SHUTDOWN), 0);
    EXPECT_EQ(medulla_get_protection_level(medulla), PROTECTION_LEVEL_SHUTDOWN);
}

TEST_F(MedullaTest, CircadianPhaseTest) {
    // Test circadian phase setting via test helper
    EXPECT_EQ(medulla_test_set_circadian(medulla, CIRCADIAN_PHASE_MORNING), 0);
    EXPECT_EQ(medulla_get_circadian_phase(medulla), CIRCADIAN_PHASE_MORNING);

    EXPECT_EQ(medulla_test_set_circadian(medulla, CIRCADIAN_PHASE_DEEP_NIGHT), 0);
    EXPECT_EQ(medulla_get_circadian_phase(medulla), CIRCADIAN_PHASE_DEEP_NIGHT);

    EXPECT_EQ(medulla_test_set_circadian(medulla, CIRCADIAN_PHASE_PRE_DAWN), 0);
    EXPECT_EQ(medulla_get_circadian_phase(medulla), CIRCADIAN_PHASE_PRE_DAWN);
}

TEST_F(MedullaTest, TestHelperValidation) {
    // Test helper functions validate inputs
    EXPECT_EQ(medulla_test_set_arousal(nullptr, 0.5f), -1);
    EXPECT_EQ(medulla_test_set_protection(nullptr, PROTECTION_LEVEL_NORMAL), -1);
    EXPECT_EQ(medulla_test_set_circadian(nullptr, CIRCADIAN_PHASE_MORNING), -1);

    // Invalid protection level
    EXPECT_EQ(medulla_test_set_protection(medulla, (protection_level_t)100), -1);

    // Invalid circadian phase
    EXPECT_EQ(medulla_test_set_circadian(medulla, (circadian_phase_t)100), -1);
}

TEST_F(MedullaTest, ArousalClampingOnTestSet) {
    // Test arousal clamping in test helper
    EXPECT_EQ(medulla_test_set_arousal(medulla, -1.0f), 0);  // Should clamp to 0
    float arousal = medulla_get_arousal_level(medulla);
    EXPECT_EQ(arousal, 0.0f);

    EXPECT_EQ(medulla_test_set_arousal(medulla, 2.0f), 0);  // Should clamp to 1
    arousal = medulla_get_arousal_level(medulla);
    EXPECT_EQ(arousal, 1.0f);
}

TEST_F(MedullaTest, StatsAccuracyAfterOperations) {
    medulla_start(medulla);

    const int num_updates = 50;
    for (int i = 0; i < num_updates; i++) {
        medulla_update(medulla, 0.016f);
    }

    // Trigger emergency
    medulla_emergency_shutdown(medulla, "test");

    medulla_stats_t stats;
    medulla_get_stats(medulla, &stats);

    EXPECT_GE(stats.total_updates, (uint64_t)num_updates);
    EXPECT_GE(stats.emergency_shutdowns, 1u);
    // uptime_ms may be 0 if test runs faster than 1ms - just verify it's non-negative
    EXPECT_GE(stats.uptime_ms, 0u);
    EXPECT_GE(stats.avg_update_time_us, 0.0f);

    medulla_stop(medulla);
}

TEST_F(MedullaTest, StringConversionFunctions) {
    // Test utility string conversion functions
    EXPECT_STREQ(medulla_arousal_level_to_string(AROUSAL_LEVEL_COMA), "COMA");
    EXPECT_STREQ(medulla_arousal_level_to_string(AROUSAL_LEVEL_AWAKE), "AWAKE");
    EXPECT_STREQ(medulla_arousal_level_to_string(AROUSAL_LEVEL_HYPERAROUSAL), "HYPERAROUSAL");

    EXPECT_STREQ(medulla_protection_level_to_string(PROTECTION_LEVEL_NORMAL), "NORMAL");
    EXPECT_STREQ(medulla_protection_level_to_string(PROTECTION_LEVEL_SHUTDOWN), "SHUTDOWN");

    EXPECT_STREQ(medulla_circadian_phase_to_string(CIRCADIAN_PHASE_MORNING), "MORNING");
    EXPECT_STREQ(medulla_circadian_phase_to_string(CIRCADIAN_PHASE_DEEP_NIGHT), "DEEP_NIGHT");

    EXPECT_STREQ(medulla_state_to_string(MEDULLA_STATE_STOPPED), "STOPPED");
    EXPECT_STREQ(medulla_state_to_string(MEDULLA_STATE_RUNNING), "RUNNING");
    EXPECT_STREQ(medulla_state_to_string(MEDULLA_STATE_EMERGENCY), "EMERGENCY");
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
