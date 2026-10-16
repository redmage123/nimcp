//=============================================================================
// test_ternary_core.cpp - Unit Tests for Ternary Logic Module
//=============================================================================
/**
 * @file test_ternary_core.cpp
 * @brief Comprehensive unit tests for ternary types, logic, storage, vector, matrix
 *
 * WHAT: Tests all core ternary functionality
 * WHY:  Validate correctness of three-valued logic implementation
 * HOW:  GTest-based unit tests with edge cases
 *
 * @author NIMCP Development Team
 * @date 2025-12-21
 */

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "utils/ternary/nimcp_ternary_types.h"
#include "utils/ternary/nimcp_ternary_logic.h"
#include "utils/ternary/nimcp_ternary_storage.h"
#include "utils/ternary/nimcp_ternary_vector.h"
#include "utils/ternary/nimcp_ternary_matrix.h"
#include "utils/ternary/nimcp_ternary_convert.h"

//=============================================================================
// Type Tests
//=============================================================================

class TernaryTypesTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(TernaryTypesTest, TritConstants) {
    EXPECT_EQ(TRIT_NEGATIVE, -1);
    EXPECT_EQ(TRIT_UNKNOWN, 0);
    EXPECT_EQ(TRIT_POSITIVE, +1);
}

TEST_F(TernaryTypesTest, TritAliases) {
    // Logic aliases
    EXPECT_EQ(TRIT_FALSE, TRIT_NEGATIVE);
    EXPECT_EQ(TRIT_TRUE, TRIT_POSITIVE);

    // Neural aliases
    EXPECT_EQ(TRIT_INHIBITORY, TRIT_NEGATIVE);
    EXPECT_EQ(TRIT_SILENT, TRIT_UNKNOWN);
    EXPECT_EQ(TRIT_EXCITATORY, TRIT_POSITIVE);

    // Ethics aliases
    EXPECT_EQ(TRIT_FORBID, TRIT_NEGATIVE);
    EXPECT_EQ(TRIT_NEUTRAL, TRIT_UNKNOWN);
    EXPECT_EQ(TRIT_ALLOW, TRIT_POSITIVE);

    // Voting aliases
    EXPECT_EQ(TRIT_DISAGREE, TRIT_NEGATIVE);
    EXPECT_EQ(TRIT_ABSTAIN, TRIT_UNKNOWN);
    EXPECT_EQ(TRIT_AGREE, TRIT_POSITIVE);

    // Plasticity aliases
    EXPECT_EQ(TRIT_LTD, TRIT_NEGATIVE);
    EXPECT_EQ(TRIT_STABLE, TRIT_UNKNOWN);
    EXPECT_EQ(TRIT_LTP, TRIT_POSITIVE);
}

TEST_F(TernaryTypesTest, TritValidation) {
    EXPECT_TRUE(trit_is_valid(TRIT_NEGATIVE));
    EXPECT_TRUE(trit_is_valid(TRIT_UNKNOWN));
    EXPECT_TRUE(trit_is_valid(TRIT_POSITIVE));
    EXPECT_FALSE(trit_is_valid((trit_t)-2));
    EXPECT_FALSE(trit_is_valid((trit_t)+2));
    EXPECT_FALSE(trit_is_valid((trit_t)100));
}

TEST_F(TernaryTypesTest, TritFromInt) {
    EXPECT_EQ(trit_from_int(-100), TRIT_NEGATIVE);
    EXPECT_EQ(trit_from_int(-1), TRIT_NEGATIVE);
    EXPECT_EQ(trit_from_int(0), TRIT_UNKNOWN);
    EXPECT_EQ(trit_from_int(1), TRIT_POSITIVE);
    EXPECT_EQ(trit_from_int(100), TRIT_POSITIVE);
}

TEST_F(TernaryTypesTest, TritClamp) {
    EXPECT_EQ(TRIT_CLAMP(-5), TRIT_NEGATIVE);
    EXPECT_EQ(TRIT_CLAMP(-1), TRIT_NEGATIVE);
    EXPECT_EQ(TRIT_CLAMP(0), TRIT_UNKNOWN);
    EXPECT_EQ(TRIT_CLAMP(1), TRIT_POSITIVE);
    EXPECT_EQ(TRIT_CLAMP(5), TRIT_POSITIVE);
}

TEST_F(TernaryTypesTest, TritSign) {
    EXPECT_EQ(TRIT_SIGN(-100), TRIT_NEGATIVE);
    EXPECT_EQ(TRIT_SIGN(0), TRIT_UNKNOWN);
    EXPECT_EQ(TRIT_SIGN(100), TRIT_POSITIVE);
}

TEST_F(TernaryTypesTest, TritToString) {
    EXPECT_STREQ(trit_to_string(TRIT_NEGATIVE), "-1");
    EXPECT_STREQ(trit_to_string(TRIT_UNKNOWN), "0");
    EXPECT_STREQ(trit_to_string(TRIT_POSITIVE), "+1");
}

TEST_F(TernaryTypesTest, TritName) {
    EXPECT_STREQ(trit_name(TRIT_NEGATIVE), "NEGATIVE");
    EXPECT_STREQ(trit_name(TRIT_UNKNOWN), "UNKNOWN");
    EXPECT_STREQ(trit_name(TRIT_POSITIVE), "POSITIVE");
}

TEST_F(TernaryTypesTest, ExtendedTritCertain) {
    trit_extended_t ext = trit_extended_certain(TRIT_POSITIVE);
    EXPECT_EQ(ext.value, TRIT_POSITIVE);
    EXPECT_FLOAT_EQ(ext.confidence, 1.0f);
    EXPECT_FLOAT_EQ(ext.uncertainty, 0.0f);
    EXPECT_EQ(ext.inference_depth, 0U);
}

TEST_F(TernaryTypesTest, ExtendedTritCreate) {
    trit_extended_t ext = trit_extended_create(TRIT_NEGATIVE, 0.7f);
    EXPECT_EQ(ext.value, TRIT_NEGATIVE);
    EXPECT_FLOAT_EQ(ext.confidence, 0.7f);
    EXPECT_FLOAT_EQ(ext.uncertainty, 0.3f);
}

