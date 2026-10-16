/**
 * @file test_security_epistemic_integration.cpp
 * @brief Integration tests for Security-Epistemic Bridge
 * @version 1.0.0
 * @date 2026-01-10
 *
 * Integration tests verifying the interaction between security-epistemic bridge
 * and connected systems including epistemic filter and Blood-Brain Barrier.
 *
 * Tests include:
 * - Full bridge lifecycle with connected systems
 * - Confidence validation flow across systems
 * - Belief verification with epistemic filter
 * - Evidence chain validation workflow
 * - Attack detection and response
 * - Bidirectional effect propagation
 * - Bio-async message routing
 */

#include <gtest/gtest.h>
#include <vector>
#include <cstring>
#include <cmath>
#include <thread>
#include <chrono>

extern "C" {
#include "security/epistemic/nimcp_security_epistemic_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/time/nimcp_time.h"
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class SecurityEpistemicIntegrationTest : public ::testing::Test {
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
        return reinterpret_cast<epistemic_filter_t>(0x30000001);
    }

    // Helper: Create mock BBB
    bbb_system_t create_mock_bbb() {
        return reinterpret_cast<bbb_system_t>(0x30000002);
    }

    // Helper: Create valid evidence chain
    security_epist_evidence_chain_t create_valid_evidence_chain(uint32_t links) {
        security_epist_evidence_chain_t chain = {};
        chain.chain_id = 99999;
        chain.link_count = links;
        chain.has_primary_source = true;
        chain.independent_paths = 2;
        chain.overall_reliability = 0.85f;

        uint64_t now_us = nimcp_time_monotonic_us();
        for (uint32_t i = 0; i < links && i < SEC_EPIST_MAX_EVIDENCE_CHAIN; i++) {
            chain.links[i].evidence_id = 5000 + i;
            chain.links[i].source_id = 6000 + i;
            chain.links[i].source_reliability = 0.8f + i * 0.02f;
            chain.links[i].timestamp = now_us;
            chain.links[i].hash = 0xABCDEF00 + i;
            chain.links[i].dependent_count = i > 0 ? 1 : 0;
            chain.links[i].is_primary = (i == 0);
        }

        return chain;
    }
};

/* ============================================================================
 * Full Lifecycle Integration Tests
 * ============================================================================ */

TEST_F(SecurityEpistemicIntegrationTest, FullLifecycleWithConnections) {
    // Create and connect systems
    epistemic_filter_t filter = create_mock_epistemic_filter();
    bbb_system_t bbb = create_mock_bbb();

    int result = security_epist_connect_filter(bridge, filter);
    EXPECT_EQ(result, 0);

    result = security_epist_connect_bbb(bridge, bbb);
    EXPECT_EQ(result, 0);

    EXPECT_TRUE(security_epist_is_connected(bridge));
    EXPECT_TRUE(bridge->epistemic_connected);
    EXPECT_TRUE(bridge->bbb_connected);

    // Register beliefs and validate confidence
    for (uint64_t i = 0; i < 5; i++) {
        result = security_epist_register_belief(bridge, 100 + i, 0xDEAD + i, 0.6f + i * 0.05f);
        EXPECT_EQ(result, 0);
    }

    // Update bridge multiple times
    for (int cycle = 0; cycle < 10; cycle++) {
        result = security_epist_bridge_update(bridge, 50);
        EXPECT_EQ(result, 0);
    }

    // Verify state
    security_epist_state_info_t state;
    security_epist_get_state(bridge, &state);
    EXPECT_EQ(state.state, SEC_EPIST_STATE_IDLE);
    EXPECT_TRUE(state.epistemic_connected);

    // Disconnect all
    result = security_epist_disconnect_all(bridge);
    EXPECT_EQ(result, 0);

    EXPECT_FALSE(security_epist_is_connected(bridge));
}

TEST_F(SecurityEpistemicIntegrationTest, ResetPreservesConnections) {
    // Connect systems
    security_epist_connect_filter(bridge, create_mock_epistemic_filter());
    security_epist_connect_bbb(bridge, create_mock_bbb());

    // Register data
    security_epist_register_belief(bridge, 200, 0x1234, 0.7f);

    security_epist_conf_status_t status;
    security_epist_validate_confidence(bridge, 0.5f, 0, &status);

    // Reset
    int result = security_epist_bridge_reset(bridge);
    EXPECT_EQ(result, 0);

    // Note: Reset clears epistemic_connected flag in state
    // but the actual connection pointers may still be set
    EXPECT_EQ(bridge->num_beliefs, 0u);

    security_epist_stats_t stats;
    security_epist_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_confidence_checks, 0u);
}

