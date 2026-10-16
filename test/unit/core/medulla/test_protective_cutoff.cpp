/**
 * @file test_protective_cutoff.cpp
 * @brief Unit tests for the protective cutoff module
 *
 * WHAT: Tests for multi-tier emergency protective shutdown
 * WHY:  Ensure graceful degradation under threat conditions
 * HOW:  Use GoogleTest framework with threat/protection level validation
 */

#include <gtest/gtest.h>
#include <cstring>

// Headers have their own extern "C" guards
#include "core/medulla/nimcp_protective_cutoff.h"
#include "utils/error/nimcp_error_codes.h"

//=============================================================================
// Test Fixture
//=============================================================================

class ProtectiveCutoffTest : public ::testing::Test {
protected:
    protective_cutoff_t* cutoff = nullptr;

    void SetUp() override {
        protective_cutoff_config_t config;
        protective_cutoff_default_config(&config);
        cutoff = protective_cutoff_create(&config);
        ASSERT_NE(cutoff, nullptr);
    }

    void TearDown() override {
        if (cutoff) {
            protective_cutoff_destroy(cutoff);
            cutoff = nullptr;
        }
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(ProtectiveCutoffTest, DefaultConfig) {
    protective_cutoff_config_t config;
    protective_cutoff_default_config(&config);

    // Verify thresholds are ascending
    EXPECT_LT(config.thresholds.warn_threshold, config.thresholds.throttle_threshold);
    EXPECT_LT(config.thresholds.throttle_threshold, config.thresholds.shed_load_threshold);
}

TEST_F(ProtectiveCutoffTest, CreateWithNullConfig) {
    protective_cutoff_t* c = protective_cutoff_create(nullptr);
    EXPECT_NE(c, nullptr);
    if (c) protective_cutoff_destroy(c);
}

TEST_F(ProtectiveCutoffTest, DestroyNull) {
    protective_cutoff_destroy(nullptr);
}

//=============================================================================
// Threat Reporting Tests
//=============================================================================

TEST_F(ProtectiveCutoffTest, ReportThreat) {
    int result = protective_cutoff_report_threat(cutoff, PROTECTIVE_THREAT_TEMPERATURE, 0.3f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(ProtectiveCutoffTest, ReportThreatInvalidType) {
    int result = protective_cutoff_report_threat(cutoff, PROTECTIVE_THREAT_COUNT, 0.5f);
    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAMETER);
}

TEST_F(ProtectiveCutoffTest, ReportThreatNull) {
    int result = protective_cutoff_report_threat(nullptr, PROTECTIVE_THREAT_TEMPERATURE, 0.5f);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

//=============================================================================
// Threat Query Tests
//=============================================================================

TEST_F(ProtectiveCutoffTest, GetThreat) {
    // Report a threat
    protective_cutoff_report_threat(cutoff, PROTECTIVE_THREAT_TEMPERATURE, 0.5f);

    // Query it back
    float threat = protective_cutoff_get_threat(cutoff, PROTECTIVE_THREAT_TEMPERATURE);
    EXPECT_FLOAT_EQ(threat, 0.5f);
}

TEST_F(ProtectiveCutoffTest, GetThreatNull) {
    float threat = protective_cutoff_get_threat(nullptr, PROTECTIVE_THREAT_TEMPERATURE);
    EXPECT_FLOAT_EQ(threat, -1.0f);  // Returns -1.0 on error per API spec
}

//=============================================================================
// Operation Permission Tests
//=============================================================================

TEST_F(ProtectiveCutoffTest, CanExecuteNormal) {
    // At normal level, all operations should be allowed
    EXPECT_TRUE(protective_cutoff_can_execute(cutoff, OP_LEARNING));
    EXPECT_TRUE(protective_cutoff_can_execute(cutoff, OP_INFERENCE));
    EXPECT_TRUE(protective_cutoff_can_execute(cutoff, OP_NETWORK_TX));
}

TEST_F(ProtectiveCutoffTest, CanExecuteNull) {
    bool result = protective_cutoff_can_execute(nullptr, OP_LEARNING);
    EXPECT_FALSE(result);  // Returns false on error
}

//=============================================================================
// Force Level Tests
//=============================================================================

TEST_F(ProtectiveCutoffTest, ForceLevel) {
    int result = protective_cutoff_force_level(cutoff, PROTECTION_WARN);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(ProtectiveCutoffTest, ForceLevelNull) {
    int result = protective_cutoff_force_level(nullptr, PROTECTION_WARN);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

//=============================================================================
// Update and Recovery Tests
//=============================================================================

TEST_F(ProtectiveCutoffTest, Update) {
    int result = protective_cutoff_update(cutoff);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(ProtectiveCutoffTest, AttemptRecovery) {
    int result = protective_cutoff_attempt_recovery(cutoff);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

//=============================================================================
// String Conversion Tests
//=============================================================================

TEST_F(ProtectiveCutoffTest, LevelToString) {
    const char* str = protective_cutoff_level_to_string(PROTECTION_NORMAL);
    EXPECT_NE(str, nullptr);
    EXPECT_STRNE(str, "");

    str = protective_cutoff_level_to_string(PROTECTION_EMERGENCY_SHUTDOWN);
    EXPECT_NE(str, nullptr);
    EXPECT_STRNE(str, "");
}

TEST_F(ProtectiveCutoffTest, ThreatToString) {
    const char* str = protective_cutoff_threat_to_string(PROTECTIVE_THREAT_TEMPERATURE);
    EXPECT_NE(str, nullptr);
    EXPECT_STRNE(str, "");
}

TEST_F(ProtectiveCutoffTest, OperationToString) {
    const char* str = protective_cutoff_operation_to_string(OP_LEARNING);
    EXPECT_NE(str, nullptr);
    EXPECT_STRNE(str, "");
}

//=============================================================================
// Bio-async Tests
//=============================================================================

TEST_F(ProtectiveCutoffTest, BioAsyncConnection) {
    bool connected = protective_cutoff_is_bio_async_connected(cutoff);
    EXPECT_FALSE(connected);

    // Try to connect
    int result = protective_cutoff_connect_bio_async(cutoff);
    // Result depends on router availability

    // Disconnect
    protective_cutoff_disconnect_bio_async(cutoff);
    connected = protective_cutoff_is_bio_async_connected(cutoff);
    EXPECT_FALSE(connected);
}

TEST_F(ProtectiveCutoffTest, BioAsyncNullState) {
    bool connected = protective_cutoff_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
