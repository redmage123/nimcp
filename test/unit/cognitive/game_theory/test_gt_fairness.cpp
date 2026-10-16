//=============================================================================
// test_gt_fairness.cpp - Unit tests for Fairness Metrics Module
//=============================================================================

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <cstdlib>

// Headers have their own extern "C" guards
#include "cognitive/game_theory/nimcp_gt_fairness.h"
#include "cognitive/game_theory/nimcp_game_theory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class FairnessTest : public ::testing::Test {
protected:
    nimcp_fairness_config_t config;

    void SetUp() override {
        config = nimcp_fairness_default_config();
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(FairnessTest, DefaultConfigValues) {
    EXPECT_GT(config.atkinson_epsilon, 0.0f);
    EXPECT_LT(config.atkinson_epsilon, 1.0f);
    EXPECT_GT(config.tolerance, 0.0f);
}

TEST_F(FairnessTest, MeasureNames) {
    EXPECT_NE(nimcp_fairness_measure_name(NIMCP_FAIRNESS_JAIN), nullptr);
    EXPECT_NE(nimcp_fairness_measure_name(NIMCP_FAIRNESS_GINI), nullptr);
    EXPECT_NE(nimcp_fairness_measure_name(NIMCP_FAIRNESS_THEIL), nullptr);
    EXPECT_NE(nimcp_fairness_measure_name(NIMCP_FAIRNESS_ATKINSON), nullptr);
    EXPECT_NE(nimcp_fairness_measure_name(NIMCP_FAIRNESS_COEFFICIENT_VARIATION), nullptr);
}

TEST_F(FairnessTest, PropertyNames) {
    EXPECT_NE(nimcp_allocation_property_name(NIMCP_ALLOC_ENVY_FREE), nullptr);
    EXPECT_NE(nimcp_allocation_property_name(NIMCP_ALLOC_EF1), nullptr);
    EXPECT_NE(nimcp_allocation_property_name(NIMCP_ALLOC_EFX), nullptr);
    EXPECT_NE(nimcp_allocation_property_name(NIMCP_ALLOC_PROPORTIONAL), nullptr);
    EXPECT_NE(nimcp_allocation_property_name(NIMCP_ALLOC_MAXIMIN), nullptr);
    EXPECT_NE(nimcp_allocation_property_name(NIMCP_ALLOC_PARETO_OPTIMAL), nullptr);
}

//=============================================================================
// Jain's Index Tests
//=============================================================================

TEST_F(FairnessTest, JainPerfectlyFair) {
    float values[] = {10.0f, 10.0f, 10.0f, 10.0f};
    float jain = nimcp_fairness_jain_index(values, 4);
    EXPECT_FLOAT_EQ(jain, 1.0f);  // Perfect equality
}

TEST_F(FairnessTest, JainMaximallyUnfair) {
    float values[] = {100.0f, 0.0f, 0.0f, 0.0f};
    float jain = nimcp_fairness_jain_index(values, 4);
    EXPECT_FLOAT_EQ(jain, 0.25f);  // 1/n for maximally unfair
}

TEST_F(FairnessTest, JainMixedDistribution) {
    float values[] = {10.0f, 20.0f, 30.0f, 40.0f};
    float jain = nimcp_fairness_jain_index(values, 4);
    EXPECT_GT(jain, 0.25f);
    EXPECT_LT(jain, 1.0f);
}

TEST_F(FairnessTest, JainTwoPlayers) {
    float equal[] = {50.0f, 50.0f};
    float jain_equal = nimcp_fairness_jain_index(equal, 2);
    EXPECT_FLOAT_EQ(jain_equal, 1.0f);

    float unequal[] = {100.0f, 0.0f};
    float jain_unequal = nimcp_fairness_jain_index(unequal, 2);
    EXPECT_FLOAT_EQ(jain_unequal, 0.5f);  // 1/n = 1/2
}

//=============================================================================
// Gini Coefficient Tests
//=============================================================================

TEST_F(FairnessTest, GiniPerfectEquality) {
    float values[] = {10.0f, 10.0f, 10.0f, 10.0f};
    float gini = nimcp_fairness_gini_coefficient(values, 4);
    EXPECT_FLOAT_EQ(gini, 0.0f);  // Perfect equality
}

TEST_F(FairnessTest, GiniMaximalInequality) {
    float values[] = {100.0f, 0.0f, 0.0f, 0.0f};
    float gini = nimcp_fairness_gini_coefficient(values, 4);
    EXPECT_NEAR(gini, 0.75f, 0.01f);  // Close to 1 for maximum inequality
}

TEST_F(FairnessTest, GiniMixedDistribution) {
    float values[] = {10.0f, 20.0f, 30.0f, 40.0f};
    float gini = nimcp_fairness_gini_coefficient(values, 4);
    EXPECT_GT(gini, 0.0f);
    EXPECT_LT(gini, 1.0f);
}

//=============================================================================
// Theil Index Tests
//=============================================================================

TEST_F(FairnessTest, TheilPerfectEquality) {
    float values[] = {10.0f, 10.0f, 10.0f, 10.0f};
    float theil = nimcp_fairness_theil_index(values, 4);
    EXPECT_NEAR(theil, 0.0f, 0.001f);  // Perfect equality
}

TEST_F(FairnessTest, TheilMixedDistribution) {
    float values[] = {1.0f, 2.0f, 3.0f, 4.0f};
    float theil = nimcp_fairness_theil_index(values, 4);
    EXPECT_GT(theil, 0.0f);  // Some inequality
}

//=============================================================================
// Atkinson Index Tests
//=============================================================================

TEST_F(FairnessTest, AtkinsonPerfectEquality) {
    float values[] = {10.0f, 10.0f, 10.0f, 10.0f};
    float atkinson = nimcp_fairness_atkinson_index(values, 4, 0.5f);
    EXPECT_NEAR(atkinson, 0.0f, 0.001f);  // Perfect equality
}

TEST_F(FairnessTest, AtkinsonDifferentEpsilon) {
    float values[] = {10.0f, 20.0f, 30.0f, 40.0f};
    float atkinson_low = nimcp_fairness_atkinson_index(values, 4, 0.2f);
    float atkinson_high = nimcp_fairness_atkinson_index(values, 4, 0.8f);

    // Higher epsilon = more sensitivity to inequality
    EXPECT_LE(atkinson_low, atkinson_high);
}

//=============================================================================
// Coefficient of Variation Tests
//=============================================================================

TEST_F(FairnessTest, CvPerfectEquality) {
    float values[] = {10.0f, 10.0f, 10.0f, 10.0f};
    float cv = nimcp_fairness_coefficient_variation(values, 4);
    EXPECT_FLOAT_EQ(cv, 0.0f);  // No variation
}

TEST_F(FairnessTest, CvMixedDistribution) {
    float values[] = {10.0f, 20.0f, 30.0f, 40.0f};
    float cv = nimcp_fairness_coefficient_variation(values, 4);
    EXPECT_GT(cv, 0.0f);  // Some variation
}

//=============================================================================
// Allocation Structure Tests
//=============================================================================

TEST_F(FairnessTest, AllocationCreateDestroy) {
    nimcp_allocation_t* alloc = nimcp_allocation_create(3, 6);
    ASSERT_NE(alloc, nullptr);
    EXPECT_EQ(alloc->num_players, 3);
    EXPECT_EQ(alloc->num_items, 6);

    nimcp_allocation_destroy(alloc);
}

TEST_F(FairnessTest, AllocationSetValuations) {
    nimcp_allocation_t* alloc = nimcp_allocation_create(2, 3);
    ASSERT_NE(alloc, nullptr);

    float vals[] = {10.0f, 20.0f, 30.0f};
    nimcp_error_t err = nimcp_allocation_set_valuations(alloc, 0, vals);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    nimcp_allocation_destroy(alloc);
}

TEST_F(FairnessTest, AllocationAssignItem) {
    nimcp_allocation_t* alloc = nimcp_allocation_create(2, 3);
    ASSERT_NE(alloc, nullptr);

    nimcp_error_t err = nimcp_allocation_assign_item(alloc, 0, 0);  // Item 0 to player 0
    EXPECT_EQ(err, NIMCP_SUCCESS);

    err = nimcp_allocation_assign_item(alloc, 1, 1);  // Item 1 to player 1
    EXPECT_EQ(err, NIMCP_SUCCESS);

    nimcp_allocation_destroy(alloc);
}

TEST_F(FairnessTest, AllocationComputeBundleValues) {
    nimcp_allocation_t* alloc = nimcp_allocation_create(2, 4);
    ASSERT_NE(alloc, nullptr);

    // Set valuations
    float vals0[] = {10.0f, 20.0f, 30.0f, 40.0f};
    float vals1[] = {15.0f, 25.0f, 35.0f, 45.0f};
    nimcp_allocation_set_valuations(alloc, 0, vals0);
    nimcp_allocation_set_valuations(alloc, 1, vals1);

    // Assign items: 0,1 -> player 0; 2,3 -> player 1
    nimcp_allocation_assign_item(alloc, 0, 0);
    nimcp_allocation_assign_item(alloc, 1, 0);
    nimcp_allocation_assign_item(alloc, 2, 1);
    nimcp_allocation_assign_item(alloc, 3, 1);

    nimcp_error_t err = nimcp_allocation_compute_bundle_values(alloc);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Player 0's bundle value (own valuation): 10 + 20 = 30
    EXPECT_FLOAT_EQ(alloc->bundle_values[0], 30.0f);
    // Player 1's bundle value (own valuation): 35 + 45 = 80
    EXPECT_FLOAT_EQ(alloc->bundle_values[1], 80.0f);

    nimcp_allocation_destroy(alloc);
}

//=============================================================================
// Envy-Freeness Tests (using raw arrays)
//=============================================================================

TEST_F(FairnessTest, EnvyFreeAllocation) {
    // 2 players, 4 items
    // Player valuations
    float* valuations[2];
    float v0[] = {10.0f, 20.0f, 30.0f, 40.0f};  // Player 0
    float v1[] = {15.0f, 25.0f, 35.0f, 45.0f};  // Player 1
    valuations[0] = v0;
    valuations[1] = v1;

    // Assignment: Items 2,3 to player 0; items 0,1 to player 1
    uint32_t assignment[] = {1, 1, 0, 0};  // item i -> player assignment[i]

    bool is_ef = nimcp_fairness_is_envy_free((const float* const*)valuations, assignment, 2, 4);
    // Player 0 gets items 2,3: value = 30 + 40 = 70
    // Player 0 values player 1's bundle: 10 + 20 = 30
    // Player 0 doesn't envy (70 >= 30)
    // Player 1 gets items 0,1: value = 15 + 25 = 40
    // Player 1 values player 0's bundle: 35 + 45 = 80
    // Player 1 envies! (40 < 80)
    EXPECT_FALSE(is_ef);
}

TEST_F(FairnessTest, EnvyFreeEqualSplit) {
    // 2 players, 2 identical items
    float* valuations[2];
    float v0[] = {10.0f, 10.0f};
    float v1[] = {10.0f, 10.0f};
    valuations[0] = v0;
    valuations[1] = v1;

    // Each gets one item
    uint32_t assignment[] = {0, 1};

    bool is_ef = nimcp_fairness_is_envy_free((const float* const*)valuations, assignment, 2, 2);
    EXPECT_TRUE(is_ef);  // Equal split of identical items
}

//=============================================================================
// EF1 Tests
//=============================================================================

TEST_F(FairnessTest, Ef1Allocation) {
    // 2 players, 3 items
    float* valuations[2];
    float v0[] = {10.0f, 20.0f, 30.0f};
    float v1[] = {10.0f, 20.0f, 30.0f};
    valuations[0] = v0;
    valuations[1] = v1;

    // Player 0 gets items 0,2 (value 40); Player 1 gets item 1 (value 20)
    uint32_t assignment[] = {0, 1, 0};

    bool is_ef1 = nimcp_fairness_is_ef1((const float* const*)valuations, assignment, 2, 3);
    // Player 1 values own bundle: 20
    // Player 1 values player 0's bundle: 40
    // If we remove item 2 (30), player 0's bundle = 10 < 20, so EF1 holds
    EXPECT_TRUE(is_ef1);
}

//=============================================================================
// EFX Tests
//=============================================================================

TEST_F(FairnessTest, EfxAllocation) {
    // 2 players, 2 items
    float* valuations[2];
    float v0[] = {10.0f, 10.0f};
    float v1[] = {10.0f, 10.0f};
    valuations[0] = v0;
    valuations[1] = v1;

    // Each gets one item
    uint32_t assignment[] = {0, 1};

    bool is_efx = nimcp_fairness_is_efx((const float* const*)valuations, assignment, 2, 2);
    EXPECT_TRUE(is_efx);  // Symmetric allocation with identical valuations
}

//=============================================================================
// Proportionality Tests
//=============================================================================

TEST_F(FairnessTest, ProportionalAllocation) {
    // 2 players, 4 items, each player values total at 100
    float* valuations[2];
    float v0[] = {25.0f, 25.0f, 25.0f, 25.0f};
    float v1[] = {25.0f, 25.0f, 25.0f, 25.0f};
    valuations[0] = v0;
    valuations[1] = v1;

    // Split evenly: player 0 gets items 0,1; player 1 gets items 2,3
    uint32_t assignment[] = {0, 0, 1, 1};

    bool is_prop = nimcp_fairness_is_proportional((const float* const*)valuations, assignment, 2, 4);
    // Each gets 50, which is 1/2 of total 100
    EXPECT_TRUE(is_prop);
}

TEST_F(FairnessTest, NotProportionalAllocation) {
    // 2 players, 4 items
    float* valuations[2];
    float v0[] = {10.0f, 10.0f, 40.0f, 40.0f};  // Total = 100
    float v1[] = {25.0f, 25.0f, 25.0f, 25.0f};  // Total = 100
    valuations[0] = v0;
    valuations[1] = v1;

    // Player 0 gets only items 0,1 (value 20); player 1 gets items 2,3 (value 50)
    uint32_t assignment[] = {0, 0, 1, 1};

    bool is_prop = nimcp_fairness_is_proportional((const float* const*)valuations, assignment, 2, 4);
    // Player 0 gets 20, but 1/2 of their total is 50 -> not proportional
    EXPECT_FALSE(is_prop);
}

//=============================================================================
// Maximin Share Tests
//=============================================================================

TEST_F(FairnessTest, MaximinShare) {
    // 2 players, 4 items
    float* valuations[2];
    float v0[] = {10.0f, 20.0f, 30.0f, 40.0f};  // Total = 100
    float v1[] = {25.0f, 25.0f, 25.0f, 25.0f};  // Total = 100
    valuations[0] = v0;
    valuations[1] = v1;

    // Compute MMS for player 0
    float mms0 = nimcp_fairness_maximin_share((const float* const*)valuations, 0, 2, 4);
    // Player 0's MMS: best worst-case partition for 2 players
    // Best partition: {10,40} and {20,30} -> min = 50
    EXPECT_GE(mms0, 0.0f);
}

TEST_F(FairnessTest, MmsGuarantee) {
    // 2 players, 2 items (simple case)
    float* valuations[2];
    float v0[] = {50.0f, 50.0f};
    float v1[] = {50.0f, 50.0f};
    valuations[0] = v0;
    valuations[1] = v1;

    // Each gets one item
    uint32_t assignment[] = {0, 1};

    bool has_mms = nimcp_fairness_has_mms_guarantee((const float* const*)valuations, assignment, 2, 2);
    EXPECT_TRUE(has_mms);  // Each gets 50, MMS = 50
}

//=============================================================================
// Comprehensive Analysis Tests
//=============================================================================

TEST_F(FairnessTest, ComputeAllMeasures) {
    float values[] = {10.0f, 20.0f, 30.0f, 40.0f};

    nimcp_fairness_result_t result;
    nimcp_fairness_result_init(&result, 4);

    nimcp_error_t err = nimcp_fairness_compute_all(values, 4, &config, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    EXPECT_GT(result.jain_index, 0.0f);
    EXPECT_LE(result.jain_index, 1.0f);
    EXPECT_GE(result.gini_coefficient, 0.0f);
    EXPECT_LE(result.gini_coefficient, 1.0f);
    EXPECT_GE(result.theil_index, 0.0f);
    EXPECT_GE(result.atkinson_index, 0.0f);
    EXPECT_LE(result.atkinson_index, 1.0f);
    EXPECT_GE(result.coefficient_of_variation, 0.0f);

    nimcp_fairness_result_cleanup(&result);
}

TEST_F(FairnessTest, AnalyzeAllocation) {
    nimcp_allocation_t* alloc = nimcp_allocation_create(2, 4);
    ASSERT_NE(alloc, nullptr);

    float v0[] = {25.0f, 25.0f, 25.0f, 25.0f};
    float v1[] = {25.0f, 25.0f, 25.0f, 25.0f};
    nimcp_allocation_set_valuations(alloc, 0, v0);
    nimcp_allocation_set_valuations(alloc, 1, v1);

    nimcp_allocation_assign_item(alloc, 0, 0);
    nimcp_allocation_assign_item(alloc, 1, 0);
    nimcp_allocation_assign_item(alloc, 2, 1);
    nimcp_allocation_assign_item(alloc, 3, 1);

    nimcp_fairness_result_t result;
    nimcp_fairness_result_init(&result, 2);

    nimcp_error_t err = nimcp_fairness_analyze_allocation(alloc, &config, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Equal split of identical valuations should be fair
    EXPECT_TRUE(result.is_envy_free);
    EXPECT_TRUE(result.is_ef1);
    EXPECT_TRUE(result.is_efx);
    EXPECT_TRUE(result.is_proportional);

    nimcp_fairness_result_cleanup(&result);
    nimcp_allocation_destroy(alloc);
}

//=============================================================================
// Find Envious Pairs Tests
//=============================================================================

TEST_F(FairnessTest, FindEnviousPairs) {
    // 2 players, 3 items
    float* valuations[2];
    float v0[] = {10.0f, 20.0f, 70.0f};  // Wants item 2
    float v1[] = {30.0f, 30.0f, 40.0f};
    valuations[0] = v0;
    valuations[1] = v1;

    // Player 0 gets items 0,1 (value 30); Player 1 gets item 2 (value 40)
    uint32_t assignment[] = {0, 0, 1};

    nimcp_envy_pair_t pairs[10];
    uint32_t num_found = 0;

    nimcp_error_t err = nimcp_fairness_find_envious_pairs(
        (const float* const*)valuations, assignment, 2, 3, pairs, 10, &num_found);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Player 0 values own bundle: 30
    // Player 0 values player 1's bundle: 70
    // Player 0 envies player 1
    EXPECT_GE(num_found, 1);
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(FairnessTest, SinglePlayer) {
    float values[] = {100.0f};
    float jain = nimcp_fairness_jain_index(values, 1);
    EXPECT_FLOAT_EQ(jain, 1.0f);  // Single player is always fair

    float gini = nimcp_fairness_gini_coefficient(values, 1);
    EXPECT_FLOAT_EQ(gini, 0.0f);
}

TEST_F(FairnessTest, ZeroValues) {
    float values[] = {0.0f, 0.0f, 0.0f};
    // Behavior depends on implementation - may return error or 1.0
    float jain = nimcp_fairness_jain_index(values, 3);
    // Accept either error (-1) or perfectly fair (1.0) for zero distribution
    EXPECT_TRUE(jain == -1.0f || jain == 1.0f || std::isnan(jain));
}

TEST_F(FairnessTest, LargeNumberOfPlayers) {
    const int N = 100;
    float values[N];
    for (int i = 0; i < N; i++) {
        values[i] = 10.0f;  // All equal
    }

    float jain = nimcp_fairness_jain_index(values, N);
    EXPECT_FLOAT_EQ(jain, 1.0f);

    float gini = nimcp_fairness_gini_coefficient(values, N);
    EXPECT_FLOAT_EQ(gini, 0.0f);
}

TEST_F(FairnessTest, AllocationCopy) {
    nimcp_allocation_t* src = nimcp_allocation_create(2, 3);
    ASSERT_NE(src, nullptr);

    float v0[] = {10.0f, 20.0f, 30.0f};
    nimcp_allocation_set_valuations(src, 0, v0);
    nimcp_allocation_assign_item(src, 0, 0);
    nimcp_allocation_assign_item(src, 1, 1);
    nimcp_allocation_assign_item(src, 2, 0);

    nimcp_allocation_t* dst = nimcp_allocation_create(2, 3);
    ASSERT_NE(dst, nullptr);

    nimcp_error_t err = nimcp_allocation_copy(src, dst);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(dst->assignment[0], src->assignment[0]);
    EXPECT_EQ(dst->assignment[1], src->assignment[1]);
    EXPECT_EQ(dst->assignment[2], src->assignment[2]);

    nimcp_allocation_destroy(src);
    nimcp_allocation_destroy(dst);
}

TEST_F(FairnessTest, ResultInitCleanup) {
    nimcp_fairness_result_t result;

    nimcp_error_t err = nimcp_fairness_result_init(&result, 4);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Should not crash
    nimcp_fairness_result_cleanup(&result);
}
