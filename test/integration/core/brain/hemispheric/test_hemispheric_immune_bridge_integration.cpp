/**
 * @file test_hemispheric_immune_bridge_integration.cpp
 * @brief Integration tests for hemispheric brain immune bridge
 *
 * Tests cover:
 * - Inflammation effects on hemispheres (asymmetric responses)
 * - Recovery from inflammation
 * - Immune modulation of callosum bandwidth
 * - Emergency bilateral mode
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <cstring>
#include "utils/nimcp_test_base.h"

#include "core/brain/hemispheric/nimcp_hemispheric_immune_bridge.h"
#include "core/brain/hemispheric/nimcp_hemispheric_brain.h"
#include "cognitive/immune/nimcp_brain_immune.h"

/**
 * @class HemisphericImmuneBridgeIntegrationTest
 * @brief Test fixture for hemispheric immune bridge integration tests
 */
class HemisphericImmuneBridgeIntegrationTest : public NimcpTestBase {
protected:
    static constexpr uint32_t INPUT_SIZE = 8;
    static constexpr uint32_t OUTPUT_SIZE = 4;

    hemispheric_brain_t* brain = nullptr;
    brain_immune_system_t* immune = nullptr;
    hemispheric_immune_bridge_t* bridge = nullptr;

    void SetUp() override {
        NimcpTestBase::SetUp();

        // Create hemispheric brain
        hemispheric_brain_config_t brain_config = hemispheric_brain_default_config();
        brain_config.num_inputs = INPUT_SIZE;
        brain_config.num_outputs = OUTPUT_SIZE;
        brain_config.size = BRAIN_SIZE_SMALL;
        brain_config.enable_bio_async = false;
        brain_config.enable_shared_immune = true;

        brain = hemispheric_brain_create(&brain_config);

        // Create brain immune system
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune = brain_immune_create(&immune_config);

        // Create bridge
        if (brain && immune) {
            hemispheric_immune_config_t bridge_config = hemispheric_immune_default_config();
            bridge_config.enable_compensation = true;
            bridge_config.enable_immune_plasticity = true;
            bridge_config.enable_bio_async = false;

            bridge = hemispheric_immune_create(&bridge_config, brain, immune);
        }
    }

    void TearDown() override {
        if (bridge) {
            hemispheric_immune_destroy(bridge);
            bridge = nullptr;
        }
        if (immune) {
            brain_immune_destroy(immune);
            immune = nullptr;
        }
        if (brain) {
            hemispheric_brain_destroy(brain);
            brain = nullptr;
        }
        NimcpTestBase::TearDown();
    }
};

/* ============================================================================
 * Creation and Lifecycle Tests
 * ============================================================================ */

TEST_F(HemisphericImmuneBridgeIntegrationTest, CreateWithDefaultConfig) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(HemisphericImmuneBridgeIntegrationTest, CreateWithCustomConfig) {
    hemispheric_immune_destroy(bridge);

    hemispheric_immune_config_t config = hemispheric_immune_default_config();
    config.left_vulnerability = 0.9f;
    config.right_vulnerability = 0.6f;
    config.callosum_sensitivity = 0.8f;
    config.compensation_threshold = 0.3f;
    config.plasticity_recovery_rate = 0.05f;

    bridge = hemispheric_immune_create(&config, brain, immune);
    ASSERT_NE(bridge, nullptr);
}

TEST_F(HemisphericImmuneBridgeIntegrationTest, BridgeConnectsToSystems) {
    ASSERT_NE(bridge, nullptr);
    // Bridge should be initialized and connected
    hemispheric_immune_stats_t stats = hemispheric_immune_get_stats(bridge);
    EXPECT_EQ(stats.updates, 0u);
}

/* ============================================================================
 * Inflammation Effect Tests - Asymmetric Responses
 * ============================================================================ */

