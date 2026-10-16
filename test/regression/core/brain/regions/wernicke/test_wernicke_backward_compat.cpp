/**
 * @file test_wernicke_backward_compat.cpp
 * @brief Backward compatibility regression tests for Wernicke's area adapter
 *
 * WHAT: Tests API stability and backward compatibility for Wernicke module
 * WHY:  Ensure API changes don't break existing code
 * HOW:  Verify struct sizes, enum values, function signatures remain stable
 *
 * COVERAGE TARGET: API backward compatibility
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

#include "core/brain/regions/wernicke/nimcp_wernicke_adapter.h"
#include "perception/nimcp_speech_cortex.h"

//=============================================================================
// API Stability Tests - Configuration
//=============================================================================

class WernickeBackwardCompatConfigTest : public ::testing::Test {
protected:
    wernicke_config_t config;

    void SetUp() override {
        config = wernicke_default_config();
    }
};

/**
 * @test Default configuration values are stable
 * WHAT: Verify default config values match expected
 * WHY:  Code depending on defaults must continue working
 */
TEST_F(WernickeBackwardCompatConfigTest, DefaultConfigValues) {
    // These values should remain stable across versions
    EXPECT_EQ(config.max_phonemes, WERNICKE_DEFAULT_MAX_PHONEMES);
    EXPECT_EQ(config.max_words, WERNICKE_DEFAULT_MAX_WORDS);
    EXPECT_EQ(config.max_concepts, WERNICKE_DEFAULT_MAX_CONCEPTS);
    EXPECT_EQ(config.lexicon_size, WERNICKE_DEFAULT_LEXICON_SIZE);
    EXPECT_EQ(config.working_memory_slots, WERNICKE_DEFAULT_WORKING_MEMORY_SLOTS);
    EXPECT_EQ(config.embedding_dim, WERNICKE_DEFAULT_EMBEDDING_DIM);
    EXPECT_FLOAT_EQ(config.processing_window_ms, WERNICKE_DEFAULT_PROCESSING_WINDOW_MS);
    EXPECT_EQ(config.formant_count, WERNICKE_DEFAULT_FORMANT_COUNT);
}

/**
 * @test Default enable flags
 * WHAT: Verify processing layer enable flags defaults
 * WHY:  Module initialization depends on these defaults
 */
TEST_F(WernickeBackwardCompatConfigTest, DefaultEnableFlags) {
    // Phonological and lexical should be enabled by default
    EXPECT_TRUE(config.enable_phonological);
    EXPECT_TRUE(config.enable_lexical);
    EXPECT_TRUE(config.enable_lexicon);

    // Working memory should be enabled
    EXPECT_TRUE(config.enable_working_memory);
}

/**
 * @test Config struct field presence
 * WHAT: Verify all expected fields exist in config struct
 * WHY:  Field additions/removals break ABI
 */
TEST_F(WernickeBackwardCompatConfigTest, ConfigStructFields) {
    // Test all expected fields exist by assignment
    config.max_phonemes = 100;
    config.max_words = 50;
    config.max_concepts = 200;
    config.lexicon_size = 5000;
    config.enable_lexicon = true;
    config.working_memory_slots = 7;
    config.enable_working_memory = true;
    config.enable_phonological = true;
    config.enable_lexical = true;
    config.enable_semantic = true;
    config.enable_syntactic = true;
    config.embedding_dim = 64;
    config.formant_count = 4;
    config.enable_audiovisual = true;
    config.enable_prosody = true;
    config.enable_broca_connection = true;
    config.enable_semantic_memory = true;
    config.enable_kg_registration = true;
    config.enable_events = true;
    config.enable_training = true;
    config.learning_rate = 0.01f;
    config.processing_window_ms = 100.0f;
    config.enable_bio_async = true;

    // All fields should be assignable
    SUCCEED();
}

//=============================================================================
// API Stability Tests - Status and Error Enums
//=============================================================================

class WernickeBackwardCompatEnumTest : public ::testing::Test {};

/**
 * @test Status enum values are stable
 * WHAT: Verify status enum values haven't changed
 * WHY:  Code comparing status values must continue working
 */
