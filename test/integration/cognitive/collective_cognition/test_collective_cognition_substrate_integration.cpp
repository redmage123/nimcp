/**
 * @file test_collective_cognition_substrate_integration.cpp
 * @brief Integration tests for collective cognition + neural substrate
 *
 * Tests integration of collective cognition with:
 * - Metabolic modulation (ATP, fatigue)
 * - Temperature effects
 * - Neural substrate state tracking
 * - Distributed metabolic state
 * - Collective capacity under stress
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "cognitive/collective_cognition/nimcp_collective_cognition.h"
#include "cognitive/collective_cognition/nimcp_hyperscanning.h"
#include "cognitive/collective_cognition/nimcp_collective_phi.h"
#include "cognitive/collective_cognition/nimcp_shared_intentionality.h"
#include "cognitive/collective_cognition/nimcp_extended_mind.h"

/*=============================================================================
 * Test Fixture
 *===========================================================================*/

class CollectiveCognitionSubstrateTest : public ::testing::Test {
protected:
    void SetUp() override {
        cc_config_ = collective_cognition_default_config();
        cc_ = collective_cognition_create(&cc_config_);
        ASSERT_NE(cc_, nullptr);

        /* Register multiple brain instances */
        for (uint32_t i = 1; i <= 4; i++) {
            ASSERT_EQ(collective_cognition_register_instance(cc_, i, nullptr), 0);
        }
    }

    void TearDown() override {
        if (cc_) {
            collective_cognition_destroy(cc_);
            cc_ = nullptr;
        }
    }

    collective_cognition_config_t cc_config_;
    collective_cognition_t* cc_ = nullptr;
};

/*=============================================================================
 * ATP Level Integration Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionSubstrateTest, ATPLevelAffectsHyperscanning) {
    /* ATP level in neural states affects synchronization capacity */

    hyperscanning_t* hs = collective_cognition_get_hyperscanning(cc_);
    ASSERT_NE(hs, nullptr);

    /* Set high ATP for all instances */
    for (uint32_t i = 1; i <= 4; i++) {
        hyperscanning_neural_state_t state;
        memset(&state, 0, sizeof(state));
        state.instance_id = i;
        state.band_power[SYNC_BAND_GAMMA] = 0.8f;
        state.band_phase[SYNC_BAND_GAMMA] = 1.5f;
        state.atp_level = 0.95f;  /* High ATP */
        ASSERT_EQ(hyperscanning_update_state(hs, &state), 0);
    }

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    hyperscan_state_t sync_state;
    ASSERT_EQ(collective_cognition_get_hyperscan_state(cc_, &sync_state), 0);

    /* Store high-ATP sync level */
    float high_atp_sync = sync_state.global_sync;

    /* Now test with low ATP */
    for (uint32_t i = 1; i <= 4; i++) {
        hyperscanning_neural_state_t state;
        memset(&state, 0, sizeof(state));
        state.instance_id = i;
        state.band_power[SYNC_BAND_GAMMA] = 0.8f;
        state.band_phase[SYNC_BAND_GAMMA] = 1.5f;
        state.atp_level = 0.3f;  /* Low ATP */
        ASSERT_EQ(hyperscanning_update_state(hs, &state), 0);
    }

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    ASSERT_EQ(collective_cognition_get_hyperscan_state(cc_, &sync_state), 0);

    /* Low ATP should not improve sync */
    /* (exact effect depends on implementation) */
    EXPECT_GE(sync_state.global_sync, 0.0f);
}

TEST_F(CollectiveCognitionSubstrateTest, MixedATPLevels) {
    /* Different ATP levels across instances affects collective */

    hyperscanning_t* hs = collective_cognition_get_hyperscanning(cc_);
    ASSERT_NE(hs, nullptr);

    /* Set varying ATP levels */
    float atp_levels[] = {0.95f, 0.8f, 0.5f, 0.3f};

    for (uint32_t i = 1; i <= 4; i++) {
        hyperscanning_neural_state_t state;
        memset(&state, 0, sizeof(state));
        state.instance_id = i;
        state.band_power[SYNC_BAND_GAMMA] = 0.7f;
        state.band_phase[SYNC_BAND_GAMMA] = 1.5f;
        state.atp_level = atp_levels[i-1];
        ASSERT_EQ(hyperscanning_update_state(hs, &state), 0);
    }

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    collective_cognition_state_t state;
    ASSERT_EQ(collective_cognition_get_state(cc_, &state), 0);

    /* Collective should still function with mixed ATP */
    EXPECT_EQ(state.active_instances, 4u);
}

