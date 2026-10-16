/**
 * @file test_platform_tier.cpp
 * @brief Comprehensive unit tests for Platform Tier System (Portia Spider Foundation)
 *
 * WHAT: Tests for hardware-aware configuration tier detection and configuration
 * WHY:  Ensure correct tier detection, config validation, and module enablement
 * HOW:  Test tier detection logic, config retrieval, module flags, and validation
 *
 * TEST COVERAGE:
 * - Tier detection logic (FULL, MEDIUM, CONSTRAINED, MINIMAL)
 * - Config retrieval for each tier
 * - Cognitive module flags correctly set per tier
 * - Validation catches invalid configs
 * - Tier name strings
 * - Neuron count recommendations
 * - Edge cases and boundary conditions
 *
 * PORTIA SPIDER FOUNDATION:
 * Named after Portia fimbriata, a jumping spider with ~600,000 neurons that
 * demonstrates sophisticated cognition on minimal resources. This tier system
 * allows NIMCP to scale from server-class hardware down to IoT devices.
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
    #include "utils/platform/nimcp_platform_tier.h"
    #include "utils/platform/nimcp_system_resources.h"
    #include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class PlatformTierTest : public ::testing::Test {
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
// Tier Detection Tests
//=============================================================================

TEST_F(PlatformTierTest, DetectReturnsValidTier) {
    platform_tier_t tier = platform_tier_detect();

    // Auto-detected tiers range from BASIC(0) to FULL(4)
    // Specialized tiers (NEUROMORPHIC, QUANTUM) are never auto-detected
    EXPECT_GE(tier, PLATFORM_TIER_BASIC);
    EXPECT_LE(tier, PLATFORM_TIER_FULL);
}

TEST_F(PlatformTierTest, DetectIsDeterministic) {
    // Multiple calls should return same tier
    platform_tier_t tier1 = platform_tier_detect();
    platform_tier_t tier2 = platform_tier_detect();
    platform_tier_t tier3 = platform_tier_detect();

    EXPECT_EQ(tier1, tier2);
    EXPECT_EQ(tier2, tier3);
}

//=============================================================================
// Config Retrieval Tests
//=============================================================================

TEST_F(PlatformTierTest, GetConfigReturnsValidConfig) {
    for (int t = PLATFORM_TIER_BASIC; t <= PLATFORM_TIER_FULL; t++) {
        platform_tier_config_t config = platform_tier_get_config((platform_tier_t)t);

        EXPECT_GT(config.max_neurons, 0) << "Tier " << t << " has zero neurons";
        EXPECT_GT(config.memory_budget_mb, 0) << "Tier " << t << " has zero memory budget";
        EXPECT_GT(config.max_synapses_per_neuron, 0) << "Tier " << t << " has zero synapses";
        EXPECT_GT(config.initial_neurons, 0) << "Tier " << t << " has zero initial neurons";
    }
}

TEST_F(PlatformTierTest, GetConfigInvalidTierReturnsMinimal) {
    platform_tier_config_t config = platform_tier_get_config((platform_tier_t)999);

    EXPECT_EQ(config.tier, PLATFORM_TIER_MINIMAL);
}

TEST_F(PlatformTierTest, TierConstraintsDecrease) {
    // Higher tier numbers = more capable (BASIC=0 < MINIMAL=1 < ... < FULL=4)
    for (int t = PLATFORM_TIER_BASIC; t < PLATFORM_TIER_FULL; t++) {
        platform_tier_config_t lower = platform_tier_get_config((platform_tier_t)t);
        platform_tier_config_t higher = platform_tier_get_config((platform_tier_t)(t+1));

        EXPECT_LE(lower.max_neurons, higher.max_neurons)
            << "Tier " << t << " should have <= neurons than tier " << (t+1);
        EXPECT_LE(lower.memory_budget_mb, higher.memory_budget_mb)
            << "Tier " << t << " should have <= memory than tier " << (t+1);
    }
}

//=============================================================================
// FULL Tier Tests
//=============================================================================

TEST_F(PlatformTierTest, FullTierNeuronCount) {
    platform_tier_config_t config = platform_tier_get_config(PLATFORM_TIER_FULL);

    EXPECT_EQ(config.max_neurons, 1000000);
    EXPECT_EQ(config.max_synapses_per_neuron, 10000);
    EXPECT_EQ(config.initial_neurons, 10000);
}

TEST_F(PlatformTierTest, FullTierMemoryBudget) {
    platform_tier_config_t config = platform_tier_get_config(PLATFORM_TIER_FULL);

    EXPECT_EQ(config.memory_budget_mb, 4096);
}

TEST_F(PlatformTierTest, FullTierVisualCortex) {
    platform_tier_config_t config = platform_tier_get_config(PLATFORM_TIER_FULL);

    EXPECT_EQ(config.visual.max_input_width, 640);
    EXPECT_EQ(config.visual.max_input_height, 480);
    EXPECT_EQ(config.visual.num_filters_conv1, 64);
    EXPECT_EQ(config.visual.num_filters_conv2, 128);
    EXPECT_EQ(config.visual.max_feature_maps, 256);
    EXPECT_TRUE(config.visual.enable_pooling);
    EXPECT_TRUE(config.visual.enable_attention);
}

TEST_F(PlatformTierTest, FullTierAudioCortex) {
    platform_tier_config_t config = platform_tier_get_config(PLATFORM_TIER_FULL);

    EXPECT_EQ(config.audio.max_sample_rate, 48000);
    EXPECT_EQ(config.audio.num_mel_filters, 128);
    EXPECT_EQ(config.audio.num_mfcc, 40);
    EXPECT_EQ(config.audio.frame_size, 2048);
    EXPECT_TRUE(config.audio.enable_attention);
    EXPECT_TRUE(config.audio.enable_memory);
}

TEST_F(PlatformTierTest, FullTierAllModulesEnabled) {
    platform_tier_config_t config = platform_tier_get_config(PLATFORM_TIER_FULL);

    // Core modules
    EXPECT_TRUE(config.cognitive_modules_enabled & COGNITIVE_MODULE_ATTENTION);
    EXPECT_TRUE(config.cognitive_modules_enabled & COGNITIVE_MODULE_WORKING_MEMORY);
    EXPECT_TRUE(config.cognitive_modules_enabled & COGNITIVE_MODULE_SALIENCE);

    // Emotional modules
    EXPECT_TRUE(config.cognitive_modules_enabled & COGNITIVE_MODULE_EMOTIONS);
    EXPECT_TRUE(config.cognitive_modules_enabled & COGNITIVE_MODULE_EMOTIONAL_TAG);

    // Memory systems
    EXPECT_TRUE(config.cognitive_modules_enabled & COGNITIVE_MODULE_SEMANTIC_MEMORY);
    EXPECT_TRUE(config.cognitive_modules_enabled & COGNITIVE_MODULE_EPISODIC_MEMORY);
    EXPECT_TRUE(config.cognitive_modules_enabled & COGNITIVE_MODULE_CONSOLIDATION);

    // Executive functions
    EXPECT_TRUE(config.cognitive_modules_enabled & COGNITIVE_MODULE_EXECUTIVE);
    EXPECT_TRUE(config.cognitive_modules_enabled & COGNITIVE_MODULE_REASONING);
    EXPECT_TRUE(config.cognitive_modules_enabled & COGNITIVE_MODULE_CURIOSITY);

    // Meta-cognitive
    EXPECT_TRUE(config.cognitive_modules_enabled & COGNITIVE_MODULE_META_LEARNING);
    EXPECT_TRUE(config.cognitive_modules_enabled & COGNITIVE_MODULE_INTROSPECTION);
    EXPECT_TRUE(config.cognitive_modules_enabled & COGNITIVE_MODULE_SELF_AWARENESS);

    // Social cognition
    EXPECT_TRUE(config.cognitive_modules_enabled & COGNITIVE_MODULE_THEORY_OF_MIND);
    EXPECT_TRUE(config.cognitive_modules_enabled & COGNITIVE_MODULE_MIRROR_NEURONS);
    EXPECT_TRUE(config.cognitive_modules_enabled & COGNITIVE_MODULE_EMPATHY);

    // Advanced features
    EXPECT_TRUE(config.cognitive_modules_enabled & COGNITIVE_MODULE_GLOBAL_WORKSPACE);
    EXPECT_TRUE(config.cognitive_modules_enabled & COGNITIVE_MODULE_PREDICTIVE);
    EXPECT_TRUE(config.cognitive_modules_enabled & COGNITIVE_MODULE_ETHICS);

    // Perception
    EXPECT_TRUE(config.cognitive_modules_enabled & COGNITIVE_MODULE_VISUAL_CORTEX);
    EXPECT_TRUE(config.cognitive_modules_enabled & COGNITIVE_MODULE_AUDIO_CORTEX);
}

TEST_F(PlatformTierTest, FullTierFeatureFlags) {
    platform_tier_config_t config = platform_tier_get_config(PLATFORM_TIER_FULL);

    EXPECT_TRUE(config.enable_gpu);
    EXPECT_TRUE(config.enable_bio_async);
    EXPECT_TRUE(config.enable_plasticity);
    EXPECT_TRUE(config.enable_neuromodulation);
    EXPECT_TRUE(config.enable_checkpointing);
}

//=============================================================================
// MEDIUM Tier Tests
//=============================================================================

TEST_F(PlatformTierTest, MediumTierNeuronCount) {
    platform_tier_config_t config = platform_tier_get_config(PLATFORM_TIER_MEDIUM);

    EXPECT_EQ(config.max_neurons, 100000);
    EXPECT_EQ(config.max_synapses_per_neuron, 1000);
    EXPECT_EQ(config.initial_neurons, 5000);
}

TEST_F(PlatformTierTest, MediumTierMemoryBudget) {
    platform_tier_config_t config = platform_tier_get_config(PLATFORM_TIER_MEDIUM);

    EXPECT_EQ(config.memory_budget_mb, 1024);
}

TEST_F(PlatformTierTest, MediumTierVisualCortex) {
    platform_tier_config_t config = platform_tier_get_config(PLATFORM_TIER_MEDIUM);

    EXPECT_EQ(config.visual.max_input_width, 320);
    EXPECT_EQ(config.visual.max_input_height, 240);
    EXPECT_EQ(config.visual.num_filters_conv1, 32);
    EXPECT_EQ(config.visual.num_filters_conv2, 64);
    EXPECT_TRUE(config.visual.enable_pooling);
    EXPECT_TRUE(config.visual.enable_attention);
}

TEST_F(PlatformTierTest, MediumTierCoreModulesEnabled) {
    platform_tier_config_t config = platform_tier_get_config(PLATFORM_TIER_MEDIUM);

    // Core modules enabled
    EXPECT_TRUE(config.cognitive_modules_enabled & COGNITIVE_MODULE_ATTENTION);
    EXPECT_TRUE(config.cognitive_modules_enabled & COGNITIVE_MODULE_WORKING_MEMORY);
    EXPECT_TRUE(config.cognitive_modules_enabled & COGNITIVE_MODULE_EXECUTIVE);
    EXPECT_TRUE(config.cognitive_modules_enabled & COGNITIVE_MODULE_REASONING);

    // Advanced meta-cognitive disabled
    EXPECT_FALSE(config.cognitive_modules_enabled & COGNITIVE_MODULE_CURIOSITY);
    EXPECT_FALSE(config.cognitive_modules_enabled & COGNITIVE_MODULE_META_LEARNING);
    EXPECT_FALSE(config.cognitive_modules_enabled & COGNITIVE_MODULE_INTROSPECTION);
}

//=============================================================================
// CONSTRAINED Tier Tests
//=============================================================================

TEST_F(PlatformTierTest, ConstrainedTierNeuronCount) {
    platform_tier_config_t config = platform_tier_get_config(PLATFORM_TIER_CONSTRAINED);

    EXPECT_EQ(config.max_neurons, 10000);
    EXPECT_EQ(config.max_synapses_per_neuron, 100);
    EXPECT_EQ(config.initial_neurons, 1000);
}

TEST_F(PlatformTierTest, ConstrainedTierMemoryBudget) {
    platform_tier_config_t config = platform_tier_get_config(PLATFORM_TIER_CONSTRAINED);

    EXPECT_EQ(config.memory_budget_mb, 128);
}

TEST_F(PlatformTierTest, ConstrainedTierVisualCortex) {
    platform_tier_config_t config = platform_tier_get_config(PLATFORM_TIER_CONSTRAINED);

    EXPECT_EQ(config.visual.max_input_width, 160);
    EXPECT_EQ(config.visual.max_input_height, 120);
    EXPECT_FALSE(config.visual.enable_attention);
}

TEST_F(PlatformTierTest, ConstrainedTierEssentialModulesOnly) {
    platform_tier_config_t config = platform_tier_get_config(PLATFORM_TIER_CONSTRAINED);

    // Essential modules enabled
    EXPECT_TRUE(config.cognitive_modules_enabled & COGNITIVE_MODULE_ATTENTION);
    EXPECT_TRUE(config.cognitive_modules_enabled & COGNITIVE_MODULE_WORKING_MEMORY);

    // Advanced features disabled
    EXPECT_FALSE(config.cognitive_modules_enabled & COGNITIVE_MODULE_EXECUTIVE);
    EXPECT_FALSE(config.cognitive_modules_enabled & COGNITIVE_MODULE_REASONING);
    EXPECT_FALSE(config.cognitive_modules_enabled & COGNITIVE_MODULE_META_LEARNING);
}

TEST_F(PlatformTierTest, ConstrainedTierFeatureFlags) {
    platform_tier_config_t config = platform_tier_get_config(PLATFORM_TIER_CONSTRAINED);

    EXPECT_FALSE(config.enable_gpu);
    EXPECT_TRUE(config.enable_bio_async);
    EXPECT_TRUE(config.enable_plasticity);
    EXPECT_FALSE(config.enable_neuromodulation);
    EXPECT_FALSE(config.enable_checkpointing);
}

//=============================================================================
// MINIMAL Tier Tests (Portia Spider Scale)
//=============================================================================

TEST_F(PlatformTierTest, MinimalTierNeuronCount) {
    platform_tier_config_t config = platform_tier_get_config(PLATFORM_TIER_MINIMAL);

    EXPECT_EQ(config.max_neurons, 1000);  // Portia-scale!
    EXPECT_EQ(config.max_synapses_per_neuron, 10);
    EXPECT_EQ(config.initial_neurons, 100);
}

TEST_F(PlatformTierTest, MinimalTierMemoryBudget) {
    platform_tier_config_t config = platform_tier_get_config(PLATFORM_TIER_MINIMAL);

    EXPECT_EQ(config.memory_budget_mb, 32);
}

TEST_F(PlatformTierTest, MinimalTierVisualCortex) {
    platform_tier_config_t config = platform_tier_get_config(PLATFORM_TIER_MINIMAL);

    EXPECT_EQ(config.visual.max_input_width, 64);
    EXPECT_EQ(config.visual.max_input_height, 48);
    EXPECT_FALSE(config.visual.enable_pooling);
    EXPECT_FALSE(config.visual.enable_attention);
}

TEST_F(PlatformTierTest, MinimalTierBareEssentialsOnly) {
    platform_tier_config_t config = platform_tier_get_config(PLATFORM_TIER_MINIMAL);

    // Only basic attention and visual
    EXPECT_TRUE(config.cognitive_modules_enabled & COGNITIVE_MODULE_ATTENTION);
    EXPECT_TRUE(config.cognitive_modules_enabled & COGNITIVE_MODULE_VISUAL_CORTEX);

    // No audio (too expensive)
    EXPECT_FALSE(config.cognitive_modules_enabled & COGNITIVE_MODULE_AUDIO_CORTEX);

    // No working memory
    EXPECT_FALSE(config.cognitive_modules_enabled & COGNITIVE_MODULE_WORKING_MEMORY);

    // No advanced cognition
    EXPECT_FALSE(config.cognitive_modules_enabled & COGNITIVE_MODULE_EXECUTIVE);
    EXPECT_FALSE(config.cognitive_modules_enabled & COGNITIVE_MODULE_REASONING);
}

TEST_F(PlatformTierTest, MinimalTierAllFeaturesDisabled) {
    platform_tier_config_t config = platform_tier_get_config(PLATFORM_TIER_MINIMAL);

    EXPECT_FALSE(config.enable_gpu);
    EXPECT_FALSE(config.enable_bio_async);
    EXPECT_FALSE(config.enable_plasticity);
    EXPECT_FALSE(config.enable_neuromodulation);
    EXPECT_FALSE(config.enable_checkpointing);
}

//=============================================================================
// Tier Name Tests
//=============================================================================

TEST_F(PlatformTierTest, TierNameFull) {
    const char* name = platform_tier_get_name(PLATFORM_TIER_FULL);
    EXPECT_STREQ(name, "FULL");
}

TEST_F(PlatformTierTest, TierNameMedium) {
    const char* name = platform_tier_get_name(PLATFORM_TIER_MEDIUM);
    EXPECT_STREQ(name, "MEDIUM");
}

TEST_F(PlatformTierTest, TierNameConstrained) {
    const char* name = platform_tier_get_name(PLATFORM_TIER_CONSTRAINED);
    EXPECT_STREQ(name, "CONSTRAINED");
}

TEST_F(PlatformTierTest, TierNameMinimal) {
    const char* name = platform_tier_get_name(PLATFORM_TIER_MINIMAL);
    EXPECT_STREQ(name, "MINIMAL");
}

TEST_F(PlatformTierTest, TierNameInvalid) {
    const char* name = platform_tier_get_name((platform_tier_t)999);
    EXPECT_STREQ(name, "UNKNOWN");
}

//=============================================================================
// Module Enablement Tests
//=============================================================================

TEST_F(PlatformTierTest, CanEnableModuleValidation) {
    // Attention enabled on all tiers
    EXPECT_TRUE(platform_tier_can_enable_module(PLATFORM_TIER_FULL, COGNITIVE_MODULE_ATTENTION));
    EXPECT_TRUE(platform_tier_can_enable_module(PLATFORM_TIER_MEDIUM, COGNITIVE_MODULE_ATTENTION));
    EXPECT_TRUE(platform_tier_can_enable_module(PLATFORM_TIER_CONSTRAINED, COGNITIVE_MODULE_ATTENTION));
    EXPECT_TRUE(platform_tier_can_enable_module(PLATFORM_TIER_MINIMAL, COGNITIVE_MODULE_ATTENTION));
}

TEST_F(PlatformTierTest, CanEnableModuleCuriosityOnlyHighTiers) {
    // Curiosity only on FULL
    EXPECT_TRUE(platform_tier_can_enable_module(PLATFORM_TIER_FULL, COGNITIVE_MODULE_CURIOSITY));
    EXPECT_FALSE(platform_tier_can_enable_module(PLATFORM_TIER_MEDIUM, COGNITIVE_MODULE_CURIOSITY));
    EXPECT_FALSE(platform_tier_can_enable_module(PLATFORM_TIER_CONSTRAINED, COGNITIVE_MODULE_CURIOSITY));
    EXPECT_FALSE(platform_tier_can_enable_module(PLATFORM_TIER_MINIMAL, COGNITIVE_MODULE_CURIOSITY));
}

TEST_F(PlatformTierTest, CanEnableModuleInvalidTier) {
    EXPECT_FALSE(platform_tier_can_enable_module((platform_tier_t)999, COGNITIVE_MODULE_ATTENTION));
}

//=============================================================================
// Neuron Count Recommendation Tests
//=============================================================================

TEST_F(PlatformTierTest, RecommendNeuronCountCapsAtTierMax) {
    system_resources_t resources = {0};
    resources.available_ram_mb = 100000;  // Huge amount

    for (int t = PLATFORM_TIER_BASIC; t <= PLATFORM_TIER_FULL; t++) {
        platform_tier_config_t config = platform_tier_get_config((platform_tier_t)t);
        uint32_t recommended = platform_tier_recommend_neuron_count((platform_tier_t)t, &resources);

        EXPECT_LE(recommended, config.max_neurons)
            << "Recommended count exceeds tier max for tier " << t;
    }
}

TEST_F(PlatformTierTest, RecommendNeuronCountRespectsMemory) {
    system_resources_t resources = {0};
    resources.available_ram_mb = 10;  // Very low

    uint32_t recommended = platform_tier_recommend_neuron_count(PLATFORM_TIER_FULL, &resources);

    // Should be much less than tier max due to memory constraint
    EXPECT_LT(recommended, 10000);
}

TEST_F(PlatformTierTest, RecommendNeuronCountMinimumBound) {
    system_resources_t resources = {0};
    resources.available_ram_mb = 1;  // Minimal

    for (int t = PLATFORM_TIER_BASIC; t <= PLATFORM_TIER_FULL; t++) {
        uint32_t recommended = platform_tier_recommend_neuron_count((platform_tier_t)t, &resources);

        // Should return at least a small positive count even with minimal RAM
        EXPECT_GT(recommended, (uint32_t)0)
            << "Recommended count is zero for tier " << t;
    }
}

TEST_F(PlatformTierTest, RecommendNeuronCountNullResources) {
    uint32_t recommended = platform_tier_recommend_neuron_count(PLATFORM_TIER_FULL, nullptr);

    // Should return safe default
    EXPECT_EQ(recommended, 1000);
}

TEST_F(PlatformTierTest, RecommendNeuronCountInvalidTier) {
    system_resources_t resources = {0};
    resources.available_ram_mb = 8192;

    uint32_t recommended = platform_tier_recommend_neuron_count((platform_tier_t)999, &resources);

    // Should return safe default
    EXPECT_EQ(recommended, 1000);
}

//=============================================================================
// Config Validation Tests
//=============================================================================

TEST_F(PlatformTierTest, ValidateConfigAcceptsDefault) {
    for (int t = PLATFORM_TIER_BASIC; t <= PLATFORM_TIER_FULL; t++) {
        platform_tier_config_t config = platform_tier_get_config((platform_tier_t)t);
        char error[256];

        bool valid = platform_tier_validate_config((platform_tier_t)t, &config, error, sizeof(error));

        EXPECT_TRUE(valid) << "Default config invalid for tier " << t << ": " << error;
    }
}

TEST_F(PlatformTierTest, ValidateConfigRejectsExcessiveNeurons) {
    platform_tier_config_t config = platform_tier_get_config(PLATFORM_TIER_MINIMAL);
    config.max_neurons = 1000000000;  // Way too many

    char error[256];
    bool valid = platform_tier_validate_config(PLATFORM_TIER_MINIMAL, &config, error, sizeof(error));

    EXPECT_FALSE(valid);
    EXPECT_NE(strstr(error, "max_neurons"), nullptr);
}

TEST_F(PlatformTierTest, ValidateConfigRejectsExcessiveSynapses) {
    platform_tier_config_t config = platform_tier_get_config(PLATFORM_TIER_MINIMAL);
    config.max_synapses_per_neuron = 100000;  // Way too many

    char error[256];
    bool valid = platform_tier_validate_config(PLATFORM_TIER_MINIMAL, &config, error, sizeof(error));

    EXPECT_FALSE(valid);
    EXPECT_NE(strstr(error, "synapses"), nullptr);
}

TEST_F(PlatformTierTest, ValidateConfigRejectsExcessiveMemory) {
    platform_tier_config_t config = platform_tier_get_config(PLATFORM_TIER_MINIMAL);
    config.memory_budget_mb = 10000;  // Way too much

    char error[256];
    bool valid = platform_tier_validate_config(PLATFORM_TIER_MINIMAL, &config, error, sizeof(error));

    EXPECT_FALSE(valid);
    EXPECT_NE(strstr(error, "memory"), nullptr);
}

TEST_F(PlatformTierTest, ValidateConfigRejectsExcessiveVisualResolution) {
    platform_tier_config_t config = platform_tier_get_config(PLATFORM_TIER_MINIMAL);
    config.visual.max_input_width = 4096;
    config.visual.max_input_height = 2160;

    char error[256];
    bool valid = platform_tier_validate_config(PLATFORM_TIER_MINIMAL, &config, error, sizeof(error));

    EXPECT_FALSE(valid);
    EXPECT_NE(strstr(error, "Visual"), nullptr);
}

TEST_F(PlatformTierTest, ValidateConfigRejectsExcessiveAudioSampleRate) {
    platform_tier_config_t config = platform_tier_get_config(PLATFORM_TIER_MINIMAL);
    config.audio.max_sample_rate = 96000;

    char error[256];
    bool valid = platform_tier_validate_config(PLATFORM_TIER_MINIMAL, &config, error, sizeof(error));

    EXPECT_FALSE(valid);
    EXPECT_NE(strstr(error, "Audio"), nullptr);
}

TEST_F(PlatformTierTest, ValidateConfigRejectsUnsupportedModules) {
    platform_tier_config_t config = platform_tier_get_config(PLATFORM_TIER_MINIMAL);
    config.cognitive_modules_enabled = COGNITIVE_MODULE_ALL;  // Enable everything

    char error[256];
    bool valid = platform_tier_validate_config(PLATFORM_TIER_MINIMAL, &config, error, sizeof(error));

    EXPECT_FALSE(valid);
    EXPECT_NE(strstr(error, "modules"), nullptr);
}

TEST_F(PlatformTierTest, ValidateConfigNullConfig) {
    char error[256];
    bool valid = platform_tier_validate_config(PLATFORM_TIER_FULL, nullptr, error, sizeof(error));

    EXPECT_FALSE(valid);
}

TEST_F(PlatformTierTest, ValidateConfigInvalidTier) {
    platform_tier_config_t config = platform_tier_get_config(PLATFORM_TIER_FULL);
    char error[256];

    bool valid = platform_tier_validate_config((platform_tier_t)999, &config, error, sizeof(error));

    EXPECT_FALSE(valid);
}

TEST_F(PlatformTierTest, ValidateConfigNullErrorBuffer) {
    platform_tier_config_t config = platform_tier_get_config(PLATFORM_TIER_FULL);

    // Should not crash with null error buffer
    bool valid = platform_tier_validate_config(PLATFORM_TIER_FULL, &config, nullptr, 0);

    EXPECT_TRUE(valid);
}

//=============================================================================
// Performance Budget Tests
//=============================================================================

TEST_F(PlatformTierTest, ComputeBudgetDecreases) {
    // Higher tier number = more capable, so compute budget increases
    for (int t = PLATFORM_TIER_BASIC; t < PLATFORM_TIER_FULL; t++) {
        platform_tier_config_t lower = platform_tier_get_config((platform_tier_t)t);
        platform_tier_config_t higher = platform_tier_get_config((platform_tier_t)(t+1));

        EXPECT_LE(lower.compute_budget_ops, higher.compute_budget_ops);
    }
}

TEST_F(PlatformTierTest, ThreadCountDecreases) {
    // Higher tier number = more capable, so thread count increases
    for (int t = PLATFORM_TIER_BASIC; t < PLATFORM_TIER_FULL; t++) {
        platform_tier_config_t lower = platform_tier_get_config((platform_tier_t)t);
        platform_tier_config_t higher = platform_tier_get_config((platform_tier_t)(t+1));

        EXPECT_LE(lower.max_threads, higher.max_threads);
    }
}

TEST_F(PlatformTierTest, SamplingRateDecreases) {
    // Higher tier number = more capable, so sampling rate increases
    for (int t = PLATFORM_TIER_BASIC; t < PLATFORM_TIER_FULL; t++) {
        platform_tier_config_t lower = platform_tier_get_config((platform_tier_t)t);
        platform_tier_config_t higher = platform_tier_get_config((platform_tier_t)(t+1));

        EXPECT_LE(lower.sampling_rate, higher.sampling_rate);
    }
}

//=============================================================================
// Integration Tests (Config Consistency)
//=============================================================================

TEST_F(PlatformTierTest, ConfigTierMatchesRequest) {
    for (int t = PLATFORM_TIER_BASIC; t <= PLATFORM_TIER_FULL; t++) {
        platform_tier_config_t config = platform_tier_get_config((platform_tier_t)t);

        EXPECT_EQ(config.tier, (platform_tier_t)t);
    }
}

TEST_F(PlatformTierTest, InitialNeuronsLessThanMax) {
    for (int t = PLATFORM_TIER_BASIC; t <= PLATFORM_TIER_FULL; t++) {
        platform_tier_config_t config = platform_tier_get_config((platform_tier_t)t);

        EXPECT_LE(config.initial_neurons, config.max_neurons)
            << "Initial neurons exceeds max for tier " << t;
    }
}

TEST_F(PlatformTierTest, BatchSizeLessThanMax) {
    for (int t = PLATFORM_TIER_BASIC; t <= PLATFORM_TIER_FULL; t++) {
        platform_tier_config_t config = platform_tier_get_config((platform_tier_t)t);

        EXPECT_LE(config.update_batch_size, config.max_neurons)
            << "Batch size exceeds max neurons for tier " << t;
    }
}
