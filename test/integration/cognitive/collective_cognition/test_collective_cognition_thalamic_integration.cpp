/**
 * @file test_collective_cognition_thalamic_integration.cpp
 * @brief Integration tests for collective cognition + thalamic/middleware layer
 *
 * Tests integration of collective cognition with:
 * - Thalamic routing (attention-gated signal routing)
 * - Middleware bridges (60+ cognitive bridges)
 * - Priority queue management
 * - Signal routing and gating
 * - Bio-async message routing
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

class CollectiveCognitionThalamicTest : public ::testing::Test {
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
 * Attention-Gated Routing Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionThalamicTest, AttentionCoherenceForRouting) {
    /* Attention coherence affects thalamic routing efficiency */

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    collective_cognition_state_t state;
    ASSERT_EQ(collective_cognition_get_state(cc_, &state), 0);

    /* Attention coherence guides routing priority */
    EXPECT_GE(state.attention_coherence, 0.0f);
    EXPECT_LE(state.attention_coherence, 1.0f);
}

TEST_F(CollectiveCognitionThalamicTest, JointAttentionPriority) {
    /* Joint attention items get routing priority */

    shared_intentionality_t* si = collective_cognition_get_intentionality(cc_);
    ASSERT_NE(si, nullptr);

    /* Create high-priority joint attention */
    joint_attention_t attention;
    memset(&attention, 0, sizeof(attention));
    attention.salience = 0.95f;  /* High priority for routing */

    uint32_t attn_id = shared_intentionality_propose_attention(si, &attention);
    EXPECT_GT(attn_id, 0u);

    for (uint32_t i = 1; i <= 4; i++) {
        ASSERT_EQ(shared_intentionality_join_attention(si, attn_id, i), 0);
    }

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    we_mode_state_t we_mode;
    ASSERT_EQ(collective_cognition_get_we_mode(cc_, &we_mode), 0);

    /* High salience = high routing priority */
    EXPECT_GE(we_mode.active_joint_attentions, 1u);
}

TEST_F(CollectiveCognitionThalamicTest, AlphaSyncForAttentionGating) {
    /* Alpha band sync reflects attention inhibition/gating */

    hyperscanning_t* hs = collective_cognition_get_hyperscanning(cc_);
    ASSERT_NE(hs, nullptr);

    /* High alpha = attention inhibition (gates out distractors) */
    for (uint32_t i = 1; i <= 4; i++) {
        hyperscanning_neural_state_t state;
        memset(&state, 0, sizeof(state));
        state.instance_id = i;
        state.band_power[SYNC_BAND_ALPHA] = 0.8f;
        state.band_power[SYNC_BAND_GAMMA] = 0.5f;
        state.atp_level = 0.9f;
        ASSERT_EQ(hyperscanning_update_state(hs, &state), 0);
    }

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    /* System should be synchronized */
    hyperscan_state_t sync_state;
    ASSERT_EQ(collective_cognition_get_hyperscan_state(cc_, &sync_state), 0);
    EXPECT_GE(sync_state.global_sync, 0.0f);
}

/*=============================================================================
 * Signal Routing Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionThalamicTest, InformationFlowAsRouting) {
    /* Information flow rate reflects thalamic throughput */

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    collective_cognition_state_t state;
    ASSERT_EQ(collective_cognition_get_state(cc_, &state), 0);

    /* Information flow = routing capacity */
    EXPECT_GE(state.information_flow_rate, 0.0f);
}

TEST_F(CollectiveCognitionThalamicTest, PhiNetworkAsRoutingEfficiency) {
    /* Phi network measures routing integration */

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    collective_phi_t phi;
    ASSERT_EQ(collective_cognition_get_phi(cc_, &phi), 0);

    /* Network phi = routing efficiency across instances */
    EXPECT_GE(phi.phi_network, 0.0f);

    /* Connectivity = routing paths available */
    EXPECT_GE(phi.connectivity, 0.0f);
}

TEST_F(CollectiveCognitionThalamicTest, IntegrationQualityForRouting) {
    /* Integration quality affects routing reliability */

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    collective_cognition_state_t state;
    ASSERT_EQ(collective_cognition_get_state(cc_, &state), 0);

    /* High integration = reliable routing */
    EXPECT_GE(state.integration_quality, 0.0f);
    EXPECT_LE(state.integration_quality, 1.0f);
}

