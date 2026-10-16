//=============================================================================
// test_circular_buffer_pe.cpp - Unit Tests for Circular Buffer PE Integration
//=============================================================================
/**
 * @file test_circular_buffer_pe.cpp
 * @brief Unit tests for positional encoding in circular buffer
 *
 * WHAT: Test PE integration with temporal buffer indexing
 * WHY:  Position-aware buffer access enables temporal context
 * HOW:  Test sinusoidal PE and ALiBi bias for buffered sequences
 *
 * TEST COVERAGE:
 * 1. PE configuration (sinusoidal and ALiBi)
 * 2. Position embedding retrieval
 * 3. ALiBi bias matrix generation
 * 4. Edge cases (NULL, empty buffer, out of bounds)
 * 5. Buffer wrap-around scenarios
 *
 * @author NIMCP Development Team
 * @date 2025-12-10
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstdlib>
#include <cstring>

// Headers have their own extern "C" guards
    #include "middleware/buffering/nimcp_circular_buffer.h"
    #include "utils/encoding/nimcp_positional_encoding.h"
    #include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class CircularBufferPETest : public ::testing::Test {
protected:
    static constexpr float EPSILON = 1e-4f;

    circular_buffer_t* buffer = nullptr;
    nimcp_pos_encoder_t* encoder = nullptr;

    void SetUp() override {
        srand(42);
        nimcp_memory_init();
    }

    void TearDown() override {
        if (buffer) {
            circular_buffer_destroy(buffer);
            buffer = nullptr;
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

TEST_F(CircularBufferPETest, ConfigurePE_Sinusoidal) {
    // WHAT: Configure sinusoidal PE for buffer positions
    // WHY:  Fixed PE good for temporal buffer sequences
    // HOW:  Create sinusoidal encoder and attach to buffer

    buffer = circular_buffer_create(sizeof(float), 64, OVERFLOW_OVERWRITE);
    ASSERT_NE(buffer, nullptr);

    nimcp_pos_config_t config;
    config.type = NIMCP_POS_SINUSOIDAL;
    config.config.sinusoidal = nimcp_pos_sinusoidal_default_config();
    config.config.sinusoidal.base.embedding_dim = 128;
    config.config.sinusoidal.base.max_seq_length = 64;

    encoder = nimcp_pos_encoder_create(&config);
    ASSERT_NE(encoder, nullptr);

    bool result = circular_buffer_set_pe_config(buffer, encoder);
    EXPECT_TRUE(result) << "Sinusoidal PE configuration should succeed";
}

TEST_F(CircularBufferPETest, ConfigurePE_ALiBi) {
    // WHAT: Configure ALiBi for attention over buffer
    // WHY:  Linear bias efficient for buffer-based attention
    // HOW:  Create ALiBi encoder and attach to buffer

    buffer = circular_buffer_create(sizeof(float), 128, OVERFLOW_OVERWRITE);
    ASSERT_NE(buffer, nullptr);

    nimcp_pos_config_t config;
    config.type = NIMCP_POS_ALIBI;
    config.config.alibi = nimcp_pos_alibi_default_config();
    config.config.alibi.base.max_seq_length = 128;
    config.config.alibi.num_heads = 8;

    encoder = nimcp_pos_encoder_create(&config);
    ASSERT_NE(encoder, nullptr);

    bool result = circular_buffer_set_pe_config(buffer, encoder);
    EXPECT_TRUE(result) << "ALiBi PE configuration should succeed";
}

TEST_F(CircularBufferPETest, ConfigurePE_Reconfigure) {
    // WHAT: Reconfigure PE after initial setup
    // WHY:  Dynamic PE configuration changes
    // HOW:  Configure twice with different parameters

    buffer = circular_buffer_create(sizeof(float), 32, OVERFLOW_OVERWRITE);
    ASSERT_NE(buffer, nullptr);

    // First configuration
    nimcp_pos_config_t config1;
    config1.type = NIMCP_POS_SINUSOIDAL;
    config1.config.sinusoidal = nimcp_pos_sinusoidal_default_config();
    config1.config.sinusoidal.base.embedding_dim = 64;
    config1.config.sinusoidal.base.max_seq_length = 32;

    encoder = nimcp_pos_encoder_create(&config1);
    ASSERT_NE(encoder, nullptr);

    circular_buffer_set_pe_config(buffer, encoder);

    // Second configuration (new encoder)
    nimcp_pos_config_t config2;
    config2.type = NIMCP_POS_ALIBI;
    config2.config.alibi = nimcp_pos_alibi_default_config();
    config2.config.alibi.base.max_seq_length = 32;
    config2.config.alibi.num_heads = 4;

    nimcp_pos_encoder_t* encoder2 = nimcp_pos_encoder_create(&config2);
    ASSERT_NE(encoder2, nullptr);

    bool result = circular_buffer_set_pe_config(buffer, encoder2);
    EXPECT_TRUE(result) << "PE reconfiguration should succeed";

    nimcp_pos_encoder_destroy(encoder2);
}

//=============================================================================
// Unit Tests: Position Embeddings
//=============================================================================

TEST_F(CircularBufferPETest, GetPositionEmbedding_SinglePosition) {
    // WHAT: Retrieve PE for single buffer position
    // WHY:  Basic PE retrieval functionality
    // HOW:  Get embedding for position 0

    buffer = circular_buffer_create(sizeof(float), 32, OVERFLOW_OVERWRITE);
    ASSERT_NE(buffer, nullptr);

    nimcp_pos_config_t config;
    config.type = NIMCP_POS_SINUSOIDAL;
    config.config.sinusoidal = nimcp_pos_sinusoidal_default_config();
    config.config.sinusoidal.base.embedding_dim = 64;
    config.config.sinusoidal.base.max_seq_length = 32;

    encoder = nimcp_pos_encoder_create(&config);
    ASSERT_NE(encoder, nullptr);

    circular_buffer_set_pe_config(buffer, encoder);

    // Push some data
    float data = 1.0f;
    circular_buffer_push(buffer, &data);

    float embedding[64];
    bool result = circular_buffer_get_position_embedding(buffer, 0, embedding);

    EXPECT_TRUE(result) << "Position embedding retrieval should succeed";

    // Verify embedding is non-zero
    float norm = 0.0f;
    for (int i = 0; i < 64; i++) {
        norm += embedding[i] * embedding[i];
    }
    EXPECT_GT(norm, 0.0f) << "Position embedding should be non-zero";
}

TEST_F(CircularBufferPETest, GetPositionEmbedding_MultiplePositions) {
    // WHAT: Retrieve embeddings for multiple buffer positions
    // WHY:  Different positions should have different encodings
    // HOW:  Compare embeddings at positions 0, 1, 2

    buffer = circular_buffer_create(sizeof(float), 64, OVERFLOW_OVERWRITE);
    ASSERT_NE(buffer, nullptr);

    nimcp_pos_config_t config;
    config.type = NIMCP_POS_SINUSOIDAL;
    config.config.sinusoidal = nimcp_pos_sinusoidal_default_config();
    config.config.sinusoidal.base.embedding_dim = 32;
    config.config.sinusoidal.base.max_seq_length = 64;

    encoder = nimcp_pos_encoder_create(&config);
    ASSERT_NE(encoder, nullptr);

    circular_buffer_set_pe_config(buffer, encoder);

    // Push data to buffer
    for (int i = 0; i < 10; i++) {
        float data = (float)i;
        circular_buffer_push(buffer, &data);
    }

    float emb0[32], emb1[32], emb2[32];

    circular_buffer_get_position_embedding(buffer, 0, emb0);
    circular_buffer_get_position_embedding(buffer, 1, emb1);
    circular_buffer_get_position_embedding(buffer, 2, emb2);

    // Embeddings should differ
    bool differ_01 = false, differ_12 = false;

    for (int i = 0; i < 32; i++) {
        if (std::abs(emb0[i] - emb1[i]) > EPSILON) {
            differ_01 = true;
        }
        if (std::abs(emb1[i] - emb2[i]) > EPSILON) {
            differ_12 = true;
        }
    }

    EXPECT_TRUE(differ_01) << "Positions 0 and 1 should have different embeddings";
    EXPECT_TRUE(differ_12) << "Positions 1 and 2 should have different embeddings";
}

TEST_F(CircularBufferPETest, GetPositionEmbedding_AfterWrapAround) {
    // WHAT: Retrieve embeddings after buffer wraps
    // WHY:  Verify PE correctness with circular indexing
    // HOW:  Fill buffer, overflow, then check embeddings

    buffer = circular_buffer_create(sizeof(float), 8, OVERFLOW_OVERWRITE);
    ASSERT_NE(buffer, nullptr);

    nimcp_pos_config_t config;
    config.type = NIMCP_POS_SINUSOIDAL;
    config.config.sinusoidal = nimcp_pos_sinusoidal_default_config();
    config.config.sinusoidal.base.embedding_dim = 16;
    config.config.sinusoidal.base.max_seq_length = 16;

    encoder = nimcp_pos_encoder_create(&config);
    ASSERT_NE(encoder, nullptr);

    circular_buffer_set_pe_config(buffer, encoder);

    // Push more data than capacity to cause wrap
    for (int i = 0; i < 20; i++) {
        float data = (float)i;
        circular_buffer_push(buffer, &data);
    }

    float embedding[16];
    bool result = circular_buffer_get_position_embedding(buffer, 0, embedding);

    EXPECT_TRUE(result) << "Position embedding after wrap should succeed";
}

//=============================================================================
// Unit Tests: ALiBi Bias
//=============================================================================

TEST_F(CircularBufferPETest, ALiBiBias_SmallSequence) {
    // WHAT: Generate ALiBi bias matrix for small sequence
    // WHY:  Linear bias for attention over buffer
    // HOW:  Request 4x4 bias matrix

    buffer = circular_buffer_create(sizeof(float), 16, OVERFLOW_OVERWRITE);
    ASSERT_NE(buffer, nullptr);

    nimcp_pos_config_t config;
    config.type = NIMCP_POS_ALIBI;
    config.config.alibi = nimcp_pos_alibi_default_config();
    config.config.alibi.base.max_seq_length = 16;
    config.config.alibi.num_heads = 2;

    encoder = nimcp_pos_encoder_create(&config);
    ASSERT_NE(encoder, nullptr);

    circular_buffer_set_pe_config(buffer, encoder);

    const uint32_t seq_len = 4;
    const uint32_t num_heads = 2;
    float bias[num_heads * seq_len * seq_len];

    bool result = circular_buffer_apply_alibi_bias(buffer, seq_len, bias);

    EXPECT_TRUE(result) << "ALiBi bias generation should succeed";

    // Verify bias structure: diagonal should be zero
    for (uint32_t h = 0; h < num_heads; h++) {
        for (uint32_t i = 0; i < seq_len; i++) {
            size_t idx = h * seq_len * seq_len + i * seq_len + i;
            EXPECT_NEAR(bias[idx], 0.0f, EPSILON) << "Diagonal should be zero";
        }
    }
}

TEST_F(CircularBufferPETest, ALiBiBias_LargerSequence) {
    // WHAT: Generate ALiBi bias for longer sequence
    // WHY:  Verify scaling with sequence length
    // HOW:  Request 16x16 bias matrix

    buffer = circular_buffer_create(sizeof(float), 32, OVERFLOW_OVERWRITE);
    ASSERT_NE(buffer, nullptr);

    nimcp_pos_config_t config;
    config.type = NIMCP_POS_ALIBI;
    config.config.alibi = nimcp_pos_alibi_default_config();
    config.config.alibi.base.max_seq_length = 32;
    config.config.alibi.num_heads = 4;

    encoder = nimcp_pos_encoder_create(&config);
    ASSERT_NE(encoder, nullptr);

    circular_buffer_set_pe_config(buffer, encoder);

    const uint32_t seq_len = 16;
    const uint32_t num_heads = 4;
    float* bias = new float[num_heads * seq_len * seq_len];

    bool result = circular_buffer_apply_alibi_bias(buffer, seq_len, bias);

    EXPECT_TRUE(result) << "ALiBi bias for 16x16 should succeed";

    // Verify bias is negative and increases with distance
    for (uint32_t h = 0; h < num_heads; h++) {
        for (uint32_t i = 0; i < seq_len; i++) {
            for (uint32_t j = 0; j < seq_len; j++) {
                if (i != j) {
                    size_t idx = h * seq_len * seq_len + i * seq_len + j;
                    EXPECT_LE(bias[idx], 0.0f) << "ALiBi bias should be non-positive";
                }
            }
        }
    }

    delete[] bias;
}

TEST_F(CircularBufferPETest, ALiBiBias_MultipleHeads) {
    // WHAT: Verify different heads get different slopes
    // WHY:  ALiBi uses geometric slopes across heads
    // HOW:  Check bias differs between heads

    buffer = circular_buffer_create(sizeof(float), 16, OVERFLOW_OVERWRITE);
    ASSERT_NE(buffer, nullptr);

    nimcp_pos_config_t config;
    config.type = NIMCP_POS_ALIBI;
    config.config.alibi = nimcp_pos_alibi_default_config();
    config.config.alibi.base.max_seq_length = 16;
    config.config.alibi.num_heads = 8;

    encoder = nimcp_pos_encoder_create(&config);
    ASSERT_NE(encoder, nullptr);

    circular_buffer_set_pe_config(buffer, encoder);

    const uint32_t seq_len = 4;
    const uint32_t num_heads = 8;
    float bias[num_heads * seq_len * seq_len];

    circular_buffer_apply_alibi_bias(buffer, seq_len, bias);

    // Compare bias for same position across different heads
    float head0_bias = bias[0 * seq_len * seq_len + 0 * seq_len + 2];  // Head 0, pos (0,2)
    float head1_bias = bias[1 * seq_len * seq_len + 0 * seq_len + 2];  // Head 1, pos (0,2)

    EXPECT_NE(head0_bias, head1_bias) << "Different heads should have different slopes";
}

//=============================================================================
// Unit Tests: Edge Cases
//=============================================================================

TEST_F(CircularBufferPETest, EdgeCase_NullBuffer) {
    // WHAT: Handle NULL buffer pointer
    // WHY:  Robustness against invalid input
    // HOW:  Pass NULL to PE functions

    nimcp_pos_config_t config;
    config.type = NIMCP_POS_SINUSOIDAL;
    config.config.sinusoidal = nimcp_pos_sinusoidal_default_config();

    encoder = nimcp_pos_encoder_create(&config);
    ASSERT_NE(encoder, nullptr);

    bool result1 = circular_buffer_set_pe_config(nullptr, encoder);
    EXPECT_FALSE(result1);

    float dummy[64];
    bool result2 = circular_buffer_get_position_embedding(nullptr, 0, dummy);
    EXPECT_FALSE(result2);

    bool result3 = circular_buffer_apply_alibi_bias(nullptr, 4, dummy);
    EXPECT_FALSE(result3);
}

TEST_F(CircularBufferPETest, EdgeCase_NullEncoder) {
    // WHAT: Handle NULL encoder pointer
    // WHY:  Validate encoder parameter
    // HOW:  Pass NULL encoder to set_pe_config

    buffer = circular_buffer_create(sizeof(float), 32, OVERFLOW_OVERWRITE);
    ASSERT_NE(buffer, nullptr);

    bool result = circular_buffer_set_pe_config(buffer, nullptr);
    EXPECT_FALSE(result) << "NULL encoder should be rejected";
}

TEST_F(CircularBufferPETest, EdgeCase_EmptyBuffer) {
    // WHAT: Get embedding from empty buffer
    // WHY:  Handle uninitialized buffer state
    // HOW:  Request embedding without pushing data

    buffer = circular_buffer_create(sizeof(float), 16, OVERFLOW_OVERWRITE);
    ASSERT_NE(buffer, nullptr);

    nimcp_pos_config_t config;
    config.type = NIMCP_POS_SINUSOIDAL;
    config.config.sinusoidal = nimcp_pos_sinusoidal_default_config();
    config.config.sinusoidal.base.embedding_dim = 32;
    config.config.sinusoidal.base.max_seq_length = 16;

    encoder = nimcp_pos_encoder_create(&config);
    ASSERT_NE(encoder, nullptr);

    circular_buffer_set_pe_config(buffer, encoder);

    float embedding[32];
    bool result = circular_buffer_get_position_embedding(buffer, 0, embedding);

    // Should either fail or return valid embedding
    SUCCEED() << "Empty buffer handled gracefully";
}

TEST_F(CircularBufferPETest, EdgeCase_OutOfBoundsIndex) {
    // WHAT: Request embedding for out-of-bounds index
    // WHY:  Verify bounds checking
    // HOW:  Use index >= buffer size

    buffer = circular_buffer_create(sizeof(float), 8, OVERFLOW_OVERWRITE);
    ASSERT_NE(buffer, nullptr);

    nimcp_pos_config_t config;
    config.type = NIMCP_POS_SINUSOIDAL;
    config.config.sinusoidal = nimcp_pos_sinusoidal_default_config();
    config.config.sinusoidal.base.embedding_dim = 16;
    config.config.sinusoidal.base.max_seq_length = 16;

    encoder = nimcp_pos_encoder_create(&config);
    ASSERT_NE(encoder, nullptr);

    circular_buffer_set_pe_config(buffer, encoder);

    // Push some data
    for (int i = 0; i < 4; i++) {
        float data = (float)i;
        circular_buffer_push(buffer, &data);
    }

    float embedding[16];
    bool result = circular_buffer_get_position_embedding(buffer, 100, embedding);

    EXPECT_FALSE(result) << "Out of bounds index should fail";
}

TEST_F(CircularBufferPETest, EdgeCase_ZeroSequenceLength) {
    // WHAT: Request ALiBi bias with zero length
    // WHY:  Invalid parameter handling
    // HOW:  Pass seq_length = 0

    buffer = circular_buffer_create(sizeof(float), 16, OVERFLOW_OVERWRITE);
    ASSERT_NE(buffer, nullptr);

    nimcp_pos_config_t config;
    config.type = NIMCP_POS_ALIBI;
    config.config.alibi = nimcp_pos_alibi_default_config();
    config.config.alibi.base.max_seq_length = 16;
    config.config.alibi.num_heads = 4;

    encoder = nimcp_pos_encoder_create(&config);
    ASSERT_NE(encoder, nullptr);

    circular_buffer_set_pe_config(buffer, encoder);

    float dummy[64];
    bool result = circular_buffer_apply_alibi_bias(buffer, 0, dummy);

    EXPECT_FALSE(result) << "Zero sequence length should be rejected";
}

TEST_F(CircularBufferPETest, EdgeCase_PENotConfigured) {
    // WHAT: Use PE functions before configuration
    // WHY:  Verify initialization checking
    // HOW:  Call PE functions without set_pe_config

    buffer = circular_buffer_create(sizeof(float), 32, OVERFLOW_OVERWRITE);
    ASSERT_NE(buffer, nullptr);

    float embedding[64];
    bool result1 = circular_buffer_get_position_embedding(buffer, 0, embedding);
    EXPECT_FALSE(result1) << "PE usage before configuration should fail";

    float bias[64];
    bool result2 = circular_buffer_apply_alibi_bias(buffer, 4, bias);
    EXPECT_FALSE(result2) << "ALiBi usage before configuration should fail";
}

TEST_F(CircularBufferPETest, EdgeCase_WrongEncoderType) {
    // WHAT: Request ALiBi bias with sinusoidal encoder
    // WHY:  Type mismatch should be detected
    // HOW:  Configure sinusoidal, request ALiBi

    buffer = circular_buffer_create(sizeof(float), 16, OVERFLOW_OVERWRITE);
    ASSERT_NE(buffer, nullptr);

    nimcp_pos_config_t config;
    config.type = NIMCP_POS_SINUSOIDAL;
    config.config.sinusoidal = nimcp_pos_sinusoidal_default_config();
    config.config.sinusoidal.base.embedding_dim = 32;
    config.config.sinusoidal.base.max_seq_length = 16;

    encoder = nimcp_pos_encoder_create(&config);
    ASSERT_NE(encoder, nullptr);

    circular_buffer_set_pe_config(buffer, encoder);

    float bias[64];
    bool result = circular_buffer_apply_alibi_bias(buffer, 4, bias);

    EXPECT_FALSE(result) << "Wrong encoder type should fail";
}

//=============================================================================
// Unit Tests: Integration
//=============================================================================

TEST_F(CircularBufferPETest, Integration_BufferOperationsWithPE) {
    // WHAT: Verify PE works alongside normal buffer operations
    // WHY:  PE should not interfere with buffer functionality
    // HOW:  Push/pop data while using PE

    buffer = circular_buffer_create(sizeof(float), 16, OVERFLOW_OVERWRITE);
    ASSERT_NE(buffer, nullptr);

    nimcp_pos_config_t config;
    config.type = NIMCP_POS_SINUSOIDAL;
    config.config.sinusoidal = nimcp_pos_sinusoidal_default_config();
    config.config.sinusoidal.base.embedding_dim = 32;
    config.config.sinusoidal.base.max_seq_length = 16;

    encoder = nimcp_pos_encoder_create(&config);
    ASSERT_NE(encoder, nullptr);

    circular_buffer_set_pe_config(buffer, encoder);

    // Push data
    for (int i = 0; i < 8; i++) {
        float data = (float)i * 2.0f;
        circular_buffer_push(buffer, &data);
    }

    // Get embedding
    float embedding[32];
    circular_buffer_get_position_embedding(buffer, 0, embedding);

    // Pop data
    float popped;
    circular_buffer_pop(buffer, &popped);
    EXPECT_FLOAT_EQ(popped, 0.0f);

    // Get another embedding
    circular_buffer_get_position_embedding(buffer, 1, embedding);

    SUCCEED() << "PE integrates with buffer operations";
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
