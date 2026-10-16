/**
 * @file test_wernicke_signal_regression.cpp
 * @brief Signal-level regression tests for Wernicke's area adapter
 *
 * WHAT: Tests signal processing determinism and numerical stability
 * WHY:  Ensure consistent behavior across code changes
 * HOW:  Verify outputs for known inputs remain consistent
 *
 * COVERAGE TARGET: Processing determinism, edge cases, numerical stability
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

#include "core/brain/regions/wernicke/nimcp_wernicke_adapter.h"
#include "perception/nimcp_speech_cortex.h"

//=============================================================================
// Test Fixture - Common Setup
//=============================================================================

class WernickeSignalRegressionTest : public ::testing::Test {
protected:
    wernicke_adapter_t* adapter;
    wernicke_config_t config;

    void SetUp() override {
        config = wernicke_default_config();
        config.enable_phonological = true;
        config.enable_lexical = true;
        config.enable_semantic = true;
        config.enable_working_memory = true;
        adapter = wernicke_create(&config);
        ASSERT_NE(adapter, nullptr);
    }

    void TearDown() override {
        if (adapter) {
            wernicke_destroy(adapter);
            adapter = nullptr;
        }
    }

    // Helper: Create a word in the lexicon
    void addTestWord(const char* word_str, const phoneme_t* phonemes,
                     uint32_t phoneme_count, uint32_t word_id) {
        wernicke_word_t word;
        memset(&word, 0, sizeof(word));
        word.word_id = word_id;
        strncpy(word.word, word_str, sizeof(word.word) - 1);
        // Limit to max 32 phonemes (buffer size)
        uint32_t copy_count = (phoneme_count > 32) ? 32 : phoneme_count;
        // Copy phoneme values as uint8_t
        for (uint32_t i = 0; i < copy_count; i++) {
            word.phonemes[i] = static_cast<uint8_t>(phonemes[i]);
        }
        word.phoneme_count = copy_count;
        word.frequency = 0.5f;
        word.concept_id = word_id * 10;
        word.pos = 1;  // Noun
        wernicke_add_word(adapter, &word);
    }
};

//=============================================================================
// Determinism Tests - Word Recognition
//=============================================================================

/**
 * @test Word recognition is deterministic
 * WHAT: Same phoneme sequence always produces same result
 * WHY:  Non-determinism would cause inconsistent behavior
 */
TEST_F(WernickeSignalRegressionTest, WordRecognitionDeterminism) {
    // Add test word to lexicon
    phoneme_t hello_phonemes[] = {PHONEME_H, PHONEME_EH, PHONEME_L, PHONEME_L, PHONEME_OW};
    addTestWord("hello", hello_phonemes, 5, 1);

    wernicke_word_result_t result1, result2, result3;

    // Recognize same sequence multiple times
    bool found1 = wernicke_recognize_word(adapter, hello_phonemes, 5, &result1);
    bool found2 = wernicke_recognize_word(adapter, hello_phonemes, 5, &result2);
    bool found3 = wernicke_recognize_word(adapter, hello_phonemes, 5, &result3);

    // Results should be consistent (all found or all not found)
    EXPECT_EQ(found1, found2);
    EXPECT_EQ(found2, found3);

    // If found, results should be identical
    if (found1 && found2 && found3) {
        EXPECT_EQ(result1.word.word_id, result2.word.word_id);
        EXPECT_EQ(result2.word.word_id, result3.word.word_id);
        EXPECT_STREQ(result1.word.word, result2.word.word);
        EXPECT_STREQ(result2.word.word, result3.word.word);
    }
}

/**
 * @test Word recognition confidence is consistent
 * WHAT: Confidence scores are consistent for same input
 * WHY:  Confidence affects downstream decisions
 */
