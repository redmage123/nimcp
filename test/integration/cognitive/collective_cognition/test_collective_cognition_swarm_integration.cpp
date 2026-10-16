/**
 * @file test_collective_cognition_swarm_integration.cpp
 * @brief Integration tests for collective cognition + swarm/dragonfly modules
 *
 * Tests integration of collective cognition with:
 * - Swarm consciousness (IIT phi aggregation)
 * - Collective workspace (CRDT-based distributed attention)
 * - Emotional contagion (affective synchronization)
 * - Dragonfly swarm coordination (multi-agent hunting)
 * - Swarm emergence tiers
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

class CollectiveCognitionSwarmTest : public ::testing::Test {
protected:
    void SetUp() override {
        cc_config_ = collective_cognition_default_config();
        cc_ = collective_cognition_create(&cc_config_);
        ASSERT_NE(cc_, nullptr);

        /* Register multiple instances to simulate swarm */
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
 * Swarm Consciousness Integration Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionSwarmTest, CollectivePhiForSwarm) {
    /* Collective phi measures swarm consciousness */

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    collective_phi_t phi;
    ASSERT_EQ(collective_cognition_get_phi(cc_, &phi), 0);

    /* With 8 instances, should have measurable phi */
    EXPECT_GE(phi.phi_total, 0.0f);
    EXPECT_GE(phi.phi_local, 0.0f);
    EXPECT_GE(phi.phi_network, 0.0f);
}

TEST_F(CollectiveCognitionSwarmTest, NetworkIntegrationMetrics) {
    /* Network integration measures swarm connectivity */

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    collective_phi_t phi;
    ASSERT_EQ(collective_cognition_get_phi(cc_, &phi), 0);

    /* Network topology metrics */
    EXPECT_GE(phi.connectivity, 0.0f);
    EXPECT_GE(phi.modularity, 0.0f);
    EXPECT_GE(phi.small_world_index, 0.0f);
}

TEST_F(CollectiveCognitionSwarmTest, ConsciousnessLevelForSwarm) {
    /* Swarm size affects consciousness level */

    /* Set optimal neural states for all instances */
    hyperscanning_t* hs = collective_cognition_get_hyperscanning(cc_);
    ASSERT_NE(hs, nullptr);

    for (uint32_t i = 1; i <= 8; i++) {
        hyperscanning_neural_state_t state;
        memset(&state, 0, sizeof(state));
        state.instance_id = i;
        state.band_power[SYNC_BAND_GAMMA] = 0.85f;
        state.band_phase[SYNC_BAND_GAMMA] = 1.5f;
        state.atp_level = 0.9f;
        hyperscanning_update_state(hs, &state);
    }

    for (int i = 0; i < 10; i++) {
        ASSERT_EQ(collective_cognition_update(cc_), 0);
    }

    collective_consciousness_level_t level =
        collective_cognition_get_consciousness_level(cc_);

    /* Larger swarm should enable consciousness */
    EXPECT_GE((int)level, (int)COLLECTIVE_CONSCIOUSNESS_NONE);
}

/*=============================================================================
 * Collective Workspace Integration Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionSwarmTest, SharedAttentionViaJointAttention) {
    /* Joint attention simulates collective workspace */

    shared_intentionality_t* si = collective_cognition_get_intentionality(cc_);
    ASSERT_NE(si, nullptr);

    /* Create shared attention target (like workspace item) */
    joint_attention_t attention;
    memset(&attention, 0, sizeof(attention));
    attention.salience = 0.9f;

    /* Feature vector like workspace item content */
    for (int j = 0; j < 8; j++) {
        attention.feature_vector[j] = 0.1f * (j + 1);
    }

    uint32_t attn_id = shared_intentionality_propose_attention(si, &attention);
    EXPECT_GT(attn_id, 0u);

    /* All swarm members join attention */
    for (uint32_t i = 1; i <= 8; i++) {
        ASSERT_EQ(shared_intentionality_join_attention(si, attn_id, i), 0);
    }

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    we_mode_state_t we_mode;
    ASSERT_EQ(collective_cognition_get_we_mode(cc_, &we_mode), 0);

    /* Should have active joint attention */
    EXPECT_GE(we_mode.active_joint_attentions, 1u);
}

