/**
 * @file test_nimcp_broca_full_pipeline.cpp
 * @brief Integration tests for the full Broca's region pipeline
 *
 * WHAT: Test the interaction and data flow between nimcp_syntax_processor,
 *       nimcp_phonological.c, and nimcp_speech_motor.c.
 * WHY:  Ensure these interconnected modules function correctly as a system for
 *       speech planning and production.
 * HOW:  Simulate a full speech production pipeline: syntactic units -> phonemes -> motor commands.
 *
 * COVERAGE TARGET: Full pipeline data flow.
 */

#include <gtest/gtest.h>
#include <string.h>
#include <vector>
#include <map>

// Include headers for all three modules
#include "core/brain/regions/broca/nimcp_syntax_processor.h"
#include "core/brain/regions/broca/nimcp_phonological.h"
#include "core/brain/regions/broca/nimcp_speech_motor.h"

// Test Fixture for Broca's Full Pipeline
class BrocaFullPipelineTest : public ::testing::Test {
protected:
    syntax_processor_t* syntax_proc;
    phonological_processor_t* phono_proc;
    speech_motor_planner_t* motor_planner;

    void SetUp() override {
        // Initialize Syntax Processor
        syntax_proc = syntax_create(nullptr);
        ASSERT_NE(nullptr, syntax_proc) << "Failed to create syntax processor";

        // Initialize Phonological Processor
        phono_proc = phonological_create(nullptr);
        ASSERT_NE(nullptr, phono_proc) << "Failed to create phonological processor";

        // Initialize Speech Motor Planner
        motor_planner = speech_motor_create(nullptr);
        ASSERT_NE(nullptr, motor_planner) << "Failed to create speech motor planner";
    }

    void TearDown() override {
        syntax_destroy(syntax_proc);
        phonological_destroy(phono_proc);
        speech_motor_destroy(motor_planner);
    }

    // Helper to create a basic syntactic unit
    syntactic_unit_t create_unit(part_of_speech_t pos, uint32_t word_id, uint8_t number = 0, uint8_t person = 0, uint8_t tense = 0) {
        syntactic_unit_t unit;
        memset(&unit, 0, sizeof(syntactic_unit_t));
        unit.pos = pos;
        unit.word_id = word_id;
        unit.features.number = number;
        unit.features.person = person;
        unit.features.tense = tense;
        return unit;
    }

    // Simple word_id to phoneme sequence mapping for testing
    // In a real system, this would come from a lexicon or lexical access module
    std::map<uint32_t, std::vector<uint8_t>> word_to_phonemes_map = {
        {100, {'t', 'h', 'e'}}, // "the"
        {101, {'k', 'a', 't'}}, // "cat"
        {102, {'s', 'a', 't'}}, // "sat"
    };

    void ExpectFloatNear(float actual, float expected, float epsilon = 1e-5f, const char* msg = "") {
        EXPECT_NEAR(actual, expected, epsilon) << msg;
    }
};

// =============================================================================
// Full Pipeline Integration Test
// =============================================================================

TEST_F(BrocaFullPipelineTest, SimpleSentenceProduction) {
    // 1. Syntactic Processing: "The cat sat"
    syntactic_unit_t unit_the = create_unit(POS_DETERMINER, 100);
    syntactic_unit_t unit_cat = create_unit(POS_NOUN, 101, 1, 3, 0); // Singular, 3rd person
    syntactic_unit_t unit_sat = create_unit(POS_VERB, 102, 1, 3, 2); // Singular, 3rd person, past tense

    ASSERT_TRUE(syntax_add_unit(syntax_proc, &unit_the));
    ASSERT_TRUE(syntax_add_unit(syntax_proc, &unit_cat));
    ASSERT_TRUE(syntax_add_unit(syntax_proc, &unit_sat));
    EXPECT_EQ(3, syntax_get_unit_count(syntax_proc));

    // Build syntax tree (will depend on rules, assume simple success for integration)
    ASSERT_TRUE(syntax_build_tree(syntax_proc));
    ASSERT_NE(nullptr, syntax_get_tree_root(syntax_proc));

    bool is_valid = false;
    ASSERT_TRUE(syntax_validate_grammar(syntax_proc, &is_valid));
    EXPECT_TRUE(is_valid) << "Grammar validation should pass for 'The cat sat'";

    // 2. Phonological Processing: Convert syntactic units to phonemes
    std::vector<uint8_t> phoneme_sequence;
    for (uint32_t i = 0; i < syntax_get_unit_count(syntax_proc); ++i) {
        syntactic_unit_t current_unit;
        // In a real system, we'd retrieve the unit from the processor. For this test,
        // we'll use our original units, assuming they haven't been modified.
        if (i == 0) current_unit = unit_the;
        else if (i == 1) current_unit = unit_cat;
        else current_unit = unit_sat;

        auto it = word_to_phonemes_map.find(current_unit.word_id);
        ASSERT_NE(it, word_to_phonemes_map.end()) << "Phoneme mapping missing for word_id: " << current_unit.word_id;
        phoneme_sequence.insert(phoneme_sequence.end(), it->second.begin(), it->second.end());
    }
    ASSERT_FALSE(phoneme_sequence.empty());

    for (uint8_t phoneme_symbol : phoneme_sequence) {
        ASSERT_TRUE(phonological_add_phoneme(phono_proc, phoneme_symbol));
    }
    EXPECT_EQ(phoneme_sequence.size(), phonological_get_phoneme_count(phono_proc));

    // Generate syllables and prosody
    ASSERT_TRUE(phonological_generate_syllables(phono_proc));
    ASSERT_GT(phonological_get_syllable_count(phono_proc), 0);
    ASSERT_TRUE(phonological_generate_prosody(phono_proc, INTONATION_PATTERN_FALLING));
    ASSERT_TRUE(phonological_is_ready(phono_proc));

    // 3. Speech Motor Planning: Generate motor commands
    // We'll re-extract the phonemes, potentially with modified durations due to phonological processing
    std::vector<uint8_t> processed_phoneme_symbols;
    // For simplicity, we're not actually modifying the phoneme symbols, just durations.
    // So we can use the original phoneme_sequence for speech motor planning.
    // In a more complex integration, we might retrieve processed phonemes from phono_proc.
    
    // Plan the sequence of phonemes
    ASSERT_TRUE(speech_motor_plan_sequence(motor_planner, phoneme_sequence.data(), phoneme_sequence.size()));

    // Retrieve commands
    uint32_t total_expected_commands = phoneme_sequence.size() * SPEECH_MOTOR_NUM_ARTICULATORS;
    std::vector<motor_command_t> motor_commands(total_expected_commands);
    uint32_t retrieved_count = total_expected_commands;
    ASSERT_TRUE(speech_motor_get_commands(motor_planner, motor_commands.data(), &retrieved_count));
    EXPECT_EQ(total_expected_commands, retrieved_count);

    // Verify some properties of the commands
    speech_motor_stats_t stats;
    ASSERT_TRUE(speech_motor_get_stats(motor_planner, &stats));
    EXPECT_EQ(phoneme_sequence.size(), stats.phonemes_planned);
    EXPECT_EQ(total_expected_commands, stats.commands_generated);
    EXPECT_EQ(0, stats.queue_size); // All commands should be retrieved

    // Check that the first and last commands have different timestamps
    EXPECT_GT(motor_commands[0].timestamp, 0.0);
    EXPECT_GT(motor_commands[total_expected_commands - 1].timestamp, motor_commands[0].timestamp);
}

// =============================================================================
// Main
// =============================================================================

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
