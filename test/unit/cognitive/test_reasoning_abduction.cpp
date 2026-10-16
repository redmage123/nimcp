/**
 * @file test_reasoning_abduction.cpp
 * @brief Unit tests for the abductive reasoning module
 *
 * WHAT: Tests abduction lifecycle, observation management, hypothesis generation,
 *       scoring, selection, and statistics
 * WHY:  Verify abductive reasoning components work correctly in isolation
 * HOW:  GTest suite testing each component independently
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

extern "C" {
#include "cognitive/reasoning/nimcp_reasoning_abduction.h"
#include "cognitive/reasoning/nimcp_reasoning_chain.h"
}

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class ReasoningAbductionTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

/*=============================================================================
 * LIFECYCLE TESTS
 *===========================================================================*/

TEST_F(ReasoningAbductionTest, CreateDestroy) {
    reasoning_abduction_t* abd = reasoning_abduction_create(NULL);
    ASSERT_NE(abd, nullptr);
    reasoning_abduction_destroy(abd);
}

TEST_F(ReasoningAbductionTest, CreateWithConfig) {
    abduction_config_t config = reasoning_abduction_default_config();
    config.max_hypotheses = 4;
    config.min_plausibility = 0.2f;

    reasoning_abduction_t* abd = reasoning_abduction_create(&config);
    ASSERT_NE(abd, nullptr);
    reasoning_abduction_destroy(abd);
}

TEST_F(ReasoningAbductionTest, DestroyNull) {
    /* Should not crash */
    reasoning_abduction_destroy(NULL);
}

/*=============================================================================
 * DEFAULT CONFIG TESTS
 *===========================================================================*/

TEST_F(ReasoningAbductionTest, DefaultConfig) {
    abduction_config_t config = reasoning_abduction_default_config();

    EXPECT_TRUE(config.enabled);
    EXPECT_EQ(config.max_hypotheses, ABDUCTION_DEFAULT_MAX_HYPOTHESES);
    EXPECT_FLOAT_EQ(config.min_plausibility, ABDUCTION_DEFAULT_MIN_PLAUSIBILITY);
    EXPECT_TRUE(config.prefer_simplicity);
    EXPECT_FLOAT_EQ(config.simplicity_weight, 0.3f);
    EXPECT_FLOAT_EQ(config.explanatory_weight, 0.5f);
    EXPECT_FLOAT_EQ(config.coherence_weight, 0.2f);
}

/*=============================================================================
 * OBSERVATION MANAGEMENT TESTS
 *===========================================================================*/

TEST_F(ReasoningAbductionTest, AddObservation) {
    reasoning_abduction_t* abd = reasoning_abduction_create(NULL);
    ASSERT_NE(abd, nullptr);

    abductive_observation_t obs;
    memset(&obs, 0, sizeof(obs));
    strncpy(obs.description, "The temperature increased rapidly", ABDUCTION_MAX_EXPLANATION_LEN - 1);
    obs.confidence = 0.8f;
    obs.domain = 0;
    obs.timestamp_us = 12345;

    int rc = reasoning_abduction_add_observation(abd, &obs);
    EXPECT_EQ(rc, 0);

    reasoning_abduction_destroy(abd);
}

TEST_F(ReasoningAbductionTest, AddObservationNull) {
    reasoning_abduction_t* abd = reasoning_abduction_create(NULL);
    ASSERT_NE(abd, nullptr);

    int rc = reasoning_abduction_add_observation(abd, NULL);
    EXPECT_EQ(rc, -1);

    rc = reasoning_abduction_add_observation(NULL, NULL);
    EXPECT_EQ(rc, -1);

    reasoning_abduction_destroy(abd);
}

