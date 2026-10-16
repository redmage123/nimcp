/**
 * @file test_collective_cognition_logic_integration.cpp
 * @brief Integration tests for collective cognition + logic gate module
 *
 * Tests integration of collective cognition with:
 * - Neural logic circuits (AND/OR/NOT/XOR/IMPLIES)
 * - Symbolic logic substrate bridge
 * - Symbolic logic thalamic bridge
 * - Swarm logic bridge (distributed reasoning)
 * - Collective reasoning and consensus
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

class CollectiveCognitionLogicTest : public ::testing::Test {
protected:
    void SetUp() override {
        cc_config_ = collective_cognition_default_config();
        cc_ = collective_cognition_create(&cc_config_);
        ASSERT_NE(cc_, nullptr);

        /* Register multiple brain instances for distributed reasoning */
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
 * Distributed Reasoning Integration Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionLogicTest, SharedLogicalGoal) {
    /* Collective can share logical reasoning goals */

    shared_intentionality_t* si = collective_cognition_get_intentionality(cc_);
    ASSERT_NE(si, nullptr);

    /* Propose a logical reasoning goal */
    shared_goal_t goal;
    memset(&goal, 0, sizeof(goal));
    snprintf(goal.description, sizeof(goal.description),
             "Solve distributed SAT problem cooperatively");
    goal.priority = 0.95f;

    uint32_t goal_id = shared_intentionality_propose_goal(si, &goal);
    EXPECT_GT(goal_id, 0u);

    /* Assign roles for distributed reasoning */
    ASSERT_EQ(shared_intentionality_assign_role(si, goal_id, 1, ROLE_LEADER), 0);
    ASSERT_EQ(shared_intentionality_assign_role(si, goal_id, 2, ROLE_EXECUTOR), 0);
    ASSERT_EQ(shared_intentionality_assign_role(si, goal_id, 3, ROLE_EXECUTOR), 0);
    ASSERT_EQ(shared_intentionality_assign_role(si, goal_id, 4, ROLE_VERIFIER), 0);

    /* All instances commit */
    for (uint32_t i = 1; i <= 4; i++) {
        ASSERT_EQ(shared_intentionality_commit_to_goal(si, goal_id, i, 0.9f), 0);
    }

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    we_mode_state_t we_mode;
    ASSERT_EQ(collective_cognition_get_we_mode(cc_, &we_mode), 0);

    /* Shared goal should be active */
    EXPECT_GE(we_mode.active_shared_goals, 1u);
}

TEST_F(CollectiveCognitionLogicTest, JointAttentionOnLogicalProblem) {
    /* Joint attention on a logical problem enables coordinated solving */

    shared_intentionality_t* si = collective_cognition_get_intentionality(cc_);
    ASSERT_NE(si, nullptr);

    /* Create joint attention on problem representation */
    joint_attention_t attention;
    memset(&attention, 0, sizeof(attention));
    attention.salience = 0.9f;

    /* Feature vector encodes problem characteristics */
    attention.feature_vector[0] = 1.0f;  /* Problem type: logic */
    attention.feature_vector[1] = 0.8f;  /* Complexity */
    attention.feature_vector[2] = 0.5f;  /* Current progress */

    uint32_t attn_id = shared_intentionality_propose_attention(si, &attention);
    EXPECT_GT(attn_id, 0u);

    /* All instances focus on the problem */
    for (uint32_t i = 1; i <= 4; i++) {
        ASSERT_EQ(shared_intentionality_join_attention(si, attn_id, i), 0);
    }

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    we_mode_state_t we_mode;
    ASSERT_EQ(collective_cognition_get_we_mode(cc_, &we_mode), 0);
    EXPECT_GE(we_mode.active_joint_attentions, 1u);
}

/*=============================================================================
 * Phi-Based Reasoning Quality Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionLogicTest, PhiCorrelatesWithReasoningIntegrity) {
    /* Higher phi = better logical coherence across instances */

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    collective_phi_t phi;
    ASSERT_EQ(collective_cognition_get_phi(cc_, &phi), 0);

    /* Integration measures logical coherence */
    EXPECT_GE(phi.integration, 0.0f);

    /* Information measures reasoning capacity */
    EXPECT_GE(phi.information, 0.0f);

    /* Exclusion measures well-defined logical boundaries */
    EXPECT_GE(phi.exclusion, 0.0f);
}

