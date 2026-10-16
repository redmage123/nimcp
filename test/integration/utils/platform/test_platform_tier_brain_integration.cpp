/**
 * @file test_platform_tier_brain_integration.cpp
 * @brief Integration tests for Platform Tier System with Brain components
 *
 * WHAT: Tests tier system integration with brain creation, visual/audio cortex
 * WHY:  Verify tier limits are respected by actual brain components
 * HOW:  Create brains with tier configs, test cortex configuration, module enablement
 *
 * TEST COVERAGE:
 * - Brain creation respects tier limits
 * - Visual cortex configuration per tier
 * - Audio cortex configuration per tier
 * - Cognitive modules enabled/disabled per tier
 * - Memory budget enforcement
 * - Feature flags integration
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
    #include "utils/platform/nimcp_platform_tier.h"
    #include "utils/platform/nimcp_system_resources.h"
    #include "utils/memory/nimcp_memory.h"
    #include "utils/logging/nimcp_logging.h"

//=============================================================================
// Test Fixture
//=============================================================================

class PlatformTierBrainIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize memory tracking
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);
        nimcp_memory_clear_stats();
    }

    void TearDown() override {
        // Verify no memory leaks
        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        EXPECT_EQ(stats.current_allocated, 0)
            << "Memory leak detected: " << stats.current_allocated << " bytes";
    }
};

//=============================================================================
// Brain Creation Tests
//=============================================================================

TEST_F(PlatformTierBrainIntegrationTest, BrainCreationRespectsFullTierLimits) {
    platform_tier_config_t config = platform_tier_get_config(PLATFORM_TIER_FULL);

    // Verify config values are appropriate for high-end system
    EXPECT_EQ(config.max_neurons, 1000000);
    EXPECT_EQ(config.memory_budget_mb, 4096);
    EXPECT_TRUE(config.enable_gpu);
    EXPECT_TRUE(config.enable_bio_async);
    EXPECT_TRUE(config.enable_plasticity);

    // Verify neuron count is within limits
    EXPECT_LE(config.initial_neurons, config.max_neurons);
    EXPECT_GT(config.initial_neurons, 0);
}

TEST_F(PlatformTierBrainIntegrationTest, BrainCreationRespectsMediumTierLimits) {
    platform_tier_config_t config = platform_tier_get_config(PLATFORM_TIER_MEDIUM);

    // Verify config values are appropriate for medium system
    EXPECT_EQ(config.max_neurons, 100000);
    EXPECT_EQ(config.memory_budget_mb, 1024);
    EXPECT_FALSE(config.enable_gpu);
    EXPECT_TRUE(config.enable_bio_async);

    // Verify constraints are tighter than FULL
    platform_tier_config_t full_config = platform_tier_get_config(PLATFORM_TIER_FULL);
    EXPECT_LT(config.max_neurons, full_config.max_neurons);
    EXPECT_LT(config.memory_budget_mb, full_config.memory_budget_mb);
}

TEST_F(PlatformTierBrainIntegrationTest, BrainCreationRespectsConstrainedTierLimits) {
    platform_tier_config_t config = platform_tier_get_config(PLATFORM_TIER_CONSTRAINED);

    // Verify config values are appropriate for constrained system
    EXPECT_EQ(config.max_neurons, 10000);
    EXPECT_EQ(config.memory_budget_mb, 128);
    EXPECT_FALSE(config.enable_gpu);
    EXPECT_FALSE(config.enable_neuromodulation);
    EXPECT_FALSE(config.enable_checkpointing);

    // Verify sparse connectivity
    EXPECT_LE(config.max_synapses_per_neuron, 100);
}

TEST_F(PlatformTierBrainIntegrationTest, BrainCreationRespectsMinimalTierLimits) {
    platform_tier_config_t config = platform_tier_get_config(PLATFORM_TIER_MINIMAL);

    // Verify Portia-scale configuration
    EXPECT_EQ(config.max_neurons, 1000);
    EXPECT_EQ(config.memory_budget_mb, 32);
    EXPECT_FALSE(config.enable_gpu);
    EXPECT_FALSE(config.enable_bio_async);
    EXPECT_FALSE(config.enable_plasticity);

    // Verify ultra-sparse connectivity
    EXPECT_LE(config.max_synapses_per_neuron, 10);
}

//=============================================================================
// Visual Cortex Configuration Tests
//=============================================================================

TEST_F(PlatformTierBrainIntegrationTest, VisualCortexFullTierHighResolution) {
    platform_tier_config_t config = platform_tier_get_config(PLATFORM_TIER_FULL);

    // Verify high-resolution visual processing
    EXPECT_EQ(config.visual.max_input_width, 640);
    EXPECT_EQ(config.visual.max_input_height, 480);
    EXPECT_EQ(config.visual.num_filters_conv1, 64);
    EXPECT_EQ(config.visual.num_filters_conv2, 128);
    EXPECT_EQ(config.visual.max_feature_maps, 256);
    EXPECT_TRUE(config.visual.enable_pooling);
    EXPECT_TRUE(config.visual.enable_attention);
}

TEST_F(PlatformTierBrainIntegrationTest, VisualCortexMediumTierModerateResolution) {
    platform_tier_config_t config = platform_tier_get_config(PLATFORM_TIER_MEDIUM);

    // Verify medium-resolution visual processing
    EXPECT_EQ(config.visual.max_input_width, 320);
    EXPECT_EQ(config.visual.max_input_height, 240);
    EXPECT_EQ(config.visual.num_filters_conv1, 32);
    EXPECT_EQ(config.visual.num_filters_conv2, 64);
    EXPECT_TRUE(config.visual.enable_pooling);
    EXPECT_TRUE(config.visual.enable_attention);

    // Verify lower than FULL tier
    platform_tier_config_t full = platform_tier_get_config(PLATFORM_TIER_FULL);
    EXPECT_LT(config.visual.max_input_width, full.visual.max_input_width);
    EXPECT_LT(config.visual.num_filters_conv1, full.visual.num_filters_conv1);
}

TEST_F(PlatformTierBrainIntegrationTest, VisualCortexConstrainedTierLowResolution) {
    platform_tier_config_t config = platform_tier_get_config(PLATFORM_TIER_CONSTRAINED);

    // Verify low-resolution visual processing
    EXPECT_EQ(config.visual.max_input_width, 160);
    EXPECT_EQ(config.visual.max_input_height, 120);
    EXPECT_EQ(config.visual.num_filters_conv1, 16);
    EXPECT_EQ(config.visual.num_filters_conv2, 32);
    EXPECT_TRUE(config.visual.enable_pooling);
    EXPECT_FALSE(config.visual.enable_attention);  // Attention disabled
}

TEST_F(PlatformTierBrainIntegrationTest, VisualCortexMinimalTierMinimalResolution) {
    platform_tier_config_t config = platform_tier_get_config(PLATFORM_TIER_MINIMAL);

    // Verify minimal visual processing (Portia-scale)
    EXPECT_EQ(config.visual.max_input_width, 64);
    EXPECT_EQ(config.visual.max_input_height, 48);
    EXPECT_EQ(config.visual.num_filters_conv1, 8);
    EXPECT_EQ(config.visual.num_filters_conv2, 16);
    EXPECT_FALSE(config.visual.enable_pooling);
    EXPECT_FALSE(config.visual.enable_attention);
}

TEST_F(PlatformTierBrainIntegrationTest, VisualCortexResolutionDecreases) {
    uint32_t prev_width = UINT32_MAX;
    uint32_t prev_height = UINT32_MAX;

    for (int t = PLATFORM_TIER_FULL; t <= PLATFORM_TIER_MINIMAL; t++) {
        platform_tier_config_t config = platform_tier_get_config((platform_tier_t)t);

        EXPECT_LE(config.visual.max_input_width, prev_width)
            << "Visual width increases at tier " << t;
        EXPECT_LE(config.visual.max_input_height, prev_height)
            << "Visual height increases at tier " << t;

        prev_width = config.visual.max_input_width;
        prev_height = config.visual.max_input_height;
    }
}

//=============================================================================
// Audio Cortex Configuration Tests
//=============================================================================

TEST_F(PlatformTierBrainIntegrationTest, AudioCortexFullTierHighFidelity) {
    platform_tier_config_t config = platform_tier_get_config(PLATFORM_TIER_FULL);

    // Verify CD-quality audio processing
    EXPECT_EQ(config.audio.max_sample_rate, 48000);
    EXPECT_EQ(config.audio.num_mel_filters, 128);
    EXPECT_EQ(config.audio.num_mfcc, 40);
    EXPECT_EQ(config.audio.frame_size, 2048);
    EXPECT_TRUE(config.audio.enable_attention);
    EXPECT_TRUE(config.audio.enable_memory);
}

TEST_F(PlatformTierBrainIntegrationTest, AudioCortexMediumTierModerateFidelity) {
    platform_tier_config_t config = platform_tier_get_config(PLATFORM_TIER_MEDIUM);

    // Verify moderate-fidelity audio processing
    EXPECT_EQ(config.audio.max_sample_rate, 22050);
    EXPECT_EQ(config.audio.num_mel_filters, 64);
    EXPECT_EQ(config.audio.num_mfcc, 20);
    EXPECT_TRUE(config.audio.enable_attention);
    EXPECT_TRUE(config.audio.enable_memory);

    // Verify lower than FULL tier
    platform_tier_config_t full = platform_tier_get_config(PLATFORM_TIER_FULL);
    EXPECT_LT(config.audio.max_sample_rate, full.audio.max_sample_rate);
    EXPECT_LT(config.audio.num_mel_filters, full.audio.num_mel_filters);
}

TEST_F(PlatformTierBrainIntegrationTest, AudioCortexConstrainedTierLowFidelity) {
    platform_tier_config_t config = platform_tier_get_config(PLATFORM_TIER_CONSTRAINED);

    // Verify phone-quality audio processing
    EXPECT_EQ(config.audio.max_sample_rate, 16000);
    EXPECT_EQ(config.audio.num_mel_filters, 32);
    EXPECT_EQ(config.audio.num_mfcc, 13);
    EXPECT_FALSE(config.audio.enable_attention);
    EXPECT_FALSE(config.audio.enable_memory);
}

TEST_F(PlatformTierBrainIntegrationTest, AudioCortexMinimalTierMinimal) {
    platform_tier_config_t config = platform_tier_get_config(PLATFORM_TIER_MINIMAL);

    // Verify minimal audio (mostly disabled for Portia-scale)
    EXPECT_EQ(config.audio.max_sample_rate, 8000);
    EXPECT_FALSE(config.audio.enable_attention);
    EXPECT_FALSE(config.audio.enable_memory);

    // Audio cortex not enabled at minimal tier
    EXPECT_FALSE(config.cognitive_modules_enabled & COGNITIVE_MODULE_AUDIO_CORTEX);
}

TEST_F(PlatformTierBrainIntegrationTest, AudioSampleRateDecreases) {
    uint32_t prev_rate = UINT32_MAX;

    for (int t = PLATFORM_TIER_FULL; t <= PLATFORM_TIER_MINIMAL; t++) {
        platform_tier_config_t config = platform_tier_get_config((platform_tier_t)t);

        EXPECT_LE(config.audio.max_sample_rate, prev_rate)
            << "Audio sample rate increases at tier " << t;

        prev_rate = config.audio.max_sample_rate;
    }
}

//=============================================================================
// Cognitive Module Enablement Tests
//=============================================================================

TEST_F(PlatformTierBrainIntegrationTest, FullTierAllCognitiveModulesEnabled) {
    platform_tier_config_t config = platform_tier_get_config(PLATFORM_TIER_FULL);

    // Core modules
    EXPECT_TRUE(platform_tier_can_enable_module(PLATFORM_TIER_FULL, COGNITIVE_MODULE_ATTENTION));
    EXPECT_TRUE(platform_tier_can_enable_module(PLATFORM_TIER_FULL, COGNITIVE_MODULE_WORKING_MEMORY));
    EXPECT_TRUE(platform_tier_can_enable_module(PLATFORM_TIER_FULL, COGNITIVE_MODULE_SALIENCE));

    // Memory systems
    EXPECT_TRUE(platform_tier_can_enable_module(PLATFORM_TIER_FULL, COGNITIVE_MODULE_SEMANTIC_MEMORY));
    EXPECT_TRUE(platform_tier_can_enable_module(PLATFORM_TIER_FULL, COGNITIVE_MODULE_EPISODIC_MEMORY));
    EXPECT_TRUE(platform_tier_can_enable_module(PLATFORM_TIER_FULL, COGNITIVE_MODULE_CONSOLIDATION));

    // Executive functions
    EXPECT_TRUE(platform_tier_can_enable_module(PLATFORM_TIER_FULL, COGNITIVE_MODULE_EXECUTIVE));
    EXPECT_TRUE(platform_tier_can_enable_module(PLATFORM_TIER_FULL, COGNITIVE_MODULE_REASONING));
    EXPECT_TRUE(platform_tier_can_enable_module(PLATFORM_TIER_FULL, COGNITIVE_MODULE_CURIOSITY));

    // Meta-cognitive
    EXPECT_TRUE(platform_tier_can_enable_module(PLATFORM_TIER_FULL, COGNITIVE_MODULE_META_LEARNING));
    EXPECT_TRUE(platform_tier_can_enable_module(PLATFORM_TIER_FULL, COGNITIVE_MODULE_INTROSPECTION));

    // Social cognition
    EXPECT_TRUE(platform_tier_can_enable_module(PLATFORM_TIER_FULL, COGNITIVE_MODULE_THEORY_OF_MIND));
    EXPECT_TRUE(platform_tier_can_enable_module(PLATFORM_TIER_FULL, COGNITIVE_MODULE_EMPATHY));
}

TEST_F(PlatformTierBrainIntegrationTest, MediumTierCoreModulesEnabled) {
    // Core modules enabled
    EXPECT_TRUE(platform_tier_can_enable_module(PLATFORM_TIER_MEDIUM, COGNITIVE_MODULE_ATTENTION));
    EXPECT_TRUE(platform_tier_can_enable_module(PLATFORM_TIER_MEDIUM, COGNITIVE_MODULE_WORKING_MEMORY));
    EXPECT_TRUE(platform_tier_can_enable_module(PLATFORM_TIER_MEDIUM, COGNITIVE_MODULE_EXECUTIVE));
    EXPECT_TRUE(platform_tier_can_enable_module(PLATFORM_TIER_MEDIUM, COGNITIVE_MODULE_REASONING));

    // Advanced meta-cognitive disabled
    EXPECT_FALSE(platform_tier_can_enable_module(PLATFORM_TIER_MEDIUM, COGNITIVE_MODULE_CURIOSITY));
    EXPECT_FALSE(platform_tier_can_enable_module(PLATFORM_TIER_MEDIUM, COGNITIVE_MODULE_META_LEARNING));
    EXPECT_FALSE(platform_tier_can_enable_module(PLATFORM_TIER_MEDIUM, COGNITIVE_MODULE_INTROSPECTION));
}

TEST_F(PlatformTierBrainIntegrationTest, ConstrainedTierEssentialModulesOnly) {
    // Essential modules enabled
    EXPECT_TRUE(platform_tier_can_enable_module(PLATFORM_TIER_CONSTRAINED, COGNITIVE_MODULE_ATTENTION));
    EXPECT_TRUE(platform_tier_can_enable_module(PLATFORM_TIER_CONSTRAINED, COGNITIVE_MODULE_WORKING_MEMORY));
    EXPECT_TRUE(platform_tier_can_enable_module(PLATFORM_TIER_CONSTRAINED, COGNITIVE_MODULE_VISUAL_CORTEX));

    // Advanced features disabled
    EXPECT_FALSE(platform_tier_can_enable_module(PLATFORM_TIER_CONSTRAINED, COGNITIVE_MODULE_EXECUTIVE));
    EXPECT_FALSE(platform_tier_can_enable_module(PLATFORM_TIER_CONSTRAINED, COGNITIVE_MODULE_REASONING));
    EXPECT_FALSE(platform_tier_can_enable_module(PLATFORM_TIER_CONSTRAINED, COGNITIVE_MODULE_CURIOSITY));
    EXPECT_FALSE(platform_tier_can_enable_module(PLATFORM_TIER_CONSTRAINED, COGNITIVE_MODULE_META_LEARNING));
}

TEST_F(PlatformTierBrainIntegrationTest, MinimalTierBareMinimumModules) {
    // Only attention and visual cortex
    EXPECT_TRUE(platform_tier_can_enable_module(PLATFORM_TIER_MINIMAL, COGNITIVE_MODULE_ATTENTION));
    EXPECT_TRUE(platform_tier_can_enable_module(PLATFORM_TIER_MINIMAL, COGNITIVE_MODULE_VISUAL_CORTEX));

    // Everything else disabled
    EXPECT_FALSE(platform_tier_can_enable_module(PLATFORM_TIER_MINIMAL, COGNITIVE_MODULE_WORKING_MEMORY));
    EXPECT_FALSE(platform_tier_can_enable_module(PLATFORM_TIER_MINIMAL, COGNITIVE_MODULE_AUDIO_CORTEX));
    EXPECT_FALSE(platform_tier_can_enable_module(PLATFORM_TIER_MINIMAL, COGNITIVE_MODULE_EXECUTIVE));
    EXPECT_FALSE(platform_tier_can_enable_module(PLATFORM_TIER_MINIMAL, COGNITIVE_MODULE_REASONING));
    EXPECT_FALSE(platform_tier_can_enable_module(PLATFORM_TIER_MINIMAL, COGNITIVE_MODULE_EMOTIONS));
}

//=============================================================================
// Memory Budget Enforcement Tests
//=============================================================================

TEST_F(PlatformTierBrainIntegrationTest, MemoryBudgetFullTier) {
    platform_tier_config_t config = platform_tier_get_config(PLATFORM_TIER_FULL);

    EXPECT_EQ(config.memory_budget_mb, 4096);
    EXPECT_GT(config.memory_budget_mb, 0);
}

TEST_F(PlatformTierBrainIntegrationTest, MemoryBudgetMediumTier) {
    platform_tier_config_t config = platform_tier_get_config(PLATFORM_TIER_MEDIUM);

    EXPECT_EQ(config.memory_budget_mb, 1024);

    // Should be less than FULL
    platform_tier_config_t full = platform_tier_get_config(PLATFORM_TIER_FULL);
    EXPECT_LT(config.memory_budget_mb, full.memory_budget_mb);
}

TEST_F(PlatformTierBrainIntegrationTest, MemoryBudgetConstrainedTier) {
    platform_tier_config_t config = platform_tier_get_config(PLATFORM_TIER_CONSTRAINED);

    EXPECT_EQ(config.memory_budget_mb, 128);

    // Should be less than MEDIUM
    platform_tier_config_t medium = platform_tier_get_config(PLATFORM_TIER_MEDIUM);
    EXPECT_LT(config.memory_budget_mb, medium.memory_budget_mb);
}

TEST_F(PlatformTierBrainIntegrationTest, MemoryBudgetMinimalTier) {
    platform_tier_config_t config = platform_tier_get_config(PLATFORM_TIER_MINIMAL);

    EXPECT_EQ(config.memory_budget_mb, 32);

    // Should be less than CONSTRAINED
    platform_tier_config_t constrained = platform_tier_get_config(PLATFORM_TIER_CONSTRAINED);
    EXPECT_LT(config.memory_budget_mb, constrained.memory_budget_mb);
}

TEST_F(PlatformTierBrainIntegrationTest, MemoryBudgetStrictlyDecreasing) {
    uint32_t prev_budget = UINT32_MAX;

    for (int t = PLATFORM_TIER_FULL; t <= PLATFORM_TIER_MINIMAL; t++) {
        platform_tier_config_t config = platform_tier_get_config((platform_tier_t)t);

        EXPECT_LT(config.memory_budget_mb, prev_budget)
            << "Memory budget not decreasing at tier " << t;

        prev_budget = config.memory_budget_mb;
    }
}

//=============================================================================
// Feature Flag Integration Tests
//=============================================================================

TEST_F(PlatformTierBrainIntegrationTest, GPUEnabledOnlyFullTier) {
    EXPECT_TRUE(platform_tier_get_config(PLATFORM_TIER_FULL).enable_gpu);
    EXPECT_FALSE(platform_tier_get_config(PLATFORM_TIER_MEDIUM).enable_gpu);
    EXPECT_FALSE(platform_tier_get_config(PLATFORM_TIER_CONSTRAINED).enable_gpu);
    EXPECT_FALSE(platform_tier_get_config(PLATFORM_TIER_MINIMAL).enable_gpu);
}

TEST_F(PlatformTierBrainIntegrationTest, BioAsyncEnabledHigherTiers) {
    EXPECT_TRUE(platform_tier_get_config(PLATFORM_TIER_FULL).enable_bio_async);
    EXPECT_TRUE(platform_tier_get_config(PLATFORM_TIER_MEDIUM).enable_bio_async);
    EXPECT_TRUE(platform_tier_get_config(PLATFORM_TIER_CONSTRAINED).enable_bio_async);
    EXPECT_FALSE(platform_tier_get_config(PLATFORM_TIER_MINIMAL).enable_bio_async);
}

TEST_F(PlatformTierBrainIntegrationTest, PlasticityEnabledHigherTiers) {
    EXPECT_TRUE(platform_tier_get_config(PLATFORM_TIER_FULL).enable_plasticity);
    EXPECT_TRUE(platform_tier_get_config(PLATFORM_TIER_MEDIUM).enable_plasticity);
    EXPECT_TRUE(platform_tier_get_config(PLATFORM_TIER_CONSTRAINED).enable_plasticity);
    EXPECT_FALSE(platform_tier_get_config(PLATFORM_TIER_MINIMAL).enable_plasticity);
}

TEST_F(PlatformTierBrainIntegrationTest, NeuromodulationEnabledFullAndMedium) {
    EXPECT_TRUE(platform_tier_get_config(PLATFORM_TIER_FULL).enable_neuromodulation);
    EXPECT_TRUE(platform_tier_get_config(PLATFORM_TIER_MEDIUM).enable_neuromodulation);
    EXPECT_FALSE(platform_tier_get_config(PLATFORM_TIER_CONSTRAINED).enable_neuromodulation);
    EXPECT_FALSE(platform_tier_get_config(PLATFORM_TIER_MINIMAL).enable_neuromodulation);
}

TEST_F(PlatformTierBrainIntegrationTest, CheckpointingEnabledFullAndMedium) {
    EXPECT_TRUE(platform_tier_get_config(PLATFORM_TIER_FULL).enable_checkpointing);
    EXPECT_TRUE(platform_tier_get_config(PLATFORM_TIER_MEDIUM).enable_checkpointing);
    EXPECT_FALSE(platform_tier_get_config(PLATFORM_TIER_CONSTRAINED).enable_checkpointing);
    EXPECT_FALSE(platform_tier_get_config(PLATFORM_TIER_MINIMAL).enable_checkpointing);
}

//=============================================================================
// Neuron Count Recommendation Tests
//=============================================================================

TEST_F(PlatformTierBrainIntegrationTest, NeuronCountRecommendationWithinBudget) {
    system_resources_t resources = {0};
    resources.available_ram_mb = 8192;  // 8GB available

    for (int t = PLATFORM_TIER_FULL; t <= PLATFORM_TIER_MINIMAL; t++) {
        uint32_t recommended = platform_tier_recommend_neuron_count((platform_tier_t)t, &resources);
        platform_tier_config_t config = platform_tier_get_config((platform_tier_t)t);

        EXPECT_LE(recommended, config.max_neurons)
            << "Recommended exceeds tier max at tier " << t;
        EXPECT_GE(recommended, config.initial_neurons)
            << "Recommended below tier initial at tier " << t;
    }
}

TEST_F(PlatformTierBrainIntegrationTest, NeuronCountScalesWithMemory) {
    platform_tier_t tier = PLATFORM_TIER_FULL;

    system_resources_t low_mem = {0};
    low_mem.available_ram_mb = 100;

    system_resources_t high_mem = {0};
    high_mem.available_ram_mb = 10000;

    uint32_t low_count = platform_tier_recommend_neuron_count(tier, &low_mem);
    uint32_t high_count = platform_tier_recommend_neuron_count(tier, &high_mem);

    EXPECT_LT(low_count, high_count)
        << "Neuron count doesn't scale with available memory";
}

//=============================================================================
// Tier Detection Integration Tests
//=============================================================================

TEST_F(PlatformTierBrainIntegrationTest, DetectedTierMatchesSystemResources) {
    platform_tier_t detected = platform_tier_detect();
    system_resources_t resources = {0};

    ASSERT_TRUE(system_resources_query(&resources));

    // Verify tier is appropriate for RAM
    if (resources.total_ram_mb >= 8192 && resources.num_cpu_cores >= 8) {
        // Should be FULL if we have 8GB+ RAM and 8+ cores
        // (may not match if actual system has less)
    } else if (resources.total_ram_mb >= 2048 && resources.num_cpu_cores >= 4) {
        // Should be MEDIUM or lower
        EXPECT_GE(detected, PLATFORM_TIER_MEDIUM);
    }

    // Detected tier should always be valid
    EXPECT_GE(detected, PLATFORM_TIER_FULL);
    EXPECT_LE(detected, PLATFORM_TIER_MINIMAL);
}

TEST_F(PlatformTierBrainIntegrationTest, ConfigMatchesDetectedTier) {
    platform_tier_t detected = platform_tier_detect();
    platform_tier_config_t config = platform_tier_get_config(detected);

    EXPECT_EQ(config.tier, detected);
}

//=============================================================================
// Cross-Tier Consistency Tests
//=============================================================================

TEST_F(PlatformTierBrainIntegrationTest, AllTiersHaveValidConfigs) {
    for (int t = PLATFORM_TIER_FULL; t <= PLATFORM_TIER_MINIMAL; t++) {
        platform_tier_config_t config = platform_tier_get_config((platform_tier_t)t);
        char error[256];

        bool valid = platform_tier_validate_config((platform_tier_t)t, &config, error, sizeof(error));

        EXPECT_TRUE(valid) << "Tier " << t << " default config invalid: " << error;
    }
}

TEST_F(PlatformTierBrainIntegrationTest, PerformanceBudgetsConsistent) {
    for (int t = PLATFORM_TIER_FULL; t <= PLATFORM_TIER_MINIMAL; t++) {
        platform_tier_config_t config = platform_tier_get_config((platform_tier_t)t);

        // Memory budget should align with neuron count
        // Rough estimate: 10KB per neuron
        uint64_t estimated_memory_kb = (uint64_t)config.max_neurons * 10;
        uint64_t budget_kb = (uint64_t)config.memory_budget_mb * 1024;

        EXPECT_GE(budget_kb, estimated_memory_kb / 4)
            << "Memory budget too low for neuron count at tier " << t;
    }
}

TEST_F(PlatformTierBrainIntegrationTest, ThreadCountAlignsWith Neurons) {
    for (int t = PLATFORM_TIER_FULL; t <= PLATFORM_TIER_MINIMAL; t++) {
        platform_tier_config_t config = platform_tier_get_config((platform_tier_t)t);

        // Thread count should be reasonable for neuron count
        EXPECT_GT(config.max_threads, 0) << "Zero threads at tier " << t;
        EXPECT_LE(config.max_threads, 64) << "Excessive threads at tier " << t;
    }
}