TEST_F(TernaryTypesTest, PackedBytesCalculation) {
    EXPECT_EQ(trit_packed_bytes(0, TERNARY_PACK_NONE), 0UL);
    EXPECT_EQ(trit_packed_bytes(10, TERNARY_PACK_NONE), 10UL);

    EXPECT_EQ(trit_packed_bytes(0, TERNARY_PACK_2BIT), 0UL);
    EXPECT_EQ(trit_packed_bytes(4, TERNARY_PACK_2BIT), 1UL);
    EXPECT_EQ(trit_packed_bytes(5, TERNARY_PACK_2BIT), 2UL);
    EXPECT_EQ(trit_packed_bytes(8, TERNARY_PACK_2BIT), 2UL);

    EXPECT_EQ(trit_packed_bytes(0, TERNARY_PACK_BASE243), 0UL);
    EXPECT_EQ(trit_packed_bytes(5, TERNARY_PACK_BASE243), 1UL);
    EXPECT_EQ(trit_packed_bytes(6, TERNARY_PACK_BASE243), 2UL);
    EXPECT_EQ(trit_packed_bytes(10, TERNARY_PACK_BASE243), 2UL);
}

//=============================================================================
// Logic Tests
//=============================================================================

class TernaryLogicTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(TernaryLogicTest, KleeneNot) {
    EXPECT_EQ(trit_not(TRIT_NEGATIVE), TRIT_POSITIVE);
    EXPECT_EQ(trit_not(TRIT_UNKNOWN), TRIT_UNKNOWN);
    EXPECT_EQ(trit_not(TRIT_POSITIVE), TRIT_NEGATIVE);
}

TEST_F(TernaryLogicTest, KleeneAnd) {
    // Truth table for AND (min operation)
    EXPECT_EQ(trit_and(TRIT_NEGATIVE, TRIT_NEGATIVE), TRIT_NEGATIVE);
    EXPECT_EQ(trit_and(TRIT_NEGATIVE, TRIT_UNKNOWN), TRIT_NEGATIVE);
    EXPECT_EQ(trit_and(TRIT_NEGATIVE, TRIT_POSITIVE), TRIT_NEGATIVE);

    EXPECT_EQ(trit_and(TRIT_UNKNOWN, TRIT_NEGATIVE), TRIT_NEGATIVE);
    EXPECT_EQ(trit_and(TRIT_UNKNOWN, TRIT_UNKNOWN), TRIT_UNKNOWN);
    EXPECT_EQ(trit_and(TRIT_UNKNOWN, TRIT_POSITIVE), TRIT_UNKNOWN);

    EXPECT_EQ(trit_and(TRIT_POSITIVE, TRIT_NEGATIVE), TRIT_NEGATIVE);
    EXPECT_EQ(trit_and(TRIT_POSITIVE, TRIT_UNKNOWN), TRIT_UNKNOWN);
    EXPECT_EQ(trit_and(TRIT_POSITIVE, TRIT_POSITIVE), TRIT_POSITIVE);
}

TEST_F(TernaryLogicTest, KleeneOr) {
    // Truth table for OR (max operation)
    EXPECT_EQ(trit_or(TRIT_NEGATIVE, TRIT_NEGATIVE), TRIT_NEGATIVE);
    EXPECT_EQ(trit_or(TRIT_NEGATIVE, TRIT_UNKNOWN), TRIT_UNKNOWN);
    EXPECT_EQ(trit_or(TRIT_NEGATIVE, TRIT_POSITIVE), TRIT_POSITIVE);

    EXPECT_EQ(trit_or(TRIT_UNKNOWN, TRIT_NEGATIVE), TRIT_UNKNOWN);
    EXPECT_EQ(trit_or(TRIT_UNKNOWN, TRIT_UNKNOWN), TRIT_UNKNOWN);
    EXPECT_EQ(trit_or(TRIT_UNKNOWN, TRIT_POSITIVE), TRIT_POSITIVE);

    EXPECT_EQ(trit_or(TRIT_POSITIVE, TRIT_NEGATIVE), TRIT_POSITIVE);
    EXPECT_EQ(trit_or(TRIT_POSITIVE, TRIT_UNKNOWN), TRIT_POSITIVE);
    EXPECT_EQ(trit_or(TRIT_POSITIVE, TRIT_POSITIVE), TRIT_POSITIVE);
}

TEST_F(TernaryLogicTest, KleeneXor) {
    EXPECT_EQ(trit_xor(TRIT_NEGATIVE, TRIT_NEGATIVE), TRIT_NEGATIVE);
    EXPECT_EQ(trit_xor(TRIT_NEGATIVE, TRIT_POSITIVE), TRIT_POSITIVE);
    EXPECT_EQ(trit_xor(TRIT_POSITIVE, TRIT_NEGATIVE), TRIT_POSITIVE);
    EXPECT_EQ(trit_xor(TRIT_POSITIVE, TRIT_POSITIVE), TRIT_NEGATIVE);
    EXPECT_EQ(trit_xor(TRIT_UNKNOWN, TRIT_POSITIVE), TRIT_UNKNOWN);
    EXPECT_EQ(trit_xor(TRIT_POSITIVE, TRIT_UNKNOWN), TRIT_UNKNOWN);
}

TEST_F(TernaryLogicTest, KleeneImplies) {
    // A → B = ¬A ∨ B
    EXPECT_EQ(trit_implies(TRIT_NEGATIVE, TRIT_NEGATIVE), TRIT_POSITIVE);  // ¬(-1) ∨ (-1) = 1 ∨ -1 = 1
    EXPECT_EQ(trit_implies(TRIT_NEGATIVE, TRIT_POSITIVE), TRIT_POSITIVE);  // ¬(-1) ∨ 1 = 1 ∨ 1 = 1
    EXPECT_EQ(trit_implies(TRIT_POSITIVE, TRIT_NEGATIVE), TRIT_NEGATIVE);  // ¬1 ∨ -1 = -1 ∨ -1 = -1
    EXPECT_EQ(trit_implies(TRIT_POSITIVE, TRIT_POSITIVE), TRIT_POSITIVE);  // ¬1 ∨ 1 = -1 ∨ 1 = 1
    EXPECT_EQ(trit_implies(TRIT_UNKNOWN, TRIT_POSITIVE), TRIT_POSITIVE);   // ¬0 ∨ 1 = 0 ∨ 1 = 1
}

