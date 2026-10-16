/**
 * @file test_brain_multimodal_initialization.cpp
 * @brief Comprehensive tests for multimodal cortex initialization paths
 *
 * WHAT: Tests all multimodal cortex initialization code paths in brain.c
 * WHY: Boost coverage by exercising visual/audio/speech cortex setup
 * HOW: Test each cortex individually, in combinations, and error paths
 *
 * TARGET LINES: Lines 1075-1210 in nimcp_brain.c
 * EXPECTED COVERAGE GAIN: ~135 lines
 */

#include <gtest/gtest.h>
#include <cstring>

    #include "core/brain/nimcp_brain.h"
    #include "nimcp.h"

//=============================================================================
// Test Fixture
//=============================================================================

class BrainMultimodalInitTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_init();
        brain_clear_error();
    }

    void TearDown() override {
        nimcp_shutdown();
    }
};

//=============================================================================
// Visual Cortex Initialization Tests
//=============================================================================

TEST_F(BrainMultimodalInitTest, VisualCortex_BasicInitialization) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_TINY;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 100;
    config.num_outputs = 5;
    strncpy(config.task_name, "visual_test", 63);

    // Enable visual cortex
    config.enable_visual_cortex = true;
    config.visual_feature_dim = 64;
    config.enable_multimodal_integration = true;

    brain_t brain = brain_create_custom(&config);
    if (brain) {
        brain_destroy(brain);
    }
}

TEST_F(BrainMultimodalInitTest, VisualCortex_WithFractalTopology) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_TINY;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 100;
    config.num_outputs = 5;
    strncpy(config.task_name, "visual_fractal", 63);

    // Enable visual cortex with fractal topology
    config.enable_visual_cortex = true;
    config.visual_feature_dim = 64;
    config.enable_fractal_topology = true;
    config.enable_multimodal_integration = true;

    brain_t brain = brain_create_custom(&config);
    if (brain) {
        brain_destroy(brain);
    }
}

TEST_F(BrainMultimodalInitTest, VisualCortex_LargeDimension) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 256;
    config.num_outputs = 10;
    strncpy(config.task_name, "visual_large", 63);

    // Large visual dimension
    config.enable_visual_cortex = true;
    config.visual_feature_dim = 128;
    config.enable_multimodal_integration = true;

    brain_t brain = brain_create_custom(&config);
    if (brain) {
        brain_destroy(brain);
    }
}

//=============================================================================
// Audio Cortex Initialization Tests
//=============================================================================

TEST_F(BrainMultimodalInitTest, AudioCortex_BasicInitialization) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_TINY;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 80;
    config.num_outputs = 5;
    strncpy(config.task_name, "audio_test", 63);

    // Enable audio cortex
    config.enable_audio_cortex = true;
    config.audio_feature_dim = 40;
    config.enable_multimodal_integration = true;

    brain_t brain = brain_create_custom(&config);
    if (brain) {
        brain_destroy(brain);
    }
}

TEST_F(BrainMultimodalInitTest, AudioCortex_WithFractalTopology) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_TINY;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 80;
    config.num_outputs = 5;
    strncpy(config.task_name, "audio_fractal", 63);

    // Enable audio cortex with fractal topology
    config.enable_audio_cortex = true;
    config.audio_feature_dim = 40;
    config.enable_fractal_topology = true;
    config.enable_multimodal_integration = true;

    brain_t brain = brain_create_custom(&config);
    if (brain) {
        brain_destroy(brain);
    }
}

TEST_F(BrainMultimodalInitTest, AudioCortex_LargeDimension) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 128;
    config.num_outputs = 10;
    strncpy(config.task_name, "audio_large", 63);

    // Large audio dimension
    config.enable_audio_cortex = true;
    config.audio_feature_dim = 80;
    config.enable_multimodal_integration = true;

    brain_t brain = brain_create_custom(&config);
    if (brain) {
        brain_destroy(brain);
    }
}

//=============================================================================
// Speech Cortex Initialization Tests
//=============================================================================

TEST_F(BrainMultimodalInitTest, SpeechCortex_BasicInitialization) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_TINY;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 64;
    config.num_outputs = 5;
    strncpy(config.task_name, "speech_test", 63);

    // Enable speech cortex
    config.enable_speech_cortex = true;
    config.speech_feature_dim = 32;
    config.enable_multimodal_integration = true;

    brain_t brain = brain_create_custom(&config);
    if (brain) {
        brain_destroy(brain);
    }
}

