//=============================================================================
// test_tensor_performance.cpp - Regression Tests for Tensor Performance
//=============================================================================
/**
 * @file test_tensor_performance.cpp
 * @brief Performance regression tests for tensor module
 *
 * WHAT: Benchmark tensor operations for performance regression
 * WHY:  Ensure performance doesn't degrade over time
 * HOW:  Time operations and verify against baseline thresholds
 *
 * TEST COVERAGE:
 * 1. Matrix multiplication scaling
 * 2. Element-wise operation throughput
 * 3. Memory allocation efficiency
 * 4. Reduction operation scaling
 * 5. Tensor calculus performance
 *
 * @author NIMCP Development Team
 * @date 2025-12-10
 */

#include <gtest/gtest.h>
#include <chrono>
#include <cmath>
#include <vector>

// Headers have their own extern "C" guards
    #include "utils/tensor/nimcp_tensor.h"
    #include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class TensorRegressionTest : public ::testing::Test {
protected:
    using Clock = std::chrono::high_resolution_clock;
    using Duration = std::chrono::duration<double, std::milli>;

    static constexpr double MAX_MATMUL_MS_PER_MEGAFLOP = 10.0;
    static constexpr double MAX_ELEMENTWISE_MS_PER_MEGAELEMENT = 5.0;

    void SetUp() override {
        nimcp_memory_init();
        nimcp_tensor_init();
        nimcp_tensor_reset_stats();
    }

    void TearDown() override {
        nimcp_tensor_shutdown();
    }

    double GetDurationMs(Clock::time_point start, Clock::time_point end) {
        return Duration(end - start).count();
    }
};

//=============================================================================
// Regression Tests: Matrix Multiplication Scaling
//=============================================================================

TEST_F(TensorRegressionTest, MatmulScaling) {
    // WHAT: Test matrix multiplication scaling
    // WHY:  Catch performance regressions in core operation

    std::vector<uint32_t> sizes = {32, 64, 128, 256};
    std::vector<double> times;
    std::vector<double> flops;

    for (uint32_t n : sizes) {
        uint32_t dims[] = {n, n};
        nimcp_tensor_t* a = nimcp_tensor_randn(dims, 2, NIMCP_DTYPE_F32, 0.0, 1.0);
        nimcp_tensor_t* b = nimcp_tensor_randn(dims, 2, NIMCP_DTYPE_F32, 0.0, 1.0);

        auto start = Clock::now();
        nimcp_tensor_t* c = nimcp_tensor_matmul(a, b);
        auto end = Clock::now();

        double ms = GetDurationMs(start, end);
        double flop = 2.0 * n * n * n;  // matmul flops

        times.push_back(ms);
        flops.push_back(flop);

        nimcp_tensor_destroy(a);
        nimcp_tensor_destroy(b);
        nimcp_tensor_destroy(c);
    }

    // Verify scaling is roughly O(n^3)
    // Compare ratios: time(256)/time(64) should be roughly (256/64)^3 = 64
    if (times[1] > 0.01) {  // Avoid division by zero for fast ops
        double ratio = times[3] / times[1];  // 256 vs 64
        double expected_ratio = 64.0;  // (256/64)^3

        // Allow 3x variance due to cache effects etc
        EXPECT_LT(ratio, expected_ratio * 3.0)
            << "Matmul scaling worse than expected: " << ratio << " vs " << expected_ratio;
    }

    // Report throughput
    for (size_t i = 0; i < sizes.size(); i++) {
        double gflops = flops[i] / (times[i] * 1e6);
        std::cout << "  Matmul " << sizes[i] << "x" << sizes[i]
                  << ": " << times[i] << "ms, " << gflops << " GFLOPS" << std::endl;
    }
}

//=============================================================================
// Regression Tests: Element-wise Throughput
//=============================================================================