TEST_F(WernickeBackwardCompatEnumTest, StatusEnumValues) {
    // These specific values must remain stable
    EXPECT_EQ(static_cast<int>(WERNICKE_STATUS_IDLE), 0);
    EXPECT_EQ(static_cast<int>(WERNICKE_STATUS_PHONOLOGICAL), 1);
    EXPECT_EQ(static_cast<int>(WERNICKE_STATUS_LEXICAL_ACCESS), 2);
    EXPECT_EQ(static_cast<int>(WERNICKE_STATUS_SEMANTIC), 3);
    EXPECT_EQ(static_cast<int>(WERNICKE_STATUS_SYNTACTIC), 4);
    EXPECT_EQ(static_cast<int>(WERNICKE_STATUS_COMPREHENSION_READY), 5);
    EXPECT_EQ(static_cast<int>(WERNICKE_STATUS_ERROR), 6);
}

/**
 * @test Error enum values are stable
 * WHAT: Verify error enum values haven't changed
 * WHY:  Error handling code depends on these values
 */
TEST_F(WernickeBackwardCompatEnumTest, ErrorEnumValues) {
    EXPECT_EQ(static_cast<int>(WERNICKE_ERROR_NONE), 0);
    EXPECT_EQ(static_cast<int>(WERNICKE_ERROR_INVALID_INPUT), 1);
    EXPECT_EQ(static_cast<int>(WERNICKE_ERROR_PHONOLOGICAL_FAILURE), 2);
    EXPECT_EQ(static_cast<int>(WERNICKE_ERROR_LEXICAL_FAILURE), 3);
    EXPECT_EQ(static_cast<int>(WERNICKE_ERROR_SEMANTIC_FAILURE), 4);
    EXPECT_EQ(static_cast<int>(WERNICKE_ERROR_SYNTACTIC_FAILURE), 5);
    EXPECT_EQ(static_cast<int>(WERNICKE_ERROR_WORKING_MEMORY_FULL), 6);
    EXPECT_EQ(static_cast<int>(WERNICKE_ERROR_WORD_NOT_FOUND), 7);
    EXPECT_EQ(static_cast<int>(WERNICKE_ERROR_CONCEPT_NOT_FOUND), 8);
    EXPECT_EQ(static_cast<int>(WERNICKE_ERROR_BUFFER_OVERFLOW), 9);
    EXPECT_EQ(static_cast<int>(WERNICKE_ERROR_BROCA_DISCONNECTED), 10);
    EXPECT_EQ(static_cast<int>(WERNICKE_ERROR_INTERNAL), 11);
}

/**
 * @test Status string function exists
 * WHAT: Verify wernicke_status_string returns valid strings
 * WHY:  Logging code depends on this function
 */
TEST_F(WernickeBackwardCompatEnumTest, StatusStringFunction) {
    const char* str = wernicke_status_string(WERNICKE_STATUS_IDLE);
    ASSERT_NE(str, nullptr);
    EXPECT_GT(strlen(str), 0u);

    str = wernicke_status_string(WERNICKE_STATUS_COMPREHENSION_READY);
    ASSERT_NE(str, nullptr);
    EXPECT_GT(strlen(str), 0u);
}

/**
 * @test Error string function exists
 * WHAT: Verify wernicke_error_string returns valid strings
 * WHY:  Error handling/logging depends on this function
 */
TEST_F(WernickeBackwardCompatEnumTest, ErrorStringFunction) {
    const char* str = wernicke_error_string(WERNICKE_ERROR_NONE);
    ASSERT_NE(str, nullptr);
    EXPECT_GT(strlen(str), 0u);

    str = wernicke_error_string(WERNICKE_ERROR_LEXICAL_FAILURE);
    ASSERT_NE(str, nullptr);
    EXPECT_GT(strlen(str), 0u);
}

//=============================================================================
// API Stability Tests - Lifecycle Functions
//=============================================================================

class WernickeBackwardCompatLifecycleTest : public ::testing::Test {
protected:
    wernicke_adapter_t* adapter;

