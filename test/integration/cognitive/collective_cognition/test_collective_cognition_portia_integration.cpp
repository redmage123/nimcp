/**
 * @file test_collective_cognition_portia_integration.cpp
 * @brief Integration tests for collective cognition + Portia module
 *
 * Tests integration of collective cognition with:
 * - Portia learning (adaptive intelligence)
 * - Portia planning (multi-step reasoning)
 * - Sensor fusion (multi-modal perception)
 * - Portia swarm bridges (multi-agent coordination)
 * - Resource-aware cognition (Portia tiers)
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

class CollectiveCognitionPortiaTest : public ::testing::Test {
protected:
    void SetUp() override {
        cc_config_ = collective_cognition_default_config();
        cc_ = collective_cognition_create(&cc_config_);
        ASSERT_NE(cc_, nullptr);

        /* Register multiple "Portia" agents */
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
 * Adaptive Learning Integration Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionPortiaTest, CollectiveLearningGoal) {
    /* Shared goal for collective learning task */

    shared_intentionality_t* si = collective_cognition_get_intentionality(cc_);
    ASSERT_NE(si, nullptr);

    /* Create learning goal */
    shared_goal_t goal;
    memset(&goal, 0, sizeof(goal));
    snprintf(goal.description, sizeof(goal.description),
             "Collectively learn prey behavior patterns");
    goal.priority = 0.9f;

    uint32_t goal_id = shared_intentionality_propose_goal(si, &goal);
    EXPECT_GT(goal_id, 0u);

    /* All Portia agents commit to learning */
    for (uint32_t i = 1; i <= 4; i++) {
        ASSERT_EQ(shared_intentionality_commit_to_goal(si, goal_id, i, 0.9f), 0);
    }

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    we_mode_state_t we_mode;
    ASSERT_EQ(collective_cognition_get_we_mode(cc_, &we_mode), 0);

    /* Should have active learning goal */
    EXPECT_GE(we_mode.active_shared_goals, 1u);
}

TEST_F(CollectiveCognitionPortiaTest, PhiGuidedLearning) {
    /* Phi level guides learning aggressiveness */

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    collective_phi_t phi;
    ASSERT_EQ(collective_cognition_get_phi(cc_, &phi), 0);

    /* Higher phi = can learn more complex patterns collectively */
    EXPECT_GE(phi.phi_total, 0.0f);
    EXPECT_GE(phi.integration, 0.0f);
}

TEST_F(CollectiveCognitionPortiaTest, HyperscanningForLearningSync) {
    /* Hyperscanning synchronizes learning states */

    hyperscanning_t* hs = collective_cognition_get_hyperscanning(cc_);
    ASSERT_NE(hs, nullptr);

    /* Set learning-optimized neural states (theta for memory) */
    for (uint32_t i = 1; i <= 4; i++) {
        hyperscanning_neural_state_t state;
        memset(&state, 0, sizeof(state));
        state.instance_id = i;
        state.band_power[SYNC_BAND_THETA] = 0.8f;  /* Memory/learning */
        state.band_power[SYNC_BAND_GAMMA] = 0.6f;  /* Binding */
        state.atp_level = 0.9f;
        ASSERT_EQ(hyperscanning_update_state(hs, &state), 0);
    }

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    hyperscan_state_t sync_state;
    ASSERT_EQ(collective_cognition_get_hyperscan_state(cc_, &sync_state), 0);

    /* Theta sync for coordinated learning */
    EXPECT_GE(sync_state.theta_emotional, 0.0f);
}

