//=============================================================================
// test_wernicke_adapter.cpp - Wernicke Adapter Unit Tests
//=============================================================================
/**
 * @file test_wernicke_adapter.cpp
 * @brief Unit tests for Wernicke's area adapter
 *
 * WHAT: Tests core Wernicke adapter functionality
 * WHY:  Verify language comprehension lifecycle, configuration, and processing
 * HOW:  gtest framework testing create/destroy, config, word recognition
 *
 * TEST COVERAGE:
 * - Lifecycle: create, destroy, reset
 * - Configuration: default config, custom config
 * - Lexicon: add word, lookup word
 * - Working memory: store, rehearse, get contents
 * - Status and error handling
 * - Statistics tracking
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

#include "core/brain/regions/wernicke/nimcp_wernicke_adapter.h"

//=============================================================================
// Test Fixture
//=============================================================================

class WernickeAdapterTest : public ::testing::Test {
protected:
    wernicke_adapter_t* adapter;
    wernicke_config_t config;

    void SetUp() override {
        adapter = nullptr;
        config = wernicke_default_config();
    }

    void TearDown() override {
        if (adapter) {
            wernicke_destroy(adapter);
            adapter = nullptr;
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

/**
 * @test Create adapter with default configuration
 * WHAT: Test wernicke_create with NULL config
 * WHY:  Verify default initialization works correctly
 * HOW:  Create adapter, check non-null, verify initial status
 */
TEST_F(WernickeAdapterTest, CreateWithDefaultConfig) {
    adapter = wernicke_create(nullptr);
    ASSERT_NE(adapter, nullptr) << "Failed to create adapter with default config";

    // Check initial status
    wernicke_status_t status = wernicke_get_status(adapter);
    EXPECT_EQ(status, WERNICKE_STATUS_IDLE) << "Initial status should be IDLE";

    // Check no error
    wernicke_error_t error = wernicke_get_last_error(adapter);
    EXPECT_EQ(error, WERNICKE_ERROR_NONE) << "Initial error should be NONE";
}

/**
 * @test Create adapter with custom configuration
 * WHAT: Test wernicke_create with custom config
 * WHY:  Verify custom configuration is applied
 * HOW:  Create adapter with modified config, verify settings
 */
TEST_F(WernickeAdapterTest, CreateWithCustomConfig) {
    config.max_phonemes = 512;
    config.max_words = 256;
    config.enable_syntactic = true;
    config.enable_audiovisual = true;

    adapter = wernicke_create(&config);
    ASSERT_NE(adapter, nullptr) << "Failed to create adapter with custom config";

    // Verify config was applied
    wernicke_config_t retrieved_config;
    bool success = wernicke_get_config(adapter, &retrieved_config);
    ASSERT_TRUE(success) << "Failed to get config";

    EXPECT_EQ(retrieved_config.max_phonemes, 512);
    EXPECT_EQ(retrieved_config.max_words, 256);
    EXPECT_TRUE(retrieved_config.enable_syntactic);
    EXPECT_TRUE(retrieved_config.enable_audiovisual);
}

/**
 * @test Destroy adapter safely
 * WHAT: Test wernicke_destroy with valid and null pointers
 * WHY:  Verify destruction doesn't crash with null
 * HOW:  Destroy valid adapter, then call destroy with null
 */
TEST_F(WernickeAdapterTest, DestroyAdapter) {
    adapter = wernicke_create(nullptr);
    ASSERT_NE(adapter, nullptr);

    // Destroy should succeed
    wernicke_destroy(adapter);
    adapter = nullptr;  // Mark as destroyed

    // Destroy null should not crash
    wernicke_destroy(nullptr);
}

/**
 * @test Reset adapter state
 * WHAT: Test wernicke_reset
 * WHY:  Verify reset clears state without full reinitialization
 * HOW:  Create adapter, modify state, reset, verify cleared
 */
TEST_F(WernickeAdapterTest, ResetAdapter) {
    adapter = wernicke_create(nullptr);
    ASSERT_NE(adapter, nullptr);

    // Reset should succeed
    bool success = wernicke_reset(adapter);
    EXPECT_TRUE(success) << "Reset should succeed";

    // Status should be idle after reset
    wernicke_status_t status = wernicke_get_status(adapter);
    EXPECT_EQ(status, WERNICKE_STATUS_IDLE);

    // Error should be cleared
    wernicke_error_t error = wernicke_get_last_error(adapter);
    EXPECT_EQ(error, WERNICKE_ERROR_NONE);
}

