//=============================================================================
// test_statistics_exceptions.cpp - Exception Handling Tests for Statistics
//=============================================================================
/**
 * @file test_statistics_exceptions.cpp
 * @brief Comprehensive exception handling tests for statistics module
 *
 * Tests cover:
 * - NULL pointer handling for all functions
 * - Invalid parameter detection (zero sizes, out of range, etc.)
 * - Edge cases (empty arrays, single elements, extreme values)
 * - Memory safety (no crashes on invalid input)
 * - Proper error code returns
 *
 * @author NIMCP Development Team
 * @date 2026-01-30
 */

#include <gtest/gtest.h>
#include "utils/statistics/nimcp_statistics.h"
#include "utils/statistics/nimcp_quantum_statistics.h"
#include <cmath>
#include <limits>
#include <vector>

//=============================================================================
// Test Fixture
//=============================================================================

class StatisticsExceptionTest : public ::testing::Test {
protected:
    void SetUp() override {
        config = nimcp_stats_default_config();
        nimcp_stats_init(&config);
    }

    void TearDown() override {
        nimcp_stats_shutdown();
    }

    nimcp_stats_config_t config;
};

//=============================================================================
// NULL Pointer Tests - Descriptive Statistics
//=============================================================================

TEST_F(StatisticsExceptionTest, Null_Mean) {
    EXPECT_TRUE(std::isnan(nimcp_stats_mean(nullptr, 5)));

    float data[] = {1.0f, 2.0f};
    EXPECT_FALSE(std::isnan(nimcp_stats_mean(data, 2)));  // Valid case works
}

TEST_F(StatisticsExceptionTest, Null_Variance) {
    EXPECT_TRUE(std::isnan(nimcp_stats_variance(nullptr, 5)));
}

TEST_F(StatisticsExceptionTest, Null_Stddev) {
    EXPECT_TRUE(std::isnan(nimcp_stats_std_dev(nullptr, 5)));
}

TEST_F(StatisticsExceptionTest, Null_Skewness) {
    EXPECT_TRUE(std::isnan(nimcp_stats_skewness(nullptr, 5)));
}

TEST_F(StatisticsExceptionTest, Null_Kurtosis) {
    EXPECT_TRUE(std::isnan(nimcp_stats_kurtosis(nullptr, 5)));
}

TEST_F(StatisticsExceptionTest, Null_Min) {
    EXPECT_TRUE(std::isnan(nimcp_stats_min(nullptr, 5)));
}

TEST_F(StatisticsExceptionTest, Null_Max) {
    EXPECT_TRUE(std::isnan(nimcp_stats_max(nullptr, 5)));
}

TEST_F(StatisticsExceptionTest, Null_Range) {
    EXPECT_TRUE(std::isnan(nimcp_stats_range(nullptr, 5)));
}

TEST_F(StatisticsExceptionTest, Null_Median) {
    EXPECT_TRUE(std::isnan(nimcp_stats_median(nullptr, 5)));
}

TEST_F(StatisticsExceptionTest, Null_Quantile) {
    EXPECT_TRUE(std::isnan(nimcp_stats_quantile(nullptr, 5, 0.5f)));
}

TEST_F(StatisticsExceptionTest, Null_StdError) {
    EXPECT_TRUE(std::isnan(nimcp_stats_std_error(nullptr, 5)));
}

//=============================================================================
// NULL Pointer Tests - Entropy and Information
//=============================================================================

TEST_F(StatisticsExceptionTest, Null_Entropy) {
    EXPECT_TRUE(std::isnan(nimcp_stats_entropy(nullptr, 5)));
}

TEST_F(StatisticsExceptionTest, Null_KLDivergence) {
    float p[] = {0.5f, 0.5f};

    EXPECT_TRUE(std::isnan(nimcp_stats_kl_divergence(nullptr, p, 2)));
    EXPECT_TRUE(std::isnan(nimcp_stats_kl_divergence(p, nullptr, 2)));
    EXPECT_TRUE(std::isnan(nimcp_stats_kl_divergence(nullptr, nullptr, 2)));
}

