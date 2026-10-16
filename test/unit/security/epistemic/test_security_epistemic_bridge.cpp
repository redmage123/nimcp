/**
 * @file test_security_epistemic_bridge.cpp
 * @brief Unit tests for Security-Epistemic Bridge
 * @version 1.0.0
 * @date 2026-01-10
 *
 * Comprehensive tests for the security-epistemic bridge including:
 * - Lifecycle (default config, create, destroy, reset)
 * - Connection tests for epistemic filter and BBB
 * - Confidence validation (bounds, rate, anomaly)
 * - Belief verification (register, update, lock, revoke)
 * - Uncertainty enforcement
 * - Evidence chain validation
 * - Attack detection
 * - Bidirectional updates
 * - Audit logging
 * - Statistics tracking
 */

#include <gtest/gtest.h>
#include <vector>
#include <cstring>
#include <cmath>
#include <chrono>

extern "C" {
#include "security/epistemic/nimcp_security_epistemic_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_codes.h"
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class SecurityEpistemicBridgeTest : public ::testing::Test {
protected:
    security_epist_bridge_t* bridge = nullptr;
    security_epist_config_t config;

    void SetUp() override {
        int result = security_epist_default_config(&config);
        ASSERT_EQ(result, 0);
        bridge = security_epist_bridge_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            security_epist_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }

    // Helper: Create mock epistemic filter
    epistemic_filter_t create_mock_epistemic_filter() {
        return reinterpret_cast<epistemic_filter_t>(0x20000001);
    }

    // Helper: Create mock BBB
    bbb_system_t create_mock_bbb() {
        return reinterpret_cast<bbb_system_t>(0x20000002);
    }

    // Helper: Create test evidence chain
    security_epist_evidence_chain_t create_test_evidence_chain(
        uint32_t link_count,
        float reliability = 0.8f
    ) {
        security_epist_evidence_chain_t chain = {};
        chain.chain_id = 12345;
        chain.link_count = link_count;
        chain.has_primary_source = true;
        chain.independent_paths = 1;
        chain.overall_reliability = reliability;

        // Use current timestamp to avoid expiration
        auto now = std::chrono::steady_clock::now();
        uint64_t now_us = std::chrono::duration_cast<std::chrono::microseconds>(
            now.time_since_epoch()).count();
        for (uint32_t i = 0; i < link_count && i < SEC_EPIST_MAX_EVIDENCE_CHAIN; i++) {
            chain.links[i].evidence_id = 1000 + i;
            chain.links[i].source_id = 2000 + i;
            chain.links[i].source_reliability = reliability;
            chain.links[i].timestamp = now_us;
            chain.links[i].hash = 0xDEADBEEF + i;
            chain.links[i].dependent_count = 0;
            chain.links[i].is_primary = (i == 0);
        }

        return chain;
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(SecurityEpistemicBridgeTest, DefaultConfigIsValid) {
    security_epist_config_t cfg;
    int result = security_epist_default_config(&cfg);
    EXPECT_EQ(result, 0);

    // Verify default values
    EXPECT_TRUE(cfg.enable_confidence_validation);
    EXPECT_TRUE(cfg.enable_belief_verification);
    EXPECT_TRUE(cfg.enable_uncertainty_enforcement);
    EXPECT_TRUE(cfg.enable_evidence_validation);
    EXPECT_TRUE(cfg.enable_attack_detection);
    EXPECT_TRUE(cfg.enable_audit);
    EXPECT_TRUE(cfg.enable_auto_correction);

    EXPECT_FLOAT_EQ(cfg.min_confidence, SEC_EPIST_DEFAULT_MIN_CONFIDENCE);
    EXPECT_FLOAT_EQ(cfg.max_confidence, SEC_EPIST_DEFAULT_MAX_CONFIDENCE);
    EXPECT_FLOAT_EQ(cfg.min_uncertainty, SEC_EPIST_DEFAULT_UNCERTAINTY_MIN);
    EXPECT_FLOAT_EQ(cfg.security_sensitivity, 1.0f);
    EXPECT_FLOAT_EQ(cfg.epistemic_sensitivity, 1.0f);
}

TEST_F(SecurityEpistemicBridgeTest, DefaultConfigNullFails) {
    int result = security_epist_default_config(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityEpistemicBridgeTest, CreateWithNullConfigUsesDefaults) {
    security_epist_bridge_t* br = security_epist_bridge_create(nullptr);
    ASSERT_NE(br, nullptr);

    security_epist_state_info_t state;
    int result = security_epist_get_state(br, &state);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(state.state, SEC_EPIST_STATE_IDLE);

    security_epist_bridge_destroy(br);
}

TEST_F(SecurityEpistemicBridgeTest, CreateWithCustomConfig) {
    security_epist_config_t custom_cfg;
    security_epist_default_config(&custom_cfg);

    custom_cfg.min_confidence = 0.2f;
    custom_cfg.max_confidence = 0.9f;
    custom_cfg.min_uncertainty = 0.05f;
    custom_cfg.security_sensitivity = 1.5f;

    security_epist_bridge_t* br = security_epist_bridge_create(&custom_cfg);
    ASSERT_NE(br, nullptr);

    EXPECT_FLOAT_EQ(br->config.min_confidence, 0.2f);
    EXPECT_FLOAT_EQ(br->config.max_confidence, 0.9f);
    EXPECT_FLOAT_EQ(br->config.min_uncertainty, 0.05f);
    EXPECT_FLOAT_EQ(br->config.security_sensitivity, 1.5f);

    security_epist_bridge_destroy(br);
}

TEST_F(SecurityEpistemicBridgeTest, DestroyNullIsSafe) {
    security_epist_bridge_destroy(nullptr);
    // Should not crash
}

TEST_F(SecurityEpistemicBridgeTest, ResetClearsState) {
    // Register some beliefs
    security_epist_register_belief(bridge, 100, 0xABCD, 0.7f);
    security_epist_register_belief(bridge, 101, 0xEF01, 0.8f);

    // Validate some confidence values to create stats
    security_epist_conf_status_t status;
    security_epist_validate_confidence(bridge, 0.5f, 0, &status);
    security_epist_validate_confidence(bridge, 0.6f, 0, &status);

    // Reset
    int result = security_epist_bridge_reset(bridge);
    EXPECT_EQ(result, 0);

    // State should be cleared
    security_epist_state_info_t state;
    security_epist_get_state(bridge, &state);
    EXPECT_EQ(state.state, SEC_EPIST_STATE_IDLE);

    // Stats should be reset
    security_epist_stats_t stats;
    security_epist_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_confidence_checks, 0u);
    EXPECT_EQ(stats.total_belief_checks, 0u);

    // Beliefs should be cleared
    EXPECT_EQ(bridge->num_beliefs, 0u);
}

TEST_F(SecurityEpistemicBridgeTest, ResetNullFails) {
    int result = security_epist_bridge_reset(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Connection Tests
 * ============================================================================ */

TEST_F(SecurityEpistemicBridgeTest, ConnectEpistemicFilter) {
    epistemic_filter_t filter = create_mock_epistemic_filter();

    int result = security_epist_connect_filter(bridge, filter);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(bridge->epistemic_connected);
    EXPECT_EQ(bridge->epistemic_filter, filter);
}

TEST_F(SecurityEpistemicBridgeTest, ConnectEpistemicFilterNullBridgeFails) {
    epistemic_filter_t filter = create_mock_epistemic_filter();
    int result = security_epist_connect_filter(nullptr, filter);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityEpistemicBridgeTest, ConnectEpistemicFilterNullFilterFails) {
    int result = security_epist_connect_filter(bridge, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityEpistemicBridgeTest, ConnectBBB) {
    bbb_system_t bbb = create_mock_bbb();

    int result = security_epist_connect_bbb(bridge, bbb);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(bridge->bbb_connected);
    EXPECT_EQ(bridge->bbb, bbb);
}

TEST_F(SecurityEpistemicBridgeTest, ConnectBBBNullBridgeFails) {
    bbb_system_t bbb = create_mock_bbb();
    int result = security_epist_connect_bbb(nullptr, bbb);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityEpistemicBridgeTest, ConnectBBBNullBBBFails) {
    int result = security_epist_connect_bbb(bridge, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityEpistemicBridgeTest, IsConnectedReturnsCorrectly) {
    EXPECT_FALSE(security_epist_is_connected(bridge));

    security_epist_connect_filter(bridge, create_mock_epistemic_filter());
    EXPECT_TRUE(security_epist_is_connected(bridge));
}

TEST_F(SecurityEpistemicBridgeTest, IsConnectedNullFails) {
    EXPECT_FALSE(security_epist_is_connected(nullptr));
}

TEST_F(SecurityEpistemicBridgeTest, DisconnectAll) {
    security_epist_connect_filter(bridge, create_mock_epistemic_filter());
    security_epist_connect_bbb(bridge, create_mock_bbb());

    EXPECT_TRUE(bridge->epistemic_connected);
    EXPECT_TRUE(bridge->bbb_connected);

    int result = security_epist_disconnect_all(bridge);
    EXPECT_EQ(result, 0);

    EXPECT_FALSE(bridge->epistemic_connected);
    EXPECT_FALSE(bridge->bbb_connected);
    EXPECT_EQ(bridge->epistemic_filter, nullptr);
    EXPECT_EQ(bridge->bbb, nullptr);
}

TEST_F(SecurityEpistemicBridgeTest, DisconnectAllNullFails) {
    int result = security_epist_disconnect_all(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Confidence Validation Tests
 * ============================================================================ */

TEST_F(SecurityEpistemicBridgeTest, ValidateConfidenceValid) {
    security_epist_conf_status_t status;
    bool valid = security_epist_validate_confidence(bridge, 0.5f, 0, &status);
    EXPECT_TRUE(valid);
    EXPECT_EQ(status, SEC_EPIST_CONF_VALID);

    security_epist_stats_t stats;
    security_epist_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_confidence_checks, 1u);
    EXPECT_EQ(stats.confidence_valid, 1u);
}

TEST_F(SecurityEpistemicBridgeTest, ValidateConfidenceTooLow) {
    security_epist_conf_status_t status;
    bool valid = security_epist_validate_confidence(bridge, 0.05f, 0, &status);
    EXPECT_FALSE(valid);
    EXPECT_EQ(status, SEC_EPIST_CONF_TOO_LOW);

    security_epist_stats_t stats;
    security_epist_get_stats(bridge, &stats);
    EXPECT_EQ(stats.confidence_rejected, 1u);
}

TEST_F(SecurityEpistemicBridgeTest, ValidateConfidenceTooHigh) {
    security_epist_conf_status_t status;
    bool valid = security_epist_validate_confidence(bridge, 0.995f, 0, &status);
    EXPECT_FALSE(valid);
    EXPECT_EQ(status, SEC_EPIST_CONF_TOO_HIGH);
}

TEST_F(SecurityEpistemicBridgeTest, ValidateConfidenceAtMinBound) {
    security_epist_conf_status_t status;
    bool valid = security_epist_validate_confidence(
        bridge, SEC_EPIST_DEFAULT_MIN_CONFIDENCE, 0, &status);
    EXPECT_TRUE(valid);
    EXPECT_EQ(status, SEC_EPIST_CONF_VALID);
}

TEST_F(SecurityEpistemicBridgeTest, ValidateConfidenceAtMaxBound) {
    security_epist_conf_status_t status;
    bool valid = security_epist_validate_confidence(
        bridge, SEC_EPIST_DEFAULT_MAX_CONFIDENCE, 0, &status);
    EXPECT_TRUE(valid);
    EXPECT_EQ(status, SEC_EPIST_CONF_VALID);
}

TEST_F(SecurityEpistemicBridgeTest, ValidateConfidenceMultipleValues) {
    security_epist_conf_status_t status;

    float test_values[] = {0.2f, 0.4f, 0.6f, 0.8f};
    for (float val : test_values) {
        bool valid = security_epist_validate_confidence(bridge, val, 0, &status);
        EXPECT_TRUE(valid);
        EXPECT_EQ(status, SEC_EPIST_CONF_VALID);
    }

    security_epist_stats_t stats;
    security_epist_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_confidence_checks, 4u);
    EXPECT_EQ(stats.confidence_valid, 4u);
}

TEST_F(SecurityEpistemicBridgeTest, ValidateConfidenceNullBridgeFails) {
    security_epist_conf_status_t status;
    bool valid = security_epist_validate_confidence(nullptr, 0.5f, 0, &status);
    EXPECT_FALSE(valid);
}

TEST_F(SecurityEpistemicBridgeTest, ValidateConfidenceNullStatusSafe) {
    bool valid = security_epist_validate_confidence(bridge, 0.5f, 0, nullptr);
    EXPECT_TRUE(valid);
}

TEST_F(SecurityEpistemicBridgeTest, CorrectConfidenceLow) {
    float corrected;
    int result = security_epist_correct_confidence(bridge, 0.05f, &corrected);
    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(corrected, SEC_EPIST_DEFAULT_MIN_CONFIDENCE);

    security_epist_stats_t stats;
    security_epist_get_stats(bridge, &stats);
    EXPECT_EQ(stats.confidence_corrected, 1u);
}

TEST_F(SecurityEpistemicBridgeTest, CorrectConfidenceHigh) {
    float corrected;
    int result = security_epist_correct_confidence(bridge, 0.999f, &corrected);
    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(corrected, SEC_EPIST_DEFAULT_MAX_CONFIDENCE);
}

TEST_F(SecurityEpistemicBridgeTest, CorrectConfidenceNoChangeNeeded) {
    float corrected;
    int result = security_epist_correct_confidence(bridge, 0.5f, &corrected);
    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(corrected, 0.5f);

    security_epist_stats_t stats;
    security_epist_get_stats(bridge, &stats);
    EXPECT_EQ(stats.confidence_corrected, 0u);
}

TEST_F(SecurityEpistemicBridgeTest, CorrectConfidenceNullBridgeFails) {
    float corrected;
    int result = security_epist_correct_confidence(nullptr, 0.5f, &corrected);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityEpistemicBridgeTest, CorrectConfidenceNullOutputFails) {
    int result = security_epist_correct_confidence(bridge, 0.5f, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityEpistemicBridgeTest, SetConfidenceBounds) {
    int result = security_epist_set_confidence_bounds(bridge, 0.2f, 0.8f);
    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(bridge->config.min_confidence, 0.2f);
    EXPECT_FLOAT_EQ(bridge->config.max_confidence, 0.8f);

    // Test with new bounds
    security_epist_conf_status_t status;
    bool valid = security_epist_validate_confidence(bridge, 0.15f, 0, &status);
    EXPECT_FALSE(valid);
    EXPECT_EQ(status, SEC_EPIST_CONF_TOO_LOW);

    valid = security_epist_validate_confidence(bridge, 0.85f, 0, &status);
    EXPECT_FALSE(valid);
    EXPECT_EQ(status, SEC_EPIST_CONF_TOO_HIGH);
}

TEST_F(SecurityEpistemicBridgeTest, SetConfidenceBoundsInvalidRange) {
    int result = security_epist_set_confidence_bounds(bridge, 0.8f, 0.2f);
    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAMETER);
}

TEST_F(SecurityEpistemicBridgeTest, SetConfidenceBoundsNullFails) {
    int result = security_epist_set_confidence_bounds(nullptr, 0.2f, 0.8f);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Belief Verification Tests
 * ============================================================================ */

TEST_F(SecurityEpistemicBridgeTest, RegisterBelief) {
    int result = security_epist_register_belief(bridge, 1001, 0xDEADBEEF, 0.75f);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(bridge->num_beliefs, 1u);
}

TEST_F(SecurityEpistemicBridgeTest, RegisterMultipleBeliefs) {
    for (uint64_t i = 0; i < 10; i++) {
        int result = security_epist_register_belief(bridge, 1000 + i, 0xABCD + i, 0.5f + i * 0.01f);
        EXPECT_EQ(result, 0);
    }
    EXPECT_EQ(bridge->num_beliefs, 10u);
}

TEST_F(SecurityEpistemicBridgeTest, RegisterBeliefNullFails) {
    int result = security_epist_register_belief(nullptr, 100, 0x1234, 0.5f);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityEpistemicBridgeTest, RegisterBeliefUpdateExisting) {
    security_epist_register_belief(bridge, 1001, 0xAAAA, 0.5f);
    EXPECT_EQ(bridge->num_beliefs, 1u);

    // Register same ID again - should update, not add
    security_epist_register_belief(bridge, 1001, 0xBBBB, 0.7f);
    EXPECT_EQ(bridge->num_beliefs, 1u);
}

TEST_F(SecurityEpistemicBridgeTest, VerifyBeliefValid) {
    security_epist_register_belief(bridge, 2001, 0x12345678, 0.6f);

    security_epist_belief_status_t status;
    bool valid = security_epist_verify_belief(bridge, 2001, 0x12345678, &status);
    EXPECT_TRUE(valid);
    EXPECT_EQ(status, SEC_EPIST_BELIEF_VALID);

    security_epist_stats_t stats;
    security_epist_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_belief_checks, 1u);
    EXPECT_EQ(stats.beliefs_verified, 1u);
}

TEST_F(SecurityEpistemicBridgeTest, VerifyBeliefCorrupted) {
    security_epist_register_belief(bridge, 2002, 0x12345678, 0.6f);

    security_epist_belief_status_t status;
    bool valid = security_epist_verify_belief(bridge, 2002, 0xDEADBEEF, &status);
    EXPECT_FALSE(valid);
    EXPECT_EQ(status, SEC_EPIST_BELIEF_CORRUPTED);

    security_epist_stats_t stats;
    security_epist_get_stats(bridge, &stats);
    EXPECT_EQ(stats.beliefs_corrupted, 1u);
}

TEST_F(SecurityEpistemicBridgeTest, VerifyBeliefUnknown) {
    security_epist_belief_status_t status;
    bool valid = security_epist_verify_belief(bridge, 99999, 0x12345678, &status);
    // Unknown beliefs are considered valid (no corruption detected)
    EXPECT_TRUE(valid);
}

TEST_F(SecurityEpistemicBridgeTest, VerifyBeliefNullBridgeFails) {
    security_epist_belief_status_t status;
    bool valid = security_epist_verify_belief(nullptr, 100, 0x1234, &status);
    EXPECT_FALSE(valid);
}

TEST_F(SecurityEpistemicBridgeTest, UpdateBelief) {
    security_epist_register_belief(bridge, 3001, 0xAAAA, 0.5f);

    int result = security_epist_update_belief(bridge, 3001, 0.8f, 0xBBBB);
    EXPECT_EQ(result, 0);
}

TEST_F(SecurityEpistemicBridgeTest, UpdateBeliefNotFound) {
    int result = security_epist_update_belief(bridge, 99999, 0.8f, 0xAAAA);
    EXPECT_EQ(result, NIMCP_ERROR_NOT_FOUND);
}

TEST_F(SecurityEpistemicBridgeTest, UpdateBeliefNullFails) {
    int result = security_epist_update_belief(nullptr, 100, 0.5f, 0x1234);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityEpistemicBridgeTest, LockBelief) {
    security_epist_register_belief(bridge, 4001, 0xAAAA, 0.5f);

    int result = security_epist_lock_belief(bridge, 4001);
    EXPECT_EQ(result, 0);

    // Try to update locked belief
    result = security_epist_update_belief(bridge, 4001, 0.9f, 0xBBBB);
    EXPECT_EQ(result, NIMCP_ERROR_PERMISSION_DENIED);
}

TEST_F(SecurityEpistemicBridgeTest, LockBeliefNotFound) {
    int result = security_epist_lock_belief(bridge, 99999);
    EXPECT_EQ(result, NIMCP_ERROR_NOT_FOUND);
}

TEST_F(SecurityEpistemicBridgeTest, LockBeliefNullFails) {
    int result = security_epist_lock_belief(nullptr, 100);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityEpistemicBridgeTest, RevokeBelief) {
    security_epist_register_belief(bridge, 5001, 0xAAAA, 0.5f);
    security_epist_register_belief(bridge, 5002, 0xBBBB, 0.6f);
    EXPECT_EQ(bridge->num_beliefs, 2u);

    int result = security_epist_revoke_belief(bridge, 5001);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(bridge->num_beliefs, 1u);

    // Verify second belief still exists
    security_epist_belief_status_t status;
    bool valid = security_epist_verify_belief(bridge, 5002, 0xBBBB, &status);
    EXPECT_TRUE(valid);
}

TEST_F(SecurityEpistemicBridgeTest, RevokeBeliefNotFound) {
    int result = security_epist_revoke_belief(bridge, 99999);
    EXPECT_EQ(result, NIMCP_ERROR_NOT_FOUND);
}

TEST_F(SecurityEpistemicBridgeTest, RevokeBeliefNullFails) {
    int result = security_epist_revoke_belief(nullptr, 100);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Uncertainty Enforcement Tests
 * ============================================================================ */

TEST_F(SecurityEpistemicBridgeTest, EnforceUncertaintyValid) {
    bool is_valid;
    int result = security_epist_enforce_uncertainty(bridge, 0.5f, &is_valid);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(is_valid);
}

TEST_F(SecurityEpistemicBridgeTest, EnforceUncertaintyTooLow) {
    bool is_valid;
    int result = security_epist_enforce_uncertainty(bridge, 0.001f, &is_valid);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(is_valid);

    security_epist_stats_t stats;
    security_epist_get_stats(bridge, &stats);
    EXPECT_GE(stats.uncertainty_violations, 1u);
}

TEST_F(SecurityEpistemicBridgeTest, EnforceUncertaintyNullBridgeFails) {
    bool is_valid;
    int result = security_epist_enforce_uncertainty(nullptr, 0.5f, &is_valid);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityEpistemicBridgeTest, EnforceUncertaintyNullOutputFails) {
    int result = security_epist_enforce_uncertainty(bridge, 0.5f, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityEpistemicBridgeTest, AdjustUncertaintyLow) {
    float adjusted;
    int result = security_epist_adjust_uncertainty(bridge, 0.001f, &adjusted);
    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(adjusted, SEC_EPIST_DEFAULT_UNCERTAINTY_MIN);
}

TEST_F(SecurityEpistemicBridgeTest, AdjustUncertaintyHigh) {
    float adjusted;
    int result = security_epist_adjust_uncertainty(bridge, 1.5f, &adjusted);
    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(adjusted, 1.0f);
}

TEST_F(SecurityEpistemicBridgeTest, AdjustUncertaintyNoChange) {
    float adjusted;
    int result = security_epist_adjust_uncertainty(bridge, 0.5f, &adjusted);
    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(adjusted, 0.5f);
}

TEST_F(SecurityEpistemicBridgeTest, SetUncertaintyBounds) {
    int result = security_epist_set_uncertainty_bounds(bridge, 0.05f, 0.9f);
    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(bridge->config.min_uncertainty, 0.05f);
    EXPECT_FLOAT_EQ(bridge->config.max_uncertainty, 0.9f);
}

TEST_F(SecurityEpistemicBridgeTest, SetUncertaintyBoundsInvalid) {
    int result = security_epist_set_uncertainty_bounds(bridge, 0.5f, 0.1f);
    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAMETER);
}

TEST_F(SecurityEpistemicBridgeTest, SetUncertaintyBoundsNullFails) {
    int result = security_epist_set_uncertainty_bounds(nullptr, 0.1f, 0.9f);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Evidence Validation Tests
 * ============================================================================ */

TEST_F(SecurityEpistemicBridgeTest, ValidateEvidenceValid) {
    security_epist_evidence_chain_t chain = create_test_evidence_chain(3, 0.8f);

    security_epist_evidence_status_t status;
    bool valid = security_epist_validate_evidence(bridge, &chain, &status);
    EXPECT_TRUE(valid);
    EXPECT_EQ(status, SEC_EPIST_EVIDENCE_VALID);

    security_epist_stats_t stats;
    security_epist_get_stats(bridge, &stats);
    EXPECT_EQ(stats.evidence_chains_checked, 1u);
    EXPECT_EQ(stats.evidence_chains_valid, 1u);
}

TEST_F(SecurityEpistemicBridgeTest, ValidateEvidenceEmpty) {
    security_epist_evidence_chain_t chain = {};
    chain.link_count = 0;

    security_epist_evidence_status_t status;
    bool valid = security_epist_validate_evidence(bridge, &chain, &status);
    EXPECT_FALSE(valid);
    EXPECT_EQ(status, SEC_EPIST_EVIDENCE_BROKEN);
}

TEST_F(SecurityEpistemicBridgeTest, ValidateEvidenceCircular) {
    security_epist_evidence_chain_t chain = create_test_evidence_chain(3, 0.8f);
    // Create circular reference
    chain.links[2].evidence_id = chain.links[0].evidence_id;

    security_epist_evidence_status_t status;
    bool valid = security_epist_validate_evidence(bridge, &chain, &status);
    EXPECT_FALSE(valid);
    EXPECT_EQ(status, SEC_EPIST_EVIDENCE_CIRCULAR);

    security_epist_stats_t stats;
    security_epist_get_stats(bridge, &stats);
    EXPECT_EQ(stats.circular_evidence_detected, 1u);
}

TEST_F(SecurityEpistemicBridgeTest, ValidateEvidenceUntrustedSource) {
    security_epist_evidence_chain_t chain = create_test_evidence_chain(3, 0.1f);  // Low reliability

    security_epist_evidence_status_t status;
    bool valid = security_epist_validate_evidence(bridge, &chain, &status);
    EXPECT_FALSE(valid);
    EXPECT_EQ(status, SEC_EPIST_EVIDENCE_SOURCE_UNTRUSTED);
}

TEST_F(SecurityEpistemicBridgeTest, ValidateEvidenceNullBridgeFails) {
    security_epist_evidence_chain_t chain = create_test_evidence_chain(1);
    security_epist_evidence_status_t status;
    bool valid = security_epist_validate_evidence(nullptr, &chain, &status);
    EXPECT_FALSE(valid);
}

TEST_F(SecurityEpistemicBridgeTest, ValidateEvidenceNullChainFails) {
    security_epist_evidence_status_t status;
    bool valid = security_epist_validate_evidence(bridge, nullptr, &status);
    EXPECT_FALSE(valid);
}

TEST_F(SecurityEpistemicBridgeTest, CheckCircularEvidenceTrue) {
    security_epist_evidence_chain_t chain = create_test_evidence_chain(5);
    chain.links[3].evidence_id = chain.links[1].evidence_id;

    bool circular = security_epist_check_circular_evidence(bridge, &chain);
    EXPECT_TRUE(circular);
}

TEST_F(SecurityEpistemicBridgeTest, CheckCircularEvidenceFalse) {
    security_epist_evidence_chain_t chain = create_test_evidence_chain(5);

    bool circular = security_epist_check_circular_evidence(bridge, &chain);
    EXPECT_FALSE(circular);
}

TEST_F(SecurityEpistemicBridgeTest, CalculateChainReliability) {
    security_epist_evidence_chain_t chain = create_test_evidence_chain(3, 0.9f);

    float reliability;
    int result = security_epist_calculate_chain_reliability(bridge, &chain, &reliability);
    EXPECT_EQ(result, 0);
    // 0.9 * 0.9 * 0.9 = 0.729
    EXPECT_NEAR(reliability, 0.729f, 0.01f);
}

TEST_F(SecurityEpistemicBridgeTest, CalculateChainReliabilityEmpty) {
    security_epist_evidence_chain_t chain = {};
    chain.link_count = 0;

    float reliability;
    int result = security_epist_calculate_chain_reliability(bridge, &chain, &reliability);
    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(reliability, 0.0f);
}

TEST_F(SecurityEpistemicBridgeTest, RegisterSource) {
    int result = security_epist_register_source(bridge, 10001, 0.85f);
    EXPECT_EQ(result, 0);
}

TEST_F(SecurityEpistemicBridgeTest, RegisterSourceNullFails) {
    int result = security_epist_register_source(nullptr, 10001, 0.85f);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityEpistemicBridgeTest, UpdateSource) {
    security_epist_register_source(bridge, 10002, 0.5f);

    // Mark as correct multiple times
    for (int i = 0; i < 5; i++) {
        int result = security_epist_update_source(bridge, 10002, true);
        EXPECT_EQ(result, 0);
    }
}

TEST_F(SecurityEpistemicBridgeTest, UpdateSourceNotFound) {
    int result = security_epist_update_source(bridge, 99999, true);
    EXPECT_EQ(result, NIMCP_ERROR_NOT_FOUND);
}

TEST_F(SecurityEpistemicBridgeTest, UpdateSourceNullFails) {
    int result = security_epist_update_source(nullptr, 10001, true);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Attack Detection Tests
 * ============================================================================ */

TEST_F(SecurityEpistemicBridgeTest, DetectAttackNone) {
    security_epist_attack_t attack;
    float severity;
    char details[256];

    bool detected = security_epist_detect_attack(bridge, &attack, &severity, details, sizeof(details));
    // Initially, no attack should be detected
    EXPECT_FALSE(detected);
    EXPECT_EQ(attack, SEC_EPIST_ATTACK_NONE);

    security_epist_stats_t stats;
    security_epist_get_stats(bridge, &stats);
    EXPECT_EQ(stats.attack_checks, 1u);
}

TEST_F(SecurityEpistemicBridgeTest, DetectAttackNullBridgeFails) {
    security_epist_attack_t attack;
    float severity;
    bool detected = security_epist_detect_attack(nullptr, &attack, &severity, nullptr, 0);
    EXPECT_FALSE(detected);
}

TEST_F(SecurityEpistemicBridgeTest, DetectAttackNullOutputsSafe) {
    bool detected = security_epist_detect_attack(bridge, nullptr, nullptr, nullptr, 0);
    // Should not crash, just return result
    EXPECT_FALSE(detected);
}

TEST_F(SecurityEpistemicBridgeTest, GetAttackSignature) {
    const char* sig = security_epist_get_attack_signature(SEC_EPIST_ATTACK_CONFIDENCE_INFLATE);
    EXPECT_NE(sig, nullptr);
    EXPECT_NE(strlen(sig), 0u);
}

TEST_F(SecurityEpistemicBridgeTest, GetAttackSignatureAllTypes) {
    for (int i = 0; i < SEC_EPIST_ATTACK_COUNT; i++) {
        const char* sig = security_epist_get_attack_signature(static_cast<security_epist_attack_t>(i));
        EXPECT_NE(sig, nullptr);
    }
}

TEST_F(SecurityEpistemicBridgeTest, ReportFalsePositive) {
    int result = security_epist_report_false_positive(bridge, SEC_EPIST_ATTACK_CONFIDENCE_INFLATE);
    EXPECT_EQ(result, 0);

    security_epist_stats_t stats;
    security_epist_get_stats(bridge, &stats);
    EXPECT_EQ(stats.false_positives, 1u);
}

TEST_F(SecurityEpistemicBridgeTest, ReportFalsePositiveNullFails) {
    int result = security_epist_report_false_positive(nullptr, SEC_EPIST_ATTACK_NONE);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Bidirectional Update Tests
 * ============================================================================ */

TEST_F(SecurityEpistemicBridgeTest, BridgeUpdate) {
    int result = security_epist_bridge_update(bridge, 100);
    EXPECT_EQ(result, 0);
}

TEST_F(SecurityEpistemicBridgeTest, BridgeUpdateNullFails) {
    int result = security_epist_bridge_update(nullptr, 100);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityEpistemicBridgeTest, BridgeUpdateMultipleCycles) {
    for (int i = 0; i < 10; i++) {
        int result = security_epist_bridge_update(bridge, 50);
        EXPECT_EQ(result, 0);
    }
}

TEST_F(SecurityEpistemicBridgeTest, ApplySecurityEffects) {
    int result = security_epist_apply_security_effects(bridge);
    EXPECT_EQ(result, 0);

    security_to_epist_effects_t effects;
    security_epist_get_security_effects(bridge, &effects);
    EXPECT_FLOAT_EQ(effects.enforced_min_confidence, bridge->config.min_confidence);
    EXPECT_FLOAT_EQ(effects.enforced_max_confidence, bridge->config.max_confidence);
}

TEST_F(SecurityEpistemicBridgeTest, ApplySecurityEffectsNullFails) {
    int result = security_epist_apply_security_effects(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityEpistemicBridgeTest, GatherEpistEffects) {
    // Add some confidence values
    for (int i = 0; i < 10; i++) {
        security_epist_conf_status_t status;
        security_epist_validate_confidence(bridge, 0.5f + i * 0.02f, 0, &status);
    }

    int result = security_epist_gather_epist_effects(bridge);
    EXPECT_EQ(result, 0);

    epist_to_security_effects_t effects;
    security_epist_get_epist_effects(bridge, &effects);
    EXPECT_GT(effects.average_confidence, 0.0f);
}

TEST_F(SecurityEpistemicBridgeTest, GatherEpistEffectsNullFails) {
    int result = security_epist_gather_epist_effects(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Query Functions Tests
 * ============================================================================ */

TEST_F(SecurityEpistemicBridgeTest, GetSecurityEffects) {
    security_to_epist_effects_t effects;
    int result = security_epist_get_security_effects(bridge, &effects);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(effects.attack_mode_active);
}

TEST_F(SecurityEpistemicBridgeTest, GetSecurityEffectsNullBridgeFails) {
    security_to_epist_effects_t effects;
    int result = security_epist_get_security_effects(nullptr, &effects);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityEpistemicBridgeTest, GetSecurityEffectsNullOutputFails) {
    int result = security_epist_get_security_effects(bridge, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityEpistemicBridgeTest, GetEpistEffects) {
    epist_to_security_effects_t effects;
    int result = security_epist_get_epist_effects(bridge, &effects);
    EXPECT_EQ(result, 0);
}

TEST_F(SecurityEpistemicBridgeTest, GetEpistEffectsNullBridgeFails) {
    epist_to_security_effects_t effects;
    int result = security_epist_get_epist_effects(nullptr, &effects);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityEpistemicBridgeTest, GetEpistEffectsNullOutputFails) {
    int result = security_epist_get_epist_effects(bridge, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityEpistemicBridgeTest, GetState) {
    security_epist_state_info_t state;
    int result = security_epist_get_state(bridge, &state);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(state.state, SEC_EPIST_STATE_IDLE);
}

TEST_F(SecurityEpistemicBridgeTest, GetStateNullBridgeFails) {
    security_epist_state_info_t state;
    int result = security_epist_get_state(nullptr, &state);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityEpistemicBridgeTest, GetStateNullOutputFails) {
    int result = security_epist_get_state(bridge, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityEpistemicBridgeTest, GetStats) {
    security_epist_stats_t stats;
    int result = security_epist_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
}

TEST_F(SecurityEpistemicBridgeTest, GetStatsNullBridgeFails) {
    security_epist_stats_t stats;
    int result = security_epist_get_stats(nullptr, &stats);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityEpistemicBridgeTest, GetStatsNullOutputFails) {
    int result = security_epist_get_stats(bridge, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityEpistemicBridgeTest, ResetStats) {
    // Generate some stats
    security_epist_conf_status_t status;
    security_epist_validate_confidence(bridge, 0.5f, 0, &status);
    security_epist_validate_confidence(bridge, 0.6f, 0, &status);

    security_epist_stats_t stats;
    security_epist_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_confidence_checks, 2u);

    // Reset
    int result = security_epist_reset_stats(bridge);
    EXPECT_EQ(result, 0);

    security_epist_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_confidence_checks, 0u);
}

TEST_F(SecurityEpistemicBridgeTest, ResetStatsNullFails) {
    int result = security_epist_reset_stats(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Audit Function Tests
 * ============================================================================ */

TEST_F(SecurityEpistemicBridgeTest, AuditEvent) {
    int result = security_epist_audit_event(
        bridge,
        SEC_EPIST_AUDIT_CONFIDENCE,
        0,
        0.05f,
        0.1f,
        true,
        "Confidence corrected to min bound"
    );
    EXPECT_EQ(result, 0);

    security_epist_stats_t stats;
    security_epist_get_stats(bridge, &stats);
    EXPECT_EQ(stats.audit_entries, 1u);
}

TEST_F(SecurityEpistemicBridgeTest, AuditEventNullBridgeFails) {
    int result = security_epist_audit_event(
        nullptr, SEC_EPIST_AUDIT_CONFIDENCE, 0, 0.5f, 0.5f, true, "test");
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityEpistemicBridgeTest, AuditEventNullDetailsSafe) {
    int result = security_epist_audit_event(
        bridge, SEC_EPIST_AUDIT_CONFIDENCE, 0, 0.5f, 0.5f, true, nullptr);
    EXPECT_EQ(result, 0);
}

TEST_F(SecurityEpistemicBridgeTest, GetAuditLog) {
    // Create some audit entries
    for (int i = 0; i < 5; i++) {
        security_epist_audit_event(
            bridge, SEC_EPIST_AUDIT_CONFIDENCE, static_cast<uint64_t>(i + 100),
            0.5f, 0.6f, true, "test entry");
    }

    security_epist_audit_entry_t entries[10];
    size_t count = 0;

    int result = security_epist_get_audit_log(bridge, entries, 10, &count);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(count, 5u);

    for (size_t i = 0; i < count; i++) {
        EXPECT_EQ(entries[i].type, SEC_EPIST_AUDIT_CONFIDENCE);
        EXPECT_TRUE(entries[i].success);
    }
}

TEST_F(SecurityEpistemicBridgeTest, GetAuditLogNullBridgeFails) {
    security_epist_audit_entry_t entries[10];
    size_t count;
    int result = security_epist_get_audit_log(nullptr, entries, 10, &count);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityEpistemicBridgeTest, GetAuditLogNullEntriesFails) {
    size_t count;
    int result = security_epist_get_audit_log(bridge, nullptr, 10, &count);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityEpistemicBridgeTest, GetAuditLogNullCountFails) {
    security_epist_audit_entry_t entries[10];
    int result = security_epist_get_audit_log(bridge, entries, 10, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityEpistemicBridgeTest, ClearAuditLog) {
    // Add entries
    for (int i = 0; i < 3; i++) {
        security_epist_audit_event(
            bridge, SEC_EPIST_AUDIT_BELIEF, 100, 0.5f, 0.5f, true, "test");
    }

    // Clear
    int result = security_epist_clear_audit_log(bridge);
    EXPECT_EQ(result, 0);

    // Verify cleared
    security_epist_audit_entry_t entries[10];
    size_t count = 0;
    security_epist_get_audit_log(bridge, entries, 10, &count);
    EXPECT_EQ(count, 0u);
}

TEST_F(SecurityEpistemicBridgeTest, ClearAuditLogNullFails) {
    int result = security_epist_clear_audit_log(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Bio-Async Integration Tests
 * ============================================================================ */

TEST_F(SecurityEpistemicBridgeTest, ConnectBioAsync) {
    int result = security_epist_connect_bio_async(bridge);
    // May succeed or fail depending on bio-async availability
    if (result != 0) {
        GTEST_SKIP() << "Bio-async router not available";
    }
    EXPECT_EQ(result, 0);
}

TEST_F(SecurityEpistemicBridgeTest, ConnectBioAsyncNullFails) {
    int result = security_epist_connect_bio_async(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityEpistemicBridgeTest, DisconnectBioAsync) {
    int connect_result = security_epist_connect_bio_async(bridge);
    if (connect_result != 0) {
        GTEST_SKIP() << "Bio-async router not available";
    }

    int result = security_epist_disconnect_bio_async(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(SecurityEpistemicBridgeTest, DisconnectBioAsyncNullFails) {
    int result = security_epist_disconnect_bio_async(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityEpistemicBridgeTest, IsBioAsyncConnected) {
    bool connected = security_epist_is_bio_async_connected(bridge);
    EXPECT_FALSE(connected);

    int connect_result = security_epist_connect_bio_async(bridge);
    connected = security_epist_is_bio_async_connected(bridge);
    if (connect_result == 0 && connected) {
        EXPECT_TRUE(connected);
    }
}

TEST_F(SecurityEpistemicBridgeTest, IsBioAsyncConnectedNullFails) {
    bool connected = security_epist_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);
}

/* ============================================================================
 * Utility Function Tests
 * ============================================================================ */

TEST_F(SecurityEpistemicBridgeTest, AttackNameAllTypes) {
    for (int i = 0; i < SEC_EPIST_ATTACK_COUNT; i++) {
        const char* name = security_epist_attack_name(static_cast<security_epist_attack_t>(i));
        EXPECT_NE(name, nullptr);
        EXPECT_STRNE(name, "");
    }
}

TEST_F(SecurityEpistemicBridgeTest, ConfStatusNameAll) {
    const char* names[] = {
        security_epist_conf_status_name(SEC_EPIST_CONF_VALID),
        security_epist_conf_status_name(SEC_EPIST_CONF_TOO_LOW),
        security_epist_conf_status_name(SEC_EPIST_CONF_TOO_HIGH),
        security_epist_conf_status_name(SEC_EPIST_CONF_RATE_ANOMALY),
        security_epist_conf_status_name(SEC_EPIST_CONF_SOURCE_MISMATCH),
        security_epist_conf_status_name(SEC_EPIST_CONF_CALIBRATION_FAIL)
    };

    for (const char* name : names) {
        EXPECT_NE(name, nullptr);
        EXPECT_STRNE(name, "");
    }
}

TEST_F(SecurityEpistemicBridgeTest, BeliefStatusNameAll) {
    const char* names[] = {
        security_epist_belief_status_name(SEC_EPIST_BELIEF_VALID),
        security_epist_belief_status_name(SEC_EPIST_BELIEF_CORRUPTED),
        security_epist_belief_status_name(SEC_EPIST_BELIEF_UNAUTHORIZED),
        security_epist_belief_status_name(SEC_EPIST_BELIEF_CIRCULAR),
        security_epist_belief_status_name(SEC_EPIST_BELIEF_INCONSISTENT),
        security_epist_belief_status_name(SEC_EPIST_BELIEF_EXPIRED)
    };

    for (const char* name : names) {
        EXPECT_NE(name, nullptr);
        EXPECT_STRNE(name, "");
    }
}

TEST_F(SecurityEpistemicBridgeTest, EvidenceStatusNameAll) {
    const char* names[] = {
        security_epist_evidence_status_name(SEC_EPIST_EVIDENCE_VALID),
        security_epist_evidence_status_name(SEC_EPIST_EVIDENCE_BROKEN),
        security_epist_evidence_status_name(SEC_EPIST_EVIDENCE_CIRCULAR),
        security_epist_evidence_status_name(SEC_EPIST_EVIDENCE_FORGED),
        security_epist_evidence_status_name(SEC_EPIST_EVIDENCE_TAMPERED),
        security_epist_evidence_status_name(SEC_EPIST_EVIDENCE_EXPIRED),
        security_epist_evidence_status_name(SEC_EPIST_EVIDENCE_SOURCE_UNTRUSTED)
    };

    for (const char* name : names) {
        EXPECT_NE(name, nullptr);
        EXPECT_STRNE(name, "");
    }
}

TEST_F(SecurityEpistemicBridgeTest, StateNameAll) {
    const char* names[] = {
        security_epist_state_name(SEC_EPIST_STATE_IDLE),
        security_epist_state_name(SEC_EPIST_STATE_VALIDATING),
        security_epist_state_name(SEC_EPIST_STATE_VERIFYING),
        security_epist_state_name(SEC_EPIST_STATE_DETECTING),
        security_epist_state_name(SEC_EPIST_STATE_ENFORCING),
        security_epist_state_name(SEC_EPIST_STATE_AUDITING),
        security_epist_state_name(SEC_EPIST_STATE_ERROR)
    };

    for (const char* name : names) {
        EXPECT_NE(name, nullptr);
        EXPECT_STRNE(name, "");
    }
}

TEST_F(SecurityEpistemicBridgeTest, AuditTypeNameAll) {
    const char* names[] = {
        security_epist_audit_type_name(SEC_EPIST_AUDIT_CONFIDENCE),
        security_epist_audit_type_name(SEC_EPIST_AUDIT_BELIEF),
        security_epist_audit_type_name(SEC_EPIST_AUDIT_UNCERTAINTY),
        security_epist_audit_type_name(SEC_EPIST_AUDIT_EVIDENCE),
        security_epist_audit_type_name(SEC_EPIST_AUDIT_ATTACK),
        security_epist_audit_type_name(SEC_EPIST_AUDIT_CORRECTION),
        security_epist_audit_type_name(SEC_EPIST_AUDIT_REJECTION)
    };

    for (const char* name : names) {
        EXPECT_NE(name, nullptr);
        EXPECT_STRNE(name, "");
    }
}

/* ============================================================================
 * Debug/Print Function Tests (Smoke Tests)
 * ============================================================================ */

TEST_F(SecurityEpistemicBridgeTest, PrintSummaryNullSafe) {
    security_epist_print_summary(nullptr);
    // Should not crash
}

TEST_F(SecurityEpistemicBridgeTest, PrintSummaryValid) {
    security_epist_print_summary(bridge);
    // Should not crash
}

TEST_F(SecurityEpistemicBridgeTest, PrintStatsNullSafe) {
    security_epist_print_stats(nullptr);
    // Should not crash
}

TEST_F(SecurityEpistemicBridgeTest, PrintStatsValid) {
    security_epist_stats_t stats;
    security_epist_get_stats(bridge, &stats);
    security_epist_print_stats(&stats);
    // Should not crash
}

/* ============================================================================
 * Edge Cases and Stress Tests
 * ============================================================================ */

TEST_F(SecurityEpistemicBridgeTest, MaxBeliefsLimit) {
    for (uint32_t i = 0; i < SEC_EPIST_MAX_BELIEFS; i++) {
        int result = security_epist_register_belief(bridge, i + 1, 0xAABB + i, 0.5f);
        if (result != 0) {
            EXPECT_LE(i, SEC_EPIST_MAX_BELIEFS);
            break;
        }
    }
}

TEST_F(SecurityEpistemicBridgeTest, RapidConfidenceValidation) {
    security_epist_conf_status_t status;
    for (int i = 0; i < 1000; i++) {
        float conf = 0.1f + (i % 90) * 0.01f;
        security_epist_validate_confidence(bridge, conf, 0, &status);
    }

    security_epist_stats_t stats;
    security_epist_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_confidence_checks, 1000u);
}

TEST_F(SecurityEpistemicBridgeTest, AuditLogCircularBuffer) {
    // Fill audit log beyond max capacity
    for (uint32_t i = 0; i < SEC_EPIST_MAX_AUDIT_ENTRIES + 100; i++) {
        security_epist_audit_event(
            bridge, SEC_EPIST_AUDIT_CONFIDENCE, i, 0.5f, 0.5f, true, "overflow test");
    }

    // Should still work
    security_epist_audit_entry_t entries[10];
    size_t count = 0;
    int result = security_epist_get_audit_log(bridge, entries, 10, &count);
    EXPECT_EQ(result, 0);
    EXPECT_GT(count, 0u);
}

TEST_F(SecurityEpistemicBridgeTest, ValidateMultipleEvidenceChains) {
    for (int i = 0; i < 50; i++) {
        security_epist_evidence_chain_t chain = create_test_evidence_chain(
            3 + (i % 5),
            0.5f + (i % 5) * 0.1f
        );

        security_epist_evidence_status_t status;
        security_epist_validate_evidence(bridge, &chain, &status);
    }

    security_epist_stats_t stats;
    security_epist_get_stats(bridge, &stats);
    EXPECT_EQ(stats.evidence_chains_checked, 50u);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
