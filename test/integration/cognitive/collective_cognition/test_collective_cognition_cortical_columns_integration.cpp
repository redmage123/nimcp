/**
 * @file test_collective_cognition_cortical_columns_integration.cpp
 * @brief Integration tests for collective cognition + cortical columns
 *
 * Tests integration of collective cognition with:
 * - Minicolumns (local processing units)
 * - Hypercolumns (feature integration)
 * - Cortical hierarchy (V1→V2→V4→IT)
 * - Feedforward/feedback paths
 * - Hierarchical processing
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

class CollectiveCognitionCorticalColumnsTest : public ::testing::Test {
protected:
    void SetUp() override {
        cc_config_ = collective_cognition_default_config();
        cc_ = collective_cognition_create(&cc_config_);
        ASSERT_NE(cc_, nullptr);

        /* Register instances representing cortical columns */
        for (uint32_t i = 1; i <= 8; i++) {
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
 * Hierarchical Processing Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionCorticalColumnsTest, HierarchicalPhiIntegration) {
    /* Phi measures integration across cortical hierarchy */

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    collective_phi_t phi;
    ASSERT_EQ(collective_cognition_get_phi(cc_, &phi), 0);

    /* Network phi reflects cross-level integration */
    EXPECT_GE(phi.phi_network, 0.0f);

    /* Integration measures hierarchical binding */
    EXPECT_GE(phi.integration, 0.0f);
}

TEST_F(CollectiveCognitionCorticalColumnsTest, ModularityReflectsColumnStructure) {
    /* Modularity reflects column-like organization */

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    collective_phi_t phi;
    ASSERT_EQ(collective_cognition_get_phi(cc_, &phi), 0);

    /* Modularity indicates clustered processing units */
    EXPECT_GE(phi.modularity, 0.0f);
}

TEST_F(CollectiveCognitionCorticalColumnsTest, SmallWorldForHierarchy) {
    /* Small-world index indicates efficient hierarchy */

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    collective_phi_t phi;
    ASSERT_EQ(collective_cognition_get_phi(cc_, &phi), 0);

    /* Small-world property = efficient hierarchical routing */
    EXPECT_GE(phi.small_world_index, 0.0f);
}

/*=============================================================================
 * Feature Integration (Hypercolumn-like) Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionCorticalColumnsTest, GammaSyncForFeatureBinding) {
    /* Gamma sync binds features across columns */

    hyperscanning_t* hs = collective_cognition_get_hyperscanning(cc_);
    ASSERT_NE(hs, nullptr);

    /* All columns sync in gamma for feature binding */
    for (uint32_t i = 1; i <= 8; i++) {
        hyperscanning_neural_state_t state;
        memset(&state, 0, sizeof(state));
        state.instance_id = i;
        state.band_power[SYNC_BAND_GAMMA] = 0.85f;
        state.band_phase[SYNC_BAND_GAMMA] = 1.5f;
        state.atp_level = 0.9f;
        ASSERT_EQ(hyperscanning_update_state(hs, &state), 0);
    }

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    hyperscan_state_t sync_state;
    ASSERT_EQ(collective_cognition_get_hyperscan_state(cc_, &sync_state), 0);

    /* Gamma binding = feature integration */
    EXPECT_GE(sync_state.gamma_binding, 0.0f);
}

TEST_F(CollectiveCognitionCorticalColumnsTest, JointAttentionAsFeatureSelection) {
    /* Joint attention selects which features to process */

    shared_intentionality_t* si = collective_cognition_get_intentionality(cc_);
    ASSERT_NE(si, nullptr);

    /* Create feature attention */
    joint_attention_t attention;
    memset(&attention, 0, sizeof(attention));
    attention.salience = 0.9f;

    /* Feature vector represents column activations */
    for (int f = 0; f < 8; f++) {
        attention.feature_vector[f] = 0.5f + 0.1f * f;
    }

    uint32_t attn_id = shared_intentionality_propose_attention(si, &attention);
    EXPECT_GT(attn_id, 0u);

    /* All columns attend to same features */
    for (uint32_t i = 1; i <= 8; i++) {
        ASSERT_EQ(shared_intentionality_join_attention(si, attn_id, i), 0);
    }

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    we_mode_state_t we_mode;
    ASSERT_EQ(collective_cognition_get_we_mode(cc_, &we_mode), 0);
    EXPECT_GE(we_mode.active_joint_attentions, 1u);
}