TEST_F(StatisticsExceptionTest, Null_JSDivergence) {
    float p[] = {0.5f, 0.5f};

    EXPECT_TRUE(std::isnan(nimcp_stats_js_divergence(nullptr, p, 2)));
    EXPECT_TRUE(std::isnan(nimcp_stats_js_divergence(p, nullptr, 2)));
}

TEST_F(StatisticsExceptionTest, Null_MutualInformation) {
    EXPECT_TRUE(std::isnan(nimcp_stats_mutual_information(nullptr, 2, 2)));
}

TEST_F(StatisticsExceptionTest, Null_CrossEntropy) {
    float p[] = {0.5f, 0.5f};

    EXPECT_TRUE(std::isnan(nimcp_stats_cross_entropy(nullptr, p, 2)));
    EXPECT_TRUE(std::isnan(nimcp_stats_cross_entropy(p, nullptr, 2)));
}

//=============================================================================
// NULL Pointer Tests - Hypothesis Testing
//=============================================================================

TEST_F(StatisticsExceptionTest, Null_TTestOneSample) {
    nimcp_test_result_t result;
    float data[] = {1.0f, 2.0f, 3.0f};

    EXPECT_EQ(nimcp_stats_ttest_one_sample(nullptr, 3, 0, NIMCP_TEST_TWO_SIDED, 0.95f, &result),
              NIMCP_STATS_ERROR_NULL);
    EXPECT_EQ(nimcp_stats_ttest_one_sample(data, 3, 0, NIMCP_TEST_TWO_SIDED, 0.95f, nullptr),
              NIMCP_STATS_ERROR_NULL);
}

TEST_F(StatisticsExceptionTest, Null_TTestTwoSample) {
    nimcp_test_result_t result;
    float a[] = {1.0f, 2.0f, 3.0f};
    float b[] = {4.0f, 5.0f, 6.0f};

    // Signature: nimcp_stats_ttest_two_sample(data1, n1, data2, n2, equal_var, type, confidence, result)
    EXPECT_EQ(nimcp_stats_ttest_two_sample(nullptr, 3, b, 3, false, NIMCP_TEST_TWO_SIDED, 0.95f, &result),
              NIMCP_STATS_ERROR_NULL);
    EXPECT_EQ(nimcp_stats_ttest_two_sample(a, 3, nullptr, 3, false, NIMCP_TEST_TWO_SIDED, 0.95f, &result),
              NIMCP_STATS_ERROR_NULL);
    EXPECT_EQ(nimcp_stats_ttest_two_sample(a, 3, b, 3, false, NIMCP_TEST_TWO_SIDED, 0.95f, nullptr),
              NIMCP_STATS_ERROR_NULL);
}

TEST_F(StatisticsExceptionTest, Null_ChiSquaredTest) {
    nimcp_test_result_t result;
    float observed[] = {10, 20, 30};
    float expected[] = {15, 20, 25};

    // Signature: nimcp_stats_chi_squared_gof(observed, expected, n, result)
    EXPECT_EQ(nimcp_stats_chi_squared_gof(nullptr, expected, 3, &result),
              NIMCP_STATS_ERROR_NULL);
    EXPECT_EQ(nimcp_stats_chi_squared_gof(observed, nullptr, 3, &result),
              NIMCP_STATS_ERROR_NULL);
}

//=============================================================================
// NULL Pointer Tests - Correlation
//=============================================================================

TEST_F(StatisticsExceptionTest, Null_PearsonCorrelation) {
    nimcp_correlation_result_t result;
    float x[] = {1.0f, 2.0f, 3.0f};
    float y[] = {2.0f, 4.0f, 6.0f};

    EXPECT_EQ(nimcp_stats_correlation_pearson(nullptr, y, 3, &result), NIMCP_STATS_ERROR_NULL);
    EXPECT_EQ(nimcp_stats_correlation_pearson(x, nullptr, 3, &result), NIMCP_STATS_ERROR_NULL);
    EXPECT_EQ(nimcp_stats_correlation_pearson(x, y, 3, nullptr), NIMCP_STATS_ERROR_NULL);
}

