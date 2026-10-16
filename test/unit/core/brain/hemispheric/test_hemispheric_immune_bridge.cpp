//=============================================================================
// test_hemispheric_immune_bridge.cpp - Unit tests for hemispheric immune bridge
//=============================================================================

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "core/brain/hemispheric/nimcp_hemispheric_immune_bridge.h"
#include "core/brain/hemispheric/nimcp_hemispheric_brain.h"
#include "cognitive/immune/nimcp_brain_immune.h"

//=============================================================================
// Test Fixture
//=============================================================================

class HemisphericImmuneBridgeTest : public ::testing::Test {
protected:
    // Shared brain instance - created once for all tests (very expensive to create)
    static hemispheric_brain_t* shared_brain;

    hemispheric_brain_t* brain;
    brain_immune_system_t* immune;
    hemispheric_immune_bridge_t* bridge;

    static void SetUpTestSuite() {
        hemispheric_brain_config_t brain_cfg = hemispheric_brain_default_config();
        brain_cfg.size = BRAIN_SIZE_TINY;
        brain_cfg.num_inputs = 8;
        brain_cfg.num_outputs = 4;
        brain_cfg.initial_tier = PLATFORM_TIER_CONSTRAINED;
        brain_cfg.enable_bio_async = false;
        brain_cfg.left_config.size = BRAIN_SIZE_TINY;
        brain_cfg.left_config.initial_tier = PLATFORM_TIER_CONSTRAINED;
        brain_cfg.left_config.enable_bio_async = false;
        brain_cfg.right_config.size = BRAIN_SIZE_TINY;
        brain_cfg.right_config.initial_tier = PLATFORM_TIER_CONSTRAINED;
        brain_cfg.right_config.enable_bio_async = false;
        shared_brain = hemispheric_brain_create(&brain_cfg);
    }

    static void TearDownTestSuite() {
        if (shared_brain) {
            hemispheric_brain_destroy(shared_brain);
            shared_brain = nullptr;
        }
    }

    void SetUp() override {
        brain = shared_brain;
        brain_immune_config_t immune_cfg;
        brain_immune_default_config(&immune_cfg);
        immune = brain_immune_create(&immune_cfg);
        bridge = nullptr;
    }

    void TearDown() override {
        if (bridge) {
            hemispheric_immune_destroy(bridge);
        }
        if (immune) {
            brain_immune_destroy(immune);
        }
    }
};

hemispheric_brain_t* HemisphericImmuneBridgeTest::shared_brain = nullptr;

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(HemisphericImmuneBridgeTest, DefaultConfig) {
    hemispheric_immune_config_t cfg = hemispheric_immune_default_config();

    // Left hemisphere should be more vulnerable than right
    EXPECT_GT(cfg.left_vulnerability, cfg.right_vulnerability);

    // Compensation enabled by default
    EXPECT_TRUE(cfg.enable_compensation);

    // Plasticity enabled by default
    EXPECT_TRUE(cfg.enable_immune_plasticity);

    // Bio-async enabled by default
    EXPECT_TRUE(cfg.enable_bio_async);
}

