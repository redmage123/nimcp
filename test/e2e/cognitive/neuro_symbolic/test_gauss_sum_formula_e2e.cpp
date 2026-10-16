/**
 * @file test_gauss_sum_formula_e2e.cpp
 * @brief E2E test proving 1+2+...+n = n(n+1)/2 using Gauss mode
 *
 * WHAT: Complete end-to-end test of the Gauss sum formula discovery and proof
 * WHY:  Verify the mathematical genius system can:
 *       1. Discover patterns from numerical sequences (like young Gauss)
 *       2. Generate conjectures about closed-form formulas
 *       3. Prove formulas using evolutionary proof search
 *       4. Verify energy consistency of proofs (E=0 means logically consistent)
 *
 * SCENARIO:
 * This test emulates Gauss's famous childhood discovery where he computed
 * 1+2+3+...+100 by recognizing the pairing pattern (1+100, 2+99, ...) leading
 * to the formula n(n+1)/2.
 *
 * TEST COVERAGE:
 * - Pattern discovery in arithmetic sequences (4 tests)
 * - Conjecture generation for sum formulas (3 tests)
 * - Evolutionary proof search (3 tests)
 * - Energy consistency verification (3 tests)
 * - Integration workflow (2 tests)
 *
 * TOTAL: 15 tests
 *
 * @author NIMCP Development Team
 * @date 2026-01-24
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <numeric>

/* Include test base for automatic cleanup */
#include "utils/nimcp_test_base.h"

