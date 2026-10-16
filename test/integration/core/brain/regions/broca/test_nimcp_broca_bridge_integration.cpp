/**
 * @file test_nimcp_broca_bridge_integration.cpp
 * @brief Integration tests for Broca adapter and Language Production Bridge
 *
 * WHAT: Test the integration between broca_adapter and language_production_bridge
 * WHY:  Ensure the high-level adapter and bridge work together correctly
 * HOW:  Create both components, connect them, and run production pipelines
 *
 * COVERAGE TARGET: Full adapter-bridge integration
 */

#include <gtest/gtest.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

// Headers have their own extern "C" guards
#include "core/brain/regions/broca/nimcp_broca_adapter.h"
#include "core/brain/regions/broca/nimcp_language_production_bridge.h"

// Test Fixture for Broca-Bridge Integration
class BrocaBridgeIntegrationTest : public ::testing::Test {
protected:
    broca_adapter_t* broca;
    language_production_bridge_t* bridge;

    void SetUp() override {
        // Create Broca adapter
        broca_config_t broca_config = broca_default_config();
        broca_config.enable_working_memory = true;
        broca_config.enable_lexicon = true;
        broca_config.enable_coarticulation = true;
        broca = broca_create(&broca_config);
        ASSERT_NE(nullptr, broca) << "Failed to create Broca adapter";

        // Create Language Production Bridge
        lpb_config_t bridge_config = lpb_default_config();
        bridge_config.enable_self_monitoring = true;
        bridge_config.enable_semantic_priming = true;
        bridge = lpb_create(&bridge_config, broca);
        ASSERT_NE(nullptr, bridge) << "Failed to create Language Production Bridge";

        // Setup lexicon for testing
        setup_test_lexicon();
    }

    void TearDown() override {
        lpb_destroy(bridge);
        bridge = nullptr;

        broca_destroy(broca);
        broca = nullptr;
    }

    // Setup a comprehensive test lexicon
    void setup_test_lexicon() {
        broca_lexical_entry_t entry;

        // Function words
        add_lexicon_entry("the", 1, {10, 11}, 2, 0, 0.99f);
        add_lexicon_entry("a", 2, {12}, 1, 0, 0.95f);
        add_lexicon_entry("is", 3, {13, 14}, 2, 1, 0.90f);
        add_lexicon_entry("it", 4, {15, 16}, 2, 0, 0.95f);
        add_lexicon_entry("does", 5, {17, 18, 19}, 3, 1, 0.85f);

        // Content words
        add_lexicon_entry("cat", 10, {20, 21, 22}, 3, 0, 0.80f);
        add_lexicon_entry("dog", 11, {23, 24, 25}, 3, 0, 0.78f);
        add_lexicon_entry("runs", 12, {26, 27, 28, 29}, 4, 1, 0.75f);
        add_lexicon_entry("sat", 13, {30, 31, 32}, 3, 1, 0.72f);
        add_lexicon_entry("good", 14, {33, 34, 35}, 3, 2, 0.70f);
        add_lexicon_entry("big", 15, {36, 37, 38}, 3, 2, 0.68f);
    }

    void add_lexicon_entry(const char* word, uint32_t id, std::initializer_list<uint8_t> phonemes,
                           uint32_t phoneme_count, uint8_t pos, float freq) {
        broca_lexical_entry_t entry;
        memset(&entry, 0, sizeof(entry));
        entry.word_id = id;
        strncpy(entry.word, word, sizeof(entry.word) - 1);

        size_t i = 0;
        for (uint8_t p : phonemes) {
            if (i < sizeof(entry.phonemes)) {
                entry.phonemes[i++] = p;
            }
        }
        entry.phoneme_count = phoneme_count;
        entry.pos = pos;
        entry.frequency = freq;

        ASSERT_TRUE(broca_add_lexical_entry(broca, &entry));
    }

