/**
 * @file test_path_traversal.cpp
 * @brief Unit Tests for Path Traversal Detection
 *
 * WHAT: Comprehensive unit tests for path traversal detection module
 * WHY:  Ensure all attack patterns are detected correctly
 * HOW:  Test each pattern type, edge cases, and configuration options
 *
 * @author NIMCP Team
 * @date 2025-12-07
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "security/nimcp_path_traversal.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

//=============================================================================
// Test Fixture
//=============================================================================

class PathTraversalTest : public ::testing::Test {
protected:
    nimcp_path_validator_t validator;
    nimcp_path_validator_config_t config;

    void SetUp() override {
        /* Get default config */
        config = nimcp_path_validator_default_config();

        /* Create validator with default config */
        validator = nimcp_path_validator_create(&config);
        ASSERT_NE(nullptr, validator);
    }

    void TearDown() override {
        if (validator) {
            nimcp_path_validator_destroy(validator);
            validator = nullptr;
        }
    }

    /* Helper: Validate path and expect threat */
    void expect_threat(const char* path, nimcp_path_pattern_t expected_pattern) {
        nimcp_path_validation_result_t result;
        nimcp_path_error_t err = nimcp_path_validate(
            validator, path, NIMCP_PATH_CONTEXT_AUTO, &result);

        EXPECT_EQ(NIMCP_PATH_ERROR_THREAT_DETECTED, err);
        EXPECT_FALSE(result.valid);
        EXPECT_EQ(expected_pattern, result.pattern);
        EXPECT_GT(result.severity, NIMCP_PATH_SEVERITY_NONE);
    }

    /* Helper: Validate path and expect success */
    void expect_success(const char* path) {
        nimcp_path_validation_result_t result;
        nimcp_path_error_t err = nimcp_path_validate(
            validator, path, NIMCP_PATH_CONTEXT_AUTO, &result);

        EXPECT_EQ(NIMCP_PATH_SUCCESS, err);
        EXPECT_TRUE(result.valid);
        EXPECT_EQ(NIMCP_PATH_PATTERN_NONE, result.pattern);
    }
};

//=============================================================================
// Basic Tests
//=============================================================================

TEST_F(PathTraversalTest, CreateDestroy) {
    /* Test create/destroy cycle */
    nimcp_path_validator_t v = nimcp_path_validator_create(nullptr);
    ASSERT_NE(nullptr, v);
    nimcp_path_validator_destroy(v);
}

TEST_F(PathTraversalTest, NullParameters) {
    nimcp_path_validation_result_t result;

    /* NULL validator */
    nimcp_path_error_t err = nimcp_path_validate(
        nullptr, "test", NIMCP_PATH_CONTEXT_AUTO, &result);
    EXPECT_EQ(NIMCP_PATH_ERROR_NULL_POINTER, err);

    /* NULL path */
    err = nimcp_path_validate(
        validator, nullptr, NIMCP_PATH_CONTEXT_AUTO, &result);
    EXPECT_EQ(NIMCP_PATH_ERROR_NULL_POINTER, err);

    /* NULL result */
    err = nimcp_path_validate(
        validator, "test", NIMCP_PATH_CONTEXT_AUTO, nullptr);
    EXPECT_EQ(NIMCP_PATH_ERROR_INVALID_PARAM, err);
}

TEST_F(PathTraversalTest, EmptyPath) {
    expect_success("");
}

TEST_F(PathTraversalTest, ValidPaths) {
    expect_success("file.txt");
    expect_success("documents/report.pdf");
    expect_success("images/photo.jpg");
    expect_success("data/2024/january/data.csv");
    expect_success("./current/file.txt");
    expect_success("subdir/file.txt");
}

//=============================================================================
// Basic Traversal Pattern Tests
//=============================================================================

TEST_F(PathTraversalTest, BasicTraversal_UnixForwardSlash) {
    expect_threat("../file.txt", NIMCP_PATH_PATTERN_BASIC);
    expect_threat("../../etc/passwd", NIMCP_PATH_PATTERN_BASIC);
    expect_threat("../../../root/.ssh/id_rsa", NIMCP_PATH_PATTERN_BASIC);
    expect_threat("dir/../../../etc/shadow", NIMCP_PATH_PATTERN_BASIC);
}

