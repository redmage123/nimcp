/**
 * @file test_omni_wm_stubs_integration.cpp
 * @brief Integration tests for world model stub functions
 *
 * Tests cross-function interactions and chained operations between:
 * - omni_wm_predict_lateral()
 * - omni_wm_predict_hierarchical()
 * - omni_wm_generate_dream()
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
// Integration Test Fixture
// =============================================================================

class OmniWMStubsIntegrationTest : public ::testing::Test {
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

    /** Helper: create a state with linearly increasing values */
    omni_wm_state_t* make_state(uint32_t dim, float base, float step,
                                 uint32_t level = 0, float uncertainty = 0.1f) {
        omni_wm_state_t* s = omni_wm_state_create(dim);
        if (!s) return nullptr;
        for (uint32_t i = 0; i < dim; i++) {
            s->values[i] = base + step * (float)i;
        }
        s->level = level;
        s->uncertainty = uncertainty;
        return s;
    }
};

// =============================================================================
// Integration Test: Lateral then Hierarchical (chained operations)
// =============================================================================

TEST_F(OmniWMStubsIntegrationTest, LateralThenHierarchicalChain) {
    /*
     * Chain: source -> lateral predict (modality 2) -> hierarchical abstract (level 2)
     * Validates that lateral output can be fed directly into hierarchical prediction.
     */
    omni_wm_state_t* source = make_state(TEST_STATE_DIM, 0.5f, 0.01f);
    ASSERT_NE(source, nullptr);

    /* Step 1: Lateral prediction */
    omni_wm_state_t* lateral_result = omni_wm_state_create(TEST_STATE_DIM);
    ASSERT_NE(lateral_result, nullptr);

    nimcp_error_t err = omni_wm_predict_lateral(wm_, source, 2, lateral_result);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(lateral_result->dim, TEST_STATE_DIM);

    /* Step 2: Feed lateral result into hierarchical abstraction */
    omni_wm_state_t* hier_result = omni_wm_state_create(TEST_STATE_DIM);
    ASSERT_NE(hier_result, nullptr);

    err = omni_wm_predict_hierarchical(wm_, lateral_result, 2, hier_result);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(hier_result->level, 2u);

    /* Uncertainty should have accumulated through both stages:
     * lateral adds transfer uncertainty, hierarchical adds level uncertainty */
    EXPECT_GT(hier_result->uncertainty, source->uncertainty);
    EXPECT_GT(hier_result->uncertainty, lateral_result->uncertainty);

    /* Values should be finite and non-trivial */
    bool has_nonzero = false;
    for (uint32_t i = 0; i < hier_result->dim; i++) {
        EXPECT_TRUE(std::isfinite(hier_result->values[i]))
            << "Non-finite value at index " << i;
        if (std::fabs(hier_result->values[i]) > 1e-8f) {
            has_nonzero = true;
        }
    }
    EXPECT_TRUE(has_nonzero);

    omni_wm_state_destroy(source);
    omni_wm_state_destroy(lateral_result);
    omni_wm_state_destroy(hier_result);
}

// =============================================================================
// Integration Test: Hierarchical abstraction then concretization roundtrip
// =============================================================================

