//=============================================================================
// test_ternary_performance_regression.cpp - Ternary Performance Regression Tests
//=============================================================================
/**
 * @file test_ternary_performance_regression.cpp
 * @brief Regression tests for ternary operation performance
 *
 * WHAT: Benchmark ternary matrix operations and verify no performance regression
 * WHY:  Performance is critical for SNN and neural network applications
 * HOW:  Compare against float baseline and historical benchmarks
 *
 * PERFORMANCE BASELINES:
 * - Vector operations: >= 1M ops/sec
 * - Matrix-vector multiply: >= 100K multiplies/sec
 * - Pack/unpack: >= 10M trits/sec
 * - Random access: >= 5M accesses/sec
 *
 * @author NIMCP Test Team
 * @date 2025-12-31
 */

#include <gtest/gtest.h>
#include <chrono>
#include <vector>
#include <random>
#include <iostream>
#include <iomanip>

// Headers have their own extern "C" guards
#include "utils/ternary/nimcp_ternary.h"

//=============================================================================
// Test Fixture
//=============================================================================

class TernaryPerformanceRegressionTest : public ::testing::Test {
protected:
    std::mt19937 rng;

    void SetUp() override {
        rng.seed(42);
    }

    void TearDown() override {}

    // Timing helper
    template<typename Func>
    double benchmark_ms(Func f, size_t iterations = 1) {
        auto start = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < iterations; i++) {
            f();
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        return duration.count() / 1000.0;
    }

    // Generate random trit array
    std::vector<trit_t> generate_random_trits(size_t n) {
        std::uniform_int_distribution<int> dist(-1, 1);
        std::vector<trit_t> result(n);
        for (size_t i = 0; i < n; i++) {
            result[i] = static_cast<trit_t>(dist(rng));
        }
        return result;
    }

    // Generate random float array
    std::vector<float> generate_random_floats(size_t n) {
        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
        std::vector<float> result(n);
        for (size_t i = 0; i < n; i++) {
            result[i] = dist(rng);
        }
        return result;
    }
};

//=============================================================================
// Vector Operation Performance Tests
//=============================================================================

TEST_F(TernaryPerformanceRegressionTest, VectorCreateDestroyThroughput) {
    // WHAT: Measure vector creation/destruction throughput
    // WHY:  Allocation overhead affects real-world performance
    // BASELINE: >= 10K creates/sec for 1K-element vectors

    const size_t vector_size = 1000;
    const size_t num_iterations = 10000;

    double elapsed_ms = benchmark_ms([&]() {
        for (size_t i = 0; i < num_iterations; i++) {
            trit_vector_t* vec = trit_vector_create(vector_size, TERNARY_PACK_NONE);
            trit_vector_destroy(vec);
        }
    });

    double ops_per_sec = (num_iterations * 1000.0) / elapsed_ms;

    std::cout << "Vector create/destroy throughput: " << std::fixed << std::setprecision(0)
              << ops_per_sec << " ops/sec" << std::endl;

    EXPECT_GE(ops_per_sec, 10000.0);  // >= 10K ops/sec
}

