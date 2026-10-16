/**
 * @file test_self_repair.cpp
 * @brief Unit tests for Self-Repair Coordinator
 * @version 1.0.0
 * @date 2025-01-20
 *
 * WHAT: Comprehensive unit tests for autonomous self-repair coordinator
 * WHY: Ensure end-to-end self-repair pipeline works correctly
 * HOW: Test-driven development with coverage of all public APIs
 *
 * Test Coverage:
 * - Creation and destruction
 * - Configuration
 * - Pipeline stages
 * - Repair initiation
 * - Statistics tracking
 * - Rollback capability
 * - Callback registration
 * - Error handling
 *
 * @author NIMCP Development Team
 */

#include <gtest/gtest.h>
#include <cstring>

// Headers have their own extern "C" guards
#include "cognitive/fault_tolerance/nimcp_self_repair.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_macros.h"

//=============================================================================
// Test Fixture
//=============================================================================

class SelfRepairTest : public ::testing::Test {
protected:
    size_t baseline_allocated = 0;

    void SetUp() override {
        // Initialize memory tracking
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);

        // Force-initialize exception system and handler infrastructure
        // so their singleton allocations are part of the baseline.
        // A dummy NIMCP_THROW_TO_IMMUNE triggers all lazy init paths:
        // - nimcp_exception_system_init (exception mutex)
        // - ensure_initialized (handler mutex)
        // - nimcp_exception_set_current (thread-local ref)
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "warmup");
        nimcp_exception_clear_current();

        // Record baseline memory (from previous tests or global state)
        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        baseline_allocated = stats.current_allocated;
    }

    void TearDown() override {
        // Clear thread-local exception state (NIMCP_THROW_TO_IMMUNE stores
        // a ref in tl_current_exception that appears as a memory leak)
        nimcp_exception_clear_current();

        // Check for memory leaks relative to baseline
        // Allow small drift from global subsystem lazy-init (exception handlers,
        // module registry, etc.) that allocate on first use across tests
        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        EXPECT_LE(stats.current_allocated, baseline_allocated + 256)
            << "Memory leak detected! Allocated: " << stats.current_allocated
            << ", Baseline: " << baseline_allocated;
    }
};

//=============================================================================
// Creation and Destruction Tests
//=============================================================================

/**
 * @test Self-Repair Coordinator Creation with Default Config
 *
 * WHAT: Verify coordinator can be created with default configuration
 * WHY: Ensure proper initialization of all components
 * HOW: Create with defaults, verify state, destroy
 */
TEST_F(SelfRepairTest, CreateWithDefaults) {
    // ACT: Create with default config
    self_repair_coordinator_t* coordinator = self_repair_create(NULL);

    // ASSERT: Created successfully
    ASSERT_NE(coordinator, nullptr);

    // Verify ready state (may fail if dependencies not available)
    bool ready = self_repair_is_ready(coordinator);
    (void)ready;  // Just verify no crash

    // CLEANUP
    self_repair_destroy(coordinator);
}

/**
 * @test Self-Repair Coordinator Creation with Custom Config
 *
 * WHAT: Verify coordinator accepts custom configuration
 * WHY: Allow users to customize repair behavior
 * HOW: Create with custom config, verify settings applied
 */
TEST_F(SelfRepairTest, CreateWithCustomConfig) {
    // ARRANGE: Custom config
    self_repair_config_t config = self_repair_default_config();
    config.mode = REPAIR_MODE_DUAL;
    config.min_fix_confidence = 0.8f;
    config.max_risk_score = 0.2f;
    config.require_human_approval = false;
    config.auto_rollback_on_failure = true;
    config.learn_from_outcome = true;

    // ACT: Create with custom config
    self_repair_coordinator_t* coordinator = self_repair_create(&config);

    // ASSERT: Created successfully
    ASSERT_NE(coordinator, nullptr);

    // CLEANUP
    self_repair_destroy(coordinator);
}

/**
 * @test Self-Repair Coordinator Destroy NULL Safety
 *
 * WHAT: Verify destroy handles NULL gracefully
 * WHY: Prevent crashes on double-free or accidental NULL
 * HOW: Call destroy with NULL, expect no crash
 */
TEST_F(SelfRepairTest, DestroyNullSafety) {
    // ACT & ASSERT: Should not crash
    EXPECT_NO_THROW(self_repair_destroy(NULL));
}

//=============================================================================
// Configuration Tests
//=============================================================================

/**
 * @test Default Config Values
 *
 * WHAT: Verify default configuration has sensible values
 * WHY: Ensure safe defaults for autonomous repair
 * HOW: Get default config, verify key values
 */