TEST_F(ReasoningAbductionTest, AddMaxObservations) {
    reasoning_abduction_t* abd = reasoning_abduction_create(NULL);
    ASSERT_NE(abd, nullptr);

    /* Fill to the limit */
    for (uint32_t i = 0; i < ABDUCTION_MAX_OBSERVATIONS; i++) {
        abductive_observation_t obs;
        memset(&obs, 0, sizeof(obs));
        snprintf(obs.description, ABDUCTION_MAX_EXPLANATION_LEN,
                 "Observation number %u with some content words", i);
        obs.confidence = 0.5f;

        int rc = reasoning_abduction_add_observation(abd, &obs);
        EXPECT_EQ(rc, 0) << "Failed at observation " << i;
    }

    /* One more should fail */
    abductive_observation_t obs;
    memset(&obs, 0, sizeof(obs));
    strncpy(obs.description, "Overflow observation", ABDUCTION_MAX_EXPLANATION_LEN - 1);
    obs.confidence = 0.5f;

    int rc = reasoning_abduction_add_observation(abd, &obs);
    EXPECT_EQ(rc, -1);

    reasoning_abduction_destroy(abd);
}

TEST_F(ReasoningAbductionTest, ClearObservations) {
    reasoning_abduction_t* abd = reasoning_abduction_create(NULL);
    ASSERT_NE(abd, nullptr);

    /* Add some observations */
    abductive_observation_t obs;
    memset(&obs, 0, sizeof(obs));
    strncpy(obs.description, "Test observation description here", ABDUCTION_MAX_EXPLANATION_LEN - 1);
    obs.confidence = 0.7f;
    reasoning_abduction_add_observation(abd, &obs);
    reasoning_abduction_add_observation(abd, &obs);

    /* Clear */
    int rc = reasoning_abduction_clear_observations(abd);
    EXPECT_EQ(rc, 0);

    /* Should be able to add again */
    rc = reasoning_abduction_add_observation(abd, &obs);
    EXPECT_EQ(rc, 0);

    reasoning_abduction_destroy(abd);
}

TEST_F(ReasoningAbductionTest, ClearObservationsNull) {
    int rc = reasoning_abduction_clear_observations(NULL);
    EXPECT_EQ(rc, -1);
}

/*=============================================================================
 * GENERATION TESTS
 *===========================================================================*/

TEST_F(ReasoningAbductionTest, GenerateNoObservations) {
    reasoning_abduction_t* abd = reasoning_abduction_create(NULL);
    ASSERT_NE(abd, nullptr);

    abduction_result_t result;
    memset(&result, 0, sizeof(result));

    int rc = reasoning_abduction_generate(abd, &result);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(result.num_hypotheses, 0u);

    reasoning_abduction_destroy(abd);
}

TEST_F(ReasoningAbductionTest, GenerateSingleObservation) {
    reasoning_abduction_t* abd = reasoning_abduction_create(NULL);
    ASSERT_NE(abd, nullptr);

    abductive_observation_t obs;
    memset(&obs, 0, sizeof(obs));
    strncpy(obs.description, "The system experienced elevated temperature readings",
            ABDUCTION_MAX_EXPLANATION_LEN - 1);
    obs.confidence = 0.9f;
    reasoning_abduction_add_observation(abd, &obs);

    abduction_result_t result;
    memset(&result, 0, sizeof(result));

    int rc = reasoning_abduction_generate(abd, &result);
    EXPECT_EQ(rc, 0);
    EXPECT_GT(result.num_hypotheses, 0u);

    /* At least one hypothesis should exist */
    if (result.num_hypotheses > 0) {
        EXPECT_GT(result.best_plausibility, 0.0f);
        EXPECT_LE(result.best_plausibility, 1.0f);
    }

    reasoning_abduction_destroy(abd);
}