TEST_F(TernaryPerformanceRegressionTest, VectorSetGetThroughputUnpacked) {
    // WHAT: Measure random access throughput for unpacked vectors
    // WHY:  Random access is common in neural network operations
    // BASELINE: >= 10M accesses/sec for unpacked

    const size_t vector_size = 100000;
    const size_t num_accesses = 1000000;

    trit_vector_t* vec = trit_vector_create(vector_size, TERNARY_PACK_NONE);
    ASSERT_NE(vec, nullptr);

    // Pre-generate random indices
    std::uniform_int_distribution<size_t> idx_dist(0, vector_size - 1);
    std::vector<size_t> indices(num_accesses);
    for (size_t i = 0; i < num_accesses; i++) {
        indices[i] = idx_dist(rng);
    }

    // Benchmark set operations
    double set_ms = benchmark_ms([&]() {
        for (size_t i = 0; i < num_accesses; i++) {
            trit_vector_set(vec, indices[i], TRIT_POSITIVE);
        }
    });

    // Benchmark get operations
    volatile trit_t dummy = 0;
    double get_ms = benchmark_ms([&]() {
        for (size_t i = 0; i < num_accesses; i++) {
            dummy = trit_vector_get(vec, indices[i]);
        }
    });
    (void)dummy;

    double set_ops_per_sec = (num_accesses * 1000.0) / set_ms;
    double get_ops_per_sec = (num_accesses * 1000.0) / get_ms;

    std::cout << "Unpacked vector access throughput:" << std::endl;
    std::cout << "  Set: " << std::fixed << std::setprecision(1)
              << set_ops_per_sec / 1e6 << " M ops/sec" << std::endl;
    std::cout << "  Get: " << std::fixed << std::setprecision(1)
              << get_ops_per_sec / 1e6 << " M ops/sec" << std::endl;

    EXPECT_GE(set_ops_per_sec, 10e6);  // >= 10M ops/sec
    EXPECT_GE(get_ops_per_sec, 10e6);  // >= 10M ops/sec

    trit_vector_destroy(vec);
}

TEST_F(TernaryPerformanceRegressionTest, VectorSetGetThroughputPacked) {
    // WHAT: Measure random access throughput for packed vectors
    // WHY:  Packed access is slower but memory-efficient
    // BASELINE: >= 1M accesses/sec for base-243

    const size_t vector_size = 100000;
    const size_t num_accesses = 100000;

    trit_vector_t* vec_2bit = trit_vector_create(vector_size, TERNARY_PACK_2BIT);
    trit_vector_t* vec_243 = trit_vector_create(vector_size, TERNARY_PACK_BASE243);
    ASSERT_NE(vec_2bit, nullptr);
    ASSERT_NE(vec_243, nullptr);

    std::uniform_int_distribution<size_t> idx_dist(0, vector_size - 1);
    std::vector<size_t> indices(num_accesses);
    for (size_t i = 0; i < num_accesses; i++) {
        indices[i] = idx_dist(rng);
    }

    // 2-bit packed
    double set_2bit_ms = benchmark_ms([&]() {
        for (size_t i = 0; i < num_accesses; i++) {
            trit_vector_set(vec_2bit, indices[i], TRIT_POSITIVE);
        }
    });

    volatile trit_t dummy = 0;
    double get_2bit_ms = benchmark_ms([&]() {
        for (size_t i = 0; i < num_accesses; i++) {
            dummy = trit_vector_get(vec_2bit, indices[i]);
        }
    });

    // Base-243 packed
    double set_243_ms = benchmark_ms([&]() {
        for (size_t i = 0; i < num_accesses; i++) {
            trit_vector_set(vec_243, indices[i], TRIT_NEGATIVE);
        }
    });

    double get_243_ms = benchmark_ms([&]() {
        for (size_t i = 0; i < num_accesses; i++) {
            dummy = trit_vector_get(vec_243, indices[i]);
        }
    });
    (void)dummy;

    double set_2bit_ops = (num_accesses * 1000.0) / set_2bit_ms;
    double get_2bit_ops = (num_accesses * 1000.0) / get_2bit_ms;
    double set_243_ops = (num_accesses * 1000.0) / set_243_ms;
    double get_243_ops = (num_accesses * 1000.0) / get_243_ms;

    std::cout << "Packed vector access throughput:" << std::endl;
    std::cout << "  2-bit set: " << set_2bit_ops / 1e6 << " M ops/sec" << std::endl;
    std::cout << "  2-bit get: " << get_2bit_ops / 1e6 << " M ops/sec" << std::endl;
    std::cout << "  Base-243 set: " << set_243_ops / 1e6 << " M ops/sec" << std::endl;
    std::cout << "  Base-243 get: " << get_243_ops / 1e6 << " M ops/sec" << std::endl;

    EXPECT_GE(set_2bit_ops, 1e6);  // >= 1M ops/sec
    EXPECT_GE(get_2bit_ops, 1e6);
    EXPECT_GE(set_243_ops, 500e3); // >= 500K ops/sec (slower due to div/mod)
    EXPECT_GE(get_243_ops, 500e3);

    trit_vector_destroy(vec_2bit);
    trit_vector_destroy(vec_243);
}