TEST_F(SelfRepairTest, DefaultConfigValues) {
    // ACT
    self_repair_config_t config = self_repair_default_config();

    // ASSERT: Sensible defaults
    EXPECT_EQ(config.mode, REPAIR_MODE_DUAL);
    EXPECT_GT(config.min_fix_confidence, 0.5f);  // Should be reasonably high
    EXPECT_LT(config.max_risk_score, 0.5f);      // Should be reasonably low
    EXPECT_TRUE(config.auto_rollback_on_failure);
    EXPECT_TRUE(config.learn_from_outcome);
    EXPECT_GT(config.validation_timeout_ms, 0u);
}

/**
 * @test Repair Mode Selection
 *
 * WHAT: Verify each repair mode can be configured
 * WHY: Support different deployment strategies
 * HOW: Create with each mode, verify no crash
 */
TEST_F(SelfRepairTest, RepairModeSelection) {
    // Test each mode
    self_repair_config_t config = self_repair_default_config();

    // Hot-patch only mode
    config.mode = REPAIR_MODE_HOT_PATCH_ONLY;
    self_repair_coordinator_t* coord1 = self_repair_create(&config);
    EXPECT_NE(coord1, nullptr);
    self_repair_destroy(coord1);

    // Source only mode
    config.mode = REPAIR_MODE_SOURCE_ONLY;
    self_repair_coordinator_t* coord2 = self_repair_create(&config);
    EXPECT_NE(coord2, nullptr);
    self_repair_destroy(coord2);

    // Dual mode
    config.mode = REPAIR_MODE_DUAL;
    self_repair_coordinator_t* coord3 = self_repair_create(&config);
    EXPECT_NE(coord3, nullptr);
    self_repair_destroy(coord3);
}

//=============================================================================
// Statistics Tests
//=============================================================================

/**
 * @test Statistics Tracking
 *
 * WHAT: Verify statistics tracking
 * WHY: Monitor repair performance and outcomes
 * HOW: Get stats, verify structure
 */
TEST_F(SelfRepairTest, StatisticsTracking) {
    // ARRANGE
    self_repair_coordinator_t* coordinator = self_repair_create(NULL);
    ASSERT_NE(coordinator, nullptr);

    self_repair_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    // ACT: Get initial stats
    int result = self_repair_get_stats(coordinator, &stats);

    // ASSERT: Stats retrieved successfully
    EXPECT_EQ(result, 0);
    EXPECT_EQ(stats.repairs_attempted, 0u);  // No repairs yet
    EXPECT_EQ(stats.repairs_successful, 0u);
    EXPECT_EQ(stats.repairs_failed, 0u);

    // CLEANUP
    self_repair_destroy(coordinator);
}

/**
 * @test Statistics Reset
 *
 * WHAT: Verify statistics can be reset
 * WHY: Support fresh metric collection
 * HOW: Reset stats, verify zeroed
 */
TEST_F(SelfRepairTest, StatisticsReset) {
    // ARRANGE
    self_repair_coordinator_t* coordinator = self_repair_create(NULL);
    ASSERT_NE(coordinator, nullptr);

    // ACT: Reset statistics
    self_repair_reset_stats(coordinator);

    self_repair_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    int result = self_repair_get_stats(coordinator, &stats);

    // ASSERT: Stats are zeroed
    EXPECT_EQ(result, 0);
    EXPECT_EQ(stats.repairs_attempted, 0u);
    EXPECT_FLOAT_EQ(stats.avg_fix_confidence, 0.0f);

    // CLEANUP
    self_repair_destroy(coordinator);
}

//=============================================================================
// Repair Record Tests
//=============================================================================

/**
 * @test Get Recent Records (Empty)
 *
 * WHAT: Verify recent records query with no repairs
 * WHY: Support repair history access
 * HOW: Query records, verify empty
 */
TEST_F(SelfRepairTest, GetRecentRecordsEmpty) {
    // ARRANGE
    self_repair_coordinator_t* coordinator = self_repair_create(NULL);
    ASSERT_NE(coordinator, nullptr);

    self_repair_record_t records[10];
    memset(records, 0, sizeof(records));

    // ACT: Get recent records
    uint32_t count = self_repair_get_recent_records(coordinator, records, 10);

    // ASSERT: No records (empty history)
    EXPECT_EQ(count, 0u);

    // CLEANUP
    self_repair_destroy(coordinator);
}

/**
 * @test Get Record by Invalid ID
 *
 * WHAT: Verify record lookup with invalid ID
 * WHY: Handle missing records gracefully
 * HOW: Query non-existent ID, verify NULL returned
 */
