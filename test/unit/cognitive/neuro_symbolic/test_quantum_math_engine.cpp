/**
 * @file test_quantum_math_engine.cpp
 * @brief Unit tests for Quantum Mathematical Engine
 *
 * Tests the quantum-enhanced mathematical computations including
 * Monte Carlo integration, partition functions, and path integrals.
 */

#include <gtest/gtest.h>
#include <cmath>
#include "utils/nimcp_test_base.h"

extern "C" {
#include "cognitive/neuro_symbolic/nimcp_quantum_math_engine.h"
}

/**
 * @brief Test fixture for Quantum Math Engine tests
 */
class QuantumMathEngineTest : public NimcpTestBase {
protected:
    qme_math_simulation_t* sim;
    qme_simulation_config_t config;

    void SetUp() override {
        NimcpTestBase::SetUp();
        sim = NULL;
        memset(&config, 0, sizeof(config));
        qme_math_get_default_config(&config);
    }

    void TearDown() override {
        if (sim) {
            qme_math_destroy(sim);
            sim = NULL;
        }
        NimcpTestBase::TearDown();
    }

    // Simple constant function for testing
    static float constant_function(const float* x, uint32_t dim, void* user_data) {
        (void)x;
        (void)dim;
        (void)user_data;
        return 1.0f;  // Integral over unit cube = 1.0
    }

    // Linear function x[0]
    static float linear_function(const float* x, uint32_t dim, void* user_data) {
        (void)dim;
        (void)user_data;
        return x ? x[0] : 0.0f;  // Integral over [0,1] = 0.5
    }

    // Quadratic function x[0]^2
    static float quadratic_function(const float* x, uint32_t dim, void* user_data) {
        (void)dim;
        (void)user_data;
        return x ? x[0] * x[0] : 0.0f;  // Integral over [0,1] = 1/3
    }

    // Gaussian function exp(-x^2)
    static float gaussian_function(const float* x, uint32_t dim, void* user_data) {
        (void)dim;
        (void)user_data;
        if (!x) return 0.0f;
        return expf(-x[0] * x[0]);
    }

    // Simple energy function for partition function
    static float simple_energy(const float* x, uint32_t dim, void* user_data) {
        (void)dim;
        (void)user_data;
        if (!x) return 0.0f;
        return x[0] * x[0];  // Quadratic energy
    }

    // Simple action for path integral
    static float simple_action(const float* path, uint32_t num_points,
                               uint32_t dim, void* user_data) {
        (void)dim;
        (void)user_data;
        if (!path || num_points < 2) return 0.0f;

        // Simple action: sum of squared differences
        float action = 0.0f;
        for (uint32_t i = 1; i < num_points; i++) {
            float diff = path[i] - path[i-1];
            action += diff * diff;
        }
        return action;
    }

    // Uniform distribution on [0,1] (PDF = 1)
    static float uniform_distribution(const float* x, uint32_t dim, void* user_data) {
        (void)x;
        (void)dim;
        (void)user_data;
        return 1.0f;  // Uniform distribution has constant PDF
    }
};

// ============================================================================
// Default Configuration Tests
// ============================================================================

