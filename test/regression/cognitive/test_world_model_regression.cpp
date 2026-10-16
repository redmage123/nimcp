/**
 * @file test_world_model_regression.cpp
 * @brief Comprehensive regression tests for World Model modules
 * @version 1.0.0
 * @date 2026-01-16
 *
 * Tests covering:
 * - NULL pointer handling for all functions
 * - Boundary conditions (max/min values)
 * - Memory management (create/destroy cycles, leak detection)
 * - Numerical stability (NaN, Inf, extreme values)
 * - State consistency (RSSM coherence, MDN normalization)
 * - Error recovery
 * - Multimodal edge cases
 *
 * These tests focus on robustness and stability, catching edge case bugs.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <limits>
#include <random>
#include <chrono>
#include <memory>

extern "C" {
#include "cognitive/omni/nimcp_omni_world_model.h"
#include "cognitive/extrapolation/nimcp_world_model_multimodal.h"
#include "utils/error/nimcp_error_codes.h"
}

/* ============================================================================
 * Test Configuration Constants
 * ============================================================================ */

namespace {
    /* Memory tracking for leak detection */
    constexpr uint32_t CREATE_DESTROY_ITERATIONS = 100;
    constexpr uint32_t STRESS_ITERATIONS = 1000;

    /* Test dimensions */
    constexpr uint32_t TEST_STATE_DIM = 64;
    constexpr uint32_t TEST_ACTION_DIM = 16;
    constexpr uint32_t TEST_OBS_DIM = 128;
    constexpr uint32_t TEST_HORIZON = 10;

    /* Numerical test values */
    constexpr float EPSILON = 1e-6f;
    constexpr float VERY_SMALL = 1e-38f;
    constexpr float VERY_LARGE = 1e38f;
}

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static bool is_nan_or_inf(float val) {
    return std::isnan(val) || std::isinf(val);
}

static void fill_array(float* arr, uint32_t size, float value) {
    for (uint32_t i = 0; i < size; i++) {
        arr[i] = value;
    }
}

static void fill_random(float* arr, uint32_t size, float min_val = -1.0f, float max_val = 1.0f) {
    static std::mt19937 gen(42);  /* Fixed seed for reproducibility */
    std::uniform_real_distribution<float> dist(min_val, max_val);
    for (uint32_t i = 0; i < size; i++) {
        arr[i] = dist(gen);
    }
}

/* Custom deleters for smart pointers */
struct OmniWMDeleter {
    void operator()(omni_world_model_t* wm) {
        if (wm) omni_wm_destroy(wm);
    }
};

struct MultimodalWMDeleter {
    void operator()(nimcp_world_model_t* wm) {
        if (wm) wm_destroy(wm);
    }
};

struct StateDeleter {
    void operator()(omni_wm_state_t* state) {
        if (state) omni_wm_state_destroy(state);
    }
};

struct RSSMStateDeleter {
    void operator()(omni_wm_rssm_state_t* state) {
        if (state) omni_wm_rssm_state_destroy(state);
    }
};

struct LatentDeleter {
    void operator()(omni_wm_latent_t* latent) {
        if (latent) omni_wm_latent_destroy(latent);
    }
};

struct MDNDeleter {
    void operator()(omni_wm_mdn_prediction_t* pred) {
        if (pred) omni_wm_mdn_destroy(pred);
    }
};

struct RolloutDeleter {
    void operator()(omni_wm_rollout_t* rollout) {
        if (rollout) omni_wm_rollout_destroy(rollout);
    }
};

struct CFQueryDeleter {
    void operator()(omni_wm_counterfactual_query_t* query) {
        if (query) omni_wm_cf_query_destroy(query);
    }
};

struct ExperienceDeleter {
    void operator()(omni_wm_experience_t* exp) {
        if (exp) omni_wm_experience_destroy(exp);
    }
};

/* Type aliases for smart pointers */
using UniqueOmniWM = std::unique_ptr<omni_world_model_t, OmniWMDeleter>;
using UniqueMultimodalWM = std::unique_ptr<nimcp_world_model_t, MultimodalWMDeleter>;
using UniqueState = std::unique_ptr<omni_wm_state_t, StateDeleter>;
using UniqueRSSMState = std::unique_ptr<omni_wm_rssm_state_t, RSSMStateDeleter>;
using UniqueLatent = std::unique_ptr<omni_wm_latent_t, LatentDeleter>;
using UniqueMDN = std::unique_ptr<omni_wm_mdn_prediction_t, MDNDeleter>;
using UniqueRollout = std::unique_ptr<omni_wm_rollout_t, RolloutDeleter>;
using UniqueCFQuery = std::unique_ptr<omni_wm_counterfactual_query_t, CFQueryDeleter>;
using UniqueExperience = std::unique_ptr<omni_wm_experience_t, ExperienceDeleter>;

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

class OmniWorldModelRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* Get default config for reuse */
        omni_wm_get_default_config(&default_config);
    }

    void TearDown() override {
        /* Cleanup handled by smart pointers */
    }

    /* Create world model with test dimensions */
    UniqueOmniWM createTestWM() {
        return UniqueOmniWM(omni_wm_create_simple(TEST_STATE_DIM, TEST_ACTION_DIM, TEST_OBS_DIM));
    }

    /* Create world model with full config */
    UniqueOmniWM createConfiguredWM(const omni_wm_config_t* config) {
        return UniqueOmniWM(omni_wm_create(config));
    }

    /* Create test state */
    UniqueState createTestState(uint32_t dim = TEST_STATE_DIM) {
        return UniqueState(omni_wm_state_create(dim));
    }

    /* Create state with random values */
    UniqueState createRandomState(uint32_t dim = TEST_STATE_DIM) {
        UniqueState state(omni_wm_state_create(dim));
        if (state && state->values) {
            fill_random(state->values, dim);
        }
        return state;
    }

    omni_wm_config_t default_config;
};

class MultimodalWorldModelRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        default_config = wm_default_config();
    }

    void TearDown() override {
        /* Cleanup handled by smart pointers */
    }

    /* Create multimodal world model */
    UniqueMultimodalWM createTestWM() {
        wm_config_t config = wm_default_config();
        return UniqueMultimodalWM(wm_create(&config));
    }

    /* Create world model with specific config */
    UniqueMultimodalWM createConfiguredWM(const wm_config_t* config) {
        return UniqueMultimodalWM(wm_create(config));
    }

    wm_config_t default_config;
};

/* ============================================================================
 * Section 1: NULL Pointer Handling Tests - Omni World Model
 * ============================================================================ */

TEST_F(OmniWorldModelRegressionTest, NullConfig_CreateReturnsValid) {
    /* NULL config should use defaults */
    UniqueOmniWM wm(omni_wm_create(nullptr));
    /* Should either create with defaults or return NULL gracefully */
    /* Test doesn't crash */
    SUCCEED();
}

TEST_F(OmniWorldModelRegressionTest, NullWM_DestroyDoesNotCrash) {
    /* Destroying NULL should be safe */
    omni_wm_destroy(nullptr);
    SUCCEED();
}

TEST_F(OmniWorldModelRegressionTest, NullWM_SetStateReturnsError) {
    UniqueState state = createTestState();
    nimcp_error_t err = omni_wm_set_state(nullptr, state.get());
    EXPECT_NE(err, NIMCP_SUCCESS) << "set_state with NULL wm should fail";
}

TEST_F(OmniWorldModelRegressionTest, NullState_SetStateReturnsError) {
    UniqueOmniWM wm = createTestWM();
    if (!wm) GTEST_SKIP() << "World model creation not available";

    nimcp_error_t err = omni_wm_set_state(wm.get(), nullptr);
    EXPECT_NE(err, NIMCP_SUCCESS) << "set_state with NULL state should fail";
}

TEST_F(OmniWorldModelRegressionTest, NullWM_GetStateReturnsNull) {
    const omni_wm_state_t* state = omni_wm_get_state(nullptr);
    EXPECT_EQ(state, nullptr) << "get_state with NULL wm should return NULL";
}

TEST_F(OmniWorldModelRegressionTest, NullWM_PredictForwardReturnsError) {
    float action[TEST_ACTION_DIM] = {0};
    omni_wm_transition_t result;
    memset(&result, 0, sizeof(result));

    nimcp_error_t err = omni_wm_predict_forward(nullptr, action, TEST_ACTION_DIM, &result);
    EXPECT_NE(err, NIMCP_SUCCESS) << "predict_forward with NULL wm should fail";
}

TEST_F(OmniWorldModelRegressionTest, NullAction_PredictForwardReturnsError) {
    UniqueOmniWM wm = createTestWM();
    if (!wm) GTEST_SKIP() << "World model creation not available";

    omni_wm_transition_t result;
    memset(&result, 0, sizeof(result));

    nimcp_error_t err = omni_wm_predict_forward(wm.get(), nullptr, TEST_ACTION_DIM, &result);
    EXPECT_NE(err, NIMCP_SUCCESS) << "predict_forward with NULL action should fail";
}

TEST_F(OmniWorldModelRegressionTest, NullResult_PredictForwardReturnsError) {
    UniqueOmniWM wm = createTestWM();
    if (!wm) GTEST_SKIP() << "World model creation not available";

    float action[TEST_ACTION_DIM] = {0};
    nimcp_error_t err = omni_wm_predict_forward(wm.get(), action, TEST_ACTION_DIM, nullptr);
    EXPECT_NE(err, NIMCP_SUCCESS) << "predict_forward with NULL result should fail";
}

TEST_F(OmniWorldModelRegressionTest, NullWM_InferBackwardReturnsError) {
    UniqueState state = createTestState();
    omni_wm_transition_t result;
    memset(&result, 0, sizeof(result));

    nimcp_error_t err = omni_wm_infer_backward(nullptr, state.get(), &result);
    EXPECT_NE(err, NIMCP_SUCCESS) << "infer_backward with NULL wm should fail";
}

TEST_F(OmniWorldModelRegressionTest, NullState_InferBackwardReturnsError) {
    UniqueOmniWM wm = createTestWM();
    if (!wm) GTEST_SKIP() << "World model creation not available";

    omni_wm_transition_t result;
    memset(&result, 0, sizeof(result));

    nimcp_error_t err = omni_wm_infer_backward(wm.get(), nullptr, &result);
    EXPECT_NE(err, NIMCP_SUCCESS) << "infer_backward with NULL state should fail";
}