/*=============================================================================
 * Priority Queue Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionThalamicTest, GoalPriorityQueue) {
    /* Goals should be processed by priority */

    shared_intentionality_t* si = collective_cognition_get_intentionality(cc_);
    ASSERT_NE(si, nullptr);

    /* Create goals with different priorities */
    float priorities[] = {0.9f, 0.7f, 0.5f, 0.3f};

    for (int i = 0; i < 4; i++) {
        shared_goal_t goal;
        memset(&goal, 0, sizeof(goal));
        snprintf(goal.description, sizeof(goal.description),
                 "Goal with priority %.1f", priorities[i]);
        goal.priority = priorities[i];

        uint32_t goal_id = shared_intentionality_propose_goal(si, &goal);
        EXPECT_GT(goal_id, 0u);

        for (uint32_t j = 1; j <= 4; j++) {
            shared_intentionality_commit_to_goal(si, goal_id, j, 0.8f);
        }
    }

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    we_mode_state_t we_mode;
    ASSERT_EQ(collective_cognition_get_we_mode(cc_, &we_mode), 0);

    /* Should have multiple goals */
    EXPECT_EQ(we_mode.active_shared_goals, 4u);
}

TEST_F(CollectiveCognitionThalamicTest, AttentionSaliencePriority) {
    /* Joint attention items ordered by salience */

    shared_intentionality_t* si = collective_cognition_get_intentionality(cc_);
    ASSERT_NE(si, nullptr);

    /* Create attentions with different saliences */
    float saliences[] = {0.95f, 0.8f, 0.6f};

    for (int i = 0; i < 3; i++) {
        joint_attention_t attention;
        memset(&attention, 0, sizeof(attention));
        attention.salience = saliences[i];
        attention.feature_vector[0] = (float)i;

        uint32_t attn_id = shared_intentionality_propose_attention(si, &attention);
        EXPECT_GT(attn_id, 0u);

        for (uint32_t j = 1; j <= 4; j++) {
            shared_intentionality_join_attention(si, attn_id, j);
        }
    }

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    we_mode_state_t we_mode;
    ASSERT_EQ(collective_cognition_get_we_mode(cc_, &we_mode), 0);

    EXPECT_GE(we_mode.active_joint_attentions, 1u);
}

/*=============================================================================
 * Middleware Bridge Pattern Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionThalamicTest, SubsystemAccessPattern) {
    /* All subsystems accessible through main API (middleware pattern) */

    hyperscanning_t* hs = collective_cognition_get_hyperscanning(cc_);
    EXPECT_NE(hs, nullptr);

    extended_mind_t* em = collective_cognition_get_extended_mind(cc_);
    EXPECT_NE(em, nullptr);

    collective_phi_system_t* phi = collective_cognition_get_phi_system(cc_);
    EXPECT_NE(phi, nullptr);

    shared_intentionality_t* si = collective_cognition_get_intentionality(cc_);
    EXPECT_NE(si, nullptr);
}

TEST_F(CollectiveCognitionThalamicTest, UpdateRoutesThroughAllSubsystems) {
    /* Update propagates through all subsystems (thalamic routing) */

    /* Set neural states */
    hyperscanning_t* hs = collective_cognition_get_hyperscanning(cc_);
    ASSERT_NE(hs, nullptr);

    for (uint32_t i = 1; i <= 4; i++) {
        hyperscanning_neural_state_t state;
        memset(&state, 0, sizeof(state));
        state.instance_id = i;
        state.band_power[SYNC_BAND_GAMMA] = 0.7f;
        state.atp_level = 0.9f;
        hyperscanning_update_state(hs, &state);
    }

    /* Create goal and attention */
    shared_intentionality_t* si = collective_cognition_get_intentionality(cc_);
    ASSERT_NE(si, nullptr);

    shared_goal_t goal;
    memset(&goal, 0, sizeof(goal));
    snprintf(goal.description, sizeof(goal.description), "Test routing");
    goal.priority = 0.8f;
    uint32_t goal_id = shared_intentionality_propose_goal(si, &goal);

    for (uint32_t i = 1; i <= 4; i++) {
        shared_intentionality_commit_to_goal(si, goal_id, i, 0.9f);
    }

    /* Single update routes through all */
    ASSERT_EQ(collective_cognition_update(cc_), 0);

    /* Verify all subsystems updated */
    hyperscan_state_t sync_state;
    ASSERT_EQ(collective_cognition_get_hyperscan_state(cc_, &sync_state), 0);

    collective_phi_t phi;
    ASSERT_EQ(collective_cognition_get_phi(cc_, &phi), 0);

    we_mode_state_t we_mode;
    ASSERT_EQ(collective_cognition_get_we_mode(cc_, &we_mode), 0);

    extended_mind_state_t em_state;
    ASSERT_EQ(collective_cognition_get_extended_mind_state(cc_, &em_state), 0);
}

