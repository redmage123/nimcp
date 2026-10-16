/**
 * @file test_omni_wm_tom_plasticity_integration.cpp
 * @brief Integration tests for World Model Theory of Mind + Plasticity bridge integration
 * @version 1.0.0
 * @date 2026-01-24
 *
 * Tests cross-bridge data flow between ToM and Plasticity bridges:
 * - Belief prediction -> Learning rate modulation -> Social learning
 * - Mirror neuron activation -> STDP modulation -> Action understanding
 * - Perspective taking -> Predictive coding -> Surprise-based plasticity
 * - Mental state attribution -> Eligibility traces -> Social reward learning
 * - Intentionality detection -> BCM plasticity -> Goal inference
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <cstdlib>
#include <vector>
#include <algorithm>

/* World Model core */
#include "cognitive/omni/nimcp_omni_world_model.h"

/* ToM and Plasticity bridges */
#include "cognitive/omni/bridges/nimcp_omni_wm_tom_bridge.h"
#include "cognitive/omni/bridges/nimcp_omni_wm_plasticity_bridge.h"

/* Memory bridge for context */
#include "cognitive/omni/bridges/nimcp_omni_wm_memory_bridge.h"

/* Cognitive bridge for goals */
#include "cognitive/omni/bridges/nimcp_omni_wm_cognitive_bridge.h"

/* Logging for audit trail */
#include "cognitive/omni/bridges/nimcp_omni_wm_logging_bridge.h"

/* Bio-async for message routing */
#include "async/nimcp_bio_messages.h"

/* Memory utilities */
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * Test Fixture for ToM-Plasticity Bridge Integration
 * ============================================================================ */

class WMToMPlasticityTest : public ::testing::Test {
protected:
    static constexpr uint32_t STATE_DIM = 64;
    static constexpr uint32_t ACTION_DIM = 16;
    static constexpr uint32_t OBS_DIM = 64;
    static constexpr uint32_t MAX_AGENTS = 8;
    static constexpr float DT = 0.016f;

    /* Core components */
    omni_world_model_t* wm = nullptr;

    /* Bridges */
    omni_wm_tom_bridge_t* tom_bridge = nullptr;
    omni_wm_plasticity_bridge_t* plasticity_bridge = nullptr;
    omni_wm_memory_bridge_t* memory_bridge = nullptr;
    omni_wm_cognitive_bridge_t* cognitive_bridge = nullptr;
    omni_wm_logging_bridge_t* logging_bridge = nullptr;

    void SetUp() override {
        /* Create world model */
        omni_wm_config_t wm_config;
        omni_wm_get_default_config(&wm_config);
        wm_config.state_dim = STATE_DIM;
        wm_config.action_dim = ACTION_DIM;
        wm_config.obs_dim = OBS_DIM;
        wm_config.use_rssm = true;
        wm_config.enable_dreaming = true;
        wm_config.replay_buffer_size = 1000;

        wm = omni_wm_create(&wm_config);
    }

    void TearDown() override {
        if (logging_bridge) omni_wm_logging_bridge_destroy(logging_bridge);
        if (cognitive_bridge) omni_wm_cognitive_bridge_destroy(cognitive_bridge);
        if (memory_bridge) omni_wm_memory_bridge_destroy(memory_bridge);
        if (plasticity_bridge) omni_wm_plasticity_bridge_destroy(plasticity_bridge);
        if (tom_bridge) omni_wm_tom_bridge_destroy(tom_bridge);
        if (wm) omni_wm_destroy(wm);
    }

    void CreateBridges() {
        /* ToM Bridge */
        omni_wm_tom_bridge_config_t tom_config;
        omni_wm_tom_bridge_default_config(&tom_config);
        tom_config.enable_belief_prediction = true;
        tom_config.enable_mirror_neurons = true;
        tom_config.enable_perspective_taking = true;
        tom_config.enable_intentionality = true;
        tom_config.enable_mental_state_attribution = true;
        tom_config.max_tracked_agents = MAX_AGENTS;
        tom_config.social_learning_enabled = true;

        tom_bridge = omni_wm_tom_bridge_create(&tom_config);
        if (tom_bridge && wm) {
            omni_wm_tom_bridge_connect_world_model(tom_bridge, wm);
        }

        /* Plasticity Bridge */
        omni_wm_plasticity_bridge_config_t plast_config;
        omni_wm_plasticity_bridge_default_config(&plast_config);
        plast_config.enable_stdp = true;
        plast_config.enable_bcm = true;
        plast_config.enable_eligibility_traces = true;
        plast_config.enable_stp = true;
        plast_config.enable_neuromodulation = true;
        plast_config.enable_metaplasticity = true;
        plast_config.surprise_modulation_enabled = true;
        plast_config.stdp_time_window_ms = 20.0f;
        plast_config.bcm_threshold = 0.5f;

        plasticity_bridge = omni_wm_plasticity_bridge_create(&plast_config);
        if (plasticity_bridge && wm) {
            omni_wm_plasticity_bridge_connect_world_model(plasticity_bridge, wm);
        }

        /* Memory Bridge for episodic context */
        omni_wm_memory_bridge_config_t mem_config;
        omni_wm_memory_bridge_default_config(&mem_config);
        memory_bridge = omni_wm_memory_bridge_create(&mem_config);
        if (memory_bridge && wm) {
            omni_wm_memory_bridge_connect_world_model(memory_bridge, wm);
        }

        /* Cognitive Bridge for goals */
        omni_wm_cognitive_bridge_config_t cog_config = omni_wm_cognitive_bridge_default_config();
        cognitive_bridge = omni_wm_cognitive_bridge_create(&cog_config);
        if (cognitive_bridge && wm) {
            omni_wm_cognitive_bridge_connect_world_model(cognitive_bridge, wm);
        }

        /* Logging Bridge */
        omni_wm_logging_bridge_config_t log_config;
        omni_wm_logging_bridge_default_config(&log_config);
        logging_bridge = omni_wm_logging_bridge_create(&log_config);
        if (logging_bridge && wm) {
            omni_wm_logging_bridge_connect_world_model(logging_bridge, wm);
        }
    }