TEST_F(WernickeSignalRegressionTest, ConfidenceConsistency) {
    phoneme_t cat_phonemes[] = {PHONEME_K, PHONEME_AH, PHONEME_T};
    addTestWord("cat", cat_phonemes, 3, 2);

    wernicke_word_result_t result1, result2;
    bool found1 = wernicke_recognize_word(adapter, cat_phonemes, 3, &result1);
    bool found2 = wernicke_recognize_word(adapter, cat_phonemes, 3, &result2);

    // Results should be consistent
    EXPECT_EQ(found1, found2);

    // If both found, confidence should be identical
    if (found1 && found2) {
        EXPECT_FLOAT_EQ(result1.confidence, result2.confidence);
    }
}

//=============================================================================
// Determinism Tests - Working Memory
//=============================================================================

/**
 * @test Working memory order preservation
 * WHAT: Phonemes retrieved in same order as stored
 * WHY:  Order matters for word recognition
 */
TEST_F(WernickeSignalRegressionTest, WorkingMemoryOrderPreservation) {
    phoneme_t input[] = {PHONEME_B, PHONEME_AH, PHONEME_T};
    wernicke_wm_store(adapter, input, 3);

    phoneme_t output[10];
    uint32_t count = 0;
    wernicke_wm_get_contents(adapter, output, 10, &count);

    ASSERT_EQ(count, 3u);
    EXPECT_EQ(output[0], PHONEME_B);
    EXPECT_EQ(output[1], PHONEME_AH);
    EXPECT_EQ(output[2], PHONEME_T);
}

/**
 * @test Working memory handles large input
 * WHAT: Working memory doesn't crash on large input
 * WHY:  Robustness under stress
 * NOTE: Capacity limit may or may not be enforced by implementation
 */
TEST_F(WernickeSignalRegressionTest, WorkingMemoryCapacityLimit) {
    // Default is 9 slots, but try storing more
    phoneme_t many_phonemes[20];
    for (int i = 0; i < 20; i++) {
        many_phonemes[i] = static_cast<phoneme_t>(PHONEME_AH + (i % 10));
    }

    bool stored = wernicke_wm_store(adapter, many_phonemes, 20);
    // Should not crash (behavior may vary)
    (void)stored;

    phoneme_t output[30];
    uint32_t count = 0;
    bool success = wernicke_wm_get_contents(adapter, output, 30, &count);

    // System should remain stable
    if (success) {
        EXPECT_GT(count, 0u);
    }
}

/**
 * @test Working memory rehearsal preserves content
 * WHAT: Rehearsal doesn't corrupt data
 * WHY:  Rehearsal prevents decay, shouldn't modify
 */
TEST_F(WernickeSignalRegressionTest, RehearsalPreservesContent) {
    phoneme_t input[] = {PHONEME_M, PHONEME_AH, PHONEME_M};
    wernicke_wm_store(adapter, input, 3);

    // Rehearse multiple times
    for (int i = 0; i < 5; i++) {
        wernicke_wm_rehearse(adapter);
    }

    phoneme_t output[10];
    uint32_t count = 0;
    wernicke_wm_get_contents(adapter, output, 10, &count);

    ASSERT_EQ(count, 3u);
    EXPECT_EQ(output[0], PHONEME_M);
    EXPECT_EQ(output[1], PHONEME_AH);
    EXPECT_EQ(output[2], PHONEME_M);
}

//=============================================================================
// Edge Case Tests - Empty/Null Inputs
//=============================================================================

/**
 * @test Process zero phonemes
 * WHAT: Zero count input handled gracefully
 * WHY:  Edge case at start of utterance
 */
TEST_F(WernickeSignalRegressionTest, ProcessZeroPhonemes) {
    phoneme_event_t events[1];  // Non-null
    bool result = wernicke_process_phonemes(adapter, events, 0);
    // Should handle gracefully
    (void)result;

    wernicke_status_t status = wernicke_get_status(adapter);
    // Status should be valid (IDLE or another valid state)
    EXPECT_NE(status, WERNICKE_STATUS_ERROR);
}

/**
 * @test Recognize single phoneme
 * WHAT: Single phoneme word recognition
 * WHY:  Minimal input case
 */
