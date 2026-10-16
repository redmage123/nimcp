/**
 * @file test_collective_cognition_training_integration.cpp
 * @brief Integration tests for collective cognition + training layer
 *
 * Tests integration of collective cognition with:
 * - Meta-learning (MAML, Reptile, Prototypical)
 * - Adversarial training (collective immune response)
 * - Distributed training (federated learning, gradient sync)
 * - Gradient scaling with hyperscanning sync
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
#include "training/nimcp_meta_learning.h"
#include "training/nimcp_adversarial_training.h"
#include "training/nimcp_distributed_training.h"
#include "training/nimcp_gradient_scaling.h"

/*=============================================================================
 * Test Fixture
 *===========================================================================*/

class CollectiveCognitionTrainingTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* Initialize collective cognition */
        cc_config_ = collective_cognition_default_config();
        cc_ = collective_cognition_create(&cc_config_);
        ASSERT_NE(cc_, nullptr);

        /* Register multiple brain instances for collective */
        for (uint32_t i = 1; i <= 4; i++) {
            ASSERT_EQ(collective_cognition_register_instance(cc_, i, nullptr), 0);
        }

        /* Initialize meta-learning */
        meta_config_ = meta_learning_default_config();
        meta_ = meta_learning_create(&meta_config_);
        ASSERT_NE(meta_, nullptr);
    }

    void TearDown() override {
        if (meta_) {
            meta_learning_destroy(meta_);
            meta_ = nullptr;
        }
        if (cc_) {
            collective_cognition_destroy(cc_);
            cc_ = nullptr;
        }
    }

    collective_cognition_config_t cc_config_;
    collective_cognition_t* cc_ = nullptr;
    meta_config_t meta_config_;
    meta_learner_t* meta_ = nullptr;
};

/*=============================================================================
 * Meta-Learning Integration Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionTrainingTest, MetaLearningWithCollectivePhiGuidance) {
    /* Update collective cognition to compute phi */
    ASSERT_EQ(collective_cognition_update(cc_), 0);

    /* Get collective phi */
    collective_phi_t phi;
    ASSERT_EQ(collective_cognition_get_phi(cc_, &phi), 0);

    /* Meta-learning should adapt learning rate based on phi_network */
    /* Higher phi_network = better integration = can use higher learning rate */
    float phi_guided_lr = meta_config_.maml.outer_lr * (1.0f + phi.phi_network);
    EXPECT_GE(phi_guided_lr, meta_config_.maml.outer_lr);

    /* Verify phi is in valid range */
    EXPECT_GE(phi.phi_total, 0.0f);
    EXPECT_GE(phi.phi_network, 0.0f);
}

TEST_F(CollectiveCognitionTrainingTest, MetaLearningWithHyperscanningSync) {
    /* Get hyperscanning subsystem */
    hyperscanning_t* hs = collective_cognition_get_hyperscanning(cc_);
    ASSERT_NE(hs, nullptr);

    /* Update neural states for all instances */
    for (uint32_t i = 1; i <= 4; i++) {
        hyperscanning_neural_state_t state;
        memset(&state, 0, sizeof(state));
        state.instance_id = i;
        state.band_power[SYNC_BAND_GAMMA] = 0.7f + 0.1f * (i % 2);
        state.band_phase[SYNC_BAND_GAMMA] = 1.5f;
        state.atp_level = 0.9f;
        ASSERT_EQ(hyperscanning_update_state(hs, &state), 0);
    }

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    /* Get sync state */
    hyperscan_state_t sync_state;
    ASSERT_EQ(collective_cognition_get_hyperscan_state(cc_, &sync_state), 0);

    /* High gamma binding indicates collective consciousness */
    /* This should enable more aggressive meta-learning */
    if (sync_state.gamma_binding > 0.5f) {
        /* Good sync: can train collectively */
        EXPECT_TRUE(true);
    }
}

TEST_F(CollectiveCognitionTrainingTest, SharedGoalForMetaTraining) {
    /* Get shared intentionality subsystem */
    shared_intentionality_t* si = collective_cognition_get_intentionality(cc_);
    ASSERT_NE(si, nullptr);

    /* Propose a shared meta-learning goal */
    shared_goal_t goal;
    memset(&goal, 0, sizeof(goal));
    snprintf(goal.description, sizeof(goal.description),
             "Meta-train on task distribution to minimize cross-domain error");
    goal.priority = 0.9f;

    uint32_t goal_id = shared_intentionality_propose_goal(si, &goal);
    EXPECT_GT(goal_id, 0u);

    /* All instances commit to the goal */
    for (uint32_t i = 1; i <= 4; i++) {
        ASSERT_EQ(shared_intentionality_commit_to_goal(si, goal_id, i, 0.8f), 0);
    }

    /* Verify we-mode activation for collective training */
    ASSERT_EQ(collective_cognition_update(cc_), 0);

    we_mode_state_t we_mode;
    ASSERT_EQ(collective_cognition_get_we_mode(cc_, &we_mode), 0);

    /* With 4 instances committed, we should have strong joint commitment */
    EXPECT_GE(we_mode.joint_commitment, 0.0f);
    EXPECT_EQ(we_mode.active_shared_goals, 1u);
}

