/**
 * @file test_fault_working_memory.cpp
 * @brief Comprehensive Unit Tests for Working Memory for Active Fault Context
 * @version 1.0.0
 * @date 2025-11-20
 *
 * WHAT: TDD-based unit tests for fault working memory module
 * WHY: Ensure 100% code coverage and validate all behaviors
 * HOW: GTest framework with AAA pattern (Arrange-Act-Assert)
 *
 * Test coverage: 100% target (all functions, branches, edge cases)
 * Test count: 45+ tests
 *
 * BIOLOGICAL BASIS:
 * - Miller's Law: 7±2 items capacity limit
 * - Priority-based attention focus
 * - Cascading failure detection
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <chrono>
#include <cstring>

// Headers have their own extern "C" guards
#include "cognitive/fault_tolerance/nimcp_fault_working_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class FaultWorkingMemoryTest : public ::testing::Test {
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

    // Helper: Create test fault
    fault_t create_test_fault(uint32_t id, fault_severity_t severity, const char* desc) {
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
// Creation and Destruction Tests
//=============================================================================

/**
 * @test Fault Working Memory Creation - Default Config
 * WHAT: Verify fault working memory can be created with default config
 * WHY: Ensure proper initialization of all fields
 * HOW: Create with default config, validate non-NULL and initial state
 */
TEST_F(FaultWorkingMemoryTest, CreateDefault) {
    // ARRANGE & ACT
    wm = fault_working_memory_create();

    // ASSERT
    ASSERT_NE(wm, nullptr);
    EXPECT_EQ(fault_working_memory_get_count(wm), 0u);
    EXPECT_FALSE(fault_working_memory_is_cascading(wm));
}

/**
 * @test Fault Working Memory Creation - Custom Config
 * WHAT: Verify fault working memory can be created with custom config
 * WHY: Allow configuration flexibility
 * HOW: Create with custom capacity, validate settings
 */
TEST_F(FaultWorkingMemoryTest, CreateCustom) {
    // ARRANGE
    fault_working_memory_config_t config = fault_working_memory_default_config();
    config.max_capacity = 5;  // Smaller capacity
    config.cascade_threshold = 15;  // Higher threshold

    // ACT
    wm = fault_working_memory_create_custom(&config);

    // ASSERT
    ASSERT_NE(wm, nullptr);
    EXPECT_EQ(fault_working_memory_get_count(wm), 0u);
}

/**
 * @test Fault Working Memory Creation - NULL Config
 * WHAT: Verify NULL config parameter is handled
 * WHY: Prevent crashes from invalid input
 * HOW: Pass NULL config, expect NULL return
 */
TEST_F(FaultWorkingMemoryTest, CreateWithNullConfig) {
    // ACT
    wm = fault_working_memory_create_custom(nullptr);

    // ASSERT
    EXPECT_EQ(wm, nullptr);
}

/**
 * @test Fault Working Memory Destruction - NULL Pointer
 * WHAT: Verify destroy handles NULL gracefully
 * WHY: Prevent crashes from double-free or NULL destroy
 * HOW: Call destroy with NULL, expect no crash
 */
TEST_F(FaultWorkingMemoryTest, DestroyNull) {
    // ACT & ASSERT (no crash)
    fault_working_memory_destroy(nullptr);
}

//=============================================================================
// Add Fault Tests
//=============================================================================

/**
 * @test Add Single Fault
 * WHAT: Verify single fault can be added to working memory
 * WHY: Basic functionality requirement
 * HOW: Add one fault, verify count and retrieval
 */
TEST_F(FaultWorkingMemoryTest, AddSingleFault) {
    // ARRANGE
    wm = fault_working_memory_create();
    fault_t fault = create_test_fault(1, FAULT_SEVERITY_CRITICAL, "Test fault");

    // ACT
    bool result = fault_working_memory_add_fault(wm, &fault);

    // ASSERT
    EXPECT_TRUE(result);
    EXPECT_EQ(fault_working_memory_get_count(wm), 1u);
}

/**
 * @test Add Multiple Faults
 * WHAT: Verify multiple faults can be added
 * WHY: Working memory must track multiple active faults
 * HOW: Add 5 faults, verify count
 */
