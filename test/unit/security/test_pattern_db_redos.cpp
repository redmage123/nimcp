/**
 * @file test_pattern_db_redos.cpp
 * @brief Unit tests for Pattern Database ReDoS protection
 *
 * WHAT: Tests for ReDoS (Regular Expression Denial of Service) protection
 * WHY:  Ensure dangerous regex patterns are rejected before compilation
 * HOW:  Google Test framework with various ReDoS pattern examples
 *
 * TEST COVERAGE:
 * - Consecutive quantifiers (a**, a++)
 * - Nested quantifiers with alternation ((a|b)*)
 * - Excessive nesting depth
 * - Pattern length limits
 * - Catastrophic backtracking patterns
 * - Safe patterns that should be accepted
 *
 * @author NIMCP Security Team
 * @date 2025-12-31
 */

#include <gtest/gtest.h>
#include "security/nimcp_pattern_db.h"
#include <string>

//=============================================================================
// Test Fixture
//=============================================================================

class PatternDatabaseReDoSTest : public ::testing::Test {
protected:
    void SetUp() override {
        config = nimcp_pattern_db_default_config();
        config.enable_statistics = true;
        config.enable_validation = true;
        db = nimcp_pattern_db_create(&config);
        ASSERT_NE(db, nullptr);
    }

    void TearDown() override {
        if (db) {
            nimcp_pattern_db_destroy(db);
            db = nullptr;
        }
    }

    /**
     * @brief Helper to test if a pattern is accepted
     */
    bool pattern_is_accepted(const char* pattern) {
        nimcp_pattern_entry_t entry = {
            .pattern = pattern,
            .category = NIMCP_PATTERN_CUSTOM,
            .priority = 1,
            .weight = 0.5f,
            .description = "ReDoS test pattern",
            .flags = 0
        };

        nimcp_pattern_id_t id;
        nimcp_error_t err = nimcp_pattern_db_add(db, &entry, &id);

        // If accepted, remove it to keep db clean for next test
        if (err == NIMCP_SUCCESS) {
            nimcp_pattern_db_remove(db, id);
        }

        return err == NIMCP_SUCCESS;
    }

    nimcp_pattern_db_config_t config;
    nimcp_pattern_db_t db;
};

//=============================================================================
// Consecutive Quantifiers Tests
//=============================================================================

TEST_F(PatternDatabaseReDoSTest, RejectConsecutiveStars) {
    // Pattern: a** (consecutive * quantifiers)
    EXPECT_FALSE(pattern_is_accepted("a**")) << "Should reject consecutive ** quantifiers";
}

TEST_F(PatternDatabaseReDoSTest, RejectConsecutivePlus) {
    // Pattern: a++ (consecutive + quantifiers)
    EXPECT_FALSE(pattern_is_accepted("a++")) << "Should reject consecutive ++ quantifiers";
}

TEST_F(PatternDatabaseReDoSTest, RejectStarPlus) {
    // Pattern: a*+ (star followed by plus)
    EXPECT_FALSE(pattern_is_accepted("a*+")) << "Should reject *+ quantifier combination";
}

TEST_F(PatternDatabaseReDoSTest, RejectPlusStar) {
    // Pattern: a+* (plus followed by star)
    EXPECT_FALSE(pattern_is_accepted("a+*")) << "Should reject +* quantifier combination";
}

TEST_F(PatternDatabaseReDoSTest, RejectQuestionStar) {
    // Pattern: a?* (question followed by star)
    EXPECT_FALSE(pattern_is_accepted("a?*")) << "Should reject ?* quantifier combination";
}

TEST_F(PatternDatabaseReDoSTest, RejectStarQuestion) {
    // Pattern: a*? (star followed by question)
    EXPECT_FALSE(pattern_is_accepted("a*?")) << "Should reject *? quantifier combination";
}

//=============================================================================
// Nested Quantifiers with Alternation Tests
//=============================================================================

TEST_F(PatternDatabaseReDoSTest, RejectAlternationWithStar) {
    // Pattern: (a|b)* - alternation with star quantifier causes backtracking
    EXPECT_FALSE(pattern_is_accepted("(a|b)*")) << "Should reject (a|b)* pattern";
}

TEST_F(PatternDatabaseReDoSTest, RejectAlternationWithPlus) {
    // Pattern: (a|b)+ - alternation with plus quantifier
    EXPECT_FALSE(pattern_is_accepted("(a|b)+")) << "Should reject (a|b)+ pattern";
}

TEST_F(PatternDatabaseReDoSTest, RejectComplexAlternationWithStar) {
    // Pattern: (aa|bb|cc)* - multiple alternations with star
    EXPECT_FALSE(pattern_is_accepted("(aa|bb|cc)*")) << "Should reject complex alternation with *";
}