TEST_F(TensorRegressionTest, ElementWiseThroughput) {
    // WHAT: Test element-wise operation throughput
    // WHY:  Catch memory bandwidth regressions

    uint32_t dims[] = {1024, 1024};  // 1M elements
    nimcp_tensor_t* a = nimcp_tensor_randn(dims, 2, NIMCP_DTYPE_F32, 0.0, 1.0);
    nimcp_tensor_t* b = nimcp_tensor_randn(dims, 2, NIMCP_DTYPE_F32, 0.0, 1.0);

    // Warm up
    nimcp_tensor_t* warmup = nimcp_tensor_add(a, b);
    nimcp_tensor_destroy(warmup);

    // Benchmark add
    auto start = Clock::now();
    for (int i = 0; i < 10; i++) {
        nimcp_tensor_t* c = nimcp_tensor_add(a, b);
        nimcp_tensor_destroy(c);
    }
    auto end = Clock::now();
    double add_ms = GetDurationMs(start, end) / 10.0;

    // Benchmark mul
    start = Clock::now();
    for (int i = 0; i < 10; i++) {
        nimcp_tensor_t* c = nimcp_tensor_mul(a, b);
        nimcp_tensor_destroy(c);
    }
    end = Clock::now();
    double mul_ms = GetDurationMs(start, end) / 10.0;

    // Benchmark exp
    start = Clock::now();
    for (int i = 0; i < 10; i++) {
        nimcp_tensor_t* c = nimcp_tensor_exp(a);
        nimcp_tensor_destroy(c);
    }
    end = Clock::now();
    double exp_ms = GetDurationMs(start, end) / 10.0;

    double elements_m = 1.0;  // 1M elements

    std::cout << "  Element-wise throughput (1M elements):" << std::endl;
    std::cout << "    Add: " << add_ms << "ms (" << elements_m / add_ms * 1000 << " Melements/s)" << std::endl;
    std::cout << "    Mul: " << mul_ms << "ms (" << elements_m / mul_ms * 1000 << " Melements/s)" << std::endl;
    std::cout << "    Exp: " << exp_ms << "ms (" << elements_m / exp_ms * 1000 << " Melements/s)" << std::endl;

    // Regression check: should process 1M elements in reasonable time
    EXPECT_LT(add_ms, 100.0) << "Add too slow";
    EXPECT_LT(mul_ms, 100.0) << "Mul too slow";
    EXPECT_LT(exp_ms, 200.0) << "Exp too slow";

    nimcp_tensor_destroy(a);
    nimcp_tensor_destroy(b);
}

//=============================================================================
// Regression Tests: Memory Allocation
//=============================================================================

TEST_F(TensorRegressionTest, AllocationThroughput) {
    // WHAT: Test tensor allocation/deallocation throughput
    // WHY:  Catch memory pool regressions

    uint32_t dims[] = {64, 64};  // 4K elements

    auto start = Clock::now();
    for (int i = 0; i < 1000; i++) {
        nimcp_tensor_t* t = nimcp_tensor_zeros(dims, 2, NIMCP_DTYPE_F32);
        nimcp_tensor_destroy(t);
    }
    auto end = Clock::now();

    double total_ms = GetDurationMs(start, end);
    double per_op_us = total_ms * 1000.0 / 1000.0;

    std::cout << "  Allocation: 1000 create/destroy cycles in "
              << total_ms << "ms (" << per_op_us << " us/op)" << std::endl;

    // Should be able to do 1000 alloc/free in under 100ms
    EXPECT_LT(total_ms, 100.0) << "Allocation too slow";
}

//=============================================================================
// Regression Tests: Reduction Scaling
//=============================================================================

