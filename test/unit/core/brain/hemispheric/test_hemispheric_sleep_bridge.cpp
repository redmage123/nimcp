//=============================================================================
// test_hemispheric_sleep_bridge.cpp - Hemispheric Sleep Bridge Unit Tests
//=============================================================================
/**
 * @file test_hemispheric_sleep_bridge.cpp
 * @brief Unit tests for hemispheric-sleep bidirectional integration
 *
 * Tests:
 * - Lifecycle (creation, destruction)
 * - Sleep stage effects on activity
 * - Callosum recovery across sleep stages
 * - Learning rate modulation
 * - Consolidation modes
 * - Bio-async integration
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "core/brain/hemispheric/nimcp_hemispheric_sleep_bridge.h"
#include "core/brain/hemispheric/nimcp_hemispheric_brain.h"
#include "cognitive/free_energy/nimcp_fep_sleep.h"

//=============================================================================
// Test Fixture
//=============================================================================

class HemisphericSleepBridgeTest : public ::testing::Test {
protected:
    // Shared brain instance - created once for all tests (very expensive to create)
    static hemispheric_brain_t* shared_brain;

    hemispheric_sleep_bridge_t* bridge = nullptr;
    hemispheric_brain_t* brain = nullptr;

    static void SetUpTestSuite() {
        hemispheric_brain_config_t brain_config = hemispheric_brain_default_config();
        brain_config.size = BRAIN_SIZE_TINY;
        brain_config.num_inputs = 8;
        brain_config.num_outputs = 4;
        brain_config.initial_tier = PLATFORM_TIER_CONSTRAINED;
        brain_config.enable_bio_async = false;
        brain_config.left_config.size = BRAIN_SIZE_TINY;
        brain_config.left_config.initial_tier = PLATFORM_TIER_CONSTRAINED;
        brain_config.left_config.enable_bio_async = false;
        brain_config.right_config.size = BRAIN_SIZE_TINY;
        brain_config.right_config.initial_tier = PLATFORM_TIER_CONSTRAINED;
        brain_config.right_config.enable_bio_async = false;
        shared_brain = hemispheric_brain_create(&brain_config);
    }

    static void TearDownTestSuite() {
        if (shared_brain) {
            hemispheric_brain_destroy(shared_brain);
            shared_brain = nullptr;
        }
    }

    void SetUp() override {
        brain = shared_brain;
        ASSERT_NE(brain, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            hemispheric_sleep_destroy(bridge);
            bridge = nullptr;
        }
    }
};

hemispheric_brain_t* HemisphericSleepBridgeTest::shared_brain = nullptr;

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(HemisphericSleepBridgeTest, DefaultConfigValues) {
    hemispheric_sleep_config_t config = hemispheric_sleep_default_config();

    EXPECT_FLOAT_EQ(config.left_sleep_depth_bias, 0.3f);
    EXPECT_FLOAT_EQ(config.right_vigilance_bias, 0.2f);
    EXPECT_FLOAT_EQ(config.callosum_recovery_rate, 1.0f);
    EXPECT_FLOAT_EQ(config.max_callosum_efficiency, 1.0f);
    EXPECT_TRUE(config.enable_consolidation);
    EXPECT_FLOAT_EQ(config.consolidation_threshold, 0.3f);
    EXPECT_TRUE(config.enable_bio_async);
}

TEST_F(HemisphericSleepBridgeTest, CreateWithDefaultConfig) {
    hemispheric_sleep_config_t config = hemispheric_sleep_default_config();
    bridge = hemispheric_sleep_create(&config, brain, nullptr);

    ASSERT_NE(bridge, nullptr);
}

TEST_F(HemisphericSleepBridgeTest, CreateWithNullConfig) {
    bridge = hemispheric_sleep_create(nullptr, brain, nullptr);

    ASSERT_NE(bridge, nullptr);
}

TEST_F(HemisphericSleepBridgeTest, CreateWithNullBrain) {
    hemispheric_sleep_config_t config = hemispheric_sleep_default_config();
    bridge = hemispheric_sleep_create(&config, nullptr, nullptr);

    EXPECT_EQ(bridge, nullptr);
}

TEST_F(HemisphericSleepBridgeTest, DestroyNullBridge) {
    // Should not crash
    hemispheric_sleep_destroy(nullptr);
}

//=============================================================================
// Sleep Stage Effect Tests
//=============================================================================

TEST_F(HemisphericSleepBridgeTest, AwakeStateFullActivity) {
    bridge = hemispheric_sleep_create(nullptr, brain, nullptr);
    ASSERT_NE(bridge, nullptr);

    // Set awake state
    int result = hemispheric_sleep_set_stage(bridge, SLEEP_STAGE_WAKE);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Check activity levels
    float left_activity = hemispheric_sleep_get_activity_level(bridge, HEMISPHERE_LEFT);
    float right_activity = hemispheric_sleep_get_activity_level(bridge, HEMISPHERE_RIGHT);

    EXPECT_NEAR(left_activity, 1.0f, 0.15f);
    EXPECT_NEAR(right_activity, 1.0f, 0.15f);
}

TEST_F(HemisphericSleepBridgeTest, NREM1ReducedActivity) {
    bridge = hemispheric_sleep_create(nullptr, brain, nullptr);
    ASSERT_NE(bridge, nullptr);

    hemispheric_sleep_set_stage(bridge, SLEEP_STAGE_N1);

    float left_activity = hemispheric_sleep_get_activity_level(bridge, HEMISPHERE_LEFT);
    float right_activity = hemispheric_sleep_get_activity_level(bridge, HEMISPHERE_RIGHT);

    // Left should be around 0.80, right around 0.85
    EXPECT_LT(left_activity, 0.90f);
    EXPECT_LT(right_activity, 0.95f);
    EXPECT_GT(left_activity, 0.60f);
    EXPECT_GT(right_activity, 0.70f);
}

TEST_F(HemisphericSleepBridgeTest, NREM3DeepSleepLowActivity) {
    bridge = hemispheric_sleep_create(nullptr, brain, nullptr);
    ASSERT_NE(bridge, nullptr);

    hemispheric_sleep_set_stage(bridge, SLEEP_STAGE_SWS);

    float left_activity = hemispheric_sleep_get_activity_level(bridge, HEMISPHERE_LEFT);
    float right_activity = hemispheric_sleep_get_activity_level(bridge, HEMISPHERE_RIGHT);

    // Deep sleep should have much lower activity
    EXPECT_LT(left_activity, 0.40f);
    EXPECT_LT(right_activity, 0.45f);
}

TEST_F(HemisphericSleepBridgeTest, REMDreamingState) {
    bridge = hemispheric_sleep_create(nullptr, brain, nullptr);
    ASSERT_NE(bridge, nullptr);

    hemispheric_sleep_set_stage(bridge, SLEEP_STAGE_REM);

    // Check dreaming state
    bool left_dreaming = hemispheric_sleep_is_dreaming(bridge, HEMISPHERE_LEFT);
    bool right_dreaming = hemispheric_sleep_is_dreaming(bridge, HEMISPHERE_RIGHT);

    EXPECT_TRUE(left_dreaming);
    EXPECT_TRUE(right_dreaming);
}

TEST_F(HemisphericSleepBridgeTest, NotDreamingDuringNREM) {
    bridge = hemispheric_sleep_create(nullptr, brain, nullptr);
    ASSERT_NE(bridge, nullptr);

    hemispheric_sleep_set_stage(bridge, SLEEP_STAGE_N2);

    bool left_dreaming = hemispheric_sleep_is_dreaming(bridge, HEMISPHERE_LEFT);
    bool right_dreaming = hemispheric_sleep_is_dreaming(bridge, HEMISPHERE_RIGHT);

    EXPECT_FALSE(left_dreaming);
    EXPECT_FALSE(right_dreaming);
}

//=============================================================================
// Callosum Recovery Tests
//=============================================================================

TEST_F(HemisphericSleepBridgeTest, CallosumRecoveryInSWS) {
    bridge = hemispheric_sleep_create(nullptr, brain, nullptr);
    ASSERT_NE(bridge, nullptr);

    hemispheric_sleep_set_stage(bridge, SLEEP_STAGE_SWS);

    callosum_sleep_effects_t effects = hemispheric_sleep_get_callosum_effects(bridge);

    EXPECT_TRUE(effects.recovery_active);
    EXPECT_GT(effects.bandwidth_recovery, 0.0f);
}

TEST_F(HemisphericSleepBridgeTest, CallosumRecoveryHighestInREM) {
    bridge = hemispheric_sleep_create(nullptr, brain, nullptr);
    ASSERT_NE(bridge, nullptr);

    // Get REM recovery rate
    hemispheric_sleep_set_stage(bridge, SLEEP_STAGE_REM);
    callosum_sleep_effects_t rem_effects = hemispheric_sleep_get_callosum_effects(bridge);

    // Get SWS recovery rate
    hemispheric_sleep_set_stage(bridge, SLEEP_STAGE_SWS);
    callosum_sleep_effects_t sws_effects = hemispheric_sleep_get_callosum_effects(bridge);

    // REM should have higher recovery than SWS
    EXPECT_GT(rem_effects.bandwidth_recovery, sws_effects.bandwidth_recovery);
}

TEST_F(HemisphericSleepBridgeTest, CallosumDegradesAwake) {
    bridge = hemispheric_sleep_create(nullptr, brain, nullptr);
    ASSERT_NE(bridge, nullptr);

    hemispheric_sleep_set_stage(bridge, SLEEP_STAGE_WAKE);

    callosum_sleep_effects_t effects = hemispheric_sleep_get_callosum_effects(bridge);

    // Awake should have negative recovery (degradation)
    EXPECT_FALSE(effects.recovery_active);
    EXPECT_LT(effects.bandwidth_recovery, 0.0f);
}

TEST_F(HemisphericSleepBridgeTest, ResetCallosum) {
    bridge = hemispheric_sleep_create(nullptr, brain, nullptr);
    ASSERT_NE(bridge, nullptr);

    int result = hemispheric_sleep_reset_callosum(bridge);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    callosum_sleep_effects_t effects = hemispheric_sleep_get_callosum_effects(bridge);
    EXPECT_FLOAT_EQ(effects.current_efficiency, 1.0f);
}

//=============================================================================
// Consolidation Mode Tests
//=============================================================================

TEST_F(HemisphericSleepBridgeTest, NoConsolidationAwake) {
    bridge = hemispheric_sleep_create(nullptr, brain, nullptr);
    ASSERT_NE(bridge, nullptr);

    hemispheric_sleep_set_stage(bridge, SLEEP_STAGE_WAKE);

    consolidation_mode_t mode = hemispheric_sleep_get_consolidation_mode(bridge);
    EXPECT_EQ(mode, CONSOLIDATION_NONE);
}

TEST_F(HemisphericSleepBridgeTest, BilateralConsolidationInSWS) {
    bridge = hemispheric_sleep_create(nullptr, brain, nullptr);
    ASSERT_NE(bridge, nullptr);

    hemispheric_sleep_set_stage(bridge, SLEEP_STAGE_SWS);

    consolidation_mode_t mode = hemispheric_sleep_get_consolidation_mode(bridge);
    EXPECT_EQ(mode, CONSOLIDATION_BILATERAL);
}

TEST_F(HemisphericSleepBridgeTest, InterhemisphericConsolidationInREM) {
    bridge = hemispheric_sleep_create(nullptr, brain, nullptr);
    ASSERT_NE(bridge, nullptr);

    hemispheric_sleep_set_stage(bridge, SLEEP_STAGE_REM);

    consolidation_mode_t mode = hemispheric_sleep_get_consolidation_mode(bridge);
    EXPECT_EQ(mode, CONSOLIDATION_INTERHEMISPHERIC);
}

TEST_F(HemisphericSleepBridgeTest, TriggerTransfer) {
    bridge = hemispheric_sleep_create(nullptr, brain, nullptr);
    ASSERT_NE(bridge, nullptr);

    int result = hemispheric_sleep_trigger_transfer(bridge);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    consolidation_mode_t mode = hemispheric_sleep_get_consolidation_mode(bridge);
    EXPECT_EQ(mode, CONSOLIDATION_INTERHEMISPHERIC);
}

//=============================================================================
// Learning Rate Modulation Tests
//=============================================================================

TEST_F(HemisphericSleepBridgeTest, FullLearningRateAwake) {
    bridge = hemispheric_sleep_create(nullptr, brain, nullptr);
    ASSERT_NE(bridge, nullptr);

    hemispheric_sleep_set_stage(bridge, SLEEP_STAGE_WAKE);

    hemisphere_sleep_effects_t left_effects = hemispheric_sleep_get_left_effects(bridge);
    hemisphere_sleep_effects_t right_effects = hemispheric_sleep_get_right_effects(bridge);

    EXPECT_FLOAT_EQ(left_effects.learning_rate_factor, 1.0f);
    EXPECT_FLOAT_EQ(right_effects.learning_rate_factor, 1.0f);
}

TEST_F(HemisphericSleepBridgeTest, ReducedLearningRateSWS) {
    bridge = hemispheric_sleep_create(nullptr, brain, nullptr);
    ASSERT_NE(bridge, nullptr);

    hemispheric_sleep_set_stage(bridge, SLEEP_STAGE_SWS);

    hemisphere_sleep_effects_t effects = hemispheric_sleep_get_left_effects(bridge);

    // SWS should have minimal learning (consolidation focus)
    EXPECT_LT(effects.learning_rate_factor, 0.15f);
}

//=============================================================================
// Update and Apply Tests
//=============================================================================

TEST_F(HemisphericSleepBridgeTest, UpdateSucceeds) {
    bridge = hemispheric_sleep_create(nullptr, brain, nullptr);
    ASSERT_NE(bridge, nullptr);

    int result = hemispheric_sleep_update(bridge);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(HemisphericSleepBridgeTest, ApplyModulationSucceeds) {
    bridge = hemispheric_sleep_create(nullptr, brain, nullptr);
    ASSERT_NE(bridge, nullptr);

    int result = hemispheric_sleep_apply_modulation(bridge);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(HemisphericSleepBridgeTest, UpdateWithNullBridge) {
    int result = hemispheric_sleep_update(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(HemisphericSleepBridgeTest, ApplyWithNullBridge) {
    int result = hemispheric_sleep_apply_modulation(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(HemisphericSleepBridgeTest, StatsAccumulate) {
    bridge = hemispheric_sleep_create(nullptr, brain, nullptr);
    ASSERT_NE(bridge, nullptr);

    // Initial stats should be zero
    hemispheric_sleep_stats_t initial = hemispheric_sleep_get_stats(bridge);
    EXPECT_EQ(initial.updates, 0u);

    // Do some updates
    hemispheric_sleep_set_stage(bridge, SLEEP_STAGE_N1);
    hemispheric_sleep_set_stage(bridge, SLEEP_STAGE_N2);
    hemispheric_sleep_set_stage(bridge, SLEEP_STAGE_REM);

    hemispheric_sleep_stats_t after = hemispheric_sleep_get_stats(bridge);
    EXPECT_GT(after.updates, 0u);
}

TEST_F(HemisphericSleepBridgeTest, StatsTrackTransfers) {
    bridge = hemispheric_sleep_create(nullptr, brain, nullptr);
    ASSERT_NE(bridge, nullptr);

    hemispheric_sleep_stats_t initial = hemispheric_sleep_get_stats(bridge);
    uint64_t initial_transfers = initial.interhemispheric_transfers;

    hemispheric_sleep_trigger_transfer(bridge);
    hemispheric_sleep_trigger_transfer(bridge);

    hemispheric_sleep_stats_t after = hemispheric_sleep_get_stats(bridge);
    EXPECT_EQ(after.interhemispheric_transfers, initial_transfers + 2);
}

TEST_F(HemisphericSleepBridgeTest, ResetStats) {
    bridge = hemispheric_sleep_create(nullptr, brain, nullptr);
    ASSERT_NE(bridge, nullptr);

    // Accumulate some stats
    hemispheric_sleep_set_stage(bridge, SLEEP_STAGE_REM);
    hemispheric_sleep_trigger_transfer(bridge);

    // Reset
    hemispheric_sleep_reset_stats(bridge);

    hemispheric_sleep_stats_t stats = hemispheric_sleep_get_stats(bridge);
    EXPECT_EQ(stats.updates, 0u);
    EXPECT_EQ(stats.interhemispheric_transfers, 0u);
}

//=============================================================================
// Bio-async Tests
//=============================================================================

TEST_F(HemisphericSleepBridgeTest, ConnectBioAsync) {
    bridge = hemispheric_sleep_create(nullptr, brain, nullptr);
    ASSERT_NE(bridge, nullptr);

    int result = hemispheric_sleep_connect_bio_async(bridge);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(HemisphericSleepBridgeTest, DisconnectBioAsync) {
    bridge = hemispheric_sleep_create(nullptr, brain, nullptr);
    ASSERT_NE(bridge, nullptr);

    hemispheric_sleep_connect_bio_async(bridge);
    int result = hemispheric_sleep_disconnect_bio_async(bridge);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(HemisphericSleepBridgeTest, ConnectBioAsyncNull) {
    int result = hemispheric_sleep_connect_bio_async(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

//=============================================================================
// Asymmetric Sleep Tests
//=============================================================================

TEST_F(HemisphericSleepBridgeTest, LeftSleepsDeeper) {
    hemispheric_sleep_config_t config = hemispheric_sleep_default_config();
    config.left_sleep_depth_bias = 0.5f;  // Strong left bias
    config.right_vigilance_bias = 0.3f;

    bridge = hemispheric_sleep_create(&config, brain, nullptr);
    ASSERT_NE(bridge, nullptr);

    hemispheric_sleep_set_stage(bridge, SLEEP_STAGE_N2);

    float left_activity = hemispheric_sleep_get_activity_level(bridge, HEMISPHERE_LEFT);
    float right_activity = hemispheric_sleep_get_activity_level(bridge, HEMISPHERE_RIGHT);

    // Left should have lower activity (sleeping deeper)
    EXPECT_LT(left_activity, right_activity);
}

TEST_F(HemisphericSleepBridgeTest, RightMaintainsVigilance) {
    hemispheric_sleep_config_t config = hemispheric_sleep_default_config();
    bridge = hemispheric_sleep_create(&config, brain, nullptr);
    ASSERT_NE(bridge, nullptr);

    hemispheric_sleep_set_stage(bridge, SLEEP_STAGE_N1);

    float left_activity = hemispheric_sleep_get_activity_level(bridge, HEMISPHERE_LEFT);
    float right_activity = hemispheric_sleep_get_activity_level(bridge, HEMISPHERE_RIGHT);

    // Right maintains higher vigilance
    EXPECT_GE(right_activity, left_activity);
}

//=============================================================================
// Query with Null/Invalid Bridge Tests
//=============================================================================

TEST_F(HemisphericSleepBridgeTest, GetLeftEffectsNullBridge) {
    hemisphere_sleep_effects_t effects = hemispheric_sleep_get_left_effects(nullptr);
    EXPECT_FLOAT_EQ(effects.activity_factor, 1.0f);
}

TEST_F(HemisphericSleepBridgeTest, GetRightEffectsNullBridge) {
    hemisphere_sleep_effects_t effects = hemispheric_sleep_get_right_effects(nullptr);
    EXPECT_FLOAT_EQ(effects.activity_factor, 1.0f);
}

TEST_F(HemisphericSleepBridgeTest, GetCallosumEffectsNullBridge) {
    callosum_sleep_effects_t effects = hemispheric_sleep_get_callosum_effects(nullptr);
    EXPECT_FLOAT_EQ(effects.current_efficiency, 1.0f);
}

TEST_F(HemisphericSleepBridgeTest, GetActivityLevelNullBridge) {
    float activity = hemispheric_sleep_get_activity_level(nullptr, HEMISPHERE_LEFT);
    EXPECT_FLOAT_EQ(activity, 1.0f);
}

TEST_F(HemisphericSleepBridgeTest, GetConsolidationModeNullBridge) {
    consolidation_mode_t mode = hemispheric_sleep_get_consolidation_mode(nullptr);
    EXPECT_EQ(mode, CONSOLIDATION_NONE);
}

TEST_F(HemisphericSleepBridgeTest, IsDreamingNullBridge) {
    bool dreaming = hemispheric_sleep_is_dreaming(nullptr, HEMISPHERE_LEFT);
    EXPECT_FALSE(dreaming);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
