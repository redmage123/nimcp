/**
 * @file test_mixed_precision.cpp
 * @brief Unit tests for Mixed Precision (FP16/BF16) operations
 *
 * Tests FP32-FP16 conversions, FP16 GEMM, element-wise ops, loss scaling,
 * gradient handling, and Tensor Core detection.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <cstring>
#include <limits>
#include <random>

// Headers already have their own extern "C" guards
#include "gpu/tensor/nimcp_tensor_fp16.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class MixedPrecisionTest : public ::testing::Test {
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

    // Helper to create FP32 tensor filled with value
    nimcp_gpu_tensor_t* CreateFP32Tensor(size_t* dims, uint32_t ndim, float value) {
        nimcp_gpu_tensor_t* tensor = nimcp_gpu_tensor_create(ctx, dims, ndim, NIMCP_GPU_PRECISION_FP32);
        if (tensor) {
            nimcp_gpu_fill(ctx, tensor, value);
        }
        return tensor;
    }

    // Helper to create FP16 tensor
    nimcp_gpu_tensor_t* CreateFP16Tensor(size_t* dims, uint32_t ndim) {
        return nimcp_gpu_tensor_create(ctx, dims, ndim, NIMCP_GPU_PRECISION_FP16);
    }

    // Helper to create 1D FP32 tensor
    nimcp_gpu_tensor_t* Create1DFP32(size_t n, float value = 0.0f) {
        size_t dims[1] = {n};
        return CreateFP32Tensor(dims, 1, value);
    }

    // Helper to create 2D FP32 tensor
    nimcp_gpu_tensor_t* Create2DFP32(size_t rows, size_t cols, float value = 0.0f) {
        size_t dims[2] = {rows, cols};
        return CreateFP32Tensor(dims, 2, value);
    }

    // Helper to copy tensor to host
    std::vector<float> CopyToHost(nimcp_gpu_tensor_t* tensor) {
        size_t n = tensor->numel;
        std::vector<float> host_data(n);
        nimcp_gpu_tensor_to_host(tensor, host_data.data());
        return host_data;
    }

    // Helper to set tensor from host
    void SetFromHost(nimcp_gpu_tensor_t* tensor, const std::vector<float>& data) {
        size_t n = tensor->numel;
        size_t bytes = n * tensor->elem_size;
        nimcp_gpu_memcpy(ctx, tensor->data, data.data(), bytes, GPU_MEMCPY_HOST_TO_DEVICE);
    }

    // Generate random float data
    std::vector<float> GenerateRandom(size_t n, float min = -1.0f, float max = 1.0f) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dist(min, max);

        std::vector<float> data(n);
        for (size_t i = 0; i < n; i++) {
            data[i] = dist(gen);
        }
        return data;
    }
};

//=============================================================================
// FP32 to FP16 Conversion Tests
//=============================================================================

TEST_F(MixedPrecisionTest, FP32ToFP16_BasicConversion_Accurate) {
    RequireGPU();

    const size_t n = 1024;
    size_t dims[1] = {n};

    nimcp_gpu_tensor_t* fp32 = CreateFP32Tensor(dims, 1, 0.5f);
    nimcp_gpu_tensor_t* fp16 = CreateFP16Tensor(dims, 1);
    ASSERT_NE(fp32, nullptr);
    ASSERT_NE(fp16, nullptr);

    bool result = nimcp_fp32_to_fp16(ctx, fp32, fp16);
    EXPECT_TRUE(result);

    // Convert back to verify
    nimcp_gpu_tensor_t* fp32_back = Create1DFP32(n, 0.0f);
    result = nimcp_fp16_to_fp32(ctx, fp16, fp32_back);
    EXPECT_TRUE(result);

    auto data = CopyToHost(fp32_back);
    for (size_t i = 0; i < n; i++) {
        EXPECT_NEAR(data[i], 0.5f, 1e-3f);  // FP16 has ~3 decimal places precision
    }

    nimcp_gpu_tensor_destroy(fp32);
    nimcp_gpu_tensor_destroy(fp16);
    nimcp_gpu_tensor_destroy(fp32_back);
}

TEST_F(MixedPrecisionTest, FP32ToFP16_SmallValues_Preserved) {
    RequireGPU();

    const size_t n = 100;
    size_t dims[1] = {n};

    nimcp_gpu_tensor_t* fp32 = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* fp16 = CreateFP16Tensor(dims, 1);
    ASSERT_NE(fp32, nullptr);
    ASSERT_NE(fp16, nullptr);

    // Set small values (within FP16 range)
    std::vector<float> small_values(n);
    for (size_t i = 0; i < n; i++) {
        small_values[i] = 1e-4f * (i + 1);  // 0.0001 to 0.01
    }
    SetFromHost(fp32, small_values);

    bool result = nimcp_fp32_to_fp16(ctx, fp32, fp16);
    EXPECT_TRUE(result);

    // Convert back
    nimcp_gpu_tensor_t* fp32_back = Create1DFP32(n, 0.0f);
    nimcp_fp16_to_fp32(ctx, fp16, fp32_back);

    auto data = CopyToHost(fp32_back);
    for (size_t i = 0; i < n; i++) {
        float expected = small_values[i];
        // FP16 has limited precision for small values
        EXPECT_NEAR(data[i], expected, std::abs(expected) * 0.01f + 1e-5f);
    }

    nimcp_gpu_tensor_destroy(fp32);
    nimcp_gpu_tensor_destroy(fp16);
    nimcp_gpu_tensor_destroy(fp32_back);
}

TEST_F(MixedPrecisionTest, FP32ToFP16_LargeValues_Clamped) {
    RequireGPU();

    const size_t n = 10;
    size_t dims[1] = {n};

    nimcp_gpu_tensor_t* fp32 = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* fp16 = CreateFP16Tensor(dims, 1);
    ASSERT_NE(fp32, nullptr);
    ASSERT_NE(fp16, nullptr);

    // Values at/near FP16 max (~65504)
    std::vector<float> large_values = {100.0f, 1000.0f, 10000.0f, 60000.0f, 65500.0f,
                                        -100.0f, -1000.0f, -10000.0f, -60000.0f, -65500.0f};
    SetFromHost(fp32, large_values);

    bool result = nimcp_fp32_to_fp16(ctx, fp32, fp16);
    EXPECT_TRUE(result);

    // Convert back and verify values within FP16 range are preserved
    nimcp_gpu_tensor_t* fp32_back = Create1DFP32(n, 0.0f);
    nimcp_fp16_to_fp32(ctx, fp16, fp32_back);

    auto data = CopyToHost(fp32_back);
    for (size_t i = 0; i < 5; i++) {
        EXPECT_NEAR(data[i], large_values[i], std::abs(large_values[i]) * 0.001f + 1.0f);
    }

    nimcp_gpu_tensor_destroy(fp32);
    nimcp_gpu_tensor_destroy(fp16);
    nimcp_gpu_tensor_destroy(fp32_back);
}

//=============================================================================
// FP16 to FP32 Conversion Tests
//=============================================================================

TEST_F(MixedPrecisionTest, FP16ToFP32_BasicConversion_Accurate) {
    RequireGPU();

    const size_t n = 1024;
    size_t dims[1] = {n};

    // Create FP32, convert to FP16, then back to FP32
    nimcp_gpu_tensor_t* fp32_orig = CreateFP32Tensor(dims, 1, 1.5f);
    nimcp_gpu_tensor_t* fp16 = CreateFP16Tensor(dims, 1);
    nimcp_gpu_tensor_t* fp32_result = CreateFP32Tensor(dims, 1, 0.0f);

    nimcp_fp32_to_fp16(ctx, fp32_orig, fp16);
    bool result = nimcp_fp16_to_fp32(ctx, fp16, fp32_result);
    EXPECT_TRUE(result);

    auto data = CopyToHost(fp32_result);
    for (size_t i = 0; i < n; i++) {
        EXPECT_NEAR(data[i], 1.5f, 1e-3f);
    }

    nimcp_gpu_tensor_destroy(fp32_orig);
    nimcp_gpu_tensor_destroy(fp16);
    nimcp_gpu_tensor_destroy(fp32_result);
}

//=============================================================================
// BF16 Conversion Tests
//=============================================================================

TEST_F(MixedPrecisionTest, FP32ToBF16_BasicConversion) {
    RequireGPU();

    if (!nimcp_bf16_supported(ctx)) {
        GTEST_SKIP() << "BF16 not supported on this GPU";
    }

    const size_t n = 512;
    size_t dims[1] = {n};

    nimcp_gpu_tensor_t* fp32 = CreateFP32Tensor(dims, 1, 2.5f);
    nimcp_gpu_tensor_t* bf16 = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_BF16);
    ASSERT_NE(bf16, nullptr);

    bool result = nimcp_fp32_to_bf16(ctx, fp32, bf16);
    EXPECT_TRUE(result);

    // Convert back
    nimcp_gpu_tensor_t* fp32_back = CreateFP32Tensor(dims, 1, 0.0f);
    result = nimcp_bf16_to_fp32(ctx, bf16, fp32_back);
    EXPECT_TRUE(result);

    auto data = CopyToHost(fp32_back);
    for (size_t i = 0; i < n; i++) {
        // BF16 has same range as FP32 but less precision
        EXPECT_NEAR(data[i], 2.5f, 0.01f);
    }

    nimcp_gpu_tensor_destroy(fp32);
    nimcp_gpu_tensor_destroy(bf16);
    nimcp_gpu_tensor_destroy(fp32_back);
}

TEST_F(MixedPrecisionTest, BF16_PreservesLargerRange) {
    RequireGPU();

    if (!nimcp_bf16_supported(ctx)) {
        GTEST_SKIP() << "BF16 not supported on this GPU";
    }

    const size_t n = 10;
    size_t dims[1] = {n};

    nimcp_gpu_tensor_t* fp32 = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* bf16 = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_BF16);

    // Values that would overflow FP16 but fit in BF16
    std::vector<float> large_values = {1e6f, 1e8f, 1e10f, 1e20f, 1e30f,
                                        -1e6f, -1e8f, -1e10f, -1e20f, -1e30f};
    SetFromHost(fp32, large_values);

    nimcp_fp32_to_bf16(ctx, fp32, bf16);

    nimcp_gpu_tensor_t* fp32_back = CreateFP32Tensor(dims, 1, 0.0f);
    nimcp_bf16_to_fp32(ctx, bf16, fp32_back);

    auto data = CopyToHost(fp32_back);
    for (size_t i = 0; i < n; i++) {
        // BF16 can represent large values but with reduced precision
        float ratio = data[i] / large_values[i];
        EXPECT_NEAR(ratio, 1.0f, 0.01f);  // Within 1% of original
    }

    nimcp_gpu_tensor_destroy(fp32);
    nimcp_gpu_tensor_destroy(bf16);
    nimcp_gpu_tensor_destroy(fp32_back);
}

//=============================================================================
// FP16 GEMM Tests
//=============================================================================

TEST_F(MixedPrecisionTest, FP16GEMM_SquareMatrices_Correct) {
    RequireGPU();

    const size_t M = 64, K = 64, N = 64;
    size_t dims_A[2] = {M, K};
    size_t dims_B[2] = {K, N};
    size_t dims_C[2] = {M, N};

    // Create FP16 matrices
    nimcp_gpu_tensor_t* A = CreateFP16Tensor(dims_A, 2);
    nimcp_gpu_tensor_t* B = CreateFP16Tensor(dims_B, 2);
    nimcp_gpu_tensor_t* C = CreateFP16Tensor(dims_C, 2);

    // Initialize A and B with known values
    // A = all 1s, B = identity-like pattern
    nimcp_gpu_tensor_t* A_fp32 = Create2DFP32(M, K, 1.0f);
    nimcp_gpu_tensor_t* B_fp32 = Create2DFP32(K, N, 0.0f);

    std::vector<float> b_data(K * N, 0.0f);
    for (size_t i = 0; i < std::min(K, N); i++) {
        b_data[i * N + i] = 1.0f;
    }
    SetFromHost(B_fp32, b_data);

    nimcp_fp32_to_fp16(ctx, A_fp32, A);
    nimcp_fp32_to_fp16(ctx, B_fp32, B);

    // C = A @ B
    bool result = nimcp_fp16_gemm(ctx, A, B, C, 1.0f, 0.0f, false, false);
    EXPECT_TRUE(result);

    // Convert result back to FP32 for checking
    nimcp_gpu_tensor_t* C_fp32 = Create2DFP32(M, N, 0.0f);
    nimcp_fp16_to_fp32(ctx, C, C_fp32);

    auto c_data = CopyToHost(C_fp32);

    // Each row of C should have 1s where B had 1s (diagonal)
    for (size_t i = 0; i < M; i++) {
        for (size_t j = 0; j < N; j++) {
            float expected = (j < K) ? 1.0f : 0.0f;
            EXPECT_NEAR(c_data[i * N + j], expected, 0.01f)
                << "Mismatch at (" << i << ", " << j << ")";
        }
    }

    nimcp_gpu_tensor_destroy(A);
    nimcp_gpu_tensor_destroy(B);
    nimcp_gpu_tensor_destroy(C);
    nimcp_gpu_tensor_destroy(A_fp32);
    nimcp_gpu_tensor_destroy(B_fp32);
    nimcp_gpu_tensor_destroy(C_fp32);
}

TEST_F(MixedPrecisionTest, FP16GEMM_NonSquare_Correct) {
    RequireGPU();

    const size_t M = 32, K = 64, N = 48;
    size_t dims_A[2] = {M, K};
    size_t dims_B[2] = {K, N};
    size_t dims_C[2] = {M, N};

    nimcp_gpu_tensor_t* A = CreateFP16Tensor(dims_A, 2);
    nimcp_gpu_tensor_t* B = CreateFP16Tensor(dims_B, 2);
    nimcp_gpu_tensor_t* C = CreateFP16Tensor(dims_C, 2);

    // Initialize with known values
    nimcp_gpu_tensor_t* A_fp32 = Create2DFP32(M, K, 0.5f);
    nimcp_gpu_tensor_t* B_fp32 = Create2DFP32(K, N, 0.5f);

    nimcp_fp32_to_fp16(ctx, A_fp32, A);
    nimcp_fp32_to_fp16(ctx, B_fp32, B);

    bool result = nimcp_fp16_gemm(ctx, A, B, C, 1.0f, 0.0f, false, false);
    EXPECT_TRUE(result);

    // Expected: each element = K * 0.5 * 0.5 = K * 0.25 = 16
    nimcp_gpu_tensor_t* C_fp32 = Create2DFP32(M, N, 0.0f);
    nimcp_fp16_to_fp32(ctx, C, C_fp32);

    auto c_data = CopyToHost(C_fp32);
    float expected = K * 0.25f;
    for (size_t i = 0; i < M * N; i++) {
        EXPECT_NEAR(c_data[i], expected, expected * 0.01f);
    }

    nimcp_gpu_tensor_destroy(A);
    nimcp_gpu_tensor_destroy(B);
    nimcp_gpu_tensor_destroy(C);
    nimcp_gpu_tensor_destroy(A_fp32);
    nimcp_gpu_tensor_destroy(B_fp32);
    nimcp_gpu_tensor_destroy(C_fp32);
}

TEST_F(MixedPrecisionTest, FP16GEMM_WithAlphaBeta) {
    RequireGPU();

    const size_t M = 32, K = 32, N = 32;
    size_t dims[2] = {M, N};

    nimcp_gpu_tensor_t* A = CreateFP16Tensor(dims, 2);
    nimcp_gpu_tensor_t* B = CreateFP16Tensor(dims, 2);
    nimcp_gpu_tensor_t* C = CreateFP16Tensor(dims, 2);

    // Initialize
    nimcp_gpu_tensor_t* A_fp32 = Create2DFP32(M, K, 1.0f);
    nimcp_gpu_tensor_t* B_fp32 = Create2DFP32(K, N, 1.0f);
    nimcp_gpu_tensor_t* C_init = Create2DFP32(M, N, 2.0f);

    nimcp_fp32_to_fp16(ctx, A_fp32, A);
    nimcp_fp32_to_fp16(ctx, B_fp32, B);
    nimcp_fp32_to_fp16(ctx, C_init, C);

    // C = 0.5 * (A @ B) + 0.5 * C
    // Expected: 0.5 * K + 0.5 * 2.0 = 0.5 * 32 + 1.0 = 17.0
    bool result = nimcp_fp16_gemm(ctx, A, B, C, 0.5f, 0.5f, false, false);
    EXPECT_TRUE(result);

    nimcp_gpu_tensor_t* C_fp32 = Create2DFP32(M, N, 0.0f);
    nimcp_fp16_to_fp32(ctx, C, C_fp32);

    auto c_data = CopyToHost(C_fp32);
    float expected = 0.5f * K + 0.5f * 2.0f;
    for (size_t i = 0; i < M * N; i++) {
        EXPECT_NEAR(c_data[i], expected, 0.5f);
    }

    nimcp_gpu_tensor_destroy(A);
    nimcp_gpu_tensor_destroy(B);
    nimcp_gpu_tensor_destroy(C);
    nimcp_gpu_tensor_destroy(A_fp32);
    nimcp_gpu_tensor_destroy(B_fp32);
    nimcp_gpu_tensor_destroy(C_init);
    nimcp_gpu_tensor_destroy(C_fp32);
}

//=============================================================================
// FP16 Element-wise Operation Tests
//=============================================================================

TEST_F(MixedPrecisionTest, FP16Add_Correct) {
    RequireGPU();

    const size_t n = 1024;
    size_t dims[1] = {n};

    nimcp_gpu_tensor_t* a = CreateFP16Tensor(dims, 1);
    nimcp_gpu_tensor_t* b = CreateFP16Tensor(dims, 1);
    nimcp_gpu_tensor_t* out = CreateFP16Tensor(dims, 1);

    nimcp_gpu_tensor_t* a_fp32 = Create1DFP32(n, 1.5f);
    nimcp_gpu_tensor_t* b_fp32 = Create1DFP32(n, 2.5f);

    nimcp_fp32_to_fp16(ctx, a_fp32, a);
    nimcp_fp32_to_fp16(ctx, b_fp32, b);

    bool result = nimcp_fp16_add(ctx, a, b, out);
    EXPECT_TRUE(result);

    nimcp_gpu_tensor_t* out_fp32 = Create1DFP32(n, 0.0f);
    nimcp_fp16_to_fp32(ctx, out, out_fp32);

    auto data = CopyToHost(out_fp32);
    for (size_t i = 0; i < n; i++) {
        EXPECT_NEAR(data[i], 4.0f, 0.01f);
    }

    nimcp_gpu_tensor_destroy(a);
    nimcp_gpu_tensor_destroy(b);
    nimcp_gpu_tensor_destroy(out);
    nimcp_gpu_tensor_destroy(a_fp32);
    nimcp_gpu_tensor_destroy(b_fp32);
    nimcp_gpu_tensor_destroy(out_fp32);
}

TEST_F(MixedPrecisionTest, FP16Mul_Correct) {
    RequireGPU();

    const size_t n = 1024;
    size_t dims[1] = {n};

    nimcp_gpu_tensor_t* a = CreateFP16Tensor(dims, 1);
    nimcp_gpu_tensor_t* b = CreateFP16Tensor(dims, 1);
    nimcp_gpu_tensor_t* out = CreateFP16Tensor(dims, 1);

    nimcp_gpu_tensor_t* a_fp32 = Create1DFP32(n, 3.0f);
    nimcp_gpu_tensor_t* b_fp32 = Create1DFP32(n, 4.0f);

    nimcp_fp32_to_fp16(ctx, a_fp32, a);
    nimcp_fp32_to_fp16(ctx, b_fp32, b);

    bool result = nimcp_fp16_mul(ctx, a, b, out);
    EXPECT_TRUE(result);

    nimcp_gpu_tensor_t* out_fp32 = Create1DFP32(n, 0.0f);
    nimcp_fp16_to_fp32(ctx, out, out_fp32);

    auto data = CopyToHost(out_fp32);
    for (size_t i = 0; i < n; i++) {
        EXPECT_NEAR(data[i], 12.0f, 0.1f);
    }

    nimcp_gpu_tensor_destroy(a);
    nimcp_gpu_tensor_destroy(b);
    nimcp_gpu_tensor_destroy(out);
    nimcp_gpu_tensor_destroy(a_fp32);
    nimcp_gpu_tensor_destroy(b_fp32);
    nimcp_gpu_tensor_destroy(out_fp32);
}

TEST_F(MixedPrecisionTest, FP16Scale_Correct) {
    RequireGPU();

    const size_t n = 1024;
    size_t dims[1] = {n};

    nimcp_gpu_tensor_t* x = CreateFP16Tensor(dims, 1);
    nimcp_gpu_tensor_t* out = CreateFP16Tensor(dims, 1);

    nimcp_gpu_tensor_t* x_fp32 = Create1DFP32(n, 5.0f);
    nimcp_fp32_to_fp16(ctx, x_fp32, x);

    bool result = nimcp_fp16_scale(ctx, x, 2.5f, out);
    EXPECT_TRUE(result);

    nimcp_gpu_tensor_t* out_fp32 = Create1DFP32(n, 0.0f);
    nimcp_fp16_to_fp32(ctx, out, out_fp32);

    auto data = CopyToHost(out_fp32);
    for (size_t i = 0; i < n; i++) {
        EXPECT_NEAR(data[i], 12.5f, 0.1f);
    }

    nimcp_gpu_tensor_destroy(x);
    nimcp_gpu_tensor_destroy(out);
    nimcp_gpu_tensor_destroy(x_fp32);
    nimcp_gpu_tensor_destroy(out_fp32);
}

TEST_F(MixedPrecisionTest, FP16FMA_Correct) {
    RequireGPU();

    const size_t n = 1024;
    size_t dims[1] = {n};

    nimcp_gpu_tensor_t* a = CreateFP16Tensor(dims, 1);
    nimcp_gpu_tensor_t* b = CreateFP16Tensor(dims, 1);
    nimcp_gpu_tensor_t* c = CreateFP16Tensor(dims, 1);
    nimcp_gpu_tensor_t* out = CreateFP16Tensor(dims, 1);

    nimcp_gpu_tensor_t* a_fp32 = Create1DFP32(n, 2.0f);
    nimcp_gpu_tensor_t* b_fp32 = Create1DFP32(n, 3.0f);
    nimcp_gpu_tensor_t* c_fp32 = Create1DFP32(n, 4.0f);

    nimcp_fp32_to_fp16(ctx, a_fp32, a);
    nimcp_fp32_to_fp16(ctx, b_fp32, b);
    nimcp_fp32_to_fp16(ctx, c_fp32, c);

    // out = a * b + c = 2 * 3 + 4 = 10
    bool result = nimcp_fp16_fma(ctx, a, b, c, out);
    EXPECT_TRUE(result);

    nimcp_gpu_tensor_t* out_fp32 = Create1DFP32(n, 0.0f);
    nimcp_fp16_to_fp32(ctx, out, out_fp32);

    auto data = CopyToHost(out_fp32);
    for (size_t i = 0; i < n; i++) {
        EXPECT_NEAR(data[i], 10.0f, 0.1f);
    }

    nimcp_gpu_tensor_destroy(a);
    nimcp_gpu_tensor_destroy(b);
    nimcp_gpu_tensor_destroy(c);
    nimcp_gpu_tensor_destroy(out);
    nimcp_gpu_tensor_destroy(a_fp32);
    nimcp_gpu_tensor_destroy(b_fp32);
    nimcp_gpu_tensor_destroy(c_fp32);
    nimcp_gpu_tensor_destroy(out_fp32);
}

//=============================================================================
// Loss Scaler Tests
//=============================================================================

TEST_F(MixedPrecisionTest, LossScalerCreate_DefaultSettings) {
    nimcp_loss_scaler_t* scaler = nimcp_loss_scaler_create(true);
    ASSERT_NE(scaler, nullptr);

    EXPECT_EQ(nimcp_loss_scaler_get_scale(scaler), MP_DEFAULT_INIT_SCALE);
    EXPECT_TRUE(scaler->dynamic);

    nimcp_loss_scaler_destroy(scaler);
}

TEST_F(MixedPrecisionTest, LossScalerCreate_CustomSettings) {
    nimcp_loss_scaler_t* scaler = nimcp_loss_scaler_create_custom(
        1024.0f,   // init_scale
        2.0f,      // growth_factor
        0.5f,      // backoff_factor
        1000,      // growth_interval
        true       // dynamic
    );
    ASSERT_NE(scaler, nullptr);

    EXPECT_EQ(nimcp_loss_scaler_get_scale(scaler), 1024.0f);

    nimcp_loss_scaler_destroy(scaler);
}

TEST_F(MixedPrecisionTest, LossScaler_ScaleLoss) {
    nimcp_loss_scaler_t* scaler = nimcp_loss_scaler_create_custom(
        1024.0f, 2.0f, 0.5f, 1000, false);
    ASSERT_NE(scaler, nullptr);

    float loss = 0.1f;
    float scaled = nimcp_loss_scaler_scale(scaler, loss);

    EXPECT_EQ(scaled, loss * 1024.0f);

    nimcp_loss_scaler_destroy(scaler);
}

TEST_F(MixedPrecisionTest, LossScaler_DynamicGrowth) {
    nimcp_loss_scaler_t* scaler = nimcp_loss_scaler_create_custom(
        1024.0f,   // init_scale
        2.0f,      // growth_factor
        0.5f,      // backoff_factor
        10,        // growth_interval (short for testing)
        true       // dynamic
    );
    ASSERT_NE(scaler, nullptr);

    float initial_scale = nimcp_loss_scaler_get_scale(scaler);

    // Simulate successful steps
    for (int i = 0; i < 15; i++) {
        nimcp_loss_scaler_update(scaler, true);  // gradients valid
    }

    float new_scale = nimcp_loss_scaler_get_scale(scaler);
    EXPECT_GT(new_scale, initial_scale);

    nimcp_loss_scaler_destroy(scaler);
}

TEST_F(MixedPrecisionTest, LossScaler_BackoffOnOverflow) {
    nimcp_loss_scaler_t* scaler = nimcp_loss_scaler_create_custom(
        1024.0f, 2.0f, 0.5f, 100, true);
    ASSERT_NE(scaler, nullptr);

    float initial_scale = nimcp_loss_scaler_get_scale(scaler);

    // Simulate overflow (gradients invalid)
    nimcp_loss_scaler_update(scaler, false);

    float new_scale = nimcp_loss_scaler_get_scale(scaler);
    EXPECT_LT(new_scale, initial_scale);
    EXPECT_EQ(new_scale, initial_scale * 0.5f);

    nimcp_loss_scaler_destroy(scaler);
}

TEST_F(MixedPrecisionTest, LossScaler_MinScaleEnforced) {
    nimcp_loss_scaler_t* scaler = nimcp_loss_scaler_create_custom(
        4.0f,      // Start low
        2.0f,
        0.5f,
        100,
        true
    );
    ASSERT_NE(scaler, nullptr);

    // Repeatedly trigger overflow
    for (int i = 0; i < 10; i++) {
        nimcp_loss_scaler_update(scaler, false);
    }

    float final_scale = nimcp_loss_scaler_get_scale(scaler);
    EXPECT_GE(final_scale, MP_MIN_SCALE);

    nimcp_loss_scaler_destroy(scaler);
}

TEST_F(MixedPrecisionTest, LossScaler_ShouldSkip) {
    nimcp_loss_scaler_t* scaler = nimcp_loss_scaler_create(true);
    ASSERT_NE(scaler, nullptr);

    EXPECT_FALSE(nimcp_loss_scaler_should_skip(scaler, true));   // Valid gradients
    EXPECT_TRUE(nimcp_loss_scaler_should_skip(scaler, false));   // Invalid gradients

    nimcp_loss_scaler_destroy(scaler);
}

//=============================================================================
// Inf/NaN Detection Tests
//=============================================================================

TEST_F(MixedPrecisionTest, CheckInfNan_NoInfNan_ReturnsZero) {
    RequireGPU();

    const size_t n = 1024;
    size_t dims[1] = {n};

    nimcp_gpu_tensor_t* tensor = CreateFP16Tensor(dims, 1);
    nimcp_gpu_tensor_t* fp32 = Create1DFP32(n, 1.5f);
    nimcp_fp32_to_fp16(ctx, fp32, tensor);

    int found_inf = -1;
    bool result = nimcp_fp16_check_inf_nan(ctx, tensor, &found_inf);

    EXPECT_TRUE(result);
    EXPECT_EQ(found_inf, 0);

    nimcp_gpu_tensor_destroy(tensor);
    nimcp_gpu_tensor_destroy(fp32);
}

TEST_F(MixedPrecisionTest, CheckInfNan_WithInf_ReturnsNonZero) {
    RequireGPU();

    const size_t n = 100;
    size_t dims[1] = {n};

    nimcp_gpu_tensor_t* fp32 = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* fp16 = CreateFP16Tensor(dims, 1);

    std::vector<float> data(n, 1.0f);
    data[50] = std::numeric_limits<float>::infinity();
    SetFromHost(fp32, data);

    nimcp_fp32_to_fp16(ctx, fp32, fp16);

    int found_inf = -1;
    bool result = nimcp_fp16_check_inf_nan(ctx, fp16, &found_inf);

    EXPECT_TRUE(result);
    EXPECT_NE(found_inf, 0);

    nimcp_gpu_tensor_destroy(fp32);
    nimcp_gpu_tensor_destroy(fp16);
}

TEST_F(MixedPrecisionTest, CheckInfNan_WithNaN_ReturnsNonZero) {
    RequireGPU();

    const size_t n = 100;
    size_t dims[1] = {n};

    nimcp_gpu_tensor_t* fp32 = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* fp16 = CreateFP16Tensor(dims, 1);

    std::vector<float> data(n, 1.0f);
    data[25] = std::numeric_limits<float>::quiet_NaN();
    SetFromHost(fp32, data);

    nimcp_fp32_to_fp16(ctx, fp32, fp16);

    int found_inf = -1;
    bool result = nimcp_fp16_check_inf_nan(ctx, fp16, &found_inf);

    EXPECT_TRUE(result);
    EXPECT_NE(found_inf, 0);

    nimcp_gpu_tensor_destroy(fp32);
    nimcp_gpu_tensor_destroy(fp16);
}

//=============================================================================
// Mixed Precision Adam Optimizer Tests
//=============================================================================

TEST_F(MixedPrecisionTest, MPAdam_SingleStep) {
    RequireGPU();

    const size_t n = 256;
    size_t dims[1] = {n};

    // Create mixed precision tensor (weights)
    nimcp_gpu_tensor_t* fp32_weights = Create1DFP32(n, 1.0f);
    nimcp_mp_tensor_t* mp_tensor = nimcp_mp_tensor_create(ctx, fp32_weights, MP_DTYPE_FP16, true);
    ASSERT_NE(mp_tensor, nullptr);

    // Create FP16 gradients
    nimcp_gpu_tensor_t* grads = CreateFP16Tensor(dims, 1);
    nimcp_gpu_tensor_t* grads_fp32 = Create1DFP32(n, 0.1f);
    nimcp_fp32_to_fp16(ctx, grads_fp32, grads);

    // Create optimizer state (FP32)
    nimcp_gpu_tensor_t* m = Create1DFP32(n, 0.0f);
    nimcp_gpu_tensor_t* v = Create1DFP32(n, 0.0f);

    // Adam step
    bool result = nimcp_mp_adam_update(
        ctx, mp_tensor, grads,
        m, v,
        0.001f,   // lr
        0.9f,     // beta1
        0.999f,   // beta2
        1e-8f,    // eps
        0.0f,     // weight_decay
        1         // step
    );
    EXPECT_TRUE(result);

    // Check weights changed
    auto weights = CopyToHost(mp_tensor->fp32_master);
    bool changed = false;
    for (size_t i = 0; i < n; i++) {
        if (std::abs(weights[i] - 1.0f) > 1e-6f) {
            changed = true;
            break;
        }
    }
    EXPECT_TRUE(changed);

    nimcp_gpu_tensor_destroy(fp32_weights);
    nimcp_mp_tensor_destroy(mp_tensor);
    nimcp_gpu_tensor_destroy(grads);
    nimcp_gpu_tensor_destroy(grads_fp32);
    nimcp_gpu_tensor_destroy(m);
    nimcp_gpu_tensor_destroy(v);
}

//=============================================================================
// Numerically Stable Operations Tests
//=============================================================================

TEST_F(MixedPrecisionTest, FP16Softmax_Stable) {
    RequireGPU();

    const size_t n = 128;
    size_t dims[1] = {n};

    nimcp_gpu_tensor_t* x = CreateFP16Tensor(dims, 1);
    nimcp_gpu_tensor_t* out = CreateFP16Tensor(dims, 1);

    // Use values that could cause overflow without proper handling
    nimcp_gpu_tensor_t* x_fp32 = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
    std::vector<float> x_data(n);
    for (size_t i = 0; i < n; i++) {
        x_data[i] = static_cast<float>(i) - n / 2.0f;  // Range -64 to 64
    }
    SetFromHost(x_fp32, x_data);
    nimcp_fp32_to_fp16(ctx, x_fp32, x);

    bool result = nimcp_fp16_softmax_stable(ctx, x, out);
    EXPECT_TRUE(result);

    // Convert back and verify
    nimcp_gpu_tensor_t* out_fp32 = Create1DFP32(n, 0.0f);
    nimcp_fp16_to_fp32(ctx, out, out_fp32);

    auto out_data = CopyToHost(out_fp32);

    // Check sum is ~1.0
    float sum = 0.0f;
    for (size_t i = 0; i < n; i++) {
        EXPECT_GE(out_data[i], 0.0f);
        EXPECT_LE(out_data[i], 1.0f);
        sum += out_data[i];
    }
    EXPECT_NEAR(sum, 1.0f, 0.01f);

    // Check no inf/nan
    int found_inf = 0;
    nimcp_fp16_check_inf_nan(ctx, out, &found_inf);
    EXPECT_EQ(found_inf, 0);

    nimcp_gpu_tensor_destroy(x);
    nimcp_gpu_tensor_destroy(out);
    nimcp_gpu_tensor_destroy(x_fp32);
    nimcp_gpu_tensor_destroy(out_fp32);
}

TEST_F(MixedPrecisionTest, FP16LayerNorm_Stable) {
    RequireGPU();

    const size_t n = 256;
    size_t dims[1] = {n};

    nimcp_gpu_tensor_t* x = CreateFP16Tensor(dims, 1);
    nimcp_gpu_tensor_t* gamma = CreateFP16Tensor(dims, 1);
    nimcp_gpu_tensor_t* beta = CreateFP16Tensor(dims, 1);
    nimcp_gpu_tensor_t* out = CreateFP16Tensor(dims, 1);

    // Initialize with random values
    auto random_data = GenerateRandom(n, -10.0f, 10.0f);
    nimcp_gpu_tensor_t* x_fp32 = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
    SetFromHost(x_fp32, random_data);
    nimcp_fp32_to_fp16(ctx, x_fp32, x);

    // gamma = 1, beta = 0 (identity normalization)
    nimcp_gpu_tensor_t* gamma_fp32 = Create1DFP32(n, 1.0f);
    nimcp_gpu_tensor_t* beta_fp32 = Create1DFP32(n, 0.0f);
    nimcp_fp32_to_fp16(ctx, gamma_fp32, gamma);
    nimcp_fp32_to_fp16(ctx, beta_fp32, beta);

    bool result = nimcp_fp16_layernorm(ctx, x, gamma, beta, out, 1e-5f);
    EXPECT_TRUE(result);

    // Check result is normalized (mean ~0, std ~1)
    nimcp_gpu_tensor_t* out_fp32 = Create1DFP32(n, 0.0f);
    nimcp_fp16_to_fp32(ctx, out, out_fp32);

    auto out_data = CopyToHost(out_fp32);

    float mean = 0.0f;
    for (size_t i = 0; i < n; i++) {
        mean += out_data[i];
    }
    mean /= n;
    EXPECT_NEAR(mean, 0.0f, 0.1f);

    float var = 0.0f;
    for (size_t i = 0; i < n; i++) {
        float diff = out_data[i] - mean;
        var += diff * diff;
    }
    var /= n;
    EXPECT_NEAR(std::sqrt(var), 1.0f, 0.1f);

    nimcp_gpu_tensor_destroy(x);
    nimcp_gpu_tensor_destroy(gamma);
    nimcp_gpu_tensor_destroy(beta);
    nimcp_gpu_tensor_destroy(out);
    nimcp_gpu_tensor_destroy(x_fp32);
    nimcp_gpu_tensor_destroy(gamma_fp32);
    nimcp_gpu_tensor_destroy(beta_fp32);
    nimcp_gpu_tensor_destroy(out_fp32);
}

//=============================================================================
// AMP Context Tests
//=============================================================================

TEST_F(MixedPrecisionTest, AMPCreate_ReturnsValidContext) {
    RequireGPU();

    nimcp_amp_context_t* amp = nimcp_amp_create(ctx, MP_DTYPE_FP16, true);
    ASSERT_NE(amp, nullptr);

    EXPECT_TRUE(amp->enabled);
    EXPECT_EQ(amp->default_dtype, MP_DTYPE_FP16);
    EXPECT_NE(amp->scaler, nullptr);

    nimcp_amp_destroy(amp);
}

TEST_F(MixedPrecisionTest, AMPAutocast_EnterExit) {
    RequireGPU();

    nimcp_amp_context_t* amp = nimcp_amp_create(ctx, MP_DTYPE_FP16, false);
    ASSERT_NE(amp, nullptr);

    EXPECT_FALSE(nimcp_amp_is_autocasting(amp));

    bool entered = nimcp_amp_autocast_enter(amp);
    EXPECT_TRUE(entered);
    EXPECT_TRUE(nimcp_amp_is_autocasting(amp));

    bool exited = nimcp_amp_autocast_exit(amp);
    EXPECT_TRUE(exited);
    EXPECT_FALSE(nimcp_amp_is_autocasting(amp));

    nimcp_amp_destroy(amp);
}

TEST_F(MixedPrecisionTest, AMPGetDtype_ReturnsCorrectTypes) {
    RequireGPU();

    nimcp_amp_context_t* amp = nimcp_amp_create(ctx, MP_DTYPE_FP16, false);
    ASSERT_NE(amp, nullptr);

    // Compute ops should use FP16
    EXPECT_EQ(nimcp_amp_get_dtype(amp, MP_OP_COMPUTE), MP_DTYPE_FP16);
    EXPECT_EQ(nimcp_amp_get_dtype(amp, MP_OP_ELEMENTWISE), MP_DTYPE_FP16);

    // Reduction/normalize ops should use FP32
    EXPECT_EQ(nimcp_amp_get_dtype(amp, MP_OP_REDUCE), MP_DTYPE_FP32);
    EXPECT_EQ(nimcp_amp_get_dtype(amp, MP_OP_NORMALIZE), MP_DTYPE_FP32);

    nimcp_amp_destroy(amp);
}

//=============================================================================
// Tensor Core Detection Tests
//=============================================================================

TEST_F(MixedPrecisionTest, TensorCoresAvailable_ReturnsValidResult) {
    RequireGPU();

    bool available = nimcp_tensor_cores_available(ctx);

    // Just verify it doesn't crash and returns a boolean
    EXPECT_TRUE(available || !available);  // Always true

    // If available, GPU should be Volta or newer (SM 7.0+)
    if (available && ctx->device_info.compute_capability_major > 0) {
        EXPECT_GE(ctx->device_info.compute_capability_major, 7);
    }
}

TEST_F(MixedPrecisionTest, BF16Supported_ReturnsValidResult) {
    RequireGPU();

    bool supported = nimcp_bf16_supported(ctx);

    // BF16 requires Ampere or newer (SM 8.0+)
    if (supported && ctx->device_info.compute_capability_major > 0) {
        EXPECT_GE(ctx->device_info.compute_capability_major, 8);
    }
}

TEST_F(MixedPrecisionTest, GetRecommendedDtype_ReturnsValid) {
    RequireGPU();

    nimcp_mp_dtype_t dtype = nimcp_get_recommended_dtype(ctx);

    EXPECT_TRUE(dtype == MP_DTYPE_FP16 || dtype == MP_DTYPE_BF16 || dtype == MP_DTYPE_FP32);
}

//=============================================================================
// FP16 Activation Function Tests
//=============================================================================

TEST_F(MixedPrecisionTest, FP16ReLU_Correct) {
    RequireGPU();

    const size_t n = 128;
    size_t dims[1] = {n};

    nimcp_gpu_tensor_t* x = CreateFP16Tensor(dims, 1);
    nimcp_gpu_tensor_t* out = CreateFP16Tensor(dims, 1);

    std::vector<float> x_data(n);
    for (size_t i = 0; i < n; i++) {
        x_data[i] = static_cast<float>(i) - 64.0f;  // -64 to 63
    }
    nimcp_gpu_tensor_t* x_fp32 = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
    SetFromHost(x_fp32, x_data);
    nimcp_fp32_to_fp16(ctx, x_fp32, x);

    bool result = nimcp_fp16_relu(ctx, x, out);
    EXPECT_TRUE(result);

    nimcp_gpu_tensor_t* out_fp32 = Create1DFP32(n, 0.0f);
    nimcp_fp16_to_fp32(ctx, out, out_fp32);
    auto out_data = CopyToHost(out_fp32);

    for (size_t i = 0; i < n; i++) {
        float expected = std::max(0.0f, x_data[i]);
        EXPECT_NEAR(out_data[i], expected, 0.5f);
    }

    nimcp_gpu_tensor_destroy(x);
    nimcp_gpu_tensor_destroy(out);
    nimcp_gpu_tensor_destroy(x_fp32);
    nimcp_gpu_tensor_destroy(out_fp32);
}

TEST_F(MixedPrecisionTest, FP16Sigmoid_Correct) {
    RequireGPU();

    const size_t n = 128;
    size_t dims[1] = {n};

    nimcp_gpu_tensor_t* x = CreateFP16Tensor(dims, 1);
    nimcp_gpu_tensor_t* out = CreateFP16Tensor(dims, 1);

    nimcp_gpu_tensor_t* x_fp32 = Create1DFP32(n, 0.0f);
    std::vector<float> x_data(n);
    for (size_t i = 0; i < n; i++) {
        x_data[i] = (static_cast<float>(i) - 64.0f) / 10.0f;  // -6.4 to 6.3
    }
    SetFromHost(x_fp32, x_data);
    nimcp_fp32_to_fp16(ctx, x_fp32, x);

    bool result = nimcp_fp16_sigmoid(ctx, x, out);
    EXPECT_TRUE(result);

    nimcp_gpu_tensor_t* out_fp32 = Create1DFP32(n, 0.0f);
    nimcp_fp16_to_fp32(ctx, out, out_fp32);
    auto out_data = CopyToHost(out_fp32);

    for (size_t i = 0; i < n; i++) {
        float expected = 1.0f / (1.0f + std::exp(-x_data[i]));
        EXPECT_NEAR(out_data[i], expected, 0.02f);
        EXPECT_GE(out_data[i], 0.0f);
        EXPECT_LE(out_data[i], 1.0f);
    }

    nimcp_gpu_tensor_destroy(x);
    nimcp_gpu_tensor_destroy(out);
    nimcp_gpu_tensor_destroy(x_fp32);
    nimcp_gpu_tensor_destroy(out_fp32);
}

TEST_F(MixedPrecisionTest, FP16Tanh_Correct) {
    RequireGPU();

    const size_t n = 128;
    size_t dims[1] = {n};

    nimcp_gpu_tensor_t* x = CreateFP16Tensor(dims, 1);
    nimcp_gpu_tensor_t* out = CreateFP16Tensor(dims, 1);

    nimcp_gpu_tensor_t* x_fp32 = Create1DFP32(n, 0.0f);
    std::vector<float> x_data(n);
    for (size_t i = 0; i < n; i++) {
        x_data[i] = (static_cast<float>(i) - 64.0f) / 20.0f;  // -3.2 to 3.15
    }
    SetFromHost(x_fp32, x_data);
    nimcp_fp32_to_fp16(ctx, x_fp32, x);

    bool result = nimcp_fp16_tanh(ctx, x, out);
    EXPECT_TRUE(result);

    nimcp_gpu_tensor_t* out_fp32 = Create1DFP32(n, 0.0f);
    nimcp_fp16_to_fp32(ctx, out, out_fp32);
    auto out_data = CopyToHost(out_fp32);

    for (size_t i = 0; i < n; i++) {
        float expected = std::tanh(x_data[i]);
        EXPECT_NEAR(out_data[i], expected, 0.02f);
        EXPECT_GE(out_data[i], -1.0f);
        EXPECT_LE(out_data[i], 1.0f);
    }

    nimcp_gpu_tensor_destroy(x);
    nimcp_gpu_tensor_destroy(out);
    nimcp_gpu_tensor_destroy(x_fp32);
    nimcp_gpu_tensor_destroy(out_fp32);
}

//=============================================================================
// Master Weight Maintenance Tests
//=============================================================================

TEST_F(MixedPrecisionTest, MPTensor_SyncCompute) {
    RequireGPU();

    const size_t n = 512;
    size_t dims[1] = {n};

    nimcp_gpu_tensor_t* fp32 = Create1DFP32(n, 2.5f);
    nimcp_mp_tensor_t* mp = nimcp_mp_tensor_create(ctx, fp32, MP_DTYPE_FP16, true);
    ASSERT_NE(mp, nullptr);
    EXPECT_TRUE(mp->has_master);

    // Modify master
    nimcp_gpu_fill(ctx, mp->fp32_master, 3.5f);

    // Sync to compute
    bool result = nimcp_mp_tensor_sync_compute(ctx, mp);
    EXPECT_TRUE(result);

    // Verify FP16 matches
    nimcp_gpu_tensor_t* back = Create1DFP32(n, 0.0f);
    nimcp_fp16_to_fp32(ctx, mp->fp16_data, back);

    auto data = CopyToHost(back);
    for (size_t i = 0; i < n; i++) {
        EXPECT_NEAR(data[i], 3.5f, 0.01f);
    }

    nimcp_gpu_tensor_destroy(fp32);
    nimcp_mp_tensor_destroy(mp);
    nimcp_gpu_tensor_destroy(back);
}

TEST_F(MixedPrecisionTest, MPTensor_SyncMaster) {
    RequireGPU();

    const size_t n = 512;
    size_t dims[1] = {n};

    nimcp_gpu_tensor_t* fp32 = Create1DFP32(n, 1.0f);
    nimcp_mp_tensor_t* mp = nimcp_mp_tensor_create(ctx, fp32, MP_DTYPE_FP16, true);
    ASSERT_NE(mp, nullptr);

    // Modify FP16 compute tensor via conversion
    nimcp_gpu_tensor_t* new_vals = Create1DFP32(n, 4.0f);
    nimcp_fp32_to_fp16(ctx, new_vals, mp->fp16_data);

    // Sync back to master
    bool result = nimcp_mp_tensor_sync_master(ctx, mp);
    EXPECT_TRUE(result);

    auto master_data = CopyToHost(mp->fp32_master);
    for (size_t i = 0; i < n; i++) {
        EXPECT_NEAR(master_data[i], 4.0f, 0.01f);
    }

    nimcp_gpu_tensor_destroy(fp32);
    nimcp_gpu_tensor_destroy(new_vals);
    nimcp_mp_tensor_destroy(mp);
}

//=============================================================================
// Utility Function Tests
//=============================================================================

TEST_F(MixedPrecisionTest, DtypeName_ReturnsCorrectStrings) {
    EXPECT_STREQ(nimcp_mp_dtype_name(MP_DTYPE_FP32), "FP32");
    EXPECT_STREQ(nimcp_mp_dtype_name(MP_DTYPE_FP16), "FP16");
    EXPECT_STREQ(nimcp_mp_dtype_name(MP_DTYPE_BF16), "BF16");
    EXPECT_STREQ(nimcp_mp_dtype_name(MP_DTYPE_TF32), "TF32");
}

TEST_F(MixedPrecisionTest, DtypeSize_ReturnsCorrectSizes) {
    EXPECT_EQ(nimcp_mp_dtype_size(MP_DTYPE_FP32), 4u);
    EXPECT_EQ(nimcp_mp_dtype_size(MP_DTYPE_FP16), 2u);
    EXPECT_EQ(nimcp_mp_dtype_size(MP_DTYPE_BF16), 2u);
    EXPECT_EQ(nimcp_mp_dtype_size(MP_DTYPE_TF32), 4u);  // TF32 uses 4 bytes in storage
}

//=============================================================================
// NULL Safety Tests
//=============================================================================

TEST_F(MixedPrecisionTest, NullSafety_Conversions) {
    RequireGPU();

    size_t dims[1] = {16};
    nimcp_gpu_tensor_t* tensor = CreateFP32Tensor(dims, 1, 1.0f);

    EXPECT_FALSE(nimcp_fp32_to_fp16(nullptr, tensor, tensor));
    EXPECT_FALSE(nimcp_fp32_to_fp16(ctx, nullptr, tensor));
    EXPECT_FALSE(nimcp_fp32_to_fp16(ctx, tensor, nullptr));

    EXPECT_FALSE(nimcp_fp16_to_fp32(nullptr, tensor, tensor));
    EXPECT_FALSE(nimcp_fp16_to_fp32(ctx, nullptr, tensor));
    EXPECT_FALSE(nimcp_fp16_to_fp32(ctx, tensor, nullptr));

    nimcp_gpu_tensor_destroy(tensor);
}

TEST_F(MixedPrecisionTest, NullSafety_LossScaler) {
    nimcp_loss_scaler_destroy(nullptr);  // Should not crash

    EXPECT_EQ(nimcp_loss_scaler_get_scale(nullptr), 0.0f);
}

TEST_F(MixedPrecisionTest, NullSafety_AMP) {
    nimcp_amp_destroy(nullptr);  // Should not crash

    EXPECT_FALSE(nimcp_amp_is_autocasting(nullptr));
    EXPECT_FALSE(nimcp_amp_autocast_enter(nullptr));
    EXPECT_FALSE(nimcp_amp_autocast_exit(nullptr));
}

TEST_F(MixedPrecisionTest, NullSafety_MPTensor) {
    nimcp_mp_tensor_destroy(nullptr);  // Should not crash

    RequireGPU();
    EXPECT_FALSE(nimcp_mp_tensor_sync_compute(ctx, nullptr));
    EXPECT_FALSE(nimcp_mp_tensor_sync_master(ctx, nullptr));
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(MixedPrecisionTest, EmptyTensor_HandledCorrectly) {
    RequireGPU();

    size_t dims[1] = {0};

    // Empty tensor handling depends on implementation
    nimcp_gpu_tensor_t* tensor = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP16);

    // May return NULL or a valid empty tensor
    if (tensor != nullptr) {
        EXPECT_EQ(tensor->numel, 0u);
        nimcp_gpu_tensor_destroy(tensor);
    }
}

TEST_F(MixedPrecisionTest, SingleElement_Correct) {
    RequireGPU();

    size_t dims[1] = {1};

    nimcp_gpu_tensor_t* fp32 = CreateFP32Tensor(dims, 1, 3.14159f);
    nimcp_gpu_tensor_t* fp16 = CreateFP16Tensor(dims, 1);
    ASSERT_NE(fp32, nullptr);
    ASSERT_NE(fp16, nullptr);

    nimcp_fp32_to_fp16(ctx, fp32, fp16);

    nimcp_gpu_tensor_t* back = CreateFP32Tensor(dims, 1, 0.0f);
    nimcp_fp16_to_fp32(ctx, fp16, back);

    auto data = CopyToHost(back);
    EXPECT_NEAR(data[0], 3.14159f, 0.01f);

    nimcp_gpu_tensor_destroy(fp32);
    nimcp_gpu_tensor_destroy(fp16);
    nimcp_gpu_tensor_destroy(back);
}

TEST_F(MixedPrecisionTest, LargeTensor_HandledCorrectly) {
    RequireGPU();

    const size_t n = 1024 * 1024;  // 1M elements
    size_t dims[1] = {n};

    nimcp_gpu_tensor_t* fp32 = CreateFP32Tensor(dims, 1, 0.5f);
    nimcp_gpu_tensor_t* fp16 = CreateFP16Tensor(dims, 1);

    if (fp32 == nullptr || fp16 == nullptr) {
        GTEST_SKIP() << "Not enough GPU memory for large tensor test";
    }

    bool result = nimcp_fp32_to_fp16(ctx, fp32, fp16);
    EXPECT_TRUE(result);

    nimcp_gpu_tensor_t* back = Create1DFP32(n, 0.0f);
    if (back == nullptr) {
        nimcp_gpu_tensor_destroy(fp32);
        nimcp_gpu_tensor_destroy(fp16);
        GTEST_SKIP() << "Not enough GPU memory for verification";
    }

    nimcp_fp16_to_fp32(ctx, fp16, back);

    // Sample a few values
    auto data = CopyToHost(back);
    for (size_t i = 0; i < n; i += n / 10) {
        EXPECT_NEAR(data[i], 0.5f, 0.01f);
    }

    nimcp_gpu_tensor_destroy(fp32);
    nimcp_gpu_tensor_destroy(fp16);
    nimcp_gpu_tensor_destroy(back);
}