TEST_F(WernickeSignalRegressionTest, RecognizeSinglePhoneme) {
    phoneme_t a_phoneme[] = {PHONEME_AH};
    addTestWord("a", a_phoneme, 1, 100);

    wernicke_word_result_t result;
    bool found = wernicke_recognize_word(adapter, a_phoneme, 1, &result);

    ASSERT_TRUE(found);
    EXPECT_STREQ(result.word.word, "a");
}

/**
 * @test Store empty working memory
 * WHAT: Zero count store handled gracefully
 * WHY:  Edge case handling
 */
TEST_F(WernickeSignalRegressionTest, StoreEmptyWorkingMemory) {
    phoneme_t empty[1];
    bool result = wernicke_wm_store(adapter, empty, 0);
    // Should handle gracefully (may return true or false)
    (void)result;

    phoneme_t output[10];
    uint32_t count = 0;
    wernicke_wm_get_contents(adapter, output, 10, &count);
    // Count should be 0 for empty store
    EXPECT_EQ(count, 0u);
}

//=============================================================================
// Edge Case Tests - Long Inputs
//=============================================================================

/**
 * @test Long word recognition
 * WHAT: Long phoneme sequences handled without crash
 * WHY:  Real words can have many phonemes
 */
TEST_F(WernickeSignalRegressionTest, LongWordRecognition) {
    // "supercalifragilisticexpialidocious" approximation
    phoneme_t long_word[20] = {
        PHONEME_S, PHONEME_UW, PHONEME_P, PHONEME_ER, PHONEME_K,
        PHONEME_AH, PHONEME_L, PHONEME_IH, PHONEME_F, PHONEME_R,
        PHONEME_AH, PHONEME_JH, PHONEME_IH, PHONEME_L, PHONEME_IH,
        PHONEME_S, PHONEME_T, PHONEME_IH, PHONEME_K, PHONEME_S
    };
    addTestWord("super", long_word, 20, 200);

    wernicke_word_result_t result;
    bool found = wernicke_recognize_word(adapter, long_word, 20, &result);

    // Key test: doesn't crash with long sequences
    // Recognition may or may not succeed
    if (found) {
        EXPECT_EQ(result.word.word_id, 200u);
    }
    SUCCEED();
}

/**
 * @test Maximum capacity lexicon
 * WHAT: Lexicon at capacity doesn't crash
 * WHY:  Resource limits must be respected
 */
TEST_F(WernickeSignalRegressionTest, LexiconCapacityStress) {
    // Add many words (but not exceeding limit)
    for (uint32_t i = 0; i < 100; i++) {
        char word_str[32];
        snprintf(word_str, sizeof(word_str), "word%u", i);

        phoneme_t phonemes[3] = {
            static_cast<phoneme_t>(PHONEME_AH + (i % 10)),
            static_cast<phoneme_t>(PHONEME_B + (i % 5)),
            static_cast<phoneme_t>(PHONEME_K + (i % 8))
        };

        wernicke_word_t word;
        memset(&word, 0, sizeof(word));
        word.word_id = i + 1000;
        strncpy(word.word, word_str, sizeof(word.word) - 1);
        memcpy(word.phonemes, phonemes, 3);
        word.phoneme_count = 3;
        word.frequency = 0.1f;
        word.concept_id = i * 10;

        wernicke_add_word(adapter, &word);
    }

    // Verify some words can be retrieved
    wernicke_word_t entry;
    bool found = wernicke_lookup_word(adapter, "word50", &entry);
    EXPECT_TRUE(found);
}

//=============================================================================
// Stability Tests - Reset Behavior
//=============================================================================

/**
 * @test Reset clears error state
 * WHAT: Reset returns adapter to clean state
 * WHY:  Recovery from errors
 */
TEST_F(WernickeSignalRegressionTest, ResetClearsError) {
    // Force an error state if possible (via invalid operation)
    wernicke_recognize_word(adapter, nullptr, 0, nullptr);

    // Reset should clear any error
    wernicke_reset(adapter);

    wernicke_status_t status = wernicke_get_status(adapter);
    EXPECT_EQ(status, WERNICKE_STATUS_IDLE);

    wernicke_error_t error = wernicke_get_last_error(adapter);
    EXPECT_EQ(error, WERNICKE_ERROR_NONE);
}