    void generate_random_pattern(float* pattern, uint32_t dim) {
        for (uint32_t i = 0; i < dim; i++) {
            pattern[i] = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
        }
    }

    void normalize_pattern(float* pattern, uint32_t dim) {
        float sum_sq = 0.0f;
        for (uint32_t i = 0; i < dim; i++) {
            sum_sq += pattern[i] * pattern[i];
        }
        if (sum_sq > 0.0f) {
            float norm = sqrtf(sum_sq);
            for (uint32_t i = 0; i < dim; i++) {
                pattern[i] /= norm;
            }
        }
    }

    /* Create simulated agent state */
    void create_agent_state(
        wm_tom_agent_state_t* agent,
        uint32_t agent_id,
        float confidence
    ) {
        memset(agent, 0, sizeof(wm_tom_agent_state_t));
        agent->agent_id = agent_id;
        generate_random_pattern(agent->mental_state, STATE_DIM);
        agent->state_dim = STATE_DIM;
        agent->confidence = confidence;
        agent->is_active = true;
    }
};

/* ============================================================================
 * Basic Bridge Setup Tests
 * ============================================================================ */

TEST_F(WMToMPlasticityTest, BridgeCreation) {
    ASSERT_NE(wm, nullptr);

    CreateBridges();

    EXPECT_NE(tom_bridge, nullptr);
    EXPECT_NE(plasticity_bridge, nullptr);
}

TEST_F(WMToMPlasticityTest, BridgeConnections) {
    if (!wm) GTEST_SKIP();

    CreateBridges();

    if (tom_bridge) {
        EXPECT_TRUE(omni_wm_tom_bridge_is_connected(tom_bridge));
    }
    if (plasticity_bridge) {
        EXPECT_TRUE(omni_wm_plasticity_bridge_is_connected(plasticity_bridge));
    }
}

/* ============================================================================
 * Agent Tracking Tests
 * ============================================================================ */

TEST_F(WMToMPlasticityTest, AgentRegistration) {
    if (!wm) GTEST_SKIP();

    CreateBridges();
    if (!tom_bridge) GTEST_SKIP();

    /* Register an agent */
    wm_tom_agent_state_t agent;
    create_agent_state(&agent, 1, 0.8f);

    nimcp_error_t ret = omni_wm_tom_bridge_register_agent(
        tom_bridge,
        &agent
    );
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    /* Get agent count */
    uint32_t num_agents = omni_wm_tom_bridge_get_agent_count(tom_bridge);
    EXPECT_GE(num_agents, 1u);
}

TEST_F(WMToMPlasticityTest, MultipleAgentRegistration) {
    if (!wm) GTEST_SKIP();

    CreateBridges();
    if (!tom_bridge) GTEST_SKIP();

    /* Register multiple agents */
    for (uint32_t i = 1; i <= 5; i++) {
        wm_tom_agent_state_t agent;
        create_agent_state(&agent, i, 0.5f + (float)i * 0.05f);

        nimcp_error_t ret = omni_wm_tom_bridge_register_agent(
            tom_bridge,
            &agent
        );
        EXPECT_EQ(ret, NIMCP_SUCCESS);
    }

    uint32_t num_agents = omni_wm_tom_bridge_get_agent_count(tom_bridge);
    EXPECT_EQ(num_agents, 5u);
}

TEST_F(WMToMPlasticityTest, AgentUnregistration) {
    if (!wm) GTEST_SKIP();

    CreateBridges();
    if (!tom_bridge) GTEST_SKIP();

    /* Register agent */
    wm_tom_agent_state_t agent;
    create_agent_state(&agent, 1, 0.8f);
    omni_wm_tom_bridge_register_agent(tom_bridge, &agent);

    uint32_t count_before = omni_wm_tom_bridge_get_agent_count(tom_bridge);

    /* Unregister agent */
    nimcp_error_t ret = omni_wm_tom_bridge_unregister_agent(tom_bridge, 1);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    uint32_t count_after = omni_wm_tom_bridge_get_agent_count(tom_bridge);
    EXPECT_EQ(count_after, count_before - 1);
}

/* ============================================================================
 * Belief Prediction Tests
 * ============================================================================ */

TEST_F(WMToMPlasticityTest, BeliefPrediction) {
    if (!wm) GTEST_SKIP();

    CreateBridges();
    if (!tom_bridge) GTEST_SKIP();

    /* Register agent first */
    wm_tom_agent_state_t agent;
    create_agent_state(&agent, 1, 0.8f);
    omni_wm_tom_bridge_register_agent(tom_bridge, &agent);

    /* Predict agent's belief */
    float predicted_belief[STATE_DIM];
    float confidence;
    nimcp_error_t ret = omni_wm_tom_bridge_predict_belief(
        tom_bridge,
        1,  /* agent_id */
        predicted_belief,
        STATE_DIM,
        &confidence
    );
    EXPECT_EQ(ret, NIMCP_SUCCESS);
    EXPECT_GE(confidence, 0.0f);
    EXPECT_LE(confidence, 1.0f);

    /* Get stats */
    omni_wm_tom_bridge_stats_t stats;
    omni_wm_tom_bridge_get_stats(tom_bridge, &stats);
    EXPECT_GE(stats.belief_predictions, 1u);
}

TEST_F(WMToMPlasticityTest, BeliefUpdate) {
    if (!wm) GTEST_SKIP();

    CreateBridges();
    if (!tom_bridge) GTEST_SKIP();

    /* Register agent */
    wm_tom_agent_state_t agent;
    create_agent_state(&agent, 1, 0.8f);
    omni_wm_tom_bridge_register_agent(tom_bridge, &agent);

    /* Observe agent behavior */
    float observed_action[ACTION_DIM];
    generate_random_pattern(observed_action, ACTION_DIM);

    nimcp_error_t ret = omni_wm_tom_bridge_update_belief(
        tom_bridge,
        1,  /* agent_id */
        observed_action,
        ACTION_DIM,
        0.5f  /* observation confidence */
    );
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    /* Get stats */
    omni_wm_tom_bridge_stats_t stats;
    omni_wm_tom_bridge_get_stats(tom_bridge, &stats);
    EXPECT_GE(stats.belief_updates, 1u);
}

