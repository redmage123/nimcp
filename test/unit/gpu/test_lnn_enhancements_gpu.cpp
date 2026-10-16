/**
 * @file test_lnn_enhancements_gpu.cpp
 * @brief Unit tests for GPU LNN enhancements
 *
 * WHAT: Tests for gradient accumulation, CSR weights, and spectral radius
 * WHY:  Verify correctness of new LNN GPU features
 * HOW:  GoogleTest with GPU context setup/teardown
 *
 * TEST COVERAGE:
 * - Gradient DAO: accumulation, clipping, normalization, apply
 * - CSR weights: create, transfer, SpMV
 * - Spectral radius: identity matrix, known matrices, rescaling
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <algorithm>
#include <numeric>

// GPU headers
#include "gpu/lnn/nimcp_lnn_gpu.h"
#include "gpu/lnn/nimcp_lnn_gradient_dao.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"

//=============================================================================
// Test Constants
//=============================================================================

static constexpr size_t DEFAULT_GRAD_SIZE = 1000;
static constexpr size_t SMALL_SIZE = 10;
static constexpr float DEFAULT_CLIP_VALUE = 1.0f;
static constexpr float NUMERICAL_EPS = 1e-4f;

//=============================================================================
// Test Fixture
//=============================================================================

/**
 * @brief Test fixture for LNN GPU enhancement tests
 */
class LNNEnhancementsTest : public ::testing::Test {
protected:
    nimcp_gpu_context_t* ctx = nullptr;
    bool gpu_available = false;

    void SetUp() override {
        ctx = nimcp_gpu_context_create_auto();
        gpu_available = (ctx != nullptr && nimcp_gpu_context_is_valid(ctx));
    }

    void TearDown() override {
        if (ctx) {
            nimcp_gpu_context_destroy(ctx);
            ctx = nullptr;
        }
    }

    void RequireGPU() {
        if (!gpu_available) {
            GTEST_SKIP() << "GPU not available, skipping test";
        }
    }

    /**
     * @brief Create identity matrix
     */
    std::vector<float> create_identity_matrix(size_t n) {
        std::vector<float> mat(n * n, 0.0f);
        for (size_t i = 0; i < n; i++) {
            mat[i * n + i] = 1.0f;
        }
        return mat;
    }

    /**
     * @brief Create scaled identity matrix
     */
    std::vector<float> create_scaled_identity(size_t n, float scale) {
        std::vector<float> mat(n * n, 0.0f);
        for (size_t i = 0; i < n; i++) {
            mat[i * n + i] = scale;
        }
        return mat;
    }

    /**
     * @brief Create random symmetric matrix with known spectral properties
     */
    std::vector<float> create_symmetric_matrix(size_t n, float max_eigenvalue) {
        // Create diagonal matrix with eigenvalues, then rotate
        // For simplicity, just create a diagonal matrix
        std::vector<float> mat(n * n, 0.0f);
        for (size_t i = 0; i < n; i++) {
            // Eigenvalues from 0.1 to max_eigenvalue
            float eigenvalue = 0.1f + (max_eigenvalue - 0.1f) * ((float)i / (n - 1));
            mat[i * n + i] = eigenvalue;
        }
        return mat;
    }

    /**
     * @brief Create tridiagonal matrix (known spectral radius formula)
     * For tridiagonal with 1s on sub/super-diagonals and 0 on main diagonal:
     * eigenvalues = 2 * cos(k * pi / (n+1)) for k = 1..n
     * spectral radius = 2 * cos(pi / (n+1))
     */
    std::vector<float> create_tridiagonal_matrix(size_t n) {
        std::vector<float> mat(n * n, 0.0f);
        for (size_t i = 0; i < n; i++) {
            if (i > 0) mat[i * n + (i - 1)] = 1.0f;
            if (i < n - 1) mat[i * n + (i + 1)] = 1.0f;
        }
        return mat;
    }

    /**
     * @brief Expected spectral radius for tridiagonal matrix
     */
    float tridiagonal_spectral_radius(size_t n) {
        return 2.0f * cosf(M_PI / (n + 1));
    }
};

//=============================================================================
// Gradient DAO Tests
//=============================================================================

