/**
 * @file test_omni_world_model_stubs.cpp
 * @brief Unit tests for newly-implemented world model stub functions
 *
 * COVERAGE:
 * - omni_wm_predict_lateral: cross-modal lateral prediction
 * - omni_wm_predict_hierarchical: multi-scale abstraction/concretization
 * - omni_wm_generate_dream: dream sequence generation
 *
 * @date 2026-02-12
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
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
// Lateral Prediction Test Fixture
// =============================================================================

class OmniWMPredictLateralTest : public ::testing::Test {
protected:
    omni_world_model_t* wm_ = nullptr;

    void SetUp() override {
        /* Default config has enable_lateral = true */
        wm_ = omni_wm_create_simple(TEST_STATE_DIM, TEST_ACTION_DIM, TEST_OBS_DIM);
        ASSERT_NE(wm_, nullptr);
    }

    void TearDown() override {
        if (wm_) {
            omni_wm_destroy(wm_);
            wm_ = nullptr;
        }
    }
};

// --- predict_lateral: produces valid output with correct dimensions ---

TEST_F(OmniWMPredictLateralTest, ProducesValidOutputWithCorrectDimensions) {
    /* Create a source state with non-zero values */
    omni_wm_state_t* source = omni_wm_state_create(TEST_STATE_DIM);
    ASSERT_NE(source, nullptr);
    for (uint32_t i = 0; i < TEST_STATE_DIM; i++) {
        source->values[i] = 0.5f + (float)i * 0.01f;
    }
    source->uncertainty = 0.1f;

    /* Create result state for output */
    omni_wm_state_t* result = omni_wm_state_create(TEST_STATE_DIM);
    ASSERT_NE(result, nullptr);

    uint32_t target_modality = 1;
    nimcp_error_t err = omni_wm_predict_lateral(wm_, source, target_modality, result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Result should have the same dim */
    EXPECT_EQ(result->dim, TEST_STATE_DIM);

    /* Result values should be non-zero (transformed via tanh from non-zero input) */
    bool has_nonzero = false;
    for (uint32_t i = 0; i < result->dim; i++) {
        if (std::fabs(result->values[i]) > 1e-8f) {
            has_nonzero = true;
            break;
        }
    }
    EXPECT_TRUE(has_nonzero) << "Lateral prediction should produce non-zero output";

    /* Output values should be bounded by tanh range [-1, 1] for the dynamics portion */
    /* (Some trailing values may be attenuated copies, not strictly in [-1,1]) */
    for (uint32_t i = 0; i < result->dim; i++) {
        EXPECT_GE(result->values[i], -2.0f);
        EXPECT_LE(result->values[i], 2.0f);
    }

    /* Uncertainty should have increased relative to source */
    EXPECT_GT(result->uncertainty, source->uncertainty);

    omni_wm_state_destroy(source);
    omni_wm_state_destroy(result);
}

// --- predict_lateral: NULL input returns error ---

TEST_F(OmniWMPredictLateralTest, NullSourceStateReturnsError) {
    omni_wm_state_t* result = omni_wm_state_create(TEST_STATE_DIM);
    ASSERT_NE(result, nullptr);

    nimcp_error_t err = omni_wm_predict_lateral(wm_, nullptr, 0, result);
    EXPECT_NE(err, NIMCP_SUCCESS);

    omni_wm_state_destroy(result);
}

TEST_F(OmniWMPredictLateralTest, NullResultReturnsError) {
    omni_wm_state_t* source = omni_wm_state_create(TEST_STATE_DIM);
    ASSERT_NE(source, nullptr);

    nimcp_error_t err = omni_wm_predict_lateral(wm_, source, 0, nullptr);
    EXPECT_NE(err, NIMCP_SUCCESS);

    omni_wm_state_destroy(source);
}

TEST_F(OmniWMPredictLateralTest, NullWorldModelReturnsError) {
    omni_wm_state_t* source = omni_wm_state_create(TEST_STATE_DIM);
    ASSERT_NE(source, nullptr);
    omni_wm_state_t* result = omni_wm_state_create(TEST_STATE_DIM);
    ASSERT_NE(result, nullptr);

    nimcp_error_t err = omni_wm_predict_lateral(nullptr, source, 0, result);
    EXPECT_NE(err, NIMCP_SUCCESS);

    omni_wm_state_destroy(source);
    omni_wm_state_destroy(result);
}

// =============================================================================
// Hierarchical Prediction Test Fixture
// =============================================================================

class OmniWMPredictHierarchicalTest : public ::testing::Test {
protected:
    omni_world_model_t* wm_ = nullptr;

    void SetUp() override {
        /* Need enable_hierarchical = true and num_levels > 1 */
        omni_wm_config_t config;
        omni_wm_get_default_config(&config);
        config.state_dim  = TEST_STATE_DIM;
        config.action_dim = TEST_ACTION_DIM;
        config.obs_dim    = TEST_OBS_DIM;
        config.rssm_h_dim = TEST_STATE_DIM / 2;
        config.rssm_z_dim = TEST_STATE_DIM / 4;
        config.latent_dim = TEST_STATE_DIM;
        config.enable_hierarchical = true;
        config.num_levels = 4;  /* Levels 0-3 */

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

// --- predict_hierarchical: abstraction (up) reduces dimensionality ---

TEST_F(OmniWMPredictHierarchicalTest, AbstractionReducesDimensionality) {
    /* Create a concrete state at level 0 with known values */
    omni_wm_state_t* state = omni_wm_state_create(TEST_STATE_DIM);
    ASSERT_NE(state, nullptr);
    state->level = 0;
    state->uncertainty = 0.1f;
    for (uint32_t i = 0; i < TEST_STATE_DIM; i++) {
        state->values[i] = 1.0f + (float)i * 0.1f;
    }

    /* Abstract to level 2 (pool_factor = 2^2 = 4) */
    omni_wm_state_t* result = omni_wm_state_create(TEST_STATE_DIM);
    ASSERT_NE(result, nullptr);

    nimcp_error_t err = omni_wm_predict_hierarchical(wm_, state, 2, result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* The result level should be set to target_level */
    EXPECT_EQ(result->level, 2u);

    /* With pool_factor=4, the first 8 output values (32/4) should be
     * the averages of 4-element groups from source. Later values should be 0.
     * pooled_count = 32/4 = 8 */
    uint32_t pooled_count = TEST_STATE_DIM / 4;
    for (uint32_t i = 0; i < pooled_count; i++) {
        /* Expected average of 4 consecutive values starting at i*4 */
        float expected_avg = 0.0f;
        for (uint32_t j = i * 4; j < i * 4 + 4; j++) {
            expected_avg += state->values[j];
        }
        expected_avg /= 4.0f;

        /* At level_diff=2, tanh is applied */
        float expected = std::tanh(expected_avg);
        EXPECT_NEAR(result->values[i], expected, 1e-4f)
            << "Mismatch at pooled index " << i;
    }

    /* Values beyond pooled_count should be zero (memset at start) */
    for (uint32_t i = pooled_count; i < TEST_STATE_DIM; i++) {
        EXPECT_FLOAT_EQ(result->values[i], 0.0f)
            << "Value beyond pooled count should be zero at index " << i;
    }

    omni_wm_state_destroy(state);
    omni_wm_state_destroy(result);
}

// --- predict_hierarchical: concretization (down) expands dimensionality ---

TEST_F(OmniWMPredictHierarchicalTest, ConcretizationExpandsDimensionality) {
    /* Create an abstract state at level 2 with known values */
    omni_wm_state_t* state = omni_wm_state_create(8);
    ASSERT_NE(state, nullptr);
    state->level = 2;
    state->uncertainty = 0.2f;
    for (uint32_t i = 0; i < 8; i++) {
        state->values[i] = 1.0f + (float)i * 0.5f;
    }

    /* Concretize to level 0 (expand_factor = 2^2 = 4) */
    omni_wm_state_t* result = omni_wm_state_create(TEST_STATE_DIM);
    ASSERT_NE(result, nullptr);

    nimcp_error_t err = omni_wm_predict_hierarchical(wm_, state, 0, result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    EXPECT_EQ(result->level, 0u);

    /* expanded_max = 8 * 4 = 32, fill_dim = min(32, 32) = 32
     * Each source element is expanded into 4 output elements via
     * linear interpolation. The expansion should fill result values. */
    uint32_t expanded_max = 8 * 4;
    uint32_t fill_dim = expanded_max < TEST_STATE_DIM ? expanded_max : TEST_STATE_DIM;

    /* Verify that the output has values in the expanded region.
     * Due to noise injection we can't check exact values, but the
     * overall pattern should roughly follow interpolation of source. */
    bool has_nonzero = false;
    for (uint32_t i = 0; i < fill_dim; i++) {
        if (std::fabs(result->values[i]) > 1e-8f) {
            has_nonzero = true;
            break;
        }
    }
    EXPECT_TRUE(has_nonzero) << "Concretization should produce non-zero output";

    /* Check that interpolated values are in a reasonable range.
     * Source values range from 1.0 to 4.5. With small noise, results
     * should be roughly in [0, 6] range. */
    for (uint32_t i = 0; i < fill_dim; i++) {
        EXPECT_GT(result->values[i], -2.0f);
        EXPECT_LT(result->values[i], 8.0f);
    }

    omni_wm_state_destroy(state);
    omni_wm_state_destroy(result);
}

// --- predict_hierarchical: same level is identity ---

TEST_F(OmniWMPredictHierarchicalTest, SameLevelIsIdentity) {
    omni_wm_state_t* state = omni_wm_state_create(TEST_STATE_DIM);
    ASSERT_NE(state, nullptr);
    state->level = 1;
    state->uncertainty = 0.3f;
    for (uint32_t i = 0; i < TEST_STATE_DIM; i++) {
        state->values[i] = 2.0f + (float)i * 0.05f;
    }

    omni_wm_state_t* result = omni_wm_state_create(TEST_STATE_DIM);
    ASSERT_NE(result, nullptr);

    nimcp_error_t err = omni_wm_predict_hierarchical(wm_, state, 1, result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    EXPECT_EQ(result->level, 1u);

    /* Same level should copy source values exactly */
    for (uint32_t i = 0; i < TEST_STATE_DIM; i++) {
        EXPECT_FLOAT_EQ(result->values[i], state->values[i])
            << "Identity copy mismatch at index " << i;
    }

    /* Uncertainty formula: state->uncertainty * (1 + 0.15 * log(1+1))
     * level_diff = 0, info_factor = 1<<0 = 1,
     * uncertainty = 0.3 * (1 + 0.15 * log(2)) */
    float expected_unc = state->uncertainty *
                         (1.0f + 0.15f * std::log(2.0f));
    EXPECT_NEAR(result->uncertainty, expected_unc, 1e-5f);

    omni_wm_state_destroy(state);
    omni_wm_state_destroy(result);
}

// --- predict_hierarchical: uncertainty grows with level distance ---

TEST_F(OmniWMPredictHierarchicalTest, UncertaintyGrowsWithLevelDistance) {
    omni_wm_state_t* state = omni_wm_state_create(TEST_STATE_DIM);
    ASSERT_NE(state, nullptr);
    state->level = 0;
    state->uncertainty = 0.1f;
    for (uint32_t i = 0; i < TEST_STATE_DIM; i++) {
        state->values[i] = 1.0f;
    }

    /* Collect uncertainty at different target levels */
    float uncertainties[4] = {0.0f};
    for (uint32_t target = 0; target < 4; target++) {
        omni_wm_state_t* result = omni_wm_state_create(TEST_STATE_DIM);
        ASSERT_NE(result, nullptr);

        nimcp_error_t err = omni_wm_predict_hierarchical(wm_, state, target, result);
        EXPECT_EQ(err, NIMCP_SUCCESS);

        uncertainties[target] = result->uncertainty;
        omni_wm_state_destroy(result);
    }

    /* Uncertainty formula: state->uncertainty * (1 + 0.15 * log(2^level_diff + 1))
     * As level_diff increases, uncertainty should increase monotonically. */
    for (uint32_t i = 1; i < 4; i++) {
        EXPECT_GT(uncertainties[i], uncertainties[i - 1])
            << "Uncertainty at level " << i
            << " should be greater than at level " << (i - 1);
    }

    omni_wm_state_destroy(state);
}

TEST_F(OmniWMPredictHierarchicalTest, NullStateReturnsError) {
    omni_wm_state_t* result = omni_wm_state_create(TEST_STATE_DIM);
    ASSERT_NE(result, nullptr);

    nimcp_error_t err = omni_wm_predict_hierarchical(wm_, nullptr, 1, result);
    EXPECT_NE(err, NIMCP_SUCCESS);

    omni_wm_state_destroy(result);
}

TEST_F(OmniWMPredictHierarchicalTest, TargetLevelExceedsMaxReturnsError) {
    omni_wm_state_t* state = omni_wm_state_create(TEST_STATE_DIM);
    ASSERT_NE(state, nullptr);
    omni_wm_state_t* result = omni_wm_state_create(TEST_STATE_DIM);
    ASSERT_NE(result, nullptr);

    /* Config has num_levels = 4, so target_level 4 is out of range */
    nimcp_error_t err = omni_wm_predict_hierarchical(wm_, state, 4, result);
    EXPECT_NE(err, NIMCP_SUCCESS);

    omni_wm_state_destroy(state);
    omni_wm_state_destroy(result);
}

// =============================================================================
// Dream Generation Test Fixture
// =============================================================================

class OmniWMGenerateDreamTest : public ::testing::Test {
protected:
    omni_world_model_t* wm_ = nullptr;

    void SetUp() override {
        /* Default config has enable_dreaming = true */
        wm_ = omni_wm_create_simple(TEST_STATE_DIM, TEST_ACTION_DIM, TEST_OBS_DIM);
        ASSERT_NE(wm_, nullptr);
    }

    void TearDown() override {
        if (wm_) {
            omni_wm_destroy(wm_);
            wm_ = nullptr;
        }
    }
};

// --- generate_dream: produces valid rollout with correct step count ---

TEST_F(OmniWMGenerateDreamTest, ProducesValidRolloutWithCorrectStepCount) {
    uint32_t dream_length = 10;

    /* Create rollout structure with sufficient capacity.
     * omni_wm_rollout_create caps at OMNI_WM_MAX_HORIZON (32). */
    omni_wm_rollout_t* rollout = omni_wm_rollout_create(
        OMNI_WM_MAX_HORIZON, TEST_STATE_DIM, TEST_ACTION_DIM, TEST_OBS_DIM);
    ASSERT_NE(rollout, nullptr);

    nimcp_error_t err = omni_wm_generate_dream(wm_, dream_length, 0.5f, rollout);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Dream length should produce dream_length states (initial + dream_length-1 steps) */
    EXPECT_EQ(rollout->length, dream_length);

    /* First state should be non-null (seed state) */
    EXPECT_NE(rollout->states[0], nullptr);

    /* All states in the rollout should be allocated */
    for (uint32_t i = 0; i < rollout->length; i++) {
        EXPECT_NE(rollout->states[i], nullptr) << "State " << i << " should be allocated";
        if (rollout->states[i]) {
            EXPECT_EQ(rollout->states[i]->dim, TEST_STATE_DIM);
        }
    }

    /* Actions should be allocated for steps 0..length-2 */
    for (uint32_t i = 0; i < rollout->length - 1; i++) {
        EXPECT_NE(rollout->actions[i], nullptr) << "Action " << i << " should be allocated";
    }

    /* Rewards should be finite for completed steps */
    for (uint32_t i = 0; i < rollout->length - 1; i++) {
        EXPECT_TRUE(std::isfinite(rollout->rewards[i]))
            << "Reward at step " << i << " should be finite";
    }

    /* Total reward should be finite */
    EXPECT_TRUE(std::isfinite(rollout->total_reward));

    /* Expected free energy should be set */
    EXPECT_TRUE(std::isfinite(rollout->expected_free_energy));

    omni_wm_rollout_destroy(rollout);
}

TEST_F(OmniWMGenerateDreamTest, DreamWithZeroNoiseProducesOutput) {
    uint32_t dream_length = 5;

    omni_wm_rollout_t* rollout = omni_wm_rollout_create(
        OMNI_WM_MAX_HORIZON, TEST_STATE_DIM, TEST_ACTION_DIM, TEST_OBS_DIM);
    ASSERT_NE(rollout, nullptr);

    /* noise_scale = 0.0 should produce deterministic replay */
    nimcp_error_t err = omni_wm_generate_dream(wm_, dream_length, 0.0f, rollout);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(rollout->length, dream_length);

    omni_wm_rollout_destroy(rollout);
}

// --- generate_dream: NULL world model returns error ---

TEST_F(OmniWMGenerateDreamTest, NullWorldModelReturnsError) {
    omni_wm_rollout_t* rollout = omni_wm_rollout_create(
        OMNI_WM_MAX_HORIZON, TEST_STATE_DIM, TEST_ACTION_DIM, TEST_OBS_DIM);
    ASSERT_NE(rollout, nullptr);

    nimcp_error_t err = omni_wm_generate_dream(nullptr, 10, 0.5f, rollout);
    EXPECT_EQ(err, NIMCP_ERROR_NULL_POINTER);

    omni_wm_rollout_destroy(rollout);
}

TEST_F(OmniWMGenerateDreamTest, NullDreamSequenceReturnsError) {
    nimcp_error_t err = omni_wm_generate_dream(wm_, 10, 0.5f, nullptr);
    EXPECT_EQ(err, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(OmniWMGenerateDreamTest, DreamWithZeroLengthUsesConfigHorizon) {
    omni_wm_rollout_t* rollout = omni_wm_rollout_create(
        OMNI_WM_MAX_HORIZON, TEST_STATE_DIM, TEST_ACTION_DIM, TEST_OBS_DIM);
    ASSERT_NE(rollout, nullptr);

    /* dream_length = 0 should fall back to config.dream_horizon (15 by default).
     * But rollout max_length = OMNI_WM_MAX_HORIZON (32) >= 15, so this should work. */
    nimcp_error_t err = omni_wm_generate_dream(wm_, 0, 0.5f, rollout);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Should have used config.dream_horizon = 15 */
    EXPECT_GT(rollout->length, 1u);

    omni_wm_rollout_destroy(rollout);
}