/* ============================================================================
 * Mirror Neuron Activation Tests
 * ============================================================================ */

TEST_F(WMToMPlasticityTest, MirrorNeuronActivation) {
    if (!wm) GTEST_SKIP();

    CreateBridges();
    if (!tom_bridge) GTEST_SKIP();

    /* Observe action to trigger mirror neurons */
    float observed_action[ACTION_DIM];
    generate_random_pattern(observed_action, ACTION_DIM);
    normalize_pattern(observed_action, ACTION_DIM);

    float mirror_activation[STATE_DIM];
    nimcp_error_t ret = omni_wm_tom_bridge_mirror_activate(
        tom_bridge,
        observed_action,
        ACTION_DIM,
        mirror_activation,
        STATE_DIM
    );
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    /* Verify non-zero activation */
    float sum = 0.0f;
    for (uint32_t i = 0; i < STATE_DIM; i++) {
        sum += fabsf(mirror_activation[i]);
    }
    EXPECT_GT(sum, 0.0f);

    /* Get stats */
    omni_wm_tom_bridge_stats_t stats;
    omni_wm_tom_bridge_get_stats(tom_bridge, &stats);
    EXPECT_GE(stats.mirror_activations, 1u);
}

TEST_F(WMToMPlasticityTest, MirrorNeuronSequence) {
    if (!wm) GTEST_SKIP();

    CreateBridges();
    if (!tom_bridge) GTEST_SKIP();

    /* Observe sequence of actions */
    for (int i = 0; i < 10; i++) {
        float action[ACTION_DIM];
        generate_random_pattern(action, ACTION_DIM);

        float activation[STATE_DIM];
        omni_wm_tom_bridge_mirror_activate(
            tom_bridge,
            action,
            ACTION_DIM,
            activation,
            STATE_DIM
        );

        omni_wm_tom_bridge_update(tom_bridge, DT);
    }

    omni_wm_tom_bridge_stats_t stats;
    omni_wm_tom_bridge_get_stats(tom_bridge, &stats);
    EXPECT_EQ(stats.mirror_activations, 10u);
}

/* ============================================================================
 * Perspective Taking Tests
 * ============================================================================ */

TEST_F(WMToMPlasticityTest, PerspectiveTaking) {
    if (!wm) GTEST_SKIP();

    CreateBridges();
    if (!tom_bridge) GTEST_SKIP();

    /* Register agent */
    wm_tom_agent_state_t agent;
    create_agent_state(&agent, 1, 0.8f);
    omni_wm_tom_bridge_register_agent(tom_bridge, &agent);

    /* Take agent's perspective */
    float perspective_state[STATE_DIM];
    float perspective_confidence;
    nimcp_error_t ret = omni_wm_tom_bridge_take_perspective(
        tom_bridge,
        1,  /* agent_id */
        perspective_state,
        STATE_DIM,
        &perspective_confidence
    );
    EXPECT_EQ(ret, NIMCP_SUCCESS);
    EXPECT_GE(perspective_confidence, 0.0f);

    /* Get stats */
    omni_wm_tom_bridge_stats_t stats;
    omni_wm_tom_bridge_get_stats(tom_bridge, &stats);
    EXPECT_GE(stats.perspective_takes, 1u);
}

/* ============================================================================
 * Intentionality Detection Tests
 * ============================================================================ */

TEST_F(WMToMPlasticityTest, IntentionalityDetection) {
    if (!wm) GTEST_SKIP();

    CreateBridges();
    if (!tom_bridge) GTEST_SKIP();

    /* Register agent */
    wm_tom_agent_state_t agent;
    create_agent_state(&agent, 1, 0.8f);
    omni_wm_tom_bridge_register_agent(tom_bridge, &agent);

    /* Observe behavior sequence for intentionality */
    for (int i = 0; i < 5; i++) {
        float action[ACTION_DIM];
        generate_random_pattern(action, ACTION_DIM);
        omni_wm_tom_bridge_update_belief(
            tom_bridge, 1, action, ACTION_DIM, 0.7f
        );
    }

    /* Infer intention */
    float inferred_goal[STATE_DIM];
    float intentionality_score;
    nimcp_error_t ret = omni_wm_tom_bridge_infer_intention(
        tom_bridge,
        1,  /* agent_id */
        inferred_goal,
        STATE_DIM,
        &intentionality_score
    );
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    omni_wm_tom_bridge_stats_t stats;
    omni_wm_tom_bridge_get_stats(tom_bridge, &stats);
    EXPECT_GE(stats.intention_inferences, 1u);
}

/* ============================================================================
 * Mental State Attribution Tests
 * ============================================================================ */

TEST_F(WMToMPlasticityTest, MentalStateAttribution) {
    if (!wm) GTEST_SKIP();

    CreateBridges();
    if (!tom_bridge) GTEST_SKIP();

    /* Register agent */
    wm_tom_agent_state_t agent;
    create_agent_state(&agent, 1, 0.8f);
    omni_wm_tom_bridge_register_agent(tom_bridge, &agent);

    /* Attribute mental state */
    wm_tom_mental_state_t mental_state;
    memset(&mental_state, 0, sizeof(mental_state));

    nimcp_error_t ret = omni_wm_tom_bridge_attribute_mental_state(
        tom_bridge,
        1,  /* agent_id */
        &mental_state
    );
    EXPECT_EQ(ret, NIMCP_SUCCESS);
    EXPECT_GE(mental_state.confidence, 0.0f);
}

/* ============================================================================
 * STDP Modulation Tests
 * ============================================================================ */