TEST_F(LNNEnhancementsTest, GradientDAO_Create) {
    // Test creation without GPU context (CPU mode)
    nimcp_lnn_gradient_dao_t* dao = nimcp_lnn_gradient_dao_create(
        nullptr, DEFAULT_GRAD_SIZE, DEFAULT_CLIP_VALUE, true);

    ASSERT_NE(dao, nullptr);
    EXPECT_EQ(nimcp_lnn_gradient_dao_get_size(dao), DEFAULT_GRAD_SIZE);
    EXPECT_EQ(nimcp_lnn_gradient_dao_get_accumulation_steps(dao), 0);
    EXPECT_FLOAT_EQ(nimcp_lnn_gradient_dao_get_clip_value(dao), DEFAULT_CLIP_VALUE);
    EXPECT_TRUE(nimcp_lnn_gradient_dao_is_normalizing(dao));

    nimcp_lnn_gradient_dao_destroy(dao);
}

TEST_F(LNNEnhancementsTest, GradientDAO_CreateWithGPU) {
    RequireGPU();

    nimcp_lnn_gradient_dao_t* dao = nimcp_lnn_gradient_dao_create(
        ctx, DEFAULT_GRAD_SIZE, DEFAULT_CLIP_VALUE, false);

    ASSERT_NE(dao, nullptr);
    EXPECT_EQ(nimcp_lnn_gradient_dao_get_size(dao), DEFAULT_GRAD_SIZE);
    EXPECT_FALSE(nimcp_lnn_gradient_dao_is_normalizing(dao));

    nimcp_lnn_gradient_dao_destroy(dao);
}

TEST_F(LNNEnhancementsTest, GradientDAO_AccumulateCPU) {
    // Test gradient accumulation in CPU mode
    nimcp_lnn_gradient_dao_t* dao = nimcp_lnn_gradient_dao_create(
        nullptr, SMALL_SIZE, 0.0f, false);  // No clipping, no normalization

    ASSERT_NE(dao, nullptr);

    // Create test gradients
    std::vector<float> grads1(SMALL_SIZE, 1.0f);
    std::vector<float> grads2(SMALL_SIZE, 2.0f);

    // Accumulate
    EXPECT_EQ(dao->accumulate(dao, grads1.data()), 0);
    EXPECT_EQ(nimcp_lnn_gradient_dao_get_accumulation_steps(dao), 1);

    EXPECT_EQ(dao->accumulate(dao, grads2.data()), 0);
    EXPECT_EQ(nimcp_lnn_gradient_dao_get_accumulation_steps(dao), 2);

    // Check accumulated values via host cache
    const float* cache = nimcp_lnn_gradient_dao_get_host_cache(dao);
    ASSERT_NE(cache, nullptr);

    // Should be 1.0 + 2.0 = 3.0 for each element
    for (size_t i = 0; i < SMALL_SIZE; i++) {
        EXPECT_FLOAT_EQ(cache[i], 3.0f);
    }

    nimcp_lnn_gradient_dao_destroy(dao);
}

TEST_F(LNNEnhancementsTest, GradientDAO_Reset) {
    nimcp_lnn_gradient_dao_t* dao = nimcp_lnn_gradient_dao_create(
        nullptr, SMALL_SIZE, 0.0f, false);

    ASSERT_NE(dao, nullptr);

    std::vector<float> grads(SMALL_SIZE, 5.0f);
    dao->accumulate(dao, grads.data());
    EXPECT_EQ(nimcp_lnn_gradient_dao_get_accumulation_steps(dao), 1);

    // Reset
    EXPECT_EQ(dao->reset(dao), 0);
    EXPECT_EQ(nimcp_lnn_gradient_dao_get_accumulation_steps(dao), 0);

    // Check host cache is zeroed
    const float* cache = nimcp_lnn_gradient_dao_get_host_cache(dao);
    for (size_t i = 0; i < SMALL_SIZE; i++) {
        EXPECT_FLOAT_EQ(cache[i], 0.0f);
    }

    nimcp_lnn_gradient_dao_destroy(dao);
}

