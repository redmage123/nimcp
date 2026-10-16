/**
 * @file test_multimodal_integration_coverage.cpp
 * @brief Comprehensive tests for nimcp_multimodal_integration.c (TARGET: 100% coverage)
 *
 * WHAT: Test multi-modal sensory integration
 * WHY:  Achieve 100% line/branch coverage for nimcp_multimodal_integration.c
 * HOW:  Test all public functions with valid/invalid inputs and all integration methods
 *
 * COVERAGE GOALS:
 * - Line coverage: 100%
 * - Branch coverage: 100%
 * - Function coverage: 100%
 *
 * @author NIMCP Development Team
 * @date 2025-11-10
 */

#include <gtest/gtest.h>
#include <cmath>

#include "core/integration/nimcp_multimodal_integration.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class MultimodalIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Seed random for reproducible tests
        srand(42);
    }

    void TearDown() override {
        // Cleanup
    }

    // Helper: Create simple test features
    float* create_features(uint32_t dim, float base_value) {
        float* features = (float*)malloc(dim * sizeof(float));
        for (uint32_t i = 0; i < dim; i++) {
            features[i] = base_value + i * 0.1f;
        }
        return features;
    }

    // Helper: Verify output is non-zero
    bool has_nonzero_output(const float* output, uint32_t dim) {
        for (uint32_t i = 0; i < dim; i++) {
            if (std::abs(output[i]) > 1e-6f) {
                return true;
            }
        }
        return false;
    }
};

//=============================================================================
// Test Suite: Factory Functions
//=============================================================================

TEST_F(MultimodalIntegrationTest, CreateValidConfig) {
    // Create with valid configuration
    multimodal_config_t config = multimodal_default_config(64, 32, 16, 8);

    multimodal_integration_t integration = multimodal_integration_create(&config);

    ASSERT_NE(integration, nullptr);

    multimodal_integration_destroy(integration);
}

TEST_F(MultimodalIntegrationTest, CreateNullConfig) {
    // Guard: NULL config should return NULL
    multimodal_integration_t integration = multimodal_integration_create(NULL);
    EXPECT_EQ(integration, nullptr);
}

TEST_F(MultimodalIntegrationTest, CreateZeroOutputDim) {
    // Guard: Zero output dimension should fail
    multimodal_config_t config = multimodal_default_config(64, 32, 16, 8);
    config.output_dim = 0;

    multimodal_integration_t integration = multimodal_integration_create(&config);
    EXPECT_EQ(integration, nullptr);
}

TEST_F(MultimodalIntegrationTest, CreateWithLearnedMethod) {
    // Create with learned integration method
    multimodal_config_t config = multimodal_default_config(64, 32, 16, 8);
    config.method = INTEGRATION_LEARNED;

    multimodal_integration_t integration = multimodal_integration_create(&config);

    ASSERT_NE(integration, nullptr);

    multimodal_integration_destroy(integration);
}

TEST_F(MultimodalIntegrationTest, CreateWithOnlyVisual) {
    // Create with only visual input
    multimodal_config_t config = multimodal_default_config(64, 0, 0, 0);

    multimodal_integration_t integration = multimodal_integration_create(&config);

    ASSERT_NE(integration, nullptr);

    multimodal_integration_destroy(integration);
}

TEST_F(MultimodalIntegrationTest, DestroyNull) {
    // Guard: Destroying NULL should be safe
    multimodal_integration_destroy(NULL);
    SUCCEED();
}

//=============================================================================
// Test Suite: Integration - Concatenate Method
//=============================================================================