TEST_F(WMToMPlasticityTest, STDPModulation) {
    if (!wm) GTEST_SKIP();

    CreateBridges();
    if (!plasticity_bridge) GTEST_SKIP();

    /* Create pre/post spike patterns */
    float pre_spike[STATE_DIM], post_spike[STATE_DIM];
    generate_random_pattern(pre_spike, STATE_DIM);
    generate_random_pattern(post_spike, STATE_DIM);

    /* Apply STDP */
    nimcp_error_t ret = omni_wm_plasticity_bridge_apply_stdp(
        plasticity_bridge,
        pre_spike,
        STATE_DIM,
        post_spike,
        STATE_DIM,
        5.0f  /* timing: post after pre -> LTP */
    );
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    /* Get stats */
    omni_wm_plasticity_bridge_stats_t stats;
    omni_wm_plasticity_bridge_get_stats(plasticity_bridge, &stats);
    EXPECT_GE(stats.stdp_updates, 1u);
}

TEST_F(WMToMPlasticityTest, STDPTimingDependence) {
    if (!wm) GTEST_SKIP();

    CreateBridges();
    if (!plasticity_bridge) GTEST_SKIP();

    float pre_spike[STATE_DIM], post_spike[STATE_DIM];
    generate_random_pattern(pre_spike, STATE_DIM);
    generate_random_pattern(post_spike, STATE_DIM);

    /* Apply STDP with LTP timing (post after pre) */
    omni_wm_plasticity_bridge_apply_stdp(
        plasticity_bridge,
        pre_spike, STATE_DIM,
        post_spike, STATE_DIM,
        5.0f  /* LTP timing */
    );

    /* Apply STDP with LTD timing (post before pre) */
    omni_wm_plasticity_bridge_apply_stdp(
        plasticity_bridge,
        pre_spike, STATE_DIM,
        post_spike, STATE_DIM,
        -5.0f  /* LTD timing */
    );

    omni_wm_plasticity_bridge_stats_t stats;
    omni_wm_plasticity_bridge_get_stats(plasticity_bridge, &stats);
    EXPECT_GE(stats.stdp_updates, 2u);
}

/* ============================================================================
 * BCM Plasticity Tests
 * ============================================================================ */

TEST_F(WMToMPlasticityTest, BCMPlasticity) {
    if (!wm) GTEST_SKIP();

    CreateBridges();
    if (!plasticity_bridge) GTEST_SKIP();

    /* Apply BCM rule */
    float input[STATE_DIM], output[STATE_DIM];
    generate_random_pattern(input, STATE_DIM);
    generate_random_pattern(output, STATE_DIM);

    nimcp_error_t ret = omni_wm_plasticity_bridge_apply_bcm(
        plasticity_bridge,
        input,
        STATE_DIM,
        output,
        STATE_DIM
    );
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    omni_wm_plasticity_bridge_stats_t stats;
    omni_wm_plasticity_bridge_get_stats(plasticity_bridge, &stats);
    EXPECT_GE(stats.bcm_updates, 1u);
}

TEST_F(WMToMPlasticityTest, BCMThresholdUpdate) {
    if (!wm) GTEST_SKIP();

    CreateBridges();
    if (!plasticity_bridge) GTEST_SKIP();

    /* Get current threshold */
    float threshold_before = omni_wm_plasticity_bridge_get_bcm_threshold(plasticity_bridge);

    /* Apply multiple BCM updates with varying activity */
    for (int i = 0; i < 20; i++) {
        float input[STATE_DIM], output[STATE_DIM];
        generate_random_pattern(input, STATE_DIM);
        generate_random_pattern(output, STATE_DIM);

        /* Scale to simulate varying activity levels */
        float scale = 0.5f + (float)i / 20.0f;
        for (uint32_t j = 0; j < STATE_DIM; j++) {
            output[j] *= scale;
        }

        omni_wm_plasticity_bridge_apply_bcm(
            plasticity_bridge,
            input, STATE_DIM,
            output, STATE_DIM
        );
    }

    /* Threshold may have adapted */
    float threshold_after = omni_wm_plasticity_bridge_get_bcm_threshold(plasticity_bridge);
    /* Just verify threshold is valid (may or may not have changed) */
    EXPECT_GT(threshold_after, 0.0f);
}

/* ============================================================================
 * Eligibility Trace Tests
 * ============================================================================ */

TEST_F(WMToMPlasticityTest, EligibilityTraceAccumulation) {
    if (!wm) GTEST_SKIP();

    CreateBridges();
    if (!plasticity_bridge) GTEST_SKIP();

    /* Record eligibility trace */
    float trace_pattern[STATE_DIM];
    generate_random_pattern(trace_pattern, STATE_DIM);

    uint32_t trace_id;
    nimcp_error_t ret = omni_wm_plasticity_bridge_record_eligibility(
        plasticity_bridge,
        trace_pattern,
        STATE_DIM,
        &trace_id
    );
    EXPECT_EQ(ret, NIMCP_SUCCESS);
    EXPECT_GT(trace_id, 0u);

    omni_wm_plasticity_bridge_stats_t stats;
    omni_wm_plasticity_bridge_get_stats(plasticity_bridge, &stats);
    EXPECT_GE(stats.eligibility_traces_created, 1u);
}

TEST_F(WMToMPlasticityTest, EligibilityTraceReward) {
    if (!wm) GTEST_SKIP();

    CreateBridges();
    if (!plasticity_bridge) GTEST_SKIP();

    /* Record trace */
    float trace[STATE_DIM];
    generate_random_pattern(trace, STATE_DIM);

    uint32_t trace_id;
    omni_wm_plasticity_bridge_record_eligibility(
        plasticity_bridge,
        trace,
        STATE_DIM,
        &trace_id
    );

    /* Apply reward signal */
    nimcp_error_t ret = omni_wm_plasticity_bridge_apply_reward(
        plasticity_bridge,
        trace_id,
        1.0f  /* positive reward */
    );
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    omni_wm_plasticity_bridge_stats_t stats;
    omni_wm_plasticity_bridge_get_stats(plasticity_bridge, &stats);
    EXPECT_GE(stats.eligibility_traces_reinforced, 1u);
}