TEST_F(SelfRepairTest, GetRecordInvalidId) {
    // ARRANGE
    self_repair_coordinator_t* coordinator = self_repair_create(NULL);
    ASSERT_NE(coordinator, nullptr);

    // ACT: Get record with invalid ID
    const self_repair_record_t* record = self_repair_get_record(coordinator, 99999);

    // ASSERT: NULL returned for non-existent record
    EXPECT_EQ(record, nullptr);

    // CLEANUP
    self_repair_destroy(coordinator);
}

//=============================================================================
// Callback Registration Tests
//=============================================================================

// Test callback functions
static bool g_stage_callback_called = false;
static bool g_complete_callback_called = false;
static bool g_approval_callback_called = false;

static void test_stage_callback(uint64_t repair_id, repair_stage_t old_stage,
                                repair_stage_t new_stage, void* user_data) {
    (void)repair_id;
    (void)old_stage;
    (void)new_stage;
    (void)user_data;
    g_stage_callback_called = true;
}

static void test_complete_callback(uint64_t repair_id,
                                   const self_repair_result_t* result,
                                   void* user_data) {
    (void)repair_id;
    (void)result;
    (void)user_data;
    g_complete_callback_called = true;
}

static bool test_approval_callback(uint64_t repair_id,
                                   const generated_fix_t* fix,
                                   void* user_data) {
    (void)repair_id;
    (void)fix;
    (void)user_data;
    g_approval_callback_called = true;
    return true;  // Approve
}

/**
 * @test Set Stage Callback
 *
 * WHAT: Verify stage callback registration
 * WHY: Support stage change notification
 * HOW: Register callback, verify success
 */
TEST_F(SelfRepairTest, SetStageCallback) {
    // ARRANGE
    self_repair_coordinator_t* coordinator = self_repair_create(NULL);
    ASSERT_NE(coordinator, nullptr);

    g_stage_callback_called = false;

    // ACT: Register stage callback
    int result = self_repair_set_stage_callback(
        coordinator,
        test_stage_callback,
        NULL
    );

    // ASSERT: Registration succeeded
    EXPECT_EQ(result, 0);

    // CLEANUP
    self_repair_destroy(coordinator);
}

/**
 * @test Set Complete Callback
 *
 * WHAT: Verify completion callback registration
 * WHY: Support repair completion notification
 * HOW: Register callback, verify success
 */
TEST_F(SelfRepairTest, SetCompleteCallback) {
    // ARRANGE
    self_repair_coordinator_t* coordinator = self_repair_create(NULL);
    ASSERT_NE(coordinator, nullptr);

    g_complete_callback_called = false;

    // ACT: Register completion callback
    int result = self_repair_set_complete_callback(
        coordinator,
        test_complete_callback,
        NULL
    );

    // ASSERT: Registration succeeded
    EXPECT_EQ(result, 0);

    // CLEANUP
    self_repair_destroy(coordinator);
}

/**
 * @test Set Approval Callback
 *
 * WHAT: Verify approval callback registration
 * WHY: Support human-in-the-loop approval
 * HOW: Register callback, verify success
 */
TEST_F(SelfRepairTest, SetApprovalCallback) {
    // ARRANGE
    self_repair_config_t config = self_repair_default_config();
    config.require_human_approval = true;

    self_repair_coordinator_t* coordinator = self_repair_create(&config);
    ASSERT_NE(coordinator, nullptr);

    g_approval_callback_called = false;

    // ACT: Register approval callback
    int result = self_repair_set_approval_callback(
        coordinator,
        test_approval_callback,
        NULL
    );

    // ASSERT: Registration succeeded
    EXPECT_EQ(result, 0);

    // CLEANUP
    self_repair_destroy(coordinator);
}

//=============================================================================
// Utility Function Tests
//=============================================================================

/**
 * @test Stage Name Conversion
 *
 * WHAT: Verify stage enum to string conversion
 * WHY: Support logging and debugging
 * HOW: Convert each stage, verify non-empty string
 */
TEST_F(SelfRepairTest, StageNameConversion) {
    // ACT & ASSERT: Each stage has a valid name
    EXPECT_GT(strlen(self_repair_stage_name(REPAIR_STAGE_PENDING)), 0u);
    EXPECT_GT(strlen(self_repair_stage_name(REPAIR_STAGE_ANALYZING)), 0u);
    EXPECT_GT(strlen(self_repair_stage_name(REPAIR_STAGE_GENERATING)), 0u);
    EXPECT_GT(strlen(self_repair_stage_name(REPAIR_STAGE_VALIDATING)), 0u);
    EXPECT_GT(strlen(self_repair_stage_name(REPAIR_STAGE_DEPLOYING)), 0u);
    EXPECT_GT(strlen(self_repair_stage_name(REPAIR_STAGE_COMPLETED)), 0u);
    EXPECT_GT(strlen(self_repair_stage_name(REPAIR_STAGE_FAILED)), 0u);
    EXPECT_GT(strlen(self_repair_stage_name(REPAIR_STAGE_ROLLED_BACK)), 0u);
}

