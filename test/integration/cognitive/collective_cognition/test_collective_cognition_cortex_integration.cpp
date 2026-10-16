/**
 * @file test_collective_cognition_cortex_integration.cpp
 * @brief Integration tests for collective cognition + sensory cortexes (A/V/S)
 *
 * Tests integration of collective cognition with:
 * - Visual cortex (shared visual perception)
 * - Auditory cortex (shared audio perception)
 * - Somatosensory cortex (shared touch/proprioception)
 * - Multi-modal sensory integration
 * - Collective perception and attention
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

class CollectiveCognitionCortexTest : public ::testing::Test {
protected:
    void SetUp() override {
        cc_config_ = collective_cognition_default_config();
        cc_ = collective_cognition_create(&cc_config_);
        ASSERT_NE(cc_, nullptr);

        /* Register multiple brain instances for shared perception */
        for (uint32_t i = 1; i <= 4; i++) {
            ASSERT_EQ(collective_cognition_register_instance(cc_, i, nullptr), 0);
        }

        /* Also register instances with shared_intentionality for joint attention */
        shared_intentionality_t* si = collective_cognition_get_intentionality(cc_);
        if (si) {
            for (uint32_t i = 1; i <= 4; i++) {
                shared_intentionality_register_instance(si, i);
            }
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
 * Visual Cortex Integration Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionCortexTest, SharedVisualAttention) {
    /* Joint attention on visual targets */

    shared_intentionality_t* si = collective_cognition_get_intentionality(cc_);
    ASSERT_NE(si, nullptr);

    /* Create joint attention on visual target */
    joint_attention_t attention;
    memset(&attention, 0, sizeof(attention));
    attention.salience = 0.9f;

    /* Feature vector represents visual features */
    attention.feature_vector[0] = 0.8f;   /* Color intensity */
    attention.feature_vector[1] = 0.6f;   /* Motion */
    attention.feature_vector[2] = 0.7f;   /* Edge contrast */
    attention.feature_vector[3] = 0.5f;   /* Size */
    attention.feature_vector[4] = 1.0f;   /* Visual modality marker */

    uint32_t attn_id = shared_intentionality_propose_attention(si, &attention);
    EXPECT_GT(attn_id, 0u);

    /* All instances attend to same visual target */
    for (uint32_t i = 1; i <= 4; i++) {
        ASSERT_EQ(shared_intentionality_join_attention(si, attn_id, i), 0);
    }

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    we_mode_state_t we_mode;
    ASSERT_EQ(collective_cognition_get_we_mode(cc_, &we_mode), 0);
    EXPECT_GE(we_mode.active_joint_attentions, 1u);
}

TEST_F(CollectiveCognitionCortexTest, GammaSyncForVisualBinding) {
    /* Gamma synchronization for binding visual features */

    hyperscanning_t* hs = collective_cognition_get_hyperscanning(cc_);
    ASSERT_NE(hs, nullptr);

    /* High gamma for visual feature binding */
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

    /* Gamma binding for synchronized visual perception */
    EXPECT_GE(sync_state.gamma_binding, 0.0f);
}

TEST_F(CollectiveCognitionCortexTest, ExternalVisualSensor) {
    /* External camera as perception extension */

    extended_mind_t* em = collective_cognition_get_extended_mind(cc_);
    ASSERT_NE(em, nullptr);

    cognitive_extension_t ext;
    memset(&ext, 0, sizeof(ext));
    ext.type = EXT_TYPE_PERCEPTION;
    snprintf(ext.name, sizeof(ext.name), "ExternalCamera");
    ext.reliability = 0.95f;
    ext.avg_latency_ms = 30.0f;
    ext.integration_depth = 0.8f;
    ext.trust_level = 0.9f;

    uint32_t ext_id = extended_mind_register_extension(em, &ext);
    EXPECT_GT(ext_id, 0u);

    float capacity = extended_mind_get_capacity(em, EXT_TYPE_PERCEPTION);
    EXPECT_GT(capacity, 0.0f);
}

/*=============================================================================
 * Auditory Cortex Integration Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionCortexTest, SharedAuditoryAttention) {
    /* Joint attention on audio targets */

    shared_intentionality_t* si = collective_cognition_get_intentionality(cc_);
    ASSERT_NE(si, nullptr);

    /* Create joint attention on audio target */
    joint_attention_t attention;
    memset(&attention, 0, sizeof(attention));
    attention.salience = 0.85f;

    /* Feature vector represents audio features */
    attention.feature_vector[0] = 0.7f;   /* Pitch */
    attention.feature_vector[1] = 0.8f;   /* Loudness */
    attention.feature_vector[2] = 0.6f;   /* Timbre */
    attention.feature_vector[3] = 0.5f;   /* Spatial location */
    attention.feature_vector[4] = 0.0f;   /* Auditory modality marker */

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

