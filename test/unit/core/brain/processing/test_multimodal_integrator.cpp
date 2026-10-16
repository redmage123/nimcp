//=============================================================================
// test_multimodal_integrator.cpp - Multimodal Integration Unit Tests
//=============================================================================

#include <gtest/gtest.h>
#include <memory>
#include <cmath>
#include <vector>

// Headers have their own extern "C" guards
#include "core/brain/processing/multimodal_integrator.h"
#include "core/brain/processing/sensory_extractor.h"
#include "core/brain/nimcp_brain.h"

//=============================================================================
// Test Fixture
//=============================================================================

class MultimodalIntegratorTest : public ::testing::Test {
protected:
    brain_t brain;
    sensory_features_t features;
    integrated_features_t output;
    float visual_feats[50];
    float audio_feats[30];
    float speech_feats[20];
    float direct_feats[40];
    float integrated_buffer[200];

    void SetUp() override {
        // Create brain
        brain = brain_create(100, 10);
        ASSERT_NE(brain, nullptr);

        // Initialize sensory features
        sensory_features_init(&features);

        // Setup visual features
        for (int i = 0; i < 50; i++) {
            visual_feats[i] = 0.1f * i;
        }
        features.visual_features = visual_feats;
        features.visual_dim = 50;
        features.visual_valid = true;

        // Setup audio features
        for (int i = 0; i < 30; i++) {
            audio_feats[i] = 0.2f * i;
        }
        features.audio_features = audio_feats;
        features.audio_dim = 30;
        features.audio_valid = true;

        // Setup speech features
        for (int i = 0; i < 20; i++) {
            speech_feats[i] = 0.15f * i;
        }
        features.speech_features = speech_feats;
        features.speech_dim = 20;
        features.speech_valid = true;

        // Setup direct features
        for (int i = 0; i < 40; i++) {
            direct_feats[i] = 0.25f * i;
        }
        features.direct_features = direct_feats;
        features.direct_dim = 40;
        features.direct_valid = true;

        // Initialize output
        integrated_features_init(&output);
        output.integrated_features = integrated_buffer;
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }
};

//=============================================================================
// Basic Functionality Tests
//=============================================================================

TEST_F(MultimodalIntegratorTest, InitializeIntegratedFeatures) {
    integrated_features_t feat;
    integrated_features_init(&feat);

    EXPECT_EQ(feat.integrated_features, nullptr);
    EXPECT_EQ(feat.integrated_dim, 0);
    EXPECT_FLOAT_EQ(feat.visual_attention, 0.0f);
    EXPECT_FLOAT_EQ(feat.audio_attention, 0.0f);
    EXPECT_FLOAT_EQ(feat.speech_attention, 0.0f);
    EXPECT_FLOAT_EQ(feat.direct_attention, 0.0f);
}

TEST_F(MultimodalIntegratorTest, IntegrateAllModalities) {
    bool success = multimodal_integrate_features(brain, &features, &output);

    EXPECT_TRUE(success) << "Integration should succeed with all modalities";

    // Check attention weights sum approximately to 1
    float total_attention = output.visual_attention +
                           output.audio_attention +
                           output.speech_attention +
                           output.direct_attention;
    EXPECT_NEAR(total_attention, 1.0f, 0.1f) << "Attention weights should sum to ~1";

    // Check individual attention weights are in valid range
    EXPECT_GE(output.visual_attention, 0.0f);
    EXPECT_LE(output.visual_attention, 1.0f);
    EXPECT_GE(output.audio_attention, 0.0f);
    EXPECT_LE(output.audio_attention, 1.0f);
    EXPECT_GE(output.speech_attention, 0.0f);
    EXPECT_LE(output.speech_attention, 1.0f);
    EXPECT_GE(output.direct_attention, 0.0f);
    EXPECT_LE(output.direct_attention, 1.0f);

    // Integrated dimension should be positive
    EXPECT_GT(output.integrated_dim, 0);
}

TEST_F(MultimodalIntegratorTest, IntegrationRepeatable) {
    integrated_features_t out1, out2;
    float buffer1[200], buffer2[200];

    integrated_features_init(&out1);
    integrated_features_init(&out2);
    out1.integrated_features = buffer1;
    out2.integrated_features = buffer2;

    bool success1 = multimodal_integrate_features(brain, &features, &out1);
    bool success2 = multimodal_integrate_features(brain, &features, &out2);

    EXPECT_TRUE(success1);
    EXPECT_TRUE(success2);

    // Results should be deterministic
    EXPECT_NEAR(out1.visual_attention, out2.visual_attention, 0.001f);
    EXPECT_NEAR(out1.audio_attention, out2.audio_attention, 0.001f);
    EXPECT_NEAR(out1.speech_attention, out2.speech_attention, 0.001f);
    EXPECT_NEAR(out1.direct_attention, out2.direct_attention, 0.001f);
}

//=============================================================================
// Single Modality Tests
//=============================================================================

