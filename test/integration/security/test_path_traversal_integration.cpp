/**
 * @file test_path_traversal_integration.cpp
 * @brief Integration Tests for Path Traversal Detection with BBB and Bio-Async
 *
 * WHAT: Integration tests for path traversal detector with other NIMCP modules
 * WHY:  Ensure proper integration with BBB, bio-async messaging, and logging
 * HOW:  Test end-to-end workflows, message passing, and statistics tracking
 *
 * @author NIMCP Team
 * @date 2025-12-07
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>

// Headers have their own extern "C" guards
#include "security/nimcp_path_traversal.h"
#include "async/nimcp_bio_async.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class PathTraversalIntegrationTest : public ::testing::Test {
protected:
    nimcp_path_validator_t validator;
    nimcp_bio_async_t bio_async;

    void SetUp() override {
        /* Initialize bio-async system */
        bio_async = nimcp_bio_async_get_instance();
        if (!bio_async) {
            nimcp_error_t err = nimcp_bio_async_init();
            ASSERT_EQ(NIMCP_SUCCESS, err);
            bio_async = nimcp_bio_async_get_instance();
            ASSERT_NE(nullptr, bio_async);
        }

        /* Create validator (should auto-register with bio-async) */
        validator = nimcp_path_validator_create(nullptr);
        ASSERT_NE(nullptr, validator);

        /* Give bio-async time to process registration */
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    void TearDown() override {
        if (validator) {
            nimcp_path_validator_destroy(validator);
            validator = nullptr;
        }
        /* Note: bio-async cleanup happens at program exit */
    }
};

//=============================================================================
// Bio-Async Integration Tests
//=============================================================================

TEST_F(PathTraversalIntegrationTest, BioAsyncRegistration) {
    /* Validator should be registered with bio-async */
    /* Verify the validator was created successfully in SetUp */
    ASSERT_NE(validator, nullptr) << "Validator should be created";
    ASSERT_NE(bio_async, nullptr) << "Bio-async instance should exist";

    /* Verify validator can perform basic operation after registration */
    nimcp_path_validation_result_t result;
    nimcp_path_error_t err = nimcp_path_validate(
        validator, "test.txt", NIMCP_PATH_CONTEXT_AUTO, &result);

    /* Validator should respond (either success or threat detected, but not error) */
    EXPECT_TRUE(err == NIMCP_PATH_SUCCESS || err == NIMCP_PATH_ERROR_THREAT_DETECTED)
        << "Validator should be functional after bio-async registration";
}

TEST_F(PathTraversalIntegrationTest, MultipleValidatorsCoexist) {
    /* Create multiple validators */
    nimcp_path_validator_t v1 = nimcp_path_validator_create(nullptr);
    nimcp_path_validator_t v2 = nimcp_path_validator_create(nullptr);
    nimcp_path_validator_t v3 = nimcp_path_validator_create(nullptr);

    ASSERT_NE(nullptr, v1);
    ASSERT_NE(nullptr, v2);
    ASSERT_NE(nullptr, v3);

    /* All should work independently */
    nimcp_path_validation_result_t result;

    EXPECT_EQ(NIMCP_PATH_SUCCESS,
              nimcp_path_validate(v1, "safe.txt",
                                 NIMCP_PATH_CONTEXT_AUTO, &result));
    EXPECT_EQ(NIMCP_PATH_ERROR_THREAT_DETECTED,
              nimcp_path_validate(v2, "../bad.txt",
                                 NIMCP_PATH_CONTEXT_AUTO, &result));
    EXPECT_EQ(NIMCP_PATH_SUCCESS,
              nimcp_path_validate(v3, "also_safe.txt",
                                 NIMCP_PATH_CONTEXT_AUTO, &result));

    nimcp_path_validator_destroy(v1);
    nimcp_path_validator_destroy(v2);
    nimcp_path_validator_destroy(v3);
}

//=============================================================================
// End-to-End Workflow Tests
//=============================================================================

TEST_F(PathTraversalIntegrationTest, FullWorkflow_ValidPath) {
    nimcp_path_validation_result_t result;
    nimcp_path_validator_stats_t stats;

    /* Get initial stats */
    nimcp_path_validator_get_stats(validator, &stats);
    uint64_t initial_validations = stats.total_validations;

    /* Validate a safe path */
    nimcp_path_error_t err = nimcp_path_validate(
        validator, "documents/report.pdf",
        NIMCP_PATH_CONTEXT_FILE, &result);

    /* Verify result */
    EXPECT_EQ(NIMCP_PATH_SUCCESS, err);
    EXPECT_TRUE(result.valid);
    EXPECT_EQ(NIMCP_PATH_PATTERN_NONE, result.pattern);
    EXPECT_EQ(NIMCP_PATH_SEVERITY_NONE, result.severity);

    /* Verify stats updated */
    nimcp_path_validator_get_stats(validator, &stats);
    EXPECT_EQ(initial_validations + 1, stats.total_validations);
    EXPECT_EQ(0, stats.threats_detected);
}

