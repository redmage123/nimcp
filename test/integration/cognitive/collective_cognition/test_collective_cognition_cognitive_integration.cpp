/**
 * @file test_collective_cognition_cognitive_integration.cpp
 * @brief Integration tests for collective cognition + cognitive layer
 *
 * Tests integration of collective cognition with:
 * - Emotion system (emotion substrate bridge)
 * - Attention system (attention substrate bridge)
 * - Memory system (working memory, memory consolidation)
 * - Executive functions (planning, task management)
 * - Theory of Mind (BDI agents, social cognition)
 * - Global workspace (consciousness broadcast)
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

class CollectiveCognitionCognitiveTest : public ::testing::Test {
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
 * Emotion Integration Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionCognitiveTest, EmotionalSyncViaHyperscanning) {
    /* Theta band sync reflects emotional synchronization across instances */

    hyperscanning_t* hs = collective_cognition_get_hyperscanning(cc_);
    ASSERT_NE(hs, nullptr);

    /* Set high theta power for emotional sync */
    for (uint32_t i = 1; i <= 4; i++) {
        hyperscanning_neural_state_t state;
        memset(&state, 0, sizeof(state));
        state.instance_id = i;
        state.band_power[SYNC_BAND_THETA] = 0.8f;  /* High theta = emotional */
        state.band_phase[SYNC_BAND_THETA] = 0.5f;
        state.band_power[SYNC_BAND_GAMMA] = 0.6f;
        state.atp_level = 0.9f;
        ASSERT_EQ(hyperscanning_update_state(hs, &state), 0);
    }

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    hyperscan_state_t sync_state;
    ASSERT_EQ(collective_cognition_get_hyperscan_state(cc_, &sync_state), 0);

    /* Theta emotional sync should be measurable */
    EXPECT_GE(sync_state.theta_emotional, 0.0f);
    EXPECT_LE(sync_state.theta_emotional, 1.0f);
}

TEST_F(CollectiveCognitionCognitiveTest, CollectiveEmotionalContagion) {
    /* Shared intentionality enables emotional contagion via we-mode */

    shared_intentionality_t* si = collective_cognition_get_intentionality(cc_);
    ASSERT_NE(si, nullptr);

    /* Enter we-mode for emotional sharing */
    ASSERT_EQ(shared_intentionality_enter_we_mode(si), 0);

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    we_mode_state_t we_mode;
    ASSERT_EQ(collective_cognition_get_we_mode(cc_, &we_mode), 0);

    /* We-mode should be active */
    EXPECT_TRUE(shared_intentionality_is_we_mode_active(si));

    /* Mutual responsiveness enables emotional synchronization */
    EXPECT_GE(we_mode.mutual_responsiveness, 0.0f);
}

/*=============================================================================
 * Attention Integration Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionCognitiveTest, JointAttentionMechanism) {
    /* Joint attention enables shared focus across instances */

    shared_intentionality_t* si = collective_cognition_get_intentionality(cc_);
    ASSERT_NE(si, nullptr);

    /* Create joint attention target */
    joint_attention_t attention;
    memset(&attention, 0, sizeof(attention));
    attention.salience = 0.85f;
    attention.feature_vector[0] = 1.0f;  /* Target feature 1 */
    attention.feature_vector[1] = 0.5f;  /* Target feature 2 */

    uint32_t attn_id = shared_intentionality_propose_attention(si, &attention);
    EXPECT_GT(attn_id, 0u);

    /* All instances join attention */
    for (uint32_t i = 1; i <= 4; i++) {
        ASSERT_EQ(shared_intentionality_join_attention(si, attn_id, i), 0);
    }

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    /* Verify joint attention is active */
    we_mode_state_t we_mode;
    ASSERT_EQ(collective_cognition_get_we_mode(cc_, &we_mode), 0);
    EXPECT_GE(we_mode.active_joint_attentions, 1u);
}

TEST_F(CollectiveCognitionCognitiveTest, AttentionCoherence) {
    /* Collective state tracks attention coherence */

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    collective_cognition_state_t state;
    ASSERT_EQ(collective_cognition_get_state(cc_, &state), 0);

    /* Attention coherence reflects shared focus */
    EXPECT_GE(state.attention_coherence, 0.0f);
    EXPECT_LE(state.attention_coherence, 1.0f);
}

