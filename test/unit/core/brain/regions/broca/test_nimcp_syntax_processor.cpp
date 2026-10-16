/**
 * @file test_nimcp_syntax_processor.cpp
 * @brief Unit tests for nimcp_syntax_processor.c
 *
 * WHAT: Comprehensive unit tests for the syntax processing module
 * WHY:  Ensure correct grammatical parsing, morphological processing, and tree construction
 * HOW:  Use Google Test framework to test lifecycle, unit management, tree building,
 *       grammar validation, and morphological operations with various inputs and edge cases.
 *
 * COVERAGE TARGET: 100%
 */

#include <gtest/gtest.h>
#include <string.h> // For memset, strcmp
#include <stdlib.h> // For NULL

#include "core/brain/regions/broca/nimcp_syntax_processor.h"

// Helper function to create a basic syntactic unit
syntactic_unit_t create_unit(part_of_speech_t pos, uint32_t word_id, uint8_t number, uint8_t person, uint8_t tense) {
    syntactic_unit_t unit;
    memset(&unit, 0, sizeof(syntactic_unit_t));
    unit.pos = pos;
    unit.word_id = word_id;
    unit.features.number = number;
    unit.features.person = person;
    unit.features.tense = tense;
    return unit;
}

// Test Fixture for Syntax Processor
class SyntaxProcessorTest : public ::testing::Test {
protected:
    syntax_processor_t* processor;
    syntax_config_t config;

    void SetUp() override {
        // Use default config, but with small max_units for easier testing of limits
        config = syntax_default_config();
        config.max_units = 5; // Small buffer for testing full/reset
        config.max_rules = 25; // Enough for 18 default rules + testing
        processor = syntax_create(&config);
        ASSERT_NE(nullptr, processor) << "Failed to create syntax processor";
    }

    void TearDown() override {
        syntax_destroy(processor);
        processor = nullptr;
    }
};

// =============================================================================
// Lifecycle Tests
// =============================================================================

TEST_F(SyntaxProcessorTest, CreateAndDestroy) {
    // Processor created and destroyed in SetUp/TearDown
    ASSERT_NE(nullptr, processor);
    SUCCEED();
}

TEST_F(SyntaxProcessorTest, CreateWithNullConfig) {
    syntax_processor_t* null_config_processor = syntax_create(nullptr);
    ASSERT_NE(nullptr, null_config_processor) << "Should create with default config";
    syntax_destroy(null_config_processor);
}

// =============================================================================
// Unit Management Tests
// =============================================================================

TEST_F(SyntaxProcessorTest, AddUnits) {
    syntactic_unit_t unit1 = create_unit(POS_NOUN, 1, 0, 0, 0); // Word "cat"
    syntactic_unit_t unit2 = create_unit(POS_VERB, 2, 0, 0, 0); // Word "eats"

    EXPECT_TRUE(syntax_add_unit(processor, &unit1));
    EXPECT_EQ(1, syntax_get_unit_count(processor));
    EXPECT_TRUE(syntax_add_unit(processor, &unit2));
    EXPECT_EQ(2, syntax_get_unit_count(processor));
}

TEST_F(SyntaxProcessorTest, AddUnitNullInputs) {
    syntactic_unit_t unit1 = create_unit(POS_NOUN, 1, 0, 0, 0);
    EXPECT_FALSE(syntax_add_unit(nullptr, &unit1)) << "Should fail with NULL processor";
    EXPECT_FALSE(syntax_add_unit(processor, nullptr)) << "Should fail with NULL unit";
    EXPECT_EQ(0, syntax_get_unit_count(processor));
}

TEST_F(SyntaxProcessorTest, AddUnitLimit) {
    for (uint32_t i = 0; i < config.max_units; ++i) {
        syntactic_unit_t unit = create_unit(POS_NOUN, i, 0, 0, 0);
        ASSERT_TRUE(syntax_add_unit(processor, &unit)) << "Should add unit " << i;
    }
    EXPECT_EQ(config.max_units, syntax_get_unit_count(processor));

    syntactic_unit_t extra_unit = create_unit(POS_NOUN, 99, 0, 0, 0);
    EXPECT_FALSE(syntax_add_unit(processor, &extra_unit)) << "Should fail to add beyond max_units";
    EXPECT_EQ(config.max_units, syntax_get_unit_count(processor));
}

