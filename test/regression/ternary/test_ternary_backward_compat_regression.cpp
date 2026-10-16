//=============================================================================
// test_ternary_backward_compat_regression.cpp - Ternary Backward Compatibility
//=============================================================================
/**
 * @file test_ternary_backward_compat_regression.cpp
 * @brief Regression tests for ternary API backward compatibility
 *
 * WHAT: Test that existing float-based code still works with ternary
 * WHY:  API changes must not break existing code
 * HOW:  Verify API contracts, config migration, and interoperability
 *
 * REGRESSION CATEGORIES:
 * - API signature stability
 * - Return value conventions
 * - Error code stability
 * - Data structure layout
 * - Config parameter compatibility
 *
 * @author NIMCP Test Team
 * @date 2025-12-31
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>

// Headers have their own extern "C" guards
#include "utils/ternary/nimcp_ternary.h"

//=============================================================================
// Test Fixture
//=============================================================================

class TernaryBackwardCompatRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

//=============================================================================
// API Signature Stability Tests
//=============================================================================

TEST_F(TernaryBackwardCompatRegressionTest, VectorCreateSignature) {
    // WHAT: Verify trit_vector_create signature is stable
    // WHY:  Function signature changes break compilation
    // BASELINE: trit_vector_t* trit_vector_create(size_t, ternary_pack_mode_t)

    // This test verifies the signature by calling the function
    trit_vector_t* vec = trit_vector_create(100, TERNARY_PACK_NONE);
    ASSERT_NE(vec, nullptr);

    // Verify basic properties
    EXPECT_EQ(vec->length, 100UL);
    EXPECT_EQ(vec->pack_mode, TERNARY_PACK_NONE);
    EXPECT_EQ(vec->magic, TERNARY_MAGIC);

    trit_vector_destroy(vec);
}

TEST_F(TernaryBackwardCompatRegressionTest, VectorSetGetSignature) {
    // WHAT: Verify trit_vector_set/get signatures are stable
    // WHY:  Accessor signature changes break code
    // BASELINE: ternary_error_t set(vec, index, value), trit_t get(vec, index)

    trit_vector_t* vec = trit_vector_create(10, TERNARY_PACK_NONE);
    ASSERT_NE(vec, nullptr);

    // Set returns error code
    ternary_error_t err = trit_vector_set(vec, 0, TRIT_POSITIVE);
    EXPECT_EQ(err, TERNARY_OK);

    // Get returns trit value
    trit_t val = trit_vector_get(vec, 0);
    EXPECT_EQ(val, TRIT_POSITIVE);

    trit_vector_destroy(vec);
}

TEST_F(TernaryBackwardCompatRegressionTest, MatrixCreateSignature) {
    // WHAT: Verify trit_matrix_create signature is stable
    // WHY:  Matrix API is core functionality
    // BASELINE: trit_matrix_t* trit_matrix_create(rows, cols, pack_mode)

    trit_matrix_t* mat = trit_matrix_create(10, 20, TERNARY_PACK_NONE);
    ASSERT_NE(mat, nullptr);

    EXPECT_EQ(mat->rows, 10UL);
    EXPECT_EQ(mat->cols, 20UL);
    EXPECT_EQ(mat->numel, 200UL);
    EXPECT_EQ(mat->magic, TERNARY_MAGIC);

    trit_matrix_destroy(mat);
}

TEST_F(TernaryBackwardCompatRegressionTest, ConversionFunctionSignatures) {
    // WHAT: Verify conversion function signatures are stable
    // WHY:  Conversion is the interface to float-based code
    // BASELINE: trit_from_float_threshold(float, float), trit_to_float(trit_t)

    // Float to trit
    trit_t t1 = trit_from_float_threshold(1.0f, 0.5f);
    EXPECT_EQ(t1, TRIT_POSITIVE);

    // Trit to float
    float f1 = trit_to_float(TRIT_POSITIVE);
    EXPECT_FLOAT_EQ(f1, 1.0f);

    // Scaled conversion
    float f2 = trit_to_float_scaled(TRIT_POSITIVE, 2.0f);
    EXPECT_FLOAT_EQ(f2, 2.0f);
}

//=============================================================================
// Error Code Stability Tests
//=============================================================================

TEST_F(TernaryBackwardCompatRegressionTest, ErrorCodeValues) {
    // WHAT: Verify error code values are stable
    // WHY:  Error codes may be used in switch statements
    // BASELINE: Error codes must match documented values

    EXPECT_EQ(TERNARY_OK, 0);
    EXPECT_EQ(TERNARY_ERR_NULL, -1);
    EXPECT_EQ(TERNARY_ERR_SHAPE, -2);
    EXPECT_EQ(TERNARY_ERR_ALLOC, -3);
    EXPECT_EQ(TERNARY_ERR_INDEX, -4);
    EXPECT_EQ(TERNARY_ERR_INVALID, -5);
    EXPECT_EQ(TERNARY_ERR_OVERFLOW, -6);
    EXPECT_EQ(TERNARY_ERR_CONVERSION, -7);
    EXPECT_EQ(TERNARY_ERR_PACK_MODE, -8);
}

TEST_F(TernaryBackwardCompatRegressionTest, ErrorHandlingBehavior) {
    // WHAT: Verify error handling behavior is consistent
    // WHY:  Error handling patterns must be reliable
    // BASELINE: NULL inputs return appropriate errors

    // NULL vector set
    ternary_error_t err = trit_vector_set(nullptr, 0, TRIT_POSITIVE);
    EXPECT_EQ(err, TERNARY_ERR_NULL);

    // NULL vector get returns UNKNOWN
    trit_t val = trit_vector_get(nullptr, 0);
    EXPECT_EQ(val, TRIT_UNKNOWN);

    // Out of bounds
    trit_vector_t* vec = trit_vector_create(10, TERNARY_PACK_NONE);
    ASSERT_NE(vec, nullptr);

    err = trit_vector_set(vec, 100, TRIT_POSITIVE);  // Index 100 is out of bounds
    EXPECT_EQ(err, TERNARY_ERR_INDEX);

    val = trit_vector_get(vec, 100);
    EXPECT_EQ(val, TRIT_UNKNOWN);

    // Invalid trit value
    err = trit_vector_set(vec, 0, (trit_t)5);  // 5 is not a valid trit
    EXPECT_EQ(err, TERNARY_ERR_INVALID);

    trit_vector_destroy(vec);
}

//=============================================================================
// Data Structure Layout Tests
//=============================================================================

TEST_F(TernaryBackwardCompatRegressionTest, VectorStructLayout) {
    // WHAT: Verify trit_vector_t struct has expected fields
    // WHY:  Struct layout changes break binary compatibility
    // BASELINE: All documented fields must exist

    trit_vector_t* vec = trit_vector_create(100, TERNARY_PACK_NONE);
    ASSERT_NE(vec, nullptr);

    // These field accesses verify struct layout
    uint32_t magic = vec->magic;
    size_t length = vec->length;
    size_t capacity = vec->capacity;
    ternary_pack_mode_t mode = vec->pack_mode;
    size_t packed_bytes = vec->packed_bytes;
    bool owns = vec->owns_data;

    EXPECT_EQ(magic, TERNARY_MAGIC);
    EXPECT_EQ(length, 100UL);
    EXPECT_EQ(capacity, 100UL);
    EXPECT_EQ(mode, TERNARY_PACK_NONE);
    EXPECT_EQ(packed_bytes, 100UL);
    EXPECT_TRUE(owns);

    trit_vector_destroy(vec);
}

TEST_F(TernaryBackwardCompatRegressionTest, MatrixStructLayout) {
    // WHAT: Verify trit_matrix_t struct has expected fields
    // WHY:  Struct layout changes break binary compatibility
    // BASELINE: All documented fields must exist

    trit_matrix_t* mat = trit_matrix_create(10, 20, TERNARY_PACK_2BIT);
    ASSERT_NE(mat, nullptr);

    // Verify field access
    EXPECT_EQ(mat->magic, TERNARY_MAGIC);
    EXPECT_EQ(mat->rows, 10UL);
    EXPECT_EQ(mat->cols, 20UL);
    EXPECT_EQ(mat->numel, 200UL);
    EXPECT_EQ(mat->pack_mode, TERNARY_PACK_2BIT);
    EXPECT_EQ(mat->row_stride, 20UL);
    EXPECT_TRUE(mat->owns_data);

    trit_matrix_destroy(mat);
}

TEST_F(TernaryBackwardCompatRegressionTest, ExtendedTritStructLayout) {
    // WHAT: Verify trit_extended_t struct has expected fields
    // WHY:  Extended trit is used in reasoning systems
    // BASELINE: All documented fields must exist

    trit_extended_t ext = trit_extended_create(TRIT_POSITIVE, 0.8f);

    EXPECT_EQ(ext.value, TRIT_POSITIVE);
    EXPECT_FLOAT_EQ(ext.confidence, 0.8f);
    EXPECT_FLOAT_EQ(ext.uncertainty, 0.2f);
    EXPECT_EQ(ext.inference_depth, 0U);
}

//=============================================================================
// Logic Operation Compatibility Tests
//=============================================================================

TEST_F(TernaryBackwardCompatRegressionTest, KleeneLogicTruthTables) {
    // WHAT: Verify Kleene logic truth tables are stable
    // WHY:  Logic semantics must not change
    // BASELINE: Truth tables match documentation

    // NOT
    EXPECT_EQ(trit_not(TRIT_NEGATIVE), TRIT_POSITIVE);
    EXPECT_EQ(trit_not(TRIT_UNKNOWN), TRIT_UNKNOWN);
    EXPECT_EQ(trit_not(TRIT_POSITIVE), TRIT_NEGATIVE);

    // AND (min semantics)
    EXPECT_EQ(trit_and(TRIT_POSITIVE, TRIT_POSITIVE), TRIT_POSITIVE);
    EXPECT_EQ(trit_and(TRIT_POSITIVE, TRIT_UNKNOWN), TRIT_UNKNOWN);
    EXPECT_EQ(trit_and(TRIT_POSITIVE, TRIT_NEGATIVE), TRIT_NEGATIVE);
    EXPECT_EQ(trit_and(TRIT_UNKNOWN, TRIT_UNKNOWN), TRIT_UNKNOWN);
    EXPECT_EQ(trit_and(TRIT_NEGATIVE, TRIT_NEGATIVE), TRIT_NEGATIVE);

    // OR (max semantics)
    EXPECT_EQ(trit_or(TRIT_NEGATIVE, TRIT_NEGATIVE), TRIT_NEGATIVE);
    EXPECT_EQ(trit_or(TRIT_NEGATIVE, TRIT_UNKNOWN), TRIT_UNKNOWN);
    EXPECT_EQ(trit_or(TRIT_NEGATIVE, TRIT_POSITIVE), TRIT_POSITIVE);
    EXPECT_EQ(trit_or(TRIT_UNKNOWN, TRIT_UNKNOWN), TRIT_UNKNOWN);
    EXPECT_EQ(trit_or(TRIT_POSITIVE, TRIT_POSITIVE), TRIT_POSITIVE);
}

TEST_F(TernaryBackwardCompatRegressionTest, LukasiewiczLogicTruthTables) {
    // WHAT: Verify Lukasiewicz logic truth tables are stable
    // WHY:  Alternative logic semantics must be consistent
    // BASELINE: Truth tables match documentation

    // Lukasiewicz implication differs from Kleene
    EXPECT_EQ(trit_luk_implies(TRIT_UNKNOWN, TRIT_UNKNOWN), TRIT_POSITIVE);
    EXPECT_EQ(trit_luk_implies(TRIT_POSITIVE, TRIT_NEGATIVE), TRIT_NEGATIVE);
    EXPECT_EQ(trit_luk_implies(TRIT_NEGATIVE, TRIT_POSITIVE), TRIT_POSITIVE);

    // Lukasiewicz equivalence
    EXPECT_EQ(trit_luk_equiv(TRIT_POSITIVE, TRIT_POSITIVE), TRIT_POSITIVE);
    EXPECT_EQ(trit_luk_equiv(TRIT_NEGATIVE, TRIT_NEGATIVE), TRIT_POSITIVE);
    EXPECT_EQ(trit_luk_equiv(TRIT_UNKNOWN, TRIT_UNKNOWN), TRIT_POSITIVE);
}

//=============================================================================
// Interoperability Tests
//=============================================================================

TEST_F(TernaryBackwardCompatRegressionTest, FloatArrayInterop) {
    // WHAT: Verify ternary interoperates with float arrays
    // WHY:  Most existing code uses float arrays
    // BASELINE: Seamless conversion both ways

    // Create float array
    std::vector<float> floats = {1.0f, -0.3f, 0.0f, -1.0f, 0.8f};
    const size_t n = floats.size();

    // Convert to ternary
    trit_vector_t* vec = trit_vector_from_floats(floats.data(), n, 0.5f, TERNARY_PACK_NONE);
    ASSERT_NE(vec, nullptr);

    // Convert back to float
    std::vector<float> output(n);
    ternary_error_t err = trit_vector_to_floats(vec, output.data(), 1.0f);
    EXPECT_EQ(err, TERNARY_OK);

    // Verify expected values
    EXPECT_FLOAT_EQ(output[0], 1.0f);   // 1.0 >= 0.5 -> +1
    EXPECT_FLOAT_EQ(output[1], 0.0f);   // -0.3 in (-0.5, 0.5) -> 0
    EXPECT_FLOAT_EQ(output[2], 0.0f);   // 0.0 in (-0.5, 0.5) -> 0
    EXPECT_FLOAT_EQ(output[3], -1.0f);  // -1.0 <= -0.5 -> -1
    EXPECT_FLOAT_EQ(output[4], 1.0f);   // 0.8 >= 0.5 -> +1

    trit_vector_destroy(vec);
}

TEST_F(TernaryBackwardCompatRegressionTest, RawPointerAccess) {
    // WHAT: Verify raw pointer access for performance-critical code
    // WHY:  Some code needs direct buffer access
    // BASELINE: trit_vector_data returns valid pointer for unpacked

    trit_vector_t* vec = trit_vector_create(100, TERNARY_PACK_NONE);
    ASSERT_NE(vec, nullptr);

    // Get raw pointer
    trit_t* data = trit_vector_data(vec);
    ASSERT_NE(data, nullptr);

    // Direct write
    data[0] = TRIT_POSITIVE;
    data[1] = TRIT_NEGATIVE;

    // Verify through API
    EXPECT_EQ(trit_vector_get(vec, 0), TRIT_POSITIVE);
    EXPECT_EQ(trit_vector_get(vec, 1), TRIT_NEGATIVE);

    // Packed vectors should return NULL
    trit_vector_t* packed = trit_vector_create(100, TERNARY_PACK_2BIT);
    ASSERT_NE(packed, nullptr);
    EXPECT_EQ(trit_vector_data(packed), nullptr);

    trit_vector_destroy(vec);
    trit_vector_destroy(packed);
}

//=============================================================================
// Version Compatibility Tests
//=============================================================================

TEST_F(TernaryBackwardCompatRegressionTest, VersionConstants) {
    // WHAT: Verify version constants are defined
    // WHY:  Version checking is needed for compatibility
    // BASELINE: Version constants must be defined

    EXPECT_GE(NIMCP_TERNARY_VERSION_MAJOR, 1);
    EXPECT_GE(NIMCP_TERNARY_VERSION_MINOR, 0);
    EXPECT_GE(NIMCP_TERNARY_VERSION_PATCH, 0);

    const char* version = nimcp_ternary_version();
    ASSERT_NE(version, nullptr);
    EXPECT_NE(strlen(version), 0UL);
}

TEST_F(TernaryBackwardCompatRegressionTest, CapabilitiesStruct) {
    // WHAT: Verify capabilities can be queried
    // WHY:  Feature detection is needed for conditional code
    // BASELINE: Capabilities struct must be fillable

    ternary_capabilities_t caps;
    nimcp_ternary_get_capabilities(&caps);

    // These fields must exist
    (void)caps.has_simd;
    (void)caps.has_packed_base243;
    (void)caps.has_packed_2bit;
    (void)caps.max_vector_size;
    (void)caps.max_matrix_elements;

    // Base capabilities should be available
    EXPECT_TRUE(caps.has_packed_2bit);
    EXPECT_TRUE(caps.has_packed_base243);
    EXPECT_GT(caps.max_vector_size, 0UL);
}

//=============================================================================
// Convenience Macro Compatibility Tests
//=============================================================================

TEST_F(TernaryBackwardCompatRegressionTest, TritMacros) {
    // WHAT: Verify convenience macros are available
    // WHY:  Macros simplify common patterns
    // BASELINE: TRIT_* macros must be defined

    // Validation macros
    EXPECT_TRUE(TRIT_IS_VALID(TRIT_POSITIVE));
    EXPECT_TRUE(TRIT_IS_VALID(TRIT_UNKNOWN));
    EXPECT_TRUE(TRIT_IS_VALID(TRIT_NEGATIVE));
    EXPECT_FALSE(TRIT_IS_VALID((trit_t)5));

    // Clamp macro
    EXPECT_EQ(TRIT_CLAMP(-5), TRIT_NEGATIVE);
    EXPECT_EQ(TRIT_CLAMP(0), TRIT_UNKNOWN);
    EXPECT_EQ(TRIT_CLAMP(5), TRIT_POSITIVE);

    // Sign macro
    EXPECT_EQ(TRIT_SIGN(-10), TRIT_NEGATIVE);
    EXPECT_EQ(TRIT_SIGN(0), TRIT_UNKNOWN);
    EXPECT_EQ(TRIT_SIGN(10), TRIT_POSITIVE);
}

TEST_F(TernaryBackwardCompatRegressionTest, ContextAliases) {
    // WHAT: Verify context-specific aliases are available
    // WHY:  Different contexts use different terminology
    // BASELINE: All documented aliases must be defined

    // Neural context
    EXPECT_EQ(TRIT_INHIBITORY, TRIT_NEGATIVE);
    EXPECT_EQ(TRIT_SILENT, TRIT_UNKNOWN);
    EXPECT_EQ(TRIT_EXCITATORY, TRIT_POSITIVE);

    // Logic context
    EXPECT_EQ(TRIT_FALSE, TRIT_NEGATIVE);
    EXPECT_EQ(TRIT_TRUE, TRIT_POSITIVE);

    // Ethics context
    EXPECT_EQ(TRIT_FORBID, TRIT_NEGATIVE);
    EXPECT_EQ(TRIT_NEUTRAL, TRIT_UNKNOWN);
    EXPECT_EQ(TRIT_ALLOW, TRIT_POSITIVE);

    // Voting context
    EXPECT_EQ(TRIT_DISAGREE, TRIT_NEGATIVE);
    EXPECT_EQ(TRIT_ABSTAIN, TRIT_UNKNOWN);
    EXPECT_EQ(TRIT_AGREE, TRIT_POSITIVE);

    // Plasticity context
    EXPECT_EQ(TRIT_LTD, TRIT_NEGATIVE);
    EXPECT_EQ(TRIT_STABLE, TRIT_UNKNOWN);
    EXPECT_EQ(TRIT_LTP, TRIT_POSITIVE);
}

//=============================================================================
// Destruction Safety Tests
//=============================================================================

TEST_F(TernaryBackwardCompatRegressionTest, NullDestructionSafe) {
    // WHAT: Verify NULL destruction is safe
    // WHY:  NULL destruction must not crash
    // BASELINE: No crash on NULL destroy

    trit_vector_destroy(nullptr);  // Should not crash
    trit_matrix_destroy(nullptr);  // Should not crash

    SUCCEED();  // If we get here, no crash occurred
}

TEST_F(TernaryBackwardCompatRegressionTest, DoubleDestructionDetected) {
    // WHAT: Verify double destruction is detected
    // WHY:  Magic number should be cleared on destroy
    // BASELINE: Magic cleared after destroy (don't actually double-free)

    trit_vector_t* vec = trit_vector_create(10, TERNARY_PACK_NONE);
    ASSERT_NE(vec, nullptr);

    uint32_t magic_before = vec->magic;
    EXPECT_EQ(magic_before, TERNARY_MAGIC);

    // We can't safely test double-free, but we can verify magic is used
    // The implementation should clear magic on destroy

    trit_vector_destroy(vec);
    // vec is now invalid - don't access it
}

//=============================================================================
// Clone and Copy Compatibility Tests
//=============================================================================

TEST_F(TernaryBackwardCompatRegressionTest, VectorCloneDeepCopy) {
    // WHAT: Verify vector clone creates independent copy
    // WHY:  Clone must not share data with original
    // BASELINE: Modifying clone doesn't affect original

    trit_vector_t* original = trit_vector_create(10, TERNARY_PACK_NONE);
    ASSERT_NE(original, nullptr);

    trit_vector_set(original, 0, TRIT_POSITIVE);

    trit_vector_t* clone = trit_vector_clone(original);
    ASSERT_NE(clone, nullptr);

    // Verify clone has same data
    EXPECT_EQ(trit_vector_get(clone, 0), TRIT_POSITIVE);

    // Modify clone
    trit_vector_set(clone, 0, TRIT_NEGATIVE);

    // Original should be unchanged
    EXPECT_EQ(trit_vector_get(original, 0), TRIT_POSITIVE);
    EXPECT_EQ(trit_vector_get(clone, 0), TRIT_NEGATIVE);

    trit_vector_destroy(original);
    trit_vector_destroy(clone);
}

TEST_F(TernaryBackwardCompatRegressionTest, MatrixCloneDeepCopy) {
    // WHAT: Verify matrix clone creates independent copy
    // WHY:  Matrix clone must work like vector clone
    // BASELINE: Independent copies

    trit_matrix_t* original = trit_matrix_create(5, 5, TERNARY_PACK_NONE);
    ASSERT_NE(original, nullptr);

    trit_matrix_set(original, 0, 0, TRIT_POSITIVE);

    trit_matrix_t* clone = trit_matrix_clone(original);
    ASSERT_NE(clone, nullptr);

    EXPECT_EQ(trit_matrix_get(clone, 0, 0), TRIT_POSITIVE);

    trit_matrix_set(clone, 0, 0, TRIT_NEGATIVE);

    EXPECT_EQ(trit_matrix_get(original, 0, 0), TRIT_POSITIVE);
    EXPECT_EQ(trit_matrix_get(clone, 0, 0), TRIT_NEGATIVE);

    trit_matrix_destroy(original);
    trit_matrix_destroy(clone);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