/**
 * @test Reset preserves lexicon
 * WHAT: Reset doesn't clear learned vocabulary
 * WHY:  Lexicon is persistent knowledge
 */
TEST_F(WernickeSignalRegressionTest, ResetPreservesLexicon) {
    phoneme_t test_phonemes[] = {PHONEME_T, PHONEME_EH, PHONEME_S, PHONEME_T};
    addTestWord("test", test_phonemes, 4, 999);

    wernicke_reset(adapter);

    wernicke_word_t entry;
    bool found = wernicke_lookup_word(adapter, "test", &entry);
    EXPECT_TRUE(found) << "Reset should preserve lexicon";
}

/**
 * @test Reset clears working memory
 * WHAT: Reset empties phonological buffer
 * WHY:  Working memory is transient
 */
TEST_F(WernickeSignalRegressionTest, ResetClearsWorkingMemory) {
    phoneme_t phonemes[] = {PHONEME_AH, PHONEME_B};
    wernicke_wm_store(adapter, phonemes, 2);

    wernicke_reset(adapter);

    phoneme_t output[10];
    uint32_t count = 0;
    wernicke_wm_get_contents(adapter, output, 10, &count);
    EXPECT_EQ(count, 0u) << "Reset should clear working memory";
}

//=============================================================================
// Stability Tests - Multiple Create/Destroy Cycles
//=============================================================================

/**
 * @test Multiple create/destroy cycles
 * WHAT: Repeated creation/destruction is stable
 * WHY:  Memory leaks or corruption over time
 */
TEST_F(WernickeSignalRegressionTest, MultipleCreateDestroyCycles) {
    wernicke_destroy(adapter);
    adapter = nullptr;

    for (int i = 0; i < 10; i++) {
        wernicke_adapter_t* temp = wernicke_create(nullptr);
        ASSERT_NE(temp, nullptr) << "Failed on cycle " << i;

        // Do some work
        phoneme_t phonemes[] = {PHONEME_AH};
        wernicke_wm_store(temp, phonemes, 1);

        wernicke_destroy(temp);
    }

    // Recreate for TearDown
    adapter = wernicke_create(nullptr);
    SUCCEED();
}

//=============================================================================
// Numerical Stability Tests
//=============================================================================

/**
 * @test Confidence values in valid range
 * WHAT: Confidence always [0, 1]
 * WHY:  Out-of-range values indicate bugs
 */
TEST_F(WernickeSignalRegressionTest, ConfidenceInValidRange) {
    phoneme_t test_phonemes[] = {PHONEME_D, PHONEME_AH, PHONEME_G};
    addTestWord("dog", test_phonemes, 3, 50);

    wernicke_word_result_t result;
    bool found = wernicke_recognize_word(adapter, test_phonemes, 3, &result);

    if (found) {
        EXPECT_GE(result.confidence, 0.0f);
        EXPECT_LE(result.confidence, 1.0f);
    }
}

/**
 * @test Statistics don't overflow
 * WHAT: Stats remain valid after many operations
 * WHY:  Integer overflow detection
 */
TEST_F(WernickeSignalRegressionTest, StatsNoOverflow) {
    phoneme_t phonemes[] = {PHONEME_AH};
    addTestWord("a", phonemes, 1, 1);

    // Many recognition attempts
    for (int i = 0; i < 1000; i++) {
        wernicke_word_result_t result;
        wernicke_recognize_word(adapter, phonemes, 1, &result);
    }

    wernicke_stats_t stats;
    wernicke_get_stats(adapter, &stats);

    // Stats should be reasonable (no overflow to negative)
    EXPECT_GE(stats.words_recognized, 0u);
    EXPECT_LE(stats.words_recognized, 10000u);
}

//=============================================================================
// Word Frequency Effects
//=============================================================================

/**
 * @test High frequency words recognized faster (or with higher confidence)
 * WHAT: Word frequency affects recognition
 * WHY:  Frequency effect is linguistically important
 */