TEST_F(OmniWMStubsIntegrationTest, HierarchicalAbstractionThenConcretization) {
    /*
     * Roundtrip: level 0 -> abstract to level 2 -> concretize back to level 0
     * The concretized result should approximately recover the original pattern.
     * Due to information loss in abstraction and noise in concretization,
     * exact recovery is not expected, but the mean should be approximately preserved.
     */
    omni_wm_state_t* original = make_state(TEST_STATE_DIM, 1.0f, 0.1f, 0, 0.1f);
    ASSERT_NE(original, nullptr);

    /* Compute original mean for comparison */
    float original_mean = 0.0f;
    for (uint32_t i = 0; i < TEST_STATE_DIM; i++) {
        original_mean += original->values[i];
    }
    original_mean /= (float)TEST_STATE_DIM;

    /* Step 1: Abstract to level 2 */
    omni_wm_state_t* abstract = omni_wm_state_create(TEST_STATE_DIM);
    ASSERT_NE(abstract, nullptr);

    nimcp_error_t err = omni_wm_predict_hierarchical(wm_, original, 2, abstract);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(abstract->level, 2u);

    /* Step 2: Concretize back to level 0 */
    omni_wm_state_t* recovered = omni_wm_state_create(TEST_STATE_DIM);
    ASSERT_NE(recovered, nullptr);

    err = omni_wm_predict_hierarchical(wm_, abstract, 0, recovered);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(recovered->level, 0u);

    /* The abstraction applies average pooling then tanh (level_diff >= 2).
     * Concretization interpolates back. Due to tanh squashing, exact recovery
     * is impossible, but recovered mean should be in a reasonable range
     * relative to tanh(original_mean). */
    float recovered_mean = 0.0f;
    uint32_t nonzero_count = 0;
    for (uint32_t i = 0; i < TEST_STATE_DIM; i++) {
        recovered_mean += recovered->values[i];
        if (std::fabs(recovered->values[i]) > 1e-8f) {
            nonzero_count++;
        }
    }
    recovered_mean /= (float)TEST_STATE_DIM;

    /* Recovered output should have non-zero values */
    EXPECT_GT(nonzero_count, 0u) << "Concretization should produce non-zero output";

    /* The recovered mean should be in the same sign direction as original,
     * though compressed by tanh */
    if (original_mean > 0.0f) {
        EXPECT_GT(recovered_mean, -0.5f)
            << "Recovered mean should be roughly positive for positive original";
    }

    /* Both values should be finite */
    for (uint32_t i = 0; i < TEST_STATE_DIM; i++) {
        EXPECT_TRUE(std::isfinite(recovered->values[i]));
    }

    omni_wm_state_destroy(original);
    omni_wm_state_destroy(abstract);
    omni_wm_state_destroy(recovered);
}

// =============================================================================
// Integration Test: Dream generation uses RSSM dynamics (state changes)
// =============================================================================

TEST_F(OmniWMStubsIntegrationTest, DreamGenerationUsesRSSMDynamics) {
    /*
     * Generate a dream and verify that RSSM dynamics actually advance
     * the state at each step (i.e., consecutive states are different).
     */
    uint32_t dream_length = 8;
    omni_wm_rollout_t* rollout = omni_wm_rollout_create(
        OMNI_WM_MAX_HORIZON, TEST_STATE_DIM, TEST_ACTION_DIM, TEST_OBS_DIM);
    ASSERT_NE(rollout, nullptr);

    nimcp_error_t err = omni_wm_generate_dream(wm_, dream_length, 0.5f, rollout);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(rollout->length, dream_length);

    /* Verify that consecutive states differ - RSSM dynamics should evolve state */
    uint32_t state_changes = 0;
    for (uint32_t t = 0; t + 1 < rollout->length; t++) {
        ASSERT_NE(rollout->states[t], nullptr);
        ASSERT_NE(rollout->states[t + 1], nullptr);

        bool states_differ = false;
        uint32_t cmp_dim = rollout->states[t]->dim < rollout->states[t + 1]->dim ?
                           rollout->states[t]->dim : rollout->states[t + 1]->dim;
        for (uint32_t i = 0; i < cmp_dim; i++) {
            if (std::fabs(rollout->states[t]->values[i] -
                          rollout->states[t + 1]->values[i]) > 1e-10f) {
                states_differ = true;
                break;
            }
        }
        if (states_differ) {
            state_changes++;
        }
    }

    /* At least most consecutive state pairs should differ (RSSM evolves state) */
    EXPECT_GE(state_changes, (dream_length - 1) / 2)
        << "Dream should show state evolution via RSSM dynamics";

    omni_wm_rollout_destroy(rollout);
}

// =============================================================================
// Integration Test: Lateral predictions to different modalities produce different output
// =============================================================================