//=============================================================================
// Packing/Unpacking Performance Tests
//=============================================================================

TEST_F(TernaryPerformanceRegressionTest, BulkPackingThroughput) {
    // WHAT: Measure bulk packing throughput
    // WHY:  Conversion between formats is common operation
    // BASELINE: >= 10M trits/sec for 2-bit, >= 5M for base-243

    const size_t num_trits = 1000000;
    const size_t num_iterations = 10;

    std::vector<trit_t> input = generate_random_trits(num_trits);
    std::vector<uint8_t> packed_2bit((num_trits + 3) / 4);
    std::vector<uint8_t> packed_243((num_trits + 4) / 5);

    // 2-bit packing
    double pack_2bit_ms = benchmark_ms([&]() {
        trit_pack_array_2bit(input.data(), packed_2bit.data(), num_trits);
    }, num_iterations);

    // Base-243 packing
    double pack_243_ms = benchmark_ms([&]() {
        trit_pack_array_243(input.data(), packed_243.data(), num_trits);
    }, num_iterations);

    double pack_2bit_rate = (num_trits * num_iterations * 1000.0) / pack_2bit_ms;
    double pack_243_rate = (num_trits * num_iterations * 1000.0) / pack_243_ms;

    std::cout << "Bulk packing throughput:" << std::endl;
    std::cout << "  2-bit: " << pack_2bit_rate / 1e6 << " M trits/sec" << std::endl;
    std::cout << "  Base-243: " << pack_243_rate / 1e6 << " M trits/sec" << std::endl;

    EXPECT_GE(pack_2bit_rate, 10e6);  // >= 10M trits/sec
    EXPECT_GE(pack_243_rate, 5e6);    // >= 5M trits/sec

    // Verify packing is correct
    std::vector<trit_t> unpacked_2bit(num_trits);
    trit_unpack_array_2bit(packed_2bit.data(), unpacked_2bit.data(), num_trits);
    for (size_t i = 0; i < num_trits; i++) {
        ASSERT_EQ(input[i], unpacked_2bit[i]) << "Mismatch at index " << i;
    }
}

TEST_F(TernaryPerformanceRegressionTest, BulkUnpackingThroughput) {
    // WHAT: Measure bulk unpacking throughput
    // WHY:  Unpacking is needed for computation
    // BASELINE: >= 10M trits/sec for 2-bit, >= 5M for base-243

    const size_t num_trits = 1000000;
    const size_t num_iterations = 10;

    std::vector<trit_t> input = generate_random_trits(num_trits);
    std::vector<uint8_t> packed_2bit((num_trits + 3) / 4);
    std::vector<uint8_t> packed_243((num_trits + 4) / 5);
    std::vector<trit_t> output(num_trits);

    // Pack first
    trit_pack_array_2bit(input.data(), packed_2bit.data(), num_trits);
    trit_pack_array_243(input.data(), packed_243.data(), num_trits);

    // 2-bit unpacking
    double unpack_2bit_ms = benchmark_ms([&]() {
        trit_unpack_array_2bit(packed_2bit.data(), output.data(), num_trits);
    }, num_iterations);

    // Base-243 unpacking
    double unpack_243_ms = benchmark_ms([&]() {
        trit_unpack_array_243(packed_243.data(), output.data(), num_trits);
    }, num_iterations);

    double unpack_2bit_rate = (num_trits * num_iterations * 1000.0) / unpack_2bit_ms;
    double unpack_243_rate = (num_trits * num_iterations * 1000.0) / unpack_243_ms;

    std::cout << "Bulk unpacking throughput:" << std::endl;
    std::cout << "  2-bit: " << unpack_2bit_rate / 1e6 << " M trits/sec" << std::endl;
    std::cout << "  Base-243: " << unpack_243_rate / 1e6 << " M trits/sec" << std::endl;

    EXPECT_GE(unpack_2bit_rate, 10e6);
    EXPECT_GE(unpack_243_rate, 5e6);
}