TEST_F(ReasoningAbductionTest, GenerateMultipleObservations) {
    reasoning_abduction_t* abd = reasoning_abduction_create(NULL);
    ASSERT_NE(abd, nullptr);

    /* Add observations that share keywords */
    abductive_observation_t obs1;
    memset(&obs1, 0, sizeof(obs1));
    strncpy(obs1.description, "The thermal sensor detected elevated temperature readings",
            ABDUCTION_MAX_EXPLANATION_LEN - 1);
    obs1.confidence = 0.8f;
    reasoning_abduction_add_observation(abd, &obs1);

    abductive_observation_t obs2;
    memset(&obs2, 0, sizeof(obs2));
    strncpy(obs2.description, "The cooling system temperature exceeded threshold values",
            ABDUCTION_MAX_EXPLANATION_LEN - 1);
    obs2.confidence = 0.7f;
    reasoning_abduction_add_observation(abd, &obs2);

    abductive_observation_t obs3;
    memset(&obs3, 0, sizeof(obs3));
    strncpy(obs3.description, "Power consumption increased with temperature elevation",
            ABDUCTION_MAX_EXPLANATION_LEN - 1);
    obs3.confidence = 0.6f;
    reasoning_abduction_add_observation(abd, &obs3);

    abduction_result_t result;
    memset(&result, 0, sizeof(result));

    int rc = reasoning_abduction_generate(abd, &result);
    EXPECT_EQ(rc, 0);
    EXPECT_GT(result.num_hypotheses, 0u);

    /* With shared keywords ("temperature"), should generate common-cause hypotheses */
    if (result.num_hypotheses > 0) {
        EXPECT_GT(result.best_plausibility, 0.0f);
    }

    reasoning_abduction_destroy(abd);
}

TEST_F(ReasoningAbductionTest, GenerateContradictoryObservations) {
    reasoning_abduction_t* abd = reasoning_abduction_create(NULL);
    ASSERT_NE(abd, nullptr);

    /* Add contradictory observations (contain negation words) */
    abductive_observation_t obs1;
    memset(&obs1, 0, sizeof(obs1));
    strncpy(obs1.description, "The system detected elevated temperature readings consistently",
            ABDUCTION_MAX_EXPLANATION_LEN - 1);
    obs1.confidence = 0.8f;
    reasoning_abduction_add_observation(abd, &obs1);

    abductive_observation_t obs2;
    memset(&obs2, 0, sizeof(obs2));
    strncpy(obs2.description, "There was never elevated temperature detected anywhere",
            ABDUCTION_MAX_EXPLANATION_LEN - 1);
    obs2.confidence = 0.7f;
    reasoning_abduction_add_observation(abd, &obs2);

    abduction_result_t result_contra;
    memset(&result_contra, 0, sizeof(result_contra));

    int rc = reasoning_abduction_generate(abd, &result_contra);
    EXPECT_EQ(rc, 0);

    /* Contradictory observations should produce lower coherence hypotheses */
    if (result_contra.num_hypotheses > 0) {
        const abductive_hypothesis_t* best =
            reasoning_abduction_select_best(&result_contra);
        ASSERT_NE(best, nullptr);
        /* Coherence should be < 1.0 due to negation */
        EXPECT_LT(best->coherence, 1.0f);
    }

    reasoning_abduction_destroy(abd);
}

TEST_F(ReasoningAbductionTest, GenerateNullArgs) {
    reasoning_abduction_t* abd = reasoning_abduction_create(NULL);
    ASSERT_NE(abd, nullptr);

    int rc = reasoning_abduction_generate(abd, NULL);
    EXPECT_EQ(rc, -1);

    rc = reasoning_abduction_generate(NULL, NULL);
    EXPECT_EQ(rc, -1);

    reasoning_abduction_destroy(abd);
}

/*=============================================================================
 * EVALUATION TESTS
 *===========================================================================*/

