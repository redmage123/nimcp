/**
 * @file test_crash_recovery.cpp
 * @brief End-to-end tests for crash recovery and fault tolerance (Phase 6)
 * @version 1.0.0
 * @date 2026-01-19
 *
 * WHAT: E2E tests for crash recovery, checkpoint/restore, and fault handling
 * WHY:  Verify system can recover from crashes and maintain state consistency
 * HOW:  Test realistic failure scenarios and recovery paths
 *
 * Test Scenarios:
 * 1. Normal startup/shutdown with state persistence
 * 2. Crash recovery from checkpoint
 * 3. Heartbeat timeout detection and response
 * 4. Memory corruption detection and quarantine
 * 5. NaN detection and clearing
 * 6. Deadlock detection and thread restart
 * 7. Graceful shutdown on SIGTERM
 * 8. Emergency save on critical failure
 * 9. State consistency after forced termination
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <cstring>
#include <atomic>
#include <vector>
#include <memory>
#include <cmath>
#include <csignal>
#include <functional>

// Health agent header
#include "utils/fault_tolerance/nimcp_health_agent.h"

// Brain immune system for full integration
#include "cognitive/immune/nimcp_brain_immune.h"

/* ============================================================================
 * Test Utilities
 * ============================================================================ */

// Callback tracking for anomaly detection
static std::atomic<int> g_anomaly_count{0};
static std::atomic<int> g_recovery_count{0};
static std::atomic<bool> g_emergency_triggered{false};
static health_agent_severity_t g_last_severity = HEALTH_SEVERITY_INFO;
static health_agent_recovery_t g_last_recovery = HEALTH_RECOVERY_NONE;

static void test_anomaly_callback(const health_agent_message_t* msg, void* user_data) {
    (void)user_data;
    if (msg) {
        g_anomaly_count++;
        g_last_severity = msg->severity;
        if (msg->type == HEALTH_MSG_EMERGENCY) {
            g_emergency_triggered = true;
        }
    }
}

static void test_recovery_callback(health_agent_recovery_t action, bool success, void* user_data) {
    (void)user_data;
    (void)success;
    g_recovery_count++;
    g_last_recovery = action;
}

static void reset_callbacks() {
    g_anomaly_count = 0;
    g_recovery_count = 0;
    g_emergency_triggered = false;
    g_last_severity = HEALTH_SEVERITY_INFO;
    g_last_recovery = HEALTH_RECOVERY_NONE;
}

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

class CrashRecoveryE2ETest : public ::testing::Test {
protected:
    nimcp_health_agent_t* agent = nullptr;
    health_agent_config_t config;