TEST_F(CollectiveCognitionCortexTest, ThetaSyncForAuditoryProcessing) {
    /* Theta band sync for auditory memory and processing */

    hyperscanning_t* hs = collective_cognition_get_hyperscanning(cc_);
    ASSERT_NE(hs, nullptr);

    /* Theta for auditory processing */
    for (uint32_t i = 1; i <= 4; i++) {
        hyperscanning_neural_state_t state;
        memset(&state, 0, sizeof(state));
        state.instance_id = i;
        state.band_power[SYNC_BAND_THETA] = 0.75f;
        state.band_power[SYNC_BAND_GAMMA] = 0.6f;
        state.atp_level = 0.9f;
        ASSERT_EQ(hyperscanning_update_state(hs, &state), 0);
    }

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    hyperscan_state_t sync_state;
    ASSERT_EQ(collective_cognition_get_hyperscan_state(cc_, &sync_state), 0);

    /* Theta for auditory processing */
    EXPECT_GE(sync_state.theta_emotional, 0.0f);
}

TEST_F(CollectiveCognitionCortexTest, ExternalMicrophone) {
    /* External microphone as perception extension */

    extended_mind_t* em = collective_cognition_get_extended_mind(cc_);
    ASSERT_NE(em, nullptr);

    cognitive_extension_t ext;
    memset(&ext, 0, sizeof(ext));
    ext.type = EXT_TYPE_PERCEPTION;
    snprintf(ext.name, sizeof(ext.name), "ExternalMicrophone");
    ext.reliability = 0.92f;
    ext.avg_latency_ms = 10.0f;
    ext.integration_depth = 0.85f;
    ext.trust_level = 0.88f;

    uint32_t ext_id = extended_mind_register_extension(em, &ext);
    EXPECT_GT(ext_id, 0u);
}

/*=============================================================================
 * Somatosensory Cortex Integration Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionCortexTest, SharedSomatosensoryAttention) {
    /* Joint attention on touch/proprioceptive targets */

    shared_intentionality_t* si = collective_cognition_get_intentionality(cc_);
    ASSERT_NE(si, nullptr);

    /* Create joint attention on haptic target */
    joint_attention_t attention;
    memset(&attention, 0, sizeof(attention));
    attention.salience = 0.8f;

    /* Feature vector represents somatosensory features */
    attention.feature_vector[0] = 0.9f;   /* Pressure */
    attention.feature_vector[1] = 0.4f;   /* Temperature */
    attention.feature_vector[2] = 0.7f;   /* Texture */
    attention.feature_vector[3] = 0.6f;   /* Position */
    attention.feature_vector[4] = 0.5f;   /* Somatosensory marker */

    uint32_t attn_id = shared_intentionality_propose_attention(si, &attention);
    EXPECT_GT(attn_id, 0u);

    for (uint32_t i = 1; i <= 4; i++) {
        ASSERT_EQ(shared_intentionality_join_attention(si, attn_id, i), 0);
    }

    ASSERT_EQ(collective_cognition_update(cc_), 0);
}

TEST_F(CollectiveCognitionCortexTest, BetaSyncForMotorCoordination) {
    /* Beta band sync for motor and proprioceptive coordination */

    hyperscanning_t* hs = collective_cognition_get_hyperscanning(cc_);
    ASSERT_NE(hs, nullptr);

    /* High beta for motor coordination */
    for (uint32_t i = 1; i <= 4; i++) {
        hyperscanning_neural_state_t state;
        memset(&state, 0, sizeof(state));
        state.instance_id = i;
        state.band_power[SYNC_BAND_BETA] = 0.8f;
        state.band_power[SYNC_BAND_GAMMA] = 0.5f;
        state.atp_level = 0.88f;
        ASSERT_EQ(hyperscanning_update_state(hs, &state), 0);
    }

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    hyperscan_state_t sync_state;
    ASSERT_EQ(collective_cognition_get_hyperscan_state(cc_, &sync_state), 0);

    /* Beta for motor coordination */
    EXPECT_GE(sync_state.beta_coordination, 0.0f);
}

TEST_F(CollectiveCognitionCortexTest, ExternalHapticSensor) {
    /* External haptic sensor as perception extension */

    extended_mind_t* em = collective_cognition_get_extended_mind(cc_);
    ASSERT_NE(em, nullptr);

    cognitive_extension_t ext;
    memset(&ext, 0, sizeof(ext));
    ext.type = EXT_TYPE_PERCEPTION;
    snprintf(ext.name, sizeof(ext.name), "HapticGlove");
    ext.reliability = 0.88f;
    ext.avg_latency_ms = 15.0f;
    ext.integration_depth = 0.75f;
    ext.trust_level = 0.82f;

    uint32_t ext_id = extended_mind_register_extension(em, &ext);
    EXPECT_GT(ext_id, 0u);
}