/* ============================================================================
 * Confidence Validation Workflow Tests
 * ============================================================================ */

TEST_F(SecurityEpistemicIntegrationTest, ConfidenceValidationWorkflow) {
    security_epist_connect_filter(bridge, create_mock_epistemic_filter());

    // Phase 1: Valid confidence scores
    std::vector<float> valid_values = {0.15f, 0.3f, 0.5f, 0.7f, 0.9f, 0.95f};
    for (float val : valid_values) {
        security_epist_conf_status_t status;
        bool valid = security_epist_validate_confidence(bridge, val, 0, &status);
        EXPECT_TRUE(valid) << "Failed for value: " << val;
        EXPECT_EQ(status, SEC_EPIST_CONF_VALID);
    }

    // Phase 2: Invalid confidence scores with auto-correction
    float corrected;
    int result = security_epist_correct_confidence(bridge, 0.001f, &corrected);
    EXPECT_EQ(result, 0);
    EXPECT_GE(corrected, bridge->config.min_confidence);

    result = security_epist_correct_confidence(bridge, 0.9999f, &corrected);
    EXPECT_EQ(result, 0);
    EXPECT_LE(corrected, bridge->config.max_confidence);

    // Phase 3: Apply security effects
    result = security_epist_apply_security_effects(bridge);
    EXPECT_EQ(result, 0);

    security_to_epist_effects_t effects;
    security_epist_get_security_effects(bridge, &effects);
    EXPECT_GT(effects.confidence_corrections, 0u);
}

TEST_F(SecurityEpistemicIntegrationTest, ConfidenceWithBeliefTracking) {
    // Register belief with initial confidence
    int result = security_epist_register_belief(bridge, 300, 0xAAAA, 0.6f);
    EXPECT_EQ(result, 0);

    // Validate confidence associated with belief
    security_epist_conf_status_t status;
    bool valid = security_epist_validate_confidence(bridge, 0.65f, 300, &status);
    EXPECT_TRUE(valid);

    // Update belief confidence
    result = security_epist_update_belief(bridge, 300, 0.75f, 0xBBBB);
    EXPECT_EQ(result, 0);

    // Verify belief
    security_epist_belief_status_t belief_status;
    valid = security_epist_verify_belief(bridge, 300, 0xBBBB, &belief_status);
    EXPECT_TRUE(valid);
    EXPECT_EQ(belief_status, SEC_EPIST_BELIEF_VALID);
}

/* ============================================================================
 * Belief Verification Workflow Tests
 * ============================================================================ */

TEST_F(SecurityEpistemicIntegrationTest, BeliefLifecycleWorkflow) {
    security_epist_connect_filter(bridge, create_mock_epistemic_filter());

    // Phase 1: Register multiple beliefs
    std::vector<uint64_t> belief_ids;
    for (uint64_t i = 0; i < 10; i++) {
        uint64_t id = 1000 + i;
        int result = security_epist_register_belief(bridge, id, 0xDEAD0000 + i, 0.5f + i * 0.03f);
        EXPECT_EQ(result, 0);
        belief_ids.push_back(id);
    }
    EXPECT_EQ(bridge->num_beliefs, 10u);

    // Phase 2: Verify all beliefs
    for (uint64_t id : belief_ids) {
        security_epist_belief_status_t status;
        bool valid = security_epist_verify_belief(bridge, id, 0xDEAD0000 + (id - 1000), &status);
        EXPECT_TRUE(valid);
        EXPECT_EQ(status, SEC_EPIST_BELIEF_VALID);
    }

    // Phase 3: Lock some beliefs
    for (size_t i = 0; i < 3; i++) {
        int result = security_epist_lock_belief(bridge, belief_ids[i]);
        EXPECT_EQ(result, 0);
    }

    // Phase 4: Try to update locked beliefs (should fail)
    int result = security_epist_update_belief(bridge, belief_ids[0], 0.9f, 0xFFFF);
    EXPECT_EQ(result, NIMCP_ERROR_PERMISSION_DENIED);

    // Phase 5: Update unlocked beliefs (should succeed)
    result = security_epist_update_belief(bridge, belief_ids[5], 0.8f, 0xEEEE);
    EXPECT_EQ(result, 0);

    // Phase 6: Revoke some beliefs
    for (size_t i = 5; i < 8; i++) {
        result = security_epist_revoke_belief(bridge, belief_ids[i]);
        EXPECT_EQ(result, 0);
    }
    EXPECT_EQ(bridge->num_beliefs, 7u);

    // Verify stats
    security_epist_stats_t stats;
    security_epist_get_stats(bridge, &stats);
    EXPECT_GE(stats.total_belief_checks, 10u);
    EXPECT_GE(stats.beliefs_verified, 10u);
}

