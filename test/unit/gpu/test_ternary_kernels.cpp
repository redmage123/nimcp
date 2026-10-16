/**
 * @file test_ternary_kernels.cpp
 * @brief Unit tests for GPU ternary tensor kernels
 *
 * Tests ternary tensor operations including:
 * - Tensor creation and lifecycle
 * - 2-bit packing/unpacking
 * - Float to ternary quantization
 * - Ternary GEMV and GEMM (no-multiply operations)
 * - Element-wise operations (mul, gate, mask)
 * - Sparse ternary CSR format
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <cstring>
#include <random>

// Headers have their own extern "C" guards
#include "gpu/ternary/nimcp_ternary_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class TernaryKernelTest : public ::testing::Test {
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

    // Helper to create a float tensor filled with random values
    nimcp_gpu_tensor_t* CreateRandomTensor(size_t rows, size_t cols, float min_val = -1.0f, float max_val = 1.0f) {
        if (!ctx) return nullptr;

        size_t dims[2] = {rows, cols};
        nimcp_gpu_tensor_t* tensor = nimcp_gpu_tensor_create(ctx, dims, 2, NIMCP_GPU_PRECISION_FP32);
        if (!tensor) return nullptr;

        // Generate random data on host
        std::vector<float> host_data(rows * cols);
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dist(min_val, max_val);

        for (auto& v : host_data) {
            v = dist(gen);
        }

        // Copy to device
        cudaMemcpy(tensor->data, host_data.data(), rows * cols * sizeof(float), cudaMemcpyHostToDevice);

        return tensor;
    }

    // Helper to create a 1D float vector
    nimcp_gpu_tensor_t* CreateVector(size_t n, float value = 1.0f) {
        if (!ctx) return nullptr;

        size_t dims[1] = {n};
        nimcp_gpu_tensor_t* tensor = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
        if (!tensor) return nullptr;

        std::vector<float> host_data(n, value);
        cudaMemcpy(tensor->data, host_data.data(), n * sizeof(float), cudaMemcpyHostToDevice);

        return tensor;
    }

    // Helper to copy tensor to host
    std::vector<float> CopyToHost(nimcp_gpu_tensor_t* tensor) {
        std::vector<float> host_data(tensor->numel);
        cudaMemcpy(host_data.data(), tensor->data, tensor->numel * sizeof(float), cudaMemcpyDeviceToHost);
        return host_data;
    }

    // Helper to copy ternary tensor to host
    std::vector<int8_t> CopyTernaryToHost(nimcp_ternary_tensor_t* tensor) {
        std::vector<int8_t> host_data(tensor->numel);
        nimcp_ternary_to_host(tensor, host_data.data());
        return host_data;
    }
};

//=============================================================================
// Tensor Lifecycle Tests
//=============================================================================

TEST_F(TernaryKernelTest, CreateUnpackedTensor) {
    RequireGPU();

    int64_t dims[] = {16, 32};
    nimcp_ternary_tensor_t* tensor = nimcp_ternary_tensor_create(ctx, dims, 2, TERNARY_PACK_NONE);

    ASSERT_NE(tensor, nullptr);
    EXPECT_TRUE(nimcp_ternary_tensor_is_valid(tensor));
    EXPECT_EQ(tensor->numel, 16 * 32);
    EXPECT_EQ(tensor->packed_size, (size_t)(16 * 32));  // 1 byte per trit
    EXPECT_EQ(tensor->pack_mode, TERNARY_PACK_NONE);
    EXPECT_EQ(tensor->rank, 2);

    nimcp_ternary_tensor_destroy(tensor);
}

TEST_F(TernaryKernelTest, CreatePackedTensor2Bit) {
    RequireGPU();

    int64_t dims[] = {64, 128};
    nimcp_ternary_tensor_t* tensor = nimcp_ternary_tensor_create(ctx, dims, 2, TERNARY_PACK_2BIT);

    ASSERT_NE(tensor, nullptr);
    EXPECT_TRUE(nimcp_ternary_tensor_is_valid(tensor));
    EXPECT_EQ(tensor->numel, 64 * 128);
    EXPECT_EQ(tensor->packed_size, (size_t)((64 * 128 + 3) / 4));  // 4 trits per byte
    EXPECT_EQ(tensor->pack_mode, TERNARY_PACK_2BIT);

    // Compression ratio should be ~4x
    float ratio = nimcp_ternary_compression_ratio(tensor);
    EXPECT_GT(ratio, 3.9f);

    nimcp_ternary_tensor_destroy(tensor);
}

TEST_F(TernaryKernelTest, CloneTensor) {
    RequireGPU();

    int64_t dims[] = {8, 16};
    nimcp_ternary_tensor_t* original = nimcp_ternary_tensor_create(ctx, dims, 2, TERNARY_PACK_NONE);
    ASSERT_NE(original, nullptr);

    nimcp_ternary_tensor_t* clone = nimcp_ternary_tensor_clone(ctx, original);
    ASSERT_NE(clone, nullptr);

    EXPECT_EQ(clone->numel, original->numel);
    EXPECT_EQ(clone->pack_mode, original->pack_mode);

    nimcp_ternary_tensor_destroy(original);
    nimcp_ternary_tensor_destroy(clone);
}

//=============================================================================
// Quantization Tests
//=============================================================================

TEST_F(TernaryKernelTest, QuantizeFloatToTernary) {
    RequireGPU();

    // Create float tensor with known values
    size_t dims[2] = {4, 4};
    nimcp_gpu_tensor_t* float_tensor = nimcp_gpu_tensor_create(ctx, dims, 2, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(float_tensor, nullptr);

    // Values: {-0.5, -0.2, 0.1, 0.6} repeated
    std::vector<float> host_data = {
        -0.5f, -0.2f, 0.1f, 0.6f,
        -0.8f, -0.1f, 0.0f, 0.9f,
        -0.4f, -0.3f, 0.2f, 0.5f,
        -0.7f,  0.05f, 0.25f, 0.35f
    };
    cudaMemcpy(float_tensor->data, host_data.data(), 16 * sizeof(float), cudaMemcpyHostToDevice);

    // Quantize with threshold 0.3
    nimcp_ternary_quant_config_t config = nimcp_ternary_quant_config_default();
    config.threshold = 0.3f;

    nimcp_ternary_tensor_t* ternary = nimcp_ternary_from_float(ctx, float_tensor, &config, TERNARY_PACK_NONE);
    ASSERT_NE(ternary, nullptr);

    // Verify quantization
    std::vector<int8_t> trits = CopyTernaryToHost(ternary);

    // Expected: -0.5 -> -1, -0.2 -> 0, 0.1 -> 0, 0.6 -> 1, etc.
    EXPECT_EQ(trits[0], -1);  // -0.5 < -0.3
    EXPECT_EQ(trits[1], 0);   // -0.2 in [-0.3, 0.3]
    EXPECT_EQ(trits[2], 0);   // 0.1 in [-0.3, 0.3]
    EXPECT_EQ(trits[3], 1);   // 0.6 > 0.3

    nimcp_gpu_tensor_destroy(float_tensor);
    nimcp_ternary_tensor_destroy(ternary);
}

TEST_F(TernaryKernelTest, TernaryToFloat) {
    RequireGPU();

    // Create ternary tensor with known values
    int64_t dims[] = {8};
    nimcp_ternary_tensor_t* ternary = nimcp_ternary_tensor_create(ctx, dims, 1, TERNARY_PACK_NONE);
    ASSERT_NE(ternary, nullptr);

    std::vector<int8_t> trits = {-1, 0, 1, 1, 0, -1, 1, 0};
    cudaMemcpy(ternary->data, trits.data(), 8, cudaMemcpyHostToDevice);

    // Convert back to float
    nimcp_gpu_tensor_t* float_tensor = nimcp_ternary_to_float(ctx, ternary);
    ASSERT_NE(float_tensor, nullptr);

    std::vector<float> floats = CopyToHost(float_tensor);

    EXPECT_FLOAT_EQ(floats[0], -1.0f);
    EXPECT_FLOAT_EQ(floats[1], 0.0f);
    EXPECT_FLOAT_EQ(floats[2], 1.0f);
    EXPECT_FLOAT_EQ(floats[3], 1.0f);
    EXPECT_FLOAT_EQ(floats[4], 0.0f);
    EXPECT_FLOAT_EQ(floats[5], -1.0f);

    nimcp_ternary_tensor_destroy(ternary);
    nimcp_gpu_tensor_destroy(float_tensor);
}

//=============================================================================
// Packing/Unpacking Tests
//=============================================================================

TEST_F(TernaryKernelTest, Pack2BitUnpack) {
    RequireGPU();

    // Create unpacked tensor
    int64_t dims[] = {32};
    nimcp_ternary_tensor_t* original = nimcp_ternary_tensor_create(ctx, dims, 1, TERNARY_PACK_NONE);
    ASSERT_NE(original, nullptr);

    // Fill with pattern: -1, 0, 1, 0, -1, 0, 1, 0, ...
    std::vector<int8_t> pattern(32);
    for (int i = 0; i < 32; i++) {
        pattern[i] = (i % 3) - 1;  // -1, 0, 1, -1, 0, 1, ...
    }
    cudaMemcpy(original->data, pattern.data(), 32, cudaMemcpyHostToDevice);

    // Pack
    nimcp_ternary_tensor_t* packed = nimcp_ternary_pack_2bit(ctx, original);
    ASSERT_NE(packed, nullptr);
    EXPECT_EQ(packed->pack_mode, TERNARY_PACK_2BIT);
    EXPECT_EQ(packed->packed_size, (size_t)8);  // 32 trits / 4 = 8 bytes

    // Unpack
    nimcp_ternary_tensor_t* unpacked = nimcp_ternary_unpack_2bit(ctx, packed);
    ASSERT_NE(unpacked, nullptr);
    EXPECT_EQ(unpacked->pack_mode, TERNARY_PACK_NONE);

    // Verify roundtrip
    std::vector<int8_t> result = CopyTernaryToHost(unpacked);
    for (int i = 0; i < 32; i++) {
        EXPECT_EQ(result[i], pattern[i]) << "Mismatch at index " << i;
    }

    nimcp_ternary_tensor_destroy(original);
    nimcp_ternary_tensor_destroy(packed);
    nimcp_ternary_tensor_destroy(unpacked);
}

//=============================================================================
// GEMV Tests (Matrix-Vector Multiply)
//=============================================================================

TEST_F(TernaryKernelTest, TernaryGEMV) {
    RequireGPU();

    // Create ternary matrix A [4, 8]
    int64_t a_dims[] = {4, 8};
    nimcp_ternary_tensor_t* A = nimcp_ternary_tensor_create(ctx, a_dims, 2, TERNARY_PACK_NONE);
    ASSERT_NE(A, nullptr);

    // Fill A with pattern: row 0 all +1, row 1 all -1, row 2 all 0, row 3 alternating
    std::vector<int8_t> a_data = {
        1, 1, 1, 1, 1, 1, 1, 1,       // Row 0: all +1
        -1, -1, -1, -1, -1, -1, -1, -1, // Row 1: all -1
        0, 0, 0, 0, 0, 0, 0, 0,       // Row 2: all 0
        1, -1, 1, -1, 1, -1, 1, -1    // Row 3: alternating
    };
    cudaMemcpy(A->data, a_data.data(), 32, cudaMemcpyHostToDevice);

    // Create float vector x [8] with all 1s
    nimcp_gpu_tensor_t* x = CreateVector(8, 1.0f);
    ASSERT_NE(x, nullptr);

    // Compute y = A * x
    nimcp_gpu_tensor_t* y = nimcp_ternary_gemv(ctx, A, x, nullptr);
    ASSERT_NE(y, nullptr);

    std::vector<float> result = CopyToHost(y);

    // Expected results:
    // Row 0: sum of 8 ones = 8
    // Row 1: sum of 8 negatives = -8
    // Row 2: sum of 8 zeros = 0
    // Row 3: 4*(+1) + 4*(-1) = 0
    EXPECT_FLOAT_EQ(result[0], 8.0f);
    EXPECT_FLOAT_EQ(result[1], -8.0f);
    EXPECT_FLOAT_EQ(result[2], 0.0f);
    EXPECT_FLOAT_EQ(result[3], 0.0f);

    nimcp_ternary_tensor_destroy(A);
    nimcp_gpu_tensor_destroy(x);
    nimcp_gpu_tensor_destroy(y);
}

TEST_F(TernaryKernelTest, TernaryGEMVWithWeightedInput) {
    RequireGPU();

    // Create ternary matrix A [2, 4]
    int64_t a_dims[] = {2, 4};
    nimcp_ternary_tensor_t* A = nimcp_ternary_tensor_create(ctx, a_dims, 2, TERNARY_PACK_NONE);
    ASSERT_NE(A, nullptr);

    std::vector<int8_t> a_data = {
        1, -1, 0, 1,   // Row 0
        -1, 1, 1, -1   // Row 1
    };
    cudaMemcpy(A->data, a_data.data(), 8, cudaMemcpyHostToDevice);

    // Create x = [1, 2, 3, 4]
    size_t x_dims[] = {4};
    nimcp_gpu_tensor_t* x = nimcp_gpu_tensor_create(ctx, x_dims, 1, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(x, nullptr);

    std::vector<float> x_data = {1.0f, 2.0f, 3.0f, 4.0f};
    cudaMemcpy(x->data, x_data.data(), 4 * sizeof(float), cudaMemcpyHostToDevice);

    // Compute y = A * x
    nimcp_gpu_tensor_t* y = nimcp_ternary_gemv(ctx, A, x, nullptr);
    ASSERT_NE(y, nullptr);

    std::vector<float> result = CopyToHost(y);

    // Expected:
    // Row 0: 1*1 + (-1)*2 + 0*3 + 1*4 = 1 - 2 + 0 + 4 = 3
    // Row 1: (-1)*1 + 1*2 + 1*3 + (-1)*4 = -1 + 2 + 3 - 4 = 0
    EXPECT_FLOAT_EQ(result[0], 3.0f);
    EXPECT_FLOAT_EQ(result[1], 0.0f);

    nimcp_ternary_tensor_destroy(A);
    nimcp_gpu_tensor_destroy(x);
    nimcp_gpu_tensor_destroy(y);
}

//=============================================================================
// GEMM Tests (Matrix-Matrix Multiply)
//=============================================================================

TEST_F(TernaryKernelTest, TernaryGEMM) {
    RequireGPU();

    // A: ternary [4, 8], B: float [8, 3] -> C: float [4, 3]
    int64_t a_dims[] = {4, 8};
    nimcp_ternary_tensor_t* A = nimcp_ternary_tensor_create(ctx, a_dims, 2, TERNARY_PACK_NONE);
    ASSERT_NE(A, nullptr);

    // A is identity-like: first 4 rows have +1 at diagonal positions
    std::vector<int8_t> a_data(32, 0);
    a_data[0] = 1;   // A[0,0] = 1
    a_data[9] = 1;   // A[1,1] = 1
    a_data[18] = 1;  // A[2,2] = 1
    a_data[27] = 1;  // A[3,3] = 1
    cudaMemcpy(A->data, a_data.data(), 32, cudaMemcpyHostToDevice);

    // B: [8, 3] filled with incremental values
    size_t b_dims[] = {8, 3};
    nimcp_gpu_tensor_t* B = nimcp_gpu_tensor_create(ctx, b_dims, 2, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(B, nullptr);

    std::vector<float> b_data(24);
    for (int i = 0; i < 24; i++) {
        b_data[i] = (float)(i + 1);
    }
    cudaMemcpy(B->data, b_data.data(), 24 * sizeof(float), cudaMemcpyHostToDevice);

    // Compute C = A * B
    nimcp_ternary_gemm_config_t config = nimcp_ternary_gemm_config_default();
    nimcp_gpu_tensor_t* C = nimcp_ternary_gemm(ctx, A, B, nullptr, &config);
    ASSERT_NE(C, nullptr);

    std::vector<float> result = CopyToHost(C);

    // C[i,:] should be B[i,:] since A selects rows
    // C[0,:] = B[0,:] = [1, 2, 3]
    EXPECT_FLOAT_EQ(result[0], 1.0f);
    EXPECT_FLOAT_EQ(result[1], 2.0f);
    EXPECT_FLOAT_EQ(result[2], 3.0f);

    // C[1,:] = B[1,:] = [4, 5, 6]
    EXPECT_FLOAT_EQ(result[3], 4.0f);
    EXPECT_FLOAT_EQ(result[4], 5.0f);
    EXPECT_FLOAT_EQ(result[5], 6.0f);

    nimcp_ternary_tensor_destroy(A);
    nimcp_gpu_tensor_destroy(B);
    nimcp_gpu_tensor_destroy(C);
}

//=============================================================================
// Element-wise Operation Tests
//=============================================================================

TEST_F(TernaryKernelTest, TernaryMul) {
    RequireGPU();

    // Create two ternary tensors
    int64_t dims[] = {16};
    nimcp_ternary_tensor_t* A = nimcp_ternary_tensor_create(ctx, dims, 1, TERNARY_PACK_NONE);
    nimcp_ternary_tensor_t* B = nimcp_ternary_tensor_create(ctx, dims, 1, TERNARY_PACK_NONE);
    nimcp_ternary_tensor_t* C = nimcp_ternary_tensor_create(ctx, dims, 1, TERNARY_PACK_NONE);
    ASSERT_NE(A, nullptr);
    ASSERT_NE(B, nullptr);
    ASSERT_NE(C, nullptr);

    // A = [1, -1, 0, 1, -1, 0, 1, -1, 0, 1, -1, 0, 1, -1, 0, 1]
    // B = [1,  1, 1,-1, -1,-1, 0,  0, 0, 1, -1, 0, 1, -1, 0, 0]
    std::vector<int8_t> a_data = {1, -1, 0, 1, -1, 0, 1, -1, 0, 1, -1, 0, 1, -1, 0, 1};
    std::vector<int8_t> b_data = {1,  1, 1,-1, -1,-1, 0,  0, 0, 1, -1, 0, 1, -1, 0, 0};
    cudaMemcpy(A->data, a_data.data(), 16, cudaMemcpyHostToDevice);
    cudaMemcpy(B->data, b_data.data(), 16, cudaMemcpyHostToDevice);

    bool success = nimcp_ternary_mul(ctx, A, B, C);
    ASSERT_TRUE(success);

    std::vector<int8_t> result = CopyTernaryToHost(C);

    // Expected: C[i] = A[i] * B[i]
    // 1*1=1, -1*1=-1, 0*1=0, 1*-1=-1, -1*-1=1, 0*-1=0, etc.
    EXPECT_EQ(result[0], 1);   // 1 * 1
    EXPECT_EQ(result[1], -1);  // -1 * 1
    EXPECT_EQ(result[2], 0);   // 0 * 1
    EXPECT_EQ(result[3], -1);  // 1 * -1
    EXPECT_EQ(result[4], 1);   // -1 * -1
    EXPECT_EQ(result[5], 0);   // 0 * -1
    EXPECT_EQ(result[6], 0);   // 1 * 0
    EXPECT_EQ(result[7], 0);   // -1 * 0

    nimcp_ternary_tensor_destroy(A);
    nimcp_ternary_tensor_destroy(B);
    nimcp_ternary_tensor_destroy(C);
}

TEST_F(TernaryKernelTest, TernaryGate) {
    RequireGPU();

    // Gate tensor
    int64_t g_dims[] = {8};
    nimcp_ternary_tensor_t* gate = nimcp_ternary_tensor_create(ctx, g_dims, 1, TERNARY_PACK_NONE);
    ASSERT_NE(gate, nullptr);

    std::vector<int8_t> gate_data = {1, -1, 0, 1, -1, 0, 1, -1};
    cudaMemcpy(gate->data, gate_data.data(), 8, cudaMemcpyHostToDevice);

    // Input tensor
    size_t in_dims[] = {8};
    nimcp_gpu_tensor_t* input = nimcp_gpu_tensor_create(ctx, in_dims, 1, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* output = nimcp_gpu_tensor_create(ctx, in_dims, 1, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(input, nullptr);
    ASSERT_NE(output, nullptr);

    std::vector<float> in_data = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
    cudaMemcpy(input->data, in_data.data(), 8 * sizeof(float), cudaMemcpyHostToDevice);

    bool success = nimcp_ternary_gate(ctx, gate, input, output);
    ASSERT_TRUE(success);

    std::vector<float> result = CopyToHost(output);

    // Expected: gate=1 -> pass, gate=-1 -> negate, gate=0 -> zero
    EXPECT_FLOAT_EQ(result[0], 1.0f);   // 1 * 1.0
    EXPECT_FLOAT_EQ(result[1], -2.0f);  // -1 * 2.0
    EXPECT_FLOAT_EQ(result[2], 0.0f);   // 0 * 3.0
    EXPECT_FLOAT_EQ(result[3], 4.0f);   // 1 * 4.0
    EXPECT_FLOAT_EQ(result[4], -5.0f);  // -1 * 5.0
    EXPECT_FLOAT_EQ(result[5], 0.0f);   // 0 * 6.0
    EXPECT_FLOAT_EQ(result[6], 7.0f);   // 1 * 7.0
    EXPECT_FLOAT_EQ(result[7], -8.0f);  // -1 * 8.0

    nimcp_ternary_tensor_destroy(gate);
    nimcp_gpu_tensor_destroy(input);
    nimcp_gpu_tensor_destroy(output);
}

TEST_F(TernaryKernelTest, TernaryMask) {
    RequireGPU();

    // Mask tensor
    int64_t m_dims[] = {8};
    nimcp_ternary_tensor_t* mask = nimcp_ternary_tensor_create(ctx, m_dims, 1, TERNARY_PACK_NONE);
    ASSERT_NE(mask, nullptr);

    std::vector<int8_t> mask_data = {1, -1, 0, 1, -1, 0, 1, 0};
    cudaMemcpy(mask->data, mask_data.data(), 8, cudaMemcpyHostToDevice);

    // Input tensor
    size_t in_dims[] = {8};
    nimcp_gpu_tensor_t* input = nimcp_gpu_tensor_create(ctx, in_dims, 1, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* output = nimcp_gpu_tensor_create(ctx, in_dims, 1, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(input, nullptr);
    ASSERT_NE(output, nullptr);

    std::vector<float> in_data = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
    cudaMemcpy(input->data, in_data.data(), 8 * sizeof(float), cudaMemcpyHostToDevice);

    bool success = nimcp_ternary_mask(ctx, mask, input, output);
    ASSERT_TRUE(success);

    std::vector<float> result = CopyToHost(output);

    // Expected: mask!=0 -> pass, mask=0 -> zero
    EXPECT_FLOAT_EQ(result[0], 1.0f);  // mask=1, pass
    EXPECT_FLOAT_EQ(result[1], 2.0f);  // mask=-1, pass
    EXPECT_FLOAT_EQ(result[2], 0.0f);  // mask=0, zero
    EXPECT_FLOAT_EQ(result[3], 4.0f);  // mask=1, pass
    EXPECT_FLOAT_EQ(result[4], 5.0f);  // mask=-1, pass
    EXPECT_FLOAT_EQ(result[5], 0.0f);  // mask=0, zero
    EXPECT_FLOAT_EQ(result[6], 7.0f);  // mask=1, pass
    EXPECT_FLOAT_EQ(result[7], 0.0f);  // mask=0, zero

    nimcp_ternary_tensor_destroy(mask);
    nimcp_gpu_tensor_destroy(input);
    nimcp_gpu_tensor_destroy(output);
}

//=============================================================================
// Utility Function Tests
//=============================================================================

TEST_F(TernaryKernelTest, CountNonzero) {
    RequireGPU();

    int64_t dims[] = {16};
    nimcp_ternary_tensor_t* tensor = nimcp_ternary_tensor_create(ctx, dims, 1, TERNARY_PACK_NONE);
    ASSERT_NE(tensor, nullptr);

    // 10 non-zeros, 6 zeros
    std::vector<int8_t> data = {1, -1, 0, 1, -1, 0, 1, -1, 0, 1, -1, 0, 1, -1, 0, 0};
    cudaMemcpy(tensor->data, data.data(), 16, cudaMemcpyHostToDevice);

    int64_t nnz = nimcp_ternary_count_nonzero(tensor);
    EXPECT_EQ(nnz, 10);

    float sparsity = nimcp_ternary_compute_sparsity(tensor);
    EXPECT_NEAR(sparsity, 6.0f / 16.0f, 0.01f);  // 6 zeros / 16 total

    nimcp_ternary_tensor_destroy(tensor);
}

TEST_F(TernaryKernelTest, CompressionRatio) {
    RequireGPU();

    // Unpacked: 1 byte per trit
    int64_t dims[] = {1024};
    nimcp_ternary_tensor_t* unpacked = nimcp_ternary_tensor_create(ctx, dims, 1, TERNARY_PACK_NONE);
    ASSERT_NE(unpacked, nullptr);

    float ratio_unpacked = nimcp_ternary_compression_ratio(unpacked);
    EXPECT_NEAR(ratio_unpacked, 4.0f, 0.01f);  // float32 / int8 = 4x

    // Packed 2-bit: 4 trits per byte
    nimcp_ternary_tensor_t* packed = nimcp_ternary_tensor_create(ctx, dims, 1, TERNARY_PACK_2BIT);
    ASSERT_NE(packed, nullptr);

    float ratio_packed = nimcp_ternary_compression_ratio(packed);
    EXPECT_NEAR(ratio_packed, 16.0f, 0.1f);  // float32 / 2-bit = 16x

    nimcp_ternary_tensor_destroy(unpacked);
    nimcp_ternary_tensor_destroy(packed);
}

//=============================================================================
// Sparse Ternary Tests
//=============================================================================

TEST_F(TernaryKernelTest, SparseFromDense) {
    RequireGPU();

    // Create dense ternary with ~75% zeros
    int64_t dims[] = {4, 8};
    nimcp_ternary_tensor_t* dense = nimcp_ternary_tensor_create(ctx, dims, 2, TERNARY_PACK_NONE);
    ASSERT_NE(dense, nullptr);

    // Sparse pattern: few non-zeros
    std::vector<int8_t> data(32, 0);
    data[0] = 1;   // (0, 0)
    data[3] = -1;  // (0, 3)
    data[8] = 1;   // (1, 0)
    data[15] = -1; // (1, 7)
    data[24] = 1;  // (3, 0)
    cudaMemcpy(dense->data, data.data(), 32, cudaMemcpyHostToDevice);

    nimcp_ternary_sparse_t* sparse = nimcp_ternary_sparse_from_dense(ctx, dense);
    ASSERT_NE(sparse, nullptr);

    EXPECT_EQ(sparse->rows, 4);
    EXPECT_EQ(sparse->cols, 8);
    EXPECT_EQ(sparse->nnz, 5);  // 5 non-zeros
    EXPECT_GT(sparse->sparsity, 0.8f);  // ~84% zeros

    nimcp_ternary_tensor_destroy(dense);
    nimcp_ternary_sparse_destroy(sparse);
}

TEST_F(TernaryKernelTest, SparseGEMV) {
    RequireGPU();

    // Create dense ternary
    int64_t dims[] = {3, 4};
    nimcp_ternary_tensor_t* dense = nimcp_ternary_tensor_create(ctx, dims, 2, TERNARY_PACK_NONE);
    ASSERT_NE(dense, nullptr);

    std::vector<int8_t> data = {
        1, 0, -1, 0,   // Row 0: 1 at col 0, -1 at col 2
        0, 1, 0, -1,   // Row 1: 1 at col 1, -1 at col 3
        -1, 0, 1, 0    // Row 2: -1 at col 0, 1 at col 2
    };
    cudaMemcpy(dense->data, data.data(), 12, cudaMemcpyHostToDevice);

    nimcp_ternary_sparse_t* sparse = nimcp_ternary_sparse_from_dense(ctx, dense);
    ASSERT_NE(sparse, nullptr);

    // x = [1, 2, 3, 4]
    size_t x_dims[] = {4};
    nimcp_gpu_tensor_t* x = nimcp_gpu_tensor_create(ctx, x_dims, 1, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(x, nullptr);

    std::vector<float> x_data = {1.0f, 2.0f, 3.0f, 4.0f};
    cudaMemcpy(x->data, x_data.data(), 4 * sizeof(float), cudaMemcpyHostToDevice);

    nimcp_gpu_tensor_t* y = nimcp_ternary_sparse_gemv(ctx, sparse, x, nullptr);
    ASSERT_NE(y, nullptr);

    std::vector<float> result = CopyToHost(y);

    // Expected:
    // Row 0: 1*1 + (-1)*3 = 1 - 3 = -2
    // Row 1: 1*2 + (-1)*4 = 2 - 4 = -2
    // Row 2: (-1)*1 + 1*3 = -1 + 3 = 2
    EXPECT_FLOAT_EQ(result[0], -2.0f);
    EXPECT_FLOAT_EQ(result[1], -2.0f);
    EXPECT_FLOAT_EQ(result[2], 2.0f);

    nimcp_ternary_tensor_destroy(dense);
    nimcp_ternary_sparse_destroy(sparse);
    nimcp_gpu_tensor_destroy(x);
    nimcp_gpu_tensor_destroy(y);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