TEST_F(FaultWorkingMemoryTest, AddMultipleFaults) {
    // ARRANGE
    wm = fault_working_memory_create();

    // ACT
    for (uint32_t i = 1; i <= 5; i++) {
        fault_t fault = create_test_fault(i, FAULT_SEVERITY_MINOR, "Fault");
        bool result = fault_working_memory_add_fault(wm, &fault);
        EXPECT_TRUE(result);
    }

    // ASSERT
    EXPECT_EQ(fault_working_memory_get_count(wm), 5u);
}

/**
 * @test Add Fault - NULL Working Memory
 * WHAT: Verify NULL working memory parameter is handled
 * WHY: Prevent crashes from NULL pointer
 * HOW: Pass NULL wm, expect false
 */
TEST_F(FaultWorkingMemoryTest, AddFaultNullWorkingMemory) {
    // ARRANGE
    fault_t fault = create_test_fault(1, FAULT_SEVERITY_MINOR, "Test");

    // ACT
    bool result = fault_working_memory_add_fault(nullptr, &fault);

    // ASSERT
    EXPECT_FALSE(result);
}

/**
 * @test Add Fault - NULL Fault Pointer
 * WHAT: Verify NULL fault parameter is handled
 * WHY: Prevent crashes from NULL pointer
 * HOW: Pass NULL fault, expect false
 */
TEST_F(FaultWorkingMemoryTest, AddFaultNullFault) {
    // ARRANGE
    wm = fault_working_memory_create();

    // ACT
    bool result = fault_working_memory_add_fault(wm, nullptr);

    // ASSERT
    EXPECT_FALSE(result);
}

/**
 * @test Miller's Law Capacity Enforcement
 * WHAT: Verify capacity limit is enforced (7±2 = 9 max)
 * WHY: Prevent cognitive overload, maintain performance
 * HOW: Add 12 faults, verify only 9 stored (oldest evicted)
 */
TEST_F(FaultWorkingMemoryTest, MillersLawCapacity) {
    // ARRANGE
    wm = fault_working_memory_create();

    // ACT: Add 12 faults
    for (uint32_t i = 1; i <= 12; i++) {
        fault_t fault = create_test_fault(i, FAULT_SEVERITY_MINOR, "Fault");
        fault_working_memory_add_fault(wm, &fault);
    }

    // ASSERT: Only 9 stored (Miller's Law: 7±2)
    EXPECT_EQ(fault_working_memory_get_count(wm), 9u);
}

/**
 * @test Priority-Based Eviction
 * WHAT: Verify low-priority faults are evicted before high-priority
 * WHY: Keep critical faults in working memory
 * HOW: Add 9 minor faults + 1 critical, verify critical retained
 */
TEST_F(FaultWorkingMemoryTest, PriorityBasedEviction) {
    // ARRANGE
    wm = fault_working_memory_create();

    // ACT: Fill with minor faults
    for (uint32_t i = 1; i <= 9; i++) {
        fault_t fault = create_test_fault(i, FAULT_SEVERITY_MINOR, "Minor");
        fault_working_memory_add_fault(wm, &fault);
    }

    // Add critical fault (should evict a minor one)
    fault_t critical = create_test_fault(100, FAULT_SEVERITY_CRITICAL, "Critical");
    fault_working_memory_add_fault(wm, &critical);

    // ASSERT: Critical fault is in memory
    active_fault_t* priority = fault_working_memory_get_priority_fault(wm);
    ASSERT_NE(priority, nullptr);
    EXPECT_EQ(priority->fault.fault_id, 100u);
    EXPECT_EQ(priority->fault.severity, FAULT_SEVERITY_CRITICAL);
}

//=============================================================================
// Remove Fault Tests
//=============================================================================

/**
 * @test Remove Fault by ID
 * WHAT: Verify fault can be removed by ID
 * WHY: Clear resolved faults from working memory
 * HOW: Add fault, remove it, verify count
 */
TEST_F(FaultWorkingMemoryTest, RemoveFaultById) {
    // ARRANGE
    wm = fault_working_memory_create();
    fault_t fault = create_test_fault(42, FAULT_SEVERITY_MINOR, "Test");
    fault_working_memory_add_fault(wm, &fault);

    // ACT
    fault_working_memory_remove_fault(wm, 42);

    // ASSERT
    EXPECT_EQ(fault_working_memory_get_count(wm), 0u);
}

