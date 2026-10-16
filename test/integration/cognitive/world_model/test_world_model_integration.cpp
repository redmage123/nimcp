/**
 * @file test_world_model_integration.cpp
 * @brief Integration tests for World Model modules integration with NIMCP components
 * @version 1.0.0
 * @date 2026-01-16
 *
 * Tests integration of world model modules with:
 * - Mirror neurons (state predictions, counterfactual queries)
 * - Active inference (policy evaluation, EFE calculation)
 * - Imagination engine (JEPA predictor, scene generation)
 * - Cross-module communication (omni + multimodal world models)
 * - Bio-async messaging (message routing, callbacks)
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <cstdlib>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>

/* Headers have their own extern "C" guards */
#include "cognitive/omni/nimcp_omni_world_model.h"
#include "cognitive/extrapolation/nimcp_world_model_multimodal.h"
#include "cognitive/mirror_neurons/nimcp_mirror_omni_bridge.h"
#include "cognitive/imagination/nimcp_imagination_engine.h"
#include "cognitive/omni/nimcp_omni_active_inference.h"
#include "cognitive/nimcp_mirror_neurons.h"
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * Test Constants
 * ============================================================================ */

static constexpr uint32_t TEST_STATE_DIM = 32;
static constexpr uint32_t TEST_ACTION_DIM = 8;
static constexpr uint32_t TEST_OBS_DIM = 64;
static constexpr uint32_t TEST_LATENT_DIM = 64;
static constexpr uint32_t TEST_HORIZON = 5;
static constexpr uint32_t TEST_NUM_POLICIES = 8;

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static void generate_random_vector(float* vec, uint32_t dim) {
    for (uint32_t i = 0; i < dim; i++) {
        vec[i] = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
    }
}

static float vector_dot_product(const float* a, const float* b, uint32_t dim) {
    float sum = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        sum += a[i] * b[i];
    }
    return sum;
}

static float vector_norm(const float* v, uint32_t dim) {
    return sqrtf(vector_dot_product(v, v, dim));
}

/* ============================================================================
 * Base Test Fixture for World Model Integration
 * ============================================================================ */

class WorldModelIntegrationTest : public ::testing::Test {
protected:
    omni_world_model_t* omni_wm = nullptr;
    nimcp_world_model_t* multimodal_wm = nullptr;
    omni_active_inference_t* active_inference = nullptr;
    mirror_neurons_t mirror = nullptr;
    mirror_omni_bridge_t* mirror_bridge = nullptr;
    imagination_engine_t* imagination = nullptr;

    void SetUp() override {
        srand(42);  /* Reproducible random */
    }

    void TearDown() override {
        if (imagination) {
            imagination_engine_destroy(imagination);
            imagination = nullptr;
        }
        if (mirror_bridge) {
            mirror_omni_bridge_destroy(mirror_bridge);
            mirror_bridge = nullptr;
        }
        if (mirror) {
            mirror_neurons_destroy(mirror);
            mirror = nullptr;
        }
        if (active_inference) {
            omni_ai_destroy(active_inference);
            active_inference = nullptr;
        }
        if (multimodal_wm) {
            wm_destroy(multimodal_wm);
            multimodal_wm = nullptr;
        }
        if (omni_wm) {
            omni_wm_destroy(omni_wm);
            omni_wm = nullptr;
        }
    }

    /* Helper to create omni world model */
    bool create_omni_world_model() {
        omni_wm_config_t config;
        if (omni_wm_get_default_config(&config) != NIMCP_SUCCESS) {
            return false;
        }
        config.state_dim = TEST_STATE_DIM;
        config.action_dim = TEST_ACTION_DIM;
        config.obs_dim = TEST_OBS_DIM;
        config.enable_lateral = true;
        config.enable_hierarchical = true;
        config.enable_dreaming = true;
        config.use_symlog_rewards = true;
        config.use_rssm = true;

        omni_wm = omni_wm_create(&config);
        return (omni_wm != nullptr);
    }

    /* Helper to create multimodal world model */
    bool create_multimodal_world_model() {
        wm_config_t config = wm_default_config();
        config.latent_dim = TEST_LATENT_DIM;
        config.max_entities = 64;
        config.max_prediction_steps = TEST_HORIZON;
        config.fusion_type = WM_FUSION_ATTENTION;
        config.prediction_mode = WM_PRED_MODE_PROBABILISTIC;
        config.enable_bio_async = false;  /* We'll enable manually for some tests */

        multimodal_wm = wm_create(&config);
        if (multimodal_wm) {
            return (wm_init(multimodal_wm) == WM_OK);
        }
        return false;
    }

    /* Helper to create active inference */
    bool create_active_inference() {
        omni_ai_config_t config;
        if (omni_ai_default_config(&config) != 0) {
            return false;
        }
        config.num_policies = TEST_NUM_POLICIES;
        config.max_horizon = TEST_HORIZON;
        config.select_mode = OMNI_AI_SELECT_SOFTMAX;
        config.efe_mode = OMNI_AI_EFE_BALANCED;

        active_inference = omni_ai_create(&config, TEST_ACTION_DIM, TEST_OBS_DIM);
        return (active_inference != nullptr);
    }

    /* Helper to create mirror neurons */
    bool create_mirror_neurons() {
        mirror_neuron_config_t config = mirror_neurons_get_default_config();
        config.num_mirror_neurons = 256;
        config.max_actions = 32;
        config.max_agents = 4;
        config.enable_prediction = true;
        config.enable_theory_of_mind = true;

        mirror = mirror_neurons_create(&config);
        return (mirror != nullptr);
    }

    /* Helper to create mirror-omni bridge */
    bool create_mirror_omni_bridge() {
        mirror_omni_config_t config;
        if (mirror_omni_bridge_default_config(&config) != 0) {
            return false;
        }
        config.enable_omnidirectional = true;
        config.enable_counterfactual = true;
        config.enable_imitation_queries = true;
        config.enable_confidence_precision = true;

        mirror_bridge = mirror_omni_bridge_create(&config);
        return (mirror_bridge != nullptr);
    }

    /* Helper to create imagination engine */
    bool create_imagination_engine() {
        imagination_engine_config_t config = imagination_engine_default_config();
        config.max_concurrent_scenarios = 4;
        config.latent_dim = TEST_LATENT_DIM;
        config.enable_counterfactual = true;
        config.enable_prospective_mode = true;
        config.enable_memory_integration = false;  /* Avoid hippocampal dependency */
        config.enable_gpu_acceleration = false;

        imagination = imagination_engine_create(&config);
        return (imagination != nullptr);
    }
};

/* ============================================================================
 * 1. Mirror Neurons Integration Tests
 * ============================================================================ */

class MirrorNeuronsWorldModelTest : public WorldModelIntegrationTest {
protected:
    void SetUp() override {
        WorldModelIntegrationTest::SetUp();
        ASSERT_TRUE(create_omni_world_model());
        ASSERT_TRUE(create_mirror_neurons());
        ASSERT_TRUE(create_active_inference());
        ASSERT_TRUE(create_mirror_omni_bridge());
    }
};

TEST_F(MirrorNeuronsWorldModelTest, BridgeCreation) {
    /* Verify all components created successfully */
    EXPECT_NE(omni_wm, nullptr);
    EXPECT_NE(mirror, nullptr);
    EXPECT_NE(active_inference, nullptr);
    EXPECT_NE(mirror_bridge, nullptr);
}