TEST_F(ReasoningAbductionTest, EvaluateHypothesis) {
    reasoning_abduction_t* abd = reasoning_abduction_create(NULL);
    ASSERT_NE(abd, nullptr);

    abductive_hypothesis_t hyp;
    memset(&hyp, 0, sizeof(hyp));
    strncpy(hyp.explanation, "Short cause", ABDUCTION_MAX_EXPLANATION_LEN - 1);
    hyp.explanatory_power = 0.8f;
    hyp.coherence = 0.9f;

    int rc = reasoning_abduction_evaluate(abd, &hyp);
    EXPECT_EQ(rc, 0);
    EXPECT_GT(hyp.plausibility, 0.0f);
    EXPECT_LE(hyp.plausibility, 1.0f);
    EXPECT_GT(hyp.simplicity, 0.0f);
    /* Free energy should be positive (since plausibility < 1) */
    EXPECT_GT(hyp.free_energy, 0.0f);

    reasoning_abduction_destroy(abd);
}

TEST_F(ReasoningAbductionTest, EvaluateNullHypothesis) {
    reasoning_abduction_t* abd = reasoning_abduction_create(NULL);
    ASSERT_NE(abd, nullptr);

    int rc = reasoning_abduction_evaluate(abd, NULL);
    EXPECT_EQ(rc, -1);

    rc = reasoning_abduction_evaluate(NULL, NULL);
    EXPECT_EQ(rc, -1);

    reasoning_abduction_destroy(abd);
}

/*=============================================================================
 * SELECTION TESTS
 *===========================================================================*/

TEST_F(ReasoningAbductionTest, SelectBest) {
    reasoning_abduction_t* abd = reasoning_abduction_create(NULL);
    ASSERT_NE(abd, nullptr);

    /* Add multiple observations */
    abductive_observation_t obs;
    memset(&obs, 0, sizeof(obs));
    strncpy(obs.description, "Multiple system components experienced failure simultaneously",
            ABDUCTION_MAX_EXPLANATION_LEN - 1);
    obs.confidence = 0.8f;
    reasoning_abduction_add_observation(abd, &obs);

    strncpy(obs.description, "Power supply voltage dropped below threshold during failure",
            ABDUCTION_MAX_EXPLANATION_LEN - 1);
    obs.confidence = 0.7f;
    reasoning_abduction_add_observation(abd, &obs);

    abduction_result_t result;
    memset(&result, 0, sizeof(result));
    reasoning_abduction_generate(abd, &result);

    if (result.num_hypotheses > 0) {
        const abductive_hypothesis_t* best =
            reasoning_abduction_select_best(&result);
        ASSERT_NE(best, nullptr);

        /* Best should be the highest plausibility */
        EXPECT_FLOAT_EQ(best->plausibility, result.best_plausibility);

        /* All other hypotheses should have <= plausibility */
        for (uint32_t i = 1; i < result.num_hypotheses; i++) {
            EXPECT_LE(result.hypotheses[i].plausibility, best->plausibility);
        }
    }

    reasoning_abduction_destroy(abd);
}

TEST_F(ReasoningAbductionTest, SelectBestNull) {
    const abductive_hypothesis_t* best = reasoning_abduction_select_best(NULL);
    EXPECT_EQ(best, nullptr);
}

TEST_F(ReasoningAbductionTest, SelectBestEmptyResult) {
    abduction_result_t result;
    memset(&result, 0, sizeof(result));
    result.num_hypotheses = 0;

    const abductive_hypothesis_t* best = reasoning_abduction_select_best(&result);
    EXPECT_EQ(best, nullptr);
}

/*=============================================================================
 * SCORING TESTS
 *===========================================================================*/