TEST_F(PathTraversalIntegrationTest, FullWorkflow_ThreatDetection) {
    nimcp_path_validation_result_t result;
    nimcp_path_validator_stats_t stats;

    /* Get initial stats */
    nimcp_path_validator_get_stats(validator, &stats);
    uint64_t initial_threats = stats.threats_detected;

    /* Validate a malicious path */
    nimcp_path_error_t err = nimcp_path_validate(
        validator, "../../etc/passwd",
        NIMCP_PATH_CONTEXT_FILE, &result);

    /* Verify threat detected */
    EXPECT_EQ(NIMCP_PATH_ERROR_THREAT_DETECTED, err);
    EXPECT_FALSE(result.valid);
    EXPECT_EQ(NIMCP_PATH_PATTERN_BASIC, result.pattern);
    EXPECT_GT(result.severity, NIMCP_PATH_SEVERITY_NONE);
    EXPECT_GT(strlen(result.reason), 0);
    EXPECT_GT(result.traversal_depth, 0);

    /* Verify stats updated */
    nimcp_path_validator_get_stats(validator, &stats);
    EXPECT_EQ(initial_threats + 1, stats.threats_detected);
    EXPECT_GT(stats.basic_patterns, 0);
}

//=============================================================================
// Multi-Pattern Detection Tests
//=============================================================================

TEST_F(PathTraversalIntegrationTest, DetectMultiplePatternTypes) {
    nimcp_path_validation_result_t result;
    nimcp_path_validator_stats_t stats;

    /* Reset stats */
    nimcp_path_validator_reset_stats(validator);

    /* Detect various pattern types */
    nimcp_path_validate(validator, "../file.txt",
                       NIMCP_PATH_CONTEXT_AUTO, &result);
    nimcp_path_validate(validator, "%2e%2e%2ffile",
                       NIMCP_PATH_CONTEXT_AUTO, &result);
    nimcp_path_validate(validator, "/etc/passwd",
                       NIMCP_PATH_CONTEXT_AUTO, &result);
    nimcp_path_validate(validator, "test%00.jpg",
                       NIMCP_PATH_CONTEXT_AUTO, &result);

    /* Verify different pattern types detected */
    nimcp_path_validator_get_stats(validator, &stats);
    EXPECT_EQ(4, stats.total_validations);
    EXPECT_EQ(4, stats.threats_detected);
    EXPECT_GT(stats.basic_patterns, 0);
    EXPECT_GT(stats.url_encoded_patterns, 0);
    EXPECT_GT(stats.absolute_paths, 0);
    EXPECT_GT(stats.null_byte_patterns, 0);
}

//=============================================================================
// Context-Specific Detection Tests
//=============================================================================

TEST_F(PathTraversalIntegrationTest, FileContextDetection) {
    nimcp_path_validation_result_t result;

    /* File context should detect traversal */
    nimcp_path_error_t err = nimcp_path_validate(
        validator, "../../secret.txt",
        NIMCP_PATH_CONTEXT_FILE, &result);

    EXPECT_EQ(NIMCP_PATH_ERROR_THREAT_DETECTED, err);
    EXPECT_FALSE(result.valid);
}

TEST_F(PathTraversalIntegrationTest, URLContextDetection) {
    nimcp_path_validation_result_t result;

    /* URL context should detect encoded traversal */
    nimcp_path_error_t err = nimcp_path_validate(
        validator, "%2e%2e%2ffile",
        NIMCP_PATH_CONTEXT_URL, &result);

    EXPECT_EQ(NIMCP_PATH_ERROR_THREAT_DETECTED, err);
    EXPECT_FALSE(result.valid);
}

//=============================================================================
// Normalization Integration Tests
//=============================================================================

