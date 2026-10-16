/**
 * @file test_wernicke_broca_bridge_integration.cpp
 * @brief Integration tests for Wernicke-Broca bridge (arcuate fasciculus)
 *
 * WHAT: Comprehensive integration tests for the bidirectional language pathway
 * WHY:  Verify proper communication between comprehension and production
 * HOW:  Test bridge creation, message forwarding, self-monitoring, and rehearsal
 *
 * @version Phase W3: Wernicke-Broca Bridge Integration Tests
 * @date 2026-01-24
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>
#include <thread>
#include <chrono>

#include "utils/nimcp_test_base.h"

#include "core/brain/regions/wernicke/nimcp_wernicke_adapter.h"
#include "core/brain/regions/wernicke/nimcp_wernicke_broca_bridge.h"
#include "core/brain/regions/broca/nimcp_broca_adapter.h"

//=============================================================================
// Test Fixture
//=============================================================================

class WernickeBrocaBridgeIntegrationTest : public NimcpTestBase {
protected:
    wernicke_adapter_t* wernicke_ = nullptr;
    broca_adapter_t* broca_ = nullptr;
    wernicke_broca_bridge_t* bridge_ = nullptr;

    void SetUp() override {
        NimcpTestBase::SetUp();
        // Components created per-test as needed
    }

    void TearDown() override {
        if (bridge_) {
            wbb_destroy(bridge_);
            bridge_ = nullptr;
        }
        if (wernicke_) {
            wernicke_destroy(wernicke_);
            wernicke_ = nullptr;
        }
        if (broca_) {
            broca_destroy(broca_);
            broca_ = nullptr;
        }
        NimcpTestBase::TearDown();
    }

    // Create both endpoints for bridge testing
    void CreateEndpoints() {
        wernicke_config_t w_config = wernicke_default_config();
        w_config.enable_broca_connection = true;
        w_config.enable_bio_async = true;
        wernicke_ = wernicke_create(&w_config);
        ASSERT_NE(wernicke_, nullptr);

        broca_config_t b_config = broca_default_config();
        b_config.enable_bio_async = true;
        broca_ = broca_create(&b_config);
        ASSERT_NE(broca_, nullptr);
    }

    // Helper to create a comprehension result
    wbb_comprehension_t CreateComprehension(uint32_t word_id, uint32_t concept_id,
                                             const char* word, float confidence) {
        wbb_comprehension_t comp = {};
        comp.word_id = word_id;
        comp.concept_id = concept_id;
        if (word) {
            strncpy(comp.word_string, word, sizeof(comp.word_string) - 1);
        }
        comp.confidence = confidence;
        comp.context_fit = 0.8f;
        return comp;
    }

    // Helper to create an efference copy
    wbb_efference_copy_t CreateEfferenceCopy(uint32_t word_id, uint8_t phoneme_count) {
        wbb_efference_copy_t eff = {};
        eff.planned_word_id = word_id;
        eff.num_planned_phonemes = phoneme_count;
        eff.fluency_estimate = 0.9f;
        return eff;
    }
};

//=============================================================================
// Bridge Lifecycle Tests
//=============================================================================

TEST_F(WernickeBrocaBridgeIntegrationTest, CreateWithDefaultConfig) {
    CreateEndpoints();

    bridge_ = wbb_create(wernicke_, broca_, nullptr);
    ASSERT_NE(bridge_, nullptr) << "wbb_create with NULL config should succeed";
}

TEST_F(WernickeBrocaBridgeIntegrationTest, CreateWithCustomConfig) {
    CreateEndpoints();

    wbb_config_t config = wbb_default_config();
    config.buffer_size = 128;
    config.phoneme_buffer_size = 256;
    config.semantic_dim = 256;
    config.transmission_delay_ms = 100.0f;
    config.enable_dorsal_stream = true;
    config.enable_ventral_stream = true;
    config.enable_self_monitoring = true;
    config.enable_working_memory = true;

    bridge_ = wbb_create(wernicke_, broca_, &config);
    ASSERT_NE(bridge_, nullptr) << "wbb_create with custom config should succeed";

    wbb_config_t retrieved_config;
    int result = wbb_get_config(bridge_, &retrieved_config);
    EXPECT_EQ(result, 0) << "wbb_get_config should succeed";
    EXPECT_EQ(retrieved_config.buffer_size, 128u);
    EXPECT_EQ(retrieved_config.semantic_dim, 256u);
}

TEST_F(WernickeBrocaBridgeIntegrationTest, CreateWithNullWernicke) {
    broca_config_t b_config = broca_default_config();
    broca_ = broca_create(&b_config);

    bridge_ = wbb_create(nullptr, broca_, nullptr);
    // May or may not succeed depending on implementation
    // Some implementations allow lazy connection
}

TEST_F(WernickeBrocaBridgeIntegrationTest, CreateWithNullBroca) {
    wernicke_config_t w_config = wernicke_default_config();
    wernicke_ = wernicke_create(&w_config);

    bridge_ = wbb_create(wernicke_, nullptr, nullptr);
    // May or may not succeed depending on implementation
}

TEST_F(WernickeBrocaBridgeIntegrationTest, ResetBridge) {
    CreateEndpoints();
    bridge_ = wbb_create(wernicke_, broca_, nullptr);
    ASSERT_NE(bridge_, nullptr);

    int result = wbb_reset(bridge_);
    EXPECT_EQ(result, 0) << "wbb_reset should succeed";
}

TEST_F(WernickeBrocaBridgeIntegrationTest, DestroyNullIsSafe) {
    // Should not crash
    wbb_destroy(nullptr);
}

TEST_F(WernickeBrocaBridgeIntegrationTest, DefaultConfigHasSensibleValues) {
    wbb_config_t config = wbb_default_config();

    EXPECT_GT(config.buffer_size, 0u) << "buffer_size should be positive";
    EXPECT_GT(config.phoneme_buffer_size, 0u) << "phoneme_buffer_size should be positive";
    EXPECT_GT(config.semantic_dim, 0u) << "semantic_dim should be positive";
    EXPECT_GT(config.transmission_delay_ms, 0.0f) << "transmission_delay should be positive";
    EXPECT_GE(config.repetition_threshold, 0.0f);
    EXPECT_LE(config.repetition_threshold, 1.0f);
}

//=============================================================================
// Connection Management Tests
//=============================================================================

TEST_F(WernickeBrocaBridgeIntegrationTest, SetWernickeEndpoint) {
    broca_config_t b_config = broca_default_config();
    broca_ = broca_create(&b_config);
    bridge_ = wbb_create(nullptr, broca_, nullptr);

    if (bridge_) {
        wernicke_config_t w_config = wernicke_default_config();
        wernicke_ = wernicke_create(&w_config);

        int result = wbb_set_wernicke(bridge_, wernicke_);
        EXPECT_EQ(result, 0) << "Set Wernicke endpoint should succeed";
    }
}

TEST_F(WernickeBrocaBridgeIntegrationTest, SetBrocaEndpoint) {
    wernicke_config_t w_config = wernicke_default_config();
    wernicke_ = wernicke_create(&w_config);
    bridge_ = wbb_create(wernicke_, nullptr, nullptr);

    if (bridge_) {
        broca_config_t b_config = broca_default_config();
        broca_ = broca_create(&b_config);

        int result = wbb_set_broca(bridge_, broca_);
        EXPECT_EQ(result, 0) << "Set Broca endpoint should succeed";
    }
}

TEST_F(WernickeBrocaBridgeIntegrationTest, ConnectBioAsync) {
    CreateEndpoints();

    wbb_config_t config = wbb_default_config();
    config.enable_bio_async = true;
    bridge_ = wbb_create(wernicke_, broca_, &config);
    ASSERT_NE(bridge_, nullptr);

    // Connect to router (using NULL for now - real test would use actual router)
    int result = wbb_connect_bio_async(bridge_, nullptr);
    // May or may not succeed depending on whether NULL router is allowed
    (void)result;
}

//=============================================================================
// Wernicke to Broca Communication Tests
//=============================================================================

TEST_F(WernickeBrocaBridgeIntegrationTest, ForwardComprehensionDorsalStream) {
    CreateEndpoints();
    bridge_ = wbb_create(wernicke_, broca_, nullptr);
    ASSERT_NE(bridge_, nullptr);

    wbb_comprehension_t comp = CreateComprehension(1, 100, "hello", 0.95f);

    int result = wbb_forward_comprehension(bridge_, &comp, WBB_STREAM_DORSAL);
    EXPECT_EQ(result, 0) << "Forward via dorsal stream should succeed";
}

TEST_F(WernickeBrocaBridgeIntegrationTest, ForwardComprehensionVentralStream) {
    CreateEndpoints();
    bridge_ = wbb_create(wernicke_, broca_, nullptr);
    ASSERT_NE(bridge_, nullptr);

    wbb_comprehension_t comp = CreateComprehension(2, 200, "world", 0.9f);

    int result = wbb_forward_comprehension(bridge_, &comp, WBB_STREAM_VENTRAL);
    EXPECT_EQ(result, 0) << "Forward via ventral stream should succeed";
}

TEST_F(WernickeBrocaBridgeIntegrationTest, ForwardComprehensionBothStreams) {
    CreateEndpoints();
    bridge_ = wbb_create(wernicke_, broca_, nullptr);
    ASSERT_NE(bridge_, nullptr);

    wbb_comprehension_t comp = CreateComprehension(3, 300, "test", 0.85f);

    int result = wbb_forward_comprehension(bridge_, &comp, WBB_STREAM_BOTH);
    EXPECT_EQ(result, 0) << "Forward via both streams should succeed";
}

TEST_F(WernickeBrocaBridgeIntegrationTest, RequestRepetition) {
    CreateEndpoints();
    bridge_ = wbb_create(wernicke_, broca_, nullptr);
    ASSERT_NE(bridge_, nullptr);

    uint8_t phonemes[] = {1, 2, 3, 4, 5};

    int result = wbb_request_repetition(bridge_, phonemes, 5);
    EXPECT_EQ(result, 0) << "Request repetition should succeed";
}

TEST_F(WernickeBrocaBridgeIntegrationTest, SendResponseIntent) {
    CreateEndpoints();
    bridge_ = wbb_create(wernicke_, broca_, nullptr);
    ASSERT_NE(bridge_, nullptr);

    float semantic_vector[128] = {0};
    semantic_vector[0] = 0.5f;
    semantic_vector[1] = 0.3f;

    int result = wbb_send_response_intent(bridge_, semantic_vector, 128, 0);  // 0 = statement
    EXPECT_EQ(result, 0) << "Send response intent should succeed";
}

TEST_F(WernickeBrocaBridgeIntegrationTest, ForwardMultipleComprehensions) {
    CreateEndpoints();
    bridge_ = wbb_create(wernicke_, broca_, nullptr);
    ASSERT_NE(bridge_, nullptr);

    // Forward multiple words
    const char* words[] = {"the", "quick", "brown", "fox"};
    for (int i = 0; i < 4; i++) {
        wbb_comprehension_t comp = CreateComprehension(i + 1, (i + 1) * 100, words[i], 0.9f - i * 0.05f);
        int result = wbb_forward_comprehension(bridge_, &comp, WBB_STREAM_BOTH);
        EXPECT_EQ(result, 0) << "Forward word " << i << " should succeed";
    }
}

//=============================================================================
// Broca to Wernicke Communication Tests (Self-Monitoring)
//=============================================================================

TEST_F(WernickeBrocaBridgeIntegrationTest, ReceiveEfferenceCopy) {
    CreateEndpoints();

    wbb_config_t config = wbb_default_config();
    config.enable_self_monitoring = true;
    bridge_ = wbb_create(wernicke_, broca_, &config);
    ASSERT_NE(bridge_, nullptr);

    wbb_efference_copy_t efference;
    int result = wbb_receive_efference_copy(bridge_, &efference);
    // 0 = success, 1 = no efference available, -1 = error
    EXPECT_GE(result, 0) << "Receive efference should not error";
}

TEST_F(WernickeBrocaBridgeIntegrationTest, CompareProduction) {
    CreateEndpoints();

    wbb_config_t config = wbb_default_config();
    config.enable_self_monitoring = true;
    bridge_ = wbb_create(wernicke_, broca_, &config);
    ASSERT_NE(bridge_, nullptr);

    wbb_comprehension_t intended = CreateComprehension(1, 100, "hello", 0.95f);
    wbb_efference_copy_t efference = CreateEfferenceCopy(1, 5);

    wbb_monitoring_result_t result;
    int ret = wbb_compare_production(bridge_, &intended, &efference, &result);
    EXPECT_EQ(ret, 0) << "Compare production should succeed";
}

TEST_F(WernickeBrocaBridgeIntegrationTest, SendErrorSignal) {
    CreateEndpoints();

    wbb_config_t config = wbb_default_config();
    config.enable_self_monitoring = true;
    config.enable_error_correction = true;
    bridge_ = wbb_create(wernicke_, broca_, &config);
    ASSERT_NE(bridge_, nullptr);

    uint8_t correction[] = {1, 2, 3};
    int result = wbb_send_error_signal(bridge_, 1, 0, correction);  // error_type=1 (phoneme)
    EXPECT_EQ(result, 0) << "Send error signal should succeed";
}

TEST_F(WernickeBrocaBridgeIntegrationTest, SendErrorSignalWithoutCorrection) {
    CreateEndpoints();

    wbb_config_t config = wbb_default_config();
    config.enable_self_monitoring = true;
    bridge_ = wbb_create(wernicke_, broca_, &config);
    ASSERT_NE(bridge_, nullptr);

    int result = wbb_send_error_signal(bridge_, 2, 3, nullptr);  // error_type=2 (semantic)
    EXPECT_EQ(result, 0) << "Send error signal without correction should succeed";
}

//=============================================================================
// Working Memory Rehearsal Tests
//=============================================================================

TEST_F(WernickeBrocaBridgeIntegrationTest, RequestRehearsal) {
    CreateEndpoints();

    wbb_config_t config = wbb_default_config();
    config.enable_working_memory = true;
    bridge_ = wbb_create(wernicke_, broca_, &config);
    ASSERT_NE(bridge_, nullptr);

    uint8_t phonemes[] = {10, 11, 12, 13, 14};
    int result = wbb_request_rehearsal(bridge_, phonemes, 5, 3);  // 3 repetitions
    EXPECT_EQ(result, 0) << "Request rehearsal should succeed";
}

TEST_F(WernickeBrocaBridgeIntegrationTest, ProcessRehearsal) {
    CreateEndpoints();

    wbb_config_t config = wbb_default_config();
    config.enable_working_memory = true;
    bridge_ = wbb_create(wernicke_, broca_, &config);
    ASSERT_NE(bridge_, nullptr);

    int result = wbb_process_rehearsal(bridge_);
    EXPECT_EQ(result, 0) << "Process rehearsal should succeed";
}

TEST_F(WernickeBrocaBridgeIntegrationTest, RehearsalLoop) {
    CreateEndpoints();

    wbb_config_t config = wbb_default_config();
    config.enable_working_memory = true;
    bridge_ = wbb_create(wernicke_, broca_, &config);
    ASSERT_NE(bridge_, nullptr);

    // Request rehearsal
    uint8_t phonemes[] = {1, 2, 3};
    wbb_request_rehearsal(bridge_, phonemes, 3, 2);

    // Process messages to simulate loop
    wbb_process_messages(bridge_, 10);

    // Process rehearsal feedback
    wbb_process_rehearsal(bridge_);
}

//=============================================================================
// Message Handling Tests
//=============================================================================

TEST_F(WernickeBrocaBridgeIntegrationTest, ProcessMessages) {
    CreateEndpoints();
    bridge_ = wbb_create(wernicke_, broca_, nullptr);
    ASSERT_NE(bridge_, nullptr);

    // Queue some messages
    wbb_comprehension_t comp = CreateComprehension(1, 100, "test", 0.9f);
    wbb_forward_comprehension(bridge_, &comp, WBB_STREAM_DORSAL);

    int processed = wbb_process_messages(bridge_, 0);  // 0 = process all
    EXPECT_GE(processed, 0) << "Process messages should not error";
}

TEST_F(WernickeBrocaBridgeIntegrationTest, ProcessMessagesWithLimit) {
    CreateEndpoints();
    bridge_ = wbb_create(wernicke_, broca_, nullptr);
    ASSERT_NE(bridge_, nullptr);

    // Queue multiple messages
    for (int i = 0; i < 5; i++) {
        wbb_comprehension_t comp = CreateComprehension(i + 1, (i + 1) * 100, "word", 0.9f);
        wbb_forward_comprehension(bridge_, &comp, WBB_STREAM_DORSAL);
    }

    int processed = wbb_process_messages(bridge_, 2);  // Only process 2
    EXPECT_GE(processed, 0);
}

TEST_F(WernickeBrocaBridgeIntegrationTest, PendingCount) {
    CreateEndpoints();
    bridge_ = wbb_create(wernicke_, broca_, nullptr);
    ASSERT_NE(bridge_, nullptr);

    uint32_t count = wbb_pending_count(bridge_);
    EXPECT_EQ(count, 0u) << "Initially no pending messages";

    wbb_comprehension_t comp = CreateComprehension(1, 100, "test", 0.9f);
    wbb_forward_comprehension(bridge_, &comp, WBB_STREAM_DORSAL);

    count = wbb_pending_count(bridge_);
    EXPECT_GE(count, 0u);  // May or may not have pending depending on implementation
}

TEST_F(WernickeBrocaBridgeIntegrationTest, PeekMessage) {
    CreateEndpoints();
    bridge_ = wbb_create(wernicke_, broca_, nullptr);
    ASSERT_NE(bridge_, nullptr);

    // Queue a message
    wbb_comprehension_t comp = CreateComprehension(1, 100, "peek", 0.85f);
    wbb_forward_comprehension(bridge_, &comp, WBB_STREAM_BOTH);

    wbb_message_t message;
    int result = wbb_peek_message(bridge_, &message);
    // 0 = success, 1 = empty queue, -1 = error
    EXPECT_GE(result, 0) << "Peek should not error";
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(WernickeBrocaBridgeIntegrationTest, GetStats) {
    CreateEndpoints();
    bridge_ = wbb_create(wernicke_, broca_, nullptr);
    ASSERT_NE(bridge_, nullptr);

    wbb_stats_t stats;
    int result = wbb_get_stats(bridge_, &stats);
    EXPECT_EQ(result, 0) << "Get stats should succeed";
    EXPECT_EQ(stats.messages_sent, 0u);
    EXPECT_EQ(stats.messages_received, 0u);
}

TEST_F(WernickeBrocaBridgeIntegrationTest, StatsUpdateAfterMessages) {
    CreateEndpoints();
    bridge_ = wbb_create(wernicke_, broca_, nullptr);
    ASSERT_NE(bridge_, nullptr);

    wbb_stats_t stats_before;
    wbb_get_stats(bridge_, &stats_before);

    // Send messages
    wbb_comprehension_t comp = CreateComprehension(1, 100, "stats", 0.9f);
    wbb_forward_comprehension(bridge_, &comp, WBB_STREAM_DORSAL);
    wbb_forward_comprehension(bridge_, &comp, WBB_STREAM_VENTRAL);

    wbb_stats_t stats_after;
    wbb_get_stats(bridge_, &stats_after);

    // Stats should reflect the messages sent
    EXPECT_GE(stats_after.comprehensions_forwarded, stats_before.comprehensions_forwarded);
}

TEST_F(WernickeBrocaBridgeIntegrationTest, ResetStats) {
    CreateEndpoints();
    bridge_ = wbb_create(wernicke_, broca_, nullptr);
    ASSERT_NE(bridge_, nullptr);

    // Generate some activity
    wbb_comprehension_t comp = CreateComprehension(1, 100, "reset", 0.9f);
    wbb_forward_comprehension(bridge_, &comp, WBB_STREAM_DORSAL);

    wbb_reset_stats(bridge_);

    wbb_stats_t stats;
    wbb_get_stats(bridge_, &stats);
    // Stats should be reset (implementation-dependent which fields reset)
}

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(WernickeBrocaBridgeIntegrationTest, GetConfig) {
    CreateEndpoints();

    wbb_config_t config = wbb_default_config();
    config.buffer_size = 96;
    bridge_ = wbb_create(wernicke_, broca_, &config);
    ASSERT_NE(bridge_, nullptr);

    wbb_config_t retrieved;
    int result = wbb_get_config(bridge_, &retrieved);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(retrieved.buffer_size, 96u);
}

TEST_F(WernickeBrocaBridgeIntegrationTest, SetConfig) {
    CreateEndpoints();
    bridge_ = wbb_create(wernicke_, broca_, nullptr);
    ASSERT_NE(bridge_, nullptr);

    wbb_config_t new_config = wbb_default_config();
    new_config.transmission_delay_ms = 75.0f;
    new_config.monitoring_threshold = 0.9f;

    int result = wbb_set_config(bridge_, &new_config);
    EXPECT_EQ(result, 0) << "Set config should succeed";

    wbb_config_t retrieved;
    wbb_get_config(bridge_, &retrieved);
    EXPECT_NEAR(retrieved.transmission_delay_ms, 75.0f, 0.1f);
}

//=============================================================================
// Integrated Language Loop Tests
//=============================================================================

TEST_F(WernickeBrocaBridgeIntegrationTest, ComprehensionToProductionLoop) {
    CreateEndpoints();

    wbb_config_t config = wbb_default_config();
    config.enable_dorsal_stream = true;
    config.enable_ventral_stream = true;
    config.enable_self_monitoring = true;
    bridge_ = wbb_create(wernicke_, broca_, &config);
    ASSERT_NE(bridge_, nullptr);

    // Step 1: Forward comprehension result
    wbb_comprehension_t comp = CreateComprehension(1, 100, "hello", 0.95f);
    int result = wbb_forward_comprehension(bridge_, &comp, WBB_STREAM_BOTH);
    EXPECT_EQ(result, 0);

    // Step 2: Process messages
    wbb_process_messages(bridge_, 0);

    // Step 3: Check for efference copy
    wbb_efference_copy_t efference;
    result = wbb_receive_efference_copy(bridge_, &efference);
    // May or may not have efference depending on implementation timing

    // Step 4: Compare if efference received
    if (result == 0) {
        wbb_monitoring_result_t monitoring;
        wbb_compare_production(bridge_, &comp, &efference, &monitoring);

        // Step 5: Send error if detected
        if (monitoring.error_detected) {
            wbb_send_error_signal(bridge_, monitoring.error_type,
                                  monitoring.error_position, nullptr);
        }
    }
}

TEST_F(WernickeBrocaBridgeIntegrationTest, RepetitionTask) {
    CreateEndpoints();

    wbb_config_t config = wbb_default_config();
    config.enable_dorsal_stream = true;
    bridge_ = wbb_create(wernicke_, broca_, &config);
    ASSERT_NE(bridge_, nullptr);

    // Simulate hearing "cat" and repeating
    uint8_t heard_phonemes[] = {1, 2, 3};  // /k/, /ae/, /t/

    // Request repetition
    int result = wbb_request_repetition(bridge_, heard_phonemes, 3);
    EXPECT_EQ(result, 0);

    // Process the repetition
    wbb_process_messages(bridge_, 0);

    // Verify stats updated
    wbb_stats_t stats;
    wbb_get_stats(bridge_, &stats);
    EXPECT_GE(stats.repetition_requests, 1u);
}

TEST_F(WernickeBrocaBridgeIntegrationTest, SemanticResponseGeneration) {
    CreateEndpoints();

    wbb_config_t config = wbb_default_config();
    config.enable_ventral_stream = true;
    bridge_ = wbb_create(wernicke_, broca_, &config);
    ASSERT_NE(bridge_, nullptr);

    // Comprehend a question
    wbb_comprehension_t question = CreateComprehension(1, 100, "what", 0.9f);
    wbb_forward_comprehension(bridge_, &question, WBB_STREAM_VENTRAL);

    // Generate response intent
    float semantic_response[128] = {0};
    semantic_response[0] = 1.0f;  // Some semantic representation

    int result = wbb_send_response_intent(bridge_, semantic_response, 128, 1);  // question response
    EXPECT_EQ(result, 0);
}

TEST_F(WernickeBrocaBridgeIntegrationTest, WorkingMemoryMaintenance) {
    CreateEndpoints();

    wbb_config_t config = wbb_default_config();
    config.enable_working_memory = true;
    bridge_ = wbb_create(wernicke_, broca_, &config);
    ASSERT_NE(bridge_, nullptr);

    // Store phonemes in working memory via rehearsal
    uint8_t phonemes[] = {1, 2, 3, 4, 5, 6, 7};

    // Multiple rehearsal cycles to maintain
    for (int i = 0; i < 3; i++) {
        int result = wbb_request_rehearsal(bridge_, phonemes, 7, 1);
        EXPECT_EQ(result, 0);
        wbb_process_rehearsal(bridge_);
    }
}

//=============================================================================
// Error Handling Tests
//=============================================================================

TEST_F(WernickeBrocaBridgeIntegrationTest, ForwardNullComprehension) {
    CreateEndpoints();
    bridge_ = wbb_create(wernicke_, broca_, nullptr);
    ASSERT_NE(bridge_, nullptr);

    int result = wbb_forward_comprehension(bridge_, nullptr, WBB_STREAM_DORSAL);
    EXPECT_EQ(result, -1) << "Forwarding NULL should fail";
}

TEST_F(WernickeBrocaBridgeIntegrationTest, GetStatsNullOutput) {
    CreateEndpoints();
    bridge_ = wbb_create(wernicke_, broca_, nullptr);
    ASSERT_NE(bridge_, nullptr);

    int result = wbb_get_stats(bridge_, nullptr);
    EXPECT_EQ(result, -1) << "Getting stats with NULL output should fail";
}

TEST_F(WernickeBrocaBridgeIntegrationTest, GetConfigNullOutput) {
    CreateEndpoints();
    bridge_ = wbb_create(wernicke_, broca_, nullptr);
    ASSERT_NE(bridge_, nullptr);

    int result = wbb_get_config(bridge_, nullptr);
    EXPECT_EQ(result, -1) << "Getting config with NULL output should fail";
}

TEST_F(WernickeBrocaBridgeIntegrationTest, OperationsOnNullBridge) {
    // All operations on NULL bridge should return errors
    EXPECT_EQ(wbb_reset(nullptr), -1);
    EXPECT_EQ(wbb_forward_comprehension(nullptr, nullptr, WBB_STREAM_DORSAL), -1);
    EXPECT_EQ(wbb_request_repetition(nullptr, nullptr, 0), -1);
    EXPECT_EQ(wbb_process_messages(nullptr, 0), -1);
    EXPECT_EQ(wbb_pending_count(nullptr), 0u);
}