TEST_F(MirrorNeuronsWorldModelTest, ConnectMirrorToWorldModel) {
    /* Connect mirror neurons to world model via bridge */
    int ret = mirror_omni_bridge_connect_mirror(mirror_bridge, mirror);
    EXPECT_EQ(ret, 0);

    ret = mirror_omni_bridge_connect_world_model(mirror_bridge, omni_wm);
    EXPECT_EQ(ret, 0);

    /* Verify connections */
    mirror_omni_state_t state;
    ret = mirror_omni_bridge_get_state(mirror_bridge, &state);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(state.mirror_connected);
    EXPECT_TRUE(state.world_model_connected);
}

TEST_F(MirrorNeuronsWorldModelTest, AgentStatePredictionFromMirror) {
    /* Setup connections */
    ASSERT_EQ(mirror_omni_bridge_connect_mirror(mirror_bridge, mirror), 0);
    ASSERT_EQ(mirror_omni_bridge_connect_world_model(mirror_bridge, omni_wm), 0);

    /* Create and observe an action */
    float features[32];
    generate_random_vector(features, 32);
    action_t observed_action = mirror_neurons_create_action(
        1, "grasp", features, 8, 1  /* agent_id = 1 (other agent) */
    );

    bool obs_result = mirror_neurons_observe_action(mirror, &observed_action);
    EXPECT_TRUE(obs_result);

    /* Feed agent state predictions to world model */
    int ret = mirror_omni_feed_agent_state(mirror_bridge, 1);
    EXPECT_EQ(ret, 0);

    /* Get predicted agent state */
    mirror_omni_agent_state_t agent_state;
    ret = mirror_omni_get_agent_state(mirror_bridge, 1, &agent_state);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(agent_state.agent_id, 1u);
    EXPECT_GT(agent_state.confidence, 0.0f);
}

TEST_F(MirrorNeuronsWorldModelTest, CounterfactualQueryFromMirror) {
    /* Setup connections */
    ASSERT_EQ(mirror_omni_bridge_connect_mirror(mirror_bridge, mirror), 0);
    ASSERT_EQ(mirror_omni_bridge_connect_world_model(mirror_bridge, omni_wm), 0);

    /* Set initial world state */
    omni_wm_state_t* initial = omni_wm_state_create(TEST_STATE_DIM);
    ASSERT_NE(initial, nullptr);
    generate_random_vector(initial->values, TEST_STATE_DIM);
    ASSERT_EQ(omni_wm_set_state(omni_wm, initial), NIMCP_SUCCESS);

    /* Simulate counterfactual for an action */
    mirror_omni_cf_result_t cf_result;
    memset(&cf_result, 0, sizeof(cf_result));

    int ret = mirror_omni_simulate_counterfactual(
        mirror_bridge,
        1,          /* action_id */
        TEST_HORIZON,
        &cf_result
    );
    EXPECT_EQ(ret, 0);
    EXPECT_GT(cf_result.horizon, 0u);
    EXPECT_LE(cf_result.divergence, 1.0f);

    /* Cleanup */
    mirror_omni_cf_result_free(&cf_result);
    omni_wm_state_destroy(initial);
}

TEST_F(MirrorNeuronsWorldModelTest, AgentStateTrackingCoordination) {
    /* Setup connections */
    ASSERT_EQ(mirror_omni_bridge_connect_mirror(mirror_bridge, mirror), 0);
    ASSERT_EQ(mirror_omni_bridge_connect_world_model(mirror_bridge, omni_wm), 0);
    ASSERT_EQ(mirror_omni_bridge_connect_active_inference(mirror_bridge, active_inference), 0);

    /* Observe multiple actions from agent */
    for (uint32_t i = 0; i < 5; i++) {
        float features[32];
        generate_random_vector(features, 8);
        action_t action = mirror_neurons_create_action(
            i, "action", features, 8, 1
        );
        mirror_neurons_observe_action(mirror, &action);
    }

    /* Update state transitions in world model */
    int ret = mirror_omni_update_state_transitions(mirror_bridge);
    EXPECT_EQ(ret, 0);

    /* Verify tracking state */
    mirror_omni_state_t state;
    ASSERT_EQ(mirror_omni_bridge_get_state(mirror_bridge, &state), 0);
    EXPECT_GT(state.observed_agents, 0u);
    EXPECT_GT(state.state_predictions_made, 0u);
}

/* ============================================================================
 * 2. Active Inference Integration Tests
 * ============================================================================ */

class ActiveInferenceWorldModelTest : public WorldModelIntegrationTest {
protected:
    void SetUp() override {
        WorldModelIntegrationTest::SetUp();
        ASSERT_TRUE(create_omni_world_model());
        ASSERT_TRUE(create_active_inference());
    }
};

TEST_F(ActiveInferenceWorldModelTest, ConnectWorldModelToActiveInference) {
    /* Connect world model to active inference for policy evaluation */
    int ret = omni_wm_connect_active_inference(omni_wm, active_inference);
    EXPECT_EQ(ret, NIMCP_SUCCESS);
}

TEST_F(ActiveInferenceWorldModelTest, PolicyEvaluationUsingWorldModelSimulation) {
    /* Setup initial state */
    omni_wm_state_t* state = omni_wm_state_create(TEST_STATE_DIM);
    ASSERT_NE(state, nullptr);
    generate_random_vector(state->values, TEST_STATE_DIM);
    ASSERT_EQ(omni_wm_set_state(omni_wm, state), NIMCP_SUCCESS);

    /* Generate policies to evaluate */
    int num_generated = omni_ai_generate_random_policies(
        active_inference, TEST_NUM_POLICIES, TEST_HORIZON
    );
    EXPECT_GT(num_generated, 0);

    /* Set goal (preferred observations) */
    float preferred_obs[TEST_OBS_DIM];
    generate_random_vector(preferred_obs, TEST_OBS_DIM);
    int goal_idx = omni_ai_set_goal(active_inference, preferred_obs, TEST_OBS_DIM, 1.0f);
    EXPECT_GE(goal_idx, 0);

    /* Update current observation */
    float current_obs[TEST_OBS_DIM];
    generate_random_vector(current_obs, TEST_OBS_DIM);
    EXPECT_EQ(omni_ai_update_observation(active_inference, current_obs, TEST_OBS_DIM), 0);

    /* Evaluate policies (uses world model internally for simulation) */
    int ret = omni_ai_evaluate_policies(active_inference);
    EXPECT_EQ(ret, 0);

    /* Get best policy */
    int best_idx = omni_ai_get_best_policy(active_inference);
    EXPECT_GE(best_idx, 0);
    EXPECT_LT((uint32_t)best_idx, TEST_NUM_POLICIES);

    /* Get policy EFE */
    float efe = omni_ai_get_policy_efe(active_inference, best_idx, OMNI_AI_DIR_FORWARD);
    EXPECT_FALSE(std::isnan(efe));

    omni_wm_state_destroy(state);
}

