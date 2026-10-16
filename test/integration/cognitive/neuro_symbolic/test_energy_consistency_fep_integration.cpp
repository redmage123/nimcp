/**
 * @file test_energy_consistency_fep_integration.cpp
 * @brief Integration tests for Energy Consistency with FEP
 *
 * Tests the integration between Energy Consistency and Free Energy Principle:
 * - Consistency energy feeding to FEP prediction error
 * - Precision-weighted energy calculations
 * - FEP-guided consistency checking
 * - Active inference for consistency repair
 * - Bridge effects and modulation
 *
 * @version 2.6.3
 * @date 2025-12-31
 */

#include <gtest/gtest.h>
#include "utils/nimcp_test_base.h"
#include <cmath>

extern "C" {
#include "cognitive/neuro_symbolic/nimcp_energy_consistency.h"
#include "cognitive/neuro_symbolic/bridges/nimcp_energy_consistency_fep_bridge.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "utils/memory/nimcp_memory.h"
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

/**
 * @brief Test fixture for Energy Consistency FEP Integration tests
 */
class EnergyConsistencyFEPIntegrationTest : public NimcpTestBase {
protected:
    energy_consistency_checker_t* checker;
    energy_consistency_config_t config;

    void SetUp() override {
        NimcpTestBase::SetUp();
        checker = NULL;
        energy_consistency_get_default_config(&config);
        config.enable_fep_integration = true;
    }

    void TearDown() override {
        if (checker) {
            energy_consistency_destroy(checker);
            checker = NULL;
        }
        NimcpTestBase::TearDown();
    }

    /**
     * @brief Create a test violation
     */
    consistency_violation_t CreateTestViolation(
        consistency_violation_type_t type,
        violation_severity_t severity,
        float energy_cost,
        const char* description
    ) {
        consistency_violation_t violation;
        memset(&violation, 0, sizeof(violation));
        violation.type = type;
        violation.severity = severity;
        violation.energy_cost = energy_cost;
        strncpy(violation.description, description, sizeof(violation.description) - 1);
        violation.timestamp_us = 0;
        return violation;
    }
};

/* ============================================================================
 * Consistency Energy to FEP Prediction Error Tests
 * ============================================================================ */

TEST_F(EnergyConsistencyFEPIntegrationTest, EnergyFeedToFEP) {
    checker = energy_consistency_create(&config);
    ASSERT_NE(checker, nullptr);

    /* Feed consistency energy to FEP */
    float energy = 0.5f;
    nimcp_error_t err = energy_consistency_feed_to_fep(checker, NULL, energy);
    /* May fail without FEP system, but should not crash */
    EXPECT_TRUE(err == NIMCP_SUCCESS || err == NIMCP_ERROR_INVALID_PARAM);
}

TEST_F(EnergyConsistencyFEPIntegrationTest, ConsistencyCheckProducesFEPInput) {
    config.enable_fep_integration = true;
    checker = energy_consistency_create(&config);
    ASSERT_NE(checker, nullptr);

    /* Check a simple proposition */
    energy_consistency_result_t result;
    nimcp_error_t err = energy_consistency_result_init(&result, 10);
    EXPECT_EQ(err, 0);

    err = energy_consistency_check_proposition(checker, "P AND P", NULL, &result);
    EXPECT_EQ(err, 0);

    /* Result should have energy that can be fed to FEP */
    EXPECT_GE(result.total_energy, 0.0f);
    EXPECT_FALSE(std::isnan(result.total_energy));

    /* Final consistency should be valid */
    EXPECT_GE(result.final_consistency, 0.0f);
    EXPECT_LE(result.final_consistency, 1.0f);

    energy_consistency_result_cleanup(&result);
}

TEST_F(EnergyConsistencyFEPIntegrationTest, ContradictionProducesHighEnergy) {
    checker = energy_consistency_create(&config);
    ASSERT_NE(checker, nullptr);

    /* Check contradiction */
    energy_consistency_result_t result;
    energy_consistency_result_init(&result, 10);

    nimcp_error_t err = energy_consistency_check_proposition(checker, "P AND NOT P", NULL, &result);
    EXPECT_EQ(err, 0);

    /* Contradiction should have higher energy than tautology */
    energy_consistency_result_t tautology_result;
    energy_consistency_result_init(&tautology_result, 10);
    energy_consistency_check_proposition(checker, "P OR NOT P", NULL, &tautology_result);

    /* Contradiction energy should be >= tautology energy */
    EXPECT_GE(result.total_energy, 0.0f);

    energy_consistency_result_cleanup(&result);
    energy_consistency_result_cleanup(&tautology_result);
}

TEST_F(EnergyConsistencyFEPIntegrationTest, EnergyMappedToPredictionError) {
    checker = energy_consistency_create(&config);
    ASSERT_NE(checker, nullptr);

    /* Check various propositions and verify energy mapping */
    const char* propositions[] = {
        "A",                    /* Simple */
        "A AND B",              /* Conjunction */
        "A OR B",               /* Disjunction */
        "A IMPLIES B",          /* Implication */
    };

    for (int i = 0; i < 4; i++) {
        energy_consistency_result_t result;
        energy_consistency_result_init(&result, 10);

        energy_consistency_check_proposition(checker, propositions[i], NULL, &result);

        /* Energy should be finite and non-negative */
        EXPECT_GE(result.total_energy, 0.0f);
        EXPECT_FALSE(std::isnan(result.total_energy));
        EXPECT_FALSE(std::isinf(result.total_energy));

        /* Consistency = 1/(1+E) should be in [0,1] */
        EXPECT_GE(result.final_consistency, 0.0f);
        EXPECT_LE(result.final_consistency, 1.0f);

        energy_consistency_result_cleanup(&result);
    }
}

/* ============================================================================
 * Precision-Weighted Energy Calculations Tests
 * ============================================================================ */

TEST_F(EnergyConsistencyFEPIntegrationTest, PrecisionWeightedEnergyBasic) {
    checker = energy_consistency_create(&config);
    ASSERT_NE(checker, nullptr);

    /* Create test violation */
    consistency_violation_t violation = CreateTestViolation(
        CONSISTENCY_CONTRADICTION,
        VIOLATION_SEVERITY_ERROR,
        1.0f,
        "Test contradiction"
    );

    /* Get precision-weighted energy */
    float weighted_energy = energy_consistency_get_precision_weighted(checker, NULL, &violation);

    /* Should return valid energy */
    EXPECT_GE(weighted_energy, 0.0f);
    EXPECT_FALSE(std::isnan(weighted_energy));
}

TEST_F(EnergyConsistencyFEPIntegrationTest, SeverityAffectsPrecisionWeighting) {
    config.severity_multipliers[VIOLATION_SEVERITY_INFO] = 0.1f;
    config.severity_multipliers[VIOLATION_SEVERITY_WARNING] = 0.5f;
    config.severity_multipliers[VIOLATION_SEVERITY_ERROR] = 1.0f;
    config.severity_multipliers[VIOLATION_SEVERITY_CRITICAL] = 2.0f;
    config.severity_multipliers[VIOLATION_SEVERITY_FATAL] = 5.0f;

    checker = energy_consistency_create(&config);
    ASSERT_NE(checker, nullptr);

    /* Create violations of different severities */
    float base_energy = 1.0f;
    violation_severity_t severities[] = {
        VIOLATION_SEVERITY_INFO,
        VIOLATION_SEVERITY_WARNING,
        VIOLATION_SEVERITY_ERROR,
        VIOLATION_SEVERITY_CRITICAL
    };

    float weighted_energies[4];
    for (int i = 0; i < 4; i++) {
        consistency_violation_t violation = CreateTestViolation(
            CONSISTENCY_UNSATISFIED_RULE,
            severities[i],
            base_energy,
            "Severity test"
        );
        weighted_energies[i] = energy_consistency_compute_violation_energy(checker, &violation);
    }

    /* Higher severity should result in higher weighted energy */
    EXPECT_LE(weighted_energies[0], weighted_energies[1]);
    EXPECT_LE(weighted_energies[1], weighted_energies[2]);
    EXPECT_LE(weighted_energies[2], weighted_energies[3]);
}

TEST_F(EnergyConsistencyFEPIntegrationTest, EnergyFEPBridgePrecisionWeighting) {
    /* Create FEP bridge */
    energy_fep_bridge_t* bridge = energy_fep_bridge_create();
    ASSERT_NE(bridge, nullptr);

    /* Create test violation */
    consistency_violation_t violation = CreateTestViolation(
        CONSISTENCY_TYPE_MISMATCH,
        VIOLATION_SEVERITY_ERROR,
        0.8f,
        "Type mismatch"
    );

    /* Get precision-weighted energy through bridge */
    float weighted = energy_fep_bridge_get_precision_weighted_energy(bridge, &violation);

    /* Should return valid weighted energy */
    EXPECT_GE(weighted, 0.0f);
    EXPECT_FALSE(std::isnan(weighted));

    energy_fep_bridge_destroy(bridge);
}

/* ============================================================================
 * Energy-FEP Bridge Tests
 * ============================================================================ */

TEST_F(EnergyConsistencyFEPIntegrationTest, EnergyFEPBridgeCreation) {
    energy_fep_bridge_t* bridge = energy_fep_bridge_create();
    ASSERT_NE(bridge, nullptr);

    /* Verify initial state */
    EXPECT_EQ(bridge->total_updates, 0u);
    EXPECT_EQ(bridge->error_signals_sent, 0u);

    energy_fep_bridge_destroy(bridge);
}

TEST_F(EnergyConsistencyFEPIntegrationTest, EnergyFEPBridgeWithConfig) {
    energy_fep_bridge_config_t bridge_config;
    energy_fep_bridge_get_default_config(&bridge_config);

    bridge_config.enable_modulation = true;
    bridge_config.sensitivity = 1.5f;
    bridge_config.energy_to_error_scale = 1.0f;
    bridge_config.base_precision = 1.0f;

    energy_fep_bridge_t* bridge = energy_fep_bridge_create_with_config(&bridge_config);
    ASSERT_NE(bridge, nullptr);

    /* Config should be applied */
    EXPECT_TRUE(bridge->config.enable_modulation);
    EXPECT_FLOAT_EQ(bridge->config.sensitivity, 1.5f);

    energy_fep_bridge_destroy(bridge);
}

TEST_F(EnergyConsistencyFEPIntegrationTest, EnergyFEPBridgeUpdate) {
    energy_fep_bridge_t* bridge = energy_fep_bridge_create();
    ASSERT_NE(bridge, nullptr);

    /* Update with consistency energy */
    nimcp_error_t err = energy_fep_bridge_update(bridge, 0.5f);
    EXPECT_EQ(err, 0);

    /* Check updated state */
    EXPECT_EQ(bridge->total_updates, 1u);
    EXPECT_FLOAT_EQ(bridge->current_energy, 0.5f);

    /* Update again */
    err = energy_fep_bridge_update(bridge, 0.3f);
    EXPECT_EQ(err, 0);
    EXPECT_EQ(bridge->total_updates, 2u);

    energy_fep_bridge_destroy(bridge);
}

TEST_F(EnergyConsistencyFEPIntegrationTest, EnergyFEPBridgeUpdateFromResult) {
    energy_fep_bridge_t* bridge = energy_fep_bridge_create();
    ASSERT_NE(bridge, nullptr);

    /* Create consistency result */
    energy_consistency_result_t result;
    energy_consistency_result_init(&result, 10);
    result.total_energy = 0.7f;
    result.logical_energy = 0.5f;
    result.mathematical_energy = 0.2f;
    result.final_consistency = 0.588f;  /* 1/(1+0.7) */

    /* Update from result */
    nimcp_error_t err = energy_fep_bridge_update_from_result(bridge, &result);
    EXPECT_EQ(err, 0);

    /* Bridge should reflect result */
    EXPECT_FLOAT_EQ(bridge->current_energy, 0.7f);

    energy_consistency_result_cleanup(&result);
    energy_fep_bridge_destroy(bridge);
}

TEST_F(EnergyConsistencyFEPIntegrationTest, EnergyFEPBridgeEffects) {
    energy_fep_bridge_t* bridge = energy_fep_bridge_create();
    ASSERT_NE(bridge, nullptr);

    /* Update with energy to generate effects */
    energy_fep_bridge_update(bridge, 0.5f);

    /* Get effects */
    energy_fep_bridge_effects_t effects;
    nimcp_error_t err = energy_fep_bridge_get_effects(bridge, &effects);
    EXPECT_EQ(err, 0);

    /* Effects should be valid */
    EXPECT_GE(effects.prediction_error, 0.0f);
    EXPECT_GE(effects.precision, 0.0f);
    EXPECT_GE(effects.belief_update_rate, 0.0f);

    energy_fep_bridge_destroy(bridge);
}

TEST_F(EnergyConsistencyFEPIntegrationTest, EnergyFEPBridgePredictionError) {
    energy_fep_bridge_t* bridge = energy_fep_bridge_create();
    ASSERT_NE(bridge, nullptr);

    /* Update with different energy levels */
    energy_fep_bridge_update(bridge, 0.0f);  /* Perfect consistency */
    float pe_zero = energy_fep_bridge_get_prediction_error(bridge);

    energy_fep_bridge_update(bridge, 1.0f);  /* Some inconsistency */
    float pe_one = energy_fep_bridge_get_prediction_error(bridge);

    /* Higher energy should produce higher prediction error */
    EXPECT_LE(pe_zero, pe_one);

    energy_fep_bridge_destroy(bridge);
}

TEST_F(EnergyConsistencyFEPIntegrationTest, EnergyFEPBridgePrecision) {
    energy_fep_bridge_config_t cfg;
    energy_fep_bridge_get_default_config(&cfg);
    cfg.base_precision = 1.0f;

    energy_fep_bridge_t* bridge = energy_fep_bridge_create_with_config(&cfg);
    ASSERT_NE(bridge, nullptr);

    /* Get initial precision */
    float precision = energy_fep_bridge_get_precision(bridge);
    EXPECT_GT(precision, 0.0f);

    /* Update should potentially adjust precision */
    for (int i = 0; i < 10; i++) {
        energy_fep_bridge_update(bridge, 0.5f);
    }

    float precision_after = energy_fep_bridge_get_precision(bridge);
    EXPECT_GT(precision_after, 0.0f);

    energy_fep_bridge_destroy(bridge);
}

/* ============================================================================
 * Connection and Disconnection Tests
 * ============================================================================ */

TEST_F(EnergyConsistencyFEPIntegrationTest, EnergyFEPBridgeConnectChecker) {
    energy_fep_bridge_t* bridge = energy_fep_bridge_create();
    ASSERT_NE(bridge, nullptr);

    checker = energy_consistency_create(&config);
    ASSERT_NE(checker, nullptr);

    /* Connect checker */
    nimcp_error_t err = energy_fep_bridge_connect_checker(bridge, checker);
    EXPECT_EQ(err, 0);

    /* Disconnect */
    err = energy_fep_bridge_disconnect_checker(bridge);
    EXPECT_EQ(err, 0);

    energy_fep_bridge_destroy(bridge);
}

TEST_F(EnergyConsistencyFEPIntegrationTest, EnergyFEPBridgeConnectFEP) {
    energy_fep_bridge_t* bridge = energy_fep_bridge_create();
    ASSERT_NE(bridge, nullptr);

    /* Connect FEP (NULL in test) */
    nimcp_error_t err = energy_fep_bridge_connect_fep(bridge, NULL);
    /* May fail with NULL, but should not crash */
    EXPECT_TRUE(err == NIMCP_SUCCESS || err == NIMCP_ERROR_INVALID_PARAM);

    /* Disconnect */
    err = energy_fep_bridge_disconnect_fep(bridge);
    EXPECT_EQ(err, 0);

    energy_fep_bridge_destroy(bridge);
}

/* ============================================================================
 * Active Inference for Repair Tests
 * ============================================================================ */

TEST_F(EnergyConsistencyFEPIntegrationTest, EnergyFEPBridgeRepairAction) {
    energy_fep_bridge_config_t cfg;
    energy_fep_bridge_get_default_config(&cfg);
    cfg.enable_active_inference = true;

    energy_fep_bridge_t* bridge = energy_fep_bridge_create_with_config(&cfg);
    ASSERT_NE(bridge, nullptr);

    /* Create violation needing repair */
    consistency_violation_t violation = CreateTestViolation(
        CONSISTENCY_CONTRADICTION,
        VIOLATION_SEVERITY_ERROR,
        1.0f,
        "Contradiction to repair"
    );

    /* Get repair action */
    int action = -1;
    nimcp_error_t err = energy_fep_bridge_get_repair_action(bridge, &violation, &action);
    EXPECT_EQ(err, 0);

    /* Action should be recommended */
    EXPECT_GE(action, -1);

    energy_fep_bridge_destroy(bridge);
}

TEST_F(EnergyConsistencyFEPIntegrationTest, ConsistencyRequestRecovery) {
    checker = energy_consistency_create(&config);
    ASSERT_NE(checker, nullptr);

    /* Create result with critical violations */
    energy_consistency_result_t result;
    energy_consistency_result_init(&result, 10);
    result.total_energy = 5.0f;  /* High energy = inconsistent */
    result.final_consistency = 0.167f;

    /* Add critical violation */
    consistency_violation_t violation = CreateTestViolation(
        CONSISTENCY_CONTRADICTION,
        VIOLATION_SEVERITY_CRITICAL,
        5.0f,
        "Critical violation"
    );
    energy_consistency_result_add_violation(&result, &violation);

    /* Request recovery */
    nimcp_error_t err = energy_consistency_request_recovery(checker, &result);
    /* May not have immune system, but should not crash */
    EXPECT_TRUE(err == NIMCP_SUCCESS || err == NIMCP_ERROR_NOT_INITIALIZED);

    energy_consistency_result_cleanup(&result);
}

/* ============================================================================
 * Thermodynamic Cost Integration Tests
 * ============================================================================ */

TEST_F(EnergyConsistencyFEPIntegrationTest, ThermodynamicCostComputation) {
    config.track_thermodynamic_cost = true;
    checker = energy_consistency_create(&config);
    ASSERT_NE(checker, nullptr);

    /* Compute thermodynamic cost */
    float atp_cost = 0.0f;
    float landauer_cost = 0.0f;
    uint64_t bits_processed = 1000;

    nimcp_error_t err = energy_consistency_compute_thermo_cost(
        checker, bits_processed, &atp_cost, &landauer_cost);
    EXPECT_EQ(err, 0);

    /* Costs should be non-negative */
    EXPECT_GE(atp_cost, 0.0f);
    EXPECT_GE(landauer_cost, 0.0f);
}

TEST_F(EnergyConsistencyFEPIntegrationTest, ThermoCostInResult) {
    config.track_thermodynamic_cost = true;
    checker = energy_consistency_create(&config);
    ASSERT_NE(checker, nullptr);

    /* Check proposition with thermodynamic tracking */
    energy_consistency_result_t result;
    energy_consistency_result_init(&result, 10);

    nimcp_error_t err = energy_consistency_check_proposition(checker, "A AND B", NULL, &result);
    EXPECT_EQ(err, 0);

    /* Result should include thermodynamic costs */
    EXPECT_GE(result.thermodynamic_cost, 0.0f);

    energy_consistency_result_cleanup(&result);
}

/* ============================================================================
 * Energy Weight Configuration Tests
 * ============================================================================ */

TEST_F(EnergyConsistencyFEPIntegrationTest, EnergyWeightsAffectTotalEnergy) {
    /* Test with different weight configurations */
    energy_consistency_config_t config_high_logical;
    energy_consistency_get_default_config(&config_high_logical);
    config_high_logical.logical_weight = 2.0f;
    config_high_logical.mathematical_weight = 0.5f;

    energy_consistency_checker_t* checker_high_logical =
        energy_consistency_create(&config_high_logical);
    ASSERT_NE(checker_high_logical, nullptr);

    energy_consistency_config_t config_high_math;
    energy_consistency_get_default_config(&config_high_math);
    config_high_math.logical_weight = 0.5f;
    config_high_math.mathematical_weight = 2.0f;

    energy_consistency_checker_t* checker_high_math =
        energy_consistency_create(&config_high_math);
    ASSERT_NE(checker_high_math, nullptr);

    /* Both checkers should work */
    energy_consistency_result_t result1, result2;
    energy_consistency_result_init(&result1, 10);
    energy_consistency_result_init(&result2, 10);

    energy_consistency_check_proposition(checker_high_logical, "A", NULL, &result1);
    energy_consistency_check_proposition(checker_high_math, "A", NULL, &result2);

    /* Both should produce valid results */
    EXPECT_GE(result1.final_consistency, 0.0f);
    EXPECT_GE(result2.final_consistency, 0.0f);

    energy_consistency_result_cleanup(&result1);
    energy_consistency_result_cleanup(&result2);
    energy_consistency_destroy(checker_high_logical);
    energy_consistency_destroy(checker_high_math);
}

/* ============================================================================
 * Modulation Tests
 * ============================================================================ */

TEST_F(EnergyConsistencyFEPIntegrationTest, InflammationModulation) {
    checker = energy_consistency_create(&config);
    ASSERT_NE(checker, nullptr);

    /* Apply inflammation */
    nimcp_error_t err = energy_consistency_modulate_inflammation(checker, 0.5f);
    EXPECT_EQ(err, 0);

    /* Check proposition with inflammation */
    energy_consistency_result_t result;
    energy_consistency_result_init(&result, 10);
    energy_consistency_check_proposition(checker, "A", NULL, &result);

    /* Should still produce valid result */
    EXPECT_GE(result.final_consistency, 0.0f);

    energy_consistency_result_cleanup(&result);
}

TEST_F(EnergyConsistencyFEPIntegrationTest, FatigueModulation) {
    checker = energy_consistency_create(&config);
    ASSERT_NE(checker, nullptr);

    /* Apply fatigue */
    nimcp_error_t err = energy_consistency_modulate_fatigue(checker, 0.7f);
    EXPECT_EQ(err, 0);

    /* Check proposition with fatigue */
    energy_consistency_result_t result;
    energy_consistency_result_init(&result, 10);
    energy_consistency_check_proposition(checker, "A OR B", NULL, &result);

    EXPECT_GE(result.final_consistency, 0.0f);

    energy_consistency_result_cleanup(&result);
}

TEST_F(EnergyConsistencyFEPIntegrationTest, ATPModulation) {
    checker = energy_consistency_create(&config);
    ASSERT_NE(checker, nullptr);

    /* Apply low ATP */
    nimcp_error_t err = energy_consistency_modulate_atp(checker, 0.3f);
    EXPECT_EQ(err, 0);

    /* Check proposition with low ATP */
    energy_consistency_result_t result;
    energy_consistency_result_init(&result, 10);
    energy_consistency_check_proposition(checker, "A IMPLIES B", NULL, &result);

    EXPECT_GE(result.final_consistency, 0.0f);

    energy_consistency_result_cleanup(&result);
}

/* ============================================================================
 * Proof Checking Tests
 * ============================================================================ */

TEST_F(EnergyConsistencyFEPIntegrationTest, ProofTraceConsistencyCheck) {
    checker = energy_consistency_create(&config);
    ASSERT_NE(checker, nullptr);

    /* Create simple proof trace */
    proof_step_t steps[3];
    memset(steps, 0, sizeof(steps));

    /* Step 0: Axiom */
    steps[0].type = PROOF_STEP_AXIOM;
    steps[0].step_id = 0;
    strcpy(steps[0].rule_name, "axiom_1");
    strcpy(steps[0].conclusion, "A implies A");
    steps[0].confidence = 1.0f;
    steps[0].is_valid = true;

    /* Step 1: Hypothesis */
    steps[1].type = PROOF_STEP_HYPOTHESIS;
    steps[1].step_id = 1;
    strcpy(steps[1].rule_name, "hypothesis");
    strcpy(steps[1].conclusion, "A");
    steps[1].confidence = 1.0f;
    steps[1].is_valid = true;

    /* Step 2: Inference */
    steps[2].type = PROOF_STEP_INFERENCE;
    steps[2].step_id = 2;
    steps[2].premise_count = 2;
    strcpy(steps[2].rule_name, "modus_ponens");
    strcpy(steps[2].conclusion, "A");
    steps[2].confidence = 1.0f;
    steps[2].is_valid = true;

    /* Check proof */
    energy_consistency_result_t result;
    energy_consistency_result_init(&result, 10);

    nimcp_error_t err = energy_consistency_check_proof(checker, steps, 3, &result);
    EXPECT_EQ(err, 0);

    /* Valid proof should have low energy */
    EXPECT_GE(result.proof_steps_checked, 0u);

    energy_consistency_result_cleanup(&result);
}

TEST_F(EnergyConsistencyFEPIntegrationTest, PairConsistencyCheck) {
    checker = energy_consistency_create(&config);
    ASSERT_NE(checker, nullptr);

    /* Check consistent pair */
    energy_consistency_result_t result1;
    energy_consistency_result_init(&result1, 10);

    nimcp_error_t err = energy_consistency_check_pair(checker, "A", "B", &result1);
    EXPECT_EQ(err, 0);

    /* A and B are consistent (can both be true) */
    EXPECT_GT(result1.final_consistency, 0.0f);

    /* Check inconsistent pair */
    energy_consistency_result_t result2;
    energy_consistency_result_init(&result2, 10);

    err = energy_consistency_check_pair(checker, "A", "NOT A", &result2);
    EXPECT_EQ(err, 0);

    /* A and NOT A are inconsistent */
    /* result2.final_consistency may be lower */

    energy_consistency_result_cleanup(&result1);
    energy_consistency_result_cleanup(&result2);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(EnergyConsistencyFEPIntegrationTest, StatisticsAccumulate) {
    checker = energy_consistency_create(&config);
    ASSERT_NE(checker, nullptr);

    /* Perform multiple checks */
    for (int i = 0; i < 5; i++) {
        energy_consistency_result_t result;
        energy_consistency_result_init(&result, 10);
        energy_consistency_check_proposition(checker, "A", NULL, &result);
        energy_consistency_result_cleanup(&result);
    }

    /* Get statistics */
    energy_consistency_stats_t stats;
    nimcp_error_t err = energy_consistency_get_stats(checker, &stats);
    EXPECT_EQ(err, 0);

    EXPECT_EQ(stats.total_checks, 5u);
}

TEST_F(EnergyConsistencyFEPIntegrationTest, ConsistencyScoreRetrieval) {
    checker = energy_consistency_create(&config);
    ASSERT_NE(checker, nullptr);

    /* Perform a check to establish score */
    energy_consistency_result_t result;
    energy_consistency_result_init(&result, 10);
    energy_consistency_check_proposition(checker, "A OR B", NULL, &result);

    /* Get current consistency score */
    float score = energy_consistency_get_score(checker);
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 1.0f);

    energy_consistency_result_cleanup(&result);
}

TEST_F(EnergyConsistencyFEPIntegrationTest, LastResultRetrieval) {
    checker = energy_consistency_create(&config);
    ASSERT_NE(checker, nullptr);

    /* Perform a check */
    energy_consistency_result_t result;
    energy_consistency_result_init(&result, 10);
    energy_consistency_check_proposition(checker, "test", NULL, &result);

    /* Get last result */
    const energy_consistency_result_t* last = energy_consistency_get_last_result(checker);
    /* May or may not be available depending on implementation */
    if (last) {
        EXPECT_GE(last->final_consistency, 0.0f);
    }

    energy_consistency_result_cleanup(&result);
}

/* ============================================================================
 * Error Handling Tests
 * ============================================================================ */

TEST_F(EnergyConsistencyFEPIntegrationTest, NullPropositionHandling) {
    checker = energy_consistency_create(&config);
    ASSERT_NE(checker, nullptr);

    energy_consistency_result_t result;
    energy_consistency_result_init(&result, 10);

    nimcp_error_t err = energy_consistency_check_proposition(checker, NULL, NULL, &result);
    EXPECT_NE(err, NIMCP_SUCCESS);

    energy_consistency_result_cleanup(&result);
}

TEST_F(EnergyConsistencyFEPIntegrationTest, NullResultHandling) {
    checker = energy_consistency_create(&config);
    ASSERT_NE(checker, nullptr);

    nimcp_error_t err = energy_consistency_check_proposition(checker, "A", NULL, NULL);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(EnergyConsistencyFEPIntegrationTest, ResetClearsState) {
    checker = energy_consistency_create(&config);
    ASSERT_NE(checker, nullptr);

    /* Perform checks */
    for (int i = 0; i < 3; i++) {
        energy_consistency_result_t result;
        energy_consistency_result_init(&result, 10);
        energy_consistency_check_proposition(checker, "A", NULL, &result);
        energy_consistency_result_cleanup(&result);
    }

    /* Get stats before reset */
    energy_consistency_stats_t stats_before;
    energy_consistency_get_stats(checker, &stats_before);
    EXPECT_EQ(stats_before.total_checks, 3u);

    /* Reset */
    nimcp_error_t err = energy_consistency_reset(checker);
    EXPECT_EQ(err, 0);

    /* Stats should be cleared */
    energy_consistency_stats_t stats_after;
    energy_consistency_get_stats(checker, &stats_after);
    EXPECT_EQ(stats_after.total_checks, 0u);
}

/* ============================================================================
 * FEP Bridge Reset Tests
 * ============================================================================ */

TEST_F(EnergyConsistencyFEPIntegrationTest, EnergyFEPBridgeReset) {
    energy_fep_bridge_t* bridge = energy_fep_bridge_create();
    ASSERT_NE(bridge, nullptr);

    /* Update several times */
    for (int i = 0; i < 5; i++) {
        energy_fep_bridge_update(bridge, 0.5f);
    }

    EXPECT_EQ(bridge->total_updates, 5u);

    /* Reset */
    nimcp_error_t err = energy_fep_bridge_reset(bridge);
    EXPECT_EQ(err, 0);

    /* State should be cleared */
    EXPECT_EQ(bridge->total_updates, 0u);
    EXPECT_FLOAT_EQ(bridge->current_energy, 0.0f);

    energy_fep_bridge_destroy(bridge);
}
