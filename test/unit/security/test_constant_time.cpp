/**
 * @file test_constant_time.cpp
 * @brief Unit tests for constant-time cryptographic operations
 *
 * WHAT: Comprehensive tests for timing-safe comparison and selection functions
 * WHY:  Ensure constant-time operations are functionally correct and secure
 * HOW:  GoogleTest framework with correctness and timing variance tests
 *
 * TEST COVERAGE:
 * 1. Memory comparison (ct_memcmp)
 * 2. String comparison (ct_strcmp, ct_strncmp)
 * 3. Conditional selection (ct_select_*)
 * 4. Array lookup (ct_lookup_*)
 * 5. Hash comparison (ct_hash_equal)
 * 6. Secure memory wiping
 * 7. Context management
 * 8. Statistics tracking
 *
 * @author NIMCP Security Team
 * @date 2025-12-07
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <random>

// Headers have their own extern "C" guards
#include "security/nimcp_constant_time.h"
#include "utils/logging/nimcp_logging.h"

//=============================================================================
// TEST FIXTURE
//=============================================================================

class ConstantTimeTest : public ::testing::Test {
protected:
    nimcp_ct_context_t ctx;

    void SetUp() override {
        nimcp_log_set_level(NULL, LOG_LEVEL_WARN);
        ctx = nimcp_ct_create();
        ASSERT_NE(ctx, nullptr);
    }

    void TearDown() override {
        if (ctx) {
            nimcp_ct_destroy(ctx);
            ctx = nullptr;
        }
    }

    // Helper: Fill buffer with random data
    void fill_random(uint8_t* buf, size_t len) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 255);

        for (size_t i = 0; i < len; i++) {
            buf[i] = static_cast<uint8_t>(dis(gen));
        }
    }
};

//=============================================================================
// MEMORY COMPARISON TESTS
//=============================================================================

TEST_F(ConstantTimeTest, MemcmpEqual) {
    uint8_t buf1[32] = {1, 2, 3, 4, 5};
    uint8_t buf2[32] = {1, 2, 3, 4, 5};

    EXPECT_EQ(0, nimcp_ct_memcmp(buf1, buf2, 5));
}

TEST_F(ConstantTimeTest, MemcmpDifferent) {
    uint8_t buf1[32] = {1, 2, 3, 4, 5};
    uint8_t buf2[32] = {1, 2, 3, 4, 6};  // Last byte differs

    EXPECT_NE(0, nimcp_ct_memcmp(buf1, buf2, 5));
}

TEST_F(ConstantTimeTest, MemcmpFirstByteDiffers) {
    uint8_t buf1[32] = {1, 2, 3, 4, 5};
    uint8_t buf2[32] = {0, 2, 3, 4, 5};  // First byte differs

    EXPECT_NE(0, nimcp_ct_memcmp(buf1, buf2, 5));
}

TEST_F(ConstantTimeTest, MemcmpMiddleByteDiffers) {
    uint8_t buf1[32] = {1, 2, 3, 4, 5};
    uint8_t buf2[32] = {1, 2, 0, 4, 5};  // Middle byte differs

    EXPECT_NE(0, nimcp_ct_memcmp(buf1, buf2, 5));
}

TEST_F(ConstantTimeTest, MemcmpZeroLength) {
    uint8_t buf1[32] = {1, 2, 3};
    uint8_t buf2[32] = {4, 5, 6};

    EXPECT_EQ(0, nimcp_ct_memcmp(buf1, buf2, 0));  // Zero length = equal
}

TEST_F(ConstantTimeTest, MemcmpAllZeros) {
    uint8_t buf1[32] = {0};
    uint8_t buf2[32] = {0};

    EXPECT_EQ(0, nimcp_ct_memcmp(buf1, buf2, 32));
}

TEST_F(ConstantTimeTest, MemcmpAllOnes) {
    uint8_t buf1[32];
    uint8_t buf2[32];
    memset(buf1, 0xFF, sizeof(buf1));
    memset(buf2, 0xFF, sizeof(buf2));

    EXPECT_EQ(0, nimcp_ct_memcmp(buf1, buf2, 32));
}

TEST_F(ConstantTimeTest, MemcmpLargeBuffer) {
    std::vector<uint8_t> buf1(4096);
    std::vector<uint8_t> buf2(4096);

    fill_random(buf1.data(), buf1.size());
    memcpy(buf2.data(), buf1.data(), buf1.size());

    EXPECT_EQ(0, nimcp_ct_memcmp(buf1.data(), buf2.data(), buf1.size()));

    // Change one byte
    buf2[2048] ^= 0x01;
    EXPECT_NE(0, nimcp_ct_memcmp(buf1.data(), buf2.data(), buf1.size()));
}

TEST_F(ConstantTimeTest, MemcmpWithTracking) {
    uint8_t buf1[32] = {1, 2, 3, 4, 5};
    uint8_t buf2[32] = {1, 2, 3, 4, 5};

    nimcp_ct_stats_t stats_before;
    nimcp_ct_get_stats(ctx, &stats_before);

    EXPECT_EQ(0, nimcp_ct_memcmp_tracked(ctx, buf1, buf2, 5));

    nimcp_ct_stats_t stats_after;
    nimcp_ct_get_stats(ctx, &stats_after);

    EXPECT_GT(stats_after.memcmp_operations, stats_before.memcmp_operations);
    EXPECT_GT(stats_after.total_bytes_compared, stats_before.total_bytes_compared);
}

//=============================================================================
// STRING COMPARISON TESTS
//=============================================================================

TEST_F(ConstantTimeTest, StrcmpEqual) {
    const char* str1 = "hello";
    const char* str2 = "hello";

    EXPECT_EQ(0, nimcp_ct_strcmp(str1, str2));
}

TEST_F(ConstantTimeTest, StrcmpDifferent) {
    const char* str1 = "hello";
    const char* str2 = "world";

    EXPECT_NE(0, nimcp_ct_strcmp(str1, str2));
}

TEST_F(ConstantTimeTest, StrcmpDifferentLength) {
    const char* str1 = "hello";
    const char* str2 = "hello world";

    EXPECT_NE(0, nimcp_ct_strcmp(str1, str2));
}

TEST_F(ConstantTimeTest, StrcmpEmpty) {
    const char* str1 = "";
    const char* str2 = "";

    EXPECT_EQ(0, nimcp_ct_strcmp(str1, str2));
}

TEST_F(ConstantTimeTest, StrcmpOneEmpty) {
    const char* str1 = "hello";
    const char* str2 = "";

    EXPECT_NE(0, nimcp_ct_strcmp(str1, str2));
}

TEST_F(ConstantTimeTest, StrcmpLongStrings) {
    std::string str1(1024, 'a');
    std::string str2(1024, 'a');

    EXPECT_EQ(0, nimcp_ct_strcmp(str1.c_str(), str2.c_str()));

    str2[512] = 'b';
    EXPECT_NE(0, nimcp_ct_strcmp(str1.c_str(), str2.c_str()));
}

TEST_F(ConstantTimeTest, StrncmpEqual) {
    const char* str1 = "hello world";
    const char* str2 = "hello there";

    EXPECT_EQ(0, nimcp_ct_strncmp(str1, str2, 5));  // First 5 bytes equal
}

TEST_F(ConstantTimeTest, StrncmpDifferent) {
    const char* str1 = "hello";
    const char* str2 = "world";

    EXPECT_NE(0, nimcp_ct_strncmp(str1, str2, 5));
}

TEST_F(ConstantTimeTest, StrncmpZeroLength) {
    const char* str1 = "hello";
    const char* str2 = "world";

    EXPECT_EQ(0, nimcp_ct_strncmp(str1, str2, 0));
}

//=============================================================================
// CONDITIONAL SELECTION TESTS
//=============================================================================

TEST_F(ConstantTimeTest, SelectU8_ChooseA) {
    uint8_t a = 0x42;
    uint8_t b = 0x13;
    uint8_t select = 0;

    EXPECT_EQ(a, nimcp_ct_select_u8(a, b, select));
}

TEST_F(ConstantTimeTest, SelectU8_ChooseB) {
    uint8_t a = 0x42;
    uint8_t b = 0x13;
    uint8_t select = 1;

    EXPECT_EQ(b, nimcp_ct_select_u8(a, b, select));
}

TEST_F(ConstantTimeTest, SelectU8_NonZeroSelect) {
    uint8_t a = 0x42;
    uint8_t b = 0x13;

    // Any non-zero select should choose b
    EXPECT_EQ(b, nimcp_ct_select_u8(a, b, 5));
    EXPECT_EQ(b, nimcp_ct_select_u8(a, b, 255));
}

TEST_F(ConstantTimeTest, SelectU32_ChooseA) {
    uint32_t a = 0x12345678;
    uint32_t b = 0x87654321;
    uint32_t select = 0;

    EXPECT_EQ(a, nimcp_ct_select_u32(a, b, select));
}

TEST_F(ConstantTimeTest, SelectU32_ChooseB) {
    uint32_t a = 0x12345678;
    uint32_t b = 0x87654321;
    uint32_t select = 1;

    EXPECT_EQ(b, nimcp_ct_select_u32(a, b, select));
}

TEST_F(ConstantTimeTest, SelectU64_ChooseA) {
    uint64_t a = 0x123456789ABCDEF0ULL;
    uint64_t b = 0xFEDCBA9876543210ULL;
    uint64_t select = 0;

    EXPECT_EQ(a, nimcp_ct_select_u64(a, b, select));
}

TEST_F(ConstantTimeTest, SelectU64_ChooseB) {
    uint64_t a = 0x123456789ABCDEF0ULL;
    uint64_t b = 0xFEDCBA9876543210ULL;
    uint64_t select = 1;

    EXPECT_EQ(b, nimcp_ct_select_u64(a, b, select));
}

TEST_F(ConstantTimeTest, SelectSize_ChooseA) {
    size_t a = 1024;
    size_t b = 2048;
    uint8_t select = 0;

    EXPECT_EQ(a, nimcp_ct_select_size(a, b, select));
}

TEST_F(ConstantTimeTest, SelectSize_ChooseB) {
    size_t a = 1024;
    size_t b = 2048;
    uint8_t select = 1;

    EXPECT_EQ(b, nimcp_ct_select_size(a, b, select));
}

TEST_F(ConstantTimeTest, SelectSameValue) {
    uint8_t val = 0x42;

    // Selecting between same values should always return that value
    EXPECT_EQ(val, nimcp_ct_select_u8(val, val, 0));
    EXPECT_EQ(val, nimcp_ct_select_u8(val, val, 1));
}

//=============================================================================
// ARRAY LOOKUP TESTS
//=============================================================================

TEST_F(ConstantTimeTest, LookupU8_ValidIndex) {
    uint8_t table[] = {10, 20, 30, 40, 50};

    EXPECT_EQ(10, nimcp_ct_lookup_u8(table, 5, 0));
    EXPECT_EQ(30, nimcp_ct_lookup_u8(table, 5, 2));
    EXPECT_EQ(50, nimcp_ct_lookup_u8(table, 5, 4));
}

TEST_F(ConstantTimeTest, LookupU8_OutOfBounds) {
    uint8_t table[] = {10, 20, 30, 40, 50};

    EXPECT_EQ(0, nimcp_ct_lookup_u8(table, 5, 10));  // Out of bounds returns 0
}

TEST_F(ConstantTimeTest, LookupU8_EmptyTable) {
    uint8_t table[] = {10};

    EXPECT_EQ(0, nimcp_ct_lookup_u8(table, 0, 0));  // Empty table returns 0
}

TEST_F(ConstantTimeTest, LookupU32_ValidIndex) {
    uint32_t table[] = {100, 200, 300, 400, 500};

    EXPECT_EQ(100U, nimcp_ct_lookup_u32(table, 5, 0));
    EXPECT_EQ(300U, nimcp_ct_lookup_u32(table, 5, 2));
    EXPECT_EQ(500U, nimcp_ct_lookup_u32(table, 5, 4));
}

TEST_F(ConstantTimeTest, LookupU32_OutOfBounds) {
    uint32_t table[] = {100, 200, 300, 400, 500};

    EXPECT_EQ(0U, nimcp_ct_lookup_u32(table, 5, 10));
}

TEST_F(ConstantTimeTest, LookupLargeTable) {
    std::vector<uint8_t> table(256);
    for (size_t i = 0; i < 256; i++) {
        table[i] = static_cast<uint8_t>(i);
    }

    // Verify all positions
    for (size_t i = 0; i < 256; i++) {
        EXPECT_EQ(static_cast<uint8_t>(i),
                 nimcp_ct_lookup_u8(table.data(), table.size(), i));
    }
}

//=============================================================================
// HASH COMPARISON TESTS
//=============================================================================

TEST_F(ConstantTimeTest, HashEqualSHA256) {
    uint8_t hash1[32];
    uint8_t hash2[32];

    fill_random(hash1, 32);
    memcpy(hash2, hash1, 32);

    EXPECT_TRUE(nimcp_ct_hash_equal(hash1, hash2, 32));
    EXPECT_TRUE(nimcp_ct_sha256_equal(hash1, hash2));
}

TEST_F(ConstantTimeTest, HashNotEqual) {
    uint8_t hash1[32];
    uint8_t hash2[32];

    fill_random(hash1, 32);
    fill_random(hash2, 32);

    // Very unlikely to be equal
    if (memcmp(hash1, hash2, 32) != 0) {
        EXPECT_FALSE(nimcp_ct_hash_equal(hash1, hash2, 32));
    }
}

TEST_F(ConstantTimeTest, HashEqualOneBitDifference) {
    uint8_t hash1[32];
    uint8_t hash2[32];

    fill_random(hash1, 32);
    memcpy(hash2, hash1, 32);

    // Flip one bit
    hash2[16] ^= 0x01;

    EXPECT_FALSE(nimcp_ct_hash_equal(hash1, hash2, 32));
}

TEST_F(ConstantTimeTest, HashEqualDifferentSizes) {
    uint8_t hash1[64];
    uint8_t hash2[64];

    fill_random(hash1, 64);
    memcpy(hash2, hash1, 64);

    // SHA-512 size
    EXPECT_TRUE(nimcp_ct_hash_equal(hash1, hash2, 64));

    // Change one byte
    hash2[32] ^= 0xFF;
    EXPECT_FALSE(nimcp_ct_hash_equal(hash1, hash2, 64));
}

//=============================================================================
// SECURE MEMORY WIPING TESTS
//=============================================================================

TEST_F(ConstantTimeTest, SecureZero) {
    uint8_t buffer[128];

    // Fill with random data
    fill_random(buffer, sizeof(buffer));

    // Verify not all zeros
    bool has_nonzero = false;
    for (size_t i = 0; i < sizeof(buffer); i++) {
        if (buffer[i] != 0) {
            has_nonzero = true;
            break;
        }
    }
    EXPECT_TRUE(has_nonzero);

    // Secure zero
    nimcp_secure_zero(buffer, sizeof(buffer));

    // Verify all zeros
    for (size_t i = 0; i < sizeof(buffer); i++) {
        EXPECT_EQ(0, buffer[i]);
    }
}

TEST_F(ConstantTimeTest, SecureWipe) {
    uint8_t buffer[128];

    fill_random(buffer, sizeof(buffer));

    nimcp_result_t result = nimcp_secure_wipe(buffer, sizeof(buffer));
    EXPECT_EQ(NIMCP_SUCCESS, result);

    // Should be all zeros after wipe
    for (size_t i = 0; i < sizeof(buffer); i++) {
        EXPECT_EQ(0, buffer[i]);
    }
}

TEST_F(ConstantTimeTest, SecureZeroNullPointer) {
    // Should not crash
    nimcp_secure_zero(nullptr, 100);
}

TEST_F(ConstantTimeTest, SecureWipeNullPointer) {
    nimcp_result_t result = nimcp_secure_wipe(nullptr, 100);
    EXPECT_NE(NIMCP_SUCCESS, result);
}

TEST_F(ConstantTimeTest, SecureZeroZeroLength) {
    uint8_t buffer[16];
    fill_random(buffer, sizeof(buffer));

    // Should not modify buffer
    uint8_t original[16];
    memcpy(original, buffer, sizeof(buffer));

    nimcp_secure_zero(buffer, 0);

    EXPECT_EQ(0, memcmp(buffer, original, sizeof(buffer)));
}

//=============================================================================
// CONTEXT MANAGEMENT TESTS
//=============================================================================

TEST_F(ConstantTimeTest, ContextCreate) {
    nimcp_ct_context_t ctx2 = nimcp_ct_create();
    ASSERT_NE(ctx2, nullptr);

    nimcp_ct_destroy(ctx2);
}

TEST_F(ConstantTimeTest, ContextDestroy) {
    nimcp_ct_context_t ctx2 = nimcp_ct_create();
    ASSERT_NE(ctx2, nullptr);

    // Should not crash
    nimcp_ct_destroy(ctx2);

    // Double destroy should not crash
    nimcp_ct_destroy(nullptr);
}

TEST_F(ConstantTimeTest, GetStats) {
    nimcp_ct_stats_t stats;
    nimcp_result_t result = nimcp_ct_get_stats(ctx, &stats);

    EXPECT_EQ(NIMCP_SUCCESS, result);
}

TEST_F(ConstantTimeTest, GetStatsInvalidContext) {
    nimcp_ct_stats_t stats;
    nimcp_result_t result = nimcp_ct_get_stats(nullptr, &stats);

    EXPECT_NE(NIMCP_SUCCESS, result);
}

TEST_F(ConstantTimeTest, GetStatsNullPointer) {
    nimcp_result_t result = nimcp_ct_get_stats(ctx, nullptr);

    EXPECT_NE(NIMCP_SUCCESS, result);
}

TEST_F(ConstantTimeTest, ResetStats) {
    // Perform some operations
    uint8_t buf1[32] = {1, 2, 3};
    uint8_t buf2[32] = {1, 2, 3};
    nimcp_ct_memcmp_tracked(ctx, buf1, buf2, 3);

    nimcp_ct_stats_t stats_before;
    nimcp_ct_get_stats(ctx, &stats_before);
    EXPECT_GT(stats_before.memcmp_operations, 0UL);

    // Reset
    nimcp_result_t result = nimcp_ct_reset_stats(ctx);
    EXPECT_EQ(NIMCP_SUCCESS, result);

    // Check stats are zero
    nimcp_ct_stats_t stats_after;
    nimcp_ct_get_stats(ctx, &stats_after);
    EXPECT_EQ(0UL, stats_after.memcmp_operations);
}

//=============================================================================
// ERROR HANDLING TESTS
//=============================================================================

TEST_F(ConstantTimeTest, MemcmpNullPointers) {
    uint8_t buf[32];

    EXPECT_NE(0, nimcp_ct_memcmp(nullptr, buf, 32));
    EXPECT_NE(0, nimcp_ct_memcmp(buf, nullptr, 32));
    EXPECT_NE(0, nimcp_ct_memcmp(nullptr, nullptr, 32));
}

TEST_F(ConstantTimeTest, StrcmpNullPointers) {
    const char* str = "test";

    EXPECT_NE(0, nimcp_ct_strcmp(nullptr, str));
    EXPECT_NE(0, nimcp_ct_strcmp(str, nullptr));
    EXPECT_NE(0, nimcp_ct_strcmp(nullptr, nullptr));
}

TEST_F(ConstantTimeTest, LookupNullTable) {
    EXPECT_EQ(0, nimcp_ct_lookup_u8(nullptr, 10, 5));
    EXPECT_EQ(0U, nimcp_ct_lookup_u32(nullptr, 10, 5));
}

TEST_F(ConstantTimeTest, HashEqualNullPointers) {
    uint8_t hash[32];

    EXPECT_FALSE(nimcp_ct_hash_equal(nullptr, hash, 32));
    EXPECT_FALSE(nimcp_ct_hash_equal(hash, nullptr, 32));
    EXPECT_FALSE(nimcp_ct_hash_equal(nullptr, nullptr, 32));
}

//=============================================================================
// TIMING VARIANCE TESTS (Basic Check)
//=============================================================================

TEST_F(ConstantTimeTest, DISABLED_TimingVarianceCheck) {
    // NOTE: This test is disabled by default as timing tests are non-deterministic
    // and can fail on heavily loaded systems. Enable manually for security audits.

    bool is_constant_time = nimcp_ct_verify_timing("ct_memcmp", 1000, 10.0);

    // Should have low timing variance (<10%)
    EXPECT_TRUE(is_constant_time);
}

//=============================================================================
// EDGE CASES AND BOUNDARY CONDITIONS
//=============================================================================

TEST_F(ConstantTimeTest, MemcmpSingleByte) {
    uint8_t a = 0x42;
    uint8_t b = 0x42;

    EXPECT_EQ(0, nimcp_ct_memcmp(&a, &b, 1));

    b = 0x43;
    EXPECT_NE(0, nimcp_ct_memcmp(&a, &b, 1));
}

TEST_F(ConstantTimeTest, SelectAllBitsSet) {
    uint8_t a = 0xFF;
    uint8_t b = 0x00;

    EXPECT_EQ(a, nimcp_ct_select_u8(a, b, 0));
    EXPECT_EQ(b, nimcp_ct_select_u8(a, b, 1));
}

TEST_F(ConstantTimeTest, LookupSingleElement) {
    uint8_t table[] = {42};

    EXPECT_EQ(42, nimcp_ct_lookup_u8(table, 1, 0));
    EXPECT_EQ(0, nimcp_ct_lookup_u8(table, 1, 1));  // Out of bounds
}

TEST_F(ConstantTimeTest, HashZeroLength) {
    uint8_t hash1[1] = {0};
    uint8_t hash2[1] = {0};

    // Zero-length comparison should return true
    EXPECT_TRUE(nimcp_ct_hash_equal(hash1, hash2, 0));
}

//=============================================================================
// MAIN
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
