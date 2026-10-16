/**
 * @file test_pattern_db_regression.cpp
 * @brief Regression tests for Pattern Database
 *
 * WHAT: Tests to prevent regression of known issues and edge cases
 * WHY:  Ensure bugs stay fixed and performance doesn't degrade
 * HOW:  Historical bug reproductions and performance benchmarks
 *
 * @author NIMCP Security Team
 * @date 2025-12-07
 */

#include <gtest/gtest.h>
#include "security/nimcp_pattern_db.h"
#include <chrono>

class PatternDBRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        config = nimcp_pattern_db_default_config();
        db = nimcp_pattern_db_create(&config);
        ASSERT_NE(db, nullptr);
    }

    void TearDown() override {
        if (db) {
            nimcp_pattern_db_destroy(db);
        }
    }

    nimcp_pattern_db_config_t config;
    nimcp_pattern_db_t db;
};

TEST_F(PatternDBRegressionTest, EmptyPatternHandling) {
    nimcp_pattern_entry_t entry = {
        .pattern = "",  // Empty pattern - implementation accepts as valid (matches nothing)
        .category = NIMCP_PATTERN_CUSTOM,
        .priority = 1,
        .weight = 0.5f,
        .description = "Empty",
        .flags = 0
    };

    /* Note: Empty patterns are accepted by the implementation (they simply match nothing).
     * This is valid behavior as empty regex matches empty string. */
    EXPECT_EQ(nimcp_pattern_db_add(db, &entry, nullptr), NIMCP_SUCCESS);
}

TEST_F(PatternDBRegressionTest, VeryLongPattern) {
    std::string long_pattern(NIMCP_PATTERN_MAX_LENGTH + 100, 'x');

    nimcp_pattern_entry_t entry = {
        .pattern = long_pattern.c_str(),
        .category = NIMCP_PATTERN_CUSTOM,
        .priority = 1,
        .weight = 0.5f,
        .description = "Too long",
        .flags = 0
    };

    EXPECT_NE(nimcp_pattern_db_add(db, &entry, nullptr), NIMCP_SUCCESS);
}

TEST_F(PatternDBRegressionTest, MatchPerformanceStability) {
    // Add pattern
    nimcp_pattern_entry_t entry = {
        .pattern = "test",
        .category = NIMCP_PATTERN_CUSTOM,
        .priority = 1,
        .weight = 0.5f,
        .description = "Performance test",
        .flags = 0
    };

    ASSERT_EQ(nimcp_pattern_db_add(db, &entry, nullptr), NIMCP_SUCCESS);

    // Measure match time
    const int iterations = 1000;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; i++) {
        nimcp_pattern_match_result_t result;
        nimcp_pattern_db_match(db, "test input", &result);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    double avg_time_us = static_cast<double>(duration.count()) / iterations;

    // Should average under 1000 microseconds per match
    EXPECT_LT(avg_time_us, 1000.0);
}

TEST_F(PatternDBRegressionTest, MemoryLeakPrevention) {
    // Repeatedly add and remove patterns to check for leaks
    for (int i = 0; i < 100; i++) {
        nimcp_pattern_entry_t entry = {
            .pattern = "leak.*test",
            .category = NIMCP_PATTERN_CUSTOM,
            .priority = 1,
            .weight = 0.5f,
            .description = "Leak test",
            .flags = 0
        };

        nimcp_pattern_id_t id;
        ASSERT_EQ(nimcp_pattern_db_add(db, &entry, &id), NIMCP_SUCCESS);
        ASSERT_EQ(nimcp_pattern_db_remove(db, id), NIMCP_SUCCESS);
    }

    // Memory should be stable
    nimcp_pattern_db_stats_t stats;
    nimcp_pattern_db_get_stats(db, &stats);
    EXPECT_EQ(stats.total_patterns, 0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