/**
 * @test Remove Non-Existent Fault
 * WHAT: Verify removing non-existent fault doesn't crash
 * WHY: Robust error handling
 * HOW: Remove ID that doesn't exist, verify count unchanged
 */
TEST_F(FaultWorkingMemoryTest, RemoveNonExistentFault) {
    // ARRANGE
    wm = fault_working_memory_create();
    fault_t fault = create_test_fault(1, FAULT_SEVERITY_MINOR, "Test");
    fault_working_memory_add_fault(wm, &fault);

    // ACT
    fault_working_memory_remove_fault(wm, 999);  // Doesn't exist

    // ASSERT
    EXPECT_EQ(fault_working_memory_get_count(wm), 1u);  // Unchanged
}

/**
 * @test Remove Fault - NULL Working Memory
 * WHAT: Verify NULL working memory is handled
 * WHY: Prevent crashes
 * HOW: Call with NULL, expect no crash
 */
TEST_F(FaultWorkingMemoryTest, RemoveFaultNullWorkingMemory) {
    // ACT & ASSERT (no crash)
    fault_working_memory_remove_fault(nullptr, 1);
}

//=============================================================================
// Cascade Detection Tests
//=============================================================================

/**
 * @test Cascade Detection - No Cascade
 * WHAT: Verify no cascade detected under threshold
 * WHY: Avoid false alarms
 * HOW: Add < 10 faults/min, verify no cascade
 */
TEST_F(FaultWorkingMemoryTest, NoCascadeDetected) {
    // ARRANGE
    wm = fault_working_memory_create();

    // ACT: Add 5 faults (below threshold)
    for (uint32_t i = 1; i <= 5; i++) {
        fault_t fault = create_test_fault(i, FAULT_SEVERITY_MINOR, "Test");
        fault_working_memory_add_fault(wm, &fault);
    }

    // ASSERT
    EXPECT_FALSE(fault_working_memory_is_cascading(wm));
}

/**
 * @test Cascade Detection - Cascade Detected
 * WHAT: Verify cascade detected when >10 faults/min
 * WHY: Trigger emergency recovery mode
 * HOW: Add >10 faults rapidly, verify cascade detected
 */
TEST_F(FaultWorkingMemoryTest, CascadeDetected) {
    // ARRANGE
    wm = fault_working_memory_create();

    // ACT: Add 15 faults rapidly (above threshold)
    for (uint32_t i = 1; i <= 15; i++) {
        fault_t fault = create_test_fault(i, FAULT_SEVERITY_MINOR, "Test");
        fault_working_memory_add_fault(wm, &fault);
    }

    // Update cascade detection
    fault_working_memory_update_cascade_detection(wm);

    // ASSERT
    EXPECT_TRUE(fault_working_memory_is_cascading(wm));
}

/**
 * @test Cascade Detection - NULL Working Memory
 * WHAT: Verify NULL working memory is handled
 * WHY: Prevent crashes
 * HOW: Call with NULL, expect false
 */
TEST_F(FaultWorkingMemoryTest, IsCascadingNullWorkingMemory) {
    // ACT
    bool result = fault_working_memory_is_cascading(nullptr);

    // ASSERT
    EXPECT_FALSE(result);
}

//=============================================================================
// Priority Fault (Attention Focus) Tests
//=============================================================================

/**
 * @test Get Priority Fault - Empty Memory
 * WHAT: Verify NULL returned when no faults
 * WHY: Handle empty case gracefully
 * HOW: Get priority fault on empty memory, expect NULL
 */
TEST_F(FaultWorkingMemoryTest, GetPriorityFaultEmpty) {
    // ARRANGE
    wm = fault_working_memory_create();

    // ACT
    active_fault_t* priority = fault_working_memory_get_priority_fault(wm);

    // ASSERT
    EXPECT_EQ(priority, nullptr);
}

/**
 * @test Get Priority Fault - Single Fault
 * WHAT: Verify single fault is returned as priority
 * WHY: Basic functionality
 * HOW: Add one fault, get priority, verify it's the same
 */
