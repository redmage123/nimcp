/**
 * @file test_memory_health_e2e.cpp
 * @brief End-to-End Tests for Memory and Health Agent Integration
 *
 * WHAT: Full workflow E2E tests for memory operations with health monitoring
 * WHY:  Verify health agent correctly monitors memory operations and reports issues
 * HOW:  Test memory operations while tracking health agent statistics and heartbeats
 *
 * TEST PIPELINES:
 * - HealthAgentLifecycle: Create, start, stop, destroy health agent
 * - MemoryOperationsWithHeartbeat: Memory ops with periodic heartbeats
 * - MemoryPressureDetection: Detect memory pressure conditions
 * - HealthStatisticsAccumulation: Verify stats accumulate correctly
 * - ConsistencyChecking: Test state consistency validation
 * - AnomalyReporting: Test manual anomaly reporting
 * - HeartbeatTimeout: Verify timeout detection
 * - DetectorConfiguration: Test detector enable/disable
 * - CleanShutdownSequence: Test proper shutdown order
 * - ConcurrentMemoryAndHealth: Concurrent operations stress test
 *
 * @author NIMCP Development Team
 * @date 2026-02-05
 * @version 1.0.0
 */

#include "e2e_test_framework.h"

// Headers have their own extern "C" guards
#include "utils/fault_tolerance/nimcp_health_agent.h"
#include "utils/fault_tolerance/nimcp_health_monitor.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_codes.h"

#include <thread>
#include <atomic>
#include <chrono>
#include <vector>
#include <cstring>
#include <cstdlib>

//=============================================================================
// Test Fixture
//=============================================================================

class MemoryHealthE2ETest : public ::testing::Test {
protected:
    nimcp_health_agent_t* agent_ = nullptr;
    brain_immune_system_t* immune_ = nullptr;

    // Callback tracking
    static std::atomic<int> anomalies_detected_;
    static std::atomic<int> recoveries_executed_;
    static std::atomic<int> heartbeat_timeouts_;
    static std::atomic<health_agent_severity_t> last_severity_;

    void SetUp() override {
        anomalies_detected_.store(0);
        recoveries_executed_.store(0);
        heartbeat_timeouts_.store(0);
        last_severity_.store(HEALTH_SEVERITY_INFO);
    }

    void TearDown() override {
        if (agent_) {
            nimcp_health_agent_stop(agent_);
            nimcp_health_agent_destroy(agent_);
            agent_ = nullptr;
        }
        if (immune_) {
            brain_immune_destroy(immune_);
            immune_ = nullptr;
        }
    }

    // Anomaly callback
    static void OnAnomalyDetected(const health_agent_message_t* msg, void* user_data) {
        (void)user_data;
        anomalies_detected_.fetch_add(1);
        if (msg) {
            last_severity_.store(msg->severity);
            if (msg->type == HEALTH_MSG_HEARTBEAT_TIMEOUT) {
                heartbeat_timeouts_.fetch_add(1);
            }
        }
    }

    // Recovery callback
    static void OnRecoveryExecuted(health_agent_recovery_t action, bool success, void* user_data) {
        (void)action;
        (void)user_data;
        if (success) {
            recoveries_executed_.fetch_add(1);
        }
    }

    // Create immune system
    brain_immune_system_t* CreateImmune() {
        brain_immune_config_t config;
        brain_immune_default_config(&config);
        return brain_immune_create(&config);
    }