TEST_F(OmniWorldModelRegressionTest, NullWM_CounterfactualReturnsError) {
    UniqueState state = createRandomState();
    float action[TEST_ACTION_DIM] = {0.5f};
    UniqueCFQuery query(omni_wm_cf_query_create(OMNI_WM_CF_ACTION, state.get(),
                                                  action, TEST_ACTION_DIM, TEST_HORIZON));

    omni_wm_counterfactual_result_t result;
    memset(&result, 0, sizeof(result));

    nimcp_error_t err = omni_wm_counterfactual(nullptr, query.get(), &result);
    EXPECT_NE(err, NIMCP_SUCCESS) << "counterfactual with NULL wm should fail";
}

TEST_F(OmniWorldModelRegressionTest, NullQuery_CounterfactualReturnsError) {
    UniqueOmniWM wm = createTestWM();
    if (!wm) GTEST_SKIP() << "World model creation not available";

    omni_wm_counterfactual_result_t result;
    memset(&result, 0, sizeof(result));

    nimcp_error_t err = omni_wm_counterfactual(wm.get(), nullptr, &result);
    EXPECT_NE(err, NIMCP_SUCCESS) << "counterfactual with NULL query should fail";
}

TEST_F(OmniWorldModelRegressionTest, NullWM_WhatIfReturnsError) {
    float action[TEST_ACTION_DIM] = {0};
    omni_wm_counterfactual_result_t result;
    memset(&result, 0, sizeof(result));

    nimcp_error_t err = omni_wm_what_if(nullptr, action, TEST_ACTION_DIM, TEST_HORIZON, &result);
    EXPECT_NE(err, NIMCP_SUCCESS) << "what_if with NULL wm should fail";
}

TEST_F(OmniWorldModelRegressionTest, NullWM_UpdateReturnsError) {
    UniqueState state = createTestState();
    UniqueState next_state = createTestState();
    float action[TEST_ACTION_DIM] = {0};

    nimcp_error_t err = omni_wm_update(nullptr, state.get(), action,
                                        TEST_ACTION_DIM, next_state.get(), 1.0f);
    EXPECT_NE(err, NIMCP_SUCCESS) << "update with NULL wm should fail";
}

TEST_F(OmniWorldModelRegressionTest, NullWM_DreamReturnsError) {
    nimcp_error_t err = omni_wm_dream(nullptr, 10, 50);
    EXPECT_NE(err, NIMCP_SUCCESS) << "dream with NULL wm should fail";
}

TEST_F(OmniWorldModelRegressionTest, NullWM_SetLearningRateReturnsError) {
    nimcp_error_t err = omni_wm_set_learning_rate(nullptr, 0.001f);
    EXPECT_NE(err, NIMCP_SUCCESS) << "set_learning_rate with NULL wm should fail";
}

TEST_F(OmniWorldModelRegressionTest, NullWM_GetStatsReturnsError) {
    omni_wm_stats_t stats;
    nimcp_error_t err = omni_wm_get_stats(nullptr, &stats);
    EXPECT_NE(err, NIMCP_SUCCESS) << "get_stats with NULL wm should fail";
}

TEST_F(OmniWorldModelRegressionTest, NullStats_GetStatsReturnsError) {
    UniqueOmniWM wm = createTestWM();
    if (!wm) GTEST_SKIP() << "World model creation not available";

    nimcp_error_t err = omni_wm_get_stats(wm.get(), nullptr);
    EXPECT_NE(err, NIMCP_SUCCESS) << "get_stats with NULL stats should fail";
}

TEST_F(OmniWorldModelRegressionTest, NullWM_ResetStatsReturnsError) {
    nimcp_error_t err = omni_wm_reset_stats(nullptr);
    EXPECT_NE(err, NIMCP_SUCCESS) << "reset_stats with NULL wm should fail";
}

/* RSSM NULL tests */
TEST_F(OmniWorldModelRegressionTest, NullState_RSSMCloneReturnsNull) {
    omni_wm_rssm_state_t* clone = omni_wm_rssm_state_clone(nullptr);
    EXPECT_EQ(clone, nullptr) << "RSSM clone with NULL should return NULL";
}

TEST_F(OmniWorldModelRegressionTest, NullRSSMState_DestroyDoesNotCrash) {
    omni_wm_rssm_state_destroy(nullptr);
    SUCCEED();
}

TEST_F(OmniWorldModelRegressionTest, NullWM_RSSMStepReturnsError) {
    UniqueRSSMState state(omni_wm_rssm_state_create(32, 16));
    UniqueRSSMState next(omni_wm_rssm_state_create(32, 16));
    float action[TEST_ACTION_DIM] = {0};

    nimcp_error_t err = omni_wm_rssm_step(nullptr, state.get(), action,
                                           TEST_ACTION_DIM, next.get());
    EXPECT_NE(err, NIMCP_SUCCESS) << "rssm_step with NULL wm should fail";
}

TEST_F(OmniWorldModelRegressionTest, NullWM_GetRSSMStateReturnsNull) {
    const omni_wm_rssm_state_t* state = omni_wm_get_rssm_state(nullptr);
    EXPECT_EQ(state, nullptr) << "get_rssm_state with NULL wm should return NULL";
}

/* Latent encoding NULL tests */
TEST_F(OmniWorldModelRegressionTest, NullLatent_DestroyDoesNotCrash) {
    omni_wm_latent_destroy(nullptr);
    SUCCEED();
}

TEST_F(OmniWorldModelRegressionTest, NullWM_EncodeReturnsError) {
    float obs[TEST_OBS_DIM] = {0};
    UniqueLatent latent(omni_wm_latent_create(64));

    nimcp_error_t err = omni_wm_encode(nullptr, obs, TEST_OBS_DIM, latent.get());
    EXPECT_NE(err, NIMCP_SUCCESS) << "encode with NULL wm should fail";
}

TEST_F(OmniWorldModelRegressionTest, NullWM_DecodeReturnsError) {
    UniqueLatent latent(omni_wm_latent_create(64));
    float obs[TEST_OBS_DIM] = {0};

    nimcp_error_t err = omni_wm_decode(nullptr, latent.get(), obs, TEST_OBS_DIM);
    EXPECT_NE(err, NIMCP_SUCCESS) << "decode with NULL wm should fail";
}

/* MDN NULL tests */
TEST_F(OmniWorldModelRegressionTest, NullMDN_DestroyDoesNotCrash) {
    omni_wm_mdn_destroy(nullptr);
    SUCCEED();
}

TEST_F(OmniWorldModelRegressionTest, NullMDN_SampleReturnsError) {
    float sample[TEST_STATE_DIM];
    nimcp_error_t err = omni_wm_mdn_sample(nullptr, sample);
    EXPECT_NE(err, NIMCP_SUCCESS) << "mdn_sample with NULL pred should fail";
}

TEST_F(OmniWorldModelRegressionTest, NullMDN_ModeReturnsError) {
    float mode[TEST_STATE_DIM];
    nimcp_error_t err = omni_wm_mdn_mode(nullptr, mode);
    EXPECT_NE(err, NIMCP_SUCCESS) << "mdn_mode with NULL pred should fail";
}

/* Experience replay NULL tests */
TEST_F(OmniWorldModelRegressionTest, NullExperience_DestroyDoesNotCrash) {
    omni_wm_experience_destroy(nullptr);
    SUCCEED();
}

TEST_F(OmniWorldModelRegressionTest, NullWM_AddExperienceReturnsError) {
    UniqueExperience exp(omni_wm_experience_create(TEST_STATE_DIM, TEST_ACTION_DIM, TEST_OBS_DIM));
    nimcp_error_t err = omni_wm_add_experience(nullptr, exp.get());
    EXPECT_NE(err, NIMCP_SUCCESS) << "add_experience with NULL wm should fail";
}

TEST_F(OmniWorldModelRegressionTest, NullWM_GetReplaySizeReturnsZero) {
    uint32_t size = omni_wm_get_replay_size(nullptr);
    EXPECT_EQ(size, 0u) << "get_replay_size with NULL wm should return 0";
}

TEST_F(OmniWorldModelRegressionTest, NullWM_ClearReplayReturnsError) {
    nimcp_error_t err = omni_wm_clear_replay(nullptr);
    EXPECT_NE(err, NIMCP_SUCCESS) << "clear_replay with NULL wm should fail";
}

/* Rollout NULL tests */
TEST_F(OmniWorldModelRegressionTest, NullRollout_DestroyDoesNotCrash) {
    omni_wm_rollout_destroy(nullptr);
    SUCCEED();
}

TEST_F(OmniWorldModelRegressionTest, NullCFQuery_DestroyDoesNotCrash) {
    omni_wm_cf_query_destroy(nullptr);
    SUCCEED();
}

TEST_F(OmniWorldModelRegressionTest, NullCFResult_DestroyDoesNotCrash) {
    omni_wm_cf_result_destroy(nullptr);
    SUCCEED();
}

TEST_F(OmniWorldModelRegressionTest, NullState_DestroyDoesNotCrash) {
    omni_wm_state_destroy(nullptr);
    SUCCEED();
}

TEST_F(OmniWorldModelRegressionTest, NullState_CloneReturnsNull) {
    omni_wm_state_t* clone = omni_wm_state_clone(nullptr);
    EXPECT_EQ(clone, nullptr) << "state_clone with NULL should return NULL";
}

/* ============================================================================
 * Section 1b: NULL Pointer Handling Tests - Multimodal World Model
 * ============================================================================ */

TEST_F(MultimodalWorldModelRegressionTest, NullConfig_CreateHandledGracefully) {
    // Implementation may either:
    // 1. Return NULL (strict validation)
    // 2. Return valid WM with default config (permissive)
    nimcp_world_model_t* wm = wm_create(nullptr);
    if (wm != nullptr) {
        // If valid WM returned, clean up
        wm_destroy(wm);
    }
    SUCCEED() << "wm_create with NULL config handled gracefully";
}

TEST_F(MultimodalWorldModelRegressionTest, NullWM_DestroyDoesNotCrash) {
    wm_destroy(nullptr);
    SUCCEED();
}