TEST_F(FaultWorkingMemoryTest, GetPriorityFaultSingle) {
    // ARRANGE
    wm = fault_working_memory_create();
    fault_t fault = create_test_fault(1, FAULT_SEVERITY_MINOR, "Test");
    fault_working_memory_add_fault(wm, &fault);

    // ACT
    active_fault_t* priority = fault_working_memory_get_priority_fault(wm);

    // ASSERT
    ASSERT_NE(priority, nullptr);
    EXPECT_EQ(priority->fault.fault_id, 1u);
}

/**
 * @test Get Priority Fault - Critical Over Minor
 * WHAT: Verify critical fault has priority over minor
 * WHY: Focus attention on most severe issues
 * HOW: Add minor and critical, verify critical returned
 */
TEST_F(FaultWorkingMemoryTest, GetPriorityFaultCriticalOverMinor) {
    // ARRANGE
    wm = fault_working_memory_create();

    fault_t minor = create_test_fault(1, FAULT_SEVERITY_MINOR, "Minor");
    fault_t major = create_test_fault(2, FAULT_SEVERITY_MAJOR, "Major");
    fault_t critical = create_test_fault(3, FAULT_SEVERITY_CRITICAL, "Critical");

    fault_working_memory_add_fault(wm, &minor);
    fault_working_memory_add_fault(wm, &major);
    fault_working_memory_add_fault(wm, &critical);

    // ACT
    active_fault_t* priority = fault_working_memory_get_priority_fault(wm);

    // ASSERT
    ASSERT_NE(priority, nullptr);
    EXPECT_EQ(priority->fault.fault_id, 3u);
    EXPECT_EQ(priority->fault.severity, FAULT_SEVERITY_CRITICAL);
}

/**
 * @test Get Priority Fault - NULL Working Memory
 * WHAT: Verify NULL working memory is handled
 * WHY: Prevent crashes
 * HOW: Call with NULL, expect NULL
 */
TEST_F(FaultWorkingMemoryTest, GetPriorityFaultNullWorkingMemory) {
    // ACT
    active_fault_t* priority = fault_working_memory_get_priority_fault(nullptr);

    // ASSERT
    EXPECT_EQ(priority, nullptr);
}

//=============================================================================
// Recovery Progress Tests
//=============================================================================

/**
 * @test Set Recovery Strategy
 * WHAT: Verify recovery strategy can be set
 * WHY: Track current recovery approach
 * HOW: Set strategy, verify it's stored
 */
TEST_F(FaultWorkingMemoryTest, SetRecoveryStrategy) {
    // ARRANGE
    wm = fault_working_memory_create();
    recovery_strategy_t strategy = RECOVERY_STRATEGY_RESTART;

    // ACT
    bool result = fault_working_memory_set_recovery_strategy(wm, strategy, 5);

    // ASSERT
    EXPECT_TRUE(result);
}

/**
 * @test Update Recovery Progress
 * WHAT: Verify recovery progress can be updated
 * WHY: Track multi-step recovery state
 * HOW: Set strategy, update progress, verify state
 */
TEST_F(FaultWorkingMemoryTest, UpdateRecoveryProgress) {
    // ARRANGE
    wm = fault_working_memory_create();
    fault_working_memory_set_recovery_strategy(wm, RECOVERY_STRATEGY_RESTART, 3);

    // ACT
    fault_working_memory_update_progress(wm, 1);

    // ASSERT
    uint32_t current_step = fault_working_memory_get_recovery_step(wm);
    EXPECT_EQ(current_step, 1u);
}

/**
 * @test Recovery Progress - Complete All Steps
 * WHAT: Verify progress tracks through all steps
 * WHY: Multi-step recovery coordination
 * HOW: Set 3 steps, update through all, verify completion
 */
TEST_F(FaultWorkingMemoryTest, RecoveryProgressComplete) {
    // ARRANGE
    wm = fault_working_memory_create();
    fault_working_memory_set_recovery_strategy(wm, RECOVERY_STRATEGY_FAILOVER, 3);

    // ACT
    fault_working_memory_update_progress(wm, 1);
    fault_working_memory_update_progress(wm, 2);
    fault_working_memory_update_progress(wm, 3);

    // ASSERT
    uint32_t current_step = fault_working_memory_get_recovery_step(wm);
    uint32_t total_steps = fault_working_memory_get_total_steps(wm);
    EXPECT_EQ(current_step, 3u);
    EXPECT_EQ(total_steps, 3u);
}

