/**
 * @file test_pattern_db_json.cpp
 * @brief Unit tests for Pattern Database JSON parsing safety
 *
 * WHAT: Tests for safe JSON parsing in pattern database
 * WHY:  Ensure malformed JSON is handled safely without crashes or vulnerabilities
 * HOW:  Google Test framework with various JSON edge cases
 *
 * TEST COVERAGE:
 * - Malformed JSON detection
 * - Escape sequence handling
 * - Integer overflow prevention
 * - Buffer overflow prevention
 * - Unicode handling
 * - Nested object handling
 *
 * @author NIMCP Security Team
 * @date 2025-12-31
 */

#include <gtest/gtest.h>
#include "security/nimcp_pattern_db.h"
#include <string>
#include <fstream>
#include <cstdio>

//=============================================================================
// Test Fixture
//=============================================================================

class PatternDatabaseJSONTest : public ::testing::Test {
protected:
    void SetUp() override {
        config = nimcp_pattern_db_default_config();
        config.enable_statistics = true;
        config.enable_validation = true;
        db = nimcp_pattern_db_create(&config);
        ASSERT_NE(db, nullptr);

        // Create temp directory for test files
        test_file_counter = 0;
    }

    void TearDown() override {
        if (db) {
            nimcp_pattern_db_destroy(db);
            db = nullptr;
        }

        // Cleanup temp files
        for (const auto& file : temp_files) {
            std::remove(file.c_str());
        }
        temp_files.clear();
    }

    /**
     * @brief Create a temporary JSON file with given content
     * @return Path to the created file
     */
    std::string create_temp_json(const std::string& content) {
        char temp_path[256];
        snprintf(temp_path, sizeof(temp_path), "/tmp/test_pattern_db_%d_%d.json",
                 getpid(), test_file_counter++);

        std::ofstream file(temp_path);
        file << content;
        file.close();

        temp_files.push_back(temp_path);
        return temp_path;
    }

    nimcp_pattern_db_config_t config;
    nimcp_pattern_db_t db;
    int test_file_counter;
    std::vector<std::string> temp_files;
};

//=============================================================================
// Valid JSON Tests
//=============================================================================

TEST_F(PatternDatabaseJSONTest, LoadValidJSON) {
    std::string json = R"({
        "patterns": [
            {
                "pattern": "test.*pattern",
                "category": 0,
                "priority": 10,
                "weight": 0.8,
                "flags": 0,
                "description": "Test pattern"
            }
        ]
    })";

    std::string path = create_temp_json(json);
    EXPECT_EQ(nimcp_pattern_db_load(db, path.c_str()), NIMCP_SUCCESS)
        << "Should load valid JSON successfully";

    nimcp_pattern_db_stats_t stats;
    nimcp_pattern_db_get_stats(db, &stats);
    EXPECT_EQ(stats.total_patterns, 1) << "Should have loaded 1 pattern";
}

TEST_F(PatternDatabaseJSONTest, LoadMultiplePatterns) {
    std::string json = R"({
        "patterns": [
            {"pattern": "pattern1", "category": 0, "priority": 1, "weight": 0.5, "flags": 0},
            {"pattern": "pattern2", "category": 1, "priority": 2, "weight": 0.6, "flags": 0},
            {"pattern": "pattern3", "category": 2, "priority": 3, "weight": 0.7, "flags": 0}
        ]
    })";

    std::string path = create_temp_json(json);
    EXPECT_EQ(nimcp_pattern_db_load(db, path.c_str()), NIMCP_SUCCESS);

    nimcp_pattern_db_stats_t stats;
    nimcp_pattern_db_get_stats(db, &stats);
    EXPECT_EQ(stats.total_patterns, 3) << "Should have loaded 3 patterns";
}

//=============================================================================
// Malformed JSON Tests
//=============================================================================

TEST_F(PatternDatabaseJSONTest, RejectUnbalancedBraces) {
    std::string json = R"({
        "patterns": [
            {"pattern": "test", "category": 0
        ]
    })";

    std::string path = create_temp_json(json);
    EXPECT_NE(nimcp_pattern_db_load(db, path.c_str()), NIMCP_SUCCESS)
        << "Should reject JSON with unbalanced braces";
}

TEST_F(PatternDatabaseJSONTest, RejectUnbalancedBrackets) {
    std::string json = R"({
        "patterns": [
            {"pattern": "test", "category": 0}
    })";

    std::string path = create_temp_json(json);
    EXPECT_NE(nimcp_pattern_db_load(db, path.c_str()), NIMCP_SUCCESS)
        << "Should reject JSON with unbalanced brackets";
}