TEST_F(SyntaxProcessorTest, ResetProcessor) {
    syntactic_unit_t unit1 = create_unit(POS_NOUN, 1, 0, 0, 0);
    syntax_add_unit(processor, &unit1);
    ASSERT_EQ(1, syntax_get_unit_count(processor));

    EXPECT_TRUE(syntax_reset(processor));
    EXPECT_EQ(0, syntax_get_unit_count(processor));
    EXPECT_EQ(nullptr, syntax_get_tree_root(processor)) << "Tree root should be NULL after reset";
}

TEST_F(SyntaxProcessorTest, ResetNullProcessor) {
    EXPECT_FALSE(syntax_reset(nullptr)) << "Should fail with NULL processor";
}

// =============================================================================
// Tree Building and Grammar Tests
// =============================================================================

TEST_F(SyntaxProcessorTest, BuildSimpleTree) {
    // Sentence: "Cat sleeps"
    syntactic_unit_t unit1 = create_unit(POS_NOUN, 1, 1, 3, 0); // Cat (singular, 3rd person)
    syntactic_unit_t unit2 = create_unit(POS_VERB, 2, 1, 3, 1); // sleeps (singular, 3rd person, present)
    syntax_add_unit(processor, &unit1);
    syntax_add_unit(processor, &unit2);

    ASSERT_TRUE(syntax_build_tree(processor)) << "Should successfully build tree for 'Cat sleeps'";
    const syntax_tree_node_t* root = syntax_get_tree_root(processor);
    ASSERT_NE(nullptr, root);
    EXPECT_EQ(PHRASE_IP, root->unit.phrase_type) << "Root should be IP (Sentence)";

    bool is_valid = false;
    EXPECT_TRUE(syntax_validate_grammar(processor, &is_valid));
    EXPECT_TRUE(is_valid) << "Grammar should be valid for 'Cat sleeps'";
}

TEST_F(SyntaxProcessorTest, BuildInvalidTree) {
    // Sentence: "Sleeps Cat" (grammatically incorrect order for simple rules)
    syntactic_unit_t unit1 = create_unit(POS_VERB, 1, 1, 3, 1); // Sleeps
    syntactic_unit_t unit2 = create_unit(POS_NOUN, 2, 1, 3, 0); // Cat
    syntax_add_unit(processor, &unit1);
    syntax_add_unit(processor, &unit2);

    ASSERT_FALSE(syntax_build_tree(processor)) << "Should fail to build tree for 'Sleeps Cat'";
    EXPECT_EQ(nullptr, syntax_get_tree_root(processor));

    bool is_valid = true; // Assume valid initially
    EXPECT_TRUE(syntax_validate_grammar(processor, &is_valid)); // Check for agreement might still run
    EXPECT_TRUE(is_valid) << "Grammar validation might still pass if no agreement violation, but tree build failed";
}

TEST_F(SyntaxProcessorTest, ValidateAgreementSuccess) {
    // Sentence: "Cats sleep"
    syntactic_unit_t unit1 = create_unit(POS_NOUN, 1, 2, 3, 0); // Cats (plural, 3rd person)
    syntactic_unit_t unit2 = create_unit(POS_VERB, 2, 2, 3, 1); // sleep (plural, 3rd person, present)
    syntax_add_unit(processor, &unit1);
    syntax_add_unit(processor, &unit2);

    bool is_valid = false;
    EXPECT_TRUE(syntax_validate_grammar(processor, &is_valid));
    EXPECT_TRUE(is_valid) << "Agreement should pass for 'Cats sleep'";
}

TEST_F(SyntaxProcessorTest, ValidateAgreementFailure) {
    // Sentence: "Cat sleep" (agreement failure: singular noun, plural verb)
    syntactic_unit_t unit1 = create_unit(POS_NOUN, 1, 1, 3, 0); // Cat (singular, 3rd person)
    syntactic_unit_t unit2 = create_unit(POS_VERB, 2, 2, 3, 1); // sleep (plural, 3rd person, present)
    syntax_add_unit(processor, &unit1);
    syntax_add_unit(processor, &unit2);

    bool is_valid = true;
    EXPECT_TRUE(syntax_validate_grammar(processor, &is_valid));
    EXPECT_FALSE(is_valid) << "Agreement should fail for 'Cat sleep'";

    // Check stats (agreement violations only increment if validation detects a problem)
    syntax_stats_t stats;
    syntax_get_stats(processor, &stats);
    EXPECT_EQ(1, stats.agreement_violations) << "Agreement violations stat should increment";
}

