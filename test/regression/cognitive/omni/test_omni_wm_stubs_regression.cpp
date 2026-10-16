/**
 * @file test_omni_wm_stubs_regression.cpp
 * @brief Regression tests for world model stub functions
 *
 * Validates numerical stability, determinism, and boundary conditions for:
 * - omni_wm_predict_lateral()
 * - omni_wm_predict_hierarchical()
 * - omni_wm_generate_dream()
 *
 * @date 2026-02-12
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <vector>

extern "C" {
#include "cognitive/omni/nimcp_omni_world_model.h"
#include "utils/error/nimcp_error_codes.h"
}

// =============================================================================
// Constants
// =============================================================================

static constexpr uint32_t TEST_STATE_DIM  = 32;
static constexpr uint32_t TEST_ACTION_DIM = 4;
static constexpr uint32_t TEST_OBS_DIM    = 32;

// =============================================================================
// Regression Test Fixture
// =============================================================================

class OmniWMStubsRegressionTest : public ::testing::Test {
protected:
    omni_world_model_t* wm_ = nullptr;

    void SetUp() override {
        omni_wm_config_t config;
        omni_wm_get_default_config(&config);
        config.state_dim  = TEST_STATE_DIM;
        config.action_dim = TEST_ACTION_DIM;
        config.obs_dim    = TEST_OBS_DIM;
        config.rssm_h_dim = TEST_STATE_DIM / 2;
        config.rssm_z_dim = TEST_STATE_DIM / 4;
        config.latent_dim = TEST_STATE_DIM;
        config.enable_lateral      = true;
        config.enable_hierarchical = true;
        config.enable_dreaming     = true;
        config.num_levels          = 4;

        wm_ = omni_wm_create(&config);
        ASSERT_NE(wm_, nullptr);
    }

    void TearDown() override {
        if (wm_) {
            omni_wm_destroy(wm_);
            wm_ = nullptr;
        }
    }
};

// =============================================================================
// Regression: Lateral prediction output bounded in [-1, 1] for 100 random inputs
// =============================================================================

TEST_F(OmniWMStubsRegressionTest, LateralOutputBoundedForRandomInputs) {
    /*
     * The lateral dynamics core loop computes tanh(sum), which is bounded
     * to [-1, 1]. For dimensions beyond the dynamics model, a scaled copy
     * is used. We verify that the tanh-covered portion stays in [-1, 1]
     * across 100 random inputs.
     */
    unsigned int seed = 42;

    for (int trial = 0; trial < 100; trial++) {
        omni_wm_state_t* source = omni_wm_state_create(TEST_STATE_DIM);
        ASSERT_NE(source, nullptr);
        for (uint32_t i = 0; i < TEST_STATE_DIM; i++) {
            /* Random values in [-5, 5] to stress the tanh */
            source->values[i] = ((float)rand_r(&seed) / (float)RAND_MAX) * 10.0f - 5.0f;
        }
        source->uncertainty = 0.1f;

        omni_wm_state_t* result = omni_wm_state_create(TEST_STATE_DIM);
        ASSERT_NE(result, nullptr);

        uint32_t target_modality = trial % 5;  /* Cycle through modalities */
        nimcp_error_t err = omni_wm_predict_lateral(wm_, source, target_modality, result);
        EXPECT_EQ(err, NIMCP_SUCCESS);

        /* The first out_dim values are tanh-bounded. out_dim = min(result->dim, h_dim).
         * h_dim = rssm_h_dim = 16 in our config. */
        uint32_t tanh_portion = TEST_STATE_DIM / 2;  /* rssm_h_dim */
        if (tanh_portion > result->dim) tanh_portion = result->dim;

        for (uint32_t i = 0; i < tanh_portion; i++) {
            EXPECT_GE(result->values[i], -1.0f)
                << "Trial " << trial << ", index " << i
                << ": lateral output below -1.0";
            EXPECT_LE(result->values[i], 1.0f)
                << "Trial " << trial << ", index " << i
                << ": lateral output above 1.0";
        }

        omni_wm_state_destroy(source);
        omni_wm_state_destroy(result);
    }
}

// =============================================================================
// Regression: Hierarchical abstraction preserves mean value approximately
// =============================================================================

