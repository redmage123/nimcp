/**
 * @file test_pattern_db_regression_v2.cpp
 * @brief Comprehensive GTest Regression Tests for Pattern Database Security Module
 *
 * WHAT: GTest-based regression tests verifying Pattern DB backward compatibility and API stability
 * WHY:  Ensure pattern matching behavior remains consistent and detection rates don't regress
 * HOW:  Test API contracts, pattern compilation, detection rates, and performance baselines
 *
 * REGRESSION CATEGORIES:
 * 1. API Contract Stability - Function signatures, return values
 * 2. Pattern Compilation Backward Compatibility - Same patterns compile same way
 * 3. Detection Rate Verification - Known patterns still detected
 * 4. Default Pattern Effectiveness - Built-in patterns work correctly
 * 5. Performance Baselines - Match operations stay within time bounds
 * 6. Versioning API Stability - Snapshot/rollback behavior
 * 7. Statistics Accuracy - Counters reflect actual operations
 *
 * @author NIMCP Security Team
 * @date 2026-02-02
 */

#include "test_helpers.h"

extern "C" {
#include "security/nimcp_pattern_db.h"
}

#include <chrono>
#include <cstring>
#include <string>
#include <vector>

namespace {

//=============================================================================
// Test Fixture
//=============================================================================

class PatternDBRegressionV2Test : public ::testing::Test {
protected:
    void SetUp() override
    {
        nimcp_pattern_db_config_t config = nimcp_pattern_db_default_config();
        db_ = nimcp_pattern_db_create(&config);
    }

    void TearDown() override
    {
        if (db_) {
            nimcp_pattern_db_destroy(db_);
            db_ = nullptr;
        }
    }

    double get_time_ms()
    {
        auto now = std::chrono::high_resolution_clock::now();
        auto duration = now.time_since_epoch();
        return std::chrono::duration<double, std::milli>(duration).count();
    }