TEST_F(MultimodalIntegratorTest, VisualOnlyIntegration) {
    // Disable other modalities
    features.audio_valid = false;
    features.speech_valid = false;
    features.direct_valid = false;

    bool success = multimodal_integrate_features(brain, &features, &output);

    EXPECT_TRUE(success);
    // Visual should get all attention
    EXPECT_GT(output.visual_attention, 0.8f) << "Visual should dominate";
    EXPECT_LT(output.audio_attention, 0.1f);
    EXPECT_LT(output.speech_attention, 0.1f);
    EXPECT_LT(output.direct_attention, 0.1f);
}

TEST_F(MultimodalIntegratorTest, AudioOnlyIntegration) {
    features.visual_valid = false;
    features.speech_valid = false;
    features.direct_valid = false;

    bool success = multimodal_integrate_features(brain, &features, &output);

    EXPECT_TRUE(success);
    EXPECT_GT(output.audio_attention, 0.8f) << "Audio should dominate";
}

TEST_F(MultimodalIntegratorTest, SpeechOnlyIntegration) {
    features.visual_valid = false;
    features.audio_valid = false;
    features.direct_valid = false;

    bool success = multimodal_integrate_features(brain, &features, &output);

    EXPECT_TRUE(success);
    EXPECT_GT(output.speech_attention, 0.8f) << "Speech should dominate";
}