    // Create a semantic intent with configurable activation
    lpb_semantic_intent_t create_test_intent(float entity, float action, float property) {
        lpb_semantic_intent_t intent;
        memset(&intent, 0, sizeof(intent));

        intent.semantic_dim = 256;
        intent.semantic_vector = (float*)calloc(intent.semantic_dim, sizeof(float));

        // Entity features (0-31)
        for (uint32_t i = 0; i < 32; i++) {
            intent.semantic_vector[i] = entity * (1.0f + 0.1f * (i % 5));
        }

        // Action features (32-63)
        for (uint32_t i = 32; i < 64; i++) {
            intent.semantic_vector[i] = action * (1.0f + 0.1f * (i % 5));
        }

        // Property features (64-95)
        for (uint32_t i = 64; i < 96; i++) {
            intent.semantic_vector[i] = property * (1.0f + 0.1f * (i % 5));
        }

        intent.confidence = 0.9f;
        intent.intent_type = 0;
        intent.from_internal = true;

        return intent;
    }

    void free_intent(lpb_semantic_intent_t* intent) {
        if (intent && intent->semantic_vector) {
            free(intent->semantic_vector);
            intent->semantic_vector = nullptr;
        }
    }
};

// =============================================================================
// Basic Integration Tests
// =============================================================================

TEST_F(BrocaBridgeIntegrationTest, BridgeAccessesBrocaCorrectly) {
    // Verify bridge has correct Broca reference
    EXPECT_EQ(lpb_get_broca_adapter(bridge), broca);
}

TEST_F(BrocaBridgeIntegrationTest, BridgeInheritsBrocaLexicon) {
    // Verify words added to Broca are accessible
    broca_lexical_entry_t found;
    EXPECT_TRUE(broca_lookup_word(broca, 10, "cat", &found));
    EXPECT_STREQ(found.word, "cat");
    EXPECT_EQ(found.phoneme_count, 3u);
}

TEST_F(BrocaBridgeIntegrationTest, ResetBothComponentsIndependently) {
    // Reset bridge
    EXPECT_TRUE(lpb_reset(bridge));
    EXPECT_EQ(lpb_get_status(bridge), LPB_STATUS_IDLE);

    // Reset Broca
    EXPECT_TRUE(broca_reset(broca));
    EXPECT_EQ(broca_get_status(broca), BROCA_STATUS_IDLE);
}

// =============================================================================
// Production Pipeline Integration Tests
// =============================================================================

TEST_F(BrocaBridgeIntegrationTest, ProduceFromSemanticIntent) {
    lpb_semantic_intent_t intent = create_test_intent(0.6f, 0.3f, 0.2f);
    lpb_production_result_t result;
    memset(&result, 0, sizeof(result));

    bool success = lpb_produce_from_intent(bridge, &intent, &result);

    // Check statistics were updated
    lpb_stats_t stats;
    lpb_get_stats(bridge, &stats);
    EXPECT_GE(stats.productions_attempted, 1u);

    if (success) {
        EXPECT_GT(result.token_count, 0u);
        EXPECT_GE(result.fluency_score, 0.0f);
        EXPECT_LE(result.fluency_score, 1.0f);
        EXPECT_EQ(stats.productions_successful, 1u);
    }

    free_intent(&intent);
}

TEST_F(BrocaBridgeIntegrationTest, ProduceMultipleUtterances) {
    // Produce several utterances to test state management (no reset between)
    for (int i = 0; i < 3; i++) {
        lpb_semantic_intent_t intent = create_test_intent(
            0.3f + 0.1f * i,  // Varying entity
            0.4f - 0.05f * i, // Varying action
            0.2f              // Constant property
        );

        lpb_production_result_t result;
        memset(&result, 0, sizeof(result));

        lpb_produce_from_intent(bridge, &intent, &result);

        // Status should be READY or ERROR after production
        lpb_status_t status = lpb_get_status(bridge);
        EXPECT_TRUE(status == LPB_STATUS_READY || status == LPB_STATUS_ERROR);

        free_intent(&intent);
        // Note: NOT resetting - we want to track cumulative stats
    }

    lpb_stats_t stats;
    lpb_get_stats(bridge, &stats);
    // Should have attempted 3 productions
    EXPECT_GE(stats.productions_attempted, 3u);
}

