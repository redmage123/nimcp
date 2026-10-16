/**
 * @file test_platform_tier_regression.cpp
 * @brief Regression tests for Platform Tier System (Portia Spider Foundation)
 *
 * WHAT: Regression tests to ensure tier system stability and correctness
 * WHY:  Prevent regressions in tier detection, config limits, and module flags
 * HOW:  Test determinism, documented limits, tier consistency, memory budgets
 *
 * TEST COVERAGE:
 * - Tier detection is deterministic
 * - Config values don't exceed documented limits
 * - FULL tier enables all modules
 * - MINIMAL tier disables advanced cognition
 * - Memory budget stays within tier limits
 * - Visual/audio configs match specifications
 * - Feature flags match tier capabilities
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

class PlatformTierRegressionTest : public ::testing::Test {
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
// Determinism Regression Tests
//=============================================================================

TEST_F(PlatformTierRegressionTest, TierDetectionIsDeterministic) {
    // Call detection 100 times, should get same result
    platform_tier_t first = platform_tier_detect();

    for (int i = 0; i < 100; i++) {
        platform_tier_t tier = platform_tier_detect();
        EXPECT_EQ(tier, first)
            << "Tier detection non-deterministic at iteration " << i;
    }
}

TEST_F(PlatformTierRegressionTest, ConfigRetrievalIsDeterministic) {
    // Get config multiple times, should be identical
    for (int t = PLATFORM_TIER_FULL; t <= PLATFORM_TIER_MINIMAL; t++) {
        platform_tier_config_t first = platform_tier_get_config((platform_tier_t)t);

        for (int i = 0; i < 10; i++) {
            platform_tier_config_t config = platform_tier_get_config((platform_tier_t)t);

            EXPECT_EQ(config.max_neurons, first.max_neurons);
            EXPECT_EQ(config.memory_budget_mb, first.memory_budget_mb);
            EXPECT_EQ(config.cognitive_modules_enabled, first.cognitive_modules_enabled);
        }
    }
}

TEST_F(PlatformTierRegressionTest, ModuleCheckIsDeterministic) {
    for (int t = PLATFORM_TIER_FULL; t <= PLATFORM_TIER_MINIMAL; t++) {
        bool first = platform_tier_can_enable_module((platform_tier_t)t, COGNITIVE_MODULE_ATTENTION);

        for (int i = 0; i < 10; i++) {
            bool result = platform_tier_can_enable_module((platform_tier_t)t, COGNITIVE_MODULE_ATTENTION);
            EXPECT_EQ(result, first);
        }
    }
}

//=============================================================================
// Documented Limit Regression Tests
//=============================================================================

TEST_F(PlatformTierRegressionTest, FullTierNeuronLimitExact) {
    platform_tier_config_t config = platform_tier_get_config(PLATFORM_TIER_FULL);

    // Per header documentation: 1M neurons for FULL tier
    EXPECT_EQ(config.max_neurons, 1000000)
        << "FULL tier neuron limit changed from documented value";
}

TEST_F(PlatformTierRegressionTest, MediumTierNeuronLimitExact) {
    platform_tier_config_t config = platform_tier_get_config(PLATFORM_TIER_MEDIUM);

    // Per header documentation: 100K neurons for MEDIUM tier
    EXPECT_EQ(config.max_neurons, 100000)
        << "MEDIUM tier neuron limit changed from documented value";
}

TEST_F(PlatformTierRegressionTest, ConstrainedTierNeuronLimitExact) {
    platform_tier_config_t config = platform_tier_get_config(PLATFORM_TIER_CONSTRAINED);

    // Per header documentation: 10K neurons for CONSTRAINED tier
    EXPECT_EQ(config.max_neurons, 10000)
        << "CONSTRAINED tier neuron limit changed from documented value";
}

TEST_F(PlatformTierRegressionTest, MinimalTierNeuronLimitExact) {
    platform_tier_config_t config = platform_tier_get_config(PLATFORM_TIER_MINIMAL);

    // Per header documentation: 1K neurons for MINIMAL tier (Portia-scale)
    EXPECT_EQ(config.max_neurons, 1000)
        << "MINIMAL tier neuron limit changed from documented value";
}

TEST_F(PlatformTierRegressionTest, MemoryBudgetsMatchDocumentation) {
    EXPECT_EQ(platform_tier_get_config(PLATFORM_TIER_FULL).memory_budget_mb, 4096);
    EXPECT_EQ(platform_tier_get_config(PLATFORM_TIER_MEDIUM).memory_budget_mb, 1024);
    EXPECT_EQ(platform_tier_get_config(PLATFORM_TIER_CONSTRAINED).memory_budget_mb, 128);
    EXPECT_EQ(platform_tier_get_config(PLATFORM_TIER_MINIMAL).memory_budget_mb, 32);
}

TEST_F(PlatformTierRegressionTest, VisualResolutionsMatchDocumentation) {
    EXPECT_EQ(platform_tier_get_config(PLATFORM_TIER_FULL).visual.max_input_width, 640);
    EXPECT_EQ(platform_tier_get_config(PLATFORM_TIER_FULL).visual.max_input_height, 480);

    EXPECT_EQ(platform_tier_get_config(PLATFORM_TIER_MEDIUM).visual.max_input_width, 320);
    EXPECT_EQ(platform_tier_get_config(PLATFORM_TIER_MEDIUM).visual.max_input_height, 240);

    EXPECT_EQ(platform_tier_get_config(PLATFORM_TIER_CONSTRAINED).visual.max_input_width, 160);
    EXPECT_EQ(platform_tier_get_config(PLATFORM_TIER_CONSTRAINED).visual.max_input_height, 120);

    EXPECT_EQ(platform_tier_get_config(PLATFORM_TIER_MINIMAL).visual.max_input_width, 64);
    EXPECT_EQ(platform_tier_get_config(PLATFORM_TIER_MINIMAL).visual.max_input_height, 48);
}

TEST_F(PlatformTierRegressionTest, AudioSampleRatesMatchDocumentation) {
    EXPECT_EQ(platform_tier_get_config(PLATFORM_TIER_FULL).audio.max_sample_rate, 48000);
    EXPECT_EQ(platform_tier_get_config(PLATFORM_TIER_MEDIUM).audio.max_sample_rate, 22050);
    EXPECT_EQ(platform_tier_get_config(PLATFORM_TIER_CONSTRAINED).audio.max_sample_rate, 16000);
    EXPECT_EQ(platform_tier_get_config(PLATFORM_TIER_MINIMAL).audio.max_sample_rate, 8000);
}

//=============================================================================
// FULL Tier Module Regression Tests
//=============================================================================

TEST_F(PlatformTierRegressionTest, FullTierEnablesAllCoreModules) {
    platform_tier_t tier = PLATFORM_TIER_FULL;

    // Core modules must be enabled
    EXPECT_TRUE(platform_tier_can_enable_module(tier, COGNITIVE_MODULE_ATTENTION));
    EXPECT_TRUE(platform_tier_can_enable_module(tier, COGNITIVE_MODULE_WORKING_MEMORY));
    EXPECT_TRUE(platform_tier_can_enable_module(tier, COGNITIVE_MODULE_SALIENCE));
}

TEST_F(PlatformTierRegressionTest, FullTierEnablesAllMemorySystems) {
    platform_tier_t tier = PLATFORM_TIER_FULL;

    EXPECT_TRUE(platform_tier_can_enable_module(tier, COGNITIVE_MODULE_SEMANTIC_MEMORY));
    EXPECT_TRUE(platform_tier_can_enable_module(tier, COGNITIVE_MODULE_EPISODIC_MEMORY));
    EXPECT_TRUE(platform_tier_can_enable_module(tier, COGNITIVE_MODULE_CONSOLIDATION));
}

TEST_F(PlatformTierRegressionTest, FullTierEnablesAllExecutiveFunctions) {
    platform_tier_t tier = PLATFORM_TIER_FULL;

    EXPECT_TRUE(platform_tier_can_enable_module(tier, COGNITIVE_MODULE_EXECUTIVE));
    EXPECT_TRUE(platform_tier_can_enable_module(tier, COGNITIVE_MODULE_REASONING));
    EXPECT_TRUE(platform_tier_can_enable_module(tier, COGNITIVE_MODULE_CURIOSITY));
}

TEST_F(PlatformTierRegressionTest, FullTierEnablesAllMetaCognitive) {
    platform_tier_t tier = PLATFORM_TIER_FULL;

    EXPECT_TRUE(platform_tier_can_enable_module(tier, COGNITIVE_MODULE_META_LEARNING));
    EXPECT_TRUE(platform_tier_can_enable_module(tier, COGNITIVE_MODULE_INTROSPECTION));
    EXPECT_TRUE(platform_tier_can_enable_module(tier, COGNITIVE_MODULE_SELF_AWARENESS));
}

TEST_F(PlatformTierRegressionTest, FullTierEnablesAllSocialCognition) {
    platform_tier_t tier = PLATFORM_TIER_FULL;

    EXPECT_TRUE(platform_tier_can_enable_module(tier, COGNITIVE_MODULE_THEORY_OF_MIND));
    EXPECT_TRUE(platform_tier_can_enable_module(tier, COGNITIVE_MODULE_MIRROR_NEURONS));
    EXPECT_TRUE(platform_tier_can_enable_module(tier, COGNITIVE_MODULE_EMPATHY));
}

TEST_F(PlatformTierRegressionTest, FullTierEnablesAllAdvancedFeatures) {
    platform_tier_t tier = PLATFORM_TIER_FULL;

    EXPECT_TRUE(platform_tier_can_enable_module(tier, COGNITIVE_MODULE_GLOBAL_WORKSPACE));
    EXPECT_TRUE(platform_tier_can_enable_module(tier, COGNITIVE_MODULE_PREDICTIVE));
    EXPECT_TRUE(platform_tier_can_enable_module(tier, COGNITIVE_MODULE_ETHICS));
}

TEST_F(PlatformTierRegressionTest, FullTierEnablesAllPerception) {
    platform_tier_t tier = PLATFORM_TIER_FULL;

    EXPECT_TRUE(platform_tier_can_enable_module(tier, COGNITIVE_MODULE_VISUAL_CORTEX));
    EXPECT_TRUE(platform_tier_can_enable_module(tier, COGNITIVE_MODULE_AUDIO_CORTEX));
}

TEST_F(PlatformTierRegressionTest, FullTierEnablesAllFeatureFlags) {
    platform_tier_config_t config = platform_tier_get_config(PLATFORM_TIER_FULL);

    EXPECT_TRUE(config.enable_gpu);
    EXPECT_TRUE(config.enable_bio_async);
    EXPECT_TRUE(config.enable_plasticity);
    EXPECT_TRUE(config.enable_neuromodulation);
    EXPECT_TRUE(config.enable_checkpointing);
}

//=============================================================================
// MINIMAL Tier Module Regression Tests
//=============================================================================

TEST_F(PlatformTierRegressionTest, MinimalTierDisablesAdvancedCognition) {
    platform_tier_t tier = PLATFORM_TIER_MINIMAL;

    // No executive functions
    EXPECT_FALSE(platform_tier_can_enable_module(tier, COGNITIVE_MODULE_EXECUTIVE));
    EXPECT_FALSE(platform_tier_can_enable_module(tier, COGNITIVE_MODULE_REASONING));
    EXPECT_FALSE(platform_tier_can_enable_module(tier, COGNITIVE_MODULE_CURIOSITY));

    // No meta-cognitive
    EXPECT_FALSE(platform_tier_can_enable_module(tier, COGNITIVE_MODULE_META_LEARNING));
    EXPECT_FALSE(platform_tier_can_enable_module(tier, COGNITIVE_MODULE_INTROSPECTION));
    EXPECT_FALSE(platform_tier_can_enable_module(tier, COGNITIVE_MODULE_SELF_AWARENESS));

    // No social cognition
    EXPECT_FALSE(platform_tier_can_enable_module(tier, COGNITIVE_MODULE_THEORY_OF_MIND));
    EXPECT_FALSE(platform_tier_can_enable_module(tier, COGNITIVE_MODULE_MIRROR_NEURONS));
    EXPECT_FALSE(platform_tier_can_enable_module(tier, COGNITIVE_MODULE_EMPATHY));
}

TEST_F(PlatformTierRegressionTest, MinimalTierDisablesAdvancedMemory) {
    platform_tier_t tier = PLATFORM_TIER_MINIMAL;

    EXPECT_FALSE(platform_tier_can_enable_module(tier, COGNITIVE_MODULE_WORKING_MEMORY));
    EXPECT_FALSE(platform_tier_can_enable_module(tier, COGNITIVE_MODULE_SEMANTIC_MEMORY));
    EXPECT_FALSE(platform_tier_can_enable_module(tier, COGNITIVE_MODULE_EPISODIC_MEMORY));
    EXPECT_FALSE(platform_tier_can_enable_module(tier, COGNITIVE_MODULE_CONSOLIDATION));
}

TEST_F(PlatformTierRegressionTest, MinimalTierDisablesAudioCortex) {
    platform_tier_t tier = PLATFORM_TIER_MINIMAL;

    // Audio too expensive for minimal tier
    EXPECT_FALSE(platform_tier_can_enable_module(tier, COGNITIVE_MODULE_AUDIO_CORTEX));
}

TEST_F(PlatformTierRegressionTest, MinimalTierEnablesOnlyBasicAttentionAndVision) {
    platform_tier_t tier = PLATFORM_TIER_MINIMAL;

    // Only these two modules should be enabled
    EXPECT_TRUE(platform_tier_can_enable_module(tier, COGNITIVE_MODULE_ATTENTION));
    EXPECT_TRUE(platform_tier_can_enable_module(tier, COGNITIVE_MODULE_VISUAL_CORTEX));
}

TEST_F(PlatformTierRegressionTest, MinimalTierDisablesAllFeatureFlags) {
    platform_tier_config_t config = platform_tier_get_config(PLATFORM_TIER_MINIMAL);

    EXPECT_FALSE(config.enable_gpu);
    EXPECT_FALSE(config.enable_bio_async);
    EXPECT_FALSE(config.enable_plasticity);
    EXPECT_FALSE(config.enable_neuromodulation);
    EXPECT_FALSE(config.enable_checkpointing);
}

//=============================================================================
// Memory Budget Regression Tests
//=============================================================================

TEST_F(PlatformTierRegressionTest, MemoryBudgetNeverExceedsDocumented) {
    EXPECT_LE(platform_tier_get_config(PLATFORM_TIER_FULL).memory_budget_mb, 4096);
    EXPECT_LE(platform_tier_get_config(PLATFORM_TIER_MEDIUM).memory_budget_mb, 1024);
    EXPECT_LE(platform_tier_get_config(PLATFORM_TIER_CONSTRAINED).memory_budget_mb, 128);
    EXPECT_LE(platform_tier_get_config(PLATFORM_TIER_MINIMAL).memory_budget_mb, 32);
}

TEST_F(PlatformTierRegressionTest, MemoryBudgetStrictlyMonotonic) {
    uint32_t full = platform_tier_get_config(PLATFORM_TIER_FULL).memory_budget_mb;
    uint32_t medium = platform_tier_get_config(PLATFORM_TIER_MEDIUM).memory_budget_mb;
    uint32_t constrained = platform_tier_get_config(PLATFORM_TIER_CONSTRAINED).memory_budget_mb;
    uint32_t minimal = platform_tier_get_config(PLATFORM_TIER_MINIMAL).memory_budget_mb;

    EXPECT_GT(full, medium);
    EXPECT_GT(medium, constrained);
    EXPECT_GT(constrained, minimal);
}

TEST_F(PlatformTierRegressionTest, NeuronCountNeverExceedsDocumented) {
    EXPECT_LE(platform_tier_get_config(PLATFORM_TIER_FULL).max_neurons, 1000000);
    EXPECT_LE(platform_tier_get_config(PLATFORM_TIER_MEDIUM).max_neurons, 100000);
    EXPECT_LE(platform_tier_get_config(PLATFORM_TIER_CONSTRAINED).max_neurons, 10000);
    EXPECT_LE(platform_tier_get_config(PLATFORM_TIER_MINIMAL).max_neurons, 1000);
}

TEST_F(PlatformTierRegressionTest, NeuronCountStrictlyMonotonic) {
    uint32_t full = platform_tier_get_config(PLATFORM_TIER_FULL).max_neurons;
    uint32_t medium = platform_tier_get_config(PLATFORM_TIER_MEDIUM).max_neurons;
    uint32_t constrained = platform_tier_get_config(PLATFORM_TIER_CONSTRAINED).max_neurons;
    uint32_t minimal = platform_tier_get_config(PLATFORM_TIER_MINIMAL).max_neurons;

    EXPECT_GT(full, medium);
    EXPECT_GT(medium, constrained);
    EXPECT_GT(constrained, minimal);
}

//=============================================================================
// Visual Cortex Regression Tests
//=============================================================================

TEST_F(PlatformTierRegressionTest, VisualResolutionStrictlyMonotonic) {
    uint32_t full_w = platform_tier_get_config(PLATFORM_TIER_FULL).visual.max_input_width;
    uint32_t medium_w = platform_tier_get_config(PLATFORM_TIER_MEDIUM).visual.max_input_width;
    uint32_t constrained_w = platform_tier_get_config(PLATFORM_TIER_CONSTRAINED).visual.max_input_width;
    uint32_t minimal_w = platform_tier_get_config(PLATFORM_TIER_MINIMAL).visual.max_input_width;

    EXPECT_GT(full_w, medium_w);
    EXPECT_GT(medium_w, constrained_w);
    EXPECT_GT(constrained_w, minimal_w);
}

TEST_F(PlatformTierRegressionTest, VisualFilterCountMonotonic) {
    uint32_t full_f = platform_tier_get_config(PLATFORM_TIER_FULL).visual.num_filters_conv1;
    uint32_t medium_f = platform_tier_get_config(PLATFORM_TIER_MEDIUM).visual.num_filters_conv1;
    uint32_t constrained_f = platform_tier_get_config(PLATFORM_TIER_CONSTRAINED).visual.num_filters_conv1;
    uint32_t minimal_f = platform_tier_get_config(PLATFORM_TIER_MINIMAL).visual.num_filters_conv1;

    EXPECT_GT(full_f, medium_f);
    EXPECT_GT(medium_f, constrained_f);
    EXPECT_GT(constrained_f, minimal_f);
}

TEST_F(PlatformTierRegressionTest, VisualAttentionOnlyHigherTiers) {
    EXPECT_TRUE(platform_tier_get_config(PLATFORM_TIER_FULL).visual.enable_attention);
    EXPECT_TRUE(platform_tier_get_config(PLATFORM_TIER_MEDIUM).visual.enable_attention);
    EXPECT_FALSE(platform_tier_get_config(PLATFORM_TIER_CONSTRAINED).visual.enable_attention);
    EXPECT_FALSE(platform_tier_get_config(PLATFORM_TIER_MINIMAL).visual.enable_attention);
}

TEST_F(PlatformTierRegressionTest, VisualPoolingOnlyHigherTiers) {
    EXPECT_TRUE(platform_tier_get_config(PLATFORM_TIER_FULL).visual.enable_pooling);
    EXPECT_TRUE(platform_tier_get_config(PLATFORM_TIER_MEDIUM).visual.enable_pooling);
    EXPECT_TRUE(platform_tier_get_config(PLATFORM_TIER_CONSTRAINED).visual.enable_pooling);
    EXPECT_FALSE(platform_tier_get_config(PLATFORM_TIER_MINIMAL).visual.enable_pooling);
}

//=============================================================================
// Audio Cortex Regression Tests
//=============================================================================

TEST_F(PlatformTierRegressionTest, AudioSampleRateStrictlyMonotonic) {
    uint32_t full_sr = platform_tier_get_config(PLATFORM_TIER_FULL).audio.max_sample_rate;
    uint32_t medium_sr = platform_tier_get_config(PLATFORM_TIER_MEDIUM).audio.max_sample_rate;
    uint32_t constrained_sr = platform_tier_get_config(PLATFORM_TIER_CONSTRAINED).audio.max_sample_rate;
    uint32_t minimal_sr = platform_tier_get_config(PLATFORM_TIER_MINIMAL).audio.max_sample_rate;

    EXPECT_GT(full_sr, medium_sr);
    EXPECT_GT(medium_sr, constrained_sr);
    EXPECT_GT(constrained_sr, minimal_sr);
}

TEST_F(PlatformTierRegressionTest, AudioMelFiltersMonotonic) {
    uint32_t full_mf = platform_tier_get_config(PLATFORM_TIER_FULL).audio.num_mel_filters;
    uint32_t medium_mf = platform_tier_get_config(PLATFORM_TIER_MEDIUM).audio.num_mel_filters;
    uint32_t constrained_mf = platform_tier_get_config(PLATFORM_TIER_CONSTRAINED).audio.num_mel_filters;
    uint32_t minimal_mf = platform_tier_get_config(PLATFORM_TIER_MINIMAL).audio.num_mel_filters;

    EXPECT_GT(full_mf, medium_mf);
    EXPECT_GT(medium_mf, constrained_mf);
    EXPECT_GT(constrained_mf, minimal_mf);
}

TEST_F(PlatformTierRegressionTest, AudioAttentionOnlyHigherTiers) {
    EXPECT_TRUE(platform_tier_get_config(PLATFORM_TIER_FULL).audio.enable_attention);
    EXPECT_TRUE(platform_tier_get_config(PLATFORM_TIER_MEDIUM).audio.enable_attention);
    EXPECT_FALSE(platform_tier_get_config(PLATFORM_TIER_CONSTRAINED).audio.enable_attention);
    EXPECT_FALSE(platform_tier_get_config(PLATFORM_TIER_MINIMAL).audio.enable_attention);
}

TEST_F(PlatformTierRegressionTest, AudioMemoryOnlyHigherTiers) {
    EXPECT_TRUE(platform_tier_get_config(PLATFORM_TIER_FULL).audio.enable_memory);
    EXPECT_TRUE(platform_tier_get_config(PLATFORM_TIER_MEDIUM).audio.enable_memory);
    EXPECT_FALSE(platform_tier_get_config(PLATFORM_TIER_CONSTRAINED).audio.enable_memory);
    EXPECT_FALSE(platform_tier_get_config(PLATFORM_TIER_MINIMAL).audio.enable_memory);
}

//=============================================================================
// Performance Budget Regression Tests
//=============================================================================

TEST_F(PlatformTierRegressionTest, ComputeBudgetStrictlyMonotonic) {
    uint64_t full = platform_tier_get_config(PLATFORM_TIER_FULL).compute_budget_ops;
    uint64_t medium = platform_tier_get_config(PLATFORM_TIER_MEDIUM).compute_budget_ops;
    uint64_t constrained = platform_tier_get_config(PLATFORM_TIER_CONSTRAINED).compute_budget_ops;
    uint64_t minimal = platform_tier_get_config(PLATFORM_TIER_MINIMAL).compute_budget_ops;

    EXPECT_GT(full, medium);
    EXPECT_GT(medium, constrained);
    EXPECT_GT(constrained, minimal);
}

TEST_F(PlatformTierRegressionTest, ThreadCountStrictlyMonotonic) {
    uint32_t full = platform_tier_get_config(PLATFORM_TIER_FULL).max_threads;
    uint32_t medium = platform_tier_get_config(PLATFORM_TIER_MEDIUM).max_threads;
    uint32_t constrained = platform_tier_get_config(PLATFORM_TIER_CONSTRAINED).max_threads;
    uint32_t minimal = platform_tier_get_config(PLATFORM_TIER_MINIMAL).max_threads;

    EXPECT_GT(full, medium);
    EXPECT_GT(medium, constrained);
    EXPECT_GT(constrained, minimal);
}

TEST_F(PlatformTierRegressionTest, SamplingRateStrictlyMonotonic) {
    float full = platform_tier_get_config(PLATFORM_TIER_FULL).sampling_rate;
    float medium = platform_tier_get_config(PLATFORM_TIER_MEDIUM).sampling_rate;
    float constrained = platform_tier_get_config(PLATFORM_TIER_CONSTRAINED).sampling_rate;
    float minimal = platform_tier_get_config(PLATFORM_TIER_MINIMAL).sampling_rate;

    EXPECT_GT(full, medium);
    EXPECT_GT(medium, constrained);
    EXPECT_GT(constrained, minimal);
}

TEST_F(PlatformTierRegressionTest, BatchSizeStrictlyMonotonic) {
    uint32_t full = platform_tier_get_config(PLATFORM_TIER_FULL).update_batch_size;
    uint32_t medium = platform_tier_get_config(PLATFORM_TIER_MEDIUM).update_batch_size;
    uint32_t constrained = platform_tier_get_config(PLATFORM_TIER_CONSTRAINED).update_batch_size;
    uint32_t minimal = platform_tier_get_config(PLATFORM_TIER_MINIMAL).update_batch_size;

    EXPECT_GT(full, medium);
    EXPECT_GT(medium, constrained);
    EXPECT_GT(constrained, minimal);
}

//=============================================================================
// Validation Regression Tests
//=============================================================================

TEST_F(PlatformTierRegressionTest, ValidationAcceptsAllDefaultConfigs) {
    for (int t = PLATFORM_TIER_FULL; t <= PLATFORM_TIER_MINIMAL; t++) {
        platform_tier_config_t config = platform_tier_get_config((platform_tier_t)t);
        char error[256];

        bool valid = platform_tier_validate_config((platform_tier_t)t, &config, error, sizeof(error));

        EXPECT_TRUE(valid) << "Default config invalid for tier " << t << ": " << error;
    }
}

TEST_F(PlatformTierRegressionTest, ValidationRejectsExcessiveNeurons) {
    for (int t = PLATFORM_TIER_FULL; t <= PLATFORM_TIER_MINIMAL; t++) {
        platform_tier_config_t config = platform_tier_get_config((platform_tier_t)t);
        config.max_neurons = config.max_neurons * 10;  // Exceed by 10x

        char error[256];
        bool valid = platform_tier_validate_config((platform_tier_t)t, &config, error, sizeof(error));

        EXPECT_FALSE(valid) << "Validation accepted excessive neurons for tier " << t;
    }
}

TEST_F(PlatformTierRegressionTest, ValidationRejectsUnsupportedModules) {
    for (int t = PLATFORM_TIER_MEDIUM; t <= PLATFORM_TIER_MINIMAL; t++) {
        platform_tier_config_t config = platform_tier_get_config((platform_tier_t)t);
        config.cognitive_modules_enabled = COGNITIVE_MODULE_ALL;  // Enable everything

        char error[256];
        bool valid = platform_tier_validate_config((platform_tier_t)t, &config, error, sizeof(error));

        EXPECT_FALSE(valid) << "Validation accepted unsupported modules for tier " << t;
    }
}

//=============================================================================
// Tier Name Regression Tests
//=============================================================================

TEST_F(PlatformTierRegressionTest, TierNamesNeverChange) {
    EXPECT_STREQ(platform_tier_get_name(PLATFORM_TIER_FULL), "FULL");
    EXPECT_STREQ(platform_tier_get_name(PLATFORM_TIER_MEDIUM), "MEDIUM");
    EXPECT_STREQ(platform_tier_get_name(PLATFORM_TIER_CONSTRAINED), "CONSTRAINED");
    EXPECT_STREQ(platform_tier_get_name(PLATFORM_TIER_MINIMAL), "MINIMAL");
}

TEST_F(PlatformTierRegressionTest, InvalidTierNameReturnsUnknown) {
    EXPECT_STREQ(platform_tier_get_name((platform_tier_t)999), "UNKNOWN");
    EXPECT_STREQ(platform_tier_get_name((platform_tier_t)-1), "UNKNOWN");
}

//=============================================================================
// Consistency Regression Tests
//=============================================================================

TEST_F(PlatformTierRegressionTest, InitialNeuronsAlwaysLessThanMax) {
    for (int t = PLATFORM_TIER_FULL; t <= PLATFORM_TIER_MINIMAL; t++) {
        platform_tier_config_t config = platform_tier_get_config((platform_tier_t)t);

        EXPECT_LE(config.initial_neurons, config.max_neurons)
            << "Initial neurons exceeds max for tier " << t;
        EXPECT_GT(config.initial_neurons, 0)
            << "Zero initial neurons for tier " << t;
    }
}

TEST_F(PlatformTierRegressionTest, SynapsesPerNeuronReasonable) {
    for (int t = PLATFORM_TIER_FULL; t <= PLATFORM_TIER_MINIMAL; t++) {
        platform_tier_config_t config = platform_tier_get_config((platform_tier_t)t);

        EXPECT_GT(config.max_synapses_per_neuron, 0)
            << "Zero synapses per neuron for tier " << t;
        EXPECT_LE(config.max_synapses_per_neuron, 100000)
            << "Excessive synapses per neuron for tier " << t;
    }
}

TEST_F(PlatformTierRegressionTest, AllConfigFieldsNonNegative) {
    for (int t = PLATFORM_TIER_FULL; t <= PLATFORM_TIER_MINIMAL; t++) {
        platform_tier_config_t config = platform_tier_get_config((platform_tier_t)t);

        EXPECT_GE(config.max_neurons, 0);
        EXPECT_GE(config.max_synapses_per_neuron, 0);
        EXPECT_GE(config.initial_neurons, 0);
        EXPECT_GE(config.memory_budget_mb, 0);
        EXPECT_GE(config.compute_budget_ops, 0);
        EXPECT_GE(config.max_threads, 0);
        EXPECT_GE(config.update_batch_size, 0);
        EXPECT_GE(config.spike_buffer_size, 0);
        EXPECT_GE(config.sampling_rate, 0.0f);
        EXPECT_LE(config.sampling_rate, 1.0f);
    }
}