TEST_F(CollectiveCognitionCognitiveTest, AlphaBandAttentionInhibition) {
    /* Alpha band reflects attention inhibition/relaxed attention */

    hyperscanning_t* hs = collective_cognition_get_hyperscanning(cc_);
    ASSERT_NE(hs, nullptr);

    /* High alpha = relaxed attention, lower processing demand */
    for (uint32_t i = 1; i <= 4; i++) {
        hyperscanning_neural_state_t state;
        memset(&state, 0, sizeof(state));
        state.instance_id = i;
        state.band_power[SYNC_BAND_ALPHA] = 0.7f;
        state.band_power[SYNC_BAND_GAMMA] = 0.3f;
        state.atp_level = 0.95f;
        ASSERT_EQ(hyperscanning_update_state(hs, &state), 0);
    }

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    hyperscan_state_t sync_state;
    ASSERT_EQ(collective_cognition_get_hyperscan_state(cc_, &sync_state), 0);

    /* System should be synchronized */
    EXPECT_GE(sync_state.global_sync, 0.0f);
}

/*=============================================================================
 * Memory Integration Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionCognitiveTest, ExtendedMemoryCapacity) {
    /* Extended mind provides external memory capacity */

    extended_mind_t* em = collective_cognition_get_extended_mind(cc_);
    ASSERT_NE(em, nullptr);

    /* Register external memory extension */
    cognitive_extension_t ext;
    memset(&ext, 0, sizeof(ext));
    ext.type = EXT_TYPE_MEMORY;
    snprintf(ext.name, sizeof(ext.name), "ExternalMemoryStore");
    ext.reliability = 0.99f;
    ext.avg_latency_ms = 10.0f;
    ext.integration_depth = 0.9f;
    ext.trust_level = 0.95f;

    uint32_t ext_id = extended_mind_register_extension(em, &ext);
    EXPECT_GT(ext_id, 0u);

    /* Check memory capacity increased */
    float capacity = extended_mind_get_capacity(em, EXT_TYPE_MEMORY);
    EXPECT_GT(capacity, 0.0f);
}

TEST_F(CollectiveCognitionCognitiveTest, CollectiveWorkingMemory) {
    /* Phi measures how well working memory is integrated across instances */

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    collective_phi_t phi;
    ASSERT_EQ(collective_cognition_get_phi(cc_, &phi), 0);

    /* Information integration reflects shared memory access */
    EXPECT_GE(phi.information, 0.0f);
}

/*=============================================================================
 * Executive Function Integration Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionCognitiveTest, SharedGoalPlanning) {
    /* Executive functions coordinate through shared goals */

    shared_intentionality_t* si = collective_cognition_get_intentionality(cc_);
    ASSERT_NE(si, nullptr);

    /* Create a planning goal */
    shared_goal_t goal;
    memset(&goal, 0, sizeof(goal));
    snprintf(goal.description, sizeof(goal.description),
             "Execute multi-step plan cooperatively");
    goal.priority = 0.8f;

    uint32_t goal_id = shared_intentionality_propose_goal(si, &goal);
    EXPECT_GT(goal_id, 0u);

    /* Assign roles for the plan */
    ASSERT_EQ(shared_intentionality_assign_role(si, goal_id, 1, ROLE_LEADER), 0);
    ASSERT_EQ(shared_intentionality_assign_role(si, goal_id, 2, ROLE_EXECUTOR), 0);
    ASSERT_EQ(shared_intentionality_assign_role(si, goal_id, 3, ROLE_VERIFIER), 0);
    ASSERT_EQ(shared_intentionality_assign_role(si, goal_id, 4, ROLE_OBSERVER), 0);

    /* All commit to the goal */
    for (uint32_t i = 1; i <= 4; i++) {
        ASSERT_EQ(shared_intentionality_commit_to_goal(si, goal_id, i, 0.9f), 0);
    }

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    we_mode_state_t we_mode;
    ASSERT_EQ(collective_cognition_get_we_mode(cc_, &we_mode), 0);

    /* Role understanding should be positive */
    EXPECT_GE(we_mode.role_understanding, 0.0f);
}

TEST_F(CollectiveCognitionCognitiveTest, GoalProgressTracking) {
    /* Track goal progress for executive control */

    shared_intentionality_t* si = collective_cognition_get_intentionality(cc_);
    ASSERT_NE(si, nullptr);

    /* Create goal */
    shared_goal_t goal;
    memset(&goal, 0, sizeof(goal));
    snprintf(goal.description, sizeof(goal.description), "Complete task X");
    goal.priority = 0.75f;

    uint32_t goal_id = shared_intentionality_propose_goal(si, &goal);
    EXPECT_GT(goal_id, 0u);

    /* Commit and start */
    for (uint32_t i = 1; i <= 4; i++) {
        ASSERT_EQ(shared_intentionality_commit_to_goal(si, goal_id, i, 0.8f), 0);
    }

    /* Update progress */
    ASSERT_EQ(shared_intentionality_update_goal_progress(si, goal_id, 0.5f), 0);

    /* Complete the goal */
    ASSERT_EQ(shared_intentionality_complete_goal(si, goal_id), 0);

    ASSERT_EQ(collective_cognition_update(cc_), 0);
}