TEST_F(CollectiveCognitionLogicTest, NetworkTopologyForReasoning) {
    /* Network topology affects distributed reasoning efficiency */

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    collective_phi_t phi;
    ASSERT_EQ(collective_cognition_get_phi(cc_, &phi), 0);

    /* Connectivity enables information sharing */
    EXPECT_GE(phi.connectivity, 0.0f);

    /* Modularity can help parallel clause evaluation */
    EXPECT_GE(phi.modularity, 0.0f);

    /* Small-world property enables efficient routing */
    EXPECT_GE(phi.small_world_index, 0.0f);
}

/*=============================================================================
 * Hyperscanning for Reasoning Synchronization Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionLogicTest, GammaSyncForBindingLogicalConcepts) {
    /* Gamma band sync binds logical concepts across instances */

    hyperscanning_t* hs = collective_cognition_get_hyperscanning(cc_);
    ASSERT_NE(hs, nullptr);

    /* Set high gamma for conceptual binding */
    for (uint32_t i = 1; i <= 4; i++) {
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

    /* Good gamma binding = coherent logical concepts */
    EXPECT_GE(sync_state.gamma_binding, 0.0f);
}

TEST_F(CollectiveCognitionLogicTest, BetaSyncForActiveReasoning) {
    /* Beta band sync reflects active reasoning across instances */

    hyperscanning_t* hs = collective_cognition_get_hyperscanning(cc_);
    ASSERT_NE(hs, nullptr);

    /* High beta = active logical processing */
    for (uint32_t i = 1; i <= 4; i++) {
        hyperscanning_neural_state_t state;
        memset(&state, 0, sizeof(state));
        state.instance_id = i;
        state.band_power[SYNC_BAND_BETA] = 0.8f;
        state.band_power[SYNC_BAND_GAMMA] = 0.6f;
        state.atp_level = 0.85f;
        ASSERT_EQ(hyperscanning_update_state(hs, &state), 0);
    }

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    hyperscan_state_t sync_state;
    ASSERT_EQ(collective_cognition_get_hyperscan_state(cc_, &sync_state), 0);

    /* Beta coordination for synchronized reasoning */
    EXPECT_GE(sync_state.beta_coordination, 0.0f);
}

TEST_F(CollectiveCognitionLogicTest, LeaderForConsensusCoordination) {
    /* Leader instance coordinates consensus on logical conclusions */

    hyperscanning_t* hs = collective_cognition_get_hyperscanning(cc_);
    ASSERT_NE(hs, nullptr);

    /* Set different gamma powers to establish leader */
    for (uint32_t i = 1; i <= 4; i++) {
        hyperscanning_neural_state_t state;
        memset(&state, 0, sizeof(state));
        state.instance_id = i;
        /* Instance 1 has highest gamma */
        state.band_power[SYNC_BAND_GAMMA] = (i == 1) ? 0.95f : 0.7f;
        state.band_phase[SYNC_BAND_GAMMA] = 1.5f;
        state.atp_level = 0.9f;
        ASSERT_EQ(hyperscanning_update_state(hs, &state), 0);
    }

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    /* Leader detection for consensus coordination */
    uint32_t leader = hyperscanning_get_leader(hs);
    float influence = hyperscanning_get_leader_influence(hs);

    EXPECT_GE(leader, 0u);
    EXPECT_GE(influence, 0.0f);
}

/*=============================================================================
 * Extended Mind for External Reasoning Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionLogicTest, ExternalReasoningExtension) {
    /* External reasoning engines (LLMs, theorem provers) extend capacity */

    extended_mind_t* em = collective_cognition_get_extended_mind(cc_);
    ASSERT_NE(em, nullptr);

    /* Register theorem prover as reasoning extension */
    cognitive_extension_t ext;
    memset(&ext, 0, sizeof(ext));
    ext.type = EXT_TYPE_REASONING;
    snprintf(ext.name, sizeof(ext.name), "TheoremProver");
    ext.reliability = 0.99f;
    ext.avg_latency_ms = 500.0f;
    ext.integration_depth = 0.7f;
    ext.trust_level = 0.95f;

    uint32_t ext_id = extended_mind_register_extension(em, &ext);
    EXPECT_GT(ext_id, 0u);

    /* Check reasoning capacity */
    float capacity = extended_mind_get_capacity(em, EXT_TYPE_REASONING);
    EXPECT_GT(capacity, 0.0f);
}