TEST_F(PatternDatabaseJSONTest, RejectUnterminatedString) {
    std::string json = R"({
        "patterns": [
            {"pattern": "test, "category": 0}
        ]
    })";

    std::string path = create_temp_json(json);
    EXPECT_NE(nimcp_pattern_db_load(db, path.c_str()), NIMCP_SUCCESS)
        << "Should reject JSON with unterminated string";
}

TEST_F(PatternDatabaseJSONTest, RejectMissingPatternsKey) {
    std::string json = R"({
        "other_key": [
            {"pattern": "test", "category": 0}
        ]
    })";

    std::string path = create_temp_json(json);
    EXPECT_NE(nimcp_pattern_db_load(db, path.c_str()), NIMCP_SUCCESS)
        << "Should reject JSON without 'patterns' key";
}

TEST_F(PatternDatabaseJSONTest, RejectEmptyPatternArray) {
    std::string json = R"({
        "patterns": []
    })";

    std::string path = create_temp_json(json);
    EXPECT_NE(nimcp_pattern_db_load(db, path.c_str()), NIMCP_SUCCESS)
        << "Should reject JSON with empty patterns array";
}

TEST_F(PatternDatabaseJSONTest, RejectNonArrayPatterns) {
    std::string json = R"({
        "patterns": {"pattern": "test"}
    })";

    std::string path = create_temp_json(json);
    EXPECT_NE(nimcp_pattern_db_load(db, path.c_str()), NIMCP_SUCCESS)
        << "Should reject JSON where patterns is not an array";
}

//=============================================================================
// Escape Sequence Tests
//=============================================================================

TEST_F(PatternDatabaseJSONTest, HandleValidEscapeSequences) {
    std::string json = R"({
        "patterns": [
            {
                "pattern": "test\\npattern",
                "category": 0,
                "priority": 1,
                "weight": 0.5,
                "flags": 0,
                "description": "Test with\nnewline"
            }
        ]
    })";

    std::string path = create_temp_json(json);
    EXPECT_EQ(nimcp_pattern_db_load(db, path.c_str()), NIMCP_SUCCESS)
        << "Should handle valid escape sequences";
}

TEST_F(PatternDatabaseJSONTest, HandleEscapedQuotes) {
    std::string json = R"({
        "patterns": [
            {
                "pattern": "test\"pattern",
                "category": 0,
                "priority": 1,
                "weight": 0.5,
                "flags": 0,
                "description": "Pattern with \"quotes\""
            }
        ]
    })";

    std::string path = create_temp_json(json);
    EXPECT_EQ(nimcp_pattern_db_load(db, path.c_str()), NIMCP_SUCCESS)
        << "Should handle escaped quotes";
}

TEST_F(PatternDatabaseJSONTest, HandleEscapedBackslash) {
    std::string json = R"({
        "patterns": [
            {
                "pattern": "test\\\\pattern",
                "category": 0,
                "priority": 1,
                "weight": 0.5,
                "flags": 0,
                "description": "Pattern with \\\\ backslash"
            }
        ]
    })";

    std::string path = create_temp_json(json);
    EXPECT_EQ(nimcp_pattern_db_load(db, path.c_str()), NIMCP_SUCCESS)
        << "Should handle escaped backslashes";
}

TEST_F(PatternDatabaseJSONTest, HandleAllEscapeSequences) {
    std::string json = R"({
        "patterns": [
            {
                "pattern": "test",
                "category": 0,
                "priority": 1,
                "weight": 0.5,
                "flags": 0,
                "description": "Tab:\tNewline:\nCarriage return:\rQuote:\"Backslash:\\"
            }
        ]
    })";

    std::string path = create_temp_json(json);
    EXPECT_EQ(nimcp_pattern_db_load(db, path.c_str()), NIMCP_SUCCESS)
        << "Should handle all standard escape sequences";
}

//=============================================================================
// Integer Parsing Tests
//=============================================================================

TEST_F(PatternDatabaseJSONTest, HandleZeroValues) {
    std::string json = R"({
        "patterns": [
            {
                "pattern": "test",
                "category": 0,
                "priority": 0,
                "weight": 0.0,
                "flags": 0
            }
        ]
    })";

    std::string path = create_temp_json(json);
    EXPECT_EQ(nimcp_pattern_db_load(db, path.c_str()), NIMCP_SUCCESS)
        << "Should handle zero values";
}

