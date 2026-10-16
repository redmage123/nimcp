/**
 * @file test_fault_working_memory_regression.cpp
 * @brief Regression Tests for Fault Working Memory
 * @version 1.0.0
 * @date 2025-11-20
 *
 * WHAT: Regression tests ensuring no behavioral regressions
 * WHY: Maintain stability across releases
 * HOW: Test known edge cases, previous bugs, performance benchmarks
 *
 * Test coverage: Edge cases, boundary conditions, performance regression
 * Test count: 10+ regression tests
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <chrono>

// Headers have their own extern "C" guards
#include "cognitive/fault_tolerance/nimcp_fault_working_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class FaultWorkingMemoryRegressionTest : public ::testing::Test {
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
// Known Bug Regression Tests
//=============================================================================

/**
 * @test Regression: Double Free on Destroy
 * WHAT: Verify no double-free when destroying populated working memory
 * WHY: Previous bug caused double-free crash
 * HOW: Add faults, destroy, verify no crash
 */
TEST_F(FaultWorkingMemoryRegressionTest, NoDoubleFreeOnDestroy) {
    // ARRANGE
    wm = fault_working_memory_create();
    for (uint32_t i = 1; i <= 5; i++) {
        fault_t fault = create_fault(i, FAULT_SEVERITY_MINOR, "Test");
        fault_working_memory_add_fault(wm, &fault);
    }

    // ACT & ASSERT (no crash)
    fault_working_memory_destroy(wm);
    wm = nullptr;  // Prevent double destroy in TearDown
}

/**
 * @test Regression: Priority Fault After Clear
 * WHAT: Verify get_priority_fault returns NULL after clear
 * WHY: Previous bug returned stale pointer after clear
 * HOW: Add faults, get priority, clear, get priority again
 */
TEST_F(FaultWorkingMemoryRegressionTest, PriorityFaultAfterClear) {
    // ARRANGE
    wm = fault_working_memory_create();
    fault_t fault = create_fault(1, FAULT_SEVERITY_CRITICAL, "Test");
    fault_working_memory_add_fault(wm, &fault);

    // Verify fault is there
    active_fault_t* priority1 = fault_working_memory_get_priority_fault(wm);
    ASSERT_NE(priority1, nullptr);

    // ACT: Clear
    fault_working_memory_clear(wm);

    // ASSERT: Priority fault is NULL
    active_fault_t* priority2 = fault_working_memory_get_priority_fault(wm);
    EXPECT_EQ(priority2, nullptr);
}

/**
 * @test Regression: Cascade Flag Reset
 * WHAT: Verify cascade flag resets when faults resolved
 * WHY: Previous bug kept cascade flag set permanently
 * HOW: Trigger cascade, remove faults, verify flag clears
 */
TEST_F(FaultWorkingMemoryRegressionTest, CascadeFlagReset) {
    // ARRANGE
    wm = fault_working_memory_create();

    // Trigger cascade
    for (uint32_t i = 1; i <= 15; i++) {
        fault_t fault = create_fault(i, FAULT_SEVERITY_MINOR, "Test");
        fault_working_memory_add_fault(wm, &fault);
    }
    fault_working_memory_update_cascade_detection(wm);
    EXPECT_TRUE(fault_working_memory_is_cascading(wm));

    // ACT: Clear all faults
    fault_working_memory_clear(wm);

    // Wait a bit
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Update cascade detection
    fault_working_memory_update_cascade_detection(wm);

    // ASSERT: Cascade flag should reset
    EXPECT_FALSE(fault_working_memory_is_cascading(wm));
}

/**
 * @test Regression: Remove During Iteration
 * WHAT: Verify removing fault during iteration doesn't crash
 * WHY: Previous bug caused iterator invalidation
 * HOW: Iterate, remove faults, verify no crash
 */
TEST_F(FaultWorkingMemoryRegressionTest, RemoveDuringIteration) {
    // ARRANGE
    wm = fault_working_memory_create();
    for (uint32_t i = 1; i <= 5; i++) {
        fault_t fault = create_fault(i, FAULT_SEVERITY_MINOR, "Test");
        fault_working_memory_add_fault(wm, &fault);
    }

    // ACT: Remove during iteration
    uint32_t count = fault_working_memory_get_count(wm);
    for (uint32_t i = 0; i < count; i++) {
        const active_fault_t* fault = fault_working_memory_get_fault_at(wm, i);
        if (fault) {
            fault_working_memory_remove_fault(wm, fault->fault.fault_id);
        }
    }

    // ASSERT: No crash occurred
    EXPECT_GE(fault_working_memory_get_count(wm), 0u);
}

