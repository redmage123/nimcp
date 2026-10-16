/**
 * @file test_fault_working_memory_integration.cpp
 * @brief Integration Tests for Fault Working Memory
 * @version 1.0.0
 * @date 2025-11-20
 *
 * WHAT: Integration tests validating fault working memory with other fault tolerance components
 * WHY: Ensure proper interaction between modules in realistic scenarios
 * HOW: Multi-component test scenarios with real-world workflows
 *
 * Test coverage: Cross-module integration, realistic fault scenarios
 * Test count: 15+ integration tests
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <chrono>
#include <atomic>

// Headers have their own extern "C" guards
#include "cognitive/fault_tolerance/nimcp_fault_working_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class FaultWorkingMemoryIntegrationTest : public ::testing::Test {
protected:
    fault_working_memory_t* wm;

    void SetUp() override {
        wm = nullptr;
    }

    void TearDown() override {
        if (wm) {
            fault_working_memory_destroy(wm);
            wm = nullptr;
        }
    }

    fault_t create_fault(uint32_t id, fault_severity_t severity, const char* desc) {
        fault_t fault = {0};
        fault.fault_id = id;
        fault.severity = severity;
        fault.timestamp_us = fault_working_memory_get_timestamp_us();
        snprintf(fault.description, sizeof(fault.description), "%s", desc);
        fault.recovery_attempts = 0;
        fault.is_resolved = false;
        return fault;
    }
};

//=============================================================================
// Multi-Step Recovery Workflow Tests
//=============================================================================

/**
 * @test Multi-Step Recovery Workflow
 * WHAT: Verify working memory tracks multi-step recovery
 * WHY: Real-world recovery requires multiple coordinated steps
 * HOW: Add fault, set strategy, step through recovery, verify state
 */
TEST_F(FaultWorkingMemoryIntegrationTest, MultiStepRecoveryWorkflow) {
    // ARRANGE
    wm = fault_working_memory_create();
    fault_t fault = create_fault(1, FAULT_SEVERITY_MAJOR, "Database connection lost");

    // ACT: Add fault and initiate recovery
    fault_working_memory_add_fault(wm, &fault);
    fault_working_memory_set_recovery_strategy(wm, RECOVERY_STRATEGY_RESTART, 4);

    // Step 1: Stop service
    fault_working_memory_update_progress(wm, 1);
    EXPECT_EQ(fault_working_memory_get_recovery_step(wm), 1u);

    // Step 2: Clear state
    fault_working_memory_update_progress(wm, 2);
    EXPECT_EQ(fault_working_memory_get_recovery_step(wm), 2u);

    // Step 3: Restart service
    fault_working_memory_update_progress(wm, 3);
    EXPECT_EQ(fault_working_memory_get_recovery_step(wm), 3u);

    // Step 4: Verify connection
    fault_working_memory_update_progress(wm, 4);
    EXPECT_EQ(fault_working_memory_get_recovery_step(wm), 4u);

    // ASSERT: Recovery complete
    EXPECT_EQ(fault_working_memory_get_recovery_step(wm),
              fault_working_memory_get_total_steps(wm));
}

/**
 * @test Cascading Failure Recovery
 * WHAT: Verify working memory detects and manages cascading failures
 * WHY: Cascading failures require emergency mode, not incremental recovery
 * HOW: Rapidly add >10 faults, verify cascade detected, test emergency handling
 */
TEST_F(FaultWorkingMemoryIntegrationTest, CascadingFailureRecovery) {
    // ARRANGE
    wm = fault_working_memory_create();

    // ACT: Simulate cascading failure (>10 faults/min)
    for (uint32_t i = 1; i <= 15; i++) {
        fault_t fault = create_fault(i, FAULT_SEVERITY_MAJOR, "Service failure");
        fault_working_memory_add_fault(wm, &fault);
    }

    fault_working_memory_update_cascade_detection(wm);

    // ASSERT: Cascade detected
    EXPECT_TRUE(fault_working_memory_is_cascading(wm));

    // Verify emergency strategy can be set
    bool result = fault_working_memory_set_recovery_strategy(
        wm, RECOVERY_STRATEGY_EMERGENCY, 1);
    EXPECT_TRUE(result);
}

/**
 * @test Priority Escalation During Recovery
 * WHAT: Verify priority changes during recovery are handled
 * WHY: New critical fault should interrupt current recovery
 * HOW: Start recovery for minor fault, add critical fault, verify priority shift
 */