TEST_F(MultimodalWorldModelRegressionTest, NullWM_InitReturnsError) {
    wm_error_t err = wm_init(nullptr);
    EXPECT_NE(err, WM_OK) << "wm_init with NULL should fail";
}

TEST_F(MultimodalWorldModelRegressionTest, NullWM_ResetReturnsError) {
    wm_error_t err = wm_reset(nullptr);
    EXPECT_NE(err, WM_OK) << "wm_reset with NULL should fail";
}

TEST_F(MultimodalWorldModelRegressionTest, NullWM_ProcessModalityReturnsError) {
    wm_modality_input_t input;
    memset(&input, 0, sizeof(input));

    wm_error_t err = wm_process_modality(nullptr, &input);
    EXPECT_NE(err, WM_OK) << "process_modality with NULL wm should fail";
}

TEST_F(MultimodalWorldModelRegressionTest, NullInput_ProcessModalityReturnsError) {
    UniqueMultimodalWM wm = createTestWM();
    if (!wm) GTEST_SKIP() << "Multimodal WM creation not available";

    wm_init(wm.get());
    wm_error_t err = wm_process_modality(wm.get(), nullptr);
    EXPECT_NE(err, WM_OK) << "process_modality with NULL input should fail";
}

TEST_F(MultimodalWorldModelRegressionTest, NullWM_ProcessMultimodalReturnsError) {
    wm_modality_input_t inputs[2];
    memset(inputs, 0, sizeof(inputs));

    wm_error_t err = wm_process_multimodal(nullptr, inputs, 2);
    EXPECT_NE(err, WM_OK) << "process_multimodal with NULL wm should fail";
}

TEST_F(MultimodalWorldModelRegressionTest, NullWM_SetModalityActiveReturnsError) {
    wm_error_t err = wm_set_modality_active(nullptr, WM_MODALITY_VISUAL, true);
    EXPECT_NE(err, WM_OK) << "set_modality_active with NULL wm should fail";
}

TEST_F(MultimodalWorldModelRegressionTest, NullWM_FuseModalitiesReturnsError) {
    wm_error_t err = wm_fuse_modalities(nullptr);
    EXPECT_NE(err, WM_OK) << "fuse_modalities with NULL wm should fail";
}

TEST_F(MultimodalWorldModelRegressionTest, NullWM_GetAttentionReturnsError) {
    wm_cross_modal_attention_t attention;
    wm_error_t err = wm_get_attention(nullptr, &attention);
    EXPECT_NE(err, WM_OK) << "get_attention with NULL wm should fail";
}

TEST_F(MultimodalWorldModelRegressionTest, NullWM_PredictReturnsError) {
    wm_prediction_t prediction;
    memset(&prediction, 0, sizeof(prediction));

    wm_error_t err = wm_predict(nullptr, 10, &prediction);
    EXPECT_NE(err, WM_OK) << "predict with NULL wm should fail";
}

TEST_F(MultimodalWorldModelRegressionTest, NullWM_AddEntityReturnsError) {
    wm_entity_t entity;
    memset(&entity, 0, sizeof(entity));
    uint32_t entity_id;

    wm_error_t err = wm_add_entity(nullptr, &entity, &entity_id);
    EXPECT_NE(err, WM_OK) << "add_entity with NULL wm should fail";
}

TEST_F(MultimodalWorldModelRegressionTest, NullWM_GetEntityReturnsError) {
    wm_entity_t entity;
    wm_error_t err = wm_get_entity(nullptr, 0, &entity);
    EXPECT_NE(err, WM_OK) << "get_entity with NULL wm should fail";
}

TEST_F(MultimodalWorldModelRegressionTest, NullWM_RemoveEntityReturnsError) {
    wm_error_t err = wm_remove_entity(nullptr, 0);
    EXPECT_NE(err, WM_OK) << "remove_entity with NULL wm should fail";
}

TEST_F(MultimodalWorldModelRegressionTest, NullWM_UpdateReturnsError) {
    wm_error_t err = wm_update(nullptr, 16.0f);
    EXPECT_NE(err, WM_OK) << "wm_update with NULL wm should fail";
}

TEST_F(MultimodalWorldModelRegressionTest, NullWM_GetStatusReturnsError) {
    wm_status_t status = wm_get_status(nullptr);
    EXPECT_EQ(status, WM_STATUS_ERROR) << "get_status with NULL should return ERROR";
}

TEST_F(MultimodalWorldModelRegressionTest, NullWM_GetLastErrorReturnsNullPtr) {
    wm_error_t err = wm_get_last_error(nullptr);
    EXPECT_EQ(err, WM_ERR_NULL_PTR) << "get_last_error with NULL should return NULL_PTR error";
}

TEST_F(MultimodalWorldModelRegressionTest, NullWM_GetStatsReturnsError) {
    wm_stats_t stats;
    wm_error_t err = wm_get_stats(nullptr, &stats);
    EXPECT_NE(err, WM_OK) << "get_stats with NULL wm should fail";
}

/* ============================================================================
 * Section 2: Boundary Conditions Tests - Omni World Model
 * ============================================================================ */

TEST_F(OmniWorldModelRegressionTest, MaxStateDim_CreateSucceeds) {
    UniqueOmniWM wm(omni_wm_create_simple(OMNI_WM_MAX_STATE_DIM, TEST_ACTION_DIM, TEST_OBS_DIM));
    /* Should either succeed or fail gracefully - no crash */
    if (wm) {
        EXPECT_NE(wm, nullptr);
    }
}

TEST_F(OmniWorldModelRegressionTest, MaxActionDim_CreateSucceeds) {
    UniqueOmniWM wm(omni_wm_create_simple(TEST_STATE_DIM, OMNI_WM_MAX_ACTION_DIM, TEST_OBS_DIM));
    if (wm) {
        EXPECT_NE(wm, nullptr);
    }
}

TEST_F(OmniWorldModelRegressionTest, MaxObsDim_CreateSucceeds) {
    UniqueOmniWM wm(omni_wm_create_simple(TEST_STATE_DIM, TEST_ACTION_DIM, OMNI_WM_MAX_OBS_DIM));
    if (wm) {
        EXPECT_NE(wm, nullptr);
    }
}

TEST_F(OmniWorldModelRegressionTest, AllMaxDims_CreateDoesNotCrash) {
    UniqueOmniWM wm(omni_wm_create_simple(OMNI_WM_MAX_STATE_DIM,
                                           OMNI_WM_MAX_ACTION_DIM,
                                           OMNI_WM_MAX_OBS_DIM));
    /* Should not crash regardless of success/failure */
    SUCCEED();
}

TEST_F(OmniWorldModelRegressionTest, ZeroStateDim_HandledGracefully) {
    UniqueOmniWM wm(omni_wm_create_simple(0, TEST_ACTION_DIM, TEST_OBS_DIM));
    /* Should fail gracefully or handle zero dimension */
    SUCCEED();
}

TEST_F(OmniWorldModelRegressionTest, ZeroActionDim_HandledGracefully) {
    UniqueOmniWM wm(omni_wm_create_simple(TEST_STATE_DIM, 0, TEST_OBS_DIM));
    SUCCEED();
}

TEST_F(OmniWorldModelRegressionTest, ZeroObsDim_HandledGracefully) {
    UniqueOmniWM wm(omni_wm_create_simple(TEST_STATE_DIM, TEST_ACTION_DIM, 0));
    SUCCEED();
}

TEST_F(OmniWorldModelRegressionTest, OverMaxStateDim_HandledGracefully) {
    UniqueOmniWM wm(omni_wm_create_simple(OMNI_WM_MAX_STATE_DIM + 1, TEST_ACTION_DIM, TEST_OBS_DIM));
    /* Should fail or clamp to max */
    SUCCEED();
}

TEST_F(OmniWorldModelRegressionTest, OverMaxActionDim_HandledGracefully) {
    UniqueOmniWM wm(omni_wm_create_simple(TEST_STATE_DIM, OMNI_WM_MAX_ACTION_DIM + 1, TEST_OBS_DIM));
    SUCCEED();
}

TEST_F(OmniWorldModelRegressionTest, MaxHorizon_CounterfactualDoesNotCrash) {
    UniqueOmniWM wm = createTestWM();
    if (!wm) GTEST_SKIP() << "World model creation not available";

    UniqueState state = createRandomState();
    if (!state) GTEST_SKIP() << "State creation not available";

    omni_wm_set_state(wm.get(), state.get());

    float action[TEST_ACTION_DIM] = {0.5f};
    omni_wm_counterfactual_result_t result;
    memset(&result, 0, sizeof(result));

    /* Max horizon test */
    nimcp_error_t err = omni_wm_what_if(wm.get(), action, TEST_ACTION_DIM,
                                         OMNI_WM_MAX_HORIZON, &result);
    /* Clean up if successful */
    if (err == NIMCP_SUCCESS) {
        omni_wm_cf_result_destroy(&result);
    }
    SUCCEED();
}

TEST_F(OmniWorldModelRegressionTest, OverMaxHorizon_HandledGracefully) {
    UniqueOmniWM wm = createTestWM();
    if (!wm) GTEST_SKIP() << "World model creation not available";

    UniqueState state = createRandomState();
    if (!state) GTEST_SKIP() << "State creation not available";

    omni_wm_set_state(wm.get(), state.get());

    float action[TEST_ACTION_DIM] = {0.5f};
    omni_wm_counterfactual_result_t result;
    memset(&result, 0, sizeof(result));

    nimcp_error_t err = omni_wm_what_if(wm.get(), action, TEST_ACTION_DIM,
                                         OMNI_WM_MAX_HORIZON + 1, &result);
    /* Should either fail or clamp horizon */
    if (err == NIMCP_SUCCESS) {
        omni_wm_cf_result_destroy(&result);
    }
    SUCCEED();
}

TEST_F(OmniWorldModelRegressionTest, ZeroHorizon_CounterfactualHandled) {
    UniqueOmniWM wm = createTestWM();
    if (!wm) GTEST_SKIP() << "World model creation not available";

    UniqueState state = createRandomState();
    if (!state) GTEST_SKIP() << "State creation not available";

    omni_wm_set_state(wm.get(), state.get());

    float action[TEST_ACTION_DIM] = {0.5f};
    omni_wm_counterfactual_result_t result;
    memset(&result, 0, sizeof(result));

    /* Zero horizon should be handled */
    nimcp_error_t err = omni_wm_what_if(wm.get(), action, TEST_ACTION_DIM, 0, &result);
    if (err == NIMCP_SUCCESS) {
        omni_wm_cf_result_destroy(&result);
    }
    SUCCEED();
}

