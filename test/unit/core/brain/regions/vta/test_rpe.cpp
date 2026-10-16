/**
 * @file test_rpe.cpp
 * @brief Unit tests for Reward Prediction Error (RPE) system
 * @date 2026-01-11
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

extern "C" {
#include "core/brain/regions/vta/nimcp_reward_prediction_error.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class RPETest : public ::testing::Test {
protected:
    nimcp_rpe_system_t system;

    void SetUp() override {
        int err = nimcp_rpe_init(&system, nullptr);
        ASSERT_EQ(err, 0);
    }

    void TearDown() override {
        nimcp_rpe_shutdown(&system);
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(RPETest, InitSucceeds) {
    EXPECT_TRUE(system.initialized);
}

TEST_F(RPETest, InitNullReturnsError) {
    int err = nimcp_rpe_init(nullptr, nullptr);
    EXPECT_EQ(err, -1);
}

TEST_F(RPETest, ShutdownClearsState) {
    nimcp_rpe_shutdown(&system);
    EXPECT_FALSE(system.initialized);
}

TEST_F(RPETest, ResetWorks) {
    system.current_rpe = 0.8f;
    int err = nimcp_rpe_reset(&system);
    EXPECT_EQ(err, 0);
    EXPECT_FLOAT_EQ(system.current_rpe, 0.0f);
}

TEST_F(RPETest, DefaultConfigValid) {
    nimcp_rpe_config_t config = nimcp_rpe_default_config();
    EXPECT_FLOAT_EQ(config.alpha, RPE_DEFAULT_ALPHA);
    EXPECT_FLOAT_EQ(config.gamma, RPE_DEFAULT_GAMMA);
    EXPECT_FLOAT_EQ(config.lambda, RPE_DEFAULT_LAMBDA);
}

TEST_F(RPETest, CustomConfigApplied) {
    nimcp_rpe_shutdown(&system);

    nimcp_rpe_config_t config = nimcp_rpe_default_config();
    config.alpha = 0.2f;
    config.gamma = 0.9f;

    nimcp_rpe_init(&system, &config);

    EXPECT_FLOAT_EQ(system.config.alpha, 0.2f);
    EXPECT_FLOAT_EQ(system.config.gamma, 0.9f);
}

//=============================================================================
// Core RPE Computation Tests
//=============================================================================

TEST_F(RPETest, ComputeSucceeds) {
    nimcp_rpe_result_t result;
    int err = nimcp_rpe_compute(&system, 1.0f, &result);
    EXPECT_EQ(err, 0);
}

TEST_F(RPETest, ComputeNullReturnsError) {
    nimcp_rpe_result_t result;
    EXPECT_EQ(nimcp_rpe_compute(nullptr, 1.0f, &result), -1);
    EXPECT_EQ(nimcp_rpe_compute(&system, 1.0f, nullptr), -1);
}

TEST_F(RPETest, PositiveRPEWhenRewardExceedsExpectation) {
    nimcp_rpe_set_expectation(&system, 0.0f);

    nimcp_rpe_result_t result;
    nimcp_rpe_compute(&system, 1.0f, &result);

    EXPECT_GT(result.rpe, 0.0f);
    EXPECT_EQ(result.type, RPE_TYPE_POSITIVE);
    EXPECT_TRUE(result.triggers_burst);
    EXPECT_FALSE(result.triggers_pause);
}

TEST_F(RPETest, NegativeRPEWhenExpectationExceedsReward) {
    nimcp_rpe_set_expectation(&system, 1.0f);

    nimcp_rpe_result_t result;
    nimcp_rpe_compute(&system, 0.0f, &result);

    EXPECT_LT(result.rpe, 0.0f);
    EXPECT_EQ(result.type, RPE_TYPE_NEGATIVE);
    EXPECT_FALSE(result.triggers_burst);
    EXPECT_TRUE(result.triggers_pause);
}

TEST_F(RPETest, ZeroRPEWhenMatchesExpectation) {
    nimcp_rpe_set_expectation(&system, 0.5f);

    nimcp_rpe_result_t result;
    nimcp_rpe_compute(&system, 0.5f, &result);

    EXPECT_FLOAT_EQ(result.rpe, 0.0f);
    EXPECT_EQ(result.type, RPE_TYPE_NONE);
}

TEST_F(RPETest, RPEMagnitudeCorrect) {
    nimcp_rpe_set_expectation(&system, 0.3f);

    nimcp_rpe_result_t result;
    nimcp_rpe_compute(&system, 0.8f, &result);

    EXPECT_FLOAT_EQ(result.expected, 0.3f);
    EXPECT_FLOAT_EQ(result.actual, 0.8f);
    EXPECT_FLOAT_EQ(result.magnitude, 0.5f);
}

//=============================================================================
// Learning Tests
//=============================================================================

TEST_F(RPETest, LearnSucceeds) {
    int err = nimcp_rpe_learn(&system, 0.5f);
    EXPECT_EQ(err, 0);
}

TEST_F(RPETest, LearnNullReturnsError) {
    EXPECT_EQ(nimcp_rpe_learn(nullptr, 0.5f), -1);
}

TEST_F(RPETest, LearnUpdatesStateValue) {
    nimcp_rpe_transition_state(&system, 1);

    float before;
    nimcp_rpe_get_state_value(&system, 1, &before);

    /* Positive RPE should increase value */
    nimcp_rpe_learn(&system, 0.5f);

    float after;
    nimcp_rpe_get_state_value(&system, 1, &after);

    EXPECT_GT(after, before);
}

