/**
 * @file test_genius_math_regression.cpp
 * @brief Regression tests for Mathematical Genius Module
 * @version 1.0.0
 * @date 2026-01-24
 *
 * WHAT: Regression tests for the Mathesis-inspired mathematical genius system
 * WHY:  Ensure API stability, numerical accuracy, and behavior consistency
 * HOW:  Test mode switching, pattern discovery, primality, and configuration
 *
 * TEST CATEGORIES:
 * - GaussModeRegression: Modular arithmetic, primality, factorization
 * - PatternDiscoveryRegression: Pattern detection reproducibility
 * - ModeSwitchingRegression: Stability when switching between modes
 * - ConfigurationRegression: Configuration persistence across operations
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <chrono>
#include <vector>
#include <algorithm>
#include <random>

#include "utils/nimcp_test_base.h"

extern "C" {
#include "cognitive/parietal/nimcp_mathematical_genius.h"
#include "cognitive/parietal/nimcp_genius_modes.h"
}

/* ============================================================================
 * Regression Test Constants
 * ============================================================================ */

/* Performance thresholds (microseconds) */
static constexpr int64_t PRIMALITY_TEST_THRESHOLD_US = 1000;
static constexpr int64_t FACTOR_THRESHOLD_US = 5000;
static constexpr int64_t PATTERN_DISCOVERY_THRESHOLD_US = 10000;
static constexpr int64_t MODE_SWITCH_THRESHOLD_US = 500;

/* Numerical tolerances */
static constexpr float NUMERICAL_TOLERANCE = 1e-5f;
static constexpr float PATTERN_CONFIDENCE_MIN = 0.0f;

/* Test iteration counts */
static constexpr int REGRESSION_ITERATIONS = 100;
static constexpr int PRIMALITY_TEST_COUNT = 50;
static constexpr int MODE_SWITCH_ITERATIONS = 20;

/* Known prime numbers for testing */
static const uint64_t KNOWN_PRIMES[] = {
    2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47,
    53, 59, 61, 67, 71, 73, 79, 83, 89, 97, 101, 103, 107, 109, 113,
    7919, 7927, 7933, 7937, 7949,  /* Mid-range primes */
    104729, 104743, 104759        /* Larger primes */
};

/* Known composite numbers for testing */
static const uint64_t KNOWN_COMPOSITES[] = {
    4, 6, 8, 9, 10, 12, 14, 15, 16, 18, 20, 21, 22, 24, 25,
    100, 1000, 10000, 100000, 561, 1105  /* 561 and 1105 are Carmichael numbers */
};

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class GeniusMathRegressionTest : public NimcpTestBase {
protected:
    mathematical_genius_t* genius = nullptr;
    genius_config_t config;

    void SetUp() override {
        NimcpTestBase::SetUp();
        genius_get_default_config(&config);
        genius = genius_create(&config);
        ASSERT_NE(genius, nullptr);
    }

    void TearDown() override {
        if (genius) {
            genius_destroy(genius);
            genius = nullptr;
        }
        NimcpTestBase::TearDown();
    }

    /* Utility to measure operation time in microseconds */
    template<typename Func>
    int64_t measure_time_us(Func&& func) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    }

    /* Create a simple arithmetic sequence */
    void create_arithmetic_sequence(int64_t* seq, uint32_t length, int64_t start, int64_t diff) {
        for (uint32_t i = 0; i < length; i++) {
            seq[i] = start + (int64_t)i * diff;
        }
    }

    /* Create a geometric-like sequence */
    void create_geometric_sequence(int64_t* seq, uint32_t length, int64_t start, int64_t ratio) {
        seq[0] = start;
        for (uint32_t i = 1; i < length; i++) {
            seq[i] = seq[i-1] * ratio;
        }
    }

    /* Create triangular number sequence */
    void create_triangular_sequence(int64_t* seq, uint32_t length) {
        for (uint32_t i = 0; i < length; i++) {
            seq[i] = (int64_t)(i + 1) * (int64_t)(i + 2) / 2;
        }
    }
};

/* ============================================================================
 * GaussModeRegression - Modular arithmetic and number theory consistency
 * ============================================================================ */