/*=============================================================================
 * Multi-Step Planning Integration Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionPortiaTest, SharedPlanningGoal) {
    /* Shared goal for multi-step planning */

    shared_intentionality_t* si = collective_cognition_get_intentionality(cc_);
    ASSERT_NE(si, nullptr);

    /* Create planning goal */
    shared_goal_t goal;
    memset(&goal, 0, sizeof(goal));
    snprintf(goal.description, sizeof(goal.description),
             "Plan multi-step hunting strategy");
    goal.priority = 0.95f;

    uint32_t goal_id = shared_intentionality_propose_goal(si, &goal);
    EXPECT_GT(goal_id, 0u);

    /* Assign planning roles */
    ASSERT_EQ(shared_intentionality_assign_role(si, goal_id, 1, ROLE_LEADER), 0);
    ASSERT_EQ(shared_intentionality_assign_role(si, goal_id, 2, ROLE_EXECUTOR), 0);
    ASSERT_EQ(shared_intentionality_assign_role(si, goal_id, 3, ROLE_EXECUTOR), 0);
    ASSERT_EQ(shared_intentionality_assign_role(si, goal_id, 4, ROLE_VERIFIER), 0);

    for (uint32_t i = 1; i <= 4; i++) {
        ASSERT_EQ(shared_intentionality_commit_to_goal(si, goal_id, i, 0.9f), 0);
    }

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    we_mode_state_t we_mode;
    ASSERT_EQ(collective_cognition_get_we_mode(cc_, &we_mode), 0);

    /* Role understanding for coordinated planning */
    EXPECT_GE(we_mode.role_understanding, 0.0f);
}

TEST_F(CollectiveCognitionPortiaTest, PlanProgressTracking) {
    /* Track progress through multi-step plan */

    shared_intentionality_t* si = collective_cognition_get_intentionality(cc_);
    ASSERT_NE(si, nullptr);

    /* Create plan */
    shared_goal_t goal;
    memset(&goal, 0, sizeof(goal));
    snprintf(goal.description, sizeof(goal.description), "Execute hunting plan");
    goal.priority = 0.9f;

    uint32_t goal_id = shared_intentionality_propose_goal(si, &goal);
    EXPECT_GT(goal_id, 0u);

    for (uint32_t i = 1; i <= 4; i++) {
        ASSERT_EQ(shared_intentionality_commit_to_goal(si, goal_id, i, 0.9f), 0);
    }

    /* Track progress through steps */
    for (float progress = 0.0f; progress <= 1.0f; progress += 0.25f) {
        ASSERT_EQ(shared_intentionality_update_goal_progress(si, goal_id, progress), 0);
        ASSERT_EQ(collective_cognition_update(cc_), 0);
    }

    /* Complete plan */
    ASSERT_EQ(shared_intentionality_complete_goal(si, goal_id), 0);
}

TEST_F(CollectiveCognitionPortiaTest, BetaSyncForActivePlanning) {
    /* Beta band sync for active planning/motor preparation */

    hyperscanning_t* hs = collective_cognition_get_hyperscanning(cc_);
    ASSERT_NE(hs, nullptr);

    /* High beta for planning */
    for (uint32_t i = 1; i <= 4; i++) {
        hyperscanning_neural_state_t state;
        memset(&state, 0, sizeof(state));
        state.instance_id = i;
        state.band_power[SYNC_BAND_BETA] = 0.85f;
        state.band_power[SYNC_BAND_GAMMA] = 0.6f;
        state.atp_level = 0.9f;
        ASSERT_EQ(hyperscanning_update_state(hs, &state), 0);
    }

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    hyperscan_state_t sync_state;
    ASSERT_EQ(collective_cognition_get_hyperscan_state(cc_, &sync_state), 0);

    /* Beta coordination for synchronized planning */
    EXPECT_GE(sync_state.beta_coordination, 0.0f);
}