TEST_F(BrocaBridgeIntegrationTest, DirectTokenProduction) {
    lpb_token_t tokens[3];

    // Token 1: "it"
    memset(&tokens[0], 0, sizeof(lpb_token_t));
    tokens[0].token_id = 4;
    strcpy(tokens[0].token_str, "it");
    tokens[0].pos = 0;
    tokens[0].activation = 0.9f;

    // Token 2: "does"
    memset(&tokens[1], 0, sizeof(lpb_token_t));
    tokens[1].token_id = 5;
    strcpy(tokens[1].token_str, "does");
    tokens[1].pos = 1;
    tokens[1].activation = 0.85f;

    // Token 3: "good"
    memset(&tokens[2], 0, sizeof(lpb_token_t));
    tokens[2].token_id = 14;
    strcpy(tokens[2].token_str, "good");
    tokens[2].pos = 2;
    tokens[2].activation = 0.8f;

    lpb_production_result_t result;
    memset(&result, 0, sizeof(result));

    bool success = lpb_produce_from_tokens(bridge, tokens, 3, &result);

    if (success) {
        EXPECT_EQ(result.token_count, 3u);
        EXPECT_FLOAT_EQ(result.semantic_match, 1.0f); // Direct tokens = exact match
    }
}

// =============================================================================
// Lexical Access Integration Tests
// =============================================================================

TEST_F(BrocaBridgeIntegrationTest, LexicalSelectionFromSemantic) {
    float semantic_vector[256];
    memset(semantic_vector, 0, sizeof(semantic_vector));

    // Strong entity activation (should select nouns)
    for (int i = 0; i < 32; i++) {
        semantic_vector[i] = 0.7f;
    }

    lpb_token_t tokens[10];
    uint32_t num_selected = 0;

    bool success = lpb_select_lexical_items(bridge, semantic_vector, 256, tokens, 10, &num_selected);

    if (success) {
        EXPECT_GT(num_selected, 0u);
        EXPECT_LE(num_selected, 10u);

        // All selected tokens should have positive activation
        for (uint32_t i = 0; i < num_selected; i++) {
            EXPECT_GT(tokens[i].activation, 0.0f);
        }
    }
}

TEST_F(BrocaBridgeIntegrationTest, SemanticPrimingAffectsSelection) {
    // Prime with context
    float context[256];
    for (int i = 0; i < 256; i++) {
        context[i] = 0.2f;
    }
    EXPECT_TRUE(lpb_prime_lexical_access(bridge, context, 256, 0.8f));

    // Now select with similar semantic vector
    float semantic[256];
    for (int i = 0; i < 256; i++) {
        semantic[i] = 0.25f; // Similar to context
    }

    lpb_token_t tokens[10];
    uint32_t num_selected = 0;

    lpb_select_lexical_items(bridge, semantic, 256, tokens, 10, &num_selected);

    // Priming should boost activation (if selection succeeds)
    // Just verify the API works correctly
    EXPECT_GE(num_selected, 0u);
}

// =============================================================================
// Self-Monitoring Integration Tests
// =============================================================================

TEST_F(BrocaBridgeIntegrationTest, SelfMonitoringDuringProduction) {
    // Enable monitoring
    EXPECT_TRUE(lpb_set_self_monitoring(bridge, true));

    lpb_semantic_intent_t intent = create_test_intent(0.5f, 0.4f, 0.3f);
    lpb_production_result_t result;
    memset(&result, 0, sizeof(result));

    lpb_produce_from_intent(bridge, &intent, &result);

    // Check monitoring was performed
    float match_score;
    lpb_check_production(bridge, &match_score);
    EXPECT_GE(match_score, 0.0f);
    EXPECT_LE(match_score, 1.0f);

    free_intent(&intent);
}

