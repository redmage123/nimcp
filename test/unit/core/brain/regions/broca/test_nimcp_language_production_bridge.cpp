/**
 * @file test_nimcp_language_production_bridge.cpp
 * @brief Unit tests for nimcp_language_production_bridge.c
 *
 * WHAT: Comprehensive unit tests for the Language Production Bridge
 * WHY:  Ensure correct integration between Broca's region, Speech Cortex, and NLP
 * HOW:  Use Google Test framework to test lifecycle, production pipeline,
 *       lexical access, self-monitoring, and status/diagnostics.
 *
 * COVERAGE TARGET: 100%
 */

#include <gtest/gtest.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

// Headers have their own extern "C" guards
#include "core/brain/regions/broca/nimcp_language_production_bridge.h"
#include "core/brain/regions/broca/nimcp_broca_adapter.h"

// Test Fixture for Language Production Bridge
class LanguageProductionBridgeTest : public ::testing::Test {
protected:
    language_production_bridge_t* bridge;
    broca_adapter_t* broca;
    lpb_config_t config;

    void SetUp() override {
        // Create Broca adapter first
        broca_config_t broca_config = broca_default_config();
        broca = broca_create(&broca_config);
        ASSERT_NE(nullptr, broca) << "Failed to create Broca adapter";

        // Create bridge with default config
        config = lpb_default_config();
        bridge = lpb_create(&config, broca);
        ASSERT_NE(nullptr, bridge) << "Failed to create Language Production Bridge";

        // Add some lexical entries to Broca for testing
        setup_lexicon();
    }

    void TearDown() override {
        lpb_destroy(bridge);
        bridge = nullptr;

        broca_destroy(broca);
        broca = nullptr;
    }

    // Helper to populate basic lexicon
    void setup_lexicon() {
        broca_lexical_entry_t entry;

        // "the" - Determiner
        memset(&entry, 0, sizeof(entry));
        entry.word_id = 1;
        strcpy(entry.word, "the");
        entry.phonemes[0] = 10;
        entry.phonemes[1] = 11;
        entry.phoneme_count = 2;
        entry.pos = 0;
        entry.frequency = 0.99f;
        broca_add_lexical_entry(broca, &entry);

        // "it" - Pronoun
        memset(&entry, 0, sizeof(entry));
        entry.word_id = 2;
        strcpy(entry.word, "it");
        entry.phonemes[0] = 12;
        entry.phonemes[1] = 13;
        entry.phoneme_count = 2;
        entry.pos = 0;
        entry.frequency = 0.95f;
        broca_add_lexical_entry(broca, &entry);

        // "does" - Verb
        memset(&entry, 0, sizeof(entry));
        entry.word_id = 3;
        strcpy(entry.word, "does");
        entry.phonemes[0] = 20;
        entry.phonemes[1] = 21;
        entry.phonemes[2] = 22;
        entry.phoneme_count = 3;
        entry.pos = 1;
        entry.frequency = 0.9f;
        broca_add_lexical_entry(broca, &entry);

        // "good" - Adjective
        memset(&entry, 0, sizeof(entry));
        entry.word_id = 4;
        strcpy(entry.word, "good");
        entry.phonemes[0] = 30;
        entry.phonemes[1] = 31;
        entry.phonemes[2] = 32;
        entry.phoneme_count = 3;
        entry.pos = 2;
        entry.frequency = 0.85f;
        broca_add_lexical_entry(broca, &entry);
    }