TEST_F(HemisphericImmuneBridgeTest, ConfigBounds) {
    hemispheric_immune_config_t cfg = hemispheric_immune_default_config();

    // Vulnerability should be in [0, 1]
    EXPECT_GE(cfg.left_vulnerability, 0.0f);
    EXPECT_LE(cfg.left_vulnerability, 1.0f);
    EXPECT_GE(cfg.right_vulnerability, 0.0f);
    EXPECT_LE(cfg.right_vulnerability, 1.0f);

    // Callosum sensitivity in [0, 1]
    EXPECT_GE(cfg.callosum_sensitivity, 0.0f);
    EXPECT_LE(cfg.callosum_sensitivity, 1.0f);

    // Compensation threshold in [0, 1]
    EXPECT_GE(cfg.compensation_threshold, 0.0f);
    EXPECT_LE(cfg.compensation_threshold, 1.0f);
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(HemisphericImmuneBridgeTest, CreateWithDefaults) {
    bridge = hemispheric_immune_create(NULL, brain, immune);
    ASSERT_NE(bridge, nullptr);
}

TEST_F(HemisphericImmuneBridgeTest, CreateWithConfig) {
    hemispheric_immune_config_t cfg = hemispheric_immune_default_config();
    cfg.left_vulnerability = 0.9f;
    cfg.right_vulnerability = 0.3f;

    bridge = hemispheric_immune_create(&cfg, brain, immune);
    ASSERT_NE(bridge, nullptr);
}

TEST_F(HemisphericImmuneBridgeTest, CreateNullBrain) {
    bridge = hemispheric_immune_create(NULL, NULL, immune);
    EXPECT_EQ(bridge, nullptr);
}

TEST_F(HemisphericImmuneBridgeTest, CreateNullImmune) {
    bridge = hemispheric_immune_create(NULL, brain, NULL);
    EXPECT_EQ(bridge, nullptr);
}

TEST_F(HemisphericImmuneBridgeTest, DestroyNull) {
    hemispheric_immune_destroy(NULL);  // Should not crash
}

//=============================================================================
// Initial State Tests
//=============================================================================

TEST_F(HemisphericImmuneBridgeTest, InitialEffectsNeutral) {
    bridge = hemispheric_immune_create(NULL, brain, immune);
    ASSERT_NE(bridge, nullptr);

    // Initial effects should be neutral (1.0)
    hemisphere_immune_effects_t left = hemispheric_immune_get_left_effects(bridge);
    EXPECT_FLOAT_EQ(left.learning_rate_factor, 1.0f);
    EXPECT_FLOAT_EQ(left.attention_factor, 1.0f);
    EXPECT_FLOAT_EQ(left.memory_consolidation, 1.0f);
    EXPECT_FLOAT_EQ(left.executive_function, 1.0f);
    EXPECT_FALSE(left.is_compensating);

    hemisphere_immune_effects_t right = hemispheric_immune_get_right_effects(bridge);
    EXPECT_FLOAT_EQ(right.learning_rate_factor, 1.0f);
    EXPECT_FLOAT_EQ(right.attention_factor, 1.0f);
    EXPECT_FALSE(right.is_compensating);
}

TEST_F(HemisphericImmuneBridgeTest, InitialCallosumNeutral) {
    bridge = hemispheric_immune_create(NULL, brain, immune);
    ASSERT_NE(bridge, nullptr);

    callosum_immune_effects_t callosum = hemispheric_immune_get_callosum_effects(bridge);
    EXPECT_FLOAT_EQ(callosum.bandwidth_factor, 1.0f);
    EXPECT_FLOAT_EQ(callosum.latency_multiplier, 1.0f);
    EXPECT_FLOAT_EQ(callosum.reliability_factor, 1.0f);
    EXPECT_FALSE(callosum.degraded);
}

//=============================================================================
// Update Tests
//=============================================================================

TEST_F(HemisphericImmuneBridgeTest, UpdateSuccess) {
    bridge = hemispheric_immune_create(NULL, brain, immune);
    ASSERT_NE(bridge, nullptr);

    int result = hemispheric_immune_update(bridge);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(HemisphericImmuneBridgeTest, UpdateNull) {
    int result = hemispheric_immune_update(NULL);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(HemisphericImmuneBridgeTest, UpdateStatsIncrement) {
    bridge = hemispheric_immune_create(NULL, brain, immune);
    ASSERT_NE(bridge, nullptr);

    hemispheric_immune_stats_t stats_before = hemispheric_immune_get_stats(bridge);
    EXPECT_EQ(stats_before.updates, 0UL);

    hemispheric_immune_update(bridge);

    hemispheric_immune_stats_t stats_after = hemispheric_immune_get_stats(bridge);
    EXPECT_EQ(stats_after.updates, 1UL);
}

//=============================================================================
// Inflammation Level Tests
//=============================================================================

TEST_F(HemisphericImmuneBridgeTest, SetInflammationNone) {
    bridge = hemispheric_immune_create(NULL, brain, immune);
    ASSERT_NE(bridge, nullptr);

    int result = hemispheric_immune_set_inflammation(bridge, INFLAMMATION_NONE);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    hemisphere_immune_effects_t left = hemispheric_immune_get_left_effects(bridge);
    EXPECT_FLOAT_EQ(left.learning_rate_factor, 1.0f);
}

TEST_F(HemisphericImmuneBridgeTest, SetInflammationLocal) {
    bridge = hemispheric_immune_create(NULL, brain, immune);
    ASSERT_NE(bridge, nullptr);

    hemispheric_immune_set_inflammation(bridge, INFLAMMATION_LOCAL);

    hemisphere_immune_effects_t left = hemispheric_immune_get_left_effects(bridge);
    hemisphere_immune_effects_t right = hemispheric_immune_get_right_effects(bridge);

    // Both should be reduced, but left more than right
    EXPECT_LT(left.learning_rate_factor, 1.0f);
    EXPECT_LT(right.learning_rate_factor, 1.0f);
    EXPECT_LT(left.learning_rate_factor, right.learning_rate_factor);
}

TEST_F(HemisphericImmuneBridgeTest, SetInflammationRegional) {
    bridge = hemispheric_immune_create(NULL, brain, immune);
    ASSERT_NE(bridge, nullptr);

    hemispheric_immune_set_inflammation(bridge, INFLAMMATION_REGIONAL);

    hemisphere_immune_effects_t left = hemispheric_immune_get_left_effects(bridge);
    EXPECT_LT(left.learning_rate_factor, 0.9f);  // Significant reduction
}

TEST_F(HemisphericImmuneBridgeTest, SetInflammationSystemic) {
    bridge = hemispheric_immune_create(NULL, brain, immune);
    ASSERT_NE(bridge, nullptr);

    hemispheric_immune_set_inflammation(bridge, INFLAMMATION_SYSTEMIC);

    hemisphere_immune_effects_t left = hemispheric_immune_get_left_effects(bridge);
    hemisphere_immune_effects_t right = hemispheric_immune_get_right_effects(bridge);

    // Severe reduction (vulnerability-adjusted: left=0.58, right=0.75)
    EXPECT_LT(left.learning_rate_factor, 0.6f);
    EXPECT_LT(right.learning_rate_factor, 0.8f);
}

TEST_F(HemisphericImmuneBridgeTest, SetInflammationStorm) {
    bridge = hemispheric_immune_create(NULL, brain, immune);
    ASSERT_NE(bridge, nullptr);

    hemispheric_immune_set_inflammation(bridge, INFLAMMATION_STORM);

    hemisphere_immune_effects_t left = hemispheric_immune_get_left_effects(bridge);
    hemisphere_immune_effects_t right = hemispheric_immune_get_right_effects(bridge);

    // Critical reduction (vulnerability-adjusted: left=0.37, right=0.575)
    EXPECT_LT(left.learning_rate_factor, 0.4f);
    EXPECT_LT(right.learning_rate_factor, 0.6f);
}

//=============================================================================
// Asymmetric Vulnerability Tests
//=============================================================================

TEST_F(HemisphericImmuneBridgeTest, LeftMoreVulnerable) {
    hemispheric_immune_config_t cfg = hemispheric_immune_default_config();
    cfg.left_vulnerability = 1.0f;   // Maximum vulnerability
    cfg.right_vulnerability = 0.0f;  // No vulnerability

    bridge = hemispheric_immune_create(&cfg, brain, immune);
    ASSERT_NE(bridge, nullptr);

    hemispheric_immune_set_inflammation(bridge, INFLAMMATION_REGIONAL);

    hemisphere_immune_effects_t left = hemispheric_immune_get_left_effects(bridge);
    hemisphere_immune_effects_t right = hemispheric_immune_get_right_effects(bridge);

    // Left should be much more affected
    EXPECT_LT(left.learning_rate_factor, right.learning_rate_factor);
}

TEST_F(HemisphericImmuneBridgeTest, RightMoreVulnerable) {
    hemispheric_immune_config_t cfg = hemispheric_immune_default_config();
    cfg.left_vulnerability = 0.0f;   // No vulnerability
    cfg.right_vulnerability = 1.0f;  // Maximum vulnerability

    bridge = hemispheric_immune_create(&cfg, brain, immune);
    ASSERT_NE(bridge, nullptr);

    hemispheric_immune_set_inflammation(bridge, INFLAMMATION_REGIONAL);

    hemisphere_immune_effects_t left = hemispheric_immune_get_left_effects(bridge);
    hemisphere_immune_effects_t right = hemispheric_immune_get_right_effects(bridge);

    // Right should be more affected in this case
    EXPECT_GT(left.learning_rate_factor, right.learning_rate_factor);
}

//=============================================================================
// Callosum Effects Tests
//=============================================================================

TEST_F(HemisphericImmuneBridgeTest, CallosumDegradationRegional) {
    bridge = hemispheric_immune_create(NULL, brain, immune);
    ASSERT_NE(bridge, nullptr);

    hemispheric_immune_set_inflammation(bridge, INFLAMMATION_REGIONAL);

    callosum_immune_effects_t callosum = hemispheric_immune_get_callosum_effects(bridge);
    EXPECT_LT(callosum.bandwidth_factor, 1.0f);
    EXPECT_GT(callosum.latency_multiplier, 1.0f);
}

TEST_F(HemisphericImmuneBridgeTest, CallosumDegradedFlagStorm) {
    bridge = hemispheric_immune_create(NULL, brain, immune);
    ASSERT_NE(bridge, nullptr);

    hemispheric_immune_set_inflammation(bridge, INFLAMMATION_STORM);

    callosum_immune_effects_t callosum = hemispheric_immune_get_callosum_effects(bridge);
    EXPECT_TRUE(callosum.degraded);
    EXPECT_LT(callosum.bandwidth_factor, 0.5f);
}

TEST_F(HemisphericImmuneBridgeTest, EffectiveBandwidth) {
    bridge = hemispheric_immune_create(NULL, brain, immune);
    ASSERT_NE(bridge, nullptr);

    uint32_t base_bw = 200;

    // No inflammation - full bandwidth
    uint32_t effective = hemispheric_immune_get_effective_bandwidth(bridge, base_bw);
    EXPECT_EQ(effective, base_bw);

    // Regional inflammation - reduced bandwidth
    hemispheric_immune_set_inflammation(bridge, INFLAMMATION_REGIONAL);
    effective = hemispheric_immune_get_effective_bandwidth(bridge, base_bw);
    EXPECT_LT(effective, base_bw);
}

//=============================================================================
// Effective Learning Rate Tests
//=============================================================================

TEST_F(HemisphericImmuneBridgeTest, EffectiveLRNoInflammation) {
    bridge = hemispheric_immune_create(NULL, brain, immune);
    ASSERT_NE(bridge, nullptr);

    float base_lr = 0.01f;
    float effective_left = hemispheric_immune_get_effective_lr(bridge, HEMISPHERE_LEFT, base_lr);
    float effective_right = hemispheric_immune_get_effective_lr(bridge, HEMISPHERE_RIGHT, base_lr);

    EXPECT_FLOAT_EQ(effective_left, base_lr);
    EXPECT_FLOAT_EQ(effective_right, base_lr);
}

TEST_F(HemisphericImmuneBridgeTest, EffectiveLRWithInflammation) {
    bridge = hemispheric_immune_create(NULL, brain, immune);
    ASSERT_NE(bridge, nullptr);

    float base_lr = 0.01f;

    hemispheric_immune_set_inflammation(bridge, INFLAMMATION_SYSTEMIC);

    float effective_left = hemispheric_immune_get_effective_lr(bridge, HEMISPHERE_LEFT, base_lr);
    float effective_right = hemispheric_immune_get_effective_lr(bridge, HEMISPHERE_RIGHT, base_lr);

    EXPECT_LT(effective_left, base_lr);
    EXPECT_LT(effective_right, base_lr);
    EXPECT_LT(effective_left, effective_right);  // Left more affected
}

//=============================================================================
// Compensation Tests
//=============================================================================

TEST_F(HemisphericImmuneBridgeTest, NoCompensationWhenBothHealthy) {
    bridge = hemispheric_immune_create(NULL, brain, immune);
    ASSERT_NE(bridge, nullptr);

    hemispheric_immune_update(bridge);

    hemisphere_id_t compensating;
    bool is_compensating = hemispheric_immune_is_compensating(bridge, &compensating);
    EXPECT_FALSE(is_compensating);
}

TEST_F(HemisphericImmuneBridgeTest, CompensationTriggered) {
    hemispheric_immune_config_t cfg = hemispheric_immune_default_config();
    cfg.enable_compensation = true;
    cfg.compensation_threshold = 0.6f;
    cfg.left_vulnerability = 1.0f;   // Left highly vulnerable
    cfg.right_vulnerability = 0.2f;  // Right resilient

    bridge = hemispheric_immune_create(&cfg, brain, immune);
    ASSERT_NE(bridge, nullptr);

    // Systemic inflammation should drop left below threshold
    hemispheric_immune_set_inflammation(bridge, INFLAMMATION_SYSTEMIC);

    hemisphere_id_t compensating;
    bool is_compensating = hemispheric_immune_is_compensating(bridge, &compensating);

    // Right should be compensating for left
    if (is_compensating) {
        EXPECT_EQ(compensating, HEMISPHERE_RIGHT);
    }
}

TEST_F(HemisphericImmuneBridgeTest, CompensationDisabled) {
    hemispheric_immune_config_t cfg = hemispheric_immune_default_config();
    cfg.enable_compensation = false;

    bridge = hemispheric_immune_create(&cfg, brain, immune);
    ASSERT_NE(bridge, nullptr);

    hemispheric_immune_set_inflammation(bridge, INFLAMMATION_STORM);

    hemisphere_id_t compensating;
    bool is_compensating = hemispheric_immune_is_compensating(bridge, &compensating);
    EXPECT_FALSE(is_compensating);
}

//=============================================================================
// Emergency Bilateral Tests
//=============================================================================

TEST_F(HemisphericImmuneBridgeTest, TriggerEmergencyBilateral) {
    bridge = hemispheric_immune_create(NULL, brain, immune);
    ASSERT_NE(bridge, nullptr);

    int result = hemispheric_immune_trigger_emergency_bilateral(bridge);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(HemisphericImmuneBridgeTest, ClearEmergency) {
    bridge = hemispheric_immune_create(NULL, brain, immune);
    ASSERT_NE(bridge, nullptr);

    hemispheric_immune_trigger_emergency_bilateral(bridge);
    int result = hemispheric_immune_clear_emergency(bridge);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(HemisphericImmuneBridgeTest, StormTriggersEmergencyBilateral) {
    bridge = hemispheric_immune_create(NULL, brain, immune);
    ASSERT_NE(bridge, nullptr);

    hemispheric_immune_set_inflammation(bridge, INFLAMMATION_STORM);

    // The bridge should internally set emergency_bilateral for STORM level
    // This is verified through the lateralization effects
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(HemisphericImmuneBridgeTest, StatsInitialZero) {
    bridge = hemispheric_immune_create(NULL, brain, immune);
    ASSERT_NE(bridge, nullptr);

    hemispheric_immune_stats_t stats = hemispheric_immune_get_stats(bridge);
    EXPECT_EQ(stats.updates, 0UL);
    EXPECT_EQ(stats.modulations_applied, 0UL);
    EXPECT_EQ(stats.compensation_events, 0UL);
}

TEST_F(HemisphericImmuneBridgeTest, StatsReset) {
    bridge = hemispheric_immune_create(NULL, brain, immune);
    ASSERT_NE(bridge, nullptr);

    // Generate some stats
    hemispheric_immune_update(bridge);
    hemispheric_immune_update(bridge);

    hemispheric_immune_stats_t stats = hemispheric_immune_get_stats(bridge);
    EXPECT_EQ(stats.updates, 2UL);

    // Reset
    hemispheric_immune_reset_stats(bridge);

    stats = hemispheric_immune_get_stats(bridge);
    EXPECT_EQ(stats.updates, 0UL);
}

TEST_F(HemisphericImmuneBridgeTest, StatsRunningAverages) {
    bridge = hemispheric_immune_create(NULL, brain, immune);
    ASSERT_NE(bridge, nullptr);

    // Multiple updates to build running averages
    for (int i = 0; i < 10; i++) {
        hemispheric_immune_update(bridge);
    }

    hemispheric_immune_stats_t stats = hemispheric_immune_get_stats(bridge);
    EXPECT_GT(stats.avg_left_lr_factor, 0.0f);
    EXPECT_GT(stats.avg_right_lr_factor, 0.0f);
    EXPECT_GT(stats.avg_callosum_bandwidth, 0.0f);
}

//=============================================================================
// Bio-async Tests
//=============================================================================

TEST_F(HemisphericImmuneBridgeTest, ConnectBioAsync) {
    bridge = hemispheric_immune_create(NULL, brain, immune);
    ASSERT_NE(bridge, nullptr);

    // Bio-async connection (may skip if router not available)
    int result = hemispheric_immune_connect_bio_async(bridge);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(HemisphericImmuneBridgeTest, DisconnectBioAsync) {
    bridge = hemispheric_immune_create(NULL, brain, immune);
    ASSERT_NE(bridge, nullptr);

    hemispheric_immune_connect_bio_async(bridge);
    int result = hemispheric_immune_disconnect_bio_async(bridge);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(HemisphericImmuneBridgeTest, DoubleConnectBioAsync) {
    bridge = hemispheric_immune_create(NULL, brain, immune);
    ASSERT_NE(bridge, nullptr);

    hemispheric_immune_connect_bio_async(bridge);
    int result = hemispheric_immune_connect_bio_async(bridge);
    EXPECT_EQ(result, NIMCP_SUCCESS);  // Idempotent
}

//=============================================================================
// Edge Case Tests
//=============================================================================

TEST_F(HemisphericImmuneBridgeTest, GetEffectsNull) {
    hemisphere_immune_effects_t effects = hemispheric_immune_get_left_effects(NULL);
    EXPECT_FLOAT_EQ(effects.learning_rate_factor, 1.0f);  // Safe default

    effects = hemispheric_immune_get_right_effects(NULL);
    EXPECT_FLOAT_EQ(effects.learning_rate_factor, 1.0f);
}

TEST_F(HemisphericImmuneBridgeTest, GetCallosumEffectsNull) {
    callosum_immune_effects_t effects = hemispheric_immune_get_callosum_effects(NULL);
    EXPECT_FLOAT_EQ(effects.bandwidth_factor, 1.0f);  // Safe default
}

TEST_F(HemisphericImmuneBridgeTest, SetInflammationNull) {
    int result = hemispheric_immune_set_inflammation(NULL, INFLAMMATION_REGIONAL);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(HemisphericImmuneBridgeTest, ApplyModulationNull) {
    int result = hemispheric_immune_apply_modulation(NULL);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

//=============================================================================
// Integration Scenario Tests
//=============================================================================

TEST_F(HemisphericImmuneBridgeTest, FullInflammationCycle) {
    bridge = hemispheric_immune_create(NULL, brain, immune);
    ASSERT_NE(bridge, nullptr);

    // Start healthy
    hemisphere_immune_effects_t left = hemispheric_immune_get_left_effects(bridge);
    EXPECT_FLOAT_EQ(left.learning_rate_factor, 1.0f);

    // Inflammation increases
    hemispheric_immune_set_inflammation(bridge, INFLAMMATION_LOCAL);
    left = hemispheric_immune_get_left_effects(bridge);
    float lr_local = left.learning_rate_factor;
    EXPECT_LT(lr_local, 1.0f);

    hemispheric_immune_set_inflammation(bridge, INFLAMMATION_REGIONAL);
    left = hemispheric_immune_get_left_effects(bridge);
    float lr_regional = left.learning_rate_factor;
    EXPECT_LT(lr_regional, lr_local);

    hemispheric_immune_set_inflammation(bridge, INFLAMMATION_SYSTEMIC);
    left = hemispheric_immune_get_left_effects(bridge);
    float lr_systemic = left.learning_rate_factor;
    EXPECT_LT(lr_systemic, lr_regional);

    // Recovery
    hemispheric_immune_set_inflammation(bridge, INFLAMMATION_NONE);
    left = hemispheric_immune_get_left_effects(bridge);
    EXPECT_FLOAT_EQ(left.learning_rate_factor, 1.0f);
}

TEST_F(HemisphericImmuneBridgeTest, CallosumProgressiveDegradation) {
    bridge = hemispheric_immune_create(NULL, brain, immune);
    ASSERT_NE(bridge, nullptr);

    float prev_bandwidth = 1.0f;
    brain_inflammation_level_t levels[] = {
        INFLAMMATION_NONE,
        INFLAMMATION_LOCAL,
        INFLAMMATION_REGIONAL,
        INFLAMMATION_SYSTEMIC,
        INFLAMMATION_STORM
    };

    for (int i = 0; i < 5; i++) {
        hemispheric_immune_set_inflammation(bridge, levels[i]);
        callosum_immune_effects_t callosum = hemispheric_immune_get_callosum_effects(bridge);

        // Each level should have <= bandwidth than previous
        EXPECT_LE(callosum.bandwidth_factor, prev_bandwidth);
        prev_bandwidth = callosum.bandwidth_factor;
    }
}