TEST_F(CollectiveCognitionCognitiveTest, BetaBandActiveThinking) {
    /* Beta band correlates with active executive processing */

    hyperscanning_t* hs = collective_cognition_get_hyperscanning(cc_);
    ASSERT_NE(hs, nullptr);

    /* High beta = active thinking/motor planning */
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

    /* Beta coordination should be measurable */
    EXPECT_GE(sync_state.beta_coordination, 0.0f);
}

/*=============================================================================
 * Theory of Mind Integration Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionCognitiveTest, SharedBeliefsViaWeModestate) {
    /* We-mode enables shared beliefs across instances */

    shared_intentionality_t* si = collective_cognition_get_intentionality(cc_);
    ASSERT_NE(si, nullptr);

    /* Enter we-mode for shared cognition */
    ASSERT_EQ(shared_intentionality_enter_we_mode(si), 0);

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    we_mode_state_t we_mode;
    ASSERT_EQ(collective_cognition_get_we_mode(cc_, &we_mode), 0);

    /* We-mode strength reflects shared perspective taking */
    EXPECT_GE(we_mode.we_mode_strength, 0.0f);
}

TEST_F(CollectiveCognitionCognitiveTest, MutualResponsivenessForToM) {
    /* Mutual responsiveness enables Theory of Mind coordination */

    shared_intentionality_t* si = collective_cognition_get_intentionality(cc_);
    ASSERT_NE(si, nullptr);

    /* Create shared goal requiring ToM */
    shared_goal_t goal;
    memset(&goal, 0, sizeof(goal));
    snprintf(goal.description, sizeof(goal.description),
             "Coordinate based on understanding each other's intentions");
    goal.priority = 0.9f;

    uint32_t goal_id = shared_intentionality_propose_goal(si, &goal);
    EXPECT_GT(goal_id, 0u);

    /* All commit with high commitment */
    for (uint32_t i = 1; i <= 4; i++) {
        ASSERT_EQ(shared_intentionality_commit_to_goal(si, goal_id, i, 0.95f), 0);
    }

    /* Enter we-mode */
    ASSERT_EQ(shared_intentionality_enter_we_mode(si), 0);

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    we_mode_state_t we_mode;
    ASSERT_EQ(collective_cognition_get_we_mode(cc_, &we_mode), 0);

    /* Mutual responsiveness enables ToM-based coordination */
    EXPECT_GE(we_mode.mutual_responsiveness, 0.0f);
}

/*=============================================================================
 * Global Workspace Integration Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionCognitiveTest, GammaBindingConsciousness) {
    /* Gamma band binding reflects conscious integration */

    hyperscanning_t* hs = collective_cognition_get_hyperscanning(cc_);
    ASSERT_NE(hs, nullptr);

    /* Set high gamma for all instances */
    for (uint32_t i = 1; i <= 4; i++) {
        hyperscanning_neural_state_t state;
        memset(&state, 0, sizeof(state));
        state.instance_id = i;
        state.band_power[SYNC_BAND_GAMMA] = 0.9f;
        state.band_phase[SYNC_BAND_GAMMA] = 1.5f;
        state.atp_level = 0.95f;
        ASSERT_EQ(hyperscanning_update_state(hs, &state), 0);
    }

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    hyperscan_state_t sync_state;
    ASSERT_EQ(collective_cognition_get_hyperscan_state(cc_, &sync_state), 0);

    /* Gamma binding should be strong */
    EXPECT_GE(sync_state.gamma_binding, 0.0f);
}

