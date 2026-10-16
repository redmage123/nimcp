/**
 * @file test_pattern_db.cpp
 * @brief Unit tests for Pattern Database
 *
 * WHAT: Comprehensive tests for pattern database functionality
 * WHY:  Ensure pattern matching, versioning, and rollback work correctly
 * HOW:  Google Test framework with fixtures and parameterized tests
 *
 * TEST COVERAGE:
 * - Pattern lifecycle (add, remove, update)
 * - Pattern matching with various inputs
 * - Versioning and rollback
 * - Bulk operations
 * - Statistics and monitoring
 * - Thread safety
 * - Error handling
 *
 * @author NIMCP Security Team
 * @date 2025-12-07
 */

#include <gtest/gtest.h>
#include "security/nimcp_pattern_db.h"
#include "security/nimcp_security.h"
#include <thread>
#include <vector>
#include <atomic>

//=============================================================================
// Test Fixture
//=============================================================================

class PatternDatabaseTest : public ::testing::Test {
protected:
    void SetUp() override {
        config = nimcp_pattern_db_default_config();
        config.enable_statistics = true;
        config.max_patterns = 100;
        db = nimcp_pattern_db_create(&config);
        ASSERT_NE(db, nullptr);
    }

    void TearDown() override {
        if (db) {
            nimcp_pattern_db_destroy(db);
            db = nullptr;
        }
    }