TEST_F(WMToMPlasticityTest, EligibilityTraceDecay) {
    if (!wm) GTEST_SKIP();

    CreateBridges();
    if (!plasticity_bridge) GTEST_SKIP();

    /* Record trace */
    float trace[STATE_DIM];
    generate_random_pattern(trace, STATE_DIM);

    uint32_t trace_id;
    omni_wm_plasticity_bridge_record_eligibility(
        plasticity_bridge,
        trace,
        STATE_DIM,
        &trace_id
    );

    /* Let time pass - trace should decay */
    for (int i = 0; i < 100; i++) {
        omni_wm_plasticity_bridge_update(plasticity_bridge, DT);
    }

    /* Get trace strength (should have decayed) */
    float strength = omni_wm_plasticity_bridge_get_trace_strength(
        plasticity_bridge,
        trace_id
    );
    /* Strength should be reduced but still valid */
    EXPECT_LT(strength, 1.0f);  /* Assuming initial strength was 1.0 */
}

/* ============================================================================
 * Short-Term Plasticity (STP) Tests
 * ============================================================================ */

TEST_F(WMToMPlasticityTest, STPFacilitation) {
    if (!wm) GTEST_SKIP();

    CreateBridges();
    if (!plasticity_bridge) GTEST_SKIP();

    /* Apply repeated stimulation for facilitation */
    float stim[STATE_DIM];
    generate_random_pattern(stim, STATE_DIM);

    for (int i = 0; i < 5; i++) {
        nimcp_error_t ret = omni_wm_plasticity_bridge_apply_stp(
            plasticity_bridge,
            stim,
            STATE_DIM,
            10.0f  /* short ISI for facilitation */
        );
        EXPECT_EQ(ret, NIMCP_SUCCESS);
    }

    omni_wm_plasticity_bridge_stats_t stats;
    omni_wm_plasticity_bridge_get_stats(plasticity_bridge, &stats);
    EXPECT_GE(stats.stp_updates, 5u);
}

TEST_F(WMToMPlasticityTest, STPDepression) {
    if (!wm) GTEST_SKIP();

    CreateBridges();
    if (!plasticity_bridge) GTEST_SKIP();

    /* Apply rapid repeated stimulation for depression */
    float stim[STATE_DIM];
    generate_random_pattern(stim, STATE_DIM);

    for (int i = 0; i < 20; i++) {
        omni_wm_plasticity_bridge_apply_stp(
            plasticity_bridge,
            stim,
            STATE_DIM,
            2.0f  /* very short ISI - should cause depression */
        );
    }

    /* Just verify updates happened - depression effects are internal */
    omni_wm_plasticity_bridge_stats_t stats;
    omni_wm_plasticity_bridge_get_stats(plasticity_bridge, &stats);
    EXPECT_GE(stats.stp_updates, 20u);
}

/* ============================================================================
 * Neuromodulation Tests
 * ============================================================================ */

TEST_F(WMToMPlasticityTest, NeuromodulationUpdate) {
    if (!wm) GTEST_SKIP();

    CreateBridges();
    if (!plasticity_bridge) GTEST_SKIP();

    /* Set neuromodulator levels */
    nimcp_error_t ret = omni_wm_plasticity_bridge_set_neuromodulation(
        plasticity_bridge,
        0.7f,  /* dopamine */
        0.5f,  /* serotonin */
        0.6f,  /* norepinephrine */
        0.4f   /* acetylcholine */
    );
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    /* Run updates with neuromodulation active */
    for (int i = 0; i < 10; i++) {
        omni_wm_plasticity_bridge_update(plasticity_bridge, DT);
    }

    omni_wm_plasticity_bridge_stats_t stats;
    omni_wm_plasticity_bridge_get_stats(plasticity_bridge, &stats);
    EXPECT_GT(stats.neuromodulation_events, 0u);
}

TEST_F(WMToMPlasticityTest, SurpriseModulation) {
    if (!wm) GTEST_SKIP();

    CreateBridges();
    if (!plasticity_bridge) GTEST_SKIP();

    /* Signal high surprise */
    nimcp_error_t ret = omni_wm_plasticity_bridge_signal_surprise(
        plasticity_bridge,
        0.9f  /* high surprise */
    );
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    /* Apply plasticity - should be enhanced by surprise */
    float pre[STATE_DIM], post[STATE_DIM];
    generate_random_pattern(pre, STATE_DIM);
    generate_random_pattern(post, STATE_DIM);

    omni_wm_plasticity_bridge_apply_stdp(
        plasticity_bridge,
        pre, STATE_DIM,
        post, STATE_DIM,
        5.0f
    );

    omni_wm_plasticity_bridge_stats_t stats;
    omni_wm_plasticity_bridge_get_stats(plasticity_bridge, &stats);
    EXPECT_GE(stats.surprise_modulations, 1u);
}

/* ============================================================================
 * Metaplasticity Tests
 * ============================================================================ */

TEST_F(WMToMPlasticityTest, MetaplasticityUpdate) {
    if (!wm) GTEST_SKIP();

    CreateBridges();
    if (!plasticity_bridge) GTEST_SKIP();

    /* Get initial metaplasticity state */
    float initial_lr = omni_wm_plasticity_bridge_get_effective_lr(plasticity_bridge);

    /* Trigger many plasticity events to induce metaplasticity */
    for (int i = 0; i < 50; i++) {
        float pre[STATE_DIM], post[STATE_DIM];
        generate_random_pattern(pre, STATE_DIM);
        generate_random_pattern(post, STATE_DIM);

        omni_wm_plasticity_bridge_apply_stdp(
            plasticity_bridge,
            pre, STATE_DIM,
            post, STATE_DIM,
            5.0f
        );

        omni_wm_plasticity_bridge_update(plasticity_bridge, DT);
    }

    /* Effective learning rate may have changed */
    float final_lr = omni_wm_plasticity_bridge_get_effective_lr(plasticity_bridge);
    /* Just verify it's still valid */
    EXPECT_GT(final_lr, 0.0f);
}

