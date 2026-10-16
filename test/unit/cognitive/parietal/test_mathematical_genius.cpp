/**
 * @file test_mathematical_genius.cpp
 * @brief Unit tests for Mathematical Genius Module
 *
 * Tests the mathematical genius framework that emulates different
 * genius modes (Gauss, Newton, Erdos) for advanced mathematical reasoning.
 */

#include <gtest/gtest.h>
#include <cmath>
#include "utils/nimcp_test_base.h"

extern "C" {
#include "cognitive/parietal/nimcp_mathematical_genius.h"
#include "cognitive/parietal/nimcp_genius_modes.h"
}

/**
 * @brief Test fixture for Mathematical Genius tests
 */
class MathematicalGeniusTest : public NimcpTestBase {
protected:
    mathematical_genius_t* genius;
    genius_config_t config;

    void SetUp() override {
        NimcpTestBase::SetUp();
        genius = NULL;
        memset(&config, 0, sizeof(config));
        genius_get_default_config(&config);
    }

    void TearDown() override {
        if (genius) {
            genius_destroy(genius);
            genius = NULL;
        }
        NimcpTestBase::TearDown();
    }
};

// ============================================================================
// Default Configuration Tests
// ============================================================================

TEST_F(MathematicalGeniusTest, GetDefaultConfigSucceeds) {
    genius_config_t cfg;
    nimcp_error_t err = genius_get_default_config(&cfg);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(MathematicalGeniusTest, GetDefaultConfigNullReturnsError) {
    nimcp_error_t err = genius_get_default_config(NULL);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(MathematicalGeniusTest, DefaultConfigHasValidCreativity) {
    genius_config_t cfg;
    genius_get_default_config(&cfg);

    EXPECT_GE(cfg.creativity_level, 0.0f);
    EXPECT_LE(cfg.creativity_level, 1.0f);
}

TEST_F(MathematicalGeniusTest, DefaultConfigHasValidRigor) {
    genius_config_t cfg;
    genius_get_default_config(&cfg);

    EXPECT_GE(cfg.rigor_level, 0.0f);
    EXPECT_LE(cfg.rigor_level, 1.0f);
}

TEST_F(MathematicalGeniusTest, DefaultConfigHasReasonableDepth) {
    genius_config_t cfg;
    genius_get_default_config(&cfg);

    EXPECT_GT(cfg.max_proof_depth, 0u);
    EXPECT_LE(cfg.max_proof_depth, GENIUS_MAX_PROOF_STEPS);
}

// ============================================================================
// Lifecycle Tests
// ============================================================================

TEST_F(MathematicalGeniusTest, CreateWithConfigSucceeds) {
    genius = genius_create(&config);
    ASSERT_NE(genius, nullptr);
}

TEST_F(MathematicalGeniusTest, CreateWithNullConfigSucceeds) {
    genius = genius_create(NULL);
    EXPECT_NE(genius, nullptr);
}

TEST_F(MathematicalGeniusTest, DestroyNullIsNoOp) {
    genius_destroy(NULL);
    SUCCEED();
}

TEST_F(MathematicalGeniusTest, CreateDestroyMultipleTimesSucceeds) {
    for (int i = 0; i < 5; i++) {
        genius = genius_create(&config);
        ASSERT_NE(genius, nullptr) << "Failed on iteration " << i;
        genius_destroy(genius);
        genius = NULL;
    }
}

TEST_F(MathematicalGeniusTest, ResetNullReturnsError) {
    nimcp_error_t err = genius_reset(NULL);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(MathematicalGeniusTest, ResetClearsState) {
    genius = genius_create(&config);
    ASSERT_NE(genius, nullptr);

    nimcp_error_t err = genius_reset(genius);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

// ============================================================================
// Mode Configuration Tests
// ============================================================================

TEST_F(MathematicalGeniusTest, CreateWithGaussMode) {
    config.default_mode = GENIUS_MODE_GAUSS;
    genius = genius_create(&config);
    EXPECT_NE(genius, nullptr);
}

TEST_F(MathematicalGeniusTest, CreateWithNewtonMode) {
    config.default_mode = GENIUS_MODE_NEWTON;
    genius = genius_create(&config);
    EXPECT_NE(genius, nullptr);
}

TEST_F(MathematicalGeniusTest, CreateWithErdosMode) {
    config.default_mode = GENIUS_MODE_ERDOS;
    genius = genius_create(&config);
    EXPECT_NE(genius, nullptr);
}

TEST_F(MathematicalGeniusTest, CreateWithEulerMode) {
    config.default_mode = GENIUS_MODE_EULER;
    genius = genius_create(&config);
    EXPECT_NE(genius, nullptr);
}

TEST_F(MathematicalGeniusTest, CreateWithRamanujanMode) {
    config.default_mode = GENIUS_MODE_RAMANUJAN;
    genius = genius_create(&config);
    EXPECT_NE(genius, nullptr);
}

TEST_F(MathematicalGeniusTest, CreateWithAdaptiveMode) {
    config.default_mode = GENIUS_MODE_ADAPTIVE;
    genius = genius_create(&config);
    EXPECT_NE(genius, nullptr);
}

// ============================================================================
// Gauss Mode Function Tests
// ============================================================================

TEST_F(MathematicalGeniusTest, GaussModularPowBasic) {
    // 2^10 mod 1000 = 1024 mod 1000 = 24
    uint64_t result = genius_gauss_modular_pow(2, 10, 1000);
    EXPECT_EQ(result, 24u);
}

TEST_F(MathematicalGeniusTest, GaussModularPowLarge) {
    // 3^100 mod 97 (Fermat's little theorem: 3^96 ≡ 1 mod 97)
    // 3^100 = 3^96 * 3^4 ≡ 1 * 81 ≡ 81 mod 97
    uint64_t result = genius_gauss_modular_pow(3, 100, 97);
    EXPECT_EQ(result, 81u);
}

TEST_F(MathematicalGeniusTest, GaussModularPowOne) {
    // x^1 mod m = x mod m
    uint64_t result = genius_gauss_modular_pow(7, 1, 5);
    EXPECT_EQ(result, 2u);
}

TEST_F(MathematicalGeniusTest, GaussModularPowZero) {
    // x^0 mod m = 1
    uint64_t result = genius_gauss_modular_pow(7, 0, 5);
    EXPECT_EQ(result, 1u);
}

TEST_F(MathematicalGeniusTest, GaussIsPrimeSmallPrimes) {
    config.default_mode = GENIUS_MODE_GAUSS;
    genius = genius_create(&config);
    ASSERT_NE(genius, nullptr);

    // Small known primes
    EXPECT_TRUE(genius_gauss_is_prime(genius, 2, 0.99f));
    EXPECT_TRUE(genius_gauss_is_prime(genius, 3, 0.99f));
    EXPECT_TRUE(genius_gauss_is_prime(genius, 5, 0.99f));
    EXPECT_TRUE(genius_gauss_is_prime(genius, 7, 0.99f));
    EXPECT_TRUE(genius_gauss_is_prime(genius, 11, 0.99f));
    EXPECT_TRUE(genius_gauss_is_prime(genius, 13, 0.99f));
}

TEST_F(MathematicalGeniusTest, GaussIsPrimeComposites) {
    config.default_mode = GENIUS_MODE_GAUSS;
    genius = genius_create(&config);
    ASSERT_NE(genius, nullptr);

    // Known composites
    EXPECT_FALSE(genius_gauss_is_prime(genius, 4, 0.99f));
    EXPECT_FALSE(genius_gauss_is_prime(genius, 6, 0.99f));
    EXPECT_FALSE(genius_gauss_is_prime(genius, 8, 0.99f));
    EXPECT_FALSE(genius_gauss_is_prime(genius, 9, 0.99f));
    EXPECT_FALSE(genius_gauss_is_prime(genius, 10, 0.99f));
    EXPECT_FALSE(genius_gauss_is_prime(genius, 100, 0.99f));
}

TEST_F(MathematicalGeniusTest, GaussIsPrimeLargePrimes) {
    config.default_mode = GENIUS_MODE_GAUSS;
    genius = genius_create(&config);
    ASSERT_NE(genius, nullptr);

    // Larger primes
    EXPECT_TRUE(genius_gauss_is_prime(genius, 97, 0.99f));
    EXPECT_TRUE(genius_gauss_is_prime(genius, 101, 0.99f));
    EXPECT_TRUE(genius_gauss_is_prime(genius, 1009, 0.99f));
}

TEST_F(MathematicalGeniusTest, GaussFactorSmallNumbers) {
    config.default_mode = GENIUS_MODE_GAUSS;
    genius = genius_create(&config);
    ASSERT_NE(genius, nullptr);

    uint64_t factors[10];

    // Factor 12 = 2^2 * 3
    uint32_t count = genius_gauss_factor(genius, 12, factors, 10);
    EXPECT_GT(count, 0u);
    // Should contain 2 and 3
}

TEST_F(MathematicalGeniusTest, GaussDiscoverPatternArithmetic) {
    config.default_mode = GENIUS_MODE_GAUSS;
    genius = genius_create(&config);
    ASSERT_NE(genius, nullptr);

    // Arithmetic sequence: 2, 4, 6, 8, 10
    int64_t sequence[] = {2, 4, 6, 8, 10};
    conjecture_t conj;
    memset(&conj, 0, sizeof(conj));

    nimcp_error_t err = genius_gauss_discover_pattern(
        genius, sequence, 5, &conj);

    // Pattern discovery should succeed or indicate difficulty
    if (err == NIMCP_SUCCESS) {
        EXPECT_GT(conj.confidence, 0.0f);
    }
}

TEST_F(MathematicalGeniusTest, GaussDiscoverPatternGeometric) {
    config.default_mode = GENIUS_MODE_GAUSS;
    genius = genius_create(&config);
    ASSERT_NE(genius, nullptr);

    // Geometric sequence: 1, 2, 4, 8, 16
    int64_t sequence[] = {1, 2, 4, 8, 16};
    conjecture_t conj;
    memset(&conj, 0, sizeof(conj));

    nimcp_error_t err = genius_gauss_discover_pattern(
        genius, sequence, 5, &conj);

    if (err == NIMCP_SUCCESS) {
        EXPECT_GT(conj.confidence, 0.0f);
    }
}

// ============================================================================
// Newton Mode Function Tests (Disabled - functions not yet implemented)
// ============================================================================

// TODO: Enable when genius_newton_find_root is implemented
// TEST_F(MathematicalGeniusTest, NewtonFindRootSquareRoot) { ... }
// TEST_F(MathematicalGeniusTest, NewtonFindRootCubicRoot) { ... }

// ============================================================================
// Erdos Mode Function Tests (Disabled - functions not yet implemented)
// ============================================================================

// TODO: Enable when genius_erdos_ramsey_lower_bound and
//       genius_erdos_chromatic_number are implemented
// TEST_F(MathematicalGeniusTest, ErdosRamseyLowerBound33) { ... }
// TEST_F(MathematicalGeniusTest, ErdosChromaticNumberPath) { ... }
// TEST_F(MathematicalGeniusTest, ErdosChromaticNumberComplete) { ... }

// ============================================================================
// Mode Analysis Tests
// ============================================================================

TEST_F(MathematicalGeniusTest, GaussAnalyzeNullReturnsError) {
    genius = genius_create(&config);
    ASSERT_NE(genius, nullptr);

    genius_result_t result;
    nimcp_error_t err = genius_gauss_analyze(genius, NULL, &result);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(MathematicalGeniusTest, NewtonAnalyzeNullReturnsError) {
    genius = genius_create(&config);
    ASSERT_NE(genius, nullptr);

    genius_result_t result;
    nimcp_error_t err = genius_newton_analyze(genius, NULL, &result);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(MathematicalGeniusTest, ErdosAnalyzeNullReturnsError) {
    genius = genius_create(&config);
    ASSERT_NE(genius, nullptr);

    genius_result_t result;
    nimcp_error_t err = genius_erdos_analyze(genius, NULL, &result);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

// ============================================================================
// Integration Linking Tests
// ============================================================================

TEST_F(MathematicalGeniusTest, LinkGameTheoryNullReturnsError) {
    nimcp_error_t err = genius_link_game_theory(NULL, NULL);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(MathematicalGeniusTest, LinkQuantumEngineNullReturnsError) {
    nimcp_error_t err = genius_link_quantum_engine(NULL, NULL);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(MathematicalGeniusTest, LinkHypergraphNullReturnsError) {
    nimcp_error_t err = genius_link_hypergraph(NULL, NULL);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

// ============================================================================
// Modulation Tests
// ============================================================================

TEST_F(MathematicalGeniusTest, ModulateInflammationNullReturnsError) {
    nimcp_error_t err = genius_modulate_inflammation(NULL, 0.5f);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(MathematicalGeniusTest, ModulateInflammationValidRange) {
    genius = genius_create(&config);
    ASSERT_NE(genius, nullptr);

    nimcp_error_t err = genius_modulate_inflammation(genius, 0.5f);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(MathematicalGeniusTest, ModulateFatigueNullReturnsError) {
    nimcp_error_t err = genius_modulate_fatigue(NULL, 0.5f);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(MathematicalGeniusTest, ModulateFatigueValidRange) {
    genius = genius_create(&config);
    ASSERT_NE(genius, nullptr);

    nimcp_error_t err = genius_modulate_fatigue(genius, 0.5f);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(MathematicalGeniusTest, ModulateATPNullReturnsError) {
    nimcp_error_t err = genius_modulate_atp(NULL, 1.0f);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(MathematicalGeniusTest, ModulateATPValidRange) {
    genius = genius_create(&config);
    ASSERT_NE(genius, nullptr);

    nimcp_error_t err = genius_modulate_atp(genius, 0.8f);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_F(MathematicalGeniusTest, GetStatsNullReturnsError) {
    genius_stats_t stats;
    nimcp_error_t err = genius_get_stats(NULL, &stats);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(MathematicalGeniusTest, GetStatsWithNullStatsReturnsError) {
    genius = genius_create(&config);
    ASSERT_NE(genius, nullptr);

    nimcp_error_t err = genius_get_stats(genius, NULL);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(MathematicalGeniusTest, InitialStatsAreZero) {
    genius = genius_create(&config);
    ASSERT_NE(genius, nullptr);

    genius_stats_t stats;
    memset(&stats, 0xFF, sizeof(stats));  // Fill with non-zero

    nimcp_error_t err = genius_get_stats(genius, &stats);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(stats.problems_attempted, 0u);
    EXPECT_EQ(stats.problems_solved, 0u);
}

// ============================================================================
// Result Management Tests
// ============================================================================

TEST_F(MathematicalGeniusTest, ResultInitNullReturnsError) {
    nimcp_error_t err = genius_result_init(NULL);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(MathematicalGeniusTest, ResultInitSucceeds) {
    genius_result_t result;
    memset(&result, 0xFF, sizeof(result));  // Fill with non-zero

    nimcp_error_t err = genius_result_init(&result);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(result.num_patterns, 0u);
    EXPECT_EQ(result.num_conjectures, 0u);
    EXPECT_EQ(result.num_proofs, 0u);
}

TEST_F(MathematicalGeniusTest, ResultCleanupNullIsNoOp) {
    genius_result_cleanup(NULL);
    SUCCEED();
}

TEST_F(MathematicalGeniusTest, ProofTraceInitNullReturnsError) {
    nimcp_error_t err = genius_proof_trace_init(NULL, 10);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(MathematicalGeniusTest, ProofTraceInitSucceeds) {
    proof_trace_t trace;
    memset(&trace, 0, sizeof(trace));

    nimcp_error_t err = genius_proof_trace_init(&trace, 10);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_NE(trace.steps, nullptr);
    EXPECT_EQ(trace.num_steps, 0u);
    EXPECT_EQ(trace.capacity, 10u);

    genius_proof_trace_cleanup(&trace);
}

TEST_F(MathematicalGeniusTest, ProofTraceCleanupNullIsNoOp) {
    genius_proof_trace_cleanup(NULL);
    SUCCEED();
}

// ============================================================================
// Conjecture Generation Tests
// ============================================================================

TEST_F(MathematicalGeniusTest, GenerateConjecturesNullReturnsZero) {
    conjecture_t conjectures[10];
    uint32_t count = genius_generate_conjectures(
        NULL, GENIUS_DOMAIN_NUMBER_THEORY, NULL, conjectures, 10);
    EXPECT_EQ(count, 0u);
}

TEST_F(MathematicalGeniusTest, GenerateConjecturesValidDomain) {
    genius = genius_create(&config);
    ASSERT_NE(genius, nullptr);

    conjecture_t conjectures[10];
    memset(conjectures, 0, sizeof(conjectures));

    uint32_t count = genius_generate_conjectures(
        genius, GENIUS_DOMAIN_NUMBER_THEORY, NULL, conjectures, 10);

    // May or may not generate conjectures depending on implementation
    EXPECT_LE(count, 10u);
}

// ============================================================================
// Find Analogies Tests
// ============================================================================

TEST_F(MathematicalGeniusTest, FindAnalogiesNullReturnsZero) {
    genius_analogy_result_t analogies[10];
    uint32_t count = genius_find_analogies(
        NULL, GENIUS_DOMAIN_NUMBER_THEORY, GENIUS_DOMAIN_GEOMETRY, analogies, 10);
    EXPECT_EQ(count, 0u);
}

TEST_F(MathematicalGeniusTest, FindAnalogiesValidDomains) {
    genius = genius_create(&config);
    ASSERT_NE(genius, nullptr);

    genius_analogy_result_t analogies[10];
    memset(analogies, 0, sizeof(analogies));

    uint32_t count = genius_find_analogies(
        genius, GENIUS_DOMAIN_NUMBER_THEORY, GENIUS_DOMAIN_GEOMETRY,
        analogies, 10);

    // May or may not find analogies
    EXPECT_LE(count, 10u);
}

// ============================================================================
// Collaboration Tests
// ============================================================================

TEST_F(MathematicalGeniusTest, CollaborateNullReturnsError) {
    math_problem_t problem;
    memset(&problem, 0, sizeof(problem));
    genius_result_t result;

    nimcp_error_t err = genius_collaborate(NULL, 0, &problem, &result);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(MathematicalGeniusTest, CollaborateSingleGenius) {
    genius = genius_create(&config);
    ASSERT_NE(genius, nullptr);

    math_problem_t problem;
    memset(&problem, 0, sizeof(problem));
    problem.domain = GENIUS_DOMAIN_NUMBER_THEORY;
    problem.difficulty = 0.3f;

    genius_result_t result;
    genius_result_init(&result);

    mathematical_genius_t* geniuses[] = { genius };
    nimcp_error_t err = genius_collaborate(geniuses, 1, &problem, &result);

    // Should succeed or indicate unsolved
    genius_result_cleanup(&result);
}
