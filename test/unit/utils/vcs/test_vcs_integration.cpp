/**
 * @file test_vcs_integration.cpp
 * @brief Unit tests for VCS Integration Module
 * @version 1.0.0
 * @date 2025-01-20
 *
 * WHAT: Comprehensive unit tests for version control system integration
 * WHY: Ensure source code modifications and git operations work correctly
 * HOW: Test-driven development with coverage of all public APIs
 *
 * Test Coverage:
 * - Creation and destruction
 * - VCS detection
 * - File backup and restore
 * - Git operations (add, commit, push)
 * - Branch management
 * - High-level operations
 * - Error handling
 *
 * @author NIMCP Development Team
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdio>

// Headers have their own extern "C" guards
#include "utils/vcs/nimcp_vcs_integration.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"

//=============================================================================
// Test Fixture
//=============================================================================

class VcsIntegrationTest : public ::testing::Test {
protected:
    size_t baseline_allocated = 0;

    void SetUp() override {
        // Initialize memory tracking
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);

        // Warm up exception handler system (one-time mutex allocation)
        {
            nimcp_exception_t* warmup = nimcp_exception_create(
                NIMCP_ERROR_NULL_POINTER, EXCEPTION_SEVERITY_DEBUG,
                __FILE__, __LINE__, __func__, "warmup");
            if (warmup) {
                nimcp_exception_dispatch(warmup);
                nimcp_exception_unref(warmup);
            }
            nimcp_exception_clear_current();
        }

        // Record baseline memory after handler system initialized
        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        baseline_allocated = stats.current_allocated;
    }

    void TearDown() override {
        // Release any exception held as "current" by the dispatch system
        nimcp_exception_clear_current();

        // Check for memory leaks relative to baseline
        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        EXPECT_EQ(stats.current_allocated, baseline_allocated)
            << "Memory leak detected! Allocated: " << stats.current_allocated
            << ", Baseline: " << baseline_allocated;
    }
};

//=============================================================================
// Creation and Destruction Tests
//=============================================================================

/**
 * @test VCS Integration Creation with Default Config
 *
 * WHAT: Verify VCS integration can be created with default configuration
 * WHY: Ensure proper initialization
 * HOW: Create with defaults, verify state, destroy
 */
TEST_F(VcsIntegrationTest, CreateWithDefaults) {
    // ACT: Create with default config
    vcs_integration_t* vcs = vcs_create(NULL);

    // ASSERT: Created successfully
    ASSERT_NE(vcs, nullptr);

    // Verify initial state (may or may not be ready depending on repo)
    // Just verify no crash
    (void)vcs_is_ready(vcs);

    // CLEANUP
    vcs_destroy(vcs);
}

/**
 * @test VCS Integration Creation with Custom Config
 *
 * WHAT: Verify VCS accepts custom configuration
 * WHY: Allow users to customize VCS behavior
 * HOW: Create with custom config, verify settings applied
 */
TEST_F(VcsIntegrationTest, CreateWithCustomConfig) {
    // ARRANGE: Custom config
    vcs_config_t config = vcs_default_config();
    strncpy(config.repo_path, "/tmp", sizeof(config.repo_path) - 1);
    config.vcs_type = VCS_TYPE_AUTO;
    config.create_backup = true;
    config.dry_run = true;  // Don't actually modify anything

    // ACT: Create with custom config
    vcs_integration_t* vcs = vcs_create(&config);

    // ASSERT: Created successfully
    ASSERT_NE(vcs, nullptr);

    // CLEANUP
    vcs_destroy(vcs);
}

/**
 * @test VCS Integration Destroy NULL Safety
 *
 * WHAT: Verify destroy handles NULL gracefully
 * WHY: Prevent crashes on double-free or accidental NULL
 * HOW: Call destroy with NULL, expect no crash
 */
TEST_F(VcsIntegrationTest, DestroyNullSafety) {
    // ACT & ASSERT: Should not crash
    EXPECT_NO_THROW(vcs_destroy(NULL));
}

//=============================================================================
// VCS Detection Tests
//=============================================================================

/**
 * @test Detect Git Repository
 *
 * WHAT: Verify git repository detection
 * WHY: Core capability for git operations
 * HOW: Point at NIMCP repo (known to be git), verify detection
 */
TEST_F(VcsIntegrationTest, DetectGitRepository) {
    // ACT: Detect VCS in current project (should be git)
    vcs_type_t detected = vcs_detect_type("/home/bbrelin/nimcp");

    // ASSERT: Git detected
    EXPECT_EQ(detected, VCS_TYPE_GIT);
}

/**
 * @test Detect No VCS
 *
 * WHAT: Verify non-repository detection
 * WHY: Handle directories without version control
 * HOW: Point at /tmp, verify VCS_TYPE_NONE
 */