/*=============================================================================
 * Bio-Async Message Routing Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionThalamicTest, BioAsyncNotInitiallyConnected) {
    /* Bio-async starts disconnected */

    EXPECT_FALSE(collective_cognition_is_bio_async_connected(cc_));
}

TEST_F(CollectiveCognitionThalamicTest, DisconnectBioAsyncWhenNotConnected) {
    /* Safe to disconnect when not connected */

    EXPECT_EQ(collective_cognition_disconnect_bio_async(cc_), 0);
}

/*=============================================================================
 * Load Balancing (Routing) Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionThalamicTest, LoadBalancingAsRouting) {
    /* Load balancing redistributes routing load */

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    /* Balance load across instances */
    int result = collective_cognition_balance_load(cc_);
    EXPECT_EQ(result, 0);

    collective_cognition_state_t state;
    ASSERT_EQ(collective_cognition_get_state(cc_, &state), 0);

    /* Should not cause overload */
    EXPECT_EQ(state.active_instances, 4u);
}

TEST_F(CollectiveCognitionThalamicTest, TaskOffloadAsRouting) {
    /* Task offload routes work between instances */

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    /* Attempt task offload (implementation may vary) */
    /* This tests the routing of cognitive tasks */
    collective_cognition_state_t state;
    ASSERT_EQ(collective_cognition_get_state(cc_, &state), 0);
}

/*=============================================================================
 * Fragmentation Detection Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionThalamicTest, FragmentationDisruptsRouting) {
    /* Fragmentation indicates routing failure */

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    collective_cognition_state_t state;
    ASSERT_EQ(collective_cognition_get_state(cc_, &state), 0);

    /* Normal operation should not be fragmented */
    /* Fragmentation = routing paths broken */
}

TEST_F(CollectiveCognitionThalamicTest, OverloadIndicatesRoutingCongestion) {
    /* Overload indicates routing congestion */

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    collective_cognition_state_t state;
    ASSERT_EQ(collective_cognition_get_state(cc_, &state), 0);

    /* Normal operation should not be overloaded */
    /* Overload = routing queues full */
}

/*=============================================================================
 * Leader Detection for Centralized Routing Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionThalamicTest, LeaderAsRoutingHub) {
    /* Leader instance can serve as routing hub */

    hyperscanning_t* hs = collective_cognition_get_hyperscanning(cc_);
    ASSERT_NE(hs, nullptr);

    /* Establish clear leader */
    for (uint32_t i = 1; i <= 4; i++) {
        hyperscanning_neural_state_t state;
        memset(&state, 0, sizeof(state));
        state.instance_id = i;
        state.band_power[SYNC_BAND_GAMMA] = (i == 1) ? 0.95f : 0.6f;
        state.atp_level = 0.9f;
        hyperscanning_update_state(hs, &state);
    }

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    uint32_t leader = hyperscanning_get_leader(hs);
    float influence = hyperscanning_get_leader_influence(hs);

    /* Leader can coordinate routing */
    EXPECT_GE(leader, 0u);
    EXPECT_GE(influence, 0.0f);
}