TEST_F(TernaryLogicTest, KleeneIff) {
    EXPECT_EQ(trit_iff(TRIT_NEGATIVE, TRIT_NEGATIVE), TRIT_POSITIVE);
    EXPECT_EQ(trit_iff(TRIT_POSITIVE, TRIT_POSITIVE), TRIT_POSITIVE);
    EXPECT_EQ(trit_iff(TRIT_NEGATIVE, TRIT_POSITIVE), TRIT_NEGATIVE);
    EXPECT_EQ(trit_iff(TRIT_POSITIVE, TRIT_NEGATIVE), TRIT_NEGATIVE);
    EXPECT_EQ(trit_iff(TRIT_UNKNOWN, TRIT_UNKNOWN), TRIT_UNKNOWN);
}

TEST_F(TernaryLogicTest, LukasiewiczImplies) {
    // Łukasiewicz: min(1, 1 - a + b)
    EXPECT_EQ(trit_luk_implies(TRIT_NEGATIVE, TRIT_NEGATIVE), TRIT_POSITIVE);  // min(1, 1-(-1)+(-1)) = min(1,1) = 1
    EXPECT_EQ(trit_luk_implies(TRIT_POSITIVE, TRIT_NEGATIVE), TRIT_NEGATIVE);  // min(1, 1-1+(-1)) = min(1,-1) = -1
    EXPECT_EQ(trit_luk_implies(TRIT_POSITIVE, TRIT_POSITIVE), TRIT_POSITIVE);  // min(1, 1-1+1) = min(1,1) = 1
    EXPECT_EQ(trit_luk_implies(TRIT_UNKNOWN, TRIT_POSITIVE), TRIT_POSITIVE);   // min(1, 1-0+1) = min(1,2) = 1
}

TEST_F(TernaryLogicTest, Majority) {
    trit_t trits1[] = {TRIT_POSITIVE, TRIT_POSITIVE, TRIT_NEGATIVE};
    EXPECT_EQ(trit_majority(trits1, 3), TRIT_POSITIVE);  // 2 positive, 1 negative

    trit_t trits2[] = {TRIT_NEGATIVE, TRIT_NEGATIVE, TRIT_POSITIVE};
    EXPECT_EQ(trit_majority(trits2, 3), TRIT_NEGATIVE);  // 1 positive, 2 negative

    trit_t trits3[] = {TRIT_POSITIVE, TRIT_NEGATIVE};
    EXPECT_EQ(trit_majority(trits3, 2), TRIT_UNKNOWN);  // Tie

    trit_t trits4[] = {TRIT_UNKNOWN, TRIT_UNKNOWN, TRIT_UNKNOWN};
    EXPECT_EQ(trit_majority(trits4, 3), TRIT_UNKNOWN);  // All unknown
}

TEST_F(TernaryLogicTest, Unanimous) {
    trit_t trits1[] = {TRIT_POSITIVE, TRIT_POSITIVE, TRIT_POSITIVE};
    EXPECT_EQ(trit_unanimous(trits1, 3), TRIT_POSITIVE);

    trit_t trits2[] = {TRIT_NEGATIVE, TRIT_NEGATIVE, TRIT_NEGATIVE};
    EXPECT_EQ(trit_unanimous(trits2, 3), TRIT_NEGATIVE);

    trit_t trits3[] = {TRIT_POSITIVE, TRIT_NEGATIVE, TRIT_POSITIVE};
    EXPECT_EQ(trit_unanimous(trits3, 3), TRIT_UNKNOWN);  // Not unanimous
}

TEST_F(TernaryLogicTest, AllPositive) {
    trit_t trits1[] = {TRIT_POSITIVE, TRIT_POSITIVE, TRIT_POSITIVE};
    EXPECT_EQ(trit_all(trits1, 3), TRIT_POSITIVE);

    trit_t trits2[] = {TRIT_POSITIVE, TRIT_UNKNOWN, TRIT_POSITIVE};
    EXPECT_EQ(trit_all(trits2, 3), TRIT_UNKNOWN);

    trit_t trits3[] = {TRIT_POSITIVE, TRIT_NEGATIVE, TRIT_POSITIVE};
    EXPECT_EQ(trit_all(trits3, 3), TRIT_NEGATIVE);
}

TEST_F(TernaryLogicTest, AnyPositive) {
    trit_t trits1[] = {TRIT_NEGATIVE, TRIT_NEGATIVE, TRIT_POSITIVE};
    EXPECT_EQ(trit_any(trits1, 3), TRIT_POSITIVE);

    trit_t trits2[] = {TRIT_NEGATIVE, TRIT_UNKNOWN, TRIT_NEGATIVE};
    EXPECT_EQ(trit_any(trits2, 3), TRIT_UNKNOWN);

    trit_t trits3[] = {TRIT_NEGATIVE, TRIT_NEGATIVE, TRIT_NEGATIVE};
    EXPECT_EQ(trit_any(trits3, 3), TRIT_NEGATIVE);
}

//=============================================================================
// Storage Tests
//=============================================================================

class TernaryStorageTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(TernaryStorageTest, Pack4Unpack4RoundTrip) {
    trit_t input[4] = {TRIT_NEGATIVE, TRIT_UNKNOWN, TRIT_POSITIVE, TRIT_NEGATIVE};
    trit_packed2_t packed = trit_pack4(input);

    trit_t output[4];
    trit_unpack4(packed, output);

    EXPECT_EQ(output[0], TRIT_NEGATIVE);
    EXPECT_EQ(output[1], TRIT_UNKNOWN);
    EXPECT_EQ(output[2], TRIT_POSITIVE);
    EXPECT_EQ(output[3], TRIT_NEGATIVE);
}

TEST_F(TernaryStorageTest, Pack4AllValues) {
    trit_t all_neg[4] = {TRIT_NEGATIVE, TRIT_NEGATIVE, TRIT_NEGATIVE, TRIT_NEGATIVE};
    trit_t all_unk[4] = {TRIT_UNKNOWN, TRIT_UNKNOWN, TRIT_UNKNOWN, TRIT_UNKNOWN};
    trit_t all_pos[4] = {TRIT_POSITIVE, TRIT_POSITIVE, TRIT_POSITIVE, TRIT_POSITIVE};

    trit_t output[4];

    trit_unpack4(trit_pack4(all_neg), output);
    for (int i = 0; i < 4; i++) EXPECT_EQ(output[i], TRIT_NEGATIVE);

    trit_unpack4(trit_pack4(all_unk), output);
    for (int i = 0; i < 4; i++) EXPECT_EQ(output[i], TRIT_UNKNOWN);

    trit_unpack4(trit_pack4(all_pos), output);
    for (int i = 0; i < 4; i++) EXPECT_EQ(output[i], TRIT_POSITIVE);
}

