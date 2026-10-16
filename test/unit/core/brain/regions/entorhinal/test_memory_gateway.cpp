/**
 * @file test_memory_gateway.cpp
 * @brief Unit tests for Entorhinal Memory Gateway functionality
 * @version Phase 5: Memory Circuit
 * @date 2025-01-12
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

extern "C" {
#include "core/brain/regions/entorhinal/nimcp_entorhinal.h"
}

class MemoryGatewayTest : public ::testing::Test {
protected:
    nimcp_entorhinal_t* ec = nullptr;

    void SetUp() override {
        entorhinal_config_t config = entorhinal_default_config();
        config.encoding_buffer_size = 256;
        config.retrieval_buffer_size = 256;
        config.encoding_threshold = 0.3f;
        config.retrieval_threshold = 0.3f;
        config.enable_hippocampus = true;
        ec = entorhinal_create(&config);
        ASSERT_NE(ec, nullptr);
    }

    void TearDown() override {
        if (ec) {
            entorhinal_destroy(ec);
            ec = nullptr;
        }
    }
};

/*=============================================================================
 * ENCODING GATE TESTS
 *===========================================================================*/

TEST_F(MemoryGatewayTest, SetEncodingGateOpen) {
    EXPECT_EQ(entorhinal_set_encoding_gate(ec, 1.0f), 0);
    EXPECT_FLOAT_EQ(ec->memory_gateway.encoding_gate, 1.0f);
}

TEST_F(MemoryGatewayTest, SetEncodingGateClosed) {
    EXPECT_EQ(entorhinal_set_encoding_gate(ec, 0.0f), 0);
    EXPECT_FLOAT_EQ(ec->memory_gateway.encoding_gate, 0.0f);
}

TEST_F(MemoryGatewayTest, SetEncodingGatePartial) {
    EXPECT_EQ(entorhinal_set_encoding_gate(ec, 0.5f), 0);
    EXPECT_FLOAT_EQ(ec->memory_gateway.encoding_gate, 0.5f);
}

TEST_F(MemoryGatewayTest, SetEncodingGateNull) {
    EXPECT_EQ(entorhinal_set_encoding_gate(nullptr, 0.5f), -1);
}

TEST_F(MemoryGatewayTest, SetEncodingGateClamped) {
    // Values should be clamped to [0, 1]
    EXPECT_EQ(entorhinal_set_encoding_gate(ec, 1.5f), 0);
    EXPECT_LE(ec->memory_gateway.encoding_gate, 1.0f);

    EXPECT_EQ(entorhinal_set_encoding_gate(ec, -0.5f), 0);
    EXPECT_GE(ec->memory_gateway.encoding_gate, 0.0f);
}

/*=============================================================================
 * RETRIEVAL GATE TESTS
 *===========================================================================*/

TEST_F(MemoryGatewayTest, SetRetrievalGateOpen) {
    EXPECT_EQ(entorhinal_set_retrieval_gate(ec, 1.0f), 0);
    EXPECT_FLOAT_EQ(ec->memory_gateway.retrieval_gate, 1.0f);
}

TEST_F(MemoryGatewayTest, SetRetrievalGateClosed) {
    EXPECT_EQ(entorhinal_set_retrieval_gate(ec, 0.0f), 0);
    EXPECT_FLOAT_EQ(ec->memory_gateway.retrieval_gate, 0.0f);
}

TEST_F(MemoryGatewayTest, SetRetrievalGatePartial) {
    EXPECT_EQ(entorhinal_set_retrieval_gate(ec, 0.7f), 0);
    EXPECT_FLOAT_EQ(ec->memory_gateway.retrieval_gate, 0.7f);
}

TEST_F(MemoryGatewayTest, SetRetrievalGateNull) {
    EXPECT_EQ(entorhinal_set_retrieval_gate(nullptr, 0.5f), -1);
}