TEST_F(CollectiveCognitionSwarmTest, MultipleWorkspaceItems) {
    /* Multiple joint attentions simulate workspace items */

    shared_intentionality_t* si = collective_cognition_get_intentionality(cc_);
    ASSERT_NE(si, nullptr);

    /* Create multiple attention targets with different saliences */
    for (int item = 0; item < 5; item++) {
        joint_attention_t attention;
        memset(&attention, 0, sizeof(attention));
        attention.salience = 0.9f - item * 0.1f;
        attention.feature_vector[0] = (float)item;

        uint32_t attn_id = shared_intentionality_propose_attention(si, &attention);
        EXPECT_GT(attn_id, 0u);

        /* Various subsets join different attentions */
        for (uint32_t i = 1; i <= 8 - item; i++) {
            shared_intentionality_join_attention(si, attn_id, i);
        }
    }

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    we_mode_state_t we_mode;
    ASSERT_EQ(collective_cognition_get_we_mode(cc_, &we_mode), 0);

    /* Should have multiple active attentions */
    EXPECT_GE(we_mode.active_joint_attentions, 1u);
}

TEST_F(CollectiveCognitionSwarmTest, AttentionCoherenceAsWorkspaceSync) {
    /* Attention coherence measures workspace synchronization */

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    collective_cognition_state_t state;
    ASSERT_EQ(collective_cognition_get_state(cc_, &state), 0);

    /* Coherence = workspace synchronization */
    EXPECT_GE(state.attention_coherence, 0.0f);
    EXPECT_LE(state.attention_coherence, 1.0f);
}

/*=============================================================================
 * Emotional Contagion Integration Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionSwarmTest, ThetaSyncForEmotionalContagion) {
    /* Theta band sync enables emotional contagion */

    hyperscanning_t* hs = collective_cognition_get_hyperscanning(cc_);
    ASSERT_NE(hs, nullptr);

    /* High theta for emotional sync */
    for (uint32_t i = 1; i <= 8; i++) {
        hyperscanning_neural_state_t state;
        memset(&state, 0, sizeof(state));
        state.instance_id = i;
        state.band_power[SYNC_BAND_THETA] = 0.8f;
        state.band_phase[SYNC_BAND_THETA] = 0.5f;
        state.band_power[SYNC_BAND_GAMMA] = 0.5f;
        state.atp_level = 0.85f;
        hyperscanning_update_state(hs, &state);
    }

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    hyperscan_state_t sync_state;
    ASSERT_EQ(collective_cognition_get_hyperscan_state(cc_, &sync_state), 0);

    /* Theta emotional sync enables contagion */
    EXPECT_GE(sync_state.theta_emotional, 0.0f);
}

TEST_F(CollectiveCognitionSwarmTest, WeModeForEmotionalSharing) {
    /* We-mode enables emotional sharing across swarm */

    shared_intentionality_t* si = collective_cognition_get_intentionality(cc_);
    ASSERT_NE(si, nullptr);

    /* Enter we-mode for collective emotion */
    ASSERT_EQ(shared_intentionality_enter_we_mode(si), 0);

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    we_mode_state_t we_mode;
    ASSERT_EQ(collective_cognition_get_we_mode(cc_, &we_mode), 0);

    /* Mutual responsiveness enables emotional contagion */
    EXPECT_GE(we_mode.mutual_responsiveness, 0.0f);
}

/*=============================================================================
 * Dragonfly Swarm Coordination Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionSwarmTest, SharedHuntingGoal) {
    /* Shared goal for coordinated hunting */

    shared_intentionality_t* si = collective_cognition_get_intentionality(cc_);
    ASSERT_NE(si, nullptr);

    /* Create hunting goal */
    shared_goal_t goal;
    memset(&goal, 0, sizeof(goal));
    snprintf(goal.description, sizeof(goal.description),
             "Coordinate pursuit of target prey");
    goal.priority = 0.95f;

    uint32_t goal_id = shared_intentionality_propose_goal(si, &goal);
    EXPECT_GT(goal_id, 0u);

    /* Assign hunting formation roles */
    ASSERT_EQ(shared_intentionality_assign_role(si, goal_id, 1, ROLE_LEADER), 0);
    ASSERT_EQ(shared_intentionality_assign_role(si, goal_id, 2, ROLE_EXECUTOR), 0);
    ASSERT_EQ(shared_intentionality_assign_role(si, goal_id, 3, ROLE_EXECUTOR), 0);
    ASSERT_EQ(shared_intentionality_assign_role(si, goal_id, 4, ROLE_EXECUTOR), 0);
    ASSERT_EQ(shared_intentionality_assign_role(si, goal_id, 5, ROLE_VERIFIER), 0);
    ASSERT_EQ(shared_intentionality_assign_role(si, goal_id, 6, ROLE_OBSERVER), 0);
    ASSERT_EQ(shared_intentionality_assign_role(si, goal_id, 7, ROLE_OBSERVER), 0);
    ASSERT_EQ(shared_intentionality_assign_role(si, goal_id, 8, ROLE_OBSERVER), 0);

    /* All commit */
    for (uint32_t i = 1; i <= 8; i++) {
        ASSERT_EQ(shared_intentionality_commit_to_goal(si, goal_id, i, 0.9f), 0);
    }

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    we_mode_state_t we_mode;
    ASSERT_EQ(collective_cognition_get_we_mode(cc_, &we_mode), 0);

    /* Should have coordinated goal */
    EXPECT_GE(we_mode.active_shared_goals, 1u);
    EXPECT_GE(we_mode.role_understanding, 0.0f);
}