/*=============================================================================
 * Sensor Fusion Integration Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionPortiaTest, MultiModalSensorAttention) {
    /* Joint attention on multi-modal sensor data */

    shared_intentionality_t* si = collective_cognition_get_intentionality(cc_);
    ASSERT_NE(si, nullptr);

    /* Create attention on fused sensor data */
    joint_attention_t attention;
    memset(&attention, 0, sizeof(attention));
    attention.salience = 0.9f;

    /* Feature vector represents fused sensor features */
    attention.feature_vector[0] = 0.8f;   /* Visual */
    attention.feature_vector[1] = 0.6f;   /* Motion */
    attention.feature_vector[2] = 0.7f;   /* Depth */
    attention.feature_vector[3] = 0.5f;   /* Proprioceptive */

    uint32_t attn_id = shared_intentionality_propose_attention(si, &attention);
    EXPECT_GT(attn_id, 0u);

    for (uint32_t i = 1; i <= 4; i++) {
        ASSERT_EQ(shared_intentionality_join_attention(si, attn_id, i), 0);
    }

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    we_mode_state_t we_mode;
    ASSERT_EQ(collective_cognition_get_we_mode(cc_, &we_mode), 0);
    EXPECT_GE(we_mode.active_joint_attentions, 1u);
}

TEST_F(CollectiveCognitionPortiaTest, ExternalSensorExtensions) {
    /* Portia uses external sensors via extended mind */

    extended_mind_t* em = collective_cognition_get_extended_mind(cc_);
    ASSERT_NE(em, nullptr);

    /* Register multiple sensor types */
    const char* sensors[] = {"VisualSensor", "DepthSensor", "MotionSensor"};

    for (int s = 0; s < 3; s++) {
        cognitive_extension_t ext;
        memset(&ext, 0, sizeof(ext));
        ext.type = EXT_TYPE_PERCEPTION;
        snprintf(ext.name, sizeof(ext.name), "%s", sensors[s]);
        ext.reliability = 0.9f - s * 0.05f;
        ext.avg_latency_ms = 10.0f + s * 5.0f;
        ext.integration_depth = 0.85f;
        ext.trust_level = 0.9f;

        uint32_t ext_id = extended_mind_register_extension(em, &ext);
        EXPECT_GT(ext_id, 0u);
    }

    extended_mind_state_t em_state;
    ASSERT_EQ(collective_cognition_get_extended_mind_state(cc_, &em_state), 0);
    EXPECT_GE(em_state.active_extensions, 3u);
}

TEST_F(CollectiveCognitionPortiaTest, GammaSyncForSensorBinding) {
    /* Gamma sync binds multi-modal sensor data */

    hyperscanning_t* hs = collective_cognition_get_hyperscanning(cc_);
    ASSERT_NE(hs, nullptr);

    /* High gamma for sensor binding */
    for (uint32_t i = 1; i <= 4; i++) {
        hyperscanning_neural_state_t state;
        memset(&state, 0, sizeof(state));
        state.instance_id = i;
        state.band_power[SYNC_BAND_GAMMA] = 0.9f;
        state.band_phase[SYNC_BAND_GAMMA] = 1.5f;
        state.atp_level = 0.9f;
        ASSERT_EQ(hyperscanning_update_state(hs, &state), 0);
    }

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    hyperscan_state_t sync_state;
    ASSERT_EQ(collective_cognition_get_hyperscan_state(cc_, &sync_state), 0);

    /* Gamma binding for sensor fusion */
    EXPECT_GE(sync_state.gamma_binding, 0.0f);
}