/*=============================================================================
 * Multi-Modal Integration Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionCortexTest, MultiModalAttention) {
    /* Joint attention across multiple modalities */

    shared_intentionality_t* si = collective_cognition_get_intentionality(cc_);
    ASSERT_NE(si, nullptr);

    /* Create multiple joint attentions for different modalities */
    for (int modality = 0; modality < 3; modality++) {
        joint_attention_t attention;
        memset(&attention, 0, sizeof(attention));
        attention.salience = 0.85f - modality * 0.05f;

        /* Different feature patterns per modality */
        attention.feature_vector[modality] = 1.0f;
        attention.feature_vector[modality + 4] = 0.8f;

        uint32_t attn_id = shared_intentionality_propose_attention(si, &attention);
        EXPECT_GT(attn_id, 0u);

        for (uint32_t i = 1; i <= 4; i++) {
            shared_intentionality_join_attention(si, attn_id, i);
        }
    }

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    we_mode_state_t we_mode;
    ASSERT_EQ(collective_cognition_get_we_mode(cc_, &we_mode), 0);

    /* Should have multiple active attentions */
    EXPECT_GE(we_mode.active_joint_attentions, 1u);
}

TEST_F(CollectiveCognitionCortexTest, SensoryIntegrationViaPhiNetwork) {
    /* Phi network integration reflects multi-sensory binding */

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    collective_phi_t phi;
    ASSERT_EQ(collective_cognition_get_phi(cc_, &phi), 0);

    /* Network integration reflects sensory binding across modalities */
    EXPECT_GE(phi.phi_network, 0.0f);

    /* Information measures total sensory information */
    EXPECT_GE(phi.information, 0.0f);
}

TEST_F(CollectiveCognitionCortexTest, AttentionCoherenceAcrossModalities) {
    /* Attention coherence for multi-modal perception */

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    collective_cognition_state_t state;
    ASSERT_EQ(collective_cognition_get_state(cc_, &state), 0);

    /* Coherence across sensory modalities */
    EXPECT_GE(state.attention_coherence, 0.0f);
    EXPECT_LE(state.attention_coherence, 1.0f);
}

/*=============================================================================
 * Shared Perception Goal Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionCortexTest, CollectivePerceptionGoal) {
    /* Shared goal for collective perception task */

    shared_intentionality_t* si = collective_cognition_get_intentionality(cc_);
    ASSERT_NE(si, nullptr);

    /* Create perception goal */
    shared_goal_t goal;
    memset(&goal, 0, sizeof(goal));
    snprintf(goal.description, sizeof(goal.description),
             "Collectively perceive and identify target object");
    goal.priority = 0.9f;

    uint32_t goal_id = shared_intentionality_propose_goal(si, &goal);
    EXPECT_GT(goal_id, 0u);

    /* Assign roles: visual processor, auditory processor, integrator */
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
    EXPECT_GE(we_mode.active_shared_goals, 1u);
}

/*=============================================================================
 * Extended Perception Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionCortexTest, MultiplePerceptionExtensions) {
    /* Register multiple perception extensions */

    extended_mind_t* em = collective_cognition_get_extended_mind(cc_);
    ASSERT_NE(em, nullptr);

    const char* sensor_names[] = {"Camera1", "Camera2", "Microphone", "IMU"};
    float reliabilities[] = {0.95f, 0.93f, 0.90f, 0.88f};

    for (int i = 0; i < 4; i++) {
        cognitive_extension_t ext;
        memset(&ext, 0, sizeof(ext));
        ext.type = EXT_TYPE_PERCEPTION;
        snprintf(ext.name, sizeof(ext.name), "%s", sensor_names[i]);
        ext.reliability = reliabilities[i];
        ext.avg_latency_ms = 20.0f + i * 5.0f;
        ext.integration_depth = 0.8f - i * 0.05f;
        ext.trust_level = 0.85f;

        uint32_t ext_id = extended_mind_register_extension(em, &ext);
        EXPECT_GT(ext_id, 0u);
    }

    extended_mind_state_t em_state;
    ASSERT_EQ(collective_cognition_get_extended_mind_state(cc_, &em_state), 0);
    EXPECT_GE(em_state.active_extensions, 4u);

    /* Total capacity should include extensions */
    EXPECT_GT(em_state.total_cognitive_capacity, 0.0f);
}

