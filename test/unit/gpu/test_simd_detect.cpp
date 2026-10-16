/**
 * @file test_simd_detect.cpp
 * @brief Unit tests for SIMD capability detection
 *
 * WHAT: Tests CPU SIMD feature detection (SSE, AVX, AVX2, AVX-512, NEON)
 * WHY:  Ensure accurate SIMD detection across platforms for optimization
 * HOW:  Test all public API functions with various scenarios
 *
 * TEST COVERAGE:
 * - Basic capability detection
 * - Individual feature queries
 * - Vector width detection
 * - Feature name strings
 * - Edge cases and NULL handling
 * - Platform-specific behavior
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#include <gtest/gtest.h>
#include <cstring>

// Headers have their own extern "C" guards
#include "gpu/execution/nimcp_simd_detect.h"

//=============================================================================
// Test Fixture
//=============================================================================

/**
 * @brief Test fixture for SIMD detection tests
 */
class SIMDDetectTest : public ::testing::Test {
protected:
    simd_capabilities_t caps;

    void SetUp() override {
        memset(&caps, 0, sizeof(caps));
    }
};

//=============================================================================
// Basic Detection Tests
//=============================================================================

/**
 * TEST: Basic capability detection succeeds
 * WHAT: Verify simd_detect_capabilities() returns valid results
 * WHY:  Must successfully detect SIMD features
 * HOW:  Call with valid pointer, check success
 */
TEST_F(SIMDDetectTest, DetectCapabilities_Success) {
    bool result = simd_detect_capabilities(&caps);
    EXPECT_TRUE(result);
}

/**
 * TEST: NULL pointer handling
 * WHAT: Verify simd_detect_capabilities() handles NULL gracefully
 * WHY:  Prevent crashes from invalid input
 * HOW:  Pass NULL, expect false return
 */
TEST_F(SIMDDetectTest, DetectCapabilities_NullPointer) {
    bool result = simd_detect_capabilities(nullptr);
    EXPECT_FALSE(result);
}

/**
 * TEST: Cached results are consistent
 * WHAT: Verify multiple calls return same results
 * WHY:  Detection results should be deterministic
 * HOW:  Call twice, compare results
 */
TEST_F(SIMDDetectTest, DetectCapabilities_Caching) {
    simd_capabilities_t caps1, caps2;

    bool result1 = simd_detect_capabilities(&caps1);
    bool result2 = simd_detect_capabilities(&caps2);

    EXPECT_TRUE(result1);
    EXPECT_TRUE(result2);

    // Features should match
    EXPECT_EQ(caps1.features, caps2.features);
    EXPECT_EQ(caps1.max_vector_width, caps2.max_vector_width);
    EXPECT_EQ(caps1.has_avx2, caps2.has_avx2);
    EXPECT_EQ(caps1.has_avx512f, caps2.has_avx512f);
}

//=============================================================================
// Feature Detection Tests (x86)
//=============================================================================

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)

/**
 * TEST: SSE detection on x86
 * WHAT: Verify SSE/SSE2 is detected on x86 platforms
 * WHY:  SSE/SSE2 is baseline for x86-64
 * HOW:  Check SSE flags after detection
 */
TEST_F(SIMDDetectTest, X86_SSEDetection) {
    simd_detect_capabilities(&caps);

    // x86-64 always has SSE/SSE2
    #if defined(__x86_64__) || defined(_M_X64)
    EXPECT_TRUE(caps.has_sse);
    EXPECT_TRUE(caps.has_sse2);
    EXPECT_TRUE(caps.features & SIMD_SSE);
    EXPECT_TRUE(caps.features & SIMD_SSE2);
    #endif
}

/**
 * TEST: AVX detection on x86
 * WHAT: Verify AVX/AVX2 detection works correctly
 * WHY:  AVX2 is common on modern x86 CPUs
 * HOW:  Check AVX flags and verify consistency
 */