//=============================================================================
// Matrix Operation Performance Tests
//=============================================================================

TEST_F(TernaryPerformanceRegressionTest, MatrixVectorMultiplyThroughput) {
    // WHAT: Measure matrix-vector multiply throughput
    // WHY:  Core operation for neural network forward pass
    // BASELINE: >= 100K multiplications/sec

    const size_t rows = 256;
    const size_t cols = 256;
    const size_t num_iterations = 100;

    trit_matrix_t* mat = trit_matrix_create(rows, cols, TERNARY_PACK_NONE);
    trit_vector_t* vec = trit_vector_create(cols, TERNARY_PACK_NONE);
    ASSERT_NE(mat, nullptr);
    ASSERT_NE(vec, nullptr);

    // Initialize with random values
    std::uniform_int_distribution<int> dist(-1, 1);
    for (size_t r = 0; r < rows; r++) {
        for (size_t c = 0; c < cols; c++) {
            trit_matrix_set(mat, r, c, static_cast<trit_t>(dist(rng)));
        }
    }
    for (size_t i = 0; i < cols; i++) {
        trit_vector_set(vec, i, static_cast<trit_t>(dist(rng)));
    }

    double elapsed_ms = benchmark_ms([&]() {
        for (size_t i = 0; i < num_iterations; i++) {
            trit_vector_t* result = trit_matrix_vector_mul(mat, vec);
            trit_vector_destroy(result);
        }
    });

    double multiplies = rows * cols * num_iterations;
    double multiplies_per_sec = (multiplies * 1000.0) / elapsed_ms;
    double ops_per_sec = (num_iterations * 1000.0) / elapsed_ms;

    std::cout << "Matrix-vector multiply throughput:" << std::endl;
    std::cout << "  " << ops_per_sec << " multiplies/sec (" << rows << "x" << cols << ")" << std::endl;
    std::cout << "  " << multiplies_per_sec / 1e6 << " M element-wise ops/sec" << std::endl;

    EXPECT_GE(ops_per_sec, 100.0);  // >= 100 full multiplies/sec
    EXPECT_GE(multiplies_per_sec, 1e6);  // >= 1M element ops/sec

    trit_matrix_destroy(mat);
    trit_vector_destroy(vec);
}

TEST_F(TernaryPerformanceRegressionTest, VectorDotProductThroughput) {
    // WHAT: Measure vector dot product throughput
    // WHY:  Common operation in similarity computation
    // BASELINE: >= 1K dot products/sec for 10K-element vectors

    const size_t vector_size = 10000;
    const size_t num_iterations = 1000;

    trit_vector_t* a = trit_vector_create(vector_size, TERNARY_PACK_NONE);
    trit_vector_t* b = trit_vector_create(vector_size, TERNARY_PACK_NONE);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    std::uniform_int_distribution<int> dist(-1, 1);
    for (size_t i = 0; i < vector_size; i++) {
        trit_vector_set(a, i, static_cast<trit_t>(dist(rng)));
        trit_vector_set(b, i, static_cast<trit_t>(dist(rng)));
    }

    volatile int dummy = 0;
    double elapsed_ms = benchmark_ms([&]() {
        for (size_t i = 0; i < num_iterations; i++) {
            dummy = trit_vector_dot(a, b);
        }
    });
    (void)dummy;

    double dots_per_sec = (num_iterations * 1000.0) / elapsed_ms;

    std::cout << "Dot product throughput (" << vector_size << " elements): "
              << dots_per_sec << " dots/sec" << std::endl;

    EXPECT_GE(dots_per_sec, 1000.0);

    trit_vector_destroy(a);
    trit_vector_destroy(b);
}