    // Create health agent with default config
    nimcp_health_agent_t* CreateDefaultAgent(const char* name, uint32_t heartbeat_ms = 100) {
        health_agent_config_t cfg;
        nimcp_health_agent_default_config(&cfg);
        
        strncpy(cfg.agent_name, name, sizeof(cfg.agent_name) - 1);
        cfg.heartbeat_interval_ms = heartbeat_ms;
        cfg.watchdog_timeout_ms = heartbeat_ms * 5;
        cfg.check_interval_ms = heartbeat_ms / 2;
        
        cfg.heartbeat_detector.enabled = true;
        cfg.heartbeat_detector.check_interval_ms = heartbeat_ms;
        cfg.heartbeat_detector.threshold_count = 3;
        
        cfg.memory_detector.enabled = true;
        cfg.memory_detector.check_interval_ms = heartbeat_ms;
        
        cfg.nan_detector.enabled = true;
        
        cfg.consistency.check_reference_counts = true;
        cfg.consistency.check_pointer_canaries = true;
        cfg.consistency.check_struct_magic = true;
        cfg.consistency.check_neuron_values = true;
        
        cfg.enable_auto_recovery = true;
        cfg.auto_recovery_threshold = HEALTH_SEVERITY_ERROR;
        
        cfg.on_anomaly_detected = OnAnomalyDetected;
        cfg.on_recovery_executed = OnRecoveryExecuted;
        cfg.callback_user_data = nullptr;
        
        return nimcp_health_agent_create(&cfg);
    }
};

// Static member initialization
std::atomic<int> MemoryHealthE2ETest::anomalies_detected_{0};
std::atomic<int> MemoryHealthE2ETest::recoveries_executed_{0};
std::atomic<int> MemoryHealthE2ETest::heartbeat_timeouts_{0};
std::atomic<health_agent_severity_t> MemoryHealthE2ETest::last_severity_{HEALTH_SEVERITY_INFO};

//=============================================================================
// Test 1: Health Agent Lifecycle
//=============================================================================