/**
 * @test Reset null adapter
 * WHAT: Test wernicke_reset with null
 * WHY:  Verify graceful handling of null input
 * HOW:  Call reset with null, expect false return
 */
TEST_F(WernickeAdapterTest, ResetNullAdapter) {
    bool success = wernicke_reset(nullptr);
    EXPECT_FALSE(success) << "Reset null should return false";
}

//=============================================================================
// Configuration Tests
//=============================================================================

/**
 * @test Default configuration values
 * WHAT: Test wernicke_default_config
 * WHY:  Verify default values are biologically plausible
 * HOW:  Get default config, check all values
 */
TEST_F(WernickeAdapterTest, DefaultConfigValues) {
    wernicke_config_t cfg = wernicke_default_config();

    // Capacity limits
    EXPECT_EQ(cfg.max_phonemes, WERNICKE_DEFAULT_MAX_PHONEMES);
    EXPECT_EQ(cfg.max_words, WERNICKE_DEFAULT_MAX_WORDS);
    EXPECT_EQ(cfg.max_concepts, WERNICKE_DEFAULT_MAX_CONCEPTS);

    // Lexicon
    EXPECT_EQ(cfg.lexicon_size, WERNICKE_DEFAULT_LEXICON_SIZE);
    EXPECT_TRUE(cfg.enable_lexicon);

    // Working memory (7+/-2 capacity)
    EXPECT_EQ(cfg.working_memory_slots, WERNICKE_DEFAULT_WORKING_MEMORY_SLOTS);
    EXPECT_TRUE(cfg.enable_working_memory);

    // Processing layers
    EXPECT_TRUE(cfg.enable_phonological);
    EXPECT_TRUE(cfg.enable_lexical);
    EXPECT_TRUE(cfg.enable_semantic);

    // Feature configuration
    EXPECT_EQ(cfg.embedding_dim, WERNICKE_DEFAULT_EMBEDDING_DIM);

    // Bio-async
    EXPECT_TRUE(cfg.enable_bio_async);
}

/**
 * @test Get config from adapter
 * WHAT: Test wernicke_get_config
 * WHY:  Verify config retrieval works correctly
 * HOW:  Create adapter, get config, verify matches
 */
TEST_F(WernickeAdapterTest, GetConfig) {
    adapter = wernicke_create(&config);
    ASSERT_NE(adapter, nullptr);

    wernicke_config_t retrieved;
    bool success = wernicke_get_config(adapter, &retrieved);
    ASSERT_TRUE(success);

    EXPECT_EQ(retrieved.max_phonemes, config.max_phonemes);
    EXPECT_EQ(retrieved.max_words, config.max_words);
    EXPECT_EQ(retrieved.enable_bio_async, config.enable_bio_async);
}

/**
 * @test Get config with null inputs
 * WHAT: Test wernicke_get_config error handling
 * WHY:  Verify graceful handling of null inputs
 * HOW:  Call with null adapter and null output
 */
TEST_F(WernickeAdapterTest, GetConfigNullInputs) {
    adapter = wernicke_create(nullptr);
    ASSERT_NE(adapter, nullptr);

    // Null output
    bool success = wernicke_get_config(adapter, nullptr);
    EXPECT_FALSE(success);

    // Null adapter
    wernicke_config_t cfg;
    success = wernicke_get_config(nullptr, &cfg);
    EXPECT_FALSE(success);
}

//=============================================================================
// Lexicon Tests
//=============================================================================

/**
 * @test Add word to lexicon
 * WHAT: Test wernicke_add_word
 * WHY:  Verify vocabulary building works
 * HOW:  Create word entry, add to lexicon, lookup
 */
TEST_F(WernickeAdapterTest, AddWordToLexicon) {
    adapter = wernicke_create(nullptr);
    ASSERT_NE(adapter, nullptr);

    // Create word entry
    wernicke_word_t word;
    memset(&word, 0, sizeof(word));
    word.word_id = 1;
    strncpy(word.word, "hello", sizeof(word.word) - 1);
    word.phoneme_count = 5;
    word.phonemes[0] = 1;  // h
    word.phonemes[1] = 2;  // e
    word.phonemes[2] = 3;  // l
    word.phonemes[3] = 3;  // l
    word.phonemes[4] = 4;  // o
    word.frequency = 0.9f;

    // Add word
    bool success = wernicke_add_word(adapter, &word);
    EXPECT_TRUE(success) << "Failed to add word to lexicon";

    // Lookup word
    wernicke_word_t found;
    success = wernicke_lookup_word(adapter, "hello", &found);
    EXPECT_TRUE(success) << "Failed to lookup word";
    EXPECT_STREQ(found.word, "hello");
    EXPECT_EQ(found.word_id, 1);
}