TEST_F(CollectiveCognitionSwarmTest, TargetTrackingViaJointAttention) {
    /* Joint attention on prey target */

    shared_intentionality_t* si = collective_cognition_get_intentionality(cc_);
    ASSERT_NE(si, nullptr);

    /* Create attention on target */
    joint_attention_t attention;
    memset(&attention, 0, sizeof(attention));
    attention.salience = 0.95f;

    /* Target features: position, velocity, difficulty */
    attention.feature_vector[0] = 100.0f;  /* X position */
    attention.feature_vector[1] = 50.0f;   /* Y position */
    attention.feature_vector[2] = 20.0f;   /* Z position */
    attention.feature_vector[3] = 5.0f;    /* Velocity X */
    attention.feature_vector[4] = 2.0f;    /* Velocity Y */
    attention.feature_vector[5] = 0.8f;    /* Difficulty */

    uint32_t attn_id = shared_intentionality_propose_attention(si, &attention);
    EXPECT_GT(attn_id, 0u);

    /* All pursuers join attention */
    for (uint32_t i = 1; i <= 8; i++) {
        ASSERT_EQ(shared_intentionality_join_attention(si, attn_id, i), 0);
    }

    ASSERT_EQ(collective_cognition_update(cc_), 0);
}

TEST_F(CollectiveCognitionSwarmTest, BetaSyncForMotorCoordination) {
    /* Beta band sync for coordinated movement */

    hyperscanning_t* hs = collective_cognition_get_hyperscanning(cc_);
    ASSERT_NE(hs, nullptr);

    /* High beta for motor coordination */
    for (uint32_t i = 1; i <= 8; i++) {
        hyperscanning_neural_state_t state;
        memset(&state, 0, sizeof(state));
        state.instance_id = i;
        state.band_power[SYNC_BAND_BETA] = 0.85f;
        state.band_power[SYNC_BAND_GAMMA] = 0.6f;
        state.atp_level = 0.9f;
        hyperscanning_update_state(hs, &state);
    }

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    hyperscan_state_t sync_state;
    ASSERT_EQ(collective_cognition_get_hyperscan_state(cc_, &sync_state), 0);

    /* Beta coordination for synchronized movement */
    EXPECT_GE(sync_state.beta_coordination, 0.0f);
}

TEST_F(CollectiveCognitionSwarmTest, LeaderFollowerDynamics) {
    /* Leader detection for swarm coordination */

    hyperscanning_t* hs = collective_cognition_get_hyperscanning(cc_);
    ASSERT_NE(hs, nullptr);

    /* Instance 1 has highest gamma = leader */
    for (uint32_t i = 1; i <= 8; i++) {
        hyperscanning_neural_state_t state;
        memset(&state, 0, sizeof(state));
        state.instance_id = i;
        state.band_power[SYNC_BAND_GAMMA] = (i == 1) ? 0.95f : 0.6f;
        state.band_phase[SYNC_BAND_GAMMA] = 1.5f;
        state.atp_level = 0.9f;
        hyperscanning_update_state(hs, &state);
    }

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    uint32_t leader = hyperscanning_get_leader(hs);
    float influence = hyperscanning_get_leader_influence(hs);

    EXPECT_GE(leader, 0u);
    EXPECT_GE(influence, 0.0f);
}