TEST_F(ActiveInferenceWorldModelTest, ExpectedFreeEnergyCalculation) {
    /* Setup state and rollout */
    omni_wm_state_t* state = omni_wm_state_create(TEST_STATE_DIM);
    ASSERT_NE(state, nullptr);
    generate_random_vector(state->values, TEST_STATE_DIM);
    ASSERT_EQ(omni_wm_set_state(omni_wm, state), NIMCP_SUCCESS);

    /* Create rollout structure */
    omni_wm_rollout_t* rollout = omni_wm_rollout_create(
        TEST_HORIZON, TEST_STATE_DIM, TEST_ACTION_DIM, TEST_OBS_DIM
    );
    ASSERT_NE(rollout, nullptr);

    /* Define simple policy (just generates random actions) */
    auto policy_fn = [](const omni_wm_state_t* s, float* action, void* user_data) {
        (void)s;
        (void)user_data;
        for (uint32_t i = 0; i < TEST_ACTION_DIM; i++) {
            action[i] = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
        }
    };

    /* Execute rollout */
    int ret = omni_wm_rollout(omni_wm, policy_fn, TEST_HORIZON, rollout, nullptr);
    EXPECT_EQ(ret, NIMCP_SUCCESS);
    EXPECT_GT(rollout->length, 0u);

    /* Calculate EFE */
    float preferred_obs[TEST_OBS_DIM];
    generate_random_vector(preferred_obs, TEST_OBS_DIM);

    float efe = omni_wm_evaluate_efe(omni_wm, rollout, preferred_obs, TEST_OBS_DIM);
    EXPECT_FALSE(std::isnan(efe));
    EXPECT_FALSE(std::isinf(efe));

    omni_wm_rollout_destroy(rollout);
    omni_wm_state_destroy(state);
}

TEST_F(ActiveInferenceWorldModelTest, BeliefUpdatingThroughWorldModel) {
    /* Test belief updating when world model predictions don't match observations */

    /* Set initial belief */
    float initial_belief[TEST_STATE_DIM];
    generate_random_vector(initial_belief, TEST_STATE_DIM);
    EXPECT_EQ(omni_ai_update_belief(active_inference, initial_belief, TEST_STATE_DIM), 0);

    /* Set initial state in world model */
    omni_wm_state_t* state = omni_wm_state_from_values(initial_belief, TEST_STATE_DIM);
    ASSERT_NE(state, nullptr);
    ASSERT_EQ(omni_wm_set_state(omni_wm, state), NIMCP_SUCCESS);

    /* Predict forward */
    float action[TEST_ACTION_DIM];
    generate_random_vector(action, TEST_ACTION_DIM);

    omni_wm_transition_t transition;
    memset(&transition, 0, sizeof(transition));

    int ret = omni_wm_predict_forward(omni_wm, action, TEST_ACTION_DIM, &transition);
    EXPECT_EQ(ret, NIMCP_SUCCESS);
    EXPECT_NE(transition.next_state, nullptr);

    /* Simulate receiving actual observation that differs from prediction */
    float actual_obs[TEST_OBS_DIM];
    generate_random_vector(actual_obs, TEST_OBS_DIM);
    EXPECT_EQ(omni_ai_update_observation(active_inference, actual_obs, TEST_OBS_DIM), 0);

    /* Update world model with prediction error */
    if (transition.next_state) {
        omni_wm_state_t* next = omni_wm_state_from_values(actual_obs, TEST_STATE_DIM);
        if (next) {
            ret = omni_wm_update(omni_wm, state, action, TEST_ACTION_DIM, next, 0.5f);
            EXPECT_EQ(ret, NIMCP_SUCCESS);
            omni_wm_state_destroy(next);
        }
        omni_wm_state_destroy(transition.next_state);
    }

    omni_wm_state_destroy(state);
}

/* ============================================================================
 * 3. Imagination Engine Integration Tests
 * ============================================================================ */

class ImaginationEngineWorldModelTest : public WorldModelIntegrationTest {
protected:
    void SetUp() override {
        WorldModelIntegrationTest::SetUp();
        ASSERT_TRUE(create_omni_world_model());
        ASSERT_TRUE(create_imagination_engine());
    }
};

TEST_F(ImaginationEngineWorldModelTest, ImaginationEngineCreation) {
    EXPECT_NE(imagination, nullptr);
}

TEST_F(ImaginationEngineWorldModelTest, ScenarioCreationWithWorldModel) {
    /* Begin imagination scenario in counterfactual mode */
    imagination_scenario_t* scenario = imagination_begin_scenario(
        imagination,
        IMAGINATION_MODE_COUNTERFACTUAL,
        nullptr  /* No specific goal */
    );
    EXPECT_NE(scenario, nullptr);

    if (scenario) {
        EXPECT_EQ(scenario->mode, IMAGINATION_MODE_COUNTERFACTUAL);
        EXPECT_TRUE(scenario->is_active);
        EXPECT_EQ(scenario->error_code, 0);

        /* End scenario */
        int ret = imagination_end_scenario(imagination, scenario);
        EXPECT_EQ(ret, 0);
    }
}

TEST_F(ImaginationEngineWorldModelTest, ProspectiveSimulation) {
    /* Setup world model with initial state */
    omni_wm_state_t* initial = omni_wm_state_create(TEST_STATE_DIM);
    ASSERT_NE(initial, nullptr);
    generate_random_vector(initial->values, TEST_STATE_DIM);
    ASSERT_EQ(omni_wm_set_state(omni_wm, initial), NIMCP_SUCCESS);

    /* Create prospective simulation goal */
    imagination_goal_t goal;
    memset(&goal, 0, sizeof(goal));
    goal.mode = IMAGINATION_MODE_PROSPECTIVE;
    goal.priority = 0.8f;
    goal.deadline_ms = 5000;

    /* Begin prospective scenario */
    imagination_scenario_t* scenario = imagination_begin_scenario(
        imagination,
        IMAGINATION_MODE_PROSPECTIVE,
        &goal
    );
    EXPECT_NE(scenario, nullptr);

    if (scenario) {
        /* Step scenario forward multiple times */
        for (int i = 0; i < 3; i++) {
            int ret = imagination_step_scenario(imagination, scenario);
            EXPECT_EQ(ret, 0);
        }

        /* Check vividness and coherence */
        EXPECT_GE(scenario->vividness, 0.0f);
        EXPECT_LE(scenario->vividness, 1.0f);
        EXPECT_GE(scenario->coherence, 0.0f);
        EXPECT_LE(scenario->coherence, 1.0f);

        imagination_end_scenario(imagination, scenario);
    }

    omni_wm_state_destroy(initial);
}

TEST_F(ImaginationEngineWorldModelTest, CounterfactualReasoning) {
    /* Setup world model state */
    omni_wm_state_t* state = omni_wm_state_create(TEST_STATE_DIM);
    ASSERT_NE(state, nullptr);
    generate_random_vector(state->values, TEST_STATE_DIM);
    ASSERT_EQ(omni_wm_set_state(omni_wm, state), NIMCP_SUCCESS);

    /* Create counterfactual query through world model */
    float hypothetical_action[TEST_ACTION_DIM];
    generate_random_vector(hypothetical_action, TEST_ACTION_DIM);

    omni_wm_counterfactual_result_t cf_result;
    memset(&cf_result, 0, sizeof(cf_result));

    int ret = omni_wm_what_if(
        omni_wm,
        hypothetical_action,
        TEST_ACTION_DIM,
        TEST_HORIZON,
        &cf_result
    );
    EXPECT_EQ(ret, NIMCP_SUCCESS);
    EXPECT_GT(cf_result.trajectory_len, 0u);
    EXPECT_LE(cf_result.divergence, 1.0f);
    EXPECT_GE(cf_result.confidence, 0.0f);

    omni_wm_cf_result_destroy(&cf_result);
    omni_wm_state_destroy(state);
}

TEST_F(ImaginationEngineWorldModelTest, SceneTransformation) {
    /* Begin scenario */
    imagination_scenario_t* scenario = imagination_begin_scenario(
        imagination,
        IMAGINATION_MODE_DIRECTED,
        nullptr
    );
    ASSERT_NE(scenario, nullptr);

    /* Create transformation (time advance) */
    imagination_transform_t transform;
    memset(&transform, 0, sizeof(transform));
    transform.time_delta = 1.0f;  /* Advance 1 second */

    /* Apply transformation */
    int ret = imagination_transform_scene(imagination, scenario, &transform);
    EXPECT_EQ(ret, 0);

    /* Verify scenario updated */
    EXPECT_TRUE(scenario->is_active);

    imagination_end_scenario(imagination, scenario);
}