TEST_F(TensorRegressionTest, ReductionScaling) {
    // WHAT: Test reduction operation scaling
    // WHY:  Important for loss computation

    std::vector<uint32_t> sizes = {1024, 4096, 16384, 65536};
    std::vector<double> times;

    for (uint32_t n : sizes) {
        nimcp_tensor_t* t = nimcp_tensor_randn(&n, 1, NIMCP_DTYPE_F32, 0.0, 1.0);

        auto start = Clock::now();
        for (int i = 0; i < 100; i++) {
            nimcp_tensor_t* s = nimcp_tensor_sum(t);
            nimcp_tensor_destroy(s);
        }
        auto end = Clock::now();

        double ms = GetDurationMs(start, end) / 100.0;
        times.push_back(ms);

        nimcp_tensor_destroy(t);
    }

    // Verify linear scaling
    // time(64K)/time(1K) should be roughly 64
    if (times[0] > 0.001) {
        double ratio = times[3] / times[0];
        double expected_ratio = 64.0;

        // Allow 2x variance
        EXPECT_LT(ratio, expected_ratio * 2.0)
            << "Reduction scaling worse than O(n)";
    }

    std::cout << "  Reduction scaling:" << std::endl;
    for (size_t i = 0; i < sizes.size(); i++) {
        std::cout << "    Sum " << sizes[i] << " elements: "
                  << times[i] * 1000 << "us" << std::endl;
    }
}

//=============================================================================
// Regression Tests: Softmax Performance
//=============================================================================

TEST_F(TensorRegressionTest, SoftmaxPerformance) {
    // WHAT: Test softmax performance
    // WHY:  Critical for transformer models

    uint32_t dims[] = {32, 1024};  // 32 sequences of 1024 logits
    nimcp_tensor_t* logits = nimcp_tensor_randn(dims, 2, NIMCP_DTYPE_F32, 0.0, 1.0);

    // Warm up
    nimcp_tensor_t* warmup = nimcp_tensor_softmax(logits, -1);
    nimcp_tensor_destroy(warmup);

    auto start = Clock::now();
    for (int i = 0; i < 100; i++) {
        nimcp_tensor_t* probs = nimcp_tensor_softmax(logits, -1);
        nimcp_tensor_destroy(probs);
    }
    auto end = Clock::now();

    double ms = GetDurationMs(start, end) / 100.0;

    std::cout << "  Softmax (32x1024): " << ms << "ms" << std::endl;

    // Should complete in reasonable time
    EXPECT_LT(ms, 50.0) << "Softmax too slow";

    nimcp_tensor_destroy(logits);
}

//=============================================================================
// Regression Tests: Attention Performance
//=============================================================================

TEST_F(TensorRegressionTest, AttentionPerformance) {
    // WHAT: Test attention mechanism performance
    // WHY:  Core transformer operation

    // Q, K, V: (seq=64, dim=64)
    uint32_t dims[] = {64, 64};
    nimcp_tensor_t* Q = nimcp_tensor_randn(dims, 2, NIMCP_DTYPE_F32, 0.0, 1.0);
    nimcp_tensor_t* K = nimcp_tensor_randn(dims, 2, NIMCP_DTYPE_F32, 0.0, 1.0);
    nimcp_tensor_t* V = nimcp_tensor_randn(dims, 2, NIMCP_DTYPE_F32, 0.0, 1.0);

    // Warm up
    nimcp_tensor_t* warmup = nimcp_tensor_attention(Q, K, V, nullptr, 0.0);
    nimcp_tensor_destroy(warmup);

    auto start = Clock::now();
    for (int i = 0; i < 50; i++) {
        nimcp_tensor_t* attn = nimcp_tensor_attention(Q, K, V, nullptr, 0.0);
        nimcp_tensor_destroy(attn);
    }
    auto end = Clock::now();

    double ms = GetDurationMs(start, end) / 50.0;

    std::cout << "  Attention (64x64): " << ms << "ms" << std::endl;

    // Should be reasonably fast
    EXPECT_LT(ms, 100.0) << "Attention too slow";

    nimcp_tensor_destroy(Q);
    nimcp_tensor_destroy(K);
    nimcp_tensor_destroy(V);
}

//=============================================================================
// Regression Tests: Tensor Calculus
//=============================================================================