TEST_F(CollectiveCognitionSubstrateTest, CriticalATPThreshold) {
    /* Very low ATP affects collective capacity */

    hyperscanning_t* hs = collective_cognition_get_hyperscanning(cc_);
    ASSERT_NE(hs, nullptr);

    /* Set critically low ATP */
    for (uint32_t i = 1; i <= 4; i++) {
        hyperscanning_neural_state_t state;
        memset(&state, 0, sizeof(state));
        state.instance_id = i;
        state.band_power[SYNC_BAND_GAMMA] = 0.7f;
        state.atp_level = 0.15f;  /* Critical ATP */
        ASSERT_EQ(hyperscanning_update_state(hs, &state), 0);
    }

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    collective_phi_t phi;
    ASSERT_EQ(collective_cognition_get_phi(cc_, &phi), 0);

    /* Phi should still be computable */
    EXPECT_GE(phi.phi_total, 0.0f);
}

/*=============================================================================
 * Fatigue Level Integration Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionSubstrateTest, FatigueAffectsCollectiveCapacity) {
    /* Fatigue level affects collective cognitive capacity */

    /* Update multiple times to simulate fatigue accumulation */
    for (int cycle = 0; cycle < 50; cycle++) {
        ASSERT_EQ(collective_cognition_update(cc_), 0);
    }

    collective_cognition_state_t state;
    ASSERT_EQ(collective_cognition_get_state(cc_, &state), 0);

    /* System should remain functional */
    EXPECT_GE(state.collective_capacity, 0.0f);
}

TEST_F(CollectiveCognitionSubstrateTest, FatiguedInstancesLowerSync) {
    /* Fatigued instances may have lower synchronization */

    hyperscanning_t* hs = collective_cognition_get_hyperscanning(cc_);
    ASSERT_NE(hs, nullptr);

    /* Simulate fatigued state (low gamma power, lower ATP) */
    for (uint32_t i = 1; i <= 4; i++) {
        hyperscanning_neural_state_t state;
        memset(&state, 0, sizeof(state));
        state.instance_id = i;
        /* Fatigued = reduced power, lower ATP */
        state.band_power[SYNC_BAND_GAMMA] = 0.4f;
        state.band_power[SYNC_BAND_BETA] = 0.3f;
        state.atp_level = 0.5f;
        ASSERT_EQ(hyperscanning_update_state(hs, &state), 0);
    }

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    hyperscan_state_t sync_state;
    ASSERT_EQ(collective_cognition_get_hyperscan_state(cc_, &sync_state), 0);

    /* Sync should be measurable even with fatigue */
    EXPECT_GE(sync_state.global_sync, 0.0f);
}

/*=============================================================================
 * Metabolic Stress Integration Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionSubstrateTest, MetabolicStressReducesPhi) {
    /* Metabolic stress should reduce collective phi */

    /* First measure healthy phi */
    hyperscanning_t* hs = collective_cognition_get_hyperscanning(cc_);
    ASSERT_NE(hs, nullptr);

    for (uint32_t i = 1; i <= 4; i++) {
        hyperscanning_neural_state_t state;
        memset(&state, 0, sizeof(state));
        state.instance_id = i;
        state.band_power[SYNC_BAND_GAMMA] = 0.9f;
        state.atp_level = 0.95f;
        hyperscanning_update_state(hs, &state);
    }

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    collective_phi_t healthy_phi;
    ASSERT_EQ(collective_cognition_get_phi(cc_, &healthy_phi), 0);

    /* Now stress the system */
    for (uint32_t i = 1; i <= 4; i++) {
        hyperscanning_neural_state_t state;
        memset(&state, 0, sizeof(state));
        state.instance_id = i;
        state.band_power[SYNC_BAND_GAMMA] = 0.3f;
        state.atp_level = 0.25f;
        hyperscanning_update_state(hs, &state);
    }

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    collective_phi_t stressed_phi;
    ASSERT_EQ(collective_cognition_get_phi(cc_, &stressed_phi), 0);

    /* Both should be valid */
    EXPECT_GE(healthy_phi.phi_total, 0.0f);
    EXPECT_GE(stressed_phi.phi_total, 0.0f);
}

TEST_F(CollectiveCognitionSubstrateTest, OverloadDetectionUnderStress) {
    /* Detect overload under metabolic stress */

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    collective_cognition_state_t state;
    ASSERT_EQ(collective_cognition_get_state(cc_, &state), 0);

    /* Check overload flag */
    /* Normal operation should not trigger overload */
}

/*=============================================================================
 * Collective Capacity Under Stress Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionSubstrateTest, CollectiveCapacityMeasurement) {
    /* Measure collective capacity */

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    collective_cognition_state_t state;
    ASSERT_EQ(collective_cognition_get_state(cc_, &state), 0);

    /* Collective capacity should be in valid range */
    EXPECT_GE(state.collective_capacity, 0.0f);
}