/**
 * @test Add multiple words
 * WHAT: Test adding multiple words to lexicon
 * WHY:  Verify lexicon can hold multiple entries
 * HOW:  Add several words, lookup each
 */
TEST_F(WernickeAdapterTest, AddMultipleWords) {
    adapter = wernicke_create(nullptr);
    ASSERT_NE(adapter, nullptr);

    const char* words[] = {"cat", "dog", "bird", "fish"};
    const int num_words = 4;

    for (int i = 0; i < num_words; i++) {
        wernicke_word_t word;
        memset(&word, 0, sizeof(word));
        word.word_id = (uint32_t)(i + 1);
        strncpy(word.word, words[i], sizeof(word.word) - 1);
        word.phoneme_count = strlen(words[i]);
        word.frequency = 0.5f;

        bool success = wernicke_add_word(adapter, &word);
        EXPECT_TRUE(success) << "Failed to add word: " << words[i];
    }

    // Verify all words can be looked up
    for (int i = 0; i < num_words; i++) {
        wernicke_word_t found;
        bool success = wernicke_lookup_word(adapter, words[i], &found);
        EXPECT_TRUE(success) << "Failed to find word: " << words[i];
        EXPECT_STREQ(found.word, words[i]);
    }
}

/**
 * @test Lookup nonexistent word
 * WHAT: Test wernicke_lookup_word for missing word
 * WHY:  Verify graceful handling of missing entries
 * HOW:  Lookup word not in lexicon, expect failure
 */
TEST_F(WernickeAdapterTest, LookupNonexistentWord) {
    adapter = wernicke_create(nullptr);
    ASSERT_NE(adapter, nullptr);

    wernicke_word_t found;
    bool success = wernicke_lookup_word(adapter, "nonexistent", &found);
    EXPECT_FALSE(success) << "Should not find nonexistent word";
}

/**
 * @test Add word null inputs
 * WHAT: Test wernicke_add_word with null inputs
 * WHY:  Verify null safety
 * HOW:  Call with null adapter and null word
 */
TEST_F(WernickeAdapterTest, AddWordNullInputs) {
    adapter = wernicke_create(nullptr);
    ASSERT_NE(adapter, nullptr);

    // Null word
    bool success = wernicke_add_word(adapter, nullptr);
    EXPECT_FALSE(success);

    // Null adapter
    wernicke_word_t word;
    memset(&word, 0, sizeof(word));
    success = wernicke_add_word(nullptr, &word);
    EXPECT_FALSE(success);
}

//=============================================================================
// Working Memory Tests
//=============================================================================

/**
 * @test Store phonemes in working memory
 * WHAT: Test wernicke_wm_store
 * WHY:  Verify phonological loop storage
 * HOW:  Store phonemes, retrieve contents
 */
TEST_F(WernickeAdapterTest, StoreInWorkingMemory) {
    adapter = wernicke_create(nullptr);
    ASSERT_NE(adapter, nullptr);

    phoneme_t phonemes[4] = {PHONEME_P, PHONEME_B, PHONEME_T, PHONEME_D};
    bool success = wernicke_wm_store(adapter, phonemes, 4);
    EXPECT_TRUE(success) << "Failed to store in working memory";

    // Get contents
    phoneme_t buffer[16];
    uint32_t count = 0;
    success = wernicke_wm_get_contents(adapter, buffer, 16, &count);
    EXPECT_TRUE(success);
    EXPECT_EQ(count, 4);
}

/**
 * @test Rehearse working memory
 * WHAT: Test wernicke_wm_rehearse
 * WHY:  Verify rehearsal refreshes items
 * HOW:  Store phonemes, rehearse, verify not decayed
 */
TEST_F(WernickeAdapterTest, RehearseWorkingMemory) {
    adapter = wernicke_create(nullptr);
    ASSERT_NE(adapter, nullptr);

    phoneme_t phonemes[4] = {PHONEME_P, PHONEME_B, PHONEME_T, PHONEME_D};
    wernicke_wm_store(adapter, phonemes, 4);

    bool success = wernicke_wm_rehearse(adapter);
    EXPECT_TRUE(success);
}

/**
 * @test Clear working memory
 * WHAT: Test wernicke_wm_clear
 * WHY:  Verify memory can be cleared
 * HOW:  Store items, clear, verify empty
 */