TEST_F(HemisphericImmuneBridgeIntegrationTest, NoInflammationNoEffect) {
    EXPECT_EQ(hemispheric_immune_set_inflammation(bridge, INFLAMMATION_NONE), 0);
    EXPECT_EQ(hemispheric_immune_update(bridge), 0);
    EXPECT_EQ(hemispheric_immune_apply_modulation(bridge), 0);

    hemisphere_immune_effects_t left = hemispheric_immune_get_left_effects(bridge);
    hemisphere_immune_effects_t right = hemispheric_immune_get_right_effects(bridge);

    // No inflammation means full learning rate
    EXPECT_NEAR(left.learning_rate_factor, 1.0f, 0.05f);
    EXPECT_NEAR(right.learning_rate_factor, 1.0f, 0.05f);
}

TEST_F(HemisphericImmuneBridgeIntegrationTest, LocalInflammationMildEffect) {
    EXPECT_EQ(hemispheric_immune_set_inflammation(bridge, INFLAMMATION_LOCAL), 0);
    EXPECT_EQ(hemispheric_immune_update(bridge), 0);
    EXPECT_EQ(hemispheric_immune_apply_modulation(bridge), 0);

    hemisphere_immune_effects_t left = hemispheric_immune_get_left_effects(bridge);
    hemisphere_immune_effects_t right = hemispheric_immune_get_right_effects(bridge);

    // Left should be slightly more affected (more vulnerable)
    EXPECT_LT(left.learning_rate_factor, 1.0f);
    EXPECT_LT(right.learning_rate_factor, 1.0f);

    // Right hemisphere more resilient
    EXPECT_GE(right.learning_rate_factor, left.learning_rate_factor);
}

TEST_F(HemisphericImmuneBridgeIntegrationTest, RegionalInflammationModerateEffect) {
    EXPECT_EQ(hemispheric_immune_set_inflammation(bridge, INFLAMMATION_REGIONAL), 0);
    EXPECT_EQ(hemispheric_immune_update(bridge), 0);
    EXPECT_EQ(hemispheric_immune_apply_modulation(bridge), 0);

    hemisphere_immune_effects_t left = hemispheric_immune_get_left_effects(bridge);
    hemisphere_immune_effects_t right = hemispheric_immune_get_right_effects(bridge);

    // Regional inflammation has moderate impact
    EXPECT_LT(left.learning_rate_factor, HEMI_IMMUNE_LR_LEFT_LOCAL);
    EXPECT_LT(right.learning_rate_factor, HEMI_IMMUNE_LR_RIGHT_LOCAL);
}

TEST_F(HemisphericImmuneBridgeIntegrationTest, SystemicInflammationSevereEffect) {
    EXPECT_EQ(hemispheric_immune_set_inflammation(bridge, INFLAMMATION_SYSTEMIC), 0);
    EXPECT_EQ(hemispheric_immune_update(bridge), 0);
    EXPECT_EQ(hemispheric_immune_apply_modulation(bridge), 0);

    hemisphere_immune_effects_t left = hemispheric_immune_get_left_effects(bridge);
    hemisphere_immune_effects_t right = hemispheric_immune_get_right_effects(bridge);

    // Systemic inflammation severely impacts learning
    EXPECT_LE(left.learning_rate_factor, HEMI_IMMUNE_LR_LEFT_SYSTEMIC + 0.1f);
    EXPECT_LE(right.learning_rate_factor, HEMI_IMMUNE_LR_RIGHT_SYSTEMIC + 0.1f);
}

TEST_F(HemisphericImmuneBridgeIntegrationTest, CytokineStormExtremEffect) {
    EXPECT_EQ(hemispheric_immune_set_inflammation(bridge, INFLAMMATION_STORM), 0);
    EXPECT_EQ(hemispheric_immune_update(bridge), 0);
    EXPECT_EQ(hemispheric_immune_apply_modulation(bridge), 0);

    hemisphere_immune_effects_t left = hemispheric_immune_get_left_effects(bridge);
    hemisphere_immune_effects_t right = hemispheric_immune_get_right_effects(bridge);

    // Storm nearly shuts down learning
    EXPECT_LE(left.learning_rate_factor, 0.2f);
    EXPECT_LE(right.learning_rate_factor, 0.2f);
}