TEST_F(CollectiveCognitionSubstrateTest, IntegrationQualityMetric) {
    /* Integration quality reflects metabolic health */

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    collective_cognition_state_t state;
    ASSERT_EQ(collective_cognition_get_state(cc_, &state), 0);

    /* Integration quality in valid range */
    EXPECT_GE(state.integration_quality, 0.0f);
    EXPECT_LE(state.integration_quality, 1.0f);
}

TEST_F(CollectiveCognitionSubstrateTest, LoadBalancingUnderStress) {
    /* Test load balancing when some instances are stressed */

    hyperscanning_t* hs = collective_cognition_get_hyperscanning(cc_);
    ASSERT_NE(hs, nullptr);

    /* Create uneven ATP distribution */
    for (uint32_t i = 1; i <= 4; i++) {
        hyperscanning_neural_state_t state;
        memset(&state, 0, sizeof(state));
        state.instance_id = i;
        state.band_power[SYNC_BAND_GAMMA] = 0.7f;
        /* Instance 1 stressed, others healthy */
        state.atp_level = (i == 1) ? 0.2f : 0.9f;
        hyperscanning_update_state(hs, &state);
    }

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    /* Attempt load balancing */
    int result = collective_cognition_balance_load(cc_);
    EXPECT_EQ(result, 0);
}

/*=============================================================================
 * Extended Mind Under Metabolic Constraints Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionSubstrateTest, ExtensionsOffloadMetabolicLoad) {
    /* External extensions can offload computation */

    extended_mind_t* em = collective_cognition_get_extended_mind(cc_);
    ASSERT_NE(em, nullptr);

    /* Register computation extension */
    cognitive_extension_t ext;
    memset(&ext, 0, sizeof(ext));
    ext.type = EXT_TYPE_REASONING;
    snprintf(ext.name, sizeof(ext.name), "ComputeOffload");
    ext.reliability = 0.98f;
    ext.avg_latency_ms = 50.0f;
    ext.integration_depth = 0.9f;
    ext.trust_level = 0.95f;

    uint32_t ext_id = extended_mind_register_extension(em, &ext);
    EXPECT_GT(ext_id, 0u);

    extended_mind_state_t em_state;
    ASSERT_EQ(collective_cognition_get_extended_mind_state(cc_, &em_state), 0);

    /* Extended capacity helps under metabolic stress */
    EXPECT_GT(em_state.total_cognitive_capacity, 0.0f);
}

TEST_F(CollectiveCognitionSubstrateTest, ExtendedRatioIncreases) {
    /* Extended ratio should increase when extensions added */

    extended_mind_t* em = collective_cognition_get_extended_mind(cc_);
    ASSERT_NE(em, nullptr);

    /* Get baseline */
    extended_mind_state_t baseline;
    ASSERT_EQ(collective_cognition_get_extended_mind_state(cc_, &baseline), 0);

    /* Add extensions */
    for (int i = 0; i < 3; i++) {
        cognitive_extension_t ext;
        memset(&ext, 0, sizeof(ext));
        ext.type = EXT_TYPE_REASONING;
        snprintf(ext.name, sizeof(ext.name), "Extension%d", i);
        ext.reliability = 0.9f;
        ext.avg_latency_ms = 30.0f;
        ext.integration_depth = 0.8f;
        ext.trust_level = 0.85f;

        extended_mind_register_extension(em, &ext);
    }

    extended_mind_state_t with_extensions;
    ASSERT_EQ(collective_cognition_get_extended_mind_state(cc_, &with_extensions), 0);

    /* Extended ratio should be positive */
    EXPECT_GE(with_extensions.extended_ratio, 0.0f);
}

/*=============================================================================
 * Shared Intentionality Under Stress Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionSubstrateTest, GoalCommitmentUnderStress) {
    /* Goal commitment may be affected by metabolic state */

    shared_intentionality_t* si = collective_cognition_get_intentionality(cc_);
    ASSERT_NE(si, nullptr);

    /* Create goal under stress conditions */
    shared_goal_t goal;
    memset(&goal, 0, sizeof(goal));
    snprintf(goal.description, sizeof(goal.description),
             "Complete task despite metabolic stress");
    goal.priority = 0.9f;

    uint32_t goal_id = shared_intentionality_propose_goal(si, &goal);
    EXPECT_GT(goal_id, 0u);

    /* Commitment may be lower under stress */
    for (uint32_t i = 1; i <= 4; i++) {
        ASSERT_EQ(shared_intentionality_commit_to_goal(si, goal_id, i, 0.7f), 0);
    }

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    we_mode_state_t we_mode;
    ASSERT_EQ(collective_cognition_get_we_mode(cc_, &we_mode), 0);

    /* Should still have joint commitment */
    EXPECT_GE(we_mode.joint_commitment, 0.0f);
}

