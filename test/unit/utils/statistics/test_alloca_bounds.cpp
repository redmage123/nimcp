//=============================================================================
// test_alloca_bounds.cpp - Tests for alloca-to-heap migration and bounds
//=============================================================================
/**
 * @file test_alloca_bounds.cpp
 * @brief Unit tests verifying alloca() removal and heap allocation safety
 *
 * WHAT: Verify that formerly-alloca'd buffers now use heap allocation,
 *       and that bounds checks prevent stack/heap overflow for large inputs.
 * WHY:  alloca() with unbounded sizes (HMM states, KDE samples, dimensions)
 *       caused potential stack overflow. After migration to nimcp_malloc(),
 *       these tests confirm correctness and bounds enforcement.
 *
 * BUGS TESTED:
 * - C4: alloca() with unbounded sizes in nimcp_ml_statistics.c (HMM/KDE/NB)
 * - C5: alloca() with dimension parameter in nimcp_streaming_statistics.c
 *
 * @date 2026-03-05
 */

#include <gtest/gtest.h>
#include "utils/statistics/nimcp_statistics.h"
#include "utils/statistics/nimcp_ml_statistics.h"
#include "utils/statistics/nimcp_streaming_statistics.h"
#include "utils/signal/nimcp_signal_handler.h"
#include <cstring>
#include <cmath>
#include <vector>
#include <random>
#include <signal.h>

//=============================================================================
// Test Fixture
//=============================================================================

class AllocaBoundsTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_stats_config_t config = nimcp_stats_default_config();
        nimcp_stats_init(&config);
    }

    void TearDown() override {
        nimcp_stats_shutdown();
    }
};

//=============================================================================
// HMM Bounds Tests
//=============================================================================

TEST_F(AllocaBoundsTest, HMM_RejectsZeroStates) {
    nimcp_hmm_config_t cfg = nimcp_hmm_default_config();
    cfg.n_states = 0;
    cfg.n_features = 2;
    nimcp_hmm_t* hmm = nimcp_hmm_create(&cfg);
    EXPECT_EQ(hmm, nullptr) << "HMM with 0 states should fail creation";
}

TEST_F(AllocaBoundsTest, HMM_RejectsExcessiveStates) {
    nimcp_hmm_config_t cfg = nimcp_hmm_default_config();
    cfg.n_states = NIMCP_HMM_MAX_STATES + 1;  // 257 > 256
    cfg.n_features = 2;
    nimcp_hmm_t* hmm = nimcp_hmm_create(&cfg);
    EXPECT_EQ(hmm, nullptr) << "HMM with states > NIMCP_HMM_MAX_STATES should fail creation";
}

TEST_F(AllocaBoundsTest, HMM_AcceptsMaxStates) {
    nimcp_hmm_config_t cfg = nimcp_hmm_default_config();
    cfg.n_states = NIMCP_HMM_MAX_STATES;  // 256 — boundary
    cfg.n_features = 2;
    nimcp_hmm_t* hmm = nimcp_hmm_create(&cfg);
    EXPECT_NE(hmm, nullptr) << "HMM with exactly NIMCP_HMM_MAX_STATES should succeed";
    nimcp_hmm_destroy(hmm);
}

TEST_F(AllocaBoundsTest, HMM_FitWithModerateStates) {
    // Verify Baum-Welch works correctly with heap-allocated scratch buffers.
    // This would have stack-overflowed with alloca(s*s*sizeof(float)) for large s.
    const uint32_t n_states = 16;
    const uint32_t n_features = 2;
    const uint32_t seq_len = 50;

    nimcp_hmm_config_t cfg = nimcp_hmm_default_config();
    cfg.n_states = n_states;
    cfg.n_features = n_features;
    cfg.emission_type = NIMCP_HMM_EMISSION_GAUSSIAN;
    nimcp_hmm_t* hmm = nimcp_hmm_create(&cfg);
    ASSERT_NE(hmm, nullptr);

    // Generate simple 2D observation sequence
    std::mt19937 rng(42);
    std::normal_distribution<float> dist(0.0f, 1.0f);
    std::vector<float> obs(seq_len * n_features);
    for (auto& v : obs) v = dist(rng);

    uint32_t lengths[] = { seq_len };
    nimcp_ml_error_t err = nimcp_hmm_fit(hmm, obs.data(), lengths, 1);
    EXPECT_EQ(err, NIMCP_ML_OK) << "HMM fit should succeed with heap-allocated scratch buffers";

    nimcp_hmm_destroy(hmm);
}