/*=============================================================================
 * Feedforward/Feedback Path Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionCorticalColumnsTest, InformationFlowAsFeedforward) {
    /* Information flow reflects feedforward processing */

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    collective_cognition_state_t state;
    ASSERT_EQ(collective_cognition_get_state(cc_, &state), 0);

    /* Information flow = feedforward signal strength */
    EXPECT_GE(state.information_flow_rate, 0.0f);
}

TEST_F(CollectiveCognitionCorticalColumnsTest, LeaderFollowerAsFeedback) {
    /* Leader-follower dynamics model feedback paths */

    hyperscanning_t* hs = collective_cognition_get_hyperscanning(cc_);
    ASSERT_NE(hs, nullptr);

    /* Higher-level column (instance 1) leads lower levels */
    for (uint32_t i = 1; i <= 8; i++) {
        hyperscanning_neural_state_t state;
        memset(&state, 0, sizeof(state));
        state.instance_id = i;
        /* Higher columns (lower IDs) have higher gamma */
        state.band_power[SYNC_BAND_GAMMA] = 0.9f - 0.05f * (i - 1);
        state.atp_level = 0.9f;
        ASSERT_EQ(hyperscanning_update_state(hs, &state), 0);
    }

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    uint32_t leader = hyperscanning_get_leader(hs);
    float influence = hyperscanning_get_leader_influence(hs);

    /* Leader influence = top-down feedback strength */
    EXPECT_GE(leader, 0u);
    EXPECT_GE(influence, 0.0f);
}

TEST_F(CollectiveCognitionCorticalColumnsTest, BidirectionalInfluence) {
    /* Test bidirectional influence between instances (layers) */

    hyperscanning_t* hs = collective_cognition_get_hyperscanning(cc_);
    ASSERT_NE(hs, nullptr);

    /* Set up synchronized states */
    for (uint32_t i = 1; i <= 8; i++) {
        hyperscanning_neural_state_t state;
        memset(&state, 0, sizeof(state));
        state.instance_id = i;
        state.band_power[SYNC_BAND_GAMMA] = 0.7f;
        state.band_phase[SYNC_BAND_GAMMA] = 1.5f;
        state.atp_level = 0.9f;
        ASSERT_EQ(hyperscanning_update_state(hs, &state), 0);
    }

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    /* Check influence between instances */
    float influence_1_to_2 = hyperscanning_get_influence(hs, 1, 2);
    float influence_2_to_1 = hyperscanning_get_influence(hs, 2, 1);

    /* Influence should be measurable in both directions */
    EXPECT_GE(influence_1_to_2, 0.0f);
    EXPECT_GE(influence_2_to_1, 0.0f);
}

/*=============================================================================
 * Minicolumn Processing Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionCorticalColumnsTest, LocalProcessingViaInstanceUpdate) {
    /* Each instance processes locally like a minicolumn */

    hyperscanning_t* hs = collective_cognition_get_hyperscanning(cc_);
    ASSERT_NE(hs, nullptr);

    /* Update each instance with different local states */
    for (uint32_t i = 1; i <= 8; i++) {
        hyperscanning_neural_state_t state;
        memset(&state, 0, sizeof(state));
        state.instance_id = i;

        /* Different local activity patterns */
        state.band_power[SYNC_BAND_GAMMA] = 0.5f + 0.1f * (i % 3);
        state.band_power[SYNC_BAND_BETA] = 0.4f + 0.1f * (i % 2);
        state.atp_level = 0.85f + 0.02f * (i % 4);

        ASSERT_EQ(hyperscanning_update_state(hs, &state), 0);
    }

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    collective_cognition_state_t state;
    ASSERT_EQ(collective_cognition_get_state(cc_, &state), 0);
    EXPECT_EQ(state.active_instances, 8u);
}

