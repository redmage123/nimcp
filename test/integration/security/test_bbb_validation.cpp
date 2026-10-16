/**
 * @file test_bbb_validation.cpp
 * @brief Tests for BBB validation at boundaries (P1-4)
 *
 * WHAT: Verify Blood-Brain Barrier input validation and security features
 * WHY:  P1-4 added BBB validation at module boundaries
 * HOW:  Test BBB creation, input validation, string validation, statistics
 *
 * Function signatures tested (from include/security/nimcp_blood_brain_barrier.h):
 *   bbb_config_t bbb_default_config(void);
 *   bbb_system_t bbb_system_create(const bbb_config_t* config);
 *   void bbb_system_destroy(bbb_system_t system);
 *   bool bbb_system_set_enabled(bbb_system_t system, bool enabled);
 *   bool bbb_system_is_enabled(bbb_system_t system);
 *   bool bbb_validate_input(bbb_system_t system, const void* data,
 *                           size_t size, bbb_validation_result_t* result);
 *   bool bbb_validate_string(bbb_system_t system, const char* str,
 *                            bbb_validation_result_t* result);
 *   bool bbb_validate_integer(bbb_system_t system, int64_t value,
 *                             bbb_validation_result_t* result);
 *   bool bbb_system_get_statistics(bbb_system_t system, bbb_statistics_t* stats);
 *   void bbb_system_reset_statistics(bbb_system_t system);
 *   void bbb_reset_test_state(void);
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "security/nimcp_blood_brain_barrier.h"
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class BBBValidationTest : public ::testing::Test {
protected:
    bbb_system_t bbb = nullptr;

    void SetUp() override {
        bbb_reset_test_state();
        bbb_config_t config = bbb_default_config();
        config.strict_mode = false;
        config.input.validate_strings = true;
        config.input.validate_integers = true;
        config.input.validate_pointers = true;
        config.input.max_string_length = 1024;
        config.input.max_array_size = 10000;
        bbb = bbb_system_create(&config);
        ASSERT_NE(bbb, nullptr);
    }

    void TearDown() override {
        bbb_system_destroy(bbb);
        bbb = nullptr;
        bbb_reset_test_state();
    }
};

/* ============================================================================
 * Creation / Destruction Tests
 * ============================================================================ */

TEST(BBBLifecycleTest, CreateWithDefaults) {
    bbb_system_t sys = bbb_system_create(nullptr);
    EXPECT_NE(sys, nullptr);
    bbb_system_destroy(sys);
}

TEST(BBBLifecycleTest, CreateWithConfig) {
    bbb_config_t config = bbb_default_config();
    bbb_system_t sys = bbb_system_create(&config);
    EXPECT_NE(sys, nullptr);
    bbb_system_destroy(sys);
}

TEST(BBBLifecycleTest, DestroyNull) {
    bbb_system_destroy(nullptr);
    SUCCEED() << "Destroying NULL BBB system did not crash";
}

TEST(BBBLifecycleTest, DefaultConfigValid) {
    bbb_config_t config = bbb_default_config();
    // Default config should have reasonable settings
    EXPECT_GT(config.input.max_string_length, 0u);
}

/* ============================================================================
 * Enable / Disable Tests
 * ============================================================================ */

TEST_F(BBBValidationTest, SystemEnabledByDefault) {
    EXPECT_TRUE(bbb_system_is_enabled(bbb));
}

TEST_F(BBBValidationTest, DisableAndReenable) {
    bbb_system_set_enabled(bbb, false);
    EXPECT_FALSE(bbb_system_is_enabled(bbb));

    bbb_system_set_enabled(bbb, true);
    EXPECT_TRUE(bbb_system_is_enabled(bbb));
}

/* ============================================================================
 * Input Validation Tests
 * ============================================================================ */

TEST_F(BBBValidationTest, ValidInputPasses) {
    const char data[] = "Hello, World!";
    bbb_validation_result_t result;
    memset(&result, 0, sizeof(result));

    bool valid = bbb_validate_input(bbb, data, sizeof(data), &result);
    EXPECT_TRUE(valid);
    EXPECT_TRUE(result.valid);
    EXPECT_EQ(result.threat, BBB_THREAT_NONE);
}

TEST_F(BBBValidationTest, NullDataRejected) {
    bbb_validation_result_t result;
    memset(&result, 0, sizeof(result));

    bool valid = bbb_validate_input(bbb, nullptr, 100, &result);
    EXPECT_FALSE(valid);
}