//=============================================================================
// Expectation Tests
//=============================================================================

TEST_F(RPETest, GetExpectationSucceeds) {
    float expected;
    int err = nimcp_rpe_get_expectation(&system, &expected);
    EXPECT_EQ(err, 0);
}

TEST_F(RPETest, SetExpectationSucceeds) {
    int err = nimcp_rpe_set_expectation(&system, 0.7f);
    EXPECT_EQ(err, 0);

    float expected;
    nimcp_rpe_get_expectation(&system, &expected);
    EXPECT_FLOAT_EQ(expected, 0.7f);
}

//=============================================================================
// State Transition Tests
//=============================================================================

TEST_F(RPETest, TransitionStateSucceeds) {
    int err = nimcp_rpe_transition_state(&system, 5);
    EXPECT_EQ(err, 0);
    EXPECT_EQ(system.current_state, 5u);
}

TEST_F(RPETest, TransitionStateBoundsCheck) {
    int err = nimcp_rpe_transition_state(&system, RPE_MAX_STATES + 1);
    EXPECT_EQ(err, -1);
}

TEST_F(RPETest, GetStateValueSucceeds) {
    float value;
    int err = nimcp_rpe_get_state_value(&system, 0, &value);
    EXPECT_EQ(err, 0);
}

TEST_F(RPETest, SetStateValueSucceeds) {
    int err = nimcp_rpe_set_state_value(&system, 10, 0.5f);
    EXPECT_EQ(err, 0);

    float value;
    nimcp_rpe_get_state_value(&system, 10, &value);
    EXPECT_FLOAT_EQ(value, 0.5f);
}

//=============================================================================
// Cue Tests
//=============================================================================

TEST_F(RPETest, AddCueSucceeds) {
    uint32_t cue_id;
    int err = nimcp_rpe_add_cue(&system, 0.5f, &cue_id);
    EXPECT_EQ(err, 0);
    EXPECT_EQ(system.num_cues, 1u);
}

TEST_F(RPETest, CueCapacityEnforced) {
    for (int i = 0; i < RPE_MAX_CUES; i++) {
        uint32_t id;
        nimcp_rpe_add_cue(&system, 0.5f, &id);
    }

    uint32_t id;
    int err = nimcp_rpe_add_cue(&system, 0.5f, &id);
    EXPECT_EQ(err, -1);
}

TEST_F(RPETest, CueOnsetSucceeds) {
    uint32_t cue_id;
    nimcp_rpe_add_cue(&system, 0.5f, &cue_id);

    int err = nimcp_rpe_cue_onset(&system, cue_id, 100.0f);
    EXPECT_EQ(err, 0);
    EXPECT_TRUE(system.cues[cue_id].active);
}

TEST_F(RPETest, CueOffsetSucceeds) {
    uint32_t cue_id;
    nimcp_rpe_add_cue(&system, 0.5f, &cue_id);
    nimcp_rpe_cue_onset(&system, cue_id, 100.0f);

    int err = nimcp_rpe_cue_offset(&system, cue_id);
    EXPECT_EQ(err, 0);
    EXPECT_FALSE(system.cues[cue_id].active);
}

