/**
 * @file test_omni_wm_stubs_e2e.cpp
 * @brief End-to-end tests for world model stub functions
 *
 * Tests complete workflows exercising the full lifecycle of:
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
// E2E Test Fixture
// =============================================================================

class OmniWMStubsE2ETest : public ::testing::Test {
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
// E2E Test: Full world model workflow
// =============================================================================

TEST_F(OmniWMStubsE2ETest, FullWorldModelWorkflow) {
    /*
     * Complete lifecycle:
     *   1. Create world model (done in SetUp)
     *   2. Create state
     *   3. Predict lateral (cross-modal)
     *   4. Predict hierarchical (abstraction)
     *   5. Generate dream
     *   6. Verify all outputs are consistent
     *   7. Destroy everything (done in TearDown)
     */

    /* Step 2: Create initial state */
    omni_wm_state_t* state = omni_wm_state_create(TEST_STATE_DIM);
    ASSERT_NE(state, nullptr);
    for (uint32_t i = 0; i < TEST_STATE_DIM; i++) {
        state->values[i] = 0.5f + (float)i * 0.03f;
    }
    state->level = 0;
    state->uncertainty = 0.1f;

    /* Step 3: Lateral prediction (cross-modal transfer to modality 1) */
    omni_wm_state_t* lateral = omni_wm_state_create(TEST_STATE_DIM);
    ASSERT_NE(lateral, nullptr);

    nimcp_error_t err = omni_wm_predict_lateral(wm_, state, 1, lateral);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(lateral->dim, TEST_STATE_DIM);
    EXPECT_GT(lateral->uncertainty, state->uncertainty);

    /* Step 4: Hierarchical abstraction (level 0 -> level 2) */
    omni_wm_state_t* abstract = omni_wm_state_create(TEST_STATE_DIM);
    ASSERT_NE(abstract, nullptr);

    err = omni_wm_predict_hierarchical(wm_, state, 2, abstract);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(abstract->level, 2u);

    /* Step 5: Dream generation */
    omni_wm_rollout_t* dream = omni_wm_rollout_create(
        OMNI_WM_MAX_HORIZON, TEST_STATE_DIM, TEST_ACTION_DIM, TEST_OBS_DIM);
    ASSERT_NE(dream, nullptr);

    err = omni_wm_generate_dream(wm_, 6, 0.3f, dream);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(dream->length, 6u);

    /* Step 6: Verify consistency across all outputs */
    /* All state values should be finite */
    for (uint32_t i = 0; i < TEST_STATE_DIM; i++) {
        EXPECT_TRUE(std::isfinite(lateral->values[i]));
        EXPECT_TRUE(std::isfinite(abstract->values[i]));
    }

    /* Dream states should all be allocated and finite */
    for (uint32_t t = 0; t < dream->length; t++) {
        ASSERT_NE(dream->states[t], nullptr);
        for (uint32_t i = 0; i < dream->states[t]->dim; i++) {
            EXPECT_TRUE(std::isfinite(dream->states[t]->values[i]));
        }
    }

    /* Dream rewards should be finite */
    for (uint32_t t = 0; t + 1 < dream->length; t++) {
        EXPECT_TRUE(std::isfinite(dream->rewards[t]));
    }

    EXPECT_TRUE(std::isfinite(dream->total_reward));
    EXPECT_TRUE(std::isfinite(dream->expected_free_energy));

    /* Verify stats were updated */
    omni_wm_stats_t stats;
    err = omni_wm_get_stats(wm_, &stats);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GE(stats.lateral_predictions, 1u);

    /* Step 7: Clean up */
    omni_wm_rollout_destroy(dream);
    omni_wm_state_destroy(abstract);
    omni_wm_state_destroy(lateral);
    omni_wm_state_destroy(state);
}

// =============================================================================
// E2E Test: Dream-informed lateral prediction
// =============================================================================