TEST_F(LNNEnhancementsTest, GradientDAO_ApplyWithClipping) {
    nimcp_lnn_gradient_dao_t* dao = nimcp_lnn_gradient_dao_create(
        nullptr, SMALL_SIZE, 1.0f, false);  // Clip at 1.0

    ASSERT_NE(dao, nullptr);

    // Accumulate large gradients
    std::vector<float> grads(SMALL_SIZE, 5.0f);  // Will be clipped to 1.0
    dao->accumulate(dao, grads.data());

    // Create weights
    std::vector<float> weights(SMALL_SIZE, 10.0f);

    // Apply with lr=0.1
    // Expected: weights = 10.0 - 0.1 * 1.0 = 9.9 (clipped gradient)
    EXPECT_EQ(dao->apply(dao, weights.data(), 0.1f), 0);

    for (size_t i = 0; i < SMALL_SIZE; i++) {
        EXPECT_NEAR(weights[i], 9.9f, NUMERICAL_EPS);
    }

    nimcp_lnn_gradient_dao_destroy(dao);
}

TEST_F(LNNEnhancementsTest, GradientDAO_ApplyWithNormalization) {
    nimcp_lnn_gradient_dao_t* dao = nimcp_lnn_gradient_dao_create(
        nullptr, SMALL_SIZE, 0.0f, true);  // Normalize enabled

    ASSERT_NE(dao, nullptr);

    // Create gradients with known L2 norm
    // All ones: norm = sqrt(SMALL_SIZE)
    std::vector<float> grads(SMALL_SIZE, 1.0f);
    dao->accumulate(dao, grads.data());

    std::vector<float> weights(SMALL_SIZE, 0.0f);
    EXPECT_EQ(dao->apply(dao, weights.data(), 1.0f), 0);

    // After normalization, each element should be 1/sqrt(SMALL_SIZE)
    // So weights = 0 - 1.0 * (1/sqrt(10)) = -0.316...
    float expected = -1.0f / sqrtf((float)SMALL_SIZE);
    for (size_t i = 0; i < SMALL_SIZE; i++) {
        EXPECT_NEAR(weights[i], expected, NUMERICAL_EPS);
    }

    nimcp_lnn_gradient_dao_destroy(dao);
}

TEST_F(LNNEnhancementsTest, GradientDAO_AccumulationAveraging) {
    nimcp_lnn_gradient_dao_t* dao = nimcp_lnn_gradient_dao_create(
        nullptr, SMALL_SIZE, 0.0f, false);

    ASSERT_NE(dao, nullptr);

    // Accumulate 4 batches of gradients
    for (int batch = 0; batch < 4; batch++) {
        std::vector<float> grads(SMALL_SIZE, 1.0f);
        dao->accumulate(dao, grads.data());
    }

    EXPECT_EQ(nimcp_lnn_gradient_dao_get_accumulation_steps(dao), 4);

    // Weights start at 100
    std::vector<float> weights(SMALL_SIZE, 100.0f);

    // Apply with lr=0.4
    // Accumulated = 4.0, averaged = 1.0, update = 100 - 0.4 * 1.0 = 99.6
    EXPECT_EQ(dao->apply(dao, weights.data(), 0.4f), 0);

    for (size_t i = 0; i < SMALL_SIZE; i++) {
        EXPECT_NEAR(weights[i], 99.6f, NUMERICAL_EPS);
    }

    nimcp_lnn_gradient_dao_destroy(dao);
}

//=============================================================================
// CSR Weight Tests
//=============================================================================

TEST_F(LNNEnhancementsTest, CSR_Create) {
    RequireGPU();

    // Create a 4x4 sparse matrix with 6 non-zeros
    nimcp_lnn_csr_weights_t* csr = nimcp_lnn_csr_create(ctx, 4, 4, 6);

    ASSERT_NE(csr, nullptr);
    EXPECT_EQ(csr->rows, 4u);
    EXPECT_EQ(csr->cols, 4u);
    EXPECT_EQ(csr->nnz, 6u);

    nimcp_lnn_csr_destroy(csr);
}