    void SetUp() override {
        adapter = nullptr;
    }

    void TearDown() override {
        if (adapter) {
            wernicke_destroy(adapter);
            adapter = nullptr;
        }
    }
};

/**
 * @test wernicke_create with NULL config returns valid adapter
 * WHAT: NULL config should use defaults
 * WHY:  Legacy code may pass NULL for defaults
 */
TEST_F(WernickeBackwardCompatLifecycleTest, CreateWithNullConfig) {
    adapter = wernicke_create(nullptr);
    ASSERT_NE(adapter, nullptr);

    wernicke_status_t status = wernicke_get_status(adapter);
    EXPECT_EQ(status, WERNICKE_STATUS_IDLE);
}

/**
 * @test wernicke_destroy with NULL is safe
 * WHAT: Destroying NULL should not crash
 * WHY:  Code may call destroy on potentially-NULL pointers
 */
TEST_F(WernickeBackwardCompatLifecycleTest, DestroyNullSafe) {
    wernicke_destroy(nullptr);  // Should not crash
    SUCCEED();
}

/**
 * @test wernicke_reset returns false for NULL
 * WHAT: Reset on NULL should return false
 * WHY:  Safety check for invalid input
 */
TEST_F(WernickeBackwardCompatLifecycleTest, ResetNullReturnsFalse) {
    bool result = wernicke_reset(nullptr);
    EXPECT_FALSE(result);
}

/**
 * @test wernicke_default_config function exists
 * WHAT: Default config function must exist
 * WHY:  Initialization code depends on this
 */
TEST_F(WernickeBackwardCompatLifecycleTest, DefaultConfigFunctionExists) {
    wernicke_config_t config = wernicke_default_config();
    // Just verify function exists and returns valid struct
    EXPECT_GT(config.max_phonemes, 0u);
}

//=============================================================================
// API Stability Tests - Word Structure
//=============================================================================

class WernickeBackwardCompatWordTest : public ::testing::Test {
protected:
    wernicke_adapter_t* adapter;

    void SetUp() override {
        adapter = wernicke_create(nullptr);
        ASSERT_NE(adapter, nullptr);
    }

    void TearDown() override {
        if (adapter) {
            wernicke_destroy(adapter);
        }
    }
};

/**
 * @test wernicke_word_t struct field access
 * WHAT: Verify all expected fields exist in word struct
 * WHY:  Lexicon code depends on these fields
 */
TEST_F(WernickeBackwardCompatWordTest, WordStructFields) {
    wernicke_word_t word;
    memset(&word, 0, sizeof(word));

    // Test all expected fields exist
    word.word_id = 1;
    word.frequency = 0.5f;
    word.concept_id = 100;
    word.pos = 1;  // Part of speech
    word.phoneme_count = 3;

    // String fields
    strncpy(word.word, "test", sizeof(word.word) - 1);
    word.phonemes[0] = 't';
    word.phonemes[1] = 'e';
    word.phonemes[2] = 's';

    EXPECT_EQ(word.word_id, 1u);
    EXPECT_FLOAT_EQ(word.frequency, 0.5f);
    EXPECT_STREQ(word.word, "test");
}

/**
 * @test wernicke_add_word and wernicke_lookup_word roundtrip
 * WHAT: Words added can be looked up
 * WHY:  Lexicon operations must work correctly
 */
TEST_F(WernickeBackwardCompatWordTest, AddAndLookupWord) {
    wernicke_word_t word;
    memset(&word, 0, sizeof(word));
    word.word_id = 42;
    strncpy(word.word, "hello", sizeof(word.word) - 1);
    word.phonemes[0] = 'h';
    word.phonemes[1] = 'e';
    word.phonemes[2] = 'l';
    word.phonemes[3] = 'l';
    word.phonemes[4] = 'o';
    word.phoneme_count = 5;
    word.frequency = 0.8f;
    word.concept_id = 100;
    word.pos = 1;

    bool added = wernicke_add_word(adapter, &word);
    ASSERT_TRUE(added) << "Failed to add word to lexicon";

    wernicke_word_t retrieved;
    memset(&retrieved, 0, sizeof(retrieved));
    bool found = wernicke_lookup_word(adapter, "hello", &retrieved);
    ASSERT_TRUE(found) << "Failed to find word in lexicon";

    EXPECT_EQ(retrieved.word_id, word.word_id);
    EXPECT_STREQ(retrieved.word, word.word);
    EXPECT_EQ(retrieved.phoneme_count, word.phoneme_count);
}