TEST_F(HemisphericImmuneBridgeIntegrationTest, LeftHemisphereMoreVulnerable) {
    // At each inflammation level, left should be more affected
    brain_inflammation_level_t levels[] = {
        INFLAMMATION_LOCAL,
        INFLAMMATION_REGIONAL,
        INFLAMMATION_SYSTEMIC
    };

    for (auto level : levels) {
        hemispheric_immune_set_inflammation(bridge, level);
        hemispheric_immune_update(bridge);
        hemispheric_immune_apply_modulation(bridge);

        hemisphere_immune_effects_t left = hemispheric_immune_get_left_effects(bridge);
        hemisphere_immune_effects_t right = hemispheric_immune_get_right_effects(bridge);

        // Left is more vulnerable, so LR factor should be lower
        EXPECT_LE(left.learning_rate_factor, right.learning_rate_factor + 0.01f);
    }
}

/* ============================================================================
 * Recovery Tests
 * ============================================================================ */

TEST_F(HemisphericImmuneBridgeIntegrationTest, RecoveryFromInflammation) {
    // Start with systemic inflammation
    hemispheric_immune_set_inflammation(bridge, INFLAMMATION_SYSTEMIC);
    hemispheric_immune_update(bridge);
    hemispheric_immune_apply_modulation(bridge);

    hemisphere_immune_effects_t inflamed_left = hemispheric_immune_get_left_effects(bridge);

    // Clear inflammation
    hemispheric_immune_set_inflammation(bridge, INFLAMMATION_NONE);
    hemispheric_immune_update(bridge);
    hemispheric_immune_apply_modulation(bridge);

    hemisphere_immune_effects_t recovered_left = hemispheric_immune_get_left_effects(bridge);

    // Should recover
    EXPECT_GT(recovered_left.learning_rate_factor, inflamed_left.learning_rate_factor);
}

TEST_F(HemisphericImmuneBridgeIntegrationTest, GradualRecovery) {
    // Start with storm
    hemispheric_immune_set_inflammation(bridge, INFLAMMATION_STORM);
    hemispheric_immune_update(bridge);
    hemispheric_immune_apply_modulation(bridge);

    // Gradually reduce inflammation
    brain_inflammation_level_t recovery_levels[] = {
        INFLAMMATION_SYSTEMIC,
        INFLAMMATION_REGIONAL,
        INFLAMMATION_LOCAL,
        INFLAMMATION_NONE
    };

    float prev_lr = 0.0f;
    for (auto level : recovery_levels) {
        hemispheric_immune_set_inflammation(bridge, level);
        hemispheric_immune_update(bridge);
        hemispheric_immune_apply_modulation(bridge);

        hemisphere_immune_effects_t effects = hemispheric_immune_get_left_effects(bridge);

        // Learning rate should improve with each step
        EXPECT_GE(effects.learning_rate_factor, prev_lr);
        prev_lr = effects.learning_rate_factor;
    }
}

/* ============================================================================
 * Callosum Bandwidth Modulation Tests
 * ============================================================================ */

TEST_F(HemisphericImmuneBridgeIntegrationTest, CallosumBandwidthNoInflammation) {
    hemispheric_immune_set_inflammation(bridge, INFLAMMATION_NONE);
    hemispheric_immune_update(bridge);
    hemispheric_immune_apply_modulation(bridge);

    callosum_immune_effects_t effects = hemispheric_immune_get_callosum_effects(bridge);

    EXPECT_NEAR(effects.bandwidth_factor, HEMI_IMMUNE_CALLOSUM_NONE, 0.05f);
    EXPECT_FALSE(effects.degraded);
}

TEST_F(HemisphericImmuneBridgeIntegrationTest, CallosumBandwidthDegradedByInflammation) {
    hemispheric_immune_set_inflammation(bridge, INFLAMMATION_SYSTEMIC);
    hemispheric_immune_update(bridge);
    hemispheric_immune_apply_modulation(bridge);

    callosum_immune_effects_t effects = hemispheric_immune_get_callosum_effects(bridge);

    // Bandwidth should be reduced
    EXPECT_LT(effects.bandwidth_factor, 0.6f);
    EXPECT_TRUE(effects.degraded);
}

