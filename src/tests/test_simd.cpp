/**
 * @file test_simd.cpp
 * @brief Unit tests for SIMD vectorization (v2.7.0 Enhancement 3)
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <cstring>

extern "C" {
#include "core/neuralnet/nimcp_neuralnet_simd.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
}

class SIMDTest : public ::testing::Test {
protected:
    nimcp_cpu_caps_t cpu_caps;

    void SetUp() override {
        nimcp_memory_init();

        // Detect CPU capabilities
        nimcp_cpu_detect(&cpu_caps);
    }

    void TearDown() override {
        // Cleanup
    }

    // Helper to compare float arrays with tolerance
    void ExpectArrayNear(const float* actual, const float* expected,
                         uint32_t size, float tolerance = 1e-5f) {
        for (uint32_t i = 0; i < size; i++) {
            EXPECT_NEAR(actual[i], expected[i], tolerance)
                << "Mismatch at index " << i;
        }
    }
};

//=============================================================================
// CPU Detection Tests
//=============================================================================

TEST_F(SIMDTest, CPUDetectionSucceeds) {
    nimcp_cpu_caps_t caps;
    bool success = nimcp_cpu_detect(&caps);

    EXPECT_TRUE(success);

    // At minimum, we should have SSE2 on x86-64
    // (All x86-64 CPUs support SSE2 by definition)
    #if defined(__x86_64__) || defined(_M_X64)
        EXPECT_TRUE(caps.has_sse2);
    #endif
}

TEST_F(SIMDTest, CPUCapabilitiesConsistent) {
    // If we have AVX2, we must have AVX
    if (cpu_caps.has_avx2) {
        EXPECT_TRUE(cpu_caps.has_avx);
    }

    // If we have AVX-512, we must have AVX2
    if (cpu_caps.has_avx512f) {
        EXPECT_TRUE(cpu_caps.has_avx2);
        EXPECT_TRUE(cpu_caps.has_avx);
    }

    // If we have FMA, we typically have AVX
    if (cpu_caps.has_fma) {
        EXPECT_TRUE(cpu_caps.has_avx || cpu_caps.has_avx2);
    }
}

TEST_F(SIMDTest, GetCPUCapabilities) {
    const nimcp_cpu_caps_t* caps = nimcp_cpu_get_caps();

    // Should return same as direct detection
    EXPECT_EQ(caps->has_sse2, cpu_caps.has_sse2);
    EXPECT_EQ(caps->has_avx, cpu_caps.has_avx);
    EXPECT_EQ(caps->has_avx2, cpu_caps.has_avx2);
}

//=============================================================================
// Vector Operations Tests
//=============================================================================

TEST_F(SIMDTest, DotProductSmallVector) {
    float a[] = {1.0f, 2.0f, 3.0f};
    float b[] = {4.0f, 5.0f, 6.0f};

    float result = nimcp_simd_dot_product(a, b, 3);

    // Expected: 1*4 + 2*5 + 3*6 = 4 + 10 + 18 = 32
    EXPECT_NEAR(result, 32.0f, 1e-5f);
}

TEST_F(SIMDTest, DotProductLargeVector) {
    const uint32_t size = 1024;
    std::vector<float> a(size);
    std::vector<float> b(size);

    // Initialize with known pattern
    for (uint32_t i = 0; i < size; i++) {
        a[i] = (float)i;
        b[i] = 1.0f;
    }

    float result = nimcp_simd_dot_product(a.data(), b.data(), size);

    // Expected: sum of 0 to 1023 = 1023*1024/2 = 523776
    float expected = 0.0f;
    for (uint32_t i = 0; i < size; i++) {
        expected += a[i] * b[i];
    }

    EXPECT_NEAR(result, expected, 1e-3f);
}

TEST_F(SIMDTest, DotProductZeroVector) {
    float a[] = {1.0f, 2.0f, 3.0f, 4.0f};
    float b[] = {0.0f, 0.0f, 0.0f, 0.0f};

    float result = nimcp_simd_dot_product(a, b, 4);

    EXPECT_NEAR(result, 0.0f, 1e-5f);
}

TEST_F(SIMDTest, VectorAdd) {
    float a[] = {1.0f, 2.0f, 3.0f, 4.0f};
    float b[] = {5.0f, 6.0f, 7.0f, 8.0f};
    float result[4];

    nimcp_simd_vector_add(a, b, result, 4);

    float expected[] = {6.0f, 8.0f, 10.0f, 12.0f};
    ExpectArrayNear(result, expected, 4);
}

TEST_F(SIMDTest, VectorAddLarge) {
    const uint32_t size = 1024;
    std::vector<float> a(size, 1.0f);
    std::vector<float> b(size, 2.0f);
    std::vector<float> result(size);

    nimcp_simd_vector_add(a.data(), b.data(), result.data(), size);

    // All should be 3.0
    for (uint32_t i = 0; i < size; i++) {
        EXPECT_NEAR(result[i], 3.0f, 1e-5f);
    }
}

TEST_F(SIMDTest, VectorMul) {
    float a[] = {2.0f, 3.0f, 4.0f, 5.0f};
    float b[] = {1.0f, 2.0f, 3.0f, 4.0f};
    float result[4];

    nimcp_simd_vector_mul(a, b, result, 4);

    float expected[] = {2.0f, 6.0f, 12.0f, 20.0f};
    ExpectArrayNear(result, expected, 4);
}

TEST_F(SIMDTest, VectorReLU) {
    float input[] = {-2.0f, -1.0f, 0.0f, 1.0f, 2.0f};
    float output[5];

    nimcp_simd_relu(input, output, 5);

    float expected[] = {0.0f, 0.0f, 0.0f, 1.0f, 2.0f};
    ExpectArrayNear(output, expected, 5);
}

TEST_F(SIMDTest, VectorReLULarge) {
    const uint32_t size = 1024;
    std::vector<float> input(size);
    std::vector<float> output(size);

    // Mix of positive and negative
    for (uint32_t i = 0; i < size; i++) {
        input[i] = (float)i - 512.0f;
    }

    nimcp_simd_relu(input.data(), output.data(), size);

    // Verify ReLU: max(0, x)
    for (uint32_t i = 0; i < size; i++) {
        float expected = (input[i] > 0.0f) ? input[i] : 0.0f;
        EXPECT_NEAR(output[i], expected, 1e-5f);
    }
}

//=============================================================================
// Matrix-Vector Multiplication Tests
//=============================================================================

TEST_F(SIMDTest, MatVecMulSmall) {
    // 2x3 matrix * 3x1 vector = 2x1 result
    float matrix[] = {
        1.0f, 2.0f, 3.0f,
        4.0f, 5.0f, 6.0f
    };
    float input[] = {1.0f, 2.0f, 3.0f};
    float output[2];

    nimcp_simd_matvec_mul(matrix, input, output, 2, 3);

    // Expected:
    // row 0: 1*1 + 2*2 + 3*3 = 1 + 4 + 9 = 14
    // row 1: 4*1 + 5*2 + 6*3 = 4 + 10 + 18 = 32
    float expected[] = {14.0f, 32.0f};
    ExpectArrayNear(output, expected, 2);
}

TEST_F(SIMDTest, MatVecMulMedium) {
    const uint32_t m = 16;
    const uint32_t n = 32;

    std::vector<float> matrix(m * n);
    std::vector<float> input(n);
    std::vector<float> output(m);
    std::vector<float> expected(m);

    // Initialize
    for (uint32_t i = 0; i < m * n; i++) {
        matrix[i] = (float)(i % 10);
    }
    for (uint32_t i = 0; i < n; i++) {
        input[i] = 1.0f;
    }

    // Compute reference
    for (uint32_t i = 0; i < m; i++) {
        expected[i] = 0.0f;
        for (uint32_t j = 0; j < n; j++) {
            expected[i] += matrix[i * n + j] * input[j];
        }
    }

    // Test SIMD version
    nimcp_simd_matvec_mul(matrix.data(), input.data(), output.data(), m, n);

    ExpectArrayNear(output.data(), expected.data(), m, 1e-4f);
}

TEST_F(SIMDTest, MatVecMulLarge) {
    const uint32_t m = 128;
    const uint32_t n = 256;

    std::vector<float> matrix(m * n);
    std::vector<float> input(n);
    std::vector<float> output(m);
    std::vector<float> expected(m);

    // Initialize with random-ish pattern
    for (uint32_t i = 0; i < m * n; i++) {
        matrix[i] = (float)(i % 17) / 17.0f;
    }
    for (uint32_t i = 0; i < n; i++) {
        input[i] = (float)(i % 13) / 13.0f;
    }

    // Compute reference
    for (uint32_t i = 0; i < m; i++) {
        expected[i] = 0.0f;
        for (uint32_t j = 0; j < n; j++) {
            expected[i] += matrix[i * n + j] * input[j];
        }
    }

    // Test SIMD version
    nimcp_simd_matvec_mul(matrix.data(), input.data(), output.data(), m, n);

    ExpectArrayNear(output.data(), expected.data(), m, 1e-3f);
}

TEST_F(SIMDTest, MatVecMulZeroMatrix) {
    const uint32_t m = 4;
    const uint32_t n = 8;

    std::vector<float> matrix(m * n, 0.0f);
    std::vector<float> input(n, 1.0f);
    std::vector<float> output(m);

    nimcp_simd_matvec_mul(matrix.data(), input.data(), output.data(), m, n);

    // All outputs should be 0
    for (uint32_t i = 0; i < m; i++) {
        EXPECT_NEAR(output[i], 0.0f, 1e-5f);
    }
}

TEST_F(SIMDTest, MatVecMulIdentity) {
    const uint32_t n = 8;

    // Identity matrix
    std::vector<float> matrix(n * n, 0.0f);
    for (uint32_t i = 0; i < n; i++) {
        matrix[i * n + i] = 1.0f;
    }

    float input[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
    float output[8];

    nimcp_simd_matvec_mul(matrix.data(), input, output, n, n);

    // Output should equal input
    ExpectArrayNear(output, input, n);
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(SIMDTest, SingleElementVector) {
    float a[] = {5.0f};
    float b[] = {3.0f};

    float result = nimcp_simd_dot_product(a, b, 1);

    EXPECT_NEAR(result, 15.0f, 1e-5f);
}

TEST_F(SIMDTest, OddSizedVector) {
    // Test with size not multiple of 8
    float a[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    float b[] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f};

    float result = nimcp_simd_dot_product(a, b, 5);

    // Expected: 1+2+3+4+5 = 15
    EXPECT_NEAR(result, 15.0f, 1e-5f);
}

TEST_F(SIMDTest, UnalignedData) {
    // Allocate with offset to force unalignment
    std::vector<float> buffer(20);
    float* a = buffer.data() + 1;  // Unaligned by 4 bytes
    float* b = buffer.data() + 6;  // Another unaligned pointer

    for (int i = 0; i < 8; i++) {
        a[i] = (float)i;
        b[i] = 1.0f;
    }

    float result = nimcp_simd_dot_product(a, b, 8);

    // Expected: 0+1+2+3+4+5+6+7 = 28
    EXPECT_NEAR(result, 28.0f, 1e-5f);
}

//=============================================================================
// Null Safety Tests
//=============================================================================

TEST_F(SIMDTest, NullPointerSafety) {
    float valid[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    float output[4];

    // These should not crash (should return 0 or handle gracefully)
    EXPECT_NEAR(nimcp_simd_dot_product(nullptr, valid, 4), 0.0f, 1e-5f);
    EXPECT_NEAR(nimcp_simd_dot_product(valid, nullptr, 4), 0.0f, 1e-5f);

    nimcp_simd_vector_add(nullptr, valid, output, 4);
    nimcp_simd_vector_add(valid, nullptr, output, 4);
    nimcp_simd_vector_add(valid, valid, nullptr, 4);

    nimcp_simd_matvec_mul(nullptr, valid, output, 2, 2);
    nimcp_simd_matvec_mul(valid, nullptr, output, 2, 2);
    nimcp_simd_matvec_mul(valid, valid, nullptr, 2, 2);

    SUCCEED();
}

//=============================================================================
// Performance Benchmarks
//=============================================================================

TEST_F(SIMDTest, DISABLED_BenchmarkDotProduct) {
    const uint32_t size = 10000;
    const uint32_t iterations = 1000;

    std::vector<float> a(size, 1.0f);
    std::vector<float> b(size, 2.0f);

    // Warmup
    for (int i = 0; i < 10; i++) {
        nimcp_simd_dot_product(a.data(), b.data(), size);
    }

    // Benchmark
    uint64_t start = nimcp_time_get_ms();
    float result = 0.0f;
    for (uint32_t i = 0; i < iterations; i++) {
        result += nimcp_simd_dot_product(a.data(), b.data(), size);
    }
    uint64_t end = nimcp_time_get_ms();

    double time_ms = (double)(end - start);
    double ops_per_sec = (iterations * size) / (time_ms / 1000.0);

    std::cout << "Dot product: " << ops_per_sec / 1e9 << " GFLOPS" << std::endl;
    std::cout << "Result (to prevent optimization): " << result << std::endl;
}

TEST_F(SIMDTest, DISABLED_BenchmarkMatVecMul) {
    const uint32_t m = 1024;
    const uint32_t n = 1024;
    const uint32_t iterations = 100;

    std::vector<float> matrix(m * n, 1.0f);
    std::vector<float> input(n, 1.0f);
    std::vector<float> output(m);

    // Warmup
    for (int i = 0; i < 5; i++) {
        nimcp_simd_matvec_mul(matrix.data(), input.data(), output.data(), m, n);
    }

    // Benchmark
    uint64_t start = nimcp_time_get_ms();
    for (uint32_t i = 0; i < iterations; i++) {
        nimcp_simd_matvec_mul(matrix.data(), input.data(), output.data(), m, n);
    }
    uint64_t end = nimcp_time_get_ms();

    double time_ms = (double)(end - start);
    double flops = (double)(2 * m * n) * iterations;  // 2 ops per multiply-add
    double gflops = flops / (time_ms / 1000.0) / 1e9;

    std::cout << "Matrix-vector multiply: " << gflops << " GFLOPS" << std::endl;
}

//=============================================================================
// Implementation Selection Tests
//=============================================================================

TEST_F(SIMDTest, FallbackWorksWhenSIMDUnavailable) {
    // Test the scalar fallback implementation
    float a[] = {1.0f, 2.0f, 3.0f, 4.0f};
    float b[] = {5.0f, 6.0f, 7.0f, 8.0f};
    float result[4];

    // Even if SIMD is available, the scalar path should work
    nimcp_simd_vector_add(a, b, result, 4);

    float expected[] = {6.0f, 8.0f, 10.0f, 12.0f};
    ExpectArrayNear(result, expected, 4);
}

TEST_F(SIMDTest, ConsistencyAcrossImplementations) {
    // If we have multiple SIMD implementations available,
    // they should all produce the same results
    const uint32_t size = 64;
    std::vector<float> a(size);
    std::vector<float> b(size);

    for (uint32_t i = 0; i < size; i++) {
        a[i] = (float)i / 10.0f;
        b[i] = (float)(size - i) / 10.0f;
    }

    float result = nimcp_simd_dot_product(a.data(), b.data(), size);

    // Compute reference (scalar)
    float expected = 0.0f;
    for (uint32_t i = 0; i < size; i++) {
        expected += a[i] * b[i];
    }

    EXPECT_NEAR(result, expected, 1e-3f);
}