TEST_F(TernaryStorageTest, Pack5Unpack5RoundTrip) {
    trit_t input[5] = {TRIT_POSITIVE, TRIT_NEGATIVE, TRIT_UNKNOWN, TRIT_POSITIVE, TRIT_NEGATIVE};
    trit_packed243_t packed = trit_pack5(input);

    trit_t output[5];
    trit_unpack5(packed, output);

    EXPECT_EQ(output[0], TRIT_POSITIVE);
    EXPECT_EQ(output[1], TRIT_NEGATIVE);
    EXPECT_EQ(output[2], TRIT_UNKNOWN);
    EXPECT_EQ(output[3], TRIT_POSITIVE);
    EXPECT_EQ(output[4], TRIT_NEGATIVE);
}

TEST_F(TernaryStorageTest, Pack5AllCombinations) {
    // Test all 243 possible combinations
    trit_t input[5];
    trit_t output[5];

    for (int t0 = -1; t0 <= 1; t0++) {
        for (int t1 = -1; t1 <= 1; t1++) {
            for (int t2 = -1; t2 <= 1; t2++) {
                for (int t3 = -1; t3 <= 1; t3++) {
                    for (int t4 = -1; t4 <= 1; t4++) {
                        input[0] = (trit_t)t0;
                        input[1] = (trit_t)t1;
                        input[2] = (trit_t)t2;
                        input[3] = (trit_t)t3;
                        input[4] = (trit_t)t4;

                        trit_packed243_t packed = trit_pack5(input);
                        EXPECT_LT(packed, 243);  // Must fit in byte

                        trit_unpack5(packed, output);
                        EXPECT_EQ(output[0], input[0]);
                        EXPECT_EQ(output[1], input[1]);
                        EXPECT_EQ(output[2], input[2]);
                        EXPECT_EQ(output[3], input[3]);
                        EXPECT_EQ(output[4], input[4]);
                    }
                }
            }
        }
    }
}

TEST_F(TernaryStorageTest, Pack5MaxValue) {
    // Maximum value is when all trits are +1 (maps to 2)
    // 2 + 3*2 + 9*2 + 27*2 + 81*2 = 2 + 6 + 18 + 54 + 162 = 242
    trit_t all_pos[5] = {TRIT_POSITIVE, TRIT_POSITIVE, TRIT_POSITIVE, TRIT_POSITIVE, TRIT_POSITIVE};
    trit_packed243_t packed = trit_pack5(all_pos);
    EXPECT_EQ(packed, 242);
}

TEST_F(TernaryStorageTest, Pack5MinValue) {
    // Minimum value is when all trits are -1 (maps to 0)
    // 0 + 3*0 + 9*0 + 27*0 + 81*0 = 0
    trit_t all_neg[5] = {TRIT_NEGATIVE, TRIT_NEGATIVE, TRIT_NEGATIVE, TRIT_NEGATIVE, TRIT_NEGATIVE};
    trit_packed243_t packed = trit_pack5(all_neg);
    EXPECT_EQ(packed, 0);
}

TEST_F(TernaryStorageTest, BulkPack2Bit) {
    const size_t n = 20;
    trit_t input[20];
    for (size_t i = 0; i < n; i++) {
        input[i] = (trit_t)((i % 3) - 1);  // -1, 0, 1, -1, 0, ...
    }

    size_t packed_size = (n + 3) / 4;
    uint8_t packed[5];  // ceil(20/4) = 5
    trit_pack_array_2bit(input, packed, n);

    trit_t output[20];
    trit_unpack_array_2bit(packed, output, n);

    for (size_t i = 0; i < n; i++) {
        EXPECT_EQ(output[i], input[i]);
    }
}

TEST_F(TernaryStorageTest, BulkPackBase243) {
    const size_t n = 25;
    trit_t input[25];
    for (size_t i = 0; i < n; i++) {
        input[i] = (trit_t)((i % 3) - 1);
    }

    size_t packed_size = (n + 4) / 5;
    uint8_t packed[5];  // ceil(25/5) = 5
    trit_pack_array_243(input, packed, n);

    trit_t output[25];
    trit_unpack_array_243(packed, output, n);

    for (size_t i = 0; i < n; i++) {
        EXPECT_EQ(output[i], input[i]);
    }
}

//=============================================================================
// Vector Tests
//=============================================================================

class TernaryVectorTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(TernaryVectorTest, CreateDestroy) {
    trit_vector_t* vec = trit_vector_create(100, TERNARY_PACK_NONE);
    ASSERT_NE(vec, nullptr);
    EXPECT_EQ(vec->length, 100UL);
    EXPECT_EQ(vec->pack_mode, TERNARY_PACK_NONE);
    trit_vector_destroy(vec);
}

TEST_F(TernaryVectorTest, CreateFilled) {
    trit_vector_t* vec = trit_vector_create_filled(50, TRIT_POSITIVE, TERNARY_PACK_NONE);
    ASSERT_NE(vec, nullptr);

    for (size_t i = 0; i < 50; i++) {
        EXPECT_EQ(trit_vector_get(vec, i), TRIT_POSITIVE);
    }

    trit_vector_destroy(vec);
}

TEST_F(TernaryVectorTest, SetGetUnpacked) {
    trit_vector_t* vec = trit_vector_create(10, TERNARY_PACK_NONE);
    ASSERT_NE(vec, nullptr);

    trit_vector_set(vec, 0, TRIT_NEGATIVE);
    trit_vector_set(vec, 1, TRIT_UNKNOWN);
    trit_vector_set(vec, 2, TRIT_POSITIVE);

    EXPECT_EQ(trit_vector_get(vec, 0), TRIT_NEGATIVE);
    EXPECT_EQ(trit_vector_get(vec, 1), TRIT_UNKNOWN);
    EXPECT_EQ(trit_vector_get(vec, 2), TRIT_POSITIVE);

    trit_vector_destroy(vec);
}