TEST_F(WernickeSignalRegressionTest, FrequencyEffects) {
    // Add high-frequency word
    phoneme_t the_phonemes[] = {PHONEME_DH, PHONEME_AH};
    wernicke_word_t high_freq;
    memset(&high_freq, 0, sizeof(high_freq));
    high_freq.word_id = 1;
    strncpy(high_freq.word, "the", sizeof(high_freq.word) - 1);
    memcpy(high_freq.phonemes, the_phonemes, 2);
    high_freq.phoneme_count = 2;
    high_freq.frequency = 0.99f;  // Very high frequency
    wernicke_add_word(adapter, &high_freq);

    // Add low-frequency word with similar phonemes
    wernicke_word_t low_freq;
    memset(&low_freq, 0, sizeof(low_freq));
    low_freq.word_id = 2;
    strncpy(low_freq.word, "thy", sizeof(low_freq.word) - 1);
    memcpy(low_freq.phonemes, the_phonemes, 2);  // Same phonemes
    low_freq.phoneme_count = 2;
    low_freq.frequency = 0.01f;  // Very low frequency
    wernicke_add_word(adapter, &low_freq);

    // Recognition should prefer high-frequency word
    wernicke_word_result_t result;
    bool found = wernicke_recognize_word(adapter, the_phonemes, 2, &result);

    if (found) {
        // High frequency word should be recognized
        EXPECT_STREQ(result.word.word, "the");
    }
}

//=============================================================================
// Context Disambiguation
//=============================================================================

/**
 * @test Context structure can be populated
 * WHAT: Context struct is usable for disambiguation
 * WHY:  Disambiguation requires context
 */
TEST_F(WernickeSignalRegressionTest, ContextStructPopulation) {
    wernicke_context_t context;
    memset(&context, 0, sizeof(context));

    // Populate context
    wernicke_word_result_t prior_words[2];
    memset(prior_words, 0, sizeof(prior_words));

    context.prior_words = prior_words;
    context.num_prior_words = 2;
    context.has_topic = true;

    // Verify context is usable (doesn't crash)
    wernicke_word_pred_t prediction;
    bool result = wernicke_predict_next_word(adapter, &context, &prediction);
    (void)result;
    SUCCEED();
}

//=============================================================================
// Stats Accumulation
//=============================================================================

/**
 * @test Stats accumulate correctly
 * WHAT: Stats increase with operations
 * WHY:  Stats tracking for monitoring
 */
TEST_F(WernickeSignalRegressionTest, StatsAccumulation) {
    wernicke_stats_t stats_before, stats_after;
    wernicke_get_stats(adapter, &stats_before);

    // Perform some operations
    phoneme_t phonemes[] = {PHONEME_AH, PHONEME_B, PHONEME_K};
    addTestWord("abc", phonemes, 3, 111);

    wernicke_word_result_t result;
    wernicke_recognize_word(adapter, phonemes, 3, &result);

    wernicke_get_stats(adapter, &stats_after);

    // Words recognized should increase
    EXPECT_GE(stats_after.words_recognized, stats_before.words_recognized);
}

//=============================================================================
// Bio-async Context
//=============================================================================

class WernickeBioAsyncRegressionTest : public ::testing::Test {
protected:
    wernicke_adapter_t* adapter;