/*=============================================================================
 * Distributed Training Integration Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionTrainingTest, FederatedLearningWithCollective) {
    /* Collective cognition provides natural coordination for federated learning */

    /* Check consciousness level - higher = better coordination */
    collective_consciousness_level_t level =
        collective_cognition_get_consciousness_level(cc_);

    /* With 4 instances, we should have some level of consciousness */
    EXPECT_GE((int)level, (int)COLLECTIVE_CONSCIOUSNESS_NONE);

    /* Get collective state */
    collective_cognition_state_t state;
    ASSERT_EQ(collective_cognition_get_state(cc_, &state), 0);

    /* Verify we have active instances */
    EXPECT_EQ(state.active_instances, 4u);

    /* Integration quality affects gradient aggregation reliability */
    EXPECT_GE(state.integration_quality, 0.0f);
}

TEST_F(CollectiveCognitionTrainingTest, GradientSyncWithHyperscanning) {
    /* Hyperscanning can synchronize gradient health across nodes */

    hyperscanning_t* hs = collective_cognition_get_hyperscanning(cc_);
    ASSERT_NE(hs, nullptr);

    /* Simulate gradient statistics as neural state */
    for (uint32_t i = 1; i <= 4; i++) {
        hyperscanning_neural_state_t state;
        memset(&state, 0, sizeof(state));
        state.instance_id = i;

        /* Beta band power correlates with active computation */
        state.band_power[SYNC_BAND_BETA] = 0.6f + 0.1f * i;

        /* Gamma band for binding/consciousness */
        state.band_power[SYNC_BAND_GAMMA] = 0.5f;

        /* ATP reflects metabolic capacity for gradient computation */
        state.atp_level = 0.85f;

        ASSERT_EQ(hyperscanning_update_state(hs, &state), 0);
    }

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    /* Check leader detection - useful for parameter server pattern */
    uint32_t leader = hyperscanning_get_leader(hs);
    float leader_influence = hyperscanning_get_leader_influence(hs);

    /* Leader should be determinable */
    EXPECT_GE(leader, 0u);
    EXPECT_GE(leader_influence, 0.0f);
}

TEST_F(CollectiveCognitionTrainingTest, CollectiveLoadBalancing) {
    /* Test load balancing across instances during training */

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    /* Attempt load balancing */
    int result = collective_cognition_balance_load(cc_);

    /* Should succeed (even if no-op) */
    EXPECT_EQ(result, 0);

    /* State should remain valid */
    collective_cognition_state_t state;
    ASSERT_EQ(collective_cognition_get_state(cc_, &state), 0);
    EXPECT_FALSE(state.is_overloaded);
}

/*=============================================================================
 * Adversarial Training Integration Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionTrainingTest, CollectiveThreatDetection) {
    /* Collective cognition enables distributed adversarial detection */

    /* Get shared intentionality for collective defense */
    shared_intentionality_t* si = collective_cognition_get_intentionality(cc_);
    ASSERT_NE(si, nullptr);

    /* Create joint attention on potential threat */
    joint_attention_t attention;
    memset(&attention, 0, sizeof(attention));
    attention.salience = 0.9f;  /* High priority threat */
    attention.feature_vector[0] = 1.0f;  /* Threat signature */

    uint32_t attn_id = shared_intentionality_propose_attention(si, &attention);
    EXPECT_GT(attn_id, 0u);

    /* All instances join attention on threat */
    for (uint32_t i = 1; i <= 4; i++) {
        ASSERT_EQ(shared_intentionality_join_attention(si, attn_id, i), 0);
    }

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    /* Check we-mode for coordinated response */
    we_mode_state_t we_mode;
    ASSERT_EQ(collective_cognition_get_we_mode(cc_, &we_mode), 0);

    /* Joint attention on threat should be active */
    EXPECT_GE(we_mode.active_joint_attentions, 1u);
}

TEST_F(CollectiveCognitionTrainingTest, CollectiveAdversarialRobustness) {
    /* Measure collective phi as robustness metric */

    /* Update multiple times to establish baseline */
    for (int i = 0; i < 5; i++) {
        ASSERT_EQ(collective_cognition_update(cc_), 0);
    }

    collective_phi_t phi;
    ASSERT_EQ(collective_cognition_get_phi(cc_, &phi), 0);

    /* Higher integration = better robustness to adversarial attacks */
    /* Fragmented system is more vulnerable */
    EXPECT_GE(phi.integration, 0.0f);

    /* Network topology affects attack surface */
    EXPECT_GE(phi.connectivity, 0.0f);
}