/**
 * @test wernicke_lookup_word returns false for unknown word
 * WHAT: Looking up nonexistent word returns false
 * WHY:  Error handling for missing words
 */
TEST_F(WernickeBackwardCompatWordTest, LookupUnknownWord) {
    wernicke_word_t entry;
    bool found = wernicke_lookup_word(adapter, "xyzzynonexistent", &entry);
    EXPECT_FALSE(found);
}

//=============================================================================
// API Stability Tests - Working Memory
//=============================================================================

class WernickeBackwardCompatWorkingMemoryTest : public ::testing::Test {
protected:
    wernicke_adapter_t* adapter;
    wernicke_config_t config;

    void SetUp() override {
        config = wernicke_default_config();
        config.enable_working_memory = true;
        adapter = wernicke_create(&config);
        ASSERT_NE(adapter, nullptr);
    }

    void TearDown() override {
        if (adapter) {
            wernicke_destroy(adapter);
        }
    }
};

/**
 * @test Working memory store and retrieve
 * WHAT: Phonemes can be stored and retrieved from working memory
 * WHY:  Phonological loop is fundamental to language processing
 */
TEST_F(WernickeBackwardCompatWorkingMemoryTest, StoreAndRetrieve) {
    phoneme_t phonemes[] = {PHONEME_AH, PHONEME_B, PHONEME_K};
    uint32_t count = 3;

    bool stored = wernicke_wm_store(adapter, phonemes, count);
    ASSERT_TRUE(stored) << "Failed to store phonemes in working memory";

    phoneme_t retrieved[10];
    uint32_t retrieved_count = 0;
    bool success = wernicke_wm_get_contents(adapter, retrieved, 10, &retrieved_count);
    ASSERT_TRUE(success) << "Failed to get working memory contents";

    EXPECT_EQ(retrieved_count, count);
    EXPECT_EQ(retrieved[0], PHONEME_AH);
    EXPECT_EQ(retrieved[1], PHONEME_B);
    EXPECT_EQ(retrieved[2], PHONEME_K);
}

/**
 * @test Working memory clear
 * WHAT: Clear empties working memory
 * WHY:  Reset between utterances
 */
TEST_F(WernickeBackwardCompatWorkingMemoryTest, ClearWorkingMemory) {
    phoneme_t phonemes[] = {PHONEME_AH, PHONEME_B};
    wernicke_wm_store(adapter, phonemes, 2);

    wernicke_wm_clear(adapter);

    phoneme_t retrieved[10];
    uint32_t count = 0;
    wernicke_wm_get_contents(adapter, retrieved, 10, &count);
    EXPECT_EQ(count, 0u);
}

/**
 * @test Working memory rehearsal function exists
 * WHAT: Rehearse function is callable
 * WHY:  Memory decay prevention depends on this
 */
TEST_F(WernickeBackwardCompatWorkingMemoryTest, RehearseFunction) {
    phoneme_t phonemes[] = {PHONEME_AH};
    wernicke_wm_store(adapter, phonemes, 1);

    bool result = wernicke_wm_rehearse(adapter);
    EXPECT_TRUE(result);
}

//=============================================================================
// API Stability Tests - Statistics Structure
//=============================================================================

class WernickeBackwardCompatStatsTest : public ::testing::Test {
protected:
    wernicke_adapter_t* adapter;

    void SetUp() override {
        adapter = wernicke_create(nullptr);
        ASSERT_NE(adapter, nullptr);
    }

    void TearDown() override {
        if (adapter) {
            wernicke_destroy(adapter);
        }
    }
};

/**
 * @test Stats struct fields exist
 * WHAT: Verify all expected stats fields are present
 * WHY:  Monitoring/logging code depends on these
 */