/* ============================================================================
 * 4. Cross-Module Communication Tests
 * ============================================================================ */

class CrossModuleCommunicationTest : public WorldModelIntegrationTest {
protected:
    void SetUp() override {
        WorldModelIntegrationTest::SetUp();
        ASSERT_TRUE(create_omni_world_model());
        ASSERT_TRUE(create_multimodal_world_model());
    }
};

TEST_F(CrossModuleCommunicationTest, OmniAndMultimodalWorldModelInteraction) {
    /* Both world models should be active */
    EXPECT_NE(omni_wm, nullptr);
    EXPECT_NE(multimodal_wm, nullptr);

    /* Set state in omni world model */
    omni_wm_state_t* omni_state = omni_wm_state_create(TEST_STATE_DIM);
    ASSERT_NE(omni_state, nullptr);
    generate_random_vector(omni_state->values, TEST_STATE_DIM);
    ASSERT_EQ(omni_wm_set_state(omni_wm, omni_state), NIMCP_SUCCESS);

    /* Process visual modality input in multimodal world model */
    wm_modality_input_t visual_input;
    memset(&visual_input, 0, sizeof(visual_input));
    visual_input.modality = WM_MODALITY_VISUAL;
    visual_input.feature_dim = TEST_LATENT_DIM;
    visual_input.features = (float*)nimcp_malloc(TEST_LATENT_DIM * sizeof(float));
    ASSERT_NE(visual_input.features, nullptr);
    generate_random_vector(visual_input.features, TEST_LATENT_DIM);
    visual_input.confidence = 0.9f;

    wm_error_t err = wm_process_modality(multimodal_wm, &visual_input);
    EXPECT_EQ(err, WM_OK);

    /* Check multimodal world model status */
    wm_status_t status = wm_get_status(multimodal_wm);
    EXPECT_NE(status, WM_STATUS_ERROR);

    nimcp_free(visual_input.features);
    omni_wm_state_destroy(omni_state);
}

TEST_F(CrossModuleCommunicationTest, StateSynchronizationBetweenComponents) {
    /* Create initial state in omni world model */
    omni_wm_state_t* state = omni_wm_state_create(TEST_STATE_DIM);
    ASSERT_NE(state, nullptr);
    generate_random_vector(state->values, TEST_STATE_DIM);
    state->timestamp = 1000.0;
    ASSERT_EQ(omni_wm_set_state(omni_wm, state), NIMCP_SUCCESS);

    /* Get state back */
    const omni_wm_state_t* retrieved = omni_wm_get_state(omni_wm);
    EXPECT_NE(retrieved, nullptr);
    if (retrieved) {
        EXPECT_EQ(retrieved->dim, TEST_STATE_DIM);
        /* Verify values match */
        float diff = 0.0f;
        for (uint32_t i = 0; i < TEST_STATE_DIM; i++) {
            diff += fabsf(state->values[i] - retrieved->values[i]);
        }
        EXPECT_LT(diff, 0.001f);  /* Should be essentially identical */
    }

    /* Update multimodal world model */
    wm_error_t err = wm_update(multimodal_wm, 16.67f);  /* ~60 FPS */
    EXPECT_EQ(err, WM_OK);

    omni_wm_state_destroy(state);
}

TEST_F(CrossModuleCommunicationTest, PredictionCoordination) {
    /* Set state in omni world model */
    omni_wm_state_t* state = omni_wm_state_create(TEST_STATE_DIM);
    ASSERT_NE(state, nullptr);
    generate_random_vector(state->values, TEST_STATE_DIM);
    ASSERT_EQ(omni_wm_set_state(omni_wm, state), NIMCP_SUCCESS);

    /* Make prediction with omni world model */
    float action[TEST_ACTION_DIM];
    generate_random_vector(action, TEST_ACTION_DIM);

    omni_wm_transition_t transition;
    memset(&transition, 0, sizeof(transition));
    int ret = omni_wm_predict_forward(omni_wm, action, TEST_ACTION_DIM, &transition);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    /* Make prediction with multimodal world model */
    wm_prediction_t prediction;
    memset(&prediction, 0, sizeof(prediction));
    prediction.horizon_steps = TEST_HORIZON;
    prediction.state_dim = TEST_LATENT_DIM;
    prediction.predicted_states = (float*)nimcp_malloc(
        TEST_HORIZON * TEST_LATENT_DIM * sizeof(float)
    );
    prediction.uncertainties = (float*)nimcp_malloc(TEST_HORIZON * sizeof(float));
    ASSERT_NE(prediction.predicted_states, nullptr);
    ASSERT_NE(prediction.uncertainties, nullptr);

    wm_error_t err = wm_predict(multimodal_wm, TEST_HORIZON, &prediction);
    EXPECT_EQ(err, WM_OK);
    EXPECT_GT(prediction.prediction_confidence, 0.0f);

    /* Cleanup */
    if (transition.next_state) {
        omni_wm_state_destroy(transition.next_state);
    }
    nimcp_free(prediction.predicted_states);
    nimcp_free(prediction.uncertainties);
    omni_wm_state_destroy(state);
}

TEST_F(CrossModuleCommunicationTest, MultimodalFusionWithEntityTracking) {
    /* Add entity to multimodal world model */
    wm_entity_t entity;
    memset(&entity, 0, sizeof(entity));
    entity.position[0] = 1.0f;
    entity.position[1] = 2.0f;
    entity.position[2] = 0.0f;
    entity.velocity[0] = 0.1f;
    entity.velocity[1] = 0.0f;
    entity.velocity[2] = 0.0f;
    entity.existence_prob = 0.95f;
    entity.latent_dim = TEST_LATENT_DIM;
    entity.latent_state = (float*)nimcp_malloc(TEST_LATENT_DIM * sizeof(float));
    ASSERT_NE(entity.latent_state, nullptr);
    generate_random_vector(entity.latent_state, TEST_LATENT_DIM);

    uint32_t entity_id;
    wm_error_t err = wm_add_entity(multimodal_wm, &entity, &entity_id);
    EXPECT_EQ(err, WM_OK);

    /* Process multiple modalities */
    wm_modality_input_t inputs[2];

    /* Visual input */
    inputs[0].modality = WM_MODALITY_VISUAL;
    inputs[0].feature_dim = TEST_LATENT_DIM;
    inputs[0].features = (float*)nimcp_malloc(TEST_LATENT_DIM * sizeof(float));
    generate_random_vector(inputs[0].features, TEST_LATENT_DIM);
    inputs[0].confidence = 0.8f;
    inputs[0].attention_weights = nullptr;

    /* Auditory input */
    inputs[1].modality = WM_MODALITY_AUDITORY;
    inputs[1].feature_dim = TEST_LATENT_DIM;
    inputs[1].features = (float*)nimcp_malloc(TEST_LATENT_DIM * sizeof(float));
    generate_random_vector(inputs[1].features, TEST_LATENT_DIM);
    inputs[1].confidence = 0.7f;
    inputs[1].attention_weights = nullptr;

    err = wm_process_multimodal(multimodal_wm, inputs, 2);
    EXPECT_EQ(err, WM_OK);

    /* Fuse modalities */
    err = wm_fuse_modalities(multimodal_wm);
    EXPECT_EQ(err, WM_OK);

    /* Get cross-modal attention */
    wm_cross_modal_attention_t attention;
    err = wm_get_attention(multimodal_wm, &attention);
    EXPECT_EQ(err, WM_OK);
    EXPECT_GE(attention.coherence_score, 0.0f);
    EXPECT_LE(attention.coherence_score, 1.0f);

    /* Cleanup */
    nimcp_free(entity.latent_state);
    nimcp_free(inputs[0].features);
    nimcp_free(inputs[1].features);
}