TEST_F(StatisticsExceptionTest, Null_SpearmanCorrelation) {
    nimcp_correlation_result_t result;
    float x[] = {1.0f, 2.0f, 3.0f};
    float y[] = {2.0f, 4.0f, 6.0f};

    EXPECT_EQ(nimcp_stats_correlation_spearman(nullptr, y, 3, &result), NIMCP_STATS_ERROR_NULL);
    EXPECT_EQ(nimcp_stats_correlation_spearman(x, nullptr, 3, &result), NIMCP_STATS_ERROR_NULL);
}

//=============================================================================
// Zero/Invalid Size Tests
//=============================================================================

TEST_F(StatisticsExceptionTest, ZeroSize_Mean) {
    float data[] = {1.0f, 2.0f};
    EXPECT_TRUE(std::isnan(nimcp_stats_mean(data, 0)));
}

TEST_F(StatisticsExceptionTest, ZeroSize_Variance) {
    float data[] = {1.0f, 2.0f};
    EXPECT_TRUE(std::isnan(nimcp_stats_variance(data, 0)));
}

TEST_F(StatisticsExceptionTest, ZeroSize_Entropy) {
    float data[] = {0.5f, 0.5f};
    EXPECT_TRUE(std::isnan(nimcp_stats_entropy(data, 0)));
}

TEST_F(StatisticsExceptionTest, SingleElement_Variance) {
    float data[] = {42.0f};
    // Variance of single element should be 0 or NaN depending on implementation
    float var = nimcp_stats_variance(data, 1);
    EXPECT_TRUE(std::isnan(var) || var == 0.0f);
}

TEST_F(StatisticsExceptionTest, SingleElement_Skewness) {
    float data[] = {42.0f};
    EXPECT_TRUE(std::isnan(nimcp_stats_skewness(data, 1)));
}

TEST_F(StatisticsExceptionTest, TooSmall_TTest) {
    nimcp_test_result_t result;
    float data[] = {1.0f};  // Need at least 2 elements

    EXPECT_EQ(nimcp_stats_ttest_one_sample(data, 1, 0, NIMCP_TEST_TWO_SIDED, 0.95f, &result),
              NIMCP_STATS_ERROR_SIZE);
}

//=============================================================================
// Invalid Parameter Tests
//=============================================================================

TEST_F(StatisticsExceptionTest, InvalidQuantile_OutOfRange) {
    float data[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};

    EXPECT_TRUE(std::isnan(nimcp_stats_quantile(data, 5, -0.1f)));
    EXPECT_TRUE(std::isnan(nimcp_stats_quantile(data, 5, 1.1f)));
    EXPECT_TRUE(std::isnan(nimcp_stats_quantile(data, 5, 2.0f)));
}

TEST_F(StatisticsExceptionTest, InvalidConfidence_TTest) {
    nimcp_test_result_t result;
    float data[] = {1.0f, 2.0f, 3.0f};

    // Confidence should be in (0, 1)
    EXPECT_EQ(nimcp_stats_ttest_one_sample(data, 3, 0, NIMCP_TEST_TWO_SIDED, 0.0f, &result),
              NIMCP_STATS_ERROR_PARAMS);
    EXPECT_EQ(nimcp_stats_ttest_one_sample(data, 3, 0, NIMCP_TEST_TWO_SIDED, 1.0f, &result),
              NIMCP_STATS_ERROR_PARAMS);
    EXPECT_EQ(nimcp_stats_ttest_one_sample(data, 3, 0, NIMCP_TEST_TWO_SIDED, -0.5f, &result),
              NIMCP_STATS_ERROR_PARAMS);
    EXPECT_EQ(nimcp_stats_ttest_one_sample(data, 3, 0, NIMCP_TEST_TWO_SIDED, 1.5f, &result),
              NIMCP_STATS_ERROR_PARAMS);
}

TEST_F(StatisticsExceptionTest, InvalidDegreesOfFreedom) {
    // CDF with df <= 0
    EXPECT_TRUE(std::isnan(nimcp_stats_cdf_student_t(1.0f, 0.0f)));
    EXPECT_TRUE(std::isnan(nimcp_stats_cdf_student_t(1.0f, -1.0f)));

    EXPECT_TRUE(std::isnan(nimcp_stats_cdf_chi_squared(1.0f, 0.0f)));
    EXPECT_TRUE(std::isnan(nimcp_stats_cdf_f(1.0f, 0.0f, 5.0f)));
    EXPECT_TRUE(std::isnan(nimcp_stats_cdf_f(1.0f, 5.0f, 0.0f)));
}