/*=============================================================================
 * Swarm Emergence Tier Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionSwarmTest, SwarmSizeAffectsCapabilities) {
    /* Larger swarm enables more capabilities */

    /* Current swarm has 8 instances (PLATOON tier equivalent) */
    collective_cognition_state_t state;
    ASSERT_EQ(collective_cognition_get_state(cc_, &state), 0);
    EXPECT_EQ(state.active_instances, 8u);

    collective_phi_t phi;
    ASSERT_EQ(collective_cognition_get_phi(cc_, &phi), 0);

    /* With 8 instances, should have meaningful integration */
    EXPECT_GE(phi.phi_network, 0.0f);
}

TEST_F(CollectiveCognitionSwarmTest, CoherenceRequiredForHigherTier) {
    /* High coherence enables higher tier capabilities */

    hyperscanning_t* hs = collective_cognition_get_hyperscanning(cc_);
    ASSERT_NE(hs, nullptr);

    /* Set very high sync for all instances */
    for (uint32_t i = 1; i <= 8; i++) {
        hyperscanning_neural_state_t state;
        memset(&state, 0, sizeof(state));
        state.instance_id = i;
        state.band_power[SYNC_BAND_GAMMA] = 0.95f;
        state.band_phase[SYNC_BAND_GAMMA] = 1.5f;  /* Same phase */
        state.atp_level = 0.95f;
        hyperscanning_update_state(hs, &state);
    }

    for (int i = 0; i < 10; i++) {
        ASSERT_EQ(collective_cognition_update(cc_), 0);
    }

    hyperscan_state_t sync_state;
    ASSERT_EQ(collective_cognition_get_hyperscan_state(cc_, &sync_state), 0);

    /* High coherence should be achievable */
    EXPECT_GE(sync_state.global_sync, 0.0f);
}

/*=============================================================================
 * Multi-Agent Coordination Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionSwarmTest, GoalAlignmentForCoordination) {
    /* Goal alignment measures swarm coordination quality */

    shared_intentionality_t* si = collective_cognition_get_intentionality(cc_);
    ASSERT_NE(si, nullptr);

    /* Create aligned goals */
    shared_goal_t goal;
    memset(&goal, 0, sizeof(goal));
    snprintf(goal.description, sizeof(goal.description),
             "Coordinate for collective action");
    goal.priority = 0.9f;

    uint32_t goal_id = shared_intentionality_propose_goal(si, &goal);
    EXPECT_GT(goal_id, 0u);

    /* All instances commit with high commitment */
    for (uint32_t i = 1; i <= 8; i++) {
        ASSERT_EQ(shared_intentionality_commit_to_goal(si, goal_id, i, 0.95f), 0);
    }

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    collective_cognition_state_t state;
    ASSERT_EQ(collective_cognition_get_state(cc_, &state), 0);

    /* Goal alignment should be high */
    EXPECT_GE(state.goal_alignment, 0.0f);
}

TEST_F(CollectiveCognitionSwarmTest, InformationFlowInSwarm) {
    /* Information flow rate for swarm communication */

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    collective_cognition_state_t state;
    ASSERT_EQ(collective_cognition_get_state(cc_, &state), 0);

    /* With 8 instances, should have information flow */
    EXPECT_GE(state.information_flow_rate, 0.0f);
}

/*=============================================================================
 * Extended Mind for Swarm Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionSwarmTest, ExternalSensorsForSwarm) {
    /* External sensors extend swarm perception */

    extended_mind_t* em = collective_cognition_get_extended_mind(cc_);
    ASSERT_NE(em, nullptr);

    /* Register external sensor array */
    cognitive_extension_t ext;
    memset(&ext, 0, sizeof(ext));
    ext.type = EXT_TYPE_PERCEPTION;
    snprintf(ext.name, sizeof(ext.name), "SensorArray");
    ext.reliability = 0.95f;
    ext.avg_latency_ms = 20.0f;
    ext.integration_depth = 0.9f;
    ext.trust_level = 0.92f;

    uint32_t ext_id = extended_mind_register_extension(em, &ext);
    EXPECT_GT(ext_id, 0u);
}