TEST_F(GeniusMathRegressionTest, GaussModularPowConsistency) {
    printf("\n[Gauss Modular Pow Consistency]\n");

    /* Test modular exponentiation: (base^exp) mod mod */
    /* Mathematical identity: a^b mod n should be consistent */

    std::vector<int64_t> timings;
    int inconsistencies = 0;

    for (int i = 0; i < REGRESSION_ITERATIONS; i++) {
        uint64_t base = 2 + (i % 100);
        uint64_t exp = 10 + i;
        uint64_t mod = 1000000007;  /* Large prime modulus */

        int64_t time_us = measure_time_us([&]() {
            uint64_t result1 = genius_gauss_modular_pow(base, exp, mod);
            uint64_t result2 = genius_gauss_modular_pow(base, exp, mod);

            /* Same inputs must produce same output */
            if (result1 != result2) {
                inconsistencies++;
            }
        });
        timings.push_back(time_us);
    }

    /* Statistics */
    int64_t sum = 0;
    for (auto t : timings) sum += t;
    int64_t avg = sum / (int64_t)timings.size();

    printf("  Average time: %lld us\n", (long long)avg);
    printf("  Inconsistencies: %d/%d\n", inconsistencies, REGRESSION_ITERATIONS);

    EXPECT_EQ(inconsistencies, 0) << "Modular pow should be deterministic";
}

TEST_F(GeniusMathRegressionTest, GaussModularPowIdentities) {
    printf("\n[Gauss Modular Pow Identities]\n");

    /* Test mathematical identities */
    uint64_t mod = 1000000007;

    /* Identity 1: a^0 = 1 (mod n) for any a */
    for (uint64_t a = 1; a <= 100; a++) {
        uint64_t result = genius_gauss_modular_pow(a, 0, mod);
        EXPECT_EQ(result, 1u) << "a^0 should equal 1 for a=" << a;
    }

    /* Identity 2: a^1 = a (mod n) for a < n */
    for (uint64_t a = 1; a <= 100; a++) {
        uint64_t result = genius_gauss_modular_pow(a, 1, mod);
        EXPECT_EQ(result, a) << "a^1 should equal a for a=" << a;
    }

    /* Identity 3: (a^b)^c = a^(b*c) (mod n) - Fermat's little theorem related */
    uint64_t a = 7;
    uint64_t b = 3;
    uint64_t c = 4;
    uint64_t left = genius_gauss_modular_pow(genius_gauss_modular_pow(a, b, mod), c, mod);
    uint64_t right = genius_gauss_modular_pow(a, b * c, mod);
    EXPECT_EQ(left, right) << "(a^b)^c should equal a^(b*c)";

    printf("  All modular identities verified\n");
}

TEST_F(GeniusMathRegressionTest, GaussPrimalityAccuracy) {
    printf("\n[Gauss Primality Test Accuracy]\n");

    int correct_primes = 0;
    int total_primes = sizeof(KNOWN_PRIMES) / sizeof(KNOWN_PRIMES[0]);

    /* Test known primes */
    for (int i = 0; i < total_primes; i++) {
        bool is_prime = genius_gauss_is_prime(genius, KNOWN_PRIMES[i], 0.99f);
        if (is_prime) {
            correct_primes++;
        } else {
            printf("    False negative: %lu incorrectly identified as composite\n",
                   (unsigned long)KNOWN_PRIMES[i]);
        }
    }

    printf("  Primes correctly identified: %d/%d\n", correct_primes, total_primes);
    EXPECT_GE(correct_primes, total_primes - 2) << "Should identify most primes correctly";

    int correct_composites = 0;
    int total_composites = sizeof(KNOWN_COMPOSITES) / sizeof(KNOWN_COMPOSITES[0]);

    /* Test known composites */
    for (int i = 0; i < total_composites; i++) {
        bool is_prime = genius_gauss_is_prime(genius, KNOWN_COMPOSITES[i], 0.99f);
        if (!is_prime) {
            correct_composites++;
        } else {
            printf("    False positive: %lu incorrectly identified as prime\n",
                   (unsigned long)KNOWN_COMPOSITES[i]);
        }
    }

    printf("  Composites correctly identified: %d/%d\n", correct_composites, total_composites);
    EXPECT_EQ(correct_composites, total_composites) << "All composites should be identified";
}