TEST_F(LNNEnhancementsTest, CSR_TransferToGPU) {
    RequireGPU();

    // Create a simple 3x3 matrix:
    // [1 0 2]
    // [0 3 0]
    // [4 0 5]
    //
    // CSR representation:
    // row_offsets = [0, 2, 3, 5]
    // col_indices = [0, 2, 1, 0, 2]
    // values = [1, 2, 3, 4, 5]

    nimcp_lnn_csr_weights_t* csr = nimcp_lnn_csr_create(ctx, 3, 3, 5);
    ASSERT_NE(csr, nullptr);

    int h_row_offsets[] = {0, 2, 3, 5};
    int h_col_indices[] = {0, 2, 1, 0, 2};
    float h_values[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};

    EXPECT_EQ(nimcp_lnn_csr_to_gpu(csr, h_row_offsets, h_col_indices, h_values), 0);

    // Read back and verify
    int r_row_offsets[4];
    int r_col_indices[5];
    float r_values[5];

    EXPECT_EQ(nimcp_lnn_csr_from_gpu(csr, r_row_offsets, r_col_indices, r_values), 0);

    for (int i = 0; i < 4; i++) {
        EXPECT_EQ(r_row_offsets[i], h_row_offsets[i]);
    }
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(r_col_indices[i], h_col_indices[i]);
        EXPECT_FLOAT_EQ(r_values[i], h_values[i]);
    }

    nimcp_lnn_csr_destroy(csr);
}

TEST_F(LNNEnhancementsTest, CSR_SpMV_Identity) {
    RequireGPU();

    // 3x3 identity matrix in CSR
    // row_offsets = [0, 1, 2, 3]
    // col_indices = [0, 1, 2]
    // values = [1, 1, 1]

    nimcp_lnn_csr_weights_t* csr = nimcp_lnn_csr_create(ctx, 3, 3, 3);
    ASSERT_NE(csr, nullptr);

    int h_row_offsets[] = {0, 1, 2, 3};
    int h_col_indices[] = {0, 1, 2};
    float h_values[] = {1.0f, 1.0f, 1.0f};

    EXPECT_EQ(nimcp_lnn_csr_to_gpu(csr, h_row_offsets, h_col_indices, h_values), 0);

    // Create GPU tensors for x and y
    float h_x[] = {1.0f, 2.0f, 3.0f};
    size_t dims[] = {3};
    nimcp_gpu_tensor_t* x_tensor = nimcp_gpu_tensor_from_host(ctx, h_x, dims, 1, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* y_tensor = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);

    ASSERT_NE(x_tensor, nullptr);
    ASSERT_NE(y_tensor, nullptr);

    // Perform SpMV: y = I * x = x
    EXPECT_EQ(nimcp_lnn_csr_spmv(csr, (float*)x_tensor->data, (float*)y_tensor->data), 0);

    // Copy result back
    float h_y[3];
    nimcp_gpu_tensor_to_host(y_tensor, h_y);

    EXPECT_FLOAT_EQ(h_y[0], 1.0f);
    EXPECT_FLOAT_EQ(h_y[1], 2.0f);
    EXPECT_FLOAT_EQ(h_y[2], 3.0f);

    nimcp_gpu_tensor_destroy(x_tensor);
    nimcp_gpu_tensor_destroy(y_tensor);
    nimcp_lnn_csr_destroy(csr);
}