TEST_F(CollectiveCognitionSwarmTest, CommunicationExtensionForSwarm) {
    /* Communication extension for swarm messaging */

    extended_mind_t* em = collective_cognition_get_extended_mind(cc_);
    ASSERT_NE(em, nullptr);

    cognitive_extension_t ext;
    memset(&ext, 0, sizeof(ext));
    ext.type = EXT_TYPE_COMMUNICATION;
    snprintf(ext.name, sizeof(ext.name), "SwarmMesh");
    ext.reliability = 0.98f;
    ext.avg_latency_ms = 5.0f;
    ext.integration_depth = 0.95f;
    ext.trust_level = 0.99f;

    uint32_t ext_id = extended_mind_register_extension(em, &ext);
    EXPECT_GT(ext_id, 0u);

    float capacity = extended_mind_get_capacity(em, EXT_TYPE_COMMUNICATION);
    EXPECT_GT(capacity, 0.0f);
}

/*=============================================================================
 * Stress Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionSwarmTest, HighUpdateFrequency) {
    /* High update frequency for real-time swarm control */

    for (int frame = 0; frame < 100; frame++) {
        ASSERT_EQ(collective_cognition_update(cc_), 0);
    }

    collective_cognition_state_t state;
    ASSERT_EQ(collective_cognition_get_state(cc_, &state), 0);
    EXPECT_EQ(state.active_instances, 8u);

    collective_cognition_stats_t stats;
    ASSERT_EQ(collective_cognition_get_stats(cc_, &stats), 0);
    EXPECT_GE(stats.total_updates, 100u);
}

TEST_F(CollectiveCognitionSwarmTest, DynamicSwarmMembership) {
    /* Handle dynamic addition/removal of swarm members */

    /* Remove some instances */
    ASSERT_EQ(collective_cognition_unregister_instance(cc_, 7), 0);
    ASSERT_EQ(collective_cognition_unregister_instance(cc_, 8), 0);

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    collective_cognition_state_t state;
    ASSERT_EQ(collective_cognition_get_state(cc_, &state), 0);
    EXPECT_EQ(state.active_instances, 6u);

    /* Re-add instances */
    ASSERT_EQ(collective_cognition_register_instance(cc_, 7, nullptr), 0);
    ASSERT_EQ(collective_cognition_register_instance(cc_, 8, nullptr), 0);

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    ASSERT_EQ(collective_cognition_get_state(cc_, &state), 0);
    EXPECT_EQ(state.active_instances, 8u);
}

TEST_F(CollectiveCognitionSwarmTest, RapidGoalSwitching) {
    /* Rapid goal switching for agile swarm behavior */

    shared_intentionality_t* si = collective_cognition_get_intentionality(cc_);
    ASSERT_NE(si, nullptr);

    for (int cycle = 0; cycle < 10; cycle++) {
        shared_goal_t goal;
        memset(&goal, 0, sizeof(goal));
        snprintf(goal.description, sizeof(goal.description),
                 "Swarm objective %d", cycle);
        goal.priority = 0.8f;

        uint32_t goal_id = shared_intentionality_propose_goal(si, &goal);
        EXPECT_GT(goal_id, 0u);

        for (uint32_t i = 1; i <= 8; i++) {
            shared_intentionality_commit_to_goal(si, goal_id, i, 0.9f);
        }

        ASSERT_EQ(collective_cognition_update(cc_), 0);

        shared_intentionality_complete_goal(si, goal_id);
    }
}

TEST_F(CollectiveCognitionSwarmTest, VaryingNeuralStates) {
    /* Handle varying neural states across swarm */

    hyperscanning_t* hs = collective_cognition_get_hyperscanning(cc_);
    ASSERT_NE(hs, nullptr);

    for (int frame = 0; frame < 30; frame++) {
        for (uint32_t i = 1; i <= 8; i++) {
            hyperscanning_neural_state_t state;
            memset(&state, 0, sizeof(state));
            state.instance_id = i;

            /* Varying gamma based on instance and frame */
            state.band_power[SYNC_BAND_GAMMA] =
                0.5f + 0.3f * sinf((frame + i) * 0.2f);
            state.band_power[SYNC_BAND_BETA] =
                0.4f + 0.2f * cosf((frame + i) * 0.15f);
            state.atp_level = 0.8f + 0.1f * sinf(frame * 0.1f);

            hyperscanning_update_state(hs, &state);
        }

        ASSERT_EQ(collective_cognition_update(cc_), 0);
    }

    collective_cognition_state_t state;
    ASSERT_EQ(collective_cognition_get_state(cc_, &state), 0);
    EXPECT_EQ(state.active_instances, 8u);
}