TEST_F(TernaryVectorTest, SetGet2BitPacked) {
    trit_vector_t* vec = trit_vector_create(20, TERNARY_PACK_2BIT);
    ASSERT_NE(vec, nullptr);

    for (size_t i = 0; i < 20; i++) {
        trit_t val = (trit_t)((i % 3) - 1);
        trit_vector_set(vec, i, val);
    }

    for (size_t i = 0; i < 20; i++) {
        trit_t expected = (trit_t)((i % 3) - 1);
        EXPECT_EQ(trit_vector_get(vec, i), expected);
    }

    trit_vector_destroy(vec);
}

TEST_F(TernaryVectorTest, SetGetBase243Packed) {
    trit_vector_t* vec = trit_vector_create(25, TERNARY_PACK_BASE243);
    ASSERT_NE(vec, nullptr);

    for (size_t i = 0; i < 25; i++) {
        trit_t val = (trit_t)((i % 3) - 1);
        trit_vector_set(vec, i, val);
    }

    for (size_t i = 0; i < 25; i++) {
        trit_t expected = (trit_t)((i % 3) - 1);
        EXPECT_EQ(trit_vector_get(vec, i), expected);
    }

    trit_vector_destroy(vec);
}

TEST_F(TernaryVectorTest, Clone) {
    trit_vector_t* vec = trit_vector_create(10, TERNARY_PACK_NONE);
    ASSERT_NE(vec, nullptr);

    for (size_t i = 0; i < 10; i++) {
        trit_vector_set(vec, i, (trit_t)((i % 3) - 1));
    }

    trit_vector_t* clone = trit_vector_clone(vec);
    ASSERT_NE(clone, nullptr);

    for (size_t i = 0; i < 10; i++) {
        EXPECT_EQ(trit_vector_get(clone, i), trit_vector_get(vec, i));
    }

    trit_vector_destroy(vec);
    trit_vector_destroy(clone);
}

TEST_F(TernaryVectorTest, VectorAnd) {
    trit_vector_t* a = trit_vector_create(3, TERNARY_PACK_NONE);
    trit_vector_t* b = trit_vector_create(3, TERNARY_PACK_NONE);

    trit_vector_set(a, 0, TRIT_POSITIVE);
    trit_vector_set(a, 1, TRIT_UNKNOWN);
    trit_vector_set(a, 2, TRIT_NEGATIVE);

    trit_vector_set(b, 0, TRIT_POSITIVE);
    trit_vector_set(b, 1, TRIT_POSITIVE);
    trit_vector_set(b, 2, TRIT_POSITIVE);

    trit_vector_t* result = trit_vector_and(a, b);
    ASSERT_NE(result, nullptr);

    EXPECT_EQ(trit_vector_get(result, 0), TRIT_POSITIVE);  // 1 AND 1 = 1
    EXPECT_EQ(trit_vector_get(result, 1), TRIT_UNKNOWN);   // 0 AND 1 = 0
    EXPECT_EQ(trit_vector_get(result, 2), TRIT_NEGATIVE);  // -1 AND 1 = -1

    trit_vector_destroy(a);
    trit_vector_destroy(b);
    trit_vector_destroy(result);
}

TEST_F(TernaryVectorTest, VectorOr) {
    trit_vector_t* a = trit_vector_create(3, TERNARY_PACK_NONE);
    trit_vector_t* b = trit_vector_create(3, TERNARY_PACK_NONE);

    trit_vector_set(a, 0, TRIT_NEGATIVE);
    trit_vector_set(a, 1, TRIT_UNKNOWN);
    trit_vector_set(a, 2, TRIT_POSITIVE);

    trit_vector_set(b, 0, TRIT_NEGATIVE);
    trit_vector_set(b, 1, TRIT_NEGATIVE);
    trit_vector_set(b, 2, TRIT_NEGATIVE);

    trit_vector_t* result = trit_vector_or(a, b);
    ASSERT_NE(result, nullptr);

    EXPECT_EQ(trit_vector_get(result, 0), TRIT_NEGATIVE);  // -1 OR -1 = -1
    EXPECT_EQ(trit_vector_get(result, 1), TRIT_UNKNOWN);   // 0 OR -1 = 0
    EXPECT_EQ(trit_vector_get(result, 2), TRIT_POSITIVE);  // 1 OR -1 = 1

    trit_vector_destroy(a);
    trit_vector_destroy(b);
    trit_vector_destroy(result);
}

TEST_F(TernaryVectorTest, VectorNot) {
    trit_vector_t* vec = trit_vector_create(3, TERNARY_PACK_NONE);
    trit_vector_set(vec, 0, TRIT_NEGATIVE);
    trit_vector_set(vec, 1, TRIT_UNKNOWN);
    trit_vector_set(vec, 2, TRIT_POSITIVE);

    trit_vector_t* result = trit_vector_not(vec);
    ASSERT_NE(result, nullptr);

    EXPECT_EQ(trit_vector_get(result, 0), TRIT_POSITIVE);
    EXPECT_EQ(trit_vector_get(result, 1), TRIT_UNKNOWN);
    EXPECT_EQ(trit_vector_get(result, 2), TRIT_NEGATIVE);

    trit_vector_destroy(vec);
    trit_vector_destroy(result);
}

TEST_F(TernaryVectorTest, VectorAdd) {
    trit_vector_t* a = trit_vector_create(3, TERNARY_PACK_NONE);
    trit_vector_t* b = trit_vector_create(3, TERNARY_PACK_NONE);

    trit_vector_set(a, 0, TRIT_POSITIVE);
    trit_vector_set(a, 1, TRIT_POSITIVE);
    trit_vector_set(a, 2, TRIT_UNKNOWN);

    trit_vector_set(b, 0, TRIT_POSITIVE);
    trit_vector_set(b, 1, TRIT_NEGATIVE);
    trit_vector_set(b, 2, TRIT_UNKNOWN);

    trit_vector_t* result = trit_vector_add(a, b);
    ASSERT_NE(result, nullptr);

    EXPECT_EQ(trit_vector_get(result, 0), TRIT_POSITIVE);  // 1+1=2 clamped to 1
    EXPECT_EQ(trit_vector_get(result, 1), TRIT_UNKNOWN);   // 1+(-1)=0
    EXPECT_EQ(trit_vector_get(result, 2), TRIT_UNKNOWN);   // 0+0=0

    trit_vector_destroy(a);
    trit_vector_destroy(b);
    trit_vector_destroy(result);
}