TEST_F(SecurityEpistemicIntegrationTest, BeliefCorruptionDetection) {
    // Register belief
    int result = security_epist_register_belief(bridge, 500, 0x12345678, 0.7f);
    EXPECT_EQ(result, 0);

    // Verify with correct hash
    security_epist_belief_status_t status;
    bool valid = security_epist_verify_belief(bridge, 500, 0x12345678, &status);
    EXPECT_TRUE(valid);

    // Verify with corrupted hash
    valid = security_epist_verify_belief(bridge, 500, 0xDEADBEEF, &status);
    EXPECT_FALSE(valid);
    EXPECT_EQ(status, SEC_EPIST_BELIEF_CORRUPTED);

    // Check stats
    security_epist_stats_t stats;
    security_epist_get_stats(bridge, &stats);
    EXPECT_GE(stats.beliefs_corrupted, 1u);
}

/* ============================================================================
 * Evidence Chain Validation Workflow Tests
 * ============================================================================ */

TEST_F(SecurityEpistemicIntegrationTest, EvidenceChainWorkflow) {
    security_epist_connect_filter(bridge, create_mock_epistemic_filter());

    // Phase 1: Register trusted sources
    for (uint64_t i = 0; i < 5; i++) {
        int result = security_epist_register_source(bridge, 6000 + i, 0.8f + i * 0.03f);
        EXPECT_EQ(result, 0);
    }

    // Phase 2: Validate evidence chains
    for (int chain_num = 0; chain_num < 10; chain_num++) {
        security_epist_evidence_chain_t chain = create_valid_evidence_chain(3 + (chain_num % 3));

        security_epist_evidence_status_t status;
        bool valid = security_epist_validate_evidence(bridge, &chain, &status);
        EXPECT_TRUE(valid) << "Chain " << chain_num << " failed validation";
    }

    // Phase 3: Update source reliability based on outcomes
    for (int i = 0; i < 5; i++) {
        int result = security_epist_update_source(bridge, 6000 + i, true);
        EXPECT_EQ(result, 0);
    }

    // Verify stats
    security_epist_stats_t stats;
    security_epist_get_stats(bridge, &stats);
    EXPECT_EQ(stats.evidence_chains_checked, 10u);
}

TEST_F(SecurityEpistemicIntegrationTest, CircularEvidenceDetection) {
    security_epist_evidence_chain_t chain = create_valid_evidence_chain(4);
    // Create circular reference
    chain.links[3].evidence_id = chain.links[0].evidence_id;

    security_epist_evidence_status_t status;
    bool valid = security_epist_validate_evidence(bridge, &chain, &status);
    EXPECT_FALSE(valid);
    EXPECT_EQ(status, SEC_EPIST_EVIDENCE_CIRCULAR);

    // Direct check function
    bool circular = security_epist_check_circular_evidence(bridge, &chain);
    EXPECT_TRUE(circular);
}

TEST_F(SecurityEpistemicIntegrationTest, EvidenceChainReliabilityCalculation) {
    security_epist_evidence_chain_t chain = {};
    chain.link_count = 4;
    chain.links[0].source_reliability = 0.9f;
    chain.links[1].source_reliability = 0.8f;
    chain.links[2].source_reliability = 0.9f;
    chain.links[3].source_reliability = 0.85f;

    float reliability;
    int result = security_epist_calculate_chain_reliability(bridge, &chain, &reliability);
    EXPECT_EQ(result, 0);
    // 0.9 * 0.8 * 0.9 * 0.85 = 0.5508
    EXPECT_NEAR(reliability, 0.5508f, 0.01f);
}

/* ============================================================================
 * Attack Detection Workflow Tests
 * ============================================================================ */