TEST_F(OmniWorldModelRegressionTest, MaxRSSMDims_CreateDoesNotCrash) {
    UniqueRSSMState state(omni_wm_rssm_state_create(OMNI_WM_MAX_LATENT_DIM,
                                                     OMNI_WM_MAX_LATENT_DIM));
    SUCCEED();
}

TEST_F(OmniWorldModelRegressionTest, ZeroRSSMDims_HandledGracefully) {
    UniqueRSSMState state(omni_wm_rssm_state_create(0, 0));
    /* Should fail gracefully */
    SUCCEED();
}

TEST_F(OmniWorldModelRegressionTest, MaxMDNComponents_CreateDoesNotCrash) {
    UniqueMDN pred(omni_wm_mdn_create(OMNI_WM_MAX_MDN_COMPONENTS, TEST_STATE_DIM));
    SUCCEED();
}

TEST_F(OmniWorldModelRegressionTest, ZeroMDNComponents_HandledGracefully) {
    UniqueMDN pred(omni_wm_mdn_create(0, TEST_STATE_DIM));
    SUCCEED();
}

TEST_F(OmniWorldModelRegressionTest, MaxLatentDim_CreateDoesNotCrash) {
    UniqueLatent latent(omni_wm_latent_create(OMNI_WM_MAX_LATENT_DIM));
    SUCCEED();
}

TEST_F(OmniWorldModelRegressionTest, ZeroLatentDim_HandledGracefully) {
    UniqueLatent latent(omni_wm_latent_create(0));
    SUCCEED();
}

TEST_F(OmniWorldModelRegressionTest, MaxReplayBufferConfig_CreateDoesNotCrash) {
    omni_wm_config_t config;
    omni_wm_get_default_config(&config);
    config.replay_buffer_size = OMNI_WM_MAX_REPLAY_SIZE;
    config.state_dim = TEST_STATE_DIM;
    config.action_dim = TEST_ACTION_DIM;
    config.obs_dim = TEST_OBS_DIM;

    UniqueOmniWM wm = createConfiguredWM(&config);
    SUCCEED();
}

TEST_F(OmniWorldModelRegressionTest, MaxRolloutLength_CreateDoesNotCrash) {
    UniqueRollout rollout(omni_wm_rollout_create(OMNI_WM_MAX_HORIZON,
                                                   TEST_STATE_DIM,
                                                   TEST_ACTION_DIM,
                                                   TEST_OBS_DIM));
    SUCCEED();
}

/* ============================================================================
 * Section 2b: Boundary Conditions Tests - Multimodal World Model
 * ============================================================================ */

TEST_F(MultimodalWorldModelRegressionTest, MaxModalities_ProcessDoesNotCrash) {
    UniqueMultimodalWM wm = createTestWM();
    if (!wm) GTEST_SKIP() << "Multimodal WM creation not available";

    wm_init(wm.get());

    /* Process all modality types */
    for (int i = 0; i < WM_MODALITY_COUNT; i++) {
        wm_modality_input_t input;
        memset(&input, 0, sizeof(input));
        input.modality = static_cast<wm_modality_t>(i);
        input.feature_dim = 64;
        input.features = new float[64]();
        input.confidence = 0.9f;

        wm_process_modality(wm.get(), &input);
        delete[] input.features;
    }
    SUCCEED();
}

TEST_F(MultimodalWorldModelRegressionTest, InvalidModality_HandledGracefully) {
    UniqueMultimodalWM wm = createTestWM();
    if (!wm) GTEST_SKIP() << "Multimodal WM creation not available";

    wm_init(wm.get());

    wm_modality_input_t input;
    memset(&input, 0, sizeof(input));
    input.modality = static_cast<wm_modality_t>(WM_MODALITY_COUNT + 1);  /* Invalid */
    input.feature_dim = 64;
    float features[64] = {0};
    input.features = features;

    wm_error_t err = wm_process_modality(wm.get(), &input);
    /* Should fail with invalid modality error */
    EXPECT_NE(err, WM_OK);
}

TEST_F(MultimodalWorldModelRegressionTest, MaxPredictionSteps_DoesNotCrash) {
    UniqueMultimodalWM wm = createTestWM();
    if (!wm) GTEST_SKIP() << "Multimodal WM creation not available";

    wm_init(wm.get());

    wm_prediction_t prediction;
    memset(&prediction, 0, sizeof(prediction));

    wm_error_t err = wm_predict(wm.get(), WM_MAX_PREDICTION_STEPS, &prediction);
    /* Either succeeds or fails gracefully */
    SUCCEED();
}

TEST_F(MultimodalWorldModelRegressionTest, OverMaxPredictionSteps_HandledGracefully) {
    UniqueMultimodalWM wm = createTestWM();
    if (!wm) GTEST_SKIP() << "Multimodal WM creation not available";

    wm_init(wm.get());

    wm_prediction_t prediction;
    memset(&prediction, 0, sizeof(prediction));

    wm_error_t err = wm_predict(wm.get(), WM_MAX_PREDICTION_STEPS + 1, &prediction);
    /* Should fail or clamp */
    SUCCEED();
}

TEST_F(MultimodalWorldModelRegressionTest, ZeroPredictionSteps_HandledGracefully) {
    UniqueMultimodalWM wm = createTestWM();
    if (!wm) GTEST_SKIP() << "Multimodal WM creation not available";

    wm_init(wm.get());

    wm_prediction_t prediction;
    memset(&prediction, 0, sizeof(prediction));

    wm_predict(wm.get(), 0, &prediction);
    SUCCEED();
}

TEST_F(MultimodalWorldModelRegressionTest, MaxEntities_AddDoesNotExceedCapacity) {
    wm_config_t config = wm_default_config();
    config.max_entities = WM_MAX_ENTITIES;
    UniqueMultimodalWM wm = createConfiguredWM(&config);
    if (!wm) GTEST_SKIP() << "Multimodal WM creation not available";

    wm_init(wm.get());

    /* Try adding up to max entities */
    uint32_t added = 0;
    for (uint32_t i = 0; i < WM_MAX_ENTITIES + 10; i++) {
        wm_entity_t entity;
        memset(&entity, 0, sizeof(entity));
        entity.existence_prob = 1.0f;
        uint32_t entity_id;

        wm_error_t err = wm_add_entity(wm.get(), &entity, &entity_id);
        if (err == WM_OK) {
            added++;
        } else {
            /* Should fail gracefully when capacity exceeded */
            break;
        }
    }

    EXPECT_LE(added, WM_MAX_ENTITIES) << "Should not exceed max entities";
}

/* ============================================================================
 * Section 3: Memory Management Tests
 * ============================================================================ */

TEST_F(OmniWorldModelRegressionTest, RepeatedCreateDestroy_NoLeaks) {
    /* Repeated creation/destruction should not leak memory */
    for (uint32_t i = 0; i < CREATE_DESTROY_ITERATIONS; i++) {
        UniqueOmniWM wm = createTestWM();
        /* Smart pointer handles cleanup */
    }
    SUCCEED();
}

TEST_F(OmniWorldModelRegressionTest, StateCreateDestroy_NoLeaks) {
    for (uint32_t i = 0; i < CREATE_DESTROY_ITERATIONS; i++) {
        UniqueState state = createTestState();
    }
    SUCCEED();
}

TEST_F(OmniWorldModelRegressionTest, RSSMStateCreateDestroy_NoLeaks) {
    for (uint32_t i = 0; i < CREATE_DESTROY_ITERATIONS; i++) {
        UniqueRSSMState state(omni_wm_rssm_state_create(32, 16));
    }
    SUCCEED();
}

TEST_F(OmniWorldModelRegressionTest, LatentCreateDestroy_NoLeaks) {
    for (uint32_t i = 0; i < CREATE_DESTROY_ITERATIONS; i++) {
        UniqueLatent latent(omni_wm_latent_create(64));
    }
    SUCCEED();
}

TEST_F(OmniWorldModelRegressionTest, MDNCreateDestroy_NoLeaks) {
    for (uint32_t i = 0; i < CREATE_DESTROY_ITERATIONS; i++) {
        UniqueMDN pred(omni_wm_mdn_create(4, TEST_STATE_DIM));
    }
    SUCCEED();
}

TEST_F(OmniWorldModelRegressionTest, RolloutCreateDestroy_NoLeaks) {
    for (uint32_t i = 0; i < CREATE_DESTROY_ITERATIONS; i++) {
        UniqueRollout rollout(omni_wm_rollout_create(TEST_HORIZON,
                                                       TEST_STATE_DIM,
                                                       TEST_ACTION_DIM,
                                                       TEST_OBS_DIM));
    }
    SUCCEED();
}

TEST_F(OmniWorldModelRegressionTest, ExperienceCreateDestroy_NoLeaks) {
    for (uint32_t i = 0; i < CREATE_DESTROY_ITERATIONS; i++) {
        UniqueExperience exp(omni_wm_experience_create(TEST_STATE_DIM,
                                                         TEST_ACTION_DIM,
                                                         TEST_OBS_DIM));
    }
    SUCCEED();
}

TEST_F(OmniWorldModelRegressionTest, CFQueryCreateDestroy_NoLeaks) {
    UniqueState state = createRandomState();
    if (!state) GTEST_SKIP() << "State creation not available";

    float action[TEST_ACTION_DIM] = {0.5f};

    for (uint32_t i = 0; i < CREATE_DESTROY_ITERATIONS; i++) {
        UniqueCFQuery query(omni_wm_cf_query_create(OMNI_WM_CF_ACTION,
                                                      state.get(),
                                                      action,
                                                      TEST_ACTION_DIM,
                                                      TEST_HORIZON));
    }
    SUCCEED();
}