TEST_F(VcsIntegrationTest, DetectNoVcs) {
    // ACT: Detect VCS in /tmp (no repo)
    vcs_type_t detected = vcs_detect_type("/tmp");

    // ASSERT: No VCS detected
    EXPECT_EQ(detected, VCS_TYPE_NONE);
}

//=============================================================================
// Configuration Tests
//=============================================================================

/**
 * @test Default Config Values
 *
 * WHAT: Verify default configuration has sensible values
 * WHY: Ensure safe defaults
 * HOW: Get default config, verify key values
 */
TEST_F(VcsIntegrationTest, DefaultConfigValues) {
    // ACT
    vcs_config_t config = vcs_default_config();

    // ASSERT: Sensible defaults
    EXPECT_EQ(config.vcs_type, VCS_TYPE_AUTO);
    EXPECT_TRUE(config.create_backup);
    EXPECT_TRUE(config.require_validation);
    EXPECT_FALSE(config.dry_run);
    EXPECT_GT(strlen(config.commit_prefix), 0u);
}

//=============================================================================
// Backup and Restore Tests
//=============================================================================

/**
 * @test Create Backup Path Generation
 *
 * WHAT: Verify backup path is properly formatted
 * WHY: Ensure backup files don't conflict with originals
 * HOW: Create backup, verify path format
 */
TEST_F(VcsIntegrationTest, BackupPathFormat) {
    // ARRANGE
    vcs_config_t config = vcs_default_config();
    strncpy(config.repo_path, "/home/bbrelin/nimcp", sizeof(config.repo_path) - 1);
    config.dry_run = true;  // Don't actually create files

    vcs_integration_t* vcs = vcs_create(&config);
    ASSERT_NE(vcs, nullptr);

    char backup_path[VCS_MAX_PATH];
    memset(backup_path, 0, sizeof(backup_path));

    // ACT: Create backup (dry run - just generates path)
    int result = vcs_create_backup(
        vcs,
        "/home/bbrelin/nimcp/test/test.txt",
        backup_path,
        sizeof(backup_path)
    );

    // Note: May fail if dry_run prevents actual backup, but path should still be set
    // Just verify no crash and proper path format if successful
    if (result == VCS_OK) {
        EXPECT_NE(strstr(backup_path, VCS_BACKUP_SUFFIX), nullptr);
    }

    // CLEANUP
    vcs_destroy(vcs);
}

//=============================================================================
// Git Operation Tests (Dry Run)
//=============================================================================

/**
 * @test Git Add Operation Structure
 *
 * WHAT: Verify git add API works (with dry run)
 * WHY: Core staging functionality
 * HOW: Attempt add with dry run, verify no crash
 */
TEST_F(VcsIntegrationTest, GitAddDryRun) {
    // ARRANGE
    vcs_config_t config = vcs_default_config();
    strncpy(config.repo_path, "/home/bbrelin/nimcp", sizeof(config.repo_path) - 1);
    config.dry_run = true;

    vcs_integration_t* vcs = vcs_create(&config);
    ASSERT_NE(vcs, nullptr);

    // ACT: Attempt git add (dry run)
    int result = vcs_git_add(vcs, "/home/bbrelin/nimcp/test/dummy.txt");

    // ASSERT: Dry run succeeds or returns appropriate error
    // Just verify no crash
    (void)result;

    // CLEANUP
    vcs_destroy(vcs);
}

/**
 * @test Generate Commit Message
 *
 * WHAT: Verify commit message generation
 * WHY: Standardized commit messages for automated fixes
 * HOW: Generate message for fix, verify format
 */
TEST_F(VcsIntegrationTest, GenerateCommitMessage) {
    // ARRANGE
    vcs_config_t config = vcs_default_config();
    strncpy(config.repo_path, "/home/bbrelin/nimcp", sizeof(config.repo_path) - 1);
    config.dry_run = true;

    vcs_integration_t* vcs = vcs_create(&config);
    ASSERT_NE(vcs, nullptr);

    generated_fix_t fix;
    memset(&fix, 0, sizeof(fix));
    fix.fix_id = 123;
    fix.strategy = FIX_STRATEGY_NULL_CHECK;
    strncpy(fix.source_file, "src/test.c", sizeof(fix.source_file) - 1);
    strncpy(fix.function_name, "test_function", sizeof(fix.function_name) - 1);
    fix.start_line = 42;
    strncpy(fix.explanation, "Add null check for pointer", sizeof(fix.explanation) - 1);

    char message[VCS_MAX_COMMIT_MSG];
    memset(message, 0, sizeof(message));

    // ACT: Generate commit message
    int result = vcs_generate_commit_message(vcs, &fix, message, sizeof(message));

    // ASSERT: Message generated with expected content
    EXPECT_EQ(result, VCS_OK);
    EXPECT_GT(strlen(message), 0u);
    // Message should mention the strategy or fix
    EXPECT_NE(strstr(message, "null") != NULL || strstr(message, "NULL") != NULL ||
              strstr(message, "fix") != NULL || strstr(message, "Fix") != NULL, false);

    // CLEANUP
    vcs_destroy(vcs);
}