TEST_F(StatisticsExceptionTest, InvalidDistributionParams) {
    // Normal with negative sigma
    EXPECT_TRUE(std::isnan(nimcp_stats_pdf_normal(0.0f, 0.0f, -1.0f)));
    EXPECT_TRUE(std::isnan(nimcp_stats_cdf_normal(0.0f, 0.0f, -1.0f)));

    // Beta with invalid alpha/beta
    EXPECT_TRUE(std::isnan(nimcp_stats_pdf_beta(0.5f, 0.0f, 1.0f)));
    EXPECT_TRUE(std::isnan(nimcp_stats_pdf_beta(0.5f, 1.0f, 0.0f)));
    EXPECT_TRUE(std::isnan(nimcp_stats_pdf_beta(0.5f, -1.0f, 1.0f)));

    // Gamma with invalid shape/scale
    EXPECT_TRUE(std::isnan(nimcp_stats_pdf_gamma(1.0f, 0.0f, 1.0f)));
    EXPECT_TRUE(std::isnan(nimcp_stats_pdf_gamma(1.0f, 1.0f, 0.0f)));
}

//=============================================================================
// Numerical Edge Cases
//=============================================================================

TEST_F(StatisticsExceptionTest, Entropy_ZeroProbability) {
    // Entropy should handle zero probabilities without NaN
    float probs[] = {0.5f, 0.0f, 0.5f, 0.0f};
    float h = nimcp_stats_entropy(probs, 4);

    EXPECT_FALSE(std::isnan(h));
    EXPECT_FALSE(std::isinf(h));
    EXPECT_GE(h, 0.0f);
}

TEST_F(StatisticsExceptionTest, Entropy_AllZero) {
    float probs[] = {0.0f, 0.0f, 0.0f};
    float h = nimcp_stats_entropy(probs, 3);

    // Should be 0 or handle gracefully
    EXPECT_FALSE(std::isinf(h));
}

TEST_F(StatisticsExceptionTest, KLDivergence_QHasZero) {
    float p[] = {0.5f, 0.5f};
    float q[] = {1.0f, 0.0f};  // Q has zero where P is non-zero

    float kl = nimcp_stats_kl_divergence(p, q, 2);

    // Should be +infinity, not NaN
    EXPECT_TRUE(std::isinf(kl));
    EXPECT_GT(kl, 0.0f);
}

TEST_F(StatisticsExceptionTest, LogOfZero_Protected) {
    // All log computations should be protected
    float probs[] = {1.0f, 0.0f, 0.0f};  // Degenerate distribution
    float h = nimcp_stats_entropy(probs, 3);

    EXPECT_FALSE(std::isnan(h));
    EXPECT_NEAR(h, 0.0f, 1e-5f);  // Entropy of deterministic = 0
}

TEST_F(StatisticsExceptionTest, VerySmallProbabilities) {
    float probs[] = {1.0f - 1e-10f, 1e-10f};
    float h = nimcp_stats_entropy(probs, 2);

    EXPECT_FALSE(std::isnan(h));
    EXPECT_FALSE(std::isinf(h));
    EXPECT_GE(h, 0.0f);
}

TEST_F(StatisticsExceptionTest, VeryLargeValues) {
    float data[] = {1e30f, 2e30f, 3e30f};

    float mean = nimcp_stats_mean(data, 3);
    EXPECT_FALSE(std::isnan(mean));
    EXPECT_NEAR(mean, 2e30f, 1e25f);

    float var = nimcp_stats_variance(data, 3);
    EXPECT_FALSE(std::isnan(var));
    EXPECT_GT(var, 0.0f);
}

TEST_F(StatisticsExceptionTest, VerySmallValues) {
    float data[] = {1e-30f, 2e-30f, 3e-30f};

    float mean = nimcp_stats_mean(data, 3);
    EXPECT_FALSE(std::isnan(mean));
    EXPECT_NEAR(mean, 2e-30f, 1e-35f);
}

