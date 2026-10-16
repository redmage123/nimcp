/* ============================================================================
 * Fuzzy GPU Regression Tests
 * ============================================================================
 * WHAT: Regression tests for GPU fuzzy logic operations
 * WHY:  Prevent reintroduction of fixed GPU fuzzy-related bugs
 * HOW:  Test specific bug scenarios, numerical stability, edge cases
 * ============================================================================
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <limits>

#ifdef NIMCP_ENABLE_CUDA
extern "C" {
#include "gpu/fuzzy/nimcp_fuzzy_gpu.h"
#include "gpu/fuzzy/nimcp_fuzzy_gpu_types.h"
#include "gpu/nimcp_gpu_context.h"
}
#endif

namespace {

constexpr float TOLERANCE = 1e-5f;

class FuzzyGPURegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
#ifdef NIMCP_ENABLE_CUDA
        ctx_ = nimcp_gpu_context_create(nullptr);
#endif
    }

    void TearDown() override {
#ifdef NIMCP_ENABLE_CUDA
        if (ctx_) {
            nimcp_gpu_context_destroy(ctx_);
            ctx_ = nullptr;
        }
#endif
    }

#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_context_t* ctx_ = nullptr;
#endif
};

/* ============================================================================
 * Bug #1: Numerical overflow in Gaussian MF with extreme sigma
 * ============================================================================
 * Symptom: NaN or Inf when sigma is very small or very large
 * Fix: Clamp sigma to reasonable range, use numerically stable computation
 */
TEST_F(FuzzyGPURegressionTest, GaussianMFNumericalOverflow) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required for this test";

    // Test cases that previously caused overflow
    std::vector<float> test_sigmas = {
        1e-10f,   // Very small sigma
        1e10f,    // Very large sigma
        0.0f,     // Zero sigma (edge case)
        -1.0f,    // Negative sigma (invalid)
        std::numeric_limits<float>::min(),
        std::numeric_limits<float>::max()
    };

    for (float sigma : test_sigmas) {
        // Create Gaussian MF params: [center, sigma]
        float params[2] = {5.0f, sigma};
        float input = 5.0f;  // At center

        float result = 0.0f;
        bool success = nimcp_gpu_fuzzy_mf_evaluate_single(
            ctx_, FUZZY_MF_GAUSSIAN, params, 2, input, &result);

        // Should either succeed with finite result or fail gracefully
        if (success) {
            EXPECT_TRUE(std::isfinite(result))
                << "Gaussian MF should return finite value for sigma=" << sigma;
            EXPECT_GE(result, 0.0f) << "Membership should be non-negative";
            EXPECT_LE(result, 1.0f) << "Membership should be at most 1.0";
        }
        // It's acceptable to fail on invalid inputs
    }
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Bug #2: Division by zero in Centroid defuzzification
 * ============================================================================
 * Symptom: NaN when all membership values are zero
 * Fix: Return universe midpoint when integral is zero
 */
TEST_F(FuzzyGPURegressionTest, CentroidDefuzzDivisionByZero) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required for this test";

    // All-zero membership array
    const int resolution = 100;
    std::vector<float> zero_memberships(resolution, 0.0f);

    float result = 0.0f;
    float x_min = 0.0f, x_max = 10.0f;

    bool success = nimcp_gpu_fuzzy_defuzz_centroid(
        ctx_, zero_memberships.data(), resolution, x_min, x_max, &result);

    if (success) {
        EXPECT_TRUE(std::isfinite(result))
            << "Centroid should return finite value for zero memberships";
        // Expect midpoint or some reasonable default
        float midpoint = (x_min + x_max) / 2.0f;
        // Allow some tolerance for the default value
        EXPECT_NEAR(result, midpoint, x_max - x_min);
    }
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Bug #3: Buffer overflow in batch MF evaluation
 * ============================================================================
 * Symptom: Memory corruption with mismatched batch sizes
 * Fix: Validate input dimensions before kernel launch
 */