//=============================================================================
// Logic Operation Performance Tests
//=============================================================================

TEST_F(TernaryPerformanceRegressionTest, ScalarLogicThroughput) {
    // WHAT: Measure scalar logic operation throughput
    // WHY:  Logic ops are used throughout ternary reasoning
    // BASELINE: >= 100M ops/sec for inline functions

    const size_t num_ops = 10000000;

    volatile trit_t a = TRIT_POSITIVE;
    volatile trit_t b = TRIT_NEGATIVE;
    volatile trit_t result = TRIT_UNKNOWN;

    // AND
    double and_ms = benchmark_ms([&]() {
        for (size_t i = 0; i < num_ops; i++) {
            result = trit_and(a, b);
        }
    });

    // OR
    double or_ms = benchmark_ms([&]() {
        for (size_t i = 0; i < num_ops; i++) {
            result = trit_or(a, b);
        }
    });

    // NOT
    double not_ms = benchmark_ms([&]() {
        for (size_t i = 0; i < num_ops; i++) {
            result = trit_not(a);
        }
    });
    (void)result;

    double and_rate = (num_ops * 1000.0) / and_ms;
    double or_rate = (num_ops * 1000.0) / or_ms;
    double not_rate = (num_ops * 1000.0) / not_ms;

    std::cout << "Scalar logic throughput:" << std::endl;
    std::cout << "  AND: " << and_rate / 1e6 << " M ops/sec" << std::endl;
    std::cout << "  OR:  " << or_rate / 1e6 << " M ops/sec" << std::endl;
    std::cout << "  NOT: " << not_rate / 1e6 << " M ops/sec" << std::endl;

    EXPECT_GE(and_rate, 50e6);  // >= 50M ops/sec (allowing for volatile overhead)
    EXPECT_GE(or_rate, 50e6);
    EXPECT_GE(not_rate, 50e6);
}

TEST_F(TernaryPerformanceRegressionTest, VectorLogicThroughput) {
    // WHAT: Measure vector logic operation throughput
    // WHY:  Vector ops are used in batch processing
    // BASELINE: >= 10K vector ops/sec for 1K-element vectors

    const size_t vector_size = 1000;
    const size_t num_iterations = 10000;

    trit_vector_t* a = trit_vector_create_filled(vector_size, TRIT_POSITIVE, TERNARY_PACK_NONE);
    trit_vector_t* b = trit_vector_create_filled(vector_size, TRIT_NEGATIVE, TERNARY_PACK_NONE);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    double elapsed_ms = benchmark_ms([&]() {
        for (size_t i = 0; i < num_iterations; i++) {
            trit_vector_t* result = trit_vector_and(a, b);
            trit_vector_destroy(result);
        }
    });

    double ops_per_sec = (num_iterations * 1000.0) / elapsed_ms;

    std::cout << "Vector AND throughput (" << vector_size << " elements): "
              << ops_per_sec << " ops/sec" << std::endl;

    EXPECT_GE(ops_per_sec, 10000.0);

    trit_vector_destroy(a);
    trit_vector_destroy(b);
}

//=============================================================================
// Conversion Performance Tests
//=============================================================================