TEST_F(WernickeBackwardCompatStatsTest, StatsStructFields) {
    wernicke_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    bool success = wernicke_get_stats(adapter, &stats);
    ASSERT_TRUE(success);

    // Verify fields exist by accessing them
    (void)stats.phonemes_processed;
    (void)stats.words_recognized;
    (void)stats.concepts_activated;
    (void)stats.utterances_comprehended;
    (void)stats.successful_recognitions;
    (void)stats.phonological_errors;
    (void)stats.lexical_misses;
    (void)stats.semantic_errors;
    (void)stats.syntactic_errors;
    (void)stats.avg_phoneme_latency_ms;
    (void)stats.avg_word_latency_ms;
    (void)stats.avg_comprehension_latency_ms;
    (void)stats.max_latency_ms;
    (void)stats.training_iterations;
    (void)stats.training_loss;
    (void)stats.audiovisual_fusions;
    (void)stats.mcgurk_effects;

    SUCCEED();
}

/**
 * @test Initial stats are zero
 * WHAT: Fresh adapter has zero counts
 * WHY:  Stats accumulate from baseline
 */
TEST_F(WernickeBackwardCompatStatsTest, InitialStatsZero) {
    wernicke_stats_t stats;
    wernicke_get_stats(adapter, &stats);

    EXPECT_EQ(stats.phonemes_processed, 0u);
    EXPECT_EQ(stats.words_recognized, 0u);
    EXPECT_EQ(stats.phonological_errors, 0u);
    EXPECT_EQ(stats.lexical_misses, 0u);
}

//=============================================================================
// API Stability Tests - Concept and Comprehension Structures
//=============================================================================

class WernickeBackwardCompatStructTest : public ::testing::Test {};

/**
 * @test wernicke_concept_t struct fields
 * WHAT: Verify concept struct fields exist
 * WHY:  Semantic processing depends on this structure
 */
TEST_F(WernickeBackwardCompatStructTest, ConceptStructFields) {
    wernicke_concept_t concept;
    memset(&concept, 0, sizeof(concept));

    concept.concept_id = 1;
    concept.activation = 0.5f;
    concept.embedding = nullptr;  // Optional
    concept.embedding_dim = 128;
    concept.related_concepts = nullptr;  // Optional
    concept.num_related = 0;
    strncpy(concept.concept_name, "test_concept", sizeof(concept.concept_name) - 1);

    EXPECT_EQ(concept.concept_id, 1u);
    EXPECT_FLOAT_EQ(concept.activation, 0.5f);
    EXPECT_STREQ(concept.concept_name, "test_concept");
}

/**
 * @test wernicke_word_result_t struct fields
 * WHAT: Verify word result struct fields exist
 * WHY:  Recognition results depend on this structure
 */
TEST_F(WernickeBackwardCompatStructTest, WordResultStructFields) {
    wernicke_word_result_t result;
    memset(&result, 0, sizeof(result));

    result.confidence = 0.9f;
    result.onset_time_ms = 100;
    result.offset_time_ms = 200;
    result.position_in_utterance = 0;

    EXPECT_FLOAT_EQ(result.confidence, 0.9f);
    EXPECT_EQ(result.onset_time_ms, 100u);
    EXPECT_EQ(result.offset_time_ms, 200u);
}

/**
 * @test wernicke_context_t struct fields
 * WHAT: Verify context struct fields exist
 * WHY:  Disambiguation depends on context structure
 */
TEST_F(WernickeBackwardCompatStructTest, ContextStructFields) {
    wernicke_context_t context;
    memset(&context, 0, sizeof(context));

    context.prior_words = nullptr;
    context.num_prior_words = 0;
    context.active_concepts = nullptr;
    context.num_active_concepts = 0;
    context.has_topic = false;

    EXPECT_EQ(context.num_prior_words, 0u);
    EXPECT_FALSE(context.has_topic);
}

/**
 * @test wernicke_comprehension_t struct fields
 * WHAT: Verify comprehension result struct exists
 * WHY:  Complete comprehension pipeline depends on this
 */
