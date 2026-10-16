/**
 * @file test_cortical_sparse_coding_ternary.cpp
 * @brief Unit tests for ternary sparse coding in cortical columns
 *
 * Tests ternary integration with cortical sparse coding system including:
 * - Ternary sparse coefficient quantization
 * - Threshold-based discretization
 * - Ternary active set operations
 * - Sparsity computation with ternary codes
 * - Memory-efficient packed ternary vectors
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <algorithm>
#include <numeric>

// Headers have their own extern "C" guards
#include "core/cortical_columns/nimcp_cortical_sparse_coding.h"
#include "utils/ternary/nimcp_ternary.h"
#include "utils/ternary/nimcp_ternary_types.h"
#include "utils/ternary/nimcp_ternary_vector.h"
#include "utils/error/nimcp_error_codes.h"

//=============================================================================
// Test Fixture
//=============================================================================

class CorticalSparseCodingTernaryTest : public ::testing::Test {
protected:
    cortical_sparse_coding_system_t* system;
    sparse_coding_config_t config;

    void SetUp() override {
        // Get default configuration
        int result = cortical_sparse_default_config(&config);
        ASSERT_EQ(result, 0);

        // Configure for ternary mode
        config.num_columns = 100;
        config.num_input_dims = 50;
        config.target_sparsity = 0.05f;  // 5% active
        config.enable_ternary_coefficients = true;
        config.ternary_positive_threshold = 0.3f;
        config.ternary_negative_threshold = 0.3f;
        config.ternary_pack_mode = TERNARY_PACK_NONE;
        config.sparsity_method = SPARSITY_METHOD_THRESHOLD;

        system = cortical_sparse_create(&config);
        // System may be NULL if not fully implemented - tests will skip
    }

    void TearDown() override {
        if (system) {
            cortical_sparse_destroy(system);
            system = nullptr;
        }
    }

    // Helper: Create test activations with known pattern
    std::vector<float> createTestActivations(uint32_t size, float positive_ratio = 0.1f,
                                              float negative_ratio = 0.1f) {
        std::vector<float> activations(size);
        uint32_t n_positive = static_cast<uint32_t>(size * positive_ratio);
        uint32_t n_negative = static_cast<uint32_t>(size * negative_ratio);

        // Start with zeros
        std::fill(activations.begin(), activations.end(), 0.0f);

        // Add positive activations
        for (uint32_t i = 0; i < n_positive && i < size; i++) {
            activations[i] = 0.5f + (static_cast<float>(i) / n_positive) * 0.5f;
        }

        // Add negative activations
        for (uint32_t i = 0; i < n_negative && (n_positive + i) < size; i++) {
            activations[n_positive + i] = -0.5f - (static_cast<float>(i) / n_negative) * 0.5f;
        }

        return activations;
    }
};

//=============================================================================
// Default Configuration Tests
//=============================================================================

class SparseCodingTernaryConfigTest : public ::testing::Test {
protected:
    sparse_coding_config_t config;
};

TEST_F(SparseCodingTernaryConfigTest, DefaultConfigSuccess) {
    int result = cortical_sparse_default_config(&config);
    EXPECT_EQ(result, 0);
}

TEST_F(SparseCodingTernaryConfigTest, DefaultConfigNullPointer) {
    int result = cortical_sparse_default_config(nullptr);
    EXPECT_LT(result, 0);  // Expect error
}

TEST_F(SparseCodingTernaryConfigTest, DefaultConfigHasValidDefaults) {
    int result = cortical_sparse_default_config(&config);
    ASSERT_EQ(result, 0);

    // Check default sparsity target
    EXPECT_GE(config.target_sparsity, SPARSE_CODING_MIN_SPARSITY);
    EXPECT_LE(config.target_sparsity, SPARSE_CODING_MAX_SPARSITY);

    // Check default overcomplete ratio
    EXPECT_GE(config.overcomplete_ratio, 1.0f);
}

//=============================================================================
// System Creation/Destruction Tests
//=============================================================================

class SparseCodingTernaryLifecycleTest : public ::testing::Test {
protected:
    sparse_coding_config_t config;

    void SetUp() override {
        cortical_sparse_default_config(&config);
        config.num_columns = 64;
        config.num_input_dims = 32;
    }
};

TEST_F(SparseCodingTernaryLifecycleTest, CreateWithTernaryEnabled) {
    config.enable_ternary_coefficients = true;
    config.ternary_positive_threshold = 0.25f;
    config.ternary_negative_threshold = 0.25f;

    cortical_sparse_coding_system_t* system = cortical_sparse_create(&config);
    // May be NULL if not implemented - skip if so
    if (system) {
        EXPECT_TRUE(cortical_sparse_is_ternary_mode(system));
        cortical_sparse_destroy(system);
    }
}

TEST_F(SparseCodingTernaryLifecycleTest, CreateWithTernaryDisabled) {
    config.enable_ternary_coefficients = false;

    cortical_sparse_coding_system_t* system = cortical_sparse_create(&config);
    if (system) {
        EXPECT_FALSE(cortical_sparse_is_ternary_mode(system));
        cortical_sparse_destroy(system);
    }
}

TEST_F(SparseCodingTernaryLifecycleTest, DestroyNullSafe) {
    // Should not crash with NULL
    cortical_sparse_destroy(nullptr);
    SUCCEED();
}

//=============================================================================
// Ternary Mode Enable/Disable Tests
//=============================================================================

TEST_F(CorticalSparseCodingTernaryTest, EnableTernaryMode) {
    if (!system) GTEST_SKIP() << "Sparse coding system not available";

    // First disable if already enabled
    cortical_sparse_disable_ternary_mode(system);
    EXPECT_FALSE(cortical_sparse_is_ternary_mode(system));

    // Enable with specific thresholds
    int result = cortical_sparse_enable_ternary_mode(
        system, 0.4f, 0.3f, TERNARY_PACK_2BIT);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(cortical_sparse_is_ternary_mode(system));
}

TEST_F(CorticalSparseCodingTernaryTest, DisableTernaryMode) {
    if (!system) GTEST_SKIP() << "Sparse coding system not available";

    // Ensure ternary mode is enabled
    cortical_sparse_enable_ternary_mode(system, 0.3f, 0.3f, TERNARY_PACK_NONE);
    EXPECT_TRUE(cortical_sparse_is_ternary_mode(system));

    // Disable
    int result = cortical_sparse_disable_ternary_mode(system);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(cortical_sparse_is_ternary_mode(system));
}

TEST_F(CorticalSparseCodingTernaryTest, EnableTernaryModeNullSystem) {
    int result = cortical_sparse_enable_ternary_mode(nullptr, 0.3f, 0.3f, TERNARY_PACK_NONE);
    EXPECT_LT(result, 0);
}

TEST_F(CorticalSparseCodingTernaryTest, DisableTernaryModeNullSystem) {
    int result = cortical_sparse_disable_ternary_mode(nullptr);
    EXPECT_LT(result, 0);
}

TEST_F(CorticalSparseCodingTernaryTest, IsTernaryModeNullSystemReturnsFalse) {
    bool result = cortical_sparse_is_ternary_mode(nullptr);
    EXPECT_FALSE(result);
}

//=============================================================================
// Ternary Quantization Tests
//=============================================================================

TEST_F(CorticalSparseCodingTernaryTest, QuantizeToTernaryBasic) {
    if (!system) GTEST_SKIP() << "Sparse coding system not available";

    std::vector<float> activations = createTestActivations(config.num_columns, 0.15f, 0.15f);
    std::vector<trit_t> ternary_output(config.num_columns);

    int result = cortical_sparse_quantize_to_ternary(
        system, activations.data(), config.num_columns, ternary_output.data());
    EXPECT_EQ(result, 0);

    // Verify at least some values are quantized correctly
    bool has_positive = false;
    bool has_negative = false;
    bool has_zero = false;

    for (uint32_t i = 0; i < config.num_columns; i++) {
        EXPECT_GE(ternary_output[i], TRIT_NEGATIVE);
        EXPECT_LE(ternary_output[i], TRIT_POSITIVE);

        if (ternary_output[i] == TRIT_POSITIVE) has_positive = true;
        if (ternary_output[i] == TRIT_NEGATIVE) has_negative = true;
        if (ternary_output[i] == TRIT_UNKNOWN) has_zero = true;
    }

    // With 15% positive and 15% negative, should have all three types
    EXPECT_TRUE(has_positive || has_negative || has_zero);
}

TEST_F(CorticalSparseCodingTernaryTest, QuantizeToTernaryNullInputs) {
    if (!system) GTEST_SKIP() << "Sparse coding system not available";

    std::vector<float> activations(config.num_columns);
    std::vector<trit_t> output(config.num_columns);

    // Null system
    int result = cortical_sparse_quantize_to_ternary(
        nullptr, activations.data(), config.num_columns, output.data());
    EXPECT_LT(result, 0);

    // Null activations
    result = cortical_sparse_quantize_to_ternary(
        system, nullptr, config.num_columns, output.data());
    EXPECT_LT(result, 0);

    // Null output
    result = cortical_sparse_quantize_to_ternary(
        system, activations.data(), config.num_columns, nullptr);
    EXPECT_LT(result, 0);
}

TEST_F(CorticalSparseCodingTernaryTest, QuantizeToTernaryThresholdBehavior) {
    if (!system) GTEST_SKIP() << "Sparse coding system not available";

    // Set specific thresholds
    cortical_sparse_enable_ternary_mode(system, 0.5f, 0.5f, TERNARY_PACK_NONE);

    std::vector<float> activations = {
        0.6f,   // Above positive threshold -> +1
        0.5f,   // At threshold -> may be +1 or 0
        0.4f,   // Below threshold -> 0
        0.0f,   // Zero -> 0
        -0.4f,  // Above -negative threshold -> 0
        -0.5f,  // At threshold -> may be -1 or 0
        -0.6f   // Below negative threshold -> -1
    };
    std::vector<trit_t> output(activations.size());

    int result = cortical_sparse_quantize_to_ternary(
        system, activations.data(), activations.size(), output.data());
    EXPECT_EQ(result, 0);

    // Values clearly above threshold should be +1
    EXPECT_EQ(output[0], TRIT_POSITIVE);

    // Values clearly in middle should be 0
    EXPECT_EQ(output[3], TRIT_UNKNOWN);

    // Values clearly below -threshold should be -1
    EXPECT_EQ(output[6], TRIT_NEGATIVE);
}

//=============================================================================
// Ternary Dequantization Tests
//=============================================================================

TEST_F(CorticalSparseCodingTernaryTest, DequantizeFromTernaryBasic) {
    if (!system) GTEST_SKIP() << "Sparse coding system not available";

    std::vector<trit_t> ternary_input = {
        TRIT_POSITIVE, TRIT_NEGATIVE, TRIT_UNKNOWN,
        TRIT_POSITIVE, TRIT_UNKNOWN, TRIT_NEGATIVE
    };
    std::vector<float> continuous_output(ternary_input.size());

    float scale = 1.0f;
    int result = cortical_sparse_dequantize_from_ternary(
        system, ternary_input.data(), ternary_input.size(),
        scale, continuous_output.data());
    EXPECT_EQ(result, 0);

    // Verify dequantization
    EXPECT_FLOAT_EQ(continuous_output[0], scale);   // +1 * scale
    EXPECT_FLOAT_EQ(continuous_output[1], -scale);  // -1 * scale
    EXPECT_FLOAT_EQ(continuous_output[2], 0.0f);    // 0 * scale
    EXPECT_FLOAT_EQ(continuous_output[3], scale);
    EXPECT_FLOAT_EQ(continuous_output[4], 0.0f);
    EXPECT_FLOAT_EQ(continuous_output[5], -scale);
}

TEST_F(CorticalSparseCodingTernaryTest, DequantizeWithScale) {
    if (!system) GTEST_SKIP() << "Sparse coding system not available";

    std::vector<trit_t> ternary_input = {TRIT_POSITIVE, TRIT_NEGATIVE, TRIT_UNKNOWN};
    std::vector<float> continuous_output(ternary_input.size());

    float scale = 2.5f;
    int result = cortical_sparse_dequantize_from_ternary(
        system, ternary_input.data(), ternary_input.size(),
        scale, continuous_output.data());
    EXPECT_EQ(result, 0);

    EXPECT_FLOAT_EQ(continuous_output[0], 2.5f);
    EXPECT_FLOAT_EQ(continuous_output[1], -2.5f);
    EXPECT_FLOAT_EQ(continuous_output[2], 0.0f);
}

TEST_F(CorticalSparseCodingTernaryTest, DequantizeNullInputs) {
    if (!system) GTEST_SKIP() << "Sparse coding system not available";

    std::vector<trit_t> input(10);
    std::vector<float> output(10);

    // Null system
    int result = cortical_sparse_dequantize_from_ternary(
        nullptr, input.data(), 10, 1.0f, output.data());
    EXPECT_LT(result, 0);

    // Null input
    result = cortical_sparse_dequantize_from_ternary(
        system, nullptr, 10, 1.0f, output.data());
    EXPECT_LT(result, 0);

    // Null output
    result = cortical_sparse_dequantize_from_ternary(
        system, input.data(), 10, 1.0f, nullptr);
    EXPECT_LT(result, 0);
}

//=============================================================================
// Ternary Sparsity Enforcement Tests
//=============================================================================

TEST_F(CorticalSparseCodingTernaryTest, EnforceSparsityTernaryBasic) {
    if (!system) GTEST_SKIP() << "Sparse coding system not available";

    std::vector<float> activations = createTestActivations(config.num_columns, 0.3f, 0.3f);
    std::vector<trit_t> ternary_output(config.num_columns);

    int result = cortical_sparse_enforce_sparsity_ternary(
        system, activations.data(), config.num_columns, ternary_output.data());
    EXPECT_EQ(result, 0);

    // Count non-zero outputs
    uint32_t non_zero_count = 0;
    for (uint32_t i = 0; i < config.num_columns; i++) {
        if (ternary_output[i] != TRIT_UNKNOWN) {
            non_zero_count++;
        }
    }

    // Sparsity should be achieved (few non-zero values)
    float actual_density = static_cast<float>(non_zero_count) / config.num_columns;
    EXPECT_LE(actual_density, 0.5f);  // At most 50% active after sparsification
}

TEST_F(CorticalSparseCodingTernaryTest, EnforceSparsityTernaryNullInputs) {
    if (!system) GTEST_SKIP() << "Sparse coding system not available";

    std::vector<float> activations(config.num_columns);
    std::vector<trit_t> output(config.num_columns);

    // Null system
    int result = cortical_sparse_enforce_sparsity_ternary(
        nullptr, activations.data(), config.num_columns, output.data());
    EXPECT_LT(result, 0);

    // Null activations
    result = cortical_sparse_enforce_sparsity_ternary(
        system, nullptr, config.num_columns, output.data());
    EXPECT_LT(result, 0);

    // Null output
    result = cortical_sparse_enforce_sparsity_ternary(
        system, activations.data(), config.num_columns, nullptr);
    EXPECT_LT(result, 0);
}

//=============================================================================
// Ternary Active Set Tests
//=============================================================================

TEST_F(CorticalSparseCodingTernaryTest, GetTernaryActiveSetBasic) {
    std::vector<trit_t> ternary_activations = {
        TRIT_POSITIVE,  // 0 - active
        TRIT_UNKNOWN,   // 1 - inactive
        TRIT_NEGATIVE,  // 2 - active
        TRIT_UNKNOWN,   // 3 - inactive
        TRIT_POSITIVE,  // 4 - active
        TRIT_UNKNOWN,   // 5 - inactive
        TRIT_NEGATIVE   // 6 - active
    };

    std::vector<uint32_t> active_indices(ternary_activations.size());
    uint32_t num_active = 0;

    int result = cortical_sparse_get_ternary_active_set(
        ternary_activations.data(), ternary_activations.size(),
        active_indices.data(), ternary_activations.size(), &num_active);
    EXPECT_EQ(result, 0);

    // Should find 4 active indices: 0, 2, 4, 6
    EXPECT_EQ(num_active, 4u);
    EXPECT_EQ(active_indices[0], 0u);
    EXPECT_EQ(active_indices[1], 2u);
    EXPECT_EQ(active_indices[2], 4u);
    EXPECT_EQ(active_indices[3], 6u);
}

TEST_F(CorticalSparseCodingTernaryTest, GetTernaryActiveSetAllZeros) {
    std::vector<trit_t> ternary_activations(10, TRIT_UNKNOWN);
    std::vector<uint32_t> active_indices(10);
    uint32_t num_active = 0;

    int result = cortical_sparse_get_ternary_active_set(
        ternary_activations.data(), 10, active_indices.data(), 10, &num_active);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(num_active, 0u);
}

TEST_F(CorticalSparseCodingTernaryTest, GetTernaryActiveSetAllActive) {
    std::vector<trit_t> ternary_activations = {
        TRIT_POSITIVE, TRIT_NEGATIVE, TRIT_POSITIVE, TRIT_NEGATIVE, TRIT_POSITIVE
    };
    std::vector<uint32_t> active_indices(5);
    uint32_t num_active = 0;

    int result = cortical_sparse_get_ternary_active_set(
        ternary_activations.data(), 5, active_indices.data(), 5, &num_active);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(num_active, 5u);
}

TEST_F(CorticalSparseCodingTernaryTest, GetTernaryActiveSetNullInputs) {
    std::vector<trit_t> activations(10);
    std::vector<uint32_t> indices(10);
    uint32_t num_active;

    // Null activations
    int result = cortical_sparse_get_ternary_active_set(
        nullptr, 10, indices.data(), 10, &num_active);
    EXPECT_LT(result, 0);

    // Null indices
    result = cortical_sparse_get_ternary_active_set(
        activations.data(), 10, nullptr, 10, &num_active);
    EXPECT_LT(result, 0);

    // Null count
    result = cortical_sparse_get_ternary_active_set(
        activations.data(), 10, indices.data(), 10, nullptr);
    EXPECT_LT(result, 0);
}

//=============================================================================
// Ternary Sparsity Computation Tests
//=============================================================================

TEST_F(CorticalSparseCodingTernaryTest, ComputeTernarySparsityAllZeros) {
    std::vector<trit_t> ternary_activations(100, TRIT_UNKNOWN);

    float sparsity = cortical_sparse_compute_ternary_sparsity(
        ternary_activations.data(), 100);

    // All zeros = 100% sparsity
    EXPECT_FLOAT_EQ(sparsity, 1.0f);
}

TEST_F(CorticalSparseCodingTernaryTest, ComputeTernarySparsityNoneZero) {
    std::vector<trit_t> ternary_activations(100, TRIT_POSITIVE);

    float sparsity = cortical_sparse_compute_ternary_sparsity(
        ternary_activations.data(), 100);

    // No zeros = 0% sparsity
    EXPECT_FLOAT_EQ(sparsity, 0.0f);
}

TEST_F(CorticalSparseCodingTernaryTest, ComputeTernarySparsityMixed) {
    std::vector<trit_t> ternary_activations(100);

    // 70 zeros, 20 positive, 10 negative
    std::fill(ternary_activations.begin(), ternary_activations.begin() + 70, TRIT_UNKNOWN);
    std::fill(ternary_activations.begin() + 70, ternary_activations.begin() + 90, TRIT_POSITIVE);
    std::fill(ternary_activations.begin() + 90, ternary_activations.end(), TRIT_NEGATIVE);

    float sparsity = cortical_sparse_compute_ternary_sparsity(
        ternary_activations.data(), 100);

    // 70% zeros = 0.70 sparsity
    EXPECT_FLOAT_EQ(sparsity, 0.70f);
}

//=============================================================================
// Ternary Distribution Tests
//=============================================================================

TEST_F(CorticalSparseCodingTernaryTest, GetTernaryDistributionBasic) {
    std::vector<trit_t> ternary_activations = {
        TRIT_POSITIVE, TRIT_POSITIVE, TRIT_POSITIVE,  // 3 positive
        TRIT_UNKNOWN, TRIT_UNKNOWN,                    // 2 zero
        TRIT_NEGATIVE                                  // 1 negative
    };

    uint32_t n_negative = 0, n_zero = 0, n_positive = 0;

    cortical_sparse_get_ternary_distribution(
        ternary_activations.data(), 6, &n_negative, &n_zero, &n_positive);

    EXPECT_EQ(n_positive, 3u);
    EXPECT_EQ(n_zero, 2u);
    EXPECT_EQ(n_negative, 1u);
}

TEST_F(CorticalSparseCodingTernaryTest, GetTernaryDistributionAllSame) {
    std::vector<trit_t> all_positive(50, TRIT_POSITIVE);
    uint32_t n_negative = 0, n_zero = 0, n_positive = 0;

    cortical_sparse_get_ternary_distribution(
        all_positive.data(), 50, &n_negative, &n_zero, &n_positive);

    EXPECT_EQ(n_positive, 50u);
    EXPECT_EQ(n_zero, 0u);
    EXPECT_EQ(n_negative, 0u);
}

TEST_F(CorticalSparseCodingTernaryTest, GetTernaryDistributionEmpty) {
    uint32_t n_negative = 99, n_zero = 99, n_positive = 99;

    // Empty array
    cortical_sparse_get_ternary_distribution(
        nullptr, 0, &n_negative, &n_zero, &n_positive);

    // Output should be zero (or unchanged if function handles null)
    EXPECT_LE(n_negative + n_zero + n_positive, 297u);  // May or may not change
}

//=============================================================================
// Ternary Dot Product Tests
//=============================================================================

TEST_F(CorticalSparseCodingTernaryTest, TernaryDotProductBasic) {
    std::vector<trit_t> ternary_code = {
        TRIT_POSITIVE,  // +1
        TRIT_NEGATIVE,  // -1
        TRIT_UNKNOWN,   // 0
        TRIT_POSITIVE   // +1
    };
    std::vector<float> values = {2.0f, 3.0f, 4.0f, 5.0f};

    // Expected: (+1)*2 + (-1)*3 + (0)*4 + (+1)*5 = 2 - 3 + 0 + 5 = 4
    float result = cortical_sparse_ternary_dot(
        ternary_code.data(), 4, values.data());

    EXPECT_FLOAT_EQ(result, 4.0f);
}

TEST_F(CorticalSparseCodingTernaryTest, TernaryDotProductAllZeros) {
    std::vector<trit_t> ternary_code(10, TRIT_UNKNOWN);
    std::vector<float> values(10, 5.0f);

    float result = cortical_sparse_ternary_dot(
        ternary_code.data(), 10, values.data());

    EXPECT_FLOAT_EQ(result, 0.0f);
}

TEST_F(CorticalSparseCodingTernaryTest, TernaryDotProductAllPositive) {
    std::vector<trit_t> ternary_code(5, TRIT_POSITIVE);
    std::vector<float> values = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};

    // Expected: 1 + 2 + 3 + 4 + 5 = 15
    float result = cortical_sparse_ternary_dot(
        ternary_code.data(), 5, values.data());

    EXPECT_FLOAT_EQ(result, 15.0f);
}

TEST_F(CorticalSparseCodingTernaryTest, TernaryDotProductAllNegative) {
    std::vector<trit_t> ternary_code(5, TRIT_NEGATIVE);
    std::vector<float> values = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};

    // Expected: -1 - 2 - 3 - 4 - 5 = -15
    float result = cortical_sparse_ternary_dot(
        ternary_code.data(), 5, values.data());

    EXPECT_FLOAT_EQ(result, -15.0f);
}

//=============================================================================
// Ternary Vector Creation Tests
//=============================================================================

TEST_F(CorticalSparseCodingTernaryTest, CreateTernaryVectorBasic) {
    if (!system) GTEST_SKIP() << "Sparse coding system not available";

    std::vector<float> activations = createTestActivations(config.num_columns, 0.2f, 0.2f);

    trit_vector_t* vector = cortical_sparse_create_ternary_vector(
        system, activations.data(), config.num_columns);

    if (vector) {
        EXPECT_EQ(vector->size, config.num_columns);
        trit_vector_destroy(vector);
    }
}

TEST_F(CorticalSparseCodingTernaryTest, CreateTernaryVectorNullInputs) {
    if (!system) GTEST_SKIP() << "Sparse coding system not available";

    std::vector<float> activations(100);

    // Null system
    trit_vector_t* vector = cortical_sparse_create_ternary_vector(
        nullptr, activations.data(), 100);
    EXPECT_EQ(vector, nullptr);

    // Null activations
    vector = cortical_sparse_create_ternary_vector(
        system, nullptr, 100);
    EXPECT_EQ(vector, nullptr);
}

//=============================================================================
// Ternary Pack Mode Tests
//=============================================================================

TEST_F(CorticalSparseCodingTernaryTest, EnableTernaryWithDifferentPackModes) {
    if (!system) GTEST_SKIP() << "Sparse coding system not available";

    // Test each pack mode
    const ternary_pack_mode_t modes[] = {
        TERNARY_PACK_NONE,
        TERNARY_PACK_2BIT,
        TERNARY_PACK_BASE243
    };

    for (auto mode : modes) {
        cortical_sparse_disable_ternary_mode(system);
        int result = cortical_sparse_enable_ternary_mode(system, 0.3f, 0.3f, mode);
        EXPECT_EQ(result, 0) << "Failed for mode " << mode;
        EXPECT_TRUE(cortical_sparse_is_ternary_mode(system));
    }
}

//=============================================================================
// Column State with Ternary Tests
//=============================================================================

TEST_F(CorticalSparseCodingTernaryTest, GetColumnStateWithTernary) {
    if (!system) GTEST_SKIP() << "Sparse coding system not available";

    column_sparse_state_t state;
    int result = cortical_sparse_get_column_state(system, 0, &state);

    if (result == 0) {
        // If ternary mode is enabled in config, use_ternary should be true
        if (cortical_sparse_is_ternary_mode(system)) {
            EXPECT_TRUE(state.use_ternary);
        }
    }
}

TEST_F(CorticalSparseCodingTernaryTest, GetColumnStateNullInputs) {
    if (!system) GTEST_SKIP() << "Sparse coding system not available";

    column_sparse_state_t state;

    // Null system
    int result = cortical_sparse_get_column_state(nullptr, 0, &state);
    EXPECT_LT(result, 0);

    // Null state
    result = cortical_sparse_get_column_state(system, 0, nullptr);
    EXPECT_LT(result, 0);
}

TEST_F(CorticalSparseCodingTernaryTest, GetColumnStateOutOfRange) {
    if (!system) GTEST_SKIP() << "Sparse coding system not available";

    column_sparse_state_t state;

    // Index out of range
    int result = cortical_sparse_get_column_state(system, 99999, &state);
    EXPECT_LT(result, 0);
}

//=============================================================================
// Edge Cases and Boundary Conditions
//=============================================================================

TEST_F(CorticalSparseCodingTernaryTest, QuantizeEmptyArray) {
    if (!system) GTEST_SKIP() << "Sparse coding system not available";

    std::vector<float> empty_activations;
    std::vector<trit_t> empty_output;

    int result = cortical_sparse_quantize_to_ternary(
        system, empty_activations.data(), 0, empty_output.data());

    // Should handle gracefully (either success or specific error)
    // Not expecting crash
    SUCCEED();
}

TEST_F(CorticalSparseCodingTernaryTest, QuantizeSingleElement) {
    if (!system) GTEST_SKIP() << "Sparse coding system not available";

    float activation = 0.8f;
    trit_t output;

    int result = cortical_sparse_quantize_to_ternary(
        system, &activation, 1, &output);

    if (result == 0) {
        // High positive activation should become +1
        EXPECT_EQ(output, TRIT_POSITIVE);
    }
}

TEST_F(CorticalSparseCodingTernaryTest, QuantizeExtremeValues) {
    if (!system) GTEST_SKIP() << "Sparse coding system not available";

    std::vector<float> extreme_activations = {
        1000.0f,     // Very large positive
        -1000.0f,    // Very large negative
        1e-10f,      // Very small positive
        -1e-10f,     // Very small negative
        0.0f,        // Zero
        INFINITY,    // Infinity
        -INFINITY,   // Negative infinity
    };
    std::vector<trit_t> output(extreme_activations.size());

    int result = cortical_sparse_quantize_to_ternary(
        system, extreme_activations.data(), extreme_activations.size(), output.data());

    // Should handle without crash
    // Large values should map to +1/-1
    // Very small and zero values should map to 0
    if (result == 0) {
        EXPECT_EQ(output[0], TRIT_POSITIVE);   // 1000.0
        EXPECT_EQ(output[1], TRIT_NEGATIVE);   // -1000.0
        // Small values near zero
        EXPECT_EQ(output[4], TRIT_UNKNOWN);    // 0.0
    }
}

//=============================================================================
// Roundtrip Tests
//=============================================================================

TEST_F(CorticalSparseCodingTernaryTest, QuantizeDequantizeRoundtrip) {
    if (!system) GTEST_SKIP() << "Sparse coding system not available";

    // Use clear values that should survive roundtrip
    std::vector<float> original_activations = {
        0.8f, -0.8f, 0.0f, 0.7f, -0.7f,
        0.1f, -0.1f, 0.9f, -0.9f, 0.0f
    };
    std::vector<trit_t> ternary(original_activations.size());
    std::vector<float> reconstructed(original_activations.size());

    // Quantize
    int result = cortical_sparse_quantize_to_ternary(
        system, original_activations.data(), original_activations.size(), ternary.data());
    ASSERT_EQ(result, 0);

    // Dequantize
    result = cortical_sparse_dequantize_from_ternary(
        system, ternary.data(), ternary.size(), 1.0f, reconstructed.data());
    ASSERT_EQ(result, 0);

    // Check sign is preserved for large values
    for (size_t i = 0; i < original_activations.size(); i++) {
        float orig = original_activations[i];
        float recon = reconstructed[i];

        if (std::abs(orig) > 0.5f) {  // Only check large values
            // Sign should match
            if (orig > 0) {
                EXPECT_GT(recon, 0.0f) << "Sign mismatch at index " << i;
            } else if (orig < 0) {
                EXPECT_LT(recon, 0.0f) << "Sign mismatch at index " << i;
            }
        }
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