TEST_F(SyntaxProcessorTest, ValidateAgreementNullInputs) {
    bool is_valid = true;
    EXPECT_FALSE(syntax_validate_grammar(nullptr, &is_valid)) << "Should fail with NULL processor";
    EXPECT_FALSE(syntax_validate_grammar(processor, nullptr)) << "Should fail with NULL is_valid pointer";
}

TEST_F(SyntaxProcessorTest, GetTreeRootAndDepth) {
    EXPECT_EQ(nullptr, syntax_get_tree_root(processor));
    EXPECT_EQ(0, syntax_get_tree_depth(processor));

    syntactic_unit_t unit1 = create_unit(POS_NOUN, 1, 1, 3, 0);
    syntactic_unit_t unit2 = create_unit(POS_VERB, 2, 1, 3, 1);
    syntax_add_unit(processor, &unit1);
    syntax_add_unit(processor, &unit2);
    syntax_build_tree(processor);

    ASSERT_NE(nullptr, syntax_get_tree_root(processor));
    // With recursive tree building: root (depth 0) + children (depth 1) = tree depth 2
    EXPECT_GE(syntax_get_tree_depth(processor), 1) << "Tree should have depth >= 1";
}

TEST_F(SyntaxProcessorTest, GetTreeNullInputs) {
    EXPECT_EQ(nullptr, syntax_get_tree_root(nullptr));
    EXPECT_EQ(0, syntax_get_tree_depth(nullptr));
}

// =============================================================================
// Morphological Processing Tests
// =============================================================================

TEST_F(SyntaxProcessorTest, ApplyNounInflectionPlural) {
    syntactic_unit_t unit_cat = create_unit(POS_NOUN, 100, 2, 0, 0); // "cat" + plural feature
    char inflected[32];
    EXPECT_TRUE(syntax_apply_inflection(processor, &unit_cat, inflected, sizeof(inflected)));
    EXPECT_STREQ("word_100-s", inflected);
}

TEST_F(SyntaxProcessorTest, ApplyVerbInflectionPast) {
    syntactic_unit_t unit_walk = create_unit(POS_VERB, 200, 0, 0, 2); // "walk" + past tense
    char inflected[32];
    EXPECT_TRUE(syntax_apply_inflection(processor, &unit_walk, inflected, sizeof(inflected)));
    EXPECT_STREQ("word_200-ed", inflected);
}

TEST_F(SyntaxProcessorTest, ApplyVerbInflectionThirdSingular) {
    syntactic_unit_t unit_eat = create_unit(POS_VERB, 300, 1, 3, 1); // "eat" + 3rd singular present
    char inflected[32];
    EXPECT_TRUE(syntax_apply_inflection(processor, &unit_eat, inflected, sizeof(inflected)));
    EXPECT_STREQ("word_300-s", inflected);
}

TEST_F(SyntaxProcessorTest, ApplyInflectionDisabled) {
    config.enable_morphology = false;
    syntax_destroy(processor); // Recreate with new config
    processor = syntax_create(&config);
    ASSERT_NE(nullptr, processor);

    syntactic_unit_t unit_cat = create_unit(POS_NOUN, 100, 2, 0, 0);
    char inflected[32];
    EXPECT_TRUE(syntax_apply_inflection(processor, &unit_cat, inflected, sizeof(inflected)));
    EXPECT_STREQ("word_100", inflected) << "Should not inflect if morphology is disabled";
}

TEST_F(SyntaxProcessorTest, ApplyInflectionNullInputs) {
    syntactic_unit_t unit_cat = create_unit(POS_NOUN, 100, 2, 0, 0);
    char inflected[32];
    EXPECT_FALSE(syntax_apply_inflection(nullptr, &unit_cat, inflected, sizeof(inflected))) << "NULL processor";
    EXPECT_FALSE(syntax_apply_inflection(processor, nullptr, inflected, sizeof(inflected))) << "NULL unit";
    EXPECT_FALSE(syntax_apply_inflection(processor, &unit_cat, nullptr, sizeof(inflected))) << "NULL inflected_form buffer";
}