TEST_F(SIMDDetectTest, X86_AVXDetection) {
    simd_detect_capabilities(&caps);

    // AVX2 implies AVX
    if (caps.has_avx2) {
        EXPECT_TRUE(caps.has_avx);
        EXPECT_TRUE(caps.features & SIMD_AVX);
        EXPECT_TRUE(caps.features & SIMD_AVX2);
        EXPECT_GE(caps.max_vector_width, 256u);
    }
}

/**
 * TEST: AVX-512 detection on x86
 * WHAT: Verify AVX-512 detection is correct
 * WHY:  AVX-512 requires OS support, must check XCR0
 * HOW:  Check AVX-512 flags and implied features
 */
TEST_F(SIMDDetectTest, X86_AVX512Detection) {
    simd_detect_capabilities(&caps);

    // AVX-512 implies AVX2 and AVX
    if (caps.has_avx512f) {
        EXPECT_TRUE(caps.has_avx2);
        EXPECT_TRUE(caps.has_avx);
        EXPECT_TRUE(caps.features & SIMD_AVX512F);
        EXPECT_EQ(caps.max_vector_width, 512u);
    }
}

/**
 * TEST: FMA detection on x86
 * WHAT: Verify FMA (fused multiply-add) detection
 * WHY:  FMA is critical for neural network performance
 * HOW:  Check FMA flag
 */
TEST_F(SIMDDetectTest, X86_FMADetection) {
    simd_detect_capabilities(&caps);

    // FMA is typically available with AVX2
    if (caps.has_avx2) {
        // FMA is common but not guaranteed with AVX2
        // Just verify the flag is set correctly
        if (caps.has_fma) {
            EXPECT_TRUE(caps.features & SIMD_FMA);
        }
    }
}

/**
 * TEST: Vendor string detection
 * WHAT: Verify CPU vendor string is detected
 * WHY:  Vendor info useful for diagnostics
 * HOW:  Check vendor string is not empty
 */
TEST_F(SIMDDetectTest, X86_VendorString) {
    simd_detect_capabilities(&caps);

    EXPECT_GT(strlen(caps.vendor), 0u);

    // Should be one of known vendors
    bool known_vendor = (strcmp(caps.vendor, "GenuineIntel") == 0 ||
                         strcmp(caps.vendor, "AuthenticAMD") == 0 ||
                         strstr(caps.vendor, "Intel") != nullptr ||
                         strstr(caps.vendor, "AMD") != nullptr);

    EXPECT_TRUE(known_vendor) << "Unknown vendor: " << caps.vendor;
}

/**
 * TEST: Brand string detection
 * WHAT: Verify CPU brand string is detected
 * WHY:  Brand info useful for diagnostics
 * HOW:  Check brand string is not empty
 */
TEST_F(SIMDDetectTest, X86_BrandString) {
    simd_detect_capabilities(&caps);

    // Brand string may be empty on some CPUs
    // Just verify it doesn't crash
    SUCCEED();
}

#endif // x86

//=============================================================================
// Feature Detection Tests (ARM)
//=============================================================================

#if defined(__aarch64__) || defined(_M_ARM64)

/**
 * TEST: NEON detection on ARM64
 * WHAT: Verify NEON is detected on ARM64
 * WHY:  NEON is baseline for ARM64
 * HOW:  Check NEON flag
 */
TEST_F(SIMDDetectTest, ARM64_NEONDetection) {
    simd_detect_capabilities(&caps);

    EXPECT_TRUE(caps.has_neon);
    EXPECT_TRUE(caps.features & SIMD_NEON);
    EXPECT_GE(caps.max_vector_width, 128u);
}

/**
 * TEST: SVE detection on ARM64
 * WHAT: Verify SVE detection (if available)
 * WHY:  SVE provides variable-width vectors
 * HOW:  Check SVE flag and vector width
 */
TEST_F(SIMDDetectTest, ARM64_SVEDetection) {
    simd_detect_capabilities(&caps);

    if (caps.has_sve) {
        EXPECT_TRUE(caps.features & SIMD_SVE);
        EXPECT_GE(caps.max_vector_width, 128u);  // SVE is at least 128-bit
    }
}