TEST_F(OmniWMStubsRegressionTest, HierarchicalAbstractionPreservesMean) {
    /*
     * Average pooling should approximately preserve the mean of the input.
     * For level_diff = 1 (no tanh applied), the pooled mean should closely
     * match the source mean.
     */
    omni_wm_state_t* state = omni_wm_state_create(TEST_STATE_DIM);
    ASSERT_NE(state, nullptr);
    state->level = 0;
    state->uncertainty = 0.1f;

    /* Create a state with known mean */
    float source_sum = 0.0f;
    for (uint32_t i = 0; i < TEST_STATE_DIM; i++) {
        state->values[i] = 2.0f + (float)i * 0.05f;
        source_sum += state->values[i];
    }
    float source_mean = source_sum / (float)TEST_STATE_DIM;

    /* Abstract to level 1 (pool_factor = 2, no tanh since level_diff < 2) */
    omni_wm_state_t* result = omni_wm_state_create(TEST_STATE_DIM);
    ASSERT_NE(result, nullptr);

    nimcp_error_t err = omni_wm_predict_hierarchical(wm_, state, 1, result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* pooled_count = 32/2 = 16 pooled values */
    uint32_t pooled_count = TEST_STATE_DIM / 2;
    float result_sum = 0.0f;
    for (uint32_t i = 0; i < pooled_count; i++) {
        result_sum += result->values[i];
    }
    float result_mean = result_sum / (float)pooled_count;

    /* Without tanh, average pooling should exactly preserve mean */
    EXPECT_NEAR(result_mean, source_mean, 1e-4f)
        << "Average pooling at level 1 should preserve mean";

    omni_wm_state_destroy(state);
    omni_wm_state_destroy(result);
}

// =============================================================================
// Regression: Hierarchical concretization values are bounded
// =============================================================================

TEST_F(OmniWMStubsRegressionTest, HierarchicalConcretizationBounded) {
    /*
     * Concretization uses linear interpolation + small noise.
     * Starting from bounded abstract values, output should remain
     * in a reasonable range.
     */
    omni_wm_state_t* abstract = omni_wm_state_create(8);
    ASSERT_NE(abstract, nullptr);
    abstract->level = 2;
    abstract->uncertainty = 0.2f;
    for (uint32_t i = 0; i < 8; i++) {
        abstract->values[i] = 0.5f * (float)i;  /* 0 to 3.5 */
    }

    omni_wm_state_t* result = omni_wm_state_create(TEST_STATE_DIM);
    ASSERT_NE(result, nullptr);

    nimcp_error_t err = omni_wm_predict_hierarchical(wm_, abstract, 0, result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Interpolation from [0, 3.5] with small noise (0.01 * level_diff = 0.02)
     * should keep values roughly in [-1, 5] range */
    for (uint32_t i = 0; i < result->dim; i++) {
        EXPECT_TRUE(std::isfinite(result->values[i]))
            << "Non-finite value at index " << i;
        EXPECT_GT(result->values[i], -2.0f)
            << "Value too low at index " << i;
        EXPECT_LT(result->values[i], 6.0f)
            << "Value too high at index " << i;
    }

    omni_wm_state_destroy(abstract);
    omni_wm_state_destroy(result);
}

// =============================================================================
// Regression: Dream generation produces finite values (no NaN/Inf) for 10 dreams
// =============================================================================

TEST_F(OmniWMStubsRegressionTest, DreamGenerationProducesFiniteValues) {
    for (int dream_idx = 0; dream_idx < 10; dream_idx++) {
        omni_wm_rollout_t* rollout = omni_wm_rollout_create(
            OMNI_WM_MAX_HORIZON, TEST_STATE_DIM, TEST_ACTION_DIM, TEST_OBS_DIM);
        ASSERT_NE(rollout, nullptr);

        float noise = 0.1f + 0.1f * (float)dream_idx;
        nimcp_error_t err = omni_wm_generate_dream(wm_, 8, noise, rollout);
        EXPECT_EQ(err, NIMCP_SUCCESS) << "Dream " << dream_idx << " failed";
        EXPECT_GT(rollout->length, 0u);

        /* Check all states for finite values */
        for (uint32_t t = 0; t < rollout->length; t++) {
            ASSERT_NE(rollout->states[t], nullptr)
                << "Dream " << dream_idx << ", state " << t << " is NULL";
            for (uint32_t i = 0; i < rollout->states[t]->dim; i++) {
                EXPECT_TRUE(std::isfinite(rollout->states[t]->values[i]))
                    << "Dream " << dream_idx << ", state " << t
                    << ", index " << i << " is not finite";
            }
            EXPECT_TRUE(std::isfinite(rollout->states[t]->uncertainty))
                << "Dream " << dream_idx << ", state " << t
                << " uncertainty is not finite";
        }

        /* Check rewards for finite values */
        for (uint32_t t = 0; t + 1 < rollout->length; t++) {
            EXPECT_TRUE(std::isfinite(rollout->rewards[t]))
                << "Dream " << dream_idx << ", reward " << t << " is not finite";
        }

        /* Check aggregate values */
        EXPECT_TRUE(std::isfinite(rollout->total_reward))
            << "Dream " << dream_idx << " total_reward is not finite";
        EXPECT_TRUE(std::isfinite(rollout->expected_free_energy))
            << "Dream " << dream_idx << " EFE is not finite";

        omni_wm_rollout_destroy(rollout);
    }
}

// =============================================================================
// Regression: Repeated lateral predictions with same input are deterministic
// =============================================================================

TEST_F(OmniWMStubsRegressionTest, LateralPredictionDeterministic) {
    /*
     * The lateral prediction uses tanh(W*x + b + phase) which is fully
     * deterministic (no random components). Repeating the same prediction
     * should produce identical output.
     */
    omni_wm_state_t* source = omni_wm_state_create(TEST_STATE_DIM);
    ASSERT_NE(source, nullptr);
    for (uint32_t i = 0; i < TEST_STATE_DIM; i++) {
        source->values[i] = 0.3f + (float)i * 0.02f;
    }
    source->uncertainty = 0.15f;

    /* First prediction */
    omni_wm_state_t* result1 = omni_wm_state_create(TEST_STATE_DIM);
    ASSERT_NE(result1, nullptr);
    nimcp_error_t err1 = omni_wm_predict_lateral(wm_, source, 1, result1);
    EXPECT_EQ(err1, NIMCP_SUCCESS);

    /* Second prediction with identical inputs */
    omni_wm_state_t* result2 = omni_wm_state_create(TEST_STATE_DIM);
    ASSERT_NE(result2, nullptr);
    nimcp_error_t err2 = omni_wm_predict_lateral(wm_, source, 1, result2);
    EXPECT_EQ(err2, NIMCP_SUCCESS);

    /* Results should be bitwise identical */
    for (uint32_t i = 0; i < TEST_STATE_DIM; i++) {
        EXPECT_FLOAT_EQ(result1->values[i], result2->values[i])
            << "Mismatch at index " << i
            << ": lateral prediction should be deterministic";
    }
    EXPECT_FLOAT_EQ(result1->uncertainty, result2->uncertainty);

    omni_wm_state_destroy(source);
    omni_wm_state_destroy(result1);
    omni_wm_state_destroy(result2);
}

// =============================================================================
// Regression: Uncertainty always increases with level distance
// =============================================================================

TEST_F(OmniWMStubsRegressionTest, UncertaintyIncreasesWithLevelDistance) {
    /*
     * The uncertainty formula is:
     *   result.uncertainty = state.uncertainty * (1 + 0.15 * log(2^level_diff + 1))
     *
     * As level_diff increases, the log term grows monotonically, so
     * uncertainty should strictly increase with level distance.
     * Test from level 0 to levels 0, 1, 2, 3.
     */
    omni_wm_state_t* base = omni_wm_state_create(TEST_STATE_DIM);
    ASSERT_NE(base, nullptr);
    base->level = 0;
    base->uncertainty = 0.2f;
    for (uint32_t i = 0; i < TEST_STATE_DIM; i++) {
        base->values[i] = 1.0f;
    }

    float prev_unc = -1.0f;
    for (uint32_t target = 0; target < 4; target++) {
        omni_wm_state_t* result = omni_wm_state_create(TEST_STATE_DIM);
        ASSERT_NE(result, nullptr);

        nimcp_error_t err = omni_wm_predict_hierarchical(wm_, base, target, result);
        EXPECT_EQ(err, NIMCP_SUCCESS);

        EXPECT_GT(result->uncertainty, prev_unc)
            << "Uncertainty at target level " << target
            << " should be greater than at level " << (target > 0 ? target - 1 : 0);
        prev_unc = result->uncertainty;

        /* Also verify the computed uncertainty matches the formula */
        uint32_t level_diff = target;  /* base->level is 0 */
        float info_factor = (float)(1u << level_diff);
        float expected_unc = base->uncertainty * (1.0f + 0.15f * std::log(info_factor + 1.0f));
        EXPECT_NEAR(result->uncertainty, expected_unc, 1e-5f)
            << "Uncertainty formula mismatch at target level " << target;

        omni_wm_state_destroy(result);
    }

    omni_wm_state_destroy(base);
}
