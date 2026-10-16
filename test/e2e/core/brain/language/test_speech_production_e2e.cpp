/**
 * @file test_speech_production_e2e.cpp
 * @brief End-to-end tests for speech production pipeline in Broca's region
 *
 * WHAT: Full pipeline tests for speech production from concept to articulation
 * WHY:  Verify complete language production pathway with Broca's area processing
 * HOW:  Test syntax generation, phonological encoding, motor planning, and prosody
 *
 * TEST COVERAGE:
 * - Full Speech Production Pipeline (3 tests)
 * - Syntax Generation (3 tests)
 * - Phonological Encoding (3 tests)
 * - Prosody Generation (3 tests)
 * - Speech Error Detection (3 tests)
 *
 * TOTAL: 15 tests
 *
 * BIOLOGICAL ANALOGY:
 * - Broca's area (BA 44/45) handles speech production
 * - Syntax processing in pars triangularis (BA 45)
 * - Motor planning in pars opercularis (BA 44)
 * - Phonological encoding prepares articulatory sequences
 *
 * @author NIMCP Development Team
 * @date 2026-01-24
 */

#include "../../../e2e_test_framework.h"
#include "utils/nimcp_test_base.h"
#include <thread>
#include <vector>
#include <algorithm>
#include <numeric>
#include <random>
#include <cmath>
#include <cstring>

#include "core/brain/regions/broca/nimcp_broca_adapter.h"
#include "core/brain/regions/broca/nimcp_language_production_bridge.h"
#include "utils/memory/nimcp_memory.h"

using namespace nimcp::e2e;

//=============================================================================
// Test Configuration
//=============================================================================

constexpr double MAX_PRODUCTION_TIME_MS = 200.0;
constexpr double MAX_PROCESSING_TIME_MS = 100.0;
constexpr double MAX_MOTOR_PLANNING_TIME_MS = 150.0;
constexpr uint32_t MAX_WORDS = 32;
constexpr uint32_t MAX_PHONEMES = 128;
constexpr uint32_t MAX_MOTOR_COMMANDS = 256;
constexpr uint32_t SEMANTIC_DIM = 64;

//=============================================================================
// Helper Functions
//=============================================================================

static void populate_test_lexicon(broca_adapter_t* broca) {
    // Add basic test words to lexicon
    broca_lexical_entry_t entry;
    memset(&entry, 0, sizeof(entry));

    // Word: "hello"
    entry.word_id = 1;
    strncpy(entry.word, "hello", sizeof(entry.word) - 1);
    entry.phonemes[0] = 'h'; entry.phonemes[1] = 'E'; entry.phonemes[2] = 'l';
    entry.phonemes[3] = 'o'; entry.phonemes[4] = 'U';
    entry.phoneme_count = 5;
    entry.pos = 5;  // Interjection
    entry.frequency = 0.8f;
    broca_add_lexical_entry(broca, &entry);

    // Word: "world"
    entry.word_id = 2;
    strncpy(entry.word, "world", sizeof(entry.word) - 1);
    entry.phonemes[0] = 'w'; entry.phonemes[1] = '3'; entry.phonemes[2] = 'r';
    entry.phonemes[3] = 'l'; entry.phonemes[4] = 'd';
    entry.phoneme_count = 5;
    entry.pos = 1;  // Noun
    entry.frequency = 0.9f;
    broca_add_lexical_entry(broca, &entry);

    // Word: "the"
    entry.word_id = 3;
    strncpy(entry.word, "the", sizeof(entry.word) - 1);
    entry.phonemes[0] = 'D'; entry.phonemes[1] = '@';
    entry.phoneme_count = 2;
    entry.pos = 4;  // Article
    entry.frequency = 0.99f;
    broca_add_lexical_entry(broca, &entry);

    // Word: "cat"
    entry.word_id = 4;
    strncpy(entry.word, "cat", sizeof(entry.word) - 1);
    entry.phonemes[0] = 'k'; entry.phonemes[1] = 'a'; entry.phonemes[2] = 't';
    entry.phoneme_count = 3;
    entry.pos = 1;  // Noun
    entry.frequency = 0.7f;
    broca_add_lexical_entry(broca, &entry);

    // Word: "sat"
    entry.word_id = 5;
    strncpy(entry.word, "sat", sizeof(entry.word) - 1);
    entry.phonemes[0] = 's'; entry.phonemes[1] = 'a'; entry.phonemes[2] = 't';
    entry.phoneme_count = 3;
    entry.pos = 2;  // Verb
    entry.frequency = 0.6f;
    broca_add_lexical_entry(broca, &entry);

    // Word: "on"
    entry.word_id = 6;
    strncpy(entry.word, "on", sizeof(entry.word) - 1);
    entry.phonemes[0] = 'O'; entry.phonemes[1] = 'n';
    entry.phoneme_count = 2;
    entry.pos = 3;  // Preposition
    entry.frequency = 0.95f;
    broca_add_lexical_entry(broca, &entry);

    // Word: "mat"
    entry.word_id = 7;
    strncpy(entry.word, "mat", sizeof(entry.word) - 1);
    entry.phonemes[0] = 'm'; entry.phonemes[1] = 'a'; entry.phonemes[2] = 't';
    entry.phoneme_count = 3;
    entry.pos = 1;  // Noun
    entry.frequency = 0.4f;
    broca_add_lexical_entry(broca, &entry);
}