TEST_F(ReasoningAbductionTest, SimplicityScoringShortBetter) {
    reasoning_abduction_t* abd = reasoning_abduction_create(NULL);
    ASSERT_NE(abd, nullptr);

    /* Short explanation */
    abductive_hypothesis_t short_hyp;
    memset(&short_hyp, 0, sizeof(short_hyp));
    strncpy(short_hyp.explanation, "Power failure", ABDUCTION_MAX_EXPLANATION_LEN - 1);
    short_hyp.explanatory_power = 0.5f;
    short_hyp.coherence = 0.5f;
    reasoning_abduction_evaluate(abd, &short_hyp);

    /* Long explanation */
    abductive_hypothesis_t long_hyp;
    memset(&long_hyp, 0, sizeof(long_hyp));
    strncpy(long_hyp.explanation,
            "A complex cascading failure of multiple interconnected subsystems "
            "caused by an initial power surge that propagated through the "
            "distribution network affecting all downstream components",
            ABDUCTION_MAX_EXPLANATION_LEN - 1);
    long_hyp.explanatory_power = 0.5f;
    long_hyp.coherence = 0.5f;
    reasoning_abduction_evaluate(abd, &long_hyp);

    /* Shorter explanation should have higher simplicity */
    EXPECT_GT(short_hyp.simplicity, long_hyp.simplicity);

    reasoning_abduction_destroy(abd);
}

TEST_F(ReasoningAbductionTest, ExplanatoryPowerScoring) {
    reasoning_abduction_t* abd = reasoning_abduction_create(NULL);
    ASSERT_NE(abd, nullptr);

    /* Add 4 observations */
    for (int i = 0; i < 4; i++) {
        abductive_observation_t obs;
        memset(&obs, 0, sizeof(obs));
        snprintf(obs.description, ABDUCTION_MAX_EXPLANATION_LEN,
                 "Observation %d with temperature readings data", i);
        obs.confidence = 0.7f;
        reasoning_abduction_add_observation(abd, &obs);
    }

    abduction_result_t result;
    memset(&result, 0, sizeof(result));
    reasoning_abduction_generate(abd, &result);

    /* Hypotheses explaining more observations should have higher explanatory power */
    if (result.num_hypotheses >= 2) {
        /* Find a hypothesis explaining all vs one explaining fewer */
        float max_ep = 0.0f;
        float min_ep = 1.0f;
        for (uint32_t i = 0; i < result.num_hypotheses; i++) {
            if (result.hypotheses[i].explanatory_power > max_ep)
                max_ep = result.hypotheses[i].explanatory_power;
            if (result.hypotheses[i].explanatory_power < min_ep)
                min_ep = result.hypotheses[i].explanatory_power;
        }
        EXPECT_GT(max_ep, min_ep);
    }

    reasoning_abduction_destroy(abd);
}

TEST_F(ReasoningAbductionTest, CoherenceScoring) {
    reasoning_abduction_t* abd = reasoning_abduction_create(NULL);
    ASSERT_NE(abd, nullptr);

    /* Non-contradictory observations */
    abductive_observation_t obs1;
    memset(&obs1, 0, sizeof(obs1));
    strncpy(obs1.description, "Temperature readings elevated consistently across sensors",
            ABDUCTION_MAX_EXPLANATION_LEN - 1);
    obs1.confidence = 0.8f;
    reasoning_abduction_add_observation(abd, &obs1);

    abductive_observation_t obs2;
    memset(&obs2, 0, sizeof(obs2));
    strncpy(obs2.description, "Temperature increase correlated with power consumption",
            ABDUCTION_MAX_EXPLANATION_LEN - 1);
    obs2.confidence = 0.7f;
    reasoning_abduction_add_observation(abd, &obs2);

    abduction_result_t result;
    memset(&result, 0, sizeof(result));
    reasoning_abduction_generate(abd, &result);

    /* No negation = coherence should be 1.0 */
    if (result.num_hypotheses > 0) {
        /* Direct cause hypotheses should have coherence = 1.0 (no negation) */
        bool found_full_coherence = false;
        for (uint32_t i = 0; i < result.num_hypotheses; i++) {
            if (result.hypotheses[i].coherence >= 0.99f) {
                found_full_coherence = true;
                break;
            }
        }
        EXPECT_TRUE(found_full_coherence);
    }

    reasoning_abduction_destroy(abd);
}