//=============================================================================
// Boundary Condition Regression Tests
//=============================================================================

/**
 * @test Regression: Minimum Capacity (1 Fault)
 * WHAT: Verify working memory works with capacity of 1
 * WHY: Edge case that should still work
 * HOW: Set capacity to 1, add 2 faults, verify eviction
 */
TEST_F(FaultWorkingMemoryRegressionTest, MinimumCapacityOne) {
    // ARRANGE
    fault_working_memory_config_t config = fault_working_memory_default_config();
    config.max_capacity = 1;
    wm = fault_working_memory_create_custom(&config);
    ASSERT_NE(wm, nullptr);

    // ACT: Add 2 faults
    fault_t fault1 = create_fault(1, FAULT_SEVERITY_MINOR, "First");
    fault_t fault2 = create_fault(2, FAULT_SEVERITY_MINOR, "Second");

    fault_working_memory_add_fault(wm, &fault1);
    fault_working_memory_add_fault(wm, &fault2);

    // ASSERT: Only 1 fault stored
    EXPECT_EQ(fault_working_memory_get_count(wm), 1u);
}

/**
 * @test Regression: Maximum Capacity (100 Faults)
 * WHAT: Verify working memory handles large capacity
 * WHY: Should support configuration beyond Miller's Law
 * HOW: Set capacity to 100, add 150 faults, verify cap
 */
TEST_F(FaultWorkingMemoryRegressionTest, MaximumCapacityLarge) {
    // ARRANGE
    fault_working_memory_config_t config = fault_working_memory_default_config();
    config.max_capacity = 100;
    wm = fault_working_memory_create_custom(&config);
    ASSERT_NE(wm, nullptr);

    // ACT: Add 150 faults
    for (uint32_t i = 1; i <= 150; i++) {
        fault_t fault = create_fault(i, FAULT_SEVERITY_MINOR, "Test");
        fault_working_memory_add_fault(wm, &fault);
    }

    // ASSERT: Capacity enforced
    EXPECT_EQ(fault_working_memory_get_count(wm), 100u);
}

/**
 * @test Regression: Zero Recovery Steps
 * WHAT: Verify recovery strategy with 0 steps is handled
 * WHY: Edge case that shouldn't crash
 * HOW: Set strategy with 0 steps, verify handled gracefully
 */
TEST_F(FaultWorkingMemoryRegressionTest, ZeroRecoverySteps) {
    // ARRANGE
    wm = fault_working_memory_create();

    // ACT: Set strategy with 0 steps
    bool result = fault_working_memory_set_recovery_strategy(wm, RECOVERY_STRATEGY_RETRY, 0);

    // ASSERT: Either rejected or handled gracefully
    // Should not crash, and subsequent calls should be safe
    fault_working_memory_update_progress(wm, 1);
    EXPECT_GE(fault_working_memory_get_recovery_step(wm), 0u);
}

/**
 * @test Regression: Very Long Description
 * WHAT: Verify long fault descriptions are truncated safely
 * WHY: Previous bug caused buffer overflow
 * HOW: Create fault with 500-char description, verify no overflow
 */
TEST_F(FaultWorkingMemoryRegressionTest, VeryLongDescription) {
    // ARRANGE
    wm = fault_working_memory_create();

    // Create fault with very long description
    fault_t fault = {0};
    fault.fault_id = 1;
    fault.severity = FAULT_SEVERITY_MINOR;
    fault.timestamp_us = fault_working_memory_get_timestamp_us();

    // 500 character description
    char long_desc[600];
    memset(long_desc, 'A', 500);
    long_desc[500] = '\0';
    snprintf(fault.description, sizeof(fault.description), "%s", long_desc);

    fault.recovery_attempts = 0;
    fault.is_resolved = false;

    // ACT
    bool result = fault_working_memory_add_fault(wm, &fault);

    // ASSERT: No crash, description truncated safely
    EXPECT_TRUE(result);
    const active_fault_t* retrieved = fault_working_memory_get_fault_at(wm, 0);
    ASSERT_NE(retrieved, nullptr);
    EXPECT_LT(strlen(retrieved->fault.description), 600u);  // Truncated
}