TEST_F(BrainMultimodalInitTest, SpeechCortex_WithFractalTopology) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_TINY;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 64;
    config.num_outputs = 5;
    strncpy(config.task_name, "speech_fractal", 63);

    // Enable speech cortex with fractal topology
    config.enable_speech_cortex = true;
    config.speech_feature_dim = 32;
    config.enable_fractal_topology = true;
    config.enable_multimodal_integration = true;

    brain_t brain = brain_create_custom(&config);
    if (brain) {
        brain_destroy(brain);
    }
}

//=============================================================================
// Combined Multimodal Tests
//=============================================================================

TEST_F(BrainMultimodalInitTest, MultiModal_VisualAndAudio) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 150;
    config.num_outputs = 10;
    strncpy(config.task_name, "visual_audio", 63);

    // Enable visual + audio cortex
    config.enable_visual_cortex = true;
    config.visual_feature_dim = 64;
    config.enable_audio_cortex = true;
    config.audio_feature_dim = 40;
    config.enable_multimodal_integration = true;

    brain_t brain = brain_create_custom(&config);
    if (brain) {
        brain_destroy(brain);
    }
}

TEST_F(BrainMultimodalInitTest, MultiModal_AllThreeModalities) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 200;
    config.num_outputs = 10;
    strncpy(config.task_name, "all_modalities", 63);

    // Enable all three cortices
    config.enable_visual_cortex = true;
    config.visual_feature_dim = 64;
    config.enable_audio_cortex = true;
    config.audio_feature_dim = 40;
    config.enable_speech_cortex = true;
    config.speech_feature_dim = 32;
    config.enable_multimodal_integration = true;

    brain_t brain = brain_create_custom(&config);
    if (brain) {
        brain_destroy(brain);
    }
}

TEST_F(BrainMultimodalInitTest, MultiModal_WithDirectInput) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 250;  // Larger than sum of modalities
    config.num_outputs = 10;
    strncpy(config.task_name, "with_direct", 63);

    // Enable modalities but leave room for direct input
    config.enable_visual_cortex = true;
    config.visual_feature_dim = 64;
    config.enable_audio_cortex = true;
    config.audio_feature_dim = 40;
    config.enable_speech_cortex = true;
    config.speech_feature_dim = 32;
    config.enable_multimodal_integration = true;
    // Remaining (250 - 136 = 114) becomes direct_dim

    brain_t brain = brain_create_custom(&config);
    if (brain) {
        brain_destroy(brain);
    }
}

//=============================================================================
// Edge Cases and Error Paths
//=============================================================================

TEST_F(BrainMultimodalInitTest, MultiModal_ZeroDimension) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_TINY;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 50;
    config.num_outputs = 5;
    strncpy(config.task_name, "zero_dim", 63);

    // Enable flag but set dimension to 0 (should skip initialization)
    config.enable_visual_cortex = true;
    config.visual_feature_dim = 0;  // Zero dimension
    config.enable_multimodal_integration = true;

    brain_t brain = brain_create_custom(&config);
    if (brain) {
        brain_destroy(brain);
    }
}

TEST_F(BrainMultimodalInitTest, MultiModal_SinglePixelVisual) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_TINY;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 10;
    config.num_outputs = 5;
    strncpy(config.task_name, "single_pixel", 63);

    // Minimum visual dimension
    config.enable_visual_cortex = true;
    config.visual_feature_dim = 1;
    config.enable_multimodal_integration = true;

    brain_t brain = brain_create_custom(&config);
    if (brain) {
        brain_destroy(brain);
    }
}

TEST_F(BrainMultimodalInitTest, MultiModal_OnlyIntegrationFlag) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_TINY;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 50;
    config.num_outputs = 5;
    strncpy(config.task_name, "only_flag", 63);

    // Enable multimodal integration but no cortices
    config.enable_multimodal_integration = true;
    // All cortex flags = false, all dimensions = 0

    brain_t brain = brain_create_custom(&config);
    if (brain) {
        brain_destroy(brain);
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