TEST_F(TernaryVectorTest, VectorMajority) {
    trit_vector_t* vec = trit_vector_create(5, TERNARY_PACK_NONE);
    trit_vector_set(vec, 0, TRIT_POSITIVE);
    trit_vector_set(vec, 1, TRIT_POSITIVE);
    trit_vector_set(vec, 2, TRIT_POSITIVE);
    trit_vector_set(vec, 3, TRIT_NEGATIVE);
    trit_vector_set(vec, 4, TRIT_NEGATIVE);

    EXPECT_EQ(trit_vector_majority(vec), TRIT_POSITIVE);

    trit_vector_destroy(vec);
}

TEST_F(TernaryVectorTest, VectorDot) {
    trit_vector_t* a = trit_vector_create(4, TERNARY_PACK_NONE);
    trit_vector_t* b = trit_vector_create(4, TERNARY_PACK_NONE);

    trit_vector_set(a, 0, TRIT_POSITIVE);
    trit_vector_set(a, 1, TRIT_POSITIVE);
    trit_vector_set(a, 2, TRIT_NEGATIVE);
    trit_vector_set(a, 3, TRIT_UNKNOWN);

    trit_vector_set(b, 0, TRIT_POSITIVE);
    trit_vector_set(b, 1, TRIT_NEGATIVE);
    trit_vector_set(b, 2, TRIT_NEGATIVE);
    trit_vector_set(b, 3, TRIT_POSITIVE);

    // Dot = 1*1 + 1*(-1) + (-1)*(-1) + 0*1 = 1 - 1 + 1 + 0 = 1
    EXPECT_EQ(trit_vector_dot(a, b), 1);

    trit_vector_destroy(a);
    trit_vector_destroy(b);
}

TEST_F(TernaryVectorTest, VectorHamming) {
    trit_vector_t* a = trit_vector_create(4, TERNARY_PACK_NONE);
    trit_vector_t* b = trit_vector_create(4, TERNARY_PACK_NONE);

    trit_vector_set(a, 0, TRIT_POSITIVE);
    trit_vector_set(a, 1, TRIT_POSITIVE);
    trit_vector_set(a, 2, TRIT_NEGATIVE);
    trit_vector_set(a, 3, TRIT_UNKNOWN);

    trit_vector_set(b, 0, TRIT_POSITIVE);
    trit_vector_set(b, 1, TRIT_NEGATIVE);  // Different
    trit_vector_set(b, 2, TRIT_NEGATIVE);
    trit_vector_set(b, 3, TRIT_POSITIVE);  // Different

    EXPECT_EQ(trit_vector_hamming(a, b), 2UL);

    trit_vector_destroy(a);
    trit_vector_destroy(b);
}

TEST_F(TernaryVectorTest, VectorConvert) {
    trit_vector_t* unpacked = trit_vector_create(10, TERNARY_PACK_NONE);
    for (size_t i = 0; i < 10; i++) {
        trit_vector_set(unpacked, i, (trit_t)((i % 3) - 1));
    }

    trit_vector_t* packed = trit_vector_convert(unpacked, TERNARY_PACK_BASE243);
    ASSERT_NE(packed, nullptr);
    EXPECT_EQ(packed->pack_mode, TERNARY_PACK_BASE243);

    for (size_t i = 0; i < 10; i++) {
        EXPECT_EQ(trit_vector_get(packed, i), trit_vector_get(unpacked, i));
    }

    trit_vector_destroy(unpacked);
    trit_vector_destroy(packed);
}

//=============================================================================
// Matrix Tests
//=============================================================================

class TernaryMatrixTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(TernaryMatrixTest, CreateDestroy) {
    trit_matrix_t* mat = trit_matrix_create(10, 20, TERNARY_PACK_NONE);
    ASSERT_NE(mat, nullptr);
    EXPECT_EQ(mat->rows, 10UL);
    EXPECT_EQ(mat->cols, 20UL);
    EXPECT_EQ(mat->numel, 200UL);
    trit_matrix_destroy(mat);
}

TEST_F(TernaryMatrixTest, CreateFilled) {
    trit_matrix_t* mat = trit_matrix_create_filled(5, 5, TRIT_NEGATIVE, TERNARY_PACK_NONE);
    ASSERT_NE(mat, nullptr);

    for (size_t row = 0; row < 5; row++) {
        for (size_t col = 0; col < 5; col++) {
            EXPECT_EQ(trit_matrix_get(mat, row, col), TRIT_NEGATIVE);
        }
    }

    trit_matrix_destroy(mat);
}

TEST_F(TernaryMatrixTest, CreateIdentity) {
    trit_matrix_t* mat = trit_matrix_create_identity(4, TERNARY_PACK_NONE);
    ASSERT_NE(mat, nullptr);

    for (size_t i = 0; i < 4; i++) {
        for (size_t j = 0; j < 4; j++) {
            if (i == j) {
                EXPECT_EQ(trit_matrix_get(mat, i, j), TRIT_POSITIVE);
            } else {
                EXPECT_EQ(trit_matrix_get(mat, i, j), TRIT_UNKNOWN);
            }
        }
    }

    trit_matrix_destroy(mat);
}

TEST_F(TernaryMatrixTest, SetGet) {
    trit_matrix_t* mat = trit_matrix_create(3, 3, TERNARY_PACK_NONE);
    ASSERT_NE(mat, nullptr);

    trit_matrix_set(mat, 0, 0, TRIT_POSITIVE);
    trit_matrix_set(mat, 1, 1, TRIT_NEGATIVE);
    trit_matrix_set(mat, 2, 2, TRIT_UNKNOWN);

    EXPECT_EQ(trit_matrix_get(mat, 0, 0), TRIT_POSITIVE);
    EXPECT_EQ(trit_matrix_get(mat, 1, 1), TRIT_NEGATIVE);
    EXPECT_EQ(trit_matrix_get(mat, 2, 2), TRIT_UNKNOWN);

    trit_matrix_destroy(mat);
}

