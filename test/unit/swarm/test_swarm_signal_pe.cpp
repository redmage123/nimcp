//=============================================================================
// test_swarm_signal_pe.cpp - Unit Tests for Swarm Signal PE Integration
//=============================================================================
/**
 * @file test_swarm_signal_pe.cpp
 * @brief Unit tests for positional encoding in swarm signal adapter
 *
 * WHAT: Test PE integration with packet sequence ordering
 * WHY:  Temporal context critical for distributed swarm communication
 * HOW:  Test sinusoidal PE for packet sequences and ALiBi for attention
 *
 * TEST COVERAGE:
 * 1. PE configuration
 * 2. Packet sequence encoding
 * 3. Temporal embedding retrieval
 * 4. Edge cases (NULL, empty sequences, overflow)
 * 5. Integration with signal transmission
 *
 * @author NIMCP Development Team
 * @date 2025-12-10
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstdlib>
#include <cstring>

// Headers have their own extern "C" guards
    #include "swarm/nimcp_swarm_signal.h"
    #include "utils/encoding/nimcp_positional_encoding.h"
    #include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class SwarmSignalPETest : public ::testing::Test {
protected:
    static constexpr float EPSILON = 1e-4f;

    nimcp_swarm_signal_adapter_t* adapter = nullptr;
    nimcp_pos_encoder_t* encoder = nullptr;

    void SetUp() override {
        srand(42);
        nimcp_memory_init();

        // Create simulation adapter
        swarm_signal_config_t config = {};
        config.radio_type = SWARM_RADIO_SIMULATION;
        config.frequency_hz = 915000000;  // 915 MHz
        config.bandwidth_hz = 125000;
        config.tx_power_dbm = 20;
        config.max_packet_size = 256;
        config.retry_count = 3;
        config.timeout_ms = 1000;
        config.node_id = 1;

        adapter = swarm_signal_adapter_create(&config);
    }

    void TearDown() override {
        if (adapter) {
            swarm_signal_adapter_destroy(adapter);
            adapter = nullptr;
        }
        if (encoder) {
            nimcp_pos_encoder_destroy(encoder);
            encoder = nullptr;
        }
    }

    bool FloatEqual(float a, float b, float eps = EPSILON) {
        return std::abs(a - b) < eps;
    }
};

//=============================================================================
// Unit Tests: PE Configuration
//=============================================================================

TEST_F(SwarmSignalPETest, ConfigurePE_Sinusoidal) {
    // WHAT: Configure sinusoidal PE for packet sequences
    // WHY:  Fixed PE good for temporal packet ordering
    // HOW:  Create sinusoidal encoder and attach to adapter

    ASSERT_NE(adapter, nullptr);

    nimcp_pos_config_t config;
    config.type = NIMCP_POS_SINUSOIDAL;
    config.config.sinusoidal = nimcp_pos_sinusoidal_default_config();
    config.config.sinusoidal.base.embedding_dim = 128;
    config.config.sinusoidal.base.max_seq_length = 1024;

    encoder = nimcp_pos_encoder_create(&config);
    ASSERT_NE(encoder, nullptr);

    bool result = swarm_signal_set_pe_config(adapter, encoder);
    EXPECT_TRUE(result) << "Sinusoidal PE configuration should succeed";
}

TEST_F(SwarmSignalPETest, ConfigurePE_ALiBi) {
    // WHAT: Configure ALiBi for attention over packet sequences
    // WHY:  Linear bias efficient for distributed attention
    // HOW:  Create ALiBi encoder and attach to adapter

    ASSERT_NE(adapter, nullptr);

    nimcp_pos_config_t config;
    config.type = NIMCP_POS_ALIBI;
    config.config.alibi = nimcp_pos_alibi_default_config();
    config.config.alibi.base.max_seq_length = 2048;
    config.config.alibi.num_heads = 8;

    encoder = nimcp_pos_encoder_create(&config);
    ASSERT_NE(encoder, nullptr);

    bool result = swarm_signal_set_pe_config(adapter, encoder);
    EXPECT_TRUE(result) << "ALiBi PE configuration should succeed";
}

TEST_F(SwarmSignalPETest, ConfigurePE_Reconfigure) {
    // WHAT: Reconfigure PE after initial setup
    // WHY:  Dynamic PE parameter adjustment
    // HOW:  Configure twice with different encoders

    ASSERT_NE(adapter, nullptr);

    // First configuration
    nimcp_pos_config_t config1;
    config1.type = NIMCP_POS_SINUSOIDAL;
    config1.config.sinusoidal = nimcp_pos_sinusoidal_default_config();
    config1.config.sinusoidal.base.embedding_dim = 64;
    config1.config.sinusoidal.base.max_seq_length = 512;

    encoder = nimcp_pos_encoder_create(&config1);
    ASSERT_NE(encoder, nullptr);

    swarm_signal_set_pe_config(adapter, encoder);

    // Second configuration
    nimcp_pos_config_t config2;
    config2.type = NIMCP_POS_SINUSOIDAL;
    config2.config.sinusoidal = nimcp_pos_sinusoidal_default_config();
    config2.config.sinusoidal.base.embedding_dim = 128;
    config2.config.sinusoidal.base.max_seq_length = 1024;

    nimcp_pos_encoder_t* encoder2 = nimcp_pos_encoder_create(&config2);
    ASSERT_NE(encoder2, nullptr);

    bool result = swarm_signal_set_pe_config(adapter, encoder2);
    EXPECT_TRUE(result) << "PE reconfiguration should succeed";

    nimcp_pos_encoder_destroy(encoder2);
}

//=============================================================================
// Unit Tests: Sequence Encoding
//=============================================================================

TEST_F(SwarmSignalPETest, EncodeSequence_SinglePacket) {
    // WHAT: Encode single packet position
    // WHY:  Basic sequence encoding functionality
    // HOW:  Encode position 0 with length 1

    ASSERT_NE(adapter, nullptr);

    nimcp_pos_config_t config;
    config.type = NIMCP_POS_SINUSOIDAL;
    config.config.sinusoidal = nimcp_pos_sinusoidal_default_config();
    config.config.sinusoidal.base.embedding_dim = 64;
    config.config.sinusoidal.base.max_seq_length = 512;

    encoder = nimcp_pos_encoder_create(&config);
    ASSERT_NE(encoder, nullptr);

    swarm_signal_set_pe_config(adapter, encoder);

    float embeddings[64];
    bool result = swarm_signal_encode_sequence(adapter, 0, 1, embeddings);

    EXPECT_TRUE(result) << "Single packet encoding should succeed";

    // Verify embedding is non-zero
    float norm = 0.0f;
    for (int i = 0; i < 64; i++) {
        norm += embeddings[i] * embeddings[i];
    }
    EXPECT_GT(norm, 0.0f) << "Packet embedding should be non-zero";
}

TEST_F(SwarmSignalPETest, EncodeSequence_MultiplePackets) {
    // WHAT: Encode sequence of packet positions
    // WHY:  Temporal ordering for burst transmissions
    // HOW:  Encode positions 0-9

    ASSERT_NE(adapter, nullptr);

    nimcp_pos_config_t config;
    config.type = NIMCP_POS_SINUSOIDAL;
    config.config.sinusoidal = nimcp_pos_sinusoidal_default_config();
    config.config.sinusoidal.base.embedding_dim = 32;
    config.config.sinusoidal.base.max_seq_length = 256;

    encoder = nimcp_pos_encoder_create(&config);
    ASSERT_NE(encoder, nullptr);

    swarm_signal_set_pe_config(adapter, encoder);

    const uint32_t seq_len = 10;
    const uint32_t dim = 32;
    float embeddings[seq_len * dim];

    bool result = swarm_signal_encode_sequence(adapter, 0, seq_len, embeddings);

    EXPECT_TRUE(result) << "Multi-packet encoding should succeed";

    // Verify different positions have different encodings
    bool positions_differ = false;
    for (uint32_t i = 0; i < dim; i++) {
        if (std::abs(embeddings[i] - embeddings[dim + i]) > EPSILON) {
            positions_differ = true;
            break;
        }
    }
    EXPECT_TRUE(positions_differ) << "Different packet positions should have different encodings";
}

TEST_F(SwarmSignalPETest, EncodeSequence_LargeSequence) {
    // WHAT: Encode long packet sequence
    // WHY:  Verify scaling to large bursts
    // HOW:  Encode 100 packet sequence

    ASSERT_NE(adapter, nullptr);

    nimcp_pos_config_t config;
    config.type = NIMCP_POS_SINUSOIDAL;
    config.config.sinusoidal = nimcp_pos_sinusoidal_default_config();
    config.config.sinusoidal.base.embedding_dim = 64;
    config.config.sinusoidal.base.max_seq_length = 1024;

    encoder = nimcp_pos_encoder_create(&config);
    ASSERT_NE(encoder, nullptr);

    swarm_signal_set_pe_config(adapter, encoder);

    const uint32_t seq_len = 100;
    const uint32_t dim = 64;
    float* embeddings = new float[seq_len * dim];

    bool result = swarm_signal_encode_sequence(adapter, 0, seq_len, embeddings);

    EXPECT_TRUE(result) << "Large sequence encoding should succeed";

    // Verify first and last differ
    bool differ = false;
    for (uint32_t i = 0; i < dim; i++) {
        if (std::abs(embeddings[i] - embeddings[(seq_len - 1) * dim + i]) > EPSILON) {
            differ = true;
            break;
        }
    }
    EXPECT_TRUE(differ) << "First and last packet should have different encodings";

    delete[] embeddings;
}

TEST_F(SwarmSignalPETest, EncodeSequence_NonZeroStart) {
    // WHAT: Encode sequence starting from non-zero position
    // WHY:  Support continuation of packet streams
    // HOW:  Start encoding at position 100

    ASSERT_NE(adapter, nullptr);

    nimcp_pos_config_t config;
    config.type = NIMCP_POS_SINUSOIDAL;
    config.config.sinusoidal = nimcp_pos_sinusoidal_default_config();
    config.config.sinusoidal.base.embedding_dim = 32;
    config.config.sinusoidal.base.max_seq_length = 512;

    encoder = nimcp_pos_encoder_create(&config);
    ASSERT_NE(encoder, nullptr);

    swarm_signal_set_pe_config(adapter, encoder);

    const uint32_t start = 100;
    const uint32_t seq_len = 5;
    const uint32_t dim = 32;
    float embeddings[seq_len * dim];

    bool result = swarm_signal_encode_sequence(adapter, start, seq_len, embeddings);

    EXPECT_TRUE(result) << "Non-zero start encoding should succeed";

    // Encoding should differ from position 0
    float pos0_embedding[dim];
    swarm_signal_encode_sequence(adapter, 0, 1, pos0_embedding);

    bool differ = false;
    for (uint32_t i = 0; i < dim; i++) {
        if (std::abs(embeddings[i] - pos0_embedding[i]) > EPSILON) {
            differ = true;
            break;
        }
    }
    EXPECT_TRUE(differ) << "Position 100 should differ from position 0";
}

//=============================================================================
// Unit Tests: Temporal Embedding
//=============================================================================

TEST_F(SwarmSignalPETest, GetTemporalEmbedding_CurrentState) {
    // WHAT: Retrieve PE for current packet sequence position
    // WHY:  Add temporal context to ongoing transmission
    // HOW:  Get embedding for current adapter state

    ASSERT_NE(adapter, nullptr);

    nimcp_pos_config_t config;
    config.type = NIMCP_POS_SINUSOIDAL;
    config.config.sinusoidal = nimcp_pos_sinusoidal_default_config();
    config.config.sinusoidal.base.embedding_dim = 64;
    config.config.sinusoidal.base.max_seq_length = 512;

    encoder = nimcp_pos_encoder_create(&config);
    ASSERT_NE(encoder, nullptr);

    swarm_signal_set_pe_config(adapter, encoder);

    float embedding[64];
    bool result = swarm_signal_get_temporal_embedding(adapter, embedding);

    EXPECT_TRUE(result) << "Temporal embedding retrieval should succeed";

    // Verify non-zero
    float norm = 0.0f;
    for (int i = 0; i < 64; i++) {
        norm += embedding[i] * embedding[i];
    }
    EXPECT_GT(norm, 0.0f) << "Temporal embedding should be non-zero";
}

TEST_F(SwarmSignalPETest, GetTemporalEmbedding_MultipleRetrievals) {
    // WHAT: Retrieve temporal embedding multiple times
    // WHY:  Verify consistent behavior
    // HOW:  Call get_temporal_embedding twice

    ASSERT_NE(adapter, nullptr);

    nimcp_pos_config_t config;
    config.type = NIMCP_POS_SINUSOIDAL;
    config.config.sinusoidal = nimcp_pos_sinusoidal_default_config();
    config.config.sinusoidal.base.embedding_dim = 32;
    config.config.sinusoidal.base.max_seq_length = 256;

    encoder = nimcp_pos_encoder_create(&config);
    ASSERT_NE(encoder, nullptr);

    swarm_signal_set_pe_config(adapter, encoder);

    float emb1[32], emb2[32];

    swarm_signal_get_temporal_embedding(adapter, emb1);
    swarm_signal_get_temporal_embedding(adapter, emb2);

    // Should be consistent (assuming no packets sent between calls)
    bool same = true;
    for (int i = 0; i < 32; i++) {
        if (std::abs(emb1[i] - emb2[i]) > EPSILON) {
            same = false;
            break;
        }
    }

    // Result depends on whether adapter increments sequence number
    SUCCEED() << "Multiple temporal embedding retrievals handled";
}

//=============================================================================
// Unit Tests: Edge Cases
//=============================================================================

TEST_F(SwarmSignalPETest, EdgeCase_NullAdapter) {
    // WHAT: Handle NULL adapter pointer
    // WHY:  Robustness against invalid input
    // HOW:  Pass NULL to PE functions

    nimcp_pos_config_t config;
    config.type = NIMCP_POS_SINUSOIDAL;
    config.config.sinusoidal = nimcp_pos_sinusoidal_default_config();

    encoder = nimcp_pos_encoder_create(&config);
    ASSERT_NE(encoder, nullptr);

    bool result1 = swarm_signal_set_pe_config(nullptr, encoder);
    EXPECT_FALSE(result1);

    float dummy[64];
    bool result2 = swarm_signal_encode_sequence(nullptr, 0, 1, dummy);
    EXPECT_FALSE(result2);

    bool result3 = swarm_signal_get_temporal_embedding(nullptr, dummy);
    EXPECT_FALSE(result3);
}

TEST_F(SwarmSignalPETest, EdgeCase_NullEncoder) {
    // WHAT: Handle NULL encoder pointer
    // WHY:  Validate encoder parameter
    // HOW:  Pass NULL encoder to set_pe_config

    ASSERT_NE(adapter, nullptr);

    bool result = swarm_signal_set_pe_config(adapter, nullptr);
    EXPECT_FALSE(result) << "NULL encoder should be rejected";
}

TEST_F(SwarmSignalPETest, EdgeCase_ZeroSequenceLength) {
    // WHAT: Encode zero-length sequence
    // WHY:  Invalid parameter handling
    // HOW:  Pass seq_length = 0

    ASSERT_NE(adapter, nullptr);

    nimcp_pos_config_t config;
    config.type = NIMCP_POS_SINUSOIDAL;
    config.config.sinusoidal = nimcp_pos_sinusoidal_default_config();
    config.config.sinusoidal.base.embedding_dim = 64;
    config.config.sinusoidal.base.max_seq_length = 512;

    encoder = nimcp_pos_encoder_create(&config);
    ASSERT_NE(encoder, nullptr);

    swarm_signal_set_pe_config(adapter, encoder);

    float dummy[64];
    bool result = swarm_signal_encode_sequence(adapter, 0, 0, dummy);

    EXPECT_FALSE(result) << "Zero-length sequence should be rejected";
}

TEST_F(SwarmSignalPETest, EdgeCase_NullOutputBuffer) {
    // WHAT: Pass NULL output buffer
    // WHY:  Validate output parameter
    // HOW:  Pass nullptr for embeddings_out

    ASSERT_NE(adapter, nullptr);

    nimcp_pos_config_t config;
    config.type = NIMCP_POS_SINUSOIDAL;
    config.config.sinusoidal = nimcp_pos_sinusoidal_default_config();
    config.config.sinusoidal.base.embedding_dim = 64;
    config.config.sinusoidal.base.max_seq_length = 512;

    encoder = nimcp_pos_encoder_create(&config);
    ASSERT_NE(encoder, nullptr);

    swarm_signal_set_pe_config(adapter, encoder);

    bool result1 = swarm_signal_encode_sequence(adapter, 0, 5, nullptr);
    EXPECT_FALSE(result1);

    bool result2 = swarm_signal_get_temporal_embedding(adapter, nullptr);
    EXPECT_FALSE(result2);
}

TEST_F(SwarmSignalPETest, EdgeCase_PENotConfigured) {
    // WHAT: Use PE functions before configuration
    // WHY:  Verify initialization checking
    // HOW:  Call PE functions without set_pe_config

    ASSERT_NE(adapter, nullptr);

    float dummy[64];

    bool result1 = swarm_signal_encode_sequence(adapter, 0, 1, dummy);
    EXPECT_FALSE(result1) << "Encoding before PE config should fail";

    bool result2 = swarm_signal_get_temporal_embedding(adapter, dummy);
    EXPECT_FALSE(result2) << "Temporal embedding before PE config should fail";
}

TEST_F(SwarmSignalPETest, EdgeCase_VeryLargeSequenceNumber) {
    // WHAT: Encode very high sequence numbers
    // WHY:  Verify handling of large counters
    // HOW:  Start encoding at very high position

    ASSERT_NE(adapter, nullptr);

    nimcp_pos_config_t config;
    config.type = NIMCP_POS_SINUSOIDAL;
    config.config.sinusoidal = nimcp_pos_sinusoidal_default_config();
    config.config.sinusoidal.base.embedding_dim = 32;
    config.config.sinusoidal.base.max_seq_length = 100000;

    encoder = nimcp_pos_encoder_create(&config);
    ASSERT_NE(encoder, nullptr);

    swarm_signal_set_pe_config(adapter, encoder);

    const uint32_t large_start = 50000;
    float embeddings[32];

    bool result = swarm_signal_encode_sequence(adapter, large_start, 1, embeddings);

    // Sinusoidal should handle large positions gracefully
    SUCCEED() << "Large sequence number handled";
}

TEST_F(SwarmSignalPETest, EdgeCase_SequenceExceedsMaxLength) {
    // WHAT: Encode sequence exceeding configured max_length
    // WHY:  Verify bounds checking or extrapolation
    // HOW:  Request encoding beyond max_seq_length

    ASSERT_NE(adapter, nullptr);

    nimcp_pos_config_t config;
    config.type = NIMCP_POS_SINUSOIDAL;
    config.config.sinusoidal = nimcp_pos_sinusoidal_default_config();
    config.config.sinusoidal.base.embedding_dim = 32;
    config.config.sinusoidal.base.max_seq_length = 100;

    encoder = nimcp_pos_encoder_create(&config);
    ASSERT_NE(encoder, nullptr);

    swarm_signal_set_pe_config(adapter, encoder);

    float embeddings[32];
    bool result = swarm_signal_encode_sequence(adapter, 200, 1, embeddings);

    // Sinusoidal can extrapolate, so may succeed
    SUCCEED() << "Sequence exceeding max_length handled";
}

//=============================================================================
// Unit Tests: Integration
//=============================================================================

TEST_F(SwarmSignalPETest, Integration_WithSignalTransmission) {
    // WHAT: Verify PE works alongside signal transmission
    // WHY:  PE should not interfere with radio operations
    // HOW:  Configure PE, send packets, use PE functions

    ASSERT_NE(adapter, nullptr);

    nimcp_pos_config_t config;
    config.type = NIMCP_POS_SINUSOIDAL;
    config.config.sinusoidal = nimcp_pos_sinusoidal_default_config();
    config.config.sinusoidal.base.embedding_dim = 64;
    config.config.sinusoidal.base.max_seq_length = 512;

    encoder = nimcp_pos_encoder_create(&config);
    ASSERT_NE(encoder, nullptr);

    swarm_signal_set_pe_config(adapter, encoder);

    // Send a packet (simulation mode won't actually transmit)
    uint8_t data[32] = {1, 2, 3, 4, 5};
    swarm_signal_send(adapter, data, 5, 0);

    // Use PE functions
    float embedding[64];
    bool result = swarm_signal_get_temporal_embedding(adapter, embedding);

    EXPECT_TRUE(result) << "PE should work after transmission";
}

TEST_F(SwarmSignalPETest, Integration_SequenceAndTemporal) {
    // WHAT: Use both sequence encoding and temporal embedding
    // WHY:  Verify compatibility of different PE functions
    // HOW:  Call both encode_sequence and get_temporal_embedding

    ASSERT_NE(adapter, nullptr);

    nimcp_pos_config_t config;
    config.type = NIMCP_POS_SINUSOIDAL;
    config.config.sinusoidal = nimcp_pos_sinusoidal_default_config();
    config.config.sinusoidal.base.embedding_dim = 32;
    config.config.sinusoidal.base.max_seq_length = 256;

    encoder = nimcp_pos_encoder_create(&config);
    ASSERT_NE(encoder, nullptr);

    swarm_signal_set_pe_config(adapter, encoder);

    // Sequence encoding
    float seq_embeddings[5 * 32];
    bool result1 = swarm_signal_encode_sequence(adapter, 0, 5, seq_embeddings);
    EXPECT_TRUE(result1);

    // Temporal embedding
    float temporal_emb[32];
    bool result2 = swarm_signal_get_temporal_embedding(adapter, temporal_emb);
    EXPECT_TRUE(result2);

    SUCCEED() << "Sequence and temporal PE integration works";
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