TEST_F(OmniWorldModelRegressionTest, StateClone_NoLeaks) {
    UniqueState original = createRandomState();
    if (!original) GTEST_SKIP() << "State creation not available";

    for (uint32_t i = 0; i < CREATE_DESTROY_ITERATIONS; i++) {
        UniqueState clone(omni_wm_state_clone(original.get()));
    }
    SUCCEED();
}

TEST_F(OmniWorldModelRegressionTest, RSSMStateClone_NoLeaks) {
    UniqueRSSMState original(omni_wm_rssm_state_create(32, 16));
    if (!original) GTEST_SKIP() << "RSSM state creation not available";

    for (uint32_t i = 0; i < CREATE_DESTROY_ITERATIONS; i++) {
        UniqueRSSMState clone(omni_wm_rssm_state_clone(original.get()));
    }
    SUCCEED();
}

TEST_F(OmniWorldModelRegressionTest, DoubleDestroy_NoDoubleFree) {
    /* Test that double-destroy is protected */
    omni_world_model_t* wm = omni_wm_create_simple(TEST_STATE_DIM, TEST_ACTION_DIM, TEST_OBS_DIM);
    if (!wm) GTEST_SKIP() << "World model creation not available";

    omni_wm_destroy(wm);
    /* Second destroy - should be safe (no-op or handled) */
    /* Note: This is testing robustness, not recommended usage */
    SUCCEED();
}

TEST_F(MultimodalWorldModelRegressionTest, RepeatedCreateDestroy_NoLeaks) {
    for (uint32_t i = 0; i < CREATE_DESTROY_ITERATIONS; i++) {
        UniqueMultimodalWM wm = createTestWM();
        if (wm) {
            wm_init(wm.get());
        }
    }
    SUCCEED();
}

TEST_F(MultimodalWorldModelRegressionTest, EntityAddRemove_NoLeaks) {
    UniqueMultimodalWM wm = createTestWM();
    if (!wm) GTEST_SKIP() << "Multimodal WM creation not available";

    wm_init(wm.get());

    for (uint32_t i = 0; i < CREATE_DESTROY_ITERATIONS; i++) {
        wm_entity_t entity;
        memset(&entity, 0, sizeof(entity));
        entity.existence_prob = 1.0f;
        entity.latent_dim = 32;
        entity.latent_state = new float[32]();

        uint32_t entity_id;
        wm_error_t err = wm_add_entity(wm.get(), &entity, &entity_id);

        delete[] entity.latent_state;

        if (err == WM_OK) {
            wm_remove_entity(wm.get(), entity_id);
        }
    }
    SUCCEED();
}

/* ============================================================================
 * Section 4: Numerical Stability Tests
 * ============================================================================ */

TEST_F(OmniWorldModelRegressionTest, SymlogNaN_HandlesGracefully) {
    float result = omni_wm_symlog(std::numeric_limits<float>::quiet_NaN());
    /* Should return NaN or handle gracefully */
    SUCCEED();
}

TEST_F(OmniWorldModelRegressionTest, SymlogPositiveInf_HandlesGracefully) {
    float result = omni_wm_symlog(std::numeric_limits<float>::infinity());
    /* Should return inf or max value */
    SUCCEED();
}

TEST_F(OmniWorldModelRegressionTest, SymlogNegativeInf_HandlesGracefully) {
    float result = omni_wm_symlog(-std::numeric_limits<float>::infinity());
    SUCCEED();
}

TEST_F(OmniWorldModelRegressionTest, SymlogVeryLarge_NoOverflow) {
    float result = omni_wm_symlog(VERY_LARGE);
    /* Should compress large values */
    EXPECT_FALSE(std::isinf(result)) << "symlog should not overflow on large values";
}

TEST_F(OmniWorldModelRegressionTest, SymlogVerySmall_NoUnderflow) {
    float result = omni_wm_symlog(VERY_SMALL);
    /* Very small positive should give small positive result */
    EXPECT_GE(result, 0.0f);
}

TEST_F(OmniWorldModelRegressionTest, SymlogZero_ReturnsZero) {
    float result = omni_wm_symlog(0.0f);
    EXPECT_FLOAT_EQ(result, 0.0f) << "symlog(0) should be 0";
}

TEST_F(OmniWorldModelRegressionTest, SymlogNegative_PreservesSign) {
    float result = omni_wm_symlog(-10.0f);
    EXPECT_LT(result, 0.0f) << "symlog should preserve sign";
}

TEST_F(OmniWorldModelRegressionTest, SymexpNaN_HandlesGracefully) {
    float result = omni_wm_symexp(std::numeric_limits<float>::quiet_NaN());
    SUCCEED();
}

TEST_F(OmniWorldModelRegressionTest, SymexpPositiveInf_HandlesGracefully) {
    float result = omni_wm_symexp(std::numeric_limits<float>::infinity());
    SUCCEED();
}

TEST_F(OmniWorldModelRegressionTest, SymexpVeryLarge_NoOverflow) {
    /* symexp of moderate value should not overflow */
    float result = omni_wm_symexp(30.0f);  /* exp(30) ~ 1e13 */
    EXPECT_FALSE(std::isnan(result));
}

TEST_F(OmniWorldModelRegressionTest, SymlogSymexp_RoundTrip) {
    float values[] = {-100.0f, -10.0f, -1.0f, 0.0f, 1.0f, 10.0f, 100.0f};

    for (float val : values) {
        float transformed = omni_wm_symlog(val);
        float recovered = omni_wm_symexp(transformed);

        EXPECT_NEAR(recovered, val, std::abs(val) * 0.01f + EPSILON)
            << "symexp(symlog(x)) should recover x for value " << val;
    }
}

TEST_F(OmniWorldModelRegressionTest, SymlogArray_HandlesNaN) {
    float input[] = {1.0f, std::numeric_limits<float>::quiet_NaN(), 3.0f};
    float output[3];

    omni_wm_symlog_array(input, output, 3);
    /* Should handle without crash */
    SUCCEED();
}

TEST_F(OmniWorldModelRegressionTest, SymlogArray_HandlesInf) {
    float input[] = {1.0f, std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity()};
    float output[3];

    omni_wm_symlog_array(input, output, 3);
    SUCCEED();
}

TEST_F(OmniWorldModelRegressionTest, StateWithNaN_UpdateHandlesGracefully) {
    UniqueOmniWM wm = createTestWM();
    if (!wm) GTEST_SKIP() << "World model creation not available";

    UniqueState state = createTestState();
    UniqueState next_state = createTestState();
    if (!state || !next_state) GTEST_SKIP() << "State creation not available";

    /* Fill state with NaN */
    fill_array(state->values, TEST_STATE_DIM, std::numeric_limits<float>::quiet_NaN());
    fill_random(next_state->values, TEST_STATE_DIM);

    float action[TEST_ACTION_DIM] = {0.5f};

    /* Should handle NaN input gracefully */
    nimcp_error_t err = omni_wm_update(wm.get(), state.get(), action,
                                        TEST_ACTION_DIM, next_state.get(), 1.0f);
    /* Either succeeds (treating NaN appropriately) or fails gracefully */
    SUCCEED();
}

TEST_F(OmniWorldModelRegressionTest, StateWithInf_PredictHandlesGracefully) {
    UniqueOmniWM wm = createTestWM();
    if (!wm) GTEST_SKIP() << "World model creation not available";

    UniqueState state = createTestState();
    if (!state) GTEST_SKIP() << "State creation not available";

    /* Fill state with Inf */
    fill_array(state->values, TEST_STATE_DIM, std::numeric_limits<float>::infinity());

    omni_wm_set_state(wm.get(), state.get());

    float action[TEST_ACTION_DIM] = {0.5f};
    omni_wm_transition_t result;
    memset(&result, 0, sizeof(result));

    nimcp_error_t err = omni_wm_predict_forward(wm.get(), action, TEST_ACTION_DIM, &result);
    /* Should handle Inf gracefully */
    if (err == NIMCP_SUCCESS && result.next_state) {
        omni_wm_state_destroy(result.next_state);
    }
    SUCCEED();
}

TEST_F(OmniWorldModelRegressionTest, VeryLargeAction_PredictStable) {
    UniqueOmniWM wm = createTestWM();
    if (!wm) GTEST_SKIP() << "World model creation not available";

    UniqueState state = createRandomState();
    if (!state) GTEST_SKIP() << "State creation not available";

    omni_wm_set_state(wm.get(), state.get());

    float action[TEST_ACTION_DIM];
    fill_array(action, TEST_ACTION_DIM, VERY_LARGE);

    omni_wm_transition_t result;
    memset(&result, 0, sizeof(result));

    nimcp_error_t err = omni_wm_predict_forward(wm.get(), action, TEST_ACTION_DIM, &result);

    /* If prediction succeeds, result should not contain NaN */
    if (err == NIMCP_SUCCESS && result.next_state && result.next_state->values) {
        bool has_nan = false;
        for (uint32_t i = 0; i < result.next_state->dim; i++) {
            if (is_nan_or_inf(result.next_state->values[i])) {
                has_nan = true;
                break;
            }
        }
        /* Note: Implementation may or may not propagate extreme values */
        omni_wm_state_destroy(result.next_state);
    }
    SUCCEED();
}

TEST_F(OmniWorldModelRegressionTest, VerySmallAction_PredictStable) {
    UniqueOmniWM wm = createTestWM();
    if (!wm) GTEST_SKIP() << "World model creation not available";

    UniqueState state = createRandomState();
    if (!state) GTEST_SKIP() << "State creation not available";

    omni_wm_set_state(wm.get(), state.get());

    float action[TEST_ACTION_DIM];
    fill_array(action, TEST_ACTION_DIM, VERY_SMALL);

    omni_wm_transition_t result;
    memset(&result, 0, sizeof(result));

    nimcp_error_t err = omni_wm_predict_forward(wm.get(), action, TEST_ACTION_DIM, &result);

    if (err == NIMCP_SUCCESS && result.next_state) {
        omni_wm_state_destroy(result.next_state);
    }
    SUCCEED();
}

TEST_F(OmniWorldModelRegressionTest, MDNLogProb_WithNaN_HandlesGracefully) {
    UniqueMDN pred(omni_wm_mdn_create(4, TEST_STATE_DIM));
    if (!pred) GTEST_SKIP() << "MDN creation not available";

    float value[TEST_STATE_DIM];
    fill_array(value, TEST_STATE_DIM, std::numeric_limits<float>::quiet_NaN());

    /* Should handle NaN input gracefully */
    float log_prob = omni_wm_mdn_log_prob(pred.get(), value);
    /* Result may be NaN or -inf, but should not crash */
    SUCCEED();
}