TEST_F(StatisticsExceptionTest, MixedExtremeValues) {
    float data[] = {-1e30f, 0.0f, 1e30f};

    float mean = nimcp_stats_mean(data, 3);
    EXPECT_FALSE(std::isnan(mean));
    EXPECT_NEAR(mean, 0.0f, 1e25f);
}

TEST_F(StatisticsExceptionTest, Infinity_Handling) {
    float data[] = {1.0f, std::numeric_limits<float>::infinity(), 3.0f};

    // Mean with infinity should be infinity
    float mean = nimcp_stats_mean(data, 3);
    EXPECT_TRUE(std::isinf(mean));
}

TEST_F(StatisticsExceptionTest, NaN_Propagation) {
    float data[] = {1.0f, std::numeric_limits<float>::quiet_NaN(), 3.0f};

    // NaN should propagate
    float mean = nimcp_stats_mean(data, 3);
    EXPECT_TRUE(std::isnan(mean));
}

//=============================================================================
// Shannon Integration Exception Tests
//=============================================================================

TEST_F(StatisticsExceptionTest, ChannelCapacity_InvalidInputs) {
    // Negative bandwidth
    EXPECT_TRUE(std::isnan(nimcp_stats_channel_capacity(-1.0f, 10.0f)));

    // Negative SNR
    EXPECT_TRUE(std::isnan(nimcp_stats_channel_capacity(100.0f, -1.0f)));

    // Zero bandwidth is valid (capacity = 0)
    EXPECT_NEAR(nimcp_stats_channel_capacity(0.0f, 10.0f), 0.0f, 1e-6f);
}

TEST_F(StatisticsExceptionTest, SNRConversion_EdgeCases) {
    // 0 dB = SNR of 1
    EXPECT_NEAR(nimcp_stats_snr_from_db(0.0f), 1.0f, 1e-5f);

    // Very negative dB
    float snr = nimcp_stats_snr_from_db(-100.0f);
    EXPECT_GT(snr, 0.0f);
    EXPECT_LT(snr, 1e-5f);

    // SNR of 0 -> -infinity dB
    float db = nimcp_stats_snr_to_db(0.0f);
    EXPECT_TRUE(std::isinf(db) && db < 0);
}

//=============================================================================
// Quantum Statistics Exception Tests
//=============================================================================

class QuantumStatisticsExceptionTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(QuantumStatisticsExceptionTest, Null_PureStateCreate) {
    // Zero dimension should return NULL
    qstats_pure_state_t* state = qstats_pure_state_create(0);
    EXPECT_EQ(state, nullptr);
}

TEST_F(QuantumStatisticsExceptionTest, Null_DensityMatrixCreate) {
    qstats_density_matrix_t* dm = qstats_density_matrix_create(0);
    EXPECT_EQ(dm, nullptr);
}

TEST_F(QuantumStatisticsExceptionTest, Null_BornProbabilities) {
    float probs[4];
    EXPECT_EQ(qstats_born_probabilities(nullptr, probs), QSTATS_ERROR_NULL);

    qstats_pure_state_t* state = qstats_pure_state_create(4);
    EXPECT_EQ(qstats_born_probabilities(state, nullptr), QSTATS_ERROR_NULL);
    qstats_pure_state_destroy(state);
}

TEST_F(QuantumStatisticsExceptionTest, Null_DiagonalProbabilities) {
    float probs[4];
    EXPECT_EQ(qstats_diagonal_probabilities(nullptr, probs), QSTATS_ERROR_NULL);

    qstats_density_matrix_t* dm = qstats_density_matrix_create(4);
    EXPECT_EQ(qstats_diagonal_probabilities(dm, nullptr), QSTATS_ERROR_NULL);
    qstats_density_matrix_destroy(dm);
}

TEST_F(QuantumStatisticsExceptionTest, Null_AmplitudeEncode) {
    qstats_pure_state_t* state = qstats_amplitude_encode(nullptr, 4);
    EXPECT_EQ(state, nullptr);

    float probs[] = {0.25f, 0.25f, 0.25f, 0.25f};
    state = qstats_amplitude_encode(probs, 0);
    EXPECT_EQ(state, nullptr);
}

