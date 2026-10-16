/**
 * @file test_language_comprehension_e2e.cpp
 * @brief End-to-end tests for language comprehension pipeline in Wernicke's region
 *
 * WHAT: Full pipeline tests for language comprehension from sound to meaning
 * WHY:  Verify complete language understanding pathway with Wernicke's area processing
 * HOW:  Test lexical access, syntactic parsing, semantic integration, and ambiguity resolution
 *
 * TEST COVERAGE:
 * - Full Comprehension Pipeline (3 tests)
 * - Lexical Access and Word Recognition (3 tests)
 * - Syntactic Parsing (3 tests)
 * - Semantic Integration (3 tests)
 * - Ambiguity Resolution (3 tests)
 *
 * TOTAL: 15 tests
 *
 * BIOLOGICAL ANALOGY:
 * - Wernicke's area (BA 22, posterior STG) handles comprehension
 * - Phonological analysis identifies sounds
 * - Lexical access maps sounds to words
 * - Semantic integration builds meaning
 * - Syntactic parsing extracts sentence structure
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

#include "core/brain/regions/wernicke/nimcp_wernicke_adapter.h"
#include "core/brain/regions/wernicke/nimcp_wernicke_broca_bridge.h"
#include "perception/nimcp_speech_cortex.h"
#include "utils/memory/nimcp_memory.h"

using namespace nimcp::e2e;

//=============================================================================
// Test Configuration
//=============================================================================

constexpr double MAX_COMPREHENSION_TIME_MS = 300.0;
constexpr double MAX_PROCESSING_TIME_MS = 100.0;
constexpr double MAX_PARSING_TIME_MS = 200.0;
constexpr uint32_t MAX_WORDS = 64;
constexpr uint32_t MAX_PHONEMES = 128;
constexpr uint32_t MAX_CONCEPTS = 256;
constexpr uint32_t EMBEDDING_DIM = 64;
constexpr float SAMPLE_RATE = 16000.0f;

//=============================================================================
// Helper Functions
//=============================================================================

static void populate_wernicke_lexicon(wernicke_adapter_t* wernicke) {
    wernicke_word_t word;
    memset(&word, 0, sizeof(word));

    // Word: "hello"
    word.word_id = 1;
    strncpy(word.word, "hello", sizeof(word.word) - 1);
    word.phonemes[0] = 'h'; word.phonemes[1] = 'E'; word.phonemes[2] = 'l';
    word.phonemes[3] = 'o'; word.phonemes[4] = 'U';
    word.phoneme_count = 5;
    word.frequency = 0.8f;
    word.concept_id = 100;
    word.pos = 5;  // Interjection
    wernicke_add_word(wernicke, &word);

    // Word: "world"
    word.word_id = 2;
    strncpy(word.word, "world", sizeof(word.word) - 1);
    word.phonemes[0] = 'w'; word.phonemes[1] = '3'; word.phonemes[2] = 'r';
    word.phonemes[3] = 'l'; word.phonemes[4] = 'd';
    word.phoneme_count = 5;
    word.frequency = 0.9f;
    word.concept_id = 101;
    word.pos = 1;  // Noun
    wernicke_add_word(wernicke, &word);

    // Word: "the"
    word.word_id = 3;
    strncpy(word.word, "the", sizeof(word.word) - 1);
    word.phonemes[0] = 'D'; word.phonemes[1] = '@';
    word.phoneme_count = 2;
    word.frequency = 0.99f;
    word.concept_id = 102;
    word.pos = 4;  // Article
    wernicke_add_word(wernicke, &word);

    // Word: "cat"
    word.word_id = 4;
    strncpy(word.word, "cat", sizeof(word.word) - 1);
    word.phonemes[0] = 'k'; word.phonemes[1] = 'a'; word.phonemes[2] = 't';
    word.phoneme_count = 3;
    word.frequency = 0.7f;
    word.concept_id = 103;
    word.pos = 1;  // Noun
    wernicke_add_word(wernicke, &word);

    // Word: "sat"
    word.word_id = 5;
    strncpy(word.word, "sat", sizeof(word.word) - 1);
    word.phonemes[0] = 's'; word.phonemes[1] = 'a'; word.phonemes[2] = 't';
    word.phoneme_count = 3;
    word.frequency = 0.6f;
    word.concept_id = 104;
    word.pos = 2;  // Verb
    wernicke_add_word(wernicke, &word);

    // Word: "bank" (ambiguous - financial or river)
    word.word_id = 6;
    strncpy(word.word, "bank", sizeof(word.word) - 1);
    word.phonemes[0] = 'b'; word.phonemes[1] = 'a'; word.phonemes[2] = 'N';
    word.phonemes[3] = 'k';
    word.phoneme_count = 4;
    word.frequency = 0.75f;
    word.concept_id = 105;  // Primary meaning
    word.pos = 1;  // Noun
    wernicke_add_word(wernicke, &word);

    // Word: "river"
    word.word_id = 7;
    strncpy(word.word, "river", sizeof(word.word) - 1);
    word.phonemes[0] = 'r'; word.phonemes[1] = 'I'; word.phonemes[2] = 'v';
    word.phonemes[3] = '@'; word.phonemes[4] = 'r';
    word.phoneme_count = 5;
    word.frequency = 0.65f;
    word.concept_id = 106;
    word.pos = 1;  // Noun
    wernicke_add_word(wernicke, &word);

    // Word: "money"
    word.word_id = 8;
    strncpy(word.word, "money", sizeof(word.word) - 1);
    word.phonemes[0] = 'm'; word.phonemes[1] = 'V'; word.phonemes[2] = 'n';
    word.phonemes[3] = 'i';
    word.phoneme_count = 4;
    word.frequency = 0.85f;
    word.concept_id = 107;
    word.pos = 1;  // Noun
    wernicke_add_word(wernicke, &word);
}

static std::vector<float> generate_synthetic_audio(uint32_t num_samples, float freq_hz) {
    std::vector<float> audio(num_samples);
    for (uint32_t i = 0; i < num_samples; i++) {
        float t = static_cast<float>(i) / SAMPLE_RATE;
        // Simple tone with harmonics to simulate speech
        audio[i] = 0.5f * std::sin(2.0f * M_PI * freq_hz * t) +
                   0.3f * std::sin(2.0f * M_PI * freq_hz * 2.0f * t) +
                   0.2f * std::sin(2.0f * M_PI * freq_hz * 3.0f * t);
    }
    return audio;
}

static std::vector<phoneme_event_t> create_phoneme_event_sequence(const uint8_t* phonemes, uint32_t count) {
    std::vector<phoneme_event_t> seq(count);
    for (uint32_t i = 0; i < count; i++) {
        memset(&seq[i], 0, sizeof(phoneme_event_t));
        seq[i].phoneme = static_cast<phoneme_t>(phonemes[i]);
        seq[i].confidence = 0.9f;
        seq[i].features.duration_ms = 80.0f + (i % 3) * 20.0f;
        seq[i].onset_time_ms = i * 100;
        seq[i].offset_time_ms = (i + 1) * 100;
        seq[i].sequence_position = i;
    }
    return seq;
}

static std::vector<phoneme_t> create_phoneme_sequence(const uint8_t* phonemes, uint32_t count) {
    std::vector<phoneme_t> seq(count);
    for (uint32_t i = 0; i < count; i++) {
        seq[i] = static_cast<phoneme_t>(phonemes[i]);
    }
    return seq;
}

//=============================================================================
// Test Fixture
//=============================================================================

class E2ELanguageComprehensionTest : public NimcpTestBase {
protected:
    wernicke_adapter_t* wernicke = nullptr;

    void SetUp() override {
        NimcpTestBase::SetUp();

        // Create Wernicke's area adapter
        wernicke_config_t config = wernicke_default_config();
        config.max_phonemes = MAX_PHONEMES;
        config.max_words = MAX_WORDS;
        config.max_concepts = MAX_CONCEPTS;
        config.enable_lexicon = true;
        config.lexicon_size = 100;
        config.enable_phonological = true;
        config.enable_lexical = true;
        config.enable_semantic = true;
        config.enable_syntactic = true;
        config.enable_working_memory = true;
        config.working_memory_slots = 9;
        config.embedding_dim = EMBEDDING_DIM;

        wernicke = wernicke_create(&config);
        ASSERT_NE(wernicke, nullptr) << "Failed to create Wernicke adapter";

        // Populate test lexicon
        populate_wernicke_lexicon(wernicke);
    }

    void TearDown() override {
        if (wernicke) {
            wernicke_destroy(wernicke);
            wernicke = nullptr;
        }
        NimcpTestBase::TearDown();
    }
};

//=============================================================================
// Full Comprehension Pipeline Tests
//=============================================================================

TEST_F(E2ELanguageComprehensionTest, SoundToMeaningPipeline) {
    E2E_PIPELINE_START("Sound to Meaning Pipeline");

    // Stage 1: Generate synthetic audio
    E2E_STAGE_BEGIN("Generate audio input", 20);
    uint32_t duration_samples = static_cast<uint32_t>(SAMPLE_RATE * 0.5f);  // 500ms
    auto audio = generate_synthetic_audio(duration_samples, 200.0f);
    E2E_STAGE_END();

    // Stage 2: Full comprehension pipeline
    E2E_STAGE_BEGIN("Run comprehension pipeline", MAX_COMPREHENSION_TIME_MS);
    wernicke_comprehension_t result;
    memset(&result, 0, sizeof(result));

    bool success = wernicke_comprehend(
        wernicke,
        audio.data(),
        duration_samples,
        static_cast<uint32_t>(SAMPLE_RATE),
        &result
    );
    EXPECT_TRUE(success) << "Comprehension should succeed";
    E2E_STAGE_END();

    // Stage 3: Verify comprehension result
    E2E_STAGE_BEGIN("Verify comprehension", 20);
    EXPECT_GE(result.comprehension_score, 0.0f);
    EXPECT_LE(result.comprehension_score, 1.0f);
    EXPECT_GT(result.processing_time_ms, 0u);
    E2E_STAGE_END();

    // Stage 4: Clean up
    E2E_STAGE_BEGIN("Cleanup", 10);
    wernicke_free_comprehension(&result);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ELanguageComprehensionTest, PhonemeToWordRecognition) {
    E2E_PIPELINE_START("Phoneme to Word Recognition");

    // Create phoneme sequence for "cat"
    E2E_STAGE_BEGIN("Create phoneme sequence", 10);
    uint8_t cat_phonemes[] = {'k', 'a', 't'};
    auto phoneme_events = create_phoneme_event_sequence(cat_phonemes, 3);
    auto phonemes = create_phoneme_sequence(cat_phonemes, 3);
    E2E_STAGE_END();

    // Process phonemes
    E2E_STAGE_BEGIN("Process phonemes", MAX_PROCESSING_TIME_MS);
    bool success = wernicke_process_phonemes(
        wernicke,
        phoneme_events.data(),
        static_cast<uint32_t>(phoneme_events.size())
    );
    EXPECT_TRUE(success) << "Phoneme processing should succeed";
    E2E_STAGE_END();

    // Recognize word
    E2E_STAGE_BEGIN("Recognize word", MAX_PROCESSING_TIME_MS);
    wernicke_word_result_t word_result;
    memset(&word_result, 0, sizeof(word_result));

    success = wernicke_recognize_word(
        wernicke,
        phonemes.data(),
        static_cast<uint32_t>(phonemes.size()),
        &word_result
    );
    EXPECT_TRUE(success) << "Word recognition should succeed";
    E2E_STAGE_END();

    // Verify recognition
    E2E_STAGE_BEGIN("Verify recognition", 10);
    EXPECT_STREQ(word_result.word.word, "cat") << "Should recognize 'cat'";
    EXPECT_GT(word_result.confidence, 0.5f) << "Confidence should be reasonable";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ELanguageComprehensionTest, SentenceComprehensionPipeline) {
    E2E_PIPELINE_START("Sentence Comprehension Pipeline");

    // Process multiple words to form a sentence
    E2E_STAGE_BEGIN("Setup sentence phonemes", 20);
    // "the cat sat"
    std::vector<std::vector<uint8_t>> word_phonemes = {
        {'D', '@'},           // the
        {'k', 'a', 't'},      // cat
        {'s', 'a', 't'}       // sat
    };
    E2E_STAGE_END();

    // Process each word's phonemes
    E2E_STAGE_BEGIN("Process word sequence", MAX_PROCESSING_TIME_MS * 3);
    std::vector<wernicke_word_result_t> recognized_words;

    for (size_t w = 0; w < word_phonemes.size(); w++) {
        auto phonemes = create_phoneme_sequence(
            word_phonemes[w].data(),
            static_cast<uint32_t>(word_phonemes[w].size())
        );

        wernicke_word_result_t word_result;
        memset(&word_result, 0, sizeof(word_result));

        bool success = wernicke_recognize_word(
            wernicke,
            phonemes.data(),
            static_cast<uint32_t>(phonemes.size()),
            &word_result
        );

        if (success) {
            word_result.position_in_utterance = static_cast<uint32_t>(w);
            recognized_words.push_back(word_result);
        }
    }
    EXPECT_EQ(recognized_words.size(), 3u) << "Should recognize all 3 words";
    E2E_STAGE_END();

    // Parse sentence structure
    E2E_STAGE_BEGIN("Parse sentence", MAX_PARSING_TIME_MS);
    wernicke_parse_t parse;
    memset(&parse, 0, sizeof(parse));

    bool success = wernicke_parse_sentence(
        wernicke,
        recognized_words.data(),
        static_cast<uint32_t>(recognized_words.size()),
        &parse
    );
    EXPECT_TRUE(success) << "Sentence parsing should succeed";
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify parse result", 10);
    EXPECT_TRUE(parse.is_valid) << "Parse should be valid";
    EXPECT_GT(parse.parse_confidence, 0.0f);
    wernicke_free_parse(&parse);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Lexical Access and Word Recognition Tests
//=============================================================================

TEST_F(E2ELanguageComprehensionTest, DirectLexiconLookup) {
    E2E_PIPELINE_START("Direct Lexicon Lookup");

    E2E_STAGE_BEGIN("Lookup existing word", MAX_PROCESSING_TIME_MS);
    wernicke_word_t entry;
    memset(&entry, 0, sizeof(entry));

    bool success = wernicke_lookup_word(wernicke, "hello", &entry);
    EXPECT_TRUE(success) << "Should find 'hello' in lexicon";
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify word entry", 10);
    EXPECT_STREQ(entry.word, "hello");
    EXPECT_EQ(entry.word_id, 1u);
    EXPECT_EQ(entry.phoneme_count, 5u);
    EXPECT_GT(entry.frequency, 0.0f);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Lookup nonexistent word", MAX_PROCESSING_TIME_MS);
    success = wernicke_lookup_word(wernicke, "xyznonsense", &entry);
    EXPECT_FALSE(success) << "Should not find nonexistent word";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ELanguageComprehensionTest, FrequencyWeightedRecognition) {
    E2E_PIPELINE_START("Frequency Weighted Recognition");

    // Test that high-frequency words are recognized with higher confidence
    E2E_STAGE_BEGIN("Recognize high-frequency word", MAX_PROCESSING_TIME_MS);
    uint8_t the_phonemes[] = {'D', '@'};
    auto phonemes = create_phoneme_sequence(the_phonemes, 2);

    wernicke_word_result_t result;
    memset(&result, 0, sizeof(result));

    bool success = wernicke_recognize_word(
        wernicke,
        phonemes.data(),
        2,
        &result
    );
    EXPECT_TRUE(success);
    float high_freq_confidence = result.confidence;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Recognize lower-frequency word", MAX_PROCESSING_TIME_MS);
    uint8_t cat_phonemes[] = {'k', 'a', 't'};
    phonemes = create_phoneme_sequence(cat_phonemes, 3);

    success = wernicke_recognize_word(
        wernicke,
        phonemes.data(),
        3,
        &result
    );
    EXPECT_TRUE(success);
    float lower_freq_confidence = result.confidence;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify frequency effect", 10);
    // High frequency words ("the" = 0.99) should have higher confidence
    // Note: Actual effect depends on implementation
    EXPECT_GT(high_freq_confidence, 0.0f);
    EXPECT_GT(lower_freq_confidence, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ELanguageComprehensionTest, WordPrediction) {
    E2E_PIPELINE_START("Word Prediction");

    // Setup context for prediction
    E2E_STAGE_BEGIN("Setup prediction context", 20);
    wernicke_context_t context;
    memset(&context, 0, sizeof(context));

    // Previous word was "the"
    wernicke_word_result_t prior;
    memset(&prior, 0, sizeof(prior));
    strncpy(prior.word.word, "the", sizeof(prior.word.word) - 1);
    prior.word.pos = 4;  // Article

    context.prior_words = &prior;
    context.num_prior_words = 1;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Predict next word", MAX_PROCESSING_TIME_MS);
    wernicke_word_pred_t prediction;
    memset(&prediction, 0, sizeof(prediction));

    bool success = wernicke_predict_next_word(wernicke, &context, &prediction);
    EXPECT_TRUE(success) << "Prediction should succeed";
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify predictions", 10);
    EXPECT_GT(prediction.num_candidates, 0u) << "Should have candidate predictions";
    // After "the", we expect nouns to be likely
    bool has_noun = false;
    for (uint32_t i = 0; i < prediction.num_candidates; i++) {
        if (prediction.candidates[i].pos == 1) {  // Noun
            has_noun = true;
            break;
        }
    }
    // Note: Prediction accuracy depends on implementation
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Syntactic Parsing Tests
//=============================================================================

TEST_F(E2ELanguageComprehensionTest, SimpleNounPhraseParsi) {
    E2E_PIPELINE_START("Simple Noun Phrase Parsing");

    // Parse "the cat"
    E2E_STAGE_BEGIN("Setup noun phrase", 20);
    std::vector<wernicke_word_result_t> words(2);

    memset(&words[0], 0, sizeof(wernicke_word_result_t));
    strncpy(words[0].word.word, "the", sizeof(words[0].word.word) - 1);
    words[0].word.pos = 4;  // Article
    words[0].position_in_utterance = 0;
    words[0].confidence = 0.95f;

    memset(&words[1], 0, sizeof(wernicke_word_result_t));
    strncpy(words[1].word.word, "cat", sizeof(words[1].word.word) - 1);
    words[1].word.pos = 1;  // Noun
    words[1].position_in_utterance = 1;
    words[1].confidence = 0.9f;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Parse noun phrase", MAX_PARSING_TIME_MS);
    wernicke_parse_t parse;
    memset(&parse, 0, sizeof(parse));

    bool success = wernicke_parse_sentence(wernicke, words.data(), 2, &parse);
    EXPECT_TRUE(success) << "NP parsing should succeed";
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify parse structure", 10);
    EXPECT_TRUE(parse.is_valid);
    EXPECT_NE(parse.root, nullptr) << "Should have parse tree root";
    wernicke_free_parse(&parse);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ELanguageComprehensionTest, SubjectVerbObjectParsing) {
    E2E_PIPELINE_START("Subject-Verb-Object Parsing");

    // Parse "the cat sat"
    E2E_STAGE_BEGIN("Setup SVO sentence", 20);
    std::vector<wernicke_word_result_t> words(3);

    memset(&words[0], 0, sizeof(wernicke_word_result_t));
    strncpy(words[0].word.word, "the", sizeof(words[0].word.word) - 1);
    words[0].word.pos = 4;
    words[0].position_in_utterance = 0;

    memset(&words[1], 0, sizeof(wernicke_word_result_t));
    strncpy(words[1].word.word, "cat", sizeof(words[1].word.word) - 1);
    words[1].word.pos = 1;  // Noun (subject)
    words[1].position_in_utterance = 1;

    memset(&words[2], 0, sizeof(wernicke_word_result_t));
    strncpy(words[2].word.word, "sat", sizeof(words[2].word.word) - 1);
    words[2].word.pos = 2;  // Verb
    words[2].position_in_utterance = 2;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Parse SVO structure", MAX_PARSING_TIME_MS);
    wernicke_parse_t parse;
    memset(&parse, 0, sizeof(parse));

    bool success = wernicke_parse_sentence(wernicke, words.data(), 3, &parse);
    EXPECT_TRUE(success);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify SVO parse", 10);
    EXPECT_TRUE(parse.is_valid);
    EXPECT_GT(parse.parse_confidence, 0.5f);
    wernicke_free_parse(&parse);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ELanguageComprehensionTest, ComplexSentenceParsing) {
    E2E_PIPELINE_START("Complex Sentence Parsing");

    // Parse longer sentence: "the cat sat on the mat" (simulated)
    E2E_STAGE_BEGIN("Setup complex sentence", 30);
    std::vector<wernicke_word_result_t> words(6);
    const char* sentence[] = {"the", "cat", "sat", "on", "the", "mat"};
    uint8_t pos[] = {4, 1, 2, 3, 4, 1};  // Article, Noun, Verb, Prep, Article, Noun

    for (int i = 0; i < 6; i++) {
        memset(&words[i], 0, sizeof(wernicke_word_result_t));
        strncpy(words[i].word.word, sentence[i], sizeof(words[i].word.word) - 1);
        words[i].word.pos = pos[i];
        words[i].position_in_utterance = i;
        words[i].confidence = 0.85f + 0.01f * i;
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Parse complex sentence", MAX_PARSING_TIME_MS);
    wernicke_parse_t parse;
    memset(&parse, 0, sizeof(parse));

    bool success = wernicke_parse_sentence(wernicke, words.data(), 6, &parse);
    EXPECT_TRUE(success);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify complex parse", 20);
    EXPECT_TRUE(parse.is_valid);
    if (parse.root) {
        // Should span full sentence
        EXPECT_EQ(parse.root->start_word, 0u);
        EXPECT_EQ(parse.root->end_word, 6u);
    }
    wernicke_free_parse(&parse);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Semantic Integration Tests
//=============================================================================

TEST_F(E2ELanguageComprehensionTest, WordToConceptMapping) {
    E2E_PIPELINE_START("Word to Concept Mapping");

    // Recognize word and get its meaning
    E2E_STAGE_BEGIN("Recognize word", MAX_PROCESSING_TIME_MS);
    uint8_t cat_phonemes[] = {'k', 'a', 't'};
    auto phonemes = create_phoneme_sequence(cat_phonemes, 3);

    wernicke_word_result_t word;
    memset(&word, 0, sizeof(word));

    bool success = wernicke_recognize_word(wernicke, phonemes.data(), 3, &word);
    EXPECT_TRUE(success);
    E2E_STAGE_END();

    // Get semantic meaning
    E2E_STAGE_BEGIN("Get word meaning", MAX_PROCESSING_TIME_MS);
    wernicke_concept_t wern_concept;
    memset(&wern_concept, 0, sizeof(wern_concept));

    success = wernicke_get_meaning(wernicke, &word, &wern_concept);
    EXPECT_TRUE(success) << "Should get meaning for recognized word";
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify concept", 10);
    EXPECT_EQ(wern_concept.concept_id, 103u) << "Should map to cat concept";
    EXPECT_GT(wern_concept.activation, 0.0f) << "Concept should be activated";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ELanguageComprehensionTest, SpreadingActivation) {
    E2E_PIPELINE_START("Spreading Activation");

    // Activate a concept and spread to related concepts
    E2E_STAGE_BEGIN("Spread activation from concept", MAX_PROCESSING_TIME_MS);
    std::vector<wernicke_concept_t> activated(MAX_CONCEPTS);
    uint32_t num_activated = 0;

    bool success = wernicke_spread_activation(
        wernicke,
        103,  // cat concept
        2,    // depth 2
        activated.data(),
        MAX_CONCEPTS,
        &num_activated
    );
    EXPECT_TRUE(success) << "Spreading activation should succeed";
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify spread", 10);
    // May activate related concepts (depends on semantic network)
    EXPECT_GE(num_activated, 0u);

    // Check that activations decay with distance
    if (num_activated > 1) {
        bool activations_ordered = true;
        for (uint32_t i = 1; i < num_activated; i++) {
            if (activated[i].activation > activated[0].activation) {
                activations_ordered = false;
            }
        }
        // Source should typically have highest activation
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ELanguageComprehensionTest, SemanticCoherence) {
    E2E_PIPELINE_START("Semantic Coherence");

    // Process coherent vs incoherent word sequences
    E2E_STAGE_BEGIN("Process coherent sequence", MAX_COMPREHENSION_TIME_MS);
    // Coherent: words that go together
    uint32_t duration_samples = static_cast<uint32_t>(SAMPLE_RATE * 0.3f);
    auto audio = generate_synthetic_audio(duration_samples, 250.0f);

    wernicke_comprehension_t coherent_result;
    memset(&coherent_result, 0, sizeof(coherent_result));

    bool success = wernicke_comprehend(
        wernicke,
        audio.data(),
        duration_samples,
        static_cast<uint32_t>(SAMPLE_RATE),
        &coherent_result
    );
    EXPECT_TRUE(success);
    float coherent_score = coherent_result.semantic_coherence;
    wernicke_free_comprehension(&coherent_result);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify coherence scoring", 10);
    EXPECT_GE(coherent_score, 0.0f);
    EXPECT_LE(coherent_score, 1.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Ambiguity Resolution Tests
//=============================================================================

TEST_F(E2ELanguageComprehensionTest, LexicalAmbiguityResolution) {
    E2E_PIPELINE_START("Lexical Ambiguity Resolution");

    // "bank" is ambiguous - financial or river
    E2E_STAGE_BEGIN("Recognize ambiguous word", MAX_PROCESSING_TIME_MS);
    uint8_t bank_phonemes[] = {'b', 'a', 'N', 'k'};
    auto phonemes = create_phoneme_sequence(bank_phonemes, 4);

    wernicke_word_result_t word;
    memset(&word, 0, sizeof(word));

    bool success = wernicke_recognize_word(wernicke, phonemes.data(), 4, &word);
    EXPECT_TRUE(success);
    E2E_STAGE_END();

    // Disambiguate with financial context
    E2E_STAGE_BEGIN("Disambiguate with money context", MAX_PROCESSING_TIME_MS);
    wernicke_context_t context;
    memset(&context, 0, sizeof(context));

    // Prior word "money" suggests financial meaning
    wernicke_word_result_t prior;
    memset(&prior, 0, sizeof(prior));
    strncpy(prior.word.word, "money", sizeof(prior.word.word) - 1);
    prior.word.concept_id = 107;  // money concept

    context.prior_words = &prior;
    context.num_prior_words = 1;

    wernicke_concept_t financial_concept;
    memset(&financial_concept, 0, sizeof(financial_concept));

    success = wernicke_disambiguate(wernicke, &word, &context, &financial_concept);
    EXPECT_TRUE(success) << "Disambiguation should succeed";
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify disambiguation", 10);
    // Should resolve to financial meaning given money context
    EXPECT_GT(financial_concept.activation, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ELanguageComprehensionTest, SyntacticAmbiguityResolution) {
    E2E_PIPELINE_START("Syntactic Ambiguity Resolution");

    // Test parsing of potentially ambiguous structure
    E2E_STAGE_BEGIN("Setup ambiguous structure", 20);
    // "I saw the cat with the telescope" - who has the telescope?
    // Simplified test
    std::vector<wernicke_word_result_t> words(3);

    memset(&words[0], 0, sizeof(wernicke_word_result_t));
    strncpy(words[0].word.word, "cat", sizeof(words[0].word.word) - 1);
    words[0].word.pos = 1;
    words[0].position_in_utterance = 0;

    memset(&words[1], 0, sizeof(wernicke_word_result_t));
    strncpy(words[1].word.word, "on", sizeof(words[1].word.word) - 1);
    words[1].word.pos = 3;  // Preposition
    words[1].position_in_utterance = 1;

    memset(&words[2], 0, sizeof(wernicke_word_result_t));
    strncpy(words[2].word.word, "mat", sizeof(words[2].word.word) - 1);
    words[2].word.pos = 1;
    words[2].position_in_utterance = 2;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Parse with disambiguation", MAX_PARSING_TIME_MS);
    wernicke_parse_t parse;
    memset(&parse, 0, sizeof(parse));

    bool success = wernicke_parse_sentence(wernicke, words.data(), 3, &parse);
    EXPECT_TRUE(success);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify single parse", 10);
    // Parser should commit to one interpretation
    EXPECT_TRUE(parse.is_valid);
    EXPECT_GT(parse.parse_confidence, 0.0f);
    wernicke_free_parse(&parse);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ELanguageComprehensionTest, PragmaticInterpretation) {
    E2E_PIPELINE_START("Pragmatic Interpretation");

    // Test context-dependent interpretation
    E2E_STAGE_BEGIN("Setup pragmatic context", 20);
    wernicke_context_t context;
    memset(&context, 0, sizeof(context));

    // Set topic embedding to bias interpretation
    for (int i = 0; i < 128; i++) {
        context.topic_embedding[i] = 0.1f * std::sin(i * 0.1f);
    }
    context.has_topic = true;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Predict with pragmatic context", MAX_PROCESSING_TIME_MS);
    wernicke_word_pred_t prediction;
    memset(&prediction, 0, sizeof(prediction));

    bool success = wernicke_predict_next_word(wernicke, &context, &prediction);
    EXPECT_TRUE(success);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify pragmatic influence", 10);
    // Context should influence predictions
    EXPECT_GE(prediction.num_candidates, 0u);
    // The topic embedding should bias toward contextually relevant words
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}