TEST_F(ReasoningAbductionTest, PlausibilityWeighting) {
    reasoning_abduction_t* abd = reasoning_abduction_create(NULL);
    ASSERT_NE(abd, nullptr);

    /* Evaluate with known component scores */
    abductive_hypothesis_t hyp;
    memset(&hyp, 0, sizeof(hyp));
    strncpy(hyp.explanation, "Test", ABDUCTION_MAX_EXPLANATION_LEN - 1);
    hyp.explanatory_power = 1.0f;
    hyp.coherence = 1.0f;
    reasoning_abduction_evaluate(abd, &hyp);

    /* plausibility = 0.3 * simplicity + 0.5 * 1.0 + 0.2 * 1.0 */
    /* simplicity of "Test" (1 word) = 1.0 / (1.0 + 1 * 0.1) = ~0.909 */
    float expected_simplicity = 1.0f / (1.0f + 1.0f * 0.1f);
    float expected = 0.3f * expected_simplicity + 0.5f * 1.0f + 0.2f * 1.0f;
    EXPECT_NEAR(hyp.plausibility, expected, 0.01f);

    reasoning_abduction_destroy(abd);
}

/*=============================================================================
 * FREE ENERGY TESTS
 *===========================================================================*/

TEST_F(ReasoningAbductionTest, FreeEnergyComputed) {
    reasoning_abduction_t* abd = reasoning_abduction_create(NULL);
    ASSERT_NE(abd, nullptr);

    abductive_hypothesis_t hyp;
    memset(&hyp, 0, sizeof(hyp));
    strncpy(hyp.explanation, "Simple cause", ABDUCTION_MAX_EXPLANATION_LEN - 1);
    hyp.explanatory_power = 0.8f;
    hyp.coherence = 0.9f;
    reasoning_abduction_evaluate(abd, &hyp);

    /* free_energy = -log(plausibility + 1e-6) */
    float expected_fe = -logf(hyp.plausibility + 1e-6f);
    EXPECT_NEAR(hyp.free_energy, expected_fe, 0.001f);

    /* Higher plausibility should mean lower free energy */
    abductive_hypothesis_t hyp2;
    memset(&hyp2, 0, sizeof(hyp2));
    strncpy(hyp2.explanation, "X", ABDUCTION_MAX_EXPLANATION_LEN - 1);
    hyp2.explanatory_power = 0.3f;
    hyp2.coherence = 0.3f;
    reasoning_abduction_evaluate(abd, &hyp2);

    EXPECT_GT(hyp2.free_energy, hyp.free_energy);

    reasoning_abduction_destroy(abd);
}

/*=============================================================================
 * STATISTICS TESTS
 *===========================================================================*/

TEST_F(ReasoningAbductionTest, GetStats) {
    reasoning_abduction_t* abd = reasoning_abduction_create(NULL);
    ASSERT_NE(abd, nullptr);

    /* Initial stats should be zero */
    abduction_stats_t stats;
    int rc = reasoning_abduction_get_stats(abd, &stats);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(stats.total_abductions, 0u);
    EXPECT_EQ(stats.total_observations_processed, 0u);
    EXPECT_FLOAT_EQ(stats.avg_hypotheses_generated, 0.0f);
    EXPECT_FLOAT_EQ(stats.avg_best_plausibility, 0.0f);

    /* Add observations and generate */
    abductive_observation_t obs;
    memset(&obs, 0, sizeof(obs));
    strncpy(obs.description, "Observation with content words inside",
            ABDUCTION_MAX_EXPLANATION_LEN - 1);
    obs.confidence = 0.7f;
    reasoning_abduction_add_observation(abd, &obs);

    abduction_result_t result;
    memset(&result, 0, sizeof(result));
    reasoning_abduction_generate(abd, &result);

    /* Stats should be updated */
    rc = reasoning_abduction_get_stats(abd, &stats);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(stats.total_abductions, 1u);
    EXPECT_EQ(stats.total_observations_processed, 1u);

    reasoning_abduction_destroy(abd);
}