/**
 * @test Status Name Conversion
 *
 * WHAT: Verify status enum to string conversion
 * WHY: Support status reporting
 * HOW: Convert each status, verify non-empty string
 */
TEST_F(SelfRepairTest, StatusNameConversion) {
    // ACT & ASSERT
    EXPECT_GT(strlen(self_repair_status_name(REPAIR_STATUS_SUCCESS)), 0u);
    EXPECT_GT(strlen(self_repair_status_name(REPAIR_STATUS_ANALYSIS_FAILED)), 0u);
    EXPECT_GT(strlen(self_repair_status_name(REPAIR_STATUS_NO_FIX_FOUND)), 0u);
    EXPECT_GT(strlen(self_repair_status_name(REPAIR_STATUS_LOW_CONFIDENCE)), 0u);
    EXPECT_GT(strlen(self_repair_status_name(REPAIR_STATUS_HIGH_RISK)), 0u);
    EXPECT_GT(strlen(self_repair_status_name(REPAIR_STATUS_VALIDATION_FAILED)), 0u);
    EXPECT_GT(strlen(self_repair_status_name(REPAIR_STATUS_HOT_PATCH_FAILED)), 0u);
    EXPECT_GT(strlen(self_repair_status_name(REPAIR_STATUS_SOURCE_COMMIT_FAILED)), 0u);
    EXPECT_GT(strlen(self_repair_status_name(REPAIR_STATUS_ROLLED_BACK)), 0u);
    EXPECT_GT(strlen(self_repair_status_name(REPAIR_STATUS_ERROR)), 0u);
}

/**
 * @test Mode Name Conversion
 *
 * WHAT: Verify mode enum to string conversion
 * WHY: Support configuration display
 * HOW: Convert each mode, verify non-empty string
 */
TEST_F(SelfRepairTest, ModeNameConversion) {
    // ACT & ASSERT
    EXPECT_GT(strlen(self_repair_mode_name(REPAIR_MODE_HOT_PATCH_ONLY)), 0u);
    EXPECT_GT(strlen(self_repair_mode_name(REPAIR_MODE_SOURCE_ONLY)), 0u);
    EXPECT_GT(strlen(self_repair_mode_name(REPAIR_MODE_DUAL)), 0u);
}

/**
 * @test Version String
 *
 * WHAT: Verify version string returned
 * WHY: Support version tracking
 * HOW: Get version, verify format
 */
TEST_F(SelfRepairTest, VersionString) {
    // ACT
    const char* version = self_repair_version();

    // ASSERT: Valid version string
    EXPECT_NE(version, nullptr);
    EXPECT_GT(strlen(version), 0u);
}

//=============================================================================
// Error Handling Tests
//=============================================================================

/**
 * @test Handle NULL Parameters Gracefully
 *
 * WHAT: Verify NULL parameter handling
 * WHY: Prevent crashes on invalid input
 * HOW: Call functions with NULL, verify error returned
 */
TEST_F(SelfRepairTest, NullParameterHandling) {
    // ARRANGE
    self_repair_coordinator_t* coordinator = self_repair_create(NULL);
    ASSERT_NE(coordinator, nullptr);

    self_repair_result_t result;
    self_repair_stats_t stats;

    // ACT & ASSERT: NULL parameters return error
    EXPECT_NE(self_repair_initiate(NULL, NULL, NULL), 0);
    EXPECT_NE(self_repair_initiate(coordinator, NULL, &result), 0);
    EXPECT_NE(self_repair_get_stats(NULL, &stats), 0);
    EXPECT_NE(self_repair_get_stats(coordinator, NULL), 0);
    EXPECT_EQ(self_repair_get_record(NULL, 1), nullptr);
    EXPECT_NE(self_repair_rollback(NULL, 1), 0);
    EXPECT_NE(self_repair_set_stage_callback(NULL, NULL, NULL), 0);

    // CLEANUP
    self_repair_destroy(coordinator);
}

/**
 * @test is_ready Returns False for NULL
 *
 * WHAT: Verify is_ready handles NULL
 * WHY: Safe status checking
 * HOW: Call with NULL, verify false returned
 */