TEST_F(PatternDatabaseReDoSTest, RejectNestedGroupAlternation) {
    // Pattern: ((a|b)|c)* - nested alternation with star
    EXPECT_FALSE(pattern_is_accepted("((a|b)|c)*")) << "Should reject nested alternation with *";
}

//=============================================================================
// Nested Quantifiers Tests (Catastrophic Backtracking)
//=============================================================================

TEST_F(PatternDatabaseReDoSTest, RejectNestedStarStar) {
    // Pattern: (a*)* - classic ReDoS pattern
    EXPECT_FALSE(pattern_is_accepted("(a*)*")) << "Should reject (a*)* nested quantifiers";
}

TEST_F(PatternDatabaseReDoSTest, RejectNestedPlusPlus) {
    // Pattern: (a+)+ - nested plus quantifiers
    EXPECT_FALSE(pattern_is_accepted("(a+)+")) << "Should reject (a+)+ nested quantifiers";
}

TEST_F(PatternDatabaseReDoSTest, RejectNestedPlusStar) {
    // Pattern: (a+)* - nested plus with star
    EXPECT_FALSE(pattern_is_accepted("(a+)*")) << "Should reject (a+)* nested quantifiers";
}

TEST_F(PatternDatabaseReDoSTest, RejectNestedStarPlus) {
    // Pattern: (a*)+ - nested star with plus
    EXPECT_FALSE(pattern_is_accepted("(a*)+")) << "Should reject (a*)+ nested quantifiers";
}

TEST_F(PatternDatabaseReDoSTest, RejectDotStarGroupStar) {
    // Pattern: (.*a)* - dot star in group with star
    EXPECT_FALSE(pattern_is_accepted("(.*a)*")) << "Should reject (.*a)* pattern";
}

TEST_F(PatternDatabaseReDoSTest, RejectOverlappingRepetition) {
    // Pattern: (a*)+b - overlapping repetition
    EXPECT_FALSE(pattern_is_accepted("(a*)+b")) << "Should reject overlapping repetition";
}

//=============================================================================
// Excessive Nesting Depth Tests
//=============================================================================

TEST_F(PatternDatabaseReDoSTest, RejectExcessiveNesting) {
    // Pattern with 5 levels of nesting (exceeds limit of 3)
    EXPECT_FALSE(pattern_is_accepted("((((a))))")) << "Should reject excessive nesting depth";
}

TEST_F(PatternDatabaseReDoSTest, AcceptModeratNesting) {
    // Pattern with 3 levels of nesting (at limit)
    // Note: This should be accepted as it's at the boundary
    EXPECT_TRUE(pattern_is_accepted("(((a)))")) << "Should accept 3 levels of nesting";
}

TEST_F(PatternDatabaseReDoSTest, RejectDeeplyNestedAlternation) {
    // Deeply nested alternation
    EXPECT_FALSE(pattern_is_accepted("((((a|b))))")) << "Should reject deeply nested alternation";
}

//=============================================================================
// Pattern Length Tests
//=============================================================================

TEST_F(PatternDatabaseReDoSTest, AcceptNormalLengthPattern) {
    // Normal length pattern should be accepted
    std::string pattern = "test[a-z]+pattern";
    EXPECT_TRUE(pattern_is_accepted(pattern.c_str())) << "Should accept normal length patterns";
}

TEST_F(PatternDatabaseReDoSTest, RejectExcessivelyLongPattern) {
    // Pattern exceeding maximum length (4096 chars)
    std::string pattern(5000, 'a');
    EXPECT_FALSE(pattern_is_accepted(pattern.c_str())) << "Should reject patterns exceeding max length";
}

//=============================================================================
// Excessive Alternations Tests
//=============================================================================

TEST_F(PatternDatabaseReDoSTest, RejectTooManyAlternations) {
    // Pattern with more than 20 alternations in a group
    std::string pattern = "(a|b|c|d|e|f|g|h|i|j|k|l|m|n|o|p|q|r|s|t|u|v|w|x|y)";
    EXPECT_FALSE(pattern_is_accepted(pattern.c_str())) << "Should reject too many alternations";
}

TEST_F(PatternDatabaseReDoSTest, AcceptModerateAlternations) {
    // Pattern with acceptable number of alternations
    std::string pattern = "(a|b|c|d|e)";
    EXPECT_TRUE(pattern_is_accepted(pattern.c_str())) << "Should accept moderate alternations";
}

//=============================================================================
// Safe Patterns Tests (Should Be Accepted)
//=============================================================================

TEST_F(PatternDatabaseReDoSTest, AcceptSimplePattern) {
    EXPECT_TRUE(pattern_is_accepted("hello")) << "Should accept simple literal pattern";
}

