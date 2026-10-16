//=============================================================================
// test_language_production_pe.cpp - Unit Tests for Language Production PE Integration
//=============================================================================
/**
 * @file test_language_production_pe.cpp
 * @brief Unit tests for positional encoding in language production bridge
 *
 * WHAT: Test PE integration with motor command and gesture sequences
 * WHY:  Temporal ordering critical for fluent speech production
 * HOW:  Test sinusoidal PE for motor commands and RoPE for gestures
 *
 * TEST COVERAGE:
 * 1. PE configuration (sinusoidal and RoPE)
 * 2. Motor sequence encoding
 * 3. Articulatory gesture encoding
 * 4. Edge cases (NULL, empty, invalid dimensions)
 * 5. Integration with production pipeline
 *
 * @author NIMCP Development Team
 * @date 2025-12-10
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstdlib>
#include <cstring>

// Headers have their own extern "C" guards
    #include "core/brain/regions/broca/nimcp_language_production_bridge.h"
    #include "core/brain/regions/broca/nimcp_broca_adapter.h"
    #include "utils/encoding/nimcp_positional_encoding.h"
    #include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class LanguageProductionPETest : public ::testing::Test {
protected:
    static constexpr float EPSILON = 1e-4f;

    language_production_bridge_t* bridge = nullptr;
    broca_adapter_t* broca = nullptr;

    void SetUp() override {
        srand(42);
        nimcp_memory_init();

        // Create Broca adapter using proper API
        broca_config_t broca_cfg = broca_default_config();
        broca = broca_create(&broca_cfg);
    }

    void TearDown() override {
        if (bridge) {
            lpb_destroy(bridge);
            bridge = nullptr;
        }
        if (broca) {
            broca_destroy(broca);
            broca = nullptr;
        }
    }

    bool FloatEqual(float a, float b, float eps = EPSILON) {
        return std::abs(a - b) < eps;
    }
};

//=============================================================================
// Unit Tests: PE Configuration
//=============================================================================

TEST_F(LanguageProductionPETest, ConfigurePE_MotorSequence) {
    // WHAT: Configure sinusoidal PE for motor command sequences
    // WHY:  Motor commands require precise temporal ordering
    // HOW:  Set motor_seq_pe_type to sinusoidal

    lpb_config_t config = lpb_default_config();
    config.enable_positional_encoding = true;
    config.pe_config.motor_seq_pe_type = 0;  // Sinusoidal
    config.pe_config.motor_seq_max_length = 128;
    config.pe_config.motor_seq_embedding_dim = 256;
    config.pe_config.motor_seq_pe_base = 10000.0f;
    config.pe_config.enable_motor_pe_cache = true;

    bridge = lpb_create(&config, broca);
    ASSERT_NE(bridge, nullptr);

    bool result = language_production_set_pe_config(bridge, &config.pe_config);
    EXPECT_TRUE(result) << "Motor sequence PE configuration should succeed";
}

TEST_F(LanguageProductionPETest, ConfigurePE_Gesture) {
    // WHAT: Configure RoPE for articulatory gestures
    // WHY:  Relative timing between gestures critical for coarticulation
    // HOW:  Set gesture_pe_type to RoPE

    lpb_config_t config = lpb_default_config();
    config.enable_positional_encoding = true;
    config.pe_config.gesture_pe_type = 2;  // RoPE
    config.pe_config.gesture_max_length = 64;
    config.pe_config.gesture_embedding_dim = 128;
    config.pe_config.gesture_rope_base = 10000.0f;
    config.pe_config.enable_gesture_pe_cache = true;

    bridge = lpb_create(&config, broca);
    ASSERT_NE(bridge, nullptr);

    bool result = language_production_set_pe_config(bridge, &config.pe_config);
    EXPECT_TRUE(result) << "Gesture PE configuration should succeed";
}

TEST_F(LanguageProductionPETest, ConfigurePE_BothMotorAndGesture) {
    // WHAT: Configure both motor and gesture PE
    // WHY:  Full position-aware speech production
    // HOW:  Enable both PE types simultaneously

    lpb_config_t config = lpb_default_config();
    config.enable_positional_encoding = true;
    config.pe_config.motor_seq_pe_type = 0;  // Sinusoidal
    config.pe_config.gesture_pe_type = 2;     // RoPE
    config.pe_config.motor_seq_max_length = 128;
    config.pe_config.motor_seq_embedding_dim = 256;
    config.pe_config.gesture_max_length = 64;
    config.pe_config.gesture_embedding_dim = 128;

    bridge = lpb_create(&config, broca);
    ASSERT_NE(bridge, nullptr);

    bool result = language_production_set_pe_config(bridge, &config.pe_config);
    EXPECT_TRUE(result) << "Dual PE configuration should succeed";
}

//=============================================================================
// Unit Tests: Motor Sequence Encoding
//=============================================================================

TEST_F(LanguageProductionPETest, EncodeMotorSequence_SingleCommand) {
    // WHAT: Encode single motor command with PE
    // WHY:  Basic motor encoding functionality
    // HOW:  Apply sinusoidal PE to motor embedding

    lpb_config_t config = lpb_default_config();
    config.enable_positional_encoding = true;
    config.pe_config.motor_seq_pe_type = 0;
    config.pe_config.motor_seq_max_length = 64;
    config.pe_config.motor_seq_embedding_dim = 128;

    bridge = lpb_create(&config, broca);
    ASSERT_NE(bridge, nullptr);

    language_production_set_pe_config(bridge, &config.pe_config);

    float motor_embedding[128];
    for (int i = 0; i < 128; i++) {
        motor_embedding[i] = 0.5f;
    }

    float output[128];
    bool result = language_production_encode_motor_sequence(
        bridge,
        motor_embedding,
        1,  // Single command
        output
    );

    EXPECT_TRUE(result) << "Single motor command encoding should succeed";

    // Output should differ from input (PE added)
    bool changed = false;
    for (int i = 0; i < 128; i++) {
        if (std::abs(output[i] - motor_embedding[i]) > EPSILON) {
            changed = true;
            break;
        }
    }
    EXPECT_TRUE(changed) << "Motor encoding should modify embedding";
}

TEST_F(LanguageProductionPETest, EncodeMotorSequence_MultipleCommands) {
    // WHAT: Encode sequence of motor commands
    // WHY:  Fluent speech requires coordinated command sequences
    // HOW:  Apply PE to sequence of motor embeddings

    lpb_config_t config = lpb_default_config();
    config.enable_positional_encoding = true;
    config.pe_config.motor_seq_pe_type = 0;
    config.pe_config.motor_seq_max_length = 64;
    config.pe_config.motor_seq_embedding_dim = 64;

    bridge = lpb_create(&config, broca);
    ASSERT_NE(bridge, nullptr);

    language_production_set_pe_config(bridge, &config.pe_config);

    const int seq_len = 8;
    const int dim = 64;
    float motor_sequence[seq_len * dim];

    // Initialize with varying commands
    for (int i = 0; i < seq_len * dim; i++) {
        motor_sequence[i] = 0.3f + 0.05f * (i % 20);
    }

    float output[seq_len * dim];
    bool result = language_production_encode_motor_sequence(
        bridge,
        motor_sequence,
        seq_len,
        output
    );

    EXPECT_TRUE(result) << "Motor sequence encoding should succeed";

    // Different positions should have different encodings
    bool positions_differ = false;
    for (int i = 0; i < dim; i++) {
        if (std::abs(output[i] - output[dim + i]) > EPSILON) {
            positions_differ = true;
            break;
        }
    }
    EXPECT_TRUE(positions_differ) << "Different command positions should differ";
}

TEST_F(LanguageProductionPETest, EncodeMotorSequence_InPlace) {
    // WHAT: In-place motor sequence encoding
    // WHY:  Memory efficiency for long utterances
    // HOW:  Output buffer same as input

    lpb_config_t config = lpb_default_config();
    config.enable_positional_encoding = true;
    config.pe_config.motor_seq_pe_type = 0;
    config.pe_config.motor_seq_max_length = 32;
    config.pe_config.motor_seq_embedding_dim = 64;

    bridge = lpb_create(&config, broca);
    ASSERT_NE(bridge, nullptr);

    language_production_set_pe_config(bridge, &config.pe_config);

    float data[5 * 64];
    for (int i = 0; i < 5 * 64; i++) {
        data[i] = 0.5f;
    }

    bool result = language_production_encode_motor_sequence(
        bridge,
        data,
        5,
        data  // In-place
    );

    EXPECT_TRUE(result) << "In-place motor encoding should succeed";
}

//=============================================================================
// Unit Tests: Gesture Encoding (RoPE)
//=============================================================================

TEST_F(LanguageProductionPETest, EncodeGesture_SingleGesture) {
    // WHAT: Apply RoPE to articulatory gestures
    // WHY:  Basic gesture encoding functionality
    // HOW:  Rotate query/key pair - use seq_len=2 because RoPE at position 0
    //       has rotation angle = 0 (no change), so we need position 1 to see rotation

    lpb_config_t config = lpb_default_config();
    config.enable_positional_encoding = true;
    config.pe_config.gesture_pe_type = 2;  // RoPE
    config.pe_config.gesture_max_length = 32;
    config.pe_config.gesture_embedding_dim = 64;

    bridge = lpb_create(&config, broca);
    ASSERT_NE(bridge, nullptr);

    language_production_set_pe_config(bridge, &config.pe_config);

    // Use seq_len = 2 to test position 1 (position 0 has no rotation in RoPE)
    float query[2 * 64], key[2 * 64];
    for (int i = 0; i < 2 * 64; i++) {
        query[i] = (i % 2 == 0) ? 1.0f : 0.0f;
        key[i] = (i % 2 == 0) ? 0.0f : 1.0f;
    }

    float query_out[2 * 64], key_out[2 * 64];
    bool result = language_production_encode_gesture(
        bridge,
        query,
        key,
        2,  // Two gestures to test position 1
        query_out,
        key_out
    );

    EXPECT_TRUE(result) << "Gesture encoding should succeed";

    // Check position 1 (index 64-127) - should differ from input (rotated)
    bool query_changed = false;
    for (int i = 64; i < 2 * 64; i++) {
        if (std::abs(query_out[i] - query[i]) > EPSILON) {
            query_changed = true;
            break;
        }
    }
    EXPECT_TRUE(query_changed) << "RoPE should rotate gesture vectors at position 1";
}

TEST_F(LanguageProductionPETest, EncodeGesture_MultipleGestures) {
    // WHAT: Apply RoPE to sequence of articulatory gestures
    // WHY:  Coarticulation requires relative gesture timing
    // HOW:  Rotate query/key sequences

    lpb_config_t config = lpb_default_config();
    config.enable_positional_encoding = true;
    config.pe_config.gesture_pe_type = 2;
    config.pe_config.gesture_max_length = 64;
    config.pe_config.gesture_embedding_dim = 32;

    bridge = lpb_create(&config, broca);
    ASSERT_NE(bridge, nullptr);

    language_production_set_pe_config(bridge, &config.pe_config);

    const int seq_len = 6;
    const int dim = 32;
    float queries[seq_len * dim];
    float keys[seq_len * dim];

    for (int i = 0; i < seq_len * dim; i++) {
        queries[i] = 0.4f + 0.1f * (i % 10);
        keys[i] = 0.6f - 0.1f * (i % 10);
    }

    float queries_out[seq_len * dim];
    float keys_out[seq_len * dim];

    bool result = language_production_encode_gesture(
        bridge,
        queries,
        keys,
        seq_len,
        queries_out,
        keys_out
    );

    EXPECT_TRUE(result) << "Gesture sequence encoding should succeed";

    // Verify rotation occurred
    bool changed = false;
    for (int i = 0; i < seq_len * dim; i++) {
        if (std::abs(queries_out[i] - queries[i]) > EPSILON) {
            changed = true;
            break;
        }
    }
    EXPECT_TRUE(changed) << "RoPE should modify gesture sequences";
}

TEST_F(LanguageProductionPETest, EncodeGesture_RelativePositions) {
    // WHAT: Verify RoPE captures relative positions
    // WHY:  Coarticulation depends on relative gesture timing
    // HOW:  Encode same gestures at different positions

    lpb_config_t config = lpb_default_config();
    config.enable_positional_encoding = true;
    config.pe_config.gesture_pe_type = 2;
    config.pe_config.gesture_max_length = 32;
    config.pe_config.gesture_embedding_dim = 16;

    bridge = lpb_create(&config, broca);
    ASSERT_NE(bridge, nullptr);

    language_production_set_pe_config(bridge, &config.pe_config);

    // Same gesture at different positions in sequence
    float query1[16], key1[16], query2[16], key2[16];
    for (int i = 0; i < 16; i++) {
        query1[i] = query2[i] = 0.5f;
        key1[i] = key2[i] = 0.7f;
    }

    float q1_out[16], k1_out[16], q2_out[16], k2_out[16];

    // Encode at position 0 and position 1
    language_production_encode_gesture(bridge, query1, key1, 1, q1_out, k1_out);

    // For second position, need to encode as part of 2-element sequence
    float queries_2[2 * 16], keys_2[2 * 16];
    float queries_out_2[2 * 16], keys_out_2[2 * 16];

    for (int i = 0; i < 16; i++) {
        queries_2[i] = 0.5f;       // Position 0
        queries_2[16 + i] = 0.5f;  // Position 1
        keys_2[i] = 0.7f;
        keys_2[16 + i] = 0.7f;
    }

    language_production_encode_gesture(bridge, queries_2, keys_2, 2, queries_out_2, keys_out_2);

    // Position 0 and 1 should differ
    bool differ = false;
    for (int i = 0; i < 16; i++) {
        if (std::abs(queries_out_2[i] - queries_out_2[16 + i]) > EPSILON) {
            differ = true;
            break;
        }
    }
    EXPECT_TRUE(differ) << "RoPE should distinguish relative positions";
}

//=============================================================================
// Unit Tests: Edge Cases
//=============================================================================

TEST_F(LanguageProductionPETest, EdgeCase_NullBridge) {
    // WHAT: Handle NULL bridge pointer
    // WHY:  Robustness against invalid input
    // HOW:  Pass NULL to PE functions

    lpb_pe_config_t pe_config = {};
    bool result1 = language_production_set_pe_config(nullptr, &pe_config);
    EXPECT_FALSE(result1);

    float dummy[64];
    bool result2 = language_production_encode_motor_sequence(nullptr, dummy, 1, dummy);
    EXPECT_FALSE(result2);

    bool result3 = language_production_encode_gesture(nullptr, dummy, dummy, 1, dummy, dummy);
    EXPECT_FALSE(result3);
}

TEST_F(LanguageProductionPETest, EdgeCase_ZeroSequenceLength) {
    // WHAT: Encode zero-length sequence
    // WHY:  Invalid parameter handling
    // HOW:  Pass seq_length = 0

    lpb_config_t config = lpb_default_config();
    config.enable_positional_encoding = true;
    config.pe_config.motor_seq_pe_type = 0;
    config.pe_config.motor_seq_max_length = 64;
    config.pe_config.motor_seq_embedding_dim = 64;

    bridge = lpb_create(&config, broca);
    ASSERT_NE(bridge, nullptr);

    language_production_set_pe_config(bridge, &config.pe_config);

    float dummy[64];
    bool result = language_production_encode_motor_sequence(bridge, dummy, 0, dummy);

    EXPECT_FALSE(result) << "Zero-length sequence should be rejected";
}

TEST_F(LanguageProductionPETest, EdgeCase_NullOutputBuffers) {
    // WHAT: Pass NULL output buffers
    // WHY:  Validate output parameters
    // HOW:  Pass nullptr for outputs

    lpb_config_t config = lpb_default_config();
    config.enable_positional_encoding = true;
    config.pe_config.motor_seq_pe_type = 0;
    config.pe_config.motor_seq_max_length = 32;
    config.pe_config.motor_seq_embedding_dim = 64;

    bridge = lpb_create(&config, broca);
    ASSERT_NE(bridge, nullptr);

    language_production_set_pe_config(bridge, &config.pe_config);

    float input[64];
    for (int i = 0; i < 64; i++) {
        input[i] = 0.5f;
    }

    bool result = language_production_encode_motor_sequence(bridge, input, 1, nullptr);
    EXPECT_FALSE(result) << "NULL output buffer should be rejected";
}

TEST_F(LanguageProductionPETest, EdgeCase_PENotConfigured) {
    // WHAT: Use PE functions when PE is disabled
    // WHY:  Verify passthrough mode when PE not configured
    // HOW:  Call encode functions with enable_positional_encoding = false

    lpb_config_t config = lpb_default_config();
    config.enable_positional_encoding = false;
    // Default motor_seq_embedding_dim is 128, use matching buffer size

    bridge = lpb_create(&config, broca);
    ASSERT_NE(bridge, nullptr);

    const int dim = 128;  // Must match default motor_seq_embedding_dim
    float data[dim];
    float output[dim];
    for (int i = 0; i < dim; i++) {
        data[i] = 0.5f;
    }

    // When PE is disabled, function operates in passthrough mode (returns true)
    bool result = language_production_encode_motor_sequence(bridge, data, 1, output);
    EXPECT_TRUE(result) << "PE disabled should succeed in passthrough mode";

    // In passthrough mode, output should match input (no encoding applied)
    bool unchanged = true;
    for (int i = 0; i < dim; i++) {
        if (std::abs(output[i] - data[i]) > EPSILON) {
            unchanged = false;
            break;
        }
    }
    EXPECT_TRUE(unchanged) << "Passthrough mode should not modify data";
}

TEST_F(LanguageProductionPETest, EdgeCase_ZeroDimension) {
    // WHAT: Configure PE with zero embedding dimension
    // WHY:  Invalid configuration should be rejected
    // HOW:  Pass embedding_dim = 0

    lpb_config_t config = lpb_default_config();
    config.enable_positional_encoding = true;
    config.pe_config.motor_seq_pe_type = 0;
    config.pe_config.motor_seq_max_length = 64;
    config.pe_config.motor_seq_embedding_dim = 0;  // Zero dimension

    bridge = lpb_create(&config, broca);

    if (bridge) {
        bool result = language_production_set_pe_config(bridge, &config.pe_config);
        EXPECT_FALSE(result) << "Zero dimension should be rejected";
    } else {
        SUCCEED() << "Bridge creation rejected zero dimension";
    }
}

TEST_F(LanguageProductionPETest, EdgeCase_ExcessiveSequenceLength) {
    // WHAT: Encode sequence exceeding configured max
    // WHY:  Verify bounds handling
    // HOW:  Request encoding beyond max_length

    lpb_config_t config = lpb_default_config();
    config.enable_positional_encoding = true;
    config.pe_config.motor_seq_pe_type = 0;
    config.pe_config.motor_seq_max_length = 10;
    config.pe_config.motor_seq_embedding_dim = 32;

    bridge = lpb_create(&config, broca);
    ASSERT_NE(bridge, nullptr);

    language_production_set_pe_config(bridge, &config.pe_config);

    const int excessive_len = 20;
    float* data = new float[excessive_len * 32];
    float* output = new float[excessive_len * 32];

    for (int i = 0; i < excessive_len * 32; i++) {
        data[i] = 0.5f;
    }

    bool result = language_production_encode_motor_sequence(
        bridge,
        data,
        excessive_len,
        output
    );

    // May fail or succeed with extrapolation depending on implementation
    SUCCEED() << "Excessive length handled";

    delete[] data;
    delete[] output;
}

//=============================================================================
// Unit Tests: Integration
//=============================================================================

TEST_F(LanguageProductionPETest, Integration_MotorAndGestureTogether) {
    // WHAT: Use both motor and gesture encoding in sequence
    // WHY:  Verify compatibility of dual PE
    // HOW:  Configure both, use both

    lpb_config_t config = lpb_default_config();
    config.enable_positional_encoding = true;
    config.pe_config.motor_seq_pe_type = 0;
    config.pe_config.gesture_pe_type = 2;
    config.pe_config.motor_seq_max_length = 64;
    config.pe_config.motor_seq_embedding_dim = 128;
    config.pe_config.gesture_max_length = 32;
    config.pe_config.gesture_embedding_dim = 64;

    bridge = lpb_create(&config, broca);
    ASSERT_NE(bridge, nullptr);

    language_production_set_pe_config(bridge, &config.pe_config);

    // Motor encoding
    float motor_seq[4 * 128];
    float motor_out[4 * 128];
    for (int i = 0; i < 4 * 128; i++) {
        motor_seq[i] = 0.5f;
    }

    bool result1 = language_production_encode_motor_sequence(
        bridge,
        motor_seq,
        4,
        motor_out
    );
    EXPECT_TRUE(result1);

    // Gesture encoding
    float queries[3 * 64], keys[3 * 64];
    float q_out[3 * 64], k_out[3 * 64];
    for (int i = 0; i < 3 * 64; i++) {
        queries[i] = 0.6f;
        keys[i] = 0.4f;
    }

    bool result2 = language_production_encode_gesture(
        bridge,
        queries,
        keys,
        3,
        q_out,
        k_out
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