TEST_F(TernaryMatrixTest, SetGetPacked) {
    trit_matrix_t* mat = trit_matrix_create(4, 6, TERNARY_PACK_BASE243);
    ASSERT_NE(mat, nullptr);

    for (size_t row = 0; row < 4; row++) {
        for (size_t col = 0; col < 6; col++) {
            trit_t val = (trit_t)(((row + col) % 3) - 1);
            trit_matrix_set(mat, row, col, val);
        }
    }

    for (size_t row = 0; row < 4; row++) {
        for (size_t col = 0; col < 6; col++) {
            trit_t expected = (trit_t)(((row + col) % 3) - 1);
            EXPECT_EQ(trit_matrix_get(mat, row, col), expected);
        }
    }

    trit_matrix_destroy(mat);
}

TEST_F(TernaryMatrixTest, GetRow) {
    trit_matrix_t* mat = trit_matrix_create(3, 4, TERNARY_PACK_NONE);
    for (size_t col = 0; col < 4; col++) {
        trit_matrix_set(mat, 1, col, (trit_t)((col % 3) - 1));
    }

    trit_vector_t* row = trit_matrix_get_row(mat, 1);
    ASSERT_NE(row, nullptr);
    EXPECT_EQ(row->length, 4UL);

    for (size_t col = 0; col < 4; col++) {
        EXPECT_EQ(trit_vector_get(row, col), (trit_t)((col % 3) - 1));
    }

    trit_vector_destroy(row);
    trit_matrix_destroy(mat);
}

TEST_F(TernaryMatrixTest, MatrixVectorMul) {
    // 2x3 matrix times 3-element vector
    trit_matrix_t* mat = trit_matrix_create(2, 3, TERNARY_PACK_NONE);
    trit_vector_t* vec = trit_vector_create(3, TERNARY_PACK_NONE);

    // Row 0: [1, 0, -1]
    trit_matrix_set(mat, 0, 0, TRIT_POSITIVE);
    trit_matrix_set(mat, 0, 1, TRIT_UNKNOWN);
    trit_matrix_set(mat, 0, 2, TRIT_NEGATIVE);

    // Row 1: [1, 1, 1]
    trit_matrix_set(mat, 1, 0, TRIT_POSITIVE);
    trit_matrix_set(mat, 1, 1, TRIT_POSITIVE);
    trit_matrix_set(mat, 1, 2, TRIT_POSITIVE);

    // Vector: [1, 1, 1]
    trit_vector_set(vec, 0, TRIT_POSITIVE);
    trit_vector_set(vec, 1, TRIT_POSITIVE);
    trit_vector_set(vec, 2, TRIT_POSITIVE);

    trit_vector_t* result = trit_matrix_vector_mul(mat, vec);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->length, 2UL);

    // Row 0: 1*1 + 0*1 + (-1)*1 = 1 + 0 - 1 = 0 -> UNKNOWN
    // Row 1: 1*1 + 1*1 + 1*1 = 3 -> POSITIVE (clamped)
    EXPECT_EQ(trit_vector_get(result, 0), TRIT_UNKNOWN);
    EXPECT_EQ(trit_vector_get(result, 1), TRIT_POSITIVE);

    trit_matrix_destroy(mat);
    trit_vector_destroy(vec);
    trit_vector_destroy(result);
}

TEST_F(TernaryMatrixTest, MatrixTranspose) {
    trit_matrix_t* mat = trit_matrix_create(2, 3, TERNARY_PACK_NONE);
    trit_matrix_set(mat, 0, 0, TRIT_POSITIVE);
    trit_matrix_set(mat, 0, 1, TRIT_NEGATIVE);
    trit_matrix_set(mat, 0, 2, TRIT_UNKNOWN);
    trit_matrix_set(mat, 1, 0, TRIT_NEGATIVE);
    trit_matrix_set(mat, 1, 1, TRIT_UNKNOWN);
    trit_matrix_set(mat, 1, 2, TRIT_POSITIVE);

    trit_matrix_t* trans = trit_matrix_transpose(mat);
    ASSERT_NE(trans, nullptr);
    EXPECT_EQ(trans->rows, 3UL);
    EXPECT_EQ(trans->cols, 2UL);

    EXPECT_EQ(trit_matrix_get(trans, 0, 0), TRIT_POSITIVE);
    EXPECT_EQ(trit_matrix_get(trans, 1, 0), TRIT_NEGATIVE);
    EXPECT_EQ(trit_matrix_get(trans, 2, 0), TRIT_UNKNOWN);
    EXPECT_EQ(trit_matrix_get(trans, 0, 1), TRIT_NEGATIVE);
    EXPECT_EQ(trit_matrix_get(trans, 1, 1), TRIT_UNKNOWN);
    EXPECT_EQ(trit_matrix_get(trans, 2, 1), TRIT_POSITIVE);

    trit_matrix_destroy(mat);
    trit_matrix_destroy(trans);
}

TEST_F(TernaryMatrixTest, MatrixSparsity) {
    trit_matrix_t* mat = trit_matrix_create_filled(10, 10, TRIT_UNKNOWN, TERNARY_PACK_NONE);
    EXPECT_FLOAT_EQ(trit_matrix_sparsity(mat), 1.0f);  // All zeros

    trit_matrix_set(mat, 0, 0, TRIT_POSITIVE);
    EXPECT_NEAR(trit_matrix_sparsity(mat), 0.99f, 0.01f);  // 99 zeros out of 100

    trit_matrix_destroy(mat);
}

//=============================================================================
// Conversion Tests
//=============================================================================

class TernaryConversionTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(TernaryConversionTest, FloatSign) {
    EXPECT_EQ(trit_from_float_sign(1.5f), TRIT_POSITIVE);
    EXPECT_EQ(trit_from_float_sign(-1.5f), TRIT_NEGATIVE);
    EXPECT_EQ(trit_from_float_sign(0.0f), TRIT_UNKNOWN);
    EXPECT_EQ(trit_from_float_sign(0.001f), TRIT_POSITIVE);
    EXPECT_EQ(trit_from_float_sign(-0.001f), TRIT_NEGATIVE);
}