/* ============================================================================
 * ToM-Plasticity Cross-Bridge Integration Tests
 * ============================================================================ */

TEST_F(WMToMPlasticityTest, MirrorNeuronTriggersSTDP) {
    if (!wm) GTEST_SKIP();

    CreateBridges();
    if (!tom_bridge || !plasticity_bridge) GTEST_SKIP();

    /* Register agent */
    wm_tom_agent_state_t agent;
    create_agent_state(&agent, 1, 0.8f);
    omni_wm_tom_bridge_register_agent(tom_bridge, &agent);

    /* Observe action - activates mirror neurons */
    float observed_action[ACTION_DIM];
    generate_random_pattern(observed_action, ACTION_DIM);

    float mirror_activation[STATE_DIM];
    omni_wm_tom_bridge_mirror_activate(
        tom_bridge,
        observed_action,
        ACTION_DIM,
        mirror_activation,
        STATE_DIM
    );

    /* Use mirror activation as input to STDP */
    float motor_pattern[STATE_DIM];
    generate_random_pattern(motor_pattern, STATE_DIM);

    omni_wm_plasticity_bridge_apply_stdp(
        plasticity_bridge,
        mirror_activation,
        STATE_DIM,
        motor_pattern,
        STATE_DIM,
        10.0f  /* causal timing */
    );

    /* Verify both systems recorded activity */
    omni_wm_tom_bridge_stats_t tom_stats;
    omni_wm_plasticity_bridge_stats_t plast_stats;
    omni_wm_tom_bridge_get_stats(tom_bridge, &tom_stats);
    omni_wm_plasticity_bridge_get_stats(plasticity_bridge, &plast_stats);

    EXPECT_GE(tom_stats.mirror_activations, 1u);
    EXPECT_GE(plast_stats.stdp_updates, 1u);
}

TEST_F(WMToMPlasticityTest, SocialLearningWithEligibility) {
    if (!wm) GTEST_SKIP();

    CreateBridges();
    if (!tom_bridge || !plasticity_bridge) GTEST_SKIP();

    /* Register agent */
    wm_tom_agent_state_t agent;
    create_agent_state(&agent, 1, 0.8f);
    omni_wm_tom_bridge_register_agent(tom_bridge, &agent);

    /* Observe agent behavior sequence */
    for (int i = 0; i < 5; i++) {
        float action[ACTION_DIM];
        generate_random_pattern(action, ACTION_DIM);

        /* Update ToM belief */
        omni_wm_tom_bridge_update_belief(
            tom_bridge, 1, action, ACTION_DIM, 0.7f
        );

        /* Record eligibility trace for observed behavior */
        float observation[STATE_DIM];
        generate_random_pattern(observation, STATE_DIM);

        uint32_t trace_id;
        omni_wm_plasticity_bridge_record_eligibility(
            plasticity_bridge,
            observation,
            STATE_DIM,
            &trace_id
        );
    }

    /* Social reward signal - agent achieved goal */
    float social_reward = 1.0f;

    /* Get traces and apply reward */
    uint32_t num_traces = omni_wm_plasticity_bridge_get_active_trace_count(
        plasticity_bridge
    );
    EXPECT_GE(num_traces, 1u);

    /* Apply reward to all active traces */
    omni_wm_plasticity_bridge_apply_reward_all(
        plasticity_bridge,
        social_reward
    );

    omni_wm_plasticity_bridge_stats_t stats;
    omni_wm_plasticity_bridge_get_stats(plasticity_bridge, &stats);
    EXPECT_GE(stats.eligibility_traces_reinforced, 1u);
}

TEST_F(WMToMPlasticityTest, PredictionErrorModulatesPlasticity) {
    if (!wm) GTEST_SKIP();

    CreateBridges();
    if (!tom_bridge || !plasticity_bridge) GTEST_SKIP();

    /* Register agent */
    wm_tom_agent_state_t agent;
    create_agent_state(&agent, 1, 0.8f);
    omni_wm_tom_bridge_register_agent(tom_bridge, &agent);

    /* Predict belief */
    float predicted[STATE_DIM];
    float confidence;
    omni_wm_tom_bridge_predict_belief(
        tom_bridge, 1, predicted, STATE_DIM, &confidence
    );

    /* Observe actual behavior - compute prediction error */
    float actual[ACTION_DIM];
    generate_random_pattern(actual, ACTION_DIM);

    omni_wm_tom_bridge_update_belief(
        tom_bridge, 1, actual, ACTION_DIM, 0.9f
    );

    /* Get prediction error from ToM */
    float prediction_error = omni_wm_tom_bridge_get_prediction_error(
        tom_bridge, 1
    );

    /* Use prediction error as surprise signal for plasticity */
    omni_wm_plasticity_bridge_signal_surprise(
        plasticity_bridge,
        fminf(prediction_error, 1.0f)
    );

    /* Apply plasticity with surprise modulation */
    float pre[STATE_DIM], post[STATE_DIM];
    generate_random_pattern(pre, STATE_DIM);
    generate_random_pattern(post, STATE_DIM);

    omni_wm_plasticity_bridge_apply_stdp(
        plasticity_bridge,
        pre, STATE_DIM,
        post, STATE_DIM,
        5.0f
    );

    omni_wm_plasticity_bridge_stats_t stats;
    omni_wm_plasticity_bridge_get_stats(plasticity_bridge, &stats);
    EXPECT_GE(stats.surprise_modulations, 1u);
}

/* ============================================================================
 * Full Pipeline Integration Test
 * ============================================================================ */