TEST_F(PathTraversalIntegrationTest, NormalizationBeforeValidation) {
    char normalized[256];
    nimcp_path_validation_result_t result;

    /* Normalize then validate */
    const char* path = "dir//subdir/../file.txt";
    nimcp_path_error_t err = nimcp_path_normalize(path, normalized,
                                                   sizeof(normalized));
    EXPECT_EQ(NIMCP_PATH_SUCCESS, err);

    err = nimcp_path_validate(validator, normalized,
                              NIMCP_PATH_CONTEXT_AUTO, &result);
    EXPECT_EQ(NIMCP_PATH_SUCCESS, err);
    EXPECT_TRUE(result.valid);
}

TEST_F(PathTraversalIntegrationTest, URLDecodeBeforeValidation) {
    char decoded[256];
    nimcp_path_validation_result_t result;

    /* Decode then validate */
    const char* encoded = "%2e%2e%2fmalicious";
    nimcp_path_error_t err = nimcp_path_url_decode(encoded, decoded,
                                                    sizeof(decoded));
    EXPECT_EQ(NIMCP_PATH_SUCCESS, err);

    /* Decoded path should be detected as threat */
    err = nimcp_path_validate(validator, decoded,
                              NIMCP_PATH_CONTEXT_AUTO, &result);
    EXPECT_EQ(NIMCP_PATH_ERROR_THREAT_DETECTED, err);
}

//=============================================================================
// High-Volume Testing
//=============================================================================

TEST_F(PathTraversalIntegrationTest, HighVolumeValidation) {
    const int num_tests = 1000;
    int threats_detected = 0;
    int valid_paths = 0;

    for (int i = 0; i < num_tests; i++) {
        nimcp_path_validation_result_t result;

        /* Alternate between safe and dangerous paths */
        const char* path = (i % 2 == 0) ? "safe_file.txt" : "../malicious.txt";

        nimcp_path_error_t err = nimcp_path_validate(
            validator, path, NIMCP_PATH_CONTEXT_AUTO, &result);

        if (err == NIMCP_PATH_ERROR_THREAT_DETECTED) {
            threats_detected++;
        } else if (err == NIMCP_PATH_SUCCESS) {
            valid_paths++;
        }
    }

    /* Verify we processed all tests */
    EXPECT_EQ(num_tests / 2, threats_detected);
    EXPECT_EQ(num_tests / 2, valid_paths);

    /* Verify stats match */
    nimcp_path_validator_stats_t stats;
    nimcp_path_validator_get_stats(validator, &stats);
    EXPECT_GE(stats.total_validations, num_tests);
}

//=============================================================================
// Configuration Integration Tests
//=============================================================================

TEST_F(PathTraversalIntegrationTest, ConfigurationEffects) {
    /* Create validator with custom config */
    nimcp_path_validator_config_t cfg = nimcp_path_validator_default_config();
    cfg.enable_basic_detection = false;
    cfg.enable_url_encoding = true;
    cfg.max_path_length = 100;

    nimcp_path_validator_t custom_validator = nimcp_path_validator_create(&cfg);
    ASSERT_NE(nullptr, custom_validator);

    nimcp_path_validation_result_t result;

    /* Basic traversal should pass (disabled) */
    EXPECT_EQ(NIMCP_PATH_SUCCESS,
              nimcp_path_validate(custom_validator, "../file.txt",
                                 NIMCP_PATH_CONTEXT_AUTO, &result));

    /* URL encoded should still be detected */
    EXPECT_EQ(NIMCP_PATH_ERROR_THREAT_DETECTED,
              nimcp_path_validate(custom_validator, "%2e%2e%2ffile",
                                 NIMCP_PATH_CONTEXT_AUTO, &result));

    /* Long path should be rejected */
    std::string long_path(150, 'a');
    EXPECT_EQ(NIMCP_PATH_ERROR_THREAT_DETECTED,
              nimcp_path_validate(custom_validator, long_path.c_str(),
                                 NIMCP_PATH_CONTEXT_AUTO, &result));

    nimcp_path_validator_destroy(custom_validator);
}

//=============================================================================
// Error Handling Integration Tests
//=============================================================================

TEST_F(PathTraversalIntegrationTest, ErrorRecovery) {
    nimcp_path_validation_result_t result;

    /* Invalid parameters should not crash */
    EXPECT_NE(NIMCP_PATH_SUCCESS,
              nimcp_path_validate(nullptr, "test",
                                 NIMCP_PATH_CONTEXT_AUTO, &result));

    /* Validator should still work after error */
    EXPECT_EQ(NIMCP_PATH_SUCCESS,
              nimcp_path_validate(validator, "safe.txt",
                                 NIMCP_PATH_CONTEXT_AUTO, &result));
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