TEST_F(TernaryConversionTest, FloatThreshold) {
    float threshold = 0.5f;
    EXPECT_EQ(trit_from_float_threshold(1.0f, threshold), TRIT_POSITIVE);
    EXPECT_EQ(trit_from_float_threshold(-1.0f, threshold), TRIT_NEGATIVE);
    EXPECT_EQ(trit_from_float_threshold(0.3f, threshold), TRIT_UNKNOWN);
    EXPECT_EQ(trit_from_float_threshold(-0.3f, threshold), TRIT_UNKNOWN);
    EXPECT_EQ(trit_from_float_threshold(0.5f, threshold), TRIT_POSITIVE);  // Edge case
    EXPECT_EQ(trit_from_float_threshold(-0.5f, threshold), TRIT_NEGATIVE);
}

TEST_F(TernaryConversionTest, ToFloat) {
    EXPECT_FLOAT_EQ(trit_to_float(TRIT_NEGATIVE), -1.0f);
    EXPECT_FLOAT_EQ(trit_to_float(TRIT_UNKNOWN), 0.0f);
    EXPECT_FLOAT_EQ(trit_to_float(TRIT_POSITIVE), 1.0f);
}

TEST_F(TernaryConversionTest, ToFloatScaled) {
    float scale = 0.5f;
    EXPECT_FLOAT_EQ(trit_to_float_scaled(TRIT_NEGATIVE, scale), -0.5f);
    EXPECT_FLOAT_EQ(trit_to_float_scaled(TRIT_UNKNOWN, scale), 0.0f);
    EXPECT_FLOAT_EQ(trit_to_float_scaled(TRIT_POSITIVE, scale), 0.5f);
}

TEST_F(TernaryConversionTest, FromBool) {
    EXPECT_EQ(trit_from_bool(true), TRIT_POSITIVE);
    EXPECT_EQ(trit_from_bool(false), TRIT_NEGATIVE);
}

TEST_F(TernaryConversionTest, VectorFromFloats) {
    float floats[5] = {1.0f, -0.2f, 0.0f, -1.0f, 0.8f};
    trit_vector_t* vec = trit_vector_from_floats(floats, 5, 0.5f, TERNARY_PACK_NONE);
    ASSERT_NE(vec, nullptr);

    EXPECT_EQ(trit_vector_get(vec, 0), TRIT_POSITIVE);   // 1.0 >= 0.5
    EXPECT_EQ(trit_vector_get(vec, 1), TRIT_UNKNOWN);    // -0.2 in [-0.5, 0.5]
    EXPECT_EQ(trit_vector_get(vec, 2), TRIT_UNKNOWN);    // 0.0 in [-0.5, 0.5]
    EXPECT_EQ(trit_vector_get(vec, 3), TRIT_NEGATIVE);   // -1.0 <= -0.5
    EXPECT_EQ(trit_vector_get(vec, 4), TRIT_POSITIVE);   // 0.8 >= 0.5

    trit_vector_destroy(vec);
}

TEST_F(TernaryConversionTest, VectorToFloats) {
    trit_vector_t* vec = trit_vector_create(3, TERNARY_PACK_NONE);
    trit_vector_set(vec, 0, TRIT_POSITIVE);
    trit_vector_set(vec, 1, TRIT_UNKNOWN);
    trit_vector_set(vec, 2, TRIT_NEGATIVE);

    float floats[3];
    trit_vector_to_floats(vec, floats, 2.0f);

    EXPECT_FLOAT_EQ(floats[0], 2.0f);
    EXPECT_FLOAT_EQ(floats[1], 0.0f);
    EXPECT_FLOAT_EQ(floats[2], -2.0f);

    trit_vector_destroy(vec);
}

TEST_F(TernaryConversionTest, ExtendedFromFloat) {
    trit_extended_t ext = trit_extended_from_float(1.0f, 0.5f);
    EXPECT_EQ(ext.value, TRIT_POSITIVE);
    EXPECT_GT(ext.confidence, 0.5f);

    ext = trit_extended_from_float(-1.0f, 0.5f);
    EXPECT_EQ(ext.value, TRIT_NEGATIVE);

    ext = trit_extended_from_float(0.0f, 0.5f);
    EXPECT_EQ(ext.value, TRIT_UNKNOWN);
    EXPECT_FLOAT_EQ(ext.confidence, 1.0f);  // Very confident in unknown
}

TEST_F(TernaryConversionTest, Probabilities) {
    float p_neg, p_unk, p_pos;

    trit_to_probabilities(TRIT_POSITIVE, 0.0f, &p_neg, &p_unk, &p_pos);
    EXPECT_FLOAT_EQ(p_pos, 1.0f);
    EXPECT_FLOAT_EQ(p_neg, 0.0f);
    EXPECT_FLOAT_EQ(p_unk, 0.0f);

    trit_to_probabilities(TRIT_POSITIVE, 0.1f, &p_neg, &p_unk, &p_pos);
    EXPECT_NEAR(p_pos, 0.933f, 0.01f);  // 1 - 0.1 + 0.1/3
    EXPECT_NEAR(p_neg, 0.033f, 0.01f);
    EXPECT_NEAR(p_unk, 0.033f, 0.01f);
}

TEST_F(TernaryConversionTest, QuantizeWeight) {
    // Weights centered around 0 with std 1.0
    trit_t t;

    t = trit_quantize_weight(2.0f, 0.5f, 0.0f, 1.0f);  // 2.0 > 0.5*1.0
    EXPECT_EQ(t, TRIT_POSITIVE);

    t = trit_quantize_weight(-2.0f, 0.5f, 0.0f, 1.0f);  // -2.0 < -0.5*1.0
    EXPECT_EQ(t, TRIT_NEGATIVE);

    t = trit_quantize_weight(0.2f, 0.5f, 0.0f, 1.0f);  // 0.2 in [-0.5, 0.5]
    EXPECT_EQ(t, TRIT_UNKNOWN);
}

TEST_F(TernaryConversionTest, DequantizeWeight) {
    EXPECT_FLOAT_EQ(trit_dequantize_weight(TRIT_POSITIVE, 0.5f, -0.3f), 0.5f);
    EXPECT_FLOAT_EQ(trit_dequantize_weight(TRIT_NEGATIVE, 0.5f, -0.3f), -0.3f);
    EXPECT_FLOAT_EQ(trit_dequantize_weight(TRIT_UNKNOWN, 0.5f, -0.3f), 0.0f);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