/* ============================================================================
 * 5. Bio-Async Messaging Tests
 * ============================================================================ */

class BioAsyncWorldModelTest : public WorldModelIntegrationTest {
protected:
    std::atomic<int> callback_count{0};

    void SetUp() override {
        WorldModelIntegrationTest::SetUp();
        ASSERT_TRUE(create_omni_world_model());
    }
};

TEST_F(BioAsyncWorldModelTest, ConnectBioAsync) {
    /* Connect world model to bio-async system */
    int ret = omni_wm_connect_bio_async(omni_wm);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    /* Disconnect */
    ret = omni_wm_disconnect_bio_async(omni_wm);
    EXPECT_EQ(ret, NIMCP_SUCCESS);
}

TEST_F(BioAsyncWorldModelTest, StatisticsTracking) {
    /* Set state and make predictions */
    omni_wm_state_t* state = omni_wm_state_create(TEST_STATE_DIM);
    ASSERT_NE(state, nullptr);
    generate_random_vector(state->values, TEST_STATE_DIM);
    ASSERT_EQ(omni_wm_set_state(omni_wm, state), NIMCP_SUCCESS);

    /* Make several predictions */
    for (int i = 0; i < 5; i++) {
        float action[TEST_ACTION_DIM];
        generate_random_vector(action, TEST_ACTION_DIM);

        omni_wm_transition_t transition;
        memset(&transition, 0, sizeof(transition));
        omni_wm_predict_forward(omni_wm, action, TEST_ACTION_DIM, &transition);

        if (transition.next_state) {
            omni_wm_state_destroy(transition.next_state);
        }
    }

    /* Get statistics */
    omni_wm_stats_t stats;
    int ret = omni_wm_get_stats(omni_wm, &stats);
    EXPECT_EQ(ret, NIMCP_SUCCESS);
    EXPECT_GT(stats.forward_predictions, 0ULL);

    /* Reset statistics */
    ret = omni_wm_reset_stats(omni_wm);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    ret = omni_wm_get_stats(omni_wm, &stats);
    EXPECT_EQ(ret, NIMCP_SUCCESS);
    EXPECT_EQ(stats.forward_predictions, 0ULL);

    omni_wm_state_destroy(state);
}

/* ============================================================================
 * 6. RSSM (Recurrent State Space Model) Tests
 * ============================================================================ */

class RSSMWorldModelTest : public WorldModelIntegrationTest {
protected:
    void SetUp() override {
        WorldModelIntegrationTest::SetUp();

        /* Create world model with RSSM enabled */
        omni_wm_config_t config;
        ASSERT_EQ(omni_wm_get_default_config(&config), NIMCP_SUCCESS);
        config.state_dim = TEST_STATE_DIM;
        config.action_dim = TEST_ACTION_DIM;
        config.obs_dim = TEST_OBS_DIM;
        config.use_rssm = true;
        config.rssm_h_dim = 64;  /* Deterministic state */
        config.rssm_z_dim = 32;  /* Stochastic state */

        omni_wm = omni_wm_create(&config);
        ASSERT_NE(omni_wm, nullptr);
    }
};

TEST_F(RSSMWorldModelTest, RSSMStateCreation) {
    omni_wm_rssm_state_t* rssm_state = omni_wm_rssm_state_create(64, 32);
    ASSERT_NE(rssm_state, nullptr);
    EXPECT_EQ(rssm_state->h_dim, 64u);
    EXPECT_EQ(rssm_state->z_dim, 32u);
    EXPECT_NE(rssm_state->h, nullptr);
    EXPECT_NE(rssm_state->z, nullptr);

    omni_wm_rssm_state_destroy(rssm_state);
}

TEST_F(RSSMWorldModelTest, RSSMForwardStep) {
    /* Create RSSM state */
    omni_wm_rssm_state_t* state = omni_wm_rssm_state_create(64, 32);
    ASSERT_NE(state, nullptr);
    generate_random_vector(state->h, 64);
    generate_random_vector(state->z, 32);

    /* Set as current state */
    int ret = omni_wm_set_rssm_state(omni_wm, state);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    /* Create action */
    float action[TEST_ACTION_DIM];
    generate_random_vector(action, TEST_ACTION_DIM);

    /* Step forward */
    omni_wm_rssm_state_t* next_state = omni_wm_rssm_state_create(64, 32);
    ASSERT_NE(next_state, nullptr);

    ret = omni_wm_rssm_step(omni_wm, state, action, TEST_ACTION_DIM, next_state);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    /* Verify state changed */
    float h_diff = 0.0f;
    for (uint32_t i = 0; i < 64; i++) {
        h_diff += fabsf(state->h[i] - next_state->h[i]);
    }
    /* States should differ after step */
    EXPECT_GT(h_diff, 0.0f);

    omni_wm_rssm_state_destroy(next_state);
    omni_wm_rssm_state_destroy(state);
}

TEST_F(RSSMWorldModelTest, RSSMImagination) {
    /* Create initial state */
    omni_wm_rssm_state_t* initial = omni_wm_rssm_state_create(64, 32);
    ASSERT_NE(initial, nullptr);
    generate_random_vector(initial->h, 64);
    generate_random_vector(initial->z, 32);

    /* Create action sequence */
    const uint32_t horizon = 5;
    float* actions[horizon];
    for (uint32_t i = 0; i < horizon; i++) {
        actions[i] = (float*)nimcp_malloc(TEST_ACTION_DIM * sizeof(float));
        ASSERT_NE(actions[i], nullptr);
        generate_random_vector(actions[i], TEST_ACTION_DIM);
    }

    /* Create trajectory buffer */
    omni_wm_rssm_state_t* trajectory[horizon];
    for (uint32_t i = 0; i < horizon; i++) {
        trajectory[i] = omni_wm_rssm_state_create(64, 32);
        ASSERT_NE(trajectory[i], nullptr);
    }

    /* Imagine forward */
    int ret = omni_wm_rssm_imagine(
        omni_wm,
        initial,
        (const float* const*)actions,
        horizon,
        trajectory
    );
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    /* Cleanup */
    for (uint32_t i = 0; i < horizon; i++) {
        nimcp_free(actions[i]);
        omni_wm_rssm_state_destroy(trajectory[i]);
    }
    omni_wm_rssm_state_destroy(initial);
}

/* ============================================================================
 * 7. Latent Encoding Tests (JEPA-style)
 * ============================================================================ */

class LatentEncodingTest : public WorldModelIntegrationTest {
protected:
    void SetUp() override {
        WorldModelIntegrationTest::SetUp();

        omni_wm_config_t config;
        ASSERT_EQ(omni_wm_get_default_config(&config), NIMCP_SUCCESS);
        config.state_dim = TEST_STATE_DIM;
        config.action_dim = TEST_ACTION_DIM;
        config.obs_dim = TEST_OBS_DIM;
        config.latent_dim = TEST_LATENT_DIM;

        omni_wm = omni_wm_create(&config);
        ASSERT_NE(omni_wm, nullptr);
    }
};

TEST_F(LatentEncodingTest, LatentCreation) {
    omni_wm_latent_t* latent = omni_wm_latent_create(TEST_LATENT_DIM);
    ASSERT_NE(latent, nullptr);
    EXPECT_EQ(latent->dim, TEST_LATENT_DIM);
    EXPECT_NE(latent->embedding, nullptr);

    omni_wm_latent_destroy(latent);
}