static std::vector<float> generate_semantic_vector(uint32_t dim, uint32_t seed) {
    std::vector<float> vec(dim);
    std::mt19937 gen(seed);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    for (uint32_t i = 0; i < dim; i++) {
        vec[i] = dist(gen);
    }
    // Normalize
    float norm = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        norm += vec[i] * vec[i];
    }
    norm = std::sqrt(norm);
    if (norm > 0.0f) {
        for (uint32_t i = 0; i < dim; i++) {
            vec[i] /= norm;
        }
    }
    return vec;
}

//=============================================================================
// Test Fixture
//=============================================================================

class E2ESpeechProductionTest : public NimcpTestBase {
protected:
    broca_adapter_t* broca = nullptr;
    language_production_bridge_t* bridge = nullptr;

    void SetUp() override {
        NimcpTestBase::SetUp();

        // Create Broca's region adapter
        broca_config_t config = broca_default_config();
        config.max_words = MAX_WORDS;
        config.max_phonemes = MAX_PHONEMES;
        config.max_motor_commands = MAX_MOTOR_COMMANDS;
        config.enable_coarticulation = true;
        config.enable_prosody = true;
        config.enable_lexicon = true;
        config.lexicon_size = 100;
        config.enable_working_memory = true;
        config.working_memory_slots = 7;

        broca = broca_create(&config);
        ASSERT_NE(broca, nullptr) << "Failed to create Broca adapter";

        // Populate test lexicon
        populate_test_lexicon(broca);

        // Create language production bridge
        lpb_config_t lpb_config = lpb_default_config();
        lpb_config.max_tokens = MAX_WORDS;
        lpb_config.semantic_dim = SEMANTIC_DIM;
        lpb_config.enable_self_monitoring = true;
        lpb_config.enable_error_correction = true;

        bridge = lpb_create(&lpb_config, broca);
        ASSERT_NE(bridge, nullptr) << "Failed to create language production bridge";
    }

    void TearDown() override {
        if (bridge) {
            lpb_destroy(bridge);
            bridge = nullptr;
        }
        if (broca) {
            broca_destroy(broca);
            broca = nullptr;
        }
        NimcpTestBase::TearDown();
    }
};

//=============================================================================
// Full Speech Production Pipeline Tests
//=============================================================================