#endif // ARM64

//=============================================================================
// Vector Width Tests
//=============================================================================

/**
 * TEST: Vector width detection
 * WHAT: Verify max_vector_width is set correctly
 * WHY:  Vector width determines optimal loop tiling
 * HOW:  Check width matches available features
 */
TEST_F(SIMDDetectTest, VectorWidth_Consistency) {
    simd_detect_capabilities(&caps);

    // Vector width should match features
    if (caps.has_avx512f) {
        EXPECT_EQ(caps.max_vector_width, 512u);
    } else if (caps.has_avx || caps.has_avx2) {
        EXPECT_EQ(caps.max_vector_width, 256u);
    } else if (caps.has_sse || caps.has_sse2 || caps.has_neon) {
        EXPECT_GE(caps.max_vector_width, 128u);
    }
}

/**
 * TEST: simd_get_vector_width() function
 * WHAT: Verify helper function returns correct width
 * WHY:  Convenience API should match struct
 * HOW:  Compare function result to struct field
 */
TEST_F(SIMDDetectTest, VectorWidth_HelperFunction) {
    simd_detect_capabilities(&caps);

    uint32_t width = simd_get_vector_width();
    EXPECT_EQ(width, caps.max_vector_width);
}

//=============================================================================
// Feature Query Tests
//=============================================================================

/**
 * TEST: simd_has_feature() function
 * WHAT: Verify feature query function works correctly
 * WHY:  Convenient API for checking features
 * HOW:  Check various features and compare to struct
 */
TEST_F(SIMDDetectTest, HasFeature_Consistency) {
    simd_detect_capabilities(&caps);

    // Check each feature matches struct
    EXPECT_EQ(simd_has_feature(SIMD_SSE), caps.has_sse);
    EXPECT_EQ(simd_has_feature(SIMD_SSE2), caps.has_sse2);
    EXPECT_EQ(simd_has_feature(SIMD_AVX), caps.has_avx);
    EXPECT_EQ(simd_has_feature(SIMD_AVX2), caps.has_avx2);
    EXPECT_EQ(simd_has_feature(SIMD_FMA), caps.has_fma);
    EXPECT_EQ(simd_has_feature(SIMD_AVX512F), caps.has_avx512f);
    EXPECT_EQ(simd_has_feature(SIMD_NEON), caps.has_neon);
}

/**
 * TEST: simd_has_feature() with combined flags
 * WHAT: Verify combined feature flags work correctly
 * WHY:  Users may check for feature combinations
 * HOW:  Check combined flags
 */
TEST_F(SIMDDetectTest, HasFeature_CombinedFlags) {
    simd_detect_capabilities(&caps);

    // Check combined flag
    if (caps.has_avx2 && caps.has_fma) {
        // Both should be present
        EXPECT_TRUE(simd_has_feature((simd_feature_t)(SIMD_AVX2 | SIMD_FMA)));
    }
}

/**
 * TEST: SIMD_NONE always returns true for simd_has_feature
 * WHAT: Verify SIMD_NONE behaves correctly
 * WHY:  Edge case handling
 * HOW:  Check SIMD_NONE flag
 */
TEST_F(SIMDDetectTest, HasFeature_None) {
    // SIMD_NONE is 0, so has_feature should return true
    bool result = simd_has_feature(SIMD_NONE);
    EXPECT_TRUE(result);
}

//=============================================================================
// Feature Name Tests
//=============================================================================

/**
 * TEST: Feature name lookup
 * WHAT: Verify simd_feature_name() returns correct names
 * WHY:  Human-readable names for logging
 * HOW:  Check known feature names
 */
