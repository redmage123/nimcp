/**
 * @file test_gabor_filters.cpp
 * @brief Unit tests for unified Gabor filter library
 *
 * WHAT: Comprehensive tests for Gabor filter operations.
 * WHY:  Ensure correct behavior before integrating with perception modules.
 * HOW:  GTest tests covering parameters, kernels, convolution, and utilities.
 *
 * @version 1.0.0
 * @date 2025-12-19
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <numeric>

// Headers have their own extern "C" guards
#include "utils/gabor/nimcp_gabor.h"

// ============================================================================
// Test Constants
// ============================================================================

namespace TestConstants {
    constexpr float TOLERANCE = 1e-5f;
    constexpr float RELAXED_TOLERANCE = 1e-3f;
    constexpr uint32_t DEFAULT_KERNEL_SIZE = 7;
    constexpr float DEFAULT_WAVELENGTH = 4.0f;
    constexpr uint32_t DEFAULT_NUM_ORIENTATIONS = 8;
}

// ============================================================================
// Test Fixtures
// ============================================================================

class GaborParameterTest : public ::testing::Test {
protected:
    gabor_filter_params_t params;

    void SetUp() override {
        gabor_default_params(&params);
    }
};

class GaborKernelTest : public ::testing::Test {
protected:
    gabor_filter_params_t params;
    gabor_kernel_t* kernel = nullptr;

    void SetUp() override {
        gabor_default_params(&params);
    }

    void TearDown() override {
        if (kernel) {
            gabor_kernel_destroy(kernel);
            kernel = nullptr;
        }
    }
};

class GaborFilterBankTest : public ::testing::Test {
protected:
    gabor_filter_bank_t* bank = nullptr;

    void TearDown() override {
        if (bank) {
            gabor_filter_bank_destroy(bank);
            bank = nullptr;
        }
    }
};

class GaborConvolutionTest : public ::testing::Test {
protected:
    gabor_kernel_t* kernel = nullptr;
    std::vector<float> image;

    void SetUp() override {
        // Create simple test image (32x32)
        image.resize(32 * 32, 0.5f);
    }

    void TearDown() override {
        if (kernel) {
            gabor_kernel_destroy(kernel);
            kernel = nullptr;
        }
    }

    void createVerticalEdge() {
        // Create vertical edge in center of image
        for (int y = 0; y < 32; y++) {
            for (int x = 0; x < 16; x++) {
                image[y * 32 + x] = 0.0f;
            }
            for (int x = 16; x < 32; x++) {
                image[y * 32 + x] = 1.0f;
            }
        }
    }

    void createHorizontalEdge() {
        // Create horizontal edge in center of image
        for (int y = 0; y < 16; y++) {
            for (int x = 0; x < 32; x++) {
                image[y * 32 + x] = 0.0f;
            }
        }
        for (int y = 16; y < 32; y++) {
            for (int x = 0; x < 32; x++) {
                image[y * 32 + x] = 1.0f;
            }
        }
    }

    void createSinusoidPattern(float frequency, float orientation_deg) {
        float theta = gabor_deg_to_rad(orientation_deg);
        for (int y = 0; y < 32; y++) {
            for (int x = 0; x < 32; x++) {
                float x_rot = (x - 16) * cosf(theta) + (y - 16) * sinf(theta);
                image[y * 32 + x] = 0.5f + 0.5f * sinf(2.0f * M_PI * frequency * x_rot);
            }
        }
    }
};

// ============================================================================
// Parameter Tests
// ============================================================================

TEST_F(GaborParameterTest, DefaultParamsValid) {
    gabor_default_params(&params);

    EXPECT_FLOAT_EQ(params.orientation_deg, 0.0f);
    EXPECT_FLOAT_EQ(params.wavelength, GABOR_DEFAULT_WAVELENGTH);
    EXPECT_FLOAT_EQ(params.phase_deg, 0.0f);
    EXPECT_FLOAT_EQ(params.aspect_ratio, GABOR_DEFAULT_ASPECT_RATIO);
    EXPECT_FLOAT_EQ(params.bandwidth, GABOR_DEFAULT_BANDWIDTH);
    EXPECT_FLOAT_EQ(params.sigma_x_override, 0.0f);
    EXPECT_FLOAT_EQ(params.sigma_y_override, 0.0f);
}

TEST_F(GaborParameterTest, ParamsForOrientation) {
    gabor_params_for_orientation(&params, 45.0f);

    EXPECT_FLOAT_EQ(params.orientation_deg, 45.0f);
    EXPECT_FLOAT_EQ(params.wavelength, GABOR_DEFAULT_WAVELENGTH);
}

TEST_F(GaborParameterTest, ParamsForOrientationNormalized) {
    gabor_params_for_orientation(&params, 200.0f);

    // 200 should normalize to 20 (200 - 180)
    EXPECT_FLOAT_EQ(params.orientation_deg, 20.0f);
}

TEST_F(GaborParameterTest, ParamsFromFrequency) {
    gabor_params_from_frequency(&params, 0.25f, 90.0f);

    // wavelength = 1 / frequency = 1 / 0.25 = 4.0
    EXPECT_FLOAT_EQ(params.wavelength, 4.0f);
    EXPECT_FLOAT_EQ(params.orientation_deg, 90.0f);
}

TEST_F(GaborParameterTest, ValidateParamsValid) {
    gabor_default_params(&params);
    EXPECT_TRUE(gabor_validate_params(&params));
}

TEST_F(GaborParameterTest, ValidateParamsNullFails) {
    EXPECT_FALSE(gabor_validate_params(nullptr));
}

TEST_F(GaborParameterTest, ValidateParamsInvalidWavelength) {
    params.wavelength = 0.0f;
    EXPECT_FALSE(gabor_validate_params(&params));
}

TEST_F(GaborParameterTest, ValidateParamsInvalidAspectRatio) {
    params.aspect_ratio = 0.0f;
    EXPECT_FALSE(gabor_validate_params(&params));
}

TEST_F(GaborParameterTest, ValidateParamsInvalidBandwidth) {
    params.bandwidth = 0.0f;
    EXPECT_FALSE(gabor_validate_params(&params));
}

TEST_F(GaborParameterTest, ComputeSigmasDefault) {
    float sigma_x, sigma_y;
    gabor_compute_sigmas(&params, &sigma_x, &sigma_y);

    // sigma_x = wavelength * bandwidth = 4.0 * 1.0 = 4.0
    EXPECT_FLOAT_EQ(sigma_x, 4.0f);
    // sigma_y = sigma_x / aspect_ratio = 4.0 / 0.5 = 8.0
    EXPECT_FLOAT_EQ(sigma_y, 8.0f);
}

TEST_F(GaborParameterTest, ComputeSigmasWithOverride) {
    params.sigma_x_override = 2.0f;
    params.sigma_y_override = 3.0f;

    float sigma_x, sigma_y;
    gabor_compute_sigmas(&params, &sigma_x, &sigma_y);

    EXPECT_FLOAT_EQ(sigma_x, 2.0f);
    EXPECT_FLOAT_EQ(sigma_y, 3.0f);
}

// ============================================================================
// Point Evaluation Tests
// ============================================================================

TEST_F(GaborParameterTest, EvaluateAtOrigin) {
    // At origin (0,0), Gabor should return Gaussian(0,0) * cos(phase)
    // For phase=0, this is 1.0 * 1.0 = 1.0
    float value = gabor_evaluate(0.0f, 0.0f, &params);
    EXPECT_NEAR(value, 1.0f, TestConstants::TOLERANCE);
}

TEST_F(GaborParameterTest, EvaluateEvenOddDifference) {
    float even = gabor_evaluate_even(1.0f, 0.0f, &params);
    float odd = gabor_evaluate_odd(1.0f, 0.0f, &params);

    // Even and odd should be 90 degrees out of phase
    // They shouldn't be equal at the same point (except specific cases)
    // The relationship is: even uses cos, odd uses sin
    EXPECT_NE(even, odd);
}

TEST_F(GaborParameterTest, EvaluateEnergyNonNegative) {
    // Energy should always be non-negative
    for (float x = -5.0f; x <= 5.0f; x += 1.0f) {
        for (float y = -5.0f; y <= 5.0f; y += 1.0f) {
            float energy = gabor_compute_energy(x, y, &params);
            EXPECT_GE(energy, 0.0f) << "Energy negative at (" << x << ", " << y << ")";
        }
    }
}

TEST_F(GaborParameterTest, EvaluateEnergyAtOrigin) {
    // At origin: even=1.0 (cos(0)=1), odd=0.0 (sin(0)=0)
    // Energy = sqrt(1^2 + 0^2) = 1.0
    float energy = gabor_compute_energy(0.0f, 0.0f, &params);
    EXPECT_NEAR(energy, 1.0f, TestConstants::TOLERANCE);
}

TEST_F(GaborParameterTest, EvaluateOrientationSelectivity) {
    // Vertical orientation (90 deg) should respond differently to x vs y offsets
    gabor_params_for_orientation(&params, 90.0f);

    float response_x = gabor_evaluate(3.0f, 0.0f, &params);
    float response_y = gabor_evaluate(0.0f, 3.0f, &params);

    // For 90 degree filter, the response along x (perpendicular) should differ from y
    EXPECT_NE(fabsf(response_x), fabsf(response_y));
}

// ============================================================================
// Kernel Creation Tests
// ============================================================================

TEST_F(GaborKernelTest, CreateKernelValid) {
    kernel = gabor_kernel_create(TestConstants::DEFAULT_KERNEL_SIZE, &params, true);
    ASSERT_NE(kernel, nullptr);
    EXPECT_EQ(kernel->size, TestConstants::DEFAULT_KERNEL_SIZE);
    EXPECT_NE(kernel->data, nullptr);
}

TEST_F(GaborKernelTest, CreateKernelNullParamsFails) {
    kernel = gabor_kernel_create(TestConstants::DEFAULT_KERNEL_SIZE, nullptr, true);
    EXPECT_EQ(kernel, nullptr);
}

TEST_F(GaborKernelTest, CreateKernelInvalidSizeFails) {
    // Even size should fail
    kernel = gabor_kernel_create(6, &params, true);
    EXPECT_EQ(kernel, nullptr);

    // Too small
    kernel = gabor_kernel_create(1, &params, true);
    EXPECT_EQ(kernel, nullptr);
}

TEST_F(GaborKernelTest, CreateKernelAutoSize) {
    kernel = gabor_kernel_create_auto_size(&params, true);
    ASSERT_NE(kernel, nullptr);
    EXPECT_GT(kernel->size, 0u);
    EXPECT_EQ(kernel->size % 2, 1u);  // Must be odd
}

TEST_F(GaborKernelTest, KernelDCBalanced) {
    kernel = gabor_kernel_create(TestConstants::DEFAULT_KERNEL_SIZE, &params, true);
    ASSERT_NE(kernel, nullptr);

    // Sum of DC-balanced kernel should be near zero
    float sum = 0.0f;
    uint32_t n = kernel->size * kernel->size;
    for (uint32_t i = 0; i < n; i++) {
        sum += kernel->data[i];
    }
    EXPECT_NEAR(sum, 0.0f, TestConstants::TOLERANCE);
}

TEST_F(GaborKernelTest, KernelNotDCBalanced) {
    kernel = gabor_kernel_create(TestConstants::DEFAULT_KERNEL_SIZE, &params, false);
    ASSERT_NE(kernel, nullptr);

    // Without DC balance, sum depends on the filter
    // Just verify we can create it
    EXPECT_NE(kernel->data, nullptr);
}

TEST_F(GaborKernelTest, KernelGetValid) {
    kernel = gabor_kernel_create(TestConstants::DEFAULT_KERNEL_SIZE, &params, true);
    ASSERT_NE(kernel, nullptr);

    // Get center value (should be near max for even kernel)
    uint32_t center = kernel->size / 2;
    float center_value = gabor_kernel_get(kernel, center, center);
    EXPECT_NE(center_value, 0.0f);
}

TEST_F(GaborKernelTest, KernelGetOutOfBounds) {
    kernel = gabor_kernel_create(TestConstants::DEFAULT_KERNEL_SIZE, &params, true);
    ASSERT_NE(kernel, nullptr);

    float value = gabor_kernel_get(kernel, 100, 100);
    EXPECT_FLOAT_EQ(value, 0.0f);
}

TEST_F(GaborKernelTest, KernelNormalize) {
    kernel = gabor_kernel_create(TestConstants::DEFAULT_KERNEL_SIZE, &params, false);
    ASSERT_NE(kernel, nullptr);

    bool result = gabor_kernel_normalize(kernel, 1.0f);
    EXPECT_TRUE(result);

    // Verify sum is now 1.0
    float sum = 0.0f;
    uint32_t n = kernel->size * kernel->size;
    for (uint32_t i = 0; i < n; i++) {
        sum += kernel->data[i];
    }
    EXPECT_NEAR(sum, 1.0f, TestConstants::TOLERANCE);
}

TEST_F(GaborKernelTest, CreateKernelDataLegacy) {
    float* data = gabor_create_kernel_data(7, &params);
    ASSERT_NE(data, nullptr);

    // Verify DC balanced
    float sum = 0.0f;
    for (int i = 0; i < 49; i++) {
        sum += data[i];
    }
    EXPECT_NEAR(sum, 0.0f, TestConstants::TOLERANCE);

    // Clean up using standard free (not nimcp_free in tests)
    free(data);
}

TEST_F(GaborKernelTest, KernelSymmetry) {
    // Even kernel (phase=0) should have certain symmetry properties
    params.phase_deg = 0.0f;
    kernel = gabor_kernel_create(9, &params, false);
    ASSERT_NE(kernel, nullptr);

    // For horizontal orientation (0 deg), even phase, there should be
    // mirror symmetry about the horizontal axis through center
    uint32_t center = kernel->size / 2;

    // Check top-left vs bottom-left reflection for even kernel
    // The filter has complex symmetry, just verify it's not random
    float top = gabor_kernel_get(kernel, 0, 0);
    float bottom = gabor_kernel_get(kernel, 0, kernel->size - 1);

    // They should be related (same or negated depending on filter structure)
    EXPECT_TRUE(fabsf(top - bottom) < 0.1f || fabsf(top + bottom) < 0.1f);
}

// ============================================================================
// Filter Bank Tests
// ============================================================================

TEST_F(GaborFilterBankTest, CreateBankValid) {
    bank = gabor_filter_bank_create(
        TestConstants::DEFAULT_NUM_ORIENTATIONS,
        TestConstants::DEFAULT_KERNEL_SIZE,
        TestConstants::DEFAULT_WAVELENGTH,
        false
    );
    ASSERT_NE(bank, nullptr);
    EXPECT_EQ(bank->num_orientations, TestConstants::DEFAULT_NUM_ORIENTATIONS);
    EXPECT_EQ(bank->total_kernels, TestConstants::DEFAULT_NUM_ORIENTATIONS);
}

TEST_F(GaborFilterBankTest, CreateBankWithQuadrature) {
    bank = gabor_filter_bank_create(
        TestConstants::DEFAULT_NUM_ORIENTATIONS,
        TestConstants::DEFAULT_KERNEL_SIZE,
        TestConstants::DEFAULT_WAVELENGTH,
        true
    );
    ASSERT_NE(bank, nullptr);
    EXPECT_EQ(bank->total_kernels, TestConstants::DEFAULT_NUM_ORIENTATIONS * 2);
    EXPECT_TRUE(bank->include_quadrature);
}

TEST_F(GaborFilterBankTest, CreateBankInvalidNumOrientations) {
    bank = gabor_filter_bank_create(0, 7, 4.0f, false);
    EXPECT_EQ(bank, nullptr);

    bank = gabor_filter_bank_create(100, 7, 4.0f, false);
    EXPECT_EQ(bank, nullptr);
}

TEST_F(GaborFilterBankTest, GetKernelValid) {
    bank = gabor_filter_bank_create(8, 7, 4.0f, false);
    ASSERT_NE(bank, nullptr);

    const gabor_kernel_t* k = gabor_filter_bank_get_kernel(bank, 0, false);
    ASSERT_NE(k, nullptr);
    EXPECT_NEAR(k->params.orientation_deg, 0.0f, TestConstants::TOLERANCE);

    k = gabor_filter_bank_get_kernel(bank, 4, false);
    ASSERT_NE(k, nullptr);
    // Orientation 4 of 8 = 4 * 22.5 = 90 degrees
    EXPECT_NEAR(k->params.orientation_deg, 90.0f, TestConstants::TOLERANCE);
}

TEST_F(GaborFilterBankTest, GetKernelQuadrature) {
    bank = gabor_filter_bank_create(8, 7, 4.0f, true);
    ASSERT_NE(bank, nullptr);

    const gabor_kernel_t* even = gabor_filter_bank_get_kernel(bank, 0, false);
    const gabor_kernel_t* odd = gabor_filter_bank_get_kernel(bank, 0, true);

    ASSERT_NE(even, nullptr);
    ASSERT_NE(odd, nullptr);
    EXPECT_NEAR(even->params.phase_deg, 0.0f, TestConstants::TOLERANCE);
    EXPECT_NEAR(odd->params.phase_deg, 90.0f, TestConstants::TOLERANCE);
}

TEST_F(GaborFilterBankTest, GetKernelInvalidIndex) {
    bank = gabor_filter_bank_create(8, 7, 4.0f, false);
    ASSERT_NE(bank, nullptr);

    const gabor_kernel_t* k = gabor_filter_bank_get_kernel(bank, 100, false);
    EXPECT_EQ(k, nullptr);
}

TEST_F(GaborFilterBankTest, OrientationCoverage) {
    bank = gabor_filter_bank_create(4, 7, 4.0f, false);
    ASSERT_NE(bank, nullptr);

    // 4 orientations should cover 0, 45, 90, 135 degrees
    const gabor_kernel_t* k0 = gabor_filter_bank_get_kernel(bank, 0, false);
    const gabor_kernel_t* k1 = gabor_filter_bank_get_kernel(bank, 1, false);
    const gabor_kernel_t* k2 = gabor_filter_bank_get_kernel(bank, 2, false);
    const gabor_kernel_t* k3 = gabor_filter_bank_get_kernel(bank, 3, false);

    EXPECT_NEAR(k0->params.orientation_deg, 0.0f, TestConstants::TOLERANCE);
    EXPECT_NEAR(k1->params.orientation_deg, 45.0f, TestConstants::TOLERANCE);
    EXPECT_NEAR(k2->params.orientation_deg, 90.0f, TestConstants::TOLERANCE);
    EXPECT_NEAR(k3->params.orientation_deg, 135.0f, TestConstants::TOLERANCE);
}

// ============================================================================
// Convolution Tests
// ============================================================================

TEST_F(GaborConvolutionTest, ConvolveUniform) {
    gabor_filter_params_t params;
    gabor_default_params(&params);

    kernel = gabor_kernel_create(7, &params, true);
    ASSERT_NE(kernel, nullptr);

    // Uniform image should give near-zero response for DC-balanced kernel
    float response = gabor_convolve(kernel, image.data(), 32, 32);
    EXPECT_NEAR(response, 0.0f, TestConstants::RELAXED_TOLERANCE);
}

TEST_F(GaborConvolutionTest, ConvolveVerticalEdge) {
    gabor_filter_params_t params;
    gabor_params_for_orientation(&params, 90.0f);  // Vertical filter

    kernel = gabor_kernel_create(7, &params, true);
    ASSERT_NE(kernel, nullptr);

    createVerticalEdge();

    // Vertical filter should respond to vertical edge
    float response = gabor_convolve(kernel, image.data(), 32, 32);
    EXPECT_NE(response, 0.0f);
}

TEST_F(GaborConvolutionTest, OrientationSelectivity) {
    createVerticalEdge();

    // 0 degree filter detects vertical edges (horizontal sinusoid pattern)
    gabor_filter_params_t params_0;
    gabor_params_for_orientation(&params_0, 0.0f);
    gabor_kernel_t* kernel_0 = gabor_kernel_create(7, &params_0, true);

    // 90 degree filter detects horizontal edges (vertical sinusoid pattern)
    gabor_filter_params_t params_90;
    gabor_params_for_orientation(&params_90, 90.0f);
    gabor_kernel_t* kernel_90 = gabor_kernel_create(7, &params_90, true);

    float response_0 = fabsf(gabor_convolve(kernel_0, image.data(), 32, 32));
    float response_90 = fabsf(gabor_convolve(kernel_90, image.data(), 32, 32));

    // 0 degree filter should respond more strongly to vertical edge
    // (Gabor orientation = sinusoid direction, detects perpendicular edges)
    EXPECT_GT(response_0, response_90);

    gabor_kernel_destroy(kernel_0);
    gabor_kernel_destroy(kernel_90);
}

TEST_F(GaborConvolutionTest, EnergyResponse) {
    createVerticalEdge();

    gabor_filter_params_t params;
    gabor_params_for_orientation(&params, 90.0f);

    float energy = gabor_energy_response(&params, image.data(), 32, 32, 7);
    EXPECT_GT(energy, 0.0f);
}

TEST_F(GaborConvolutionTest, FilterBankApply) {
    createVerticalEdge();

    gabor_filter_bank_t* bank = gabor_filter_bank_create(4, 7, 4.0f, false);
    ASSERT_NE(bank, nullptr);

    std::vector<float> responses(4);
    bool result = gabor_filter_bank_apply(bank, image.data(), 32, 32, responses.data());
    EXPECT_TRUE(result);

    // Index 0 is 0 degrees, which detects vertical edges (perpendicular)
    // Verify that responses vary by orientation
    float max_response = 0.0f;
    int max_idx = -1;
    for (int i = 0; i < 4; i++) {
        if (fabsf(responses[i]) > max_response) {
            max_response = fabsf(responses[i]);
            max_idx = i;
        }
    }
    // 0 degree filter should have strongest response to vertical edge
    EXPECT_EQ(max_idx, 0);

    gabor_filter_bank_destroy(bank);
}

TEST_F(GaborConvolutionTest, FilterBankApplyQuadrature) {
    createVerticalEdge();

    gabor_filter_bank_t* bank = gabor_filter_bank_create(4, 7, 4.0f, true);
    ASSERT_NE(bank, nullptr);

    std::vector<float> responses(8);  // 4 even + 4 odd
    bool result = gabor_filter_bank_apply(bank, image.data(), 32, 32, responses.data());
    EXPECT_TRUE(result);

    // Compute energy for each orientation
    std::vector<float> energy(4);
    for (int i = 0; i < 4; i++) {
        float even = responses[i];
        float odd = responses[4 + i];
        energy[i] = sqrtf(even * even + odd * odd);
    }

    // Orientation 0 (0 deg) should have highest energy for vertical edge
    float max_energy = *std::max_element(energy.begin(), energy.end());
    EXPECT_FLOAT_EQ(max_energy, energy[0]);

    gabor_filter_bank_destroy(bank);
}

// ============================================================================
// Utility Function Tests
// ============================================================================

TEST(GaborUtilityTest, DegToRad) {
    EXPECT_NEAR(gabor_deg_to_rad(0.0f), 0.0f, TestConstants::TOLERANCE);
    EXPECT_NEAR(gabor_deg_to_rad(90.0f), M_PI / 2.0, TestConstants::TOLERANCE);
    EXPECT_NEAR(gabor_deg_to_rad(180.0f), M_PI, TestConstants::TOLERANCE);
    EXPECT_NEAR(gabor_deg_to_rad(360.0f), 2.0 * M_PI, TestConstants::TOLERANCE);
}

TEST(GaborUtilityTest, RadToDeg) {
    EXPECT_NEAR(gabor_rad_to_deg(0.0f), 0.0f, TestConstants::TOLERANCE);
    EXPECT_NEAR(gabor_rad_to_deg(M_PI / 2.0f), 90.0f, TestConstants::TOLERANCE);
    EXPECT_NEAR(gabor_rad_to_deg(M_PI), 180.0f, TestConstants::TOLERANCE);
}

TEST(GaborUtilityTest, NormalizeOrientation) {
    EXPECT_FLOAT_EQ(gabor_normalize_orientation(0.0f), 0.0f);
    EXPECT_FLOAT_EQ(gabor_normalize_orientation(45.0f), 45.0f);
    EXPECT_FLOAT_EQ(gabor_normalize_orientation(180.0f), 0.0f);
    EXPECT_FLOAT_EQ(gabor_normalize_orientation(200.0f), 20.0f);
    EXPECT_FLOAT_EQ(gabor_normalize_orientation(-10.0f), 170.0f);
    EXPECT_FLOAT_EQ(gabor_normalize_orientation(360.0f), 0.0f);
}

TEST(GaborUtilityTest, AngularDifference) {
    EXPECT_NEAR(gabor_angular_difference(0.0f, 0.0f), 0.0f, TestConstants::TOLERANCE);
    EXPECT_NEAR(gabor_angular_difference(0.0f, 45.0f), 45.0f, TestConstants::TOLERANCE);
    EXPECT_NEAR(gabor_angular_difference(0.0f, 90.0f), 90.0f, TestConstants::TOLERANCE);
    EXPECT_NEAR(gabor_angular_difference(0.0f, 135.0f), 45.0f, TestConstants::TOLERANCE);
    EXPECT_NEAR(gabor_angular_difference(0.0f, 180.0f), 0.0f, TestConstants::TOLERANCE);
    EXPECT_NEAR(gabor_angular_difference(170.0f, 10.0f), 20.0f, TestConstants::TOLERANCE);
}

TEST(GaborUtilityTest, OptimalKernelSize) {
    gabor_filter_params_t params;
    gabor_default_params(&params);

    uint32_t size = gabor_optimal_kernel_size(&params);
    EXPECT_GE(size, GABOR_MIN_KERNEL_SIZE);
    EXPECT_LE(size, GABOR_MAX_KERNEL_SIZE);
    EXPECT_EQ(size % 2, 1u);  // Must be odd
}

TEST(GaborUtilityTest, StatsTracking) {
    gabor_reset_stats();

    gabor_stats_t stats;
    gabor_get_stats(&stats);
    EXPECT_EQ(stats.kernels_created, 0u);
    EXPECT_EQ(stats.convolutions, 0u);

    // Create a kernel
    gabor_filter_params_t params;
    gabor_default_params(&params);
    gabor_kernel_t* kernel = gabor_kernel_create(7, &params, true);
    ASSERT_NE(kernel, nullptr);

    gabor_get_stats(&stats);
    EXPECT_EQ(stats.kernels_created, 1u);

    // Do some convolutions
    std::vector<float> image(32 * 32, 0.5f);
    gabor_convolve(kernel, image.data(), 32, 32);
    gabor_convolve(kernel, image.data(), 32, 32);

    gabor_get_stats(&stats);
    EXPECT_EQ(stats.convolutions, 2u);

    gabor_kernel_destroy(kernel);
}

// ============================================================================
// Edge Cases and Error Handling Tests
// ============================================================================

TEST(GaborEdgeCaseTest, NullPointerHandling) {
    // These should not crash
    gabor_default_params(nullptr);
    gabor_params_for_orientation(nullptr, 45.0f);
    gabor_params_from_frequency(nullptr, 0.25f, 45.0f);

    float sigma_x, sigma_y;
    gabor_compute_sigmas(nullptr, &sigma_x, &sigma_y);
    gabor_compute_sigmas(nullptr, nullptr, nullptr);

    EXPECT_FLOAT_EQ(gabor_evaluate(0.0f, 0.0f, nullptr), 0.0f);
    EXPECT_FLOAT_EQ(gabor_compute_energy(0.0f, 0.0f, nullptr), 0.0f);

    EXPECT_EQ(gabor_kernel_create(7, nullptr, true), nullptr);
    EXPECT_EQ(gabor_kernel_create_auto_size(nullptr, true), nullptr);

    gabor_kernel_destroy(nullptr);  // Should not crash

    EXPECT_FLOAT_EQ(gabor_kernel_get(nullptr, 0, 0), 0.0f);
    EXPECT_FALSE(gabor_kernel_normalize(nullptr, 1.0f));

    EXPECT_FLOAT_EQ(gabor_convolve(nullptr, nullptr, 32, 32), 0.0f);
    EXPECT_FALSE(gabor_filter_bank_apply(nullptr, nullptr, 32, 32, nullptr));

    gabor_filter_bank_destroy(nullptr);  // Should not crash
}

TEST(GaborEdgeCaseTest, ZeroDimensions) {
    gabor_filter_params_t params;
    gabor_default_params(&params);
    gabor_kernel_t* kernel = gabor_kernel_create(7, &params, true);
    ASSERT_NE(kernel, nullptr);

    std::vector<float> image(32 * 32, 0.5f);

    // Zero width/height should return 0
    EXPECT_FLOAT_EQ(gabor_convolve(kernel, image.data(), 0, 32), 0.0f);
    EXPECT_FLOAT_EQ(gabor_convolve(kernel, image.data(), 32, 0), 0.0f);

    gabor_kernel_destroy(kernel);
}

TEST(GaborEdgeCaseTest, ExtremeSigmas) {
    gabor_filter_params_t params;
    gabor_default_params(&params);

    // Very large sigma (should be clamped by kernel size)
    params.sigma_x_override = 100.0f;
    params.sigma_y_override = 100.0f;

    gabor_kernel_t* kernel = gabor_kernel_create_auto_size(&params, true);
    ASSERT_NE(kernel, nullptr);
    EXPECT_LE(kernel->size, GABOR_MAX_KERNEL_SIZE);

    gabor_kernel_destroy(kernel);
}