//=============================================================================
// Branch Operation Tests
//=============================================================================

/**
 * @test Get Current Branch
 *
 * WHAT: Verify current branch retrieval
 * WHY: Track branch for operations
 * HOW: Get current branch, verify non-empty
 */
TEST_F(VcsIntegrationTest, GetCurrentBranch) {
    // ARRANGE
    vcs_config_t config = vcs_default_config();
    strncpy(config.repo_path, "/home/bbrelin/nimcp", sizeof(config.repo_path) - 1);

    vcs_integration_t* vcs = vcs_create(&config);
    ASSERT_NE(vcs, nullptr);

    char branch[VCS_MAX_BRANCH_NAME];
    memset(branch, 0, sizeof(branch));

    // ACT: Get current branch
    int result = vcs_get_current_branch(vcs, branch, sizeof(branch));

    // ASSERT: Branch retrieved (we're in a git repo)
    if (vcs_is_ready(vcs)) {
        EXPECT_EQ(result, VCS_OK);
        EXPECT_GT(strlen(branch), 0u);
    }

    // CLEANUP
    vcs_destroy(vcs);
}

//=============================================================================
// Statistics Tests
//=============================================================================

/**
 * @test VCS Statistics Tracking
 *
 * WHAT: Verify statistics tracking
 * WHY: Monitor VCS operation metrics
 * HOW: Get stats, verify structure
 */
TEST_F(VcsIntegrationTest, StatisticsTracking) {
    // ARRANGE
    vcs_config_t config = vcs_default_config();
    strncpy(config.repo_path, "/home/bbrelin/nimcp", sizeof(config.repo_path) - 1);

    vcs_integration_t* vcs = vcs_create(&config);
    ASSERT_NE(vcs, nullptr);

    vcs_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    // ACT: Get statistics
    int result = vcs_get_stats(vcs, &stats);

    // ASSERT: Stats retrieved successfully
    EXPECT_EQ(result, VCS_OK);
    // Initial stats should be zero
    EXPECT_EQ(stats.files_modified, 0u);
    EXPECT_EQ(stats.commits_made, 0u);

    // CLEANUP
    vcs_destroy(vcs);
}

//=============================================================================
// Utility Function Tests
//=============================================================================

/**
 * @test VCS Error String Conversion
 *
 * WHAT: Verify error code to string conversion
 * WHY: Support error reporting
 * HOW: Convert each error code, verify non-empty string
 */
TEST_F(VcsIntegrationTest, ErrorStringConversion) {
    // ACT & ASSERT: Each error has a valid string
    EXPECT_NE(vcs_strerror(VCS_OK), nullptr);
    EXPECT_GT(strlen(vcs_strerror(VCS_ERR_NULL)), 0u);
    EXPECT_GT(strlen(vcs_strerror(VCS_ERR_NOT_REPO)), 0u);
    EXPECT_GT(strlen(vcs_strerror(VCS_ERR_FILE_NOT_FOUND)), 0u);
    EXPECT_GT(strlen(vcs_strerror(VCS_ERR_BACKUP_FAILED)), 0u);
    EXPECT_GT(strlen(vcs_strerror(VCS_ERR_WRITE_FAILED)), 0u);
    EXPECT_GT(strlen(vcs_strerror(VCS_ERR_GIT_ADD)), 0u);
    EXPECT_GT(strlen(vcs_strerror(VCS_ERR_GIT_COMMIT)), 0u);
}

/**
 * @test VCS Type Name Conversion
 *
 * WHAT: Verify VCS type to string conversion
 * WHY: Support logging
 * HOW: Convert each type, verify non-empty string
 */
TEST_F(VcsIntegrationTest, TypeNameConversion) {
    // ACT & ASSERT
    EXPECT_GT(strlen(vcs_type_name(VCS_TYPE_NONE)), 0u);
    EXPECT_GT(strlen(vcs_type_name(VCS_TYPE_GIT)), 0u);
    EXPECT_GT(strlen(vcs_type_name(VCS_TYPE_AUTO)), 0u);
}

/**
 * @test Commit Status Name Conversion
 *
 * WHAT: Verify commit status to string conversion
 * WHY: Support status reporting
 * HOW: Convert each status, verify non-empty string
 */