TEST_F(WernickeBackwardCompatStructTest, ComprehensionStructFields) {
    wernicke_comprehension_t comp;
    memset(&comp, 0, sizeof(comp));

    comp.phoneme_count = 10;
    comp.avg_phoneme_confidence = 0.8f;
    comp.words = nullptr;
    comp.word_count = 0;
    comp.concepts = nullptr;
    comp.concept_count = 0;
    comp.semantic_coherence = 0.7f;
    comp.parse = nullptr;
    comp.comprehension_score = 0.85f;
    comp.processing_time_ms = 50;

    EXPECT_EQ(comp.phoneme_count, 10u);
    EXPECT_FLOAT_EQ(comp.semantic_coherence, 0.7f);
}

//=============================================================================
// API Stability Tests - Callback Types
//=============================================================================

class WernickeBackwardCompatCallbackTest : public ::testing::Test {
protected:
    wernicke_adapter_t* adapter;
    static int callback_count;

    static void word_callback(const wernicke_word_result_t*, void*) {
        callback_count++;
    }

    static void concept_callback(const wernicke_concept_t*, void*) {
        callback_count++;
    }

    static void comprehension_callback(const wernicke_comprehension_t*, void*) {
        callback_count++;
    }

    void SetUp() override {
        callback_count = 0;
        wernicke_config_t config = wernicke_default_config();
        config.enable_events = true;
        adapter = wernicke_create(&config);
        ASSERT_NE(adapter, nullptr);
    }

    void TearDown() override {
        if (adapter) {
            wernicke_destroy(adapter);
        }
    }
};

int WernickeBackwardCompatCallbackTest::callback_count = 0;

/**
 * @test Set word callback function exists
 * WHAT: Callback registration functions exist
 * WHY:  Event-driven code depends on callbacks
 */
TEST_F(WernickeBackwardCompatCallbackTest, SetWordCallback) {
    bool result = wernicke_set_word_callback(adapter, word_callback, nullptr);
    EXPECT_TRUE(result);
}

/**
 * @test Set concept callback function exists
 */
TEST_F(WernickeBackwardCompatCallbackTest, SetConceptCallback) {
    bool result = wernicke_set_concept_callback(adapter, concept_callback, nullptr);
    EXPECT_TRUE(result);
}

/**
 * @test Set comprehension callback function exists
 */
TEST_F(WernickeBackwardCompatCallbackTest, SetComprehensionCallback) {
    bool result = wernicke_set_comprehension_callback(adapter, comprehension_callback, nullptr);
    EXPECT_TRUE(result);
}

//=============================================================================
// API Stability Tests - Default Constant Values
//=============================================================================

class WernickeBackwardCompatConstantsTest : public ::testing::Test {};

/**
 * @test Default constant values are stable
 * WHAT: Verify default constants haven't changed
 * WHY:  Code may depend on specific default values
 */
TEST_F(WernickeBackwardCompatConstantsTest, DefaultConstants) {
    EXPECT_EQ(WERNICKE_DEFAULT_MAX_PHONEMES, 256);
    EXPECT_EQ(WERNICKE_DEFAULT_MAX_WORDS, 128);
    EXPECT_EQ(WERNICKE_DEFAULT_MAX_CONCEPTS, 1024);
    EXPECT_EQ(WERNICKE_DEFAULT_LEXICON_SIZE, 10000);
    EXPECT_EQ(WERNICKE_DEFAULT_WORKING_MEMORY_SLOTS, 9);
    EXPECT_EQ(WERNICKE_DEFAULT_EMBEDDING_DIM, 128);
    EXPECT_FLOAT_EQ(WERNICKE_DEFAULT_PROCESSING_WINDOW_MS, 100.0f);
    EXPECT_EQ(WERNICKE_DEFAULT_FORMANT_COUNT, 4);
}

//=============================================================================
// API Stability Tests - Sub-module Access Functions
//=============================================================================

class WernickeBackwardCompatSubmoduleTest : public ::testing::Test {
protected:
    wernicke_adapter_t* adapter;