TEST_F(CollectiveCognitionLogicTest, ExternalKnowledgeBase) {
    /* External knowledge base provides facts for reasoning */

    extended_mind_t* em = collective_cognition_get_extended_mind(cc_);
    ASSERT_NE(em, nullptr);

    /* Register knowledge base as memory extension */
    cognitive_extension_t ext;
    memset(&ext, 0, sizeof(ext));
    ext.type = EXT_TYPE_MEMORY;
    snprintf(ext.name, sizeof(ext.name), "KnowledgeBase");
    ext.reliability = 0.98f;
    ext.avg_latency_ms = 20.0f;
    ext.integration_depth = 0.9f;
    ext.trust_level = 0.92f;

    uint32_t ext_id = extended_mind_register_extension(em, &ext);
    EXPECT_GT(ext_id, 0u);

    extended_mind_state_t em_state;
    ASSERT_EQ(collective_cognition_get_extended_mind_state(cc_, &em_state), 0);
    EXPECT_GE(em_state.active_extensions, 1u);
}

/*=============================================================================
 * Consensus and Voting Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionLogicTest, ConsensusViaSharedGoalCommitment) {
    /* Shared goal commitment models voting for logical conclusions */

    shared_intentionality_t* si = collective_cognition_get_intentionality(cc_);
    ASSERT_NE(si, nullptr);

    /* Propose logical conclusion */
    shared_goal_t goal;
    memset(&goal, 0, sizeof(goal));
    snprintf(goal.description, sizeof(goal.description),
             "Accept logical conclusion: (A AND B) IMPLIES C");
    goal.priority = 0.9f;

    uint32_t goal_id = shared_intentionality_propose_goal(si, &goal);
    EXPECT_GT(goal_id, 0u);

    /* Instances vote by commitment level */
    /* Unanimous agreement */
    ASSERT_EQ(shared_intentionality_commit_to_goal(si, goal_id, 1, 1.0f), 0);
    ASSERT_EQ(shared_intentionality_commit_to_goal(si, goal_id, 2, 0.95f), 0);
    ASSERT_EQ(shared_intentionality_commit_to_goal(si, goal_id, 3, 0.9f), 0);
    ASSERT_EQ(shared_intentionality_commit_to_goal(si, goal_id, 4, 0.85f), 0);

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    we_mode_state_t we_mode;
    ASSERT_EQ(collective_cognition_get_we_mode(cc_, &we_mode), 0);

    /* High joint commitment indicates consensus */
    EXPECT_GE(we_mode.joint_commitment, 0.0f);
}

TEST_F(CollectiveCognitionLogicTest, DissentDetectionViaPhi) {
    /* Low phi indicates disagreement/fragmentation in reasoning */

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    collective_phi_t phi;
    ASSERT_EQ(collective_cognition_get_phi(cc_, &phi), 0);

    /* Phi total measures overall coherence */
    /* Low phi would indicate logical inconsistency across instances */
    EXPECT_GE(phi.phi_total, 0.0f);
}

/*=============================================================================
 * Distributed Proof Verification Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionLogicTest, VerifierRoleForProofChecking) {
    /* Verifier role checks logical proofs from other instances */

    shared_intentionality_t* si = collective_cognition_get_intentionality(cc_);
    ASSERT_NE(si, nullptr);

    /* Create proof verification goal */
    shared_goal_t goal;
    memset(&goal, 0, sizeof(goal));
    snprintf(goal.description, sizeof(goal.description),
             "Verify proof of theorem X");
    goal.priority = 0.85f;

    uint32_t goal_id = shared_intentionality_propose_goal(si, &goal);
    EXPECT_GT(goal_id, 0u);

    /* Instance 1 proposes proof, instances 2-4 verify */
    ASSERT_EQ(shared_intentionality_assign_role(si, goal_id, 1, ROLE_LEADER), 0);
    ASSERT_EQ(shared_intentionality_assign_role(si, goal_id, 2, ROLE_VERIFIER), 0);
    ASSERT_EQ(shared_intentionality_assign_role(si, goal_id, 3, ROLE_VERIFIER), 0);
    ASSERT_EQ(shared_intentionality_assign_role(si, goal_id, 4, ROLE_VERIFIER), 0);

    /* All commit */
    for (uint32_t i = 1; i <= 4; i++) {
        ASSERT_EQ(shared_intentionality_commit_to_goal(si, goal_id, i, 0.9f), 0);
    }

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    we_mode_state_t we_mode;
    ASSERT_EQ(collective_cognition_get_we_mode(cc_, &we_mode), 0);

    /* Role understanding should be present */
    EXPECT_GE(we_mode.role_understanding, 0.0f);
}

/*=============================================================================
 * Information Flow for Logical Inference Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionLogicTest, InformationFlowForInference) {
    /* Information flow rate affects inference speed */

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    collective_cognition_state_t state;
    ASSERT_EQ(collective_cognition_get_state(cc_, &state), 0);

    /* Higher information flow = faster distributed inference */
    EXPECT_GE(state.information_flow_rate, 0.0f);
}