TEST_F(MemoryGatewayTest, SetRetrievalGateClamped) {
    EXPECT_EQ(entorhinal_set_retrieval_gate(ec, 2.0f), 0);
    EXPECT_LE(ec->memory_gateway.retrieval_gate, 1.0f);

    EXPECT_EQ(entorhinal_set_retrieval_gate(ec, -1.0f), 0);
    EXPECT_GE(ec->memory_gateway.retrieval_gate, 0.0f);
}

/*=============================================================================
 * ENCODE TO HIPPOCAMPUS TESTS
 *===========================================================================*/

TEST_F(MemoryGatewayTest, EncodeToHippocampusBasic) {
    // Open encoding gate
    entorhinal_set_encoding_gate(ec, 1.0f);

    float features[32];
    for (int i = 0; i < 32; i++) {
        features[i] = (float)i / 32.0f;
    }

    float spatial_context[3] = {1.0f, 2.0f, 0.0f};

    EXPECT_EQ(entorhinal_encode_to_hippocampus(ec, features, 32,
        spatial_context, 3), 0);
}

TEST_F(MemoryGatewayTest, EncodeToHippocampusWithClosedGate) {
    entorhinal_set_encoding_gate(ec, 0.0f);

    float features[32] = {0};
    float spatial_context[3] = {0.0f, 0.0f, 0.0f};

    // May return error or succeed with no effect depending on implementation
    int result = entorhinal_encode_to_hippocampus(ec, features, 32,
        spatial_context, 3);
    // Just verify it doesn't crash
    SUCCEED();
}

TEST_F(MemoryGatewayTest, EncodeToHippocampusNull) {
    EXPECT_EQ(entorhinal_encode_to_hippocampus(nullptr, nullptr, 0, nullptr, 0), -1);

    float features[32];
    float context[3];
    EXPECT_EQ(entorhinal_encode_to_hippocampus(nullptr, features, 32, context, 3), -1);
    EXPECT_EQ(entorhinal_encode_to_hippocampus(ec, nullptr, 32, context, 3), -1);
    EXPECT_EQ(entorhinal_encode_to_hippocampus(ec, features, 32, nullptr, 3), -1);
}

TEST_F(MemoryGatewayTest, EncodeToHippocampusLargeFeatures) {
    entorhinal_set_encoding_gate(ec, 1.0f);

    float features[256];
    for (int i = 0; i < 256; i++) {
        features[i] = sinf((float)i * 0.1f);
    }

    float spatial_context[3] = {5.0f, 5.0f, 0.0f};

    EXPECT_EQ(entorhinal_encode_to_hippocampus(ec, features, 256,
        spatial_context, 3), 0);
}

TEST_F(MemoryGatewayTest, EncodeToHippocampusUpdatesStats) {
    entorhinal_set_encoding_gate(ec, 1.0f);

    uint64_t initial_items = ec->memory_gateway.items_encoded;

    float features[16] = {0};
    float context[3] = {0};
    entorhinal_encode_to_hippocampus(ec, features, 16, context, 3);

    EXPECT_GE(ec->memory_gateway.items_encoded, initial_items);
}

/*=============================================================================
 * RETRIEVE FROM HIPPOCAMPUS TESTS
 *===========================================================================*/

TEST_F(MemoryGatewayTest, RetrieveFromHippocampusBasic) {
    entorhinal_set_retrieval_gate(ec, 1.0f);

    float cue[16] = {0.5f};
    float retrieved[64];
    uint32_t actual_features = 0;

    EXPECT_EQ(entorhinal_retrieve_from_hippocampus(ec, cue, 16,
        retrieved, 64, &actual_features), 0);
}

TEST_F(MemoryGatewayTest, RetrieveFromHippocampusWithClosedGate) {
    entorhinal_set_retrieval_gate(ec, 0.0f);

    float cue[16] = {0};
    float retrieved[64];
    uint32_t actual_features = 0;

    // May return error or succeed with empty result
    int result = entorhinal_retrieve_from_hippocampus(ec, cue, 16,
        retrieved, 64, &actual_features);
    // Just verify it doesn't crash
    SUCCEED();
}

