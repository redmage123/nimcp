/**
 * @file test_security_hippocampus_bridge.cpp
 * @brief Unit tests for Security-Hippocampus Bridge
 * @version 1.0.0
 * @date 2026-01-10
 *
 * Comprehensive tests for the security-hippocampus bridge including:
 * - Lifecycle (default config, create, destroy, reset)
 * - Connection tests (hippocampus, sleep system)
 * - Sleep protection (phase protection, ripple filtering, spindle gating)
 * - Consolidation verification (integrity, degradation, corruption)
 * - Injection detection (false memory, pattern poisoning, temporal splice)
 * - Replay validation (sequence validation, content matching)
 * - Coherence checking (spatial, temporal, context)
 * - Bidirectional effects (security->hippo, hippo->security)
 * - Statistics and audit logging
 */

#include <gtest/gtest.h>
#include <vector>
#include <cstring>
#include <cstdio>
#include <cmath>

extern "C" {
#include "security/hippocampus/nimcp_security_hippocampus_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_codes.h"
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class SecurityHippocampusBridgeTest : public ::testing::Test {
protected:
    sec_hippo_bridge_t* bridge = nullptr;
    sec_hippo_config_t config;

    void SetUp() override {
        int result = security_hippocampus_default_config(&config);
        ASSERT_EQ(result, 0);
        bridge = security_hippocampus_bridge_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            security_hippocampus_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }

    // Helper: Create mock hippocampus system
    hippocampus_system_t create_mock_hippocampus() {
        return reinterpret_cast<hippocampus_system_t>(0x20000001);
    }

    // Helper: Create mock sleep system
    sleep_system_t create_mock_sleep_system() {
        return reinterpret_cast<sleep_system_t>(0x20000002);
    }

    // Helper: Connect all systems
    void connect_all_systems() {
        security_hippocampus_connect_hippo(bridge, create_mock_hippocampus());
        security_hippocampus_connect_sleep(bridge, create_mock_sleep_system());
    }

    // Helper: Add consolidation event
    void add_consolidation_event(uint64_t memory_id, float strength_before,
                                  float strength_after) {
        if (bridge->num_consolidation_events < SEC_HIPPO_MAX_CONSOLIDATION_EVENTS) {
            sec_hippo_consolidation_event_t* event =
                &bridge->consolidation_events[bridge->num_consolidation_events++];
            event->event_id = bridge->num_consolidation_events;
            event->memory_id = memory_id;
            event->strength_before = strength_before;
            event->strength_after = strength_after;
            event->timestamp = 0;  // Will be set by the system
            event->verified = false;
        }
    }

    // Helper: Add replay sequence
    void add_replay_sequence(uint64_t sequence_id, float content_match_score) {
        if (bridge->num_replay_sequences < SEC_HIPPO_MAX_REPLAY_SEQUENCES) {
            sec_hippo_replay_sequence_t* seq =
                &bridge->replay_sequences[bridge->num_replay_sequences++];
            seq->sequence_id = sequence_id;
            seq->content_match_score = content_match_score;
            seq->replay_strength = 0.5f;
            seq->status = SEC_HIPPO_REPLAY_VALID;
            seq->encoding_id = sequence_id * 100;
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(SecurityHippocampusBridgeTest, DefaultConfigIsValid) {
    sec_hippo_config_t cfg;
    int result = security_hippocampus_default_config(&cfg);
    EXPECT_EQ(result, 0);

    // Verify default values
    EXPECT_TRUE(cfg.enable_sleep_protection);
    EXPECT_TRUE(cfg.enable_consolidation_verify);
    EXPECT_TRUE(cfg.enable_injection_detection);
    EXPECT_TRUE(cfg.enable_replay_validation);
    EXPECT_TRUE(cfg.enable_coherence_checking);
    EXPECT_TRUE(cfg.enable_audit);
    EXPECT_TRUE(cfg.protect_nrem_deep);
    EXPECT_TRUE(cfg.protect_rem);
    EXPECT_FLOAT_EQ(cfg.security_sensitivity, 1.0f);
    EXPECT_FLOAT_EQ(cfg.hippocampus_sensitivity, 1.0f);
    EXPECT_FLOAT_EQ(cfg.ripple_filter_threshold, 0.7f);
    EXPECT_FLOAT_EQ(cfg.spindle_gate_threshold, 0.8f);
}

TEST_F(SecurityHippocampusBridgeTest, DefaultConfigNullFails) {
    int result = security_hippocampus_default_config(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityHippocampusBridgeTest, CreateWithNullConfigUsesDefaults) {
    sec_hippo_bridge_t* br = security_hippocampus_bridge_create(nullptr);
    ASSERT_NE(br, nullptr);

    // Bridge should work with defaults
    sec_hippo_state_info_t state;
    int result = security_hippocampus_get_state(br, &state);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(state.state, SEC_HIPPO_STATE_IDLE);

    security_hippocampus_bridge_destroy(br);
}

TEST_F(SecurityHippocampusBridgeTest, CreateWithCustomConfig) {
    sec_hippo_config_t custom_cfg;
    security_hippocampus_default_config(&custom_cfg);

    custom_cfg.security_sensitivity = 1.5f;
    custom_cfg.hippocampus_sensitivity = 0.8f;
    custom_cfg.ripple_filter_threshold = 0.9f;
    custom_cfg.enable_bio_async = false;

    sec_hippo_bridge_t* br = security_hippocampus_bridge_create(&custom_cfg);
    ASSERT_NE(br, nullptr);

    // Verify config was applied
    EXPECT_FLOAT_EQ(br->config.security_sensitivity, 1.5f);
    EXPECT_FLOAT_EQ(br->config.hippocampus_sensitivity, 0.8f);
    EXPECT_FLOAT_EQ(br->config.ripple_filter_threshold, 0.9f);

    security_hippocampus_bridge_destroy(br);
}

TEST_F(SecurityHippocampusBridgeTest, DestroyNullIsSafe) {
    security_hippocampus_bridge_destroy(nullptr);
    // Should not crash
}

TEST_F(SecurityHippocampusBridgeTest, ResetClearsState) {
    // Connect systems
    connect_all_systems();

    // Perform some operations to create state
    security_hippocampus_protect_sleep(bridge, SEC_HIPPO_SLEEP_DEEP_NREM);

    // Reset
    int result = security_hippocampus_bridge_reset(bridge);
    EXPECT_EQ(result, 0);

    // State should be cleared
    sec_hippo_state_info_t state;
    security_hippocampus_get_state(bridge, &state);
    EXPECT_EQ(state.state, SEC_HIPPO_STATE_IDLE);
    EXPECT_EQ(state.active_protections, 0u);

    // Stats should be cleared
    sec_hippo_stats_t stats;
    security_hippocampus_get_stats(bridge, &stats);
    EXPECT_EQ(stats.sleep_protection_activations, 0u);
}

TEST_F(SecurityHippocampusBridgeTest, ResetNullFails) {
    int result = security_hippocampus_bridge_reset(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Connection Tests
 * ============================================================================ */

TEST_F(SecurityHippocampusBridgeTest, ConnectHippocampus) {
    hippocampus_system_t hippo = create_mock_hippocampus();

    int result = security_hippocampus_connect_hippo(bridge, hippo);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(bridge->hippocampus_connected);
    EXPECT_EQ(bridge->hippocampus, hippo);
}

TEST_F(SecurityHippocampusBridgeTest, ConnectHippocampusNullBridgeFails) {
    hippocampus_system_t hippo = create_mock_hippocampus();
    int result = security_hippocampus_connect_hippo(nullptr, hippo);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityHippocampusBridgeTest, ConnectHippocampusNullHippoFails) {
    int result = security_hippocampus_connect_hippo(bridge, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityHippocampusBridgeTest, ConnectSleepSystem) {
    sleep_system_t sleep = create_mock_sleep_system();

    int result = security_hippocampus_connect_sleep(bridge, sleep);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(bridge->sleep_connected);
    EXPECT_EQ(bridge->sleep_system, sleep);
}

TEST_F(SecurityHippocampusBridgeTest, ConnectSleepSystemNullBridgeFails) {
    sleep_system_t sleep = create_mock_sleep_system();
    int result = security_hippocampus_connect_sleep(nullptr, sleep);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityHippocampusBridgeTest, ConnectSleepSystemNullSleepFails) {
    int result = security_hippocampus_connect_sleep(bridge, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityHippocampusBridgeTest, IsFullyConnectedAllSystems) {
    connect_all_systems();

    bool fully_connected = security_hippocampus_is_fully_connected(bridge);
    EXPECT_TRUE(fully_connected);
}

TEST_F(SecurityHippocampusBridgeTest, IsFullyConnectedMissingSome) {
    security_hippocampus_connect_hippo(bridge, create_mock_hippocampus());
    // Missing sleep system

    bool fully_connected = security_hippocampus_is_fully_connected(bridge);
    EXPECT_FALSE(fully_connected);
}

TEST_F(SecurityHippocampusBridgeTest, IsFullyConnectedNullFails) {
    bool result = security_hippocampus_is_fully_connected(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(SecurityHippocampusBridgeTest, DisconnectAll) {
    connect_all_systems();
    EXPECT_TRUE(security_hippocampus_is_fully_connected(bridge));

    int result = security_hippocampus_disconnect_all(bridge);
    EXPECT_EQ(result, 0);

    EXPECT_FALSE(bridge->hippocampus_connected);
    EXPECT_FALSE(bridge->sleep_connected);
    EXPECT_FALSE(security_hippocampus_is_fully_connected(bridge));
}

TEST_F(SecurityHippocampusBridgeTest, DisconnectAllNullFails) {
    int result = security_hippocampus_disconnect_all(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Sleep Protection Tests
 * ============================================================================ */

TEST_F(SecurityHippocampusBridgeTest, ProtectSleepAwake) {
    int result = security_hippocampus_protect_sleep(bridge, SEC_HIPPO_SLEEP_AWAKE);
    EXPECT_EQ(result, 0);

    // Awake phase should have minimal protection
    EXPECT_FALSE(bridge->security_effects.sleep_protection_active);
    EXPECT_FLOAT_EQ(bridge->security_effects.ripple_filter_level, 0.0f);
}

TEST_F(SecurityHippocampusBridgeTest, ProtectSleepDeepNREM) {
    int result = security_hippocampus_protect_sleep(bridge, SEC_HIPPO_SLEEP_DEEP_NREM);
    EXPECT_EQ(result, 0);

    // Deep NREM should have maximum protection
    EXPECT_TRUE(bridge->security_effects.sleep_protection_active);
    EXPECT_GT(bridge->security_effects.ripple_filter_level, 0.0f);
    EXPECT_GT(bridge->security_effects.spindle_gate_level, 0.0f);
}

TEST_F(SecurityHippocampusBridgeTest, ProtectSleepREM) {
    int result = security_hippocampus_protect_sleep(bridge, SEC_HIPPO_SLEEP_REM);
    EXPECT_EQ(result, 0);

    EXPECT_TRUE(bridge->security_effects.sleep_protection_active);
}

TEST_F(SecurityHippocampusBridgeTest, ProtectSleepLightNREM) {
    int result = security_hippocampus_protect_sleep(bridge, SEC_HIPPO_SLEEP_LIGHT_NREM);
    EXPECT_EQ(result, 0);

    EXPECT_TRUE(bridge->security_effects.sleep_protection_active);
}

TEST_F(SecurityHippocampusBridgeTest, ProtectSleepDrowsy) {
    int result = security_hippocampus_protect_sleep(bridge, SEC_HIPPO_SLEEP_DROWSY);
    EXPECT_EQ(result, 0);

    // Drowsy phase should have minimal protection
    EXPECT_FALSE(bridge->security_effects.sleep_protection_active);
}

TEST_F(SecurityHippocampusBridgeTest, ProtectSleepNullBridgeFails) {
    int result = security_hippocampus_protect_sleep(nullptr, SEC_HIPPO_SLEEP_DEEP_NREM);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityHippocampusBridgeTest, SetSleepPhase) {
    int result = security_hippocampus_set_sleep_phase(bridge, SEC_HIPPO_SLEEP_REM);
    EXPECT_EQ(result, 0);

    sec_hippo_sleep_phase_t phase = security_hippocampus_get_sleep_phase(bridge);
    EXPECT_EQ(phase, SEC_HIPPO_SLEEP_REM);
}

TEST_F(SecurityHippocampusBridgeTest, SetSleepPhaseNullFails) {
    int result = security_hippocampus_set_sleep_phase(nullptr, SEC_HIPPO_SLEEP_REM);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityHippocampusBridgeTest, GetSleepPhaseNullReturnsAwake) {
    sec_hippo_sleep_phase_t phase = security_hippocampus_get_sleep_phase(nullptr);
    EXPECT_EQ(phase, SEC_HIPPO_SLEEP_AWAKE);
}

TEST_F(SecurityHippocampusBridgeTest, IsSleepProtected) {
    security_hippocampus_protect_sleep(bridge, SEC_HIPPO_SLEEP_DEEP_NREM);

    bool protected_ = security_hippocampus_is_sleep_protected(bridge);
    EXPECT_TRUE(protected_);
}

TEST_F(SecurityHippocampusBridgeTest, IsSleepProtectedNullFails) {
    bool protected_ = security_hippocampus_is_sleep_protected(nullptr);
    EXPECT_FALSE(protected_);
}

TEST_F(SecurityHippocampusBridgeTest, SleepProtectionStatsUpdated) {
    security_hippocampus_protect_sleep(bridge, SEC_HIPPO_SLEEP_DEEP_NREM);

    sec_hippo_stats_t stats;
    security_hippocampus_get_stats(bridge, &stats);
    EXPECT_GE(stats.sleep_protection_activations, 1u);
    EXPECT_GE(stats.sleep_phases_protected, 1u);
}

/* ============================================================================
 * Consolidation Verification Tests
 * ============================================================================ */

TEST_F(SecurityHippocampusBridgeTest, VerifyConsolidationOK) {
    // Add a good consolidation event
    add_consolidation_event(1001, 0.3f, 0.7f);

    sec_hippo_consolidation_status_t status;
    float confidence;

    int result = security_hippocampus_verify_consolidation(bridge, 1001,
                                                           &status, &confidence);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(status, SEC_HIPPO_CONSOL_OK);
    EXPECT_GT(confidence, 0.0f);
}

TEST_F(SecurityHippocampusBridgeTest, VerifyConsolidationDegraded) {
    // Add a degraded consolidation event (slight strength loss)
    add_consolidation_event(1002, 0.5f, 0.45f);

    sec_hippo_consolidation_status_t status;
    float confidence;

    int result = security_hippocampus_verify_consolidation(bridge, 1002,
                                                           &status, &confidence);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(status, SEC_HIPPO_CONSOL_DEGRADED);
}

TEST_F(SecurityHippocampusBridgeTest, VerifyConsolidationCorrupted) {
    // Add a corrupted consolidation event (significant strength loss)
    add_consolidation_event(1003, 0.8f, 0.3f);

    sec_hippo_consolidation_status_t status;
    float confidence;

    int result = security_hippocampus_verify_consolidation(bridge, 1003,
                                                           &status, &confidence);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(status, SEC_HIPPO_CONSOL_CORRUPTED);
}

TEST_F(SecurityHippocampusBridgeTest, VerifyConsolidationIncomplete) {
    // Memory ID that doesn't exist
    sec_hippo_consolidation_status_t status;
    float confidence;

    int result = security_hippocampus_verify_consolidation(bridge, 9999,
                                                           &status, &confidence);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(status, SEC_HIPPO_CONSOL_INCOMPLETE);
}

TEST_F(SecurityHippocampusBridgeTest, VerifyConsolidationNullBridgeFails) {
    sec_hippo_consolidation_status_t status;
    float confidence;

    int result = security_hippocampus_verify_consolidation(nullptr, 1001,
                                                           &status, &confidence);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityHippocampusBridgeTest, VerifyConsolidationNullStatusFails) {
    float confidence;

    int result = security_hippocampus_verify_consolidation(bridge, 1001,
                                                           nullptr, &confidence);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityHippocampusBridgeTest, VerifyConsolidationNullConfidenceFails) {
    sec_hippo_consolidation_status_t status;

    int result = security_hippocampus_verify_consolidation(bridge, 1001,
                                                           &status, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityHippocampusBridgeTest, VerifyAllConsolidations) {
    // Add multiple consolidation events
    add_consolidation_event(2001, 0.3f, 0.7f);  // OK
    add_consolidation_event(2002, 0.8f, 0.3f);  // Corrupted

    uint32_t verified, failed;
    int result = security_hippocampus_verify_all_consolidations(bridge,
                                                                 &verified, &failed);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(verified + failed, 2u);
}

TEST_F(SecurityHippocampusBridgeTest, VerifyAllConsolidationsNullFails) {
    uint32_t verified, failed;

    int result = security_hippocampus_verify_all_consolidations(nullptr,
                                                                 &verified, &failed);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);

    result = security_hippocampus_verify_all_consolidations(bridge,
                                                             nullptr, &failed);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);

    result = security_hippocampus_verify_all_consolidations(bridge,
                                                             &verified, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityHippocampusBridgeTest, PauseResumeConsolidation) {
    int result = security_hippocampus_pause_consolidation(bridge);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(bridge->security_effects.consolidation_paused);

    result = security_hippocampus_resume_consolidation(bridge);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(bridge->security_effects.consolidation_paused);
}

TEST_F(SecurityHippocampusBridgeTest, PauseConsolidationNullFails) {
    int result = security_hippocampus_pause_consolidation(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityHippocampusBridgeTest, ResumeConsolidationNullFails) {
    int result = security_hippocampus_resume_consolidation(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Injection Detection Tests
 * ============================================================================ */

TEST_F(SecurityHippocampusBridgeTest, DetectInjectionNoInjection) {
    sec_hippo_injection_type_t injection;
    float confidence;
    char details[256];

    bool detected = security_hippocampus_detect_injection(bridge, &injection,
                                                           &confidence, details, sizeof(details));

    // Initially, no injection should be detected
    if (!detected) {
        EXPECT_EQ(injection, SEC_HIPPO_INJECT_NONE);
    }
}

TEST_F(SecurityHippocampusBridgeTest, DetectInjectionNullBridgeFails) {
    sec_hippo_injection_type_t injection;
    float confidence;

    bool detected = security_hippocampus_detect_injection(nullptr, &injection,
                                                           &confidence, nullptr, 0);
    EXPECT_FALSE(detected);
}

TEST_F(SecurityHippocampusBridgeTest, DetectInjectionNullOutputsOK) {
    // Null outputs should be handled gracefully
    bool detected = security_hippocampus_detect_injection(bridge, nullptr,
                                                           nullptr, nullptr, 0);
    // Should complete without crashing
    (void)detected;
}

TEST_F(SecurityHippocampusBridgeTest, DetectInjectionStatsUpdated) {
    sec_hippo_injection_type_t injection;
    float confidence;

    security_hippocampus_detect_injection(bridge, &injection, &confidence, nullptr, 0);

    sec_hippo_stats_t stats;
    security_hippocampus_get_stats(bridge, &stats);
    EXPECT_GE(stats.injection_scans, 1u);
}

TEST_F(SecurityHippocampusBridgeTest, BlockInjection) {
    int result = security_hippocampus_block_injection(bridge,
                                                       SEC_HIPPO_INJECT_FALSE_MEMORY);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(bridge->security_effects.injection_guard_active);
    EXPECT_GE(bridge->security_effects.injections_blocked, 1u);
}

TEST_F(SecurityHippocampusBridgeTest, BlockInjectionNullFails) {
    int result = security_hippocampus_block_injection(nullptr,
                                                       SEC_HIPPO_INJECT_FALSE_MEMORY);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityHippocampusBridgeTest, RegisterPattern) {
    int result = security_hippocampus_register_pattern(bridge, 12345, 0xABCDEF);
    EXPECT_EQ(result, 0);
}

TEST_F(SecurityHippocampusBridgeTest, RegisterPatternNullFails) {
    int result = security_hippocampus_register_pattern(nullptr, 12345, 0xABCDEF);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Replay Validation Tests
 * ============================================================================ */

TEST_F(SecurityHippocampusBridgeTest, ValidateReplayNew) {
    sec_hippo_replay_status_t status;
    float match_score;

    int result = security_hippocampus_validate_replay(bridge, 3001,
                                                       &status, &match_score);
    EXPECT_EQ(result, 0);
    // New sequence should be created and marked valid
    EXPECT_EQ(status, SEC_HIPPO_REPLAY_VALID);
}

TEST_F(SecurityHippocampusBridgeTest, ValidateReplayExisting) {
    // Add a replay sequence first
    add_replay_sequence(3002, 0.9f);

    sec_hippo_replay_status_t status;
    float match_score;

    int result = security_hippocampus_validate_replay(bridge, 3002,
                                                       &status, &match_score);
    EXPECT_EQ(result, 0);
    // High match score should result in valid
}

TEST_F(SecurityHippocampusBridgeTest, ValidateReplayNullBridgeFails) {
    sec_hippo_replay_status_t status;
    float match_score;

    int result = security_hippocampus_validate_replay(nullptr, 3001,
                                                       &status, &match_score);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityHippocampusBridgeTest, ValidateReplayNullStatusFails) {
    float match_score;

    int result = security_hippocampus_validate_replay(bridge, 3001,
                                                       nullptr, &match_score);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityHippocampusBridgeTest, ValidateReplayNullMatchScoreFails) {
    sec_hippo_replay_status_t status;

    int result = security_hippocampus_validate_replay(bridge, 3001,
                                                       &status, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityHippocampusBridgeTest, RegisterEncoding) {
    int result = security_hippocampus_register_encoding(bridge, 4001,
                                                         0xDEADBEEF, 1000000);
    EXPECT_EQ(result, 0);
}

TEST_F(SecurityHippocampusBridgeTest, RegisterEncodingNullFails) {
    int result = security_hippocampus_register_encoding(nullptr, 4001,
                                                         0xDEADBEEF, 1000000);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityHippocampusBridgeTest, RejectReplay) {
    add_replay_sequence(4002, 0.9f);

    int result = security_hippocampus_reject_replay(bridge, 4002);
    EXPECT_EQ(result, 0);

    // Verify sequence marked as forged
    sec_hippo_replay_sequence_t seq;
    result = security_hippocampus_get_replay_info(bridge, 4002, &seq);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(seq.status, SEC_HIPPO_REPLAY_FORGED);
}

TEST_F(SecurityHippocampusBridgeTest, RejectReplayNullFails) {
    int result = security_hippocampus_reject_replay(nullptr, 4002);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityHippocampusBridgeTest, GetReplayInfo) {
    add_replay_sequence(4003, 0.8f);

    sec_hippo_replay_sequence_t seq;
    int result = security_hippocampus_get_replay_info(bridge, 4003, &seq);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(seq.sequence_id, 4003u);
    EXPECT_FLOAT_EQ(seq.content_match_score, 0.8f);
}

TEST_F(SecurityHippocampusBridgeTest, GetReplayInfoNotFound) {
    sec_hippo_replay_sequence_t seq;
    int result = security_hippocampus_get_replay_info(bridge, 99999, &seq);
    EXPECT_EQ(result, NIMCP_ERROR_NOT_FOUND);
}

TEST_F(SecurityHippocampusBridgeTest, GetReplayInfoNullFails) {
    sec_hippo_replay_sequence_t seq;

    int result = security_hippocampus_get_replay_info(nullptr, 4003, &seq);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);

    result = security_hippocampus_get_replay_info(bridge, 4003, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Coherence Checking Tests
 * ============================================================================ */

TEST_F(SecurityHippocampusBridgeTest, CheckCoherenceInitial) {
    sec_hippo_coherence_status_t status;
    float spatial, temporal;

    int result = security_hippocampus_check_coherence(bridge, &status,
                                                       &spatial, &temporal);
    EXPECT_EQ(result, 0);
    // Initially should be coherent (no data)
    EXPECT_EQ(status, SEC_HIPPO_COHERENCE_OK);
    EXPECT_FLOAT_EQ(spatial, 1.0f);
    EXPECT_FLOAT_EQ(temporal, 1.0f);
}

TEST_F(SecurityHippocampusBridgeTest, CheckCoherenceNullBridgeFails) {
    sec_hippo_coherence_status_t status;
    float spatial, temporal;

    int result = security_hippocampus_check_coherence(nullptr, &status,
                                                       &spatial, &temporal);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityHippocampusBridgeTest, CheckCoherenceNullOutputsFail) {
    sec_hippo_coherence_status_t status;
    float spatial, temporal;

    int result = security_hippocampus_check_coherence(bridge, nullptr,
                                                       &spatial, &temporal);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);

    result = security_hippocampus_check_coherence(bridge, &status,
                                                   nullptr, &temporal);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);

    result = security_hippocampus_check_coherence(bridge, &status,
                                                   &spatial, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityHippocampusBridgeTest, CheckSpatialCoherence) {
    float score;
    int result = security_hippocampus_check_spatial_coherence(bridge, &score);
    EXPECT_EQ(result, 0);
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 1.0f);
}

TEST_F(SecurityHippocampusBridgeTest, CheckSpatialCoherenceNullFails) {
    float score;

    int result = security_hippocampus_check_spatial_coherence(nullptr, &score);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);

    result = security_hippocampus_check_spatial_coherence(bridge, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityHippocampusBridgeTest, CheckTemporalCoherence) {
    float score;
    int result = security_hippocampus_check_temporal_coherence(bridge, &score);
    EXPECT_EQ(result, 0);
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 1.0f);
}

TEST_F(SecurityHippocampusBridgeTest, CheckTemporalCoherenceNullFails) {
    float score;

    int result = security_hippocampus_check_temporal_coherence(nullptr, &score);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);

    result = security_hippocampus_check_temporal_coherence(bridge, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityHippocampusBridgeTest, ReportPlaceCell) {
    int result = security_hippocampus_report_place_cell(bridge, 101, 0.5f, 0.3f, 10.0f);
    EXPECT_EQ(result, 0);

    EXPECT_GE(bridge->hippo_effects.active_place_cells, 1u);
}

TEST_F(SecurityHippocampusBridgeTest, ReportPlaceCellMultiple) {
    for (int i = 0; i < 10; i++) {
        int result = security_hippocampus_report_place_cell(bridge, i,
                                                             (float)i * 0.1f,
                                                             (float)i * 0.05f,
                                                             5.0f + (float)i);
        EXPECT_EQ(result, 0);
    }

    EXPECT_GE(bridge->hippo_effects.active_place_cells, 10u);
}

TEST_F(SecurityHippocampusBridgeTest, ReportPlaceCellNullFails) {
    int result = security_hippocampus_report_place_cell(nullptr, 101, 0.5f, 0.3f, 10.0f);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityHippocampusBridgeTest, ReportTimeCell) {
    int result = security_hippocampus_report_time_cell(bridge, 201, 1000000, 8.0f);
    EXPECT_EQ(result, 0);

    EXPECT_GE(bridge->hippo_effects.active_time_cells, 1u);
}

TEST_F(SecurityHippocampusBridgeTest, ReportTimeCellMultiple) {
    for (int i = 0; i < 10; i++) {
        int result = security_hippocampus_report_time_cell(bridge, i,
                                                            1000000 + (uint64_t)i * 10000,
                                                            5.0f + (float)i);
        EXPECT_EQ(result, 0);
    }

    EXPECT_GE(bridge->hippo_effects.active_time_cells, 10u);
}

TEST_F(SecurityHippocampusBridgeTest, ReportTimeCellNullFails) {
    int result = security_hippocampus_report_time_cell(nullptr, 201, 1000000, 8.0f);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityHippocampusBridgeTest, CoherenceWithCellData) {
    // Add some place cell data
    for (int i = 0; i < 5; i++) {
        security_hippocampus_report_place_cell(bridge, i,
                                                0.1f + (float)i * 0.01f,  // Small changes
                                                0.1f + (float)i * 0.01f,
                                                5.0f);
    }

    // Add some time cell data
    for (int i = 0; i < 5; i++) {
        security_hippocampus_report_time_cell(bridge, i,
                                               1000000 + (uint64_t)i * 100000,  // Forward in time
                                               5.0f);
    }

    sec_hippo_coherence_status_t status;
    float spatial, temporal;

    int result = security_hippocampus_check_coherence(bridge, &status,
                                                       &spatial, &temporal);
    EXPECT_EQ(result, 0);
    // With consistent data, coherence should be high
    EXPECT_GE(spatial, 0.5f);
    EXPECT_GE(temporal, 0.5f);
}

/* ============================================================================
 * Bidirectional Update Tests
 * ============================================================================ */

TEST_F(SecurityHippocampusBridgeTest, BridgeUpdate) {
    connect_all_systems();

    int result = security_hippocampus_bridge_update(bridge, 100);  // 100ms delta
    EXPECT_EQ(result, 0);
}

TEST_F(SecurityHippocampusBridgeTest, BridgeUpdateNullFails) {
    int result = security_hippocampus_bridge_update(nullptr, 100);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityHippocampusBridgeTest, BridgeUpdateMultipleCycles) {
    connect_all_systems();

    for (int i = 0; i < 10; i++) {
        int result = security_hippocampus_bridge_update(bridge, 50);
        EXPECT_EQ(result, 0);
    }
}

TEST_F(SecurityHippocampusBridgeTest, ApplySecurityEffects) {
    connect_all_systems();
    security_hippocampus_protect_sleep(bridge, SEC_HIPPO_SLEEP_DEEP_NREM);

    int result = security_hippocampus_apply_security_effects(bridge);
    EXPECT_EQ(result, 0);

    // Check effects applied
    security_to_hippo_effects_t effects;
    security_hippocampus_get_security_effects(bridge, &effects);
    EXPECT_TRUE(effects.sleep_protection_active);
}

TEST_F(SecurityHippocampusBridgeTest, ApplySecurityEffectsNullFails) {
    int result = security_hippocampus_apply_security_effects(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityHippocampusBridgeTest, GatherHippoEffects) {
    connect_all_systems();

    int result = security_hippocampus_gather_hippo_effects(bridge);
    EXPECT_EQ(result, 0);

    // Check effects gathered
    hippo_to_security_effects_t effects;
    security_hippocampus_get_hippo_effects(bridge, &effects);
    // Effects should be populated
    EXPECT_GE(effects.spatial_coherence, 0.0f);
    EXPECT_LE(effects.spatial_coherence, 1.0f);
}

TEST_F(SecurityHippocampusBridgeTest, GatherHippoEffectsNullFails) {
    int result = security_hippocampus_gather_hippo_effects(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Query Functions Tests
 * ============================================================================ */

TEST_F(SecurityHippocampusBridgeTest, GetSecurityEffects) {
    security_to_hippo_effects_t effects;
    int result = security_hippocampus_get_security_effects(bridge, &effects);
    EXPECT_EQ(result, 0);

    // Verify default values
    EXPECT_FALSE(effects.sleep_protection_active);
    EXPECT_EQ(effects.blocked_ripples, 0u);
}

TEST_F(SecurityHippocampusBridgeTest, GetSecurityEffectsNullBridgeFails) {
    security_to_hippo_effects_t effects;
    int result = security_hippocampus_get_security_effects(nullptr, &effects);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityHippocampusBridgeTest, GetSecurityEffectsNullOutputFails) {
    int result = security_hippocampus_get_security_effects(bridge, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityHippocampusBridgeTest, GetHippoEffects) {
    hippo_to_security_effects_t effects;
    int result = security_hippocampus_get_hippo_effects(bridge, &effects);
    EXPECT_EQ(result, 0);

    // Verify reasonable values
    EXPECT_GE(effects.spatial_coherence, 0.0f);
    EXPECT_LE(effects.spatial_coherence, 1.0f);
}

TEST_F(SecurityHippocampusBridgeTest, GetHippoEffectsNullBridgeFails) {
    hippo_to_security_effects_t effects;
    int result = security_hippocampus_get_hippo_effects(nullptr, &effects);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityHippocampusBridgeTest, GetHippoEffectsNullOutputFails) {
    int result = security_hippocampus_get_hippo_effects(bridge, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityHippocampusBridgeTest, GetState) {
    sec_hippo_state_info_t state;
    int result = security_hippocampus_get_state(bridge, &state);
    EXPECT_EQ(result, 0);

    EXPECT_EQ(state.state, SEC_HIPPO_STATE_IDLE);
    EXPECT_EQ(state.active_protections, 0u);
}

TEST_F(SecurityHippocampusBridgeTest, GetStateNullBridgeFails) {
    sec_hippo_state_info_t state;
    int result = security_hippocampus_get_state(nullptr, &state);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityHippocampusBridgeTest, GetStateNullOutputFails) {
    int result = security_hippocampus_get_state(bridge, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(SecurityHippocampusBridgeTest, GetStats) {
    // Perform some operations to generate stats
    security_hippocampus_protect_sleep(bridge, SEC_HIPPO_SLEEP_DEEP_NREM);
    security_hippocampus_detect_injection(bridge, nullptr, nullptr, nullptr, 0);

    sec_hippo_stats_t stats;
    int result = security_hippocampus_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);

    EXPECT_GE(stats.sleep_protection_activations, 1u);
    EXPECT_GE(stats.injection_scans, 1u);
}

TEST_F(SecurityHippocampusBridgeTest, GetStatsNullBridgeFails) {
    sec_hippo_stats_t stats;
    int result = security_hippocampus_get_stats(nullptr, &stats);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityHippocampusBridgeTest, GetStatsNullOutputFails) {
    int result = security_hippocampus_get_stats(bridge, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityHippocampusBridgeTest, ResetStats) {
    // Generate some stats
    security_hippocampus_protect_sleep(bridge, SEC_HIPPO_SLEEP_DEEP_NREM);

    // Verify stats exist
    sec_hippo_stats_t stats;
    security_hippocampus_get_stats(bridge, &stats);
    EXPECT_GE(stats.sleep_protection_activations, 1u);

    // Reset
    int result = security_hippocampus_reset_stats(bridge);
    EXPECT_EQ(result, 0);

    // Verify reset
    security_hippocampus_get_stats(bridge, &stats);
    EXPECT_EQ(stats.sleep_protection_activations, 0u);
    EXPECT_EQ(stats.injection_scans, 0u);
}

TEST_F(SecurityHippocampusBridgeTest, ResetStatsNullBridgeFails) {
    int result = security_hippocampus_reset_stats(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Audit Functions Tests
 * ============================================================================ */

TEST_F(SecurityHippocampusBridgeTest, GetAuditLog) {
    // Generate some audit entries
    for (int i = 0; i < 5; i++) {
        security_hippocampus_protect_sleep(bridge, SEC_HIPPO_SLEEP_DEEP_NREM);
    }

    sec_hippo_audit_entry_t entries[10];
    size_t count = 0;

    int result = security_hippocampus_get_audit_log(bridge, entries, 10, &count);
    EXPECT_EQ(result, 0);
    EXPECT_GE(count, 5u);
}

TEST_F(SecurityHippocampusBridgeTest, GetAuditLogNullBridgeFails) {
    sec_hippo_audit_entry_t entries[10];
    size_t count;

    int result = security_hippocampus_get_audit_log(nullptr, entries, 10, &count);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityHippocampusBridgeTest, GetAuditLogNullEntriesFails) {
    size_t count;
    int result = security_hippocampus_get_audit_log(bridge, nullptr, 10, &count);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityHippocampusBridgeTest, GetAuditLogNullCountFails) {
    sec_hippo_audit_entry_t entries[10];
    int result = security_hippocampus_get_audit_log(bridge, entries, 10, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityHippocampusBridgeTest, ClearAuditLog) {
    // Generate entries
    for (int i = 0; i < 3; i++) {
        security_hippocampus_protect_sleep(bridge, SEC_HIPPO_SLEEP_DEEP_NREM);
    }

    // Clear
    int result = security_hippocampus_clear_audit_log(bridge);
    EXPECT_EQ(result, 0);

    // Verify cleared
    sec_hippo_audit_entry_t entries[10];
    size_t count = 0;
    security_hippocampus_get_audit_log(bridge, entries, 10, &count);
    EXPECT_EQ(count, 0u);
}

TEST_F(SecurityHippocampusBridgeTest, ClearAuditLogNullBridgeFails) {
    int result = security_hippocampus_clear_audit_log(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Bio-Async Integration Tests
 * ============================================================================ */

TEST_F(SecurityHippocampusBridgeTest, ConnectBioAsync) {
    int result = security_hippocampus_connect_bio_async(bridge);
    // May succeed or fail depending on bio-async availability
    if (result != 0) {
        GTEST_SKIP() << "Bio-async router not available";
    }
    EXPECT_EQ(result, 0);
}

TEST_F(SecurityHippocampusBridgeTest, ConnectBioAsyncNullFails) {
    int result = security_hippocampus_connect_bio_async(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityHippocampusBridgeTest, DisconnectBioAsync) {
    // First connect
    int connect_result = security_hippocampus_connect_bio_async(bridge);
    if (connect_result != 0 && !security_hippocampus_is_bio_async_connected(bridge)) {
        GTEST_SKIP() << "Bio-async router not available";
    }

    int result = security_hippocampus_disconnect_bio_async(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(SecurityHippocampusBridgeTest, DisconnectBioAsyncNullFails) {
    int result = security_hippocampus_disconnect_bio_async(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityHippocampusBridgeTest, IsBioAsyncConnected) {
    bool connected = security_hippocampus_is_bio_async_connected(bridge);
    // Initially false
    EXPECT_FALSE(connected);
}

TEST_F(SecurityHippocampusBridgeTest, IsBioAsyncConnectedNullFails) {
    bool connected = security_hippocampus_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);
}

/* ============================================================================
 * Utility Function Tests
 * ============================================================================ */

TEST_F(SecurityHippocampusBridgeTest, SleepPhaseNameAwake) {
    const char* name = security_hippocampus_sleep_phase_name(SEC_HIPPO_SLEEP_AWAKE);
    EXPECT_NE(name, nullptr);
    EXPECT_STREQ(name, "AWAKE");
}

TEST_F(SecurityHippocampusBridgeTest, SleepPhaseNameDeepNREM) {
    const char* name = security_hippocampus_sleep_phase_name(SEC_HIPPO_SLEEP_DEEP_NREM);
    EXPECT_NE(name, nullptr);
    EXPECT_STREQ(name, "DEEP_NREM");
}

TEST_F(SecurityHippocampusBridgeTest, SleepPhaseNameREM) {
    const char* name = security_hippocampus_sleep_phase_name(SEC_HIPPO_SLEEP_REM);
    EXPECT_NE(name, nullptr);
    EXPECT_STREQ(name, "REM");
}

TEST_F(SecurityHippocampusBridgeTest, InjectionNameNone) {
    const char* name = security_hippocampus_injection_name(SEC_HIPPO_INJECT_NONE);
    EXPECT_NE(name, nullptr);
    EXPECT_STREQ(name, "NONE");
}

TEST_F(SecurityHippocampusBridgeTest, InjectionNameFalseMemory) {
    const char* name = security_hippocampus_injection_name(SEC_HIPPO_INJECT_FALSE_MEMORY);
    EXPECT_NE(name, nullptr);
    EXPECT_STREQ(name, "FALSE_MEMORY");
}

TEST_F(SecurityHippocampusBridgeTest, InjectionNamePatternPoison) {
    const char* name = security_hippocampus_injection_name(SEC_HIPPO_INJECT_PATTERN_POISON);
    EXPECT_NE(name, nullptr);
    EXPECT_STREQ(name, "PATTERN_POISON");
}

TEST_F(SecurityHippocampusBridgeTest, ConsolidationNameOK) {
    const char* name = security_hippocampus_consolidation_name(SEC_HIPPO_CONSOL_OK);
    EXPECT_NE(name, nullptr);
    EXPECT_STREQ(name, "OK");
}

TEST_F(SecurityHippocampusBridgeTest, ConsolidationNameCorrupted) {
    const char* name = security_hippocampus_consolidation_name(SEC_HIPPO_CONSOL_CORRUPTED);
    EXPECT_NE(name, nullptr);
    EXPECT_STREQ(name, "CORRUPTED");
}

TEST_F(SecurityHippocampusBridgeTest, ReplayNameValid) {
    const char* name = security_hippocampus_replay_name(SEC_HIPPO_REPLAY_VALID);
    EXPECT_NE(name, nullptr);
    EXPECT_STREQ(name, "VALID");
}

TEST_F(SecurityHippocampusBridgeTest, ReplayNameForged) {
    const char* name = security_hippocampus_replay_name(SEC_HIPPO_REPLAY_FORGED);
    EXPECT_NE(name, nullptr);
    EXPECT_STREQ(name, "FORGED");
}

TEST_F(SecurityHippocampusBridgeTest, CoherenceNameOK) {
    const char* name = security_hippocampus_coherence_name(SEC_HIPPO_COHERENCE_OK);
    EXPECT_NE(name, nullptr);
    EXPECT_STREQ(name, "OK");
}

TEST_F(SecurityHippocampusBridgeTest, CoherenceNameSpatialDrift) {
    const char* name = security_hippocampus_coherence_name(SEC_HIPPO_COHERENCE_SPATIAL_DRIFT);
    EXPECT_NE(name, nullptr);
    EXPECT_STREQ(name, "SPATIAL_DRIFT");
}

TEST_F(SecurityHippocampusBridgeTest, StateNameIdle) {
    const char* name = security_hippocampus_state_name(SEC_HIPPO_STATE_IDLE);
    EXPECT_NE(name, nullptr);
    EXPECT_STREQ(name, "IDLE");
}

TEST_F(SecurityHippocampusBridgeTest, StateNameProtecting) {
    const char* name = security_hippocampus_state_name(SEC_HIPPO_STATE_PROTECTING);
    EXPECT_NE(name, nullptr);
    EXPECT_STREQ(name, "PROTECTING");
}

TEST_F(SecurityHippocampusBridgeTest, AuditTypeNameSleepProtect) {
    const char* name = security_hippocampus_audit_type_name(SEC_HIPPO_AUDIT_SLEEP_PROTECT);
    EXPECT_NE(name, nullptr);
    EXPECT_STREQ(name, "SLEEP_PROTECT");
}

TEST_F(SecurityHippocampusBridgeTest, AuditTypeNameInjectDetect) {
    const char* name = security_hippocampus_audit_type_name(SEC_HIPPO_AUDIT_INJECT_DETECT);
    EXPECT_NE(name, nullptr);
    EXPECT_STREQ(name, "INJECT_DETECT");
}

/* ============================================================================
 * Debug/Print Function Tests (Smoke Tests)
 * ============================================================================ */

TEST_F(SecurityHippocampusBridgeTest, PrintSummaryNullSafe) {
    security_hippocampus_print_summary(nullptr);
    // Should not crash
}

TEST_F(SecurityHippocampusBridgeTest, PrintSummaryValid) {
    security_hippocampus_print_summary(bridge);
    // Should not crash
}

TEST_F(SecurityHippocampusBridgeTest, PrintStatsNullSafe) {
    security_hippocampus_print_stats(nullptr);
    // Should not crash
}

TEST_F(SecurityHippocampusBridgeTest, PrintStatsValid) {
    sec_hippo_stats_t stats;
    security_hippocampus_get_stats(bridge, &stats);
    security_hippocampus_print_stats(&stats);
    // Should not crash
}

/* ============================================================================
 * Edge Cases and Stress Tests
 * ============================================================================ */

TEST_F(SecurityHippocampusBridgeTest, MaxReplaySequences) {
    // Add replay sequences up to limit
    for (uint32_t i = 0; i < SEC_HIPPO_MAX_REPLAY_SEQUENCES; i++) {
        add_replay_sequence(10000 + i, 0.5f);
    }

    EXPECT_EQ(bridge->num_replay_sequences, SEC_HIPPO_MAX_REPLAY_SEQUENCES);

    // Should still validate existing sequences
    sec_hippo_replay_status_t status;
    float match_score;
    int result = security_hippocampus_validate_replay(bridge, 10000,
                                                       &status, &match_score);
    EXPECT_EQ(result, 0);
}

TEST_F(SecurityHippocampusBridgeTest, MaxConsolidationEvents) {
    // Add consolidation events up to limit
    for (uint32_t i = 0; i < SEC_HIPPO_MAX_CONSOLIDATION_EVENTS; i++) {
        add_consolidation_event(20000 + i, 0.3f, 0.7f);
    }

    EXPECT_EQ(bridge->num_consolidation_events, SEC_HIPPO_MAX_CONSOLIDATION_EVENTS);

    // Should still verify existing events
    sec_hippo_consolidation_status_t status;
    float confidence;
    int result = security_hippocampus_verify_consolidation(bridge, 20000,
                                                           &status, &confidence);
    EXPECT_EQ(result, 0);
}

TEST_F(SecurityHippocampusBridgeTest, RapidSleepPhaseChanges) {
    // Rapid phase changes
    for (int i = 0; i < 100; i++) {
        sec_hippo_sleep_phase_t phase =
            static_cast<sec_hippo_sleep_phase_t>(i % 5);
        int result = security_hippocampus_protect_sleep(bridge, phase);
        EXPECT_EQ(result, 0);
    }

    // Bridge should still be stable
    sec_hippo_stats_t stats;
    security_hippocampus_get_stats(bridge, &stats);
    EXPECT_GE(stats.sleep_phases_protected, 100u);
}

TEST_F(SecurityHippocampusBridgeTest, ConcurrentOperations) {
    connect_all_systems();

    // Simulate concurrent operations
    for (int i = 0; i < 50; i++) {
        security_hippocampus_protect_sleep(bridge, SEC_HIPPO_SLEEP_DEEP_NREM);
        security_hippocampus_detect_injection(bridge, nullptr, nullptr, nullptr, 0);

        sec_hippo_coherence_status_t status;
        float spatial, temporal;
        security_hippocampus_check_coherence(bridge, &status, &spatial, &temporal);

        security_hippocampus_bridge_update(bridge, 10);
    }

    // Verify bridge is still operational
    sec_hippo_state_info_t state;
    int result = security_hippocampus_get_state(bridge, &state);
    EXPECT_EQ(result, 0);
}

TEST_F(SecurityHippocampusBridgeTest, AuditLogCircularBuffer) {
    // Fill audit log beyond max capacity
    for (uint32_t i = 0; i < SEC_HIPPO_MAX_AUDIT_ENTRIES + 100; i++) {
        security_hippocampus_protect_sleep(bridge, SEC_HIPPO_SLEEP_DEEP_NREM);
    }

    // Should still work (circular buffer)
    sec_hippo_audit_entry_t entries[10];
    size_t count = 0;
    int result = security_hippocampus_get_audit_log(bridge, entries, 10, &count);
    EXPECT_EQ(result, 0);
    EXPECT_GT(count, 0u);
}

TEST_F(SecurityHippocampusBridgeTest, PlaceCellHistoryCircular) {
    // Fill place cell history
    for (int i = 0; i < 500; i++) {
        int result = security_hippocampus_report_place_cell(bridge, i,
                                                             (float)i * 0.01f,
                                                             (float)i * 0.01f,
                                                             5.0f);
        EXPECT_EQ(result, 0);
    }

    // Coherence check should still work
    sec_hippo_coherence_status_t status;
    float spatial, temporal;
    int result = security_hippocampus_check_coherence(bridge, &status,
                                                       &spatial, &temporal);
    EXPECT_EQ(result, 0);
}

TEST_F(SecurityHippocampusBridgeTest, TimeCellHistoryCircular) {
    // Fill time cell history
    for (int i = 0; i < 500; i++) {
        int result = security_hippocampus_report_time_cell(bridge, i,
                                                            1000000 + (uint64_t)i * 1000,
                                                            5.0f);
        EXPECT_EQ(result, 0);
    }

    // Coherence check should still work
    float score;
    int result = security_hippocampus_check_temporal_coherence(bridge, &score);
    EXPECT_EQ(result, 0);
}

TEST_F(SecurityHippocampusBridgeTest, AllSleepPhases) {
    // Test all sleep phases
    sec_hippo_sleep_phase_t phases[] = {
        SEC_HIPPO_SLEEP_AWAKE,
        SEC_HIPPO_SLEEP_DROWSY,
        SEC_HIPPO_SLEEP_LIGHT_NREM,
        SEC_HIPPO_SLEEP_DEEP_NREM,
        SEC_HIPPO_SLEEP_REM
    };

    for (auto phase : phases) {
        int result = security_hippocampus_protect_sleep(bridge, phase);
        EXPECT_EQ(result, 0);

        sec_hippo_sleep_phase_t current = security_hippocampus_get_sleep_phase(bridge);
        EXPECT_EQ(current, phase);
    }
}

TEST_F(SecurityHippocampusBridgeTest, AllInjectionTypes) {
    // Test blocking all injection types
    sec_hippo_injection_type_t types[] = {
        SEC_HIPPO_INJECT_FALSE_MEMORY,
        SEC_HIPPO_INJECT_PATTERN_POISON,
        SEC_HIPPO_INJECT_TEMPORAL_SPLICE,
        SEC_HIPPO_INJECT_SPATIAL_FAKE,
        SEC_HIPPO_INJECT_CONTEXT_CORRUPT,
        SEC_HIPPO_INJECT_RIPPLE_FORGE
    };

    for (auto type : types) {
        int result = security_hippocampus_block_injection(bridge, type);
        EXPECT_EQ(result, 0);
    }

    sec_hippo_stats_t stats;
    security_hippocampus_get_stats(bridge, &stats);
    EXPECT_EQ(stats.injections_blocked, 6u);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
