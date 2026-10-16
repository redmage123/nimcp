//=============================================================================
// test_ternary_memory_regression.cpp - Ternary Memory Usage Regression Tests
//=============================================================================
/**
 * @file test_ternary_memory_regression.cpp
 * @brief Regression tests for ternary representation memory efficiency
 *
 * WHAT: Verify memory savings match expected ratios for ternary packing
 * WHY:  Ensure memory efficiency does not regress over time
 * HOW:  Compare memory footprint of float vs ternary representations
 *
 * EXPECTED MEMORY SAVINGS:
 * - Unpacked ternary: 4x vs float32 (1 byte vs 4 bytes)
 * - 2-bit packed: 16x vs float32 (4 trits per byte)
 * - Base-243 packed: 20x vs float32 (5 trits per byte)
 *
 * REGRESSION BASELINES:
 * - 1M trits unpacked: <= 1 MB
 * - 1M trits 2-bit: <= 256 KB
 * - 1M trits base-243: <= 200 KB
 *
 * @author NIMCP Test Team
 * @date 2025-12-31
 */

#include <gtest/gtest.h>
#include <chrono>
#include <cstring>

// Headers have their own extern "C" guards
#include "utils/ternary/nimcp_ternary.h"

//=============================================================================
// Test Fixture
//=============================================================================

class TernaryMemoryRegressionTest : public ::testing::Test {
protected:
    static constexpr size_t MILLION = 1000000;
    static constexpr size_t KILO = 1024;
    static constexpr size_t MEGA = 1024 * 1024;

    void SetUp() override {}
    void TearDown() override {}

    // Calculate actual memory footprint for a packed array
    size_t calculate_packed_bytes(size_t num_trits, ternary_pack_mode_t mode) {
        return trit_packed_bytes(num_trits, mode);
    }

    // Calculate float equivalent bytes
    size_t calculate_float_bytes(size_t num_elements) {
        return num_elements * sizeof(float);
    }

    // Calculate compression ratio
    double compression_ratio(size_t float_bytes, size_t packed_bytes) {
        return static_cast<double>(float_bytes) / static_cast<double>(packed_bytes);
    }
};

//=============================================================================
// Memory Footprint Baseline Tests
//=============================================================================

TEST_F(TernaryMemoryRegressionTest, UnpackedMemoryBaseline) {
    // WHAT: Verify unpacked ternary uses 1 byte per trit
    // WHY:  Baseline memory usage must not regress
    // BASELINE: 1M trits = 1 MB exactly

    size_t packed_bytes = calculate_packed_bytes(MILLION, TERNARY_PACK_NONE);

    EXPECT_EQ(packed_bytes, MILLION);  // 1 byte per trit

    std::cout << "Unpacked 1M trits: " << packed_bytes << " bytes ("
              << packed_bytes / MEGA << " MB)" << std::endl;
}

TEST_F(TernaryMemoryRegressionTest, TwoBitPackedMemoryBaseline) {
    // WHAT: Verify 2-bit packed uses ~0.25 bytes per trit
    // WHY:  Memory efficiency must not regress
    // BASELINE: 1M trits <= 256 KB (250000 bytes theoretical)

    size_t packed_bytes = calculate_packed_bytes(MILLION, TERNARY_PACK_2BIT);

    // 4 trits per byte = 250,000 bytes for 1M trits
    EXPECT_EQ(packed_bytes, 250000UL);
    EXPECT_LE(packed_bytes, 256 * KILO);

    std::cout << "2-bit packed 1M trits: " << packed_bytes << " bytes ("
              << packed_bytes / KILO << " KB)" << std::endl;
}

TEST_F(TernaryMemoryRegressionTest, Base243PackedMemoryBaseline) {
    // WHAT: Verify base-243 packed uses ~0.2 bytes per trit
    // WHY:  Maximum memory efficiency must not regress
    // BASELINE: 1M trits <= 200 KB (200000 bytes theoretical)

    size_t packed_bytes = calculate_packed_bytes(MILLION, TERNARY_PACK_BASE243);

    // 5 trits per byte = 200,000 bytes for 1M trits
    EXPECT_EQ(packed_bytes, 200000UL);
    EXPECT_LE(packed_bytes, 200 * KILO);

    std::cout << "Base-243 packed 1M trits: " << packed_bytes << " bytes ("
              << packed_bytes / KILO << " KB)" << std::endl;
}