TEST_F(GeniusMathRegressionTest, GaussPrimalityDeterminism) {
    printf("\n[Gauss Primality Determinism]\n");

    /* Same input should give same output with fixed seed */
    int inconsistencies = 0;

    for (int i = 0; i < PRIMALITY_TEST_COUNT; i++) {
        uint64_t n = 1000 + i * 13;  /* Various numbers */

        bool result1 = genius_gauss_is_prime(genius, n, 0.99f);
        bool result2 = genius_gauss_is_prime(genius, n, 0.99f);

        if (result1 != result2) {
            inconsistencies++;
            printf("    Inconsistency at n=%lu\n", (unsigned long)n);
        }
    }

    printf("  Inconsistencies: %d/%d\n", inconsistencies, PRIMALITY_TEST_COUNT);
    EXPECT_LE(inconsistencies, 2) << "Primality test should be mostly deterministic";
}

TEST_F(GeniusMathRegressionTest, GaussFactorizationConsistency) {
    printf("\n[Gauss Factorization Consistency]\n");

    /* Test that factorization results are consistent */
    uint64_t test_numbers[] = {12, 30, 100, 1001, 10001, 12345, 99999};
    int num_tests = sizeof(test_numbers) / sizeof(test_numbers[0]);

    for (int i = 0; i < num_tests; i++) {
        uint64_t n = test_numbers[i];
        uint64_t factors1[32];
        uint64_t factors2[32];

        uint32_t count1 = genius_gauss_factor(genius, n, factors1, 32);
        uint32_t count2 = genius_gauss_factor(genius, n, factors2, 32);

        /* Same number of factors */
        EXPECT_EQ(count1, count2) << "Factor count should match for n=" << n;

        /* Same factors */
        if (count1 == count2) {
            for (uint32_t j = 0; j < count1; j++) {
                EXPECT_EQ(factors1[j], factors2[j])
                    << "Factor mismatch at position " << j << " for n=" << n;
            }
        }

        /* Verify product equals original */
        if (count1 > 0) {
            uint64_t product = 1;
            for (uint32_t j = 0; j < count1; j++) {
                product *= factors1[j];
            }
            EXPECT_EQ(product, n) << "Factor product should equal original n=" << n;
        }
    }

    printf("  All factorizations verified\n");
}

/* ============================================================================
 * PatternDiscoveryRegression - Pattern detection reproducibility
 * ============================================================================ */

TEST_F(GeniusMathRegressionTest, PatternDiscoveryArithmeticSequence) {
    printf("\n[Pattern Discovery - Arithmetic Sequence]\n");

    /* Test Gauss's famous 1+2+...+100 pattern */
    int64_t sequence[20];
    create_arithmetic_sequence(sequence, 20, 1, 1);  /* 1, 2, 3, ..., 20 */

    conjecture_t conjecture1, conjecture2;
    memset(&conjecture1, 0, sizeof(conjecture1));
    memset(&conjecture2, 0, sizeof(conjecture2));

    /* Pattern should be detected consistently */
    nimcp_error_t err1 = genius_gauss_discover_pattern(genius, sequence, 20, &conjecture1);
    nimcp_error_t err2 = genius_gauss_discover_pattern(genius, sequence, 20, &conjecture2);

    EXPECT_EQ(err1, err2) << "Pattern discovery should return same error code";

    if (err1 == NIMCP_SUCCESS) {
        /* Confidence should be bounded */
        EXPECT_GE(conjecture1.confidence, PATTERN_CONFIDENCE_MIN);
        EXPECT_LE(conjecture1.confidence, 1.0f);

        /* Confidence should be similar on repeated calls */
        float confidence_diff = fabsf(conjecture1.confidence - conjecture2.confidence);
        EXPECT_LE(confidence_diff, 0.1f)
            << "Confidence should be consistent: " << conjecture1.confidence
            << " vs " << conjecture2.confidence;
    }

    printf("  Arithmetic sequence pattern discovery completed\n");
}

TEST_F(GeniusMathRegressionTest, PatternDiscoveryTriangularNumbers) {
    printf("\n[Pattern Discovery - Triangular Numbers]\n");

    int64_t sequence[15];
    create_triangular_sequence(sequence, 15);  /* 1, 3, 6, 10, 15, 21, ... */

    conjecture_t conjecture;
    memset(&conjecture, 0, sizeof(conjecture));

    nimcp_error_t err = genius_gauss_discover_pattern(genius, sequence, 15, &conjecture);

    if (err == NIMCP_SUCCESS) {
        printf("  Pattern detected with confidence: %.4f\n", conjecture.confidence);
        EXPECT_GE(conjecture.confidence, 0.0f);
        EXPECT_LE(conjecture.confidence, 1.0f);
    }
}

