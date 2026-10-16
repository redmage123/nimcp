/**
 * @file test_genius_orchestrator.cpp
 * @brief Unit tests for Genius Math Orchestrator
 *
 * Tests the orchestrator which coordinates all mathematical reasoning
 * components including genius modes, quantum MCTS, evolutionary proof,
 * and energy consistency checking.
 */

#include <gtest/gtest.h>
#include "utils/nimcp_test_base.h"

extern "C" {
#include "cognitive/neuro_symbolic/nimcp_genius_math_orchestrator.h"
}

/**
 * @brief Test fixture for Genius Math Orchestrator tests
 */
class GeniusOrchestratorTest : public NimcpTestBase {
protected:
    genius_math_orchestrator_t* orch;
    orchestrator_config_t config;

    void SetUp() override {
        NimcpTestBase::SetUp();
        orch = NULL;
        memset(&config, 0, sizeof(config));
        genius_orchestrator_get_default_config(&config);
    }

    void TearDown() override {
        if (orch) {
            genius_orchestrator_destroy(orch);
            orch = NULL;
        }
        NimcpTestBase::TearDown();
    }
};

// ============================================================================
// Default Configuration Tests
// ============================================================================

TEST_F(GeniusOrchestratorTest, GetDefaultConfigSucceeds) {
    orchestrator_config_t cfg;
    nimcp_error_t err = genius_orchestrator_get_default_config(&cfg);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(GeniusOrchestratorTest, GetDefaultConfigNullReturnsError) {
    nimcp_error_t err = genius_orchestrator_get_default_config(NULL);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(GeniusOrchestratorTest, DefaultConfigHasReasonableValues) {
    orchestrator_config_t cfg;
    genius_orchestrator_get_default_config(&cfg);

    // Should have some components enabled by default
    EXPECT_NE(cfg.enabled_components, 0u);
    EXPECT_GT(cfg.max_proof_depth, 0u);
    EXPECT_GT(cfg.max_atp_budget, 0.0f);
    EXPECT_LE(cfg.max_atp_budget, 1000.0f);
}

// ============================================================================
// Lifecycle Tests
// ============================================================================

TEST_F(GeniusOrchestratorTest, CreateWithConfigSucceeds) {
    orch = genius_orchestrator_create(&config);
    ASSERT_NE(orch, nullptr);
}

TEST_F(GeniusOrchestratorTest, CreateWithNullConfigSucceeds) {
    orch = genius_orchestrator_create(NULL);
    EXPECT_NE(orch, nullptr);
}

TEST_F(GeniusOrchestratorTest, DestroyNullIsNoOp) {
    genius_orchestrator_destroy(NULL);
    SUCCEED();
}

TEST_F(GeniusOrchestratorTest, CreateDestroyMultipleTimesSucceeds) {
    for (int i = 0; i < 5; i++) {
        orch = genius_orchestrator_create(&config);
        ASSERT_NE(orch, nullptr) << "Failed on iteration " << i;
        genius_orchestrator_destroy(orch);
        orch = NULL;
    }
}

TEST_F(GeniusOrchestratorTest, ResetNullReturnsError) {
    nimcp_error_t err = genius_orchestrator_reset(NULL);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(GeniusOrchestratorTest, ResetSucceeds) {
    orch = genius_orchestrator_create(&config);
    ASSERT_NE(orch, nullptr);

    nimcp_error_t err = genius_orchestrator_reset(orch);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

// ============================================================================
// Component Management Tests
// ============================================================================

TEST_F(GeniusOrchestratorTest, InitComponentsNullReturnsError) {
    nimcp_error_t err = genius_orchestrator_init_components(NULL);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(GeniusOrchestratorTest, InitComponentsSucceeds) {
    orch = genius_orchestrator_create(&config);
    ASSERT_NE(orch, nullptr);

    nimcp_error_t err = genius_orchestrator_init_components(orch);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(GeniusOrchestratorTest, SetConsistencyNullOrchestratorReturnsError) {
    nimcp_error_t err = genius_orchestrator_set_consistency(NULL, NULL);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(GeniusOrchestratorTest, SetConsistencyNullCheckerSucceeds) {
    orch = genius_orchestrator_create(&config);
    ASSERT_NE(orch, nullptr);

    // Setting NULL checker should succeed (clears the checker)
    nimcp_error_t err = genius_orchestrator_set_consistency(orch, NULL);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(GeniusOrchestratorTest, SetHypergraphNullOrchestratorReturnsError) {
    nimcp_error_t err = genius_orchestrator_set_hypergraph(NULL, NULL);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(GeniusOrchestratorTest, SetHypergraphNullHypergraphSucceeds) {
    orch = genius_orchestrator_create(&config);
    ASSERT_NE(orch, nullptr);

    nimcp_error_t err = genius_orchestrator_set_hypergraph(orch, NULL);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(GeniusOrchestratorTest, SetGeniusNullOrchestratorReturnsError) {
    nimcp_error_t err = genius_orchestrator_set_genius(NULL, NULL);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(GeniusOrchestratorTest, SetQuantumMCTSNullOrchestratorReturnsError) {
    nimcp_error_t err = genius_orchestrator_set_quantum_mcts(NULL, NULL);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(GeniusOrchestratorTest, SetFEPPlannerNullOrchestratorReturnsError) {
    nimcp_error_t err = genius_orchestrator_set_fep_planner(NULL, NULL);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(GeniusOrchestratorTest, SetGameTheoryNullOrchestratorReturnsError) {
    nimcp_error_t err = genius_orchestrator_set_game_theory(NULL, NULL);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

// ============================================================================
// Problem Solving Tests
// ============================================================================

TEST_F(GeniusOrchestratorTest, SolveNullOrchestratorReturnsError) {
    math_problem_t problem;
    memset(&problem, 0, sizeof(problem));
    orchestrator_result_t result;
    memset(&result, 0, sizeof(result));

    nimcp_error_t err = genius_orchestrator_solve(NULL, &problem, &result);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(GeniusOrchestratorTest, SolveNullProblemReturnsError) {
    orch = genius_orchestrator_create(&config);
    ASSERT_NE(orch, nullptr);

    orchestrator_result_t result;
    memset(&result, 0, sizeof(result));

    nimcp_error_t err = genius_orchestrator_solve(orch, NULL, &result);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(GeniusOrchestratorTest, SolveNullResultReturnsError) {
    orch = genius_orchestrator_create(&config);
    ASSERT_NE(orch, nullptr);

    math_problem_t problem;
    memset(&problem, 0, sizeof(problem));

    nimcp_error_t err = genius_orchestrator_solve(orch, &problem, NULL);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(GeniusOrchestratorTest, SolveSimpleProblemSucceeds) {
    // Use minimal config: only GENIUS to avoid slow prover/MCTS
    config.enabled_components = ORCH_COMP_GENIUS;
    config.operation_timeout_ms = 100;  // Short timeout for tests

    orch = genius_orchestrator_create(&config);
    ASSERT_NE(orch, nullptr);

    genius_orchestrator_init_components(orch);

    math_problem_t problem;
    memset(&problem, 0, sizeof(problem));
    problem.domain = GENIUS_DOMAIN_NUMBER_THEORY;
    problem.statement = (char*)"Find pattern in 1,2,3,4,5";
    problem.difficulty = 0.1f;

    orchestrator_result_t result;
    memset(&result, 0, sizeof(result));

    nimcp_error_t err = genius_orchestrator_solve(orch, &problem, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

// ============================================================================
// Theorem Proving Tests
// ============================================================================

TEST_F(GeniusOrchestratorTest, ProveNullOrchestratorReturnsError) {
    orchestrator_proof_result_t result;
    memset(&result, 0, sizeof(result));

    nimcp_error_t err = genius_orchestrator_prove(NULL, "P -> Q", &result);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(GeniusOrchestratorTest, ProveNullTheoremReturnsError) {
    orch = genius_orchestrator_create(&config);
    ASSERT_NE(orch, nullptr);

    orchestrator_proof_result_t result;
    memset(&result, 0, sizeof(result));

    nimcp_error_t err = genius_orchestrator_prove(orch, NULL, &result);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

// ============================================================================
// Conjecture Generation Tests
// ============================================================================

TEST_F(GeniusOrchestratorTest, ConjectureNullOrchestratorReturnsError) {
    orchestrator_conjecture_result_t result;
    memset(&result, 0, sizeof(result));

    nimcp_error_t err = genius_orchestrator_conjecture(NULL, GENIUS_DOMAIN_NUMBER_THEORY, NULL, &result);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

// ============================================================================
// Game Theory Analysis Tests
// ============================================================================

TEST_F(GeniusOrchestratorTest, GameTheoryAnalysisNullReturnsError) {
    orchestrator_game_result_t result;
    memset(&result, 0, sizeof(result));

    nimcp_error_t err = genius_orchestrator_game_theory_analysis(NULL, NULL, &result);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

// ============================================================================
// Optimization Tests
// ============================================================================

TEST_F(GeniusOrchestratorTest, OptimizeNullReturnsError) {
    orchestrator_optimization_result_t result;
    memset(&result, 0, sizeof(result));

    nimcp_error_t err = genius_orchestrator_optimize(NULL, NULL, NULL, &result);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_F(GeniusOrchestratorTest, GetStatsNullOrchestratorReturnsError) {
    orchestrator_stats_t stats;
    nimcp_error_t err = genius_orchestrator_get_stats(NULL, &stats);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(GeniusOrchestratorTest, GetStatsNullStatsReturnsError) {
    orch = genius_orchestrator_create(&config);
    ASSERT_NE(orch, nullptr);

    nimcp_error_t err = genius_orchestrator_get_stats(orch, NULL);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(GeniusOrchestratorTest, GetStatsSucceeds) {
    orch = genius_orchestrator_create(&config);
    ASSERT_NE(orch, nullptr);

    orchestrator_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    nimcp_error_t err = genius_orchestrator_get_stats(orch, &stats);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

// ============================================================================
// Modulation Tests
// ============================================================================

TEST_F(GeniusOrchestratorTest, ModulateATPNullReturnsError) {
    nimcp_error_t err = genius_orchestrator_modulate_atp(NULL, 0.5f);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(GeniusOrchestratorTest, ModulateATPSucceeds) {
    orch = genius_orchestrator_create(&config);
    ASSERT_NE(orch, nullptr);

    nimcp_error_t err = genius_orchestrator_modulate_atp(orch, 0.8f);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(GeniusOrchestratorTest, ModulateATPClampsValues) {
    orch = genius_orchestrator_create(&config);
    ASSERT_NE(orch, nullptr);

    // Values outside [0,1] should be clamped
    nimcp_error_t err = genius_orchestrator_modulate_atp(orch, 1.5f);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    err = genius_orchestrator_modulate_atp(orch, -0.5f);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

// ============================================================================
// Bio-Async Tests
// ============================================================================

TEST_F(GeniusOrchestratorTest, RegisterBioAsyncNullReturnsError) {
    nimcp_error_t err = genius_orchestrator_register_bio_async(NULL);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(GeniusOrchestratorTest, RegisterBioAsyncSucceeds) {
    orch = genius_orchestrator_create(&config);
    ASSERT_NE(orch, nullptr);

    nimcp_error_t err = genius_orchestrator_register_bio_async(orch);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(GeniusOrchestratorTest, UnregisterBioAsyncNullReturnsError) {
    nimcp_error_t err = genius_orchestrator_unregister_bio_async(NULL);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(GeniusOrchestratorTest, UnregisterBioAsyncSucceeds) {
    orch = genius_orchestrator_create(&config);
    ASSERT_NE(orch, nullptr);

    genius_orchestrator_register_bio_async(orch);
    nimcp_error_t err = genius_orchestrator_unregister_bio_async(orch);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

// ============================================================================
// Diagnostics Tests
// ============================================================================

TEST_F(GeniusOrchestratorTest, PrintDiagnosticsNullIsNoOp) {
    // Should not crash
    genius_orchestrator_print_diagnostics(NULL);
    SUCCEED();
}

TEST_F(GeniusOrchestratorTest, PrintDiagnosticsSucceeds) {
    orch = genius_orchestrator_create(&config);
    ASSERT_NE(orch, nullptr);

    // Should not crash
    genius_orchestrator_print_diagnostics(orch);
    SUCCEED();
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(GeniusOrchestratorTest, FullWorkflowSucceeds) {
    // Create with minimal config: only GENIUS to avoid slow prover/MCTS
    config.enabled_components = ORCH_COMP_GENIUS;
    config.enable_bio_async = false;
    config.max_atp_budget = 100.0f;
    config.operation_timeout_ms = 100;  // Short timeout for tests

    orch = genius_orchestrator_create(&config);
    ASSERT_NE(orch, nullptr);

    // Initialize components
    nimcp_error_t err = genius_orchestrator_init_components(orch);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Modulate ATP
    err = genius_orchestrator_modulate_atp(orch, 0.9f);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Solve a simple problem
    math_problem_t problem;
    memset(&problem, 0, sizeof(problem));
    problem.domain = GENIUS_DOMAIN_NUMBER_THEORY;
    problem.statement = (char*)"Is 17 prime?";
    problem.difficulty = 0.1f;

    orchestrator_result_t result;
    memset(&result, 0, sizeof(result));
    err = genius_orchestrator_solve(orch, &problem, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Get stats
    orchestrator_stats_t stats;
    err = genius_orchestrator_get_stats(orch, &stats);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GE(stats.operations_total, 1u);

    // Print diagnostics
    genius_orchestrator_print_diagnostics(orch);

    // Reset
    err = genius_orchestrator_reset(orch);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}
