/**
 * @file test_brain_multimodal_features.cpp
 * @brief Tests for multi-modal and advanced brain features
 *
 * FOCUS: Covering advanced feature initialization paths in brain.c
 * - Visual cortex (lines 1060-1083)
 * - Audio cortex (lines 1093-1119)
 * - Speech cortex (lines 1147-1154)
 * - Mirror neurons (lines 1893-1930)
 * - Predictive processing (line 1856)
 * - Meta-learning (line 1778)
 * - Memory consolidation (lines 1972-1978)
 * - Pink noise (line 1252)
 *
 * TARGET: +30% brain.c coverage
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>

#include "core/brain/nimcp_brain.h"

//=============================================================================
// Test Fixture
//=============================================================================

class BrainMultimodalFeaturesTest : public ::testing::Test {
protected:
    brain_t brain;

    void SetUp() override {
        brain = nullptr;
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }
};

//=============================================================================
// 1. Visual Cortex Tests
//=============================================================================

TEST_F(BrainMultimodalFeaturesTest, VisualCortex_Initialization) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 128;  // Combined: visual + direct features
    config.num_outputs = 10;
    strncpy(config.task_name, "visual_test", sizeof(config.task_name) - 1);

    // Enable visual cortex
    config.enable_visual_cortex = true;
    config.visual_feature_dim = 64;
    config.enable_multimodal_integration = true;

    brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    // Verify brain created successfully
    brain_stats_t stats;
    bool result = brain_get_stats(brain, &stats);
    EXPECT_TRUE(result);
}

TEST_F(BrainMultimodalFeaturesTest, VisualCortex_WithMultimodalProcessing) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 128;
    config.num_outputs = 5;
    strncpy(config.task_name, "multimodal_visual", sizeof(config.task_name) - 1);

    config.enable_visual_cortex = true;
    config.visual_feature_dim = 64;
    config.enable_multimodal_integration = true;

    brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    // Test with visual input
    brain_multimodal_input_t input = {};

    // Create simple grayscale image (28x28)
    uint8_t visual_data[28 * 28];
    for (int i = 0; i < 28 * 28; i++) {
        visual_data[i] = (uint8_t)(i % 256);
    }

    input.visual_data = visual_data;
    input.visual_width = 28;
    input.visual_height = 28;
    input.visual_channels = 1;
    input.timestamp_ms = 0;

    brain_multimodal_output_t output = {};
    float output_vec[5] = {0};
    output.output_vector = output_vec;
    output.output_dim = 5;

    bool success = brain_process_multimodal(brain, &input, &output);
    EXPECT_TRUE(success);
}

//=============================================================================
// 2. Audio Cortex Tests
//=============================================================================

TEST_F(BrainMultimodalFeaturesTest, AudioCortex_Initialization) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 128;
    config.num_outputs = 5;
    strncpy(config.task_name, "audio_test", sizeof(config.task_name) - 1);

    // Enable audio cortex
    config.enable_audio_cortex = true;
    config.audio_feature_dim = 32;
    config.enable_multimodal_integration = true;

    brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    brain_stats_t stats;
    bool result = brain_get_stats(brain, &stats);
    EXPECT_TRUE(result);
}

TEST_F(BrainMultimodalFeaturesTest, AudioCortex_WithAudioInput) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 128;
    config.num_outputs = 5;
    strncpy(config.task_name, "audio_processing", sizeof(config.task_name) - 1);

    config.enable_audio_cortex = true;
    config.audio_feature_dim = 32;
    config.enable_multimodal_integration = true;

    brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    // Create audio samples (simple sine wave)
    float audio_samples[512];
    for (int i = 0; i < 512; i++) {
        audio_samples[i] = sinf(2.0f * M_PI * 440.0f * i / 16000.0f);
    }

    brain_multimodal_input_t input = {};
    input.audio_data = audio_samples;
    input.audio_samples = 512;
    input.audio_channels = 1;
    input.timestamp_ms = 0;

    brain_multimodal_output_t output = {};
    float output_vec[5] = {0};
    output.output_vector = output_vec;
    output.output_dim = 5;

    bool success = brain_process_multimodal(brain, &input, &output);
    EXPECT_TRUE(success);
}

//=============================================================================
// 3. Speech Cortex Tests
//=============================================================================

TEST_F(BrainMultimodalFeaturesTest, SpeechCortex_Initialization) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 128;
    config.num_outputs = 5;
    strncpy(config.task_name, "speech_test", sizeof(config.task_name) - 1);

    // Enable speech cortex
    config.enable_speech_cortex = true;
    config.speech_feature_dim = 32;
    config.enable_multimodal_integration = true;

    brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    brain_stats_t stats;
    bool result = brain_get_stats(brain, &stats);
    EXPECT_TRUE(result);
}

//=============================================================================
// 4. Combined Multi-Modal Tests
//=============================================================================

TEST_F(BrainMultimodalFeaturesTest, AllSensoryCortices_Combined) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_MEDIUM;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 256;
    config.num_outputs = 10;
    strncpy(config.task_name, "full_multimodal", sizeof(config.task_name) - 1);

    // Enable all sensory cortices
    config.enable_visual_cortex = true;
    config.enable_audio_cortex = true;
    config.enable_speech_cortex = true;
    config.enable_multimodal_integration = true;
    
    config.visual_feature_dim = 64;
    config.audio_feature_dim = 32;
    config.speech_feature_dim = 32;

    brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    // Test with combined inputs
    uint8_t visual_data[28 * 28];
    for (int i = 0; i < 28 * 28; i++) {
        visual_data[i] = (uint8_t)(i % 256);
    }

    float audio_samples[256];
    for (int i = 0; i < 256; i++) {
        audio_samples[i] = sinf(2.0f * M_PI * i / 32.0f);
    }

    brain_multimodal_input_t input = {};
    input.visual_data = visual_data;
    input.visual_width = 28;
    input.visual_height = 28;
    input.visual_channels = 1;
    input.audio_data = audio_samples;
    input.audio_samples = 256;
    input.audio_channels = 1;
    input.timestamp_ms = 0;

    brain_multimodal_output_t output = {};
    float output_vec[10] = {0};
    output.output_vector = output_vec;
    output.output_dim = 10;

    bool success = brain_process_multimodal(brain, &input, &output);
    EXPECT_TRUE(success);
}

//=============================================================================
// 5. Mirror Neurons Tests
//=============================================================================

TEST_F(BrainMultimodalFeaturesTest, MirrorNeurons_Initialization) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 20;
    config.num_outputs = 5;
    strncpy(config.task_name, "mirror_neurons_test", sizeof(config.task_name) - 1);

    // Enable mirror neurons with custom configuration
    config.enable_mirror_neurons = true;
    config.mirror_neuron_count = 500;
    config.mirror_max_actions = 50;
    config.mirror_max_agents = 5;
    config.mirror_learning_rate = 0.01f;
    config.mirror_match_threshold = 0.7f;

    brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    brain_stats_t stats;
    bool result = brain_get_stats(brain, &stats);
    EXPECT_TRUE(result);
}

TEST_F(BrainMultimodalFeaturesTest, MirrorNeurons_ObserveAction) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 20;
    config.num_outputs = 5;
    strncpy(config.task_name, "mirror_observe", sizeof(config.task_name) - 1);

    config.enable_mirror_neurons = true;
    config.mirror_neuron_count = 500;
    config.mirror_max_actions = 50;
    config.mirror_max_agents = 5;

    brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    // Observe another agent's action
    float observed_features[20];
    for (int i = 0; i < 20; i++) {
        observed_features[i] = static_cast<float>(i) / 20.0f;
    }

    bool success = brain_observe_action(brain, observed_features, 20, 1);
    EXPECT_TRUE(success);

    // Observe multiple actions
    for (int j = 0; j < 5; j++) {
        for (int i = 0; i < 20; i++) {
            observed_features[i] = static_cast<float>(i + j) / 20.0f;
        }
        success = brain_observe_action(brain, observed_features, 20, 2);
        EXPECT_TRUE(success);
    }
}

//=============================================================================
// 6. Meta-Learning Tests
//=============================================================================

TEST_F(BrainMultimodalFeaturesTest, MetaLearning_Initialization) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 20;
    config.num_outputs = 5;
    strncpy(config.task_name, "meta_learning_test", sizeof(config.task_name) - 1);

    // Enable meta-learning
    config.enable_meta_learning = true;
    config.enable_adaptive_meta_lr = true;
    config.meta_task_batch_size = 8;
    config.meta_k_shot = 5;

    brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    brain_stats_t stats;
    bool result = brain_get_stats(brain, &stats);
    EXPECT_TRUE(result);
}

//=============================================================================
// 7. Predictive Processing Tests
//=============================================================================

TEST_F(BrainMultimodalFeaturesTest, PredictiveProcessing_Initialization) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 20;
    config.num_outputs = 5;
    strncpy(config.task_name, "predictive_test", sizeof(config.task_name) - 1);

    // Enable predictive processing
    config.enable_predictive_processing = true;
    config.enable_active_inference = true;

    brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    brain_stats_t stats;
    bool result = brain_get_stats(brain, &stats);
    EXPECT_TRUE(result);
}

//=============================================================================
// 8. Memory Consolidation Tests
//=============================================================================

TEST_F(BrainMultimodalFeaturesTest, Consolidation_Enabled) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 20;
    config.num_outputs = 5;
    strncpy(config.task_name, "consolidation_test", sizeof(config.task_name) - 1);

    // Enable consolidation
    config.enable_consolidation = true;

    brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    // Learn some patterns that can be consolidated
    float features[20];
    for (int trial = 0; trial < 5; trial++) {
        for (int i = 0; i < 20; i++) {
            features[i] = static_cast<float>(i + trial) / 20.0f;
        }
        brain_learn_example(brain, features, 20, "pattern_a", 1.0f);
    }

    brain_stats_t stats;
    bool result = brain_get_stats(brain, &stats);
    EXPECT_TRUE(result);
}

//=============================================================================
// 9. Pink Noise Neuromodulation Tests
//=============================================================================

TEST_F(BrainMultimodalFeaturesTest, PinkNoise_Initialization) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 20;
    config.num_outputs = 5;
    strncpy(config.task_name, "pink_noise_test", sizeof(config.task_name) - 1);

    // Enable pink noise
    config.enable_pink_noise = true;

    brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    // Learn with pink noise modulation
    float features[20] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20};
    brain_learn_example(brain, features, 20, "test_label", 1.0f);

    brain_stats_t stats;
    bool result = brain_get_stats(brain, &stats);
    EXPECT_TRUE(result);
}

//=============================================================================
// 10. Comprehensive Feature Integration Test
//=============================================================================

TEST_F(BrainMultimodalFeaturesTest, AllAdvancedFeatures_Integrated) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_MEDIUM;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 256;
    config.num_outputs = 10;
    strncpy(config.task_name, "full_integration", sizeof(config.task_name) - 1);

    // Enable ALL advanced features
    config.enable_visual_cortex = true;
    config.enable_audio_cortex = true;
    config.enable_speech_cortex = true;
    config.enable_multimodal_integration = true;
    config.enable_mirror_neurons = true;
    config.enable_meta_learning = true;
    config.enable_predictive_processing = true;
    config.enable_consolidation = true;
    config.enable_pink_noise = true;
    
    // Cognitive features
    config.enable_working_memory = true;
    config.enable_executive_control = true;
    config.enable_ethics = true;
    config.enable_introspection = true;
    config.enable_salience = true;
    config.enable_theory_of_mind = true;
    config.enable_logic = true;
    config.enable_global_workspace = true;

    // Configure dimensions
    config.visual_feature_dim = 64;
    config.audio_feature_dim = 32;
    config.speech_feature_dim = 32;
    config.mirror_neuron_count = 500;

    brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    // Exercise the system
    float features[256];
    for (int i = 0; i < 256; i++) {
        features[i] = static_cast<float>(i) / 256.0f;
    }

    brain_learn_example(brain, features, 256, "complex_pattern", 1.0f);

    brain_decision_t* decision = brain_decide(brain, features, 256);
    if (decision) {
        EXPECT_GE(decision->confidence, 0.0f);
        EXPECT_LE(decision->confidence, 1.0f);
        brain_free_decision(decision);
    }

    brain_stats_t stats;
    bool result = brain_get_stats(brain, &stats);
    EXPECT_TRUE(result);
}

//=============================================================================
// Run Tests
//=============================================================================

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