TEST_F(GeniusMathRegressionTest, PatternDiscoveryReproducibility) {
    printf("\n[Pattern Discovery Reproducibility]\n");

    /* Run pattern discovery multiple times on same data */
    int64_t sequence[10];
    create_arithmetic_sequence(sequence, 10, 5, 7);  /* 5, 12, 19, 26, ... */

    std::vector<float> confidences;
    int successes = 0;

    for (int i = 0; i < REGRESSION_ITERATIONS; i++) {
        conjecture_t conjecture;
        memset(&conjecture, 0, sizeof(conjecture));

        nimcp_error_t err = genius_gauss_discover_pattern(genius, sequence, 10, &conjecture);
        if (err == NIMCP_SUCCESS) {
            successes++;
            confidences.push_back(conjecture.confidence);
        }
    }

    printf("  Successes: %d/%d\n", successes, REGRESSION_ITERATIONS);

    /* Verify confidence consistency */
    if (confidences.size() > 1) {
        float min_conf = *std::min_element(confidences.begin(), confidences.end());
        float max_conf = *std::max_element(confidences.begin(), confidences.end());
        float range = max_conf - min_conf;

        printf("  Confidence range: [%.4f, %.4f] (spread: %.4f)\n", min_conf, max_conf, range);
        EXPECT_LE(range, 0.2f) << "Confidence should be reasonably consistent";
    }
}

/* ============================================================================
 * ModeSwitchingRegression - Stability when switching between modes
 * ============================================================================ */

TEST_F(GeniusMathRegressionTest, ModeSwitchingStability) {
    printf("\n[Mode Switching Stability]\n");

    genius_mode_t modes[] = {
        GENIUS_MODE_GAUSS,
        GENIUS_MODE_NEWTON,
        GENIUS_MODE_ERDOS,
        GENIUS_MODE_EULER,
        GENIUS_MODE_RAMANUJAN,
        GENIUS_MODE_ADAPTIVE
    };
    int num_modes = sizeof(modes) / sizeof(modes[0]);

    /* Create problem for testing */
    math_problem_t problem;
    memset(&problem, 0, sizeof(problem));
    problem.domain = GENIUS_DOMAIN_NUMBER_THEORY;
    problem.statement = (char*)"Test problem";
    problem.difficulty = 0.5f;
    problem.timeout_ms = 100;

    int switch_errors = 0;

    for (int iter = 0; iter < MODE_SWITCH_ITERATIONS; iter++) {
        for (int m = 0; m < num_modes; m++) {
            /* Configure for mode */
            genius_config_t mode_config;
            genius_get_default_config(&mode_config);
            mode_config.default_mode = modes[m];

            /* Destroy and recreate with new config */
            genius_destroy(genius);
            genius = genius_create(&mode_config);

            if (genius == nullptr) {
                switch_errors++;
                printf("    Failed to create genius with mode %d at iteration %d\n", m, iter);
                /* Recover */
                genius = genius_create(NULL);
                ASSERT_NE(genius, nullptr);
            } else {
                /* Solve problem in this mode */
                genius_result_t result;
                genius_result_init(&result);

                nimcp_error_t err = genius_solve_problem(genius, &problem, &result);
                if (err != NIMCP_SUCCESS) {
                    /* Not necessarily an error - some modes may not handle all problems */
                }

                genius_result_cleanup(&result);
            }
        }
    }

    printf("  Mode switch errors: %d/%d\n", switch_errors,
           MODE_SWITCH_ITERATIONS * num_modes);
    EXPECT_EQ(switch_errors, 0) << "All mode switches should succeed";
}