TEST_F(WMToMPlasticityTest, FullSocialLearningPipeline) {
    if (!wm) GTEST_SKIP();

    CreateBridges();
    if (!tom_bridge || !plasticity_bridge || !memory_bridge || !cognitive_bridge) {
        GTEST_SKIP();
    }

    /* 1. Register observed agent */
    wm_tom_agent_state_t agent;
    create_agent_state(&agent, 1, 0.8f);
    omni_wm_tom_bridge_register_agent(tom_bridge, &agent);

    /* 2. Set up goal for observational learning */
    float target_state[STATE_DIM];
    generate_random_pattern(target_state, STATE_DIM);
    normalize_pattern(target_state, STATE_DIM);

    uint32_t goal_id;
    omni_wm_cognitive_bridge_register_goal(
        cognitive_bridge,
        target_state, STATE_DIM,
        0.8f, 0, "Social learning goal",
        &goal_id
    );

    /* 3. Observation phase - watch agent perform task */
    for (int i = 0; i < 10; i++) {
        /* Observe action */
        float action[ACTION_DIM];
        generate_random_pattern(action, ACTION_DIM);

        /* Mirror neuron activation */
        float mirror[STATE_DIM];
        omni_wm_tom_bridge_mirror_activate(
            tom_bridge, action, ACTION_DIM, mirror, STATE_DIM
        );

        /* Update belief about agent */
        omni_wm_tom_bridge_update_belief(
            tom_bridge, 1, action, ACTION_DIM, 0.8f
        );

        /* Record eligibility trace */
        uint32_t trace_id;
        omni_wm_plasticity_bridge_record_eligibility(
            plasticity_bridge, mirror, STATE_DIM, &trace_id
        );

        /* Apply STDP for motor-mirror coupling */
        float motor[STATE_DIM];
        memcpy(motor, mirror, STATE_DIM * sizeof(float));
        for (uint32_t j = 0; j < STATE_DIM; j++) {
            motor[j] += ((float)rand() / RAND_MAX - 0.5f) * 0.1f;
        }

        omni_wm_plasticity_bridge_apply_stdp(
            plasticity_bridge,
            mirror, STATE_DIM,
            motor, STATE_DIM,
            5.0f + (float)(rand() % 10)
        );

        /* Encode episodic memory */
        uint64_t engram_id;
        omni_wm_memory_bridge_encode_engram(
            memory_bridge,
            0.5f + (float)i * 0.05f,
            true,
            &engram_id
        );

        /* Update all bridges */
        omni_wm_tom_bridge_update(tom_bridge, DT);
        omni_wm_plasticity_bridge_update(plasticity_bridge, DT);
        omni_wm_memory_bridge_update(memory_bridge, DT);
        omni_wm_cognitive_bridge_update(cognitive_bridge, DT);
    }

    /* 4. Agent achieves goal - social reward */
    float social_reward = 1.0f;
    omni_wm_plasticity_bridge_apply_reward_all(
        plasticity_bridge,
        social_reward
    );

    /* 5. Consolidation phase */
    omni_wm_memory_bridge_set_sleep_state(memory_bridge, true, 1.0f);
    for (int i = 0; i < 20; i++) {
        omni_wm_memory_bridge_consolidation_sync(memory_bridge, 0.7f);
        omni_wm_memory_bridge_update(memory_bridge, DT);
    }
    omni_wm_memory_bridge_set_sleep_state(memory_bridge, false, 0.0f);

    /* 6. Mark goal achieved */
    omni_wm_cognitive_bridge_goal_achieved(cognitive_bridge, goal_id);

    /* 7. Verify stats across all bridges */
    omni_wm_tom_bridge_stats_t tom_stats;
    omni_wm_plasticity_bridge_stats_t plast_stats;
    omni_wm_memory_bridge_stats_t mem_stats;
    omni_wm_cognitive_bridge_stats_t cog_stats;

    omni_wm_tom_bridge_get_stats(tom_bridge, &tom_stats);
    omni_wm_plasticity_bridge_get_stats(plasticity_bridge, &plast_stats);
    omni_wm_memory_bridge_get_stats(memory_bridge, &mem_stats);
    omni_wm_cognitive_bridge_get_stats(cognitive_bridge, &cog_stats);

    EXPECT_GT(tom_stats.mirror_activations, 0u);
    EXPECT_GT(tom_stats.belief_updates, 0u);
    EXPECT_GT(plast_stats.stdp_updates, 0u);
    EXPECT_GT(plast_stats.eligibility_traces_created, 0u);
    EXPECT_GT(plast_stats.eligibility_traces_reinforced, 0u);
    EXPECT_GT(mem_stats.engrams_encoded, 0u);
    EXPECT_GT(mem_stats.consolidation_cycles, 0u);
    EXPECT_GE(cog_stats.goals_achieved, 1u);
}

/* ============================================================================
 * Effects Query Tests
 * ============================================================================ */

TEST_F(WMToMPlasticityTest, ToMBridgeEffectsQuery) {
    if (!wm) GTEST_SKIP();

    CreateBridges();
    if (!tom_bridge) GTEST_SKIP();

    /* Run some updates */
    for (int i = 0; i < 10; i++) {
        omni_wm_tom_bridge_update(tom_bridge, DT);
    }

    const omni_wm_to_tom_effects_t* wm_effects =
        omni_wm_tom_bridge_get_wm_effects(tom_bridge);
    const tom_to_omni_wm_effects_t* tom_effects =
        omni_wm_tom_bridge_get_tom_effects(tom_bridge);

    EXPECT_NE(wm_effects, nullptr);
    EXPECT_NE(tom_effects, nullptr);
}

TEST_F(WMToMPlasticityTest, PlasticityBridgeEffectsQuery) {
    if (!wm) GTEST_SKIP();

    CreateBridges();
    if (!plasticity_bridge) GTEST_SKIP();

    for (int i = 0; i < 10; i++) {
        omni_wm_plasticity_bridge_update(plasticity_bridge, DT);
    }

    const omni_wm_to_plasticity_effects_t* wm_effects =
        omni_wm_plasticity_bridge_get_wm_effects(plasticity_bridge);
    const plasticity_to_omni_wm_effects_t* plast_effects =
        omni_wm_plasticity_bridge_get_plasticity_effects(plasticity_bridge);

    EXPECT_NE(wm_effects, nullptr);
    EXPECT_NE(plast_effects, nullptr);
}

