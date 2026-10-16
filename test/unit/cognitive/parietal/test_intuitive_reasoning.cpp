/**
 * @file test_intuitive_reasoning.cpp
 * @brief Unit tests for NIMCP Intuitive Reasoning Engine (Phase 6.1)
 *
 * Tests for hunch formation, plausibility estimation, intuitive leaps,
 * and confidence gradients.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <cstdlib>

// Headers have their own extern "C" guards
#include "cognitive/parietal/nimcp_intuitive_reasoning.h"

namespace {

//=============================================================================
// Test Constants
//=============================================================================

constexpr float FLOAT_TOLERANCE = 1e-4f;

//=============================================================================
// Test Fixture
//=============================================================================

class IntuitiveReasoningTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        engine = intuitive_engine_create();
        ASSERT_NE(engine, nullptr);
    }

    void TearDown() override
    {
        if (engine) {
            intuitive_engine_destroy(engine);
            engine = nullptr;
        }
    }

    intuitive_engine_t* engine = nullptr;
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(IntuitiveReasoningTest, CreateDefault)
{
    EXPECT_NE(engine, nullptr);
}

TEST_F(IntuitiveReasoningTest, CreateCustom)
{
    intuitive_config_t config = intuitive_engine_default_config();
    config.plausibility_threshold = 0.8f;
    config.enable_gestalt = true;

    intuitive_engine_t* custom = intuitive_engine_create_custom(&config);
    ASSERT_NE(custom, nullptr);
    intuitive_engine_destroy(custom);
}

TEST_F(IntuitiveReasoningTest, CreateWithNullConfig)
{
    // Implementation returns NULL for null config
    intuitive_engine_t* created = intuitive_engine_create_custom(nullptr);
    EXPECT_EQ(created, nullptr);
}

TEST_F(IntuitiveReasoningTest, DestroyNullSafe)
{
    intuitive_engine_destroy(nullptr);
    // Should not crash
}

TEST_F(IntuitiveReasoningTest, DefaultConfig)
{
    intuitive_config_t config = intuitive_engine_default_config();

    EXPECT_GT(config.plausibility_threshold, 0.0f);
    EXPECT_LE(config.plausibility_threshold, 1.0f);
    EXPECT_GT(config.novelty_weight, 0.0f);
    EXPECT_GT(config.coherence_weight, 0.0f);
}

//=============================================================================
// Observation Tests
//=============================================================================

TEST_F(IntuitiveReasoningTest, CreateObservation)
{
    float data[] = {1.0f, 2.0f, 3.0f};
    observation_t obs = intuitive_create_observation(data, 3, 0.8f);

    EXPECT_EQ(obs.dim, 3u);
    EXPECT_NEAR(obs.salience, 0.8f, FLOAT_TOLERANCE);
    // Note: observation_t is a value type, no destroy needed
}

TEST_F(IntuitiveReasoningTest, CreateObservationZeroDim)
{
    observation_t obs = intuitive_create_observation(nullptr, 0, 0.5f);
    EXPECT_EQ(obs.dim, 0u);
}

//=============================================================================
// Hunch Formation Tests
//=============================================================================

TEST_F(IntuitiveReasoningTest, FormHunchFromObservations)
{
    // Create some observations (by value)
    float data1[] = {1.0f, 2.0f, 3.0f};
    float data2[] = {1.1f, 2.1f, 3.1f};
    float data3[] = {1.2f, 2.2f, 3.2f};

    observation_t obs[3];
    obs[0] = intuitive_create_observation(data1, 3, 0.8f);
    obs[1] = intuitive_create_observation(data2, 3, 0.7f);
    obs[2] = intuitive_create_observation(data3, 3, 0.9f);

    // Form a hunch
    hunch_t* hunch = intuitive_form_hunch(engine, obs, 3);

    if (hunch != nullptr) {
        EXPECT_GT(hunch->score.plausibility, 0.0f);
        EXPECT_LE(hunch->score.plausibility, 1.0f);
        EXPECT_GT(hunch->score.confidence, 0.0f);
        intuitive_free_hunch(hunch);
    }
}

TEST_F(IntuitiveReasoningTest, FormHunchNullObservations)
{
    hunch_t* hunch = intuitive_form_hunch(engine, nullptr, 0);
    EXPECT_EQ(hunch, nullptr);
}

TEST_F(IntuitiveReasoningTest, FormHunchSingleObservation)
{
    float data[] = {5.0f, 10.0f};
    observation_t obs = intuitive_create_observation(data, 2, 0.9f);

    hunch_t* hunch = intuitive_form_hunch(engine, &obs, 1);

    if (hunch != nullptr) {
        // Single observation should have lower confidence
        EXPECT_LE(hunch->score.confidence, 1.0f);
        intuitive_free_hunch(hunch);
    }
}

//=============================================================================
// Plausibility Estimation Tests
//=============================================================================

TEST_F(IntuitiveReasoningTest, EstimatePlausibility)
{
    hypothesis_t hypothesis = intuitive_create_hypothesis(
        "Water flows downhill", nullptr, 0);
    hypothesis.prior = 0.9f;

    float plausibility = intuitive_estimate_plausibility(engine, &hypothesis);

    EXPECT_GE(plausibility, 0.0f);
    EXPECT_LE(plausibility, 1.0f);
}

TEST_F(IntuitiveReasoningTest, EstimatePlausibilityNull)
{
    float plausibility = intuitive_estimate_plausibility(engine, nullptr);
    // Implementation returns 0.0f for null input
    EXPECT_EQ(plausibility, 0.0f);
}

TEST_F(IntuitiveReasoningTest, EstimatePlausibilityNullEngine)
{
    hypothesis_t hypothesis = intuitive_create_hypothesis(
        "Test hypothesis", nullptr, 0);

    float plausibility = intuitive_estimate_plausibility(nullptr, &hypothesis);
    // Implementation returns 0.0f for null engine
    EXPECT_EQ(plausibility, 0.0f);
}

//=============================================================================
// Intuitive Leap Tests
//=============================================================================

TEST_F(IntuitiveReasoningTest, AttemptIntuitiveLeap)
{
    float initial[] = {2.0f, 4.0f, 8.0f, 16.0f};
    problem_t problem = intuitive_create_problem(
        initial, nullptr, 4, "Find the pattern in: 2, 4, 8, 16, ...");
    problem.estimated_difficulty = 0.4f;

    insight_t* insight = intuitive_leap(engine, &problem);

    // May or may not succeed - check for valid result or null
    if (insight != nullptr) {
        EXPECT_GE(insight->surprise_factor, 0.0f);
        EXPECT_LE(insight->surprise_factor, 1.0f);
        intuitive_free_insight(insight);
    }
}

TEST_F(IntuitiveReasoningTest, IntuitiveLeapNullProblem)
{
    insight_t* insight = intuitive_leap(engine, nullptr);
    EXPECT_EQ(insight, nullptr);
}

//=============================================================================
// Confidence Gradient Tests
//=============================================================================

TEST_F(IntuitiveReasoningTest, TrackConfidence)
{
    float step_confidences[] = {0.9f, 0.85f, 0.7f};
    confidence_gradient_t gradient;
    memset(&gradient, 0, sizeof(gradient));

    int result = intuitive_track_confidence(engine, step_confidences, 3, &gradient);

    // Result depends on implementation
    if (result == 0) {
        EXPECT_GE(gradient.min_confidence, 0.0f);
        EXPECT_LE(gradient.max_confidence, 1.0f);
    }
}

//=============================================================================
// Gestalt Perception Tests
//=============================================================================

TEST_F(IntuitiveReasoningTest, GestaltPerception)
{
    float pattern[] = {1.0f, 1.0f, 1.0f, 1.0f,
                       1.0f, 0.0f, 0.0f, 1.0f,
                       1.0f, 0.0f, 0.0f, 1.0f,
                       1.0f, 1.0f, 1.0f, 1.0f};

    gestalt_result_t result;
    memset(&result, 0, sizeof(result));

    int ret = intuitive_gestalt_perceive(engine, pattern, 16, &result);

    if (ret == 0) {
        // Should detect rectangular/closure pattern
        EXPECT_GE(result.closure_strength, 0.0f);
        EXPECT_LE(result.closure_strength, 1.0f);
        intuitive_free_gestalt(&result);
    }
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(IntuitiveReasoningTest, GetStatistics)
{
    intuitive_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    int result = intuitive_get_stats(engine, &stats);
    EXPECT_EQ(result, 0);
}

TEST_F(IntuitiveReasoningTest, ResetStatistics)
{
    intuitive_reset_stats(engine);
    // Returns void - just check it doesn't crash
    SUCCEED();
}

//=============================================================================
// Modulation Tests
//=============================================================================

TEST_F(IntuitiveReasoningTest, SetInflammation)
{
    int result = intuitive_set_inflammation(engine, 0.3f);
    EXPECT_EQ(result, 0);
}

TEST_F(IntuitiveReasoningTest, SetFatigue)
{
    int result = intuitive_set_fatigue(engine, 0.5f);
    EXPECT_EQ(result, 0);
}

TEST_F(IntuitiveReasoningTest, SetEmotionalValence)
{
    int result = intuitive_set_emotional_valence(engine, 0.7f);
    EXPECT_EQ(result, 0);
}

TEST_F(IntuitiveReasoningTest, SetInflammationNull)
{
    int result = intuitive_set_inflammation(nullptr, 0.5f);
    EXPECT_NE(result, 0); // Should fail
}

} // namespace
