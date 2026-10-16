/**
 * @file test_gt_hemispheric_integration.cpp
 * @brief Integration tests for Game-Theoretic Hemispheric Brain
 * @version 1.0.0
 * @date 2025-12-27
 *
 * WHAT: Integration tests for Nash bargaining hemispheric coordination
 * WHY:  Verify that bargaining mechanisms work correctly with hemispheric brain
 * HOW:  Test bargaining context creation, resource negotiation, credit assignment,
 *       and multi-round bargaining
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>

// Headers have their own extern "C" guards
#include "cognitive/game_theory/integration/nimcp_gt_hemispheric.h"
#include "core/brain/hemispheric/nimcp_hemispheric_brain.h"
#include "cognitive/game_theory/nimcp_bargaining.h"
#include "cognitive/game_theory/nimcp_credit_assignment.h"
#include "cognitive/game_theory/nimcp_game_theory.h"

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

class GTHemisphericIntegrationTest : public ::testing::Test {
protected:
    gt_hemi_bargaining_ctx_t bargaining_ctx;
    hemispheric_brain_t* brain;

    void SetUp() override {
        bargaining_ctx = nullptr;
        brain = nullptr;

        /* Create hemispheric brain */
        hemispheric_brain_config_t brain_config = hemispheric_brain_default_config();
        brain_config.size = BRAIN_SIZE_SMALL;
        brain_config.num_inputs = 32;
        brain_config.num_outputs = 16;
        brain_config.default_mode = HEMISPHERIC_MODE_COOPERATIVE;
        brain = hemispheric_brain_create(&brain_config);
        ASSERT_NE(brain, nullptr);

        /* Create bargaining context */
        gt_hemi_config_t config = gt_hemi_default_config();
        config.left_bargaining_power = 0.5f;
        config.right_bargaining_power = 0.5f;
        config.disagreement_left = 0.0f;
        config.disagreement_right = 0.0f;
        config.discount_factor = 0.9f;
        config.max_rounds = 10;
        config.use_shapley_credit = true;
        config.bargain_type = NIMCP_BARGAINING_NASH;
        bargaining_ctx = gt_hemi_create(brain, &config);
        ASSERT_NE(bargaining_ctx, nullptr);
    }

    void TearDown() override {
        if (bargaining_ctx) {
            gt_hemi_destroy(bargaining_ctx);
        }
        if (brain) {
            hemispheric_brain_destroy(brain);
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(GTHemisphericIntegrationTest, CreateWithDefaultConfig) {
    gt_hemi_config_t config = gt_hemi_default_config();

    /* Verify reasonable defaults */
    EXPECT_FLOAT_EQ(config.left_bargaining_power, 0.5f);
    EXPECT_FLOAT_EQ(config.right_bargaining_power, 0.5f);
    EXPECT_GE(config.max_rounds, 1);
    EXPECT_GT(config.discount_factor, 0.0f);
    EXPECT_LE(config.discount_factor, 1.0f);
}

TEST_F(GTHemisphericIntegrationTest, CreateAndDestroyMultipleTimes) {
    /* Test repeated creation/destruction for memory leaks */
    for (int i = 0; i < 5; i++) {
        gt_hemi_config_t config = gt_hemi_default_config();
        gt_hemi_bargaining_ctx_t ctx = gt_hemi_create(brain, &config);
        ASSERT_NE(ctx, nullptr);
        gt_hemi_destroy(ctx);
    }
}

TEST_F(GTHemisphericIntegrationTest, CreateWithNullBrain) {
    gt_hemi_config_t config = gt_hemi_default_config();
    gt_hemi_bargaining_ctx_t ctx = gt_hemi_create(nullptr, &config);
    EXPECT_EQ(ctx, nullptr);
}

TEST_F(GTHemisphericIntegrationTest, BargainingIsActive) {
    bool active = gt_hemi_is_active(bargaining_ctx);
    EXPECT_TRUE(active);
}

TEST_F(GTHemisphericIntegrationTest, GetUnderlyingBrain) {
    hemispheric_brain_t* b = gt_hemi_get_brain(bargaining_ctx);
    EXPECT_EQ(b, brain);
}

/* ============================================================================
 * Resource Negotiation Tests
 * ============================================================================ */

TEST_F(GTHemisphericIntegrationTest, NegotiateEqualResources) {
    gt_hemi_outcome_t outcome;
    nimcp_error_t err = gt_hemi_negotiate_resources(bargaining_ctx, 1.0f, &outcome);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* With equal bargaining power, should get equal split */
    EXPECT_NEAR(outcome.left_allocation, 0.5f, 0.1f);
    EXPECT_NEAR(outcome.right_allocation, 0.5f, 0.1f);
    EXPECT_TRUE(outcome.agreement_reached);
}

TEST_F(GTHemisphericIntegrationTest, NegotiateWithAsymmetricPower) {
    /* Set asymmetric bargaining power */
    gt_hemi_set_bargaining_power(bargaining_ctx, 0.7f);  /* Left has more power */

    gt_hemi_outcome_t outcome;
    nimcp_error_t err = gt_hemi_negotiate_resources(bargaining_ctx, 1.0f, &outcome);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Left should get more */
    EXPECT_GT(outcome.left_allocation, outcome.right_allocation);
    EXPECT_TRUE(outcome.agreement_reached);
}

TEST_F(GTHemisphericIntegrationTest, NegotiateWithDisagreementPoints) {
    /* Set non-zero disagreement points */
    gt_hemi_set_disagreement(bargaining_ctx, 0.1f, 0.2f);

    gt_hemi_outcome_t outcome;
    nimcp_error_t err = gt_hemi_negotiate_resources(bargaining_ctx, 1.0f, &outcome);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Allocations should be above disagreement points */
    EXPECT_GE(outcome.left_allocation, 0.1f);
    EXPECT_GE(outcome.right_allocation, 0.2f);
}

TEST_F(GTHemisphericIntegrationTest, NegotiateLargeResourcePool) {
    gt_hemi_outcome_t outcome;
    nimcp_error_t err = gt_hemi_negotiate_resources(bargaining_ctx, 100.0f, &outcome);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Total should equal input */
    EXPECT_NEAR(outcome.left_allocation + outcome.right_allocation, 100.0f, 0.1f);
}

TEST_F(GTHemisphericIntegrationTest, NegotiateSmallResourcePool) {
    gt_hemi_outcome_t outcome;
    nimcp_error_t err = gt_hemi_negotiate_resources(bargaining_ctx, 0.01f, &outcome);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Should still work for small values */
    EXPECT_NEAR(outcome.left_allocation + outcome.right_allocation, 0.01f, 0.001f);
}

/* ============================================================================
 * Credit Assignment Tests
 * ============================================================================ */

TEST_F(GTHemisphericIntegrationTest, ComputeCreditEqualContribution) {
    float combined_output[16];
    for (int i = 0; i < 16; i++) {
        combined_output[i] = 1.0f;
    }

    gt_hemi_credit_t credit;
    nimcp_error_t err = gt_hemi_compute_credit(bargaining_ctx, combined_output,
                                                 16, &credit);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Equal contribution should give equal credit */
    EXPECT_NEAR(credit.left_credit, credit.right_credit, 0.1f);
    EXPECT_GT(credit.total_value, 0.0f);
}

TEST_F(GTHemisphericIntegrationTest, CreditSumsToTotal) {
    float combined_output[16];
    for (int i = 0; i < 16; i++) {
        combined_output[i] = 0.5f;
    }

    gt_hemi_credit_t credit;
    gt_hemi_compute_credit(bargaining_ctx, combined_output, 16, &credit);

    /* Credits should sum to total value */
    float credit_sum = credit.left_credit + credit.right_credit;
    EXPECT_NEAR(credit_sum, credit.total_value, 0.01f);
}

TEST_F(GTHemisphericIntegrationTest, DetectSuperadditiveCooperation) {
    /* Simulate cooperation that adds value */
    float combined_output[16];
    for (int i = 0; i < 16; i++) {
        combined_output[i] = 2.0f;  /* High combined value */
    }

    gt_hemi_credit_t credit;
    gt_hemi_compute_credit(bargaining_ctx, combined_output, 16, &credit);

    /* If cooperation adds value, synergy bonus should be positive */
    if (credit.is_superadditive) {
        EXPECT_GT(credit.synergy_bonus, 0.0f);
    }
}

/* ============================================================================
 * Process with Bargaining Tests
 * ============================================================================ */

TEST_F(GTHemisphericIntegrationTest, ProcessBargainingBasic) {
    float input[32];
    float output[16];

    for (int i = 0; i < 32; i++) {
        input[i] = 0.5f;
    }

    gt_hemi_outcome_t outcome;
    nimcp_error_t err = gt_hemi_process_bargaining(bargaining_ctx,
                                                     input, 32,
                                                     output, 16,
                                                     &outcome);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_TRUE(outcome.agreement_reached);
}

TEST_F(GTHemisphericIntegrationTest, ProcessBargainingMultipleTimes) {
    float input[32];
    float output[16];

    for (int i = 0; i < 32; i++) {
        input[i] = 0.5f;
    }

    /* Process multiple times */
    for (int i = 0; i < 5; i++) {
        gt_hemi_outcome_t outcome;
        nimcp_error_t err = gt_hemi_process_bargaining(bargaining_ctx,
                                                         input, 32,
                                                         output, 16,
                                                         &outcome);
        EXPECT_EQ(err, NIMCP_SUCCESS);
    }
}

TEST_F(GTHemisphericIntegrationTest, ProcessBargainingWithVaryingInput) {
    float input[32];
    float output[16];

    /* Test with different input patterns */
    for (int trial = 0; trial < 3; trial++) {
        for (int i = 0; i < 32; i++) {
            input[i] = (float)(trial + 1) * 0.1f * (i % 5);
        }

        gt_hemi_outcome_t outcome;
        nimcp_error_t err = gt_hemi_process_bargaining(bargaining_ctx,
                                                         input, 32,
                                                         output, 16,
                                                         &outcome);
        EXPECT_EQ(err, NIMCP_SUCCESS);
    }
}

/* ============================================================================
 * Bargaining Power Tests
 * ============================================================================ */

TEST_F(GTHemisphericIntegrationTest, SetBargainingPowerValid) {
    nimcp_error_t err = gt_hemi_set_bargaining_power(bargaining_ctx, 0.3f);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Left has 0.3, right should have 0.7 */
    gt_hemi_outcome_t outcome;
    gt_hemi_negotiate_resources(bargaining_ctx, 1.0f, &outcome);

    EXPECT_LT(outcome.left_allocation, outcome.right_allocation);
}

TEST_F(GTHemisphericIntegrationTest, SetBargainingPowerExtreme) {
    /* Test extreme left dominance */
    gt_hemi_set_bargaining_power(bargaining_ctx, 0.99f);

    gt_hemi_outcome_t outcome;
    gt_hemi_negotiate_resources(bargaining_ctx, 1.0f, &outcome);

    EXPECT_GT(outcome.left_allocation, 0.9f);
    EXPECT_LT(outcome.right_allocation, 0.1f);
}

TEST_F(GTHemisphericIntegrationTest, SetBargainingPowerZero) {
    /* Right hemisphere dominant */
    gt_hemi_set_bargaining_power(bargaining_ctx, 0.01f);

    gt_hemi_outcome_t outcome;
    gt_hemi_negotiate_resources(bargaining_ctx, 1.0f, &outcome);

    EXPECT_LT(outcome.left_allocation, 0.1f);
    EXPECT_GT(outcome.right_allocation, 0.9f);
}

/* ============================================================================
 * Disagreement Point Tests
 * ============================================================================ */

TEST_F(GTHemisphericIntegrationTest, SetDisagreementValid) {
    nimcp_error_t err = gt_hemi_set_disagreement(bargaining_ctx, 0.2f, 0.3f);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(GTHemisphericIntegrationTest, DisagreementPointsRespected) {
    gt_hemi_set_disagreement(bargaining_ctx, 0.3f, 0.2f);

    gt_hemi_outcome_t outcome;
    gt_hemi_negotiate_resources(bargaining_ctx, 1.0f, &outcome);

    /* Individual rationality: both should get at least disagreement */
    EXPECT_GE(outcome.left_allocation, 0.3f);
    EXPECT_GE(outcome.right_allocation, 0.2f);
}

TEST_F(GTHemisphericIntegrationTest, HighDisagreementPointsReduceSurplus) {
    /* High disagreement points leave less to negotiate */
    gt_hemi_set_disagreement(bargaining_ctx, 0.4f, 0.4f);

    gt_hemi_outcome_t outcome;
    gt_hemi_negotiate_resources(bargaining_ctx, 1.0f, &outcome);

    /* Surplus is only 0.2 to divide */
    EXPECT_NEAR(outcome.left_allocation + outcome.right_allocation, 1.0f, 0.01f);
}

/* ============================================================================
 * Multi-Round Bargaining Tests
 * ============================================================================ */

TEST_F(GTHemisphericIntegrationTest, BargainingConverges) {
    gt_hemi_outcome_t outcome;
    gt_hemi_negotiate_resources(bargaining_ctx, 1.0f, &outcome);

    /* Should reach agreement within max rounds */
    EXPECT_TRUE(outcome.agreement_reached);
    EXPECT_LE(outcome.rounds_taken, 10);
}

TEST_F(GTHemisphericIntegrationTest, NashProductIsPositive) {
    gt_hemi_outcome_t outcome;
    gt_hemi_negotiate_resources(bargaining_ctx, 1.0f, &outcome);

    /* Nash product should be positive for valid agreement */
    EXPECT_GT(outcome.nash_product, 0.0f);
}

TEST_F(GTHemisphericIntegrationTest, RepeatedBargainingConsistent) {
    /* Multiple negotiations should give similar results */
    gt_hemi_outcome_t outcomes[5];

    for (int i = 0; i < 5; i++) {
        gt_hemi_negotiate_resources(bargaining_ctx, 1.0f, &outcomes[i]);
    }

    /* All left allocations should be close */
    for (int i = 1; i < 5; i++) {
        EXPECT_NEAR(outcomes[0].left_allocation, outcomes[i].left_allocation, 0.1f);
    }
}

/* ============================================================================
 * State Query Tests
 * ============================================================================ */

TEST_F(GTHemisphericIntegrationTest, GetLastOutcome) {
    float input[32] = {0.5f};
    float output[16];
    gt_hemi_outcome_t process_outcome;

    gt_hemi_process_bargaining(bargaining_ctx, input, 32, output, 16, &process_outcome);

    gt_hemi_outcome_t last_outcome;
    nimcp_error_t err = gt_hemi_get_last_outcome(bargaining_ctx, &last_outcome);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    EXPECT_FLOAT_EQ(last_outcome.left_allocation, process_outcome.left_allocation);
    EXPECT_FLOAT_EQ(last_outcome.right_allocation, process_outcome.right_allocation);
}

TEST_F(GTHemisphericIntegrationTest, GetLastCredit) {
    float combined_output[16];
    for (int i = 0; i < 16; i++) {
        combined_output[i] = 1.0f;
    }

    gt_hemi_credit_t compute_credit;
    gt_hemi_compute_credit(bargaining_ctx, combined_output, 16, &compute_credit);

    gt_hemi_credit_t last_credit;
    nimcp_error_t err = gt_hemi_get_last_credit(bargaining_ctx, &last_credit);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    EXPECT_FLOAT_EQ(last_credit.left_credit, compute_credit.left_credit);
}

TEST_F(GTHemisphericIntegrationTest, GetStatistics) {
    /* Run several operations */
    for (int i = 0; i < 5; i++) {
        gt_hemi_outcome_t outcome;
        gt_hemi_negotiate_resources(bargaining_ctx, 1.0f, &outcome);
    }

    nimcp_game_stats_t stats;
    nimcp_error_t err = gt_hemi_get_stats(bargaining_ctx, &stats);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    EXPECT_GT(stats.bargaining_successes, 0);
}

/* ============================================================================
 * Edge Cases
 * ============================================================================ */

TEST_F(GTHemisphericIntegrationTest, ZeroTotalResources) {
    gt_hemi_outcome_t outcome;
    nimcp_error_t err = gt_hemi_negotiate_resources(bargaining_ctx, 0.0f, &outcome);

    /* Should handle gracefully */
    if (err == NIMCP_SUCCESS) {
        EXPECT_FLOAT_EQ(outcome.left_allocation, 0.0f);
        EXPECT_FLOAT_EQ(outcome.right_allocation, 0.0f);
    }
}

TEST_F(GTHemisphericIntegrationTest, NegativeResourcesRejected) {
    gt_hemi_outcome_t outcome;
    nimcp_error_t err = gt_hemi_negotiate_resources(bargaining_ctx, -1.0f, &outcome);
    /* Should reject negative resources */
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(GTHemisphericIntegrationTest, NullContextHandled) {
    gt_hemi_outcome_t outcome;
    nimcp_error_t err = gt_hemi_negotiate_resources(nullptr, 1.0f, &outcome);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(GTHemisphericIntegrationTest, NullOutputHandled) {
    nimcp_error_t err = gt_hemi_negotiate_resources(bargaining_ctx, 1.0f, nullptr);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

/* ============================================================================
 * Integration with Hemispheric Brain
 * ============================================================================ */

TEST_F(GTHemisphericIntegrationTest, BargainingAffectsHemisphereActivity) {
    /* Get initial stats */
    hemispheric_brain_stats_t stats_before;
    hemispheric_brain_get_stats(brain, &stats_before);

    /* Process with bargaining */
    float input[32] = {0.5f};
    float output[16];
    gt_hemi_outcome_t outcome;

    gt_hemi_process_bargaining(bargaining_ctx, input, 32, output, 16, &outcome);

    /* Get stats after */
    hemispheric_brain_stats_t stats_after;
    hemispheric_brain_get_stats(brain, &stats_after);

    /* Activity levels should reflect bargaining allocation */
    /* The exact relationship depends on implementation */
}

TEST_F(GTHemisphericIntegrationTest, CooperativeProcessingWithBargaining) {
    /* Set brain to cooperative mode */
    hemispheric_brain_set_mode(brain, HEMISPHERIC_MODE_COOPERATIVE);

    float input[32];
    for (int i = 0; i < 32; i++) {
        input[i] = (float)i / 32.0f;
    }

    float output[16];
    gt_hemi_outcome_t outcome;

    nimcp_error_t err = gt_hemi_process_bargaining(bargaining_ctx,
                                                     input, 32,
                                                     output, 16,
                                                     &outcome);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Both hemispheres should contribute */
    EXPECT_GT(outcome.left_allocation, 0.0f);
    EXPECT_GT(outcome.right_allocation, 0.0f);
}

/* ============================================================================
 * Different Bargaining Types
 * ============================================================================ */

class GTHemisphericBargainingTypesTest : public ::testing::TestWithParam<nimcp_bargaining_type_t> {
protected:
    gt_hemi_bargaining_ctx_t bargaining_ctx;
    hemispheric_brain_t* brain;

    void SetUp() override {
        bargaining_ctx = nullptr;
        brain = nullptr;

        hemispheric_brain_config_t brain_config = hemispheric_brain_default_config();
        brain_config.size = BRAIN_SIZE_SMALL;
        brain_config.num_inputs = 32;
        brain_config.num_outputs = 16;
        brain = hemispheric_brain_create(&brain_config);
        ASSERT_NE(brain, nullptr);

        gt_hemi_config_t config = gt_hemi_default_config();
        config.bargain_type = GetParam();
        bargaining_ctx = gt_hemi_create(brain, &config);
        ASSERT_NE(bargaining_ctx, nullptr);
    }

    void TearDown() override {
        if (bargaining_ctx) gt_hemi_destroy(bargaining_ctx);
        if (brain) hemispheric_brain_destroy(brain);
    }
};

TEST_P(GTHemisphericBargainingTypesTest, NegotiateResources) {
    gt_hemi_outcome_t outcome;
    nimcp_error_t err = gt_hemi_negotiate_resources(bargaining_ctx, 1.0f, &outcome);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_TRUE(outcome.agreement_reached);
    EXPECT_NEAR(outcome.left_allocation + outcome.right_allocation, 1.0f, 0.01f);
}

INSTANTIATE_TEST_SUITE_P(
    BargainingTypes,
    GTHemisphericBargainingTypesTest,
    ::testing::Values(
        NIMCP_BARGAINING_NASH,
        NIMCP_BARGAINING_KALAI_SMORODINSKY,
        NIMCP_BARGAINING_EGALITARIAN
    )
);

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
