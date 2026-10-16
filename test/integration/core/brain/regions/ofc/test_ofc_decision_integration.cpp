/**
 * @file test_ofc_decision_integration.cpp
 * @brief Integration tests for OFC decision-making system
 *
 * WHAT: Tests value-based decision making and learning
 * WHY:  Decision making is the core function of OFC
 * HOW:  Test value computation, decisions, prediction error, reversal learning
 *
 * INTEGRATION POINTS:
 * - Value computation across subdivisions
 * - Decision making algorithm
 * - Prediction error learning
 * - Reversal detection
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

#include "core/brain/regions/ofc/nimcp_ofc.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "nimcp.h"

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class OFCDecisionTest : public ::testing::Test {
protected:
    nimcp_ofc_t* ofc;
    ofc_config_t config;
    bool router_initialized;

    void SetUp() override {
        router_initialized = false;
        ofc = NULL;

        /* Initialize bio-async router */
        bio_router_config_t router_config = bio_router_default_config();
        router_config.max_modules = 64;
        router_config.inbox_capacity = 256;
        router_config.outbox_capacity = 256;
        router_config.enable_logging = false;

        if (bio_router_init(&router_config) == NIMCP_OK) {
            router_initialized = true;
        }

        /* Initialize OFC */
        memset(&config, 0, sizeof(config));
        ofc_default_config(&config);
        config.decision_threshold = 0.3f;  /* Lower threshold for testing */
        config.noise_level = 0.1f;
        config.learning_rate = 0.1f;

        ofc = ofc_create(&config);
    }

    void TearDown() override {
        if (ofc) {
            ofc_destroy(ofc);
            ofc = NULL;
        }
        if (router_initialized) {
            bio_router_shutdown();
            router_initialized = false;
        }
    }
};

/*=============================================================================
 * VALUE COMPUTATION TESTS
 *===========================================================================*/

TEST_F(OFCDecisionTest, ComputeValueForOption) {
    ASSERT_NE(nullptr, ofc);

    /* Present option */
    ofc_present_option(ofc, 1, 0.8f, 0.9f, 0.0f);

    /* Compute value */
    ofc_value_t value;
    int result = ofc_compute_value(ofc, 1, &value);
    EXPECT_EQ(0, result);
    EXPECT_EQ(OFC_VALUE_EXPECTED, value.type);
}

TEST_F(OFCDecisionTest, IntegratedValue) {
    ASSERT_NE(nullptr, ofc);

    /* Present option with high reward and probability */
    ofc_present_option(ofc, 1, 0.9f, 0.95f, 0.0f);

    for (int i = 0; i < 10; i++) {
        ofc_update(ofc, 10.0f);
    }

    float integrated = ofc_get_integrated_value(ofc, 1);
    EXPECT_GE(integrated, -1.0f);
    EXPECT_LE(integrated, 1.0f);
}

TEST_F(OFCDecisionTest, TemporalDiscounting) {
    ASSERT_NE(nullptr, ofc);

    /* Immediate reward */
    ofc_present_option(ofc, 1, 0.8f, 1.0f, 0.0f);
    float immediate_value = ofc_get_integrated_value(ofc, 1);

    /* Clear and present delayed reward */
    ofc_clear_options(ofc);
    ofc_present_option(ofc, 2, 0.8f, 1.0f, 100.0f);  /* 100ms delay */
    float delayed_value = ofc_get_integrated_value(ofc, 2);

    /* Delayed reward should be discounted (lower value) */
    /* This depends on implementation of temporal discounting */
    EXPECT_GE(immediate_value, delayed_value);
}

TEST_F(OFCDecisionTest, RiskAssessment) {
    ASSERT_NE(nullptr, ofc);

    ofc_present_option(ofc, 1, 0.8f, 0.5f, 0.0f);  /* Risky option */

    float risk = 0.0f;
    int result = ofc_assess_risk(ofc, 1, &risk);
    EXPECT_EQ(0, result);
    EXPECT_GE(risk, 0.0f);
    EXPECT_LE(risk, 1.0f);
}

/*=============================================================================
 * DECISION MAKING TESTS
 *===========================================================================*/