TEST_F(SyntaxProcessorTest, DecomposeMorphemesSimple) {
    char morphemes[SYNTAX_MAX_MORPHEMES][32];
    uint32_t num_morphemes = 0;

    EXPECT_TRUE(syntax_decompose_morphemes(processor, "walked", morphemes, SYNTAX_MAX_MORPHEMES, &num_morphemes));
    EXPECT_EQ(2, num_morphemes);
    EXPECT_STREQ("walk", morphemes[0]);
    EXPECT_STREQ("-ed", morphemes[1]);

    EXPECT_TRUE(syntax_decompose_morphemes(processor, "cats", morphemes, SYNTAX_MAX_MORPHEMES, &num_morphemes));
    EXPECT_EQ(2, num_morphemes);
    EXPECT_STREQ("cat", morphemes[0]);
    EXPECT_STREQ("-s", morphemes[1]);
}

TEST_F(SyntaxProcessorTest, DecomposeMorphemesNoSuffix) {
    char morphemes[SYNTAX_MAX_MORPHEMES][32];
    uint32_t num_morphemes = 0;

    EXPECT_TRUE(syntax_decompose_morphemes(processor, "house", morphemes, SYNTAX_MAX_MORPHEMES, &num_morphemes));
    EXPECT_EQ(1, num_morphemes);
    EXPECT_STREQ("house", morphemes[0]);
}

TEST_F(SyntaxProcessorTest, DecomposeMorphemesDisabled) {
    config.enable_morphology = false;
    syntax_destroy(processor); // Recreate with new config
    processor = syntax_create(&config);
    ASSERT_NE(nullptr, processor);

    char morphemes[SYNTAX_MAX_MORPHEMES][32];
    uint32_t num_morphemes = 0;
    EXPECT_FALSE(syntax_decompose_morphemes(processor, "walked", morphemes, SYNTAX_MAX_MORPHEMES, &num_morphemes)) << "Should fail if morphology disabled";
}

TEST_F(SyntaxProcessorTest, DecomposeMorphemesNullInputs) {
    char morphemes[SYNTAX_MAX_MORPHEMES][32];
    uint32_t num_morphemes = 0;
    EXPECT_FALSE(syntax_decompose_morphemes(nullptr, "word", morphemes, SYNTAX_MAX_MORPHEMES, &num_morphemes)) << "NULL processor";
    EXPECT_FALSE(syntax_decompose_morphemes(processor, nullptr, morphemes, SYNTAX_MAX_MORPHEMES, &num_morphemes)) << "NULL word";
    EXPECT_FALSE(syntax_decompose_morphemes(processor, "word", nullptr, SYNTAX_MAX_MORPHEMES, &num_morphemes)) << "NULL morphemes buffer";
    EXPECT_FALSE(syntax_decompose_morphemes(processor, "word", morphemes, SYNTAX_MAX_MORPHEMES, nullptr)) << "NULL num_morphemes pointer";
    EXPECT_FALSE(syntax_decompose_morphemes(processor, "word", morphemes, 0, &num_morphemes)) << "Zero max_morphemes";
    EXPECT_FALSE(syntax_decompose_morphemes(processor, "", morphemes, SYNTAX_MAX_MORPHEMES, &num_morphemes)) << "Empty word";
}

// =============================================================================
// Grammar Rule Management Tests
// =============================================================================

TEST_F(SyntaxProcessorTest, AddCustomRule) {
    phrase_structure_rule_t custom_rule;
    memset(&custom_rule, 0, sizeof(phrase_structure_rule_t));
    custom_rule.lhs = PHRASE_ADVP;
    custom_rule.rhs[0] = PHRASE_ADVP; // Corrected from PHRASE_ADVERB
    custom_rule.num_rhs = 1;
    custom_rule.probability = 1.0f;
    custom_rule.is_active = true;

    uint32_t initial_rule_count = syntax_get_rule_count(processor);
    EXPECT_TRUE(syntax_add_rule(processor, &custom_rule));
    EXPECT_EQ(initial_rule_count + 1, syntax_get_rule_count(processor));

    // Try adding beyond max_rules
    for (uint32_t i = initial_rule_count + 1; i < config.max_rules; ++i) {
        ASSERT_TRUE(syntax_add_rule(processor, &custom_rule));
    }
    EXPECT_EQ(config.max_rules, syntax_get_rule_count(processor));
    EXPECT_FALSE(syntax_add_rule(processor, &custom_rule)) << "Should not add rule if table is full";
}