//=============================================================================
// Compression Ratio Tests vs Float32
//=============================================================================

TEST_F(TernaryMemoryRegressionTest, CompressionRatioUnpacked) {
    // WHAT: Verify 4x memory savings vs float32 for unpacked
    // WHY:  Compression ratio is primary value proposition
    // BASELINE: >= 4x compression

    size_t float_bytes = calculate_float_bytes(MILLION);
    size_t packed_bytes = calculate_packed_bytes(MILLION, TERNARY_PACK_NONE);

    double ratio = compression_ratio(float_bytes, packed_bytes);

    std::cout << "Unpacked compression ratio: " << ratio << "x" << std::endl;
    std::cout << "  Float32: " << float_bytes / MEGA << " MB" << std::endl;
    std::cout << "  Ternary: " << packed_bytes / MEGA << " MB" << std::endl;

    EXPECT_GE(ratio, 4.0);
}

TEST_F(TernaryMemoryRegressionTest, CompressionRatio2Bit) {
    // WHAT: Verify 16x memory savings vs float32 for 2-bit
    // WHY:  2-bit packing provides significant savings
    // BASELINE: >= 16x compression

    size_t float_bytes = calculate_float_bytes(MILLION);
    size_t packed_bytes = calculate_packed_bytes(MILLION, TERNARY_PACK_2BIT);

    double ratio = compression_ratio(float_bytes, packed_bytes);

    std::cout << "2-bit packed compression ratio: " << ratio << "x" << std::endl;
    std::cout << "  Float32: " << float_bytes / MEGA << " MB" << std::endl;
    std::cout << "  Ternary: " << packed_bytes / KILO << " KB" << std::endl;

    EXPECT_GE(ratio, 16.0);
}

TEST_F(TernaryMemoryRegressionTest, CompressionRatioBase243) {
    // WHAT: Verify 20x memory savings vs float32 for base-243
    // WHY:  Base-243 provides maximum compression
    // BASELINE: >= 20x compression

    size_t float_bytes = calculate_float_bytes(MILLION);
    size_t packed_bytes = calculate_packed_bytes(MILLION, TERNARY_PACK_BASE243);

    double ratio = compression_ratio(float_bytes, packed_bytes);

    std::cout << "Base-243 packed compression ratio: " << ratio << "x" << std::endl;
    std::cout << "  Float32: " << float_bytes / MEGA << " MB" << std::endl;
    std::cout << "  Ternary: " << packed_bytes / KILO << " KB" << std::endl;

    EXPECT_GE(ratio, 20.0);
}

//=============================================================================
// Actual Allocation Size Tests
//=============================================================================

TEST_F(TernaryMemoryRegressionTest, VectorAllocationSize) {
    // WHAT: Verify actual vector allocation matches expected
    // WHY:  Ensure no hidden memory overhead in data structures
    // BASELINE: Packed bytes + minimal struct overhead

    const size_t test_size = 100000;

    trit_vector_t* vec_unpacked = trit_vector_create(test_size, TERNARY_PACK_NONE);
    trit_vector_t* vec_2bit = trit_vector_create(test_size, TERNARY_PACK_2BIT);
    trit_vector_t* vec_base243 = trit_vector_create(test_size, TERNARY_PACK_BASE243);

    ASSERT_NE(vec_unpacked, nullptr);
    ASSERT_NE(vec_2bit, nullptr);
    ASSERT_NE(vec_base243, nullptr);

    // Verify packed_bytes matches expected
    EXPECT_EQ(vec_unpacked->packed_bytes, test_size);
    EXPECT_EQ(vec_2bit->packed_bytes, (test_size + 3) / 4);
    EXPECT_EQ(vec_base243->packed_bytes, (test_size + 4) / 5);

    std::cout << "Vector allocation for " << test_size << " trits:" << std::endl;
    std::cout << "  Unpacked: " << vec_unpacked->packed_bytes << " bytes" << std::endl;
    std::cout << "  2-bit: " << vec_2bit->packed_bytes << " bytes" << std::endl;
    std::cout << "  Base-243: " << vec_base243->packed_bytes << " bytes" << std::endl;

    trit_vector_destroy(vec_unpacked);
    trit_vector_destroy(vec_2bit);
    trit_vector_destroy(vec_base243);
}

