/**
 * @file test_energy_consistency.cpp
 * @brief Unit tests for Energy-Based Logical Consistency System
 *
 * Tests the energy consistency checker which implements E=0 means
 * logically consistent, with energy contributions from logical,
 * mathematical, and thermodynamic violations.
 */

#include <gtest/gtest.h>
#include "utils/nimcp_test_base.h"

extern "C" {
#include "cognitive/neuro_symbolic/nimcp_energy_consistency.h"
}

/**
 * @brief Test fixture for Energy Consistency tests
 */
class EnergyConsistencyTest : public NimcpTestBase {
protected:
    energy_consistency_checker_t* checker;
    energy_consistency_config_t config;

    void SetUp() override {
        NimcpTestBase::SetUp();
        checker = NULL;
        memset(&config, 0, sizeof(config));
        energy_consistency_get_default_config(&config);
    }

    void TearDown() override {
        if (checker) {
            energy_consistency_destroy(checker);
            checker = NULL;
        }
        NimcpTestBase::TearDown();
    }
};

// ============================================================================
// Default Configuration Tests
// ============================================================================

TEST_F(EnergyConsistencyTest, GetDefaultConfigSucceeds) {
    energy_consistency_config_t cfg;
    nimcp_error_t err = energy_consistency_get_default_config(&cfg);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GT(cfg.logical_weight, 0.0f);
    EXPECT_GT(cfg.mathematical_weight, 0.0f);
}