TEST_F(QuantumStatisticsExceptionTest, Null_VonNeumannEntropy) {
    float h = qstats_von_neumann_entropy(nullptr);
    EXPECT_TRUE(std::isnan(h));
}

TEST_F(QuantumStatisticsExceptionTest, Null_Fidelity) {
    qstats_density_matrix_t* dm = qstats_density_matrix_create(4);

    float f1 = qstats_fidelity(nullptr, dm);
    EXPECT_TRUE(std::isnan(f1));

    float f2 = qstats_fidelity(dm, nullptr);
    EXPECT_TRUE(std::isnan(f2));

    qstats_density_matrix_destroy(dm);
}

TEST_F(QuantumStatisticsExceptionTest, Null_TraceDistance) {
    qstats_density_matrix_t* dm = qstats_density_matrix_create(4);

    float d1 = qstats_trace_distance(nullptr, dm);
    EXPECT_TRUE(std::isnan(d1));

    float d2 = qstats_trace_distance(dm, nullptr);
    EXPECT_TRUE(std::isnan(d2));

    qstats_density_matrix_destroy(dm);
}

TEST_F(QuantumStatisticsExceptionTest, DimensionMismatch_Fidelity) {
    qstats_density_matrix_t* dm1 = qstats_density_matrix_create(2);
    qstats_density_matrix_t* dm2 = qstats_density_matrix_create(4);

    float f = qstats_fidelity(dm1, dm2);
    EXPECT_TRUE(std::isnan(f));

    qstats_density_matrix_destroy(dm1);
    qstats_density_matrix_destroy(dm2);
}

TEST_F(QuantumStatisticsExceptionTest, DimensionMismatch_PartialTrace) {
    qstats_density_matrix_t* dm = qstats_density_matrix_create(4);

    // dim_a * dim_b must equal dim
    qstats_density_matrix_t* rho_a = qstats_partial_trace_b(dm, 3, 2);  // 3*2=6 != 4
    EXPECT_EQ(rho_a, nullptr);

    qstats_density_matrix_destroy(dm);
}

TEST_F(QuantumStatisticsExceptionTest, Null_Measure) {
    qstats_measurement_t result;
    uint32_t seed = 12345;

    EXPECT_EQ(qstats_measure(nullptr, &result, &seed), QSTATS_ERROR_NULL);

    qstats_pure_state_t* state = qstats_pure_state_create(4);
    EXPECT_EQ(qstats_measure(state, nullptr, &seed), QSTATS_ERROR_NULL);
    EXPECT_EQ(qstats_measure(state, &result, nullptr), QSTATS_ERROR_NULL);
    qstats_pure_state_destroy(state);
}

TEST_F(QuantumStatisticsExceptionTest, Null_QuantumWalkEntropy) {
    float real[] = {0.5f, 0.5f, 0.5f, 0.5f};
    float imag[] = {0.0f, 0.0f, 0.0f, 0.0f};

    float h1 = qstats_quantum_walk_entropy(nullptr, imag, 4);
    EXPECT_TRUE(std::isnan(h1));

    float h2 = qstats_quantum_walk_entropy(real, nullptr, 4);
    EXPECT_TRUE(std::isnan(h2));

    float h3 = qstats_quantum_walk_entropy(real, imag, 0);
    EXPECT_TRUE(std::isnan(h3));
}

TEST_F(QuantumStatisticsExceptionTest, Null_BoltzmannDistribution) {
    float energies[] = {0.0f, 1.0f};
    float probs[2];

    EXPECT_EQ(qstats_boltzmann_distribution(nullptr, 2, 1.0f, probs), QSTATS_ERROR_NULL);
    EXPECT_EQ(qstats_boltzmann_distribution(energies, 2, 1.0f, nullptr), QSTATS_ERROR_NULL);
    EXPECT_EQ(qstats_boltzmann_distribution(energies, 0, 1.0f, probs), QSTATS_ERROR_NULL);
}