TEST_F(LNNEnhancementsTest, CSR_SpMV_General) {
    RequireGPU();

    // Matrix:
    // [1 2 0]
    // [0 3 4]
    // [5 0 6]
    //
    // CSR:
    // row_offsets = [0, 2, 4, 6]
    // col_indices = [0, 1, 1, 2, 0, 2]
    // values = [1, 2, 3, 4, 5, 6]

    nimcp_lnn_csr_weights_t* csr = nimcp_lnn_csr_create(ctx, 3, 3, 6);
    ASSERT_NE(csr, nullptr);

    int h_row_offsets[] = {0, 2, 4, 6};
    int h_col_indices[] = {0, 1, 1, 2, 0, 2};
    float h_values[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};

    EXPECT_EQ(nimcp_lnn_csr_to_gpu(csr, h_row_offsets, h_col_indices, h_values), 0);

    // x = [1, 2, 3]
    // y = A * x = [1*1 + 2*2, 3*2 + 4*3, 5*1 + 6*3] = [5, 18, 23]
    float h_x[] = {1.0f, 2.0f, 3.0f};
    size_t dims[] = {3};
    nimcp_gpu_tensor_t* x_tensor = nimcp_gpu_tensor_from_host(ctx, h_x, dims, 1, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* y_tensor = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);

    EXPECT_EQ(nimcp_lnn_csr_spmv(csr, (float*)x_tensor->data, (float*)y_tensor->data), 0);

    float h_y[3];
    nimcp_gpu_tensor_to_host(y_tensor, h_y);

    EXPECT_FLOAT_EQ(h_y[0], 5.0f);
    EXPECT_FLOAT_EQ(h_y[1], 18.0f);
    EXPECT_FLOAT_EQ(h_y[2], 23.0f);

    nimcp_gpu_tensor_destroy(x_tensor);
    nimcp_gpu_tensor_destroy(y_tensor);
    nimcp_lnn_csr_destroy(csr);
}

//=============================================================================
// Spectral Radius Tests
//=============================================================================

TEST_F(LNNEnhancementsTest, SpectralRadius_Identity) {
    RequireGPU();

    // Identity matrix has spectral radius = 1.0
    const size_t n = 5;
    std::vector<float> mat = create_identity_matrix(n);

    nimcp_lnn_spectral_config_t config;
    config.max_iterations = 100;
    config.tolerance = 1e-6f;
    config.target_radius = 0.0f;

    float radius = nimcp_lnn_compute_spectral_radius(ctx, mat.data(), n, &config);

    EXPECT_NEAR(radius, 1.0f, 0.01f);
}

TEST_F(LNNEnhancementsTest, SpectralRadius_ScaledIdentity) {
    RequireGPU();

    // Scaled identity: I * 3 has spectral radius = 3.0
    const size_t n = 5;
    std::vector<float> mat = create_scaled_identity(n, 3.0f);

    nimcp_lnn_spectral_config_t config;
    config.max_iterations = 100;
    config.tolerance = 1e-6f;
    config.target_radius = 0.0f;

    float radius = nimcp_lnn_compute_spectral_radius(ctx, mat.data(), n, &config);

    EXPECT_NEAR(radius, 3.0f, 0.01f);
}

TEST_F(LNNEnhancementsTest, SpectralRadius_Tridiagonal) {
    RequireGPU();

    // Tridiagonal matrix has known spectral radius
    const size_t n = 10;
    std::vector<float> mat = create_tridiagonal_matrix(n);
    float expected_radius = tridiagonal_spectral_radius(n);

    nimcp_lnn_spectral_config_t config;
    config.max_iterations = 200;
    config.tolerance = 1e-6f;
    config.target_radius = 0.0f;

    float radius = nimcp_lnn_compute_spectral_radius(ctx, mat.data(), n, &config);

    EXPECT_NEAR(radius, expected_radius, 0.05f);
}

TEST_F(LNNEnhancementsTest, SpectralRadius_Diagonal) {
    RequireGPU();

    // Diagonal matrix: spectral radius is max absolute diagonal value
    const size_t n = 5;
    std::vector<float> mat(n * n, 0.0f);
    mat[0 * n + 0] = 0.5f;
    mat[1 * n + 1] = -2.0f;  // max absolute value
    mat[2 * n + 2] = 1.5f;
    mat[3 * n + 3] = -0.3f;
    mat[4 * n + 4] = 1.0f;

    nimcp_lnn_spectral_config_t config;
    config.max_iterations = 100;
    config.tolerance = 1e-6f;
    config.target_radius = 0.0f;

    float radius = nimcp_lnn_compute_spectral_radius(ctx, mat.data(), n, &config);

    EXPECT_NEAR(radius, 2.0f, 0.01f);
}