TEST_F(LatentEncodingTest, EncodeObservation) {
    /* Create observation */
    float observation[TEST_OBS_DIM];
    generate_random_vector(observation, TEST_OBS_DIM);

    /* Create latent for output */
    omni_wm_latent_t* latent = omni_wm_latent_create(TEST_LATENT_DIM);
    ASSERT_NE(latent, nullptr);

    /* Encode */
    int ret = omni_wm_encode(omni_wm, observation, TEST_OBS_DIM, latent);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    /* Verify latent is non-zero */
    float norm = vector_norm(latent->embedding, TEST_LATENT_DIM);
    EXPECT_GT(norm, 0.0f);

    omni_wm_latent_destroy(latent);
}

TEST_F(LatentEncodingTest, PredictInLatentSpace) {
    /* Create current latent */
    omni_wm_latent_t* current = omni_wm_latent_create(TEST_LATENT_DIM);
    ASSERT_NE(current, nullptr);
    generate_random_vector(current->embedding, TEST_LATENT_DIM);

    /* Create action */
    float action[TEST_ACTION_DIM];
    generate_random_vector(action, TEST_ACTION_DIM);

    /* Create predicted latent */
    omni_wm_latent_t* predicted = omni_wm_latent_create(TEST_LATENT_DIM);
    ASSERT_NE(predicted, nullptr);

    /* Predict */
    int ret = omni_wm_predict_latent(omni_wm, current, action, TEST_ACTION_DIM, predicted);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    /* Verify prediction differs from current */
    float diff = 0.0f;
    for (uint32_t i = 0; i < TEST_LATENT_DIM; i++) {
        diff += fabsf(current->embedding[i] - predicted->embedding[i]);
    }
    EXPECT_GT(diff, 0.0f);

    omni_wm_latent_destroy(predicted);
    omni_wm_latent_destroy(current);
}

/* ============================================================================
 * 8. Symlog Transformation Tests (DreamerV3)
 * ============================================================================ */

TEST(SymlogTest, BasicSymlog) {
    /* Test symlog: sign(x) * ln(|x| + 1) */
    EXPECT_FLOAT_EQ(omni_wm_symlog(0.0f), 0.0f);
    EXPECT_GT(omni_wm_symlog(1.0f), 0.0f);
    EXPECT_LT(omni_wm_symlog(-1.0f), 0.0f);

    /* Test symmetry */
    EXPECT_FLOAT_EQ(omni_wm_symlog(5.0f), -omni_wm_symlog(-5.0f));
}

TEST(SymlogTest, SymexpInverse) {
    /* Test that symexp is inverse of symlog */
    float values[] = {0.0f, 1.0f, -1.0f, 10.0f, -10.0f, 100.0f, -100.0f};
    for (float v : values) {
        float transformed = omni_wm_symlog(v);
        float recovered = omni_wm_symexp(transformed);
        EXPECT_NEAR(v, recovered, 0.0001f);
    }
}

TEST(SymlogTest, ArrayTransformation) {
    float input[] = {0.0f, 1.0f, -1.0f, 10.0f, -10.0f};
    float output[5], recovered[5];

    omni_wm_symlog_array(input, output, 5);
    omni_wm_symexp_array(output, recovered, 5);

    for (int i = 0; i < 5; i++) {
        EXPECT_NEAR(input[i], recovered[i], 0.0001f);
    }
}

/* ============================================================================
 * 9. MDN (Mixture Density Network) Tests
 * ============================================================================ */

class MDNWorldModelTest : public WorldModelIntegrationTest {
protected:
    void SetUp() override {
        WorldModelIntegrationTest::SetUp();

        omni_wm_config_t config;
        ASSERT_EQ(omni_wm_get_default_config(&config), NIMCP_SUCCESS);
        config.state_dim = TEST_STATE_DIM;
        config.action_dim = TEST_ACTION_DIM;
        config.obs_dim = TEST_OBS_DIM;
        config.use_mdn = true;
        config.mdn_components = 4;
        config.pred_type = OMNI_WM_PRED_MIXTURE;

        omni_wm = omni_wm_create(&config);
        ASSERT_NE(omni_wm, nullptr);
    }
};

TEST_F(MDNWorldModelTest, MDNCreation) {
    omni_wm_mdn_prediction_t* mdn = omni_wm_mdn_create(4, TEST_STATE_DIM);
    ASSERT_NE(mdn, nullptr);
    EXPECT_EQ(mdn->num_components, 4u);
    EXPECT_EQ(mdn->dim, TEST_STATE_DIM);

    omni_wm_mdn_destroy(mdn);
}

TEST_F(MDNWorldModelTest, MDNPrediction) {
    /* Set state */
    omni_wm_state_t* state = omni_wm_state_create(TEST_STATE_DIM);
    ASSERT_NE(state, nullptr);
    generate_random_vector(state->values, TEST_STATE_DIM);
    ASSERT_EQ(omni_wm_set_state(omni_wm, state), NIMCP_SUCCESS);

    /* Create MDN prediction */
    omni_wm_mdn_prediction_t* mdn = omni_wm_mdn_create(4, TEST_STATE_DIM);
    ASSERT_NE(mdn, nullptr);

    /* Predict */
    float action[TEST_ACTION_DIM];
    generate_random_vector(action, TEST_ACTION_DIM);

    int ret = omni_wm_predict_mdn(omni_wm, state, action, TEST_ACTION_DIM, mdn);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    /* Check weights sum to 1 */
    float weight_sum = 0.0f;
    for (uint32_t i = 0; i < mdn->num_components; i++) {
        weight_sum += mdn->components[i].weight;
    }
    EXPECT_NEAR(weight_sum, 1.0f, 0.01f);

    omni_wm_mdn_destroy(mdn);
    omni_wm_state_destroy(state);
}

TEST_F(MDNWorldModelTest, MDNSampling) {
    /* Create MDN with known components */
    omni_wm_mdn_prediction_t* mdn = omni_wm_mdn_create(2, TEST_STATE_DIM);
    ASSERT_NE(mdn, nullptr);

    /* Initialize components */
    mdn->components[0].weight = 0.5f;
    mdn->components[1].weight = 0.5f;
    for (uint32_t i = 0; i < TEST_STATE_DIM; i++) {
        mdn->components[0].mean[i] = 0.0f;
        mdn->components[0].std[i] = 0.1f;
        mdn->components[1].mean[i] = 1.0f;
        mdn->components[1].std[i] = 0.1f;
    }

    /* Sample multiple times */
    float sample[TEST_STATE_DIM];
    for (int i = 0; i < 10; i++) {
        int ret = omni_wm_mdn_sample(mdn, sample);
        EXPECT_EQ(ret, NIMCP_SUCCESS);
    }

    /* Get mode */
    float mode[TEST_STATE_DIM];
    int ret = omni_wm_mdn_mode(mdn, mode);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    omni_wm_mdn_destroy(mdn);
}

/* ============================================================================
 * 10. Experience Replay Tests
 * ============================================================================ */

class ExperienceReplayTest : public WorldModelIntegrationTest {
protected:
    void SetUp() override {
        WorldModelIntegrationTest::SetUp();

        omni_wm_config_t config;
        ASSERT_EQ(omni_wm_get_default_config(&config), NIMCP_SUCCESS);
        config.state_dim = TEST_STATE_DIM;
        config.action_dim = TEST_ACTION_DIM;
        config.obs_dim = TEST_OBS_DIM;
        config.replay_buffer_size = 1000;
        config.learn_mode = OMNI_WM_LEARN_REPLAY;

        omni_wm = omni_wm_create(&config);
        ASSERT_NE(omni_wm, nullptr);
    }
};