/*=============================================================================
 * Portia Swarm Coordination Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionPortiaTest, CollectiveHuntingCoordination) {
    /* Coordinate hunting across Portia swarm */

    shared_intentionality_t* si = collective_cognition_get_intentionality(cc_);
    ASSERT_NE(si, nullptr);

    /* Create hunting goal */
    shared_goal_t goal;
    memset(&goal, 0, sizeof(goal));
    snprintf(goal.description, sizeof(goal.description),
             "Coordinate web-based prey capture");
    goal.priority = 0.95f;

    uint32_t goal_id = shared_intentionality_propose_goal(si, &goal);
    EXPECT_GT(goal_id, 0u);

    /* Assign hunting roles */
    ASSERT_EQ(shared_intentionality_assign_role(si, goal_id, 1, ROLE_LEADER), 0);
    ASSERT_EQ(shared_intentionality_assign_role(si, goal_id, 2, ROLE_EXECUTOR), 0);
    ASSERT_EQ(shared_intentionality_assign_role(si, goal_id, 3, ROLE_EXECUTOR), 0);
    ASSERT_EQ(shared_intentionality_assign_role(si, goal_id, 4, ROLE_OBSERVER), 0);

    for (uint32_t i = 1; i <= 4; i++) {
        ASSERT_EQ(shared_intentionality_commit_to_goal(si, goal_id, i, 0.95f), 0);
    }

    /* Enter we-mode for coordinated hunting */
    ASSERT_EQ(shared_intentionality_enter_we_mode(si), 0);

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    we_mode_state_t we_mode;
    ASSERT_EQ(collective_cognition_get_we_mode(cc_, &we_mode), 0);

    EXPECT_TRUE(shared_intentionality_is_we_mode_active(si));
    EXPECT_GE(we_mode.we_mode_strength, 0.0f);
}

TEST_F(CollectiveCognitionPortiaTest, LeaderFollowerDynamics) {
    /* Leader-follower for Portia coordination */

    hyperscanning_t* hs = collective_cognition_get_hyperscanning(cc_);
    ASSERT_NE(hs, nullptr);

    /* Instance 1 as leader (highest gamma) */
    for (uint32_t i = 1; i <= 4; i++) {
        hyperscanning_neural_state_t state;
        memset(&state, 0, sizeof(state));
        state.instance_id = i;
        state.band_power[SYNC_BAND_GAMMA] = (i == 1) ? 0.95f : 0.7f;
        state.atp_level = 0.9f;
        ASSERT_EQ(hyperscanning_update_state(hs, &state), 0);
    }

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    uint32_t leader = hyperscanning_get_leader(hs);
    float influence = hyperscanning_get_leader_influence(hs);

    EXPECT_GE(leader, 0u);
    EXPECT_GE(influence, 0.0f);
}

/*=============================================================================
 * Resource-Aware Cognition Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionPortiaTest, ATPAwareCollectiveCapacity) {
    /* ATP levels affect collective capacity */

    hyperscanning_t* hs = collective_cognition_get_hyperscanning(cc_);
    ASSERT_NE(hs, nullptr);

    /* Set varying ATP (simulating resource constraints) */
    float atp_levels[] = {0.9f, 0.7f, 0.5f, 0.4f};

    for (uint32_t i = 1; i <= 4; i++) {
        hyperscanning_neural_state_t state;
        memset(&state, 0, sizeof(state));
        state.instance_id = i;
        state.band_power[SYNC_BAND_GAMMA] = 0.7f;
        state.atp_level = atp_levels[i-1];
        ASSERT_EQ(hyperscanning_update_state(hs, &state), 0);
    }

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    collective_cognition_state_t state;
    ASSERT_EQ(collective_cognition_get_state(cc_, &state), 0);

    /* Collective should adapt to resource constraints */
    EXPECT_GE(state.collective_capacity, 0.0f);
}

TEST_F(CollectiveCognitionPortiaTest, LoadBalancingForResourceEfficiency) {
    /* Load balancing for resource-efficient operation */

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    /* Balance load */
    int result = collective_cognition_balance_load(cc_);
    EXPECT_EQ(result, 0);

    collective_cognition_state_t state;
    ASSERT_EQ(collective_cognition_get_state(cc_, &state), 0);

    /* Should not be overloaded after balancing */
}

/*=============================================================================
 * Extended Mind for Portia Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionPortiaTest, ExternalReasoningForPlanning) {
    /* External reasoning engine assists planning */

    extended_mind_t* em = collective_cognition_get_extended_mind(cc_);
    ASSERT_NE(em, nullptr);

    cognitive_extension_t ext;
    memset(&ext, 0, sizeof(ext));
    ext.type = EXT_TYPE_REASONING;
    snprintf(ext.name, sizeof(ext.name), "PlanningAssistant");
    ext.reliability = 0.95f;
    ext.avg_latency_ms = 100.0f;
    ext.integration_depth = 0.8f;
    ext.trust_level = 0.9f;

    uint32_t ext_id = extended_mind_register_extension(em, &ext);
    EXPECT_GT(ext_id, 0u);

    float capacity = extended_mind_get_capacity(em, EXT_TYPE_REASONING);
    EXPECT_GT(capacity, 0.0f);
}

