//=============================================================================
// test_fuzzy_gpu_regression.cpp - Regression Tests for GPU Fuzzy Module
//=============================================================================
/**
 * @file test_fuzzy_gpu_regression.cpp
 * @brief Regression tests for GPU-accelerated fuzzy logic operations
 *
 * Tests numerical stability, edge cases, boundary conditions, and ensures
 * consistent behavior across updates. These tests verify that known good
 * behaviors are preserved.
 *
 * COVERAGE:
 *   - Numerical stability (extreme values, precision)
 *   - Boundary conditions for MF evaluation
 *   - Edge cases in defuzzification
 *   - Deterministic behavior across runs
 *   - Known regression scenarios
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstdlib>
#include <vector>
#include <limits>

extern "C" {
#include "gpu/fuzzy/nimcp_fuzzy_gpu.h"
#include "gpu/fuzzy/nimcp_fuzzy_gpu_types.h"
#include "gpu/fuzzy/nimcp_fuzzy_gpu_params.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "utils/fuzzy/nimcp_fuzzy_mf.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class FuzzyGPURegressionTest : public ::testing::Test {
protected:
    nimcp_gpu_context_t* ctx = nullptr;

    void SetUp() override {
        nimcp_gpu_config_t config = nimcp_gpu_config_default();
        ctx = nimcp_gpu_context_create(&config);
        ASSERT_NE(ctx, nullptr) << "Failed to create GPU context";
    }

    void TearDown() override {
        if (ctx) {
            nimcp_gpu_context_destroy(ctx);
            ctx = nullptr;
        }
    }
};

//=============================================================================
// Numerical Stability Tests
//=============================================================================

TEST_F(FuzzyGPURegressionTest, MFEvaluationNumericalStability) {
    // Test MF evaluation with extreme input values
    fuzzy_gpu_mf_t mf;
    mf.type = FUZZY_MF_GAUSSIAN;
    mf.params[0] = 50.0f;   // center
    mf.params[1] = 10.0f;   // sigma
    mf.num_params = 2;
    mf.hedge = FUZZY_HEDGE_NONE;
    mf.alpha_cut = 0.0f;

    std::vector<float> test_inputs = {
        0.0f, 50.0f, 100.0f,             // Normal range
        -1000.0f, 1000.0f,               // Far from center
        49.9999f, 50.0001f,              // Near center (precision test)
        std::numeric_limits<float>::min(),
        std::numeric_limits<float>::max() / 2.0f  // Avoid overflow
    };

    const uint32_t N = test_inputs.size();
    std::vector<float> results(N);

    float* d_inputs = nullptr;
    float* d_outputs = nullptr;
    cudaMalloc(&d_inputs, N * sizeof(float));
    cudaMalloc(&d_outputs, N * sizeof(float));
    cudaMemcpy(d_inputs, test_inputs.data(), N * sizeof(float), cudaMemcpyHostToDevice);

    nimcp_gpu_mf_eval_params_t params = {0};
    params.num_samples = N;

    bool ok = nimcp_gpu_fuzzy_mf_evaluate_batch(ctx, d_inputs, &mf, 1, d_outputs, &params);
    ASSERT_TRUE(ok);

    cudaMemcpy(results.data(), d_outputs, N * sizeof(float), cudaMemcpyDeviceToHost);

    // All results should be valid numbers in [0, 1]
    for (uint32_t i = 0; i < N; i++) {
        EXPECT_FALSE(std::isnan(results[i])) << "NaN at index " << i;
        EXPECT_FALSE(std::isinf(results[i])) << "Inf at index " << i;
        EXPECT_GE(results[i], 0.0f) << "Below 0 at index " << i;
        EXPECT_LE(results[i], 1.0f) << "Above 1 at index " << i;
    }

    // Peak should be at center
    EXPECT_NEAR(results[1], 1.0f, 0.001f) << "Gaussian peak should be 1.0";

    cudaFree(d_inputs);
    cudaFree(d_outputs);
}

TEST_F(FuzzyGPURegressionTest, GaussianMFVerySmallSigma) {
    // Regression: Very small sigma should not cause numerical issues
    fuzzy_gpu_mf_t mf;
    mf.type = FUZZY_MF_GAUSSIAN;
    mf.params[0] = 50.0f;    // center
    mf.params[1] = 0.001f;   // Very small sigma
    mf.num_params = 2;
    mf.hedge = FUZZY_HEDGE_NONE;
    mf.alpha_cut = 0.0f;

    std::vector<float> test_inputs = {49.99f, 50.0f, 50.01f};
    const uint32_t N = test_inputs.size();
    std::vector<float> results(N);

    float* d_inputs = nullptr;
    float* d_outputs = nullptr;
    cudaMalloc(&d_inputs, N * sizeof(float));
    cudaMalloc(&d_outputs, N * sizeof(float));
    cudaMemcpy(d_inputs, test_inputs.data(), N * sizeof(float), cudaMemcpyHostToDevice);

    nimcp_gpu_mf_eval_params_t params = {0};
    params.num_samples = N;

    bool ok = nimcp_gpu_fuzzy_mf_evaluate_batch(ctx, d_inputs, &mf, 1, d_outputs, &params);
    ASSERT_TRUE(ok);

    cudaMemcpy(results.data(), d_outputs, N * sizeof(float), cudaMemcpyDeviceToHost);

    // Should not produce NaN or Inf
    for (uint32_t i = 0; i < N; i++) {
        EXPECT_FALSE(std::isnan(results[i]));
        EXPECT_FALSE(std::isinf(results[i]));
    }

    // Center should still be peak
    EXPECT_GT(results[1], results[0]);
    EXPECT_GT(results[1], results[2]);

    cudaFree(d_inputs);
    cudaFree(d_outputs);
}

TEST_F(FuzzyGPURegressionTest, BellMFNumericalStability) {
    // Bell MF with various parameter combinations
    fuzzy_gpu_mf_t mf;
    mf.type = FUZZY_MF_BELL;
    mf.params[0] = 10.0f;   // width a
    mf.params[1] = 2.0f;    // slope b
    mf.params[2] = 50.0f;   // center c
    mf.num_params = 3;
    mf.hedge = FUZZY_HEDGE_NONE;
    mf.alpha_cut = 0.0f;

    std::vector<float> test_inputs = {0.0f, 25.0f, 50.0f, 75.0f, 100.0f};
    const uint32_t N = test_inputs.size();
    std::vector<float> results(N);

    float* d_inputs = nullptr;
    float* d_outputs = nullptr;
    cudaMalloc(&d_inputs, N * sizeof(float));
    cudaMalloc(&d_outputs, N * sizeof(float));
    cudaMemcpy(d_inputs, test_inputs.data(), N * sizeof(float), cudaMemcpyHostToDevice);

    nimcp_gpu_mf_eval_params_t params = {0};
    params.num_samples = N;

    bool ok = nimcp_gpu_fuzzy_mf_evaluate_batch(ctx, d_inputs, &mf, 1, d_outputs, &params);
    ASSERT_TRUE(ok);

    cudaMemcpy(results.data(), d_outputs, N * sizeof(float), cudaMemcpyDeviceToHost);

    // Bell MF should have peak at center
    EXPECT_NEAR(results[2], 1.0f, 0.001f) << "Bell peak should be at center";

    // Should be symmetric
    EXPECT_NEAR(results[1], results[3], 0.001f) << "Bell should be symmetric";

    cudaFree(d_inputs);
    cudaFree(d_outputs);
}

//=============================================================================
// Boundary Condition Tests
//=============================================================================

TEST_F(FuzzyGPURegressionTest, TriangularMFBoundaries) {
    // Test triangular MF at exact boundary points
    fuzzy_gpu_mf_t mf;
    mf.type = FUZZY_MF_TRIANGULAR;
    mf.params[0] = 20.0f;   // left
    mf.params[1] = 50.0f;   // center
    mf.params[2] = 80.0f;   // right
    mf.num_params = 3;
    mf.hedge = FUZZY_HEDGE_NONE;
    mf.alpha_cut = 0.0f;

    std::vector<float> test_inputs = {
        19.99f, 20.0f, 20.01f,   // Left boundary
        49.99f, 50.0f, 50.01f,   // Peak
        79.99f, 80.0f, 80.01f    // Right boundary
    };
    const uint32_t N = test_inputs.size();
    std::vector<float> results(N);

    float* d_inputs = nullptr;
    float* d_outputs = nullptr;
    cudaMalloc(&d_inputs, N * sizeof(float));
    cudaMalloc(&d_outputs, N * sizeof(float));
    cudaMemcpy(d_inputs, test_inputs.data(), N * sizeof(float), cudaMemcpyHostToDevice);

    nimcp_gpu_mf_eval_params_t params = {0};
    params.num_samples = N;

    bool ok = nimcp_gpu_fuzzy_mf_evaluate_batch(ctx, d_inputs, &mf, 1, d_outputs, &params);
    ASSERT_TRUE(ok);

    cudaMemcpy(results.data(), d_outputs, N * sizeof(float), cudaMemcpyDeviceToHost);

    // Check boundaries
    EXPECT_NEAR(results[0], 0.0f, 0.01f) << "Below left should be ~0";
    EXPECT_NEAR(results[1], 0.0f, 0.01f) << "At left boundary should be 0";
    EXPECT_GT(results[2], 0.0f) << "Just above left should be > 0";

    EXPECT_NEAR(results[4], 1.0f, 0.01f) << "At peak should be 1";

    EXPECT_GT(results[6], 0.0f) << "Just below right should be > 0";
    EXPECT_NEAR(results[7], 0.0f, 0.01f) << "At right boundary should be 0";
    EXPECT_NEAR(results[8], 0.0f, 0.01f) << "Above right should be ~0";

    cudaFree(d_inputs);
    cudaFree(d_outputs);
}

TEST_F(FuzzyGPURegressionTest, TrapezoidalMFPlateau) {
    // Trapezoidal MF should have flat top
    fuzzy_gpu_mf_t mf;
    mf.type = FUZZY_MF_TRAPEZOIDAL;
    mf.params[0] = 10.0f;   // left foot
    mf.params[1] = 30.0f;   // left shoulder
    mf.params[2] = 70.0f;   // right shoulder
    mf.params[3] = 90.0f;   // right foot
    mf.num_params = 4;
    mf.hedge = FUZZY_HEDGE_NONE;
    mf.alpha_cut = 0.0f;

    std::vector<float> plateau_inputs = {35.0f, 50.0f, 65.0f};  // All on plateau
    const uint32_t N = plateau_inputs.size();
    std::vector<float> results(N);

    float* d_inputs = nullptr;
    float* d_outputs = nullptr;
    cudaMalloc(&d_inputs, N * sizeof(float));
    cudaMalloc(&d_outputs, N * sizeof(float));
    cudaMemcpy(d_inputs, plateau_inputs.data(), N * sizeof(float), cudaMemcpyHostToDevice);

    nimcp_gpu_mf_eval_params_t params = {0};
    params.num_samples = N;

    bool ok = nimcp_gpu_fuzzy_mf_evaluate_batch(ctx, d_inputs, &mf, 1, d_outputs, &params);
    ASSERT_TRUE(ok);

    cudaMemcpy(results.data(), d_outputs, N * sizeof(float), cudaMemcpyDeviceToHost);

    // All plateau points should be exactly 1.0
    for (uint32_t i = 0; i < N; i++) {
        EXPECT_NEAR(results[i], 1.0f, 0.001f) << "Plateau point " << i << " should be 1.0";
    }

    cudaFree(d_inputs);
    cudaFree(d_outputs);
}

//=============================================================================
// Hedge Regression Tests
//=============================================================================

TEST_F(FuzzyGPURegressionTest, HedgeVerySquaresValues) {
    // VERY hedge should square membership values
    fuzzy_gpu_mf_t mf;
    mf.type = FUZZY_MF_TRIANGULAR;
    mf.params[0] = 0.0f;
    mf.params[1] = 50.0f;
    mf.params[2] = 100.0f;
    mf.num_params = 3;
    mf.hedge = FUZZY_HEDGE_VERY;
    mf.alpha_cut = 0.0f;

    fuzzy_gpu_mf_t mf_no_hedge = mf;
    mf_no_hedge.hedge = FUZZY_HEDGE_NONE;

    std::vector<float> test_inputs = {25.0f, 35.0f, 75.0f};
    const uint32_t N = test_inputs.size();
    std::vector<float> results_hedged(N), results_plain(N);

    float* d_inputs = nullptr;
    float* d_outputs = nullptr;
    cudaMalloc(&d_inputs, N * sizeof(float));
    cudaMalloc(&d_outputs, N * sizeof(float));
    cudaMemcpy(d_inputs, test_inputs.data(), N * sizeof(float), cudaMemcpyHostToDevice);

    nimcp_gpu_mf_eval_params_t params = {0};
    params.num_samples = N;

    // Evaluate with hedge
    nimcp_gpu_fuzzy_mf_evaluate_batch(ctx, d_inputs, &mf, 1, d_outputs, &params);
    cudaMemcpy(results_hedged.data(), d_outputs, N * sizeof(float), cudaMemcpyDeviceToHost);

    // Evaluate without hedge
    nimcp_gpu_fuzzy_mf_evaluate_batch(ctx, d_inputs, &mf_no_hedge, 1, d_outputs, &params);
    cudaMemcpy(results_plain.data(), d_outputs, N * sizeof(float), cudaMemcpyDeviceToHost);

    // VERY should square: hedged ≈ plain^2
    for (uint32_t i = 0; i < N; i++) {
        float expected = results_plain[i] * results_plain[i];
        EXPECT_NEAR(results_hedged[i], expected, 0.01f)
            << "VERY hedge should square value at " << i;
    }

    cudaFree(d_inputs);
    cudaFree(d_outputs);
}

TEST_F(FuzzyGPURegressionTest, HedgeSomewhatSquareRoots) {
    // SOMEWHAT hedge should take square root
    fuzzy_gpu_mf_t mf;
    mf.type = FUZZY_MF_TRIANGULAR;
    mf.params[0] = 0.0f;
    mf.params[1] = 50.0f;
    mf.params[2] = 100.0f;
    mf.num_params = 3;
    mf.hedge = FUZZY_HEDGE_SOMEWHAT;
    mf.alpha_cut = 0.0f;

    fuzzy_gpu_mf_t mf_no_hedge = mf;
    mf_no_hedge.hedge = FUZZY_HEDGE_NONE;

    std::vector<float> test_inputs = {25.0f, 35.0f, 75.0f};
    const uint32_t N = test_inputs.size();
    std::vector<float> results_hedged(N), results_plain(N);

    float* d_inputs = nullptr;
    float* d_outputs = nullptr;
    cudaMalloc(&d_inputs, N * sizeof(float));
    cudaMalloc(&d_outputs, N * sizeof(float));
    cudaMemcpy(d_inputs, test_inputs.data(), N * sizeof(float), cudaMemcpyHostToDevice);

    nimcp_gpu_mf_eval_params_t params = {0};
    params.num_samples = N;

    nimcp_gpu_fuzzy_mf_evaluate_batch(ctx, d_inputs, &mf, 1, d_outputs, &params);
    cudaMemcpy(results_hedged.data(), d_outputs, N * sizeof(float), cudaMemcpyDeviceToHost);

    nimcp_gpu_fuzzy_mf_evaluate_batch(ctx, d_inputs, &mf_no_hedge, 1, d_outputs, &params);
    cudaMemcpy(results_plain.data(), d_outputs, N * sizeof(float), cudaMemcpyDeviceToHost);

    // SOMEWHAT should sqrt: hedged ≈ sqrt(plain)
    for (uint32_t i = 0; i < N; i++) {
        float expected = std::sqrt(results_plain[i]);
        EXPECT_NEAR(results_hedged[i], expected, 0.01f)
            << "SOMEWHAT hedge should sqrt value at " << i;
    }

    cudaFree(d_inputs);
    cudaFree(d_outputs);
}

//=============================================================================
// Defuzzification Regression Tests
//=============================================================================

TEST_F(FuzzyGPURegressionTest, CentroidDefuzzSymmetric) {
    // Symmetric fuzzy set should have centroid at center
    const uint32_t RESOLUTION = 256;
    std::vector<float> aggregated(RESOLUTION);

    // Create symmetric triangular fuzzy set centered at 0.5
    for (uint32_t i = 0; i < RESOLUTION; i++) {
        float x = static_cast<float>(i) / (RESOLUTION - 1);
        float center = 0.5f;
        float width = 0.3f;
        aggregated[i] = std::max(0.0f, 1.0f - std::abs(x - center) / width);
    }

    float* d_aggregated = nullptr;
    float* d_output = nullptr;
    cudaMalloc(&d_aggregated, RESOLUTION * sizeof(float));
    cudaMalloc(&d_output, sizeof(float));
    cudaMemcpy(d_aggregated, aggregated.data(), RESOLUTION * sizeof(float), cudaMemcpyHostToDevice);

    nimcp_gpu_defuzz_params_t params = {0};
    params.method = FUZZY_DEFUZZ_CENTROID;
    params.resolution = RESOLUTION;
    params.num_samples = 1;
    params.x_min = 0.0f;
    params.x_max = 1.0f;

    bool ok = nimcp_gpu_fuzzy_defuzzify_batch(ctx, d_aggregated, d_output, &params);
    ASSERT_TRUE(ok);

    float result;
    cudaMemcpy(&result, d_output, sizeof(float), cudaMemcpyDeviceToHost);

    // Centroid of symmetric set should be at center
    EXPECT_NEAR(result, 0.5f, 0.02f) << "Centroid of symmetric set should be at center";

    cudaFree(d_aggregated);
    cudaFree(d_output);
}

TEST_F(FuzzyGPURegressionTest, DefuzzificationEmptySet) {
    // Empty (all zeros) fuzzy set should return mid-point or NaN
    const uint32_t RESOLUTION = 256;
    std::vector<float> aggregated(RESOLUTION, 0.0f);

    float* d_aggregated = nullptr;
    float* d_output = nullptr;
    cudaMalloc(&d_aggregated, RESOLUTION * sizeof(float));
    cudaMalloc(&d_output, sizeof(float));
    cudaMemcpy(d_aggregated, aggregated.data(), RESOLUTION * sizeof(float), cudaMemcpyHostToDevice);

    nimcp_gpu_defuzz_params_t params = {0};
    params.method = FUZZY_DEFUZZ_CENTROID;
    params.resolution = RESOLUTION;
    params.num_samples = 1;
    params.x_min = 0.0f;
    params.x_max = 100.0f;

    bool ok = nimcp_gpu_fuzzy_defuzzify_batch(ctx, d_aggregated, d_output, &params);
    ASSERT_TRUE(ok);

    float result;
    cudaMemcpy(&result, d_output, sizeof(float), cudaMemcpyDeviceToHost);

    // Empty set should give mid-point as fallback (implementation-dependent)
    // Just verify it doesn't crash and gives a valid number
    EXPECT_FALSE(std::isinf(result)) << "Should not return infinity";

    cudaFree(d_aggregated);
    cudaFree(d_output);
}

TEST_F(FuzzyGPURegressionTest, MOMDefuzzFlat) {
    // Mean of Maximum with flat top should return center of flat region
    const uint32_t RESOLUTION = 100;
    std::vector<float> aggregated(RESOLUTION, 0.0f);

    // Flat maximum from index 30-50 (x = 0.3 to 0.5)
    for (uint32_t i = 30; i <= 50; i++) {
        aggregated[i] = 1.0f;
    }

    float* d_aggregated = nullptr;
    float* d_output = nullptr;
    cudaMalloc(&d_aggregated, RESOLUTION * sizeof(float));
    cudaMalloc(&d_output, sizeof(float));
    cudaMemcpy(d_aggregated, aggregated.data(), RESOLUTION * sizeof(float), cudaMemcpyHostToDevice);

    nimcp_gpu_defuzz_params_t params = {0};
    params.method = FUZZY_DEFUZZ_MOM;
    params.resolution = RESOLUTION;
    params.num_samples = 1;
    params.x_min = 0.0f;
    params.x_max = 1.0f;

    bool ok = nimcp_gpu_fuzzy_defuzzify_batch(ctx, d_aggregated, d_output, &params);
    ASSERT_TRUE(ok);

    float result;
    cudaMemcpy(&result, d_output, sizeof(float), cudaMemcpyDeviceToHost);

    // MOM should be center of [0.3, 0.5] = 0.4
    EXPECT_NEAR(result, 0.4f, 0.02f) << "MOM should be at center of flat maximum";

    cudaFree(d_aggregated);
    cudaFree(d_output);
}

//=============================================================================
// T-norm/T-conorm Regression Tests
//=============================================================================

TEST_F(FuzzyGPURegressionTest, TnormBoundaryConditions) {
    // T-norm with boundary values should work correctly
    std::vector<float> a = {0.0f, 1.0f, 0.5f, 0.0f, 1.0f};
    std::vector<float> b = {0.0f, 1.0f, 0.5f, 1.0f, 0.0f};
    const uint32_t N = a.size();
    std::vector<float> results(N);

    float* d_a = nullptr;
    float* d_b = nullptr;
    float* d_result = nullptr;
    cudaMalloc(&d_a, N * sizeof(float));
    cudaMalloc(&d_b, N * sizeof(float));
    cudaMalloc(&d_result, N * sizeof(float));
    cudaMemcpy(d_a, a.data(), N * sizeof(float), cudaMemcpyHostToDevice);
    cudaMemcpy(d_b, b.data(), N * sizeof(float), cudaMemcpyHostToDevice);

    bool ok = nimcp_gpu_fuzzy_tnorm_batch(ctx, d_a, d_b, d_result, FUZZY_TNORM_MIN, N);
    ASSERT_TRUE(ok);

    cudaMemcpy(results.data(), d_result, N * sizeof(float), cudaMemcpyDeviceToHost);

    // MIN: min(0,0)=0, min(1,1)=1, min(0.5,0.5)=0.5, min(0,1)=0, min(1,0)=0
    EXPECT_NEAR(results[0], 0.0f, 1e-6f);
    EXPECT_NEAR(results[1], 1.0f, 1e-6f);
    EXPECT_NEAR(results[2], 0.5f, 1e-6f);
    EXPECT_NEAR(results[3], 0.0f, 1e-6f);
    EXPECT_NEAR(results[4], 0.0f, 1e-6f);

    cudaFree(d_a);
    cudaFree(d_b);
    cudaFree(d_result);
}

TEST_F(FuzzyGPURegressionTest, ProductTnormPrecision) {
    // Product T-norm should maintain precision for small values
    std::vector<float> a = {0.001f, 0.01f, 0.1f};
    std::vector<float> b = {0.001f, 0.01f, 0.1f};
    const uint32_t N = a.size();
    std::vector<float> results(N);

    float* d_a = nullptr;
    float* d_b = nullptr;
    float* d_result = nullptr;
    cudaMalloc(&d_a, N * sizeof(float));
    cudaMalloc(&d_b, N * sizeof(float));
    cudaMalloc(&d_result, N * sizeof(float));
    cudaMemcpy(d_a, a.data(), N * sizeof(float), cudaMemcpyHostToDevice);
    cudaMemcpy(d_b, b.data(), N * sizeof(float), cudaMemcpyHostToDevice);

    bool ok = nimcp_gpu_fuzzy_tnorm_batch(ctx, d_a, d_b, d_result, FUZZY_TNORM_PRODUCT, N);
    ASSERT_TRUE(ok);

    cudaMemcpy(results.data(), d_result, N * sizeof(float), cudaMemcpyDeviceToHost);

    // Product: a*b
    EXPECT_NEAR(results[0], 1e-6f, 1e-8f);
    EXPECT_NEAR(results[1], 1e-4f, 1e-6f);
    EXPECT_NEAR(results[2], 0.01f, 1e-4f);

    cudaFree(d_a);
    cudaFree(d_b);
    cudaFree(d_result);
}

//=============================================================================
// Determinism Tests
//=============================================================================

TEST_F(FuzzyGPURegressionTest, DeterministicResults) {
    // Same inputs should always produce same outputs
    fuzzy_gpu_mf_t mf;
    mf.type = FUZZY_MF_GAUSSIAN;
    mf.params[0] = 50.0f;
    mf.params[1] = 10.0f;
    mf.num_params = 2;
    mf.hedge = FUZZY_HEDGE_NONE;
    mf.alpha_cut = 0.0f;

    std::vector<float> inputs(100);
    for (uint32_t i = 0; i < 100; i++) {
        inputs[i] = static_cast<float>(i);
    }

    float* d_inputs = nullptr;
    float* d_outputs = nullptr;
    cudaMalloc(&d_inputs, 100 * sizeof(float));
    cudaMalloc(&d_outputs, 100 * sizeof(float));
    cudaMemcpy(d_inputs, inputs.data(), 100 * sizeof(float), cudaMemcpyHostToDevice);

    nimcp_gpu_mf_eval_params_t params = {0};
    params.num_samples = 100;

    std::vector<float> results1(100), results2(100);

    // First run
    nimcp_gpu_fuzzy_mf_evaluate_batch(ctx, d_inputs, &mf, 1, d_outputs, &params);
    cudaMemcpy(results1.data(), d_outputs, 100 * sizeof(float), cudaMemcpyDeviceToHost);

    // Second run
    nimcp_gpu_fuzzy_mf_evaluate_batch(ctx, d_inputs, &mf, 1, d_outputs, &params);
    cudaMemcpy(results2.data(), d_outputs, 100 * sizeof(float), cudaMemcpyDeviceToHost);

    // Results should be identical
    for (uint32_t i = 0; i < 100; i++) {
        EXPECT_EQ(results1[i], results2[i]) << "Non-deterministic result at " << i;
    }

    cudaFree(d_inputs);
    cudaFree(d_outputs);
}

//=============================================================================
// Known Regression Scenarios
//=============================================================================

TEST_F(FuzzyGPURegressionTest, KnownGaussianValues) {
    // Verify against known Gaussian MF values
    fuzzy_gpu_mf_t mf;
    mf.type = FUZZY_MF_GAUSSIAN;
    mf.params[0] = 0.0f;    // center
    mf.params[1] = 1.0f;    // sigma
    mf.num_params = 2;
    mf.hedge = FUZZY_HEDGE_NONE;
    mf.alpha_cut = 0.0f;

    // Known values for standard Gaussian
    // G(x) = exp(-0.5 * x^2)
    // G(0) = 1.0
    // G(1) = exp(-0.5) ≈ 0.6065
    // G(2) = exp(-2.0) ≈ 0.1353

    std::vector<float> inputs = {0.0f, 1.0f, 2.0f, -1.0f};
    std::vector<float> expected = {1.0f, 0.6065f, 0.1353f, 0.6065f};
    const uint32_t N = inputs.size();
    std::vector<float> results(N);

    float* d_inputs = nullptr;
    float* d_outputs = nullptr;
    cudaMalloc(&d_inputs, N * sizeof(float));
    cudaMalloc(&d_outputs, N * sizeof(float));
    cudaMemcpy(d_inputs, inputs.data(), N * sizeof(float), cudaMemcpyHostToDevice);

    nimcp_gpu_mf_eval_params_t params = {0};
    params.num_samples = N;

    bool ok = nimcp_gpu_fuzzy_mf_evaluate_batch(ctx, d_inputs, &mf, 1, d_outputs, &params);
    ASSERT_TRUE(ok);

    cudaMemcpy(results.data(), d_outputs, N * sizeof(float), cudaMemcpyDeviceToHost);

    for (uint32_t i = 0; i < N; i++) {
        EXPECT_NEAR(results[i], expected[i], 0.001f) << "Known value mismatch at " << i;
    }

    cudaFree(d_inputs);
    cudaFree(d_outputs);
}

TEST_F(FuzzyGPURegressionTest, KnownSigmoidValues) {
    // Verify against known Sigmoid MF values
    fuzzy_gpu_mf_t mf;
    mf.type = FUZZY_MF_SIGMOID;
    mf.params[0] = 1.0f;    // slope
    mf.params[1] = 0.0f;    // center
    mf.num_params = 2;
    mf.hedge = FUZZY_HEDGE_NONE;
    mf.alpha_cut = 0.0f;

    // S(x) = 1 / (1 + exp(-a*(x-c)))
    // S(0) = 0.5
    // S(2) = 1 / (1 + exp(-2)) ≈ 0.8808
    // S(-2) = 1 / (1 + exp(2)) ≈ 0.1192

    std::vector<float> inputs = {0.0f, 2.0f, -2.0f};
    std::vector<float> expected = {0.5f, 0.8808f, 0.1192f};
    const uint32_t N = inputs.size();
    std::vector<float> results(N);

    float* d_inputs = nullptr;
    float* d_outputs = nullptr;
    cudaMalloc(&d_inputs, N * sizeof(float));
    cudaMalloc(&d_outputs, N * sizeof(float));
    cudaMemcpy(d_inputs, inputs.data(), N * sizeof(float), cudaMemcpyHostToDevice);

    nimcp_gpu_mf_eval_params_t params = {0};
    params.num_samples = N;

    bool ok = nimcp_gpu_fuzzy_mf_evaluate_batch(ctx, d_inputs, &mf, 1, d_outputs, &params);
    ASSERT_TRUE(ok);

    cudaMemcpy(results.data(), d_outputs, N * sizeof(float), cudaMemcpyDeviceToHost);

    for (uint32_t i = 0; i < N; i++) {
        EXPECT_NEAR(results[i], expected[i], 0.001f) << "Sigmoid value mismatch at " << i;
    }

    cudaFree(d_inputs);
    cudaFree(d_outputs);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