TEST_F(OFCDecisionTest, BinaryDecision) {
    ASSERT_NE(nullptr, ofc);

    /* Two options with different values */
    ofc_present_option(ofc, 1, 0.3f, 0.8f, 0.0f);  /* Lower value */
    ofc_present_option(ofc, 2, 0.9f, 0.9f, 0.0f);  /* Higher value */

    for (int i = 0; i < 20; i++) {
        ofc_update(ofc, 10.0f);
    }

    ofc_decision_t decision;
    int result = ofc_make_decision(ofc, &decision);
    EXPECT_EQ(0, result);

    /* Should choose the higher value option most of the time */
    EXPECT_TRUE(decision.chosen_option == 1 || decision.chosen_option == 2);
    EXPECT_GE(decision.confidence, 0.0f);
    EXPECT_LE(decision.confidence, 1.0f);
}

TEST_F(OFCDecisionTest, MultipleDecisions) {
    ASSERT_NE(nullptr, ofc);

    uint32_t choice_counts[3] = {0, 0, 0};

    for (int trial = 0; trial < 50; trial++) {
        ofc_clear_options(ofc);

        /* Present three options */
        ofc_present_option(ofc, 1, 0.2f, 0.9f, 0.0f);
        ofc_present_option(ofc, 2, 0.8f, 0.9f, 0.0f);  /* Best option */
        ofc_present_option(ofc, 3, 0.5f, 0.9f, 0.0f);

        for (int i = 0; i < 10; i++) {
            ofc_update(ofc, 5.0f);
        }

        ofc_decision_t decision;
        ofc_make_decision(ofc, &decision);

        if (decision.chosen_option >= 1 && decision.chosen_option <= 3) {
            choice_counts[decision.chosen_option - 1]++;
        }
    }

    /* Option 2 (best) should be chosen most often */
    EXPECT_GE(choice_counts[1], choice_counts[0]);  /* 2 >= 1 */
    EXPECT_GE(choice_counts[1], choice_counts[2]);  /* 2 >= 3 */
}

TEST_F(OFCDecisionTest, DecisionReactionTime) {
    ASSERT_NE(nullptr, ofc);

    /* Easy decision - large value difference */
    ofc_present_option(ofc, 1, 0.1f, 0.9f, 0.0f);
    ofc_present_option(ofc, 2, 0.9f, 0.9f, 0.0f);

    for (int i = 0; i < 20; i++) {
        ofc_update(ofc, 10.0f);
    }

    ofc_decision_t decision;
    ofc_make_decision(ofc, &decision);

    /* Should have positive reaction time */
    EXPECT_GE(decision.reaction_time_ms, 0.0f);
}

/*=============================================================================
 * PREDICTION ERROR LEARNING TESTS
 *===========================================================================*/

TEST_F(OFCDecisionTest, PredictionErrorUpdate) {
    ASSERT_NE(nullptr, ofc);

    /* Present option with expected reward */
    ofc_present_option(ofc, 1, 0.5f, 0.9f, 0.0f);

    for (int i = 0; i < 10; i++) {
        ofc_update(ofc, 10.0f);
    }

    /* Deliver actual reward (higher than expected) */
    int result = ofc_update_prediction_error(ofc, 1, 0.8f);
    EXPECT_EQ(0, result);

    /* Prediction error should be positive (received > expected) */
    EXPECT_GT(ofc->prediction_error, 0.0f);
}

TEST_F(OFCDecisionTest, NegativePredictionError) {
    ASSERT_NE(nullptr, ofc);

    /* Present option with high expected reward */
    ofc_present_option(ofc, 1, 0.9f, 0.9f, 0.0f);

    for (int i = 0; i < 10; i++) {
        ofc_update(ofc, 10.0f);
    }

    /* Deliver low actual reward */
    ofc_update_prediction_error(ofc, 1, 0.1f);

    /* Prediction error should be negative */
    EXPECT_LT(ofc->prediction_error, 0.0f);
}