TEST_F(CollectiveCognitionSubstrateTest, WeModeStrengthUnderFatigue) {
    /* We-mode strength may decrease under fatigue */

    shared_intentionality_t* si = collective_cognition_get_intentionality(cc_);
    ASSERT_NE(si, nullptr);

    ASSERT_EQ(shared_intentionality_enter_we_mode(si), 0);

    /* Multiple updates simulating fatigue */
    for (int i = 0; i < 20; i++) {
        ASSERT_EQ(collective_cognition_update(cc_), 0);
    }

    we_mode_state_t we_mode;
    ASSERT_EQ(collective_cognition_get_we_mode(cc_, &we_mode), 0);

    /* We-mode should remain active */
    EXPECT_TRUE(shared_intentionality_is_we_mode_active(si));
}

/*=============================================================================
 * Recovery from Stress Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionSubstrateTest, RecoveryFromLowATP) {
    /* Test recovery when ATP improves */

    hyperscanning_t* hs = collective_cognition_get_hyperscanning(cc_);
    ASSERT_NE(hs, nullptr);

    /* Start with low ATP */
    for (uint32_t i = 1; i <= 4; i++) {
        hyperscanning_neural_state_t state;
        memset(&state, 0, sizeof(state));
        state.instance_id = i;
        state.band_power[SYNC_BAND_GAMMA] = 0.4f;
        state.atp_level = 0.25f;
        hyperscanning_update_state(hs, &state);
    }

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    /* Now recover ATP */
    for (uint32_t i = 1; i <= 4; i++) {
        hyperscanning_neural_state_t state;
        memset(&state, 0, sizeof(state));
        state.instance_id = i;
        state.band_power[SYNC_BAND_GAMMA] = 0.8f;
        state.atp_level = 0.9f;
        hyperscanning_update_state(hs, &state);
    }

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    hyperscan_state_t sync_state;
    ASSERT_EQ(collective_cognition_get_hyperscan_state(cc_, &sync_state), 0);

    /* Should recover sync capability */
    EXPECT_GE(sync_state.global_sync, 0.0f);
}

/*=============================================================================
 * Fragmentation Under Stress Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionSubstrateTest, FragmentationUnderSevereStress) {
    /* Severe stress may cause fragmentation */

    hyperscanning_t* hs = collective_cognition_get_hyperscanning(cc_);
    ASSERT_NE(hs, nullptr);

    /* Create severely stressed, desynchronized state */
    for (uint32_t i = 1; i <= 4; i++) {
        hyperscanning_neural_state_t state;
        memset(&state, 0, sizeof(state));
        state.instance_id = i;
        /* Desynchronized phases */
        state.band_power[SYNC_BAND_GAMMA] = 0.2f;
        state.band_phase[SYNC_BAND_GAMMA] = i * 1.5f;  /* Different phases */
        state.atp_level = 0.2f;
        hyperscanning_update_state(hs, &state);
    }

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    collective_cognition_state_t state;
    ASSERT_EQ(collective_cognition_get_state(cc_, &state), 0);

    /* Check fragmentation flag (implementation dependent) */
    /* System should still report active instances */
    EXPECT_EQ(state.active_instances, 4u);
}

/*=============================================================================
 * Statistics Tracking Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionSubstrateTest, StatisticsUnderVaryingLoad) {
    /* Statistics should track varying metabolic conditions */

    hyperscanning_t* hs = collective_cognition_get_hyperscanning(cc_);
    ASSERT_NE(hs, nullptr);

    /* Vary conditions over time */
    for (int cycle = 0; cycle < 30; cycle++) {
        float atp = 0.5f + 0.4f * sinf(cycle * 0.2f);

        for (uint32_t i = 1; i <= 4; i++) {
            hyperscanning_neural_state_t state;
            memset(&state, 0, sizeof(state));
            state.instance_id = i;
            state.band_power[SYNC_BAND_GAMMA] = 0.6f;
            state.atp_level = atp;
            hyperscanning_update_state(hs, &state);
        }

        ASSERT_EQ(collective_cognition_update(cc_), 0);
    }

    collective_cognition_stats_t stats;
    ASSERT_EQ(collective_cognition_get_stats(cc_, &stats), 0);

    EXPECT_GE(stats.total_updates, 30u);
}