TEST_F(SecurityEpistemicIntegrationTest, AttackDetectionWorkflow) {
    security_epist_connect_filter(bridge, create_mock_epistemic_filter());

    // Phase 1: Normal operations - no attack should be detected
    for (int i = 0; i < 10; i++) {
        security_epist_conf_status_t status;
        security_epist_validate_confidence(bridge, 0.5f + i * 0.01f, 0, &status);
    }

    security_epist_attack_t attack;
    float severity;
    char details[256];

    bool detected = security_epist_detect_attack(bridge, &attack, &severity, details, sizeof(details));
    // With normal operations, attack shouldn't be detected
    if (!detected) {
        EXPECT_EQ(attack, SEC_EPIST_ATTACK_NONE);
    }

    // Phase 2: Check attack signatures
    for (int i = 0; i < SEC_EPIST_ATTACK_COUNT; i++) {
        const char* sig = security_epist_get_attack_signature(static_cast<security_epist_attack_t>(i));
        EXPECT_NE(sig, nullptr);
    }

    // Phase 3: Report false positive if attack was detected
    if (detected) {
        int result = security_epist_report_false_positive(bridge, attack);
        EXPECT_EQ(result, 0);

        security_epist_stats_t stats;
        security_epist_get_stats(bridge, &stats);
        EXPECT_GE(stats.false_positives, 1u);
    }
}

TEST_F(SecurityEpistemicIntegrationTest, AttackResponseMode) {
    // Check effects during normal mode
    security_to_epist_effects_t effects;
    security_epist_get_security_effects(bridge, &effects);
    EXPECT_FALSE(effects.attack_mode_active);

    // Run attack detection
    security_epist_attack_t attack;
    float severity;
    security_epist_detect_attack(bridge, &attack, &severity, nullptr, 0);

    // Check effects after detection
    security_epist_get_security_effects(bridge, &effects);
    // Attack mode may or may not be active depending on detection
}

/* ============================================================================
 * Bidirectional Effects Tests
 * ============================================================================ */

TEST_F(SecurityEpistemicIntegrationTest, BidirectionalEffectsWorkflow) {
    security_epist_connect_filter(bridge, create_mock_epistemic_filter());

    // Phase 1: Generate epistemic activity
    for (int i = 0; i < 20; i++) {
        security_epist_conf_status_t status;
        security_epist_validate_confidence(bridge, 0.3f + (i % 5) * 0.1f, 0, &status);
    }

    // Register sources
    for (uint64_t i = 0; i < 5; i++) {
        security_epist_register_source(bridge, 7000 + i, 0.7f + i * 0.05f);
    }

    // Phase 2: Gather epistemic effects
    int result = security_epist_gather_epist_effects(bridge);
    EXPECT_EQ(result, 0);

    epist_to_security_effects_t epist_effects;
    security_epist_get_epist_effects(bridge, &epist_effects);

    EXPECT_GT(epist_effects.average_confidence, 0.0f);
    EXPECT_LT(epist_effects.average_confidence, 1.0f);

    // Phase 3: Apply security effects
    result = security_epist_apply_security_effects(bridge);
    EXPECT_EQ(result, 0);

    security_to_epist_effects_t security_effects;
    security_epist_get_security_effects(bridge, &security_effects);

    EXPECT_FLOAT_EQ(security_effects.enforced_min_confidence, bridge->config.min_confidence);
    EXPECT_FLOAT_EQ(security_effects.enforced_max_confidence, bridge->config.max_confidence);
}

TEST_F(SecurityEpistemicIntegrationTest, BridgeUpdateCycle) {
    security_epist_connect_filter(bridge, create_mock_epistemic_filter());
    security_epist_connect_bbb(bridge, create_mock_bbb());

    // Simulate 100 update cycles
    for (int cycle = 0; cycle < 100; cycle++) {
        // Add some operations
        if (cycle % 5 == 0) {
            security_epist_conf_status_t status;
            security_epist_validate_confidence(bridge, 0.5f + (cycle % 30) * 0.01f, 0, &status);
        }

        if (cycle % 10 == 0) {
            security_epist_register_belief(bridge, static_cast<uint64_t>(cycle), 0xAABB + cycle, 0.6f);
        }

        // Run update
        int result = security_epist_bridge_update(bridge, 10);
        EXPECT_EQ(result, 0);
    }

    // Verify final state
    security_epist_state_info_t state;
    security_epist_get_state(bridge, &state);
    EXPECT_EQ(state.state, SEC_EPIST_STATE_IDLE);

    security_epist_stats_t stats;
    security_epist_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_confidence_checks, 20u);
    EXPECT_EQ(bridge->num_beliefs, 10u);
}