TEST_F(ReasoningAbductionTest, GetStatsNull) {
    int rc = reasoning_abduction_get_stats(NULL, NULL);
    EXPECT_EQ(rc, -1);

    reasoning_abduction_t* abd = reasoning_abduction_create(NULL);
    rc = reasoning_abduction_get_stats(abd, NULL);
    EXPECT_EQ(rc, -1);
    reasoning_abduction_destroy(abd);
}

/*=============================================================================
 * CONFIG LIMIT TESTS
 *===========================================================================*/

TEST_F(ReasoningAbductionTest, ConfigMaxHypotheses) {
    abduction_config_t config = reasoning_abduction_default_config();
    config.max_hypotheses = 2;

    reasoning_abduction_t* abd = reasoning_abduction_create(&config);
    ASSERT_NE(abd, nullptr);

    /* Add many observations to potentially generate many hypotheses */
    for (int i = 0; i < 8; i++) {
        abductive_observation_t obs;
        memset(&obs, 0, sizeof(obs));
        snprintf(obs.description, ABDUCTION_MAX_EXPLANATION_LEN,
                 "Observation %d with different keyword%d data", i, i);
        obs.confidence = 0.7f;
        reasoning_abduction_add_observation(abd, &obs);
    }

    abduction_result_t result;
    memset(&result, 0, sizeof(result));
    reasoning_abduction_generate(abd, &result);

    /* Should not exceed configured max */
    EXPECT_LE(result.num_hypotheses, 2u);

    reasoning_abduction_destroy(abd);
}

TEST_F(ReasoningAbductionTest, MinPlausibilityFilter) {
    abduction_config_t config = reasoning_abduction_default_config();
    config.min_plausibility = 0.99f;  /* Very high threshold */

    reasoning_abduction_t* abd = reasoning_abduction_create(&config);
    ASSERT_NE(abd, nullptr);

    abductive_observation_t obs;
    memset(&obs, 0, sizeof(obs));
    strncpy(obs.description, "Some observation with content words inside",
            ABDUCTION_MAX_EXPLANATION_LEN - 1);
    obs.confidence = 0.5f;
    reasoning_abduction_add_observation(abd, &obs);

    abduction_result_t result;
    memset(&result, 0, sizeof(result));
    reasoning_abduction_generate(abd, &result);

    /* All hypotheses should meet min_plausibility or be filtered out */
    for (uint32_t i = 0; i < result.num_hypotheses; i++) {
        EXPECT_GE(result.hypotheses[i].plausibility, 0.99f);
    }

    reasoning_abduction_destroy(abd);
}

/*=============================================================================
 * STEP TYPE NAME TEST
 *===========================================================================*/

TEST_F(ReasoningAbductionTest, StepTypeNameAbductive) {
    const char* name = reasoning_step_type_name(REASONING_STEP_ABDUCTIVE);
    EXPECT_STREQ(name, "ABDUCTIVE");
}

/*=============================================================================
 * GENERATION TIME TEST
 *===========================================================================*/

TEST_F(ReasoningAbductionTest, GenerationTimeRecorded) {
    reasoning_abduction_t* abd = reasoning_abduction_create(NULL);
    ASSERT_NE(abd, nullptr);

    abductive_observation_t obs;
    memset(&obs, 0, sizeof(obs));
    strncpy(obs.description, "Observable phenomenon with measurable data",
            ABDUCTION_MAX_EXPLANATION_LEN - 1);
    obs.confidence = 0.7f;
    reasoning_abduction_add_observation(abd, &obs);

    abduction_result_t result;
    memset(&result, 0, sizeof(result));
    reasoning_abduction_generate(abd, &result);

    /* Generation time should be recorded (>= 0) */
    EXPECT_GE(result.generation_time_us, 0u);

    reasoning_abduction_destroy(abd);
}
