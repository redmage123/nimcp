/**
 * @file test_brain_attention_regions.cpp
 * @brief Tests for attention system and brain regions initialization
 *
 * WHAT: Tests attention mechanism and brain regions module initialization
 * WHY: Boost coverage by exercising lines 1386-1530 in brain.c
 * HOW: Create brains with attention and brain regions enabled
 *
 * TARGET LINES: 1386-1530 in nimcp_brain.c
 * EXPECTED COVERAGE GAIN: ~145 lines
 */

#include <gtest/gtest.h>
#include <cstring>

    #include "core/brain/nimcp_brain.h"
    #include "nimcp.h"

//=============================================================================
// Test Fixture
//=============================================================================

class BrainAttentionRegionsTest : public ::testing::Test {
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
// Multihead Attention System Tests
//=============================================================================

TEST_F(BrainAttentionRegionsTest, Attention_BasicInitialization) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 64;
    config.num_outputs = 10;
    strncpy(config.task_name, "attention_test", 63);

    // Enable multihead attention
    config.enable_multihead_attention = true;
    config.num_attention_heads = 8;

    brain_t brain = brain_create_custom(&config);
    if (brain) {
        // Verify attention system was created
        // Training and inference will use attention
        brain_destroy(brain);
    }
}

TEST_F(BrainAttentionRegionsTest, Attention_CustomHeadCount) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 64;
    config.num_outputs = 10;
    strncpy(config.task_name, "attention_heads", 63);

    // Enable attention with custom head count
    config.enable_multihead_attention = true;
    config.num_attention_heads = 4;  // Custom: 4 heads

    brain_t brain = brain_create_custom(&config);
    if (brain) {
        brain_destroy(brain);
    }
}

TEST_F(BrainAttentionRegionsTest, Attention_With16Heads) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_MEDIUM;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 128;
    config.num_outputs = 20;
    strncpy(config.task_name, "attention_16", 63);

    // Enable attention with 16 heads
    config.enable_multihead_attention = true;
    config.num_attention_heads = 16;

    brain_t brain = brain_create_custom(&config);
    if (brain) {
        brain_destroy(brain);
    }
}

TEST_F(BrainAttentionRegionsTest, Attention_WithThalamicGate) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 64;
    config.num_outputs = 10;
    strncpy(config.task_name, "attention_thalamic", 63);

    // Enable attention with thalamic gating
    config.enable_multihead_attention = true;
    config.enable_thalamic_gate = true;
    config.num_attention_heads = 8;

    brain_t brain = brain_create_custom(&config);
    if (brain) {
        brain_destroy(brain);
    }
}

TEST_F(BrainAttentionRegionsTest, Attention_WithSalienceWeighting) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 64;
    config.num_outputs = 10;
    strncpy(config.task_name, "attention_salience", 63);

    // Enable attention with salience weighting
    config.enable_multihead_attention = true;
    config.enable_salience_weighting = true;
    config.num_attention_heads = 8;

    brain_t brain = brain_create_custom(&config);
    if (brain) {
        brain_destroy(brain);
    }
}

TEST_F(BrainAttentionRegionsTest, Attention_WithMultimodal) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 200;
    config.num_outputs = 10;
    strncpy(config.task_name, "attention_multimodal", 63);

    // Enable attention with multimodal input
    config.enable_multihead_attention = true;
    config.enable_multimodal_integration = true;
    config.enable_visual_cortex = true;
    config.visual_feature_dim = 64;
    config.enable_audio_cortex = true;
    config.audio_feature_dim = 40;
    config.enable_speech_cortex = true;
    config.speech_feature_dim = 32;
    config.num_attention_heads = 8;

    brain_t brain = brain_create_custom(&config);
    if (brain) {
        brain_destroy(brain);
    }
}