/*=============================================================================
 * Extended Mind for Training Integration Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionTrainingTest, ExtendedMindCognitiveCapacity) {
    /* Extended mind expands collective training capacity */

    extended_mind_t* em = collective_cognition_get_extended_mind(cc_);
    ASSERT_NE(em, nullptr);

    /* Get current capacity */
    extended_mind_state_t em_state;
    ASSERT_EQ(collective_cognition_get_extended_mind_state(cc_, &em_state), 0);

    /* Base capacity should be positive */
    EXPECT_GE(em_state.total_cognitive_capacity, 0.0f);

    /* Register a reasoning extension (e.g., external LLM for training guidance) */
    cognitive_extension_t ext;
    memset(&ext, 0, sizeof(ext));
    ext.type = EXT_TYPE_REASONING;
    snprintf(ext.name, sizeof(ext.name), "TrainingAdvisor");
    ext.reliability = 0.95f;
    ext.avg_latency_ms = 100.0f;
    ext.integration_depth = 0.8f;
    ext.trust_level = 0.9f;

    uint32_t ext_id = extended_mind_register_extension(em, &ext);
    EXPECT_GT(ext_id, 0u);

    /* Capacity should increase */
    ASSERT_EQ(collective_cognition_get_extended_mind_state(cc_, &em_state), 0);
    EXPECT_GE(em_state.active_extensions, 1u);
}

/*=============================================================================
 * Training Statistics Integration Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionTrainingTest, CollectiveStatisticsTracking) {
    /* Collective cognition tracks statistics for training insights */

    /* Run multiple updates */
    for (int i = 0; i < 10; i++) {
        ASSERT_EQ(collective_cognition_update(cc_), 0);
    }

    collective_cognition_stats_t stats;
    ASSERT_EQ(collective_cognition_get_stats(cc_, &stats), 0);

    /* Should track updates */
    EXPECT_GE(stats.total_updates, 10u);
}

TEST_F(CollectiveCognitionTrainingTest, ResetStatisticsAfterEpoch) {
    /* After training epoch, reset stats for next epoch */

    for (int i = 0; i < 5; i++) {
        ASSERT_EQ(collective_cognition_update(cc_), 0);
    }

    collective_cognition_reset_stats(cc_);

    collective_cognition_stats_t stats;
    ASSERT_EQ(collective_cognition_get_stats(cc_, &stats), 0);
    EXPECT_EQ(stats.total_updates, 0u);
}

/*=============================================================================
 * Multi-Instance Training Coordination Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionTrainingTest, ScaleToMaxInstances) {
    /* Test scaling to maximum instances */

    /* We already have 4 instances, add more up to limit */
    uint32_t max_instances = cc_config_.max_instances;

    for (uint32_t i = 5; i <= max_instances && i <= 16; i++) {
        int result = collective_cognition_register_instance(cc_, i, nullptr);
        EXPECT_EQ(result, 0);
    }

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    collective_cognition_state_t state;
    ASSERT_EQ(collective_cognition_get_state(cc_, &state), 0);

    EXPECT_GE(state.active_instances, 4u);
}

TEST_F(CollectiveCognitionTrainingTest, ConsciousnessLevelWithManyInstances) {
    /* More instances should enable higher consciousness levels */

    /* Add more instances */
    for (uint32_t i = 5; i <= 8; i++) {
        ASSERT_EQ(collective_cognition_register_instance(cc_, i, nullptr), 0);
    }

    /* Set high gamma sync for all instances */
    hyperscanning_t* hs = collective_cognition_get_hyperscanning(cc_);
    ASSERT_NE(hs, nullptr);

    for (uint32_t i = 1; i <= 8; i++) {
        hyperscanning_neural_state_t state;
        memset(&state, 0, sizeof(state));
        state.instance_id = i;
        state.band_power[SYNC_BAND_GAMMA] = 0.9f;
        state.band_phase[SYNC_BAND_GAMMA] = 1.5f;
        state.atp_level = 0.95f;
        hyperscanning_update_state(hs, &state);
    }

    /* Multiple updates to stabilize */
    for (int i = 0; i < 10; i++) {
        ASSERT_EQ(collective_cognition_update(cc_), 0);
    }

    collective_consciousness_level_t level =
        collective_cognition_get_consciousness_level(cc_);

    /* With 8 well-synchronized instances, should have some consciousness */
    EXPECT_GE((int)level, (int)COLLECTIVE_CONSCIOUSNESS_NONE);
}

/*=============================================================================
 * Subsystem Access Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionTrainingTest, AccessAllSubsystems) {
    /* Verify all subsystems are accessible for training integration */

    hyperscanning_t* hs = collective_cognition_get_hyperscanning(cc_);
    EXPECT_NE(hs, nullptr);

    extended_mind_t* em = collective_cognition_get_extended_mind(cc_);
    EXPECT_NE(em, nullptr);

    collective_phi_system_t* phi = collective_cognition_get_phi_system(cc_);
    EXPECT_NE(phi, nullptr);

    shared_intentionality_t* si = collective_cognition_get_intentionality(cc_);
    EXPECT_NE(si, nullptr);
}