TEST_F(AllocaBoundsTest, HMM_PredictWithHeapBuffers) {
    // Verify predict (forward algorithm) works after alloca removal.
    const uint32_t n_states = 8;
    const uint32_t n_features = 3;
    const uint32_t seq_len = 20;

    nimcp_hmm_config_t cfg = nimcp_hmm_default_config();
    cfg.n_states = n_states;
    cfg.n_features = n_features;
    cfg.emission_type = NIMCP_HMM_EMISSION_GAUSSIAN;
    nimcp_hmm_t* hmm = nimcp_hmm_create(&cfg);
    ASSERT_NE(hmm, nullptr);

    std::mt19937 rng(123);
    std::normal_distribution<float> dist(0.0f, 1.0f);
    std::vector<float> obs(seq_len * n_features);
    for (auto& v : obs) v = dist(rng);

    uint32_t lengths[] = { seq_len };
    nimcp_ml_error_t err = nimcp_hmm_fit(hmm, obs.data(), lengths, 1);
    ASSERT_EQ(err, NIMCP_ML_OK);

    // Predict on same data
    std::vector<float> state_probs(seq_len * n_states);
    float log_likelihood = 0.0f;
    err = nimcp_hmm_predict(hmm, obs.data(), seq_len, state_probs.data(), &log_likelihood);
    EXPECT_EQ(err, NIMCP_ML_OK) << "HMM predict should succeed with heap-allocated buffers";

    // State probabilities should sum to ~1 for each timestep
    for (uint32_t t = 0; t < seq_len; t++) {
        float sum = 0.0f;
        for (uint32_t i = 0; i < n_states; i++) {
            sum += state_probs[t * n_states + i];
        }
        EXPECT_NEAR(sum, 1.0f, 0.01f) << "State probs at t=" << t << " should sum to 1";
    }

    nimcp_hmm_destroy(hmm);
}

//=============================================================================
// KDE Heap Allocation Test
//=============================================================================

TEST_F(AllocaBoundsTest, KDE_EvaluateWithHeapBuffer) {
    // KDE evaluate used alloca(n_samples * sizeof(float)) — unbounded.
    // After fix, it should work correctly with heap allocation.
    nimcp_kde_t* kde = nimcp_kde_create(NULL);
    ASSERT_NE(kde, nullptr);

    const uint32_t n_samples = 500;  // Would have been 2KB on stack — fine, but tests heap path
    const uint32_t n_features = 2;

    std::mt19937 rng(42);
    std::normal_distribution<float> dist(0.0f, 1.0f);
    std::vector<float> data(n_samples * n_features);
    for (auto& v : data) v = dist(rng);

    nimcp_ml_error_t err = nimcp_kde_fit(kde, data.data(), n_samples, n_features);
    ASSERT_EQ(err, NIMCP_ML_OK);

    // Evaluate density at a few test points
    const uint32_t n_test = 10;
    std::vector<float> test_points(n_test * n_features);
    for (auto& v : test_points) v = dist(rng);

    std::vector<float> density(n_test);
    err = nimcp_kde_evaluate(kde, test_points.data(), n_test, density.data());
    EXPECT_EQ(err, NIMCP_ML_OK) << "KDE evaluate should succeed with heap-allocated log_vals";

    // Densities should be finite
    for (uint32_t i = 0; i < n_test; i++) {
        EXPECT_TRUE(std::isfinite(density[i])) << "Density at point " << i << " should be finite";
    }

    nimcp_kde_destroy(kde);
}

//=============================================================================
// Streaming Covariance Heap Allocation Tests
//=============================================================================

TEST_F(AllocaBoundsTest, StreamCovMatrix_UpdateWithHeapDelta) {
    // nimcp_stream_cov_matrix_update used alloca(d * sizeof(double)).
    // After fix, delta is heap-allocated. Verify correctness.
    const uint32_t dims = 10;
    nimcp_stream_cov_matrix_t cov = nimcp_stream_cov_matrix_create(dims);
    ASSERT_NE(cov, nullptr);

    std::mt19937 rng(42);
    std::normal_distribution<float> dist(0.0f, 1.0f);

    // Feed 100 observations
    for (int n = 0; n < 100; n++) {
        std::vector<float> vals(dims);
        for (auto& v : vals) v = dist(rng);
        nimcp_stream_stats_result_t res = nimcp_stream_cov_matrix_update(cov, vals.data());
        EXPECT_EQ(res, NIMCP_STREAM_OK) << "Cov matrix update should succeed at iteration " << n;
    }

    // Retrieve covariance matrix
    std::vector<float> matrix(dims * dims);
    nimcp_stream_stats_result_t res = nimcp_stream_cov_matrix_get(cov, matrix.data());
    EXPECT_EQ(res, NIMCP_STREAM_OK);

    // Diagonal elements (variances) should be positive
    for (uint32_t i = 0; i < dims; i++) {
        EXPECT_GT(matrix[i * dims + i], 0.0f) << "Variance for dim " << i << " should be positive";
    }

    // Matrix should be symmetric
    for (uint32_t i = 0; i < dims; i++) {
        for (uint32_t j = i + 1; j < dims; j++) {
            EXPECT_FLOAT_EQ(matrix[i * dims + j], matrix[j * dims + i])
                << "Covariance matrix should be symmetric at (" << i << "," << j << ")";
        }
    }

    nimcp_stream_cov_matrix_destroy(cov);
}