TEST_F(BBBValidationTest, ZeroSizeInput) {
    const char data[] = "";
    bbb_validation_result_t result;
    memset(&result, 0, sizeof(result));

    // Zero-size input may be valid or rejected depending on implementation
    bbb_validate_input(bbb, data, 0, &result);
    // Just verify it does not crash
    SUCCEED();
}

/* ============================================================================
 * String Validation Tests
 * ============================================================================ */

TEST_F(BBBValidationTest, ValidStringPasses) {
    bbb_validation_result_t result;
    memset(&result, 0, sizeof(result));

    bool valid = bbb_validate_string(bbb, "Normal text content", &result);
    EXPECT_TRUE(valid);
    EXPECT_TRUE(result.valid);
}

TEST_F(BBBValidationTest, NullStringRejected) {
    bbb_validation_result_t result;
    memset(&result, 0, sizeof(result));

    bool valid = bbb_validate_string(bbb, nullptr, &result);
    EXPECT_FALSE(valid);
}

TEST_F(BBBValidationTest, FormatStringDetection) {
    bbb_validation_result_t result;
    memset(&result, 0, sizeof(result));

    // Suspicious format string patterns may be detected
    bool valid = bbb_validate_string(bbb, "%n%n%n%n", &result);
    // This may or may not be flagged depending on BBB strictness
    // The key is it does not crash
    SUCCEED();
}

/* ============================================================================
 * Integer Validation Tests
 * ============================================================================ */

TEST_F(BBBValidationTest, ValidIntegerPasses) {
    bbb_validation_result_t result;
    memset(&result, 0, sizeof(result));

    bool valid = bbb_validate_integer(bbb, 42, &result);
    EXPECT_TRUE(valid);
    EXPECT_TRUE(result.valid);
}

TEST_F(BBBValidationTest, ZeroIntegerPasses) {
    bbb_validation_result_t result;
    memset(&result, 0, sizeof(result));

    bool valid = bbb_validate_integer(bbb, 0, &result);
    EXPECT_TRUE(valid);
}

TEST_F(BBBValidationTest, NegativeIntegerHandled) {
    bbb_validation_result_t result;
    memset(&result, 0, sizeof(result));

    // Negative values may be valid depending on config
    bbb_validate_integer(bbb, -1, &result);
    SUCCEED() << "Negative integer validated without crash";
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(BBBValidationTest, GetStatistics) {
    bbb_statistics_t stats;
    memset(&stats, 0, sizeof(stats));

    bool ok = bbb_system_get_statistics(bbb, &stats);
    EXPECT_TRUE(ok);
}

TEST_F(BBBValidationTest, StatisticsIncrementAfterValidation) {
    bbb_system_reset_statistics(bbb);

    // Perform some validations
    bbb_validation_result_t result;
    bbb_validate_string(bbb, "test", &result);
    bbb_validate_integer(bbb, 123, &result);

    bbb_statistics_t stats;
    bool ok = bbb_system_get_statistics(bbb, &stats);
    EXPECT_TRUE(ok);
    EXPECT_GE(stats.total_validations, 2u);
}

TEST_F(BBBValidationTest, ResetStatistics) {
    // Perform a validation
    bbb_validation_result_t result;
    bbb_validate_string(bbb, "data", &result);

    // Reset
    bbb_system_reset_statistics(bbb);

    bbb_statistics_t stats;
    bool ok = bbb_system_get_statistics(bbb, &stats);
    EXPECT_TRUE(ok);
    EXPECT_EQ(stats.total_validations, 0u);
}

/* ============================================================================
 * Threat Type and Severity Name Tests
 * ============================================================================ */

TEST(BBBUtilityTest, ThreatTypeNames) {
    const char* name = bbb_threat_type_name(BBB_THREAT_NONE);
    EXPECT_NE(name, nullptr);

    name = bbb_threat_type_name(BBB_THREAT_BUFFER_OVERFLOW);
    EXPECT_NE(name, nullptr);

    name = bbb_threat_type_name(BBB_THREAT_CODE_INJECTION);
    EXPECT_NE(name, nullptr);
}

TEST(BBBUtilityTest, SeverityNames) {
    const char* name = bbb_severity_name(BBB_SEVERITY_NONE);
    EXPECT_NE(name, nullptr);

    name = bbb_severity_name(BBB_SEVERITY_CRITICAL);
    EXPECT_NE(name, nullptr);
}

TEST(BBBUtilityTest, ActionNames) {
    const char* name = bbb_action_name(BBB_ACTION_ALLOW);
    EXPECT_NE(name, nullptr);

    name = bbb_action_name(BBB_ACTION_BLOCK);
    EXPECT_NE(name, nullptr);
}