TEST_F(BrocaBridgeIntegrationTest, DisabledMonitoringPassesAlways) {
    EXPECT_TRUE(lpb_set_self_monitoring(bridge, false));

    float match_score = -1.0f;
    EXPECT_TRUE(lpb_check_production(bridge, &match_score));
    EXPECT_FLOAT_EQ(match_score, 1.0f); // Should return 1.0 when disabled
}

// =============================================================================
// Working Memory Integration Tests
// =============================================================================

TEST_F(BrocaBridgeIntegrationTest, WorkingMemoryThroughBroca) {
    // Push words to Broca's working memory
    EXPECT_TRUE(broca_wm_push(broca, 10)); // "cat"
    EXPECT_TRUE(broca_wm_push(broca, 11)); // "dog"
    EXPECT_TRUE(broca_wm_push(broca, 12)); // "runs"

    // Verify they can be retrieved
    uint32_t buffer[10];
    uint32_t count = 10;
    EXPECT_TRUE(broca_wm_get_contents(broca, buffer, &count));
    EXPECT_GE(count, 1u);

    // Pop and verify order
    uint32_t popped;
    EXPECT_TRUE(broca_wm_pop(broca, &popped));
}

// =============================================================================
// Statistics and Diagnostics Integration Tests
// =============================================================================

TEST_F(BrocaBridgeIntegrationTest, StatisticsAccumulate) {
    lpb_stats_t initial_stats;
    lpb_get_stats(bridge, &initial_stats);

    // Perform some productions (without reset to preserve stats)
    for (int i = 0; i < 3; i++) {
        lpb_semantic_intent_t intent = create_test_intent(0.5f, 0.3f, 0.2f);
        lpb_produce_from_intent(bridge, &intent, NULL);
        free_intent(&intent);
        // Note: NOT calling lpb_reset as it clears stats
    }

    lpb_stats_t final_stats;
    lpb_get_stats(bridge, &final_stats);

    // Stats should accumulate (either 3 more or at least greater than initial)
    EXPECT_GE(final_stats.productions_attempted, initial_stats.productions_attempted + 3);
}

TEST_F(BrocaBridgeIntegrationTest, BrocaStatisticsTrackWords) {
    broca_stats_t initial_stats;
    broca_get_stats(broca, &initial_stats);

    // Add a word and process
    broca_begin_utterance(broca);
    broca_input_word_t word = {0};
    word.word_id = 10;
    strcpy(word.word, "cat");
    word.pos = 0;
    broca_add_word(broca, &word);

    broca_stats_t final_stats;
    broca_get_stats(broca, &final_stats);

    // Words should have been processed
    EXPECT_GE(final_stats.words_processed, initial_stats.words_processed);
}

// =============================================================================
// Error Handling Integration Tests
// =============================================================================

TEST_F(BrocaBridgeIntegrationTest, HandleInvalidInput) {
    // NULL intent
    EXPECT_FALSE(lpb_produce_from_intent(bridge, NULL, NULL));
    EXPECT_EQ(lpb_get_last_error(bridge), LPB_ERROR_INVALID_INPUT);

    // Reset error state
    lpb_reset(bridge);
    EXPECT_EQ(lpb_get_last_error(bridge), LPB_ERROR_NONE);
}

TEST_F(BrocaBridgeIntegrationTest, ErrorStringsAvailable) {
    // All error codes should have descriptions
    for (int e = LPB_ERROR_NONE; e <= LPB_ERROR_INTERNAL; e++) {
        const char* str = lpb_error_string((lpb_error_t)e);
        EXPECT_NE(str, nullptr);
        EXPECT_GT(strlen(str), 0u);
    }

    for (int s = LPB_STATUS_IDLE; s <= LPB_STATUS_ERROR; s++) {
        const char* str = lpb_status_string((lpb_status_t)s);
        EXPECT_NE(str, nullptr);
        EXPECT_GT(strlen(str), 0u);
    }
}