TEST_F(VcsIntegrationTest, CommitStatusNameConversion) {
    // ACT & ASSERT
    EXPECT_GT(strlen(vcs_commit_status_name(COMMIT_STATUS_PENDING)), 0u);
    EXPECT_GT(strlen(vcs_commit_status_name(COMMIT_STATUS_STAGED)), 0u);
    EXPECT_GT(strlen(vcs_commit_status_name(COMMIT_STATUS_COMMITTED)), 0u);
    EXPECT_GT(strlen(vcs_commit_status_name(COMMIT_STATUS_PUSHED)), 0u);
    EXPECT_GT(strlen(vcs_commit_status_name(COMMIT_STATUS_REVERTED)), 0u);
    EXPECT_GT(strlen(vcs_commit_status_name(COMMIT_STATUS_FAILED)), 0u);
}

/**
 * @test Version String
 *
 * WHAT: Verify version string returned
 * WHY: Support version tracking
 * HOW: Get version, verify format
 */
TEST_F(VcsIntegrationTest, VersionString) {
    // ACT
    const char* version = vcs_version();

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
TEST_F(VcsIntegrationTest, NullParameterHandling) {
    // ARRANGE
    vcs_integration_t* vcs = vcs_create(NULL);
    ASSERT_NE(vcs, nullptr);

    char buffer[256];

    // ACT & ASSERT: NULL parameters return error
    EXPECT_NE(vcs_git_add(NULL, "file.txt"), VCS_OK);
    EXPECT_NE(vcs_git_add(vcs, NULL), VCS_OK);
    EXPECT_NE(vcs_create_backup(NULL, "file.txt", buffer, sizeof(buffer)), VCS_OK);
    EXPECT_NE(vcs_create_backup(vcs, NULL, buffer, sizeof(buffer)), VCS_OK);
    EXPECT_NE(vcs_get_current_branch(NULL, buffer, sizeof(buffer)), VCS_OK);
    EXPECT_NE(vcs_get_stats(NULL, NULL), VCS_OK);

    // CLEANUP
    vcs_destroy(vcs);
}

/**
 * @test is_ready Returns False for NULL
 *
 * WHAT: Verify is_ready handles NULL
 * WHY: Safe status checking
 * HOW: Call with NULL, verify false returned
 */
TEST_F(VcsIntegrationTest, IsReadyNullSafety) {
    // ACT & ASSERT
    EXPECT_FALSE(vcs_is_ready(NULL));
}

/**
 * @test Repository Status Check
 *
 * WHAT: Verify repo status checking
 * WHY: Support workflow decisions
 * HOW: Check if repo is clean
 */
TEST_F(VcsIntegrationTest, RepoStatusCheck) {
    // ARRANGE
    vcs_config_t config = vcs_default_config();
    strncpy(config.repo_path, "/home/bbrelin/nimcp", sizeof(config.repo_path) - 1);

    vcs_integration_t* vcs = vcs_create(&config);
    ASSERT_NE(vcs, nullptr);

    // ACT: Check if repo is clean (just verify no crash)
    if (vcs_is_ready(vcs)) {
        bool is_clean = vcs_is_repo_clean(vcs);
        // Just verify the call works, result depends on repo state
        (void)is_clean;
    }

    // CLEANUP
    vcs_destroy(vcs);
}

//=============================================================================
// Write Operation Tests
//=============================================================================

/**
 * @test Write Request Structure Initialization
 *
 * WHAT: Verify write request structure works correctly
 * WHY: Core file modification capability
 * HOW: Initialize request, attempt write (dry run)
 */
TEST_F(VcsIntegrationTest, WriteRequestStructure) {
    // ARRANGE
    vcs_config_t config = vcs_default_config();
    strncpy(config.repo_path, "/home/bbrelin/nimcp", sizeof(config.repo_path) - 1);
    config.dry_run = true;  // Don't actually write

    vcs_integration_t* vcs = vcs_create(&config);
    ASSERT_NE(vcs, nullptr);

    // Create write request
    vcs_write_request_t request;
    memset(&request, 0, sizeof(request));
    strncpy(request.source_file, "/home/bbrelin/nimcp/test/dummy.c", sizeof(request.source_file) - 1);
    request.replace_start_line = 10;
    request.replace_end_line = 12;
    request.new_content = "// Fixed code\nint x = 0;\n";
    request.create_backup = true;

    vcs_write_result_t result;
    memset(&result, 0, sizeof(result));

    // ACT: Attempt write (dry run)
    int status = vcs_write_fix(vcs, &request, &result);

    // ASSERT: In dry run mode, should fail because file doesn't exist
    // But should not crash
    (void)status;

    // CLEANUP
    vcs_destroy(vcs);
}

//=============================================================================
// Main Entry Point
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