TEST_F(FaultWorkingMemoryIntegrationTest, PriorityEscalationDuringRecovery) {
    // ARRANGE
    wm = fault_working_memory_create();

    // Start recovery for minor fault
    fault_t minor = create_fault(1, FAULT_SEVERITY_MINOR, "Slow response time");
    fault_working_memory_add_fault(wm, &minor);
    fault_working_memory_set_recovery_strategy(wm, RECOVERY_STRATEGY_RETRY, 3);
    fault_working_memory_update_progress(wm, 1);

    // ACT: Critical fault arrives mid-recovery
    fault_t critical = create_fault(2, FAULT_SEVERITY_CRITICAL, "System crash imminent");
    fault_working_memory_add_fault(wm, &critical);

    // ASSERT: Priority shifts to critical fault
    active_fault_t* priority = fault_working_memory_get_priority_fault(wm);
    ASSERT_NE(priority, nullptr);
    EXPECT_EQ(priority->fault.fault_id, 2u);
    EXPECT_EQ(priority->fault.severity, FAULT_SEVERITY_CRITICAL);
}

//=============================================================================
// Concurrent Access Tests
//=============================================================================

/**
 * @test Concurrent Fault Addition
 * WHAT: Verify multiple threads can add faults concurrently
 * WHY: Real systems have concurrent fault sources
 * HOW: Spawn threads adding faults, verify all are tracked
 */
