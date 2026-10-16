// test_bbb_immune_pipeline_e2e.cpp - End-to-end BBB + immune pipeline test
// Tests full security pipeline: BBB detect -> immune respond -> quarantine
#include <gtest/gtest.h>
#include <cstring>
#include <vector>

#include "cognitive/immune/nimcp_brain_immune.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "security/nimcp_bbb_helpers.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/error/nimcp_error_codes.h"

class BBBImmunePipelineE2E : public ::testing::Test {
protected:
    brain_immune_system_t* immune_ = nullptr;
    bbb_system_t bbb_ = nullptr;

    void SetUp() override {
        nimcp_exception_system_init();
        bbb_helpers_init();
        bbb_config_t bbb_cfg = bbb_default_config();
        bbb_cfg.strict_mode = true;
        bbb_cfg.input.validate_strings = true;
        bbb_cfg.input.max_string_length = 4096;
        bbb_ = bbb_system_create(&bbb_cfg);
        ASSERT_NE(bbb_, nullptr);
        bbb_system_set_enabled(bbb_, true);
        brain_immune_config_t imm_cfg;
        brain_immune_default_config(&imm_cfg);
        imm_cfg.enable_bbb_integration = true;
        imm_cfg.enable_logging = false;
        immune_ = brain_immune_create(&imm_cfg);
        ASSERT_NE(immune_, nullptr);
        int rc = brain_immune_connect_bbb(immune_, bbb_);
        ASSERT_EQ(rc, 0) << "Failed to connect BBB to immune system";
    }

    void TearDown() override {
        if (immune_) brain_immune_destroy(immune_);
        if (bbb_) bbb_system_destroy(bbb_);
        bbb_helpers_shutdown();
        nimcp_exception_system_shutdown();
    }
};

TEST_F(BBBImmunePipelineE2E, ValidInput_PassesThrough) {
    const char* safe_data = "Hello, this is safe input";
    bbb_validation_result_t vr;
    memset(&vr, 0, sizeof(vr));
    EXPECT_TRUE(bbb_validate_input(bbb_, safe_data, strlen(safe_data), &vr));
    EXPECT_TRUE(vr.valid);
    EXPECT_EQ(vr.threat, BBB_THREAT_NONE);
    brain_immune_phase_t phase = brain_immune_get_phase(immune_);
    EXPECT_EQ(phase, IMMUNE_PHASE_SURVEILLANCE);
}

TEST_F(BBBImmunePipelineE2E, ThreatTriggersImmuneResponse) {
    uint8_t malicious[] = {0xFF, 0xFE, 0xDE, 0xAD, 0xBE, 0xEF};
    bbb_threat_report_t report = bbb_report_threat(
        bbb_, BBB_THREAT_BUFFER_OVERFLOW, BBB_SEVERITY_CRITICAL,
        "Buffer overflow detected", nullptr, malicious, sizeof(malicious));
    EXPECT_NE(report.timestamp, 0u);  // Use timestamp as unique identifier
    uint32_t antigen_id = 0;
    int rc = brain_immune_present_bbb_threat(
        immune_, BBB_THREAT_BUFFER_OVERFLOW, BBB_SEVERITY_CRITICAL,
        malicious, sizeof(malicious), &antigen_id);
    EXPECT_EQ(rc, 0);
    EXPECT_GT(antigen_id, 0u);
    const brain_antigen_t* ag = brain_immune_get_antigen(immune_, antigen_id);
    EXPECT_NE(ag, nullptr);
    uint32_t b_cell_id = 0;
    rc = brain_immune_activate_b_cell(immune_, antigen_id, &b_cell_id);
    EXPECT_EQ(rc, 0);
    for (int i = 0; i < 50; i++) {
        brain_immune_update(immune_, 100);
    }
    brain_immune_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    brain_immune_get_stats(immune_, &stats);
    EXPECT_GT(stats.antigens_processed, 0u);
}

TEST_F(BBBImmunePipelineE2E, OversizedString_BBBRejects) {
    // Default max_string_length is 64KB (65536), so use a larger string
    std::string huge(70000, 'A');
    bbb_validation_result_t vr;
    memset(&vr, 0, sizeof(vr));
    EXPECT_FALSE(bbb_validate_string(bbb_, huge.c_str(), &vr));
    EXPECT_FALSE(vr.valid);
}

TEST_F(BBBImmunePipelineE2E, ExceptionRoutedToImmune) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_BBB_REJECTED, EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__, "BBB rejected malicious input");
    ASSERT_NE(ex, nullptr);
    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));
    int rc = nimcp_exception_present_to_immune(ex, &response);
    EXPECT_EQ(rc, 0);
    nimcp_exception_unref(ex);
}

TEST_F(BBBImmunePipelineE2E, QuarantineAfterThreat) {
    char suspicious_region[128];
    memset(suspicious_region, 0xDE, sizeof(suspicious_region));
    bool q = bbb_quarantine_region(bbb_, suspicious_region, sizeof(suspicious_region));
    if (q) {
        EXPECT_TRUE(bbb_is_quarantined(bbb_, suspicious_region, sizeof(suspicious_region)));
        bbb_validation_result_t vr;
        memset(&vr, 0, sizeof(vr));
        bbb_validate_pointer(bbb_, suspicious_region, sizeof(suspicious_region), &vr);
        bbb_release_quarantine(bbb_, suspicious_region);
        EXPECT_FALSE(bbb_is_quarantined(bbb_, suspicious_region, sizeof(suspicious_region)));
    }
}

TEST_F(BBBImmunePipelineE2E, MultipleThreatsCumulative) {
    for (int i = 0; i < 5; i++) {
        uint8_t data[] = {(uint8_t)i};
        uint32_t aid = 0;
        brain_immune_present_bbb_threat(
            immune_, BBB_THREAT_CODE_INJECTION, BBB_SEVERITY_HIGH,
            data, 1, &aid);
    }
    for (int i = 0; i < 20; i++) {
        brain_immune_update(immune_, 100);
    }
    brain_immune_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    brain_immune_get_stats(immune_, &stats);
    EXPECT_GE(stats.antigens_processed, 5u);
}

TEST_F(BBBImmunePipelineE2E, HelpersIntegrateWithBBB) {
    EXPECT_TRUE(bbb_register_module("e2e_test", BBB_MODULE_TYPE_CORE));
    int val = 42;
    EXPECT_TRUE(bbb_check_pointer(&val, "e2e_test"));
    EXPECT_TRUE(bbb_check_string("safe input", 4096, "e2e_test"));
    EXPECT_TRUE(bbb_validate_range(50, 0, 100, "e2e_test"));
    char buf[256];
    EXPECT_TRUE(bbb_validate_buffer_access(buf, 0, 128, 256, "e2e_test"));
    bbb_statistics_t bbb_stats;
    memset(&bbb_stats, 0, sizeof(bbb_stats));
    bbb_system_get_statistics(bbb_, &bbb_stats);
}

TEST_F(BBBImmunePipelineE2E, ConnectBBB_NullImmune) {
    int rc = brain_immune_connect_bbb(nullptr, bbb_);
    EXPECT_NE(rc, 0);
}

TEST_F(BBBImmunePipelineE2E, ConnectBBB_NullBBB) {
    int rc = brain_immune_connect_bbb(immune_, nullptr);
    EXPECT_NE(rc, 0);
}