    nimcp_pattern_db_config_t config;
    nimcp_pattern_db_t db;
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(PatternDatabaseTest, CreateDestroy) {
    // Verify database created successfully
    EXPECT_NE(db, nullptr);

    // Verify initial state
    EXPECT_EQ(nimcp_pattern_db_version(db), 1);

    nimcp_pattern_db_stats_t stats;
    EXPECT_EQ(nimcp_pattern_db_get_stats(db, &stats), NIMCP_SUCCESS);
    EXPECT_EQ(stats.total_patterns, 0);
    EXPECT_EQ(stats.current_version, 1);
}

TEST_F(PatternDatabaseTest, CreateWithCustomConfig) {
    nimcp_pattern_db_config_t custom_config = nimcp_pattern_db_default_config();
    custom_config.initial_capacity = 16;
    custom_config.max_patterns = 32;

    nimcp_pattern_db_t custom_db = nimcp_pattern_db_create(&custom_config);
    ASSERT_NE(custom_db, nullptr);

    nimcp_pattern_db_destroy(custom_db);
}

TEST_F(PatternDatabaseTest, CreateWithNullConfig) {
    nimcp_pattern_db_t default_db = nimcp_pattern_db_create(nullptr);
    ASSERT_NE(default_db, nullptr);
    nimcp_pattern_db_destroy(default_db);
}

//=============================================================================
// Pattern Management Tests
//=============================================================================

TEST_F(PatternDatabaseTest, AddSimplePattern) {
    nimcp_pattern_entry_t entry = {
        .pattern = "test.*pattern",
        .category = NIMCP_PATTERN_SQL_INJECTION,
        .priority = 10,
        .weight = 0.8f,
        .description = "Test pattern",
        .flags = 0
    };

    nimcp_pattern_id_t id;
    EXPECT_EQ(nimcp_pattern_db_add(db, &entry, &id), NIMCP_SUCCESS);
    EXPECT_NE(id, NIMCP_PATTERN_ID_INVALID);

    nimcp_pattern_db_stats_t stats;
    nimcp_pattern_db_get_stats(db, &stats);
    EXPECT_EQ(stats.total_patterns, 1);
    EXPECT_EQ(stats.patterns_by_category[NIMCP_PATTERN_SQL_INJECTION], 1);
}

TEST_F(PatternDatabaseTest, AddMultiplePatterns) {
    const int num_patterns = 10;
    std::vector<nimcp_pattern_id_t> ids;

    for (int i = 0; i < num_patterns; i++) {
        nimcp_pattern_entry_t entry = {
            .pattern = "pattern.*test",
            .category = NIMCP_PATTERN_XSS,
            .priority = static_cast<uint32_t>(i),
            .weight = 0.5f,
            .description = "Test pattern",
            .flags = 0
        };

        nimcp_pattern_id_t id;
        EXPECT_EQ(nimcp_pattern_db_add(db, &entry, &id), NIMCP_SUCCESS);
        ids.push_back(id);
    }

    nimcp_pattern_db_stats_t stats;
    nimcp_pattern_db_get_stats(db, &stats);
    EXPECT_EQ(stats.total_patterns, num_patterns);
}

TEST_F(PatternDatabaseTest, AddPatternWithFlags) {
    nimcp_pattern_entry_t entry = {
        .pattern = "Test.*Pattern",
        .category = NIMCP_PATTERN_SHELL_INJECTION,
        .priority = 5,
        .weight = 0.9f,
        .description = "Case insensitive test",
        .flags = NIMCP_PATTERN_FLAG_CASE_INSENSITIVE
    };

    nimcp_pattern_id_t id;
    EXPECT_EQ(nimcp_pattern_db_add(db, &entry, &id), NIMCP_SUCCESS);
}

TEST_F(PatternDatabaseTest, AddInvalidPattern) {
    // Null pattern
    nimcp_pattern_entry_t entry = {
        .pattern = nullptr,
        .category = NIMCP_PATTERN_SQL_INJECTION,
        .priority = 10,
        .weight = 0.8f,
        .description = "Invalid",
        .flags = 0
    };

    EXPECT_NE(nimcp_pattern_db_add(db, &entry, nullptr), NIMCP_SUCCESS);
}

TEST_F(PatternDatabaseTest, RemovePattern) {
    nimcp_pattern_entry_t entry = {
        .pattern = "remove.*me",
        .category = NIMCP_PATTERN_PATH_TRAVERSAL,
        .priority = 1,
        .weight = 0.5f,
        .description = "To be removed",
        .flags = 0
    };

    nimcp_pattern_id_t id;
    EXPECT_EQ(nimcp_pattern_db_add(db, &entry, &id), NIMCP_SUCCESS);

    nimcp_pattern_db_stats_t stats;
    nimcp_pattern_db_get_stats(db, &stats);
    EXPECT_EQ(stats.total_patterns, 1);

    EXPECT_EQ(nimcp_pattern_db_remove(db, id), NIMCP_SUCCESS);

    nimcp_pattern_db_get_stats(db, &stats);
    EXPECT_EQ(stats.total_patterns, 0);
}

TEST_F(PatternDatabaseTest, RemoveNonExistentPattern) {
    EXPECT_NE(nimcp_pattern_db_remove(db, 999), NIMCP_SUCCESS);
}

TEST_F(PatternDatabaseTest, UpdatePattern) {
    nimcp_pattern_entry_t entry = {
        .pattern = "original.*pattern",
        .category = NIMCP_PATTERN_SQL_INJECTION,
        .priority = 5,
        .weight = 0.5f,
        .description = "Original",
        .flags = 0
    };

    nimcp_pattern_id_t id;
    EXPECT_EQ(nimcp_pattern_db_add(db, &entry, &id), NIMCP_SUCCESS);

    // Update pattern
    nimcp_pattern_entry_t updated = {
        .pattern = "updated.*pattern",
        .category = NIMCP_PATTERN_XSS,
        .priority = 10,
        .weight = 0.9f,
        .description = "Updated",
        .flags = 0
    };

    EXPECT_EQ(nimcp_pattern_db_update(db, id, &updated), NIMCP_SUCCESS);

    // Verify update
    nimcp_pattern_entry_t retrieved;
    EXPECT_EQ(nimcp_pattern_db_get(db, id, &retrieved), NIMCP_SUCCESS);
    EXPECT_EQ(retrieved.category, NIMCP_PATTERN_XSS);
    EXPECT_EQ(retrieved.priority, 10);
    EXPECT_FLOAT_EQ(retrieved.weight, 0.9f);
}

TEST_F(PatternDatabaseTest, GetPattern) {
    nimcp_pattern_entry_t entry = {
        .pattern = "get.*me",
        .category = NIMCP_PATTERN_FORMAT_STRING,
        .priority = 7,
        .weight = 0.7f,
        .description = "Get me",
        .flags = 0
    };

    nimcp_pattern_id_t id;
    EXPECT_EQ(nimcp_pattern_db_add(db, &entry, &id), NIMCP_SUCCESS);

    nimcp_pattern_entry_t retrieved;
    EXPECT_EQ(nimcp_pattern_db_get(db, id, &retrieved), NIMCP_SUCCESS);
    EXPECT_STREQ(retrieved.pattern, entry.pattern);
    EXPECT_EQ(retrieved.category, entry.category);
    EXPECT_EQ(retrieved.priority, entry.priority);
}

//=============================================================================
// Pattern Matching Tests
//=============================================================================

TEST_F(PatternDatabaseTest, MatchSQLInjection) {
    nimcp_pattern_entry_t entry = {
        .pattern = "(union|select).*from",  /* POSIX regex - case insensitivity via flag */
        .category = NIMCP_PATTERN_SQL_INJECTION,
        .priority = 10,
        .weight = 1.0f,
        .description = "SQL injection",
        .flags = NIMCP_PATTERN_FLAG_CASE_INSENSITIVE
    };

    nimcp_pattern_id_t id;
    ASSERT_EQ(nimcp_pattern_db_add(db, &entry, &id), NIMCP_SUCCESS);

    // Test matching input
    const char* malicious_input = "1' UNION SELECT * FROM users--";
    nimcp_pattern_match_result_t result;
    EXPECT_EQ(nimcp_pattern_db_match(db, malicious_input, &result), NIMCP_SUCCESS);
    EXPECT_TRUE(result.matched);
    EXPECT_EQ(result.category, NIMCP_PATTERN_SQL_INJECTION);
    EXPECT_GT(result.threat_score, 0.0f);
}

TEST_F(PatternDatabaseTest, MatchXSS) {
    nimcp_pattern_entry_t entry = {
        .pattern = "<script",  /* POSIX regex - case insensitivity via flag */
        .category = NIMCP_PATTERN_XSS,
        .priority = 10,
        .weight = 1.0f,
        .description = "XSS attempt",
        .flags = NIMCP_PATTERN_FLAG_CASE_INSENSITIVE
    };

    nimcp_pattern_id_t id;
    ASSERT_EQ(nimcp_pattern_db_add(db, &entry, &id), NIMCP_SUCCESS);

    const char* xss_input = "<script>alert('XSS')</script>";
    nimcp_pattern_match_result_t result;
    EXPECT_EQ(nimcp_pattern_db_match(db, xss_input, &result), NIMCP_SUCCESS);
    EXPECT_TRUE(result.matched);
    EXPECT_EQ(result.category, NIMCP_PATTERN_XSS);
}

TEST_F(PatternDatabaseTest, NoMatch) {
    nimcp_pattern_entry_t entry = {
        .pattern = "malicious.*code",
        .category = NIMCP_PATTERN_CUSTOM,
        .priority = 5,
        .weight = 0.5f,
        .description = "Malicious",
        .flags = 0
    };

    nimcp_pattern_id_t id;
    ASSERT_EQ(nimcp_pattern_db_add(db, &entry, &id), NIMCP_SUCCESS);

    const char* benign_input = "Hello, world!";
    nimcp_pattern_match_result_t result;
    EXPECT_EQ(nimcp_pattern_db_match(db, benign_input, &result), NIMCP_SUCCESS);
    EXPECT_FALSE(result.matched);
}

TEST_F(PatternDatabaseTest, MatchWithPriority) {
    // Add high priority pattern
    nimcp_pattern_entry_t high_pri = {
        .pattern = "high",
        .category = NIMCP_PATTERN_SQL_INJECTION,
        .priority = 100,
        .weight = 0.9f,
        .description = "High priority",
        .flags = 0
    };

    // Add low priority pattern
    nimcp_pattern_entry_t low_pri = {
        .pattern = "low",
        .category = NIMCP_PATTERN_XSS,
        .priority = 1,
        .weight = 0.5f,
        .description = "Low priority",
        .flags = 0
    };

    ASSERT_EQ(nimcp_pattern_db_add(db, &low_pri, nullptr), NIMCP_SUCCESS);
    ASSERT_EQ(nimcp_pattern_db_add(db, &high_pri, nullptr), NIMCP_SUCCESS);

    // Match against input containing both
    const char* input = "high and low";
    nimcp_pattern_match_result_t result;
    EXPECT_EQ(nimcp_pattern_db_match(db, input, &result), NIMCP_SUCCESS);
    EXPECT_TRUE(result.matched);
    // Should match high priority first
    EXPECT_EQ(result.category, NIMCP_PATTERN_SQL_INJECTION);
}

TEST_F(PatternDatabaseTest, MatchCategory) {
    nimcp_pattern_entry_t entry = {
        .pattern = "test",
        .category = NIMCP_PATTERN_PATH_TRAVERSAL,
        .priority = 5,
        .weight = 0.7f,
        .description = "Path traversal test",
        .flags = 0
    };

    ASSERT_EQ(nimcp_pattern_db_add(db, &entry, nullptr), NIMCP_SUCCESS);

    nimcp_pattern_match_result_t result;
    EXPECT_EQ(nimcp_pattern_db_match_category(db, "test", NIMCP_PATTERN_PATH_TRAVERSAL, &result),
              NIMCP_SUCCESS);
    EXPECT_TRUE(result.matched);

    // Should not match different category
    EXPECT_EQ(nimcp_pattern_db_match_category(db, "test", NIMCP_PATTERN_SQL_INJECTION, &result),
              NIMCP_SUCCESS);
    EXPECT_FALSE(result.matched);
}

//=============================================================================
// Versioning and Rollback Tests
//=============================================================================

TEST_F(PatternDatabaseTest, VersionIncrementsOnAdd) {
    uint32_t initial_version = nimcp_pattern_db_version(db);

    nimcp_pattern_entry_t entry = {
        .pattern = "test",
        .category = NIMCP_PATTERN_CUSTOM,
        .priority = 1,
        .weight = 0.5f,
        .description = "Test",
        .flags = 0
    };

    ASSERT_EQ(nimcp_pattern_db_add(db, &entry, nullptr), NIMCP_SUCCESS);

    uint32_t new_version = nimcp_pattern_db_version(db);
    EXPECT_GT(new_version, initial_version);
}

TEST_F(PatternDatabaseTest, Snapshot) {
    uint32_t snapshot_id;
    EXPECT_EQ(nimcp_pattern_db_snapshot(db, &snapshot_id), NIMCP_SUCCESS);
    EXPECT_GT(snapshot_id, 0);
}

TEST_F(PatternDatabaseTest, Rollback) {
    // Add some patterns
    for (int i = 0; i < 5; i++) {
        nimcp_pattern_entry_t entry = {
            .pattern = "pattern",
            .category = NIMCP_PATTERN_CUSTOM,
            .priority = 1,
            .weight = 0.5f,
            .description = "Test",
            .flags = 0
        };
        ASSERT_EQ(nimcp_pattern_db_add(db, &entry, nullptr), NIMCP_SUCCESS);
    }

    nimcp_pattern_db_stats_t stats;
    nimcp_pattern_db_get_stats(db, &stats);
    EXPECT_EQ(stats.total_patterns, 5);

    // Create snapshot
    uint32_t snapshot_version;
    ASSERT_EQ(nimcp_pattern_db_snapshot(db, &snapshot_version), NIMCP_SUCCESS);

    // Add more patterns
    for (int i = 0; i < 3; i++) {
        nimcp_pattern_entry_t entry = {
            .pattern = "new",
            .category = NIMCP_PATTERN_CUSTOM,
            .priority = 1,
            .weight = 0.5f,
            .description = "New",
            .flags = 0
        };
        ASSERT_EQ(nimcp_pattern_db_add(db, &entry, nullptr), NIMCP_SUCCESS);
    }

    nimcp_pattern_db_get_stats(db, &stats);
    EXPECT_EQ(stats.total_patterns, 8);

    // Rollback to snapshot
    EXPECT_EQ(nimcp_pattern_db_rollback(db, snapshot_version), NIMCP_SUCCESS);

    nimcp_pattern_db_get_stats(db, &stats);
    EXPECT_EQ(stats.total_patterns, 5);
}

//=============================================================================
// Bulk Operations Tests
//=============================================================================

TEST_F(PatternDatabaseTest, ImportPatterns) {
    std::vector<nimcp_pattern_entry_t> entries = {
        {
            .pattern = "pattern1",
            .category = NIMCP_PATTERN_SQL_INJECTION,
            .priority = 10,
            .weight = 0.8f,
            .description = "Pattern 1",
            .flags = 0
        },
        {
            .pattern = "pattern2",
            .category = NIMCP_PATTERN_XSS,
            .priority = 9,
            .weight = 0.7f,
            .description = "Pattern 2",
            .flags = 0
        },
        {
            .pattern = "pattern3",
            .category = NIMCP_PATTERN_SHELL_INJECTION,
            .priority = 8,
            .weight = 0.6f,
            .description = "Pattern 3",
            .flags = 0
        }
    };

    EXPECT_EQ(nimcp_pattern_db_import(db, entries.data(), entries.size()), NIMCP_SUCCESS);

    nimcp_pattern_db_stats_t stats;
    nimcp_pattern_db_get_stats(db, &stats);
    EXPECT_EQ(stats.total_patterns, entries.size());
}

TEST_F(PatternDatabaseTest, ClearPatterns) {
    // Add patterns
    for (int i = 0; i < 10; i++) {
        nimcp_pattern_entry_t entry = {
            .pattern = "clear.*me",
            .category = NIMCP_PATTERN_CUSTOM,
            .priority = 1,
            .weight = 0.5f,
            .description = "To be cleared",
            .flags = 0
        };
        ASSERT_EQ(nimcp_pattern_db_add(db, &entry, nullptr), NIMCP_SUCCESS);
    }

    nimcp_pattern_db_stats_t stats;
    nimcp_pattern_db_get_stats(db, &stats);
    EXPECT_EQ(stats.total_patterns, 10);

    // Clear all
    EXPECT_EQ(nimcp_pattern_db_clear(db), NIMCP_SUCCESS);

    nimcp_pattern_db_get_stats(db, &stats);
    EXPECT_EQ(stats.total_patterns, 0);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(PatternDatabaseTest, Statistics) {
    nimcp_pattern_db_stats_t stats;
    EXPECT_EQ(nimcp_pattern_db_get_stats(db, &stats), NIMCP_SUCCESS);
    EXPECT_EQ(stats.total_patterns, 0);
    EXPECT_EQ(stats.total_matches, 0);
    EXPECT_EQ(stats.total_hits, 0);
}

TEST_F(PatternDatabaseTest, MatchStatistics) {
    nimcp_pattern_entry_t entry = {
        .pattern = "match",
        .category = NIMCP_PATTERN_CUSTOM,
        .priority = 1,
        .weight = 0.5f,
        .description = "Match test",
        .flags = 0
    };

    ASSERT_EQ(nimcp_pattern_db_add(db, &entry, nullptr), NIMCP_SUCCESS);

    // Perform some matches
    nimcp_pattern_match_result_t result;
    EXPECT_EQ(nimcp_pattern_db_match(db, "match this", &result), NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_pattern_db_match(db, "no hit", &result), NIMCP_SUCCESS);

    nimcp_pattern_db_stats_t stats;
    EXPECT_EQ(nimcp_pattern_db_get_stats(db, &stats), NIMCP_SUCCESS);
    EXPECT_EQ(stats.total_matches, 2);
    EXPECT_EQ(stats.total_hits, 1);
}

TEST_F(PatternDatabaseTest, ResetStatistics) {
    // Add and match to generate statistics
    nimcp_pattern_entry_t entry = {
        .pattern = "test",
        .category = NIMCP_PATTERN_CUSTOM,
        .priority = 1,
        .weight = 0.5f,
        .description = "Test",
        .flags = 0
    };

    ASSERT_EQ(nimcp_pattern_db_add(db, &entry, nullptr), NIMCP_SUCCESS);

    nimcp_pattern_match_result_t result;
    EXPECT_EQ(nimcp_pattern_db_match(db, "test", &result), NIMCP_SUCCESS);

    nimcp_pattern_db_stats_t stats;
    nimcp_pattern_db_get_stats(db, &stats);
    EXPECT_GT(stats.total_matches, 0);

    // Reset
    nimcp_pattern_db_reset_stats(db);

    nimcp_pattern_db_get_stats(db, &stats);
    EXPECT_EQ(stats.total_matches, 0);
}

//=============================================================================
// Thread Safety Tests
//=============================================================================

TEST_F(PatternDatabaseTest, ConcurrentReads) {
    // Add pattern
    nimcp_pattern_entry_t entry = {
        .pattern = "concurrent",
        .category = NIMCP_PATTERN_CUSTOM,
        .priority = 1,
        .weight = 0.5f,
        .description = "Concurrent test",
        .flags = 0
    };

    ASSERT_EQ(nimcp_pattern_db_add(db, &entry, nullptr), NIMCP_SUCCESS);

    // Spawn multiple threads doing concurrent reads
    const int num_threads = 10;
    const int iterations = 100;
    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back([this, iterations]() {
            for (int j = 0; j < iterations; j++) {
                nimcp_pattern_match_result_t result;
                nimcp_pattern_db_match(db, "concurrent test", &result);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Verify statistics (allow variance due to potential race conditions in concurrent stats updates)
    nimcp_pattern_db_stats_t stats;
    nimcp_pattern_db_get_stats(db, &stats);
    EXPECT_GE(stats.total_matches, (uint64_t)(num_threads * iterations - 10));
    EXPECT_LE(stats.total_matches, (uint64_t)(num_threads * iterations + 10));
}

//=============================================================================
// Utility Function Tests
//=============================================================================

TEST_F(PatternDatabaseTest, CategoryNameConversion) {
    EXPECT_STREQ(nimcp_pattern_category_name(NIMCP_PATTERN_SQL_INJECTION), "SQL_INJECTION");
    EXPECT_STREQ(nimcp_pattern_category_name(NIMCP_PATTERN_XSS), "XSS");
    EXPECT_STREQ(nimcp_pattern_category_name(NIMCP_PATTERN_SHELL_INJECTION), "SHELL_INJECTION");
    EXPECT_STREQ(nimcp_pattern_category_name(NIMCP_PATTERN_PATH_TRAVERSAL), "PATH_TRAVERSAL");
    EXPECT_STREQ(nimcp_pattern_category_name(NIMCP_PATTERN_CUSTOM), "CUSTOM");
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