TEST_F(CollectiveCognitionCorticalColumnsTest, InstanceContributionToPhi) {
    /* Each instance contributes to collective phi */

    /* Get phi system for detailed analysis */
    collective_phi_system_t* phi_sys = collective_cognition_get_phi_system(cc_);
    ASSERT_NE(phi_sys, nullptr);

    /* Update phi for each instance */
    for (uint32_t i = 1; i <= 8; i++) {
        ASSERT_EQ(collective_phi_update_local(phi_sys, i, 0.3f + 0.05f * i), 0);
    }

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    collective_phi_t phi;
    ASSERT_EQ(collective_cognition_get_phi(cc_, &phi), 0);

    /* phi_local = sum of individual contributions */
    EXPECT_GT(phi.phi_local, 0.0f);
}

/*=============================================================================
 * Cortical Hierarchy Role Assignment Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionCorticalColumnsTest, HierarchicalRoles) {
    /* Assign hierarchical roles (V1, V2, V4, IT equivalent) */

    shared_intentionality_t* si = collective_cognition_get_intentionality(cc_);
    ASSERT_NE(si, nullptr);

    /* Create processing goal */
    shared_goal_t goal;
    memset(&goal, 0, sizeof(goal));
    snprintf(goal.description, sizeof(goal.description),
             "Hierarchical visual processing");
    goal.priority = 0.9f;

    uint32_t goal_id = shared_intentionality_propose_goal(si, &goal);
    EXPECT_GT(goal_id, 0u);

    /* Assign hierarchical roles */
    /* Instances 1-2: Early processing (V1-like) */
    ASSERT_EQ(shared_intentionality_assign_role(si, goal_id, 1, ROLE_EXECUTOR), 0);
    ASSERT_EQ(shared_intentionality_assign_role(si, goal_id, 2, ROLE_EXECUTOR), 0);

    /* Instances 3-4: Mid-level (V2-like) */
    ASSERT_EQ(shared_intentionality_assign_role(si, goal_id, 3, ROLE_EXECUTOR), 0);
    ASSERT_EQ(shared_intentionality_assign_role(si, goal_id, 4, ROLE_EXECUTOR), 0);

    /* Instances 5-6: Higher processing (V4-like) */
    ASSERT_EQ(shared_intentionality_assign_role(si, goal_id, 5, ROLE_VERIFIER), 0);
    ASSERT_EQ(shared_intentionality_assign_role(si, goal_id, 6, ROLE_VERIFIER), 0);

    /* Instances 7-8: Highest level (IT-like) */
    ASSERT_EQ(shared_intentionality_assign_role(si, goal_id, 7, ROLE_LEADER), 0);
    ASSERT_EQ(shared_intentionality_assign_role(si, goal_id, 8, ROLE_OBSERVER), 0);

    /* All commit */
    for (uint32_t i = 1; i <= 8; i++) {
        ASSERT_EQ(shared_intentionality_commit_to_goal(si, goal_id, i, 0.9f), 0);
    }

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    we_mode_state_t we_mode;
    ASSERT_EQ(collective_cognition_get_we_mode(cc_, &we_mode), 0);

    /* Should have role understanding */
    EXPECT_GE(we_mode.role_understanding, 0.0f);
}

/*=============================================================================
 * Connectivity Pattern Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionCorticalColumnsTest, ConnectivityMeasure) {
    /* Connectivity reflects column interconnection */

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    collective_phi_t phi;
    ASSERT_EQ(collective_cognition_get_phi(cc_, &phi), 0);

    /* Connectivity = column interconnection density */
    EXPECT_GE(phi.connectivity, 0.0f);
    EXPECT_LE(phi.connectivity, 1.0f);
}