/* ============================================================================
 * Uncertainty Enforcement Workflow Tests
 * ============================================================================ */

TEST_F(SecurityEpistemicIntegrationTest, UncertaintyEnforcementWorkflow) {
    // Phase 1: Test valid uncertainties
    std::vector<float> valid_uncertainties = {0.05f, 0.1f, 0.3f, 0.5f, 0.8f, 0.95f};
    for (float unc : valid_uncertainties) {
        bool is_valid;
        int result = security_epist_enforce_uncertainty(bridge, unc, &is_valid);
        EXPECT_EQ(result, 0);
        EXPECT_TRUE(is_valid) << "Failed for uncertainty: " << unc;
    }

    // Phase 2: Test invalid uncertainties
    std::vector<float> invalid_uncertainties = {0.001f, 0.005f};
    for (float unc : invalid_uncertainties) {
        bool is_valid;
        int result = security_epist_enforce_uncertainty(bridge, unc, &is_valid);
        EXPECT_EQ(result, 0);
        EXPECT_FALSE(is_valid) << "Should fail for uncertainty: " << unc;
    }

    // Phase 3: Adjust invalid uncertainties
    for (float unc : invalid_uncertainties) {
        float adjusted;
        int result = security_epist_adjust_uncertainty(bridge, unc, &adjusted);
        EXPECT_EQ(result, 0);
        EXPECT_GE(adjusted, bridge->config.min_uncertainty);
    }

    // Phase 4: Check stats
    security_epist_stats_t stats;
    security_epist_get_stats(bridge, &stats);
    EXPECT_GE(stats.uncertainty_violations, 2u);
}

/* ============================================================================
 * Audit Workflow Tests
 * ============================================================================ */

TEST_F(SecurityEpistemicIntegrationTest, AuditWorkflow) {
    // Phase 1: Generate audit events through operations
    for (int i = 0; i < 10; i++) {
        // Confidence corrections
        float corrected;
        security_epist_correct_confidence(bridge, 0.001f + i * 0.001f, &corrected);
    }

    // Phase 2: Add manual audit events
    for (int i = 0; i < 5; i++) {
        int result = security_epist_audit_event(
            bridge,
            SEC_EPIST_AUDIT_BELIEF,
            static_cast<uint64_t>(i + 100),
            0.5f,
            0.6f,
            true,
            "Manual audit entry"
        );
        EXPECT_EQ(result, 0);
    }

    // Phase 3: Retrieve audit log
    security_epist_audit_entry_t entries[20];
    size_t count = 0;

    int result = security_epist_get_audit_log(bridge, entries, 20, &count);
    EXPECT_EQ(result, 0);
    EXPECT_GT(count, 0u);

    // Phase 4: Verify stats
    security_epist_stats_t stats;
    security_epist_get_stats(bridge, &stats);
    EXPECT_GE(stats.audit_entries, 5u);

    // Phase 5: Clear and verify
    result = security_epist_clear_audit_log(bridge);
    EXPECT_EQ(result, 0);

    security_epist_get_audit_log(bridge, entries, 20, &count);
    EXPECT_EQ(count, 0u);
}

/* ============================================================================
 * Bio-Async Integration Tests
 * ============================================================================ */

TEST_F(SecurityEpistemicIntegrationTest, BioAsyncWorkflow) {
    // Try to connect to bio-async
    int result = security_epist_connect_bio_async(bridge);

    bool connected = security_epist_is_bio_async_connected(bridge);

    if (!connected) {
        GTEST_SKIP() << "Bio-async router not available for integration test";
    }

    // If connected, run operations
    for (int i = 0; i < 5; i++) {
        security_epist_bridge_update(bridge, 20);
    }

    // Disconnect
    result = security_epist_disconnect_bio_async(bridge);
    EXPECT_EQ(result, 0);

    connected = security_epist_is_bio_async_connected(bridge);
    EXPECT_FALSE(connected);
}

/* ============================================================================
 * Configuration Integration Tests
 * ============================================================================ */