TEST_F(TernaryMemoryRegressionTest, MatrixAllocationSize) {
    // WHAT: Verify matrix allocation matches expected footprint
    // WHY:  Matrix memory is critical for SNN weight matrices
    // BASELINE: 1000x1000 matrix memory footprint

    const size_t rows = 1000;
    const size_t cols = 1000;
    const size_t total = rows * cols;

    trit_matrix_t* mat_unpacked = trit_matrix_create(rows, cols, TERNARY_PACK_NONE);
    trit_matrix_t* mat_2bit = trit_matrix_create(rows, cols, TERNARY_PACK_2BIT);
    trit_matrix_t* mat_base243 = trit_matrix_create(rows, cols, TERNARY_PACK_BASE243);

    ASSERT_NE(mat_unpacked, nullptr);
    ASSERT_NE(mat_2bit, nullptr);
    ASSERT_NE(mat_base243, nullptr);

    // Verify packed_bytes matches expected
    EXPECT_EQ(mat_unpacked->packed_bytes, total);
    EXPECT_EQ(mat_2bit->packed_bytes, (total + 3) / 4);
    EXPECT_EQ(mat_base243->packed_bytes, (total + 4) / 5);

    // Compare to float32 equivalent
    size_t float_bytes = total * sizeof(float);  // 4 MB

    std::cout << "1000x1000 matrix memory comparison:" << std::endl;
    std::cout << "  Float32: " << float_bytes / KILO << " KB" << std::endl;
    std::cout << "  Ternary unpacked: " << mat_unpacked->packed_bytes / KILO << " KB" << std::endl;
    std::cout << "  Ternary 2-bit: " << mat_2bit->packed_bytes / KILO << " KB" << std::endl;
    std::cout << "  Ternary base-243: " << mat_base243->packed_bytes / KILO << " KB" << std::endl;

    trit_matrix_destroy(mat_unpacked);
    trit_matrix_destroy(mat_2bit);
    trit_matrix_destroy(mat_base243);
}

//=============================================================================
// Edge Case Memory Tests
//=============================================================================

TEST_F(TernaryMemoryRegressionTest, SmallSizeMemoryEfficiency) {
    // WHAT: Verify memory efficiency for small arrays
    // WHY:  Small arrays may have alignment overhead
    // BASELINE: No more than 1 byte overhead per byte needed

    for (size_t size = 1; size <= 20; size++) {
        size_t bytes_none = calculate_packed_bytes(size, TERNARY_PACK_NONE);
        size_t bytes_2bit = calculate_packed_bytes(size, TERNARY_PACK_2BIT);
        size_t bytes_243 = calculate_packed_bytes(size, TERNARY_PACK_BASE243);

        EXPECT_EQ(bytes_none, size);
        EXPECT_EQ(bytes_2bit, (size + 3) / 4);
        EXPECT_EQ(bytes_243, (size + 4) / 5);
    }
}

TEST_F(TernaryMemoryRegressionTest, BoundaryAlignmentMemory) {
    // WHAT: Verify memory at packing boundaries
    // WHY:  Boundaries (4, 5 trits) are critical for correct sizing
    // BASELINE: Exact byte counts at boundaries

    // 2-bit packing boundaries (4 trits per byte)
    EXPECT_EQ(calculate_packed_bytes(4, TERNARY_PACK_2BIT), 1UL);
    EXPECT_EQ(calculate_packed_bytes(5, TERNARY_PACK_2BIT), 2UL);
    EXPECT_EQ(calculate_packed_bytes(8, TERNARY_PACK_2BIT), 2UL);
    EXPECT_EQ(calculate_packed_bytes(9, TERNARY_PACK_2BIT), 3UL);

    // Base-243 packing boundaries (5 trits per byte)
    EXPECT_EQ(calculate_packed_bytes(5, TERNARY_PACK_BASE243), 1UL);
    EXPECT_EQ(calculate_packed_bytes(6, TERNARY_PACK_BASE243), 2UL);
    EXPECT_EQ(calculate_packed_bytes(10, TERNARY_PACK_BASE243), 2UL);
    EXPECT_EQ(calculate_packed_bytes(11, TERNARY_PACK_BASE243), 3UL);
}