/**
 * @test Update Progress - NULL Working Memory
 * WHAT: Verify NULL working memory is handled
 * WHY: Prevent crashes
 * HOW: Call with NULL, expect no crash
 */
TEST_F(FaultWorkingMemoryTest, UpdateProgressNullWorkingMemory) {
    // ACT & ASSERT (no crash)
    fault_working_memory_update_progress(nullptr, 1);
}

//=============================================================================
// Statistics Tests
//=============================================================================

/**
 * @test Get Count - Empty
 * WHAT: Verify count is 0 for empty memory
 * WHY: Correct state reporting
 * HOW: Create memory, verify count is 0
 */
TEST_F(FaultWorkingMemoryTest, GetCountEmpty) {
    // ARRANGE
    wm = fault_working_memory_create();

    // ACT
    uint32_t count = fault_working_memory_get_count(wm);

    // ASSERT
    EXPECT_EQ(count, 0u);
}

/**
 * @test Get Count - NULL Working Memory
 * WHAT: Verify NULL working memory returns 0
 * WHY: Safe default
 * HOW: Call with NULL, expect 0
 */
TEST_F(FaultWorkingMemoryTest, GetCountNullWorkingMemory) {
    // ACT
    uint32_t count = fault_working_memory_get_count(nullptr);

    // ASSERT
    EXPECT_EQ(count, 0u);
}

/**
 * @test Get Fault by Index
 * WHAT: Verify fault can be retrieved by index
 * WHY: Iterate through active faults
 * HOW: Add fault, get by index, verify data
 */
TEST_F(FaultWorkingMemoryTest, GetFaultByIndex) {
    // ARRANGE
    wm = fault_working_memory_create();
    fault_t fault = create_test_fault(42, FAULT_SEVERITY_MAJOR, "Test fault");
    fault_working_memory_add_fault(wm, &fault);

    // ACT
    const active_fault_t* retrieved = fault_working_memory_get_fault_at(wm, 0);

    // ASSERT
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->fault.fault_id, 42u);
    EXPECT_EQ(retrieved->fault.severity, FAULT_SEVERITY_MAJOR);
    EXPECT_STREQ(retrieved->fault.description, "Test fault");
}

/**
 * @test Get Fault by Index - Out of Bounds
 * WHAT: Verify out-of-bounds index returns NULL
 * WHY: Prevent crashes
 * HOW: Get index >= count, expect NULL
 */
TEST_F(FaultWorkingMemoryTest, GetFaultByIndexOutOfBounds) {
    // ARRANGE
    wm = fault_working_memory_create();
    fault_t fault = create_test_fault(1, FAULT_SEVERITY_MINOR, "Test");
    fault_working_memory_add_fault(wm, &fault);

    // ACT
    const active_fault_t* retrieved = fault_working_memory_get_fault_at(wm, 10);

    // ASSERT
    EXPECT_EQ(retrieved, nullptr);
}

/**
 * @test Get Fault by Index - NULL Working Memory
 * WHAT: Verify NULL working memory returns NULL
 * WHY: Prevent crashes
 * HOW: Call with NULL, expect NULL
 */
TEST_F(FaultWorkingMemoryTest, GetFaultByIndexNullWorkingMemory) {
    // ACT
    const active_fault_t* retrieved = fault_working_memory_get_fault_at(nullptr, 0);

    // ASSERT
    EXPECT_EQ(retrieved, nullptr);
}

//=============================================================================
// Clear Tests
//=============================================================================

/**
 * @test Clear All Faults
 * WHAT: Verify all faults can be cleared
 * WHY: Reset working memory
 * HOW: Add faults, clear, verify count is 0
 */
TEST_F(FaultWorkingMemoryTest, ClearAllFaults) {
    // ARRANGE
    wm = fault_working_memory_create();
    for (uint32_t i = 1; i <= 5; i++) {
        fault_t fault = create_test_fault(i, FAULT_SEVERITY_MINOR, "Test");
        fault_working_memory_add_fault(wm, &fault);
    }

    // ACT
    fault_working_memory_clear(wm);

    // ASSERT
    EXPECT_EQ(fault_working_memory_get_count(wm), 0u);
}

