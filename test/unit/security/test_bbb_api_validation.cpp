// test_bbb_api_validation.cpp - BBB validation at API boundaries
// Tests P1-4: BBB validation for all public API entry points
#include <gtest/gtest.h>
#include <cstring>
#include <vector>
#include <string>

#include "security/nimcp_blood_brain_barrier.h"
#include "security/nimcp_bbb_helpers.h"

class BBBApiValidationTest : public ::testing::Test {
protected:
    bbb_system_t system_ = nullptr;
    void SetUp() override {
        bbb_config_t c = bbb_default_config();
        c.strict_mode = true;
        c.input.validate_strings = true;
        c.input.max_string_length = 4096;
        system_ = bbb_system_create(&c);
        ASSERT_NE(system_, nullptr) << "Failed to create BBB system";
        bbb_system_set_enabled(system_, true);
    }
    void TearDown() override {
        if (system_) { bbb_system_destroy(system_); system_ = nullptr; }
    }
};

TEST_F(BBBApiValidationTest, ValidInput_Success) {
    const char* data = "hello world";
    bbb_validation_result_t r; memset(&r, 0, sizeof(r));
    EXPECT_TRUE(bbb_validate_input(system_, data, strlen(data), &r));
    EXPECT_TRUE(r.valid);
}

TEST_F(BBBApiValidationTest, NullData_Rejected) {
    bbb_validation_result_t r; memset(&r, 0, sizeof(r));
    EXPECT_FALSE(bbb_validate_input(system_, nullptr, 100, &r));
}

TEST_F(BBBApiValidationTest, NullSystem_Rejected) {
    bbb_validation_result_t r; memset(&r, 0, sizeof(r));
    EXPECT_FALSE(bbb_validate_input(nullptr, "data", 4, &r));
}

TEST_F(BBBApiValidationTest, NullResult_Rejected) {
    const char* data = "test";
    EXPECT_FALSE(bbb_validate_input(system_, data, strlen(data), nullptr));
}

TEST_F(BBBApiValidationTest, ZeroSizeInput_Handled) {
    bbb_validation_result_t r; memset(&r, 0, sizeof(r));
    bbb_validate_input(system_, "x", 0, &r);
}

TEST_F(BBBApiValidationTest, OversizedBuffer_Handled) {
    std::vector<char> big(8192, 65);
    bbb_validation_result_t r; memset(&r, 0, sizeof(r));
    bbb_validate_input(system_, big.data(), big.size(), &r);
}

TEST_F(BBBApiValidationTest, ValidString_OK) {
    bbb_validation_result_t r; memset(&r, 0, sizeof(r));
    EXPECT_TRUE(bbb_validate_string(system_, "safe string", &r));
    EXPECT_TRUE(r.valid);
}

TEST_F(BBBApiValidationTest, NullString_Rejected) {
    bbb_validation_result_t r; memset(&r, 0, sizeof(r));
    EXPECT_FALSE(bbb_validate_string(system_, nullptr, &r));
}

TEST_F(BBBApiValidationTest, EmptyString_Accepted) {
    bbb_validation_result_t r; memset(&r, 0, sizeof(r));
    EXPECT_TRUE(bbb_validate_string(system_, "", &r));
}

TEST_F(BBBApiValidationTest, OversizedString_Rejected) {
    std::string huge(8192, 88);
    bbb_validation_result_t r; memset(&r, 0, sizeof(r));
    EXPECT_FALSE(bbb_validate_string(system_, huge.c_str(), &r));
}

TEST_F(BBBApiValidationTest, StringNullSystem_Rejected) {
    bbb_validation_result_t r; memset(&r, 0, sizeof(r));
    EXPECT_FALSE(bbb_validate_string(nullptr, "test", &r));
}

TEST_F(BBBApiValidationTest, StringNullResult_Rejected) {
    EXPECT_FALSE(bbb_validate_string(system_, "test", nullptr));
}

TEST_F(BBBApiValidationTest, ValidInteger_OK) {
    bbb_validation_result_t r; memset(&r, 0, sizeof(r));
    EXPECT_TRUE(bbb_validate_integer(system_, 42, &r));
    EXPECT_TRUE(r.valid);
}

TEST_F(BBBApiValidationTest, IntegerNullSystem_Rejected) {
    bbb_validation_result_t r; memset(&r, 0, sizeof(r));
    EXPECT_FALSE(bbb_validate_integer(nullptr, 42, &r));
}