TEST_F(PathTraversalTest, BasicTraversal_WindowsBackslash) {
    expect_threat("..\\file.txt", NIMCP_PATH_PATTERN_BASIC);
    expect_threat("..\\..\\windows\\system32", NIMCP_PATH_PATTERN_BASIC);
    expect_threat("..\\..\\..\\boot.ini", NIMCP_PATH_PATTERN_BASIC);
}

TEST_F(PathTraversalTest, BasicTraversal_Semicolon) {
    expect_threat("..;/file.txt", NIMCP_PATH_PATTERN_BASIC);
    expect_threat("..;/etc/passwd", NIMCP_PATH_PATTERN_BASIC);
}

TEST_F(PathTraversalTest, BasicTraversal_MultipleDots) {
    expect_threat("....//etc/passwd", NIMCP_PATH_PATTERN_BASIC);
    expect_threat("..../etc/passwd", NIMCP_PATH_PATTERN_BASIC);
}

//=============================================================================
// URL Encoded Pattern Tests
//=============================================================================

TEST_F(PathTraversalTest, URLEncoded_DotDotSlash) {
    expect_threat("%2e%2e%2ffile.txt", NIMCP_PATH_PATTERN_URL_ENCODED);
    expect_threat("%2e%2e/file.txt", NIMCP_PATH_PATTERN_URL_ENCODED);
    /* Note: ..%2f contains literal ".." which is caught by basic detection first */
    expect_threat("..%2ffile.txt", NIMCP_PATH_PATTERN_BASIC);
}

TEST_F(PathTraversalTest, URLEncoded_DotDotBackslash) {
    expect_threat("%2e%2e%5cfile.txt", NIMCP_PATH_PATTERN_URL_ENCODED);
    expect_threat("%2e%2e\\file.txt", NIMCP_PATH_PATTERN_URL_ENCODED);
    /* Note: ..%5c contains literal ".." which is caught by basic detection first */
    expect_threat("..%5cfile.txt", NIMCP_PATH_PATTERN_BASIC);
}

TEST_F(PathTraversalTest, URLEncoded_PartialEncoding) {
    expect_threat("%2e.%2ffile.txt", NIMCP_PATH_PATTERN_URL_ENCODED);
    expect_threat(".%2e/file.txt", NIMCP_PATH_PATTERN_URL_ENCODED);
}

TEST_F(PathTraversalTest, URLEncoded_CaseMixing) {
    expect_threat("%2E%2E%2Ffile.txt", NIMCP_PATH_PATTERN_URL_ENCODED);
    expect_threat("%2E%2e/file.txt", NIMCP_PATH_PATTERN_URL_ENCODED);
}

//=============================================================================
// Double URL Encoded Tests
//=============================================================================

TEST_F(PathTraversalTest, DoubleEncoded_DotDotSlash) {
    expect_threat("%252e%252e%252ffile.txt", NIMCP_PATH_PATTERN_DOUBLE_ENCODED);
    expect_threat("%252e%252e%252f%252e%252e%252f",
                  NIMCP_PATH_PATTERN_DOUBLE_ENCODED);
}

TEST_F(PathTraversalTest, DoubleEncoded_DotDotBackslash) {
    expect_threat("%252e%252e%255cfile.txt", NIMCP_PATH_PATTERN_DOUBLE_ENCODED);
}

TEST_F(PathTraversalTest, DoubleEncoded_DotDot) {
    expect_threat("%252e%252e", NIMCP_PATH_PATTERN_DOUBLE_ENCODED);
}

//=============================================================================
// Unicode/UTF-8 Encoded Tests
//=============================================================================

TEST_F(PathTraversalTest, Unicode_OverlongDotDotSlash) {
    expect_threat("%c0%ae%c0%ae%c0%affile.txt", NIMCP_PATH_PATTERN_UNICODE);
}