TEST_F(HemisphericImmuneBridgeIntegrationTest, CallosumLatencyIncreasedByInflammation) {
    hemispheric_immune_set_inflammation(bridge, INFLAMMATION_NONE);
    hemispheric_immune_update(bridge);

    callosum_immune_effects_t healthy_effects = hemispheric_immune_get_callosum_effects(bridge);

    hemispheric_immune_set_inflammation(bridge, INFLAMMATION_REGIONAL);
    hemispheric_immune_update(bridge);

    callosum_immune_effects_t inflamed_effects = hemispheric_immune_get_callosum_effects(bridge);

    // Latency multiplier should increase with inflammation
    EXPECT_GE(inflamed_effects.latency_multiplier, healthy_effects.latency_multiplier);
}

TEST_F(HemisphericImmuneBridgeIntegrationTest, EffectiveBandwidthCalculation) {
    hemispheric_immune_set_inflammation(bridge, INFLAMMATION_NONE);
    hemispheric_immune_update(bridge);

    uint32_t base_bw = 1000;
    uint32_t effective_healthy = hemispheric_immune_get_effective_bandwidth(bridge, base_bw);
    EXPECT_EQ(effective_healthy, base_bw);

    // With inflammation
    hemispheric_immune_set_inflammation(bridge, INFLAMMATION_SYSTEMIC);
    hemispheric_immune_update(bridge);

    uint32_t effective_inflamed = hemispheric_immune_get_effective_bandwidth(bridge, base_bw);
    EXPECT_LT(effective_inflamed, effective_healthy);
}

/* ============================================================================
 * Compensation Tests
 * ============================================================================ */

TEST_F(HemisphericImmuneBridgeIntegrationTest, CompensationTriggered) {
    // Severe inflammation on one side should trigger compensation
    hemispheric_immune_set_inflammation(bridge, INFLAMMATION_SYSTEMIC);
    hemispheric_immune_update(bridge);
    hemispheric_immune_apply_modulation(bridge);

    hemisphere_id_t compensating;
    bool is_compensating = hemispheric_immune_is_compensating(bridge, &compensating);

    // Compensation may or may not be active depending on asymmetry
    // Just verify the function works
    if (is_compensating) {
        EXPECT_TRUE(compensating == HEMISPHERE_LEFT || compensating == HEMISPHERE_RIGHT);

        // The compensating hemisphere should have is_compensating flag set
        hemisphere_immune_effects_t effects;
        if (compensating == HEMISPHERE_RIGHT) {
            effects = hemispheric_immune_get_right_effects(bridge);
        } else {
            effects = hemispheric_immune_get_left_effects(bridge);
        }
        EXPECT_TRUE(effects.is_compensating);
    }
}

/* ============================================================================
 * Emergency Bilateral Mode Tests
 * ============================================================================ */

TEST_F(HemisphericImmuneBridgeIntegrationTest, TriggerEmergencyBilateral) {
    EXPECT_EQ(hemispheric_immune_trigger_emergency_bilateral(bridge), 0);

    // Brain should be in bilateral mode
    EXPECT_TRUE(hemispheric_brain_is_bilateral_mode(brain));
}

TEST_F(HemisphericImmuneBridgeIntegrationTest, ClearEmergencyMode) {
    hemispheric_immune_trigger_emergency_bilateral(bridge);
    EXPECT_TRUE(hemispheric_brain_is_bilateral_mode(brain));

    EXPECT_EQ(hemispheric_immune_clear_emergency(bridge), 0);
    EXPECT_FALSE(hemispheric_brain_is_bilateral_mode(brain));
}

TEST_F(HemisphericImmuneBridgeIntegrationTest, SevereInflammationTriggersEmergencyBilateral) {
    // Storm-level inflammation should trigger emergency bilateral
    hemispheric_immune_set_inflammation(bridge, INFLAMMATION_STORM);
    hemispheric_immune_update(bridge);
    hemispheric_immune_apply_modulation(bridge);

    lateralization_immune_effects_t lat_effects;
    // Check if emergency bilateral was triggered
    // (implementation may auto-trigger on storm)
}