TEST_F(QuantumMathEngineTest, GetDefaultConfigSucceeds) {
    qme_simulation_config_t cfg;
    nimcp_error_t err = qme_math_get_default_config(&cfg);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(QuantumMathEngineTest, GetDefaultConfigNullReturnsError) {
    nimcp_error_t err = qme_math_get_default_config(NULL);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(QuantumMathEngineTest, DefaultConfigHasReasonableSamples) {
    qme_simulation_config_t cfg;
    qme_math_get_default_config(&cfg);

    EXPECT_GT(cfg.num_samples, 0u);
    EXPECT_LE(cfg.num_samples, 1000000u);
}

TEST_F(QuantumMathEngineTest, DefaultConfigHasValidBurnin) {
    qme_simulation_config_t cfg;
    qme_math_get_default_config(&cfg);

    EXPECT_GE(cfg.burnin, 0u);
    EXPECT_LT(cfg.burnin, cfg.num_samples);
}

TEST_F(QuantumMathEngineTest, DefaultConfigHasValidAcceptance) {
    qme_simulation_config_t cfg;
    qme_math_get_default_config(&cfg);

    EXPECT_GE(cfg.target_acceptance, 0.0f);
    EXPECT_LE(cfg.target_acceptance, 1.0f);
}

// ============================================================================
// Lifecycle Tests
// ============================================================================

TEST_F(QuantumMathEngineTest, CreateWithConfigSucceeds) {
    sim = qme_math_create(&config);
    ASSERT_NE(sim, nullptr);
}

TEST_F(QuantumMathEngineTest, CreateWithNullConfigSucceeds) {
    sim = qme_math_create(NULL);
    EXPECT_NE(sim, nullptr);
}

TEST_F(QuantumMathEngineTest, DestroyNullIsNoOp) {
    qme_math_destroy(NULL);
    SUCCEED();
}

TEST_F(QuantumMathEngineTest, CreateDestroyMultipleTimesSucceeds) {
    for (int i = 0; i < 5; i++) {
        sim = qme_math_create(&config);
        ASSERT_NE(sim, nullptr) << "Failed on iteration " << i;
        qme_math_destroy(sim);
        sim = NULL;
    }
}

TEST_F(QuantumMathEngineTest, ResetNullReturnsError) {
    nimcp_error_t err = qme_math_reset(NULL);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(QuantumMathEngineTest, ResetClearsState) {
    sim = qme_math_create(&config);
    ASSERT_NE(sim, nullptr);

    nimcp_error_t err = qme_math_reset(sim);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

// ============================================================================
// Domain Tests
// ============================================================================

TEST_F(QuantumMathEngineTest, DomainCreateBoxSucceeds) {
    float lower[] = {0.0f};
    float upper[] = {1.0f};

    qme_domain_t* domain = qme_domain_create_box(1, lower, upper);
    EXPECT_NE(domain, nullptr);

    qme_domain_destroy(domain);
}

TEST_F(QuantumMathEngineTest, DomainCreateBallSucceeds) {
    float center[] = {0.0f, 0.0f};
    float radius = 1.0f;

    qme_domain_t* domain = qme_domain_create_ball(2, center, radius);
    EXPECT_NE(domain, nullptr);

    qme_domain_destroy(domain);
}

TEST_F(QuantumMathEngineTest, DomainDestroyNullIsNoOp) {
    qme_domain_destroy(NULL);
    SUCCEED();
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(QuantumMathEngineTest, IntegrateNullSimReturnsError) {
    float lower[] = {0.0f};
    float upper[] = {1.0f};
    qme_domain_t* domain = qme_domain_create_box(1, lower, upper);
    ASSERT_NE(domain, nullptr);

    qme_integration_result_t result;
    nimcp_error_t err = qme_integrate(NULL, constant_function, domain,
                                       0.1f, &result, NULL);
    EXPECT_NE(err, NIMCP_SUCCESS);

    qme_domain_destroy(domain);
}

TEST_F(QuantumMathEngineTest, IntegrateNullFunctionReturnsError) {
    sim = qme_math_create(&config);
    ASSERT_NE(sim, nullptr);

    float lower[] = {0.0f};
    float upper[] = {1.0f};
    qme_domain_t* domain = qme_domain_create_box(1, lower, upper);
    ASSERT_NE(domain, nullptr);

    qme_integration_result_t result;
    nimcp_error_t err = qme_integrate(sim, NULL, domain, 0.1f, &result, NULL);
    EXPECT_NE(err, NIMCP_SUCCESS);

    qme_domain_destroy(domain);
}

TEST_F(QuantumMathEngineTest, IntegrateConstantFunction) {
    config.num_samples = 1000;
    sim = qme_math_create(&config);
    ASSERT_NE(sim, nullptr);

    float lower[] = {0.0f};
    float upper[] = {1.0f};
    qme_domain_t* domain = qme_domain_create_box(1, lower, upper);
    ASSERT_NE(domain, nullptr);

    qme_integration_result_t result;
    memset(&result, 0, sizeof(result));

    nimcp_error_t err = qme_integrate(sim, constant_function, domain,
                                       0.1f, &result, NULL);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Integral of 1 over [0,1] should be 1.0
    EXPECT_NEAR(result.value, 1.0f, 0.2f);
    EXPECT_GT(result.samples_used, 0u);

    qme_domain_destroy(domain);
}

TEST_F(QuantumMathEngineTest, IntegrateLinearFunction) {
    config.num_samples = 5000;
    sim = qme_math_create(&config);
    ASSERT_NE(sim, nullptr);

    float lower[] = {0.0f};
    float upper[] = {1.0f};
    qme_domain_t* domain = qme_domain_create_box(1, lower, upper);
    ASSERT_NE(domain, nullptr);

    qme_integration_result_t result;
    memset(&result, 0, sizeof(result));

    nimcp_error_t err = qme_integrate(sim, linear_function, domain,
                                       0.1f, &result, NULL);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Integral of x over [0,1] should be 0.5
    EXPECT_NEAR(result.value, 0.5f, 0.15f);

    qme_domain_destroy(domain);
}

TEST_F(QuantumMathEngineTest, IntegrateQuadraticFunction) {
    config.num_samples = 10000;
    sim = qme_math_create(&config);
    ASSERT_NE(sim, nullptr);

    float lower[] = {0.0f};
    float upper[] = {1.0f};
    qme_domain_t* domain = qme_domain_create_box(1, lower, upper);
    ASSERT_NE(domain, nullptr);

    qme_integration_result_t result;
    memset(&result, 0, sizeof(result));

    nimcp_error_t err = qme_integrate(sim, quadratic_function, domain,
                                       0.1f, &result, NULL);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Integral of x^2 over [0,1] should be 1/3 ~ 0.333
    EXPECT_NEAR(result.value, 1.0f/3.0f, 0.1f);

    qme_domain_destroy(domain);
}

// ============================================================================
// Variance Reduction Tests
// ============================================================================

TEST_F(QuantumMathEngineTest, AntitheticVariatesEnabled) {
    config.enable_antithetic = true;
    config.num_samples = 1000;
    sim = qme_math_create(&config);
    ASSERT_NE(sim, nullptr);

    float lower[] = {0.0f};
    float upper[] = {1.0f};
    qme_domain_t* domain = qme_domain_create_box(1, lower, upper);
    ASSERT_NE(domain, nullptr);

    qme_integration_result_t result;
    memset(&result, 0, sizeof(result));

    nimcp_error_t err = qme_integrate(sim, linear_function, domain,
                                       0.1f, &result, NULL);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_NEAR(result.value, 0.5f, 0.2f);

    qme_domain_destroy(domain);
}

TEST_F(QuantumMathEngineTest, StratifiedSamplingEnabled) {
    config.enable_stratified = true;
    config.num_strata = 10;
    config.num_samples = 1000;
    sim = qme_math_create(&config);
    ASSERT_NE(sim, nullptr);

    float lower[] = {0.0f};
    float upper[] = {1.0f};
    qme_domain_t* domain = qme_domain_create_box(1, lower, upper);
    ASSERT_NE(domain, nullptr);

    qme_integration_result_t result;
    memset(&result, 0, sizeof(result));

    nimcp_error_t err = qme_integrate(sim, linear_function, domain,
                                       0.1f, &result, NULL);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_NEAR(result.value, 0.5f, 0.15f);

    qme_domain_destroy(domain);
}

// ============================================================================
// Expectation Tests
// ============================================================================

TEST_F(QuantumMathEngineTest, ExpectationNullReturnsError) {
    qme_expectation_result_t result;
    nimcp_error_t err = qme_estimate_expectation(NULL, NULL, NULL, 1, &result, NULL);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(QuantumMathEngineTest, ExpectationUniformDistribution) {
    config.num_samples = 5000;
    sim = qme_math_create(&config);
    ASSERT_NE(sim, nullptr);

    qme_expectation_result_t result;
    memset(&result, 0, sizeof(result));

    // E[X] for uniform distribution
    nimcp_error_t err = qme_estimate_expectation(
        sim, linear_function, uniform_distribution, 1, &result, NULL);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    // Expect finite mean
    EXPECT_TRUE(std::isfinite(result.mean));
}

// ============================================================================
// Partition Function Tests
// ============================================================================

TEST_F(QuantumMathEngineTest, PartitionFunctionNullReturnsError) {
    qme_partition_result_t result;
    nimcp_error_t err = qme_partition_function(NULL, NULL, 1, 1.0f, &result, NULL);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(QuantumMathEngineTest, PartitionFunctionBasic) {
    config.num_samples = 5000;
    sim = qme_math_create(&config);
    ASSERT_NE(sim, nullptr);

    qme_partition_result_t result;
    memset(&result, 0, sizeof(result));

    float temperature = 1.0f;
    nimcp_error_t err = qme_partition_function(
        sim, simple_energy, 1, temperature, &result, NULL);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Z should be positive
    EXPECT_GT(result.Z, 0.0f);
    // log_Z should be finite
    EXPECT_TRUE(std::isfinite(result.log_Z));
}

TEST_F(QuantumMathEngineTest, PartitionFunctionHighTemperature) {
    config.num_samples = 2000;
    sim = qme_math_create(&config);
    ASSERT_NE(sim, nullptr);

    qme_partition_result_t result;
    memset(&result, 0, sizeof(result));

    float temperature = 10.0f;  // High temperature
    nimcp_error_t err = qme_partition_function(
        sim, simple_energy, 1, temperature, &result, NULL);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // At high temperature, distribution is nearly uniform
    EXPECT_GT(result.Z, 0.0f);
}

// ============================================================================
// Path Integral Tests
// ============================================================================

TEST_F(QuantumMathEngineTest, PathIntegralNullReturnsError) {
    qme_path_integral_result_t result;
    memset(&result, 0, sizeof(result));
    float initial[] = {0.0f};
    float final_pt[] = {1.0f};

    nimcp_error_t err = qme_path_integral(NULL, NULL, 1, 10, initial, final_pt,
                                          &result, NULL);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(QuantumMathEngineTest, PathIntegralBasic) {
    config.num_samples = 1000;
    sim = qme_math_create(&config);
    ASSERT_NE(sim, nullptr);

    qme_path_integral_result_t result;
    memset(&result, 0, sizeof(result));

    float initial[] = {0.0f};
    float final_pt[] = {1.0f};
    uint32_t dim = 1;
    uint32_t num_steps = 20;

    nimcp_error_t err = qme_path_integral(
        sim, simple_action, dim, num_steps, initial, final_pt, &result, NULL);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Path integral value should be finite
    EXPECT_TRUE(std::isfinite(result.value));
    EXPECT_GT(result.paths_sampled, 0u);

    qme_path_integral_result_cleanup(&result);
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_F(QuantumMathEngineTest, GetStatsNullReturnsError) {
    qme_stats_t stats;
    nimcp_error_t err = qme_math_get_stats(NULL, &stats);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(QuantumMathEngineTest, GetStatsWithNullStatsReturnsError) {
    sim = qme_math_create(&config);
    ASSERT_NE(sim, nullptr);

    nimcp_error_t err = qme_math_get_stats(sim, NULL);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(QuantumMathEngineTest, StatsIncrementAfterIntegration) {
    sim = qme_math_create(&config);
    ASSERT_NE(sim, nullptr);

    float lower[] = {0.0f};
    float upper[] = {1.0f};
    qme_domain_t* domain = qme_domain_create_box(1, lower, upper);
    ASSERT_NE(domain, nullptr);

    // Initial stats
    qme_stats_t stats_before;
    qme_math_get_stats(sim, &stats_before);

    // Perform integration
    qme_integration_result_t result;
    qme_integrate(sim, constant_function, domain, 0.1f, &result, NULL);

    // Stats after
    qme_stats_t stats_after;
    qme_math_get_stats(sim, &stats_after);

    EXPECT_GT(stats_after.integrations_performed, stats_before.integrations_performed);
    EXPECT_GT(stats_after.total_samples, stats_before.total_samples);

    qme_domain_destroy(domain);
}

// ============================================================================
// Multi-Dimensional Tests
// ============================================================================

TEST_F(QuantumMathEngineTest, Integrate2DConstant) {
    config.num_samples = 5000;
    sim = qme_math_create(&config);
    ASSERT_NE(sim, nullptr);

    float lower[] = {0.0f, 0.0f};
    float upper[] = {1.0f, 1.0f};
    qme_domain_t* domain = qme_domain_create_box(2, lower, upper);
    ASSERT_NE(domain, nullptr);

    qme_integration_result_t result;
    memset(&result, 0, sizeof(result));

    // Integral of 1 over unit square = 1
    nimcp_error_t err = qme_integrate(sim, constant_function, domain,
                                       0.15f, &result, NULL);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_NEAR(result.value, 1.0f, 0.25f);

    qme_domain_destroy(domain);
}

TEST_F(QuantumMathEngineTest, IntegrateBallDomain) {
    config.num_samples = 5000;
    sim = qme_math_create(&config);
    ASSERT_NE(sim, nullptr);

    float center[] = {0.0f, 0.0f};
    qme_domain_t* domain = qme_domain_create_ball(2, center, 1.0f);
    ASSERT_NE(domain, nullptr);

    qme_integration_result_t result;
    memset(&result, 0, sizeof(result));

    // Integral of 1 over unit disk = pi
    nimcp_error_t err = qme_integrate(sim, constant_function, domain,
                                       0.15f, &result, NULL);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_NEAR(result.value, M_PI, 0.5f);

    qme_domain_destroy(domain);
}