TEST_F(MultimodalIntegrationTest, IntegrateConcatenate_AllModalities) {
    // Create integration with concatenate method
    multimodal_config_t config = {
        .visual_dim = 4,
        .audio_dim = 3,
        .speech_dim = 2,
        .direct_dim = 1,
        .output_dim = 10,
        .method = INTEGRATION_CONCATENATE,
        .visual_weight = 0.25f,
        .audio_weight = 0.25f,
        .speech_weight = 0.25f,
        .direct_weight = 0.25f
    };

    multimodal_integration_t integration = multimodal_integration_create(&config);
    ASSERT_NE(integration, nullptr);

    // Create test features
    float visual[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    float audio[3] = {5.0f, 6.0f, 7.0f};
    float speech[2] = {8.0f, 9.0f};
    float direct[1] = {10.0f};

    multimodal_input_t input = {
        .visual_features = visual,
        .visual_dim = 4,
        .audio_features = audio,
        .audio_dim = 3,
        .speech_features = speech,
        .speech_dim = 2,
        .direct_features = direct,
        .direct_dim = 1,
        .timestamp = 0
    };

    float output[10] = {0};

    bool success = multimodal_integrate(integration, &input, output);

    EXPECT_TRUE(success);
    // Verify concatenation: [visual|audio|speech|direct|padding]
    EXPECT_FLOAT_EQ(output[0], 1.0f);
    EXPECT_FLOAT_EQ(output[1], 2.0f);
    EXPECT_FLOAT_EQ(output[2], 3.0f);
    EXPECT_FLOAT_EQ(output[3], 4.0f);
    EXPECT_FLOAT_EQ(output[4], 5.0f);
    EXPECT_FLOAT_EQ(output[5], 6.0f);
    EXPECT_FLOAT_EQ(output[6], 7.0f);
    EXPECT_FLOAT_EQ(output[7], 8.0f);
    EXPECT_FLOAT_EQ(output[8], 9.0f);
    EXPECT_FLOAT_EQ(output[9], 10.0f);

    multimodal_integration_destroy(integration);
}

TEST_F(MultimodalIntegrationTest, IntegrateConcatenate_PartialInputs) {
    // Test with only some modalities present
    multimodal_config_t config = {
        .visual_dim = 3,
        .audio_dim = 2,
        .speech_dim = 0,
        .direct_dim = 0,
        .output_dim = 10,
        .method = INTEGRATION_CONCATENATE,
        .visual_weight = 0.5f,
        .audio_weight = 0.5f,
        .speech_weight = 0.0f,
        .direct_weight = 0.0f
    };

    multimodal_integration_t integration = multimodal_integration_create(&config);
    ASSERT_NE(integration, nullptr);

    float visual[3] = {1.0f, 2.0f, 3.0f};
    float audio[2] = {4.0f, 5.0f};

    multimodal_input_t input = {
        .visual_features = visual,
        .visual_dim = 3,
        .audio_features = audio,
        .audio_dim = 2,
        .speech_features = NULL,
        .speech_dim = 0,
        .direct_features = NULL,
        .direct_dim = 0,
        .timestamp = 0
    };

    float output[10] = {0};

    bool success = multimodal_integrate(integration, &input, output);

    EXPECT_TRUE(success);
    // First 5 should be filled, rest padded with zeros
    EXPECT_FLOAT_EQ(output[0], 1.0f);
    EXPECT_FLOAT_EQ(output[4], 5.0f);
    EXPECT_FLOAT_EQ(output[5], 0.0f);
    EXPECT_FLOAT_EQ(output[9], 0.0f);

    multimodal_integration_destroy(integration);
}

//=============================================================================
// Test Suite: Integration - Attention Method
//=============================================================================

TEST_F(MultimodalIntegrationTest, IntegrateAttention_WeightedFusion) {
    // Create integration with attention method
    multimodal_config_t config = {
        .visual_dim = 4,
        .audio_dim = 3,
        .speech_dim = 2,
        .direct_dim = 1,
        .output_dim = 10,
        .method = INTEGRATION_ATTENTION,
        .visual_weight = 0.4f,
        .audio_weight = 0.3f,
        .speech_weight = 0.2f,
        .direct_weight = 0.1f
    };

    multimodal_integration_t integration = multimodal_integration_create(&config);
    ASSERT_NE(integration, nullptr);

    float visual[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    float audio[3] = {2.0f, 2.0f, 2.0f};
    float speech[2] = {3.0f, 3.0f};
    float direct[1] = {4.0f};

    multimodal_input_t input = {
        .visual_features = visual,
        .visual_dim = 4,
        .audio_features = audio,
        .audio_dim = 3,
        .speech_features = speech,
        .speech_dim = 2,
        .direct_features = direct,
        .direct_dim = 1,
        .timestamp = 0
    };

    float output[10] = {0};

    bool success = multimodal_integrate(integration, &input, output);

    EXPECT_TRUE(success);
    EXPECT_TRUE(has_nonzero_output(output, 10));

    multimodal_integration_destroy(integration);
}

TEST_F(MultimodalIntegrationTest, IntegrateAttention_GetWeights) {
    // Test getting attention weights
    multimodal_config_t config = multimodal_default_config(64, 32, 16, 8);
    config.method = INTEGRATION_ATTENTION;

    multimodal_integration_t integration = multimodal_integration_create(&config);
    ASSERT_NE(integration, nullptr);

    float visual_attn, audio_attn, speech_attn, direct_attn;

    bool success = multimodal_get_attention(
        integration,
        &visual_attn,
        &audio_attn,
        &speech_attn,
        &direct_attn
    );

    EXPECT_TRUE(success);
    // Weights should be normalized to sum to ~1.0
    float total = visual_attn + audio_attn + speech_attn + direct_attn;
    EXPECT_NEAR(total, 1.0f, 0.01f);

    multimodal_integration_destroy(integration);
}

//=============================================================================
// Test Suite: Integration - Learned Method
//=============================================================================

TEST_F(MultimodalIntegrationTest, IntegrateLearned_MatrixProjection) {
    // Create integration with learned method
    multimodal_config_t config = {
        .visual_dim = 8,
        .audio_dim = 4,
        .speech_dim = 2,
        .direct_dim = 2,
        .output_dim = 16,
        .method = INTEGRATION_LEARNED,
        .visual_weight = 0.4f,
        .audio_weight = 0.3f,
        .speech_weight = 0.2f,
        .direct_weight = 0.1f
    };

    multimodal_integration_t integration = multimodal_integration_create(&config);
    ASSERT_NE(integration, nullptr);

    float* visual = create_features(8, 0.5f);
    float* audio = create_features(4, 1.0f);
    float* speech = create_features(2, 1.5f);
    float* direct = create_features(2, 2.0f);

    multimodal_input_t input = {
        .visual_features = visual,
        .visual_dim = 8,
        .audio_features = audio,
        .audio_dim = 4,
        .speech_features = speech,
        .speech_dim = 2,
        .direct_features = direct,
        .direct_dim = 2,
        .timestamp = 0
    };

    float output[16] = {0};

    bool success = multimodal_integrate(integration, &input, output);

    EXPECT_TRUE(success);
    EXPECT_TRUE(has_nonzero_output(output, 16));

    free(visual);
    free(audio);
    free(speech);
    free(direct);
    multimodal_integration_destroy(integration);
}

TEST_F(MultimodalIntegrationTest, IntegrateLearned_OnlyVisual) {
    // Test learned method with only visual input
    multimodal_config_t config = {
        .visual_dim = 8,
        .audio_dim = 0,
        .speech_dim = 0,
        .direct_dim = 0,
        .output_dim = 16,
        .method = INTEGRATION_LEARNED,
        .visual_weight = 1.0f,
        .audio_weight = 0.0f,
        .speech_weight = 0.0f,
        .direct_weight = 0.0f
    };

    multimodal_integration_t integration = multimodal_integration_create(&config);
    ASSERT_NE(integration, nullptr);

    float visual[8] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};

    multimodal_input_t input = {
        .visual_features = visual,
        .visual_dim = 8,
        .audio_features = NULL,
        .audio_dim = 0,
        .speech_features = NULL,
        .speech_dim = 0,
        .direct_features = NULL,
        .direct_dim = 0,
        .timestamp = 0
    };

    float output[16] = {0};

    bool success = multimodal_integrate(integration, &input, output);

    EXPECT_TRUE(success);
    EXPECT_TRUE(has_nonzero_output(output, 16));

    multimodal_integration_destroy(integration);
}

//=============================================================================
// Test Suite: Guard Clauses and Edge Cases
//=============================================================================

TEST_F(MultimodalIntegrationTest, IntegrateNullIntegration) {
    // Guard: NULL integration
    float visual[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    multimodal_input_t input = {
        .visual_features = visual,
        .visual_dim = 4,
        .audio_features = NULL,
        .audio_dim = 0,
        .speech_features = NULL,
        .speech_dim = 0,
        .direct_features = NULL,
        .direct_dim = 0,
        .timestamp = 0
    };
    float output[10] = {0};

    bool success = multimodal_integrate(NULL, &input, output);
    EXPECT_FALSE(success);
}

TEST_F(MultimodalIntegrationTest, IntegrateNullInput) {
    // Guard: NULL input
    multimodal_config_t config = multimodal_default_config(64, 32, 16, 8);
    multimodal_integration_t integration = multimodal_integration_create(&config);
    ASSERT_NE(integration, nullptr);

    float output[120] = {0};

    bool success = multimodal_integrate(integration, NULL, output);
    EXPECT_FALSE(success);

    multimodal_integration_destroy(integration);
}

TEST_F(MultimodalIntegrationTest, IntegrateNullOutput) {
    // Guard: NULL output
    multimodal_config_t config = multimodal_default_config(64, 32, 16, 8);
    multimodal_integration_t integration = multimodal_integration_create(&config);
    ASSERT_NE(integration, nullptr);

    float visual[64] = {0};
    multimodal_input_t input = {
        .visual_features = visual,
        .visual_dim = 64,
        .audio_features = NULL,
        .audio_dim = 0,
        .speech_features = NULL,
        .speech_dim = 0,
        .direct_features = NULL,
        .direct_dim = 0,
        .timestamp = 0
    };

    bool success = multimodal_integrate(integration, &input, NULL);
    EXPECT_FALSE(success);

    multimodal_integration_destroy(integration);
}

TEST_F(MultimodalIntegrationTest, IntegrateInvalidMethod) {
    // Test invalid integration method
    multimodal_config_t config = multimodal_default_config(64, 32, 16, 8);
    config.method = (integration_method_t)999;  // Invalid

    multimodal_integration_t integration = multimodal_integration_create(&config);
    ASSERT_NE(integration, nullptr);

    float visual[64] = {0};
    multimodal_input_t input = {
        .visual_features = visual,
        .visual_dim = 64,
        .audio_features = NULL,
        .audio_dim = 0,
        .speech_features = NULL,
        .speech_dim = 0,
        .direct_features = NULL,
        .direct_dim = 0,
        .timestamp = 0
    };
    float output[120] = {0};

    bool success = multimodal_integrate(integration, &input, output);
    EXPECT_FALSE(success);

    multimodal_integration_destroy(integration);
}

//=============================================================================
// Test Suite: Weight Update
//=============================================================================

TEST_F(MultimodalIntegrationTest, UpdateWeights_PositiveReward) {
    // Test weight update with positive reward
    multimodal_config_t config = multimodal_default_config(64, 32, 16, 8);
    multimodal_integration_t integration = multimodal_integration_create(&config);
    ASSERT_NE(integration, nullptr);

    float v1, a1, s1, d1;
    multimodal_get_attention(integration, &v1, &a1, &s1, &d1);

    // Update with positive reward
    bool success = multimodal_update_weights(integration, 0.5f, 0.1f);
    EXPECT_TRUE(success);

    float v2, a2, s2, d2;
    multimodal_get_attention(integration, &v2, &a2, &s2, &d2);

    // Weights should still be normalized
    float total = v2 + a2 + s2 + d2;
    EXPECT_NEAR(total, 1.0f, 0.01f);

    multimodal_integration_destroy(integration);
}

TEST_F(MultimodalIntegrationTest, UpdateWeights_NegativeReward) {
    // Test weight update with negative reward
    multimodal_config_t config = multimodal_default_config(64, 32, 16, 8);
    multimodal_integration_t integration = multimodal_integration_create(&config);
    ASSERT_NE(integration, nullptr);

    // Update with negative reward
    bool success = multimodal_update_weights(integration, -0.5f, 0.1f);
    EXPECT_TRUE(success);

    float v, a, s, d;
    multimodal_get_attention(integration, &v, &a, &s, &d);

    // Weights should still be normalized and non-negative
    EXPECT_GE(v, 0.0f);
    EXPECT_GE(a, 0.0f);
    EXPECT_GE(s, 0.0f);
    EXPECT_GE(d, 0.0f);
    EXPECT_LE(v, 1.0f);
    EXPECT_LE(a, 1.0f);
    EXPECT_LE(s, 1.0f);
    EXPECT_LE(d, 1.0f);

    multimodal_integration_destroy(integration);
}

TEST_F(MultimodalIntegrationTest, UpdateWeightsNull) {
    // Guard: NULL integration
    bool success = multimodal_update_weights(NULL, 0.5f, 0.1f);
    EXPECT_FALSE(success);
}

//=============================================================================
// Test Suite: Attention Functions
//=============================================================================

TEST_F(MultimodalIntegrationTest, GetAttentionNull) {
    // Guard: NULL integration
    float v, a, s, d;
    bool success = multimodal_get_attention(NULL, &v, &a, &s, &d);
    EXPECT_FALSE(success);
}

TEST_F(MultimodalIntegrationTest, GetAttentionNullOutputs) {
    // Test with NULL output pointers (should still succeed)
    multimodal_config_t config = multimodal_default_config(64, 32, 16, 8);
    multimodal_integration_t integration = multimodal_integration_create(&config);
    ASSERT_NE(integration, nullptr);

    bool success = multimodal_get_attention(integration, NULL, NULL, NULL, NULL);
    EXPECT_TRUE(success);

    multimodal_integration_destroy(integration);
}

TEST_F(MultimodalIntegrationTest, GetAttentionPartialOutputs) {
    // Test with some NULL output pointers
    multimodal_config_t config = multimodal_default_config(64, 32, 16, 8);
    multimodal_integration_t integration = multimodal_integration_create(&config);
    ASSERT_NE(integration, nullptr);

    float v, a;
    bool success = multimodal_get_attention(integration, &v, &a, NULL, NULL);

    EXPECT_TRUE(success);
    EXPECT_GT(v, 0.0f);
    EXPECT_GT(a, 0.0f);

    multimodal_integration_destroy(integration);
}

//=============================================================================
// Test Suite: Helper Functions
//=============================================================================

TEST_F(MultimodalIntegrationTest, DefaultConfig) {
    // Test default configuration
    multimodal_config_t config = multimodal_default_config(64, 32, 16, 8);

    EXPECT_EQ(config.visual_dim, 64);
    EXPECT_EQ(config.audio_dim, 32);
    EXPECT_EQ(config.speech_dim, 16);
    EXPECT_EQ(config.direct_dim, 8);
    EXPECT_EQ(config.output_dim, 120);  // 64+32+16+8
    EXPECT_EQ(config.method, INTEGRATION_ATTENTION);
    EXPECT_GT(config.visual_weight, 0.0f);
    EXPECT_GT(config.audio_weight, 0.0f);
    EXPECT_GT(config.speech_weight, 0.0f);
    EXPECT_GT(config.direct_weight, 0.0f);
}

TEST_F(MultimodalIntegrationTest, ValidateInput_Valid) {
    // Test valid input
    multimodal_config_t config = multimodal_default_config(4, 3, 2, 1);
    multimodal_integration_t integration = multimodal_integration_create(&config);
    ASSERT_NE(integration, nullptr);

    float visual[4] = {1, 2, 3, 4};
    float audio[3] = {5, 6, 7};

    multimodal_input_t input = {
        .visual_features = visual,
        .visual_dim = 4,
        .audio_features = audio,
        .audio_dim = 3,
        .speech_features = NULL,
        .speech_dim = 0,
        .direct_features = NULL,
        .direct_dim = 0,
        .timestamp = 0
    };

    bool valid = multimodal_validate_input(integration, &input);
    EXPECT_TRUE(valid);

    multimodal_integration_destroy(integration);
}

TEST_F(MultimodalIntegrationTest, ValidateInput_DimensionMismatch) {
    // Test dimension mismatch
    multimodal_config_t config = multimodal_default_config(4, 3, 2, 1);
    multimodal_integration_t integration = multimodal_integration_create(&config);
    ASSERT_NE(integration, nullptr);

    float visual[5] = {1, 2, 3, 4, 5};  // Wrong dimension

    multimodal_input_t input = {
        .visual_features = visual,
        .visual_dim = 5,  // Should be 4
        .audio_features = NULL,
        .audio_dim = 0,
        .speech_features = NULL,
        .speech_dim = 0,
        .direct_features = NULL,
        .direct_dim = 0,
        .timestamp = 0
    };

    bool valid = multimodal_validate_input(integration, &input);
    EXPECT_FALSE(valid);

    multimodal_integration_destroy(integration);
}

TEST_F(MultimodalIntegrationTest, ValidateInput_NoFeatures) {
    // Test input with no features
    multimodal_config_t config = multimodal_default_config(4, 3, 2, 1);
    multimodal_integration_t integration = multimodal_integration_create(&config);
    ASSERT_NE(integration, nullptr);

    multimodal_input_t input = {
        .visual_features = NULL,
        .visual_dim = 0,
        .audio_features = NULL,
        .audio_dim = 0,
        .speech_features = NULL,
        .speech_dim = 0,
        .direct_features = NULL,
        .direct_dim = 0,
        .timestamp = 0
    };

    bool valid = multimodal_validate_input(integration, &input);
    EXPECT_FALSE(valid);

    multimodal_integration_destroy(integration);
}

TEST_F(MultimodalIntegrationTest, ValidateInputNull_Integration) {
    // Guard: NULL integration
    float visual[4] = {1, 2, 3, 4};
    multimodal_input_t input = {
        .visual_features = visual,
        .visual_dim = 4,
        .audio_features = NULL,
        .audio_dim = 0,
        .speech_features = NULL,
        .speech_dim = 0,
        .direct_features = NULL,
        .direct_dim = 0,
        .timestamp = 0
    };

    bool valid = multimodal_validate_input(NULL, &input);
    EXPECT_FALSE(valid);
}

TEST_F(MultimodalIntegrationTest, ValidateInputNull_Input) {
    // Guard: NULL input
    multimodal_config_t config = multimodal_default_config(4, 3, 2, 1);
    multimodal_integration_t integration = multimodal_integration_create(&config);
    ASSERT_NE(integration, nullptr);

    bool valid = multimodal_validate_input(integration, NULL);
    EXPECT_FALSE(valid);

    multimodal_integration_destroy(integration);
}

//=============================================================================
// Test Suite: Integration Scenarios
//=============================================================================

TEST_F(MultimodalIntegrationTest, MultipleIntegrations) {
    // Test multiple integrations in sequence
    multimodal_config_t config = multimodal_default_config(8, 4, 2, 2);
    multimodal_integration_t integration = multimodal_integration_create(&config);
    ASSERT_NE(integration, nullptr);

    float* visual = create_features(8, 0.5f);
    float* audio = create_features(4, 1.0f);

    multimodal_input_t input = {
        .visual_features = visual,
        .visual_dim = 8,
        .audio_features = audio,
        .audio_dim = 4,
        .speech_features = NULL,
        .speech_dim = 0,
        .direct_features = NULL,
        .direct_dim = 0,
        .timestamp = 0
    };

    float output[16] = {0};

    // Run multiple integrations
    for (int i = 0; i < 10; i++) {
        bool success = multimodal_integrate(integration, &input, output);
        EXPECT_TRUE(success);
    }

    free(visual);
    free(audio);
    multimodal_integration_destroy(integration);
}

TEST_F(MultimodalIntegrationTest, AllThreeMethods) {
    // Test all three integration methods with same input
    float visual[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    float audio[2] = {5.0f, 6.0f};

    multimodal_input_t input = {
        .visual_features = visual,
        .visual_dim = 4,
        .audio_features = audio,
        .audio_dim = 2,
        .speech_features = NULL,
        .speech_dim = 0,
        .direct_features = NULL,
        .direct_dim = 0,
        .timestamp = 0
    };

    float output[10] = {0};

    // Test CONCATENATE
    multimodal_config_t config1 = {
        .visual_dim = 4, .audio_dim = 2, .speech_dim = 0, .direct_dim = 0,
        .output_dim = 10, .method = INTEGRATION_CONCATENATE,
        .visual_weight = 0.5f, .audio_weight = 0.5f, .speech_weight = 0.0f, .direct_weight = 0.0f
    };
    multimodal_integration_t int1 = multimodal_integration_create(&config1);
    ASSERT_NE(int1, nullptr);
    EXPECT_TRUE(multimodal_integrate(int1, &input, output));
    multimodal_integration_destroy(int1);

    // Test ATTENTION
    multimodal_config_t config2 = config1;
    config2.method = INTEGRATION_ATTENTION;
    multimodal_integration_t int2 = multimodal_integration_create(&config2);
    ASSERT_NE(int2, nullptr);
    EXPECT_TRUE(multimodal_integrate(int2, &input, output));
    multimodal_integration_destroy(int2);

    // Test LEARNED
    multimodal_config_t config3 = config1;
    config3.method = INTEGRATION_LEARNED;
    multimodal_integration_t int3 = multimodal_integration_create(&config3);
    ASSERT_NE(int3, nullptr);
    EXPECT_TRUE(multimodal_integrate(int3, &input, output));
    multimodal_integration_destroy(int3);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