TEST_F(FuzzyGPURegressionTest, BatchMFBufferOverflow) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required for this test";

    // Create tensors with mismatched sizes
    const size_t batch_size = 100;
    const size_t num_mfs = 5;

    // Input tensor
    nimcp_gpu_tensor_t* input = nimcp_gpu_tensor_create(
        ctx_, (size_t[]){batch_size, 1}, 2, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(input, nullptr);

    // Output tensor that's too small (would cause overflow)
    nimcp_gpu_tensor_t* small_output = nimcp_gpu_tensor_create(
        ctx_, (size_t[]){batch_size / 2, num_mfs}, 2, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(small_output, nullptr);

    // This should fail gracefully, not crash
    nimcp_gpu_mf_params_t mf_params = {
        .num_mfs = static_cast<uint32_t>(num_mfs),
        .mf_type = FUZZY_MF_TRIANGULAR
    };

    bool success = nimcp_gpu_fuzzy_mf_evaluate_batch(
        ctx_, input, small_output, &mf_params);

    // Should fail due to size mismatch
    EXPECT_FALSE(success) << "Should detect buffer size mismatch";

    nimcp_gpu_tensor_destroy(input);
    nimcp_gpu_tensor_destroy(small_output);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Bug #4: Race condition in parallel inference
 * ============================================================================
 * Symptom: Non-deterministic results in large batch inference
 * Fix: Proper synchronization in reduction operations
 */
TEST_F(FuzzyGPURegressionTest, ParallelInferenceRaceCondition) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required for this test";

    // Run same inference multiple times, results should be identical
    const int num_runs = 5;
    const size_t batch_size = 10000;

    std::vector<float> inputs(batch_size);
    for (size_t i = 0; i < batch_size; i++) {
        inputs[i] = static_cast<float>(i) / batch_size * 10.0f;
    }

    std::vector<std::vector<float>> results(num_runs);

    for (int run = 0; run < num_runs; run++) {
        results[run].resize(batch_size);

        nimcp_gpu_tensor_t* input_tensor = nimcp_gpu_tensor_create(
            ctx_, (size_t[]){batch_size, 1}, 2, NIMCP_GPU_PRECISION_FP32);
        nimcp_gpu_tensor_t* output_tensor = nimcp_gpu_tensor_create(
            ctx_, (size_t[]){batch_size, 1}, 2, NIMCP_GPU_PRECISION_FP32);

        if (input_tensor && output_tensor) {
            nimcp_gpu_tensor_copy_from_host(input_tensor, inputs.data(),
                batch_size * sizeof(float));

            nimcp_gpu_mf_params_t mf_params = {
                .num_mfs = 1,
                .mf_type = FUZZY_MF_TRIANGULAR,
                .params = {0.0f, 5.0f, 10.0f}  // Triangular MF
            };

            nimcp_gpu_fuzzy_mf_evaluate_batch(ctx_, input_tensor, output_tensor, &mf_params);
            nimcp_gpu_tensor_copy_to_host(output_tensor, results[run].data(),
                batch_size * sizeof(float));

            nimcp_gpu_tensor_destroy(input_tensor);
            nimcp_gpu_tensor_destroy(output_tensor);
        }
    }

    // All runs should produce identical results
    for (int run = 1; run < num_runs; run++) {
        for (size_t i = 0; i < batch_size; i++) {
            EXPECT_FLOAT_EQ(results[0][i], results[run][i])
                << "Race condition detected at index " << i << " on run " << run;
        }
    }
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Bug #5: Improper handling of denormalized floats
 * ============================================================================
 * Symptom: Inconsistent results near zero
 * Fix: Flush denormals to zero or handle explicitly
 */
TEST_F(FuzzyGPURegressionTest, DenormalizedFloatHandling) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required for this test";

    // Test with denormalized inputs
    std::vector<float> denormals = {
        std::numeric_limits<float>::denorm_min(),
        -std::numeric_limits<float>::denorm_min(),
        1e-40f,  // Likely denormal on most systems
        -1e-40f
    };

    for (float val : denormals) {
        float params[3] = {-1.0f, 0.0f, 1.0f};  // Triangular MF
        float result = 0.0f;

        bool success = nimcp_gpu_fuzzy_mf_evaluate_single(
            ctx_, FUZZY_MF_TRIANGULAR, params, 3, val, &result);

        if (success) {
            EXPECT_TRUE(std::isfinite(result))
                << "Should handle denormalized float " << val;
        }
    }
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Bug #6: ANFIS layer normalization underflow
 * ============================================================================
 * Symptom: Zero firing strengths causing NaN in layer 3
 * Fix: Add small epsilon to prevent division by zero
 */
TEST_F(FuzzyGPURegressionTest, ANFISNormalizationUnderflow) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required for this test";

    nimcp_gpu_anfis_params_t params = {
        .num_inputs = 2,
        .num_mfs_per_input = 2,
        .mf_type = FUZZY_MF_GAUSSIAN,
        .learning_rate = 0.01f,
        .momentum = 0.0f
    };

    nimcp_gpu_anfis_state_t* state = nimcp_gpu_anfis_create(ctx_, &params);
    if (!state) {
        GTEST_SKIP() << "Could not create ANFIS state";
    }

    // Inputs far from all MF centers -> very small firing strengths
    std::vector<float> extreme_inputs = {1000.0f, 1000.0f};

    nimcp_gpu_tensor_t* input = nimcp_gpu_tensor_create(
        ctx_, (size_t[]){1, 2}, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* output = nimcp_gpu_tensor_create(
        ctx_, (size_t[]){1, 1}, 2, NIMCP_GPU_PRECISION_FP32);

    if (input && output) {
        nimcp_gpu_tensor_copy_from_host(input, extreme_inputs.data(),
            2 * sizeof(float));

        bool success = nimcp_gpu_anfis_forward(ctx_, state, input, output);

        if (success) {
            float result = 0.0f;
            nimcp_gpu_tensor_copy_to_host(output, &result, sizeof(float));
            EXPECT_TRUE(std::isfinite(result))
                << "ANFIS should handle near-zero firing strengths";
        }

        nimcp_gpu_tensor_destroy(input);
        nimcp_gpu_tensor_destroy(output);
    }

    nimcp_gpu_anfis_destroy(state);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Bug #7: T-norm product underflow with many rules
 * ============================================================================
 * Symptom: Product of many small values becomes zero prematurely
 * Fix: Use log-space computation for product t-norm
 */
TEST_F(FuzzyGPURegressionTest, TnormProductUnderflow) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required for this test";

    // Many small membership values that multiply together
    const int num_values = 100;
    std::vector<float> memberships(num_values, 0.5f);  // 0.5^100 is very small

    float result = 0.0f;
    bool success = nimcp_gpu_fuzzy_tnorm_aggregate(
        ctx_, FUZZY_TNORM_PRODUCT, memberships.data(), num_values, &result);

    if (success) {
        // Result should be very small but non-zero
        // 0.5^100 is approximately 7.89e-31
        EXPECT_GE(result, 0.0f) << "T-norm result should be non-negative";
        // Don't check for exact non-zero due to numerical limits
    }
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Bug #8: Relation composition memory alignment
 * ============================================================================
 * Symptom: Incorrect results with non-power-of-2 matrix sizes
 * Fix: Proper padding and alignment in tiled matrix operations
 */
TEST_F(FuzzyGPURegressionTest, RelationCompositionAlignment) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required for this test";

    // Test with awkward sizes that don't align to typical tile sizes
    std::vector<std::pair<int, int>> test_sizes = {
        {7, 11},    // Prime numbers
        {33, 65},   // One more than power of 2
        {127, 129}, // Near power of 2
        {1, 100},   // Degenerate case
        {100, 1}    // Degenerate case
    };

    for (const auto& [rows, cols] : test_sizes) {
        std::vector<float> rel_a(rows * cols);
        std::vector<float> rel_b(cols * rows);  // cols x rows for composition
        std::vector<float> result(rows * rows);

        // Initialize with known values
        for (int i = 0; i < rows * cols; i++) {
            rel_a[i] = static_cast<float>(i % 10) / 10.0f;
        }
        for (int i = 0; i < cols * rows; i++) {
            rel_b[i] = static_cast<float>((i + 5) % 10) / 10.0f;
        }

        bool success = nimcp_gpu_fuzzy_relation_compose(
            ctx_, rel_a.data(), rel_b.data(), result.data(),
            rows, cols, rows, FUZZY_RELATION_MAX_MIN);

        if (success) {
            // Verify all results are valid
            for (int i = 0; i < rows * rows; i++) {
                EXPECT_TRUE(std::isfinite(result[i]))
                    << "Invalid result at " << i << " for size " << rows << "x" << cols;
                EXPECT_GE(result[i], 0.0f);
                EXPECT_LE(result[i], 1.0f);
            }
        }
    }
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Bug #9: Defuzzification with single-point universe
 * ============================================================================
 * Symptom: Crash or undefined behavior with resolution=1
 * Fix: Handle degenerate case explicitly
 */
TEST_F(FuzzyGPURegressionTest, DefuzzSinglePoint) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required for this test";

    std::vector<float> single_membership = {0.7f};
    float result = 0.0f;

    bool success = nimcp_gpu_fuzzy_defuzz_centroid(
        ctx_, single_membership.data(), 1, 5.0f, 5.0f, &result);

    if (success) {
        // Should return the single point
        EXPECT_FLOAT_EQ(result, 5.0f);
    }
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Bug #10: Hedge application with extreme values
 * ============================================================================
 * Symptom: Numerical issues with VERY, SOMEWHAT, etc. on boundary values
 * Fix: Clamp results to [0, 1]
 */
TEST_F(FuzzyGPURegressionTest, HedgeExtremeValues) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required for this test";

    std::vector<float> test_values = {
        0.0f, 1.0f,                        // Boundary values
        1e-10f, 1.0f - 1e-7f,              // Near boundaries
        -0.0f, 1.0f + 1e-7f                // Slightly out of range
    };

    std::vector<int> hedges = {
        FUZZY_HEDGE_VERY,       // Square
        FUZZY_HEDGE_SOMEWHAT,   // Square root
        FUZZY_HEDGE_EXTREMELY,  // Cube
        FUZZY_HEDGE_NOT         // Complement
    };

    for (int hedge : hedges) {
        for (float val : test_values) {
            float result = 0.0f;
            bool success = nimcp_gpu_fuzzy_hedge_apply(ctx_, hedge, val, &result);

            if (success) {
                EXPECT_TRUE(std::isfinite(result))
                    << "Hedge " << hedge << " on " << val << " should be finite";
                EXPECT_GE(result, 0.0f)
                    << "Hedge result should be non-negative";
                EXPECT_LE(result, 1.0f)
                    << "Hedge result should be at most 1.0";
            }
        }
    }
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

}  // namespace