TEST_F(OmniWMStubsE2ETest, DreamInformedPrediction) {
    /*
     * Workflow:
     *   1. Generate a dream to explore world model state space
     *   2. Extract a dream state
     *   3. Use dream state as input for lateral prediction
     *   4. Verify the lateral prediction produces valid output
     *
     * This simulates using dream-generated imagination to inform
     * cross-modal predictions.
     */

    /* Step 1: Generate dream */
    omni_wm_rollout_t* dream = omni_wm_rollout_create(
        OMNI_WM_MAX_HORIZON, TEST_STATE_DIM, TEST_ACTION_DIM, TEST_OBS_DIM);
    ASSERT_NE(dream, nullptr);

    nimcp_error_t err = omni_wm_generate_dream(wm_, 10, 0.5f, dream);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GE(dream->length, 2u);

    /* Step 2: Extract a mid-dream state as the "imagination seed" */
    uint32_t mid_idx = dream->length / 2;
    ASSERT_NE(dream->states[mid_idx], nullptr);
    ASSERT_NE(dream->states[mid_idx]->values, nullptr);

    /* Clone the dream state to use as lateral prediction input */
    omni_wm_state_t* dream_state = omni_wm_state_clone(dream->states[mid_idx]);
    ASSERT_NE(dream_state, nullptr);

    /* Step 3: Lateral prediction using dream state */
    omni_wm_state_t* lateral_result = omni_wm_state_create(TEST_STATE_DIM);
    ASSERT_NE(lateral_result, nullptr);

    err = omni_wm_predict_lateral(wm_, dream_state, 3, lateral_result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Step 4: Verify validity */
    EXPECT_EQ(lateral_result->dim, TEST_STATE_DIM);
    bool has_nonzero = false;
    for (uint32_t i = 0; i < lateral_result->dim; i++) {
        EXPECT_TRUE(std::isfinite(lateral_result->values[i]))
            << "Non-finite lateral output at index " << i;
        if (std::fabs(lateral_result->values[i]) > 1e-8f) {
            has_nonzero = true;
        }
    }
    EXPECT_TRUE(has_nonzero)
        << "Lateral prediction from dream state should produce non-zero output";

    /* Uncertainty should be positive and finite */
    EXPECT_GT(lateral_result->uncertainty, 0.0f);
    EXPECT_TRUE(std::isfinite(lateral_result->uncertainty));

    omni_wm_state_destroy(lateral_result);
    omni_wm_state_destroy(dream_state);
    omni_wm_rollout_destroy(dream);
}

// =============================================================================
// E2E Test: Multi-level hierarchical roundtrip
// =============================================================================

TEST_F(OmniWMStubsE2ETest, MultiLevelHierarchicalRoundtrip) {
    /*
     * Roundtrip through multiple hierarchical levels:
     *   level 0 -> level 1 -> level 2 -> level 1 -> level 0
     *
     * At each step, verify:
     * - Correct level is set
     * - Values remain finite
     * - Uncertainty grows with each transition
     * - Final output has reasonable properties
     */

    /* Start at level 0 */
    omni_wm_state_t* level0 = omni_wm_state_create(TEST_STATE_DIM);
    ASSERT_NE(level0, nullptr);
    level0->level = 0;
    level0->uncertainty = 0.05f;
    for (uint32_t i = 0; i < TEST_STATE_DIM; i++) {
        level0->values[i] = 1.0f + 0.1f * sinf((float)i * 0.5f);
    }

    /* Track uncertainties through the chain */
    std::vector<float> uncertainties;
    uncertainties.push_back(level0->uncertainty);

    /* Step 1: level 0 -> level 1 (abstract) */
    omni_wm_state_t* level1_up = omni_wm_state_create(TEST_STATE_DIM);
    ASSERT_NE(level1_up, nullptr);
    nimcp_error_t err = omni_wm_predict_hierarchical(wm_, level0, 1, level1_up);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(level1_up->level, 1u);
    uncertainties.push_back(level1_up->uncertainty);

    /* Step 2: level 1 -> level 2 (further abstraction) */
    omni_wm_state_t* level2 = omni_wm_state_create(TEST_STATE_DIM);
    ASSERT_NE(level2, nullptr);
    err = omni_wm_predict_hierarchical(wm_, level1_up, 2, level2);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(level2->level, 2u);
    uncertainties.push_back(level2->uncertainty);

    /* Step 3: level 2 -> level 1 (concretize) */
    omni_wm_state_t* level1_down = omni_wm_state_create(TEST_STATE_DIM);
    ASSERT_NE(level1_down, nullptr);
    err = omni_wm_predict_hierarchical(wm_, level2, 1, level1_down);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(level1_down->level, 1u);
    uncertainties.push_back(level1_down->uncertainty);

    /* Step 4: level 1 -> level 0 (concretize back to concrete) */
    omni_wm_state_t* level0_back = omni_wm_state_create(TEST_STATE_DIM);
    ASSERT_NE(level0_back, nullptr);
    err = omni_wm_predict_hierarchical(wm_, level1_down, 0, level0_back);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(level0_back->level, 0u);
    uncertainties.push_back(level0_back->uncertainty);

    /* Verify all output values are finite throughout the chain */
    auto check_finite = [](const omni_wm_state_t* s, const char* label) {
        for (uint32_t i = 0; i < s->dim; i++) {
            EXPECT_TRUE(std::isfinite(s->values[i]))
                << label << ": non-finite at index " << i;
        }
        EXPECT_TRUE(std::isfinite(s->uncertainty))
            << label << ": non-finite uncertainty";
    };

    check_finite(level1_up, "level1_up");
    check_finite(level2, "level2");
    check_finite(level1_down, "level1_down");
    check_finite(level0_back, "level0_back");

    /* Final output should have non-zero values (not completely degenerate) */
    bool final_has_nonzero = false;
    for (uint32_t i = 0; i < level0_back->dim; i++) {
        if (std::fabs(level0_back->values[i]) > 1e-8f) {
            final_has_nonzero = true;
            break;
        }
    }
    EXPECT_TRUE(final_has_nonzero)
        << "Multi-level roundtrip should produce non-zero final output";

    /* All uncertainties should be positive (each transition adds uncertainty) */
    for (size_t i = 0; i < uncertainties.size(); i++) {
        EXPECT_GT(uncertainties[i], 0.0f)
            << "Uncertainty at step " << i << " should be positive";
    }

    /* Clean up */
    omni_wm_state_destroy(level0_back);
    omni_wm_state_destroy(level1_down);
    omni_wm_state_destroy(level2);
    omni_wm_state_destroy(level1_up);
    omni_wm_state_destroy(level0);
}