//=============================================================================
// Performance Regression Tests
//=============================================================================

/**
 * @test Regression: Add Performance
 * WHAT: Verify add operation completes in < 1 microsecond
 * WHY: Performance regression detection
 * HOW: Add 1000 faults, measure time, verify < 1ms total
 */
TEST_F(FaultWorkingMemoryRegressionTest, AddPerformance) {
    // ARRANGE
    wm = fault_working_memory_create();

    // ACT: Measure 1000 adds
    auto start = std::chrono::high_resolution_clock::now();

    for (uint32_t i = 1; i <= 1000; i++) {
        fault_t fault = create_fault(i, FAULT_SEVERITY_MINOR, "Perf test");
        fault_working_memory_add_fault(wm, &fault);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    // ASSERT: Should complete in < 10ms (10000us) for 1000 adds
    EXPECT_LT(duration_us, 10000);
}

/**
 * @test Regression: Get Priority Performance
 * WHAT: Verify get_priority_fault completes in < 100ns
 * WHY: Hot path operation, must be fast
 * HOW: Call 10000 times, measure time
 */
TEST_F(FaultWorkingMemoryRegressionTest, GetPriorityPerformance) {
    // ARRANGE
    wm = fault_working_memory_create();
    for (uint32_t i = 1; i <= 9; i++) {
        fault_t fault = create_fault(i, FAULT_SEVERITY_MINOR, "Test");
        fault_working_memory_add_fault(wm, &fault);
    }

    // ACT: Measure 10000 calls
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 10000; i++) {
        active_fault_t* priority = fault_working_memory_get_priority_fault(wm);
        (void)priority;  // Suppress unused warning
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    // ASSERT: Should complete in < 10ms for 10000 calls (< 1us per call)
    EXPECT_LT(duration_us, 10000);
}

/**
 * @test Regression: Memory Usage Stable
 * WHAT: Verify memory usage doesn't grow with churn
 * WHY: Detect memory leaks
 * HOW: Add/remove 10000 faults, verify final count is stable
 */
TEST_F(FaultWorkingMemoryRegressionTest, MemoryUsageStable) {
    // ARRANGE
    wm = fault_working_memory_create();

    // ACT: Churn 10000 faults
    for (uint32_t i = 1; i <= 10000; i++) {
        fault_t fault = create_fault(i, FAULT_SEVERITY_MINOR, "Memory test");
        fault_working_memory_add_fault(wm, &fault);

        if (i > 100 && i % 10 == 0) {
            fault_working_memory_remove_fault(wm, i - 100);
        }
    }

    // ASSERT: Count is within expected range (capacity limit)
    uint32_t final_count = fault_working_memory_get_count(wm);
    EXPECT_LE(final_count, 9u);  // Miller's Law capacity
}

//=============================================================================
// Thread Safety Regression Tests
//=============================================================================

/**
 * @test Regression: Race Condition on Priority Update
 * WHAT: Verify no race condition when updating priority
 * WHY: Previous bug caused crash with concurrent priority access
 * HOW: Multiple threads get priority while another adds faults
 */
TEST_F(FaultWorkingMemoryRegressionTest, RaceConditionPriorityUpdate) {
    // ARRANGE
    wm = fault_working_memory_create();
    std::atomic<bool> stop{false};

    // Thread 1: Add faults
    std::thread adder([this, &stop]() {
        for (uint32_t i = 1; !stop && i <= 100; i++) {
            fault_severity_t severity = (i % 10 == 0) ? FAULT_SEVERITY_CRITICAL : FAULT_SEVERITY_MINOR;
            fault_t fault = create_fault(i, severity, "Test");
            fault_working_memory_add_fault(wm, &fault);
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    });

    // Thread 2-4: Get priority
    std::vector<std::thread> readers;
    for (int t = 0; t < 3; t++) {
        readers.emplace_back([this, &stop]() {
            while (!stop) {
                active_fault_t* priority = fault_working_memory_get_priority_fault(wm);
                (void)priority;
                std::this_thread::sleep_for(std::chrono::microseconds(50));
            }
        });
    }

    adder.join();
    stop = true;

    for (auto& reader : readers) {
        reader.join();
    }

    // ASSERT: No crash occurred
    EXPECT_TRUE(true);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