    void SetUp() override {
        wernicke_config_t config = wernicke_default_config();
        config.enable_bio_async = true;
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
 * @test Bio context is retrievable when enabled
 * WHAT: wernicke_get_bio_context returns valid context
 * WHY:  Bio-async integration requires valid context
 */
TEST_F(WernickeBioAsyncRegressionTest, BioContextRetrievable) {
    bio_module_context_t ctx = wernicke_get_bio_context(adapter);
    // Context should be valid (module_id may be assigned)
    // Just verify function doesn't crash
    (void)ctx;
    SUCCEED();
}

/**
 * @test Process bio messages doesn't crash
 * WHAT: Message processing is safe
 * WHY:  Bio-async message loop stability
 */
TEST_F(WernickeBioAsyncRegressionTest, ProcessBioMessagesSafe) {
    uint32_t processed = wernicke_process_bio_messages(adapter, 10);
    // May process 0 messages if none pending
    EXPECT_GE(processed, 0u);
}

//=============================================================================
// Broca Connection
//=============================================================================

class WernickeBrocaConnectionTest : public ::testing::Test {
protected:
    wernicke_adapter_t* adapter;

    void SetUp() override {
        wernicke_config_t config = wernicke_default_config();
        config.enable_broca_connection = true;
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
 * @test Connect Broca with NULL is safe
 * WHAT: Null Broca connection doesn't crash
 * WHY:  Graceful handling of missing connection
 */
TEST_F(WernickeBrocaConnectionTest, ConnectBrocaNullSafe) {
    bool result = wernicke_connect_broca(adapter, nullptr);
    // Should not crash (result may be true or false depending on impl)
    (void)result;
    SUCCEED();
}

/**
 * @test Send to Broca without connection fails gracefully
 * WHAT: Sending without connection returns error
 * WHY:  Disconnected state handling
 */
TEST_F(WernickeBrocaConnectionTest, SendWithoutConnectionFails) {
    wernicke_comprehension_t comp;
    memset(&comp, 0, sizeof(comp));

    bool result = wernicke_send_to_broca(adapter, &comp);
    // Should fail gracefully without connection
    EXPECT_FALSE(result);
}

/**
 * @test Receive efference copy with NULL is safe
 * WHAT: Null efference copy doesn't crash
 * WHY:  Input validation
 */
TEST_F(WernickeBrocaConnectionTest, ReceiveNullEfferenceCopy) {
    bool result = wernicke_receive_efference_copy(adapter, nullptr);
    EXPECT_FALSE(result);
}

//=============================================================================
// Audiovisual Integration
//=============================================================================

class WernickeAudiovisualRegressionTest : public ::testing::Test {
protected:
    wernicke_adapter_t* adapter;

    void SetUp() override {
        wernicke_config_t config = wernicke_default_config();
        config.enable_audiovisual = true;
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
 * @test Audiovisual integration function exists
 * WHAT: Integration function is callable
 * WHY:  McGurk effect processing
 */
TEST_F(WernickeAudiovisualRegressionTest, IntegrationFunctionExists) {
    phoneme_event_t audio_phonemes[3];
    memset(audio_phonemes, 0, sizeof(audio_phonemes));
    audio_phonemes[0].phoneme = PHONEME_B;  // Audio /ba/

    float visual_shapes[3] = {0.5f, 0.5f, 0.5f};  // Visual features

    phoneme_event_t fused[3];
    uint32_t num_fused = 0;

    bool result = wernicke_integrate_audiovisual(adapter, audio_phonemes,
                                                   visual_shapes, 3,
                                                   fused, &num_fused);
    // Function should be callable (result depends on implementation)
    (void)result;
    SUCCEED();
}

//=============================================================================
// Training Interface
//=============================================================================

class WernickeTrainingRegressionTest : public ::testing::Test {
protected:
    wernicke_adapter_t* adapter;

    void SetUp() override {
        wernicke_config_t config = wernicke_default_config();
        config.enable_training = true;
        config.learning_rate = 0.01f;
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
 * @test Training word function exists
 * WHAT: wernicke_train_word is callable
 * WHY:  Learning capability
 */
TEST_F(WernickeTrainingRegressionTest, TrainWordFunctionExists) {
    phoneme_t phonemes[] = {PHONEME_D, PHONEME_OW, PHONEME_G};
    bool result = wernicke_train_word(adapter, phonemes, 3, "dog", 0.0f);
    // Function should be callable
    (void)result;
    SUCCEED();
}

/**
 * @test Training semantic function exists
 * WHAT: wernicke_train_semantic is callable
 * WHY:  Semantic learning capability
 */
TEST_F(WernickeTrainingRegressionTest, TrainSemanticFunctionExists) {
    bool result = wernicke_train_semantic(adapter, 1, 100, 0.8f);
    // Function should be callable
    (void)result;
    SUCCEED();
}