TEST_F(BBBApiValidationTest, IntegerNullResult_Rejected) {
    EXPECT_FALSE(bbb_validate_integer(system_, 42, nullptr));
}

TEST_F(BBBApiValidationTest, IntegerMinMax_NoOverflow) {
    bbb_validation_result_t r; memset(&r, 0, sizeof(r));
    bbb_validate_integer(system_, INT64_MIN, &r);
    memset(&r, 0, sizeof(r));
    bbb_validate_integer(system_, INT64_MAX, &r);
}

TEST_F(BBBApiValidationTest, ValidPointer_OK) {
    int value = 42;
    bbb_validation_result_t r; memset(&r, 0, sizeof(r));
    EXPECT_TRUE(bbb_validate_pointer(system_, &value, sizeof(int), &r));
    EXPECT_TRUE(r.valid);
}

TEST_F(BBBApiValidationTest, NullPointer_Rejected) {
    bbb_validation_result_t r; memset(&r, 0, sizeof(r));
    EXPECT_FALSE(bbb_validate_pointer(system_, nullptr, 4, &r));
}

TEST_F(BBBApiValidationTest, PointerNullSystem_Accepted) {
    // NULL system is acceptable - skip system-specific checks (Portia passes NULL)
    int value = 1;
    bbb_validation_result_t r; memset(&r, 0, sizeof(r));
    EXPECT_TRUE(bbb_validate_pointer(nullptr, &value, sizeof(int), &r));
}

TEST_F(BBBApiValidationTest, PointerNullResult_Accepted) {
    // NULL result is acceptable - uses local buffer internally
    int value = 1;
    EXPECT_TRUE(bbb_validate_pointer(system_, &value, sizeof(int), nullptr));
}

TEST_F(BBBApiValidationTest, ZeroSizePointer_Handled) {
    int value = 1;
    bbb_validation_result_t r; memset(&r, 0, sizeof(r));
    bbb_validate_pointer(system_, &value, 0, &r);
}

TEST(BBBSystemLifecycle, CreateWithDefaultConfig) {
    bbb_system_t s = bbb_system_create(nullptr);
    EXPECT_NE(s, nullptr);
    if (s) bbb_system_destroy(s);
}

TEST(BBBSystemLifecycle, CreateWithExplicitConfig) {
    bbb_config_t c = bbb_default_config();
    bbb_system_t s = bbb_system_create(&c);
    EXPECT_NE(s, nullptr);
    if (s) bbb_system_destroy(s);
}

TEST(BBBSystemLifecycle, DestroyNull_NoOp) {
    bbb_system_destroy(nullptr);
}

TEST(BBBSystemLifecycle, EnableDisable) {
    bbb_system_t s = bbb_system_create(nullptr);
    ASSERT_NE(s, nullptr);
    EXPECT_TRUE(bbb_system_set_enabled(s, true));
    EXPECT_TRUE(bbb_system_is_enabled(s));
    EXPECT_TRUE(bbb_system_set_enabled(s, false));
    EXPECT_FALSE(bbb_system_is_enabled(s));
    bbb_system_destroy(s);
}

TEST(BBBSystemLifecycle, EnableNull_Rejected) {
    EXPECT_FALSE(bbb_system_set_enabled(nullptr, true));
}

TEST(BBBSystemLifecycle, IsEnabledNull) {
    EXPECT_FALSE(bbb_system_is_enabled(nullptr));
}

TEST(BBBSystemLifecycle, GetStatistics) {
    bbb_system_t s = bbb_system_create(nullptr);
    ASSERT_NE(s, nullptr);
    bbb_statistics_t stats; memset(&stats, 0, sizeof(stats));
    EXPECT_TRUE(bbb_system_get_statistics(s, &stats));
    bbb_system_destroy(s);
}

TEST(BBBSystemLifecycle, GetStatisticsNullSystem) {
    bbb_statistics_t stats;
    EXPECT_FALSE(bbb_system_get_statistics(nullptr, &stats));
}

TEST(BBBSystemLifecycle, GetStatisticsNullStats) {
    bbb_system_t s = bbb_system_create(nullptr);
    ASSERT_NE(s, nullptr);
    EXPECT_FALSE(bbb_system_get_statistics(s, nullptr));
    bbb_system_destroy(s);
}

TEST_F(BBBApiValidationTest, QuarantineNullSystem) {
    char buf[16];
    EXPECT_FALSE(bbb_quarantine_region(nullptr, buf, sizeof(buf)));
}