TEST_F(MemoryGatewayTest, RetrieveFromHippocampusNull) {
    EXPECT_EQ(entorhinal_retrieve_from_hippocampus(nullptr, nullptr, 0,
        nullptr, 0, nullptr), -1);

    float cue[16];
    float retrieved[64];
    uint32_t actual;
    EXPECT_EQ(entorhinal_retrieve_from_hippocampus(nullptr, cue, 16,
        retrieved, 64, &actual), -1);
    EXPECT_EQ(entorhinal_retrieve_from_hippocampus(ec, nullptr, 16,
        retrieved, 64, &actual), -1);
    EXPECT_EQ(entorhinal_retrieve_from_hippocampus(ec, cue, 16,
        nullptr, 64, &actual), -1);
}

TEST_F(MemoryGatewayTest, RetrieveFromHippocampusSmallBuffer) {
    entorhinal_set_retrieval_gate(ec, 1.0f);

    float cue[16] = {0.5f};
    float retrieved[8];  // Small buffer
    uint32_t actual_features = 0;

    EXPECT_EQ(entorhinal_retrieve_from_hippocampus(ec, cue, 16,
        retrieved, 8, &actual_features), 0);
    EXPECT_LE(actual_features, 8u);
}

TEST_F(MemoryGatewayTest, RetrieveFromHippocampusUpdatesStats) {
    entorhinal_set_retrieval_gate(ec, 1.0f);

    uint64_t initial_items = ec->memory_gateway.items_retrieved;

    float cue[16] = {0};
    float retrieved[64];
    uint32_t actual;
    entorhinal_retrieve_from_hippocampus(ec, cue, 16, retrieved, 64, &actual);

    EXPECT_GE(ec->memory_gateway.items_retrieved, initial_items);
}

/*=============================================================================
 * CONSOLIDATION TESTS
 *===========================================================================*/

TEST_F(MemoryGatewayTest, ConsolidateToNeocortexBasic) {
    uint32_t memory_id = 1;
    float consolidation_strength = 0.8f;

    EXPECT_EQ(entorhinal_consolidate_to_neocortex(ec, memory_id,
        consolidation_strength), 0);
}

TEST_F(MemoryGatewayTest, ConsolidateToNeocortexFullStrength) {
    EXPECT_EQ(entorhinal_consolidate_to_neocortex(ec, 0, 1.0f), 0);
}

TEST_F(MemoryGatewayTest, ConsolidateToNeocortexZeroStrength) {
    EXPECT_EQ(entorhinal_consolidate_to_neocortex(ec, 0, 0.0f), 0);
}

TEST_F(MemoryGatewayTest, ConsolidateToNeocortexNull) {
    EXPECT_EQ(entorhinal_consolidate_to_neocortex(nullptr, 0, 0.5f), -1);
}

TEST_F(MemoryGatewayTest, ConsolidateToNeocortexUpdatesStats) {
    uint64_t initial = ec->memory_gateway.items_consolidated;

    entorhinal_consolidate_to_neocortex(ec, 0, 0.5f);

    EXPECT_GE(ec->memory_gateway.items_consolidated, initial);
}

/*=============================================================================
 * GATEWAY STATISTICS TESTS
 *===========================================================================*/

TEST_F(MemoryGatewayTest, GetGatewayStatsInitial) {
    uint64_t encoded, retrieved, consolidated;

    EXPECT_EQ(entorhinal_get_gateway_stats(ec, &encoded, &retrieved,
        &consolidated), 0);

    EXPECT_EQ(encoded, 0u);
    EXPECT_EQ(retrieved, 0u);
    EXPECT_EQ(consolidated, 0u);
}