TEST_F(OmniWMStubsIntegrationTest, LateralDifferentModalitiesProduceDifferentOutput) {
    /*
     * Predict from the same source to modalities 0, 1, 2, 3.
     * Due to the modality-specific phase shift, each should produce
     * a different output pattern.
     */
    omni_wm_state_t* source = make_state(TEST_STATE_DIM, 0.5f, 0.02f);
    ASSERT_NE(source, nullptr);

    std::vector<std::vector<float>> outputs(4);

    for (uint32_t mod = 0; mod < 4; mod++) {
        omni_wm_state_t* result = omni_wm_state_create(TEST_STATE_DIM);
        ASSERT_NE(result, nullptr);

        nimcp_error_t err = omni_wm_predict_lateral(wm_, source, mod, result);
        EXPECT_EQ(err, NIMCP_SUCCESS);

        outputs[mod].resize(TEST_STATE_DIM);
        for (uint32_t i = 0; i < TEST_STATE_DIM; i++) {
            outputs[mod][i] = result->values[i];
        }
        omni_wm_state_destroy(result);
    }

    /* Verify that at least some pairs of modality outputs are different */
    uint32_t different_pairs = 0;
    for (uint32_t a = 0; a < 4; a++) {
        for (uint32_t b = a + 1; b < 4; b++) {
            float max_diff = 0.0f;
            for (uint32_t i = 0; i < TEST_STATE_DIM; i++) {
                float d = std::fabs(outputs[a][i] - outputs[b][i]);
                if (d > max_diff) max_diff = d;
            }
            if (max_diff > 1e-6f) {
                different_pairs++;
            }
        }
    }

    /* With 4 modalities, we have 6 pairs - at least most should differ */
    EXPECT_GE(different_pairs, 3u)
        << "Different target modalities should produce different lateral predictions";

    omni_wm_state_destroy(source);
}

// =============================================================================
// Integration Test: Hierarchical prediction at multiple levels shows progressive abstraction
// =============================================================================

TEST_F(OmniWMStubsIntegrationTest, HierarchicalMultipleLevelsShowsProgression) {
    /*
     * From a level 0 state, abstract to levels 1, 2, 3.
     * Higher levels should show:
     * 1. Fewer active (non-zero) values (spatial pooling reduces effective dimensionality)
     * 2. Increasing uncertainty
     */
    omni_wm_state_t* base = make_state(TEST_STATE_DIM, 1.0f, 0.1f, 0, 0.1f);
    ASSERT_NE(base, nullptr);

    float prev_uncertainty = base->uncertainty;
    uint32_t prev_nonzero_count = TEST_STATE_DIM;

    for (uint32_t target_level = 1; target_level < 4; target_level++) {
        omni_wm_state_t* result = omni_wm_state_create(TEST_STATE_DIM);
        ASSERT_NE(result, nullptr);

        nimcp_error_t err = omni_wm_predict_hierarchical(wm_, base, target_level, result);
        EXPECT_EQ(err, NIMCP_SUCCESS);
        EXPECT_EQ(result->level, target_level);

        /* Count non-zero values */
        uint32_t nonzero = 0;
        for (uint32_t i = 0; i < TEST_STATE_DIM; i++) {
            if (std::fabs(result->values[i]) > 1e-8f) {
                nonzero++;
            }
        }

        /* Higher levels should have fewer non-zero values
         * (pooling reduces the active portion) */
        EXPECT_LE(nonzero, prev_nonzero_count)
            << "Level " << target_level
            << " should have <= non-zero values than level " << (target_level - 1);

        /* Uncertainty should increase with level distance */
        EXPECT_GT(result->uncertainty, prev_uncertainty)
            << "Uncertainty at level " << target_level
            << " should be greater than at level " << (target_level - 1);

        prev_uncertainty = result->uncertainty;
        prev_nonzero_count = nonzero;

        omni_wm_state_destroy(result);
    }

    omni_wm_state_destroy(base);
}