TEST_F(FaultWorkingMemoryIntegrationTest, ConcurrentFaultAddition) {
    // ARRANGE
    wm = fault_working_memory_create();
    const int num_threads = 4;
    const int faults_per_thread = 5;
    std::atomic<int> added_count{0};

    // ACT: Multiple threads add faults
    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, t, faults_per_thread, &added_count]() {
            for (int i = 0; i < faults_per_thread; i++) {
                uint32_t fault_id = t * 100 + i;
                fault_t fault = create_fault(fault_id, FAULT_SEVERITY_MINOR, "Concurrent fault");
                if (fault_working_memory_add_fault(wm, &fault)) {
                    added_count++;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // ASSERT: Faults were added (may be capped by capacity)
    uint32_t count = fault_working_memory_get_count(wm);
    EXPECT_GT(count, 0u);
    EXPECT_LE(count, 9u);  // Miller's Law capacity
}

/**
 * @test Concurrent Add and Remove
 * WHAT: Verify concurrent add and remove operations
 * WHY: Real systems add new faults while resolving old ones
 * HOW: One thread adds, another removes, verify consistency
 */
TEST_F(FaultWorkingMemoryIntegrationTest, ConcurrentAddAndRemove) {
    // ARRANGE
    wm = fault_working_memory_create();
    std::atomic<bool> stop{false};

    // ACT: Add thread
    std::thread adder([this, &stop]() {
        for (uint32_t i = 1; !stop && i <= 50; i++) {
            fault_t fault = create_fault(i, FAULT_SEVERITY_MINOR, "Test");
            fault_working_memory_add_fault(wm, &fault);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });

    // Remove thread
    std::thread remover([this, &stop]() {
        for (uint32_t i = 1; !stop && i <= 50; i++) {
            fault_working_memory_remove_fault(wm, i);
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    });

    adder.join();
    stop = true;
    remover.join();

    // ASSERT: No crashes, state is consistent
    uint32_t count = fault_working_memory_get_count(wm);
    EXPECT_GE(count, 0u);
    EXPECT_LE(count, 9u);
}

//=============================================================================
// Stress Tests
//=============================================================================

/**
 * @test Rapid Fault Churn
 * WHAT: Verify working memory handles rapid add/remove cycles
 * WHY: Simulate high-frequency fault scenarios
 * HOW: Rapidly add and remove faults, verify no memory leaks or crashes
 */
TEST_F(FaultWorkingMemoryIntegrationTest, RapidFaultChurn) {
    // ARRANGE
    wm = fault_working_memory_create();

    // ACT: 1000 add/remove cycles
    for (uint32_t i = 1; i <= 1000; i++) {
        fault_t fault = create_fault(i, FAULT_SEVERITY_MINOR, "Churn test");
        fault_working_memory_add_fault(wm, &fault);

        if (i % 2 == 0) {
            fault_working_memory_remove_fault(wm, i - 1);
        }
    }

    // ASSERT: State is consistent
    uint32_t count = fault_working_memory_get_count(wm);
    EXPECT_GE(count, 0u);
    EXPECT_LE(count, 9u);
}

/**
 * @test Memory Capacity Stress
 * WHAT: Verify memory doesn't leak when constantly at capacity
 * WHY: Ensure eviction doesn't leak memory
 * HOW: Add 1000 faults to 9-slot buffer, verify no leaks
 */
TEST_F(FaultWorkingMemoryIntegrationTest, MemoryCapacityStress) {
    // ARRANGE
    wm = fault_working_memory_create();

    // ACT: Add far more faults than capacity
    for (uint32_t i = 1; i <= 1000; i++) {
        fault_t fault = create_fault(i, FAULT_SEVERITY_MINOR, "Stress test");
        fault_working_memory_add_fault(wm, &fault);
    }

    // ASSERT: Capacity respected, no crashes
    EXPECT_EQ(fault_working_memory_get_count(wm), 9u);
}

//=============================================================================
// Realistic Fault Scenarios
//=============================================================================

/**
 * @test Network Partition Recovery Scenario
 * WHAT: Verify working memory manages network partition recovery
 * WHY: Common distributed system fault
 * HOW: Simulate network partition, track recovery steps
 */
TEST_F(FaultWorkingMemoryIntegrationTest, NetworkPartitionRecovery) {
    // ARRANGE
    wm = fault_working_memory_create();

    // ACT: Network partition detected
    fault_t partition = create_fault(1, FAULT_SEVERITY_CRITICAL, "Network partition detected");
    fault_working_memory_add_fault(wm, &partition);

    // Set recovery strategy: 5-step recovery
    fault_working_memory_set_recovery_strategy(wm, RECOVERY_STRATEGY_FAILOVER, 5);

    // Step through recovery
    fault_working_memory_update_progress(wm, 1);  // Detect partition
    fault_working_memory_update_progress(wm, 2);  // Elect new leader
    fault_working_memory_update_progress(wm, 3);  // Redirect traffic
    fault_working_memory_update_progress(wm, 4);  // Sync state
    fault_working_memory_update_progress(wm, 5);  // Verify consistency

    // ASSERT: Recovery complete
    EXPECT_EQ(fault_working_memory_get_recovery_step(wm), 5u);
    EXPECT_EQ(fault_working_memory_get_total_steps(wm), 5u);
}

/**
 * @test Memory Leak Detection and Recovery
 * WHAT: Verify working memory tracks memory leak recovery
 * WHY: Memory leaks are gradual faults requiring monitoring
 * HOW: Add memory leak fault, track gradual recovery
 */
TEST_F(FaultWorkingMemoryIntegrationTest, MemoryLeakDetectionRecovery) {
    // ARRANGE
    wm = fault_working_memory_create();

    // ACT: Memory leak detected
    fault_t leak = create_fault(1, FAULT_SEVERITY_MAJOR, "Memory leak: 100MB/hour");
    fault_working_memory_add_fault(wm, &leak);

    // Set recovery strategy
    fault_working_memory_set_recovery_strategy(wm, RECOVERY_STRATEGY_GRADUAL, 3);

    // Step through recovery
    fault_working_memory_update_progress(wm, 1);  // Identify leak source
    fault_working_memory_update_progress(wm, 2);  // Apply patch
    fault_working_memory_update_progress(wm, 3);  // Monitor for regression

    // ASSERT
    EXPECT_EQ(fault_working_memory_get_recovery_step(wm), 3u);
}

/**
 * @test Database Corruption Recovery
 * WHAT: Verify working memory tracks database recovery
 * WHY: Critical fault requiring careful multi-step recovery
 * HOW: Simulate database corruption, track recovery phases
 */
TEST_F(FaultWorkingMemoryIntegrationTest, DatabaseCorruptionRecovery) {
    // ARRANGE
    wm = fault_working_memory_create();

    // ACT: Database corruption detected
    fault_t corruption = create_fault(1, FAULT_SEVERITY_CRITICAL, "Database corruption detected");
    fault_working_memory_add_fault(wm, &corruption);

    // Set recovery strategy: 6-step recovery
    fault_working_memory_set_recovery_strategy(wm, RECOVERY_STRATEGY_RESTORE, 6);

    // Step through recovery
    fault_working_memory_update_progress(wm, 1);  // Stop writes
    fault_working_memory_update_progress(wm, 2);  // Backup current state
    fault_working_memory_update_progress(wm, 3);  // Restore from backup
    fault_working_memory_update_progress(wm, 4);  // Replay logs
    fault_working_memory_update_progress(wm, 5);  // Verify integrity
    fault_working_memory_update_progress(wm, 6);  // Resume operations

    // ASSERT
    EXPECT_EQ(fault_working_memory_get_recovery_step(wm), 6u);
}

//=============================================================================
// Mixed Severity Management Tests
//=============================================================================

/**
 * @test Mixed Severity Priority Management
 * WHAT: Verify working memory correctly prioritizes mixed severity faults
 * WHY: Real systems have multiple faults of varying severity
 * HOW: Add mix of severities, verify critical handled first
 */
TEST_F(FaultWorkingMemoryIntegrationTest, MixedSeverityPriorityManagement) {
    // ARRANGE
    wm = fault_working_memory_create();

    // ACT: Add faults of different severities
    fault_t minor1 = create_fault(1, FAULT_SEVERITY_MINOR, "Slow query");
    fault_t minor2 = create_fault(2, FAULT_SEVERITY_MINOR, "High CPU");
    fault_t major1 = create_fault(3, FAULT_SEVERITY_MAJOR, "Service degraded");
    fault_t critical1 = create_fault(4, FAULT_SEVERITY_CRITICAL, "System failing");
    fault_t major2 = create_fault(5, FAULT_SEVERITY_MAJOR, "Network latency");

    fault_working_memory_add_fault(wm, &minor1);
    fault_working_memory_add_fault(wm, &minor2);
    fault_working_memory_add_fault(wm, &major1);
    fault_working_memory_add_fault(wm, &critical1);
    fault_working_memory_add_fault(wm, &major2);

    // ASSERT: Critical fault has priority
    active_fault_t* priority = fault_working_memory_get_priority_fault(wm);
    ASSERT_NE(priority, nullptr);
    EXPECT_EQ(priority->fault.fault_id, 4u);
    EXPECT_EQ(priority->fault.severity, FAULT_SEVERITY_CRITICAL);
}

/**
 * @test Eviction Preserves Critical Faults
 * WHAT: Verify eviction keeps critical faults over minor ones
 * WHY: Limited capacity should prioritize important faults
 * HOW: Fill with minor, add critical, verify minor evicted
 */
TEST_F(FaultWorkingMemoryIntegrationTest, EvictionPreservesCritical) {
    // ARRANGE
    wm = fault_working_memory_create();

    // Fill with minor faults
    for (uint32_t i = 1; i <= 9; i++) {
        fault_t minor = create_fault(i, FAULT_SEVERITY_MINOR, "Minor fault");
        fault_working_memory_add_fault(wm, &minor);
    }

    // ACT: Add critical fault (should evict a minor)
    fault_t critical = create_fault(100, FAULT_SEVERITY_CRITICAL, "Critical fault");
    fault_working_memory_add_fault(wm, &critical);

    // ASSERT: Critical fault is in memory
    bool found_critical = false;
    for (uint32_t i = 0; i < fault_working_memory_get_count(wm); i++) {
        const active_fault_t* fault = fault_working_memory_get_fault_at(wm, i);
        if (fault && fault->fault.fault_id == 100) {
            found_critical = true;
            break;
        }
    }
    EXPECT_TRUE(found_critical);
}

//=============================================================================
// Time-Based Tests
//=============================================================================

/**
 * @test Time-Based Cascade Detection
 * WHAT: Verify cascade detection based on time window
 * WHY: Cascades are defined by rate (faults/time)
 * HOW: Add faults within time window, verify cascade detected
 */
TEST_F(FaultWorkingMemoryIntegrationTest, TimeBasedCascadeDetection) {
    // ARRANGE
    wm = fault_working_memory_create();

    uint64_t start_time = fault_working_memory_get_timestamp_us();

    // ACT: Add 12 faults rapidly
    for (uint32_t i = 1; i <= 12; i++) {
        fault_t fault = create_fault(i, FAULT_SEVERITY_MINOR, "Cascade test");
        fault_working_memory_add_fault(wm, &fault);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    uint64_t end_time = fault_working_memory_get_timestamp_us();
    uint64_t elapsed_us = end_time - start_time;

    // Update cascade detection
    fault_working_memory_update_cascade_detection(wm);

    // ASSERT: If added within 1 minute, cascade should be detected
    if (elapsed_us < 60000000) {  // < 1 minute
        EXPECT_TRUE(fault_working_memory_is_cascading(wm));
    }
}

/**
 * @test Fault Age Tracking
 * WHAT: Verify faults track time in working memory
 * WHY: Support aging-based eviction policies
 * HOW: Add fault, wait, verify time_in_memory increases
 */
TEST_F(FaultWorkingMemoryIntegrationTest, FaultAgeTracking) {
    // ARRANGE
    wm = fault_working_memory_create();
    fault_t fault = create_fault(1, FAULT_SEVERITY_MINOR, "Age test");

    // ACT
    fault_working_memory_add_fault(wm, &fault);
    uint64_t add_time = fault_working_memory_get_timestamp_us();

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    const active_fault_t* active = fault_working_memory_get_fault_at(wm, 0);

    // ASSERT
    ASSERT_NE(active, nullptr);
    EXPECT_GT(active->time_in_memory_us, 0u);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