TEST_F(MultimodalIntegratorTest, DirectOnlyIntegration) {
    features.visual_valid = false;
    features.audio_valid = false;
    features.speech_valid = false;

    bool success = multimodal_integrate_features(brain, &features, &output);

    EXPECT_TRUE(success);
    EXPECT_GT(output.direct_attention, 0.8f) << "Direct should dominate";
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(MultimodalIntegratorTest, NullBrainHandling) {
    bool success = multimodal_integrate_features(nullptr, &features, &output);

    EXPECT_FALSE(success) << "Should fail with null brain";
}

TEST_F(MultimodalIntegratorTest, NullFeaturesHandling) {
    bool success = multimodal_integrate_features(brain, nullptr, &output);

    EXPECT_FALSE(success) << "Should fail with null features";
}

TEST_F(MultimodalIntegratorTest, NullOutputHandling) {
    bool success = multimodal_integrate_features(brain, &features, nullptr);

    EXPECT_FALSE(success) << "Should fail with null output";
}

TEST_F(MultimodalIntegratorTest, NoValidModalities) {
    features.visual_valid = false;
    features.audio_valid = false;
    features.speech_valid = false;
    features.direct_valid = false;

    bool success = multimodal_integrate_features(brain, &features, &output);

    EXPECT_FALSE(success) << "Should fail with no valid modalities";
}

TEST_F(MultimodalIntegratorTest, ZeroDimensionFeatures) {
    features.visual_dim = 0;
    features.audio_dim = 0;
    features.speech_dim = 0;
    features.direct_dim = 0;

    bool success = multimodal_integrate_features(brain, &features, &output);

    // Should handle gracefully
    (void)success;
}

TEST_F(MultimodalIntegratorTest, NullFeatureVectors) {
    features.visual_features = nullptr;
    features.audio_features = nullptr;
    features.speech_features = nullptr;
    features.direct_features = nullptr;

    bool success = multimodal_integrate_features(brain, &features, &output);

    EXPECT_FALSE(success) << "Should fail with null feature vectors";
}

//=============================================================================
// Attention Weight Tests
//=============================================================================

TEST_F(MultimodalIntegratorTest, AttentionWeightsNormalized) {
    multimodal_integrate_features(brain, &features, &output);

    float sum = output.visual_attention +
                output.audio_attention +
                output.speech_attention +
                output.direct_attention;

    EXPECT_GE(sum, 0.95f) << "Attention weights should sum to approximately 1";
    EXPECT_LE(sum, 1.05f);
}

TEST_F(MultimodalIntegratorTest, AttentionWeightsInRange) {
    multimodal_integrate_features(brain, &features, &output);

    EXPECT_GE(output.visual_attention, 0.0f);
    EXPECT_LE(output.visual_attention, 1.0f);
    EXPECT_GE(output.audio_attention, 0.0f);
    EXPECT_LE(output.audio_attention, 1.0f);
    EXPECT_GE(output.speech_attention, 0.0f);
    EXPECT_LE(output.speech_attention, 1.0f);
    EXPECT_GE(output.direct_attention, 0.0f);
    EXPECT_LE(output.direct_attention, 1.0f);
}

//=============================================================================
// Feature Fusion Tests
//=============================================================================

TEST_F(MultimodalIntegratorTest, VisualAudioFusion) {
    // Enable only visual and audio
    features.speech_valid = false;
    features.direct_valid = false;

    bool success = multimodal_integrate_features(brain, &features, &output);

    EXPECT_TRUE(success);

    // Both should have non-zero attention
    EXPECT_GT(output.visual_attention, 0.0f);
    EXPECT_GT(output.audio_attention, 0.0f);

    // Sum should be close to 1
    float sum = output.visual_attention + output.audio_attention;
    EXPECT_NEAR(sum, 1.0f, 0.1f);
}

TEST_F(MultimodalIntegratorTest, AudioSpeechFusion) {
    // Speech builds on audio hierarchically
    features.visual_valid = false;
    features.direct_valid = false;

    bool success = multimodal_integrate_features(brain, &features, &output);

    EXPECT_TRUE(success);
    EXPECT_GT(output.audio_attention, 0.0f);
    EXPECT_GT(output.speech_attention, 0.0f);
}

TEST_F(MultimodalIntegratorTest, TrimodalFusion) {
    features.direct_valid = false;

    bool success = multimodal_integrate_features(brain, &features, &output);

    EXPECT_TRUE(success);

    // All three should contribute
    EXPECT_GT(output.visual_attention, 0.0f);
    EXPECT_GT(output.audio_attention, 0.0f);
    EXPECT_GT(output.speech_attention, 0.0f);
}

//=============================================================================
// Different Feature Patterns Tests
//=============================================================================

TEST_F(MultimodalIntegratorTest, UniformFeatures) {
    // All features uniform
    for (int i = 0; i < 50; i++) visual_feats[i] = 0.5f;
    for (int i = 0; i < 30; i++) audio_feats[i] = 0.5f;
    for (int i = 0; i < 20; i++) speech_feats[i] = 0.5f;
    for (int i = 0; i < 40; i++) direct_feats[i] = 0.5f;

    bool success = multimodal_integrate_features(brain, &features, &output);

    EXPECT_TRUE(success);
}

TEST_F(MultimodalIntegratorTest, SparseFeatures) {
    // Mostly zeros with few active
    for (int i = 0; i < 50; i++) visual_feats[i] = (i % 10 == 0) ? 1.0f : 0.0f;
    for (int i = 0; i < 30; i++) audio_feats[i] = (i % 10 == 0) ? 1.0f : 0.0f;
    for (int i = 0; i < 20; i++) speech_feats[i] = (i % 10 == 0) ? 1.0f : 0.0f;
    for (int i = 0; i < 40; i++) direct_feats[i] = (i % 10 == 0) ? 1.0f : 0.0f;

    bool success = multimodal_integrate_features(brain, &features, &output);

    EXPECT_TRUE(success);
}

TEST_F(MultimodalIntegratorTest, NegativeFeatures) {
    // Some negative values
    for (int i = 0; i < 50; i++) visual_feats[i] = -0.5f + 0.02f * i;

    bool success = multimodal_integrate_features(brain, &features, &output);

    EXPECT_TRUE(success);
}

//=============================================================================
// Boundary Conditions Tests
//=============================================================================

TEST_F(MultimodalIntegratorTest, VerySmallDimensions) {
    features.visual_dim = 1;
    features.audio_dim = 1;
    features.speech_dim = 1;
    features.direct_dim = 1;

    bool success = multimodal_integrate_features(brain, &features, &output);

    EXPECT_TRUE(success);
}

TEST_F(MultimodalIntegratorTest, VeryLargeDimensions) {
    std::vector<float> large_visual(1000, 0.5f);
    std::vector<float> large_audio(800, 0.5f);
    std::vector<float> large_speech(600, 0.5f);
    std::vector<float> large_direct(900, 0.5f);

    features.visual_features = large_visual.data();
    features.visual_dim = 1000;
    features.audio_features = large_audio.data();
    features.audio_dim = 800;
    features.speech_features = large_speech.data();
    features.speech_dim = 600;
    features.direct_features = large_direct.data();
    features.direct_dim = 900;

    bool success = multimodal_integrate_features(brain, &features, &output);

    EXPECT_TRUE(success);
}

TEST_F(MultimodalIntegratorTest, MismatchedDimensions) {
    features.visual_dim = 100;
    features.audio_dim = 10;
    features.speech_dim = 1;
    features.direct_dim = 500;

    bool success = multimodal_integrate_features(brain, &features, &output);

    EXPECT_TRUE(success) << "Should handle mismatched dimensions";
}

//=============================================================================
// Stress Tests
//=============================================================================

TEST_F(MultimodalIntegratorTest, MultipleIntegrations) {
    for (int i = 0; i < 100; i++) {
        integrated_features_t temp_out;
        float temp_buffer[200];
        integrated_features_init(&temp_out);
        temp_out.integrated_features = temp_buffer;

        bool success = multimodal_integrate_features(brain, &features, &temp_out);

        EXPECT_TRUE(success) << "Failed at iteration " << i;
    }
}

TEST_F(MultimodalIntegratorTest, VaryingModalityCombinations) {
    // Test all 16 combinations of 4 modalities
    for (int mask = 1; mask < 16; mask++) {
        features.visual_valid = (mask & 1) != 0;
        features.audio_valid = (mask & 2) != 0;
        features.speech_valid = (mask & 4) != 0;
        features.direct_valid = (mask & 8) != 0;

        integrated_features_t temp_out;
        float temp_buffer[200];
        integrated_features_init(&temp_out);
        temp_out.integrated_features = temp_buffer;

        bool success = multimodal_integrate_features(brain, &features, &temp_out);

        EXPECT_TRUE(success) << "Failed for modality mask " << mask;
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