    void SetUp() override {
        wernicke_config_t config = wernicke_default_config();
        config.enable_phonological = true;
        config.enable_lexical = true;
        config.enable_semantic = true;
        config.enable_syntactic = true;
        adapter = wernicke_create(&config);
        ASSERT_NE(adapter, nullptr);
    }

    void TearDown() override {
        if (adapter) {
            wernicke_destroy(adapter);
        }
    }
};

/**
 * @test Get phonological analyzer function exists
 * WHAT: Sub-module access functions exist
 * WHY:  Advanced usage requires direct sub-module access
 * NOTE: Implementation may not expose internal modules (return NULL)
 */
TEST_F(WernickeBackwardCompatSubmoduleTest, GetPhonologicalAnalyzer) {
    phonological_analyzer_t* phon = wernicke_get_phonological_analyzer(adapter);
    // Function exists and is callable (may return NULL if not exposed)
    (void)phon;
    SUCCEED();
}

/**
 * @test Get lexical access function exists
 * NOTE: Implementation may not expose internal modules (return NULL)
 */
TEST_F(WernickeBackwardCompatSubmoduleTest, GetLexicalAccess) {
    lexical_access_t* lex = wernicke_get_lexical_access(adapter);
    (void)lex;
    SUCCEED();
}

/**
 * @test Get semantic integrator function exists
 * NOTE: Implementation may not expose internal modules (return NULL)
 */
TEST_F(WernickeBackwardCompatSubmoduleTest, GetSemanticIntegrator) {
    semantic_integrator_t* sem = wernicke_get_semantic_integrator(adapter);
    (void)sem;
    SUCCEED();
}

/**
 * @test Get syntactic comprehension function exists
 * NOTE: Implementation may not expose internal modules (return NULL)
 */
TEST_F(WernickeBackwardCompatSubmoduleTest, GetSyntacticComprehension) {
    syntactic_comprehension_t* syn = wernicke_get_syntactic_comprehension(adapter);
    (void)syn;
    SUCCEED();
}

//=============================================================================
// API Stability Tests - Processing Functions Signatures
//=============================================================================

class WernickeBackwardCompatProcessingTest : public ::testing::Test {
protected:
    wernicke_adapter_t* adapter;

    void SetUp() override {
        adapter = wernicke_create(nullptr);
        ASSERT_NE(adapter, nullptr);
    }

    void TearDown() override {
        if (adapter) {
            wernicke_destroy(adapter);
        }
    }
};

/**
 * @test wernicke_process_audio signature
 * WHAT: Audio processing function has expected signature
 * WHY:  Pipeline integration depends on this
 */
TEST_F(WernickeBackwardCompatProcessingTest, ProcessAudioSignature) {
    float audio[100] = {0};
    phoneme_event_t phonemes[10];
    uint32_t detected = 0;

    // Function should be callable with these parameters
    bool result = wernicke_process_audio(adapter, audio, 100, 16000, phonemes, 10, &detected);
    // Result depends on audio content, but function should not crash
    (void)result;
    SUCCEED();
}

/**
 * @test wernicke_process_phonemes signature
 * WHAT: Phoneme processing function exists
 * WHY:  Integration with speech cortex
 */
TEST_F(WernickeBackwardCompatProcessingTest, ProcessPhonemesSignature) {
    phoneme_event_t events[3];
    memset(events, 0, sizeof(events));
    events[0].phoneme = PHONEME_AH;
    events[1].phoneme = PHONEME_B;
    events[2].phoneme = PHONEME_K;

    bool result = wernicke_process_phonemes(adapter, events, 3);
    (void)result;
    SUCCEED();
}

/**
 * @test wernicke_recognize_word signature
 * WHAT: Word recognition function exists
 * WHY:  Lexical access is core functionality
 */
TEST_F(WernickeBackwardCompatProcessingTest, RecognizeWordSignature) {
    phoneme_t phonemes[] = {PHONEME_AH};
    wernicke_word_result_t result;

    bool found = wernicke_recognize_word(adapter, phonemes, 1, &result);
    // May not find word, but function should exist
    (void)found;
    SUCCEED();
}