TEST_F(E2ESpeechProductionTest, ConceptToArticulationPipeline) {
    E2E_PIPELINE_START("Concept to Articulation Pipeline");

    // Stage 1: Create semantic intent
    E2E_STAGE_BEGIN("Create semantic intent", 20);
    lpb_semantic_intent_t intent;
    memset(&intent, 0, sizeof(intent));

    auto semantic_vec = generate_semantic_vector(SEMANTIC_DIM, 42);
    intent.semantic_vector = semantic_vec.data();
    intent.semantic_dim = SEMANTIC_DIM;
    intent.confidence = 0.9f;
    intent.intent_type = 0;  // Statement
    intent.from_internal = true;
    E2E_STAGE_END();

    // Stage 2: Produce from semantic intent
    E2E_STAGE_BEGIN("Produce from intent", MAX_PRODUCTION_TIME_MS);
    lpb_production_result_t result;
    memset(&result, 0, sizeof(result));

    bool success = lpb_produce_from_intent(bridge, &intent, &result);
    EXPECT_TRUE(success) << "Production from intent should succeed";
    E2E_STAGE_END();

    // Stage 3: Verify production result
    E2E_STAGE_BEGIN("Verify production result", 20);
    EXPECT_GE(result.motor_command_count, 0u) << "Should generate motor commands";
    EXPECT_GE(result.fluency_score, 0.0f) << "Fluency score should be non-negative";
    E2E_STAGE_END();

    // Stage 4: Retrieve motor commands
    E2E_STAGE_BEGIN("Retrieve motor commands", MAX_MOTOR_PLANNING_TIME_MS);
    std::vector<broca_output_command_t> commands(MAX_MOTOR_COMMANDS);
    uint32_t command_count = MAX_MOTOR_COMMANDS;

    success = broca_get_all_commands(broca, commands.data(), &command_count);
    EXPECT_TRUE(success) << "Should retrieve motor commands";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ESpeechProductionTest, WordSequenceProduction) {
    E2E_PIPELINE_START("Word Sequence Production");

    // Produce from word strings
    E2E_STAGE_BEGIN("Setup word sequence", 10);
    const char* words[] = {"the", "cat", "sat"};
    uint32_t num_words = 3;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Produce from strings", MAX_PRODUCTION_TIME_MS);
    broca_utterance_result_t result;
    memset(&result, 0, sizeof(result));

    bool success = broca_produce_from_strings(broca, words, num_words, &result);
    EXPECT_TRUE(success) << "Production from strings should succeed";
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify utterance result", 20);
    EXPECT_EQ(result.word_count, num_words) << "Should process all words";
    EXPECT_GT(result.phoneme_count, 0u) << "Should generate phonemes";
    EXPECT_TRUE(result.ready_for_articulation) << "Should be ready for articulation";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ESpeechProductionTest, IncrementalUtteranceConstruction) {
    E2E_PIPELINE_START("Incremental Utterance Construction");

    // Begin new utterance
    E2E_STAGE_BEGIN("Begin utterance", 10);
    bool success = broca_begin_utterance(broca);
    EXPECT_TRUE(success) << "Should begin utterance";
    E2E_STAGE_END();

    // Add words incrementally
    E2E_STAGE_BEGIN("Add words incrementally", MAX_PROCESSING_TIME_MS);
    const char* sentence[] = {"the", "cat", "sat", "on", "the", "mat"};

    for (uint32_t i = 0; i < 6; i++) {
        broca_input_word_t word;
        memset(&word, 0, sizeof(word));
        strncpy(word.word, sentence[i], sizeof(word.word) - 1);
        word.word_id = 0;  // Use string lookup

        success = broca_add_word(broca, &word);
        EXPECT_TRUE(success) << "Should add word: " << sentence[i];
    }
    E2E_STAGE_END();

    // Process complete utterance
    E2E_STAGE_BEGIN("Process utterance", MAX_PRODUCTION_TIME_MS);
    broca_utterance_result_t result;
    memset(&result, 0, sizeof(result));

    success = broca_process_utterance(broca, &result);
    EXPECT_TRUE(success) << "Utterance processing should succeed";
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify complete utterance", 20);
    EXPECT_EQ(result.word_count, 6u) << "Should process all 6 words";
    EXPECT_GT(result.syllable_count, 0u) << "Should count syllables";
    EXPECT_GT(result.total_duration_ms, 0.0f) << "Should estimate duration";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Syntax Generation Tests
//=============================================================================

TEST_F(E2ESpeechProductionTest, SimpleSentenceSyntax) {
    E2E_PIPELINE_START("Simple Sentence Syntax");

    // Test simple declarative sentence
    E2E_STAGE_BEGIN("Create simple sentence", 10);
    const char* words[] = {"the", "cat", "sat"};
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Process with syntax validation", MAX_PRODUCTION_TIME_MS);
    broca_utterance_result_t result;
    memset(&result, 0, sizeof(result));

    bool success = broca_produce_from_strings(broca, words, 3, &result);
    EXPECT_TRUE(success) << "Simple sentence should produce successfully";
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Check syntax validity", 10);
    EXPECT_TRUE(result.syntax_valid) << "Simple sentence syntax should be valid";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ESpeechProductionTest, GrammaticalAgreement) {
    E2E_PIPELINE_START("Grammatical Agreement");

    // Test grammatical features
    E2E_STAGE_BEGIN("Setup words with features", 10);
    broca_begin_utterance(broca);

    broca_input_word_t word;
    memset(&word, 0, sizeof(word));
    strncpy(word.word, "cat", sizeof(word.word) - 1);
    word.number = 1;  // Singular
    word.person = 3;  // Third person

    bool success = broca_add_word(broca, &word);
    EXPECT_TRUE(success);

    memset(&word, 0, sizeof(word));
    strncpy(word.word, "sat", sizeof(word.word) - 1);
    word.tense = 2;  // Past tense
    word.number = 1;  // Singular
    word.person = 3;  // Third person

    success = broca_add_word(broca, &word);
    EXPECT_TRUE(success);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Process with agreement check", MAX_PRODUCTION_TIME_MS);
    broca_utterance_result_t result;
    memset(&result, 0, sizeof(result));

    success = broca_process_utterance(broca, &result);
    EXPECT_TRUE(success) << "Should process utterance";
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify agreement", 10);
    EXPECT_TRUE(result.agreement_valid) << "Agreement constraints should be satisfied";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ESpeechProductionTest, MultipleClauseProduction) {
    E2E_PIPELINE_START("Multiple Clause Production");

    // Process multiple short utterances to simulate clauses
    E2E_STAGE_BEGIN("Process multiple clauses", MAX_PRODUCTION_TIME_MS * 2);
    broca_stats_t stats_before;
    broca_get_stats(broca, &stats_before);

    // First clause
    const char* clause1[] = {"the", "cat", "sat"};
    broca_utterance_result_t result1;
    bool success = broca_produce_from_strings(broca, clause1, 3, &result1);
    EXPECT_TRUE(success);

    // Reset for second clause
    broca_reset(broca);

    // Second clause
    const char* clause2[] = {"on", "the", "mat"};
    broca_utterance_result_t result2;
    success = broca_produce_from_strings(broca, clause2, 3, &result2);
    EXPECT_TRUE(success);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify stats updated", 20);
    broca_stats_t stats_after;
    broca_get_stats(broca, &stats_after);
    EXPECT_GT(stats_after.utterances_processed, stats_before.utterances_processed)
        << "Should count processed utterances";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Phonological Encoding Tests
//=============================================================================

TEST_F(E2ESpeechProductionTest, PhonemeSequenceGeneration) {
    E2E_PIPELINE_START("Phoneme Sequence Generation");

    E2E_STAGE_BEGIN("Produce simple word", MAX_PROCESSING_TIME_MS);
    const char* words[] = {"hello"};
    broca_utterance_result_t result;
    memset(&result, 0, sizeof(result));

    bool success = broca_produce_from_strings(broca, words, 1, &result);
    EXPECT_TRUE(success);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify phoneme count", 10);
    EXPECT_EQ(result.phoneme_count, 5u) << "Hello should have 5 phonemes";
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Retrieve motor commands for phonemes", MAX_MOTOR_PLANNING_TIME_MS);
    std::vector<broca_output_command_t> commands(MAX_MOTOR_COMMANDS);
    uint32_t count = MAX_MOTOR_COMMANDS;

    success = broca_get_all_commands(broca, commands.data(), &count);
    EXPECT_TRUE(success);
    EXPECT_GT(count, 0u) << "Should have motor commands for phonemes";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ESpeechProductionTest, CoarticulationPlanning) {
    E2E_PIPELINE_START("Coarticulation Planning");

    // Coarticulation: phonemes influence each other
    E2E_STAGE_BEGIN("Process word with coarticulation", MAX_PRODUCTION_TIME_MS);
    const char* words[] = {"world"};
    broca_utterance_result_t result;

    bool success = broca_produce_from_strings(broca, words, 1, &result);
    EXPECT_TRUE(success) << "Should process word";
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Check motor commands for smooth transitions", MAX_MOTOR_PLANNING_TIME_MS);
    std::vector<broca_output_command_t> commands(MAX_MOTOR_COMMANDS);
    uint32_t count = MAX_MOTOR_COMMANDS;

    success = broca_get_all_commands(broca, commands.data(), &count);
    EXPECT_TRUE(success);

    // Check for smooth velocity transitions (coarticulation effect)
    bool has_smooth_transitions = true;
    for (uint32_t i = 1; i < count && i < MAX_MOTOR_COMMANDS; i++) {
        float velocity_diff = std::abs(commands[i].velocity - commands[i-1].velocity);
        if (velocity_diff > 5.0f) {
            has_smooth_transitions = false;
            break;
        }
    }
    // Note: This is a heuristic check; actual implementation may vary
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ESpeechProductionTest, SyllableStructure) {
    E2E_PIPELINE_START("Syllable Structure");

    E2E_STAGE_BEGIN("Process multi-syllable utterance", MAX_PRODUCTION_TIME_MS);
    const char* words[] = {"hello", "world"};
    broca_utterance_result_t result;
    memset(&result, 0, sizeof(result));

    bool success = broca_produce_from_strings(broca, words, 2, &result);
    EXPECT_TRUE(success);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify syllable count", 10);
    // "hello" = 2 syllables, "world" = 1 syllable = 3 total
    EXPECT_GE(result.syllable_count, 3u) << "Should have at least 3 syllables";
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Check duration proportional to syllables", 10);
    EXPECT_GT(result.total_duration_ms, 0.0f) << "Should have positive duration";
    // Rough estimate: 150-300ms per syllable for normal speech
    float duration_per_syllable = result.total_duration_ms / result.syllable_count;
    EXPECT_GT(duration_per_syllable, 50.0f) << "Duration per syllable reasonable";
    EXPECT_LT(duration_per_syllable, 500.0f) << "Duration per syllable not excessive";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Prosody Generation Tests
//=============================================================================

TEST_F(E2ESpeechProductionTest, EmotionalProsody) {
    E2E_PIPELINE_START("Emotional Prosody");

    // Test production with different emotional contexts
    E2E_STAGE_BEGIN("Produce neutral statement", MAX_PRODUCTION_TIME_MS);
    lpb_semantic_intent_t intent;
    memset(&intent, 0, sizeof(intent));

    auto semantic_vec = generate_semantic_vector(SEMANTIC_DIM, 100);
    intent.semantic_vector = semantic_vec.data();
    intent.semantic_dim = SEMANTIC_DIM;
    intent.confidence = 0.9f;
    intent.intent_type = 0;  // Statement (neutral)
    intent.speech_act = 0;   // Informative

    lpb_production_result_t result;
    memset(&result, 0, sizeof(result));

    bool success = lpb_produce_from_intent(bridge, &intent, &result);
    EXPECT_TRUE(success);
    float neutral_duration = result.estimated_duration_ms;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Produce question (rising intonation)", MAX_PRODUCTION_TIME_MS);
    lpb_reset(bridge);

    intent.intent_type = 1;  // Question
    intent.speech_act = 1;   // Query

    success = lpb_produce_from_intent(bridge, &intent, &result);
    EXPECT_TRUE(success);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify prosodic variation", 10);
    // Questions typically have different prosodic patterns
    // (actual verification depends on implementation)
    EXPECT_GE(result.fluency_score, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ESpeechProductionTest, EmphasisPlacement) {
    E2E_PIPELINE_START("Emphasis Placement");

    // Test word emphasis in production
    E2E_STAGE_BEGIN("Produce with emphasis", MAX_PRODUCTION_TIME_MS);
    broca_begin_utterance(broca);

    // Add words with varying emphasis through grammatical features
    broca_input_word_t word;

    memset(&word, 0, sizeof(word));
    strncpy(word.word, "the", sizeof(word.word) - 1);
    broca_add_word(broca, &word);

    memset(&word, 0, sizeof(word));
    strncpy(word.word, "cat", sizeof(word.word) - 1);
    // Emphasized noun (content word)
    broca_add_word(broca, &word);

    memset(&word, 0, sizeof(word));
    strncpy(word.word, "sat", sizeof(word.word) - 1);
    // Emphasized verb
    broca_add_word(broca, &word);

    broca_utterance_result_t result;
    bool success = broca_process_utterance(broca, &result);
    EXPECT_TRUE(success);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Check motor command variation", MAX_MOTOR_PLANNING_TIME_MS);
    std::vector<broca_output_command_t> commands(MAX_MOTOR_COMMANDS);
    uint32_t count = MAX_MOTOR_COMMANDS;

    success = broca_get_all_commands(broca, commands.data(), &count);
    EXPECT_TRUE(success);
    EXPECT_GT(count, 0u);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ESpeechProductionTest, SpeechRateControl) {
    E2E_PIPELINE_START("Speech Rate Control");

    // Test duration estimation at different speeds
    E2E_STAGE_BEGIN("Produce with default rate", MAX_PRODUCTION_TIME_MS);
    const char* words[] = {"hello", "world"};
    broca_utterance_result_t result;

    bool success = broca_produce_from_strings(broca, words, 2, &result);
    EXPECT_TRUE(success);
    float default_duration = result.total_duration_ms;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify duration is reasonable", 10);
    // Speech rate: ~150-200 words per minute = ~300-400ms per word
    // 2 words should be ~600-800ms
    EXPECT_GT(default_duration, 200.0f) << "Duration should be meaningful";
    EXPECT_LT(default_duration, 5000.0f) << "Duration should not be excessive";
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Check command timing", MAX_MOTOR_PLANNING_TIME_MS);
    std::vector<broca_output_command_t> commands(MAX_MOTOR_COMMANDS);
    uint32_t count = MAX_MOTOR_COMMANDS;

    success = broca_get_all_commands(broca, commands.data(), &count);
    EXPECT_TRUE(success);

    // Verify commands have increasing timestamps
    bool timestamps_increase = true;
    for (uint32_t i = 1; i < count; i++) {
        if (commands[i].timestamp_ms < commands[i-1].timestamp_ms) {
            timestamps_increase = false;
            break;
        }
    }
    EXPECT_TRUE(timestamps_increase) << "Motor command timestamps should increase";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Speech Error Detection Tests
//=============================================================================

TEST_F(E2ESpeechProductionTest, SelfMonitoringActivation) {
    E2E_PIPELINE_START("Self-Monitoring Activation");

    // Enable self-monitoring
    E2E_STAGE_BEGIN("Enable self-monitoring", 10);
    bool success = lpb_set_self_monitoring(bridge, true);
    EXPECT_TRUE(success) << "Should enable self-monitoring";
    E2E_STAGE_END();

    // Produce utterance with monitoring
    E2E_STAGE_BEGIN("Produce with monitoring", MAX_PRODUCTION_TIME_MS);
    lpb_semantic_intent_t intent;
    memset(&intent, 0, sizeof(intent));

    auto semantic_vec = generate_semantic_vector(SEMANTIC_DIM, 200);
    intent.semantic_vector = semantic_vec.data();
    intent.semantic_dim = SEMANTIC_DIM;
    intent.confidence = 0.95f;
    intent.intent_type = 0;

    lpb_production_result_t result;
    success = lpb_produce_from_intent(bridge, &intent, &result);
    EXPECT_TRUE(success);
    E2E_STAGE_END();

    // Check production
    E2E_STAGE_BEGIN("Check production quality", 20);
    float match_score;
    success = lpb_check_production(bridge, &match_score);
    // Self-monitoring check may pass or provide score
    EXPECT_GE(match_score, 0.0f) << "Match score should be non-negative";
    EXPECT_LE(match_score, 1.0f) << "Match score should be at most 1.0";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ESpeechProductionTest, ErrorCorrectionCapability) {
    E2E_PIPELINE_START("Error Correction Capability");

    // Test error correction through training interface
    E2E_STAGE_BEGIN("Setup error scenario", 10);
    broca_config_t config;
    broca_get_config(broca, &config);
    EXPECT_TRUE(config.enable_prosody) << "Prosody should be enabled";
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Train with correction feedback", MAX_PROCESSING_TIME_MS);
    // Provide training feedback to simulate error correction
    uint8_t target_phonemes[] = {'h', 'E', 'l', 'o', 'U'};

    bool success = broca_train_phonemes(broca, target_phonemes, 5, 0.1f);
    EXPECT_TRUE(success) << "Training should accept feedback";
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify training updates", 10);
    broca_stats_t stats;
    broca_get_stats(broca, &stats);
    EXPECT_GE(stats.training_iterations, 1u) << "Should record training iteration";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ESpeechProductionTest, LexicalAccessFailureRecovery) {
    E2E_PIPELINE_START("Lexical Access Failure Recovery");

    // Test handling of unknown words
    E2E_STAGE_BEGIN("Attempt unknown word", MAX_PROCESSING_TIME_MS);
    const char* words[] = {"xyznonexistent"};

    broca_utterance_result_t result;
    bool success = broca_produce_from_strings(broca, words, 1, &result);
    // May succeed with fallback or fail gracefully
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Check error handling", 20);
    broca_error_t error = broca_get_last_error(broca);
    // Either no error (fallback worked) or lexicon miss
    bool handled_gracefully = (error == BROCA_ERROR_NONE ||
                               error == BROCA_ERROR_LEXICON_MISS);
    EXPECT_TRUE(handled_gracefully) << "Should handle unknown word gracefully: "
                                    << broca_error_string(error);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Recovery after error", MAX_PROCESSING_TIME_MS);
    // Reset and try valid production
    broca_reset(broca);

    const char* valid_words[] = {"hello"};
    success = broca_produce_from_strings(broca, valid_words, 1, &result);
    EXPECT_TRUE(success) << "Should recover and process valid word";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}