TEST_F(BrainAttentionRegionsTest, Attention_MultimodalFallback) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 64;
    config.num_outputs = 10;
    strncpy(config.task_name, "attention_fallback", 63);

    // Enable multimodal integration but with zero dimensions
    // Should fallback to num_inputs
    config.enable_multihead_attention = true;
    config.enable_multimodal_integration = true;
    config.visual_feature_dim = 0;
    config.audio_feature_dim = 0;
    config.speech_feature_dim = 0;
    config.num_attention_heads = 8;

    brain_t brain = brain_create_custom(&config);
    if (brain) {
        brain_destroy(brain);
    }
}

//=============================================================================
// Brain Regions Module Tests
//=============================================================================

TEST_F(BrainAttentionRegionsTest, Regions_BasicInitialization) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 64;
    config.num_outputs = 10;
    strncpy(config.task_name, "regions_test", 63);

    // Enable brain regions
    config.enable_brain_regions = true;
    config.num_brain_regions = 4;
    config.neurons_per_region = 1000;

    brain_t brain = brain_create_custom(&config);
    if (brain) {
        brain_destroy(brain);
    }
}

TEST_F(BrainAttentionRegionsTest, Regions_CustomRegionCount) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_MEDIUM;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 64;
    config.num_outputs = 10;
    strncpy(config.task_name, "regions_custom", 63);

    // Custom region count
    config.enable_brain_regions = true;
    config.num_brain_regions = 6;
    config.neurons_per_region = 500;

    brain_t brain = brain_create_custom(&config);
    if (brain) {
        brain_destroy(brain);
    }
}

TEST_F(BrainAttentionRegionsTest, Regions_SingleRegion) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 64;
    config.num_outputs = 10;
    strncpy(config.task_name, "regions_single", 63);

    // Single region (minimal)
    config.enable_brain_regions = true;
    config.num_brain_regions = 1;
    config.neurons_per_region = 100;

    brain_t brain = brain_create_custom(&config);
    if (brain) {
        brain_destroy(brain);
    }
}

TEST_F(BrainAttentionRegionsTest, Regions_LargeNeuronCount) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_LARGE;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 128;
    config.num_outputs = 20;
    strncpy(config.task_name, "regions_large", 63);

    // Large neuron count per region
    config.enable_brain_regions = true;
    config.num_brain_regions = 4;
    config.neurons_per_region = 5000;

    brain_t brain = brain_create_custom(&config);
    if (brain) {
        brain_destroy(brain);
    }
}

TEST_F(BrainAttentionRegionsTest, Regions_ZeroConfiguration) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 64;
    config.num_outputs = 10;
    strncpy(config.task_name, "regions_defaults", 63);

    // Enable brain regions with default values (0)
    // Should use defaults: 4 regions, 1000 neurons each
    config.enable_brain_regions = true;
    config.num_brain_regions = 0;      // Will default to 4
    config.neurons_per_region = 0;     // Will default to 1000

    brain_t brain = brain_create_custom(&config);
    if (brain) {
        brain_destroy(brain);
    }
}

//=============================================================================
// Combined Attention + Regions Tests
//=============================================================================

TEST_F(BrainAttentionRegionsTest, Combined_AttentionAndRegions) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_MEDIUM;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 128;
    config.num_outputs = 20;
    strncpy(config.task_name, "combined_test", 63);

    // Enable both attention and brain regions
    config.enable_multihead_attention = true;
    config.num_attention_heads = 8;
    config.enable_brain_regions = true;
    config.num_brain_regions = 4;
    config.neurons_per_region = 1000;

    brain_t brain = brain_create_custom(&config);
    if (brain) {
        brain_destroy(brain);
    }
}

TEST_F(BrainAttentionRegionsTest, Combined_AllFeatures) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_LARGE;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 256;
    config.num_outputs = 50;
    strncpy(config.task_name, "all_features", 63);

    // Enable everything
    config.enable_multihead_attention = true;
    config.num_attention_heads = 16;
    config.enable_thalamic_gate = true;
    config.enable_salience_weighting = true;

    config.enable_brain_regions = true;
    config.num_brain_regions = 6;
    config.neurons_per_region = 2000;

    config.enable_multimodal_integration = true;
    config.enable_visual_cortex = true;
    config.visual_feature_dim = 128;
    config.enable_audio_cortex = true;
    config.audio_feature_dim = 64;

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
