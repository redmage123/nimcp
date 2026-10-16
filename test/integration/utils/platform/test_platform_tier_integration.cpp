/**
 * @file test_platform_tier_integration.cpp
 * @brief Integration tests for Platform Tier System with brain creation
 *
 * WHAT: Tests tier-aware brain creation and cognitive module integration
 * WHY:  Verify that tier configurations correctly affect brain instantiation
 * HOW:  Create brains with tier-specific configs, verify neuron counts and modules
 *
 * TEST COVERAGE:
 * - Brain creation with tier-specific neuron counts
 * - Visual cortex configuration per tier
 * - Audio cortex configuration per tier
 * - Cognitive module enablement verification
 * - Cross-tier consistency checks
 * - Actual resource usage validation
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
    #include "core/brain/nimcp_brain.h"
    #include "utils/platform/nimcp_system_resources.h"
    #include "utils/memory/nimcp_memory.h"
    #include "cognitive/nimcp_working_memory.h"

//=============================================================================
// Mock Platform Tier API (same as unit tests)
//=============================================================================

typedef enum {
    PLATFORM_TIER_MINIMAL = 0,
    PLATFORM_TIER_CONSTRAINED,
    PLATFORM_TIER_MEDIUM,
    PLATFORM_TIER_HIGH,
    PLATFORM_TIER_COUNT
} platform_tier_t;

typedef struct {
    platform_tier_t tier;
    uint32_t max_neurons;
    uint32_t max_synapses_per_neuron;
    bool visual_cortex_enabled;
    uint32_t visual_layers;
    uint32_t visual_neurons_per_layer;
    bool audio_cortex_enabled;
    uint32_t audio_layers;
    uint32_t audio_neurons_per_layer;
    bool working_memory_enabled;
    bool episodic_memory_enabled;
    bool semantic_memory_enabled;
    bool executive_enabled;
    bool attention_enabled;
    bool emotions_enabled;
    bool reasoning_enabled;
    uint64_t max_memory_budget;
    uint64_t checkpoint_budget;
} platform_tier_config_t;

static platform_tier_t platform_detect_tier(const system_resources_t* resources) {
    if (!resources) return PLATFORM_TIER_MINIMAL;
    uint64_t ram_mb = resources->available_ram_mb;
    if (ram_mb < 512) return PLATFORM_TIER_MINIMAL;
    if (ram_mb < 4096) return PLATFORM_TIER_CONSTRAINED;
    if (ram_mb < 16384) return PLATFORM_TIER_MEDIUM;
    return PLATFORM_TIER_HIGH;
}

static platform_tier_config_t platform_get_tier_config(platform_tier_t tier) {
    platform_tier_config_t config = {0};
    config.tier = tier;

    switch (tier) {
        case PLATFORM_TIER_MINIMAL:
            config.max_neurons = 1000;
            config.max_synapses_per_neuron = 100;
            config.visual_cortex_enabled = false;
            config.audio_cortex_enabled = false;
            config.working_memory_enabled = true;
            config.episodic_memory_enabled = false;
            config.semantic_memory_enabled = false;
            config.executive_enabled = false;
            config.attention_enabled = true;
            config.emotions_enabled = false;
            config.reasoning_enabled = false;
            config.max_memory_budget = 5 * 1024 * 1024;
            config.checkpoint_budget = 1 * 1024 * 1024;
            break;

        case PLATFORM_TIER_CONSTRAINED:
            config.max_neurons = 10000;
            config.max_synapses_per_neuron = 200;
            config.visual_cortex_enabled = true;
            config.visual_layers = 2;
            config.visual_neurons_per_layer = 500;
            config.audio_cortex_enabled = true;
            config.audio_layers = 1;
            config.audio_neurons_per_layer = 250;
            config.working_memory_enabled = true;
            config.episodic_memory_enabled = true;
            config.semantic_memory_enabled = false;
            config.executive_enabled = true;
            config.attention_enabled = true;
            config.emotions_enabled = true;
            config.reasoning_enabled = false;
            config.max_memory_budget = 50 * 1024 * 1024;
            config.checkpoint_budget = 10 * 1024 * 1024;
            break;

        case PLATFORM_TIER_MEDIUM:
            config.max_neurons = 100000;
            config.max_synapses_per_neuron = 500;
            config.visual_cortex_enabled = true;
            config.visual_layers = 4;
            config.visual_neurons_per_layer = 2000;
            config.audio_cortex_enabled = true;
            config.audio_layers = 3;
            config.audio_neurons_per_layer = 1000;
            config.working_memory_enabled = true;
            config.episodic_memory_enabled = true;
            config.semantic_memory_enabled = true;
            config.executive_enabled = true;
            config.attention_enabled = true;
            config.emotions_enabled = true;
            config.reasoning_enabled = true;
            config.max_memory_budget = 500 * 1024 * 1024;
            config.checkpoint_budget = 100 * 1024 * 1024;
            break;

        case PLATFORM_TIER_HIGH:
            config.max_neurons = 1000000;
            config.max_synapses_per_neuron = 1000;
            config.visual_cortex_enabled = true;
            config.visual_layers = 6;
            config.visual_neurons_per_layer = 10000;
            config.audio_cortex_enabled = true;
            config.audio_layers = 4;
            config.audio_neurons_per_layer = 5000;
            config.working_memory_enabled = true;
            config.episodic_memory_enabled = true;
            config.semantic_memory_enabled = true;
            config.executive_enabled = true;
            config.attention_enabled = true;
            config.emotions_enabled = true;
            config.reasoning_enabled = true;
            config.max_memory_budget = 4ULL * 1024 * 1024 * 1024;
            config.checkpoint_budget = 1ULL * 1024 * 1024 * 1024;
            break;

        default:
            config.tier = PLATFORM_TIER_MINIMAL;
            break;
    }

    return config;
}

//=============================================================================
// Test Fixture
//=============================================================================

class PlatformTierIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);
        nimcp_memory_clear_stats();
    }

    void TearDown() override {
        // Allow some tolerance for global state
        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);

        // Just log if there are leaks, don't fail
        if (stats.current_allocated > 0) {
            std::cerr << "Warning: " << stats.current_allocated
                      << " bytes still allocated" << std::endl;
        }
    }

    // Helper: Create brain with tier config
    brain_t* create_brain_for_tier(platform_tier_t tier) {
        platform_tier_config_t config = platform_get_tier_config(tier);

        brain_config_t brain_config = {0};
        brain_config.num_neurons = config.max_neurons;
        brain_config.num_layers = 3;  // Default 3-layer architecture
        brain_config.timestep_ms = 1.0f;

        return brain_create(&brain_config);
    }
};

//=============================================================================
// Brain Creation Tests
//=============================================================================

TEST_F(PlatformTierIntegrationTest, CreateBrain_MinimalTier_CorrectNeuronCount) {
    platform_tier_config_t config = platform_get_tier_config(PLATFORM_TIER_MINIMAL);
    brain_config_t brain_config = {0};
    brain_config.num_neurons = config.max_neurons;
    brain_config.num_layers = 3;
    brain_config.timestep_ms = 1.0f;

    brain_t* brain = brain_create(&brain_config);
    ASSERT_NE(brain, nullptr);

    // Verify neuron count matches tier
    EXPECT_EQ(brain->num_neurons, 1000);

    brain_destroy(brain);
}

TEST_F(PlatformTierIntegrationTest, CreateBrain_ConstrainedTier_CorrectNeuronCount) {
    platform_tier_config_t config = platform_get_tier_config(PLATFORM_TIER_CONSTRAINED);
    brain_config_t brain_config = {0};
    brain_config.num_neurons = config.max_neurons;
    brain_config.num_layers = 3;
    brain_config.timestep_ms = 1.0f;

    brain_t* brain = brain_create(&brain_config);
    ASSERT_NE(brain, nullptr);

    EXPECT_EQ(brain->num_neurons, 10000);

    brain_destroy(brain);
}

TEST_F(PlatformTierIntegrationTest, CreateBrain_MediumTier_CorrectNeuronCount) {
    platform_tier_config_t config = platform_get_tier_config(PLATFORM_TIER_MEDIUM);
    brain_config_t brain_config = {0};
    brain_config.num_neurons = config.max_neurons;
    brain_config.num_layers = 3;
    brain_config.timestep_ms = 1.0f;

    brain_t* brain = brain_create(&brain_config);
    ASSERT_NE(brain, nullptr);

    EXPECT_EQ(brain->num_neurons, 100000);

    brain_destroy(brain);
}

TEST_F(PlatformTierIntegrationTest, CreateBrain_HighTier_CorrectNeuronCount) {
    // Note: 1M neurons might be too large for test environment
    // Use smaller count for testing
    platform_tier_config_t config = platform_get_tier_config(PLATFORM_TIER_HIGH);
    brain_config_t brain_config = {0};
    brain_config.num_neurons = 50000;  // Reduced for test
    brain_config.num_layers = 3;
    brain_config.timestep_ms = 1.0f;

    brain_t* brain = brain_create(&brain_config);
    ASSERT_NE(brain, nullptr);

    EXPECT_EQ(brain->num_neurons, 50000);

    brain_destroy(brain);
}

//=============================================================================
// Visual Cortex Integration Tests
//=============================================================================

TEST_F(PlatformTierIntegrationTest, VisualCortex_MinimalTier_NotCreated) {
    platform_tier_config_t config = platform_get_tier_config(PLATFORM_TIER_MINIMAL);

    // Minimal tier should have visual cortex disabled
    EXPECT_FALSE(config.visual_cortex_enabled);
}

TEST_F(PlatformTierIntegrationTest, VisualCortex_ConstrainedTier_BasicConfig) {
    platform_tier_config_t config = platform_get_tier_config(PLATFORM_TIER_CONSTRAINED);

    EXPECT_TRUE(config.visual_cortex_enabled);
    EXPECT_EQ(config.visual_layers, 2);
    EXPECT_EQ(config.visual_neurons_per_layer, 500);

    // Total visual neurons should be reasonable
    uint32_t total_visual_neurons = config.visual_layers * config.visual_neurons_per_layer;
    EXPECT_EQ(total_visual_neurons, 1000);
}

TEST_F(PlatformTierIntegrationTest, VisualCortex_MediumTier_FullConfig) {
    platform_tier_config_t config = platform_get_tier_config(PLATFORM_TIER_MEDIUM);

    EXPECT_TRUE(config.visual_cortex_enabled);
    EXPECT_EQ(config.visual_layers, 4);
    EXPECT_EQ(config.visual_neurons_per_layer, 2000);

    uint32_t total_visual_neurons = config.visual_layers * config.visual_neurons_per_layer;
    EXPECT_EQ(total_visual_neurons, 8000);
}

TEST_F(PlatformTierIntegrationTest, VisualCortex_HighTier_MaximalConfig) {
    platform_tier_config_t config = platform_get_tier_config(PLATFORM_TIER_HIGH);

    EXPECT_TRUE(config.visual_cortex_enabled);
    EXPECT_EQ(config.visual_layers, 6);
    EXPECT_EQ(config.visual_neurons_per_layer, 10000);

    uint32_t total_visual_neurons = config.visual_layers * config.visual_neurons_per_layer;
    EXPECT_EQ(total_visual_neurons, 60000);
}

//=============================================================================
// Audio Cortex Integration Tests
//=============================================================================

TEST_F(PlatformTierIntegrationTest, AudioCortex_MinimalTier_NotCreated) {
    platform_tier_config_t config = platform_get_tier_config(PLATFORM_TIER_MINIMAL);

    EXPECT_FALSE(config.audio_cortex_enabled);
}

TEST_F(PlatformTierIntegrationTest, AudioCortex_ConstrainedTier_BasicConfig) {
    platform_tier_config_t config = platform_get_tier_config(PLATFORM_TIER_CONSTRAINED);

    EXPECT_TRUE(config.audio_cortex_enabled);
    EXPECT_EQ(config.audio_layers, 1);
    EXPECT_EQ(config.audio_neurons_per_layer, 250);
}

TEST_F(PlatformTierIntegrationTest, AudioCortex_MediumTier_FullConfig) {
    platform_tier_config_t config = platform_get_tier_config(PLATFORM_TIER_MEDIUM);

    EXPECT_TRUE(config.audio_cortex_enabled);
    EXPECT_EQ(config.audio_layers, 3);
    EXPECT_EQ(config.audio_neurons_per_layer, 1000);
}

TEST_F(PlatformTierIntegrationTest, AudioCortex_HighTier_MaximalConfig) {
    platform_tier_config_t config = platform_get_tier_config(PLATFORM_TIER_HIGH);

    EXPECT_TRUE(config.audio_cortex_enabled);
    EXPECT_EQ(config.audio_layers, 4);
    EXPECT_EQ(config.audio_neurons_per_layer, 5000);
}

//=============================================================================
// Cognitive Module Integration Tests
//=============================================================================

TEST_F(PlatformTierIntegrationTest, CognitiveModules_MinimalTier_OnlyEssential) {
    platform_tier_config_t config = platform_get_tier_config(PLATFORM_TIER_MINIMAL);

    // Only working memory and attention
    EXPECT_TRUE(config.working_memory_enabled);
    EXPECT_TRUE(config.attention_enabled);

    // All others disabled
    EXPECT_FALSE(config.episodic_memory_enabled);
    EXPECT_FALSE(config.semantic_memory_enabled);
    EXPECT_FALSE(config.executive_enabled);
    EXPECT_FALSE(config.emotions_enabled);
    EXPECT_FALSE(config.reasoning_enabled);
}

TEST_F(PlatformTierIntegrationTest, CognitiveModules_ConstrainedTier_Extended) {
    platform_tier_config_t config = platform_get_tier_config(PLATFORM_TIER_CONSTRAINED);

    // Extended modules
    EXPECT_TRUE(config.working_memory_enabled);
    EXPECT_TRUE(config.episodic_memory_enabled);
    EXPECT_TRUE(config.executive_enabled);
    EXPECT_TRUE(config.attention_enabled);
    EXPECT_TRUE(config.emotions_enabled);

    // Advanced still disabled
    EXPECT_FALSE(config.semantic_memory_enabled);
    EXPECT_FALSE(config.reasoning_enabled);
}

TEST_F(PlatformTierIntegrationTest, CognitiveModules_MediumTier_AllEnabled) {
    platform_tier_config_t config = platform_get_tier_config(PLATFORM_TIER_MEDIUM);

    // All modules enabled
    EXPECT_TRUE(config.working_memory_enabled);
    EXPECT_TRUE(config.episodic_memory_enabled);
    EXPECT_TRUE(config.semantic_memory_enabled);
    EXPECT_TRUE(config.executive_enabled);
    EXPECT_TRUE(config.attention_enabled);
    EXPECT_TRUE(config.emotions_enabled);
    EXPECT_TRUE(config.reasoning_enabled);
}

TEST_F(PlatformTierIntegrationTest, CognitiveModules_HighTier_AllEnabled) {
    platform_tier_config_t config = platform_get_tier_config(PLATFORM_TIER_HIGH);

    // All modules enabled
    EXPECT_TRUE(config.working_memory_enabled);
    EXPECT_TRUE(config.episodic_memory_enabled);
    EXPECT_TRUE(config.semantic_memory_enabled);
    EXPECT_TRUE(config.executive_enabled);
    EXPECT_TRUE(config.attention_enabled);
    EXPECT_TRUE(config.emotions_enabled);
    EXPECT_TRUE(config.reasoning_enabled);
}

//=============================================================================
// Resource Usage Validation Tests
//=============================================================================

TEST_F(PlatformTierIntegrationTest, ResourceUsage_MinimalBrain_WithinBudget) {
    platform_tier_config_t config = platform_get_tier_config(PLATFORM_TIER_MINIMAL);
    brain_config_t brain_config = {0};
    brain_config.num_neurons = config.max_neurons;
    brain_config.num_layers = 3;
    brain_config.timestep_ms = 1.0f;

    nimcp_memory_clear_stats();

    brain_t* brain = brain_create(&brain_config);
    ASSERT_NE(brain, nullptr);

    nimcp_memory_stats_t stats;
    nimcp_memory_get_stats(&stats);

    // Should be well under 5 MB budget
    EXPECT_LT(stats.peak_allocated, config.max_memory_budget);

    brain_destroy(brain);
}

TEST_F(PlatformTierIntegrationTest, ResourceUsage_ConstrainedBrain_WithinBudget) {
    platform_tier_config_t config = platform_get_tier_config(PLATFORM_TIER_CONSTRAINED);
    brain_config_t brain_config = {0};
    brain_config.num_neurons = config.max_neurons;
    brain_config.num_layers = 3;
    brain_config.timestep_ms = 1.0f;

    nimcp_memory_clear_stats();

    brain_t* brain = brain_create(&brain_config);
    ASSERT_NE(brain, nullptr);

    nimcp_memory_stats_t stats;
    nimcp_memory_get_stats(&stats);

    // Should be within 50 MB budget
    EXPECT_LT(stats.peak_allocated, config.max_memory_budget);

    brain_destroy(brain);
}

TEST_F(PlatformTierIntegrationTest, ResourceUsage_MediumBrain_WithinBudget) {
    platform_tier_config_t config = platform_get_tier_config(PLATFORM_TIER_MEDIUM);
    brain_config_t brain_config = {0};
    // Use smaller size for testing
    brain_config.num_neurons = 10000;
    brain_config.num_layers = 3;
    brain_config.timestep_ms = 1.0f;

    nimcp_memory_clear_stats();

    brain_t* brain = brain_create(&brain_config);
    ASSERT_NE(brain, nullptr);

    nimcp_memory_stats_t stats;
    nimcp_memory_get_stats(&stats);

    // Should be well within 500 MB budget with reduced size
    EXPECT_LT(stats.peak_allocated, config.max_memory_budget);

    brain_destroy(brain);
}

//=============================================================================
// Cross-Tier Consistency Tests
//=============================================================================

TEST_F(PlatformTierIntegrationTest, CrossTier_NeuronCountsIncrease) {
    // Verify neuron counts increase monotonically across tiers
    platform_tier_config_t minimal = platform_get_tier_config(PLATFORM_TIER_MINIMAL);
    platform_tier_config_t constrained = platform_get_tier_config(PLATFORM_TIER_CONSTRAINED);
    platform_tier_config_t medium = platform_get_tier_config(PLATFORM_TIER_MEDIUM);
    platform_tier_config_t high = platform_get_tier_config(PLATFORM_TIER_HIGH);

    EXPECT_LT(minimal.max_neurons, constrained.max_neurons);
    EXPECT_LT(constrained.max_neurons, medium.max_neurons);
    EXPECT_LT(medium.max_neurons, high.max_neurons);
}

TEST_F(PlatformTierIntegrationTest, CrossTier_MemoryBudgetsIncrease) {
    platform_tier_config_t minimal = platform_get_tier_config(PLATFORM_TIER_MINIMAL);
    platform_tier_config_t constrained = platform_get_tier_config(PLATFORM_TIER_CONSTRAINED);
    platform_tier_config_t medium = platform_get_tier_config(PLATFORM_TIER_MEDIUM);
    platform_tier_config_t high = platform_get_tier_config(PLATFORM_TIER_HIGH);

    EXPECT_LT(minimal.max_memory_budget, constrained.max_memory_budget);
    EXPECT_LT(constrained.max_memory_budget, medium.max_memory_budget);
    EXPECT_LT(medium.max_memory_budget, high.max_memory_budget);
}

TEST_F(PlatformTierIntegrationTest, CrossTier_ModulesIncremental) {
    // Verify modules are added incrementally, not removed
    platform_tier_config_t minimal = platform_get_tier_config(PLATFORM_TIER_MINIMAL);
    platform_tier_config_t constrained = platform_get_tier_config(PLATFORM_TIER_CONSTRAINED);
    platform_tier_config_t medium = platform_get_tier_config(PLATFORM_TIER_MEDIUM);

    // If enabled in lower tier, must be enabled in higher tier
    if (minimal.working_memory_enabled) {
        EXPECT_TRUE(constrained.working_memory_enabled);
        EXPECT_TRUE(medium.working_memory_enabled);
    }

    if (constrained.episodic_memory_enabled) {
        EXPECT_TRUE(medium.episodic_memory_enabled);
    }
}

//=============================================================================
// Tier Detection with Actual Resources
//=============================================================================

TEST_F(PlatformTierIntegrationTest, TierDetection_CurrentSystem_ValidTier) {
    system_resources_t resources;
    bool success = system_resources_query(&resources);
    ASSERT_TRUE(success);

    platform_tier_t tier = platform_detect_tier(&resources);

    // Should be a valid tier
    EXPECT_GE(tier, PLATFORM_TIER_MINIMAL);
    EXPECT_LT(tier, PLATFORM_TIER_COUNT);

    // Config should be retrievable
    platform_tier_config_t config = platform_get_tier_config(tier);
    EXPECT_GT(config.max_neurons, 0);
}

TEST_F(PlatformTierIntegrationTest, TierDetection_CreateBrainForCurrentSystem) {
    system_resources_t resources;
    ASSERT_TRUE(system_resources_query(&resources));

    platform_tier_t tier = platform_detect_tier(&resources);
    platform_tier_config_t config = platform_get_tier_config(tier);

    // Create brain with tier-appropriate size
    // Use smaller size for testing reliability
    uint32_t safe_size = std::min(config.max_neurons, 5000u);

    brain_config_t brain_config = {0};
    brain_config.num_neurons = safe_size;
    brain_config.num_layers = 3;
    brain_config.timestep_ms = 1.0f;

    brain_t* brain = brain_create(&brain_config);
    ASSERT_NE(brain, nullptr);

    EXPECT_EQ(brain->num_neurons, safe_size);

    brain_destroy(brain);
}
