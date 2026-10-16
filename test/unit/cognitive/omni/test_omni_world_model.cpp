/**
 * @file test_omni_world_model.cpp
 * @brief Comprehensive unit tests for Omnidirectional World Model
 *
 * WHAT: Tests for omnidirectional generative world model
 * WHY:  World model is critical for counterfactual reasoning and planning
 * HOW:  Tests all APIs: lifecycle, state, RSSM, dynamics, MDN, counterfactuals,
 *       rollouts, learning, experience replay, symlog, and bio-async integration
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <algorithm>
#include <random>

extern "C" {
#include "cognitive/omni/nimcp_omni_world_model.h"
#include "utils/error/nimcp_error_codes.h"
}

// =============================================================================
// Constants and Helpers
// =============================================================================

static constexpr float FLOAT_TOLERANCE = 1e-5f;
static constexpr uint32_t TEST_STATE_DIM = 16;
static constexpr uint32_t TEST_ACTION_DIM = 4;
static constexpr uint32_t TEST_OBS_DIM = 32;
static constexpr uint32_t TEST_LATENT_DIM = 24;
static constexpr uint32_t TEST_RSSM_H_DIM = 32;
static constexpr uint32_t TEST_RSSM_Z_DIM = 16;
static constexpr uint32_t TEST_MDN_COMPONENTS = 3;
static constexpr uint32_t TEST_HORIZON = 5;

static bool float_equals(float a, float b, float tol = FLOAT_TOLERANCE)
{
    return std::fabs(a - b) < tol;
}

static bool array_equals(const float* a, const float* b, uint32_t size, float tol = FLOAT_TOLERANCE)
{
    if (!a || !b) return false;
    for (uint32_t i = 0; i < size; i++) {
        if (!float_equals(a[i], b[i], tol)) return false;
    }
    return true;
}

// =============================================================================
// Test Fixture
// =============================================================================

class OmniWorldModelTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // Create default world model for most tests
        wm_ = omni_wm_create_simple(TEST_STATE_DIM, TEST_ACTION_DIM, TEST_OBS_DIM);
    }

    void TearDown() override
    {
        if (wm_) {
            omni_wm_destroy(wm_);
            wm_ = nullptr;
        }
    }

    // Helper to create state with test values
    omni_wm_state_t* create_test_state(uint32_t dim, float fill_value)
    {
        std::vector<float> values(dim);
        for (uint32_t i = 0; i < dim; i++) {
            values[i] = fill_value + (float)i * 0.01f;
        }
        return omni_wm_state_from_values(values.data(), dim);
    }

    // Helper to create random action
    std::vector<float> create_random_action(uint32_t dim)
    {
        std::vector<float> action(dim);
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
        for (uint32_t i = 0; i < dim; i++) {
            action[i] = dist(gen);
        }
        return action;
    }

    // Helper to create observation
    std::vector<float> create_test_observation(uint32_t dim, float base_value)
    {
        std::vector<float> obs(dim);
        for (uint32_t i = 0; i < dim; i++) {
            obs[i] = base_value + (float)i * 0.1f;
        }
        return obs;
    }

    omni_world_model_t* wm_ = nullptr;
};

// =============================================================================
// 1. Lifecycle Tests
// =============================================================================

TEST_F(OmniWorldModelTest, CreateSimpleBasic)
{
    // wm_ was created in SetUp
    ASSERT_NE(wm_, nullptr);
}

TEST_F(OmniWorldModelTest, CreateSimpleZeroStateDimFails)
{
    omni_world_model_t* wm = omni_wm_create_simple(0, TEST_ACTION_DIM, TEST_OBS_DIM);
    EXPECT_EQ(wm, nullptr);
}

TEST_F(OmniWorldModelTest, CreateSimpleZeroActionDimFails)
{
    omni_world_model_t* wm = omni_wm_create_simple(TEST_STATE_DIM, 0, TEST_OBS_DIM);
    EXPECT_EQ(wm, nullptr);
}

TEST_F(OmniWorldModelTest, CreateSimpleZeroObsDimFails)
{
    omni_world_model_t* wm = omni_wm_create_simple(TEST_STATE_DIM, TEST_ACTION_DIM, 0);
    EXPECT_EQ(wm, nullptr);
}

TEST_F(OmniWorldModelTest, CreateSimpleMaxDimensions)
{
    omni_world_model_t* wm = omni_wm_create_simple(
        OMNI_WM_MAX_STATE_DIM, OMNI_WM_MAX_ACTION_DIM, OMNI_WM_MAX_OBS_DIM);
    ASSERT_NE(wm, nullptr);
    omni_wm_destroy(wm);
}

TEST_F(OmniWorldModelTest, CreateSimpleExceedsMaxStateDimFails)
{
    omni_world_model_t* wm = omni_wm_create_simple(
        OMNI_WM_MAX_STATE_DIM + 1, TEST_ACTION_DIM, TEST_OBS_DIM);
    EXPECT_EQ(wm, nullptr);
}

TEST_F(OmniWorldModelTest, CreateWithNullConfig)
{
    omni_world_model_t* wm = omni_wm_create(nullptr);
    ASSERT_NE(wm, nullptr);
    omni_wm_destroy(wm);
}

TEST_F(OmniWorldModelTest, CreateWithCustomConfig)
{
    omni_wm_config_t config;
    EXPECT_EQ(omni_wm_get_default_config(&config), NIMCP_SUCCESS);

    config.state_dim = 32;
    config.action_dim = 8;
    config.obs_dim = 64;
    config.enable_lateral = true;
    config.enable_hierarchical = true;
    config.enable_dreaming = true;
    config.use_symlog_rewards = true;
    config.use_rssm = true;
    config.use_mdn = true;

    omni_world_model_t* wm = omni_wm_create(&config);
    ASSERT_NE(wm, nullptr);
    omni_wm_destroy(wm);
}

TEST_F(OmniWorldModelTest, GetDefaultConfigNullFails)
{
    nimcp_error_t result = omni_wm_get_default_config(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWorldModelTest, GetDefaultConfigSetsReasonableValues)
{
    omni_wm_config_t config;
    ASSERT_EQ(omni_wm_get_default_config(&config), NIMCP_SUCCESS);

    EXPECT_GT(config.state_dim, 0u);
    EXPECT_LE(config.state_dim, OMNI_WM_MAX_STATE_DIM);
    EXPECT_GT(config.action_dim, 0u);
    EXPECT_LE(config.action_dim, OMNI_WM_MAX_ACTION_DIM);
    EXPECT_GT(config.obs_dim, 0u);
    EXPECT_LE(config.obs_dim, OMNI_WM_MAX_OBS_DIM);
    EXPECT_GT(config.learning_rate, 0.0f);
    EXPECT_LE(config.learning_rate, 1.0f);
    EXPECT_GT(config.discount_factor, 0.0f);
    EXPECT_LE(config.discount_factor, 1.0f);
}

TEST_F(OmniWorldModelTest, DestroyNullSafe)
{
    // Should not crash
    omni_wm_destroy(nullptr);
}

TEST_F(OmniWorldModelTest, DestroyValidModel)
{
    omni_world_model_t* wm = omni_wm_create_simple(TEST_STATE_DIM, TEST_ACTION_DIM, TEST_OBS_DIM);
    ASSERT_NE(wm, nullptr);
    // Should not crash
    omni_wm_destroy(wm);
}

// =============================================================================
// 2. State Management Tests
// =============================================================================

TEST_F(OmniWorldModelTest, StateCreateBasic)
{
    omni_wm_state_t* state = omni_wm_state_create(TEST_STATE_DIM);
    ASSERT_NE(state, nullptr);
    EXPECT_NE(state->values, nullptr);
    EXPECT_EQ(state->dim, TEST_STATE_DIM);
    omni_wm_state_destroy(state);
}

TEST_F(OmniWorldModelTest, StateCreateZeroDimFails)
{
    omni_wm_state_t* state = omni_wm_state_create(0);
    EXPECT_EQ(state, nullptr);
}

TEST_F(OmniWorldModelTest, StateCreateExceedsMaxFails)
{
    omni_wm_state_t* state = omni_wm_state_create(OMNI_WM_MAX_STATE_DIM + 1);
    EXPECT_EQ(state, nullptr);
}

TEST_F(OmniWorldModelTest, StateFromValuesBasic)
{
    std::vector<float> values = {1.0f, 2.0f, 3.0f, 4.0f};
    omni_wm_state_t* state = omni_wm_state_from_values(values.data(), 4);

    ASSERT_NE(state, nullptr);
    EXPECT_EQ(state->dim, 4u);
    EXPECT_TRUE(array_equals(state->values, values.data(), 4));

    omni_wm_state_destroy(state);
}

TEST_F(OmniWorldModelTest, StateFromValuesNullFails)
{
    omni_wm_state_t* state = omni_wm_state_from_values(nullptr, 4);
    EXPECT_EQ(state, nullptr);
}

TEST_F(OmniWorldModelTest, StateFromValuesZeroDimFails)
{
    std::vector<float> values = {1.0f, 2.0f};
    omni_wm_state_t* state = omni_wm_state_from_values(values.data(), 0);
    EXPECT_EQ(state, nullptr);
}

TEST_F(OmniWorldModelTest, StateCloneBasic)
{
    omni_wm_state_t* original = create_test_state(TEST_STATE_DIM, 1.0f);
    ASSERT_NE(original, nullptr);

    omni_wm_state_t* clone = omni_wm_state_clone(original);
    ASSERT_NE(clone, nullptr);

    EXPECT_EQ(clone->dim, original->dim);
    EXPECT_NE(clone->values, original->values);  // Deep copy
    EXPECT_TRUE(array_equals(clone->values, original->values, original->dim));
    EXPECT_FLOAT_EQ(clone->uncertainty, original->uncertainty);
    EXPECT_EQ(clone->level, original->level);

    omni_wm_state_destroy(original);
    omni_wm_state_destroy(clone);
}

TEST_F(OmniWorldModelTest, StateCloneNullFails)
{
    omni_wm_state_t* clone = omni_wm_state_clone(nullptr);
    EXPECT_EQ(clone, nullptr);
}

TEST_F(OmniWorldModelTest, StateDestroyNullSafe)
{
    // Should not crash
    omni_wm_state_destroy(nullptr);
}

TEST_F(OmniWorldModelTest, SetStateBasic)
{
    omni_wm_state_t* state = create_test_state(TEST_STATE_DIM, 1.5f);
    ASSERT_NE(state, nullptr);

    nimcp_error_t result = omni_wm_set_state(wm_, state);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    const omni_wm_state_t* retrieved = omni_wm_get_state(wm_);
    ASSERT_NE(retrieved, nullptr);
    EXPECT_TRUE(array_equals(retrieved->values, state->values, state->dim));

    omni_wm_state_destroy(state);
}

TEST_F(OmniWorldModelTest, SetStateNullModelFails)
{
    omni_wm_state_t* state = create_test_state(TEST_STATE_DIM, 1.0f);
    ASSERT_NE(state, nullptr);

    nimcp_error_t result = omni_wm_set_state(nullptr, state);
    EXPECT_NE(result, NIMCP_SUCCESS);

    omni_wm_state_destroy(state);
}

TEST_F(OmniWorldModelTest, SetStateNullStateFails)
{
    nimcp_error_t result = omni_wm_set_state(wm_, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWorldModelTest, GetStateNullModelReturnsNull)
{
    const omni_wm_state_t* state = omni_wm_get_state(nullptr);
    EXPECT_EQ(state, nullptr);
}

// =============================================================================
// 3. RSSM State Tests
// =============================================================================

TEST_F(OmniWorldModelTest, RssmStateCreateBasic)
{
    omni_wm_rssm_state_t* rssm = omni_wm_rssm_state_create(TEST_RSSM_H_DIM, TEST_RSSM_Z_DIM);
    ASSERT_NE(rssm, nullptr);
    EXPECT_NE(rssm->h, nullptr);
    EXPECT_NE(rssm->z, nullptr);
    EXPECT_NE(rssm->z_mean, nullptr);
    EXPECT_NE(rssm->z_std, nullptr);
    EXPECT_EQ(rssm->h_dim, TEST_RSSM_H_DIM);
    EXPECT_EQ(rssm->z_dim, TEST_RSSM_Z_DIM);
    omni_wm_rssm_state_destroy(rssm);
}

TEST_F(OmniWorldModelTest, RssmStateCreateZeroHDimFails)
{
    omni_wm_rssm_state_t* rssm = omni_wm_rssm_state_create(0, TEST_RSSM_Z_DIM);
    EXPECT_EQ(rssm, nullptr);
}

TEST_F(OmniWorldModelTest, RssmStateCreateZeroZDimFails)
{
    omni_wm_rssm_state_t* rssm = omni_wm_rssm_state_create(TEST_RSSM_H_DIM, 0);
    EXPECT_EQ(rssm, nullptr);
}

TEST_F(OmniWorldModelTest, RssmStateCloneBasic)
{
    omni_wm_rssm_state_t* original = omni_wm_rssm_state_create(TEST_RSSM_H_DIM, TEST_RSSM_Z_DIM);
    ASSERT_NE(original, nullptr);

    // Fill with test values
    for (uint32_t i = 0; i < TEST_RSSM_H_DIM; i++) {
        original->h[i] = (float)i * 0.1f;
    }
    for (uint32_t i = 0; i < TEST_RSSM_Z_DIM; i++) {
        original->z[i] = (float)i * 0.2f;
        original->z_mean[i] = (float)i * 0.15f;
        original->z_std[i] = 0.1f;
    }
    original->timestamp = 123.456;

    omni_wm_rssm_state_t* clone = omni_wm_rssm_state_clone(original);
    ASSERT_NE(clone, nullptr);

    EXPECT_EQ(clone->h_dim, original->h_dim);
    EXPECT_EQ(clone->z_dim, original->z_dim);
    EXPECT_NE(clone->h, original->h);  // Deep copy
    EXPECT_NE(clone->z, original->z);
    EXPECT_TRUE(array_equals(clone->h, original->h, original->h_dim));
    EXPECT_TRUE(array_equals(clone->z, original->z, original->z_dim));
    EXPECT_TRUE(array_equals(clone->z_mean, original->z_mean, original->z_dim));
    EXPECT_TRUE(array_equals(clone->z_std, original->z_std, original->z_dim));
    EXPECT_DOUBLE_EQ(clone->timestamp, original->timestamp);

    omni_wm_rssm_state_destroy(original);
    omni_wm_rssm_state_destroy(clone);
}

TEST_F(OmniWorldModelTest, RssmStateCloneNullFails)
{
    omni_wm_rssm_state_t* clone = omni_wm_rssm_state_clone(nullptr);
    EXPECT_EQ(clone, nullptr);
}

TEST_F(OmniWorldModelTest, RssmStateDestroyNullSafe)
{
    // Should not crash
    omni_wm_rssm_state_destroy(nullptr);
}

TEST_F(OmniWorldModelTest, RssmStepBasic)
{
    // Create RSSM-enabled world model
    omni_wm_config_t config;
    omni_wm_get_default_config(&config);
    config.use_rssm = true;
    config.rssm_h_dim = TEST_RSSM_H_DIM;
    config.rssm_z_dim = TEST_RSSM_Z_DIM;
    config.action_dim = TEST_ACTION_DIM;

    omni_world_model_t* wm = omni_wm_create(&config);
    ASSERT_NE(wm, nullptr);

    omni_wm_rssm_state_t* state = omni_wm_rssm_state_create(TEST_RSSM_H_DIM, TEST_RSSM_Z_DIM);
    ASSERT_NE(state, nullptr);

    omni_wm_rssm_state_t* next_state = omni_wm_rssm_state_create(TEST_RSSM_H_DIM, TEST_RSSM_Z_DIM);
    ASSERT_NE(next_state, nullptr);

    std::vector<float> action = create_random_action(TEST_ACTION_DIM);

    nimcp_error_t result = omni_wm_rssm_step(wm, state, action.data(), TEST_ACTION_DIM, next_state);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    omni_wm_rssm_state_destroy(state);
    omni_wm_rssm_state_destroy(next_state);
    omni_wm_destroy(wm);
}

TEST_F(OmniWorldModelTest, RssmStepNullModelFails)
{
    omni_wm_rssm_state_t* state = omni_wm_rssm_state_create(TEST_RSSM_H_DIM, TEST_RSSM_Z_DIM);
    omni_wm_rssm_state_t* next_state = omni_wm_rssm_state_create(TEST_RSSM_H_DIM, TEST_RSSM_Z_DIM);
    std::vector<float> action(TEST_ACTION_DIM, 0.5f);

    nimcp_error_t result = omni_wm_rssm_step(nullptr, state, action.data(), TEST_ACTION_DIM, next_state);
    EXPECT_NE(result, NIMCP_SUCCESS);

    omni_wm_rssm_state_destroy(state);
    omni_wm_rssm_state_destroy(next_state);
}

TEST_F(OmniWorldModelTest, RssmImagineBasic)
{
    omni_wm_config_t config;
    omni_wm_get_default_config(&config);
    config.use_rssm = true;
    config.rssm_h_dim = TEST_RSSM_H_DIM;
    config.rssm_z_dim = TEST_RSSM_Z_DIM;
    config.action_dim = TEST_ACTION_DIM;

    omni_world_model_t* wm = omni_wm_create(&config);
    ASSERT_NE(wm, nullptr);

    omni_wm_rssm_state_t* initial_state = omni_wm_rssm_state_create(TEST_RSSM_H_DIM, TEST_RSSM_Z_DIM);
    ASSERT_NE(initial_state, nullptr);

    // Create action sequence
    std::vector<std::vector<float>> action_sequence(TEST_HORIZON);
    std::vector<const float*> action_ptrs(TEST_HORIZON);
    for (uint32_t t = 0; t < TEST_HORIZON; t++) {
        action_sequence[t] = create_random_action(TEST_ACTION_DIM);
        action_ptrs[t] = action_sequence[t].data();
    }

    // Allocate trajectory
    std::vector<omni_wm_rssm_state_t*> trajectory(TEST_HORIZON);
    for (uint32_t t = 0; t < TEST_HORIZON; t++) {
        trajectory[t] = omni_wm_rssm_state_create(TEST_RSSM_H_DIM, TEST_RSSM_Z_DIM);
        ASSERT_NE(trajectory[t], nullptr);
    }

    nimcp_error_t result = omni_wm_rssm_imagine(wm, initial_state, action_ptrs.data(),
                                                 TEST_HORIZON, trajectory.data());
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Clean up
    for (uint32_t t = 0; t < TEST_HORIZON; t++) {
        omni_wm_rssm_state_destroy(trajectory[t]);
    }
    omni_wm_rssm_state_destroy(initial_state);
    omni_wm_destroy(wm);
}

TEST_F(OmniWorldModelTest, GetSetRssmState)
{
    omni_wm_config_t config;
    omni_wm_get_default_config(&config);
    config.use_rssm = true;
    config.rssm_h_dim = TEST_RSSM_H_DIM;
    config.rssm_z_dim = TEST_RSSM_Z_DIM;

    omni_world_model_t* wm = omni_wm_create(&config);
    ASSERT_NE(wm, nullptr);

    omni_wm_rssm_state_t* state = omni_wm_rssm_state_create(TEST_RSSM_H_DIM, TEST_RSSM_Z_DIM);
    ASSERT_NE(state, nullptr);

    // Set distinctive values
    for (uint32_t i = 0; i < TEST_RSSM_H_DIM; i++) {
        state->h[i] = (float)i;
    }

    nimcp_error_t result = omni_wm_set_rssm_state(wm, state);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    const omni_wm_rssm_state_t* retrieved = omni_wm_get_rssm_state(wm);
    ASSERT_NE(retrieved, nullptr);
    EXPECT_TRUE(array_equals(retrieved->h, state->h, state->h_dim));

    omni_wm_rssm_state_destroy(state);
    omni_wm_destroy(wm);
}

// =============================================================================
// 4. Dynamics Tests
// =============================================================================

TEST_F(OmniWorldModelTest, PredictForwardBasic)
{
    omni_wm_state_t* state = create_test_state(TEST_STATE_DIM, 1.0f);
    ASSERT_NE(state, nullptr);

    nimcp_error_t result = omni_wm_set_state(wm_, state);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    std::vector<float> action = create_random_action(TEST_ACTION_DIM);
    omni_wm_transition_t transition;
    memset(&transition, 0, sizeof(transition));

    result = omni_wm_predict_forward(wm_, action.data(), TEST_ACTION_DIM, &transition);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_NE(transition.next_state, nullptr);
    EXPECT_EQ(transition.direction, OMNI_WM_DIR_FORWARD);

    if (transition.next_state) {
        omni_wm_state_destroy(transition.next_state);
    }
    omni_wm_state_destroy(state);
}

TEST_F(OmniWorldModelTest, PredictForwardNullModelFails)
{
    std::vector<float> action(TEST_ACTION_DIM, 0.0f);
    omni_wm_transition_t transition;

    nimcp_error_t result = omni_wm_predict_forward(nullptr, action.data(), TEST_ACTION_DIM, &transition);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWorldModelTest, PredictForwardNullActionFails)
{
    omni_wm_transition_t transition;

    nimcp_error_t result = omni_wm_predict_forward(wm_, nullptr, TEST_ACTION_DIM, &transition);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWorldModelTest, PredictForwardNullResultFails)
{
    std::vector<float> action(TEST_ACTION_DIM, 0.0f);

    nimcp_error_t result = omni_wm_predict_forward(wm_, action.data(), TEST_ACTION_DIM, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWorldModelTest, InferBackwardBasic)
{
    omni_wm_state_t* state = create_test_state(TEST_STATE_DIM, 2.0f);
    ASSERT_NE(state, nullptr);

    omni_wm_transition_t transition;
    memset(&transition, 0, sizeof(transition));

    nimcp_error_t result = omni_wm_infer_backward(wm_, state, &transition);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(transition.direction, OMNI_WM_DIR_BACKWARD);

    if (transition.next_state) {
        omni_wm_state_destroy(transition.next_state);
    }
    omni_wm_state_destroy(state);
}

TEST_F(OmniWorldModelTest, InferBackwardNullModelFails)
{
    omni_wm_state_t* state = create_test_state(TEST_STATE_DIM, 1.0f);
    omni_wm_transition_t transition;

    nimcp_error_t result = omni_wm_infer_backward(nullptr, state, &transition);
    EXPECT_NE(result, NIMCP_SUCCESS);

    omni_wm_state_destroy(state);
}

TEST_F(OmniWorldModelTest, PredictLateralBasic)
{
    omni_wm_config_t config;
    omni_wm_get_default_config(&config);
    config.enable_lateral = true;
    config.state_dim = TEST_STATE_DIM;

    omni_world_model_t* wm = omni_wm_create(&config);
    ASSERT_NE(wm, nullptr);

    omni_wm_state_t* source = create_test_state(TEST_STATE_DIM, 1.0f);
    ASSERT_NE(source, nullptr);

    omni_wm_state_t result_state;
    result_state.values = (float*)malloc(TEST_STATE_DIM * sizeof(float));
    result_state.dim = TEST_STATE_DIM;

    nimcp_error_t result = omni_wm_predict_lateral(wm, source, 1, &result_state);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    free(result_state.values);
    omni_wm_state_destroy(source);
    omni_wm_destroy(wm);
}

TEST_F(OmniWorldModelTest, PredictHierarchicalBasic)
{
    omni_wm_config_t config;
    omni_wm_get_default_config(&config);
    config.enable_hierarchical = true;
    config.num_levels = 3;
    config.state_dim = TEST_STATE_DIM;

    omni_world_model_t* wm = omni_wm_create(&config);
    ASSERT_NE(wm, nullptr);

    omni_wm_state_t* state = create_test_state(TEST_STATE_DIM, 1.0f);
    state->level = 0;
    ASSERT_NE(state, nullptr);

    omni_wm_state_t result_state;
    result_state.values = (float*)malloc(TEST_STATE_DIM * sizeof(float));
    result_state.dim = TEST_STATE_DIM;

    nimcp_error_t result = omni_wm_predict_hierarchical(wm, state, 1, &result_state);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    free(result_state.values);
    omni_wm_state_destroy(state);
    omni_wm_destroy(wm);
}

// =============================================================================
// 5. Latent Encoding Tests (JEPA)
// =============================================================================

TEST_F(OmniWorldModelTest, LatentCreateBasic)
{
    omni_wm_latent_t* latent = omni_wm_latent_create(TEST_LATENT_DIM);
    ASSERT_NE(latent, nullptr);
    EXPECT_NE(latent->embedding, nullptr);
    EXPECT_EQ(latent->dim, TEST_LATENT_DIM);
    omni_wm_latent_destroy(latent);
}

TEST_F(OmniWorldModelTest, LatentCreateZeroDimFails)
{
    omni_wm_latent_t* latent = omni_wm_latent_create(0);
    EXPECT_EQ(latent, nullptr);
}

TEST_F(OmniWorldModelTest, LatentCreateExceedsMaxFails)
{
    omni_wm_latent_t* latent = omni_wm_latent_create(OMNI_WM_MAX_LATENT_DIM + 1);
    EXPECT_EQ(latent, nullptr);
}

TEST_F(OmniWorldModelTest, LatentDestroyNullSafe)
{
    // Should not crash
    omni_wm_latent_destroy(nullptr);
}

TEST_F(OmniWorldModelTest, EncodeBasic)
{
    omni_wm_config_t config;
    omni_wm_get_default_config(&config);
    config.latent_dim = TEST_LATENT_DIM;
    config.obs_dim = TEST_OBS_DIM;

    omni_world_model_t* wm = omni_wm_create(&config);
    ASSERT_NE(wm, nullptr);

    std::vector<float> observation = create_test_observation(TEST_OBS_DIM, 1.0f);
    omni_wm_latent_t* latent = omni_wm_latent_create(TEST_LATENT_DIM);
    ASSERT_NE(latent, nullptr);

    nimcp_error_t result = omni_wm_encode(wm, observation.data(), TEST_OBS_DIM, latent);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    omni_wm_latent_destroy(latent);
    omni_wm_destroy(wm);
}

TEST_F(OmniWorldModelTest, EncodeNullModelFails)
{
    std::vector<float> observation(TEST_OBS_DIM, 1.0f);
    omni_wm_latent_t* latent = omni_wm_latent_create(TEST_LATENT_DIM);

    nimcp_error_t result = omni_wm_encode(nullptr, observation.data(), TEST_OBS_DIM, latent);
    EXPECT_NE(result, NIMCP_SUCCESS);

    omni_wm_latent_destroy(latent);
}

TEST_F(OmniWorldModelTest, DecodeBasic)
{
    omni_wm_config_t config;
    omni_wm_get_default_config(&config);
    config.latent_dim = TEST_LATENT_DIM;
    config.obs_dim = TEST_OBS_DIM;

    omni_world_model_t* wm = omni_wm_create(&config);
    ASSERT_NE(wm, nullptr);

    omni_wm_latent_t* latent = omni_wm_latent_create(TEST_LATENT_DIM);
    ASSERT_NE(latent, nullptr);

    // Fill with test values
    for (uint32_t i = 0; i < TEST_LATENT_DIM; i++) {
        latent->embedding[i] = (float)i * 0.1f;
    }

    std::vector<float> observation(TEST_OBS_DIM);
    nimcp_error_t result = omni_wm_decode(wm, latent, observation.data(), TEST_OBS_DIM);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    omni_wm_latent_destroy(latent);
    omni_wm_destroy(wm);
}

TEST_F(OmniWorldModelTest, PredictLatentBasic)
{
    omni_wm_config_t config;
    omni_wm_get_default_config(&config);
    config.latent_dim = TEST_LATENT_DIM;
    config.action_dim = TEST_ACTION_DIM;

    omni_world_model_t* wm = omni_wm_create(&config);
    ASSERT_NE(wm, nullptr);

    omni_wm_latent_t* latent = omni_wm_latent_create(TEST_LATENT_DIM);
    omni_wm_latent_t* predicted = omni_wm_latent_create(TEST_LATENT_DIM);
    ASSERT_NE(latent, nullptr);
    ASSERT_NE(predicted, nullptr);

    // Fill with test values
    for (uint32_t i = 0; i < TEST_LATENT_DIM; i++) {
        latent->embedding[i] = (float)i * 0.1f;
    }

    std::vector<float> action = create_random_action(TEST_ACTION_DIM);
    nimcp_error_t result = omni_wm_predict_latent(wm, latent, action.data(), TEST_ACTION_DIM, predicted);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    omni_wm_latent_destroy(latent);
    omni_wm_latent_destroy(predicted);
    omni_wm_destroy(wm);
}

// =============================================================================
// 6. MDN Tests
// =============================================================================

TEST_F(OmniWorldModelTest, MdnCreateBasic)
{
    omni_wm_mdn_prediction_t* mdn = omni_wm_mdn_create(TEST_MDN_COMPONENTS, TEST_STATE_DIM);
    ASSERT_NE(mdn, nullptr);
    EXPECT_NE(mdn->components, nullptr);
    EXPECT_EQ(mdn->num_components, TEST_MDN_COMPONENTS);
    EXPECT_EQ(mdn->dim, TEST_STATE_DIM);
    omni_wm_mdn_destroy(mdn);
}

TEST_F(OmniWorldModelTest, MdnCreateZeroComponentsFails)
{
    omni_wm_mdn_prediction_t* mdn = omni_wm_mdn_create(0, TEST_STATE_DIM);
    EXPECT_EQ(mdn, nullptr);
}

TEST_F(OmniWorldModelTest, MdnCreateZeroDimFails)
{
    omni_wm_mdn_prediction_t* mdn = omni_wm_mdn_create(TEST_MDN_COMPONENTS, 0);
    EXPECT_EQ(mdn, nullptr);
}

TEST_F(OmniWorldModelTest, MdnCreateExceedsMaxComponentsFails)
{
    omni_wm_mdn_prediction_t* mdn = omni_wm_mdn_create(OMNI_WM_MAX_MDN_COMPONENTS + 1, TEST_STATE_DIM);
    EXPECT_EQ(mdn, nullptr);
}

TEST_F(OmniWorldModelTest, MdnDestroyNullSafe)
{
    // Should not crash
    omni_wm_mdn_destroy(nullptr);
}

TEST_F(OmniWorldModelTest, PredictMdnBasic)
{
    omni_wm_config_t config;
    omni_wm_get_default_config(&config);
    config.use_mdn = true;
    config.mdn_components = TEST_MDN_COMPONENTS;
    config.state_dim = TEST_STATE_DIM;
    config.action_dim = TEST_ACTION_DIM;

    omni_world_model_t* wm = omni_wm_create(&config);
    ASSERT_NE(wm, nullptr);

    omni_wm_state_t* state = create_test_state(TEST_STATE_DIM, 1.0f);
    ASSERT_NE(state, nullptr);

    omni_wm_set_state(wm, state);

    omni_wm_mdn_prediction_t* pred = omni_wm_mdn_create(TEST_MDN_COMPONENTS, TEST_STATE_DIM);
    ASSERT_NE(pred, nullptr);

    std::vector<float> action = create_random_action(TEST_ACTION_DIM);
    nimcp_error_t result = omni_wm_predict_mdn(wm, state, action.data(), TEST_ACTION_DIM, pred);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    omni_wm_mdn_destroy(pred);
    omni_wm_state_destroy(state);
    omni_wm_destroy(wm);
}

TEST_F(OmniWorldModelTest, MdnSampleBasic)
{
    omni_wm_mdn_prediction_t* pred = omni_wm_mdn_create(TEST_MDN_COMPONENTS, TEST_STATE_DIM);
    ASSERT_NE(pred, nullptr);

    // Initialize with valid distribution
    float weight_sum = 0.0f;
    for (uint32_t k = 0; k < TEST_MDN_COMPONENTS; k++) {
        pred->components[k].weight = 1.0f / (float)TEST_MDN_COMPONENTS;
        weight_sum += pred->components[k].weight;
        for (uint32_t i = 0; i < TEST_STATE_DIM; i++) {
            pred->components[k].mean[i] = (float)k + (float)i * 0.1f;
            pred->components[k].std[i] = 0.1f;
        }
    }

    std::vector<float> sample(TEST_STATE_DIM);
    nimcp_error_t result = omni_wm_mdn_sample(pred, sample.data());
    EXPECT_EQ(result, NIMCP_SUCCESS);

    omni_wm_mdn_destroy(pred);
}

TEST_F(OmniWorldModelTest, MdnSampleNullPredFails)
{
    std::vector<float> sample(TEST_STATE_DIM);
    nimcp_error_t result = omni_wm_mdn_sample(nullptr, sample.data());
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWorldModelTest, MdnModeBasic)
{
    omni_wm_mdn_prediction_t* pred = omni_wm_mdn_create(TEST_MDN_COMPONENTS, TEST_STATE_DIM);
    ASSERT_NE(pred, nullptr);

    // Set component with highest weight
    for (uint32_t k = 0; k < TEST_MDN_COMPONENTS; k++) {
        pred->components[k].weight = (k == 1) ? 0.8f : 0.1f;  // Component 1 has highest weight
        for (uint32_t i = 0; i < TEST_STATE_DIM; i++) {
            pred->components[k].mean[i] = (float)k + (float)i * 0.1f;
            pred->components[k].std[i] = 0.1f;
        }
    }

    std::vector<float> mode(TEST_STATE_DIM);
    nimcp_error_t result = omni_wm_mdn_mode(pred, mode.data());
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Mode should be from component 1
    EXPECT_TRUE(float_equals(mode[0], 1.0f, 0.01f));

    omni_wm_mdn_destroy(pred);
}

TEST_F(OmniWorldModelTest, MdnLogProbBasic)
{
    omni_wm_mdn_prediction_t* pred = omni_wm_mdn_create(TEST_MDN_COMPONENTS, TEST_STATE_DIM);
    ASSERT_NE(pred, nullptr);

    // Initialize valid distribution
    for (uint32_t k = 0; k < TEST_MDN_COMPONENTS; k++) {
        pred->components[k].weight = 1.0f / (float)TEST_MDN_COMPONENTS;
        for (uint32_t i = 0; i < TEST_STATE_DIM; i++) {
            pred->components[k].mean[i] = 0.0f;
            pred->components[k].std[i] = 1.0f;
        }
    }

    std::vector<float> value(TEST_STATE_DIM, 0.0f);
    float log_prob = omni_wm_mdn_log_prob(pred, value.data());

    // Log prob at mean should be relatively high (less negative)
    EXPECT_GT(log_prob, -100.0f);

    omni_wm_mdn_destroy(pred);
}

TEST_F(OmniWorldModelTest, MdnLogProbNullPredReturnsNegInf)
{
    std::vector<float> value(TEST_STATE_DIM, 0.0f);
    float log_prob = omni_wm_mdn_log_prob(nullptr, value.data());
    EXPECT_LT(log_prob, -1e30f);  // Should be very negative (or -inf)
}

// =============================================================================
// 7. Counterfactual Tests
// =============================================================================

TEST_F(OmniWorldModelTest, CfQueryCreateBasic)
{
    omni_wm_state_t* state = create_test_state(TEST_STATE_DIM, 1.0f);
    ASSERT_NE(state, nullptr);

    std::vector<float> action = create_random_action(TEST_ACTION_DIM);

    omni_wm_counterfactual_query_t* query = omni_wm_cf_query_create(
        OMNI_WM_CF_ACTION, state, action.data(), TEST_ACTION_DIM, TEST_HORIZON);

    ASSERT_NE(query, nullptr);
    EXPECT_EQ(query->type, OMNI_WM_CF_ACTION);
    EXPECT_EQ(query->horizon, TEST_HORIZON);
    EXPECT_EQ(query->action_dim, TEST_ACTION_DIM);

    omni_wm_cf_query_destroy(query);
    omni_wm_state_destroy(state);
}

TEST_F(OmniWorldModelTest, CfQueryCreateAllTypes)
{
    omni_wm_state_t* state = create_test_state(TEST_STATE_DIM, 1.0f);
    std::vector<float> action(TEST_ACTION_DIM, 0.5f);

    // Test all counterfactual types
    omni_wm_counterfactual_type_t types[] = {
        OMNI_WM_CF_ACTION, OMNI_WM_CF_STATE, OMNI_WM_CF_CONTEXT, OMNI_WM_CF_GOAL
    };

    for (auto type : types) {
        omni_wm_counterfactual_query_t* query = omni_wm_cf_query_create(
            type, state, action.data(), TEST_ACTION_DIM, TEST_HORIZON);
        EXPECT_NE(query, nullptr);
        EXPECT_EQ(query->type, type);
        omni_wm_cf_query_destroy(query);
    }

    omni_wm_state_destroy(state);
}

TEST_F(OmniWorldModelTest, CfQueryCreateNullStateFails)
{
    std::vector<float> action(TEST_ACTION_DIM, 0.5f);

    omni_wm_counterfactual_query_t* query = omni_wm_cf_query_create(
        OMNI_WM_CF_ACTION, nullptr, action.data(), TEST_ACTION_DIM, TEST_HORIZON);
    EXPECT_EQ(query, nullptr);
}

TEST_F(OmniWorldModelTest, CfQueryCreateZeroHorizonFails)
{
    omni_wm_state_t* state = create_test_state(TEST_STATE_DIM, 1.0f);
    std::vector<float> action(TEST_ACTION_DIM, 0.5f);

    omni_wm_counterfactual_query_t* query = omni_wm_cf_query_create(
        OMNI_WM_CF_ACTION, state, action.data(), TEST_ACTION_DIM, 0);
    EXPECT_EQ(query, nullptr);

    omni_wm_state_destroy(state);
}

TEST_F(OmniWorldModelTest, CfQueryDestroyNullSafe)
{
    // Should not crash
    omni_wm_cf_query_destroy(nullptr);
}

TEST_F(OmniWorldModelTest, CounterfactualBasic)
{
    omni_wm_state_t* state = create_test_state(TEST_STATE_DIM, 1.0f);
    ASSERT_NE(state, nullptr);

    omni_wm_set_state(wm_, state);

    std::vector<float> action = create_random_action(TEST_ACTION_DIM);
    omni_wm_counterfactual_query_t* query = omni_wm_cf_query_create(
        OMNI_WM_CF_ACTION, state, action.data(), TEST_ACTION_DIM, TEST_HORIZON);
    ASSERT_NE(query, nullptr);

    omni_wm_counterfactual_result_t result;
    memset(&result, 0, sizeof(result));

    nimcp_error_t err = omni_wm_counterfactual(wm_, query, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_NE(result.trajectory, nullptr);
    EXPECT_GT(result.trajectory_len, 0u);
    EXPECT_GE(result.confidence, 0.0f);
    EXPECT_LE(result.confidence, 1.0f);

    omni_wm_cf_result_destroy(&result);
    omni_wm_cf_query_destroy(query);
    omni_wm_state_destroy(state);
}

TEST_F(OmniWorldModelTest, CounterfactualNullModelFails)
{
    omni_wm_state_t* state = create_test_state(TEST_STATE_DIM, 1.0f);
    std::vector<float> action(TEST_ACTION_DIM, 0.5f);
    omni_wm_counterfactual_query_t* query = omni_wm_cf_query_create(
        OMNI_WM_CF_ACTION, state, action.data(), TEST_ACTION_DIM, TEST_HORIZON);

    omni_wm_counterfactual_result_t result;
    nimcp_error_t err = omni_wm_counterfactual(nullptr, query, &result);
    EXPECT_NE(err, NIMCP_SUCCESS);

    omni_wm_cf_query_destroy(query);
    omni_wm_state_destroy(state);
}

TEST_F(OmniWorldModelTest, WhatIfBasic)
{
    omni_wm_state_t* state = create_test_state(TEST_STATE_DIM, 1.0f);
    ASSERT_NE(state, nullptr);

    omni_wm_set_state(wm_, state);

    std::vector<float> action = create_random_action(TEST_ACTION_DIM);
    omni_wm_counterfactual_result_t result;
    memset(&result, 0, sizeof(result));

    nimcp_error_t err = omni_wm_what_if(wm_, action.data(), TEST_ACTION_DIM, TEST_HORIZON, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    omni_wm_cf_result_destroy(&result);
    omni_wm_state_destroy(state);
}

TEST_F(OmniWorldModelTest, WhatIfNullActionFails)
{
    omni_wm_counterfactual_result_t result;

    nimcp_error_t err = omni_wm_what_if(wm_, nullptr, TEST_ACTION_DIM, TEST_HORIZON, &result);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(OmniWorldModelTest, CfResultDestroyNullSafe)
{
    // Should not crash
    omni_wm_cf_result_destroy(nullptr);

    // Also test with empty result
    omni_wm_counterfactual_result_t result;
    memset(&result, 0, sizeof(result));
    omni_wm_cf_result_destroy(&result);  // Should not crash
}

// =============================================================================
// 8. Rollout Tests
// =============================================================================

TEST_F(OmniWorldModelTest, RolloutCreateBasic)
{
    omni_wm_rollout_t* rollout = omni_wm_rollout_create(
        TEST_HORIZON, TEST_STATE_DIM, TEST_ACTION_DIM, TEST_OBS_DIM);

    ASSERT_NE(rollout, nullptr);
    EXPECT_NE(rollout->states, nullptr);
    EXPECT_NE(rollout->actions, nullptr);
    EXPECT_NE(rollout->observations, nullptr);
    EXPECT_NE(rollout->rewards, nullptr);

    omni_wm_rollout_destroy(rollout);
}

TEST_F(OmniWorldModelTest, RolloutCreateZeroLengthFails)
{
    omni_wm_rollout_t* rollout = omni_wm_rollout_create(
        0, TEST_STATE_DIM, TEST_ACTION_DIM, TEST_OBS_DIM);
    EXPECT_EQ(rollout, nullptr);
}

TEST_F(OmniWorldModelTest, RolloutCreateExceedsMaxFails)
{
    omni_wm_rollout_t* rollout = omni_wm_rollout_create(
        OMNI_WM_MAX_HORIZON + 1, TEST_STATE_DIM, TEST_ACTION_DIM, TEST_OBS_DIM);
    EXPECT_EQ(rollout, nullptr);
}

TEST_F(OmniWorldModelTest, RolloutDestroyNullSafe)
{
    // Should not crash
    omni_wm_rollout_destroy(nullptr);
}

// Simple policy function for testing
static void test_policy(const omni_wm_state_t* state, float* action, void* user_data)
{
    (void)user_data;
    uint32_t action_dim = *(uint32_t*)user_data;
    for (uint32_t i = 0; i < action_dim && i < OMNI_WM_MAX_ACTION_DIM; i++) {
        // Simple proportional policy
        if (state && state->values && i < state->dim) {
            action[i] = state->values[i] * 0.1f;
        } else {
            action[i] = 0.0f;
        }
    }
}

TEST_F(OmniWorldModelTest, RolloutExecuteBasic)
{
    omni_wm_state_t* state = create_test_state(TEST_STATE_DIM, 1.0f);
    ASSERT_NE(state, nullptr);

    omni_wm_set_state(wm_, state);

    omni_wm_rollout_t* rollout = omni_wm_rollout_create(
        TEST_HORIZON, TEST_STATE_DIM, TEST_ACTION_DIM, TEST_OBS_DIM);
    ASSERT_NE(rollout, nullptr);

    uint32_t action_dim = TEST_ACTION_DIM;
    nimcp_error_t err = omni_wm_rollout(wm_, test_policy, TEST_HORIZON, rollout, &action_dim);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GT(rollout->length, 0u);

    omni_wm_rollout_destroy(rollout);
    omni_wm_state_destroy(state);
}

TEST_F(OmniWorldModelTest, RolloutNullModelFails)
{
    omni_wm_rollout_t* rollout = omni_wm_rollout_create(
        TEST_HORIZON, TEST_STATE_DIM, TEST_ACTION_DIM, TEST_OBS_DIM);

    uint32_t action_dim = TEST_ACTION_DIM;
    nimcp_error_t err = omni_wm_rollout(nullptr, test_policy, TEST_HORIZON, rollout, &action_dim);
    EXPECT_NE(err, NIMCP_SUCCESS);

    omni_wm_rollout_destroy(rollout);
}

TEST_F(OmniWorldModelTest, RolloutNullPolicyFails)
{
    omni_wm_rollout_t* rollout = omni_wm_rollout_create(
        TEST_HORIZON, TEST_STATE_DIM, TEST_ACTION_DIM, TEST_OBS_DIM);

    nimcp_error_t err = omni_wm_rollout(wm_, nullptr, TEST_HORIZON, rollout, nullptr);
    EXPECT_NE(err, NIMCP_SUCCESS);

    omni_wm_rollout_destroy(rollout);
}

TEST_F(OmniWorldModelTest, EvaluateEfeBasic)
{
    omni_wm_state_t* state = create_test_state(TEST_STATE_DIM, 1.0f);
    omni_wm_set_state(wm_, state);

    omni_wm_rollout_t* rollout = omni_wm_rollout_create(
        TEST_HORIZON, TEST_STATE_DIM, TEST_ACTION_DIM, TEST_OBS_DIM);
    ASSERT_NE(rollout, nullptr);

    uint32_t action_dim = TEST_ACTION_DIM;
    omni_wm_rollout(wm_, test_policy, TEST_HORIZON, rollout, &action_dim);

    std::vector<float> preferred_obs(TEST_OBS_DIM, 0.5f);
    float efe = omni_wm_evaluate_efe(wm_, rollout, preferred_obs.data(), TEST_OBS_DIM);

    // EFE should be a valid float (not NaN or inf)
    EXPECT_FALSE(std::isnan(efe));
    EXPECT_FALSE(std::isinf(efe));

    omni_wm_rollout_destroy(rollout);
    omni_wm_state_destroy(state);
}

// =============================================================================
// 9. Learning Tests
// =============================================================================

TEST_F(OmniWorldModelTest, UpdateBasic)
{
    omni_wm_state_t* state = create_test_state(TEST_STATE_DIM, 1.0f);
    omni_wm_state_t* next_state = create_test_state(TEST_STATE_DIM, 1.5f);
    ASSERT_NE(state, nullptr);
    ASSERT_NE(next_state, nullptr);

    std::vector<float> action = create_random_action(TEST_ACTION_DIM);
    float reward = 1.0f;

    nimcp_error_t result = omni_wm_update(wm_, state, action.data(), TEST_ACTION_DIM, next_state, reward);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    omni_wm_state_destroy(state);
    omni_wm_state_destroy(next_state);
}

TEST_F(OmniWorldModelTest, UpdateNullModelFails)
{
    omni_wm_state_t* state = create_test_state(TEST_STATE_DIM, 1.0f);
    omni_wm_state_t* next_state = create_test_state(TEST_STATE_DIM, 1.5f);
    std::vector<float> action(TEST_ACTION_DIM, 0.5f);

    nimcp_error_t result = omni_wm_update(nullptr, state, action.data(), TEST_ACTION_DIM, next_state, 1.0f);
    EXPECT_NE(result, NIMCP_SUCCESS);

    omni_wm_state_destroy(state);
    omni_wm_state_destroy(next_state);
}

TEST_F(OmniWorldModelTest, UpdateNullStateFails)
{
    omni_wm_state_t* next_state = create_test_state(TEST_STATE_DIM, 1.5f);
    std::vector<float> action(TEST_ACTION_DIM, 0.5f);

    nimcp_error_t result = omni_wm_update(wm_, nullptr, action.data(), TEST_ACTION_DIM, next_state, 1.0f);
    EXPECT_NE(result, NIMCP_SUCCESS);

    omni_wm_state_destroy(next_state);
}

TEST_F(OmniWorldModelTest, UpdateMultipleTimes)
{
    for (int i = 0; i < 10; i++) {
        omni_wm_state_t* state = create_test_state(TEST_STATE_DIM, (float)i);
        omni_wm_state_t* next_state = create_test_state(TEST_STATE_DIM, (float)i + 0.5f);
        std::vector<float> action = create_random_action(TEST_ACTION_DIM);

        nimcp_error_t result = omni_wm_update(wm_, state, action.data(), TEST_ACTION_DIM, next_state, (float)i * 0.1f);
        EXPECT_EQ(result, NIMCP_SUCCESS);

        omni_wm_state_destroy(state);
        omni_wm_state_destroy(next_state);
    }
}

TEST_F(OmniWorldModelTest, DreamBasic)
{
    omni_wm_config_t config;
    omni_wm_get_default_config(&config);
    config.enable_dreaming = true;
    config.dream_horizon = 10;
    config.dream_episodes = 5;

    omni_world_model_t* wm = omni_wm_create(&config);
    ASSERT_NE(wm, nullptr);

    // Add some experiences first
    for (int i = 0; i < 5; i++) {
        omni_wm_state_t* state = create_test_state(TEST_STATE_DIM, (float)i);
        omni_wm_state_t* next_state = create_test_state(TEST_STATE_DIM, (float)i + 0.5f);
        std::vector<float> action = create_random_action(TEST_ACTION_DIM);

        omni_wm_update(wm, state, action.data(), TEST_ACTION_DIM, next_state, 1.0f);

        omni_wm_state_destroy(state);
        omni_wm_state_destroy(next_state);
    }

    nimcp_error_t result = omni_wm_dream(wm, 3, 5);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    omni_wm_destroy(wm);
}

TEST_F(OmniWorldModelTest, DreamNullModelFails)
{
    nimcp_error_t result = omni_wm_dream(nullptr, 3, 5);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWorldModelTest, SetLearningRateBasic)
{
    nimcp_error_t result = omni_wm_set_learning_rate(wm_, 0.01f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(OmniWorldModelTest, SetLearningRateNullModelFails)
{
    nimcp_error_t result = omni_wm_set_learning_rate(nullptr, 0.01f);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWorldModelTest, SetLearningRateNegativeFails)
{
    nimcp_error_t result = omni_wm_set_learning_rate(wm_, -0.01f);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWorldModelTest, SetLearningRateZeroValid)
{
    // Zero learning rate might be valid for freezing the model
    nimcp_error_t result = omni_wm_set_learning_rate(wm_, 0.0f);
    // Could be success or error depending on implementation
    (void)result;
}

TEST_F(OmniWorldModelTest, SetLearningRateGreaterThanOneFails)
{
    nimcp_error_t result = omni_wm_set_learning_rate(wm_, 1.5f);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

// =============================================================================
// 10. Experience Replay Tests
// =============================================================================

TEST_F(OmniWorldModelTest, ExperienceCreateBasic)
{
    omni_wm_experience_t* exp = omni_wm_experience_create(TEST_STATE_DIM, TEST_ACTION_DIM, TEST_OBS_DIM);
    ASSERT_NE(exp, nullptr);
    EXPECT_NE(exp->state, nullptr);
    EXPECT_NE(exp->action, nullptr);
    EXPECT_NE(exp->next_state, nullptr);
    EXPECT_NE(exp->observation, nullptr);
    EXPECT_EQ(exp->action_dim, TEST_ACTION_DIM);
    EXPECT_EQ(exp->obs_dim, TEST_OBS_DIM);
    omni_wm_experience_destroy(exp);
}

TEST_F(OmniWorldModelTest, ExperienceCreateZeroDimsFails)
{
    omni_wm_experience_t* exp1 = omni_wm_experience_create(0, TEST_ACTION_DIM, TEST_OBS_DIM);
    EXPECT_EQ(exp1, nullptr);

    omni_wm_experience_t* exp2 = omni_wm_experience_create(TEST_STATE_DIM, 0, TEST_OBS_DIM);
    EXPECT_EQ(exp2, nullptr);

    omni_wm_experience_t* exp3 = omni_wm_experience_create(TEST_STATE_DIM, TEST_ACTION_DIM, 0);
    EXPECT_EQ(exp3, nullptr);
}

TEST_F(OmniWorldModelTest, ExperienceDestroyNullSafe)
{
    // Should not crash
    omni_wm_experience_destroy(nullptr);
}

TEST_F(OmniWorldModelTest, AddExperienceBasic)
{
    omni_wm_config_t config;
    omni_wm_get_default_config(&config);
    config.replay_buffer_size = 100;
    config.state_dim = TEST_STATE_DIM;
    config.action_dim = TEST_ACTION_DIM;
    config.obs_dim = TEST_OBS_DIM;

    omni_world_model_t* wm = omni_wm_create(&config);
    ASSERT_NE(wm, nullptr);

    omni_wm_experience_t* exp = omni_wm_experience_create(TEST_STATE_DIM, TEST_ACTION_DIM, TEST_OBS_DIM);
    ASSERT_NE(exp, nullptr);

    // Fill experience with test data
    exp->reward = 1.0f;
    exp->terminal = false;
    for (uint32_t i = 0; i < TEST_ACTION_DIM; i++) {
        exp->action[i] = (float)i * 0.1f;
    }

    nimcp_error_t result = omni_wm_add_experience(wm, exp);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    uint32_t size = omni_wm_get_replay_size(wm);
    EXPECT_EQ(size, 1u);

    omni_wm_experience_destroy(exp);
    omni_wm_destroy(wm);
}

TEST_F(OmniWorldModelTest, AddExperienceNullModelFails)
{
    omni_wm_experience_t* exp = omni_wm_experience_create(TEST_STATE_DIM, TEST_ACTION_DIM, TEST_OBS_DIM);

    nimcp_error_t result = omni_wm_add_experience(nullptr, exp);
    EXPECT_NE(result, NIMCP_SUCCESS);

    omni_wm_experience_destroy(exp);
}

TEST_F(OmniWorldModelTest, AddExperienceNullExpFails)
{
    nimcp_error_t result = omni_wm_add_experience(wm_, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWorldModelTest, SampleExperiencesBasic)
{
    omni_wm_config_t config;
    omni_wm_get_default_config(&config);
    config.replay_buffer_size = 100;
    config.state_dim = TEST_STATE_DIM;
    config.action_dim = TEST_ACTION_DIM;
    config.obs_dim = TEST_OBS_DIM;

    omni_world_model_t* wm = omni_wm_create(&config);
    ASSERT_NE(wm, nullptr);

    // Add multiple experiences
    for (int i = 0; i < 10; i++) {
        omni_wm_experience_t* exp = omni_wm_experience_create(TEST_STATE_DIM, TEST_ACTION_DIM, TEST_OBS_DIM);
        exp->reward = (float)i;
        omni_wm_add_experience(wm, exp);
        omni_wm_experience_destroy(exp);
    }

    // Sample batch
    std::vector<omni_wm_experience_t*> batch(5);
    uint32_t sampled = omni_wm_sample_experiences(wm, batch.data(), 5);

    EXPECT_EQ(sampled, 5u);
    for (uint32_t i = 0; i < sampled; i++) {
        EXPECT_NE(batch[i], nullptr);
        omni_wm_experience_destroy(batch[i]);
    }

    omni_wm_destroy(wm);
}

TEST_F(OmniWorldModelTest, SampleExperiencesEmptyBuffer)
{
    std::vector<omni_wm_experience_t*> batch(5);
    uint32_t sampled = omni_wm_sample_experiences(wm_, batch.data(), 5);
    EXPECT_EQ(sampled, 0u);
}

TEST_F(OmniWorldModelTest, GetReplaySizeBasic)
{
    uint32_t size = omni_wm_get_replay_size(wm_);
    EXPECT_EQ(size, 0u);
}

TEST_F(OmniWorldModelTest, GetReplaySizeNullModelReturnsZero)
{
    uint32_t size = omni_wm_get_replay_size(nullptr);
    EXPECT_EQ(size, 0u);
}

TEST_F(OmniWorldModelTest, ClearReplayBasic)
{
    omni_wm_config_t config;
    omni_wm_get_default_config(&config);
    config.replay_buffer_size = 100;
    config.state_dim = TEST_STATE_DIM;
    config.action_dim = TEST_ACTION_DIM;
    config.obs_dim = TEST_OBS_DIM;

    omni_world_model_t* wm = omni_wm_create(&config);
    ASSERT_NE(wm, nullptr);

    // Add experiences
    for (int i = 0; i < 5; i++) {
        omni_wm_experience_t* exp = omni_wm_experience_create(TEST_STATE_DIM, TEST_ACTION_DIM, TEST_OBS_DIM);
        omni_wm_add_experience(wm, exp);
        omni_wm_experience_destroy(exp);
    }

    EXPECT_EQ(omni_wm_get_replay_size(wm), 5u);

    nimcp_error_t result = omni_wm_clear_replay(wm);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(omni_wm_get_replay_size(wm), 0u);

    omni_wm_destroy(wm);
}

TEST_F(OmniWorldModelTest, ClearReplayNullModelFails)
{
    nimcp_error_t result = omni_wm_clear_replay(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

// =============================================================================
// 11. Symlog Tests
// =============================================================================

TEST_F(OmniWorldModelTest, SymlogZero)
{
    float result = omni_wm_symlog(0.0f);
    EXPECT_FLOAT_EQ(result, 0.0f);
}

TEST_F(OmniWorldModelTest, SymlogPositive)
{
    float result = omni_wm_symlog(10.0f);
    // symlog(x) = sign(x) * ln(|x| + 1)
    float expected = std::log(11.0f);
    EXPECT_NEAR(result, expected, FLOAT_TOLERANCE);
}

TEST_F(OmniWorldModelTest, SymlogNegative)
{
    float result = omni_wm_symlog(-10.0f);
    float expected = -std::log(11.0f);
    EXPECT_NEAR(result, expected, FLOAT_TOLERANCE);
}

TEST_F(OmniWorldModelTest, SymlogPreservesSign)
{
    EXPECT_GT(omni_wm_symlog(5.0f), 0.0f);
    EXPECT_LT(omni_wm_symlog(-5.0f), 0.0f);
}

TEST_F(OmniWorldModelTest, SymlogCompresses)
{
    // Symlog should compress large values
    float small = omni_wm_symlog(1.0f);
    float large = omni_wm_symlog(1000.0f);

    // Ratio of symlog outputs should be much smaller than ratio of inputs
    float input_ratio = 1000.0f / 1.0f;
    float output_ratio = large / small;

    EXPECT_LT(output_ratio, input_ratio);
}

TEST_F(OmniWorldModelTest, SymexpZero)
{
    float result = omni_wm_symexp(0.0f);
    EXPECT_FLOAT_EQ(result, 0.0f);
}

TEST_F(OmniWorldModelTest, SymexpPositive)
{
    float input = std::log(11.0f);  // symlog(10)
    float result = omni_wm_symexp(input);
    EXPECT_NEAR(result, 10.0f, 0.001f);
}

TEST_F(OmniWorldModelTest, SymexpNegative)
{
    float input = -std::log(11.0f);  // symlog(-10)
    float result = omni_wm_symexp(input);
    EXPECT_NEAR(result, -10.0f, 0.001f);
}

TEST_F(OmniWorldModelTest, SymlogSymexpInverse)
{
    // symexp(symlog(x)) should equal x
    std::vector<float> test_values = {0.0f, 1.0f, -1.0f, 10.0f, -10.0f, 100.0f, -100.0f, 0.001f, -0.001f};

    for (float x : test_values) {
        float transformed = omni_wm_symlog(x);
        float recovered = omni_wm_symexp(transformed);
        EXPECT_NEAR(recovered, x, 0.001f) << "Failed for x=" << x;
    }
}

TEST_F(OmniWorldModelTest, SymlogArrayBasic)
{
    std::vector<float> input = {0.0f, 1.0f, -1.0f, 10.0f, -10.0f};
    std::vector<float> output(input.size());

    omni_wm_symlog_array(input.data(), output.data(), input.size());

    for (size_t i = 0; i < input.size(); i++) {
        float expected = omni_wm_symlog(input[i]);
        EXPECT_FLOAT_EQ(output[i], expected);
    }
}

TEST_F(OmniWorldModelTest, SymlogArrayNullInputSafe)
{
    std::vector<float> output(5);
    // Should not crash
    omni_wm_symlog_array(nullptr, output.data(), 5);
}

TEST_F(OmniWorldModelTest, SymlogArrayNullOutputSafe)
{
    std::vector<float> input(5, 1.0f);
    // Should not crash
    omni_wm_symlog_array(input.data(), nullptr, 5);
}

TEST_F(OmniWorldModelTest, SymlogArrayZeroSizeSafe)
{
    std::vector<float> input(5, 1.0f);
    std::vector<float> output(5);
    // Should not crash
    omni_wm_symlog_array(input.data(), output.data(), 0);
}

TEST_F(OmniWorldModelTest, SymexpArrayBasic)
{
    std::vector<float> input = {0.0f, 0.693f, -0.693f, 2.398f, -2.398f};  // approx symlog values
    std::vector<float> output(input.size());

    omni_wm_symexp_array(input.data(), output.data(), input.size());

    for (size_t i = 0; i < input.size(); i++) {
        float expected = omni_wm_symexp(input[i]);
        EXPECT_NEAR(output[i], expected, FLOAT_TOLERANCE);
    }
}

TEST_F(OmniWorldModelTest, SymlogSymexpArrayInverse)
{
    std::vector<float> original = {0.0f, 1.0f, -5.0f, 100.0f, -100.0f};
    std::vector<float> transformed(original.size());
    std::vector<float> recovered(original.size());

    omni_wm_symlog_array(original.data(), transformed.data(), original.size());
    omni_wm_symexp_array(transformed.data(), recovered.data(), original.size());

    for (size_t i = 0; i < original.size(); i++) {
        EXPECT_NEAR(recovered[i], original[i], 0.001f);
    }
}

// =============================================================================
// 12. Statistics Tests
// =============================================================================

TEST_F(OmniWorldModelTest, GetStatsBasic)
{
    omni_wm_stats_t stats;
    memset(&stats, 0xFF, sizeof(stats));  // Fill with garbage

    nimcp_error_t result = omni_wm_get_stats(wm_, &stats);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Stats should be initialized to 0 for new model
    EXPECT_EQ(stats.forward_predictions, 0u);
    EXPECT_EQ(stats.backward_inferences, 0u);
    EXPECT_EQ(stats.counterfactual_queries, 0u);
    EXPECT_EQ(stats.model_updates, 0u);
}

TEST_F(OmniWorldModelTest, GetStatsNullModelFails)
{
    omni_wm_stats_t stats;
    nimcp_error_t result = omni_wm_get_stats(nullptr, &stats);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWorldModelTest, GetStatsNullStatsFails)
{
    nimcp_error_t result = omni_wm_get_stats(wm_, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWorldModelTest, StatsUpdateAfterOperations)
{
    // Set initial state
    omni_wm_state_t* state = create_test_state(TEST_STATE_DIM, 1.0f);
    omni_wm_set_state(wm_, state);

    // Do some operations
    std::vector<float> action = create_random_action(TEST_ACTION_DIM);
    omni_wm_transition_t transition;
    memset(&transition, 0, sizeof(transition));

    omni_wm_predict_forward(wm_, action.data(), TEST_ACTION_DIM, &transition);
    if (transition.next_state) omni_wm_state_destroy(transition.next_state);

    // Check stats updated
    omni_wm_stats_t stats;
    omni_wm_get_stats(wm_, &stats);
    EXPECT_GE(stats.forward_predictions, 1u);

    omni_wm_state_destroy(state);
}

TEST_F(OmniWorldModelTest, ResetStatsBasic)
{
    // Do some operations first
    omni_wm_state_t* state = create_test_state(TEST_STATE_DIM, 1.0f);
    omni_wm_state_t* next_state = create_test_state(TEST_STATE_DIM, 1.5f);
    std::vector<float> action(TEST_ACTION_DIM, 0.5f);

    omni_wm_update(wm_, state, action.data(), TEST_ACTION_DIM, next_state, 1.0f);

    // Reset
    nimcp_error_t result = omni_wm_reset_stats(wm_);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Check stats are zeroed
    omni_wm_stats_t stats;
    omni_wm_get_stats(wm_, &stats);
    EXPECT_EQ(stats.forward_predictions, 0u);
    EXPECT_EQ(stats.model_updates, 0u);

    omni_wm_state_destroy(state);
    omni_wm_state_destroy(next_state);
}

TEST_F(OmniWorldModelTest, ResetStatsNullModelFails)
{
    nimcp_error_t result = omni_wm_reset_stats(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

// =============================================================================
// 13. Observation Prediction Tests
// =============================================================================

TEST_F(OmniWorldModelTest, PredictObservationsBasic)
{
    omni_wm_state_t* state = create_test_state(TEST_STATE_DIM, 1.0f);
    ASSERT_NE(state, nullptr);

    std::vector<float> predicted_obs(TEST_OBS_DIM);
    nimcp_error_t result = omni_wm_predict_observations(wm_, state, predicted_obs.data(), TEST_OBS_DIM);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    omni_wm_state_destroy(state);
}

TEST_F(OmniWorldModelTest, PredictObservationsNullModelFails)
{
    omni_wm_state_t* state = create_test_state(TEST_STATE_DIM, 1.0f);
    std::vector<float> predicted_obs(TEST_OBS_DIM);

    nimcp_error_t result = omni_wm_predict_observations(nullptr, state, predicted_obs.data(), TEST_OBS_DIM);
    EXPECT_NE(result, NIMCP_SUCCESS);

    omni_wm_state_destroy(state);
}

TEST_F(OmniWorldModelTest, PredictObservationsNullStateFails)
{
    std::vector<float> predicted_obs(TEST_OBS_DIM);

    nimcp_error_t result = omni_wm_predict_observations(wm_, nullptr, predicted_obs.data(), TEST_OBS_DIM);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWorldModelTest, InferStateBasic)
{
    std::vector<float> observations = create_test_observation(TEST_OBS_DIM, 1.0f);

    omni_wm_state_t inferred_state;
    inferred_state.values = (float*)malloc(TEST_STATE_DIM * sizeof(float));
    inferred_state.dim = TEST_STATE_DIM;

    nimcp_error_t result = omni_wm_infer_state(wm_, observations.data(), TEST_OBS_DIM, &inferred_state);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    free(inferred_state.values);
}

TEST_F(OmniWorldModelTest, InferStateNullModelFails)
{
    std::vector<float> observations(TEST_OBS_DIM, 1.0f);
    omni_wm_state_t inferred_state;
    inferred_state.values = (float*)malloc(TEST_STATE_DIM * sizeof(float));
    inferred_state.dim = TEST_STATE_DIM;

    nimcp_error_t result = omni_wm_infer_state(nullptr, observations.data(), TEST_OBS_DIM, &inferred_state);
    EXPECT_NE(result, NIMCP_SUCCESS);

    free(inferred_state.values);
}

TEST_F(OmniWorldModelTest, InferStateNullObservationsFails)
{
    omni_wm_state_t inferred_state;
    inferred_state.values = (float*)malloc(TEST_STATE_DIM * sizeof(float));
    inferred_state.dim = TEST_STATE_DIM;

    nimcp_error_t result = omni_wm_infer_state(wm_, nullptr, TEST_OBS_DIM, &inferred_state);
    EXPECT_NE(result, NIMCP_SUCCESS);

    free(inferred_state.values);
}

// =============================================================================
// 14. Active Inference Integration Tests
// =============================================================================

TEST_F(OmniWorldModelTest, ConnectActiveInferenceNullAI)
{
    nimcp_error_t result = omni_wm_connect_active_inference(wm_, nullptr);
    // May succeed or fail depending on implementation
    (void)result;
}

TEST_F(OmniWorldModelTest, ConnectActiveInferenceNullModelFails)
{
    nimcp_error_t result = omni_wm_connect_active_inference(nullptr, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWorldModelTest, EvaluatePolicyBasic)
{
    omni_wm_state_t* state = create_test_state(TEST_STATE_DIM, 1.0f);
    omni_wm_set_state(wm_, state);

    // Create action sequence (TEST_HORIZON * TEST_ACTION_DIM values)
    std::vector<float> policy_actions(TEST_HORIZON * TEST_ACTION_DIM);
    for (uint32_t t = 0; t < TEST_HORIZON; t++) {
        for (uint32_t i = 0; i < TEST_ACTION_DIM; i++) {
            policy_actions[t * TEST_ACTION_DIM + i] = 0.1f * (float)i;
        }
    }

    std::vector<float> preferred_obs(TEST_OBS_DIM, 0.5f);

    float efe = omni_wm_evaluate_policy(wm_, policy_actions.data(), TEST_HORIZON,
                                        preferred_obs.data(), TEST_OBS_DIM);

    // EFE should be valid
    EXPECT_FALSE(std::isnan(efe));
    EXPECT_FALSE(std::isinf(efe));

    omni_wm_state_destroy(state);
}

TEST_F(OmniWorldModelTest, EvaluatePolicyNullModelReturnsInf)
{
    std::vector<float> policy_actions(TEST_HORIZON * TEST_ACTION_DIM, 0.0f);
    std::vector<float> preferred_obs(TEST_OBS_DIM, 0.5f);

    float efe = omni_wm_evaluate_policy(nullptr, policy_actions.data(), TEST_HORIZON,
                                        preferred_obs.data(), TEST_OBS_DIM);

    // Should return infinity or very large value for invalid input
    EXPECT_TRUE(std::isinf(efe) || efe > 1e30f);
}

// =============================================================================
// 15. Bio-Async Integration Tests
// =============================================================================

TEST_F(OmniWorldModelTest, ConnectBioAsyncBasic)
{
    nimcp_error_t result = omni_wm_connect_bio_async(wm_);
    // May succeed or fail depending on bio-async availability
    (void)result;
}

TEST_F(OmniWorldModelTest, ConnectBioAsyncNullModelFails)
{
    nimcp_error_t result = omni_wm_connect_bio_async(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWorldModelTest, DisconnectBioAsyncBasic)
{
    // Connect first
    omni_wm_connect_bio_async(wm_);

    nimcp_error_t result = omni_wm_disconnect_bio_async(wm_);
    // Should succeed even if not connected
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(OmniWorldModelTest, DisconnectBioAsyncNullModelFails)
{
    nimcp_error_t result = omni_wm_disconnect_bio_async(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWorldModelTest, DisconnectBioAsyncIdempotent)
{
    // Disconnect multiple times
    omni_wm_disconnect_bio_async(wm_);
    nimcp_error_t result = omni_wm_disconnect_bio_async(wm_);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

// =============================================================================
// 16. Utility Function Tests
// =============================================================================

TEST_F(OmniWorldModelTest, DirectionToStringForward)
{
    const char* str = omni_wm_direction_to_string(OMNI_WM_DIR_FORWARD);
    ASSERT_NE(str, nullptr);
    EXPECT_NE(strlen(str), 0u);
}

TEST_F(OmniWorldModelTest, DirectionToStringBackward)
{
    const char* str = omni_wm_direction_to_string(OMNI_WM_DIR_BACKWARD);
    ASSERT_NE(str, nullptr);
    EXPECT_NE(strlen(str), 0u);
}

TEST_F(OmniWorldModelTest, DirectionToStringLateral)
{
    const char* str = omni_wm_direction_to_string(OMNI_WM_DIR_LATERAL);
    ASSERT_NE(str, nullptr);
    EXPECT_NE(strlen(str), 0u);
}

TEST_F(OmniWorldModelTest, DirectionToStringHierarchical)
{
    const char* str = omni_wm_direction_to_string(OMNI_WM_DIR_HIERARCHICAL);
    ASSERT_NE(str, nullptr);
    EXPECT_NE(strlen(str), 0u);
}

TEST_F(OmniWorldModelTest, DirectionToStringInvalidReturnsUnknown)
{
    const char* str = omni_wm_direction_to_string((omni_wm_direction_t)999);
    ASSERT_NE(str, nullptr);
    // Should return "unknown" or similar
}

TEST_F(OmniWorldModelTest, LearnModeToStringOnline)
{
    const char* str = omni_wm_learn_mode_to_string(OMNI_WM_LEARN_ONLINE);
    ASSERT_NE(str, nullptr);
    EXPECT_NE(strlen(str), 0u);
}

TEST_F(OmniWorldModelTest, LearnModeToStringBatch)
{
    const char* str = omni_wm_learn_mode_to_string(OMNI_WM_LEARN_BATCH);
    ASSERT_NE(str, nullptr);
    EXPECT_NE(strlen(str), 0u);
}

TEST_F(OmniWorldModelTest, LearnModeToStringReplay)
{
    const char* str = omni_wm_learn_mode_to_string(OMNI_WM_LEARN_REPLAY);
    ASSERT_NE(str, nullptr);
    EXPECT_NE(strlen(str), 0u);
}

TEST_F(OmniWorldModelTest, LearnModeToStringDreaming)
{
    const char* str = omni_wm_learn_mode_to_string(OMNI_WM_LEARN_DREAMING);
    ASSERT_NE(str, nullptr);
    EXPECT_NE(strlen(str), 0u);
}

TEST_F(OmniWorldModelTest, LearnModeToStringInvalidReturnsUnknown)
{
    const char* str = omni_wm_learn_mode_to_string((omni_wm_learn_mode_t)999);
    ASSERT_NE(str, nullptr);
}

TEST_F(OmniWorldModelTest, CfTypeToStringAction)
{
    const char* str = omni_wm_cf_type_to_string(OMNI_WM_CF_ACTION);
    ASSERT_NE(str, nullptr);
    EXPECT_NE(strlen(str), 0u);
}

TEST_F(OmniWorldModelTest, CfTypeToStringState)
{
    const char* str = omni_wm_cf_type_to_string(OMNI_WM_CF_STATE);
    ASSERT_NE(str, nullptr);
    EXPECT_NE(strlen(str), 0u);
}

TEST_F(OmniWorldModelTest, CfTypeToStringContext)
{
    const char* str = omni_wm_cf_type_to_string(OMNI_WM_CF_CONTEXT);
    ASSERT_NE(str, nullptr);
    EXPECT_NE(strlen(str), 0u);
}

TEST_F(OmniWorldModelTest, CfTypeToStringGoal)
{
    const char* str = omni_wm_cf_type_to_string(OMNI_WM_CF_GOAL);
    ASSERT_NE(str, nullptr);
    EXPECT_NE(strlen(str), 0u);
}

TEST_F(OmniWorldModelTest, CfTypeToStringInvalidReturnsUnknown)
{
    const char* str = omni_wm_cf_type_to_string((omni_wm_counterfactual_type_t)999);
    ASSERT_NE(str, nullptr);
}

// =============================================================================
// Edge Case Tests
// =============================================================================

TEST_F(OmniWorldModelTest, LargeHorizonRollout)
{
    omni_wm_state_t* state = create_test_state(TEST_STATE_DIM, 1.0f);
    omni_wm_set_state(wm_, state);

    omni_wm_rollout_t* rollout = omni_wm_rollout_create(
        OMNI_WM_MAX_HORIZON, TEST_STATE_DIM, TEST_ACTION_DIM, TEST_OBS_DIM);
    ASSERT_NE(rollout, nullptr);

    uint32_t action_dim = TEST_ACTION_DIM;
    nimcp_error_t result = omni_wm_rollout(wm_, test_policy, OMNI_WM_MAX_HORIZON, rollout, &action_dim);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    omni_wm_rollout_destroy(rollout);
    omni_wm_state_destroy(state);
}

TEST_F(OmniWorldModelTest, HighDimensionalState)
{
    omni_world_model_t* wm = omni_wm_create_simple(
        OMNI_WM_MAX_STATE_DIM, TEST_ACTION_DIM, TEST_OBS_DIM);
    ASSERT_NE(wm, nullptr);

    omni_wm_state_t* state = omni_wm_state_create(OMNI_WM_MAX_STATE_DIM);
    ASSERT_NE(state, nullptr);

    nimcp_error_t result = omni_wm_set_state(wm, state);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    omni_wm_state_destroy(state);
    omni_wm_destroy(wm);
}

TEST_F(OmniWorldModelTest, ReplayBufferOverflow)
{
    omni_wm_config_t config;
    omni_wm_get_default_config(&config);
    config.replay_buffer_size = 5;  // Small buffer
    config.state_dim = TEST_STATE_DIM;
    config.action_dim = TEST_ACTION_DIM;
    config.obs_dim = TEST_OBS_DIM;

    omni_world_model_t* wm = omni_wm_create(&config);
    ASSERT_NE(wm, nullptr);

    // Add more than buffer size
    for (int i = 0; i < 10; i++) {
        omni_wm_experience_t* exp = omni_wm_experience_create(TEST_STATE_DIM, TEST_ACTION_DIM, TEST_OBS_DIM);
        exp->reward = (float)i;
        omni_wm_add_experience(wm, exp);
        omni_wm_experience_destroy(exp);
    }

    // Should not exceed buffer size
    uint32_t size = omni_wm_get_replay_size(wm);
    EXPECT_LE(size, 5u);

    omni_wm_destroy(wm);
}

TEST_F(OmniWorldModelTest, SymlogExtreme)
{
    // Test extreme values
    float large_pos = omni_wm_symlog(1e10f);
    float large_neg = omni_wm_symlog(-1e10f);

    EXPECT_FALSE(std::isnan(large_pos));
    EXPECT_FALSE(std::isnan(large_neg));
    EXPECT_FALSE(std::isinf(large_pos));
    EXPECT_FALSE(std::isinf(large_neg));

    // symlog should compress extreme values
    EXPECT_LT(large_pos, 1e10f);
    EXPECT_GT(large_neg, -1e10f);
}

TEST_F(OmniWorldModelTest, MultiplePredictionTypes)
{
    omni_wm_config_t config;
    omni_wm_get_default_config(&config);

    // Test each prediction type
    omni_wm_prediction_type_t types[] = {
        OMNI_WM_PRED_DETERMINISTIC,
        OMNI_WM_PRED_STOCHASTIC,
        OMNI_WM_PRED_MIXTURE
    };

    for (auto pred_type : types) {
        config.pred_type = pred_type;
        config.use_mdn = (pred_type == OMNI_WM_PRED_MIXTURE);

        omni_world_model_t* wm = omni_wm_create(&config);
        EXPECT_NE(wm, nullptr) << "Failed for prediction type: " << (int)pred_type;
        if (wm) omni_wm_destroy(wm);
    }
}

TEST_F(OmniWorldModelTest, ConcurrentStateAccess)
{
    // Create multiple states
    std::vector<omni_wm_state_t*> states;
    for (int i = 0; i < 10; i++) {
        states.push_back(create_test_state(TEST_STATE_DIM, (float)i));
    }

    // Set and get states repeatedly
    for (int i = 0; i < 10; i++) {
        omni_wm_set_state(wm_, states[i]);
        const omni_wm_state_t* retrieved = omni_wm_get_state(wm_);
        EXPECT_NE(retrieved, nullptr);
    }

    // Clean up
    for (auto state : states) {
        omni_wm_state_destroy(state);
    }
}

// =============================================================================
// Main
// =============================================================================

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