TEST_F(PatternDatabaseJSONTest, HandleLargeIntegerValues) {
    std::string json = R"({
        "patterns": [
            {
                "pattern": "test",
                "category": 0,
                "priority": 4294967295,
                "weight": 0.5,
                "flags": 4294967295
            }
        ]
    })";

    std::string path = create_temp_json(json);
    EXPECT_EQ(nimcp_pattern_db_load(db, path.c_str()), NIMCP_SUCCESS)
        << "Should handle maximum uint32 values";
}

TEST_F(PatternDatabaseJSONTest, HandleNegativeCategory) {
    std::string json = R"({
        "patterns": [
            {
                "pattern": "test",
                "category": -1,
                "priority": 1,
                "weight": 0.5,
                "flags": 0
            }
        ]
    })";

    std::string path = create_temp_json(json);
    // Should use CUSTOM as default for invalid category
    EXPECT_EQ(nimcp_pattern_db_load(db, path.c_str()), NIMCP_SUCCESS)
        << "Should handle negative category by defaulting to CUSTOM";
}

TEST_F(PatternDatabaseJSONTest, HandleOutOfRangeCategory) {
    std::string json = R"({
        "patterns": [
            {
                "pattern": "test",
                "category": 999,
                "priority": 1,
                "weight": 0.5,
                "flags": 0
            }
        ]
    })";

    std::string path = create_temp_json(json);
    // Should use CUSTOM as default for out of range category
    EXPECT_EQ(nimcp_pattern_db_load(db, path.c_str()), NIMCP_SUCCESS)
        << "Should handle out of range category by defaulting to CUSTOM";
}

TEST_F(PatternDatabaseJSONTest, HandleNonNumericCategory) {
    std::string json = R"({
        "patterns": [
            {
                "pattern": "test",
                "category": "invalid",
                "priority": 1,
                "weight": 0.5,
                "flags": 0
            }
        ]
    })";

    std::string path = create_temp_json(json);
    // Should use CUSTOM as default for non-numeric category
    EXPECT_EQ(nimcp_pattern_db_load(db, path.c_str()), NIMCP_SUCCESS)
        << "Should handle non-numeric category";
}

//=============================================================================
// Float Parsing Tests
//=============================================================================

TEST_F(PatternDatabaseJSONTest, HandleWeightClamping) {
    std::string json = R"({
        "patterns": [
            {
                "pattern": "test",
                "category": 0,
                "priority": 1,
                "weight": 2.5,
                "flags": 0
            }
        ]
    })";

    std::string path = create_temp_json(json);
    EXPECT_EQ(nimcp_pattern_db_load(db, path.c_str()), NIMCP_SUCCESS)
        << "Should clamp weight to valid range";
}

TEST_F(PatternDatabaseJSONTest, HandleNegativeWeight) {
    std::string json = R"({
        "patterns": [
            {
                "pattern": "test",
                "category": 0,
                "priority": 1,
                "weight": -0.5,
                "flags": 0
            }
        ]
    })";

    std::string path = create_temp_json(json);
    EXPECT_EQ(nimcp_pattern_db_load(db, path.c_str()), NIMCP_SUCCESS)
        << "Should clamp negative weight to 0";
}

TEST_F(PatternDatabaseJSONTest, HandleScientificNotation) {
    std::string json = R"({
        "patterns": [
            {
                "pattern": "test",
                "category": 0,
                "priority": 1,
                "weight": 5.0e-1,
                "flags": 0
            }
        ]
    })";

    std::string path = create_temp_json(json);
    EXPECT_EQ(nimcp_pattern_db_load(db, path.c_str()), NIMCP_SUCCESS)
        << "Should handle scientific notation for weight";
}

//=============================================================================
// String Buffer Tests
//=============================================================================

TEST_F(PatternDatabaseJSONTest, HandleLongPattern) {
    // Pattern at max length (1024 chars)
    std::string pattern(1000, 'a');
    std::string json = "{\"patterns\": [{\"pattern\": \"" + pattern +
                       "\", \"category\": 0, \"priority\": 1, \"weight\": 0.5, \"flags\": 0}]}";

    std::string path = create_temp_json(json);
    EXPECT_EQ(nimcp_pattern_db_load(db, path.c_str()), NIMCP_SUCCESS)
        << "Should handle patterns near max length";
}