TEST_F(QuantumStatisticsExceptionTest, InvalidTemperature_Boltzmann) {
    float energies[] = {0.0f, 1.0f};
    float probs[2];

    EXPECT_EQ(qstats_boltzmann_distribution(energies, 2, 0.0f, probs), QSTATS_ERROR_INVALID);
    EXPECT_EQ(qstats_boltzmann_distribution(energies, 2, -1.0f, probs), QSTATS_ERROR_INVALID);
}

TEST_F(QuantumStatisticsExceptionTest, FreeEnergy_InvalidInputs) {
    float energies[] = {0.0f, 1.0f};

    EXPECT_TRUE(std::isnan(qstats_free_energy(nullptr, 2, 1.0f)));
    EXPECT_TRUE(std::isnan(qstats_free_energy(energies, 0, 1.0f)));
    EXPECT_TRUE(std::isnan(qstats_free_energy(energies, 2, 0.0f)));
    EXPECT_TRUE(std::isnan(qstats_free_energy(energies, 2, -1.0f)));
}

TEST_F(QuantumStatisticsExceptionTest, DestroyNull_Safe) {
    // These should not crash
    qstats_pure_state_destroy(nullptr);
    qstats_density_matrix_destroy(nullptr);
    qstats_fisher_result_free(nullptr);

    SUCCEED();  // If we get here, no crash
}

TEST_F(QuantumStatisticsExceptionTest, InnerProduct_DimensionMismatch) {
    qstats_pure_state_t* psi = qstats_pure_state_create(2);
    qstats_pure_state_t* phi = qstats_pure_state_create(4);

    qstats_complex_t ip = qstats_inner_product(psi, phi);
    EXPECT_NEAR(ip.real, 0.0f, 1e-6f);
    EXPECT_NEAR(ip.imag, 0.0f, 1e-6f);

    qstats_pure_state_destroy(psi);
    qstats_pure_state_destroy(phi);
}

TEST_F(QuantumStatisticsExceptionTest, Concurrence_WrongDimension) {
    // Concurrence only valid for 2-qubit (dim=4)
    qstats_density_matrix_t* dm = qstats_density_matrix_create(8);

    float C = qstats_concurrence(dm);
    EXPECT_TRUE(std::isnan(C));

    qstats_density_matrix_destroy(dm);
}

//=============================================================================
// Memory Safety Tests (no crashes)
//=============================================================================

TEST_F(StatisticsExceptionTest, NoCrash_AllNullInputs) {
    // Ensure no segfaults with all null inputs
    nimcp_stats_mean(nullptr, 0);
    nimcp_stats_variance(nullptr, 0);
    nimcp_stats_std_dev(nullptr, 0);
    nimcp_stats_entropy(nullptr, 0);
    nimcp_stats_kl_divergence(nullptr, nullptr, 0);

    nimcp_test_result_t result;
    nimcp_stats_ttest_one_sample(nullptr, 0, 0, NIMCP_TEST_TWO_SIDED, 0, nullptr);

    nimcp_correlation_result_t corr_result;
    nimcp_stats_correlation_pearson(nullptr, nullptr, 0, nullptr);

    SUCCEED();  // If we reach here, no crash
}

TEST_F(QuantumStatisticsExceptionTest, NoCrash_AllNullInputs) {
    qstats_born_probabilities(nullptr, nullptr);
    qstats_diagonal_probabilities(nullptr, nullptr);
    qstats_von_neumann_entropy(nullptr);
    qstats_fidelity(nullptr, nullptr);
    qstats_trace_distance(nullptr, nullptr);
    qstats_quantum_relative_entropy(nullptr, nullptr);
    qstats_quantum_mutual_information(nullptr, 0, 0);
    qstats_entanglement_entropy(nullptr, 0, 0);
    qstats_concurrence(nullptr);
    qstats_partial_trace_a(nullptr, 0, 0);
    qstats_partial_trace_b(nullptr, 0, 0);
    qstats_quantum_walk_entropy(nullptr, nullptr, 0);
    qstats_boltzmann_distribution(nullptr, 0, 0, nullptr);
    qstats_free_energy(nullptr, 0, 0);

    uint32_t seed = 12345;
    qstats_measurement_t result;
    qstats_measure(nullptr, nullptr, nullptr);

    SUCCEED();  // If we reach here, no crash
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