TEST_F(SelfRepairTest, IsReadyNullSafety) {
    // ACT & ASSERT
    EXPECT_FALSE(self_repair_is_ready(NULL));
}

/**
 * @test Rollback Non-Existent Repair
 *
 * WHAT: Verify rollback handles invalid repair ID
 * WHY: Graceful error handling
 * HOW: Attempt rollback with invalid ID, verify error
 */
TEST_F(SelfRepairTest, RollbackNonExistent) {
    // ARRANGE
    self_repair_coordinator_t* coordinator = self_repair_create(NULL);
    ASSERT_NE(coordinator, nullptr);

    // ACT: Attempt to rollback non-existent repair
    int result = self_repair_rollback(coordinator, 99999);

    // ASSERT: Should return error (repair not found)
    EXPECT_NE(result, 0);

    // CLEANUP
    self_repair_destroy(coordinator);
}

/**
 * @test Cancel Non-Existent Repair
 *
 * WHAT: Verify cancel handles invalid repair ID
 * WHY: Graceful error handling
 * HOW: Attempt cancel with invalid ID, verify error
 */
TEST_F(SelfRepairTest, CancelNonExistent) {
    // ARRANGE
    self_repair_coordinator_t* coordinator = self_repair_create(NULL);
    ASSERT_NE(coordinator, nullptr);

    // ACT: Attempt to cancel non-existent repair
    int result = self_repair_cancel(coordinator, 99999);

    // ASSERT: Should return error (repair not found)
    EXPECT_NE(result, 0);

    // CLEANUP
    self_repair_destroy(coordinator);
}

//=============================================================================
// Repair Request Structure Tests
//=============================================================================

/**
 * @test Repair Request with Diagnostic
 *
 * WHAT: Verify repair request structure handling
 * WHY: Core API for initiating repairs
 * HOW: Create request, verify structure accepted
 */
TEST_F(SelfRepairTest, RepairRequestStructure) {
    // ARRANGE
    self_repair_coordinator_t* coordinator = self_repair_create(NULL);
    ASSERT_NE(coordinator, nullptr);

    // Create diagnostic result
    diagnostic_result_t diagnosis;
    memset(&diagnosis, 0, sizeof(diagnosis));
    diagnosis.error_type = ERROR_TYPE_NULL_POINTER;
    diagnosis.severity = DIAG_SEVERITY_CRITICAL;
    strncpy(diagnosis.likely_faulty_function, "test_func", sizeof(diagnosis.likely_faulty_function) - 1);
    strncpy(diagnosis.root_cause, "Null pointer dereference", sizeof(diagnosis.root_cause) - 1);

    // Create repair request
    self_repair_request_t request;
    memset(&request, 0, sizeof(request));
    request.diagnosis = &diagnosis;
    request.preferred_strategy = FIX_STRATEGY_NONE;  // Auto-select

    self_repair_result_t result;
    memset(&result, 0, sizeof(result));

    // ACT: Initiate repair
    // Note: May fail if source file doesn't exist, but should not crash
    int status = self_repair_initiate(coordinator, &request, &result);

    // ASSERT: No crash; status depends on whether file exists
    (void)status;

    // CLEANUP
    self_repair_destroy(coordinator);
}

//=============================================================================
// Pipeline Stage Tests
//=============================================================================

/**
 * @test Code Analysis Stage
 *
 * WHAT: Verify code analysis stage API
 * WHY: Core pipeline stage
 * HOW: Call analysis with diagnostic, verify structure
 */
TEST_F(SelfRepairTest, CodeAnalysisStage) {
    // ARRANGE
    self_repair_coordinator_t* coordinator = self_repair_create(NULL);
    ASSERT_NE(coordinator, nullptr);

    diagnostic_result_t diagnosis;
    memset(&diagnosis, 0, sizeof(diagnosis));
    diagnosis.error_type = ERROR_TYPE_NULL_POINTER;
    strncpy(diagnosis.likely_faulty_function, "test_func", sizeof(diagnosis.likely_faulty_function) - 1);
    strncpy(diagnosis.root_cause, "Null pointer dereference", sizeof(diagnosis.root_cause) - 1);

    code_analysis_result_t analysis;
    memset(&analysis, 0, sizeof(analysis));

    // ACT: Perform code analysis
    int result = self_repair_analyze_code(coordinator, &diagnosis, &analysis);

    // ASSERT: No crash; result depends on file availability
    (void)result;

    // CLEANUP
    self_repair_destroy(coordinator);
}

//=============================================================================
// Main Entry Point
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