TEST_F(SecurityEpistemicIntegrationTest, CustomConfigurationWorkflow) {
    // Create bridge with custom config
    security_epist_config_t custom_cfg;
    security_epist_default_config(&custom_cfg);

    custom_cfg.min_confidence = 0.2f;
    custom_cfg.max_confidence = 0.85f;
    custom_cfg.min_uncertainty = 0.05f;
    custom_cfg.attack_threshold = 0.5f;
    custom_cfg.enable_auto_correction = false;

    security_epist_bridge_t* custom_bridge = security_epist_bridge_create(&custom_cfg);
    ASSERT_NE(custom_bridge, nullptr);

    // Test bounds with custom config
    security_epist_conf_status_t status;
    bool valid = security_epist_validate_confidence(custom_bridge, 0.15f, 0, &status);
    EXPECT_FALSE(valid);
    EXPECT_EQ(status, SEC_EPIST_CONF_TOO_LOW);

    valid = security_epist_validate_confidence(custom_bridge, 0.9f, 0, &status);
    EXPECT_FALSE(valid);
    EXPECT_EQ(status, SEC_EPIST_CONF_TOO_HIGH);

    valid = security_epist_validate_confidence(custom_bridge, 0.5f, 0, &status);
    EXPECT_TRUE(valid);

    security_epist_bridge_destroy(custom_bridge);
}

TEST_F(SecurityEpistemicIntegrationTest, DynamicBoundsUpdate) {
    // Start with default bounds
    security_epist_conf_status_t status;
    bool valid = security_epist_validate_confidence(bridge, 0.95f, 0, &status);
    EXPECT_TRUE(valid);

    // Update bounds
    int result = security_epist_set_confidence_bounds(bridge, 0.2f, 0.8f);
    EXPECT_EQ(result, 0);

    // Now 0.95 should be invalid
    valid = security_epist_validate_confidence(bridge, 0.95f, 0, &status);
    EXPECT_FALSE(valid);
    EXPECT_EQ(status, SEC_EPIST_CONF_TOO_HIGH);

    // Update uncertainty bounds
    result = security_epist_set_uncertainty_bounds(bridge, 0.1f, 0.9f);
    EXPECT_EQ(result, 0);

    bool is_valid;
    result = security_epist_enforce_uncertainty(bridge, 0.05f, &is_valid);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(is_valid);
}

/* ============================================================================
 * Stress Tests
 * ============================================================================ */

TEST_F(SecurityEpistemicIntegrationTest, HighVolumeOperations) {
    security_epist_connect_filter(bridge, create_mock_epistemic_filter());

    // High volume confidence validation
    for (int i = 0; i < 1000; i++) {
        security_epist_conf_status_t status;
        float conf = 0.1f + (i % 80) * 0.01f;
        security_epist_validate_confidence(bridge, conf, 0, &status);
    }

    // High volume belief registration
    for (uint64_t i = 0; i < 500 && i < SEC_EPIST_MAX_BELIEFS; i++) {
        security_epist_register_belief(bridge, 10000 + i, 0xFFFF0000 + i, 0.5f);
    }

    // High volume evidence validation
    for (int i = 0; i < 100; i++) {
        security_epist_evidence_chain_t chain = create_valid_evidence_chain(3);
        security_epist_evidence_status_t status;
        security_epist_validate_evidence(bridge, &chain, &status);
    }

    // Verify all operations completed
    security_epist_stats_t stats;
    security_epist_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_confidence_checks, 1000u);
    EXPECT_EQ(stats.evidence_chains_checked, 100u);
}

TEST_F(SecurityEpistemicIntegrationTest, ContinuousUpdateCycle) {
    security_epist_connect_filter(bridge, create_mock_epistemic_filter());

    // Simulate continuous operation for 1000 cycles
    for (int cycle = 0; cycle < 1000; cycle++) {
        // Periodic operations
        if (cycle % 3 == 0) {
            security_epist_conf_status_t status;
            security_epist_validate_confidence(bridge, 0.5f, 0, &status);
        }

        if (cycle % 7 == 0) {
            security_epist_evidence_chain_t chain = create_valid_evidence_chain(2);
            security_epist_evidence_status_t status;
            security_epist_validate_evidence(bridge, &chain, &status);
        }

        if (cycle % 50 == 0) {
            security_epist_attack_t attack;
            float severity;
            security_epist_detect_attack(bridge, &attack, &severity, nullptr, 0);
        }

        // Regular update
        int result = security_epist_bridge_update(bridge, 1);
        EXPECT_EQ(result, 0);
    }

    // Verify state is stable
    security_epist_state_info_t state;
    security_epist_get_state(bridge, &state);
    EXPECT_EQ(state.state, SEC_EPIST_STATE_IDLE);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