/**
 * @test wernicke_predict_next_word signature
 * WHAT: Prediction function exists
 * WHY:  Predictive processing capability
 */
TEST_F(WernickeBackwardCompatProcessingTest, PredictNextWordSignature) {
    wernicke_context_t context;
    memset(&context, 0, sizeof(context));
    wernicke_word_pred_t prediction;

    bool result = wernicke_predict_next_word(adapter, &context, &prediction);
    (void)result;
    SUCCEED();
}

//=============================================================================
// BUG FIX REGRESSION TESTS
//=============================================================================

class WernickeRegressionBugFixTest : public ::testing::Test {
protected:
    wernicke_adapter_t* adapter;

    void SetUp() override {
        adapter = wernicke_create(nullptr);
        ASSERT_NE(adapter, nullptr);
    }

    void TearDown() override {
        if (adapter) {
            wernicke_destroy(adapter);
        }
    }
};

/**
 * @test BUG FIX: get_config should not crash on NULL output
 * REGRESSION: Previous version crashed when NULL was passed
 * FIX: Return false for NULL output parameter
 */
TEST_F(WernickeRegressionBugFixTest, GetConfigNullOutput) {
    bool result = wernicke_get_config(adapter, nullptr);
    EXPECT_FALSE(result) << "get_config with NULL output should return false";
}

/**
 * @test BUG FIX: get_stats should not crash on NULL output
 */
TEST_F(WernickeRegressionBugFixTest, GetStatsNullOutput) {
    bool result = wernicke_get_stats(adapter, nullptr);
    EXPECT_FALSE(result) << "get_stats with NULL output should return false";
}

/**
 * @test BUG FIX: Working memory operations on disabled WM
 * REGRESSION: Operations crashed when working memory was disabled
 * FIX: Operations should not crash when WM disabled (behavior may vary)
 */
TEST_F(WernickeRegressionBugFixTest, WorkingMemoryOperationsWhenDisabled) {
    wernicke_destroy(adapter);

    wernicke_config_t config = wernicke_default_config();
    config.enable_working_memory = false;
    adapter = wernicke_create(&config);
    ASSERT_NE(adapter, nullptr);

    phoneme_t phonemes[] = {PHONEME_AH};

    // These should not crash, behavior may vary by implementation
    bool store_result = wernicke_wm_store(adapter, phonemes, 1);
    (void)store_result;  // May succeed or fail depending on implementation

    bool rehearse_result = wernicke_wm_rehearse(adapter);
    (void)rehearse_result;

    phoneme_t buffer[10];
    uint32_t count = 0;
    bool get_result = wernicke_wm_get_contents(adapter, buffer, 10, &count);
    (void)get_result;

    // Key test: system remains stable after operations on disabled WM
    wernicke_status_t status = wernicke_get_status(adapter);
    EXPECT_NE(status, WERNICKE_STATUS_ERROR);
}

/**
 * @test BUG FIX: Lookup word with NULL word string
 * REGRESSION: NULL word string caused crash
 * FIX: Return false for NULL input
 */
TEST_F(WernickeRegressionBugFixTest, LookupNullWord) {
    wernicke_word_t entry;
    bool result = wernicke_lookup_word(adapter, nullptr, &entry);
    EXPECT_FALSE(result) << "lookup_word with NULL string should return false";
}

/**
 * @test BUG FIX: Add NULL word
 * REGRESSION: Adding NULL word caused crash
 * FIX: Return false for NULL input
 */
TEST_F(WernickeRegressionBugFixTest, AddNullWord) {
    bool result = wernicke_add_word(adapter, nullptr);
    EXPECT_FALSE(result) << "add_word with NULL should return false";
}

/**
 * @test BUG FIX: Process empty phoneme array
 * REGRESSION: Zero-count phoneme array caused issues
 * FIX: Early return for zero count
 */
TEST_F(WernickeRegressionBugFixTest, ProcessEmptyPhonemes) {
    phoneme_event_t events[1];  // Non-null but zero count
    bool result = wernicke_process_phonemes(adapter, events, 0);
    // Should handle gracefully (may return true or false, but not crash)
    (void)result;
    SUCCEED();
}