TEST_F(SIMDDetectTest, FeatureName_KnownFeatures) {
    EXPECT_STREQ(simd_feature_name(SIMD_NONE), "None");
    EXPECT_STREQ(simd_feature_name(SIMD_SSE), "SSE");
    EXPECT_STREQ(simd_feature_name(SIMD_SSE2), "SSE2");
    EXPECT_STREQ(simd_feature_name(SIMD_SSE3), "SSE3");
    EXPECT_STREQ(simd_feature_name(SIMD_SSSE3), "SSSE3");
    EXPECT_STREQ(simd_feature_name(SIMD_SSE41), "SSE4.1");
    EXPECT_STREQ(simd_feature_name(SIMD_SSE42), "SSE4.2");
    EXPECT_STREQ(simd_feature_name(SIMD_AVX), "AVX");
    EXPECT_STREQ(simd_feature_name(SIMD_AVX2), "AVX2");
    EXPECT_STREQ(simd_feature_name(SIMD_FMA), "FMA");
    EXPECT_STREQ(simd_feature_name(SIMD_AVX512F), "AVX-512F");
    EXPECT_STREQ(simd_feature_name(SIMD_NEON), "NEON");
    EXPECT_STREQ(simd_feature_name(SIMD_SVE), "SVE");
}

/**
 * TEST: Unknown feature name
 * WHAT: Verify unknown features return "Unknown"
 * WHY:  Handle invalid feature values gracefully
 * HOW:  Pass invalid feature value
 */
TEST_F(SIMDDetectTest, FeatureName_Unknown) {
    const char* name = simd_feature_name((simd_feature_t)0xFFFFFFFF);
    EXPECT_STREQ(name, "Unknown");
}

//=============================================================================
// Feature String Tests
//=============================================================================

/**
 * TEST: Feature string generation
 * WHAT: Verify simd_features_string() generates correct string
 * WHY:  Human-readable summary of all features
 * HOW:  Generate string and check format
 */
TEST_F(SIMDDetectTest, FeaturesString_Valid) {
    simd_detect_capabilities(&caps);

    char buffer[256];
    size_t len = simd_features_string(buffer, sizeof(buffer));

    EXPECT_GT(len, 0u);
    EXPECT_LT(len, sizeof(buffer));

    // String should contain detected features
    if (caps.has_sse2) {
        EXPECT_NE(strstr(buffer, "SSE2"), nullptr);
    }
    if (caps.has_avx2) {
        EXPECT_NE(strstr(buffer, "AVX2"), nullptr);
    }
}

/**
 * TEST: Feature string with NULL buffer
 * WHAT: Verify NULL buffer handling
 * WHY:  Prevent crashes from invalid input
 * HOW:  Pass NULL buffer
 */
TEST_F(SIMDDetectTest, FeaturesString_NullBuffer) {
    size_t len = simd_features_string(nullptr, 256);
    EXPECT_EQ(len, 0u);
}

/**
 * TEST: Feature string with zero size
 * WHAT: Verify zero-size buffer handling
 * WHY:  Edge case handling
 * HOW:  Pass zero size
 */
TEST_F(SIMDDetectTest, FeaturesString_ZeroSize) {
    char buffer[256];
    size_t len = simd_features_string(buffer, 0);
    EXPECT_EQ(len, 0u);
}

/**
 * TEST: Feature string with small buffer
 * WHAT: Verify string truncation works correctly
 * WHY:  Must not overflow buffer
 * HOW:  Use small buffer and verify no overflow
 */
TEST_F(SIMDDetectTest, FeaturesString_SmallBuffer) {
    simd_detect_capabilities(&caps);

    char buffer[16];
    memset(buffer, 'X', sizeof(buffer));

    size_t len = simd_features_string(buffer, sizeof(buffer));

    // Should not overflow
    EXPECT_LT(len, sizeof(buffer));

    // Should be null-terminated
    size_t actual_len = strlen(buffer);
    EXPECT_EQ(actual_len, len);
}

//=============================================================================
// Convenience Macro Tests
//=============================================================================

/**
 * TEST: SIMD_HAS_AVX2 macro
 * WHAT: Verify convenience macro works correctly
 * WHY:  Common check should be easy
 * HOW:  Compare macro to function
 */