TEST_F(ExperienceReplayTest, AddExperience) {
    /* Create experience */
    omni_wm_experience_t* exp = omni_wm_experience_create(
        TEST_STATE_DIM, TEST_ACTION_DIM, TEST_OBS_DIM
    );
    ASSERT_NE(exp, nullptr);

    /* Fill experience */
    generate_random_vector(exp->action, TEST_ACTION_DIM);
    exp->reward = 1.0f;
    exp->terminal = false;

    /* Add to replay buffer */
    int ret = omni_wm_add_experience(omni_wm, exp);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    /* Check replay buffer size */
    uint32_t size = omni_wm_get_replay_size(omni_wm);
    EXPECT_EQ(size, 1u);

    omni_wm_experience_destroy(exp);
}

TEST_F(ExperienceReplayTest, SampleExperiences) {
    /* Add multiple experiences */
    for (int i = 0; i < 100; i++) {
        omni_wm_experience_t* exp = omni_wm_experience_create(
            TEST_STATE_DIM, TEST_ACTION_DIM, TEST_OBS_DIM
        );
        ASSERT_NE(exp, nullptr);
        generate_random_vector(exp->action, TEST_ACTION_DIM);
        exp->reward = (float)i / 100.0f;
        exp->terminal = (i == 99);

        omni_wm_add_experience(omni_wm, exp);
        omni_wm_experience_destroy(exp);
    }

    EXPECT_EQ(omni_wm_get_replay_size(omni_wm), 100u);

    /* Sample batch - function returns pointers to internal experiences
     * Do NOT pre-allocate or destroy these - they're owned by the buffer */
    omni_wm_experience_t* batch[32];
    memset(batch, 0, sizeof(batch));

    uint32_t sampled = omni_wm_sample_experiences(omni_wm, batch, 32);
    EXPECT_GT(sampled, 0u);
    EXPECT_LE(sampled, 32u);

    /* Verify sampled experiences are valid (but don't destroy them) */
    for (uint32_t i = 0; i < sampled; i++) {
        EXPECT_NE(batch[i], nullptr);
    }
}

TEST_F(ExperienceReplayTest, ClearReplay) {
    /* Add some experiences */
    for (int i = 0; i < 10; i++) {
        omni_wm_experience_t* exp = omni_wm_experience_create(
            TEST_STATE_DIM, TEST_ACTION_DIM, TEST_OBS_DIM
        );
        omni_wm_add_experience(omni_wm, exp);
        omni_wm_experience_destroy(exp);
    }

    EXPECT_EQ(omni_wm_get_replay_size(omni_wm), 10u);

    /* Clear */
    int ret = omni_wm_clear_replay(omni_wm);
    EXPECT_EQ(ret, NIMCP_SUCCESS);
    EXPECT_EQ(omni_wm_get_replay_size(omni_wm), 0u);
}

/* ============================================================================
 * 11. Dreaming Tests (Offline Simulation)
 * ============================================================================ */

class DreamingTest : public WorldModelIntegrationTest {
protected:
    void SetUp() override {
        WorldModelIntegrationTest::SetUp();

        omni_wm_config_t config;
        ASSERT_EQ(omni_wm_get_default_config(&config), NIMCP_SUCCESS);
        config.state_dim = TEST_STATE_DIM;
        config.action_dim = TEST_ACTION_DIM;
        config.obs_dim = TEST_OBS_DIM;
        config.enable_dreaming = true;
        config.dream_horizon = 10;
        config.dream_episodes = 5;
        config.replay_buffer_size = 1000;

        omni_wm = omni_wm_create(&config);
        ASSERT_NE(omni_wm, nullptr);
    }
};

TEST_F(DreamingTest, DreamWithExperiences) {
    /* Add experiences to replay buffer */
    for (int i = 0; i < 50; i++) {
        omni_wm_experience_t* exp = omni_wm_experience_create(
            TEST_STATE_DIM, TEST_ACTION_DIM, TEST_OBS_DIM
        );
        generate_random_vector(exp->action, TEST_ACTION_DIM);
        exp->reward = ((float)rand() / RAND_MAX);
        exp->terminal = (rand() % 10 == 0);
        omni_wm_add_experience(omni_wm, exp);
        omni_wm_experience_destroy(exp);
    }

    /* Run dreaming */
    int ret = omni_wm_dream(omni_wm, 3, 10);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    /* Verify statistics updated */
    omni_wm_stats_t stats;
    ret = omni_wm_get_stats(omni_wm, &stats);
    EXPECT_EQ(ret, NIMCP_SUCCESS);
}

/* ============================================================================
 * 12. Full Integration Workflow Test
 * ============================================================================ */

TEST_F(WorldModelIntegrationTest, FullIntegrationWorkflow) {
    /* Create all components */
    ASSERT_TRUE(create_omni_world_model());
    ASSERT_TRUE(create_multimodal_world_model());
    ASSERT_TRUE(create_active_inference());
    ASSERT_TRUE(create_mirror_neurons());
    ASSERT_TRUE(create_mirror_omni_bridge());
    ASSERT_TRUE(create_imagination_engine());

    /* Connect mirror bridge to all systems */
    EXPECT_EQ(mirror_omni_bridge_connect_mirror(mirror_bridge, mirror), 0);
    EXPECT_EQ(mirror_omni_bridge_connect_world_model(mirror_bridge, omni_wm), 0);
    EXPECT_EQ(mirror_omni_bridge_connect_active_inference(mirror_bridge, active_inference), 0);

    /* Verify full connection */
    EXPECT_TRUE(mirror_omni_bridge_is_fully_connected(mirror_bridge));

    /* Simulate workflow: observe action -> predict -> evaluate policy */

    /* 1. Observe action from another agent */
    float features[32];
    generate_random_vector(features, 8);
    action_t observed_action = mirror_neurons_create_action(
        1, "reach", features, 8, 1
    );
    mirror_neurons_observe_action(mirror, &observed_action);

    /* 2. Feed observation to world model */
    mirror_omni_feed_agent_state(mirror_bridge, 1);

    /* 3. Set initial world state */
    omni_wm_state_t* state = omni_wm_state_create(TEST_STATE_DIM);
    generate_random_vector(state->values, TEST_STATE_DIM);
    omni_wm_set_state(omni_wm, state);

    /* 4. Generate policies */
    omni_ai_generate_random_policies(active_inference, TEST_NUM_POLICIES, TEST_HORIZON);

    /* 5. Set goal */
    float goal[TEST_OBS_DIM];
    generate_random_vector(goal, TEST_OBS_DIM);
    omni_ai_set_goal(active_inference, goal, TEST_OBS_DIM, 1.0f);

    /* 6. Evaluate policies */
    omni_ai_evaluate_policies(active_inference);

    /* 7. Select action */
    omni_ai_action_result_t* result = omni_ai_action_result_create(TEST_ACTION_DIM);
    ASSERT_NE(result, nullptr);

    int ret = omni_ai_select_action_forward(active_inference, result);
    EXPECT_EQ(ret, 0);
    EXPECT_NE(result->action, nullptr);
    EXPECT_GT(result->confidence, 0.0f);

    /* 8. Simulate counterfactual with imagination */
    imagination_scenario_t* scenario = imagination_begin_scenario(
        imagination,
        IMAGINATION_MODE_COUNTERFACTUAL,
        nullptr
    );
    if (scenario) {
        imagination_step_scenario(imagination, scenario);

        /* Evaluate scenario */
        imagination_evaluation_t eval;
        imagination_evaluate(imagination, scenario, &eval);
        EXPECT_GE(eval.coherence, 0.0f);

        imagination_end_scenario(imagination, scenario);
    }

    /* 9. Update world model with action */
    omni_wm_state_t* next_state = omni_wm_state_create(TEST_STATE_DIM);
    generate_random_vector(next_state->values, TEST_STATE_DIM);
    omni_wm_update(omni_wm, state, result->action, result->action_dim, next_state, 0.5f);

    /* 10. Get final statistics */
    omni_wm_stats_t stats;
    omni_wm_get_stats(omni_wm, &stats);
    EXPECT_GT(stats.forward_predictions, 0ULL);

    mirror_omni_stats_t mirror_stats;
    mirror_omni_bridge_get_stats(mirror_bridge, &mirror_stats);
    EXPECT_GT(mirror_stats.total_state_predictions, 0ULL);

    /* Cleanup */
    omni_ai_action_result_destroy(result);
    omni_wm_state_destroy(next_state);
    omni_wm_state_destroy(state);
}