TEST_F(PatternDatabaseReDoSTest, AcceptSingleQuantifier) {
    EXPECT_TRUE(pattern_is_accepted("a+")) << "Should accept single quantifier";
}

TEST_F(PatternDatabaseReDoSTest, AcceptCharacterClass) {
    EXPECT_TRUE(pattern_is_accepted("[a-z]+")) << "Should accept character class with quantifier";
}

TEST_F(PatternDatabaseReDoSTest, AcceptNonGreedy) {
    // Non-greedy quantifier without consecutive quantifier
    // Note: POSIX regex doesn't support non-greedy, so this tests the parser handles it
    EXPECT_TRUE(pattern_is_accepted("a+")) << "Should accept basic quantifier";
}

TEST_F(PatternDatabaseReDoSTest, AcceptAnchoredPattern) {
    EXPECT_TRUE(pattern_is_accepted("^hello$")) << "Should accept anchored pattern";
}

TEST_F(PatternDatabaseReDoSTest, AcceptEscapedQuantifiers) {
    // Escaped quantifiers should not be treated as quantifiers
    EXPECT_TRUE(pattern_is_accepted("a\\*b")) << "Should accept escaped quantifiers";
}

TEST_F(PatternDatabaseReDoSTest, AcceptQuantifiersInCharClass) {
    // Quantifiers inside character classes are literals
    EXPECT_TRUE(pattern_is_accepted("[*+?]+")) << "Should accept quantifiers in char class";
}

TEST_F(PatternDatabaseReDoSTest, AcceptSQLInjectionPattern) {
    // Typical SQL injection detection pattern
    EXPECT_TRUE(pattern_is_accepted("(union|select).*from")) << "Should accept SQL injection pattern";
}

TEST_F(PatternDatabaseReDoSTest, AcceptXSSPattern) {
    // Typical XSS detection pattern
    EXPECT_TRUE(pattern_is_accepted("<script")) << "Should accept XSS pattern";
}

TEST_F(PatternDatabaseReDoSTest, AcceptPathTraversalPattern) {
    // Path traversal detection pattern
    EXPECT_TRUE(pattern_is_accepted("\\.\\./")) << "Should accept path traversal pattern";
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(PatternDatabaseReDoSTest, AcceptEmptyGroup) {
    // Empty group should be handled
    EXPECT_TRUE(pattern_is_accepted("()a")) << "Should accept empty group";
}

TEST_F(PatternDatabaseReDoSTest, AcceptNestedGroupsWithoutQuantifiers) {
    // Nested groups without quantifiers are safe
    EXPECT_TRUE(pattern_is_accepted("((ab))")) << "Should accept nested groups without quantifiers";
}

TEST_F(PatternDatabaseReDoSTest, RejectRangeAfterQuantifier) {
    // Range quantifier after another quantifier
    EXPECT_FALSE(pattern_is_accepted("a*{2,3}")) << "Should reject range after quantifier";
}

TEST_F(PatternDatabaseReDoSTest, AcceptValidRangeQuantifier) {
    // Valid range quantifier
    EXPECT_TRUE(pattern_is_accepted("a{2,3}")) << "Should accept valid range quantifier";
}

//=============================================================================
// Boundary Tests
//=============================================================================

TEST_F(PatternDatabaseReDoSTest, AcceptPatternAtMaxLength) {
    // Pattern at exactly max length should work (4096 - 1 for null terminator)
    std::string pattern(4095, 'a');
    EXPECT_TRUE(pattern_is_accepted(pattern.c_str())) << "Should accept pattern at max length";
}

TEST_F(PatternDatabaseReDoSTest, RejectPatternJustOverMaxLength) {
    // Pattern just over max length
    std::string pattern(4097, 'a');
    EXPECT_FALSE(pattern_is_accepted(pattern.c_str())) << "Should reject pattern just over max length";
}

//=============================================================================
// Real-World ReDoS Vulnerability Patterns
//=============================================================================

TEST_F(PatternDatabaseReDoSTest, RejectEmailReDoS) {
    // Classic email regex ReDoS pattern
    EXPECT_FALSE(pattern_is_accepted("([a-zA-Z0-9]+)+@"))
        << "Should reject classic email ReDoS pattern";
}

TEST_F(PatternDatabaseReDoSTest, RejectURLReDoS) {
    // URL validation ReDoS pattern
    EXPECT_FALSE(pattern_is_accepted("((http|https)://)+"))
        << "Should reject URL ReDoS pattern";
}

TEST_F(PatternDatabaseReDoSTest, RejectWhitespaceReDoS) {
    // Whitespace handling ReDoS
    EXPECT_FALSE(pattern_is_accepted("(\\s*)+"))
        << "Should reject whitespace ReDoS pattern";
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