TEST_F(TernaryPerformanceRegressionTest, FloatToTernaryConversionThroughput) {
    // WHAT: Measure float-to-ternary conversion throughput
    // WHY:  Quantization is common in training pipelines
    // BASELINE: >= 1M conversions/sec

    const size_t num_values = 1000000;
    const float threshold = 0.5f;

    std::vector<float> floats = generate_random_floats(num_values);

    double elapsed_ms = benchmark_ms([&]() {
        trit_vector_t* vec = trit_vector_from_floats(floats.data(), num_values,
                                                       threshold, TERNARY_PACK_NONE);
        trit_vector_destroy(vec);
    }, 10);

    double conversions_per_sec = (num_values * 10 * 1000.0) / elapsed_ms;

    std::cout << "Float-to-ternary conversion: "
              << conversions_per_sec / 1e6 << " M conversions/sec" << std::endl;

    EXPECT_GE(conversions_per_sec, 1e6);
}

TEST_F(TernaryPerformanceRegressionTest, TernaryToFloatConversionThroughput) {
    // WHAT: Measure ternary-to-float conversion throughput
    // WHY:  Dequantization is needed for output
    // BASELINE: >= 1M conversions/sec

    const size_t num_values = 1000000;

    trit_vector_t* vec = trit_vector_create_filled(num_values, TRIT_POSITIVE, TERNARY_PACK_NONE);
    ASSERT_NE(vec, nullptr);

    std::vector<float> floats(num_values);

    double elapsed_ms = benchmark_ms([&]() {
        trit_vector_to_floats(vec, floats.data(), 1.0f);
    }, 10);

    double conversions_per_sec = (num_values * 10 * 1000.0) / elapsed_ms;

    std::cout << "Ternary-to-float conversion: "
              << conversions_per_sec / 1e6 << " M conversions/sec" << std::endl;

    EXPECT_GE(conversions_per_sec, 1e6);

    trit_vector_destroy(vec);
}

//=============================================================================
// Comparison with Float Baseline
//=============================================================================

TEST_F(TernaryPerformanceRegressionTest, TernaryVsFloatDotProduct) {
    // WHAT: Compare ternary vs float dot product performance
    // WHY:  Understand speedup/slowdown from ternary representation
    // BASELINE: Ternary should be within 10x of float performance

    const size_t vector_size = 10000;
    const size_t num_iterations = 1000;

    // Float baseline
    std::vector<float> float_a = generate_random_floats(vector_size);
    std::vector<float> float_b = generate_random_floats(vector_size);

    volatile float float_result = 0.0f;
    double float_ms = benchmark_ms([&]() {
        for (size_t iter = 0; iter < num_iterations; iter++) {
            float_result = 0.0f;
            for (size_t i = 0; i < vector_size; i++) {
                float_result += float_a[i] * float_b[i];
            }
        }
    });
    (void)float_result;

    // Ternary
    trit_vector_t* trit_a = trit_vector_from_floats(float_a.data(), vector_size, 0.3f, TERNARY_PACK_NONE);
    trit_vector_t* trit_b = trit_vector_from_floats(float_b.data(), vector_size, 0.3f, TERNARY_PACK_NONE);
    ASSERT_NE(trit_a, nullptr);
    ASSERT_NE(trit_b, nullptr);

    volatile int trit_result = 0;
    double trit_ms = benchmark_ms([&]() {
        for (size_t iter = 0; iter < num_iterations; iter++) {
            trit_result = trit_vector_dot(trit_a, trit_b);
        }
    });
    (void)trit_result;

    double float_rate = (num_iterations * 1000.0) / float_ms;
    double trit_rate = (num_iterations * 1000.0) / trit_ms;
    double speedup = trit_rate / float_rate;

    std::cout << "Dot product comparison (" << vector_size << " elements):" << std::endl;
    std::cout << "  Float: " << float_rate << " dots/sec" << std::endl;
    std::cout << "  Ternary: " << trit_rate << " dots/sec" << std::endl;
    std::cout << "  Speedup: " << speedup << "x" << std::endl;

    // Ternary should be at least 0.1x of float (within 10x slowdown)
    EXPECT_GE(speedup, 0.1);

    trit_vector_destroy(trit_a);
    trit_vector_destroy(trit_b);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