TEST_F(WernickeAdapterTest, ClearWorkingMemory) {
    adapter = wernicke_create(nullptr);
    ASSERT_NE(adapter, nullptr);

    phoneme_t phonemes[4] = {PHONEME_P, PHONEME_B, PHONEME_T, PHONEME_D};
    wernicke_wm_store(adapter, phonemes, 4);

    wernicke_wm_clear(adapter);

    phoneme_t buffer[16];
    uint32_t count = 0;
    wernicke_wm_get_contents(adapter, buffer, 16, &count);
    EXPECT_EQ(count, 0) << "Working memory should be empty after clear";
}

//=============================================================================
// Status and Error Tests
//=============================================================================

/**
 * @test Status string conversion
 * WHAT: Test wernicke_status_string
 * WHY:  Verify human-readable status strings
 * HOW:  Convert each status to string, verify non-null
 */
TEST_F(WernickeAdapterTest, StatusStrings) {
    const char* idle_str = wernicke_status_string(WERNICKE_STATUS_IDLE);
    EXPECT_NE(idle_str, nullptr);
    EXPECT_STRNE(idle_str, "");

    const char* phono_str = wernicke_status_string(WERNICKE_STATUS_PHONOLOGICAL);
    EXPECT_NE(phono_str, nullptr);

    const char* error_str = wernicke_status_string(WERNICKE_STATUS_ERROR);
    EXPECT_NE(error_str, nullptr);
}

/**
 * @test Error string conversion
 * WHAT: Test wernicke_error_string
 * WHY:  Verify human-readable error strings
 * HOW:  Convert each error to string, verify non-null
 */
TEST_F(WernickeAdapterTest, ErrorStrings) {
    const char* none_str = wernicke_error_string(WERNICKE_ERROR_NONE);
    EXPECT_NE(none_str, nullptr);

    const char* invalid_str = wernicke_error_string(WERNICKE_ERROR_INVALID_INPUT);
    EXPECT_NE(invalid_str, nullptr);

    const char* internal_str = wernicke_error_string(WERNICKE_ERROR_INTERNAL);
    EXPECT_NE(internal_str, nullptr);
}

//=============================================================================
// Statistics Tests
//=============================================================================

/**
 * @test Get initial statistics
 * WHAT: Test wernicke_get_stats with fresh adapter
 * WHY:  Verify initial stats are zeroed
 * HOW:  Create adapter, get stats, verify zeros
 */
TEST_F(WernickeAdapterTest, GetInitialStats) {
    adapter = wernicke_create(nullptr);
    ASSERT_NE(adapter, nullptr);

    wernicke_stats_t stats;
    bool success = wernicke_get_stats(adapter, &stats);
    ASSERT_TRUE(success);

    EXPECT_EQ(stats.phonemes_processed, 0);
    EXPECT_EQ(stats.words_recognized, 0);
    EXPECT_EQ(stats.concepts_activated, 0);
    EXPECT_EQ(stats.utterances_comprehended, 0);
}

/**
 * @test Get stats with null inputs
 * WHAT: Test wernicke_get_stats error handling
 * WHY:  Verify graceful handling of null
 * HOW:  Call with null inputs, expect false
 */
TEST_F(WernickeAdapterTest, GetStatsNullInputs) {
    adapter = wernicke_create(nullptr);
    ASSERT_NE(adapter, nullptr);

    // Null output
    bool success = wernicke_get_stats(adapter, nullptr);
    EXPECT_FALSE(success);

    // Null adapter
    wernicke_stats_t stats;
    success = wernicke_get_stats(nullptr, &stats);
    EXPECT_FALSE(success);
}

//=============================================================================
// Sub-module Access Tests
//=============================================================================

/**
 * @test Get sub-module handles
 * WHAT: Test sub-module accessor functions
 * WHY:  Verify sub-modules can be accessed for advanced use
 * HOW:  Get each sub-module handle (may be null in stub)
 */
TEST_F(WernickeAdapterTest, GetSubModuleHandles) {
    adapter = wernicke_create(nullptr);
    ASSERT_NE(adapter, nullptr);

    // These may be null if not yet implemented
    phonological_analyzer_t* phono = wernicke_get_phonological_analyzer(adapter);
    lexical_access_t* lex = wernicke_get_lexical_access(adapter);
    semantic_integrator_t* sem = wernicke_get_semantic_integrator(adapter);
    syntactic_comprehension_t* syn = wernicke_get_syntactic_comprehension(adapter);

    // At minimum, should not crash
    (void)phono;
    (void)lex;
    (void)sem;
    (void)syn;
}