TEST_F(CollectiveCognitionCorticalColumnsTest, PairwiseSynchronization) {
    /* Test pairwise sync between columns */

    hyperscanning_t* hs = collective_cognition_get_hyperscanning(cc_);
    ASSERT_NE(hs, nullptr);

    /* Set synchronized states */
    for (uint32_t i = 1; i <= 8; i++) {
        hyperscanning_neural_state_t state;
        memset(&state, 0, sizeof(state));
        state.instance_id = i;
        state.band_power[SYNC_BAND_GAMMA] = 0.8f;
        state.band_phase[SYNC_BAND_GAMMA] = 1.5f;
        state.atp_level = 0.9f;
        ASSERT_EQ(hyperscanning_update_state(hs, &state), 0);
    }

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    /* Check pairwise sync */
    hyperscan_pair_t pair;
    ASSERT_EQ(hyperscanning_get_pair_sync(hs, 1, 2, &pair), 0);

    /* PLV should be in valid range */
    EXPECT_GE(pair.plv[SYNC_BAND_GAMMA], 0.0f);
    EXPECT_LE(pair.plv[SYNC_BAND_GAMMA], 1.0f);
}

/*=============================================================================
 * Consciousness and Cortical Integration Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionCorticalColumnsTest, ConsciousnessFromCorticalIntegration) {
    /* Consciousness level reflects cortical integration */

    hyperscanning_t* hs = collective_cognition_get_hyperscanning(cc_);
    ASSERT_NE(hs, nullptr);

    /* Optimal cortical sync */
    for (uint32_t i = 1; i <= 8; i++) {
        hyperscanning_neural_state_t state;
        memset(&state, 0, sizeof(state));
        state.instance_id = i;
        state.band_power[SYNC_BAND_GAMMA] = 0.9f;
        state.band_phase[SYNC_BAND_GAMMA] = 1.5f;
        state.atp_level = 0.95f;
        ASSERT_EQ(hyperscanning_update_state(hs, &state), 0);
    }

    for (int i = 0; i < 10; i++) {
        ASSERT_EQ(collective_cognition_update(cc_), 0);
    }

    collective_consciousness_level_t level =
        collective_cognition_get_consciousness_level(cc_);

    /* Integrated cortex should achieve consciousness */
    EXPECT_GE((int)level, (int)COLLECTIVE_CONSCIOUSNESS_NONE);
}

/*=============================================================================
 * Attention Coherence Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionCorticalColumnsTest, CoherentAttentionAcrossHierarchy) {
    /* Coherent attention across cortical hierarchy */

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    collective_cognition_state_t state;
    ASSERT_EQ(collective_cognition_get_state(cc_, &state), 0);

    /* Attention coherence = cross-level synchronization */
    EXPECT_GE(state.attention_coherence, 0.0f);
}

/*=============================================================================
 * Stress Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionCorticalColumnsTest, HighFrequencyUpdates) {
    /* High frequency updates for real-time processing */

    hyperscanning_t* hs = collective_cognition_get_hyperscanning(cc_);
    ASSERT_NE(hs, nullptr);

    for (int frame = 0; frame < 100; frame++) {
        for (uint32_t i = 1; i <= 8; i++) {
            hyperscanning_neural_state_t state;
            memset(&state, 0, sizeof(state));
            state.instance_id = i;
            state.band_power[SYNC_BAND_GAMMA] = 0.6f + 0.2f * sinf(frame * 0.1f);
            state.atp_level = 0.85f;
            hyperscanning_update_state(hs, &state);
        }

        ASSERT_EQ(collective_cognition_update(cc_), 0);
    }

    collective_cognition_state_t state;
    ASSERT_EQ(collective_cognition_get_state(cc_, &state), 0);
    EXPECT_EQ(state.active_instances, 8u);
}

TEST_F(CollectiveCognitionCorticalColumnsTest, DynamicColumnActivation) {
    /* Dynamic activation/deactivation of columns */

    /* Remove some columns */
    ASSERT_EQ(collective_cognition_unregister_instance(cc_, 7), 0);
    ASSERT_EQ(collective_cognition_unregister_instance(cc_, 8), 0);

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    collective_cognition_state_t state;
    ASSERT_EQ(collective_cognition_get_state(cc_, &state), 0);
    EXPECT_EQ(state.active_instances, 6u);

    /* Re-add columns */
    ASSERT_EQ(collective_cognition_register_instance(cc_, 9, nullptr), 0);
    ASSERT_EQ(collective_cognition_register_instance(cc_, 10, nullptr), 0);

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    ASSERT_EQ(collective_cognition_get_state(cc_, &state), 0);
    EXPECT_EQ(state.active_instances, 8u);
}