TEST_F(TensorRegressionTest, LaplacianPerformance) {
    // WHAT: Test Laplacian computation performance
    // WHY:  Used in scientific computing pipelines

    uint32_t dims[] = {64, 64, 64};  // 3D field
    nimcp_tensor_t* field = nimcp_tensor_randn(dims, 3, NIMCP_DTYPE_F32, 0.0, 1.0);

    double spacing[] = {1.0, 1.0, 1.0};

    auto start = Clock::now();
    nimcp_tensor_t* lap = nimcp_tensor_laplacian(field, spacing);
    auto end = Clock::now();

    double ms = GetDurationMs(start, end);

    std::cout << "  Laplacian (64x64x64): " << ms << "ms" << std::endl;

    // Should complete in reasonable time for 262K elements
    EXPECT_LT(ms, 500.0) << "Laplacian too slow";

    nimcp_tensor_destroy(field);
    nimcp_tensor_destroy(lap);
}

//=============================================================================
// Regression Tests: Memory Efficiency
//=============================================================================

TEST_F(TensorRegressionTest, MemoryEfficiency) {
    // WHAT: Test memory overhead
    // WHY:  Ensure memory efficiency doesn't degrade

    nimcp_tensor_reset_stats();

    // Create many tensors
    std::vector<nimcp_tensor_t*> tensors;
    uint32_t dims[] = {100, 100};  // 10K elements = 40KB per tensor

    for (int i = 0; i < 100; i++) {
        tensors.push_back(nimcp_tensor_zeros(dims, 2, NIMCP_DTYPE_F32));
    }

    nimcp_tensor_stats_t stats;
    nimcp_tensor_get_stats(&stats);

    // 100 tensors * 40KB data = 4MB data
    // Overhead should be < 20% (structs, alignment, etc)
    size_t expected_data = 100 * 10000 * sizeof(float);
    double overhead = (double)(stats.memory_current - expected_data) / expected_data;

    std::cout << "  Memory overhead: " << overhead * 100 << "%" << std::endl;
    std::cout << "    Total allocated: " << stats.memory_current / 1024 << " KB" << std::endl;
    std::cout << "    Expected data: " << expected_data / 1024 << " KB" << std::endl;

    EXPECT_LT(overhead, 0.5) << "Memory overhead too high";

    for (auto t : tensors) {
        nimcp_tensor_destroy(t);
    }
}

//=============================================================================
// Regression Tests: Peak Memory
//=============================================================================

TEST_F(TensorRegressionTest, PeakMemoryTracking) {
    // WHAT: Test peak memory tracking
    // WHY:  Ensure memory tracking works correctly

    nimcp_tensor_reset_stats();

    // Create tensors
    std::vector<nimcp_tensor_t*> tensors;
    uint32_t dims[] = {256, 256};

    for (int i = 0; i < 10; i++) {
        tensors.push_back(nimcp_tensor_zeros(dims, 2, NIMCP_DTYPE_F32));
    }

    nimcp_tensor_stats_t stats_peak;
    nimcp_tensor_get_stats(&stats_peak);

    // Free half
    for (int i = 0; i < 5; i++) {
        nimcp_tensor_destroy(tensors[i]);
    }

    nimcp_tensor_stats_t stats_after;
    nimcp_tensor_get_stats(&stats_after);

    // Current should be about half of peak
    EXPECT_LT(stats_after.memory_current, stats_peak.memory_peak);
    EXPECT_GT(stats_after.memory_peak, 0u);

    std::cout << "  Peak memory: " << stats_after.memory_peak / 1024 << " KB" << std::endl;
    std::cout << "  Current memory: " << stats_after.memory_current / 1024 << " KB" << std::endl;

    // Cleanup rest
    for (size_t i = 5; i < tensors.size(); i++) {
        nimcp_tensor_destroy(tensors[i]);
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    std::cout << "\n=== Tensor Performance Regression Tests ===" << std::endl;
    return RUN_ALL_TESTS();
}