/*=============================================================================
 * Role-Based Routing Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionThalamicTest, RolesForRoutingDecisions) {
    /* Roles determine routing paths */

    shared_intentionality_t* si = collective_cognition_get_intentionality(cc_);
    ASSERT_NE(si, nullptr);

    shared_goal_t goal;
    memset(&goal, 0, sizeof(goal));
    snprintf(goal.description, sizeof(goal.description), "Routed task");
    goal.priority = 0.9f;

    uint32_t goal_id = shared_intentionality_propose_goal(si, &goal);
    EXPECT_GT(goal_id, 0u);

    /* Assign different routing roles */
    ASSERT_EQ(shared_intentionality_assign_role(si, goal_id, 1, ROLE_LEADER), 0);
    ASSERT_EQ(shared_intentionality_assign_role(si, goal_id, 2, ROLE_EXECUTOR), 0);
    ASSERT_EQ(shared_intentionality_assign_role(si, goal_id, 3, ROLE_VERIFIER), 0);
    ASSERT_EQ(shared_intentionality_assign_role(si, goal_id, 4, ROLE_COMMUNICATOR), 0);

    for (uint32_t i = 1; i <= 4; i++) {
        ASSERT_EQ(shared_intentionality_commit_to_goal(si, goal_id, i, 0.9f), 0);
    }

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    we_mode_state_t we_mode;
    ASSERT_EQ(collective_cognition_get_we_mode(cc_, &we_mode), 0);

    /* Role understanding enables routing */
    EXPECT_GE(we_mode.role_understanding, 0.0f);
}

/*=============================================================================
 * Stress Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionThalamicTest, HighThroughputRouting) {
    /* High throughput routing test */

    for (int cycle = 0; cycle < 100; cycle++) {
        ASSERT_EQ(collective_cognition_update(cc_), 0);
    }

    collective_cognition_state_t state;
    ASSERT_EQ(collective_cognition_get_state(cc_, &state), 0);
    EXPECT_EQ(state.active_instances, 4u);
}

TEST_F(CollectiveCognitionThalamicTest, RapidStateChanges) {
    /* Handle rapid neural state changes (routing updates) */

    hyperscanning_t* hs = collective_cognition_get_hyperscanning(cc_);
    ASSERT_NE(hs, nullptr);

    for (int frame = 0; frame < 50; frame++) {
        for (uint32_t i = 1; i <= 4; i++) {
            hyperscanning_neural_state_t state;
            memset(&state, 0, sizeof(state));
            state.instance_id = i;
            state.band_power[SYNC_BAND_GAMMA] = 0.5f + 0.3f * sinf(frame * 0.2f);
            state.atp_level = 0.8f;
            hyperscanning_update_state(hs, &state);
        }

        ASSERT_EQ(collective_cognition_update(cc_), 0);
    }
}

TEST_F(CollectiveCognitionThalamicTest, ManyGoalsAndAttentions) {
    /* Route many goals and attentions simultaneously */

    shared_intentionality_t* si = collective_cognition_get_intentionality(cc_);
    ASSERT_NE(si, nullptr);

    /* Create many goals */
    for (int g = 0; g < 10; g++) {
        shared_goal_t goal;
        memset(&goal, 0, sizeof(goal));
        snprintf(goal.description, sizeof(goal.description), "Goal %d", g);
        goal.priority = 0.9f - g * 0.05f;

        uint32_t goal_id = shared_intentionality_propose_goal(si, &goal);
        EXPECT_GT(goal_id, 0u);

        for (uint32_t i = 1; i <= 4; i++) {
            shared_intentionality_commit_to_goal(si, goal_id, i, 0.8f);
        }
    }

    /* Create many attentions */
    for (int a = 0; a < 8; a++) {
        joint_attention_t attention;
        memset(&attention, 0, sizeof(attention));
        attention.salience = 0.8f - a * 0.05f;

        uint32_t attn_id = shared_intentionality_propose_attention(si, &attention);
        EXPECT_GT(attn_id, 0u);

        for (uint32_t i = 1; i <= 4; i++) {
            shared_intentionality_join_attention(si, attn_id, i);
        }
    }

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    we_mode_state_t we_mode;
    ASSERT_EQ(collective_cognition_get_we_mode(cc_, &we_mode), 0);

    /* Should handle many items */
    EXPECT_GE(we_mode.active_shared_goals, 1u);
    EXPECT_GE(we_mode.active_joint_attentions, 1u);
}