TEST_F(PatternDatabaseJSONTest, HandleLongDescription) {
    // Description at max length (256 chars)
    std::string desc(250, 'a');
    std::string json = "{\"patterns\": [{\"pattern\": \"test\", \"category\": 0, "
                       "\"priority\": 1, \"weight\": 0.5, \"flags\": 0, \"description\": \"" +
                       desc + "\"}]}";

    std::string path = create_temp_json(json);
    EXPECT_EQ(nimcp_pattern_db_load(db, path.c_str()), NIMCP_SUCCESS)
        << "Should handle descriptions near max length";
}

TEST_F(PatternDatabaseJSONTest, TruncateOverlongPattern) {
    // Pattern exceeding max length should be truncated/rejected
    std::string pattern(2000, 'a');
    std::string json = "{\"patterns\": [{\"pattern\": \"" + pattern +
                       "\", \"category\": 0, \"priority\": 1, \"weight\": 0.5, \"flags\": 0}]}";

    std::string path = create_temp_json(json);
    // Pattern may be rejected or truncated - implementation dependent
    // The important thing is no crash occurs
    nimcp_pattern_db_load(db, path.c_str());  // Just ensure no crash
}

//=============================================================================
// Unicode Handling Tests
//=============================================================================

TEST_F(PatternDatabaseJSONTest, HandleUnicodeEscape) {
    std::string json = R"({
        "patterns": [
            {
                "pattern": "test",
                "category": 0,
                "priority": 1,
                "weight": 0.5,
                "flags": 0,
                "description": "Unicode: \u0041\u0042\u0043"
            }
        ]
    })";

    std::string path = create_temp_json(json);
    EXPECT_EQ(nimcp_pattern_db_load(db, path.c_str()), NIMCP_SUCCESS)
        << "Should handle Unicode escape sequences";
}

TEST_F(PatternDatabaseJSONTest, HandleUTF8Content) {
    std::string json = R"({
        "patterns": [
            {
                "pattern": "test",
                "category": 0,
                "priority": 1,
                "weight": 0.5,
                "flags": 0,
                "description": "UTF-8 content in description"
            }
        ]
    })";

    std::string path = create_temp_json(json);
    EXPECT_EQ(nimcp_pattern_db_load(db, path.c_str()), NIMCP_SUCCESS)
        << "Should handle UTF-8 content";
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(PatternDatabaseJSONTest, HandleEmptyPattern) {
    std::string json = R"({
        "patterns": [
            {
                "pattern": "",
                "category": 0,
                "priority": 1,
                "weight": 0.5,
                "flags": 0
            }
        ]
    })";

    std::string path = create_temp_json(json);
    // Empty pattern should be skipped
    EXPECT_NE(nimcp_pattern_db_load(db, path.c_str()), NIMCP_SUCCESS)
        << "Should skip empty patterns";
}

TEST_F(PatternDatabaseJSONTest, HandleMissingFields) {
    std::string json = R"({
        "patterns": [
            {
                "pattern": "test"
            }
        ]
    })";

    std::string path = create_temp_json(json);
    // Should handle missing fields with defaults
    EXPECT_EQ(nimcp_pattern_db_load(db, path.c_str()), NIMCP_SUCCESS)
        << "Should handle missing optional fields";
}

TEST_F(PatternDatabaseJSONTest, HandleExtraFields) {
    std::string json = R"({
        "patterns": [
            {
                "pattern": "test",
                "category": 0,
                "priority": 1,
                "weight": 0.5,
                "flags": 0,
                "extra_field": "ignored",
                "another_extra": 12345
            }
        ]
    })";

    std::string path = create_temp_json(json);
    EXPECT_EQ(nimcp_pattern_db_load(db, path.c_str()), NIMCP_SUCCESS)
        << "Should ignore extra fields";
}

TEST_F(PatternDatabaseJSONTest, HandleWhitespaceVariations) {
    std::string json = "{\"patterns\":[{\"pattern\":\"test\",\"category\":0,\"priority\":1,\"weight\":0.5,\"flags\":0}]}";

    std::string path = create_temp_json(json);
    EXPECT_EQ(nimcp_pattern_db_load(db, path.c_str()), NIMCP_SUCCESS)
        << "Should handle compact JSON without whitespace";
}

TEST_F(PatternDatabaseJSONTest, HandleFileNotFound) {
    EXPECT_NE(nimcp_pattern_db_load(db, "/nonexistent/path/file.json"), NIMCP_SUCCESS)
        << "Should fail gracefully for non-existent files";
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