TEST_F(RPETest, GetCueValueSucceeds) {
    uint32_t cue_id;
    nimcp_rpe_add_cue(&system, 0.7f, &cue_id);

    float value;
    int err = nimcp_rpe_get_cue_value(&system, cue_id, &value);
    EXPECT_EQ(err, 0);
    EXPECT_FLOAT_EQ(value, 0.7f);
}

TEST_F(RPETest, UpdateCueLearningSucceeds) {
    uint32_t cue_id;
    nimcp_rpe_add_cue(&system, 0.3f, &cue_id);
    nimcp_rpe_cue_onset(&system, cue_id, 100.0f);

    int err = nimcp_rpe_update_cue_learning(&system, 1.0f);
    EXPECT_EQ(err, 0);
}

//=============================================================================
// Eligibility Trace Tests
//=============================================================================

TEST_F(RPETest, UpdateTracesSucceeds) {
    int err = nimcp_rpe_update_traces(&system, 10.0f);
    EXPECT_EQ(err, 0);
}

TEST_F(RPETest, ResetTracesSucceeds) {
    system.eligibility_traces[0] = 0.5f;
    int err = nimcp_rpe_reset_traces(&system);
    EXPECT_EQ(err, 0);
    EXPECT_FLOAT_EQ(system.eligibility_traces[0], 0.0f);
}

TEST_F(RPETest, GetEligibilitySucceeds) {
    float eligibility;
    int err = nimcp_rpe_get_eligibility(&system, 0, &eligibility);
    EXPECT_EQ(err, 0);
}

//=============================================================================
// Update Tests
//=============================================================================

TEST_F(RPETest, UpdateSucceeds) {
    int err = nimcp_rpe_update(&system, 10.0f);
    EXPECT_EQ(err, 0);
}

TEST_F(RPETest, UpdateNullReturnsError) {
    EXPECT_EQ(nimcp_rpe_update(nullptr, 10.0f), -1);
}

TEST_F(RPETest, UpdateWithNegativeDtDoesNotCrash) {
    /* Implementation does not validate dt - just verify no crash */
    int err = nimcp_rpe_update(&system, -1.0f);
    EXPECT_EQ(err, 0);  /* Current implementation accepts any dt */
}

TEST_F(RPETest, LongTermStability) {
    for (int i = 0; i < 10000; i++) {
        nimcp_rpe_update(&system, 1.0f);
    }

    EXPECT_FALSE(std::isnan(system.current_rpe));
    EXPECT_FALSE(std::isinf(system.current_rpe));
}

//=============================================================================
// Query Tests
//=============================================================================

TEST_F(RPETest, GetCurrentSucceeds) {
    float rpe;
    int err = nimcp_rpe_get_current(&system, &rpe);
    EXPECT_EQ(err, 0);
}

TEST_F(RPETest, GetLastResultSucceeds) {
    nimcp_rpe_result_t result;
    nimcp_rpe_compute(&system, 0.5f, &result);

    nimcp_rpe_result_t last;
    int err = nimcp_rpe_get_last_result(&system, &last);
    EXPECT_EQ(err, 0);
    EXPECT_FLOAT_EQ(last.actual, 0.5f);
}

TEST_F(RPETest, ClassifyRPE) {
    EXPECT_EQ(nimcp_rpe_classify(0.5f, 0.3f), RPE_TYPE_POSITIVE);
    EXPECT_EQ(nimcp_rpe_classify(-0.5f, 0.3f), RPE_TYPE_NEGATIVE);
    EXPECT_EQ(nimcp_rpe_classify(0.1f, 0.3f), RPE_TYPE_NONE);
}

TEST_F(RPETest, RPEToDAResponseSucceeds) {
    float da_response;
    int err = nimcp_rpe_to_da_response(&system, 0.5f, &da_response);
    EXPECT_EQ(err, 0);
    EXPECT_GT(da_response, 0.0f);
}

TEST_F(RPETest, NegativeRPEGivesNegativeDAResponse) {
    float da_response;
    nimcp_rpe_to_da_response(&system, -0.5f, &da_response);
    EXPECT_LT(da_response, 0.0f);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