TEST_F(LNNEnhancementsTest, SpectralRadius_Rescale) {
    RequireGPU();

    // Start with matrix that has spectral radius != 1.0
    const size_t n = 5;
    std::vector<float> mat = create_scaled_identity(n, 2.5f);

    // Verify initial spectral radius
    nimcp_lnn_spectral_config_t config;
    config.max_iterations = 100;
    config.tolerance = 1e-6f;
    config.target_radius = 0.0f;

    float initial_radius = nimcp_lnn_compute_spectral_radius(ctx, mat.data(), n, &config);
    EXPECT_NEAR(initial_radius, 2.5f, 0.01f);

    // Rescale to target radius 0.9
    float target = 0.9f;
    EXPECT_EQ(nimcp_lnn_rescale_to_spectral_radius(ctx, mat.data(), n, target), 0);

    // Verify new spectral radius
    float new_radius = nimcp_lnn_compute_spectral_radius(ctx, mat.data(), n, &config);
    EXPECT_NEAR(new_radius, target, 0.01f);
}

TEST_F(LNNEnhancementsTest, SpectralRadius_RescaleToOne) {
    RequireGPU();

    // Rescale to edge of chaos (spectral radius = 1.0)
    const size_t n = 8;
    std::vector<float> mat = create_symmetric_matrix(n, 3.5f);

    // Rescale to 1.0
    EXPECT_EQ(nimcp_lnn_rescale_to_spectral_radius(ctx, mat.data(), n, 1.0f), 0);

    nimcp_lnn_spectral_config_t config;
    config.max_iterations = 100;
    config.tolerance = 1e-6f;
    config.target_radius = 0.0f;

    float new_radius = nimcp_lnn_compute_spectral_radius(ctx, mat.data(), n, &config);
    EXPECT_NEAR(new_radius, 1.0f, 0.02f);
}

TEST_F(LNNEnhancementsTest, SpectralRadius_LargerMatrix) {
    RequireGPU();

    // Test with a larger matrix
    const size_t n = 50;
    std::vector<float> mat = create_scaled_identity(n, 1.5f);

    nimcp_lnn_spectral_config_t config;
    config.max_iterations = 100;
    config.tolerance = 1e-6f;
    config.target_radius = 0.0f;

    float radius = nimcp_lnn_compute_spectral_radius(ctx, mat.data(), n, &config);
    EXPECT_NEAR(radius, 1.5f, 0.01f);
}

//=============================================================================
// Edge Cases and Error Handling
//=============================================================================

TEST_F(LNNEnhancementsTest, GradientDAO_NullHandling) {
    // Create with zero size should fail
    nimcp_lnn_gradient_dao_t* dao = nimcp_lnn_gradient_dao_create(nullptr, 0, 1.0f, false);
    EXPECT_EQ(dao, nullptr);

    // Valid DAO
    dao = nimcp_lnn_gradient_dao_create(nullptr, SMALL_SIZE, 1.0f, false);
    ASSERT_NE(dao, nullptr);

    // Null gradient input
    EXPECT_EQ(dao->accumulate(dao, nullptr), -1);

    // Null weight input
    EXPECT_EQ(dao->apply(dao, nullptr, 0.1f), -1);

    nimcp_lnn_gradient_dao_destroy(dao);
}

TEST_F(LNNEnhancementsTest, CSR_NullHandling) {
    // Create with null context should fail
    nimcp_lnn_csr_weights_t* csr = nimcp_lnn_csr_create(nullptr, 3, 3, 5);
    EXPECT_EQ(csr, nullptr);

    // Create with zero dimensions should fail
    if (gpu_available) {
        csr = nimcp_lnn_csr_create(ctx, 0, 3, 5);
        EXPECT_EQ(csr, nullptr);
    }
}

TEST_F(LNNEnhancementsTest, SpectralRadius_NullHandling) {
    // Null matrix
    nimcp_lnn_spectral_config_t config;
    config.max_iterations = 100;
    config.tolerance = 1e-6f;
    config.target_radius = 0.0f;

    float radius = nimcp_lnn_compute_spectral_radius(ctx, nullptr, 5, &config);
    EXPECT_LT(radius, 0.0f);  // Should return -1.0

    // Zero dimension
    std::vector<float> mat(1, 1.0f);
    radius = nimcp_lnn_compute_spectral_radius(ctx, mat.data(), 0, &config);
    EXPECT_LT(radius, 0.0f);
}