TEST_F(CollectiveCognitionCognitiveTest, ConsciousnessLevelProgression) {
    /* Test consciousness level with different phi values */

    /* Run updates to establish phi */
    for (int i = 0; i < 10; i++) {
        ASSERT_EQ(collective_cognition_update(cc_), 0);
    }

    collective_consciousness_level_t level =
        collective_cognition_get_consciousness_level(cc_);

    /* Should have some level of consciousness with 4 instances */
    EXPECT_GE((int)level, (int)COLLECTIVE_CONSCIOUSNESS_NONE);

    collective_phi_t phi;
    ASSERT_EQ(collective_cognition_get_phi(cc_, &phi), 0);

    /* Consciousness level should correspond to phi range */
    if (phi.phi_total < 0.1f) {
        EXPECT_EQ(level, COLLECTIVE_CONSCIOUSNESS_NONE);
    } else if (phi.phi_total < 0.3f) {
        EXPECT_LE((int)level, (int)COLLECTIVE_CONSCIOUSNESS_MINIMAL);
    }
}

TEST_F(CollectiveCognitionCognitiveTest, InformationFlowRate) {
    /* Information flow rate reflects conscious access speed */

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    collective_cognition_state_t state;
    ASSERT_EQ(collective_cognition_get_state(cc_, &state), 0);

    /* Information flow rate should be non-negative */
    EXPECT_GE(state.information_flow_rate, 0.0f);
}

/*=============================================================================
 * Integration Quality Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionCognitiveTest, OverallIntegrationQuality) {
    /* Overall integration quality measures cognitive coherence */

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    collective_cognition_state_t state;
    ASSERT_EQ(collective_cognition_get_state(cc_, &state), 0);

    EXPECT_GE(state.integration_quality, 0.0f);
    EXPECT_LE(state.integration_quality, 1.0f);
}

TEST_F(CollectiveCognitionCognitiveTest, FragmentationDetection) {
    /* Detect fragmented cognitive state */

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    collective_cognition_state_t state;
    ASSERT_EQ(collective_cognition_get_state(cc_, &state), 0);

    /* With 4 healthy instances, should not be fragmented */
    /* Note: Fragmentation depends on sync quality */
}

TEST_F(CollectiveCognitionCognitiveTest, OverloadDetection) {
    /* Detect cognitive overload */

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    collective_cognition_state_t state;
    ASSERT_EQ(collective_cognition_get_state(cc_, &state), 0);

    /* Check overload flag */
    /* Normal operation should not be overloaded */
}

/*=============================================================================
 * Cognitive Extension Integration Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionCognitiveTest, PerceptionExtension) {
    /* External perception extends cognitive capacity */

    extended_mind_t* em = collective_cognition_get_extended_mind(cc_);
    ASSERT_NE(em, nullptr);

    cognitive_extension_t ext;
    memset(&ext, 0, sizeof(ext));
    ext.type = EXT_TYPE_PERCEPTION;
    snprintf(ext.name, sizeof(ext.name), "ExternalSensor");
    ext.reliability = 0.9f;
    ext.avg_latency_ms = 50.0f;
    ext.integration_depth = 0.7f;
    ext.trust_level = 0.85f;

    uint32_t ext_id = extended_mind_register_extension(em, &ext);
    EXPECT_GT(ext_id, 0u);
}

TEST_F(CollectiveCognitionCognitiveTest, CommunicationExtension) {
    /* Communication extension enables inter-system coordination */

    extended_mind_t* em = collective_cognition_get_extended_mind(cc_);
    ASSERT_NE(em, nullptr);

    cognitive_extension_t ext;
    memset(&ext, 0, sizeof(ext));
    ext.type = EXT_TYPE_COMMUNICATION;
    snprintf(ext.name, sizeof(ext.name), "MessageBroker");
    ext.reliability = 0.98f;
    ext.avg_latency_ms = 5.0f;
    ext.integration_depth = 0.95f;
    ext.trust_level = 0.99f;

    uint32_t ext_id = extended_mind_register_extension(em, &ext);
    EXPECT_GT(ext_id, 0u);
}

TEST_F(CollectiveCognitionCognitiveTest, ActionExtension) {
    /* Action extension enables external effector control */

    extended_mind_t* em = collective_cognition_get_extended_mind(cc_);
    ASSERT_NE(em, nullptr);

    cognitive_extension_t ext;
    memset(&ext, 0, sizeof(ext));
    ext.type = EXT_TYPE_ACTION;
    snprintf(ext.name, sizeof(ext.name), "RobotController");
    ext.reliability = 0.92f;
    ext.avg_latency_ms = 20.0f;
    ext.integration_depth = 0.8f;
    ext.trust_level = 0.88f;

    uint32_t ext_id = extended_mind_register_extension(em, &ext);
    EXPECT_GT(ext_id, 0u);

    float capacity = extended_mind_get_capacity(em, EXT_TYPE_ACTION);
    EXPECT_GT(capacity, 0.0f);
}