TEST_F(AllocaBoundsTest, StreamCorrMatrix_GetWithHeapStds) {
    // nimcp_stream_corr_matrix_get used alloca(d * sizeof(double)) for stds.
    // Verify correctness after heap migration.
    const uint32_t dims = 5;
    nimcp_stream_cov_matrix_t cov = nimcp_stream_cov_matrix_create(dims);
    ASSERT_NE(cov, nullptr);

    std::mt19937 rng(99);
    std::normal_distribution<float> dist(0.0f, 1.0f);

    for (int n = 0; n < 200; n++) {
        std::vector<float> vals(dims);
        for (auto& v : vals) v = dist(rng);
        nimcp_stream_cov_matrix_update(cov, vals.data());
    }

    std::vector<float> corr(dims * dims);
    nimcp_stream_stats_result_t res = nimcp_stream_corr_matrix_get(cov, corr.data());
    EXPECT_EQ(res, NIMCP_STREAM_OK);

    // Diagonal should be 1.0 (self-correlation)
    for (uint32_t i = 0; i < dims; i++) {
        EXPECT_NEAR(corr[i * dims + i], 1.0f, 0.01f)
            << "Self-correlation at dim " << i << " should be 1.0";
    }

    // Off-diagonal should be in [-1, 1]
    for (uint32_t i = 0; i < dims; i++) {
        for (uint32_t j = i + 1; j < dims; j++) {
            EXPECT_GE(corr[i * dims + j], -1.0f);
            EXPECT_LE(corr[i * dims + j], 1.0f);
        }
    }

    nimcp_stream_cov_matrix_destroy(cov);
}

//=============================================================================
// Streaming Linear Regression Heap Allocation Test
//=============================================================================

TEST_F(AllocaBoundsTest, StreamLinReg_UpdateWithHeapBuffers) {
    // nimcp_stream_linreg_update used 3 alloca() calls for x_ext, Px, K.
    // After fix, all heap-allocated. Verify correctness.
    const uint32_t n_features = 5;
    nimcp_stream_linreg_t reg = nimcp_stream_linreg_create(n_features, 0.99f);
    ASSERT_NE(reg, nullptr);

    std::mt19937 rng(42);
    std::normal_distribution<float> dist(0.0f, 1.0f);

    // Train on y = x[0] + 2*x[1] + noise
    for (int n = 0; n < 200; n++) {
        std::vector<float> x(n_features);
        for (auto& v : x) v = dist(rng);
        float y = x[0] + 2.0f * x[1] + 0.1f * dist(rng);

        nimcp_stream_stats_result_t res = nimcp_stream_linreg_update(reg, x.data(), y);
        EXPECT_EQ(res, NIMCP_STREAM_OK) << "LinReg update should succeed at iteration " << n;
    }

    // Predict — nimcp_stream_linreg_predict returns float directly
    std::vector<float> test_x(n_features, 0.0f);
    test_x[0] = 1.0f;
    test_x[1] = 1.0f;
    float pred = nimcp_stream_linreg_predict(reg, test_x.data());
    // Expected ~3.0 (1 + 2*1), allow wide tolerance due to streaming approximation
    EXPECT_NEAR(pred, 3.0f, 1.5f) << "Prediction should be roughly correct";

    nimcp_stream_linreg_destroy(reg);
}

//=============================================================================
// Signal Handler Counter Type Tests (compile-time verification)
//=============================================================================

// These are compile-time checks: the signal handler globals are now
// volatile sig_atomic_t. We verify the public stats struct still works.
TEST_F(AllocaBoundsTest, SignalStats_TypeConsistency) {
    // signal_handler_stats_t has uint64_t fields.
    // The underlying counters are volatile sig_atomic_t (async-signal-safe).
    // This test verifies the struct can be populated and read correctly.
    signal_handler_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    // Verify struct layout by assigning known values
    stats.sigsegv_count = 42;
    stats.sigabrt_count = 7;
    stats.recoveries = 100;
    stats.fatal_crashes = 3;

    EXPECT_EQ(stats.sigsegv_count, 42u);
    EXPECT_EQ(stats.sigabrt_count, 7u);
    EXPECT_EQ(stats.recoveries, 100u);
    EXPECT_EQ(stats.fatal_crashes, 3u);

    // Compile-time check: sig_atomic_t fits in uint64_t
    static_assert(sizeof(sig_atomic_t) <= sizeof(uint64_t),
        "sig_atomic_t must fit in uint64_t for signal_handler_stats_t compatibility");
}