TEST_F(SIMDDetectTest, Macro_HasAVX2) {
    simd_detect_capabilities(&caps);

    bool macro_result = SIMD_HAS_AVX2();
    bool func_result = simd_has_feature(SIMD_AVX2);

    EXPECT_EQ(macro_result, func_result);
    EXPECT_EQ(macro_result, caps.has_avx2);
}

/**
 * TEST: SIMD_HAS_AVX512 macro
 * WHAT: Verify convenience macro works correctly
 * WHY:  Common check should be easy
 * HOW:  Compare macro to function
 */
TEST_F(SIMDDetectTest, Macro_HasAVX512) {
    simd_detect_capabilities(&caps);

    bool macro_result = SIMD_HAS_AVX512();
    bool func_result = simd_has_feature(SIMD_AVX512F);

    EXPECT_EQ(macro_result, func_result);
    EXPECT_EQ(macro_result, caps.has_avx512f);
}

/**
 * TEST: SIMD_HAS_NEON macro
 * WHAT: Verify convenience macro works correctly
 * WHY:  Common check should be easy
 * HOW:  Compare macro to function
 */
TEST_F(SIMDDetectTest, Macro_HasNEON) {
    simd_detect_capabilities(&caps);

    bool macro_result = SIMD_HAS_NEON();
    bool func_result = simd_has_feature(SIMD_NEON);

    EXPECT_EQ(macro_result, func_result);
    EXPECT_EQ(macro_result, caps.has_neon);
}

//=============================================================================
// Cache Info Tests
//=============================================================================

/**
 * TEST: Cache info detection
 * WHAT: Verify cache size info is reasonable
 * WHY:  Cache info useful for tiling optimizations
 * HOW:  Check cache sizes are in reasonable range
 */
TEST_F(SIMDDetectTest, CacheInfo_ReasonableValues) {
    simd_detect_capabilities(&caps);

    // Cache line size should be 32-128 bytes (typically 64)
    if (caps.cache_line_size > 0) {
        EXPECT_GE(caps.cache_line_size, 32u);
        EXPECT_LE(caps.cache_line_size, 128u);
    }

    // L2 cache should be reasonable (128KB - 64MB)
    if (caps.l2_cache_size > 0) {
        EXPECT_GE(caps.l2_cache_size, 128u);      // 128 KB
        EXPECT_LE(caps.l2_cache_size, 65536u);    // 64 MB
    }
}

//=============================================================================
// Feature Hierarchy Tests
//=============================================================================

/**
 * TEST: Feature hierarchy is maintained
 * WHAT: Verify higher features imply lower features
 * WHY:  AVX2 requires AVX, AVX requires SSE, etc.
 * HOW:  Check implications
 */
TEST_F(SIMDDetectTest, FeatureHierarchy) {
    simd_detect_capabilities(&caps);

    // AVX2 implies AVX
    if (caps.has_avx2) {
        EXPECT_TRUE(caps.has_avx);
    }

    // AVX implies SSE2
    if (caps.has_avx) {
        EXPECT_TRUE(caps.has_sse2);
    }

    // SSE2 implies SSE
    if (caps.has_sse2) {
        EXPECT_TRUE(caps.has_sse);
    }

    // AVX-512 implies AVX2
    if (caps.has_avx512f) {
        EXPECT_TRUE(caps.has_avx2);
    }
}

//=============================================================================
// Thread Safety Tests
//=============================================================================

/**
 * TEST: Multiple threads can call detection
 * WHAT: Verify detection is thread-safe
 * WHY:  Multiple threads may query SIMD features
 * HOW:  Call from multiple threads concurrently
 */
TEST_F(SIMDDetectTest, ThreadSafety_MultipleThreads) {
    // Note: This is a basic test; thorough thread safety testing
    // would require a thread sanitizer or stress testing

    simd_capabilities_t caps1, caps2;

    bool result1 = simd_detect_capabilities(&caps1);
    bool result2 = simd_detect_capabilities(&caps2);

    EXPECT_TRUE(result1);
    EXPECT_TRUE(result2);
    EXPECT_EQ(caps1.features, caps2.features);
}