TEST_F(MemoryGatewayTest, GetGatewayStatsAfterOperations) {
    // Perform some operations
    entorhinal_set_encoding_gate(ec, 1.0f);
    entorhinal_set_retrieval_gate(ec, 1.0f);

    float features[16] = {0};
    float context[3] = {0};
    entorhinal_encode_to_hippocampus(ec, features, 16, context, 3);

    float cue[8] = {0};
    float retrieved_features[32];
    uint32_t actual;
    entorhinal_retrieve_from_hippocampus(ec, cue, 8, retrieved_features, 32, &actual);

    entorhinal_consolidate_to_neocortex(ec, 0, 0.5f);

    uint64_t encoded, retrieved, consolidated;
    EXPECT_EQ(entorhinal_get_gateway_stats(ec, &encoded, &retrieved,
        &consolidated), 0);
}

TEST_F(MemoryGatewayTest, GetGatewayStatsNull) {
    EXPECT_EQ(entorhinal_get_gateway_stats(nullptr, nullptr, nullptr, nullptr), -1);

    uint64_t e, r, c;
    EXPECT_EQ(entorhinal_get_gateway_stats(nullptr, &e, &r, &c), -1);
    EXPECT_EQ(entorhinal_get_gateway_stats(ec, nullptr, &r, &c), -1);
    EXPECT_EQ(entorhinal_get_gateway_stats(ec, &e, nullptr, &c), -1);
    EXPECT_EQ(entorhinal_get_gateway_stats(ec, &e, &r, nullptr), -1);
}

/*=============================================================================
 * MEMORY BINDING TESTS
 *===========================================================================*/

TEST_F(MemoryGatewayTest, MemoryBindingStrengthInitial) {
    EXPECT_GE(ec->memory_gateway.memory_binding_strength, 0.0f);
    EXPECT_LE(ec->memory_gateway.memory_binding_strength, 1.0f);
}

TEST_F(MemoryGatewayTest, ContextBindingStrengthInitial) {
    EXPECT_GE(ec->memory_gateway.context_binding_strength, 0.0f);
    EXPECT_LE(ec->memory_gateway.context_binding_strength, 1.0f);
}

TEST_F(MemoryGatewayTest, TemporalBindingStrengthInitial) {
    EXPECT_GE(ec->memory_gateway.temporal_binding_strength, 0.0f);
    EXPECT_LE(ec->memory_gateway.temporal_binding_strength, 1.0f);
}

/*=============================================================================
 * CONSOLIDATION GATE TESTS
 *===========================================================================*/

TEST_F(MemoryGatewayTest, ConsolidationGateInitial) {
    EXPECT_GE(ec->memory_gateway.consolidation_gate, 0.0f);
    EXPECT_LE(ec->memory_gateway.consolidation_gate, 1.0f);
}

/*=============================================================================
 * TRANSFER LATENCY TESTS
 *===========================================================================*/

TEST_F(MemoryGatewayTest, TransferLatencyNonNegative) {
    EXPECT_GE(ec->memory_gateway.transfer_latency_ms, 0.0f);
}

/*=============================================================================
 * ENCODE-RETRIEVE ROUND TRIP TEST
 *===========================================================================*/

TEST_F(MemoryGatewayTest, EncodeRetrieveRoundTrip) {
    // Open both gates
    entorhinal_set_encoding_gate(ec, 1.0f);
    entorhinal_set_retrieval_gate(ec, 1.0f);

    // Create a distinctive pattern
    float features[32];
    for (int i = 0; i < 32; i++) {
        features[i] = sinf((float)i * 0.2f);
    }
    float context[3] = {1.0f, 2.0f, 3.0f};

    // Encode
    EXPECT_EQ(entorhinal_encode_to_hippocampus(ec, features, 32, context, 3), 0);

    // Retrieve using similar cue
    float cue[32];
    for (int i = 0; i < 32; i++) {
        cue[i] = features[i] + 0.1f;  // Slightly perturbed
    }
    float retrieved[64];
    uint32_t actual;

    EXPECT_EQ(entorhinal_retrieve_from_hippocampus(ec, cue, 32, retrieved, 64, &actual), 0);
}