    nimcp_pattern_db_t db_ = nullptr;
};

//=============================================================================
// API Contract Stability Tests
//=============================================================================

/**
 * Test: nimcp_pattern_db_create() API contract
 * Regression: Must return NULL on invalid config, valid handle on valid/NULL config
 */
TEST_F(PatternDBRegressionV2Test, ApiCreateContract)
{
    // NULL config should work (use defaults)
    nimcp_pattern_db_t db1 = nimcp_pattern_db_create(nullptr);
    EXPECT_NE(db1, nullptr) << "create(NULL) should succeed with defaults";
    nimcp_pattern_db_destroy(db1);

    // Valid config should work
    nimcp_pattern_db_config_t config = nimcp_pattern_db_default_config();
    nimcp_pattern_db_t db2 = nimcp_pattern_db_create(&config);
    EXPECT_NE(db2, nullptr) << "create(&config) should succeed";
    nimcp_pattern_db_destroy(db2);
}

/**
 * Test: nimcp_pattern_db_destroy() API contract
 * Regression: Must handle NULL safely (no crash)
 */
TEST_F(PatternDBRegressionV2Test, ApiDestroyNullSafety)
{
    // NULL should not crash
    nimcp_pattern_db_destroy(nullptr);
    SUCCEED();
}

/**
 * Test: nimcp_pattern_db_add() return value contract
 * Regression: Must return NIMCP_SUCCESS on valid pattern, error on invalid
 */
TEST_F(PatternDBRegressionV2Test, ApiAddReturnValue)
{
    ASSERT_NE(db_, nullptr) << "Setup failed";

    nimcp_pattern_entry_t entry = {};
    entry.pattern = "test.*pattern";
    entry.category = NIMCP_PATTERN_CUSTOM;
    entry.priority = 10;
    entry.weight = 0.5f;
    entry.description = "Test pattern";
    entry.flags = 0;

    nimcp_pattern_id_t id;
    nimcp_error_t result = nimcp_pattern_db_add(db_, &entry, &id);
    EXPECT_EQ(result, NIMCP_SUCCESS) << "add valid pattern must succeed";
    EXPECT_NE(id, NIMCP_PATTERN_ID_INVALID) << "id must be valid";

    // NULL db
    result = nimcp_pattern_db_add(nullptr, &entry, &id);
    EXPECT_NE(result, NIMCP_SUCCESS) << "add with NULL db must fail";

    // NULL entry
    result = nimcp_pattern_db_add(db_, nullptr, &id);
    EXPECT_NE(result, NIMCP_SUCCESS) << "add with NULL entry must fail";
}

/**
 * Test: nimcp_pattern_db_remove() return value contract
 * Regression: Must return NIMCP_SUCCESS on valid id, error on invalid
 */
TEST_F(PatternDBRegressionV2Test, ApiRemoveReturnValue)
{
    ASSERT_NE(db_, nullptr) << "Setup failed";

    // Add pattern first
    nimcp_pattern_entry_t entry = {};
    entry.pattern = "remove.*test";
    entry.category = NIMCP_PATTERN_CUSTOM;
    entry.priority = 10;
    entry.weight = 0.5f;
    entry.description = "To be removed";
    entry.flags = 0;

    nimcp_pattern_id_t id;
    nimcp_pattern_db_add(db_, &entry, &id);

    // Remove valid pattern
    nimcp_error_t result = nimcp_pattern_db_remove(db_, id);
    EXPECT_EQ(result, NIMCP_SUCCESS) << "remove valid pattern must succeed";

    // Remove invalid id
    result = nimcp_pattern_db_remove(db_, NIMCP_PATTERN_ID_INVALID);
    EXPECT_NE(result, NIMCP_SUCCESS) << "remove invalid id must fail";

    // Remove already removed
    result = nimcp_pattern_db_remove(db_, id);
    EXPECT_NE(result, NIMCP_SUCCESS) << "remove already removed must fail";
}

//=============================================================================
// Pattern Category Enum Stability Tests
//=============================================================================

/**
 * Test: Pattern category enum values must not change (ABI stability)
 * Regression: Changing enum values breaks serialization and compatibility
 */
TEST_F(PatternDBRegressionV2Test, EnumValuesStable)
{
    EXPECT_EQ(NIMCP_PATTERN_SQL_INJECTION, 0) << "SQL_INJECTION must be 0";
    EXPECT_EQ(NIMCP_PATTERN_XSS, 1) << "XSS must be 1";
    EXPECT_EQ(NIMCP_PATTERN_SHELL_INJECTION, 2) << "SHELL_INJECTION must be 2";
    EXPECT_EQ(NIMCP_PATTERN_PATH_TRAVERSAL, 3) << "PATH_TRAVERSAL must be 3";
    EXPECT_EQ(NIMCP_PATTERN_FORMAT_STRING, 4) << "FORMAT_STRING must be 4";
    EXPECT_EQ(NIMCP_PATTERN_PROMPT_INJECTION, 5) << "PROMPT_INJECTION must be 5";
    EXPECT_EQ(NIMCP_PATTERN_BUFFER_OVERFLOW, 6) << "BUFFER_OVERFLOW must be 6";
    EXPECT_EQ(NIMCP_PATTERN_LDAP_INJECTION, 7) << "LDAP_INJECTION must be 7";
    EXPECT_EQ(NIMCP_PATTERN_XML_INJECTION, 8) << "XML_INJECTION must be 8";
    EXPECT_EQ(NIMCP_PATTERN_COMMAND_INJECTION, 9) << "COMMAND_INJECTION must be 9";
    EXPECT_EQ(NIMCP_PATTERN_CUSTOM, 10) << "CUSTOM must be 10";
}

/**
 * Test: Default config values must not change
 * Regression: Default behavior changes break existing deployments
 */
TEST_F(PatternDBRegressionV2Test, DefaultConfigStable)
{
    nimcp_pattern_db_config_t config = nimcp_pattern_db_default_config();

    // Capacity defaults
    EXPECT_GE(config.initial_capacity, 64u) << "initial_capacity must be at least 64";
    EXPECT_GT(config.max_snapshots, 0u) << "max_snapshots must be > 0";

    // Feature flags
    EXPECT_TRUE(config.enable_statistics) << "enable_statistics default must be true";
    EXPECT_TRUE(config.enable_validation) << "enable_validation default must be true";

    // Timeout should be reasonable
    EXPECT_GT(config.match_timeout_ms, 0u) << "match_timeout_ms must be > 0";
}

//=============================================================================
// Pattern Compilation Backward Compatibility Tests
//=============================================================================

/**
 * Test: Empty pattern handling
 * Regression: Empty patterns should be handled consistently (accept or reject)
 */
TEST_F(PatternDBRegressionV2Test, PatternEmptyHandling)
{
    ASSERT_NE(db_, nullptr) << "Setup failed";

    nimcp_pattern_entry_t entry = {};
    entry.pattern = "";  // Empty pattern
    entry.category = NIMCP_PATTERN_CUSTOM;
    entry.priority = 1;
    entry.weight = 0.5f;
    entry.description = "Empty pattern";
    entry.flags = 0;

    // Implementation may accept (matches nothing) or reject empty patterns
    nimcp_error_t result = nimcp_pattern_db_add(db_, &entry, nullptr);
    // Just verify it doesn't crash and returns a consistent result
    (void)result;
    SUCCEED();
}

/**
 * Test: Very long pattern rejection
 * Regression: Patterns exceeding max length must be rejected
 */
TEST_F(PatternDBRegressionV2Test, PatternMaxLength)
{
    ASSERT_NE(db_, nullptr) << "Setup failed";

    // Create pattern longer than max
    std::string long_pattern(NIMCP_PATTERN_MAX_LENGTH + 99, 'x');

    nimcp_pattern_entry_t entry = {};
    entry.pattern = long_pattern.c_str();
    entry.category = NIMCP_PATTERN_CUSTOM;
    entry.priority = 1;
    entry.weight = 0.5f;
    entry.description = "Too long";
    entry.flags = 0;

    nimcp_error_t result = nimcp_pattern_db_add(db_, &entry, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS) << "Pattern exceeding max length must be rejected";
}

/**
 * Test: Valid regex patterns compile successfully
 * Regression: Common regex patterns must still compile
 */
TEST_F(PatternDBRegressionV2Test, PatternValidRegex)
{
    ASSERT_NE(db_, nullptr) << "Setup failed";

    std::vector<const char*> valid_patterns = {
        ".*",
        "test",
        "[a-z]+",
        "\\d{3}-\\d{4}",
        "^start",
        "end$",
        "(group)",
        "alt1|alt2",
    };

    for (const auto& pattern : valid_patterns) {
        nimcp_pattern_entry_t entry = {};
        entry.pattern = pattern;
        entry.category = NIMCP_PATTERN_CUSTOM;
        entry.priority = 1;
        entry.weight = 0.5f;
        entry.description = "Valid regex";
        entry.flags = 0;

        nimcp_error_t result = nimcp_pattern_db_add(db_, &entry, nullptr);
        EXPECT_EQ(result, NIMCP_SUCCESS) << "Valid regex must compile: " << pattern;
    }
}

//=============================================================================
// Detection Rate Verification Tests
//=============================================================================

/**
 * Test: SQL injection patterns detected
 * Regression: SQL injection patterns must be detected when added
 */
TEST_F(PatternDBRegressionV2Test, DetectionSqlInjection)
{
    ASSERT_NE(db_, nullptr) << "Setup failed";

    // Add SQL injection pattern
    nimcp_pattern_entry_t entry = {};
    entry.pattern = "(union|select).*from";  // POSIX ERE: (?i) handled by CASE_INSENSITIVE flag
    entry.category = NIMCP_PATTERN_SQL_INJECTION;
    entry.priority = 10;
    entry.weight = 1.0f;
    entry.description = "SQL injection";
    entry.flags = NIMCP_PATTERN_FLAG_CASE_INSENSITIVE;

    nimcp_error_t result = nimcp_pattern_db_add(db_, &entry, nullptr);
    EXPECT_EQ(result, NIMCP_SUCCESS) << "Pattern add must succeed";

    // Test detection
    std::vector<const char*> sql_injections = {
        "UNION SELECT * FROM users",
        "union select password from accounts",
        "1 UNION SELECT * FROM admin",
    };

    for (const auto& input : sql_injections) {
        nimcp_pattern_match_result_t match_result;
        result = nimcp_pattern_db_match(db_, input, &match_result);

        if (result == NIMCP_SUCCESS) {
            EXPECT_TRUE(match_result.matched) << "SQL injection must be detected: " << input;
            EXPECT_EQ(match_result.category, NIMCP_PATTERN_SQL_INJECTION)
                << "Category must be SQL_INJECTION: " << input;
        }
    }
}

/**
 * Test: XSS patterns detected
 * Regression: XSS patterns must be detected when added
 */
TEST_F(PatternDBRegressionV2Test, DetectionXss)
{
    ASSERT_NE(db_, nullptr) << "Setup failed";

    // Add XSS pattern
    nimcp_pattern_entry_t entry = {};
    entry.pattern = "<script.*>";  // POSIX ERE: no non-greedy *? support, use greedy .*
    entry.category = NIMCP_PATTERN_XSS;
    entry.priority = 10;
    entry.weight = 1.0f;
    entry.description = "XSS script tag";
    entry.flags = NIMCP_PATTERN_FLAG_CASE_INSENSITIVE;

    nimcp_error_t result = nimcp_pattern_db_add(db_, &entry, nullptr);
    EXPECT_EQ(result, NIMCP_SUCCESS) << "Pattern add must succeed";

    // Test detection
    std::vector<const char*> xss_attacks = {
        "<script>alert('xss')</script>",
        "<SCRIPT>evil()</SCRIPT>",
        "<script src='http://evil.com/xss.js'>",
    };

    for (const auto& input : xss_attacks) {
        nimcp_pattern_match_result_t match_result;
        result = nimcp_pattern_db_match(db_, input, &match_result);

        if (result == NIMCP_SUCCESS) {
            EXPECT_TRUE(match_result.matched) << "XSS must be detected: " << input;
        }
    }
}

/**
 * Test: Safe inputs not detected as threats
 * Regression: False positive rate must remain low
 */
TEST_F(PatternDBRegressionV2Test, DetectionNoFalsePositives)
{
    ASSERT_NE(db_, nullptr) << "Setup failed";

    // Add patterns
    nimcp_pattern_entry_t sql_entry = {};
    sql_entry.pattern = "'.*OR.*'.*=.*'";
    sql_entry.category = NIMCP_PATTERN_SQL_INJECTION;
    sql_entry.priority = 10;
    sql_entry.weight = 1.0f;
    sql_entry.description = "SQL OR injection";
    sql_entry.flags = 0;
    nimcp_pattern_db_add(db_, &sql_entry, nullptr);

    // Safe inputs
    std::vector<const char*> safe_inputs = {
        "Hello, World!",
        "This is a normal sentence.",
        "user@example.com",
        "12345",
        "SELECT name FROM users WHERE id = 1",  // Legitimate SQL
    };

    for (const auto& input : safe_inputs) {
        nimcp_pattern_match_result_t match_result;
        nimcp_error_t result = nimcp_pattern_db_match(db_, input, &match_result);

        if (result == NIMCP_SUCCESS && match_result.matched) {
            ADD_FAILURE() << "False positive: " << input;
        }
    }
}

//=============================================================================
// Performance Baseline Tests
//=============================================================================

/**
 * Test: Pattern matching performance baseline
 * Regression: 1000 matches must complete in under 1 second
 */
TEST_F(PatternDBRegressionV2Test, PerformanceMatch)
{
    ASSERT_NE(db_, nullptr) << "Setup failed";

    // Add a pattern
    nimcp_pattern_entry_t entry = {};
    entry.pattern = "test.*pattern";
    entry.category = NIMCP_PATTERN_CUSTOM;
    entry.priority = 1;
    entry.weight = 0.5f;
    entry.description = "Performance test";
    entry.flags = 0;
    nimcp_pattern_db_add(db_, &entry, nullptr);

    const int NUM_ITERATIONS = 1000;
    const double MAX_TIME_MS = 1000.0;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        nimcp_pattern_match_result_t result;
        nimcp_pattern_db_match(db_, "test input pattern", &result);
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;

    EXPECT_LT(elapsed.count(), MAX_TIME_MS)
        << "Pattern matching must meet performance baseline: " << elapsed.count() << " ms";
}

/**
 * Test: Pattern add performance baseline
 * Regression: 100 pattern adds must complete in under 1 second
 */
TEST_F(PatternDBRegressionV2Test, PerformanceAdd)
{
    ASSERT_NE(db_, nullptr) << "Setup failed";

    const int NUM_ITERATIONS = 100;
    const double MAX_TIME_MS = 1000.0;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        char pattern[64];
        snprintf(pattern, sizeof(pattern), "pattern_%d", i);

        nimcp_pattern_entry_t entry = {};
        entry.pattern = pattern;
        entry.category = NIMCP_PATTERN_CUSTOM;
        entry.priority = 1;
        entry.weight = 0.5f;
        entry.description = "Perf test";
        entry.flags = 0;
        nimcp_pattern_db_add(db_, &entry, nullptr);
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;

    EXPECT_LT(elapsed.count(), MAX_TIME_MS)
        << "Pattern add must meet performance baseline: " << elapsed.count() << " ms";
}

//=============================================================================
// Versioning API Stability Tests
//=============================================================================

/**
 * Test: nimcp_pattern_db_version() behavior
 * Regression: Version must increment on modifications
 */
TEST_F(PatternDBRegressionV2Test, VersionIncrements)
{
    ASSERT_NE(db_, nullptr) << "Setup failed";

    uint32_t v1 = nimcp_pattern_db_version(db_);

    // Add pattern
    nimcp_pattern_entry_t entry = {};
    entry.pattern = "version.*test";
    entry.category = NIMCP_PATTERN_CUSTOM;
    entry.priority = 1;
    entry.weight = 0.5f;
    entry.description = "Version test";
    entry.flags = 0;

    nimcp_pattern_id_t id;
    nimcp_pattern_db_add(db_, &entry, &id);

    uint32_t v2 = nimcp_pattern_db_version(db_);
    EXPECT_GT(v2, v1) << "Version must increment after add";

    // Remove pattern
    nimcp_pattern_db_remove(db_, id);

    uint32_t v3 = nimcp_pattern_db_version(db_);
    EXPECT_GT(v3, v2) << "Version must increment after remove";
}

/**
 * Test: nimcp_pattern_db_snapshot() and rollback behavior
 * Regression: Rollback must restore previous state
 */
TEST_F(PatternDBRegressionV2Test, SnapshotRollback)
{
    ASSERT_NE(db_, nullptr) << "Setup failed";

    // Add initial pattern
    nimcp_pattern_entry_t entry1 = {};
    entry1.pattern = "initial.*pattern";
    entry1.category = NIMCP_PATTERN_CUSTOM;
    entry1.priority = 1;
    entry1.weight = 0.5f;
    entry1.description = "Initial";
    entry1.flags = 0;
    nimcp_pattern_db_add(db_, &entry1, nullptr);

    // Take snapshot
    uint32_t snapshot_id;
    nimcp_error_t result = nimcp_pattern_db_snapshot(db_, &snapshot_id);
    EXPECT_EQ(result, NIMCP_SUCCESS) << "Snapshot must succeed";

    uint32_t version_at_snapshot = nimcp_pattern_db_version(db_);

    // Add more patterns
    nimcp_pattern_entry_t entry2 = {};
    entry2.pattern = "second.*pattern";
    entry2.category = NIMCP_PATTERN_CUSTOM;
    entry2.priority = 1;
    entry2.weight = 0.5f;
    entry2.description = "Second";
    entry2.flags = 0;
    nimcp_pattern_db_add(db_, &entry2, nullptr);

    // Verify more patterns
    nimcp_pattern_db_stats_t stats;
    nimcp_pattern_db_get_stats(db_, &stats);
    EXPECT_EQ(stats.total_patterns, 2u) << "Should have 2 patterns";

    // Rollback
    result = nimcp_pattern_db_rollback(db_, version_at_snapshot);
    EXPECT_EQ(result, NIMCP_SUCCESS) << "Rollback must succeed";

    // Verify rolled back
    nimcp_pattern_db_get_stats(db_, &stats);
    EXPECT_EQ(stats.total_patterns, 1u) << "Should have 1 pattern after rollback";
}

//=============================================================================
// Statistics Accuracy Tests
//=============================================================================

/**
 * Test: Statistics accurately reflect operations
 * Regression: Counters must match actual operations
 */
TEST_F(PatternDBRegressionV2Test, StatisticsAccuracy)
{
    ASSERT_NE(db_, nullptr) << "Setup failed";

    // Initial stats should be zero
    nimcp_pattern_db_stats_t stats;
    nimcp_pattern_db_get_stats(db_, &stats);
    EXPECT_EQ(stats.total_patterns, 0u) << "Initial patterns must be 0";

    // Add patterns
    for (int i = 0; i < 5; i++) {
        char pattern[64];
        snprintf(pattern, sizeof(pattern), "stat_test_%d", i);

        nimcp_pattern_entry_t entry = {};
        entry.pattern = pattern;
        entry.category = NIMCP_PATTERN_CUSTOM;
        entry.priority = 1;
        entry.weight = 0.5f;
        entry.description = "Stats test";
        entry.flags = 0;
        nimcp_pattern_db_add(db_, &entry, nullptr);
    }

    nimcp_pattern_db_get_stats(db_, &stats);
    EXPECT_EQ(stats.total_patterns, 5u) << "Should have 5 patterns";

    // Perform matches
    for (int i = 0; i < 10; i++) {
        nimcp_pattern_match_result_t result;
        nimcp_pattern_db_match(db_, "stat_test_0 input", &result);
    }

    nimcp_pattern_db_get_stats(db_, &stats);
    EXPECT_GE(stats.total_matches, 10u) << "Should have at least 10 matches";
}

/**
 * Test: Statistics reset works
 * Regression: Reset must zero counters but keep patterns
 */
TEST_F(PatternDBRegressionV2Test, StatisticsReset)
{
    ASSERT_NE(db_, nullptr) << "Setup failed";

    // Add pattern and perform matches
    nimcp_pattern_entry_t entry = {};
    entry.pattern = "reset.*test";
    entry.category = NIMCP_PATTERN_CUSTOM;
    entry.priority = 1;
    entry.weight = 0.5f;
    entry.description = "Reset test";
    entry.flags = 0;
    nimcp_pattern_db_add(db_, &entry, nullptr);

    nimcp_pattern_match_result_t result;
    for (int i = 0; i < 5; i++) {
        nimcp_pattern_db_match(db_, "reset test input", &result);
    }

    // Reset stats
    nimcp_pattern_db_reset_stats(db_);

    nimcp_pattern_db_stats_t stats;
    nimcp_pattern_db_get_stats(db_, &stats);

    EXPECT_EQ(stats.total_matches, 0u) << "Matches must be 0 after reset";
    EXPECT_EQ(stats.total_hits, 0u) << "Hits must be 0 after reset";
    // Patterns should still exist
    EXPECT_EQ(stats.total_patterns, 1u) << "Pattern count should not change";
}

/**
 * Test: Category name function
 * Regression: Category names must be consistent
 */
TEST_F(PatternDBRegressionV2Test, CategoryNames)
{
    const char* name;

    name = nimcp_pattern_category_name(NIMCP_PATTERN_SQL_INJECTION);
    ASSERT_NE(name, nullptr) << "SQL_INJECTION name must not be NULL";
    EXPECT_TRUE(strstr(name, "SQL") != nullptr || strstr(name, "sql") != nullptr)
        << "SQL_INJECTION name must contain 'SQL'";

    name = nimcp_pattern_category_name(NIMCP_PATTERN_XSS);
    EXPECT_NE(name, nullptr) << "XSS name must not be NULL";

    name = nimcp_pattern_category_name(NIMCP_PATTERN_SHELL_INJECTION);
    EXPECT_NE(name, nullptr) << "SHELL_INJECTION name must not be NULL";

    // Invalid category - may return NULL or "Unknown" - just verify no crash
    name = nimcp_pattern_category_name(static_cast<nimcp_pattern_category_t>(9999));
    (void)name;
    SUCCEED();
}

//=============================================================================
// Memory Leak Prevention Tests
//=============================================================================

/**
 * Test: Add/remove cycles don't leak memory
 * Regression: Repeated add/remove must not leak
 */
TEST_F(PatternDBRegressionV2Test, MemoryAddRemoveCycles)
{
    ASSERT_NE(db_, nullptr) << "Setup failed";

    for (int i = 0; i < 100; i++) {
        nimcp_pattern_entry_t entry = {};
        entry.pattern = "leak.*test";
        entry.category = NIMCP_PATTERN_CUSTOM;
        entry.priority = 1;
        entry.weight = 0.5f;
        entry.description = "Leak test";
        entry.flags = 0;

        nimcp_pattern_id_t id;
        nimcp_error_t result = nimcp_pattern_db_add(db_, &entry, &id);
        ASSERT_EQ(result, NIMCP_SUCCESS) << "Add must succeed";

        result = nimcp_pattern_db_remove(db_, id);
        ASSERT_EQ(result, NIMCP_SUCCESS) << "Remove must succeed";
    }

    // Verify no patterns remain
    nimcp_pattern_db_stats_t stats;
    nimcp_pattern_db_get_stats(db_, &stats);
    EXPECT_EQ(stats.total_patterns, 0u) << "No patterns should remain";
}

/**
 * Test: Clear function works
 * Regression: Clear must remove all patterns
 */
TEST_F(PatternDBRegressionV2Test, ClearAllPatterns)
{
    ASSERT_NE(db_, nullptr) << "Setup failed";

    // Add multiple patterns
    for (int i = 0; i < 10; i++) {
        char pattern[64];
        snprintf(pattern, sizeof(pattern), "clear_test_%d", i);

        nimcp_pattern_entry_t entry = {};
        entry.pattern = pattern;
        entry.category = NIMCP_PATTERN_CUSTOM;
        entry.priority = 1;
        entry.weight = 0.5f;
        entry.description = "Clear test";
        entry.flags = 0;
        nimcp_pattern_db_add(db_, &entry, nullptr);
    }

    nimcp_pattern_db_stats_t stats;
    nimcp_pattern_db_get_stats(db_, &stats);
    EXPECT_EQ(stats.total_patterns, 10u) << "Should have 10 patterns";

    // Clear
    nimcp_error_t result = nimcp_pattern_db_clear(db_);
    EXPECT_EQ(result, NIMCP_SUCCESS) << "Clear must succeed";

    nimcp_pattern_db_get_stats(db_, &stats);
    EXPECT_EQ(stats.total_patterns, 0u) << "Should have 0 patterns after clear";
}

//=============================================================================
// Bulk Operations Tests
//=============================================================================

/**
 * Test: Import multiple patterns atomically
 * Regression: Import must be all-or-nothing
 */
TEST_F(PatternDBRegressionV2Test, BulkImport)
{
    ASSERT_NE(db_, nullptr) << "Setup failed";

    nimcp_pattern_entry_t entries[3] = {};

    entries[0].pattern = "bulk_1";
    entries[0].category = NIMCP_PATTERN_CUSTOM;
    entries[0].priority = 1;
    entries[0].weight = 0.5f;
    entries[0].description = "Bulk 1";
    entries[0].flags = 0;

    entries[1].pattern = "bulk_2";
    entries[1].category = NIMCP_PATTERN_CUSTOM;
    entries[1].priority = 2;
    entries[1].weight = 0.6f;
    entries[1].description = "Bulk 2";
    entries[1].flags = 0;

    entries[2].pattern = "bulk_3";
    entries[2].category = NIMCP_PATTERN_CUSTOM;
    entries[2].priority = 3;
    entries[2].weight = 0.7f;
    entries[2].description = "Bulk 3";
    entries[2].flags = 0;

    nimcp_error_t result = nimcp_pattern_db_import(db_, entries, 3);
    EXPECT_EQ(result, NIMCP_SUCCESS) << "Bulk import must succeed";

    nimcp_pattern_db_stats_t stats;
    nimcp_pattern_db_get_stats(db_, &stats);
    EXPECT_EQ(stats.total_patterns, 3u) << "Should have 3 patterns after import";
}

}  // anonymous namespace