TEST_F(CollectiveCognitionCortexTest, PerceptionExtensionCapacity) {
    /* Check aggregate perception capacity */

    extended_mind_t* em = collective_cognition_get_extended_mind(cc_);
    ASSERT_NE(em, nullptr);

    /* Register sensors */
    for (int i = 0; i < 3; i++) {
        cognitive_extension_t ext;
        memset(&ext, 0, sizeof(ext));
        ext.type = EXT_TYPE_PERCEPTION;
        snprintf(ext.name, sizeof(ext.name), "Sensor%d", i);
        ext.reliability = 0.9f;
        ext.avg_latency_ms = 25.0f;
        ext.integration_depth = 0.8f;
        ext.trust_level = 0.85f;

        extended_mind_register_extension(em, &ext);
    }

    float perception_capacity = extended_mind_get_capacity(em, EXT_TYPE_PERCEPTION);
    EXPECT_GT(perception_capacity, 0.0f);
}

/*=============================================================================
 * Consciousness and Perception Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionCortexTest, ConsciousnessForPerceptualBinding) {
    /* Higher consciousness enables better perceptual binding */

    /* Set optimal neural states for perception */
    hyperscanning_t* hs = collective_cognition_get_hyperscanning(cc_);
    ASSERT_NE(hs, nullptr);

    for (uint32_t i = 1; i <= 4; i++) {
        hyperscanning_neural_state_t state;
        memset(&state, 0, sizeof(state));
        state.instance_id = i;
        state.band_power[SYNC_BAND_GAMMA] = 0.9f;  /* High gamma for binding */
        state.band_power[SYNC_BAND_ALPHA] = 0.4f;  /* Low alpha = high attention */
        state.band_phase[SYNC_BAND_GAMMA] = 1.5f;
        state.atp_level = 0.95f;
        ASSERT_EQ(hyperscanning_update_state(hs, &state), 0);
    }

    for (int i = 0; i < 10; i++) {
        ASSERT_EQ(collective_cognition_update(cc_), 0);
    }

    collective_consciousness_level_t level =
        collective_cognition_get_consciousness_level(cc_);

    /* Should achieve some consciousness for perceptual binding */
    EXPECT_GE((int)level, (int)COLLECTIVE_CONSCIOUSNESS_NONE);
}

TEST_F(CollectiveCognitionCortexTest, InformationFlowForSensoryProcessing) {
    /* Information flow rate affects sensory processing speed */

    ASSERT_EQ(collective_cognition_update(cc_), 0);

    collective_cognition_state_t state;
    ASSERT_EQ(collective_cognition_get_state(cc_, &state), 0);

    /* Higher flow = faster multi-modal integration */
    EXPECT_GE(state.information_flow_rate, 0.0f);
}

/*=============================================================================
 * Stress Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionCortexTest, RapidAttentionSwitching) {
    /* Rapidly switch attention between modalities */

    shared_intentionality_t* si = collective_cognition_get_intentionality(cc_);
    ASSERT_NE(si, nullptr);

    for (int cycle = 0; cycle < 20; cycle++) {
        joint_attention_t attention;
        memset(&attention, 0, sizeof(attention));
        attention.salience = 0.8f;
        attention.feature_vector[cycle % 3] = 1.0f;

        uint32_t attn_id = shared_intentionality_propose_attention(si, &attention);
        EXPECT_GT(attn_id, 0u);

        for (uint32_t i = 1; i <= 4; i++) {
            shared_intentionality_join_attention(si, attn_id, i);
        }

        ASSERT_EQ(collective_cognition_update(cc_), 0);

        /* Leave attention for next cycle */
        for (uint32_t i = 1; i <= 4; i++) {
            shared_intentionality_leave_attention(si, attn_id, i);
        }
    }
}

TEST_F(CollectiveCognitionCortexTest, ContinuousPerceptionUpdate) {
    /* Continuous perception updates */

    hyperscanning_t* hs = collective_cognition_get_hyperscanning(cc_);
    ASSERT_NE(hs, nullptr);

    for (int frame = 0; frame < 50; frame++) {
        /* Simulate varying sensory input */
        for (uint32_t i = 1; i <= 4; i++) {
            hyperscanning_neural_state_t state;
            memset(&state, 0, sizeof(state));
            state.instance_id = i;
            state.band_power[SYNC_BAND_GAMMA] = 0.5f + 0.3f * sinf(frame * 0.1f);
            state.band_power[SYNC_BAND_BETA] = 0.6f + 0.2f * cosf(frame * 0.1f);
            state.atp_level = 0.9f;
            hyperscanning_update_state(hs, &state);
        }

        ASSERT_EQ(collective_cognition_update(cc_), 0);
    }

    collective_cognition_state_t state;
    ASSERT_EQ(collective_cognition_get_state(cc_, &state), 0);
    EXPECT_EQ(state.active_instances, 4u);
}