TEST_F(PathTraversalTest, Unicode_PartialOverlong) {
    /* Note: ..%c0%af contains literal ".." which is caught by basic detection first */
    expect_threat("..%c0%affile.txt", NIMCP_PATH_PATTERN_BASIC);
    expect_threat("%c0%ae%c0%aetest", NIMCP_PATH_PATTERN_UNICODE);
}

TEST_F(PathTraversalTest, Unicode_AlternateEncodings) {
    expect_threat("%c0%2efile.txt", NIMCP_PATH_PATTERN_UNICODE);
    expect_threat("%e0%80%aefile.txt", NIMCP_PATH_PATTERN_UNICODE);
}

//=============================================================================
// Null Byte Injection Tests
//=============================================================================

TEST_F(PathTraversalTest, NullByte_HexEncoded) {
    expect_threat("../../etc/passwd%00.jpg", NIMCP_PATH_PATTERN_NULL_BYTE);
    expect_threat("../../../secret%00.txt", NIMCP_PATH_PATTERN_NULL_BYTE);
}

TEST_F(PathTraversalTest, NullByte_ShortEncoding) {
    expect_threat("../../etc/passwd%0.jpg", NIMCP_PATH_PATTERN_NULL_BYTE);
}

//=============================================================================
// Absolute Path Tests
//=============================================================================

TEST_F(PathTraversalTest, AbsolutePath_UnixSystem) {
    expect_threat("/etc/passwd", NIMCP_PATH_PATTERN_ABSOLUTE);
    expect_threat("/usr/bin/curl", NIMCP_PATH_PATTERN_ABSOLUTE);
    expect_threat("/var/log/auth.log", NIMCP_PATH_PATTERN_ABSOLUTE);
    expect_threat("/bin/sh", NIMCP_PATH_PATTERN_ABSOLUTE);
    expect_threat("/root/.ssh/id_rsa", NIMCP_PATH_PATTERN_ABSOLUTE);
    expect_threat("/home/user/.bashrc", NIMCP_PATH_PATTERN_ABSOLUTE);
}

TEST_F(PathTraversalTest, AbsolutePath_UnixSpecial) {
    expect_threat("/proc/self/environ", NIMCP_PATH_PATTERN_ABSOLUTE);
    expect_threat("/sys/class/net", NIMCP_PATH_PATTERN_ABSOLUTE);
}

TEST_F(PathTraversalTest, AbsolutePath_Windows) {
    expect_threat("c:\\windows\\system32", NIMCP_PATH_PATTERN_ABSOLUTE);
    expect_threat("d:\\data\\secret.txt", NIMCP_PATH_PATTERN_ABSOLUTE);
    expect_threat("e:\\backup", NIMCP_PATH_PATTERN_ABSOLUTE);
}

TEST_F(PathTraversalTest, AbsolutePath_UNC) {
    expect_threat("\\\\server\\share", NIMCP_PATH_PATTERN_ABSOLUTE);
}

TEST_F(PathTraversalTest, AbsolutePath_FileURI) {
    expect_threat("file:///etc/passwd", NIMCP_PATH_PATTERN_ABSOLUTE);
}

//=============================================================================
// Windows-Specific Pattern Tests
//=============================================================================

TEST_F(PathTraversalTest, Windows_MultipleBackslash) {
    /* Note: ..\\ is caught by basic traversal detection first */
    expect_threat("..\\..\\file.txt", NIMCP_PATH_PATTERN_BASIC);
}

TEST_F(PathTraversalTest, Windows_QuadrupleBackslash) {
    /* Note: .... contains ".." which is caught by basic detection first */
    expect_threat("....\\\\file.txt", NIMCP_PATH_PATTERN_BASIC);
}

TEST_F(PathTraversalTest, Windows_DotBackslashDot) {
    expect_threat(".\\.\\file.txt", NIMCP_PATH_PATTERN_WINDOWS);
}

TEST_F(PathTraversalTest, Windows_Win32Namespace) {
    /* Note: \\\\ is caught by absolute path (UNC) detection first */
    expect_threat("\\\\?\\C:\\file.txt", NIMCP_PATH_PATTERN_ABSOLUTE);
}