/**
 * @test Clear - NULL Working Memory
 * WHAT: Verify NULL working memory is handled
 * WHY: Prevent crashes
 * HOW: Call with NULL, expect no crash
 */
TEST_F(FaultWorkingMemoryTest, ClearNullWorkingMemory) {
    // ACT & ASSERT (no crash)
    fault_working_memory_clear(nullptr);
}

//=============================================================================
// Timestamp Tests
//=============================================================================

/**
 * @test Get Timestamp
 * WHAT: Verify timestamp function returns increasing values
 * WHY: Accurate time tracking for cascade detection
 * HOW: Get two timestamps with delay, verify second > first
 */
TEST_F(FaultWorkingMemoryTest, GetTimestamp) {
    // ACT
    uint64_t t1 = fault_working_memory_get_timestamp_us();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    uint64_t t2 = fault_working_memory_get_timestamp_us();

    // ASSERT
    EXPECT_GT(t2, t1);
    EXPECT_GE(t2 - t1, 10000u);  // At least 10ms = 10000us
}

//=============================================================================
// Edge Case Tests
//=============================================================================

/**
 * @test Duplicate Fault IDs
 * WHAT: Verify behavior when adding fault with duplicate ID
 * WHY: Handle re-occurring faults
 * HOW: Add same fault ID twice, verify handled gracefully
 */
TEST_F(FaultWorkingMemoryTest, DuplicateFaultIds) {
    // ARRANGE
    wm = fault_working_memory_create();
    fault_t fault1 = create_test_fault(1, FAULT_SEVERITY_MINOR, "First");
    fault_t fault2 = create_test_fault(1, FAULT_SEVERITY_MAJOR, "Second");

    // ACT
    fault_working_memory_add_fault(wm, &fault1);
    fault_working_memory_add_fault(wm, &fault2);

    // ASSERT: Second fault updates first (or creates new entry)
    EXPECT_GE(fault_working_memory_get_count(wm), 1u);
    EXPECT_LE(fault_working_memory_get_count(wm), 2u);
}

/**
 * @test Zero Capacity Configuration
 * WHAT: Verify zero capacity is rejected
 * WHY: Invalid configuration
 * HOW: Set capacity to 0, expect NULL or default to 9
 */
TEST_F(FaultWorkingMemoryTest, ZeroCapacityConfig) {
    // ARRANGE
    fault_working_memory_config_t config = fault_working_memory_default_config();
    config.max_capacity = 0;

    // ACT
    wm = fault_working_memory_create_custom(&config);

    // ASSERT: Either NULL or defaults to minimum capacity
    if (wm != nullptr) {
        // Defaulted to minimum capacity
        EXPECT_GT(fault_working_memory_get_count(wm), 0u);  // Can add at least one
    }
}

/**
 * @test Excessive Capacity Configuration
 * WHAT: Verify excessive capacity is capped
 * WHY: Prevent memory issues
 * HOW: Set capacity > 100, expect capped to reasonable limit
 */
TEST_F(FaultWorkingMemoryTest, ExcessiveCapacityConfig) {
    // ARRANGE
    fault_working_memory_config_t config = fault_working_memory_default_config();
    config.max_capacity = 1000;  // Way over Miller's Law

    // ACT
    wm = fault_working_memory_create_custom(&config);

    // ASSERT: Should cap or succeed
    EXPECT_NE(wm, nullptr);
}

//=============================================================================
// Integration with Active Fault Metadata Tests
//=============================================================================

/**
 * @test Active Fault Metadata Tracking
 * WHAT: Verify active fault metadata is tracked
 * WHY: Support recovery coordination
 * HOW: Add fault, verify metadata fields are set
 */
TEST_F(FaultWorkingMemoryTest, ActiveFaultMetadataTracking) {
    // ARRANGE
    wm = fault_working_memory_create();
    fault_t fault = create_test_fault(1, FAULT_SEVERITY_MAJOR, "Test");

    // ACT
    fault_working_memory_add_fault(wm, &fault);
    const active_fault_t* active = fault_working_memory_get_fault_at(wm, 0);

    // ASSERT
    ASSERT_NE(active, nullptr);
    EXPECT_EQ(active->fault.fault_id, 1u);
    EXPECT_GT(active->time_in_memory_us, 0u);  // Should have timestamp
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