TEST_F(LNNEnhancementsTest, SpectralRadius_RescaleInvalidTarget) {
    RequireGPU();

    const size_t n = 3;
    std::vector<float> mat = create_identity_matrix(n);

    // Negative target should fail
    EXPECT_EQ(nimcp_lnn_rescale_to_spectral_radius(ctx, mat.data(), n, -1.0f), -1);

    // Zero target should fail
    EXPECT_EQ(nimcp_lnn_rescale_to_spectral_radius(ctx, mat.data(), n, 0.0f), -1);
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(LNNEnhancementsTest, Integration_GradientDAOWithGPU) {
    RequireGPU();

    // Full workflow with GPU context
    nimcp_lnn_gradient_dao_t* dao = nimcp_lnn_gradient_dao_create(
        ctx, DEFAULT_GRAD_SIZE, 1.0f, true);
    ASSERT_NE(dao, nullptr);

    // Create GPU tensor for gradients
    std::vector<float> h_grads(DEFAULT_GRAD_SIZE, 0.5f);
    size_t dims[] = {DEFAULT_GRAD_SIZE};
    nimcp_gpu_tensor_t* grad_tensor = nimcp_gpu_tensor_from_host(
        ctx, h_grads.data(), dims, 1, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(grad_tensor, nullptr);

    // Accumulate GPU gradients
    EXPECT_EQ(dao->accumulate(dao, (float*)grad_tensor->data), 0);
    EXPECT_EQ(nimcp_lnn_gradient_dao_get_accumulation_steps(dao), 1);

    // Sync to host and verify
    EXPECT_EQ(dao->sync_to_host(dao), 0);
    const float* cache = nimcp_lnn_gradient_dao_get_host_cache(dao);
    EXPECT_NEAR(cache[0], 0.5f, NUMERICAL_EPS);

    nimcp_gpu_tensor_destroy(grad_tensor);
    nimcp_lnn_gradient_dao_destroy(dao);
}

TEST_F(LNNEnhancementsTest, Integration_CSRWithLargeMatrix) {
    RequireGPU();

    // Test with a larger sparse matrix (simulating NCP wiring)
    const size_t n = 100;
    const float sparsity = 0.05f;  // 5% non-zero
    const size_t approx_nnz = (size_t)(n * n * sparsity);

    // Create sparse pattern (diagonal + some random)
    std::vector<int> row_offsets(n + 1);
    std::vector<int> col_indices;
    std::vector<float> values;

    row_offsets[0] = 0;
    for (size_t i = 0; i < n; i++) {
        // Always include diagonal
        col_indices.push_back(i);
        values.push_back(1.0f);

        // Add a few off-diagonal elements
        if (i > 0) {
            col_indices.push_back(i - 1);
            values.push_back(0.1f);
        }
        if (i < n - 1) {
            col_indices.push_back(i + 1);
            values.push_back(0.1f);
        }

        row_offsets[i + 1] = col_indices.size();
    }

    size_t actual_nnz = values.size();

    nimcp_lnn_csr_weights_t* csr = nimcp_lnn_csr_create(ctx, n, n, actual_nnz);
    ASSERT_NE(csr, nullptr);

    EXPECT_EQ(nimcp_lnn_csr_to_gpu(csr, row_offsets.data(), col_indices.data(), values.data()), 0);

    // Test SpMV with ones vector
    std::vector<float> h_x(n, 1.0f);
    std::vector<float> h_y(n, 0.0f);

    size_t dims[] = {n};
    nimcp_gpu_tensor_t* x_tensor = nimcp_gpu_tensor_from_host(ctx, h_x.data(), dims, 1, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* y_tensor = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);

    EXPECT_EQ(nimcp_lnn_csr_spmv(csr, (float*)x_tensor->data, (float*)y_tensor->data), 0);

    nimcp_gpu_tensor_to_host(y_tensor, h_y.data());

    // Middle rows should sum to ~1.2 (1.0 + 0.1 + 0.1)
    EXPECT_NEAR(h_y[n/2], 1.2f, 0.01f);

    nimcp_gpu_tensor_destroy(x_tensor);
    nimcp_gpu_tensor_destroy(y_tensor);
    nimcp_lnn_csr_destroy(csr);
}