TEST_F(PathTraversalTest, Windows_DeviceNamespace) {
    /* Note: \\\\ is caught by absolute path (UNC) detection first */
    expect_threat("\\\\.\\PhysicalDrive0", NIMCP_PATH_PATTERN_ABSOLUTE);
}

//=============================================================================
// Normalization Tests
//=============================================================================

TEST_F(PathTraversalTest, Normalization_Basic) {
    char normalized[256];
    nimcp_path_error_t err;

    /* Single dot */
    err = nimcp_path_normalize("./file.txt", normalized, sizeof(normalized));
    EXPECT_EQ(NIMCP_PATH_SUCCESS, err);
    EXPECT_STREQ("./file.txt", normalized);

    /* Double slashes */
    err = nimcp_path_normalize("dir//file.txt", normalized, sizeof(normalized));
    EXPECT_EQ(NIMCP_PATH_SUCCESS, err);
    EXPECT_STREQ("dir/file.txt", normalized);

    /* Trailing slash */
    err = nimcp_path_normalize("dir/", normalized, sizeof(normalized));
    EXPECT_EQ(NIMCP_PATH_SUCCESS, err);
    EXPECT_STREQ("dir", normalized);
}

TEST_F(PathTraversalTest, Normalization_Empty) {
    char normalized[256];
    nimcp_path_error_t err = nimcp_path_normalize("", normalized,
                                                   sizeof(normalized));
    EXPECT_EQ(NIMCP_PATH_SUCCESS, err);
    EXPECT_STREQ(".", normalized);
}

TEST_F(PathTraversalTest, Normalization_BufferTooSmall) {
    char normalized[5];
    nimcp_path_error_t err = nimcp_path_normalize(
        "very/long/path/name", normalized, sizeof(normalized));
    EXPECT_EQ(NIMCP_PATH_ERROR_BUFFER_TOO_SMALL, err);
}

//=============================================================================
// URL Decode Tests
//=============================================================================

TEST_F(PathTraversalTest, URLDecode_Basic) {
    char decoded[256];
    nimcp_path_error_t err;

    err = nimcp_path_url_decode("hello%20world", decoded, sizeof(decoded));
    EXPECT_EQ(NIMCP_PATH_SUCCESS, err);
    EXPECT_STREQ("hello world", decoded);

    err = nimcp_path_url_decode("test%2Fpath", decoded, sizeof(decoded));
    EXPECT_EQ(NIMCP_PATH_SUCCESS, err);
    EXPECT_STREQ("test/path", decoded);
}

TEST_F(PathTraversalTest, URLDecode_PlusToSpace) {
    char decoded[256];
    nimcp_path_error_t err = nimcp_path_url_decode("hello+world", decoded,
                                                    sizeof(decoded));
    EXPECT_EQ(NIMCP_PATH_SUCCESS, err);
    EXPECT_STREQ("hello world", decoded);
}