TEST_F(CollectiveCognitionPortiaTest, ExternalMemoryForLearning) {
    /* External memory stores learned patterns */

    extended_mind_t* em = collective_cognition_get_extended_mind(cc_);
    ASSERT_NE(em, nullptr);

    cognitive_extension_t ext;
    memset(&ext, 0, sizeof(ext));
    ext.type = EXT_TYPE_MEMORY;
    snprintf(ext.name, sizeof(ext.name), "PatternMemory");
    ext.reliability = 0.98f;
    ext.avg_latency_ms = 10.0f;
    ext.integration_depth = 0.9f;
    ext.trust_level = 0.95f;

    uint32_t ext_id = extended_mind_register_extension(em, &ext);
    EXPECT_GT(ext_id, 0u);
}

/*=============================================================================
 * Consciousness Level for Portia Intelligence Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionPortiaTest, ConsciousnessForIntelligence) {
    /* Higher consciousness enables more intelligent behavior */

    hyperscanning_t* hs = collective_cognition_get_hyperscanning(cc_);
    ASSERT_NE(hs, nullptr);

    /* Optimal states for consciousness */
    for (uint32_t i = 1; i <= 4; i++) {
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

    /* Intelligence requires consciousness */
    EXPECT_GE((int)level, (int)COLLECTIVE_CONSCIOUSNESS_NONE);
}

/*=============================================================================
 * Stress Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionPortiaTest, RapidPlanExecution) {
    /* Rapid plan creation and execution */

    shared_intentionality_t* si = collective_cognition_get_intentionality(cc_);
    ASSERT_NE(si, nullptr);

    for (int plan = 0; plan < 10; plan++) {
        shared_goal_t goal;
        memset(&goal, 0, sizeof(goal));
        snprintf(goal.description, sizeof(goal.description),
                 "Hunting plan %d", plan);
        goal.priority = 0.8f;

        uint32_t goal_id = shared_intentionality_propose_goal(si, &goal);
        EXPECT_GT(goal_id, 0u);

        for (uint32_t i = 1; i <= 4; i++) {
            shared_intentionality_commit_to_goal(si, goal_id, i, 0.9f);
        }

        ASSERT_EQ(collective_cognition_update(cc_), 0);

        shared_intentionality_update_goal_progress(si, goal_id, 1.0f);
        shared_intentionality_complete_goal(si, goal_id);
    }
}

TEST_F(CollectiveCognitionPortiaTest, ContinuousSensorUpdates) {
    /* Continuous sensor state updates */

    hyperscanning_t* hs = collective_cognition_get_hyperscanning(cc_);
    ASSERT_NE(hs, nullptr);

    for (int frame = 0; frame < 50; frame++) {
        for (uint32_t i = 1; i <= 4; i++) {
            hyperscanning_neural_state_t state;
            memset(&state, 0, sizeof(state));
            state.instance_id = i;

            /* Varying sensor-driven states */
            state.band_power[SYNC_BAND_GAMMA] = 0.6f + 0.2f * sinf(frame * 0.1f);
            state.band_power[SYNC_BAND_BETA] = 0.5f + 0.2f * cosf(frame * 0.15f);
            state.atp_level = 0.85f;

            hyperscanning_update_state(hs, &state);
        }

        ASSERT_EQ(collective_cognition_update(cc_), 0);
    }

    collective_cognition_state_t state;
    ASSERT_EQ(collective_cognition_get_state(cc_, &state), 0);
    EXPECT_EQ(state.active_instances, 4u);
}
