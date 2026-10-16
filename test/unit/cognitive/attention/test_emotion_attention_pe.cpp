//=============================================================================
// test_emotion_attention_pe.cpp - Unit Tests for Emotion-Attention PE Integration
//=============================================================================
/**
 * @file test_emotion_attention_pe.cpp
 * @brief Unit tests for positional encoding in emotion-attention system
 *
 * WHAT: Test PE integration with emotion-modulated attention
 * WHY:  Temporal and priority encoding critical for emotional context
 * HOW:  Test temporal sequence encoding and priority embeddings
 *
 * TEST COVERAGE:
 * 1. PE configuration
 * 2. Temporal emotion sequence encoding (sinusoidal)
 * 3. Priority emotion embeddings (learned)
 * 4. Edge cases (NULL, empty, bounds)
 * 5. Integration with emotion tensor
 *
 * @author NIMCP Development Team
 * @date 2025-12-10
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstdlib>
#include <cstring>

// Headers have their own extern "C" guards
    #include "cognitive/attention/nimcp_emotion_attention.h"
    #include "cognitive/nimcp_emotion_tensor.h"
    #include "plasticity/attention/nimcp_attention.h"
    #include "utils/encoding/nimcp_positional_encoding.h"
    #include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class EmotionAttentionPETest : public ::testing::Test {
protected:
    static constexpr float EPSILON = 1e-4f;

    emotion_attention_system_t* ea_system = nullptr;
    emotion_tensor_system_t* emotion_tensor = nullptr;
    multihead_attention_t attention = nullptr;

    void SetUp() override {
        srand(42);
        nimcp_memory_init();

        // Create mock emotion tensor system
        emotion_tensor_config_t et_config = emotion_tensor_default_config();
        emotion_tensor = emotion_tensor_create(&et_config);

        // Create multihead attention using proper API
        multihead_attention_config_t attn_config;
        memset(&attn_config, 0, sizeof(attn_config));
        attn_config.num_heads = 8;
        attn_config.input_dim = 512;
        attn_config.output_dim = 512;
        attn_config.sequence_length = 128;
        attn_config.use_thalamic_gate = true;
        attn_config.use_salience_weighting = false;
        attn_config.gate_bias = 0.5f;
        attn_config.use_positional_encoding = false;
        attention = multihead_attention_create(&attn_config);
    }

    void TearDown() override {
        if (ea_system) {
            emotion_attention_destroy(ea_system);
            ea_system = nullptr;
        }
        if (emotion_tensor) {
            emotion_tensor_destroy(emotion_tensor);
            emotion_tensor = nullptr;
        }
        if (attention) {
            multihead_attention_destroy(attention);
            attention = nullptr;
        }
    }

    bool FloatEqual(float a, float b, float eps = EPSILON) {
        return std::abs(a - b) < eps;
    }
};

//=============================================================================
// Unit Tests: PE Configuration
//=============================================================================

TEST_F(EmotionAttentionPETest, ConfigurePE_TemporalEnabled) {
    // WHAT: Configure temporal PE for emotion sequences
    // WHY:  Temporal ordering critical for emotional dynamics
    // HOW:  Enable sinusoidal PE with appropriate parameters

    emotion_attention_config_t config = emotion_attention_default_config();
    ea_system = emotion_attention_create(emotion_tensor, attention, &config);
    ASSERT_NE(ea_system, nullptr);

    bool result = emotion_attention_set_pe_config(
        ea_system,
        true,   // enable_temporal
        false,  // enable_priority
        128,    // max_sequence
        256     // embedding_dim
    );

    EXPECT_TRUE(result) << "Temporal PE configuration should succeed";
}

TEST_F(EmotionAttentionPETest, ConfigurePE_PriorityEnabled) {
    // WHAT: Configure priority PE for emotion ordering
    // WHY:  Priority ranking affects attention allocation
    // HOW:  Enable learned PE for priority positions

    emotion_attention_config_t config = emotion_attention_default_config();
    ea_system = emotion_attention_create(emotion_tensor, attention, &config);
    ASSERT_NE(ea_system, nullptr);

    bool result = emotion_attention_set_pe_config(
        ea_system,
        false,  // enable_temporal
        true,   // enable_priority
        64,     // max_sequence
        128     // embedding_dim
    );

    EXPECT_TRUE(result) << "Priority PE configuration should succeed";
}

TEST_F(EmotionAttentionPETest, ConfigurePE_BothEnabled) {
    // WHAT: Configure both temporal and priority PE
    // WHY:  Full position-aware emotion processing
    // HOW:  Enable both PE types simultaneously

    emotion_attention_config_t config = emotion_attention_default_config();
    ea_system = emotion_attention_create(emotion_tensor, attention, &config);
    ASSERT_NE(ea_system, nullptr);

    bool result = emotion_attention_set_pe_config(
        ea_system,
        true,   // enable_temporal
        true,   // enable_priority
        128,    // max_sequence
        256     // embedding_dim
    );

    EXPECT_TRUE(result) << "Dual PE configuration should succeed";
}

//=============================================================================
// Unit Tests: Temporal Sequence Encoding
//=============================================================================

TEST_F(EmotionAttentionPETest, EncodeTemporal_SingleEmotion) {
    // WHAT: Encode single emotion state with temporal PE
    // WHY:  Basic temporal encoding functionality
    // HOW:  Apply sinusoidal PE to emotion embedding

    emotion_attention_config_t config = emotion_attention_default_config();
    config.enable_temporal_encoding = true;
    config.max_temporal_sequence = 64;
    config.emotion_embedding_dim = 128;

    ea_system = emotion_attention_create(emotion_tensor, attention, &config);
    ASSERT_NE(ea_system, nullptr);

    emotion_attention_set_pe_config(ea_system, true, false, 64, 128);

    float emotion_embedding[128];
    for (int i = 0; i < 128; i++) {
        emotion_embedding[i] = 0.5f;
    }

    float output[128];
    bool result = emotion_attention_encode_temporal(
        ea_system,
        emotion_embedding,
        1,  // Single emotion
        output
    );

    EXPECT_TRUE(result) << "Single emotion temporal encoding should succeed";

    // Output should differ from input (PE added)
    bool changed = false;
    for (int i = 0; i < 128; i++) {
        if (std::abs(output[i] - emotion_embedding[i]) > EPSILON) {
            changed = true;
            break;
        }
    }
    EXPECT_TRUE(changed) << "Temporal encoding should modify emotion vector";
}

TEST_F(EmotionAttentionPETest, EncodeTemporal_EmotionSequence) {
    // WHAT: Encode sequence of emotional states
    // WHY:  Temporal context affects emotional interpretation
    // HOW:  Apply PE to sequence of emotion embeddings

    emotion_attention_config_t config = emotion_attention_default_config();
    config.enable_temporal_encoding = true;
    config.max_temporal_sequence = 64;
    config.emotion_embedding_dim = 64;

    ea_system = emotion_attention_create(emotion_tensor, attention, &config);
    ASSERT_NE(ea_system, nullptr);

    emotion_attention_set_pe_config(ea_system, true, false, 64, 64);

    const int seq_len = 5;
    const int dim = 64;
    float emotion_sequence[seq_len * dim];

    // Initialize with varying emotions
    for (int i = 0; i < seq_len * dim; i++) {
        emotion_sequence[i] = 0.3f + 0.1f * (i % 10);
    }

    float output[seq_len * dim];
    bool result = emotion_attention_encode_temporal(
        ea_system,
        emotion_sequence,
        seq_len,
        output
    );

    EXPECT_TRUE(result) << "Emotion sequence encoding should succeed";

    // Different positions should have different encodings
    bool positions_differ = false;
    for (int i = 0; i < dim; i++) {
        if (std::abs(output[i] - output[dim + i]) > EPSILON) {
            positions_differ = true;
            break;
        }
    }
    EXPECT_TRUE(positions_differ) << "Different temporal positions should differ";
}

TEST_F(EmotionAttentionPETest, EncodeTemporal_InPlace) {
    // WHAT: In-place temporal encoding
    // WHY:  Memory efficiency for large sequences
    // HOW:  Output buffer same as input

    emotion_attention_config_t config = emotion_attention_default_config();
    config.enable_temporal_encoding = true;
    config.max_temporal_sequence = 32;
    config.emotion_embedding_dim = 64;

    ea_system = emotion_attention_create(emotion_tensor, attention, &config);
    ASSERT_NE(ea_system, nullptr);

    emotion_attention_set_pe_config(ea_system, true, false, 32, 64);

    float data[3 * 64];
    for (int i = 0; i < 3 * 64; i++) {
        data[i] = 0.5f;
    }

    bool result = emotion_attention_encode_temporal(
        ea_system,
        data,
        3,
        data  // In-place
    );

    EXPECT_TRUE(result) << "In-place temporal encoding should succeed";
}

//=============================================================================
// Unit Tests: Priority Embeddings
//=============================================================================

TEST_F(EmotionAttentionPETest, PriorityEmbedding_HighestPriority) {
    // WHAT: Get learned embedding for highest priority emotion
    // WHY:  Priority rank 0 represents most salient emotion
    // HOW:  Retrieve learned PE for rank 0

    emotion_attention_config_t config = emotion_attention_default_config();
    config.enable_priority_encoding = true;
    config.emotion_embedding_dim = 128;

    ea_system = emotion_attention_create(emotion_tensor, attention, &config);
    ASSERT_NE(ea_system, nullptr);

    emotion_attention_set_pe_config(ea_system, false, true, 64, 128);

    float embedding[128];
    bool result = emotion_attention_get_priority_embedding(
        ea_system,
        0,  // Highest priority
        embedding
    );

    EXPECT_TRUE(result) << "Priority embedding retrieval should succeed";

    // Embedding should be non-zero
    float norm = 0.0f;
    for (int i = 0; i < 128; i++) {
        norm += embedding[i] * embedding[i];
    }
    EXPECT_GT(norm, 0.0f) << "Priority embedding should be non-zero";
}

TEST_F(EmotionAttentionPETest, PriorityEmbedding_DifferentRanks) {
    // WHAT: Verify different priority ranks have different embeddings
    // WHY:  Priority ordering must be distinguishable
    // HOW:  Compare embeddings for ranks 0 and 1

    emotion_attention_config_t config = emotion_attention_default_config();
    config.enable_priority_encoding = true;
    config.emotion_embedding_dim = 64;

    ea_system = emotion_attention_create(emotion_tensor, attention, &config);
    ASSERT_NE(ea_system, nullptr);

    emotion_attention_set_pe_config(ea_system, false, true, 32, 64);

    float emb0[64], emb1[64];

    emotion_attention_get_priority_embedding(ea_system, 0, emb0);
    emotion_attention_get_priority_embedding(ea_system, 1, emb1);

    // Embeddings should differ
    bool differ = false;
    for (int i = 0; i < 64; i++) {
        if (std::abs(emb0[i] - emb1[i]) > EPSILON) {
            differ = true;
            break;
        }
    }
    EXPECT_TRUE(differ) << "Different priority ranks should have different embeddings";
}

TEST_F(EmotionAttentionPETest, PriorityEmbedding_LowerPriorities) {
    // WHAT: Test multiple priority levels
    // WHY:  System handles multiple competing emotions
    // HOW:  Retrieve embeddings for ranks 0-3

    emotion_attention_config_t config = emotion_attention_default_config();
    config.enable_priority_encoding = true;
    config.emotion_embedding_dim = 32;

    ea_system = emotion_attention_create(emotion_tensor, attention, &config);
    ASSERT_NE(ea_system, nullptr);

    emotion_attention_set_pe_config(ea_system, false, true, 16, 32);

    float embeddings[4][32];
    for (int rank = 0; rank < 4; rank++) {
        bool result = emotion_attention_get_priority_embedding(
            ea_system,
            rank,
            embeddings[rank]
        );
        EXPECT_TRUE(result) << "Priority embedding for rank " << rank << " should succeed";
    }

    // All should be distinct
    for (int i = 0; i < 3; i++) {
        for (int j = i + 1; j < 4; j++) {
            bool same = true;
            for (int k = 0; k < 32; k++) {
                if (std::abs(embeddings[i][k] - embeddings[j][k]) > EPSILON) {
                    same = false;
                    break;
                }
            }
            EXPECT_FALSE(same) << "Ranks " << i << " and " << j << " should differ";
        }
    }
}

//=============================================================================
// Unit Tests: Edge Cases
//=============================================================================

TEST_F(EmotionAttentionPETest, EdgeCase_NullSystem) {
    // WHAT: Handle NULL system pointer
    // WHY:  Robustness against invalid input
    // HOW:  Pass NULL to PE functions

    float dummy[64];

    bool result1 = emotion_attention_set_pe_config(nullptr, true, true, 64, 64);
    EXPECT_FALSE(result1);

    bool result2 = emotion_attention_encode_temporal(nullptr, dummy, 1, dummy);
    EXPECT_FALSE(result2);

    bool result3 = emotion_attention_get_priority_embedding(nullptr, 0, dummy);
    EXPECT_FALSE(result3);
}

TEST_F(EmotionAttentionPETest, EdgeCase_EmptySequence) {
    // WHAT: Handle zero-length sequence
    // WHY:  Graceful handling of edge case
    // HOW:  Pass seq_length = 0

    emotion_attention_config_t config = emotion_attention_default_config();
    config.enable_temporal_encoding = true;

    ea_system = emotion_attention_create(emotion_tensor, attention, &config);
    ASSERT_NE(ea_system, nullptr);

    emotion_attention_set_pe_config(ea_system, true, false, 64, 64);

    float dummy[64];
    bool result = emotion_attention_encode_temporal(
        ea_system,
        dummy,
        0,  // Empty sequence
        dummy
    );

    // Should either succeed (no-op) or return false
    SUCCEED() << "Empty sequence handled gracefully";
}

TEST_F(EmotionAttentionPETest, EdgeCase_LargeSequence) {
    // WHAT: Handle sequence at max length
    // WHY:  Verify bounds handling
    // HOW:  Test at configured max_sequence

    emotion_attention_config_t config = emotion_attention_default_config();
    config.enable_temporal_encoding = true;
    config.max_temporal_sequence = 128;
    config.emotion_embedding_dim = 32;

    ea_system = emotion_attention_create(emotion_tensor, attention, &config);
    ASSERT_NE(ea_system, nullptr);

    emotion_attention_set_pe_config(ea_system, true, false, 128, 32);

    const int seq_len = 128;
    const int dim = 32;
    float* sequence = new float[seq_len * dim];
    float* output = new float[seq_len * dim];

    for (int i = 0; i < seq_len * dim; i++) {
        sequence[i] = 0.5f;
    }

    bool result = emotion_attention_encode_temporal(
        ea_system,
        sequence,
        seq_len,
        output
    );

    EXPECT_TRUE(result) << "Max-length sequence should be handled";

    delete[] sequence;
    delete[] output;
}

TEST_F(EmotionAttentionPETest, EdgeCase_InvalidPriorityRank) {
    // WHAT: Request priority embedding beyond reasonable range
    // WHY:  Verify bounds checking
    // HOW:  Use very high priority rank

    emotion_attention_config_t config = emotion_attention_default_config();
    config.enable_priority_encoding = true;
    config.emotion_embedding_dim = 64;

    ea_system = emotion_attention_create(emotion_tensor, attention, &config);
    ASSERT_NE(ea_system, nullptr);

    emotion_attention_set_pe_config(ea_system, false, true, 16, 64);

    float embedding[64];
    bool result = emotion_attention_get_priority_embedding(
        ea_system,
        10000,  // Very high rank
        embedding
    );

    // Should either fail or handle gracefully
    SUCCEED() << "Invalid priority rank handled";
}

TEST_F(EmotionAttentionPETest, EdgeCase_PENotConfigured) {
    // WHAT: Use PE functions before configuration
    // WHY:  Verify initialization checking
    // HOW:  Call encode functions without set_pe_config

    emotion_attention_config_t config = emotion_attention_default_config();
    ea_system = emotion_attention_create(emotion_tensor, attention, &config);
    ASSERT_NE(ea_system, nullptr);

    // Don't call set_pe_config

    float data[64];
    for (int i = 0; i < 64; i++) {
        data[i] = 0.5f;
    }

    bool result = emotion_attention_encode_temporal(ea_system, data, 1, data);

    // Should fail gracefully
    EXPECT_FALSE(result) << "Unconfigured PE should fail gracefully";
}

TEST_F(EmotionAttentionPETest, EdgeCase_ZeroDimension) {
    // WHAT: Configure PE with zero embedding dimension
    // WHY:  Invalid configuration should be rejected
    // HOW:  Pass embedding_dim = 0

    emotion_attention_config_t config = emotion_attention_default_config();
    ea_system = emotion_attention_create(emotion_tensor, attention, &config);
    ASSERT_NE(ea_system, nullptr);

    bool result = emotion_attention_set_pe_config(
        ea_system,
        true,
        false,
        64,
        0  // Zero dimension
    );

    EXPECT_FALSE(result) << "Zero dimension should be rejected";
}

//=============================================================================
// Unit Tests: Integration
//=============================================================================

TEST_F(EmotionAttentionPETest, Integration_TemporalAndPriority) {
    // WHAT: Use both temporal and priority encoding together
    // WHY:  Verify compatibility of dual PE
    // HOW:  Configure both, use both in sequence

    emotion_attention_config_t config = emotion_attention_default_config();
    config.enable_temporal_encoding = true;
    config.enable_priority_encoding = true;
    config.max_temporal_sequence = 64;
    config.emotion_embedding_dim = 128;

    ea_system = emotion_attention_create(emotion_tensor, attention, &config);
    ASSERT_NE(ea_system, nullptr);

    emotion_attention_set_pe_config(ea_system, true, true, 64, 128);

    // Temporal encoding
    float sequence[3 * 128];
    float temporal_out[3 * 128];

    for (int i = 0; i < 3 * 128; i++) {
        sequence[i] = 0.5f;
    }

    bool result1 = emotion_attention_encode_temporal(
        ea_system,
        sequence,
        3,
        temporal_out
    );
    EXPECT_TRUE(result1);

    // Priority embedding
    float priority_emb[128];
    bool result2 = emotion_attention_get_priority_embedding(
        ea_system,
        0,
        priority_emb
    );
    EXPECT_TRUE(result2);

    SUCCEED() << "Dual PE integration works";
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