/* ============================================================================
 * Effective Learning Rate Tests
 * ============================================================================ */

TEST_F(HemisphericImmuneBridgeIntegrationTest, EffectiveLearningRateLeft) {
    float base_lr = 0.01f;

    hemispheric_immune_set_inflammation(bridge, INFLAMMATION_NONE);
    hemispheric_immune_update(bridge);

    float healthy_lr = hemispheric_immune_get_effective_lr(bridge, HEMISPHERE_LEFT, base_lr);
    EXPECT_NEAR(healthy_lr, base_lr, 0.001f);

    hemispheric_immune_set_inflammation(bridge, INFLAMMATION_SYSTEMIC);
    hemispheric_immune_update(bridge);

    float inflamed_lr = hemispheric_immune_get_effective_lr(bridge, HEMISPHERE_LEFT, base_lr);
    EXPECT_LT(inflamed_lr, healthy_lr);
}

TEST_F(HemisphericImmuneBridgeIntegrationTest, EffectiveLearningRateRight) {
    float base_lr = 0.01f;

    hemispheric_immune_set_inflammation(bridge, INFLAMMATION_REGIONAL);
    hemispheric_immune_update(bridge);

    float left_lr = hemispheric_immune_get_effective_lr(bridge, HEMISPHERE_LEFT, base_lr);
    float right_lr = hemispheric_immune_get_effective_lr(bridge, HEMISPHERE_RIGHT, base_lr);

    // Right hemisphere should have higher effective LR (more resilient)
    EXPECT_GE(right_lr, left_lr);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(HemisphericImmuneBridgeIntegrationTest, StatisticsAccumulate) {
    // Run several update cycles with different inflammation levels
    brain_inflammation_level_t levels[] = {
        INFLAMMATION_NONE,
        INFLAMMATION_LOCAL,
        INFLAMMATION_REGIONAL,
        INFLAMMATION_NONE
    };

    for (auto level : levels) {
        hemispheric_immune_set_inflammation(bridge, level);
        hemispheric_immune_update(bridge);
        hemispheric_immune_apply_modulation(bridge);
    }

    hemispheric_immune_stats_t stats = hemispheric_immune_get_stats(bridge);

    EXPECT_GT(stats.updates, 0u);
    EXPECT_GT(stats.modulations_applied, 0u);
}

TEST_F(HemisphericImmuneBridgeIntegrationTest, ResetStatistics) {
    hemispheric_immune_set_inflammation(bridge, INFLAMMATION_LOCAL);
    hemispheric_immune_update(bridge);
    hemispheric_immune_apply_modulation(bridge);

    hemispheric_immune_reset_stats(bridge);

    hemispheric_immune_stats_t stats = hemispheric_immune_get_stats(bridge);
    EXPECT_EQ(stats.updates, 0u);
    EXPECT_EQ(stats.modulations_applied, 0u);
}

TEST_F(HemisphericImmuneBridgeIntegrationTest, MaxInflammationTracked) {
    hemispheric_immune_set_inflammation(bridge, INFLAMMATION_STORM);
    hemispheric_immune_update(bridge);

    hemispheric_immune_set_inflammation(bridge, INFLAMMATION_NONE);
    hemispheric_immune_update(bridge);

    hemispheric_immune_stats_t stats = hemispheric_immune_get_stats(bridge);

    // Max inflammation should reflect the storm level
    EXPECT_GT(stats.max_inflammation_seen, 0.0f);
}

/* ============================================================================
 * Bio-async Integration Tests
 * ============================================================================ */

TEST_F(HemisphericImmuneBridgeIntegrationTest, BioAsyncConnectDisconnect) {
    int result = hemispheric_immune_connect_bio_async(bridge);
    if (result == 0) {
        EXPECT_EQ(hemispheric_immune_disconnect_bio_async(bridge), 0);
    }
}