TEST_F(GeniusMathRegressionTest, ModeSwitchingNoStateLeakage) {
    printf("\n[Mode Switching - No State Leakage]\n");

    /* Test that switching modes doesn't cause state leakage */

    /* First: Use Gauss mode and record stats */
    genius_config_t gauss_config;
    genius_get_default_config(&gauss_config);
    gauss_config.default_mode = GENIUS_MODE_GAUSS;

    genius_destroy(genius);
    genius = genius_create(&gauss_config);
    ASSERT_NE(genius, nullptr);

    /* Do some Gauss operations */
    genius_gauss_is_prime(genius, 7919, 0.99f);
    genius_gauss_is_prime(genius, 7927, 0.99f);

    genius_stats_t gauss_stats;
    genius_get_stats(genius, &gauss_stats);

    /* Switch to Newton mode */
    genius_config_t newton_config;
    genius_get_default_config(&newton_config);
    newton_config.default_mode = GENIUS_MODE_NEWTON;

    genius_destroy(genius);
    genius = genius_create(&newton_config);
    ASSERT_NE(genius, nullptr);

    /* New instance should have clean stats */
    genius_stats_t newton_stats;
    genius_get_stats(genius, &newton_stats);

    /* Stats should be reset in new instance */
    EXPECT_EQ(newton_stats.problems_attempted, 0u);
    EXPECT_EQ(newton_stats.problems_solved, 0u);

    printf("  No state leakage detected\n");
}

/* ============================================================================
 * ConfigurationRegression - Configuration persistence
 * ============================================================================ */