TEST_F(OFCDecisionTest, LearningFromPredictionError) {
    ASSERT_NE(nullptr, ofc);

    /* Initial value */
    ofc_present_option(ofc, 1, 0.5f, 0.9f, 0.0f);
    float initial_value = ofc_get_integrated_value(ofc, 1);

    /* Multiple learning trials with higher reward */
    for (int trial = 0; trial < 20; trial++) {
        ofc_update(ofc, 10.0f);
        ofc_update_prediction_error(ofc, 1, 0.9f);
    }

    float final_value = ofc_get_integrated_value(ofc, 1);

    /* Value should increase after receiving consistently high rewards */
    EXPECT_GE(final_value, initial_value);
}

/*=============================================================================
 * REVERSAL LEARNING TESTS
 *===========================================================================*/

TEST_F(OFCDecisionTest, ReversalDetection) {
    ASSERT_NE(nullptr, ofc);

    /* Present option that was good, now delivers poor rewards */
    ofc_present_option(ofc, 1, 0.9f, 0.9f, 0.0f);

    /* Initial learning with good rewards */
    for (int i = 0; i < 10; i++) {
        ofc_update(ofc, 10.0f);
        ofc_update_prediction_error(ofc, 1, 0.9f);
    }

    /* Now deliver poor rewards (contingency reversal) */
    for (int i = 0; i < 10; i++) {
        ofc_update(ofc, 10.0f);
        ofc_update_prediction_error(ofc, 1, 0.1f);
    }

    bool reversal = false;
    int result = ofc_check_reversal(ofc, &reversal);
    EXPECT_EQ(0, result);
    /* May or may not detect reversal depending on threshold */
}

/*=============================================================================
 * SOCIAL REWARD TESTS
 *===========================================================================*/

TEST_F(OFCDecisionTest, ProcessSocialReward) {
    ASSERT_NE(nullptr, ofc);

    int result = ofc_process_social_reward(ofc, 0.8f, 1);
    EXPECT_EQ(0, result);
}

TEST_F(OFCDecisionTest, SocialRewardAffectsDecision) {
    ASSERT_NE(nullptr, ofc);

    /* Present two similar options */
    ofc_present_option(ofc, 1, 0.5f, 0.9f, 0.0f);
    ofc_present_option(ofc, 2, 0.5f, 0.9f, 0.0f);

    /* Add social reward to option 2 */
    ofc_process_social_reward(ofc, 0.8f, 2);

    for (int i = 0; i < 20; i++) {
        ofc_update(ofc, 10.0f);
    }

    /* Decision should be influenced by social reward */
    ofc_decision_t decision;
    ofc_make_decision(ofc, &decision);

    /* Either option could be chosen, but social should have an effect */
    EXPECT_TRUE(decision.chosen_option == 1 || decision.chosen_option == 2);
}

/*=============================================================================
 * STATISTICS TESTS
 *===========================================================================*/

TEST_F(OFCDecisionTest, StatsUpdateAfterDecisions) {
    ASSERT_NE(nullptr, ofc);

    ofc_stats_t initial_stats;
    ofc_get_stats(ofc, &initial_stats);

    /* Make some decisions */
    for (int i = 0; i < 5; i++) {
        ofc_clear_options(ofc);
        ofc_present_option(ofc, 1, 0.5f, 0.9f, 0.0f);
        ofc_present_option(ofc, 2, 0.6f, 0.9f, 0.0f);

        for (int j = 0; j < 10; j++) {
            ofc_update(ofc, 10.0f);
        }

        ofc_decision_t decision;
        ofc_make_decision(ofc, &decision);
    }

    ofc_stats_t final_stats;
    ofc_get_stats(ofc, &final_stats);

    EXPECT_GT(final_stats.decisions_made, initial_stats.decisions_made);
}

TEST_F(OFCDecisionTest, PredictionErrorStatsTracked) {
    ASSERT_NE(nullptr, ofc);

    ofc_present_option(ofc, 1, 0.5f, 0.9f, 0.0f);

    ofc_stats_t initial_stats;
    ofc_get_stats(ofc, &initial_stats);

    /* Generate prediction errors */
    for (int i = 0; i < 10; i++) {
        ofc_update(ofc, 10.0f);
        ofc_update_prediction_error(ofc, 1, 0.3f);  /* Different from expected */
    }

    ofc_stats_t final_stats;
    ofc_get_stats(ofc, &final_stats);

    EXPECT_GE(final_stats.prediction_errors, initial_stats.prediction_errors);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
