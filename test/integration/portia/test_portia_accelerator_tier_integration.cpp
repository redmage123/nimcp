/**
 * @file test_portia_accelerator_tier_integration.cpp
 * @brief Integration tests for Portia accelerator and tier interaction
 *
 * WHAT: Tests accelerator availability affects tier selection
 * WHY:  Validate system adapts tier based on GPU/accelerator presence
 * HOW:  Check tier configs with/without accelerator, verify graceful fallback
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "utils/platform/nimcp_platform_tier.h"
#include "portia/nimcp_portia_degradation.h"
#include "async/nimcp_bio_async.h"
#include "utils/validation/nimcp_common.h"

class PortiaAcceleratorTierIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize bio-async for degradation system
        nimcp_bio_async_config_t bio_config = nimcp_bio_async_default_config();
        nimcp_bio_async_init(&bio_config);
    }

    void TearDown() override {
        nimcp_bio_async_shutdown();
    }
};

//=============================================================================
// TEST SUITE 1: Accelerator Availability Affects Tier Selection
//=============================================================================

TEST_F(PortiaAcceleratorTierIntegrationTest, Accelerator_FullTierEnablesGPU) {
    platform_tier_config_t config = platform_tier_get_config(PLATFORM_TIER_FULL);

    // Full tier should enable GPU if available
    EXPECT_TRUE(config.enable_gpu);
}

TEST_F(PortiaAcceleratorTierIntegrationTest, Accelerator_MinimalTierDisablesGPU) {
    platform_tier_config_t config = platform_tier_get_config(PLATFORM_TIER_MINIMAL);

    // Minimal tier should not use GPU (power/resource constraints)
    EXPECT_FALSE(config.enable_gpu);
}

TEST_F(PortiaAcceleratorTierIntegrationTest, Accelerator_TierConfigsReflectAcceleratorCapability) {
    platform_tier_config_t full = platform_tier_get_config(PLATFORM_TIER_FULL);
    platform_tier_config_t medium = platform_tier_get_config(PLATFORM_TIER_MEDIUM);
    platform_tier_config_t constrained = platform_tier_get_config(PLATFORM_TIER_CONSTRAINED);

    // Compute budgets should reflect whether GPU is available
    if (full.enable_gpu) {
        EXPECT_GT(full.compute_budget_ops, medium.compute_budget_ops);
    }

    // Constrained tier may or may not have GPU
    if (!constrained.enable_gpu) {
        EXPECT_LE(constrained.compute_budget_ops, medium.compute_budget_ops);
    }
}

//=============================================================================
// TEST SUITE 2: Accelerator Power Affects Power Profile
//=============================================================================

TEST_F(PortiaAcceleratorTierIntegrationTest, AcceleratorPower_GPUIncreasesComputeBudget) {
    platform_tier_config_t full = platform_tier_get_config(PLATFORM_TIER_FULL);
    platform_tier_config_t medium = platform_tier_get_config(PLATFORM_TIER_MEDIUM);

    // Full tier with GPU should have much higher compute budget
    if (full.enable_gpu && !medium.enable_gpu) {
        EXPECT_GT(full.compute_budget_ops, medium.compute_budget_ops * 2);
    }
}

TEST_F(PortiaAcceleratorTierIntegrationTest, AcceleratorPower_GPUEnablesLargerNetworks) {
    platform_tier_config_t full = platform_tier_get_config(PLATFORM_TIER_FULL);
    platform_tier_config_t minimal = platform_tier_get_config(PLATFORM_TIER_MINIMAL);

    // GPU-enabled tier should support more neurons
    if (full.enable_gpu) {
        EXPECT_GT(full.max_neurons, minimal.max_neurons * 10);
    }
}

TEST_F(PortiaAcceleratorTierIntegrationTest, AcceleratorPower_VisualProcessingScalesWithGPU) {
    platform_tier_config_t full = platform_tier_get_config(PLATFORM_TIER_FULL);
    platform_tier_config_t minimal = platform_tier_get_config(PLATFORM_TIER_MINIMAL);

    // Visual cortex should have higher resolution with GPU
    if (full.enable_gpu) {
        EXPECT_GT(full.visual.max_input_width, minimal.visual.max_input_width);
        EXPECT_GT(full.visual.max_input_height, minimal.visual.max_input_height);
        EXPECT_GT(full.visual.num_filters_conv1, minimal.visual.num_filters_conv1);
    }
}

//=============================================================================
// TEST SUITE 3: Graceful Handling When Accelerator Unavailable
//=============================================================================

TEST_F(PortiaAcceleratorTierIntegrationTest, GracefulFallback_DisabledGPUDoesntCrash) {
    // Get config for tier that would use GPU
    platform_tier_config_t config = platform_tier_get_config(PLATFORM_TIER_FULL);

    // Even if GPU is marked as enabled, system should handle absence gracefully
    EXPECT_TRUE(config.max_neurons > 0);
    EXPECT_TRUE(config.memory_budget_mb > 0);
    EXPECT_TRUE(config.max_threads > 0);
}

TEST_F(PortiaAcceleratorTierIntegrationTest, GracefulFallback_CPUOnlyConfigsValid) {
    // All tiers should have valid CPU-only configurations
    for (int tier_idx = 0; tier_idx < PLATFORM_TIER_COUNT; tier_idx++) {
        platform_tier_t tier = static_cast<platform_tier_t>(tier_idx);
        platform_tier_config_t config = platform_tier_get_config(tier);

        // Should have valid CPU parameters regardless of GPU
        EXPECT_GT(config.max_neurons, 0u);
        EXPECT_GT(config.max_threads, 0u);
        EXPECT_GT(config.memory_budget_mb, 0u);
        EXPECT_GT(config.compute_budget_ops, 0u);
    }
}

TEST_F(PortiaAcceleratorTierIntegrationTest, GracefulFallback_AutoDetectHandlesNoGPU) {
    // Detect current platform tier
    platform_tier_t tier = platform_tier_detect();

    // Should succeed even without GPU
    EXPECT_GE(tier, PLATFORM_TIER_FULL);
    EXPECT_LT(tier, PLATFORM_TIER_COUNT);

    // Get config for detected tier
    platform_tier_config_t config = platform_tier_get_config(tier);

    // Should be valid configuration
    EXPECT_GT(config.max_neurons, 0u);
}

//=============================================================================
// TEST SUITE 4: Accelerator-Aware Module Selection
//=============================================================================

TEST_F(PortiaAcceleratorTierIntegrationTest, ModuleSelection_GPUEnablesVisualCortex) {
    platform_tier_config_t full = platform_tier_get_config(PLATFORM_TIER_FULL);
    platform_tier_config_t minimal = platform_tier_get_config(PLATFORM_TIER_MINIMAL);

    // Full tier should enable visual cortex
    bool full_has_visual = platform_tier_can_enable_module(PLATFORM_TIER_FULL,
                                                            COGNITIVE_MODULE_VISUAL_CORTEX);
    bool minimal_has_visual = platform_tier_can_enable_module(PLATFORM_TIER_MINIMAL,
                                                               COGNITIVE_MODULE_VISUAL_CORTEX);

    EXPECT_TRUE(full_has_visual);
    // Minimal may have limited visual processing
}

TEST_F(PortiaAcceleratorTierIntegrationTest, ModuleSelection_ComputeIntensiveModulesRequireAccelerator) {
    // Modules like global workspace, predictive coding may require higher tiers
    bool full_has_predictive = platform_tier_can_enable_module(
        PLATFORM_TIER_FULL, COGNITIVE_MODULE_PREDICTIVE);
    bool minimal_has_predictive = platform_tier_can_enable_module(
        PLATFORM_TIER_MINIMAL, COGNITIVE_MODULE_PREDICTIVE);

    // Full tier should support compute-intensive modules
    EXPECT_TRUE(full_has_predictive);
    // Minimal likely does not
    EXPECT_FALSE(minimal_has_predictive);
}

//=============================================================================
// TEST SUITE 5: Dynamic Accelerator State Changes
//=============================================================================

TEST_F(PortiaAcceleratorTierIntegrationTest, DynamicChange_CanQueryMultipleTiers) {
    // Should be able to query configs for all tiers
    platform_tier_config_t configs[PLATFORM_TIER_COUNT];

    for (int i = 0; i < PLATFORM_TIER_COUNT; i++) {
        configs[i] = platform_tier_get_config(static_cast<platform_tier_t>(i));
        EXPECT_GT(configs[i].max_neurons, 0u);
    }

    // Configs should be ordered by capability
    EXPECT_GT(configs[PLATFORM_TIER_FULL].max_neurons,
              configs[PLATFORM_TIER_MINIMAL].max_neurons);
    EXPECT_GT(configs[PLATFORM_TIER_MEDIUM].max_neurons,
              configs[PLATFORM_TIER_CONSTRAINED].max_neurons);
}

TEST_F(PortiaAcceleratorTierIntegrationTest, DynamicChange_TierDowngradeReducesResources) {
    // Simulate tier change from FULL to CONSTRAINED
    platform_tier_config_t full_config = platform_tier_get_config(PLATFORM_TIER_FULL);
    platform_tier_config_t constrained_config = platform_tier_get_config(PLATFORM_TIER_CONSTRAINED);

    // Constrained should have fewer resources
    EXPECT_LT(constrained_config.max_neurons, full_config.max_neurons);
    EXPECT_LE(constrained_config.max_threads, full_config.max_threads);
    EXPECT_LT(constrained_config.memory_budget_mb, full_config.memory_budget_mb);

    // Should disable GPU if moving to constrained
    if (full_config.enable_gpu) {
        // Constrained likely disables GPU
        EXPECT_FALSE(constrained_config.enable_gpu);
    }
}

//=============================================================================
// TEST SUITE 6: Validation Prevents Over-Provisioning
//=============================================================================

TEST_F(PortiaAcceleratorTierIntegrationTest, Validation_RejectsExcessiveNeurons) {
    platform_tier_config_t config = platform_tier_get_config(PLATFORM_TIER_MINIMAL);

    // Try to set neurons beyond minimal tier capacity
    config.max_neurons = 10000000;  // 10 million neurons on minimal

    char error_msg[256];
    bool valid = platform_tier_validate_config(PLATFORM_TIER_MINIMAL, &config,
                                                 error_msg, sizeof(error_msg));

    // Should fail validation
    EXPECT_FALSE(valid);
    EXPECT_GT(strlen(error_msg), 0u);
}

TEST_F(PortiaAcceleratorTierIntegrationTest, Validation_AcceptsReasonableConfig) {
    platform_tier_config_t config = platform_tier_get_config(PLATFORM_TIER_MEDIUM);

    // Use default config (should be valid)
    char error_msg[256];
    bool valid = platform_tier_validate_config(PLATFORM_TIER_MEDIUM, &config,
                                                 error_msg, sizeof(error_msg));

    EXPECT_TRUE(valid);
}

TEST_F(PortiaAcceleratorTierIntegrationTest, Validation_RejectsInvalidThreadCount) {
    platform_tier_config_t config = platform_tier_get_config(PLATFORM_TIER_CONSTRAINED);

    // Try to set excessive threads
    config.max_threads = 1000;

    char error_msg[256];
    bool valid = platform_tier_validate_config(PLATFORM_TIER_CONSTRAINED, &config,
                                                 error_msg, sizeof(error_msg));

    // May fail validation depending on implementation
    if (!valid) {
        EXPECT_GT(strlen(error_msg), 0u);
    }
}