//=============================================================================
// Large Scale Memory Tests
//=============================================================================

TEST_F(TernaryMemoryRegressionTest, LargeMatrixMemorySavings) {
    // WHAT: Verify memory savings for large weight matrices
    // WHY:  SNN applications require millions of weights
    // BASELINE: 10M weights should show expected compression

    const size_t num_weights = 10 * MILLION;

    size_t float_bytes = num_weights * sizeof(float);  // 40 MB
    size_t ternary_none = calculate_packed_bytes(num_weights, TERNARY_PACK_NONE);
    size_t ternary_2bit = calculate_packed_bytes(num_weights, TERNARY_PACK_2BIT);
    size_t ternary_243 = calculate_packed_bytes(num_weights, TERNARY_PACK_BASE243);

    std::cout << "10M weights memory comparison:" << std::endl;
    std::cout << "  Float32: " << float_bytes / MEGA << " MB" << std::endl;
    std::cout << "  Ternary unpacked: " << ternary_none / MEGA << " MB (4x)" << std::endl;
    std::cout << "  Ternary 2-bit: " << ternary_2bit / MEGA << " MB (16x)" << std::endl;
    std::cout << "  Ternary base-243: " << ternary_243 / MEGA << " MB (20x)" << std::endl;

    // Verify compression ratios
    EXPECT_GE(static_cast<double>(float_bytes) / ternary_none, 4.0);
    EXPECT_GE(static_cast<double>(float_bytes) / ternary_2bit, 16.0);
    EXPECT_GE(static_cast<double>(float_bytes) / ternary_243, 20.0);
}

TEST_F(TernaryMemoryRegressionTest, StructOverheadConstant) {
    // WHAT: Verify struct overhead is constant regardless of size
    // WHY:  Overhead should not scale with data size
    // BASELINE: sizeof(trit_vector_t) is constant

    size_t struct_size = sizeof(trit_vector_t);

    std::cout << "trit_vector_t struct size: " << struct_size << " bytes" << std::endl;

    // Struct size should be reasonable (under 128 bytes)
    EXPECT_LE(struct_size, 128UL);

    // Create vectors of different sizes and verify struct size is constant
    trit_vector_t* v1 = trit_vector_create(100, TERNARY_PACK_NONE);
    trit_vector_t* v2 = trit_vector_create(1000000, TERNARY_PACK_NONE);

    ASSERT_NE(v1, nullptr);
    ASSERT_NE(v2, nullptr);

    // Struct memory is the same, only data differs
    // (We can't directly measure this, but packed_bytes should reflect data size only)
    EXPECT_EQ(v1->packed_bytes, 100UL);
    EXPECT_EQ(v2->packed_bytes, 1000000UL);

    trit_vector_destroy(v1);
    trit_vector_destroy(v2);
}

//=============================================================================
// Memory Stability Over Time
//=============================================================================

TEST_F(TernaryMemoryRegressionTest, RepeatedAllocationStability) {
    // WHAT: Verify memory allocation is stable over repeated create/destroy
    // WHY:  Memory leaks would cause growth over time
    // BASELINE: No memory growth after many allocations

    const size_t num_iterations = 1000;
    const size_t vector_size = 10000;

    for (size_t i = 0; i < num_iterations; i++) {
        trit_vector_t* vec = trit_vector_create(vector_size, TERNARY_PACK_BASE243);
        ASSERT_NE(vec, nullptr);

        // Use the vector briefly
        trit_vector_set(vec, 0, TRIT_POSITIVE);
        trit_vector_set(vec, vector_size - 1, TRIT_NEGATIVE);

        trit_vector_destroy(vec);
    }

    // If we get here without crash/OOM, test passes
    SUCCEED();
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