/* ============================================================================
 * Reset Tests
 * ============================================================================ */

TEST_F(WMToMPlasticityTest, ToMBridgeReset) {
    if (!wm) GTEST_SKIP();

    CreateBridges();
    if (!tom_bridge) GTEST_SKIP();

    /* Register agents and accumulate state */
    for (uint32_t i = 1; i <= 3; i++) {
        wm_tom_agent_state_t agent;
        create_agent_state(&agent, i, 0.7f);
        omni_wm_tom_bridge_register_agent(tom_bridge, &agent);
    }

    for (int i = 0; i < 10; i++) {
        omni_wm_tom_bridge_update(tom_bridge, DT);
    }

    /* Reset */
    nimcp_error_t ret = omni_wm_tom_bridge_reset(tom_bridge);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    /* Connection should remain */
    EXPECT_TRUE(omni_wm_tom_bridge_is_connected(tom_bridge));

    /* Agents should be cleared */
    uint32_t count = omni_wm_tom_bridge_get_agent_count(tom_bridge);
    EXPECT_EQ(count, 0u);
}

TEST_F(WMToMPlasticityTest, PlasticityBridgeReset) {
    if (!wm) GTEST_SKIP();

    CreateBridges();
    if (!plasticity_bridge) GTEST_SKIP();

    /* Accumulate plasticity state */
    for (int i = 0; i < 20; i++) {
        float pre[STATE_DIM], post[STATE_DIM];
        generate_random_pattern(pre, STATE_DIM);
        generate_random_pattern(post, STATE_DIM);
        omni_wm_plasticity_bridge_apply_stdp(
            plasticity_bridge,
            pre, STATE_DIM,
            post, STATE_DIM,
            5.0f
        );
    }

    /* Reset */
    nimcp_error_t ret = omni_wm_plasticity_bridge_reset(plasticity_bridge);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    /* Connection should remain */
    EXPECT_TRUE(omni_wm_plasticity_bridge_is_connected(plasticity_bridge));

    /* Traces should be cleared */
    uint32_t trace_count = omni_wm_plasticity_bridge_get_active_trace_count(
        plasticity_bridge
    );
    EXPECT_EQ(trace_count, 0u);
}

/* ============================================================================
 * Stress Tests
 * ============================================================================ */

TEST_F(WMToMPlasticityTest, StressTestToMPlasticityPipeline) {
    if (!wm) GTEST_SKIP();

    CreateBridges();
    if (!tom_bridge || !plasticity_bridge) GTEST_SKIP();

    const int NUM_ITERATIONS = 500;
    int successful_iterations = 0;

    /* Register some agents */
    for (uint32_t i = 1; i <= 3; i++) {
        wm_tom_agent_state_t agent;
        create_agent_state(&agent, i, 0.7f);
        omni_wm_tom_bridge_register_agent(tom_bridge, &agent);
    }

    for (int iter = 0; iter < NUM_ITERATIONS; iter++) {
        bool success = true;

        /* ToM operations */
        float action[ACTION_DIM];
        generate_random_pattern(action, ACTION_DIM);

        float mirror[STATE_DIM];
        if (omni_wm_tom_bridge_mirror_activate(
                tom_bridge, action, ACTION_DIM, mirror, STATE_DIM
            ) != NIMCP_SUCCESS) {
            success = false;
        }

        if (omni_wm_tom_bridge_update_belief(
                tom_bridge, 1, action, ACTION_DIM, 0.7f
            ) != NIMCP_SUCCESS) {
            success = false;
        }

        /* Plasticity operations */
        float post[STATE_DIM];
        generate_random_pattern(post, STATE_DIM);

        if (omni_wm_plasticity_bridge_apply_stdp(
                plasticity_bridge,
                mirror, STATE_DIM,
                post, STATE_DIM,
                5.0f + (float)(rand() % 20 - 10)
            ) != NIMCP_SUCCESS) {
            success = false;
        }

        /* Record trace periodically */
        if (iter % 10 == 0) {
            uint32_t trace_id;
            omni_wm_plasticity_bridge_record_eligibility(
                plasticity_bridge, mirror, STATE_DIM, &trace_id
            );
        }

        /* Apply reward periodically */
        if (iter % 50 == 0) {
            omni_wm_plasticity_bridge_apply_reward_all(
                plasticity_bridge,
                (float)(rand() % 100) / 100.0f * 2.0f - 1.0f
            );
        }

        /* Update bridges */
        if (omni_wm_tom_bridge_update(tom_bridge, DT) != NIMCP_SUCCESS) {
            success = false;
        }
        if (omni_wm_plasticity_bridge_update(plasticity_bridge, DT) != NIMCP_SUCCESS) {
            success = false;
        }

        if (success) successful_iterations++;
    }

    EXPECT_EQ(successful_iterations, NUM_ITERATIONS);
}

/* ============================================================================
 * Null Safety Tests
 * ============================================================================ */

TEST_F(WMToMPlasticityTest, NullSafetyTests) {
    /* ToM bridge null handling */
    EXPECT_NE(omni_wm_tom_bridge_update(nullptr, DT), NIMCP_SUCCESS);
    EXPECT_NE(omni_wm_tom_bridge_reset(nullptr), NIMCP_SUCCESS);
    EXPECT_EQ(omni_wm_tom_bridge_get_agent_count(nullptr), 0u);

    /* Plasticity bridge null handling */
    EXPECT_NE(omni_wm_plasticity_bridge_update(nullptr, DT), NIMCP_SUCCESS);
    EXPECT_NE(omni_wm_plasticity_bridge_reset(nullptr), NIMCP_SUCCESS);
    EXPECT_EQ(omni_wm_plasticity_bridge_get_active_trace_count(nullptr), 0u);
}