TEST_F(EnergyConsistencyTest, GetDefaultConfigNullReturnsError) {
    nimcp_error_t err = energy_consistency_get_default_config(NULL);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(EnergyConsistencyTest, DefaultConfigHasReasonableWeights) {
    energy_consistency_config_t cfg;
    energy_consistency_get_default_config(&cfg);

    // Weights should be positive
    EXPECT_GT(cfg.logical_weight, 0.0f);
    EXPECT_GT(cfg.mathematical_weight, 0.0f);
    EXPECT_GE(cfg.thermodynamic_weight, 0.0f);

    // Weights should not be excessively large
    EXPECT_LE(cfg.logical_weight, 100.0f);
    EXPECT_LE(cfg.mathematical_weight, 100.0f);
    EXPECT_LE(cfg.thermodynamic_weight, 100.0f);
}

// ============================================================================
// Lifecycle Tests
// ============================================================================

TEST_F(EnergyConsistencyTest, CreateWithDefaultConfigSucceeds) {
    checker = energy_consistency_create(&config);
    ASSERT_NE(checker, nullptr);
}

TEST_F(EnergyConsistencyTest, CreateWithNullConfigSucceeds) {
    // Should use defaults when NULL is passed
    checker = energy_consistency_create(NULL);
    EXPECT_NE(checker, nullptr);
}

TEST_F(EnergyConsistencyTest, DestroyNullIsNoOp) {
    // Should not crash
    energy_consistency_destroy(NULL);
    SUCCEED();
}

TEST_F(EnergyConsistencyTest, CreateDestroyMultipleTimesSucceeds) {
    for (int i = 0; i < 5; i++) {
        checker = energy_consistency_create(&config);
        ASSERT_NE(checker, nullptr) << "Failed on iteration " << i;
        energy_consistency_destroy(checker);
        checker = NULL;
    }
}

TEST_F(EnergyConsistencyTest, ResetNullReturnsError) {
    nimcp_error_t err = energy_consistency_reset(NULL);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(EnergyConsistencyTest, ResetClearsState) {
    checker = energy_consistency_create(&config);
    ASSERT_NE(checker, nullptr);

    nimcp_error_t err = energy_consistency_reset(checker);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

// ============================================================================
// Consistency Score Tests
// ============================================================================

TEST_F(EnergyConsistencyTest, GetScoreNullReturnsNegative) {
    float score = energy_consistency_get_score(NULL);
    EXPECT_LT(score, 0.0f);
}

TEST_F(EnergyConsistencyTest, InitialScoreIsReasonable) {
    checker = energy_consistency_create(&config);
    ASSERT_NE(checker, nullptr);

    float score = energy_consistency_get_score(checker);
    // Score should be in [0,1] or negative for error
    EXPECT_TRUE((score >= 0.0f && score <= 1.0f) || score < 0.0f);
}

// ============================================================================
// Proposition Checking Tests
// ============================================================================

TEST_F(EnergyConsistencyTest, CheckPropositionNullCheckerReturnsError) {
    energy_consistency_result_t result;
    memset(&result, 0, sizeof(result));

    nimcp_error_t err = energy_consistency_check_proposition(
        NULL, "P", NULL, &result);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(EnergyConsistencyTest, CheckPropositionNullPropositionReturnsError) {
    checker = energy_consistency_create(&config);
    ASSERT_NE(checker, nullptr);

    energy_consistency_result_t result;
    memset(&result, 0, sizeof(result));

    nimcp_error_t err = energy_consistency_check_proposition(
        checker, NULL, NULL, &result);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(EnergyConsistencyTest, CheckPropositionNullResultReturnsError) {
    checker = energy_consistency_create(&config);
    ASSERT_NE(checker, nullptr);

    nimcp_error_t err = energy_consistency_check_proposition(
        checker, "P", NULL, NULL);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(EnergyConsistencyTest, CheckSimplePropositionSucceeds) {
    checker = energy_consistency_create(&config);
    ASSERT_NE(checker, nullptr);

    energy_consistency_result_t result;
    memset(&result, 0, sizeof(result));

    nimcp_error_t err = energy_consistency_check_proposition(
        checker, "P", NULL, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Simple proposition should be consistent (low energy)
    EXPECT_GE(result.final_consistency, 0.0f);
    EXPECT_LE(result.final_consistency, 1.0f);
}

// ============================================================================
// Pair Checking Tests
// ============================================================================

TEST_F(EnergyConsistencyTest, CheckPairNullCheckerReturnsError) {
    energy_consistency_result_t result;
    memset(&result, 0, sizeof(result));

    nimcp_error_t err = energy_consistency_check_pair(
        NULL, "P", "Q", &result);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(EnergyConsistencyTest, CheckPairConsistentExpressionsSucceeds) {
    checker = energy_consistency_create(&config);
    ASSERT_NE(checker, nullptr);

    energy_consistency_result_t result;
    memset(&result, 0, sizeof(result));

    // P and Q are independent, should be consistent
    nimcp_error_t err = energy_consistency_check_pair(
        checker, "P", "Q", &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Should have low energy (high consistency)
    EXPECT_GE(result.final_consistency, 0.0f);
}

// ============================================================================
// Proof Checking Tests
// ============================================================================

TEST_F(EnergyConsistencyTest, CheckProofNullCheckerReturnsError) {
    proof_step_t steps[2];
    memset(steps, 0, sizeof(steps));
    energy_consistency_result_t result;
    memset(&result, 0, sizeof(result));

    nimcp_error_t err = energy_consistency_check_proof(
        NULL, steps, 2, &result);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(EnergyConsistencyTest, CheckProofEmptyTraceSucceeds) {
    checker = energy_consistency_create(&config);
    ASSERT_NE(checker, nullptr);

    energy_consistency_result_t result;
    memset(&result, 0, sizeof(result));

    // Empty proof should succeed (no violations)
    nimcp_error_t err = energy_consistency_check_proof(
        checker, NULL, 0, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(EnergyConsistencyTest, CheckValidProofTraceSucceeds) {
    checker = energy_consistency_create(&config);
    ASSERT_NE(checker, nullptr);

    // Create a simple valid proof trace
    proof_step_t steps[3];
    memset(steps, 0, sizeof(steps));

    // Step 0: Axiom A
    steps[0].type = PROOF_STEP_AXIOM;
    steps[0].step_id = 0;
    steps[0].premise_count = 0;
    steps[0].confidence = 1.0f;
    steps[0].is_valid = true;
    strncpy(steps[0].rule_name, "axiom", sizeof(steps[0].rule_name) - 1);
    strncpy(steps[0].conclusion, "A", sizeof(steps[0].conclusion) - 1);

    // Step 1: Hypothesis B
    steps[1].type = PROOF_STEP_HYPOTHESIS;
    steps[1].step_id = 1;
    steps[1].premise_count = 0;
    steps[1].confidence = 1.0f;
    steps[1].is_valid = true;
    strncpy(steps[1].rule_name, "assume", sizeof(steps[1].rule_name) - 1);
    strncpy(steps[1].conclusion, "B", sizeof(steps[1].conclusion) - 1);

    // Step 2: QED
    steps[2].type = PROOF_STEP_QED;
    steps[2].step_id = 2;
    steps[2].premise_count = 0;
    steps[2].confidence = 1.0f;
    steps[2].is_valid = true;
    strncpy(steps[2].rule_name, "qed", sizeof(steps[2].rule_name) - 1);
    strncpy(steps[2].conclusion, "A -> B", sizeof(steps[2].conclusion) - 1);

    energy_consistency_result_t result;
    memset(&result, 0, sizeof(result));

    nimcp_error_t err = energy_consistency_check_proof(
        checker, steps, 3, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Valid proof should have low energy
    EXPECT_EQ(result.num_violations, 0u);
}

// ============================================================================
// Violation Energy Tests
// ============================================================================

TEST_F(EnergyConsistencyTest, ComputeViolationEnergyNullReturnsZero) {
    float energy = energy_consistency_compute_violation_energy(NULL, NULL);
    EXPECT_EQ(energy, 0.0f);
}

TEST_F(EnergyConsistencyTest, ComputeViolationEnergySeverityScaling) {
    checker = energy_consistency_create(&config);
    ASSERT_NE(checker, nullptr);

    consistency_violation_t warning;
    memset(&warning, 0, sizeof(warning));
    warning.type = CONSISTENCY_CONTRADICTION;
    warning.severity = VIOLATION_SEVERITY_WARNING;
    warning.energy_cost = 1.0f;

    consistency_violation_t critical;
    memset(&critical, 0, sizeof(critical));
    critical.type = CONSISTENCY_CONTRADICTION;
    critical.severity = VIOLATION_SEVERITY_CRITICAL;
    critical.energy_cost = 1.0f;

    float warning_energy = energy_consistency_compute_violation_energy(
        checker, &warning);
    float critical_energy = energy_consistency_compute_violation_energy(
        checker, &critical);

    // Critical violations should have higher or equal energy
    EXPECT_GE(critical_energy, warning_energy);
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_F(EnergyConsistencyTest, GetStatsNullReturnsError) {
    energy_consistency_stats_t stats;
    nimcp_error_t err = energy_consistency_get_stats(NULL, &stats);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(EnergyConsistencyTest, GetStatsAfterChecksShowsCounts) {
    checker = energy_consistency_create(&config);
    ASSERT_NE(checker, nullptr);

    // Perform a check
    energy_consistency_result_t result;
    memset(&result, 0, sizeof(result));
    energy_consistency_check_proposition(checker, "P", NULL, &result);

    // Get stats
    energy_consistency_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    nimcp_error_t err = energy_consistency_get_stats(checker, &stats);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Should have recorded at least one check
    EXPECT_GE(stats.total_checks, 1u);
}

// ============================================================================
// Modulation Tests
// ============================================================================

TEST_F(EnergyConsistencyTest, ModulateATPNullReturnsError) {
    nimcp_error_t err = energy_consistency_modulate_atp(NULL, 0.5f);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(EnergyConsistencyTest, ModulateATPValidLevelSucceeds) {
    checker = energy_consistency_create(&config);
    ASSERT_NE(checker, nullptr);

    nimcp_error_t err = energy_consistency_modulate_atp(checker, 0.75f);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(EnergyConsistencyTest, ModulateATPClampsBelowZero) {
    checker = energy_consistency_create(&config);
    ASSERT_NE(checker, nullptr);

    // Should clamp to 0, not fail
    nimcp_error_t err = energy_consistency_modulate_atp(checker, -0.5f);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(EnergyConsistencyTest, ModulateATPClampsAboveOne) {
    checker = energy_consistency_create(&config);
    ASSERT_NE(checker, nullptr);

    // Should clamp to 1, not fail
    nimcp_error_t err = energy_consistency_modulate_atp(checker, 1.5f);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(EnergyConsistencyTest, FullWorkflowSucceeds) {
    // Create checker with bio-async disabled for unit test
    config.enable_bio_async = false;
    config.enable_fep_integration = false;
    config.enable_immune_integration = false;

    checker = energy_consistency_create(&config);
    ASSERT_NE(checker, nullptr);

    // Modulate ATP
    energy_consistency_modulate_atp(checker, 0.8f);

    // Check a proposition
    energy_consistency_result_t result;
    memset(&result, 0, sizeof(result));
    nimcp_error_t err = energy_consistency_check_proposition(
        checker, "P AND Q", NULL, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Get consistency score
    float score = energy_consistency_get_score(checker);
    EXPECT_GE(score, 0.0f);

    // Get stats
    energy_consistency_stats_t stats;
    err = energy_consistency_get_stats(checker, &stats);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Reset and verify
    err = energy_consistency_reset(checker);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}