TEST_F(BBBApiValidationTest, QuarantineNullAddress) {
    EXPECT_FALSE(bbb_quarantine_region(system_, nullptr, 16));
}

TEST_F(BBBApiValidationTest, IsQuarantinedNullSystem) {
    char buf[16];
    EXPECT_FALSE(bbb_is_quarantined(nullptr, buf, sizeof(buf)));
}

TEST_F(BBBApiValidationTest, QuarantineAndCheck) {
    char buf[64]; memset(buf, 0, sizeof(buf));
    bool q = bbb_quarantine_region(system_, buf, sizeof(buf));
    if (q) {
        EXPECT_TRUE(bbb_is_quarantined(system_, buf, sizeof(buf)));
        bbb_release_quarantine(system_, buf);
    }
}

TEST_F(BBBApiValidationTest, ReportThreat_Basic) {
    const char* desc = "test threat";
    uint8_t data[] = {0xDE, 0xAD};
    bbb_threat_report_t report = bbb_report_threat(
        system_, BBB_THREAT_BUFFER_OVERFLOW, BBB_SEVERITY_HIGH,
        desc, nullptr, data, sizeof(data));
    EXPECT_NE(report.timestamp, 0u);
}

TEST_F(BBBApiValidationTest, ReportThreat_NullDescription) {
    uint8_t data[] = {1};
    bbb_report_threat(system_, BBB_THREAT_CODE_INJECTION, BBB_SEVERITY_MEDIUM,
                     nullptr, nullptr, data, sizeof(data));
}

class BBBHelpersTest : public ::testing::Test {
protected:
    void SetUp() override { bbb_helpers_init(); }
    void TearDown() override { bbb_helpers_shutdown(); }
};

TEST_F(BBBHelpersTest, RegisterModule_OK) {
    EXPECT_TRUE(bbb_register_module("test_module", BBB_MODULE_TYPE_CORE));
}

TEST_F(BBBHelpersTest, RegisterModule_NullName) {
    EXPECT_FALSE(bbb_register_module(nullptr, BBB_MODULE_TYPE_CORE));
}

TEST_F(BBBHelpersTest, CheckPointer_Valid) {
    int v = 1;
    EXPECT_TRUE(bbb_check_pointer(&v, "test_func"));
}

TEST_F(BBBHelpersTest, CheckPointer_Null) {
    EXPECT_FALSE(bbb_check_pointer(nullptr, "test_func"));
}

TEST_F(BBBHelpersTest, CheckString_Valid) {
    EXPECT_TRUE(bbb_check_string("hello", 256, "test_func"));
}

TEST_F(BBBHelpersTest, CheckString_Null) {
    EXPECT_FALSE(bbb_check_string(nullptr, 256, "test_func"));
}

TEST_F(BBBHelpersTest, CheckString_ExceedsMax) {
    std::string long_str(512, 65);
    EXPECT_FALSE(bbb_check_string(long_str.c_str(), 256, "test_func"));
}

TEST_F(BBBHelpersTest, ValidateRange_OK) {
    EXPECT_TRUE(bbb_validate_range(50, 0, 100, "test_func"));
}

TEST_F(BBBHelpersTest, ValidateRange_AtBoundaries) {
    EXPECT_TRUE(bbb_validate_range(0, 0, 100, "test_func"));
    EXPECT_TRUE(bbb_validate_range(100, 0, 100, "test_func"));
}

TEST_F(BBBHelpersTest, ValidateRange_OutOfRange) {
    EXPECT_FALSE(bbb_validate_range(200, 0, 100, "test_func"));
    EXPECT_FALSE(bbb_validate_range(-1, 0, 100, "test_func"));
}

TEST_F(BBBHelpersTest, ValidateBufferAccess_OK) {
    char buf[256];
    EXPECT_TRUE(bbb_validate_buffer_access(buf, 0, 10, 256, "test_func"));
}

TEST_F(BBBHelpersTest, ValidateBufferAccess_NullBuffer) {
    EXPECT_FALSE(bbb_validate_buffer_access(nullptr, 0, 10, 256, "test_func"));
}

TEST_F(BBBHelpersTest, ValidateBufferAccess_OutOfBounds) {
    char buf[64];
    EXPECT_FALSE(bbb_validate_buffer_access(buf, 60, 10, 64, "test_func"));
}

TEST_F(BBBHelpersTest, ValidateBufferAccess_OverflowOffset) {
    char buf[64];
    EXPECT_FALSE(bbb_validate_buffer_access(buf, SIZE_MAX, 1, 64, "test_func"));
}