TEST_F(PathTraversalTest, URLDecode_InvalidHex) {
    char decoded[256];
    nimcp_path_error_t err = nimcp_path_url_decode("test%ZZinvalid", decoded,
                                                    sizeof(decoded));
    EXPECT_EQ(NIMCP_PATH_SUCCESS, err);
    /* Invalid hex should be kept as-is */
    EXPECT_STREQ("test%ZZinvalid", decoded);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(PathTraversalTest, Statistics_Tracking) {
    nimcp_path_validation_result_t result;
    nimcp_path_validator_stats_t stats;

    /* Initial stats should be zero */
    nimcp_path_validator_get_stats(validator, &stats);
    EXPECT_EQ(0, stats.total_validations);
    EXPECT_EQ(0, stats.threats_detected);

    /* Validate some paths */
    nimcp_path_validate(validator, "safe.txt", NIMCP_PATH_CONTEXT_AUTO, &result);
    nimcp_path_validate(validator, "../bad.txt", NIMCP_PATH_CONTEXT_AUTO, &result);
    nimcp_path_validate(validator, "also_safe.txt", NIMCP_PATH_CONTEXT_AUTO, &result);

    /* Check updated stats */
    nimcp_path_validator_get_stats(validator, &stats);
    EXPECT_EQ(3, stats.total_validations);
    EXPECT_EQ(1, stats.threats_detected);
    EXPECT_EQ(1, stats.basic_patterns);
}

TEST_F(PathTraversalTest, Statistics_Reset) {
    nimcp_path_validation_result_t result;
    nimcp_path_validator_stats_t stats;

    /* Generate some stats */
    nimcp_path_validate(validator, "../bad.txt", NIMCP_PATH_CONTEXT_AUTO, &result);
    nimcp_path_validator_get_stats(validator, &stats);
    EXPECT_GT(stats.total_validations, 0);

    /* Reset */
    nimcp_path_error_t err = nimcp_path_validator_reset_stats(validator);
    EXPECT_EQ(NIMCP_PATH_SUCCESS, err);

    /* Check stats are zero */
    nimcp_path_validator_get_stats(validator, &stats);
    EXPECT_EQ(0, stats.total_validations);
    EXPECT_EQ(0, stats.threats_detected);
}

//=============================================================================
// Utility Function Tests
//=============================================================================

TEST_F(PathTraversalTest, PatternName) {
    EXPECT_STREQ("NONE", nimcp_path_pattern_name(NIMCP_PATH_PATTERN_NONE));
    EXPECT_STREQ("BASIC", nimcp_path_pattern_name(NIMCP_PATH_PATTERN_BASIC));
    EXPECT_STREQ("URL_ENCODED",
                 nimcp_path_pattern_name(NIMCP_PATH_PATTERN_URL_ENCODED));
    EXPECT_STREQ("DOUBLE_ENCODED",
                 nimcp_path_pattern_name(NIMCP_PATH_PATTERN_DOUBLE_ENCODED));
    EXPECT_STREQ("UNICODE", nimcp_path_pattern_name(NIMCP_PATH_PATTERN_UNICODE));
    EXPECT_STREQ("NULL_BYTE",
                 nimcp_path_pattern_name(NIMCP_PATH_PATTERN_NULL_BYTE));
    EXPECT_STREQ("ABSOLUTE",
                 nimcp_path_pattern_name(NIMCP_PATH_PATTERN_ABSOLUTE));
    EXPECT_STREQ("NORMALIZED",
                 nimcp_path_pattern_name(NIMCP_PATH_PATTERN_NORMALIZED));
    EXPECT_STREQ("WINDOWS", nimcp_path_pattern_name(NIMCP_PATH_PATTERN_WINDOWS));
}

TEST_F(PathTraversalTest, SeverityName) {
    EXPECT_STREQ("NONE", nimcp_path_severity_name(NIMCP_PATH_SEVERITY_NONE));
    EXPECT_STREQ("LOW", nimcp_path_severity_name(NIMCP_PATH_SEVERITY_LOW));
    EXPECT_STREQ("MEDIUM", nimcp_path_severity_name(NIMCP_PATH_SEVERITY_MEDIUM));
    EXPECT_STREQ("HIGH", nimcp_path_severity_name(NIMCP_PATH_SEVERITY_HIGH));
    EXPECT_STREQ("CRITICAL",
                 nimcp_path_severity_name(NIMCP_PATH_SEVERITY_CRITICAL));
}

TEST_F(PathTraversalTest, ErrorName) {
    EXPECT_STREQ("SUCCESS", nimcp_path_error_name(NIMCP_PATH_SUCCESS));
    EXPECT_STREQ("INVALID_PARAM",
                 nimcp_path_error_name(NIMCP_PATH_ERROR_INVALID_PARAM));
    EXPECT_STREQ("NULL_POINTER",
                 nimcp_path_error_name(NIMCP_PATH_ERROR_NULL_POINTER));
    EXPECT_STREQ("BUFFER_TOO_SMALL",
                 nimcp_path_error_name(NIMCP_PATH_ERROR_BUFFER_TOO_SMALL));
    EXPECT_STREQ("THREAT_DETECTED",
                 nimcp_path_error_name(NIMCP_PATH_ERROR_THREAT_DETECTED));
}

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(PathTraversalTest, Config_DisableBasic) {
    /* Create validator with basic detection disabled */
    nimcp_path_validator_config_t cfg = nimcp_path_validator_default_config();
    cfg.enable_basic_detection = false;

    nimcp_path_validator_t v = nimcp_path_validator_create(&cfg);
    ASSERT_NE(nullptr, v);

    /* Basic traversal should now pass */
    nimcp_path_validation_result_t result;
    nimcp_path_error_t err = nimcp_path_validate(
        v, "../file.txt", NIMCP_PATH_CONTEXT_AUTO, &result);
    EXPECT_EQ(NIMCP_PATH_SUCCESS, err);
    EXPECT_TRUE(result.valid);

    nimcp_path_validator_destroy(v);
}

TEST_F(PathTraversalTest, Config_MaxPathLength) {
    /* Create validator with small max length */
    nimcp_path_validator_config_t cfg = nimcp_path_validator_default_config();
    cfg.max_path_length = 10;

    nimcp_path_validator_t v = nimcp_path_validator_create(&cfg);
    ASSERT_NE(nullptr, v);

    /* Short path should pass */
    nimcp_path_validation_result_t result;
    nimcp_path_error_t err = nimcp_path_validate(
        v, "short.txt", NIMCP_PATH_CONTEXT_AUTO, &result);
    EXPECT_EQ(NIMCP_PATH_SUCCESS, err);

    /* Long path should fail */
    err = nimcp_path_validate(
        v, "very_long_filename.txt", NIMCP_PATH_CONTEXT_AUTO, &result);
    EXPECT_EQ(NIMCP_PATH_ERROR_THREAT_DETECTED, err);
    EXPECT_FALSE(result.valid);

    nimcp_path_validator_destroy(v);
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(PathTraversalTest, EdgeCase_MultiplePatterns) {
    /* Path with multiple different patterns */
    nimcp_path_validation_result_t result;
    nimcp_path_error_t err = nimcp_path_validate(
        validator, "../../%2e%2e/etc/passwd",
        NIMCP_PATH_CONTEXT_AUTO, &result);

    EXPECT_EQ(NIMCP_PATH_ERROR_THREAT_DETECTED, err);
    EXPECT_FALSE(result.valid);
    /* Should detect the first pattern encountered */
}

TEST_F(PathTraversalTest, EdgeCase_VeryLongPath) {
    /* Create a very long path */
    std::string long_path = "";
    for (int i = 0; i < 100; i++) {
        long_path += "../";
    }

    expect_threat(long_path.c_str(), NIMCP_PATH_PATTERN_BASIC);
}

TEST_F(PathTraversalTest, EdgeCase_MixedSeparators) {
    /* Mix Unix and Windows separators */
    expect_threat("..\\/../file.txt", NIMCP_PATH_PATTERN_BASIC);
}

//=============================================================================
// Real-World Attack Examples
//=============================================================================

TEST_F(PathTraversalTest, RealWorld_EtcPasswd) {
    expect_threat("../../../../etc/passwd", NIMCP_PATH_PATTERN_BASIC);
    expect_threat("%2e%2e%2f%2e%2e%2f%2e%2e%2fetc/passwd",
                  NIMCP_PATH_PATTERN_URL_ENCODED);
}

TEST_F(PathTraversalTest, RealWorld_SSHKey) {
    expect_threat("../../../root/.ssh/id_rsa", NIMCP_PATH_PATTERN_BASIC);
}

TEST_F(PathTraversalTest, RealWorld_WindowsSystem32) {
    expect_threat("..\\..\\..\\windows\\system32\\config\\sam",
                  NIMCP_PATH_PATTERN_BASIC);
}

TEST_F(PathTraversalTest, RealWorld_NullByteBypass) {
    expect_threat("../../etc/passwd%00.jpg", NIMCP_PATH_PATTERN_NULL_BYTE);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