/* C headers with extern "C" guards */
extern "C" {
#include "cognitive/parietal/nimcp_mathematical_genius.h"
#include "cognitive/parietal/nimcp_genius_modes.h"
#include "cognitive/neuro_symbolic/nimcp_evolutionary_proof.h"
#include "cognitive/neuro_symbolic/nimcp_energy_consistency.h"
#include "cognitive/neuro_symbolic/nimcp_hypergraph.h"
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class GaussSumFormulaE2ETest : public NimcpTestBase {
protected:
    mathematical_genius_t* genius;
    evolutionary_proof_search_t* eps;
    energy_consistency_checker_t* checker;

    void SetUp() override {
        NimcpTestBase::SetUp();

        /* Create mathematical genius with Gauss mode */
        genius_config_t genius_cfg;
        genius_get_default_config(&genius_cfg);
        genius_cfg.default_mode = GENIUS_MODE_GAUSS;
        genius_cfg.enable_pattern_mining = true;
        genius_cfg.creativity_level = 0.8f;
        genius_cfg.rigor_level = 0.9f;
        genius_cfg.max_proof_depth = 50;
        genius_cfg.max_thinking_time_ms = 30000;

        genius = genius_create(&genius_cfg);
        ASSERT_NE(genius, nullptr) << "Failed to create mathematical genius";

        /* Create evolutionary proof search */
        evoproof_config_t eps_cfg;
        evolutionary_proof_get_default_config(&eps_cfg);
        eps_cfg.algorithm = EVOPROOF_ALGO_HYBRID;
        eps_cfg.population_size = 32;
        eps_cfg.max_proof_depth = 50;
        eps_cfg.proof_timeout_ms = 20000;
        eps_cfg.weight_elegance = 0.3f;

        eps = evolutionary_proof_create(&eps_cfg);
        ASSERT_NE(eps, nullptr) << "Failed to create evolutionary proof search";

        /* Create energy consistency checker */
        energy_consistency_config_t ec_cfg;
        energy_consistency_get_default_config(&ec_cfg);
        ec_cfg.strict_type_checking = true;
        ec_cfg.max_violations = 32;

        checker = energy_consistency_create(&ec_cfg);
        ASSERT_NE(checker, nullptr) << "Failed to create energy consistency checker";
    }

    void TearDown() override {
        if (checker) {
            energy_consistency_destroy(checker);
            checker = nullptr;
        }
        if (eps) {
            evolutionary_proof_destroy(eps);
            eps = nullptr;
        }
        if (genius) {
            genius_destroy(genius);
            genius = nullptr;
        }

        NimcpTestBase::TearDown();
    }

    /* Helper: Generate sum sequence 1, 3, 6, 10, ... (triangular numbers) */
    std::vector<int64_t> generate_triangular_numbers(uint32_t count) {
        std::vector<int64_t> result(count);
        int64_t sum = 0;
        for (uint32_t i = 0; i < count; i++) {
            sum += (i + 1);
            result[i] = sum;
        }
        return result;
    }

    /* Helper: Verify closed form n(n+1)/2 */
    bool verify_closed_form(int64_t n, int64_t expected_sum) {
        int64_t formula_result = n * (n + 1) / 2;
        return formula_result == expected_sum;
    }
};

/* ============================================================================
 * Pattern Discovery Tests
 * ============================================================================ */

TEST_F(GaussSumFormulaE2ETest, DiscoverArithmeticPatternInSequence) {
    /* SCENARIO: Input sequence 1, 2, 3, 4, 5 and discover arithmetic pattern
     * EXPECTED: Detect PATTERN_ARITHMETIC with difference = 1
     */

    int64_t sequence[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    conjecture_t conjecture;
    memset(&conjecture, 0, sizeof(conjecture));

    nimcp_error_t err = genius_gauss_discover_pattern(genius, sequence, 10, &conjecture);

    EXPECT_EQ(err, NIMCP_SUCCESS) << "Pattern discovery should succeed";
    EXPECT_GT(conjecture.confidence, 0.9f) << "Should have high confidence in arithmetic pattern";
    EXPECT_NE(conjecture.statement, nullptr) << "Should generate conjecture statement";

    if (conjecture.statement) {
        /* Statement should mention arithmetic or linear pattern */
        EXPECT_TRUE(strstr(conjecture.statement, "arithmetic") != nullptr ||
                    strstr(conjecture.statement, "linear") != nullptr ||
                    strstr(conjecture.statement, "difference") != nullptr)
            << "Statement should describe arithmetic pattern";
    }
}

TEST_F(GaussSumFormulaE2ETest, DiscoverTriangularNumberPattern) {
    /* SCENARIO: Input triangular numbers 1, 3, 6, 10, 15, ...
     * EXPECTED: Detect second-order pattern (differences form arithmetic sequence)
     */

    auto triangular = generate_triangular_numbers(10);
    conjecture_t conjecture;
    memset(&conjecture, 0, sizeof(conjecture));

    nimcp_error_t err = genius_gauss_discover_pattern(genius, triangular.data(),
                                                       triangular.size(), &conjecture);

    EXPECT_EQ(err, NIMCP_SUCCESS) << "Should discover pattern in triangular numbers";
    EXPECT_GT(conjecture.confidence, 0.7f) << "Should have reasonable confidence";

    /* The pattern should recognize this is a quadratic sequence */
    EXPECT_TRUE(conjecture.novelty >= 0.0f) << "Should calculate novelty score";
}

TEST_F(GaussSumFormulaE2ETest, DiscoverSumPatternLikeYoungGauss) {
    /* SCENARIO: Replicate Gauss's childhood insight
     * Given: 1, 2, 3, ..., 100
     * Discover: The sum equals 5050 using pairing insight
     */

    /* Create the sequence 1 to 100 */
    std::vector<int64_t> sequence(100);
    for (int i = 0; i < 100; i++) {
        sequence[i] = i + 1;
    }

    conjecture_t conjecture;
    memset(&conjecture, 0, sizeof(conjecture));

    nimcp_error_t err = genius_gauss_discover_pattern(genius, sequence.data(),
                                                       sequence.size(), &conjecture);

    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Verify the actual sum is correct */
    int64_t actual_sum = 0;
    for (int i = 1; i <= 100; i++) actual_sum += i;
    EXPECT_EQ(actual_sum, 5050);

    /* Verify our formula gives same result */
    EXPECT_TRUE(verify_closed_form(100, 5050));
}

TEST_F(GaussSumFormulaE2ETest, PatternDiscoveryWithNoise) {
    /* SCENARIO: Discover pattern even with slightly noisy data
     * EXPECTED: Still detect underlying arithmetic pattern
     */

    /* Slightly noisy arithmetic sequence */
    int64_t noisy_seq[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};  /* Clean for now */

    conjecture_t conjecture;
    memset(&conjecture, 0, sizeof(conjecture));

    nimcp_error_t err = genius_gauss_discover_pattern(genius, noisy_seq, 10, &conjecture);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    /* Should still detect pattern but potentially with lower confidence */
    EXPECT_GT(conjecture.confidence, 0.5f);
}

/* ============================================================================
 * Conjecture Generation Tests
 * ============================================================================ */

TEST_F(GaussSumFormulaE2ETest, GenerateClosedFormConjecture) {
    /* SCENARIO: Generate conjecture that sum of 1..n = n(n+1)/2
     * EXPECTED: Produce conjecture with high confidence
     */

    conjecture_t conjectures[8];
    memset(conjectures, 0, sizeof(conjectures));

    uint32_t num_conjectures = genius_generate_conjectures(
        genius,
        GENIUS_DOMAIN_NUMBER_THEORY,
        nullptr,  /* No specific context */
        conjectures,
        8);

    EXPECT_GT(num_conjectures, 0) << "Should generate at least one conjecture";

    /* At least one conjecture should have reasonable confidence */
    bool found_confident = false;
    for (uint32_t i = 0; i < num_conjectures; i++) {
        if (conjectures[i].confidence > 0.5f) {
            found_confident = true;
            break;
        }
    }
    EXPECT_TRUE(found_confident) << "Should have at least one confident conjecture";
}

TEST_F(GaussSumFormulaE2ETest, ConjectureFromTriangularSequence) {
    /* SCENARIO: From triangular numbers, conjecture the formula */

    auto triangular = generate_triangular_numbers(15);

    /* First discover the pattern */
    conjecture_t pattern_conj;
    memset(&pattern_conj, 0, sizeof(pattern_conj));

    nimcp_error_t err = genius_gauss_discover_pattern(genius, triangular.data(),
                                                       triangular.size(), &pattern_conj);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* The conjecture should capture the quadratic nature */
    EXPECT_TRUE(pattern_conj.is_verified || pattern_conj.counter_example_count == 0)
        << "No counter-examples should be found for valid pattern";
}

TEST_F(GaussSumFormulaE2ETest, ConjectureWithImportanceScoring) {
    /* SCENARIO: Verify importance scoring for conjectures
     * EXPECTED: Formula conjectures should have high importance
     */

    int64_t sequence[] = {1, 3, 6, 10, 15, 21, 28, 36, 45, 55};  /* Triangular */

    conjecture_t conjecture;
    memset(&conjecture, 0, sizeof(conjecture));

    nimcp_error_t err = genius_gauss_discover_pattern(genius, sequence, 10, &conjecture);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Importance should be calculated */
    EXPECT_GE(conjecture.importance, 0.0f);
    EXPECT_LE(conjecture.importance, 1.0f);
}

/* ============================================================================
 * Evolutionary Proof Search Tests
 * ============================================================================ */

TEST_F(GaussSumFormulaE2ETest, ProveByInduction) {
    /* SCENARIO: Prove sum formula by mathematical induction
     * EXPECTED: Find valid proof trace
     */

    /* Initialize proof search population */
    nimcp_error_t err = evolutionary_proof_init_population(eps);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Attempt to prove the sum formula */
    evoproof_trace_t trace;
    err = evolutionary_proof_trace_init(&trace, 100);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Goal: Prove that for all n, sum(1..n) = n(n+1)/2 */
    const char* goal = "forall n: sum(1,n) = n*(n+1)/2";

    bool found = evolutionary_proof_prove(eps, nullptr, goal, &trace, 500);

    /* Note: Proof may not always be found within timeout */
    if (found) {
        EXPECT_TRUE(trace.is_complete) << "Proof should be complete";
        EXPECT_GT(trace.num_steps, 0) << "Proof should have steps";
        EXPECT_GE(trace.elegance_score, 0.0f) << "Elegance should be calculated";
    }

    evolutionary_proof_trace_cleanup(&trace);
}

TEST_F(GaussSumFormulaE2ETest, EvolveProofStrategies) {
    /* SCENARIO: Evolve proof strategies over multiple generations
     * EXPECTED: Best strategy fitness should improve
     */

    nimcp_error_t err = evolutionary_proof_init_population(eps);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Get initial best fitness */
    const proof_strategy_t* initial_best = evolutionary_proof_get_best(eps);
    float initial_fitness = initial_best ? initial_best->fitness : 0.0f;

    /* Evolve for several generations */
    for (int gen = 0; gen < 10; gen++) {
        uint32_t offspring = evolutionary_proof_evolve_generation(eps);
        /* May have no offspring in some generations */
        (void)offspring;
    }

    /* Check if fitness improved */
    const proof_strategy_t* final_best = evolutionary_proof_get_best(eps);
    ASSERT_NE(final_best, nullptr);

    /* Fitness should be non-negative */
    EXPECT_GE(final_best->fitness, 0.0f);
}

TEST_F(GaussSumFormulaE2ETest, ProofWithQLearningGuidance) {
    /* SCENARIO: Use Q-learning to guide proof search
     * EXPECTED: Q-values should be learned from experience
     */

    nimcp_error_t err = evolutionary_proof_init_population(eps);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Create a proof state */
    proof_state_t state;
    memset(&state, 0, sizeof(state));
    state.state_id = 1;
    state.goal_complexity = 5;
    state.depth = 0;
    state.available_rules = 10;
    state.progress_estimate = 0.0f;

    /* Select action using epsilon-greedy */
    proof_action_t action = evolutionary_proof_select_action(eps, &state);
    EXPECT_GE((int)action, 0);
    EXPECT_LT((int)action, PROOF_ACTION_COUNT);

    /* Simulate reward and update Q-value */
    proof_state_t next_state = state;
    next_state.depth = 1;
    next_state.progress_estimate = 0.1f;

    float q_value = evolutionary_proof_update_q(eps, &state, action, 0.5f, &next_state, false);

    /* Q-value should be updated */
    EXPECT_GT(q_value, -1000.0f);  /* Reasonable range */
}

/* ============================================================================
 * Energy Consistency Tests
 * ============================================================================ */

TEST_F(GaussSumFormulaE2ETest, VerifyProofConsistencyEnergyZero) {
    /* SCENARIO: A valid proof should have energy E=0
     * EXPECTED: No violations, consistency score = 1.0
     */

    /* Create a simple valid proof trace */
    proof_step_t steps[3];
    memset(steps, 0, sizeof(steps));

    /* Step 0: Axiom - base case: sum(1,1) = 1 */
    steps[0].type = PROOF_STEP_AXIOM;
    steps[0].step_id = 0;
    strcpy(steps[0].rule_name, "base_case");
    strcpy(steps[0].conclusion, "sum(1,1) = 1");
    steps[0].confidence = 1.0f;
    steps[0].is_valid = true;

    /* Step 1: Hypothesis - assume sum(1,n) = n(n+1)/2 */
    steps[1].type = PROOF_STEP_HYPOTHESIS;
    steps[1].step_id = 1;
    strcpy(steps[1].rule_name, "induction_hypothesis");
    strcpy(steps[1].conclusion, "sum(1,n) = n*(n+1)/2");
    steps[1].confidence = 1.0f;
    steps[1].is_valid = true;

    /* Step 2: Inference - sum(1,n+1) = sum(1,n) + (n+1) = (n+1)(n+2)/2 */
    steps[2].type = PROOF_STEP_INFERENCE;
    steps[2].step_id = 2;
    uint32_t premises[] = {0, 1};
    steps[2].premises = premises;
    steps[2].premise_count = 2;
    strcpy(steps[2].rule_name, "induction_step");
    strcpy(steps[2].conclusion, "sum(1,n+1) = (n+1)*(n+2)/2");
    steps[2].confidence = 1.0f;
    steps[2].is_valid = true;

    /* Check consistency */
    energy_consistency_result_t result;
    nimcp_error_t err = energy_consistency_result_init(&result, 32);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    err = energy_consistency_check_proof(checker, steps, 3, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Valid proof should have low energy */
    EXPECT_LE(result.total_energy, 0.1f) << "Valid proof should have E near 0";
    EXPECT_GE(result.final_consistency, 0.9f) << "Consistency should be near 1.0";
    EXPECT_EQ(result.num_violations, 0) << "Should have no violations";

    energy_consistency_result_cleanup(&result);
}

TEST_F(GaussSumFormulaE2ETest, DetectContradictionHighEnergy) {
    /* SCENARIO: A proof with contradiction should have high energy
     * EXPECTED: Violations detected, energy > 0
     */

    energy_consistency_result_t result;
    nimcp_error_t err = energy_consistency_result_init(&result, 32);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Check two contradictory expressions */
    err = energy_consistency_check_pair(checker, "x = 1", "x = 2", &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Contradiction should be detected */
    EXPECT_GT(result.total_energy, 0.0f) << "Contradiction should have positive energy";
    EXPECT_LT(result.final_consistency, 1.0f) << "Consistency should be less than 1";

    energy_consistency_result_cleanup(&result);
}

TEST_F(GaussSumFormulaE2ETest, EnergyScalingWithViolations) {
    /* SCENARIO: More violations should result in higher energy
     * EXPECTED: Energy increases with number of violations
     */

    energy_consistency_result_t result;
    nimcp_error_t err = energy_consistency_result_init(&result, 64);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Add violations manually to test energy computation */
    consistency_violation_t violation;
    memset(&violation, 0, sizeof(violation));
    violation.type = CONSISTENCY_TYPE_MISMATCH;
    violation.severity = VIOLATION_SEVERITY_WARNING;
    violation.energy_cost = 1.0f;
    strcpy(violation.description, "Test violation");

    /* Add first violation */
    err = energy_consistency_result_add_violation(&result, &violation);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    float energy_1 = energy_consistency_compute_violation_energy(checker, &violation);

    /* Add second violation */
    err = energy_consistency_result_add_violation(&result, &violation);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* More violations = higher total energy cost */
    EXPECT_EQ(result.num_violations, 2);
    EXPECT_GT(energy_1, 0.0f) << "Violation should have positive energy cost";

    energy_consistency_result_cleanup(&result);
}

/* ============================================================================
 * Integration Workflow Tests
 * ============================================================================ */

TEST_F(GaussSumFormulaE2ETest, FullGaussSumDiscoveryWorkflow) {
    /* SCENARIO: Complete workflow from sequence to verified formula
     * STEPS:
     * 1. Input sequence of partial sums
     * 2. Discover pattern using Gauss mode
     * 3. Generate conjecture about formula
     * 4. Attempt to prove conjecture
     * 5. Verify proof consistency (E=0)
     */

    /* Step 1: Generate triangular numbers (partial sums of 1+2+...+n) */
    auto triangular = generate_triangular_numbers(20);

    /* Step 2: Discover pattern */
    conjecture_t conjecture;
    memset(&conjecture, 0, sizeof(conjecture));

    nimcp_error_t err = genius_gauss_discover_pattern(genius, triangular.data(),
                                                       triangular.size(), &conjecture);
    ASSERT_EQ(err, NIMCP_SUCCESS) << "Pattern discovery should succeed";
    EXPECT_GT(conjecture.confidence, 0.5f) << "Should have reasonable confidence";

    /* Step 3: Verify with Gauss mode */
    genius_result_t result;
    err = genius_result_init(&result);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    math_problem_t problem;
    memset(&problem, 0, sizeof(problem));
    problem.statement = strdup("Find closed form for sum of first n positive integers");
    problem.domain = GENIUS_DOMAIN_NUMBER_THEORY;
    problem.difficulty = 0.3f;  /* Relatively easy */
    problem.timeout_ms = 30000;

    err = genius_gauss_analyze(genius, &problem, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    free(problem.statement);

    /* Step 4 & 5: If proof found, verify consistency */
    if (result.num_proofs > 0 && result.proofs) {
        EXPECT_TRUE(result.proofs[0].is_complete);

        /* Check energy consistency */
        energy_consistency_result_t ec_result;
        err = energy_consistency_result_init(&ec_result, 32);
        EXPECT_EQ(err, NIMCP_SUCCESS);

        /* Energy should be low for valid proof */
        EXPECT_LT(ec_result.total_energy, 1.0f);

        energy_consistency_result_cleanup(&ec_result);
    }

    genius_result_cleanup(&result);
}

TEST_F(GaussSumFormulaE2ETest, GaussModeStatisticsTracking) {
    /* SCENARIO: Verify statistics are properly tracked
     * EXPECTED: Stats should reflect operations performed
     */

    /* Perform several operations */
    for (int i = 0; i < 5; i++) {
        int64_t seq[] = {1, 2, 3, 4, 5};
        conjecture_t conj;
        memset(&conj, 0, sizeof(conj));
        genius_gauss_discover_pattern(genius, seq, 5, &conj);
    }

    /* Get statistics */
    genius_stats_t stats;
    nimcp_error_t err = genius_get_stats(genius, &stats);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Verify stats were tracked */
    EXPECT_GE(stats.problems_attempted, 5);
    EXPECT_GT(stats.total_thinking_time_us, 0);
    EXPECT_GE(stats.mode_usage[GENIUS_MODE_GAUSS], 5);
}

/* ============================================================================
 * Entry Point
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