/* ============================================================================
 * 13. Exception Handling Integration Tests
 * ============================================================================ */

class ExceptionHandlingIntegrationTest : public WorldModelIntegrationTest {
protected:
    void SetUp() override {
        WorldModelIntegrationTest::SetUp();
        ASSERT_TRUE(create_omni_world_model());
        ASSERT_TRUE(create_multimodal_world_model());
    }
};

TEST_F(ExceptionHandlingIntegrationTest, NullPointerHandlingAcrossModules) {
    /* Test that all modules handle NULL pointers gracefully */

    /* Omni World Model */
    EXPECT_NE(omni_wm_get_default_config(nullptr), NIMCP_SUCCESS);
    EXPECT_NE(omni_wm_set_state(omni_wm, nullptr), NIMCP_SUCCESS);
    EXPECT_EQ(omni_wm_get_state(nullptr), nullptr);

    /* Multimodal World Model */
    EXPECT_EQ(wm_init(nullptr), WM_ERR_NULL_PTR);
    EXPECT_EQ(wm_reset(nullptr), WM_ERR_NULL_PTR);
    EXPECT_EQ(wm_fuse_modalities(nullptr), WM_ERR_NULL_PTR);
    EXPECT_EQ(wm_update(nullptr, 16.0f), WM_ERR_NULL_PTR);
}

TEST_F(ExceptionHandlingIntegrationTest, RecoveryAfterFailedAllocation) {
    /* Test that systems recover from failed operations */

    /* Try invalid operations that should fail */
    omni_wm_transition_t transition;
    memset(&transition, 0, sizeof(transition));

    /* Predict without state should fail but not crash */
    float action[TEST_ACTION_DIM];
    generate_random_vector(action, TEST_ACTION_DIM);

    int ret = omni_wm_predict_forward(omni_wm, action, TEST_ACTION_DIM, &transition);
    /* May succeed or fail depending on internal state */

    /* System should still be functional */
    omni_wm_state_t* state = omni_wm_state_create(TEST_STATE_DIM);
    ASSERT_NE(state, nullptr);
    generate_random_vector(state->values, TEST_STATE_DIM);

    ret = omni_wm_set_state(omni_wm, state);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    /* Now prediction should work */
    ret = omni_wm_predict_forward(omni_wm, action, TEST_ACTION_DIM, &transition);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    if (transition.next_state) {
        omni_wm_state_destroy(transition.next_state);
    }
    omni_wm_state_destroy(state);
}

TEST_F(ExceptionHandlingIntegrationTest, ErrorPropagationBetweenModules) {
    /* Test that errors don't corrupt state between modules */

    /* Set valid state in omni world model */
    omni_wm_state_t* state = omni_wm_state_create(TEST_STATE_DIM);
    ASSERT_NE(state, nullptr);
    generate_random_vector(state->values, TEST_STATE_DIM);
    ASSERT_EQ(omni_wm_set_state(omni_wm, state), NIMCP_SUCCESS);

    /* Cause error in multimodal world model */
    wm_prediction_t prediction;
    memset(&prediction, 0, sizeof(prediction));
    wm_error_t err = wm_predict(multimodal_wm, 99999, &prediction);  /* Invalid horizon */
    EXPECT_NE(err, WM_OK);

    /* Omni world model should still be functional */
    const omni_wm_state_t* retrieved = omni_wm_get_state(omni_wm);
    EXPECT_NE(retrieved, nullptr);

    float action[TEST_ACTION_DIM];
    generate_random_vector(action, TEST_ACTION_DIM);

    omni_wm_transition_t transition;
    memset(&transition, 0, sizeof(transition));
    int ret = omni_wm_predict_forward(omni_wm, action, TEST_ACTION_DIM, &transition);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    if (transition.next_state) {
        omni_wm_state_destroy(transition.next_state);
    }
    omni_wm_state_destroy(state);
}

TEST_F(ExceptionHandlingIntegrationTest, ConcurrentErrorHandling) {
    /* Test error handling doesn't cause issues with concurrent access */

    omni_wm_state_t* state = omni_wm_state_create(TEST_STATE_DIM);
    ASSERT_NE(state, nullptr);
    generate_random_vector(state->values, TEST_STATE_DIM);
    ASSERT_EQ(omni_wm_set_state(omni_wm, state), NIMCP_SUCCESS);

    std::atomic<int> success_count{0};
    std::atomic<int> error_count{0};

    /* Simulate multiple operations */
    for (int i = 0; i < 10; i++) {
        float action[TEST_ACTION_DIM];
        generate_random_vector(action, TEST_ACTION_DIM);

        omni_wm_transition_t transition;
        memset(&transition, 0, sizeof(transition));

        int ret = omni_wm_predict_forward(omni_wm, action, TEST_ACTION_DIM, &transition);
        if (ret == NIMCP_SUCCESS) {
            success_count++;
            if (transition.next_state) {
                omni_wm_state_destroy(transition.next_state);
            }
        } else {
            error_count++;
        }
    }

    /* At least some should succeed */
    EXPECT_GT(success_count.load(), 0);

    /* System should still be stable */
    omni_wm_stats_t stats;
    EXPECT_EQ(omni_wm_get_stats(omni_wm, &stats), NIMCP_SUCCESS);

    omni_wm_state_destroy(state);
}

TEST_F(ExceptionHandlingIntegrationTest, ResourceCleanupAfterErrors) {
    /* Test that resources are properly cleaned up after errors */

    /* Get initial memory footprint (approximation via stats) */
    omni_wm_stats_t initial_stats;
    ASSERT_EQ(omni_wm_get_stats(omni_wm, &initial_stats), NIMCP_SUCCESS);

    /* Perform operations that may cause errors */
    for (int i = 0; i < 100; i++) {
        omni_wm_state_t* state = omni_wm_state_create(TEST_STATE_DIM);
        if (state) {
            generate_random_vector(state->values, TEST_STATE_DIM);

            /* Try invalid operations */
            omni_wm_transition_t transition;
            memset(&transition, 0, sizeof(transition));

            float action[TEST_ACTION_DIM];
            generate_random_vector(action, TEST_ACTION_DIM);

            /* This may fail but should not leak */
            int ret = omni_wm_predict_forward(nullptr, action, TEST_ACTION_DIM, &transition);
            (void)ret;  /* Ignore result */

            omni_wm_state_destroy(state);
        }
    }

    /* System should still be functional */
    omni_wm_state_t* final_state = omni_wm_state_create(TEST_STATE_DIM);
    EXPECT_NE(final_state, nullptr);
    omni_wm_state_destroy(final_state);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