TEST_F(SyntaxProcessorTest, AddRuleNullInputs) {
    phrase_structure_rule_t rule;
    memset(&rule, 0, sizeof(phrase_structure_rule_t));
    EXPECT_FALSE(syntax_add_rule(nullptr, &rule)) << "NULL processor";
    EXPECT_FALSE(syntax_add_rule(processor, nullptr)) << "NULL rule";
}

TEST_F(SyntaxProcessorTest, LoadDefaultRules) {
    syntax_destroy(processor); // Destroy to clear previous rules
    processor = syntax_create(nullptr); // Create with default config, which loads rules
    ASSERT_NE(nullptr, processor);

    // Default rules are loaded at creation, verify count
    EXPECT_GT(syntax_get_rule_count(processor), 0);

    // Calling again should reset and reload
    EXPECT_TRUE(syntax_load_default_rules(processor));
    EXPECT_GT(syntax_get_rule_count(processor), 0);
}

TEST_F(SyntaxProcessorTest, LoadDefaultRulesNullProcessor) {
    EXPECT_FALSE(syntax_load_default_rules(nullptr));
}

// =============================================================================
// Statistics and Utility Tests
// =============================================================================

TEST_F(SyntaxProcessorTest, GetStats) {
    syntax_stats_t stats;
    EXPECT_TRUE(syntax_get_stats(processor, &stats));
    EXPECT_EQ(0, stats.sentences_processed);
    EXPECT_EQ(0, stats.successful_parses);
    EXPECT_EQ(0, stats.failed_parses);
    EXPECT_EQ(0, stats.agreement_violations);
    EXPECT_EQ(0, stats.morphological_ops);

    // NOUN (singular) + VERB (plural) - structural parse succeeds, but agreement fails
    syntactic_unit_t unit1 = create_unit(POS_NOUN, 1, 1, 3, 0);  // Singular noun
    syntactic_unit_t unit2 = create_unit(POS_VERB, 2, 2, 3, 1);  // Plural verb (agreement mismatch)
    syntax_add_unit(processor, &unit1);
    syntax_add_unit(processor, &unit2);
    syntax_build_tree(processor);  // Structural parse succeeds: NP + VP -> IP
    bool is_valid = false;
    syntax_validate_grammar(processor, &is_valid);  // This will trigger agreement violation
    char inflected_buf[32] = {0};
    syntax_apply_inflection(processor, &unit1, inflected_buf, sizeof(inflected_buf));

    EXPECT_TRUE(syntax_get_stats(processor, &stats));
    EXPECT_EQ(1, stats.sentences_processed);
    EXPECT_EQ(1, stats.successful_parses);  // Structural parse succeeds
    EXPECT_EQ(0, stats.failed_parses);      // No structural parse failure
    EXPECT_EQ(1, stats.agreement_violations);  // Agreement check fails
    EXPECT_EQ(1, stats.morphological_ops);
}

TEST_F(SyntaxProcessorTest, GetStatsNullInputs) {
    syntax_stats_t stats;
    EXPECT_FALSE(syntax_get_stats(nullptr, &stats));
    EXPECT_FALSE(syntax_get_stats(processor, nullptr));
}

TEST_F(SyntaxProcessorTest, PosName) {
    EXPECT_STREQ("NOUN", syntax_pos_name(POS_NOUN));
    EXPECT_STREQ("VERB", syntax_pos_name(POS_VERB));
    EXPECT_STREQ("UNKNOWN", syntax_pos_name(POS_UNKNOWN));
    EXPECT_STREQ("INVALID", syntax_pos_name((part_of_speech_t)999));
}

TEST_F(SyntaxProcessorTest, PhraseName) {
    EXPECT_STREQ("NP", syntax_phrase_name(PHRASE_NP));
    EXPECT_STREQ("IP", syntax_phrase_name(PHRASE_IP));
    EXPECT_STREQ("INVALID", syntax_phrase_name((phrase_type_t)999));
}

TEST_F(SyntaxProcessorTest, IsContentWord) {
    EXPECT_TRUE(syntax_is_content_word(POS_NOUN));
    EXPECT_TRUE(syntax_is_content_word(POS_VERB));
    EXPECT_TRUE(syntax_is_content_word(POS_ADJECTIVE));
    EXPECT_TRUE(syntax_is_content_word(POS_ADVERB));
    EXPECT_FALSE(syntax_is_content_word(POS_DETERMINER));
    EXPECT_FALSE(syntax_is_content_word(POS_PREPOSITION));
}

// =============================================================================
// Main
// =============================================================================

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}