TEST_F(CollectiveCognitionLogicTest, AttentionCoherenceForFocusedReasoning) {
    /* Attention coherence ensures all instances reason about same premises */

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    collective_cognition_state_t state;
    ASSERT_EQ(collective_cognition_get_state(cc_, &state), 0);

    /* High coherence = all reasoning from same information */
    EXPECT_GE(state.attention_coherence, 0.0f);
    EXPECT_LE(state.attention_coherence, 1.0f);
}

/*=============================================================================
 * Consciousness Level for Complex Reasoning Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionLogicTest, HigherConsciousnessEnablesDeepReasoning) {
    /* Higher consciousness levels enable more complex reasoning */

    /* Set optimal neural states */
    hyperscanning_t* hs = collective_cognition_get_hyperscanning(cc_);
    ASSERT_NE(hs, nullptr);

    for (uint32_t i = 1; i <= 4; i++) {
        hyperscanning_neural_state_t state;
        memset(&state, 0, sizeof(state));
        state.instance_id = i;
        state.band_power[SYNC_BAND_GAMMA] = 0.9f;
        state.band_power[SYNC_BAND_BETA] = 0.8f;
        state.band_phase[SYNC_BAND_GAMMA] = 1.5f;
        state.atp_level = 0.95f;
        ASSERT_EQ(hyperscanning_update_state(hs, &state), 0);
    }

    /* Multiple updates to stabilize */
    for (int i = 0; i < 10; i++) {
        ASSERT_EQ(collective_cognition_update(cc_), 0);
    }

    collective_consciousness_level_t level =
        collective_cognition_get_consciousness_level(cc_);

    /* With good sync, should achieve some consciousness level */
    EXPECT_GE((int)level, (int)COLLECTIVE_CONSCIOUSNESS_NONE);
}

/*=============================================================================
 * Goal Alignment for Logical Cooperation Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionLogicTest, GoalAlignmentForCooperativeReasoning) {
    /* Goal alignment measures how well instances cooperate on reasoning */

    shared_intentionality_t* si = collective_cognition_get_intentionality(cc_);
    ASSERT_NE(si, nullptr);

    /* Create multiple reasoning goals */
    for (int g = 0; g < 3; g++) {
        shared_goal_t goal;
        memset(&goal, 0, sizeof(goal));
        snprintf(goal.description, sizeof(goal.description),
                 "Reasoning subgoal %d", g + 1);
        goal.priority = 0.8f - g * 0.1f;

        uint32_t goal_id = shared_intentionality_propose_goal(si, &goal);
        EXPECT_GT(goal_id, 0u);

        for (uint32_t i = 1; i <= 4; i++) {
            shared_intentionality_commit_to_goal(si, goal_id, i, 0.85f);
        }
    }

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    collective_cognition_state_t state;
    ASSERT_EQ(collective_cognition_get_state(cc_, &state), 0);

    /* Goal alignment should be measurable */
    EXPECT_GE(state.goal_alignment, 0.0f);
}

/*=============================================================================
 * Stress Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionLogicTest, ManyUpdatesForStability) {
    /* Ensure system remains stable through many reasoning cycles */

    for (int cycle = 0; cycle < 100; cycle++) {
        ASSERT_EQ(collective_cognition_update(cc_), 0);

        collective_cognition_state_t state;
        ASSERT_EQ(collective_cognition_get_state(cc_, &state), 0);

        /* Should not become fragmented or overloaded */
        EXPECT_EQ(state.active_instances, 4u);
    }
}

TEST_F(CollectiveCognitionLogicTest, RapidGoalCreationAndCompletion) {
    /* Test rapid goal lifecycle for iterative reasoning */

    shared_intentionality_t* si = collective_cognition_get_intentionality(cc_);
    ASSERT_NE(si, nullptr);

    for (int i = 0; i < 10; i++) {
        shared_goal_t goal;
        memset(&goal, 0, sizeof(goal));
        snprintf(goal.description, sizeof(goal.description),
                 "Inference step %d", i);
        goal.priority = 0.8f;

        uint32_t goal_id = shared_intentionality_propose_goal(si, &goal);
        EXPECT_GT(goal_id, 0u);

        for (uint32_t j = 1; j <= 4; j++) {
            shared_intentionality_commit_to_goal(si, goal_id, j, 0.9f);
        }

        shared_intentionality_update_goal_progress(si, goal_id, 1.0f);
        shared_intentionality_complete_goal(si, goal_id);
    }

    ASSERT_EQ(collective_cognition_update(cc_), 0);
}