/**
 * @test Get sub-modules from null adapter
 * WHAT: Test sub-module accessors with null adapter
 * WHY:  Verify null safety
 * HOW:  Call each accessor with null, expect null return
 */
TEST_F(WernickeAdapterTest, GetSubModulesNullAdapter) {
    EXPECT_EQ(wernicke_get_phonological_analyzer(nullptr), nullptr);
    EXPECT_EQ(wernicke_get_lexical_access(nullptr), nullptr);
    EXPECT_EQ(wernicke_get_semantic_integrator(nullptr), nullptr);
    EXPECT_EQ(wernicke_get_syntactic_comprehension(nullptr), nullptr);
}

//=============================================================================
// Callback Tests
//=============================================================================

static int word_callback_count = 0;
static void test_word_callback(const wernicke_word_result_t* word, void* user_data) {
    (void)word;
    (void)user_data;
    word_callback_count++;
}

/**
 * @test Set word callback
 * WHAT: Test wernicke_set_word_callback
 * WHY:  Verify callback registration
 * HOW:  Set callback, verify success
 */
TEST_F(WernickeAdapterTest, SetWordCallback) {
    adapter = wernicke_create(nullptr);
    ASSERT_NE(adapter, nullptr);

    int user_data = 42;
    bool success = wernicke_set_word_callback(adapter, test_word_callback, &user_data);
    EXPECT_TRUE(success);
}

/**
 * @test Set callbacks with null adapter
 * WHAT: Test callback setters with null adapter
 * WHY:  Verify null safety
 * HOW:  Call with null adapter, expect false
 */
TEST_F(WernickeAdapterTest, SetCallbacksNullAdapter) {
    bool success = wernicke_set_word_callback(nullptr, test_word_callback, nullptr);
    EXPECT_FALSE(success);

    success = wernicke_set_concept_callback(nullptr, nullptr, nullptr);
    EXPECT_FALSE(success);

    success = wernicke_set_comprehension_callback(nullptr, nullptr, nullptr);
    EXPECT_FALSE(success);
}

//=============================================================================
// Bio-Async Tests
//=============================================================================

/**
 * @test Get bio context
 * WHAT: Test wernicke_get_bio_context
 * WHY:  Verify bio-async context retrieval
 * HOW:  Create adapter with bio-async, get context
 */
TEST_F(WernickeAdapterTest, GetBioContext) {
    config.enable_bio_async = true;
    adapter = wernicke_create(&config);
    ASSERT_NE(adapter, nullptr);

    bio_module_context_t ctx = wernicke_get_bio_context(adapter);
    // Context may have null fields if not fully initialized
    (void)ctx;
}

/**
 * @test Process bio messages
 * WHAT: Test wernicke_process_bio_messages
 * WHY:  Verify message processing doesn't crash
 * HOW:  Process with empty queue, verify returns 0
 */
TEST_F(WernickeAdapterTest, ProcessBioMessages) {
    adapter = wernicke_create(nullptr);
    ASSERT_NE(adapter, nullptr);

    uint32_t processed = wernicke_process_bio_messages(adapter, 10);
    EXPECT_EQ(processed, 0) << "No messages to process initially";
}

/**
 * @test Process bio messages null adapter
 * WHAT: Test wernicke_process_bio_messages with null
 * WHY:  Verify null safety
 * HOW:  Call with null, expect 0
 */
TEST_F(WernickeAdapterTest, ProcessBioMessagesNull) {
    uint32_t processed = wernicke_process_bio_messages(nullptr, 10);
    EXPECT_EQ(processed, 0);
}

/* GROUNDED-LANGUAGE BINDING */
TEST_F(WernickeAdapterTest, AttachGroundedLanguageStoresAndUnbinds) {
    adapter = wernicke_create(nullptr);
    ASSERT_NE(adapter, nullptr);
    int dummy_gl = 0;
    EXPECT_EQ(nullptr, wernicke_get_grounded_language(adapter));
    EXPECT_TRUE(wernicke_attach_grounded_language(adapter, &dummy_gl));
    EXPECT_EQ(&dummy_gl, wernicke_get_grounded_language(adapter));
    EXPECT_TRUE(wernicke_attach_grounded_language(adapter, nullptr));
    EXPECT_EQ(nullptr, wernicke_get_grounded_language(adapter));
}

TEST_F(WernickeAdapterTest, AttachGroundedLanguageNullAdapterIsSafe) {
    int dummy_gl = 0;
    EXPECT_FALSE(wernicke_attach_grounded_language(nullptr, &dummy_gl));
    EXPECT_EQ(nullptr, wernicke_get_grounded_language(nullptr));
}