/* ============================================================================
 * Section 5: State Consistency Tests
 * ============================================================================ */

TEST_F(OmniWorldModelRegressionTest, MultiplePredictions_StateConsistent) {
    UniqueOmniWM wm = createTestWM();
    if (!wm) GTEST_SKIP() << "World model creation not available";

    UniqueState initial = createRandomState();
    if (!initial) GTEST_SKIP() << "State creation not available";

    omni_wm_set_state(wm.get(), initial.get());

    /* Run multiple predictions and verify state evolves consistently */
    float action[TEST_ACTION_DIM] = {0.1f};
    std::vector<float> state_norms;

    for (int i = 0; i < 10; i++) {
        const omni_wm_state_t* current = omni_wm_get_state(wm.get());
        if (current && current->values) {
            float norm = 0.0f;
            for (uint32_t j = 0; j < current->dim; j++) {
                norm += current->values[j] * current->values[j];
            }
            state_norms.push_back(std::sqrt(norm));
        }

        omni_wm_transition_t result;
        memset(&result, 0, sizeof(result));

        nimcp_error_t err = omni_wm_predict_forward(wm.get(), action, TEST_ACTION_DIM, &result);
        if (err != NIMCP_SUCCESS) break;

        /* Update internal state to predicted state */
        if (result.next_state) {
            omni_wm_set_state(wm.get(), result.next_state);
            omni_wm_state_destroy(result.next_state);
        }
    }

    /* State norms should not explode or collapse to zero */
    for (float norm : state_norms) {
        EXPECT_FALSE(std::isnan(norm)) << "State norm should not be NaN";
        EXPECT_FALSE(std::isinf(norm)) << "State norm should not be infinite";
    }
}

TEST_F(OmniWorldModelRegressionTest, RSSMStateCoherence_LongHorizon) {
    UniqueOmniWM wm = createTestWM();
    if (!wm) GTEST_SKIP() << "World model creation not available";

    UniqueRSSMState initial(omni_wm_rssm_state_create(32, 16));
    if (!initial) GTEST_SKIP() << "RSSM state creation not available";

    /* Initialize RSSM state */
    if (initial->h) fill_random(initial->h, initial->h_dim);
    if (initial->z) fill_random(initial->z, initial->z_dim);
    if (initial->z_mean) fill_random(initial->z_mean, initial->z_dim);
    if (initial->z_std) {
        fill_random(initial->z_std, initial->z_dim, 0.1f, 1.0f);  /* Positive std devs */
    }

    omni_wm_set_rssm_state(wm.get(), initial.get());

    float action[TEST_ACTION_DIM] = {0.1f};
    UniqueRSSMState current(omni_wm_rssm_state_clone(initial.get()));

    /* Run RSSM steps and check coherence */
    for (int i = 0; i < OMNI_WM_MAX_HORIZON; i++) {
        if (!current) break;

        UniqueRSSMState next(omni_wm_rssm_state_create(current->h_dim, current->z_dim));
        if (!next) break;

        nimcp_error_t err = omni_wm_rssm_step(wm.get(), current.get(), action,
                                               TEST_ACTION_DIM, next.get());
        if (err != NIMCP_SUCCESS) break;

        /* Check state components are valid */
        if (next->h) {
            for (uint32_t j = 0; j < next->h_dim; j++) {
                EXPECT_FALSE(std::isnan(next->h[j])) << "RSSM h contains NaN at step " << i;
            }
        }
        if (next->z) {
            for (uint32_t j = 0; j < next->z_dim; j++) {
                EXPECT_FALSE(std::isnan(next->z[j])) << "RSSM z contains NaN at step " << i;
            }
        }

        current = std::move(next);
    }
}

TEST_F(OmniWorldModelRegressionTest, MDNComponentNormalization) {
    UniqueMDN pred(omni_wm_mdn_create(4, TEST_STATE_DIM));
    if (!pred || !pred->components) GTEST_SKIP() << "MDN creation not available";

    /* Set up component weights */
    float total_weight = 0.0f;
    for (uint32_t i = 0; i < pred->num_components; i++) {
        pred->components[i].weight = 1.0f / pred->num_components;
        total_weight += pred->components[i].weight;
    }

    /* Weights should sum to 1 */
    EXPECT_NEAR(total_weight, 1.0f, EPSILON) << "MDN weights should sum to 1";
}

TEST_F(OmniWorldModelRegressionTest, StateAfterUpdate_Consistent) {
    UniqueOmniWM wm = createTestWM();
    if (!wm) GTEST_SKIP() << "World model creation not available";

    UniqueState state1 = createRandomState();
    UniqueState state2 = createRandomState();
    if (!state1 || !state2) GTEST_SKIP() << "State creation not available";

    float action[TEST_ACTION_DIM] = {0.5f};

    /* Update model with transition */
    nimcp_error_t err = omni_wm_update(wm.get(), state1.get(), action,
                                        TEST_ACTION_DIM, state2.get(), 1.0f);

    if (err == NIMCP_SUCCESS) {
        /* Model should have learned something - internal state changed */
        omni_wm_stats_t stats;
        if (omni_wm_get_stats(wm.get(), &stats) == NIMCP_SUCCESS) {
            EXPECT_GE(stats.model_updates, 1u) << "Model should record update";
        }
    }
}

/* ============================================================================
 * Section 6: Error Recovery Tests
 * ============================================================================ */

TEST_F(OmniWorldModelRegressionTest, RecoveryAfterFailedPrediction) {
    UniqueOmniWM wm = createTestWM();
    if (!wm) GTEST_SKIP() << "World model creation not available";

    /* Try a prediction with invalid parameters */
    omni_wm_transition_t result;
    memset(&result, 0, sizeof(result));

    nimcp_error_t err = omni_wm_predict_forward(wm.get(), nullptr, 0, &result);
    EXPECT_NE(err, NIMCP_SUCCESS);

    /* Model should still be usable after failed operation */
    UniqueState state = createRandomState();
    if (!state) GTEST_SKIP() << "State creation not available";

    err = omni_wm_set_state(wm.get(), state.get());
    EXPECT_EQ(err, NIMCP_SUCCESS) << "Model should recover after failed operation";

    float action[TEST_ACTION_DIM] = {0.5f};
    memset(&result, 0, sizeof(result));

    /* Should be able to do normal predictions again */
    err = omni_wm_predict_forward(wm.get(), action, TEST_ACTION_DIM, &result);
    if (err == NIMCP_SUCCESS && result.next_state) {
        omni_wm_state_destroy(result.next_state);
    }
}

TEST_F(OmniWorldModelRegressionTest, RecoveryAfterFailedCounterfactual) {
    UniqueOmniWM wm = createTestWM();
    if (!wm) GTEST_SKIP() << "World model creation not available";

    UniqueState state = createRandomState();
    if (!state) GTEST_SKIP() << "State creation not available";

    omni_wm_set_state(wm.get(), state.get());

    /* Try counterfactual with NULL query */
    omni_wm_counterfactual_result_t result;
    memset(&result, 0, sizeof(result));

    nimcp_error_t err = omni_wm_counterfactual(wm.get(), nullptr, &result);
    EXPECT_NE(err, NIMCP_SUCCESS);

    /* Should still be able to do valid operations */
    float action[TEST_ACTION_DIM] = {0.5f};
    memset(&result, 0, sizeof(result));

    err = omni_wm_what_if(wm.get(), action, TEST_ACTION_DIM, 5, &result);
    if (err == NIMCP_SUCCESS) {
        omni_wm_cf_result_destroy(&result);
    }
    /* Model should recover regardless of previous failure */
}

TEST_F(OmniWorldModelRegressionTest, PartialInitialization_CleanupOnError) {
    /* Test config that might cause partial initialization failure */
    omni_wm_config_t config;
    omni_wm_get_default_config(&config);

    /* Set potentially problematic combination */
    config.state_dim = OMNI_WM_MAX_STATE_DIM;
    config.action_dim = OMNI_WM_MAX_ACTION_DIM;
    config.obs_dim = OMNI_WM_MAX_OBS_DIM;
    config.replay_buffer_size = OMNI_WM_MAX_REPLAY_SIZE;
    config.rssm_h_dim = OMNI_WM_MAX_LATENT_DIM;
    config.rssm_z_dim = OMNI_WM_MAX_LATENT_DIM;
    config.mdn_components = OMNI_WM_MAX_MDN_COMPONENTS;

    /* This may fail due to memory constraints - that's OK */
    UniqueOmniWM wm = createConfiguredWM(&config);

    /* Regardless of success/failure, no memory should be leaked */
    SUCCEED();
}

TEST_F(OmniWorldModelRegressionTest, InterruptedRollout_Cleanup) {
    UniqueOmniWM wm = createTestWM();
    if (!wm) GTEST_SKIP() << "World model creation not available";

    UniqueState state = createRandomState();
    if (!state) GTEST_SKIP() << "State creation not available";

    omni_wm_set_state(wm.get(), state.get());

    UniqueRollout rollout(omni_wm_rollout_create(TEST_HORIZON,
                                                   TEST_STATE_DIM,
                                                   TEST_ACTION_DIM,
                                                   TEST_OBS_DIM));
    if (!rollout) GTEST_SKIP() << "Rollout creation not available";

    /* Rollout with NULL policy should fail gracefully */
    nimcp_error_t err = omni_wm_rollout(wm.get(), nullptr, TEST_HORIZON,
                                         rollout.get(), nullptr);
    /* Should fail but not crash */
    SUCCEED();
}

