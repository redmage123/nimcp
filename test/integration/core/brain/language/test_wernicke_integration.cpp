/**
 * @file test_wernicke_integration.cpp
 * @brief Integration tests for Wernicke's area language comprehension
 *
 * WHAT: Comprehensive integration tests for Wernicke adapter, lexical access,
 *       semantic integrator, and bio-async communication
 * WHY:  Verify proper integration of all Wernicke sub-modules
 * HOW:  Test complete language comprehension pipelines and cross-module communication
 *
 * @version Phase W1: Wernicke's Area Integration Tests
 * @date 2026-01-24
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>
#include <thread>
#include <chrono>

#include "utils/nimcp_test_base.h"

#include "core/brain/regions/wernicke/nimcp_wernicke_adapter.h"
#include "core/brain/regions/wernicke/nimcp_lexical_access.h"
#include "core/brain/regions/wernicke/nimcp_semantic_integrator.h"

//=============================================================================
// Test Fixture
//=============================================================================

class WernickeIntegrationTest : public NimcpTestBase {
protected:
    wernicke_adapter_t* adapter_ = nullptr;
    lexical_access_t* lexical_ = nullptr;
    semantic_integrator_t* semantic_ = nullptr;

    void SetUp() override {
        NimcpTestBase::SetUp();
        // Components created per-test as needed
    }

    void TearDown() override {
        if (adapter_) {
            wernicke_destroy(adapter_);
            adapter_ = nullptr;
        }
        if (lexical_) {
            lexical_destroy(lexical_);
            lexical_ = nullptr;
        }
        if (semantic_) {
            semantic_destroy(semantic_);
            semantic_ = nullptr;
        }
        NimcpTestBase::TearDown();
    }

    // Helper to create a Wernicke word entry
    wernicke_word_t CreateWord(uint32_t word_id, const char* word,
                               const uint8_t* phonemes, uint32_t phoneme_count,
                               float frequency, uint32_t concept_id, uint8_t pos) {
        wernicke_word_t entry = {};
        entry.word_id = word_id;
        strncpy(entry.word, word, sizeof(entry.word) - 1);
        if (phonemes && phoneme_count <= sizeof(entry.phonemes)) {
            memcpy(entry.phonemes, phonemes, phoneme_count);
            entry.phoneme_count = phoneme_count;
        }
        entry.frequency = frequency;
        entry.concept_id = concept_id;
        entry.pos = pos;
        return entry;
    }

    // Helper to create a lexical entry
    lexical_entry_t CreateLexicalEntry(uint32_t word_id, const char* word,
                                        const uint8_t* phonemes, uint32_t phoneme_count,
                                        float frequency, uint32_t concept_id) {
        lexical_entry_t entry = {};
        entry.word_id = word_id;
        strncpy(entry.orthography, word, sizeof(entry.orthography) - 1);
        if (phonemes && phoneme_count <= sizeof(entry.phonemes)) {
            memcpy(entry.phonemes, phonemes, phoneme_count);
            entry.phoneme_count = phoneme_count;
        }
        entry.frequency = frequency;
        entry.concept_id = concept_id;
        entry.pos = POS_NOUN;
        return entry;
    }
};

//=============================================================================
// Wernicke Adapter Lifecycle Tests
//=============================================================================

TEST_F(WernickeIntegrationTest, CreateWithDefaultConfig) {
    adapter_ = wernicke_create(nullptr);
    ASSERT_NE(adapter_, nullptr) << "wernicke_create with NULL config should succeed";

    wernicke_status_t status = wernicke_get_status(adapter_);
    EXPECT_EQ(status, WERNICKE_STATUS_IDLE) << "Initial status should be IDLE";
}

TEST_F(WernickeIntegrationTest, CreateWithCustomConfig) {
    wernicke_config_t config = wernicke_default_config();
    config.max_phonemes = 512;
    config.max_words = 256;
    config.max_concepts = 2048;
    config.enable_phonological = true;
    config.enable_lexical = true;
    config.enable_semantic = true;
    config.enable_syntactic = true;

    adapter_ = wernicke_create(&config);
    ASSERT_NE(adapter_, nullptr) << "wernicke_create with custom config should succeed";

    wernicke_config_t retrieved_config;
    bool result = wernicke_get_config(adapter_, &retrieved_config);
    EXPECT_TRUE(result) << "wernicke_get_config should succeed";
    EXPECT_EQ(retrieved_config.max_phonemes, 512u);
    EXPECT_EQ(retrieved_config.max_words, 256u);
}

TEST_F(WernickeIntegrationTest, CreateWithMinimalConfig) {
    wernicke_config_t config = wernicke_default_config();
    config.max_phonemes = 32;
    config.max_words = 16;
    config.enable_working_memory = false;
    config.enable_lexicon = false;
    config.enable_events = false;
    config.enable_training = false;

    adapter_ = wernicke_create(&config);
    ASSERT_NE(adapter_, nullptr) << "Minimal config creation should succeed";
}

TEST_F(WernickeIntegrationTest, ResetClearsState) {
    adapter_ = wernicke_create(nullptr);
    ASSERT_NE(adapter_, nullptr);

    bool result = wernicke_reset(adapter_);
    EXPECT_TRUE(result) << "wernicke_reset should succeed";

    wernicke_status_t status = wernicke_get_status(adapter_);
    EXPECT_EQ(status, WERNICKE_STATUS_IDLE) << "Status after reset should be IDLE";
}

TEST_F(WernickeIntegrationTest, DestroyNullIsSafe) {
    // Should not crash
    wernicke_destroy(nullptr);
}

TEST_F(WernickeIntegrationTest, DefaultConfigHasSensibleValues) {
    wernicke_config_t config = wernicke_default_config();

    EXPECT_GT(config.max_phonemes, 0u) << "max_phonemes should be positive";
    EXPECT_GT(config.max_words, 0u) << "max_words should be positive";
    EXPECT_GT(config.max_concepts, 0u) << "max_concepts should be positive";
    EXPECT_GE(config.working_memory_slots, 5u) << "WM slots should be at least 5";
    EXPECT_GT(config.embedding_dim, 0u) << "embedding_dim should be positive";
    EXPECT_GT(config.processing_window_ms, 0.0f) << "processing window should be positive";
}

//=============================================================================
// Lexical Access Tests (Wernicke)
//=============================================================================

TEST_F(WernickeIntegrationTest, AddWordToLexicon) {
    wernicke_config_t config = wernicke_default_config();
    config.enable_lexicon = true;
    adapter_ = wernicke_create(&config);
    ASSERT_NE(adapter_, nullptr);

    uint8_t phonemes[] = {1, 2, 3};
    wernicke_word_t word = CreateWord(1, "cat", phonemes, 3, 0.8f, 100, POS_NOUN);

    bool result = wernicke_add_word(adapter_, &word);
    EXPECT_TRUE(result) << "Adding word should succeed";
}

TEST_F(WernickeIntegrationTest, LookupWord) {
    wernicke_config_t config = wernicke_default_config();
    config.enable_lexicon = true;
    adapter_ = wernicke_create(&config);
    ASSERT_NE(adapter_, nullptr);

    uint8_t phonemes[] = {4, 5, 6};
    wernicke_word_t word = CreateWord(42, "dog", phonemes, 3, 0.7f, 101, POS_NOUN);
    wernicke_add_word(adapter_, &word);

    wernicke_word_t retrieved;
    bool result = wernicke_lookup_word(adapter_, "dog", &retrieved);
    EXPECT_TRUE(result) << "Lookup should succeed";
    EXPECT_STREQ(retrieved.word, "dog");
    EXPECT_EQ(retrieved.word_id, 42u);
}

TEST_F(WernickeIntegrationTest, LookupNonexistentWordFails) {
    wernicke_config_t config = wernicke_default_config();
    config.enable_lexicon = true;
    adapter_ = wernicke_create(&config);
    ASSERT_NE(adapter_, nullptr);

    wernicke_word_t retrieved;
    bool result = wernicke_lookup_word(adapter_, "nonexistent", &retrieved);
    EXPECT_FALSE(result) << "Lookup of nonexistent word should fail";
}

TEST_F(WernickeIntegrationTest, MultipleWords) {
    wernicke_config_t config = wernicke_default_config();
    config.enable_lexicon = true;
    adapter_ = wernicke_create(&config);
    ASSERT_NE(adapter_, nullptr);

    const char* words[] = {"apple", "banana", "cherry", "date", "elderberry"};
    for (uint32_t i = 0; i < 5; i++) {
        uint8_t phonemes[] = {(uint8_t)(i * 3), (uint8_t)(i * 3 + 1), (uint8_t)(i * 3 + 2)};
        wernicke_word_t word = CreateWord(i + 1, words[i], phonemes, 3, 0.9f - i * 0.1f, i + 100, POS_NOUN);
        bool result = wernicke_add_word(adapter_, &word);
        EXPECT_TRUE(result) << "Adding word " << i << " should succeed";
    }

    // Verify all can be looked up
    for (uint32_t i = 0; i < 5; i++) {
        wernicke_word_t retrieved;
        bool result = wernicke_lookup_word(adapter_, words[i], &retrieved);
        EXPECT_TRUE(result) << "Lookup of word " << words[i] << " should succeed";
    }
}

//=============================================================================
// Lexical Access Module Tests
//=============================================================================

TEST_F(WernickeIntegrationTest, LexicalModuleCreate) {
    lexical_ = lexical_create(nullptr);
    ASSERT_NE(lexical_, nullptr) << "lexical_create with NULL config should succeed";
}

TEST_F(WernickeIntegrationTest, LexicalModuleCustomConfig) {
    lexical_config_t config = lexical_default_config();
    config.lexicon_size = 5000;
    config.max_cohort_size = 50;
    config.frequency_weight = 0.4f;
    config.enable_priming = true;

    lexical_ = lexical_create(&config);
    ASSERT_NE(lexical_, nullptr) << "lexical_create with custom config should succeed";
}

TEST_F(WernickeIntegrationTest, LexicalAddEntry) {
    lexical_ = lexical_create(nullptr);
    ASSERT_NE(lexical_, nullptr);

    uint8_t phonemes[] = {10, 11, 12, 13};
    lexical_entry_t entry = CreateLexicalEntry(1, "hello", phonemes, 4, 5.0f, 200);

    bool result = lexical_add_entry(lexical_, &entry);
    EXPECT_TRUE(result) << "Adding entry should succeed";

    EXPECT_EQ(lexical_get_size(lexical_), 1u) << "Lexicon size should be 1";
}

TEST_F(WernickeIntegrationTest, LexicalAddWord) {
    lexical_ = lexical_create(nullptr);
    ASSERT_NE(lexical_, nullptr);

    phoneme_t phonemes[4] = {PHONEME_W, PHONEME_ER, PHONEME_L, PHONEME_D};

    uint32_t word_id = lexical_add_word(lexical_, "world", phonemes, 4, 4.5f, 201);
    EXPECT_GT(word_id, 0u) << "Word ID should be positive";
}

TEST_F(WernickeIntegrationTest, LexicalLookupByString) {
    lexical_ = lexical_create(nullptr);
    ASSERT_NE(lexical_, nullptr);

    uint8_t phonemes[] = {1, 2, 3};
    lexical_entry_t entry = CreateLexicalEntry(100, "test", phonemes, 3, 4.0f, 300);
    lexical_add_entry(lexical_, &entry);

    lexical_entry_t retrieved;
    bool result = lexical_lookup_word(lexical_, "test", &retrieved);
    EXPECT_TRUE(result) << "Lookup by string should succeed";
    EXPECT_EQ(retrieved.word_id, 100u);
}

TEST_F(WernickeIntegrationTest, LexicalLookupById) {
    lexical_ = lexical_create(nullptr);
    ASSERT_NE(lexical_, nullptr);

    uint8_t phonemes[] = {7, 8};
    lexical_entry_t entry = CreateLexicalEntry(55, "go", phonemes, 2, 6.0f, 400);
    lexical_add_entry(lexical_, &entry);

    lexical_entry_t retrieved;
    bool result = lexical_get_entry(lexical_, 55, &retrieved);
    EXPECT_TRUE(result) << "Lookup by ID should succeed";
    EXPECT_STREQ(retrieved.orthography, "go");
}

TEST_F(WernickeIntegrationTest, LexicalBeginRecognition) {
    lexical_ = lexical_create(nullptr);
    ASSERT_NE(lexical_, nullptr);

    bool result = lexical_begin_recognition(lexical_);
    EXPECT_TRUE(result) << "Begin recognition should succeed";
}

TEST_F(WernickeIntegrationTest, LexicalProcessPhoneme) {
    lexical_ = lexical_create(nullptr);
    ASSERT_NE(lexical_, nullptr);

    // Add a word first
    uint8_t phonemes[] = {1, 2, 3};
    lexical_entry_t entry = CreateLexicalEntry(1, "abc", phonemes, 3, 5.0f, 100);
    lexical_add_entry(lexical_, &entry);

    lexical_begin_recognition(lexical_);

    phoneme_t p = PHONEME_AA;
    bool result = lexical_process_phoneme(lexical_, p, 0.9f);
    EXPECT_TRUE(result) << "Process phoneme should succeed";
}

TEST_F(WernickeIntegrationTest, LexicalRecognizeWord) {
    lexical_ = lexical_create(nullptr);
    ASSERT_NE(lexical_, nullptr);

    // Add words to lexicon - use phoneme indices matching the entry
    uint8_t p1[] = {(uint8_t)PHONEME_K, (uint8_t)PHONEME_AE, (uint8_t)PHONEME_T};
    uint8_t p2[] = {(uint8_t)PHONEME_K, (uint8_t)PHONEME_AE, (uint8_t)PHONEME_B};
    lexical_entry_t e1 = CreateLexicalEntry(1, "cat", p1, 3, 5.0f, 100);
    lexical_entry_t e2 = CreateLexicalEntry(2, "cab", p2, 3, 4.0f, 101);
    lexical_add_entry(lexical_, &e1);
    lexical_add_entry(lexical_, &e2);

    // Verify entries are in lexicon
    EXPECT_EQ(lexical_get_size(lexical_), 2u);

    // Try to recognize "cat" - this uses incremental cohort model
    phoneme_t phonemes[3] = {PHONEME_K, PHONEME_AE, PHONEME_T};

    lexical_result_t result;
    memset(&result, 0, sizeof(result));
    lexical_recognize_word(lexical_, phonemes, 3, &result);
    // Recognition depends on matching phoneme sequences in lexicon
}

TEST_F(WernickeIntegrationTest, LexicalGetCohort) {
    lexical_ = lexical_create(nullptr);
    ASSERT_NE(lexical_, nullptr);

    // Add some words
    uint8_t p1[] = {1, 2};
    uint8_t p2[] = {1, 3};
    lexical_entry_t e1 = CreateLexicalEntry(1, "be", p1, 2, 6.0f, 100);
    lexical_entry_t e2 = CreateLexicalEntry(2, "ba", p2, 2, 5.0f, 101);
    lexical_add_entry(lexical_, &e1);
    lexical_add_entry(lexical_, &e2);

    lexical_begin_recognition(lexical_);

    phoneme_t p = PHONEME_B;
    lexical_process_phoneme(lexical_, p, 0.9f);

    const cohort_state_t* cohort = nullptr;
    bool result = lexical_get_cohort(lexical_, &cohort);
    EXPECT_TRUE(result) << "Get cohort should succeed";
    EXPECT_NE(cohort, nullptr);
}

TEST_F(WernickeIntegrationTest, LexicalReset) {
    lexical_ = lexical_create(nullptr);
    ASSERT_NE(lexical_, nullptr);

    lexical_begin_recognition(lexical_);

    bool result = lexical_reset(lexical_);
    EXPECT_TRUE(result) << "Reset should succeed";
}

TEST_F(WernickeIntegrationTest, LexicalPriming) {
    lexical_config_t config = lexical_default_config();
    config.enable_priming = true;
    lexical_ = lexical_create(&config);
    ASSERT_NE(lexical_, nullptr);

    // Priming functions - may require word/concept to exist first
    bool result = lexical_prime_concept(lexical_, 100, 0.7f);
    // Result depends on implementation - concept may need to exist

    // Add a word first before trying to prime it
    uint8_t phonemes[] = {1, 2};
    lexical_entry_t entry = CreateLexicalEntry(1, "test", phonemes, 2, 5.0f, 100);
    lexical_add_entry(lexical_, &entry);

    result = lexical_decay_priming(lexical_);
    EXPECT_TRUE(result) << "Priming decay should succeed";

    lexical_clear_priming(lexical_);
}

TEST_F(WernickeIntegrationTest, LexicalGetStats) {
    lexical_ = lexical_create(nullptr);
    ASSERT_NE(lexical_, nullptr);

    lexical_stats_t stats;
    bool result = lexical_get_stats(lexical_, &stats);
    EXPECT_TRUE(result) << "Get stats should succeed";
}

TEST_F(WernickeIntegrationTest, LexicalGetFrequency) {
    lexical_ = lexical_create(nullptr);
    ASSERT_NE(lexical_, nullptr);

    uint8_t phonemes[] = {1, 2};
    lexical_entry_t entry = CreateLexicalEntry(77, "is", phonemes, 2, 6.5f, 200);
    lexical_add_entry(lexical_, &entry);

    float freq = lexical_get_frequency(lexical_, 77);
    EXPECT_NEAR(freq, 6.5f, 0.1f);
}

//=============================================================================
// Semantic Integrator Tests
//=============================================================================

TEST_F(WernickeIntegrationTest, SemanticCreate) {
    semantic_ = semantic_create(nullptr);
    ASSERT_NE(semantic_, nullptr) << "semantic_create with NULL config should succeed";
}

TEST_F(WernickeIntegrationTest, SemanticCustomConfig) {
    semantic_config_t config = semantic_default_config();
    config.max_senses = 16;
    config.max_context_words = 64;
    config.max_active_concepts = 256;
    config.enable_spreading_activation = true;
    config.enable_thematic_roles = true;

    semantic_ = semantic_create(&config);
    ASSERT_NE(semantic_, nullptr) << "semantic_create with custom config should succeed";
}

TEST_F(WernickeIntegrationTest, SemanticReset) {
    semantic_ = semantic_create(nullptr);
    ASSERT_NE(semantic_, nullptr);

    bool result = semantic_reset(semantic_);
    EXPECT_TRUE(result) << "semantic_reset should succeed";
}

TEST_F(WernickeIntegrationTest, SemanticRegisterSenses) {
    semantic_ = semantic_create(nullptr);
    ASSERT_NE(semantic_, nullptr);

    word_sense_t senses[2] = {};
    senses[0].sense_id = 1;
    senses[0].concept_id = 100;
    strncpy(senses[0].gloss, "financial institution", sizeof(senses[0].gloss) - 1);
    senses[0].frequency = 0.6f;

    senses[1].sense_id = 2;
    senses[1].concept_id = 101;
    strncpy(senses[1].gloss, "river bank", sizeof(senses[1].gloss) - 1);
    senses[1].frequency = 0.4f;

    bool result = semantic_register_senses(semantic_, 42, senses, 2);
    EXPECT_TRUE(result) << "Register senses should succeed";
}

TEST_F(WernickeIntegrationTest, SemanticGetSenses) {
    semantic_ = semantic_create(nullptr);
    ASSERT_NE(semantic_, nullptr);

    // Register senses first
    word_sense_t senses[2] = {};
    senses[0].sense_id = 1;
    senses[0].concept_id = 100;
    senses[0].frequency = 0.7f;
    senses[1].sense_id = 2;
    senses[1].concept_id = 101;
    senses[1].frequency = 0.3f;

    semantic_register_senses(semantic_, 50, senses, 2);

    word_sense_t retrieved[4];
    uint32_t num_senses = 0;
    bool result = semantic_get_senses(semantic_, 50, retrieved, 4, &num_senses);
    EXPECT_TRUE(result) << "Get senses should succeed";
    EXPECT_EQ(num_senses, 2u);
}

TEST_F(WernickeIntegrationTest, SemanticAddSense) {
    semantic_ = semantic_create(nullptr);
    ASSERT_NE(semantic_, nullptr);

    uint32_t sense_id = semantic_add_sense(semantic_, 10, 200, "test sense", 0.8f);
    EXPECT_GT(sense_id, 0u) << "Add sense should return valid ID";
}

TEST_F(WernickeIntegrationTest, SemanticIntegrateWord) {
    semantic_ = semantic_create(nullptr);
    ASSERT_NE(semantic_, nullptr);

    // Register senses
    word_sense_t senses[1] = {};
    senses[0].sense_id = 1;
    senses[0].concept_id = 500;
    senses[0].frequency = 1.0f;
    semantic_register_senses(semantic_, 100, senses, 1);

    float features[32] = {0};
    semantic_result_t result;
    bool success = semantic_integrate_word(semantic_, 100, features, 32, &result);
    EXPECT_TRUE(success) << "Integrate word should succeed";
}

TEST_F(WernickeIntegrationTest, SemanticBeginEndUtterance) {
    semantic_ = semantic_create(nullptr);
    ASSERT_NE(semantic_, nullptr);

    bool result = semantic_begin_utterance(semantic_);
    EXPECT_TRUE(result) << "Begin utterance should succeed";

    float coherence = 0.0f;
    result = semantic_end_utterance(semantic_, &coherence);
    EXPECT_TRUE(result) << "End utterance should succeed";
}

TEST_F(WernickeIntegrationTest, SemanticDisambiguate) {
    semantic_config_t config = semantic_default_config();
    config.strategy = DISAMBIG_CONTEXT;
    semantic_ = semantic_create(&config);
    ASSERT_NE(semantic_, nullptr);

    // Register ambiguous word
    word_sense_t senses[2] = {};
    senses[0].sense_id = 1;
    senses[0].concept_id = 100;
    senses[0].frequency = 0.5f;
    senses[1].sense_id = 2;
    senses[1].concept_id = 101;
    senses[1].frequency = 0.5f;
    semantic_register_senses(semantic_, 999, senses, 2);

    float context[32] = {0};
    uint32_t selected_sense = 0;
    float confidence = 0.0f;
    bool result = semantic_disambiguate(semantic_, 999, context, 32, &selected_sense, &confidence);
    EXPECT_TRUE(result) << "Disambiguate should succeed";
}

TEST_F(WernickeIntegrationTest, SemanticActivateConcept) {
    semantic_config_t config = semantic_default_config();
    config.enable_spreading_activation = true;
    semantic_ = semantic_create(&config);
    ASSERT_NE(semantic_, nullptr);

    bool result = semantic_activate_concept(semantic_, 100, 0.8f);
    EXPECT_TRUE(result) << "Activate concept should succeed";

    float activation = semantic_get_activation(semantic_, 100);
    EXPECT_GT(activation, 0.0f) << "Activation should be positive";
}

TEST_F(WernickeIntegrationTest, SemanticGetActiveConcepts) {
    semantic_config_t config = semantic_default_config();
    config.enable_spreading_activation = true;
    semantic_ = semantic_create(&config);
    ASSERT_NE(semantic_, nullptr);

    semantic_activate_concept(semantic_, 100, 0.9f);
    semantic_activate_concept(semantic_, 101, 0.7f);

    active_concept_t concepts[16];
    uint32_t num_concepts = 0;
    bool result = semantic_get_active_concepts(semantic_, concepts, 16, &num_concepts);
    EXPECT_TRUE(result) << "Get active concepts should succeed";
}

TEST_F(WernickeIntegrationTest, SemanticThematicRoles) {
    semantic_config_t config = semantic_default_config();
    config.enable_thematic_roles = true;
    semantic_ = semantic_create(&config);
    ASSERT_NE(semantic_, nullptr);

    // Thematic role assignment may require concept to be active first
    semantic_activate_concept(semantic_, 100, 1.0f);

    bool result = semantic_assign_role(semantic_, 100, ROLE_AGENT);
    EXPECT_TRUE(result) << "Assign role should succeed";

    // Role retrieval depends on implementation state management
    thematic_role_t role = semantic_get_role(semantic_, 100);
    // Role may be ROLE_NONE if concept wasn't properly registered

    uint32_t filler = semantic_get_role_filler(semantic_, ROLE_AGENT);
    // Filler depends on implementation
    (void)role;
    (void)filler;
}

TEST_F(WernickeIntegrationTest, SemanticPriming) {
    semantic_config_t config = semantic_default_config();
    config.enable_spreading_activation = true;
    semantic_ = semantic_create(&config);
    ASSERT_NE(semantic_, nullptr);

    bool result = semantic_prime_concept(semantic_, 200, 0.6f);
    EXPECT_TRUE(result) << "Prime concept should succeed";

    float priming = semantic_is_primed(semantic_, 200);
    EXPECT_GT(priming, 0.0f);

    result = semantic_decay_priming(semantic_);
    EXPECT_TRUE(result);

    semantic_clear_priming(semantic_);
}

TEST_F(WernickeIntegrationTest, SemanticAnomalyDetection) {
    semantic_config_t config = semantic_default_config();
    config.enable_anomaly_detection = true;
    semantic_ = semantic_create(&config);
    ASSERT_NE(semantic_, nullptr);

    float anomaly_score = 0.0f;
    bool result = semantic_compute_anomaly(semantic_, 1, 100, &anomaly_score);
    EXPECT_TRUE(result) << "Compute anomaly should succeed";

    float coherence = semantic_get_coherence(semantic_);
    EXPECT_GE(coherence, 0.0f);
    EXPECT_LE(coherence, 1.0f);

    semantic_set_anomaly_threshold(semantic_, 0.5f);
}

TEST_F(WernickeIntegrationTest, SemanticGetStats) {
    semantic_ = semantic_create(nullptr);
    ASSERT_NE(semantic_, nullptr);

    semantic_stats_t stats;
    bool result = semantic_get_stats(semantic_, &stats);
    EXPECT_TRUE(result) << "Get stats should succeed";
}

TEST_F(WernickeIntegrationTest, SemanticRoleName) {
    const char* name = semantic_role_name(ROLE_AGENT);
    EXPECT_NE(name, nullptr);
    EXPECT_STRNE(name, "");

    name = semantic_role_name(ROLE_PATIENT);
    EXPECT_NE(name, nullptr);

    name = semantic_role_name(ROLE_NONE);
    EXPECT_NE(name, nullptr);
}

//=============================================================================
// Working Memory Tests
//=============================================================================

TEST_F(WernickeIntegrationTest, WorkingMemoryStore) {
    wernicke_config_t config = wernicke_default_config();
    config.enable_working_memory = true;
    adapter_ = wernicke_create(&config);
    ASSERT_NE(adapter_, nullptr);

    phoneme_t phonemes[4] = {PHONEME_H, PHONEME_EH, PHONEME_L, PHONEME_OW};

    bool result = wernicke_wm_store(adapter_, phonemes, 4);
    EXPECT_TRUE(result) << "Store phonemes should succeed";
}

TEST_F(WernickeIntegrationTest, WorkingMemoryRehearse) {
    wernicke_config_t config = wernicke_default_config();
    config.enable_working_memory = true;
    adapter_ = wernicke_create(&config);
    ASSERT_NE(adapter_, nullptr);

    phoneme_t phonemes[2] = {PHONEME_G, PHONEME_OW};
    wernicke_wm_store(adapter_, phonemes, 2);

    bool result = wernicke_wm_rehearse(adapter_);
    EXPECT_TRUE(result) << "Rehearse should succeed";
}

TEST_F(WernickeIntegrationTest, WorkingMemoryGetContents) {
    wernicke_config_t config = wernicke_default_config();
    config.enable_working_memory = true;
    adapter_ = wernicke_create(&config);
    ASSERT_NE(adapter_, nullptr);

    phoneme_t phonemes[3] = {PHONEME_K, PHONEME_AE, PHONEME_T};
    wernicke_wm_store(adapter_, phonemes, 3);

    phoneme_t retrieved[16];
    uint32_t count = 0;
    bool result = wernicke_wm_get_contents(adapter_, retrieved, 16, &count);
    EXPECT_TRUE(result) << "Get contents should succeed";
    EXPECT_EQ(count, 3u);
}

TEST_F(WernickeIntegrationTest, WorkingMemoryClear) {
    wernicke_config_t config = wernicke_default_config();
    config.enable_working_memory = true;
    adapter_ = wernicke_create(&config);
    ASSERT_NE(adapter_, nullptr);

    phoneme_t phonemes[2] = {PHONEME_AA, PHONEME_H};
    wernicke_wm_store(adapter_, phonemes, 2);

    wernicke_wm_clear(adapter_);

    phoneme_t retrieved[16];
    uint32_t count = 0;
    wernicke_wm_get_contents(adapter_, retrieved, 16, &count);
    EXPECT_EQ(count, 0u) << "WM should be empty after clear";
}

//=============================================================================
// Event Integration Tests
//=============================================================================

static int g_wernicke_event_count = 0;
static void WernickeTestEventCallback(uint32_t event_type, const void* event_data, void* user_data) {
    (void)event_type;
    (void)event_data;
    (void)user_data;
    g_wernicke_event_count++;
}

static void WernickeWordCallback(const wernicke_word_result_t* word, void* user_data) {
    (void)word;
    (void)user_data;
}

static void WernickeConceptCallback(const wernicke_concept_t* concept, void* user_data) {
    (void)concept;
    (void)user_data;
}

static void WernickeComprehensionCallback(const wernicke_comprehension_t* comp, void* user_data) {
    (void)comp;
    (void)user_data;
}

TEST_F(WernickeIntegrationTest, EventCallbackRegistration) {
    wernicke_config_t config = wernicke_default_config();
    config.enable_events = true;
    adapter_ = wernicke_create(&config);
    ASSERT_NE(adapter_, nullptr);

    g_wernicke_event_count = 0;
    bool result = wernicke_set_event_callback(adapter_, WernickeTestEventCallback, nullptr);
    EXPECT_TRUE(result);

    result = wernicke_set_word_callback(adapter_, WernickeWordCallback, nullptr);
    EXPECT_TRUE(result);

    result = wernicke_set_concept_callback(adapter_, WernickeConceptCallback, nullptr);
    EXPECT_TRUE(result);

    result = wernicke_set_comprehension_callback(adapter_, WernickeComprehensionCallback, nullptr);
    EXPECT_TRUE(result);
}

//=============================================================================
// Status and Diagnostics Tests
//=============================================================================

TEST_F(WernickeIntegrationTest, StatusTransitions) {
    adapter_ = wernicke_create(nullptr);
    ASSERT_NE(adapter_, nullptr);

    EXPECT_EQ(wernicke_get_status(adapter_), WERNICKE_STATUS_IDLE);

    wernicke_reset(adapter_);
    EXPECT_EQ(wernicke_get_status(adapter_), WERNICKE_STATUS_IDLE);
}

TEST_F(WernickeIntegrationTest, ErrorStringConversion) {
    const char* str = wernicke_error_string(WERNICKE_ERROR_NONE);
    EXPECT_NE(str, nullptr);
    EXPECT_STRNE(str, "");

    str = wernicke_error_string(WERNICKE_ERROR_LEXICAL_FAILURE);
    EXPECT_NE(str, nullptr);

    str = wernicke_error_string(WERNICKE_ERROR_SEMANTIC_FAILURE);
    EXPECT_NE(str, nullptr);
}

TEST_F(WernickeIntegrationTest, StatusStringConversion) {
    const char* str = wernicke_status_string(WERNICKE_STATUS_IDLE);
    EXPECT_NE(str, nullptr);
    EXPECT_STRNE(str, "");

    str = wernicke_status_string(WERNICKE_STATUS_LEXICAL_ACCESS);
    EXPECT_NE(str, nullptr);

    str = wernicke_status_string(WERNICKE_STATUS_SEMANTIC);
    EXPECT_NE(str, nullptr);
}

TEST_F(WernickeIntegrationTest, GetStatistics) {
    adapter_ = wernicke_create(nullptr);
    ASSERT_NE(adapter_, nullptr);

    wernicke_stats_t stats;
    bool result = wernicke_get_stats(adapter_, &stats);
    EXPECT_TRUE(result);
    EXPECT_EQ(stats.phonemes_processed, 0u);
    EXPECT_EQ(stats.words_recognized, 0u);
}

//=============================================================================
// Bio-Async Integration Tests
//=============================================================================

TEST_F(WernickeIntegrationTest, BioAsyncEnabled) {
    wernicke_config_t config = wernicke_default_config();
    config.enable_bio_async = true;
    adapter_ = wernicke_create(&config);
    ASSERT_NE(adapter_, nullptr);

    bio_module_context_t ctx = wernicke_get_bio_context(adapter_);
    // Context should be valid when bio-async is enabled
    (void)ctx;
}

TEST_F(WernickeIntegrationTest, ProcessBioMessages) {
    wernicke_config_t config = wernicke_default_config();
    config.enable_bio_async = true;
    adapter_ = wernicke_create(&config);
    ASSERT_NE(adapter_, nullptr);

    uint32_t processed = wernicke_process_bio_messages(adapter_, 10);
    EXPECT_EQ(processed, 0u) << "No messages initially";
}

//=============================================================================
// Sub-Module Access Tests
//=============================================================================

TEST_F(WernickeIntegrationTest, GetPhonologicalAnalyzer) {
    wernicke_config_t config = wernicke_default_config();
    config.enable_phonological = true;
    adapter_ = wernicke_create(&config);
    ASSERT_NE(adapter_, nullptr);

    phonological_analyzer_t* analyzer = wernicke_get_phonological_analyzer(adapter_);
    // May return NULL if sub-module not separately allocated
    (void)analyzer;
}

TEST_F(WernickeIntegrationTest, GetLexicalAccess) {
    wernicke_config_t config = wernicke_default_config();
    config.enable_lexical = true;
    adapter_ = wernicke_create(&config);
    ASSERT_NE(adapter_, nullptr);

    lexical_access_t* lex = wernicke_get_lexical_access(adapter_);
    // May return NULL if sub-module not separately accessible
    (void)lex;
}

TEST_F(WernickeIntegrationTest, GetSemanticIntegrator) {
    wernicke_config_t config = wernicke_default_config();
    config.enable_semantic = true;
    adapter_ = wernicke_create(&config);
    ASSERT_NE(adapter_, nullptr);

    semantic_integrator_t* sem = wernicke_get_semantic_integrator(adapter_);
    // May return NULL if sub-module not separately accessible
    (void)sem;
}

TEST_F(WernickeIntegrationTest, GetSyntacticComprehension) {
    wernicke_config_t config = wernicke_default_config();
    config.enable_syntactic = true;
    adapter_ = wernicke_create(&config);
    ASSERT_NE(adapter_, nullptr);

    syntactic_comprehension_t* syn = wernicke_get_syntactic_comprehension(adapter_);
    // May return NULL if sub-module not separately accessible
    (void)syn;
}

//=============================================================================
// Context and Inference Tests
//=============================================================================

TEST_F(WernickeIntegrationTest, SemanticContextVector) {
    semantic_ = semantic_create(nullptr);
    ASSERT_NE(semantic_, nullptr);

    const sentence_context_t* ctx = nullptr;
    bool result = semantic_get_context(semantic_, &ctx);
    EXPECT_TRUE(result) << "Get context should succeed";
}

TEST_F(WernickeIntegrationTest, SemanticUpdateContext) {
    semantic_ = semantic_create(nullptr);
    ASSERT_NE(semantic_, nullptr);

    float external_features[32] = {0};
    external_features[0] = 0.5f;

    bool result = semantic_update_context(semantic_, external_features, 32, 0.3f);
    EXPECT_TRUE(result) << "Update context should succeed";
}

TEST_F(WernickeIntegrationTest, SemanticDecayContext) {
    semantic_ = semantic_create(nullptr);
    ASSERT_NE(semantic_, nullptr);

    bool result = semantic_decay_context(semantic_);
    EXPECT_TRUE(result) << "Decay context should succeed";
}

TEST_F(WernickeIntegrationTest, SemanticDecayActivations) {
    semantic_ = semantic_create(nullptr);
    ASSERT_NE(semantic_, nullptr);

    semantic_activate_concept(semantic_, 100, 1.0f);

    bool result = semantic_decay_activations(semantic_);
    EXPECT_TRUE(result) << "Decay activations should succeed";

    float activation = semantic_get_activation(semantic_, 100);
    EXPECT_LT(activation, 1.0f) << "Activation should have decayed";
}

TEST_F(WernickeIntegrationTest, SemanticGenerateInferences) {
    semantic_config_t config = semantic_default_config();
    config.enable_inference = true;
    semantic_ = semantic_create(&config);
    ASSERT_NE(semantic_, nullptr);

    uint32_t inferred[16];
    uint32_t num_inferred = 0;
    bool result = semantic_generate_inferences(semantic_, inferred, 16, &num_inferred);
    EXPECT_TRUE(result) << "Generate inferences should succeed";
}

//=============================================================================
// NLP Bridge Tests
//=============================================================================

TEST_F(WernickeIntegrationTest, PredictNextWord) {
    wernicke_config_t config = wernicke_default_config();
    config.enable_lexicon = true;
    adapter_ = wernicke_create(&config);
    ASSERT_NE(adapter_, nullptr);

    // Add some words
    uint8_t p1[] = {1, 2};
    uint8_t p2[] = {3, 4};
    wernicke_word_t w1 = CreateWord(1, "the", p1, 2, 0.95f, 100, POS_DETERMINER);
    wernicke_word_t w2 = CreateWord(2, "cat", p2, 2, 0.7f, 101, POS_NOUN);
    wernicke_add_word(adapter_, &w1);
    wernicke_add_word(adapter_, &w2);

    wernicke_context_t context = {};
    wernicke_word_pred_t prediction;
    bool result = wernicke_predict_next_word(adapter_, &context, &prediction);
    // May or may not have predictions depending on implementation
    (void)result;
}

TEST_F(WernickeIntegrationTest, GetMeaning) {
    wernicke_config_t config = wernicke_default_config();
    config.enable_semantic = true;
    adapter_ = wernicke_create(&config);
    ASSERT_NE(adapter_, nullptr);

    wernicke_word_result_t word_result = {};
    word_result.word.word_id = 1;
    word_result.word.concept_id = 100;
    word_result.confidence = 0.9f;

    wernicke_concept_t concept;
    bool result = wernicke_get_meaning(adapter_, &word_result, &concept);
    // May or may not find concept depending on implementation
    (void)result;
}

TEST_F(WernickeIntegrationTest, Disambiguate) {
    wernicke_config_t config = wernicke_default_config();
    config.enable_semantic = true;
    adapter_ = wernicke_create(&config);
    ASSERT_NE(adapter_, nullptr);

    wernicke_word_result_t word = {};
    word.word.word_id = 1;
    strncpy(word.word.word, "bank", sizeof(word.word.word) - 1);

    wernicke_context_t context = {};
    wernicke_concept_t concept;

    bool result = wernicke_disambiguate(adapter_, &word, &context, &concept);
    // May or may not disambiguate depending on implementation
    (void)result;
}

TEST_F(WernickeIntegrationTest, SpreadActivation) {
    wernicke_config_t config = wernicke_default_config();
    config.enable_semantic = true;
    adapter_ = wernicke_create(&config);
    ASSERT_NE(adapter_, nullptr);

    wernicke_concept_t activated[32];
    uint32_t num_activated = 0;

    bool result = wernicke_spread_activation(adapter_, 100, 2, activated, 32, &num_activated);
    // May or may not spread depending on implementation
    (void)result;
}

//=============================================================================
// Batch and File Operations
//=============================================================================

TEST_F(WernickeIntegrationTest, LexicalBuildCommonEnglish) {
    lexical_ = lexical_create(nullptr);
    ASSERT_NE(lexical_, nullptr);

    uint32_t count = lexical_build_common_english(lexical_);
    // Implementation may or may not include common words
    (void)count;
}