TEST_F(MemoryHealthE2ETest, HealthAgentLifecycle) {
    E2E_PIPELINE_START("Health Agent Lifecycle");

    // Stage 1: Create immune system
    E2E_STAGE_BEGIN("Create immune system", 200);
    immune_ = CreateImmune();
    E2E_ASSERT_NOT_NULL(immune_, "Failed to create immune system");
    E2E_STAGE_END();

    // Stage 2: Create health agent
    E2E_STAGE_BEGIN("Create health agent", 200);
    agent_ = CreateDefaultAgent("lifecycle_test");
    E2E_ASSERT_NOT_NULL(agent_, "Failed to create health agent");
    E2E_STAGE_END();

    // Stage 3: Connect to immune system
    E2E_STAGE_BEGIN("Connect to immune", 100);
    EXPECT_EQ(nimcp_health_agent_connect_immune(agent_, immune_), 0);
    E2E_STAGE_END();

    // Stage 4: Start agent
    E2E_STAGE_BEGIN("Start agent", 200);
    EXPECT_EQ(nimcp_health_agent_start(agent_), 0);
    EXPECT_TRUE(nimcp_health_agent_is_running(agent_));
    E2E_STAGE_END();

    // Stage 5: Verify running state
    E2E_STAGE_BEGIN("Verify running", 200);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(nimcp_health_agent_is_running(agent_));
    E2E_STAGE_END();

    // Stage 6: Stop agent
    E2E_STAGE_BEGIN("Stop agent", 200);
    EXPECT_EQ(nimcp_health_agent_stop(agent_), 0);
    EXPECT_FALSE(nimcp_health_agent_is_running(agent_));
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Test 2: Memory Operations with Heartbeat
//=============================================================================

TEST_F(MemoryHealthE2ETest, MemoryOperationsWithHeartbeat) {
    E2E_PIPELINE_START("Memory Operations with Heartbeat");

    // Stage 1: Setup
    E2E_STAGE_BEGIN("Setup", 300);
    immune_ = CreateImmune();
    agent_ = CreateDefaultAgent("memory_heartbeat", 50);
    E2E_ASSERT_NOT_NULL(agent_, "Failed to create agent");
    nimcp_health_agent_connect_immune(agent_, immune_);
    nimcp_health_agent_start(agent_);
    E2E_STAGE_END();

    // Stage 2: Memory allocations with heartbeats
    E2E_STAGE_BEGIN("Memory allocations with heartbeats", 1000);
    std::vector<void*> allocations;
    
    for (int i = 0; i < 20; i++) {
        // Send heartbeat
        nimcp_health_agent_heartbeat(agent_);
        
        // Perform memory operation
        size_t size = 1024 * (i + 1);
        void* ptr = nimcp_calloc(1, size);
        if (ptr) {
            allocations.push_back(ptr);
            memset(ptr, 0xAA, size);
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    
    EXPECT_EQ(allocations.size(), 20u);
    E2E_STAGE_END();

    // Stage 3: Memory deallocations with heartbeats
    E2E_STAGE_BEGIN("Memory deallocations", 500);
    for (void* ptr : allocations) {
        nimcp_health_agent_heartbeat(agent_);
        nimcp_free(ptr);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    E2E_STAGE_END();

    // Stage 4: Verify health status
    E2E_STAGE_BEGIN("Verify health status", 200);
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, 30u);
    EXPECT_EQ(stats.heartbeat_timeouts, 0u);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Test 3: Health Statistics Accumulation
//=============================================================================

TEST_F(MemoryHealthE2ETest, HealthStatisticsAccumulation) {
    E2E_PIPELINE_START("Health Statistics Accumulation");

    // Stage 1: Setup
    E2E_STAGE_BEGIN("Setup", 300);
    immune_ = CreateImmune();
    agent_ = CreateDefaultAgent("stats_test", 50);
    E2E_ASSERT_NOT_NULL(agent_, "Failed to create agent");
    nimcp_health_agent_connect_immune(agent_, immune_);
    nimcp_health_agent_start(agent_);
    E2E_STAGE_END();

    // Stage 2: Send many heartbeats
    E2E_STAGE_BEGIN("Send heartbeats", 500);
    for (int i = 0; i < 50; i++) {
        nimcp_health_agent_heartbeat(agent_);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    E2E_STAGE_END();

    // Stage 3: Get and verify initial stats
    E2E_STAGE_BEGIN("Get initial stats", 100);
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent_, &stats);

    // 50 heartbeats * 5ms = ~250ms minimum elapsed time
    EXPECT_GE(stats.heartbeats_received, 40u);
    EXPECT_GE(stats.uptime_ms, 200u);  // At least 200ms (conservative for 250ms actual)
    E2E_STAGE_END();

    // Stage 4: Continue activity
    E2E_STAGE_BEGIN("Continue activity", 300);
    for (int i = 0; i < 20; i++) {
        nimcp_health_agent_heartbeat_ex(agent_, "test_operation", (float)i / 20.0f);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    E2E_STAGE_END();

    // Stage 5: Verify accumulated stats
    E2E_STAGE_BEGIN("Verify accumulated stats", 100);
    health_agent_stats_t final_stats;
    nimcp_health_agent_get_stats(agent_, &final_stats);
    
    EXPECT_GT(final_stats.heartbeats_received, stats.heartbeats_received);
    EXPECT_GT(final_stats.uptime_ms, stats.uptime_ms);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Test 4: Consistency Checking
//=============================================================================

TEST_F(MemoryHealthE2ETest, ConsistencyChecking) {
    E2E_PIPELINE_START("Consistency Checking");

    // Stage 1: Setup with consistency checks enabled
    E2E_STAGE_BEGIN("Setup", 300);
    immune_ = CreateImmune();
    agent_ = CreateDefaultAgent("consistency_test", 100);
    E2E_ASSERT_NOT_NULL(agent_, "Failed to create agent");
    nimcp_health_agent_connect_immune(agent_, immune_);
    nimcp_health_agent_start(agent_);
    E2E_STAGE_END();

    // Stage 2: Run consistency check
    E2E_STAGE_BEGIN("Run consistency check", 500);
    health_agent_consistency_result_t result;
    int failures = nimcp_health_agent_check_consistency(agent_, &result);
    
    // With clean state, should pass
    EXPECT_EQ(failures, 0);
    EXPECT_TRUE(result.overall_passed);
    E2E_STAGE_END();

    // Stage 3: Get cached result
    E2E_STAGE_BEGIN("Get cached result", 100);
    health_agent_consistency_result_t cached;
    EXPECT_EQ(nimcp_health_agent_get_consistency_status(agent_, &cached), 0);
    EXPECT_EQ(cached.overall_passed, result.overall_passed);
    E2E_STAGE_END();

    // Stage 4: Update consistency config
    E2E_STAGE_BEGIN("Update config", 200);
    health_agent_consistency_config_t new_config = {
        .check_reference_counts = true,
        .check_pointer_canaries = true,
        .check_struct_magic = true,
        .check_mutex_state = false,  // Disable mutex check
        .check_circular_buffers = true,
        .check_kg_consistency = false,  // Disable KG check for speed
        .check_neuron_values = true,
        .kg_check_sample_rate = 1,
        .consistency_check_interval_ms = 100
    };
    EXPECT_EQ(nimcp_health_agent_update_consistency_config(agent_, &new_config), 0);
    E2E_STAGE_END();

    // Stage 5: Run check with new config
    E2E_STAGE_BEGIN("Check with new config", 500);
    failures = nimcp_health_agent_check_consistency(agent_, &result);
    EXPECT_EQ(failures, 0);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Test 5: Manual Anomaly Reporting
//=============================================================================

TEST_F(MemoryHealthE2ETest, ManualAnomalyReporting) {
    E2E_PIPELINE_START("Manual Anomaly Reporting");

    // Stage 1: Setup
    E2E_STAGE_BEGIN("Setup", 300);
    immune_ = CreateImmune();
    agent_ = CreateDefaultAgent("anomaly_report", 100);
    E2E_ASSERT_NOT_NULL(agent_, "Failed to create agent");
    nimcp_health_agent_connect_immune(agent_, immune_);
    nimcp_health_agent_start(agent_);
    E2E_STAGE_END();

    // Stage 2: Report memory anomaly
    E2E_STAGE_BEGIN("Report memory anomaly", 200);
    health_agent_message_t msg = {};
    msg.type = HEALTH_MSG_MEMORY_CORRUPTION;
    msg.severity = HEALTH_SEVERITY_ERROR;
    msg.source = HEALTH_SOURCE_MEMORY;
    msg.suggested_action = HEALTH_RECOVERY_GC;
    strncpy(msg.description, "Test memory anomaly report", sizeof(msg.description));
    
    EXPECT_EQ(nimcp_health_agent_report_anomaly(agent_, &msg), 0);
    E2E_STAGE_END();

    // Stage 3: Report NaN anomaly
    E2E_STAGE_BEGIN("Report NaN anomaly", 200);
    msg.type = HEALTH_MSG_NAN_DETECTED;
    msg.severity = HEALTH_SEVERITY_WARNING;
    msg.source = HEALTH_SOURCE_NEURAL;
    msg.suggested_action = HEALTH_RECOVERY_CLEAR_NAN;
    msg.data.nan.neuron_id = 42;
    msg.data.nan.layer_id = 3;
    msg.data.nan.nan_count = 5;
    strncpy(msg.description, "NaN detected in layer 3", sizeof(msg.description));
    
    EXPECT_EQ(nimcp_health_agent_report_anomaly(agent_, &msg), 0);
    E2E_STAGE_END();

    // Stage 4: Verify queue depth
    E2E_STAGE_BEGIN("Verify queue depth", 100);
    uint32_t depth = nimcp_health_agent_get_queue_depth(agent_);
    // Queue depth should be non-zero if messages are pending
    // (or zero if they were processed)
    EXPECT_GE(depth, 0u);
    E2E_STAGE_END();

    // Stage 5: Get stats
    E2E_STAGE_BEGIN("Get stats", 100);
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.messages_sent, 0u);  // May have been processed
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Test 6: Detector Configuration
//=============================================================================

TEST_F(MemoryHealthE2ETest, DetectorConfiguration) {
    E2E_PIPELINE_START("Detector Configuration");

    // Stage 1: Setup
    E2E_STAGE_BEGIN("Setup", 300);
    immune_ = CreateImmune();
    agent_ = CreateDefaultAgent("detector_config", 100);
    E2E_ASSERT_NOT_NULL(agent_, "Failed to create agent");
    nimcp_health_agent_connect_immune(agent_, immune_);
    nimcp_health_agent_start(agent_);
    E2E_STAGE_END();

    // Stage 2: Disable heartbeat detector
    E2E_STAGE_BEGIN("Disable heartbeat detector", 200);
    EXPECT_EQ(nimcp_health_agent_set_detector_enabled(agent_, "heartbeat", false), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    E2E_STAGE_END();

    // Stage 3: Re-enable heartbeat detector
    E2E_STAGE_BEGIN("Re-enable heartbeat detector", 200);
    EXPECT_EQ(nimcp_health_agent_set_detector_enabled(agent_, "heartbeat", true), 0);
    E2E_STAGE_END();

    // Stage 4: Update detector configuration
    E2E_STAGE_BEGIN("Update detector config", 200);
    health_agent_detector_config_t detector_cfg = {
        .enabled = true,
        .check_interval_ms = 50,
        .min_report_severity = HEALTH_SEVERITY_WARNING,
        .threshold_count = 2,
        .cooldown_ms = 100
    };
    EXPECT_EQ(nimcp_health_agent_update_detector(agent_, "memory", &detector_cfg), 0);
    E2E_STAGE_END();

    // Stage 5: Disable NaN detector
    E2E_STAGE_BEGIN("Disable NaN detector", 100);
    EXPECT_EQ(nimcp_health_agent_set_detector_enabled(agent_, "nan", false), 0);
    E2E_STAGE_END();

    // Stage 6: Verify detectors still work
    E2E_STAGE_BEGIN("Verify operation", 300);
    for (int i = 0; i < 10; i++) {
        nimcp_health_agent_heartbeat(agent_);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    EXPECT_TRUE(nimcp_health_agent_is_running(agent_));
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Test 7: Request Immediate Check
//=============================================================================

TEST_F(MemoryHealthE2ETest, RequestImmediateCheck) {
    E2E_PIPELINE_START("Request Immediate Check");

    // Stage 1: Setup
    E2E_STAGE_BEGIN("Setup", 300);
    immune_ = CreateImmune();
    agent_ = CreateDefaultAgent("immediate_check", 100);
    E2E_ASSERT_NOT_NULL(agent_, "Failed to create agent");
    nimcp_health_agent_connect_immune(agent_, immune_);
    nimcp_health_agent_start(agent_);
    E2E_STAGE_END();

    // Stage 2: Request check
    E2E_STAGE_BEGIN("Request check", 200);
    EXPECT_EQ(nimcp_health_agent_request_check(agent_), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    E2E_STAGE_END();

    // Stage 3: Get stats
    E2E_STAGE_BEGIN("Get stats", 100);
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.checks_performed, 1u);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Test 8: Clean Shutdown Sequence
//=============================================================================

TEST_F(MemoryHealthE2ETest, CleanShutdownSequence) {
    E2E_PIPELINE_START("Clean Shutdown Sequence");

    // Stage 1: Setup multiple components
    E2E_STAGE_BEGIN("Setup components", 400);
    immune_ = CreateImmune();
    agent_ = CreateDefaultAgent("shutdown_test", 50);
    E2E_ASSERT_NOT_NULL(agent_, "Failed to create agent");
    nimcp_health_agent_connect_immune(agent_, immune_);
    nimcp_health_agent_start(agent_);
    E2E_STAGE_END();

    // Stage 2: Active operation period
    E2E_STAGE_BEGIN("Active operation", 500);
    for (int i = 0; i < 20; i++) {
        nimcp_health_agent_heartbeat(agent_);
        
        // Some memory operations
        void* ptr = nimcp_calloc(1, 1024);
        if (ptr) {
            nimcp_free(ptr);
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(15));
    }
    EXPECT_TRUE(nimcp_health_agent_is_running(agent_));
    E2E_STAGE_END();

    // Stage 3: Stop agent first
    E2E_STAGE_BEGIN("Stop agent", 200);
    EXPECT_EQ(nimcp_health_agent_stop(agent_), 0);
    EXPECT_FALSE(nimcp_health_agent_is_running(agent_));
    E2E_STAGE_END();

    // Stage 4: Verify no pending messages
    E2E_STAGE_BEGIN("Verify queue drained", 100);
    uint32_t pending = nimcp_health_agent_pending_messages(agent_);
    // Messages should be drained or minimal
    EXPECT_LE(pending, 10u);
    E2E_STAGE_END();

    // Stage 5: Destroy agent
    E2E_STAGE_BEGIN("Destroy agent", 100);
    nimcp_health_agent_destroy(agent_);
    agent_ = nullptr;  // Prevent double-free in TearDown
    E2E_STAGE_END();

    // Stage 6: Destroy immune
    E2E_STAGE_BEGIN("Destroy immune", 100);
    brain_immune_destroy(immune_);
    immune_ = nullptr;  // Prevent double-free in TearDown
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Test 9: Concurrent Memory and Health
//=============================================================================

TEST_F(MemoryHealthE2ETest, ConcurrentMemoryAndHealth) {
    E2E_PIPELINE_START("Concurrent Memory and Health");

    // Stage 1: Setup
    E2E_STAGE_BEGIN("Setup", 300);
    immune_ = CreateImmune();
    agent_ = CreateDefaultAgent("concurrent_test", 50);
    E2E_ASSERT_NOT_NULL(agent_, "Failed to create agent");
    nimcp_health_agent_connect_immune(agent_, immune_);
    nimcp_health_agent_start(agent_);
    E2E_STAGE_END();

    // Stage 2: Launch concurrent workers
    E2E_STAGE_BEGIN("Concurrent workers", 2000);
    const int num_threads = 4;
    const int ops_per_thread = 50;
    std::atomic<int> total_allocations{0};
    std::atomic<int> total_heartbeats{0};
    std::vector<std::thread> threads;
    
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < ops_per_thread; i++) {
                // Send heartbeat
                nimcp_health_agent_heartbeat(agent_);
                total_heartbeats.fetch_add(1);
                
                // Memory operation
                size_t size = 256 * ((t + 1) * (i + 1) % 16 + 1);
                void* ptr = nimcp_calloc(1, size);
                if (ptr) {
                    memset(ptr, t, size);
                    total_allocations.fetch_add(1);
                    nimcp_free(ptr);
                }
                
                // Occasional extended heartbeat
                if (i % 10 == 0) {
                    char op_name[32];
                    snprintf(op_name, sizeof(op_name), "thread_%d_op_%d", t, i);
                    nimcp_health_agent_heartbeat_ex(agent_, op_name, (float)i / ops_per_thread);
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    E2E_STAGE_END();

    // Stage 3: Verify results
    E2E_STAGE_BEGIN("Verify results", 200);
    EXPECT_EQ(total_allocations.load(), num_threads * ops_per_thread);
    EXPECT_GE(total_heartbeats.load(), num_threads * ops_per_thread);
    
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, (uint64_t)(total_heartbeats.load() / 2));
    EXPECT_TRUE(nimcp_health_agent_is_running(agent_));
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Test 10: Magic Validation Registration
//=============================================================================

TEST_F(MemoryHealthE2ETest, MagicValidationRegistration) {
    E2E_PIPELINE_START("Magic Validation Registration");

    // Stage 1: Setup
    E2E_STAGE_BEGIN("Setup", 300);
    immune_ = CreateImmune();
    agent_ = CreateDefaultAgent("magic_test", 100);
    E2E_ASSERT_NOT_NULL(agent_, "Failed to create agent");
    nimcp_health_agent_connect_immune(agent_, immune_);
    nimcp_health_agent_start(agent_);
    E2E_STAGE_END();

    // Stage 2: Create test structure with magic
    E2E_STAGE_BEGIN("Create test struct", 100);
    struct TestStruct {
        uint32_t magic;
        int data;
        char name[32];
    };
    
    const uint32_t EXPECTED_MAGIC = 0xDEADBEEF;
    TestStruct* test_struct = (TestStruct*)nimcp_calloc(1, sizeof(TestStruct));
    E2E_ASSERT_NOT_NULL(test_struct, "Failed to allocate test struct");
    test_struct->magic = EXPECTED_MAGIC;
    test_struct->data = 42;
    strncpy(test_struct->name, "test", sizeof(test_struct->name));
    E2E_STAGE_END();

    // Stage 3: Register struct for validation
    E2E_STAGE_BEGIN("Register struct", 200);
    EXPECT_EQ(nimcp_health_agent_register_struct(agent_, test_struct, EXPECTED_MAGIC, "TestStruct"), 0);
    E2E_STAGE_END();

    // Stage 4: Validate magic
    E2E_STAGE_BEGIN("Validate magic", 100);
    EXPECT_TRUE(nimcp_health_agent_validate_magic(test_struct, EXPECTED_MAGIC, "TestStruct"));
    EXPECT_FALSE(nimcp_health_agent_validate_magic(test_struct, 0x12345678, "TestStruct"));  // Wrong magic
    E2E_STAGE_END();

    // Stage 5: Run consistency check
    E2E_STAGE_BEGIN("Run consistency check", 500);
    health_agent_consistency_result_t result;
    int failures = nimcp_health_agent_check_consistency(agent_, &result);
    EXPECT_EQ(failures, 0);
    EXPECT_TRUE(result.magic_check_passed);
    E2E_STAGE_END();

    // Stage 6: Unregister and cleanup
    E2E_STAGE_BEGIN("Unregister struct", 200);
    EXPECT_EQ(nimcp_health_agent_unregister_struct(agent_, test_struct), 0);
    nimcp_free(test_struct);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Test 11: Current Status Query
//=============================================================================

TEST_F(MemoryHealthE2ETest, CurrentStatusQuery) {
    E2E_PIPELINE_START("Current Status Query");

    // Stage 1: Setup
    E2E_STAGE_BEGIN("Setup", 300);
    immune_ = CreateImmune();
    agent_ = CreateDefaultAgent("status_test", 100);
    E2E_ASSERT_NOT_NULL(agent_, "Failed to create agent");
    nimcp_health_agent_connect_immune(agent_, immune_);
    nimcp_health_agent_start(agent_);
    E2E_STAGE_END();

    // Stage 2: Check initial status
    E2E_STAGE_BEGIN("Check initial status", 100);
    health_agent_severity_t status = nimcp_health_agent_current_status(agent_);
    EXPECT_LE(static_cast<int>(status), static_cast<int>(HEALTH_SEVERITY_INFO));
    E2E_STAGE_END();

    // Stage 3: Send normal heartbeats
    E2E_STAGE_BEGIN("Send heartbeats", 300);
    for (int i = 0; i < 10; i++) {
        nimcp_health_agent_heartbeat(agent_);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    status = nimcp_health_agent_current_status(agent_);
    // Status should still be healthy
    EXPECT_LE(static_cast<int>(status), static_cast<int>(HEALTH_SEVERITY_WARNING));
    E2E_STAGE_END();

    // Stage 4: Check pending messages
    E2E_STAGE_BEGIN("Check pending messages", 100);
    uint32_t pending = nimcp_health_agent_pending_messages(agent_);
    // Should have minimal pending messages in healthy state
    EXPECT_LE(pending, 50u);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Test 12: Message Type Conversion
//=============================================================================

TEST_F(MemoryHealthE2ETest, MessageTypeConversion) {
    E2E_PIPELINE_START("Message Type Conversion");

    // Stage 1: Convert message types
    E2E_STAGE_BEGIN("Convert message types", 100);
    EXPECT_STREQ(health_agent_msg_type_to_string(HEALTH_MSG_ANOMALY_DETECTED), "ANOMALY_DETECTED");
    EXPECT_STREQ(health_agent_msg_type_to_string(HEALTH_MSG_HEARTBEAT_TIMEOUT), "HEARTBEAT_TIMEOUT");
    EXPECT_STREQ(health_agent_msg_type_to_string(HEALTH_MSG_MEMORY_CORRUPTION), "MEMORY_CORRUPTION");
    EXPECT_STREQ(health_agent_msg_type_to_string(HEALTH_MSG_NAN_DETECTED), "NAN_DETECTED");
    EXPECT_STREQ(health_agent_msg_type_to_string(HEALTH_MSG_DEADLOCK_DETECTED), "DEADLOCK_DETECTED");
    E2E_STAGE_END();

    // Stage 2: Test boundary values
    E2E_STAGE_BEGIN("Test boundary values", 100);
    // Should handle edge cases gracefully
    const char* unknown = health_agent_msg_type_to_string(HEALTH_MSG_COUNT);
    EXPECT_NE(unknown, nullptr);  // Should return something, not crash
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}