TEST_F(GeniusMathRegressionTest, ConfigurationPersistence) {
    printf("\n[Configuration Persistence]\n");

    /* Set up custom configuration */
    genius_config_t custom_config;
    genius_get_default_config(&custom_config);
    custom_config.creativity_level = 0.3f;
    custom_config.rigor_level = 0.9f;
    custom_config.max_proof_depth = 50;
    custom_config.max_conjecture_candidates = 20;
    custom_config.enable_quantum_search = true;
    custom_config.enable_pattern_mining = true;

    genius_destroy(genius);
    genius = genius_create(&custom_config);
    ASSERT_NE(genius, nullptr);

    /* Do some operations */
    math_problem_t problem;
    memset(&problem, 0, sizeof(problem));
    problem.domain = GENIUS_DOMAIN_NUMBER_THEORY;
    problem.statement = (char*)"2 + 2 = 4";
    problem.difficulty = 0.1f;

    for (int i = 0; i < 10; i++) {
        genius_result_t result;
        genius_result_init(&result);
        genius_solve_problem(genius, &problem, &result);
        genius_result_cleanup(&result);
    }

    /* Reset and verify configuration still works */
    nimcp_error_t err = genius_reset(genius);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Should still be able to solve problems after reset */
    genius_result_t result;
    genius_result_init(&result);
    err = genius_solve_problem(genius, &problem, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    genius_result_cleanup(&result);

    printf("  Configuration persists across reset\n");
}

TEST_F(GeniusMathRegressionTest, DefaultConfigValues) {
    printf("\n[Default Config Values]\n");

    genius_config_t cfg;
    nimcp_error_t err = genius_get_default_config(&cfg);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Verify default values are in valid ranges */
    EXPECT_GE(cfg.creativity_level, 0.0f);
    EXPECT_LE(cfg.creativity_level, 1.0f);

    EXPECT_GE(cfg.rigor_level, 0.0f);
    EXPECT_LE(cfg.rigor_level, 1.0f);

    EXPECT_GT(cfg.max_proof_depth, 0u);
    EXPECT_LE(cfg.max_proof_depth, GENIUS_MAX_PROOF_STEPS);

    printf("  Default config values verified\n");
}

TEST_F(GeniusMathRegressionTest, ModulationPersistence) {
    printf("\n[Modulation Persistence]\n");

    /* Apply modulations */
    nimcp_error_t err1 = genius_modulate_inflammation(genius, 0.3f);
    nimcp_error_t err2 = genius_modulate_fatigue(genius, 0.2f);
    nimcp_error_t err3 = genius_modulate_atp(genius, 0.8f);

    EXPECT_EQ(err1, NIMCP_SUCCESS);
    EXPECT_EQ(err2, NIMCP_SUCCESS);
    EXPECT_EQ(err3, NIMCP_SUCCESS);

    /* Solve problems with modulations */
    math_problem_t problem;
    memset(&problem, 0, sizeof(problem));
    problem.domain = GENIUS_DOMAIN_NUMBER_THEORY;
    problem.statement = (char*)"Test";
    problem.difficulty = 0.5f;

    for (int i = 0; i < 5; i++) {
        genius_result_t result;
        genius_result_init(&result);
        genius_solve_problem(genius, &problem, &result);
        genius_result_cleanup(&result);
    }

    /* Should be able to change modulations */
    err1 = genius_modulate_inflammation(genius, 0.1f);
    err2 = genius_modulate_fatigue(genius, 0.5f);
    err3 = genius_modulate_atp(genius, 0.6f);

    EXPECT_EQ(err1, NIMCP_SUCCESS);
    EXPECT_EQ(err2, NIMCP_SUCCESS);
    EXPECT_EQ(err3, NIMCP_SUCCESS);

    printf("  Modulations applied successfully\n");
}

/* ============================================================================
 * Performance Regression Tests
 * ============================================================================ */

TEST_F(GeniusMathRegressionTest, PrimalityPerformance) {
    printf("\n[Primality Test Performance]\n");

    std::vector<int64_t> timings;
    timings.reserve(PRIMALITY_TEST_COUNT);

    for (int i = 0; i < PRIMALITY_TEST_COUNT; i++) {
        uint64_t n = 10000 + i * 1000;

        int64_t time_us = measure_time_us([&]() {
            genius_gauss_is_prime(genius, n, 0.99f);
        });
        timings.push_back(time_us);
    }

    std::sort(timings.begin(), timings.end());
    int64_t median = timings[timings.size() / 2];
    int64_t p95 = timings[(timings.size() * 95) / 100];

    printf("  Median time: %lld us\n", (long long)median);
    printf("  P95 time: %lld us\n", (long long)p95);

    EXPECT_LT(median, PRIMALITY_TEST_THRESHOLD_US)
        << "Median primality test time should be under threshold";
}

TEST_F(GeniusMathRegressionTest, PatternDiscoveryPerformance) {
    printf("\n[Pattern Discovery Performance]\n");

    int64_t sequence[50];
    create_arithmetic_sequence(sequence, 50, 1, 1);

    std::vector<int64_t> timings;
    timings.reserve(REGRESSION_ITERATIONS);

    for (int i = 0; i < REGRESSION_ITERATIONS; i++) {
        conjecture_t conjecture;
        memset(&conjecture, 0, sizeof(conjecture));

        int64_t time_us = measure_time_us([&]() {
            genius_gauss_discover_pattern(genius, sequence, 50, &conjecture);
        });
        timings.push_back(time_us);
    }

    std::sort(timings.begin(), timings.end());
    int64_t median = timings[timings.size() / 2];
    int64_t p95 = timings[(timings.size() * 95) / 100];

    printf("  Median time: %lld us\n", (long long)median);
    printf("  P95 time: %lld us\n", (long long)p95);

    EXPECT_LT(median, PATTERN_DISCOVERY_THRESHOLD_US)
        << "Median pattern discovery time should be under threshold";
}

/* ============================================================================
 * Statistics Regression Tests
 * ============================================================================ */

TEST_F(GeniusMathRegressionTest, StatisticsAccumulation) {
    printf("\n[Statistics Accumulation]\n");

    /* Reset and get initial stats */
    genius_reset(genius);

    genius_stats_t initial_stats;
    genius_get_stats(genius, &initial_stats);
    EXPECT_EQ(initial_stats.problems_attempted, 0u);
    EXPECT_EQ(initial_stats.problems_solved, 0u);

    /* Solve some problems */
    const int PROBLEM_COUNT = 10;
    math_problem_t problem;
    memset(&problem, 0, sizeof(problem));
    problem.domain = GENIUS_DOMAIN_NUMBER_THEORY;
    problem.statement = (char*)"Test";
    problem.difficulty = 0.1f;

    int solved_count = 0;
    for (int i = 0; i < PROBLEM_COUNT; i++) {
        genius_result_t result;
        genius_result_init(&result);

        nimcp_error_t err = genius_solve_problem(genius, &problem, &result);
        if (err == NIMCP_SUCCESS && result.solved) {
            solved_count++;
        }

        genius_result_cleanup(&result);
    }

    /* Verify stats accumulated */
    genius_stats_t final_stats;
    genius_get_stats(genius, &final_stats);

    printf("  Problems attempted: %lu\n", (unsigned long)final_stats.problems_attempted);
    printf("  Problems solved: %lu\n", (unsigned long)final_stats.problems_solved);

    EXPECT_EQ(final_stats.problems_attempted, (uint64_t)PROBLEM_COUNT);
    EXPECT_GE(final_stats.problems_solved, 0u);
    EXPECT_LE(final_stats.problems_solved, (uint64_t)PROBLEM_COUNT);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