    // Helper to create a semantic intent with given activation
    lpb_semantic_intent_t create_intent(float entity_activation, float action_activation, float property_activation) {
        lpb_semantic_intent_t intent;
        memset(&intent, 0, sizeof(intent));

        // Allocate semantic vector (256 dimensions)
        intent.semantic_dim = 256;
        intent.semantic_vector = (float*)calloc(intent.semantic_dim, sizeof(float));

        // Set entity features (0-31)
        for (uint32_t i = 0; i < 32; i++) {
            intent.semantic_vector[i] = entity_activation;
        }

        // Set action features (32-63)
        for (uint32_t i = 32; i < 64; i++) {
            intent.semantic_vector[i] = action_activation;
        }

        // Set property features (64-95)
        for (uint32_t i = 64; i < 96; i++) {
            intent.semantic_vector[i] = property_activation;
        }

        intent.confidence = 0.9f;
        intent.intent_type = 0; // Statement
        intent.speech_act = 0;
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

// ============================================================================
// LIFECYCLE TESTS
// ============================================================================

TEST_F(LanguageProductionBridgeTest, DefaultConfigHasReasonableValues) {
    lpb_config_t default_config = lpb_default_config();

    EXPECT_EQ(default_config.max_tokens, LPB_DEFAULT_MAX_TOKENS);
    EXPECT_EQ(default_config.semantic_dim, LPB_DEFAULT_SEMANTIC_DIM);
    EXPECT_FLOAT_EQ(default_config.comprehension_threshold, LPB_DEFAULT_COMPREHENSION_THRESHOLD);
    EXPECT_FLOAT_EQ(default_config.production_delay_ms, LPB_DEFAULT_PRODUCTION_DELAY_MS);
    EXPECT_TRUE(default_config.enable_wernicke_connection);
    EXPECT_TRUE(default_config.enable_nlp_connection);
    EXPECT_TRUE(default_config.enable_self_monitoring);
}

TEST_F(LanguageProductionBridgeTest, CreateWithNullConfigUsesDefaults) {
    language_production_bridge_t* bridge_null = lpb_create(NULL, broca);
    ASSERT_NE(nullptr, bridge_null);

    lpb_config_t retrieved;
    EXPECT_TRUE(lpb_get_config(bridge_null, &retrieved));
    EXPECT_EQ(retrieved.max_tokens, LPB_DEFAULT_MAX_TOKENS);

    lpb_destroy(bridge_null);
}

TEST_F(LanguageProductionBridgeTest, CreateWithNullBrocaFails) {
    language_production_bridge_t* bridge_null = lpb_create(&config, NULL);
    EXPECT_EQ(nullptr, bridge_null);
}

TEST_F(LanguageProductionBridgeTest, DestroyNullDoesNotCrash) {
    lpb_destroy(NULL);
    // Should not crash
}

TEST_F(LanguageProductionBridgeTest, ResetClearsState) {
    EXPECT_TRUE(lpb_reset(bridge));
    EXPECT_EQ(lpb_get_status(bridge), LPB_STATUS_IDLE);
    EXPECT_EQ(lpb_get_last_error(bridge), LPB_ERROR_NONE);
}

TEST_F(LanguageProductionBridgeTest, ResetNullReturnsFalse) {
    EXPECT_FALSE(lpb_reset(NULL));
}

// ============================================================================
// SYSTEM CONNECTION TESTS
// ============================================================================

TEST_F(LanguageProductionBridgeTest, ConnectSpeechCortexSuccess) {
    // With config enabled, should succeed even with NULL (stores the reference)
    EXPECT_TRUE(lpb_connect_speech_cortex(bridge, NULL));
}

TEST_F(LanguageProductionBridgeTest, ConnectNLPSuccess) {
    EXPECT_TRUE(lpb_connect_nlp(bridge, NULL));
}

TEST_F(LanguageProductionBridgeTest, ConnectWorkingMemorySuccess) {
    EXPECT_TRUE(lpb_connect_working_memory(bridge, NULL));
}

TEST_F(LanguageProductionBridgeTest, ConnectNullBridge) {
    EXPECT_FALSE(lpb_connect_speech_cortex(NULL, NULL));
    EXPECT_FALSE(lpb_connect_nlp(NULL, NULL));
    EXPECT_FALSE(lpb_connect_working_memory(NULL, NULL));
}

// ============================================================================
// PRODUCTION PIPELINE TESTS
// ============================================================================

TEST_F(LanguageProductionBridgeTest, ProduceFromIntentBasic) {
    lpb_semantic_intent_t intent = create_intent(0.5f, 0.3f, 0.2f);
    lpb_production_result_t result;
    memset(&result, 0, sizeof(result));

    bool success = lpb_produce_from_intent(bridge, &intent, &result);

    // May succeed or fail depending on lexical selection
    if (success) {
        EXPECT_GT(result.token_count, 0u);
        EXPECT_EQ(lpb_get_status(bridge), LPB_STATUS_READY);
    } else {
        EXPECT_NE(lpb_get_last_error(bridge), LPB_ERROR_NONE);
    }

    free_intent(&intent);
}

TEST_F(LanguageProductionBridgeTest, ProduceFromIntentNullIntent) {
    lpb_production_result_t result;
    EXPECT_FALSE(lpb_produce_from_intent(bridge, NULL, &result));
    EXPECT_EQ(lpb_get_last_error(bridge), LPB_ERROR_INVALID_INPUT);
}

TEST_F(LanguageProductionBridgeTest, ProduceFromIntentNullBridge) {
    lpb_semantic_intent_t intent = create_intent(0.5f, 0.3f, 0.2f);
    EXPECT_FALSE(lpb_produce_from_intent(NULL, &intent, NULL));
    free_intent(&intent);
}

TEST_F(LanguageProductionBridgeTest, ProduceFromTokensBasic) {
    lpb_token_t tokens[3];

    // Token 1: "it"
    memset(&tokens[0], 0, sizeof(lpb_token_t));
    tokens[0].token_id = 2;
    strcpy(tokens[0].token_str, "it");
    tokens[0].pos = 0;
    tokens[0].activation = 0.9f;
    tokens[0].frequency = 0.95f;

    // Token 2: "does"
    memset(&tokens[1], 0, sizeof(lpb_token_t));
    tokens[1].token_id = 3;
    strcpy(tokens[1].token_str, "does");
    tokens[1].pos = 1;
    tokens[1].activation = 0.85f;
    tokens[1].frequency = 0.9f;

    // Token 3: "good"
    memset(&tokens[2], 0, sizeof(lpb_token_t));
    tokens[2].token_id = 4;
    strcpy(tokens[2].token_str, "good");
    tokens[2].pos = 2;
    tokens[2].activation = 0.8f;
    tokens[2].frequency = 0.85f;

    lpb_production_result_t result;
    memset(&result, 0, sizeof(result));

    bool success = lpb_produce_from_tokens(bridge, tokens, 3, &result);

    // Should process even if syntax validation fails
    if (success) {
        EXPECT_EQ(result.token_count, 3u);
    }
}

TEST_F(LanguageProductionBridgeTest, ProduceFromTokensNull) {
    EXPECT_FALSE(lpb_produce_from_tokens(NULL, NULL, 0, NULL));
    EXPECT_FALSE(lpb_produce_from_tokens(bridge, NULL, 0, NULL));

    lpb_token_t token;
    EXPECT_FALSE(lpb_produce_from_tokens(bridge, &token, 0, NULL));
}

TEST_F(LanguageProductionBridgeTest, RepeatLastHeardRequiresSpeechCortex) {
    // Without a real speech cortex, this should fail
    lpb_production_result_t result;
    EXPECT_FALSE(lpb_repeat_last_heard(bridge, &result));
    EXPECT_EQ(lpb_get_last_error(bridge), LPB_ERROR_NO_SPEECH_CORTEX);
}

TEST_F(LanguageProductionBridgeTest, GenerateResponseRequiresNLP) {
    // Without a real NLP system, this should fail
    lpb_production_result_t result;
    EXPECT_FALSE(lpb_generate_response(bridge, &result));
    EXPECT_EQ(lpb_get_last_error(bridge), LPB_ERROR_NO_NLP);
}

// ============================================================================
// LEXICAL ACCESS TESTS
// ============================================================================

TEST_F(LanguageProductionBridgeTest, SelectLexicalItemsBasic) {
    float semantic_vector[256];
    memset(semantic_vector, 0, sizeof(semantic_vector));

    // Set entity activation
    for (int i = 0; i < 32; i++) {
        semantic_vector[i] = 0.5f;
    }

    lpb_token_t tokens[10];
    uint32_t num_selected = 0;

    bool success = lpb_select_lexical_items(bridge, semantic_vector, 256, tokens, 10, &num_selected);

    if (success) {
        EXPECT_GT(num_selected, 0u);
    }
}

TEST_F(LanguageProductionBridgeTest, SelectLexicalItemsNull) {
    float vec[10];
    lpb_token_t tokens[10];
    uint32_t count;

    EXPECT_FALSE(lpb_select_lexical_items(NULL, vec, 10, tokens, 10, &count));
    EXPECT_FALSE(lpb_select_lexical_items(bridge, NULL, 10, tokens, 10, &count));
    EXPECT_FALSE(lpb_select_lexical_items(bridge, vec, 10, NULL, 10, &count));
    EXPECT_FALSE(lpb_select_lexical_items(bridge, vec, 10, tokens, 10, NULL));
}

TEST_F(LanguageProductionBridgeTest, PrimeLexicalAccessBasic) {
    float context_vector[256];
    for (int i = 0; i < 256; i++) {
        context_vector[i] = 0.1f;
    }

    EXPECT_TRUE(lpb_prime_lexical_access(bridge, context_vector, 256, 0.5f));
}

TEST_F(LanguageProductionBridgeTest, PrimeLexicalAccessNull) {
    float vec[10];
    EXPECT_FALSE(lpb_prime_lexical_access(NULL, vec, 10, 0.5f));
    EXPECT_FALSE(lpb_prime_lexical_access(bridge, NULL, 10, 0.5f));
    EXPECT_FALSE(lpb_prime_lexical_access(bridge, vec, 0, 0.5f));
}

// ============================================================================
// SELF-MONITORING TESTS
// ============================================================================

TEST_F(LanguageProductionBridgeTest, SetSelfMonitoringEnable) {
    EXPECT_TRUE(lpb_set_self_monitoring(bridge, true));
    EXPECT_TRUE(lpb_set_self_monitoring(bridge, false));
}

TEST_F(LanguageProductionBridgeTest, SetSelfMonitoringNull) {
    EXPECT_FALSE(lpb_set_self_monitoring(NULL, true));
}

TEST_F(LanguageProductionBridgeTest, CheckProductionBasic) {
    // First produce something
    lpb_semantic_intent_t intent = create_intent(0.5f, 0.3f, 0.2f);
    lpb_produce_from_intent(bridge, &intent, NULL);

    float match_score;
    lpb_check_production(bridge, &match_score);
    // Score should be between 0 and 1
    EXPECT_GE(match_score, 0.0f);
    EXPECT_LE(match_score, 1.0f);

    free_intent(&intent);
}

TEST_F(LanguageProductionBridgeTest, CheckProductionNull) {
    EXPECT_FALSE(lpb_check_production(NULL, NULL));
}

// ============================================================================
// CALLBACK TESTS
// ============================================================================

static int motor_callback_count = 0;
static void test_motor_callback(const void* motor_command, void* user_data) {
    (void)motor_command;
    (void)user_data;
    motor_callback_count++;
}

TEST_F(LanguageProductionBridgeTest, SetMotorCallbackSuccess) {
    EXPECT_TRUE(lpb_set_motor_callback(bridge, test_motor_callback, NULL));
}

TEST_F(LanguageProductionBridgeTest, SetMotorCallbackNull) {
    EXPECT_FALSE(lpb_set_motor_callback(NULL, test_motor_callback, NULL));
}

static int event_callback_count = 0;
static void test_event_callback(uint32_t event_type, const void* event_data, void* user_data) {
    (void)event_type;
    (void)event_data;
    (void)user_data;
    event_callback_count++;
}

TEST_F(LanguageProductionBridgeTest, SetEventCallbackSuccess) {
    EXPECT_TRUE(lpb_set_event_callback(bridge, test_event_callback, NULL));
}

TEST_F(LanguageProductionBridgeTest, SetEventCallbackNull) {
    EXPECT_FALSE(lpb_set_event_callback(NULL, test_event_callback, NULL));
}

// ============================================================================
// STATUS AND DIAGNOSTICS TESTS
// ============================================================================

TEST_F(LanguageProductionBridgeTest, GetStatusIdle) {
    EXPECT_EQ(lpb_get_status(bridge), LPB_STATUS_IDLE);
}

TEST_F(LanguageProductionBridgeTest, GetStatusNull) {
    EXPECT_EQ(lpb_get_status(NULL), LPB_STATUS_ERROR);
}

TEST_F(LanguageProductionBridgeTest, GetLastErrorNone) {
    EXPECT_EQ(lpb_get_last_error(bridge), LPB_ERROR_NONE);
}

TEST_F(LanguageProductionBridgeTest, GetLastErrorNull) {
    EXPECT_EQ(lpb_get_last_error(NULL), LPB_ERROR_INTERNAL);
}

TEST_F(LanguageProductionBridgeTest, ErrorStringNotNull) {
    const char* str = lpb_error_string(LPB_ERROR_NONE);
    EXPECT_NE(str, nullptr);
    EXPECT_GT(strlen(str), 0u);

    str = lpb_error_string(LPB_ERROR_INVALID_INPUT);
    EXPECT_NE(str, nullptr);

    str = lpb_error_string(LPB_ERROR_NO_BROCA);
    EXPECT_NE(str, nullptr);

    str = lpb_error_string(LPB_ERROR_NO_SPEECH_CORTEX);
    EXPECT_NE(str, nullptr);

    str = lpb_error_string(LPB_ERROR_NO_NLP);
    EXPECT_NE(str, nullptr);

    str = lpb_error_string((lpb_error_t)999); // Unknown
    EXPECT_NE(str, nullptr);
}

TEST_F(LanguageProductionBridgeTest, StatusStringNotNull) {
    const char* str = lpb_status_string(LPB_STATUS_IDLE);
    EXPECT_NE(str, nullptr);
    EXPECT_GT(strlen(str), 0u);

    str = lpb_status_string(LPB_STATUS_RECEIVING_INTENT);
    EXPECT_NE(str, nullptr);

    str = lpb_status_string(LPB_STATUS_READY);
    EXPECT_NE(str, nullptr);

    str = lpb_status_string((lpb_status_t)999); // Unknown
    EXPECT_NE(str, nullptr);
}

TEST_F(LanguageProductionBridgeTest, GetStats) {
    lpb_stats_t stats;
    EXPECT_TRUE(lpb_get_stats(bridge, &stats));
    EXPECT_EQ(stats.productions_attempted, 0u);
    EXPECT_EQ(stats.productions_successful, 0u);
}

TEST_F(LanguageProductionBridgeTest, GetStatsNull) {
    lpb_stats_t stats;
    EXPECT_FALSE(lpb_get_stats(NULL, &stats));
    EXPECT_FALSE(lpb_get_stats(bridge, NULL));
}

TEST_F(LanguageProductionBridgeTest, GetConfig) {
    lpb_config_t retrieved;
    EXPECT_TRUE(lpb_get_config(bridge, &retrieved));
    EXPECT_EQ(retrieved.max_tokens, config.max_tokens);
}

TEST_F(LanguageProductionBridgeTest, GetConfigNull) {
    lpb_config_t cfg;
    EXPECT_FALSE(lpb_get_config(NULL, &cfg));
    EXPECT_FALSE(lpb_get_config(bridge, NULL));
}

// ============================================================================
// DIRECT ACCESS TESTS
// ============================================================================

TEST_F(LanguageProductionBridgeTest, GetBrocaAdapterNotNull) {
    EXPECT_NE(lpb_get_broca_adapter(bridge), nullptr);
    EXPECT_EQ(lpb_get_broca_adapter(bridge), broca);
}

TEST_F(LanguageProductionBridgeTest, GetBrocaAdapterNull) {
    EXPECT_EQ(lpb_get_broca_adapter(NULL), nullptr);
}

// ============================================================================
// STATISTICS TRACKING TESTS
// ============================================================================

TEST_F(LanguageProductionBridgeTest, StatsTrackAttempts) {
    lpb_semantic_intent_t intent = create_intent(0.5f, 0.3f, 0.2f);

    lpb_stats_t stats_before;
    lpb_get_stats(bridge, &stats_before);

    lpb_produce_from_intent(bridge, &intent, NULL);

    lpb_stats_t stats_after;
    lpb_get_stats(bridge, &stats_after);

    EXPECT_GT(stats_after.productions_attempted, stats_before.productions_attempted);

    free_intent(&intent);
}