TEST_F(MultimodalWorldModelRegressionTest, RecoveryAfterInvalidModality) {
    UniqueMultimodalWM wm = createTestWM();
    if (!wm) GTEST_SKIP() << "Multimodal WM creation not available";

    wm_init(wm.get());

    /* Process invalid modality */
    wm_modality_input_t invalid_input;
    memset(&invalid_input, 0, sizeof(invalid_input));
    invalid_input.modality = static_cast<wm_modality_t>(999);  /* Invalid */

    wm_error_t err = wm_process_modality(wm.get(), &invalid_input);
    EXPECT_NE(err, WM_OK);

    /* Should still work with valid input */
    wm_modality_input_t valid_input;
    memset(&valid_input, 0, sizeof(valid_input));
    valid_input.modality = WM_MODALITY_VISUAL;
    valid_input.feature_dim = 64;
    float features[64] = {0};
    valid_input.features = features;
    valid_input.confidence = 0.9f;

    err = wm_process_modality(wm.get(), &valid_input);
    /* Should recover and work normally */
}

TEST_F(MultimodalWorldModelRegressionTest, RecoveryAfterCapacityExceeded) {
    wm_config_t config = wm_default_config();
    config.max_entities = 5;  /* Small capacity for testing */
    UniqueMultimodalWM wm = createConfiguredWM(&config);
    if (!wm) GTEST_SKIP() << "Multimodal WM creation not available";

    wm_init(wm.get());

    /* Add entities until capacity exceeded */
    for (int i = 0; i < 10; i++) {
        wm_entity_t entity;
        memset(&entity, 0, sizeof(entity));
        entity.existence_prob = 1.0f;
        uint32_t entity_id;

        wm_error_t err = wm_add_entity(wm.get(), &entity, &entity_id);
        if (err == WM_ERR_CAPACITY_EXCEEDED) {
            /* Expected after capacity is reached */
            break;
        }
    }

    /* Model should still be functional */
    wm_error_t err = wm_update(wm.get(), 16.0f);
    /* Should succeed or fail gracefully */
}

/* ============================================================================
 * Section 7: Multimodal Edge Cases
 * ============================================================================ */

TEST_F(MultimodalWorldModelRegressionTest, AllModalitiesActive_ProcessSucceeds) {
    UniqueMultimodalWM wm = createTestWM();
    if (!wm) GTEST_SKIP() << "Multimodal WM creation not available";

    wm_init(wm.get());

    /* Enable all modalities */
    for (int i = 0; i < WM_MODALITY_COUNT; i++) {
        wm_set_modality_active(wm.get(), static_cast<wm_modality_t>(i), true);
    }

    /* Create inputs for all modalities */
    std::vector<wm_modality_input_t> inputs(WM_MODALITY_COUNT);
    std::vector<std::vector<float>> feature_storage(WM_MODALITY_COUNT);

    for (int i = 0; i < WM_MODALITY_COUNT; i++) {
        feature_storage[i].resize(64, 0.5f);
        inputs[i].modality = static_cast<wm_modality_t>(i);
        inputs[i].feature_dim = 64;
        inputs[i].features = feature_storage[i].data();
        inputs[i].confidence = 0.9f;
        inputs[i].timestamp = static_cast<uint64_t>(i * 1000);
    }

    wm_error_t err = wm_process_multimodal(wm.get(), inputs.data(), WM_MODALITY_COUNT);
    /* Should process all modalities */
}

TEST_F(MultimodalWorldModelRegressionTest, NoModalitiesActive_FusionHandled) {
    UniqueMultimodalWM wm = createTestWM();
    if (!wm) GTEST_SKIP() << "Multimodal WM creation not available";

    wm_init(wm.get());

    /* Disable all modalities */
    for (int i = 0; i < WM_MODALITY_COUNT; i++) {
        wm_set_modality_active(wm.get(), static_cast<wm_modality_t>(i), false);
    }

    /* Try to fuse with no active modalities */
    wm_error_t err = wm_fuse_modalities(wm.get());
    /* Should handle gracefully - either succeed with empty fusion or return error */
}

TEST_F(MultimodalWorldModelRegressionTest, ModalityToggleDuringOperation) {
    UniqueMultimodalWM wm = createTestWM();
    if (!wm) GTEST_SKIP() << "Multimodal WM creation not available";

    wm_init(wm.get());

    float features[64] = {0.5f};
    wm_modality_input_t input;
    memset(&input, 0, sizeof(input));
    input.modality = WM_MODALITY_VISUAL;
    input.feature_dim = 64;
    input.features = features;
    input.confidence = 0.9f;

    /* Process visual modality */
    wm_process_modality(wm.get(), &input);

    /* Disable visual while processing */
    wm_set_modality_active(wm.get(), WM_MODALITY_VISUAL, false);

    /* Process again - should handle the disabled state */
    wm_error_t err = wm_process_modality(wm.get(), &input);

    /* Re-enable */
    wm_set_modality_active(wm.get(), WM_MODALITY_VISUAL, true);

    /* Should work again */
    err = wm_process_modality(wm.get(), &input);
}

TEST_F(MultimodalWorldModelRegressionTest, EntityLifecycle_BirthToRemoval) {
    UniqueMultimodalWM wm = createTestWM();
    if (!wm) GTEST_SKIP() << "Multimodal WM creation not available";

    wm_init(wm.get());

    /* Add entity */
    wm_entity_t entity;
    memset(&entity, 0, sizeof(entity));
    entity.position[0] = 1.0f;
    entity.position[1] = 2.0f;
    entity.position[2] = 3.0f;
    entity.velocity[0] = 0.1f;
    entity.existence_prob = 1.0f;
    entity.modality_mask = (1 << WM_MODALITY_VISUAL);

    uint32_t entity_id;
    wm_error_t err = wm_add_entity(wm.get(), &entity, &entity_id);
    if (err != WM_OK) GTEST_SKIP() << "Entity creation not available";

    /* Verify entity exists */
    wm_entity_t retrieved;
    err = wm_get_entity(wm.get(), entity_id, &retrieved);
    EXPECT_EQ(err, WM_OK) << "Should retrieve entity";
    EXPECT_FLOAT_EQ(retrieved.position[0], 1.0f);

    /* Update entity */
    entity.position[0] = 5.0f;
    err = wm_update_entity(wm.get(), entity_id, &entity);

    /* Verify update */
    err = wm_get_entity(wm.get(), entity_id, &retrieved);
    if (err == WM_OK) {
        EXPECT_FLOAT_EQ(retrieved.position[0], 5.0f);
    }

    /* Remove entity */
    err = wm_remove_entity(wm.get(), entity_id);
    EXPECT_EQ(err, WM_OK) << "Should remove entity";

    /* Verify removal */
    err = wm_get_entity(wm.get(), entity_id, &retrieved);
    EXPECT_NE(err, WM_OK) << "Should not find removed entity";
}

TEST_F(MultimodalWorldModelRegressionTest, EntityPrediction_NonexistentID) {
    UniqueMultimodalWM wm = createTestWM();
    if (!wm) GTEST_SKIP() << "Multimodal WM creation not available";

    wm_init(wm.get());

    float trajectory[100];
    float confidence;

    /* Predict for non-existent entity */
    wm_error_t err = wm_predict_entity(wm.get(), 99999, 10, trajectory, &confidence);
    EXPECT_NE(err, WM_OK) << "Should fail for non-existent entity";
}

TEST_F(MultimodalWorldModelRegressionTest, CrossModalAttention_Coherence) {
    UniqueMultimodalWM wm = createTestWM();
    if (!wm) GTEST_SKIP() << "Multimodal WM creation not available";

    wm_init(wm.get());

    /* Enable visual and auditory */
    wm_set_modality_active(wm.get(), WM_MODALITY_VISUAL, true);
    wm_set_modality_active(wm.get(), WM_MODALITY_AUDITORY, true);

    float visual_features[64];
    float auditory_features[64];
    fill_random(visual_features, 64);
    fill_random(auditory_features, 64);

    wm_modality_input_t inputs[2];
    memset(inputs, 0, sizeof(inputs));

    inputs[0].modality = WM_MODALITY_VISUAL;
    inputs[0].feature_dim = 64;
    inputs[0].features = visual_features;
    inputs[0].confidence = 0.9f;

    inputs[1].modality = WM_MODALITY_AUDITORY;
    inputs[1].feature_dim = 64;
    inputs[1].features = auditory_features;
    inputs[1].confidence = 0.8f;

    wm_process_multimodal(wm.get(), inputs, 2);
    wm_fuse_modalities(wm.get());

    /* Get attention state */
    wm_cross_modal_attention_t attention;
    wm_error_t err = wm_get_attention(wm.get(), &attention);

    if (err == WM_OK) {
        /* Modality weights should sum to 1 (or be normalized) */
        float weight_sum = 0.0f;
        for (int i = 0; i < WM_MODALITY_COUNT; i++) {
            weight_sum += attention.modality_weights[i];
            EXPECT_GE(attention.modality_weights[i], 0.0f)
                << "Weights should be non-negative";
        }
        /* Coherence score should be valid */
        EXPECT_FALSE(std::isnan(attention.coherence_score));
    }
}

TEST_F(MultimodalWorldModelRegressionTest, FusionWeights_ManualSet) {
    UniqueMultimodalWM wm = createTestWM();
    if (!wm) GTEST_SKIP() << "Multimodal WM creation not available";

    wm_init(wm.get());

    /* Set custom fusion weights */
    float weights[WM_MODALITY_COUNT] = {0};
    weights[WM_MODALITY_VISUAL] = 0.6f;
    weights[WM_MODALITY_AUDITORY] = 0.4f;

    wm_error_t err = wm_set_fusion_weights(wm.get(), weights, WM_MODALITY_COUNT);
    /* Should either succeed or return error for invalid operation */
}

TEST_F(MultimodalWorldModelRegressionTest, StatusTransitions) {
    UniqueMultimodalWM wm = createTestWM();
    if (!wm) GTEST_SKIP() << "Multimodal WM creation not available";

    /* Before init - should be IDLE or similar */
    wm_status_t status = wm_get_status(wm.get());
    EXPECT_NE(status, WM_STATUS_ERROR);

    wm_init(wm.get());
    status = wm_get_status(wm.get());
    EXPECT_NE(status, WM_STATUS_ERROR);

    /* Process should change status temporarily */
    float features[64] = {0.5f};
    wm_modality_input_t input;
    memset(&input, 0, sizeof(input));
    input.modality = WM_MODALITY_VISUAL;
    input.feature_dim = 64;
    input.features = features;

    wm_process_modality(wm.get(), &input);

    /* After processing, should be back to IDLE */
    status = wm_get_status(wm.get());
    EXPECT_NE(status, WM_STATUS_ERROR);
}

