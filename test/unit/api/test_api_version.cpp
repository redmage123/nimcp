/**
 * @file test_api_version.cpp
 * @brief GoogleTest unit tests for NIMCP API version functions
 *
 * Tests version API functions to ensure correct version reporting
 * and semantic versioning compliance.
 */

#include <gtest/gtest.h>
#include "nimcp.h"
#include <string.h>

/**
 * @brief Test fixture for API version tests
 */
class APIVersionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // No setup needed for version tests
    }

    void TearDown() override {
        // No cleanup needed for version tests
    }
};

/**
 * @brief Test that nimcp_version() returns a non-NULL string
 */
TEST_F(APIVersionTest, VersionStringNotNull) {
    const char* version = nimcp_version();
    EXPECT_NE(version, nullptr);
}

/**
 * @brief Test that nimcp_version() returns the correct version string
 */
TEST_F(APIVersionTest, VersionStringCorrect) {
    const char* version = nimcp_version();
    EXPECT_STREQ(version, NIMCP_VERSION_STRING);
    EXPECT_STREQ(version, "2.6.3");
}

/**
 * @brief Test that nimcp_version_int() returns correct integer version
 */
TEST_F(APIVersionTest, VersionIntCorrect) {
    int version = nimcp_version_int();
    int expected = NIMCP_VERSION_MAJOR * 10000 +
                   NIMCP_VERSION_MINOR * 100 +
                   NIMCP_VERSION_PATCH;
    EXPECT_EQ(version, expected);
    EXPECT_EQ(version, 20603);  // 2.6.3 = 20603
}

/**
 * @brief Test that version format follows semantic versioning
 */
TEST_F(APIVersionTest, VersionFormatSemanticVersioning) {
    const char* version = nimcp_version();

    // Should have format: MAJOR.MINOR.PATCH
    int major, minor, patch;
    int matched = sscanf(version, "%d.%d.%d", &major, &minor, &patch);

    EXPECT_EQ(matched, 3) << "Version string should have 3 components";
    EXPECT_GE(major, 0) << "Major version should be >= 0";
    EXPECT_GE(minor, 0) << "Minor version should be >= 0";
    EXPECT_GE(patch, 0) << "Patch version should be >= 0";
}

/**
 * @brief Test that version integer matches string components
 */
TEST_F(APIVersionTest, VersionIntMatchesString) {
    const char* version_str = nimcp_version();
    int version_int = nimcp_version_int();

    int major, minor, patch;
    sscanf(version_str, "%d.%d.%d", &major, &minor, &patch);

    int expected = major * 10000 + minor * 100 + patch;
    EXPECT_EQ(version_int, expected);
}

/**
 * @brief Test version constants are consistent
 */
TEST_F(APIVersionTest, VersionConstantsConsistent) {
    // Check that the constants match
    EXPECT_EQ(NIMCP_VERSION_MAJOR, 2);
    EXPECT_EQ(NIMCP_VERSION_MINOR, 6);
    EXPECT_EQ(NIMCP_VERSION_PATCH, 3);

    // Check that string constant matches component constants
    char expected[64];
    snprintf(expected, sizeof(expected), "%d.%d.%d",
             NIMCP_VERSION_MAJOR, NIMCP_VERSION_MINOR, NIMCP_VERSION_PATCH);
    EXPECT_STREQ(NIMCP_VERSION_STRING, expected);
}