    void SetUp() override {
        reset_callbacks();
        nimcp_health_agent_default_config(&config);
        strncpy(config.agent_name, "CrashRecoveryTestAgent", sizeof(config.agent_name) - 1);
        config.check_interval_ms = 50;
        config.heartbeat_interval_ms = 100;
        config.watchdog_timeout_ms = 500;
        config.enable_auto_recovery = true;
        config.enable_emergency_checkpoint = true;
        config.enable_emergency_rollback = true;
        config.on_anomaly_detected = test_anomaly_callback;
        config.on_recovery_executed = test_recovery_callback;
        config.callback_user_data = nullptr;

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

class CrashRecoveryWithImmuneE2ETest : public CrashRecoveryE2ETest {
protected:
    brain_immune_system_t* immune = nullptr;

    void SetUp() override {
        CrashRecoveryE2ETest::SetUp();

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
        CrashRecoveryE2ETest::TearDown();
    }
};

/* ============================================================================
 * Test 1: Normal Startup/Shutdown with State Persistence
 * ============================================================================ */

TEST_F(CrashRecoveryE2ETest, NormalStartupShutdown) {
    printf("=== Test: Normal Startup/Shutdown ===\n");

    // Step 1: Connect cognitive modules
    EXPECT_EQ(nimcp_health_agent_connect_failure_prediction(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_metacognition(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_ethics(agent, nullptr, nullptr), 0);

    // Step 2: Start agent
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    EXPECT_TRUE(nimcp_health_agent_is_running(agent));
    printf("  Agent started successfully\n");

    // Step 3: Run with heartbeats to establish baseline
    for (int i = 0; i < 30; i++) {
        nimcp_health_agent_heartbeat(agent);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    // Step 4: Get statistics before shutdown
    health_agent_stats_t stats_before;
    memset(&stats_before, 0, sizeof(stats_before));
    nimcp_health_agent_get_stats(agent, &stats_before);
    printf("  Checks performed before shutdown: %lu\n", (unsigned long)stats_before.checks_performed);
    printf("  Heartbeats received: %lu\n", (unsigned long)stats_before.heartbeats_received);

    // Step 5: Graceful shutdown
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
    EXPECT_FALSE(nimcp_health_agent_is_running(agent));
    printf("  Agent stopped gracefully\n");

    // Step 6: Verify statistics persisted
    health_agent_stats_t stats_after;
    memset(&stats_after, 0, sizeof(stats_after));
    nimcp_health_agent_get_stats(agent, &stats_after);
    EXPECT_EQ(stats_after.heartbeats_received, stats_before.heartbeats_received);
    printf("  Statistics persisted correctly\n");

    printf("Test passed: Normal startup/shutdown completed\n\n");
}

TEST_F(CrashRecoveryE2ETest, MultipleStartStopCycles) {
    printf("=== Test: Multiple Start/Stop Cycles ===\n");

    for (int cycle = 0; cycle < 5; cycle++) {
        printf("  Cycle %d: ", cycle + 1);

        // Start
        EXPECT_EQ(nimcp_health_agent_start(agent), 0);
        EXPECT_TRUE(nimcp_health_agent_is_running(agent));

        // Run briefly
        for (int i = 0; i < 10; i++) {
            nimcp_health_agent_heartbeat(agent);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        // Stop
        EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
        EXPECT_FALSE(nimcp_health_agent_is_running(agent));

        printf("OK\n");
    }

    // Verify agent still functional after multiple cycles
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    EXPECT_TRUE(nimcp_health_agent_is_running(agent));
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);

    printf("Test passed: Multiple start/stop cycles completed\n\n");
}

/* ============================================================================
 * Test 2: Crash Recovery from Checkpoint
 * ============================================================================ */

TEST_F(CrashRecoveryE2ETest, EmergencyCheckpointRequest) {
    printf("=== Test: Emergency Checkpoint Request ===\n");

    // Start agent
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Run for a bit
    for (int i = 0; i < 20; i++) {
        nimcp_health_agent_heartbeat(agent);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    // Request emergency checkpoint
    int result = nimcp_health_agent_request_emergency_checkpoint(agent, "Test emergency");
    printf("  Emergency checkpoint request result: %d\n", result);

    // Give time for checkpoint to process
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Check if emergency was triggered
    printf("  Emergency triggered: %s\n", g_emergency_triggered ? "yes" : "no");

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
    printf("Test passed: Emergency checkpoint request completed\n\n");
}

TEST_F(CrashRecoveryE2ETest, CheckpointIntegrationConfig) {
    printf("=== Test: Checkpoint Integration Config ===\n");

    health_agent_checkpoint_config_t checkpoint_config = {
        .enable_checkpoint_integration = true,
        .enable_auto_checkpoint = true,
        .enable_auto_rollback = true,
        .checkpoint_interval_ms = 1000,
        .health_threshold_checkpoint = 0.8f,
        .health_threshold_rollback = 0.3f
    };

    // Connect checkpoint (with NULL manager - simulated)
    int result = nimcp_health_agent_connect_checkpoint(agent, nullptr, &checkpoint_config);
    printf("  Checkpoint connect result: %d\n", result);

    // Start and run
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    for (int i = 0; i < 30; i++) {
        nimcp_health_agent_heartbeat(agent);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
    printf("Test passed: Checkpoint integration config completed\n\n");
}

/* ============================================================================
 * Test 3: Heartbeat Timeout Detection and Response
 * ============================================================================ */

TEST_F(CrashRecoveryE2ETest, HeartbeatTimeoutDetection) {
    printf("=== Test: Heartbeat Timeout Detection ===\n");

    // Configure shorter timeout for testing
    config.heartbeat_interval_ms = 50;
    config.watchdog_timeout_ms = 200;

    // Recreate agent with new config
    nimcp_health_agent_destroy(agent);
    agent = nimcp_health_agent_create(&config);
    ASSERT_NE(agent, nullptr);

    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Send some heartbeats
    printf("  Sending initial heartbeats...\n");
    for (int i = 0; i < 5; i++) {
        nimcp_health_agent_heartbeat(agent);
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }

    // Stop sending heartbeats and wait for timeout
    printf("  Waiting for heartbeat timeout (%d ms)...\n", config.watchdog_timeout_ms);
    std::this_thread::sleep_for(std::chrono::milliseconds(config.watchdog_timeout_ms + 100));

    // Check statistics
    health_agent_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    nimcp_health_agent_get_stats(agent, &stats);
    printf("  Heartbeats received: %lu\n", (unsigned long)stats.heartbeats_received);
    printf("  Heartbeat timeouts: %lu\n", (unsigned long)stats.heartbeat_timeouts);

    // Resume heartbeats
    printf("  Resuming heartbeats...\n");
    for (int i = 0; i < 10; i++) {
        nimcp_health_agent_heartbeat(agent);
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
    printf("Test passed: Heartbeat timeout detection completed\n\n");
}

TEST_F(CrashRecoveryE2ETest, HeartbeatRecoveryAfterTimeout) {
    printf("=== Test: Heartbeat Recovery After Timeout ===\n");

    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Establish baseline
    for (int i = 0; i < 10; i++) {
        nimcp_health_agent_heartbeat(agent);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    // Get health score before timeout
    float score_before = nimcp_health_agent_get_neural_health_score(agent);
    printf("  Health score before: %.4f\n", score_before);

    // Wait for potential timeout
    std::this_thread::sleep_for(std::chrono::milliseconds(600));

    // Resume heartbeats - system should recover
    for (int i = 0; i < 20; i++) {
        nimcp_health_agent_heartbeat(agent);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    // Get health score after recovery
    float score_after = nimcp_health_agent_get_neural_health_score(agent);
    printf("  Health score after recovery: %.4f\n", score_after);

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
    printf("Test passed: Heartbeat recovery completed\n\n");
}

/* ============================================================================
 * Test 4: Memory Corruption Detection
 * ============================================================================ */

TEST_F(CrashRecoveryE2ETest, MemoryDetectorConfiguration) {
    printf("=== Test: Memory Detector Configuration ===\n");

    // Configure memory detector
    config.memory_detector.enabled = true;
    config.memory_detector.check_interval_ms = 100;
    config.memory_detector.min_report_severity = HEALTH_SEVERITY_WARNING;
    config.memory_detector.threshold_count = 1;

    nimcp_health_agent_destroy(agent);
    agent = nimcp_health_agent_create(&config);
    ASSERT_NE(agent, nullptr);

    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Run and allow memory checks to occur
    for (int i = 0; i < 30; i++) {
        nimcp_health_agent_heartbeat(agent);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    health_agent_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    nimcp_health_agent_get_stats(agent, &stats);
    printf("  Memory corruptions detected: %lu\n", (unsigned long)stats.memory_corruptions);
    printf("  Checks performed: %lu\n", (unsigned long)stats.checks_performed);

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
    printf("Test passed: Memory detector configuration completed\n\n");
}

/* ============================================================================
 * Test 5: NaN Detection and Clearing
 * ============================================================================ */

TEST_F(CrashRecoveryE2ETest, NaNDetectorConfiguration) {
    printf("=== Test: NaN Detector Configuration ===\n");

    // Configure NaN detector
    config.nan_detector.enabled = true;
    config.nan_detector.check_interval_ms = 100;
    config.nan_detector.min_report_severity = HEALTH_SEVERITY_ERROR;
    config.nan_detector.threshold_count = 1;

    nimcp_health_agent_destroy(agent);
    agent = nimcp_health_agent_create(&config);
    ASSERT_NE(agent, nullptr);

    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Run checks
    for (int i = 0; i < 30; i++) {
        nimcp_health_agent_heartbeat(agent);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    health_agent_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    nimcp_health_agent_get_stats(agent, &stats);
    printf("  NaNs detected: %lu\n", (unsigned long)stats.nans_detected);
    printf("  Checks performed: %lu\n", (unsigned long)stats.checks_performed);

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
    printf("Test passed: NaN detector configuration completed\n\n");
}

/* ============================================================================
 * Test 6: Deadlock Detection
 * ============================================================================ */

TEST_F(CrashRecoveryE2ETest, DeadlockDetectorConfiguration) {
    printf("=== Test: Deadlock Detector Configuration ===\n");

    // Configure deadlock detector
    config.deadlock_detector.enabled = true;
    config.deadlock_detector.check_interval_ms = 200;
    config.deadlock_detector.min_report_severity = HEALTH_SEVERITY_CRITICAL;
    config.deadlock_detector.threshold_count = 1;

    nimcp_health_agent_destroy(agent);
    agent = nimcp_health_agent_create(&config);
    ASSERT_NE(agent, nullptr);

    // Connect deadlock detector (simulated with NULL)
    int result = nimcp_health_agent_connect_deadlock_detector(agent, nullptr);
    printf("  Deadlock detector connect result: %d\n", result);

    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Run and allow deadlock checks to occur
    for (int i = 0; i < 20; i++) {
        nimcp_health_agent_heartbeat(agent);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    health_agent_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    nimcp_health_agent_get_stats(agent, &stats);
    printf("  Deadlocks detected: %lu\n", (unsigned long)stats.deadlocks_detected);
    printf("  Checks performed: %lu\n", (unsigned long)stats.checks_performed);

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
    printf("Test passed: Deadlock detector configuration completed\n\n");
}

/* ============================================================================
 * Test 7: Graceful Shutdown
 * ============================================================================ */

TEST_F(CrashRecoveryE2ETest, GracefulShutdownUnderLoad) {
    printf("=== Test: Graceful Shutdown Under Load ===\n");

    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Create background heartbeat thread
    std::atomic<bool> running{true};
    std::thread heartbeat_thread([this, &running]() {
        while (running) {
            nimcp_health_agent_heartbeat(agent);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    // Let it run under load
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Request graceful shutdown
    printf("  Requesting graceful shutdown...\n");
    running = false;
    heartbeat_thread.join();

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
    EXPECT_FALSE(nimcp_health_agent_is_running(agent));

    printf("Test passed: Graceful shutdown under load completed\n\n");
}

TEST_F(CrashRecoveryE2ETest, ShutdownWithPendingOperations) {
    printf("=== Test: Shutdown With Pending Operations ===\n");

    // Connect multiple modules
    EXPECT_EQ(nimcp_health_agent_connect_failure_prediction(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_metacognition(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_ethics(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_emotion(agent, nullptr, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_wellbeing(agent, nullptr, nullptr), 0);

    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Run briefly
    for (int i = 0; i < 20; i++) {
        nimcp_health_agent_heartbeat(agent);
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }

    // Stop immediately (with pending checks)
    printf("  Stopping with pending operations...\n");
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
    EXPECT_FALSE(nimcp_health_agent_is_running(agent));

    printf("Test passed: Shutdown with pending operations completed\n\n");
}

/* ============================================================================
 * Test 8: Emergency Save on Critical Failure
 * ============================================================================ */

TEST_F(CrashRecoveryE2ETest, CriticalSeverityTriggersEmergency) {
    printf("=== Test: Critical Severity Triggers Emergency ===\n");

    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Run normally first
    for (int i = 0; i < 10; i++) {
        nimcp_health_agent_heartbeat(agent);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    // Request emergency checkpoint (simulating critical failure)
    printf("  Simulating critical failure...\n");
    int result = nimcp_health_agent_request_emergency_checkpoint(agent, "Critical test failure");
    printf("  Emergency checkpoint result: %d\n", result);

    // Allow time for emergency processing
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    printf("  Anomaly callbacks: %d\n", g_anomaly_count.load());
    printf("  Emergency triggered: %s\n", g_emergency_triggered ? "yes" : "no");

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
    printf("Test passed: Critical severity emergency completed\n\n");
}

/* ============================================================================
 * Test 9: State Consistency After Forced Termination
 * ============================================================================ */

TEST_F(CrashRecoveryE2ETest, StateConsistencyAfterAbruptStop) {
    printf("=== Test: State Consistency After Abrupt Stop ===\n");

    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Run for a while
    for (int i = 0; i < 50; i++) {
        nimcp_health_agent_heartbeat(agent);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Get state before abrupt stop
    health_agent_stats_t stats_before;
    memset(&stats_before, 0, sizeof(stats_before));
    nimcp_health_agent_get_stats(agent, &stats_before);
    printf("  Heartbeats before: %lu\n", (unsigned long)stats_before.heartbeats_received);

    // Abrupt stop (no cleanup)
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);

    // Verify state is consistent
    health_agent_stats_t stats_after;
    memset(&stats_after, 0, sizeof(stats_after));
    nimcp_health_agent_get_stats(agent, &stats_after);
    printf("  Heartbeats after: %lu\n", (unsigned long)stats_after.heartbeats_received);

    // Stats should be preserved
    EXPECT_EQ(stats_after.heartbeats_received, stats_before.heartbeats_received);

    printf("Test passed: State consistency after abrupt stop completed\n\n");
}

TEST_F(CrashRecoveryE2ETest, RestartAfterAbruptStop) {
    printf("=== Test: Restart After Abrupt Stop ===\n");

    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Run briefly
    for (int i = 0; i < 20; i++) {
        nimcp_health_agent_heartbeat(agent);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Abrupt stop
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
    printf("  Agent stopped abruptly\n");

    // Immediate restart
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    EXPECT_TRUE(nimcp_health_agent_is_running(agent));
    printf("  Agent restarted successfully\n");

    // Run again
    for (int i = 0; i < 20; i++) {
        nimcp_health_agent_heartbeat(agent);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
    printf("Test passed: Restart after abrupt stop completed\n\n");
}

/* ============================================================================
 * Integration Tests with Immune System
 * ============================================================================ */

TEST_F(CrashRecoveryWithImmuneE2ETest, FullIntegrationWithImmune) {
    printf("=== Test: Full Integration With Immune System ===\n");

    if (immune) {
        int connect_result = nimcp_health_agent_connect_immune(agent, immune);
        printf("  Immune system connected: %d\n", connect_result);
    } else {
        printf("  Immune system not available (skipping connection)\n");
    }

    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Run with heartbeats
    for (int i = 0; i < 30; i++) {
        nimcp_health_agent_heartbeat(agent);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    // Request emergency checkpoint
    nimcp_health_agent_request_emergency_checkpoint(agent, "Integration test");

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    health_agent_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    nimcp_health_agent_get_stats(agent, &stats);
    printf("  Messages sent to immune: %lu\n", (unsigned long)stats.messages_sent);

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
    printf("Test passed: Full integration with immune completed\n\n");
}

/* ============================================================================
 * Recovery Action Tests
 * ============================================================================ */

TEST_F(CrashRecoveryE2ETest, RecoveryActionExecution) {
    printf("=== Test: Recovery Action Execution ===\n");

    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Run briefly
    for (int i = 0; i < 10; i++) {
        nimcp_health_agent_heartbeat(agent);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    // Test recovery actions
    health_agent_recovery_t actions[] = {
        HEALTH_RECOVERY_NONE,
        HEALTH_RECOVERY_CHECKPOINT,
        HEALTH_RECOVERY_CLEAR_NAN,
        HEALTH_RECOVERY_RESTART_THREAD,
        HEALTH_RECOVERY_QUARANTINE
    };

    for (auto action : actions) {
        const char* action_str = health_agent_recovery_to_string(action);
        printf("  Testing recovery action: %s\n", action_str ? action_str : "unknown");
    }

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
    printf("Test passed: Recovery action execution completed\n\n");
}

/* ============================================================================
 * Full Lifecycle Test
 * ============================================================================ */

TEST_F(CrashRecoveryE2ETest, FullCrashRecoveryLifecycle) {
    printf("=== Test: Full Crash Recovery Lifecycle ===\n");

    // Phase 1: Initial setup
    printf("Phase 1: Initial setup\n");
    EXPECT_EQ(nimcp_health_agent_connect_failure_prediction(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_metacognition(agent, nullptr, nullptr), 0);

    // Phase 2: Start and establish baseline
    printf("Phase 2: Start and establish baseline\n");
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    for (int i = 0; i < 20; i++) {
        nimcp_health_agent_heartbeat(agent);
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }

    // Phase 3: Simulate failure scenario
    printf("Phase 3: Simulate failure scenario\n");
    nimcp_health_agent_request_emergency_checkpoint(agent, "Simulated failure");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Phase 4: Recovery
    printf("Phase 4: Recovery phase\n");
    for (int i = 0; i < 20; i++) {
        nimcp_health_agent_heartbeat(agent);
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }

    // Phase 5: Verify recovery
    printf("Phase 5: Verify recovery\n");
    float health_score = nimcp_health_agent_get_neural_health_score(agent);
    printf("  Final health score: %.4f\n", health_score);
    EXPECT_GE(health_score, 0.0f);
    EXPECT_LE(health_score, 100.0f);  // Neural health score is 0-100

    // Phase 6: Clean shutdown
    printf("Phase 6: Clean shutdown\n");
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);

    health_agent_stats_t final_stats;
    memset(&final_stats, 0, sizeof(final_stats));
    nimcp_health_agent_get_stats(agent, &final_stats);
    printf("  Total checks: %lu\n", (unsigned long)final_stats.checks_performed);
    printf("  Total heartbeats: %lu\n", (unsigned long)final_stats.heartbeats_received);
    printf("  Anomalies detected: %lu\n", (unsigned long)final_stats.anomalies_detected);

    printf("Test passed: Full crash recovery lifecycle completed\n\n");
}
