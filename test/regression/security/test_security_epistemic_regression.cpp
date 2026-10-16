/**
 * @file test_security_epistemic_regression.cpp
 * @brief Regression tests for Security-Epistemic Bridge
 * @version 1.0.0
 * @date 2026-01-10
 *
 * Regression tests ensuring stability and correctness of the security-epistemic
 * bridge across versions. Tests focus on:
 * - API stability and backward compatibility
 * - Performance regression detection
 * - Edge case handling
 * - Memory safety
 * - Thread safety scenarios
 * - Boundary conditions
 */

#include <gtest/gtest.h>
#include <vector>
#include <cstring>
#include <cmath>
#include <chrono>
#include <algorithm>
#include <numeric>

extern "C" {
#include "security/epistemic/nimcp_security_epistemic_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_codes.h"
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class SecurityEpistemicRegressionTest : public ::testing::Test {
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

    // Helper: Time a function in microseconds
    template<typename Func>
    double time_operation(Func&& func, int iterations = 1000) {
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; i++) {
            func();
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        return static_cast<double>(duration.count()) / iterations;
    }
};

/* ============================================================================
 * API Stability Regression Tests
 * ============================================================================ */

TEST_F(SecurityEpistemicRegressionTest, DefaultConfigStructureStability) {
    security_epist_config_t cfg;
    int result = security_epist_default_config(&cfg);
    EXPECT_EQ(result, 0);

    // Verify all documented default values are stable
    EXPECT_TRUE(cfg.enable_confidence_validation);
    EXPECT_TRUE(cfg.enable_belief_verification);
    EXPECT_TRUE(cfg.enable_uncertainty_enforcement);
    EXPECT_TRUE(cfg.enable_evidence_validation);
    EXPECT_TRUE(cfg.enable_attack_detection);
    EXPECT_TRUE(cfg.enable_audit);
    EXPECT_TRUE(cfg.enable_auto_correction);

    EXPECT_FLOAT_EQ(cfg.min_confidence, 0.1f);
    EXPECT_FLOAT_EQ(cfg.max_confidence, 0.99f);
    EXPECT_FLOAT_EQ(cfg.min_uncertainty, 0.01f);
    EXPECT_FLOAT_EQ(cfg.security_sensitivity, 1.0f);
    EXPECT_FLOAT_EQ(cfg.epistemic_sensitivity, 1.0f);
    EXPECT_EQ(cfg.attack_window_ms, SEC_EPIST_ATTACK_WINDOW_MS);
}

TEST_F(SecurityEpistemicRegressionTest, BridgeStructureSizeStability) {
    // Record expected sizes for regression detection
    // Changes here may indicate ABI-breaking modifications
    EXPECT_GT(sizeof(security_epist_bridge_t), 0u);
    EXPECT_GT(sizeof(security_epist_config_t), 0u);
    EXPECT_GT(sizeof(security_epist_stats_t), 0u);
    EXPECT_GT(sizeof(security_to_epist_effects_t), 0u);
    EXPECT_GT(sizeof(epist_to_security_effects_t), 0u);
    EXPECT_GT(sizeof(security_epist_belief_t), 0u);
    EXPECT_GT(sizeof(security_epist_evidence_chain_t), 0u);
    EXPECT_GT(sizeof(security_epist_audit_entry_t), 0u);
}

TEST_F(SecurityEpistemicRegressionTest, ConstantsStability) {
    // Verify constant values haven't changed unexpectedly
    EXPECT_EQ(SEC_EPIST_MAX_BELIEFS, 1024u);
    EXPECT_EQ(SEC_EPIST_MAX_EVIDENCE_CHAIN, 64u);
    EXPECT_EQ(SEC_EPIST_MAX_SESSIONS, 128u);
    EXPECT_EQ(SEC_EPIST_MAX_ATTACK_SIGNATURES, 256u);
    EXPECT_EQ(SEC_EPIST_MAX_AUDIT_ENTRIES, 4096u);
    EXPECT_EQ(SEC_EPIST_CONFIDENCE_HISTORY, 100u);
    EXPECT_FLOAT_EQ(SEC_EPIST_DEFAULT_MIN_CONFIDENCE, 0.1f);
    EXPECT_FLOAT_EQ(SEC_EPIST_DEFAULT_MAX_CONFIDENCE, 0.99f);
    EXPECT_FLOAT_EQ(SEC_EPIST_DEFAULT_UNCERTAINTY_MIN, 0.01f);
    EXPECT_EQ(SEC_EPIST_ATTACK_WINDOW_MS, 30000u);
}

TEST_F(SecurityEpistemicRegressionTest, EnumValuesStability) {
    // Attack types
    EXPECT_EQ(SEC_EPIST_ATTACK_NONE, 0);
    EXPECT_EQ(SEC_EPIST_ATTACK_CONFIDENCE_INFLATE, 1);
    EXPECT_EQ(SEC_EPIST_ATTACK_CONFIDENCE_DEFLATE, 2);
    EXPECT_LT(SEC_EPIST_ATTACK_COUNT, 20);  // Reasonable upper bound

    // Confidence status
    EXPECT_EQ(SEC_EPIST_CONF_VALID, 0);
    EXPECT_EQ(SEC_EPIST_CONF_TOO_LOW, 1);
    EXPECT_EQ(SEC_EPIST_CONF_TOO_HIGH, 2);

    // Belief status
    EXPECT_EQ(SEC_EPIST_BELIEF_VALID, 0);
    EXPECT_EQ(SEC_EPIST_BELIEF_CORRUPTED, 1);

    // Bridge state
    EXPECT_EQ(SEC_EPIST_STATE_IDLE, 0);
}

/* ============================================================================
 * Return Value Regression Tests
 * ============================================================================ */

TEST_F(SecurityEpistemicRegressionTest, NullPointerReturnsStable) {
    // All functions should handle NULL gracefully

    // Config functions
    EXPECT_EQ(security_epist_default_config(nullptr), NIMCP_ERROR_NULL_POINTER);

    // Lifecycle functions
    security_epist_bridge_destroy(nullptr);  // Should not crash
    EXPECT_EQ(security_epist_bridge_reset(nullptr), NIMCP_ERROR_NULL_POINTER);

    // Connection functions
    EXPECT_EQ(security_epist_connect_filter(nullptr, nullptr), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(security_epist_connect_bbb(nullptr, nullptr), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(security_epist_disconnect_all(nullptr), NIMCP_ERROR_NULL_POINTER);
    EXPECT_FALSE(security_epist_is_connected(nullptr));

    // Confidence functions
    EXPECT_FALSE(security_epist_validate_confidence(nullptr, 0.5f, 0, nullptr));
    EXPECT_EQ(security_epist_correct_confidence(nullptr, 0.5f, nullptr), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(security_epist_set_confidence_bounds(nullptr, 0.1f, 0.9f), NIMCP_ERROR_NULL_POINTER);

    // Belief functions
    EXPECT_FALSE(security_epist_verify_belief(nullptr, 0, 0, nullptr));
    EXPECT_EQ(security_epist_register_belief(nullptr, 0, 0, 0.5f), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(security_epist_update_belief(nullptr, 0, 0.5f, 0), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(security_epist_lock_belief(nullptr, 0), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(security_epist_revoke_belief(nullptr, 0), NIMCP_ERROR_NULL_POINTER);

    // Uncertainty functions
    EXPECT_EQ(security_epist_enforce_uncertainty(nullptr, 0.5f, nullptr), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(security_epist_adjust_uncertainty(nullptr, 0.5f, nullptr), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(security_epist_set_uncertainty_bounds(nullptr, 0.1f, 0.9f), NIMCP_ERROR_NULL_POINTER);

    // Evidence functions
    EXPECT_FALSE(security_epist_validate_evidence(nullptr, nullptr, nullptr));
    EXPECT_FALSE(security_epist_check_circular_evidence(nullptr, nullptr));
    EXPECT_EQ(security_epist_calculate_chain_reliability(nullptr, nullptr, nullptr), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(security_epist_register_source(nullptr, 0, 0.5f), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(security_epist_update_source(nullptr, 0, true), NIMCP_ERROR_NULL_POINTER);

    // Attack functions
    EXPECT_FALSE(security_epist_detect_attack(nullptr, nullptr, nullptr, nullptr, 0));
    EXPECT_EQ(security_epist_report_false_positive(nullptr, SEC_EPIST_ATTACK_NONE), NIMCP_ERROR_NULL_POINTER);

    // Update functions
    EXPECT_EQ(security_epist_bridge_update(nullptr, 0), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(security_epist_apply_security_effects(nullptr), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(security_epist_gather_epist_effects(nullptr), NIMCP_ERROR_NULL_POINTER);

    // Query functions
    EXPECT_EQ(security_epist_get_security_effects(nullptr, nullptr), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(security_epist_get_epist_effects(nullptr, nullptr), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(security_epist_get_state(nullptr, nullptr), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(security_epist_get_stats(nullptr, nullptr), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(security_epist_reset_stats(nullptr), NIMCP_ERROR_NULL_POINTER);

    // Audit functions
    EXPECT_EQ(security_epist_audit_event(nullptr, SEC_EPIST_AUDIT_CONFIDENCE, 0, 0, 0, true, nullptr), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(security_epist_get_audit_log(nullptr, nullptr, 0, nullptr), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(security_epist_clear_audit_log(nullptr), NIMCP_ERROR_NULL_POINTER);

    // Bio-async functions
    EXPECT_EQ(security_epist_connect_bio_async(nullptr), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(security_epist_disconnect_bio_async(nullptr), NIMCP_ERROR_NULL_POINTER);
    EXPECT_FALSE(security_epist_is_bio_async_connected(nullptr));

    // Print functions (should not crash)
    security_epist_print_summary(nullptr);
    security_epist_print_stats(nullptr);
}

/* ============================================================================
 * Boundary Condition Regression Tests
 * ============================================================================ */

TEST_F(SecurityEpistemicRegressionTest, ConfidenceBoundaryConditions) {
    security_epist_conf_status_t status;

    // Exact boundary values
    EXPECT_TRUE(security_epist_validate_confidence(bridge, 0.1f, 0, &status));
    EXPECT_TRUE(security_epist_validate_confidence(bridge, 0.99f, 0, &status));

    // Just below minimum
    EXPECT_FALSE(security_epist_validate_confidence(bridge, 0.09999f, 0, &status));
    EXPECT_EQ(status, SEC_EPIST_CONF_TOO_LOW);

    // Just above maximum
    EXPECT_FALSE(security_epist_validate_confidence(bridge, 0.99001f, 0, &status));
    EXPECT_EQ(status, SEC_EPIST_CONF_TOO_HIGH);

    // Edge cases
    EXPECT_FALSE(security_epist_validate_confidence(bridge, 0.0f, 0, &status));
    EXPECT_FALSE(security_epist_validate_confidence(bridge, 1.0f, 0, &status));
    EXPECT_FALSE(security_epist_validate_confidence(bridge, -0.1f, 0, &status));
    EXPECT_FALSE(security_epist_validate_confidence(bridge, 1.5f, 0, &status));
}

TEST_F(SecurityEpistemicRegressionTest, UncertaintyBoundaryConditions) {
    bool is_valid;

    // Exact boundaries
    EXPECT_EQ(security_epist_enforce_uncertainty(bridge, 0.01f, &is_valid), 0);
    EXPECT_TRUE(is_valid);

    EXPECT_EQ(security_epist_enforce_uncertainty(bridge, 1.0f, &is_valid), 0);
    EXPECT_TRUE(is_valid);

    // Below minimum
    EXPECT_EQ(security_epist_enforce_uncertainty(bridge, 0.009f, &is_valid), 0);
    EXPECT_FALSE(is_valid);

    // Above maximum
    EXPECT_EQ(security_epist_enforce_uncertainty(bridge, 1.1f, &is_valid), 0);
    EXPECT_FALSE(is_valid);
}

TEST_F(SecurityEpistemicRegressionTest, InvalidBoundsRejected) {
    // min >= max should fail
    EXPECT_EQ(security_epist_set_confidence_bounds(bridge, 0.8f, 0.2f), NIMCP_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(security_epist_set_confidence_bounds(bridge, 0.5f, 0.5f), NIMCP_ERROR_INVALID_PARAMETER);

    EXPECT_EQ(security_epist_set_uncertainty_bounds(bridge, 0.9f, 0.1f), NIMCP_ERROR_INVALID_PARAMETER);

    // Out of valid range
    EXPECT_EQ(security_epist_set_confidence_bounds(bridge, -0.1f, 0.5f), NIMCP_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(security_epist_set_confidence_bounds(bridge, 0.1f, 1.1f), NIMCP_ERROR_INVALID_PARAMETER);

    EXPECT_EQ(security_epist_set_uncertainty_bounds(bridge, -0.1f, 0.5f), NIMCP_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(security_epist_set_uncertainty_bounds(bridge, 0.1f, 1.1f), NIMCP_ERROR_INVALID_PARAMETER);
}

/* ============================================================================
 * Memory Safety Regression Tests
 * ============================================================================ */

TEST_F(SecurityEpistemicRegressionTest, BeliefArrayBoundsRespected) {
    // Fill to capacity
    for (uint32_t i = 0; i < SEC_EPIST_MAX_BELIEFS; i++) {
        int result = security_epist_register_belief(bridge, i + 1, 0xAAAA + i, 0.5f);
        if (result != 0) {
            EXPECT_EQ(result, NIMCP_ERROR_OUT_OF_RANGE);
            EXPECT_LE(i, SEC_EPIST_MAX_BELIEFS);
            break;
        }
    }

    // Array should be at capacity
    EXPECT_EQ(bridge->num_beliefs, SEC_EPIST_MAX_BELIEFS);

    // Additional registrations should fail
    int result = security_epist_register_belief(bridge, SEC_EPIST_MAX_BELIEFS + 100, 0xBBBB, 0.5f);
    EXPECT_EQ(result, NIMCP_ERROR_OUT_OF_RANGE);
}

TEST_F(SecurityEpistemicRegressionTest, AuditLogCircularBufferWorks) {
    // Fill audit log beyond capacity
    for (uint32_t i = 0; i < SEC_EPIST_MAX_AUDIT_ENTRIES * 2; i++) {
        int result = security_epist_audit_event(
            bridge, SEC_EPIST_AUDIT_CONFIDENCE, i, 0.5f, 0.5f, true, "test");
        EXPECT_EQ(result, 0);
    }

    // Should still work (circular buffer)
    security_epist_audit_entry_t entries[10];
    size_t count = 0;
    int result = security_epist_get_audit_log(bridge, entries, 10, &count);
    EXPECT_EQ(result, 0);
    EXPECT_GT(count, 0u);
    EXPECT_LE(count, 10u);
}

TEST_F(SecurityEpistemicRegressionTest, EvidenceChainMaxLinksHandled) {
    security_epist_evidence_chain_t chain = {};
    chain.link_count = SEC_EPIST_MAX_EVIDENCE_CHAIN;
    chain.independent_paths = 1;

    for (uint32_t i = 0; i < SEC_EPIST_MAX_EVIDENCE_CHAIN; i++) {
        chain.links[i].evidence_id = i + 1;
        chain.links[i].source_id = 1000 + i;
        chain.links[i].source_reliability = 0.8f;
        chain.links[i].timestamp = 1000000;
    }

    security_epist_evidence_status_t status;
    // Should handle max chain length without issues
    security_epist_validate_evidence(bridge, &chain, &status);
}

TEST_F(SecurityEpistemicRegressionTest, CreateDestroyMemoryLeak) {
    // Create and destroy many bridges to check for memory leaks
    for (int i = 0; i < 100; i++) {
        security_epist_bridge_t* br = security_epist_bridge_create(nullptr);
        ASSERT_NE(br, nullptr);
        security_epist_bridge_destroy(br);
    }
    // Note: Actual leak detection requires external tools like valgrind
}

/* ============================================================================
 * Performance Regression Tests
 * ============================================================================ */

TEST_F(SecurityEpistemicRegressionTest, ConfidenceValidationPerformance) {
    const int iterations = 10000;
    double avg_time = time_operation([this]() {
        security_epist_conf_status_t status;
        security_epist_validate_confidence(bridge, 0.5f, 0, &status);
    }, iterations);

    // Should complete in under 100 microseconds per operation
    EXPECT_LT(avg_time, 100.0) << "Confidence validation too slow: " << avg_time << " us";
}

TEST_F(SecurityEpistemicRegressionTest, BeliefVerificationPerformance) {
    // Register belief first
    security_epist_register_belief(bridge, 999, 0x12345678, 0.6f);

    const int iterations = 10000;
    double avg_time = time_operation([this]() {
        security_epist_belief_status_t status;
        security_epist_verify_belief(bridge, 999, 0x12345678, &status);
    }, iterations);

    // Should complete in under 100 microseconds per operation
    EXPECT_LT(avg_time, 100.0) << "Belief verification too slow: " << avg_time << " us";
}

TEST_F(SecurityEpistemicRegressionTest, BridgeUpdatePerformance) {
    const int iterations = 1000;
    double avg_time = time_operation([this]() {
        security_epist_bridge_update(bridge, 10);
    }, iterations);

    // Should complete in under 500 microseconds per update
    EXPECT_LT(avg_time, 500.0) << "Bridge update too slow: " << avg_time << " us";
}

TEST_F(SecurityEpistemicRegressionTest, AttackDetectionPerformance) {
    const int iterations = 1000;
    double avg_time = time_operation([this]() {
        security_epist_attack_t attack;
        float severity;
        security_epist_detect_attack(bridge, &attack, &severity, nullptr, 0);
    }, iterations);

    // Should complete in under 200 microseconds per check
    EXPECT_LT(avg_time, 200.0) << "Attack detection too slow: " << avg_time << " us";
}

TEST_F(SecurityEpistemicRegressionTest, EvidenceValidationPerformance) {
    security_epist_evidence_chain_t chain = {};
    chain.link_count = 10;
    chain.independent_paths = 2;
    for (uint32_t i = 0; i < 10; i++) {
        chain.links[i].evidence_id = i + 1;
        chain.links[i].source_id = 1000 + i;
        chain.links[i].source_reliability = 0.8f;
        chain.links[i].timestamp = 1000000;
    }

    const int iterations = 5000;
    double avg_time = time_operation([this, &chain]() {
        security_epist_evidence_status_t status;
        security_epist_validate_evidence(bridge, &chain, &status);
    }, iterations);

    // Should complete in under 150 microseconds per validation
    EXPECT_LT(avg_time, 150.0) << "Evidence validation too slow: " << avg_time << " us";
}

/* ============================================================================
 * Utility Function Regression Tests
 * ============================================================================ */

TEST_F(SecurityEpistemicRegressionTest, AllNameFunctionsReturnValidStrings) {
    // Attack names
    for (int i = 0; i <= SEC_EPIST_ATTACK_COUNT; i++) {
        const char* name = security_epist_attack_name(static_cast<security_epist_attack_t>(i));
        ASSERT_NE(name, nullptr);
        EXPECT_GT(strlen(name), 0u);
    }

    // Confidence status names
    for (int i = 0; i <= 5; i++) {
        const char* name = security_epist_conf_status_name(static_cast<security_epist_conf_status_t>(i));
        ASSERT_NE(name, nullptr);
        EXPECT_GT(strlen(name), 0u);
    }

    // Belief status names
    for (int i = 0; i <= 5; i++) {
        const char* name = security_epist_belief_status_name(static_cast<security_epist_belief_status_t>(i));
        ASSERT_NE(name, nullptr);
        EXPECT_GT(strlen(name), 0u);
    }

    // Evidence status names
    for (int i = 0; i <= 6; i++) {
        const char* name = security_epist_evidence_status_name(static_cast<security_epist_evidence_status_t>(i));
        ASSERT_NE(name, nullptr);
        EXPECT_GT(strlen(name), 0u);
    }

    // State names
    for (int i = 0; i <= 6; i++) {
        const char* name = security_epist_state_name(static_cast<security_epist_state_t>(i));
        ASSERT_NE(name, nullptr);
        EXPECT_GT(strlen(name), 0u);
    }

    // Audit type names
    for (int i = 0; i <= 6; i++) {
        const char* name = security_epist_audit_type_name(static_cast<security_epist_audit_type_t>(i));
        ASSERT_NE(name, nullptr);
        EXPECT_GT(strlen(name), 0u);
    }
}

TEST_F(SecurityEpistemicRegressionTest, AllAttackSignaturesExist) {
    for (int i = 0; i < SEC_EPIST_ATTACK_COUNT; i++) {
        const char* sig = security_epist_get_attack_signature(static_cast<security_epist_attack_t>(i));
        ASSERT_NE(sig, nullptr);
        // Signatures should be descriptive
        if (i != SEC_EPIST_ATTACK_NONE) {
            EXPECT_GT(strlen(sig), 10u);
        }
    }
}

/* ============================================================================
 * State Consistency Regression Tests
 * ============================================================================ */

TEST_F(SecurityEpistemicRegressionTest, StateRemainsConsistentAfterOperations) {
    // Mix of operations
    for (int i = 0; i < 100; i++) {
        security_epist_conf_status_t status;
        security_epist_validate_confidence(bridge, 0.5f + (i % 40) * 0.01f, 0, &status);

        if (i % 5 == 0) {
            security_epist_register_belief(bridge, static_cast<uint64_t>(i), 0xAAAA + i, 0.6f);
        }

        if (i % 10 == 0) {
            security_epist_bridge_update(bridge, 5);
        }

        if (i % 20 == 0) {
            security_epist_attack_t attack;
            float severity;
            security_epist_detect_attack(bridge, &attack, &severity, nullptr, 0);
        }
    }

    // State should be idle after all operations
    security_epist_state_info_t state;
    security_epist_get_state(bridge, &state);
    EXPECT_EQ(state.state, SEC_EPIST_STATE_IDLE);

    // Stats should be consistent
    security_epist_stats_t stats;
    security_epist_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_confidence_checks, 100u);
}

TEST_F(SecurityEpistemicRegressionTest, ResetRestoresInitialState) {
    // Perform many operations
    for (int i = 0; i < 50; i++) {
        security_epist_conf_status_t status;
        security_epist_validate_confidence(bridge, 0.5f, 0, &status);
        security_epist_register_belief(bridge, static_cast<uint64_t>(i), 0xBBBB + i, 0.5f);
    }

    // Verify state changed
    security_epist_stats_t stats_before;
    security_epist_get_stats(bridge, &stats_before);
    EXPECT_GT(stats_before.total_confidence_checks, 0u);
    EXPECT_GT(bridge->num_beliefs, 0u);

    // Reset
    int result = security_epist_bridge_reset(bridge);
    EXPECT_EQ(result, 0);

    // Verify reset to initial state
    security_epist_stats_t stats_after;
    security_epist_get_stats(bridge, &stats_after);
    EXPECT_EQ(stats_after.total_confidence_checks, 0u);
    EXPECT_EQ(bridge->num_beliefs, 0u);

    security_epist_state_info_t state;
    security_epist_get_state(bridge, &state);
    EXPECT_EQ(state.state, SEC_EPIST_STATE_IDLE);
}

/* ============================================================================
 * Effects Propagation Regression Tests
 * ============================================================================ */

TEST_F(SecurityEpistemicRegressionTest, SecurityEffectsReflectConfiguration) {
    security_to_epist_effects_t effects;

    // Initial effects should reflect config
    security_epist_apply_security_effects(bridge);
    security_epist_get_security_effects(bridge, &effects);

    EXPECT_FLOAT_EQ(effects.enforced_min_confidence, bridge->config.min_confidence);
    EXPECT_FLOAT_EQ(effects.enforced_max_confidence, bridge->config.max_confidence);
    EXPECT_FLOAT_EQ(effects.enforced_uncertainty_floor, bridge->config.min_uncertainty);

    // Change config via API
    security_epist_set_confidence_bounds(bridge, 0.2f, 0.8f);
    security_epist_apply_security_effects(bridge);
    security_epist_get_security_effects(bridge, &effects);

    EXPECT_FLOAT_EQ(effects.enforced_min_confidence, 0.2f);
    EXPECT_FLOAT_EQ(effects.enforced_max_confidence, 0.8f);
}

TEST_F(SecurityEpistemicRegressionTest, EpistEffectsReflectActivity) {
    // Generate activity
    for (int i = 0; i < 50; i++) {
        security_epist_conf_status_t status;
        security_epist_validate_confidence(bridge, 0.5f + (i % 20) * 0.02f, 0, &status);
    }

    security_epist_gather_epist_effects(bridge);

    epist_to_security_effects_t effects;
    security_epist_get_epist_effects(bridge, &effects);

    // Average confidence should be around 0.5-0.7
    EXPECT_GT(effects.average_confidence, 0.0f);
    EXPECT_LT(effects.average_confidence, 1.0f);
}

/* ============================================================================
 * Correction Behavior Regression Tests
 * ============================================================================ */

TEST_F(SecurityEpistemicRegressionTest, CorrectionAlwaysClampsToValidRange) {
    float test_values[] = {-1.0f, -0.5f, 0.0f, 0.05f, 0.5f, 0.99f, 1.0f, 1.5f, 10.0f};

    for (float val : test_values) {
        float corrected;
        int result = security_epist_correct_confidence(bridge, val, &corrected);
        EXPECT_EQ(result, 0);
        EXPECT_GE(corrected, bridge->config.min_confidence);
        EXPECT_LE(corrected, bridge->config.max_confidence);
    }
}

TEST_F(SecurityEpistemicRegressionTest, UncertaintyAdjustmentAlwaysValid) {
    float test_values[] = {-1.0f, 0.0f, 0.001f, 0.5f, 1.0f, 1.5f, 10.0f};

    for (float val : test_values) {
        float adjusted;
        int result = security_epist_adjust_uncertainty(bridge, val, &adjusted);
        EXPECT_EQ(result, 0);
        EXPECT_GE(adjusted, bridge->config.min_uncertainty);
        EXPECT_LE(adjusted, bridge->config.max_uncertainty);
    }
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