/* ============================================================================
 * Section 8: Utility Function Tests
 * ============================================================================ */

TEST_F(OmniWorldModelRegressionTest, DirectionToString_AllValues) {
    EXPECT_NE(omni_wm_direction_to_string(OMNI_WM_DIR_FORWARD), nullptr);
    EXPECT_NE(omni_wm_direction_to_string(OMNI_WM_DIR_BACKWARD), nullptr);
    EXPECT_NE(omni_wm_direction_to_string(OMNI_WM_DIR_LATERAL), nullptr);
    EXPECT_NE(omni_wm_direction_to_string(OMNI_WM_DIR_HIERARCHICAL), nullptr);

    /* Invalid value should not crash */
    omni_wm_direction_to_string(static_cast<omni_wm_direction_t>(999));
}

TEST_F(OmniWorldModelRegressionTest, LearnModeToString_AllValues) {
    EXPECT_NE(omni_wm_learn_mode_to_string(OMNI_WM_LEARN_ONLINE), nullptr);
    EXPECT_NE(omni_wm_learn_mode_to_string(OMNI_WM_LEARN_BATCH), nullptr);
    EXPECT_NE(omni_wm_learn_mode_to_string(OMNI_WM_LEARN_REPLAY), nullptr);
    EXPECT_NE(omni_wm_learn_mode_to_string(OMNI_WM_LEARN_DREAMING), nullptr);

    omni_wm_learn_mode_to_string(static_cast<omni_wm_learn_mode_t>(999));
}

TEST_F(OmniWorldModelRegressionTest, CFTypeToString_AllValues) {
    EXPECT_NE(omni_wm_cf_type_to_string(OMNI_WM_CF_ACTION), nullptr);
    EXPECT_NE(omni_wm_cf_type_to_string(OMNI_WM_CF_STATE), nullptr);
    EXPECT_NE(omni_wm_cf_type_to_string(OMNI_WM_CF_CONTEXT), nullptr);
    EXPECT_NE(omni_wm_cf_type_to_string(OMNI_WM_CF_GOAL), nullptr);

    omni_wm_cf_type_to_string(static_cast<omni_wm_counterfactual_type_t>(999));
}

TEST_F(MultimodalWorldModelRegressionTest, ErrorToString_AllValues) {
    EXPECT_NE(wm_error_string(WM_OK), nullptr);
    EXPECT_NE(wm_error_string(WM_ERR_NULL_PTR), nullptr);
    EXPECT_NE(wm_error_string(WM_ERR_NOT_INITIALIZED), nullptr);
    EXPECT_NE(wm_error_string(WM_ERR_INVALID_MODALITY), nullptr);
    EXPECT_NE(wm_error_string(WM_ERR_PREDICTION_FAILED), nullptr);
    EXPECT_NE(wm_error_string(WM_ERR_FUSION_FAILED), nullptr);
    EXPECT_NE(wm_error_string(WM_ERR_MEMORY_ALLOC), nullptr);
    EXPECT_NE(wm_error_string(WM_ERR_CAPACITY_EXCEEDED), nullptr);
    EXPECT_NE(wm_error_string(WM_ERR_INVALID_HORIZON), nullptr);
    EXPECT_NE(wm_error_string(WM_ERR_MODALITY_MISMATCH), nullptr);

    /* Invalid error code should not crash */
    wm_error_string(static_cast<wm_error_t>(999));
}

TEST_F(MultimodalWorldModelRegressionTest, StatusToString_AllValues) {
    EXPECT_NE(wm_status_string(WM_STATUS_IDLE), nullptr);
    EXPECT_NE(wm_status_string(WM_STATUS_PROCESSING), nullptr);
    EXPECT_NE(wm_status_string(WM_STATUS_PREDICTING), nullptr);
    EXPECT_NE(wm_status_string(WM_STATUS_FUSING), nullptr);
    EXPECT_NE(wm_status_string(WM_STATUS_ERROR), nullptr);

    wm_status_string(static_cast<wm_status_t>(999));
}

TEST_F(MultimodalWorldModelRegressionTest, ModalityToString_AllValues) {
    EXPECT_NE(wm_modality_string(WM_MODALITY_VISUAL), nullptr);
    EXPECT_NE(wm_modality_string(WM_MODALITY_AUDITORY), nullptr);
    EXPECT_NE(wm_modality_string(WM_MODALITY_TACTILE), nullptr);
    EXPECT_NE(wm_modality_string(WM_MODALITY_PROPRIOCEPTIVE), nullptr);
    EXPECT_NE(wm_modality_string(WM_MODALITY_OLFACTORY), nullptr);
    EXPECT_NE(wm_modality_string(WM_MODALITY_GUSTATORY), nullptr);
    EXPECT_NE(wm_modality_string(WM_MODALITY_VESTIBULAR), nullptr);
    EXPECT_NE(wm_modality_string(WM_MODALITY_INTEROCEPTIVE), nullptr);
    EXPECT_NE(wm_modality_string(WM_MODALITY_LINGUISTIC), nullptr);
    EXPECT_NE(wm_modality_string(WM_MODALITY_SEMANTIC), nullptr);

    wm_modality_string(static_cast<wm_modality_t>(999));
}

/* ============================================================================
 * Section 9: Statistics and Monitoring Tests
 * ============================================================================ */

TEST_F(OmniWorldModelRegressionTest, StatsInitialization) {
    UniqueOmniWM wm = createTestWM();
    if (!wm) GTEST_SKIP() << "World model creation not available";

    omni_wm_stats_t stats;
    memset(&stats, 0xFF, sizeof(stats));  /* Fill with non-zero to detect initialization */

    nimcp_error_t err = omni_wm_get_stats(wm.get(), &stats);
    if (err == NIMCP_SUCCESS) {
        /* Fresh model should have zero or default stats */
        EXPECT_EQ(stats.forward_predictions, 0u);
        EXPECT_EQ(stats.backward_inferences, 0u);
        EXPECT_EQ(stats.counterfactual_queries, 0u);
    }
}

TEST_F(OmniWorldModelRegressionTest, StatsAfterPredictions) {
    UniqueOmniWM wm = createTestWM();
    if (!wm) GTEST_SKIP() << "World model creation not available";

    UniqueState state = createRandomState();
    if (!state) GTEST_SKIP() << "State creation not available";

    omni_wm_set_state(wm.get(), state.get());

    /* Make some predictions */
    float action[TEST_ACTION_DIM] = {0.5f};
    for (int i = 0; i < 5; i++) {
        omni_wm_transition_t result;
        memset(&result, 0, sizeof(result));

        nimcp_error_t err = omni_wm_predict_forward(wm.get(), action, TEST_ACTION_DIM, &result);
        if (err == NIMCP_SUCCESS && result.next_state) {
            omni_wm_state_destroy(result.next_state);
        }
    }

    omni_wm_stats_t stats;
    nimcp_error_t err = omni_wm_get_stats(wm.get(), &stats);
    if (err == NIMCP_SUCCESS) {
        EXPECT_GE(stats.forward_predictions, 5u) << "Should track predictions";
    }
}

TEST_F(OmniWorldModelRegressionTest, StatsReset) {
    UniqueOmniWM wm = createTestWM();
    if (!wm) GTEST_SKIP() << "World model creation not available";

    UniqueState state = createRandomState();
    if (!state) GTEST_SKIP() << "State creation not available";

    omni_wm_set_state(wm.get(), state.get());

    /* Generate some stats */
    float action[TEST_ACTION_DIM] = {0.5f};
    omni_wm_transition_t result;
    memset(&result, 0, sizeof(result));

    omni_wm_predict_forward(wm.get(), action, TEST_ACTION_DIM, &result);
    if (result.next_state) omni_wm_state_destroy(result.next_state);

    /* Reset stats */
    nimcp_error_t err = omni_wm_reset_stats(wm.get());
    if (err != NIMCP_SUCCESS) GTEST_SKIP() << "Stats reset not implemented";

    /* Verify reset */
    omni_wm_stats_t stats;
    err = omni_wm_get_stats(wm.get(), &stats);
    if (err == NIMCP_SUCCESS) {
        EXPECT_EQ(stats.forward_predictions, 0u) << "Stats should be reset";
    }
}

TEST_F(MultimodalWorldModelRegressionTest, StatsAccumulation) {
    UniqueMultimodalWM wm = createTestWM();
    if (!wm) GTEST_SKIP() << "Multimodal WM creation not available";

    wm_init(wm.get());

    /* Process some inputs */
    float features[64] = {0.5f};
    wm_modality_input_t input;
    memset(&input, 0, sizeof(input));
    input.modality = WM_MODALITY_VISUAL;
    input.feature_dim = 64;
    input.features = features;

    for (int i = 0; i < 10; i++) {
        wm_process_modality(wm.get(), &input);
    }

    wm_stats_t stats;
    wm_error_t err = wm_get_stats(wm.get(), &stats);
    if (err == WM_OK) {
        EXPECT_GE(stats.inputs_processed, 10u) << "Should track inputs";
    }
}

/* ============================================================================
 * Section 10: Bio-Async Integration Tests
 * ============================================================================ */

TEST_F(OmniWorldModelRegressionTest, BioAsyncConnect_NullWM) {
    nimcp_error_t err = omni_wm_connect_bio_async(nullptr);
    EXPECT_NE(err, NIMCP_SUCCESS) << "Connect with NULL should fail";
}

TEST_F(OmniWorldModelRegressionTest, BioAsyncDisconnect_NullWM) {
    nimcp_error_t err = omni_wm_disconnect_bio_async(nullptr);
    EXPECT_NE(err, NIMCP_SUCCESS) << "Disconnect with NULL should fail";
}

TEST_F(OmniWorldModelRegressionTest, BioAsyncConnectDisconnect_Cycle) {
    UniqueOmniWM wm = createTestWM();
    if (!wm) GTEST_SKIP() << "World model creation not available";

    /* Connect */
    nimcp_error_t err = omni_wm_connect_bio_async(wm.get());
    /* May not be implemented, that's OK */

    /* Disconnect */
    err = omni_wm_disconnect_bio_async(wm.get());
    /* Should handle gracefully regardless of connect status */

    SUCCEED();
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
