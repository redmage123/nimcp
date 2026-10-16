/**
 * @file test_security_hippocampus_integration.cpp
 * @brief Integration tests for Security-Hippocampus Bridge
 * @version 1.0.0
 * @date 2026-01-10
 *
 * Integration tests verifying:
 * - Sleep protection end-to-end workflows
 * - Consolidation verification with multiple events
 * - Injection detection with pattern analysis
 * - Replay validation with encoding registration
 * - Coherence checking with cell activity data
 * - Multi-system integration (hippocampus + sleep + bio-async)
 * - State transitions and recovery
 */

#include <gtest/gtest.h>
#include <vector>
#include <cstring>
#include <thread>
#include <chrono>

extern "C" {
#include "security/hippocampus/nimcp_security_hippocampus_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_codes.h"
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class SecurityHippocampusIntegrationTest : public ::testing::Test {
protected:
    sec_hippo_bridge_t* bridge = nullptr;
    sec_hippo_config_t config;

    void SetUp() override {
        security_hippocampus_default_config(&config);
        config.enable_bio_async = true;
        bridge = security_hippocampus_bridge_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            security_hippocampus_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }

    hippocampus_system_t create_mock_hippocampus() {
        return reinterpret_cast<hippocampus_system_t>(0x30000001);
    }

    sleep_system_t create_mock_sleep_system() {
        return reinterpret_cast<sleep_system_t>(0x30000002);
    }

    void connect_all_systems() {
        security_hippocampus_connect_hippo(bridge, create_mock_hippocampus());
        security_hippocampus_connect_sleep(bridge, create_mock_sleep_system());
    }

    void add_consolidation_event(uint64_t memory_id, float strength_before,
                                  float strength_after) {
        if (bridge->num_consolidation_events < SEC_HIPPO_MAX_CONSOLIDATION_EVENTS) {
            sec_hippo_consolidation_event_t* event =
                &bridge->consolidation_events[bridge->num_consolidation_events++];
            event->event_id = bridge->num_consolidation_events;
            event->memory_id = memory_id;
            event->strength_before = strength_before;
            event->strength_after = strength_after;
            event->timestamp = 0;
            event->verified = false;
        }
    }

    void add_replay_sequence(uint64_t sequence_id, uint64_t encoding_id,
                              float content_match_score) {
        if (bridge->num_replay_sequences < SEC_HIPPO_MAX_REPLAY_SEQUENCES) {
            sec_hippo_replay_sequence_t* seq =
                &bridge->replay_sequences[bridge->num_replay_sequences++];
            seq->sequence_id = sequence_id;
            seq->encoding_id = encoding_id;
            seq->content_match_score = content_match_score;
            seq->replay_strength = 0.7f;
            seq->status = SEC_HIPPO_REPLAY_VALID;
        }
    }
};

/* ============================================================================
 * Sleep Protection Integration Tests
 * ============================================================================ */

TEST_F(SecurityHippocampusIntegrationTest, SleepProtectionWorkflow) {
    connect_all_systems();

    // Simulate sleep cycle
    sec_hippo_sleep_phase_t phases[] = {
        SEC_HIPPO_SLEEP_AWAKE,
        SEC_HIPPO_SLEEP_DROWSY,
        SEC_HIPPO_SLEEP_LIGHT_NREM,
        SEC_HIPPO_SLEEP_DEEP_NREM,
        SEC_HIPPO_SLEEP_REM,
        SEC_HIPPO_SLEEP_LIGHT_NREM,
        SEC_HIPPO_SLEEP_AWAKE
    };

    for (auto phase : phases) {
        int result = security_hippocampus_protect_sleep(bridge, phase);
        EXPECT_EQ(result, 0);

        // Update bridge
        result = security_hippocampus_bridge_update(bridge, 100);
        EXPECT_EQ(result, 0);

        // Check protection state
        bool protected_ = security_hippocampus_is_sleep_protected(bridge);
        if (phase == SEC_HIPPO_SLEEP_DEEP_NREM ||
            phase == SEC_HIPPO_SLEEP_REM ||
            phase == SEC_HIPPO_SLEEP_LIGHT_NREM) {
            EXPECT_TRUE(protected_);
        }
    }

    // Verify stats
    sec_hippo_stats_t stats;
    security_hippocampus_get_stats(bridge, &stats);
    EXPECT_GE(stats.sleep_phases_protected, 7u);
}

TEST_F(SecurityHippocampusIntegrationTest, SleepProtectionWithConsolidation) {
    connect_all_systems();

    // Enter deep NREM (peak consolidation)
    security_hippocampus_protect_sleep(bridge, SEC_HIPPO_SLEEP_DEEP_NREM);

    // Add consolidation events during protected phase
    for (int i = 0; i < 10; i++) {
        add_consolidation_event(5000 + i, 0.3f, 0.7f);
    }

    // Verify consolidations
    uint32_t verified, failed;
    int result = security_hippocampus_verify_all_consolidations(bridge,
                                                                 &verified, &failed);
    EXPECT_EQ(result, 0);
    EXPECT_GE(verified, 5u);

    // Check audit log entries
    sec_hippo_audit_entry_t entries[20];
    size_t count;
    security_hippocampus_get_audit_log(bridge, entries, 20, &count);
    EXPECT_GT(count, 0u);
}

/* ============================================================================
 * Consolidation Integration Tests
 * ============================================================================ */

TEST_F(SecurityHippocampusIntegrationTest, ConsolidationVerificationWorkflow) {
    connect_all_systems();

    // Add mix of consolidation events
    add_consolidation_event(6001, 0.2f, 0.8f);  // Strong positive
    add_consolidation_event(6002, 0.5f, 0.55f); // Slight positive
    add_consolidation_event(6003, 0.6f, 0.55f); // Slight negative (degraded)
    add_consolidation_event(6004, 0.8f, 0.3f);  // Strong negative (corrupted)

    // Verify each
    sec_hippo_consolidation_status_t status;
    float confidence;

    // Strong positive should be OK
    int result = security_hippocampus_verify_consolidation(bridge, 6001,
                                                           &status, &confidence);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(status, SEC_HIPPO_CONSOL_OK);

    // Slight positive should be OK
    result = security_hippocampus_verify_consolidation(bridge, 6002,
                                                        &status, &confidence);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(status, SEC_HIPPO_CONSOL_OK);

    // Slight negative should be degraded
    result = security_hippocampus_verify_consolidation(bridge, 6003,
                                                        &status, &confidence);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(status, SEC_HIPPO_CONSOL_DEGRADED);

    // Strong negative should be corrupted
    result = security_hippocampus_verify_consolidation(bridge, 6004,
                                                        &status, &confidence);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(status, SEC_HIPPO_CONSOL_CORRUPTED);

    // Check statistics
    sec_hippo_stats_t stats;
    security_hippocampus_get_stats(bridge, &stats);
    EXPECT_GE(stats.consolidation_checks, 4u);
    EXPECT_GE(stats.consolidations_verified, 2u);
    EXPECT_GE(stats.consolidations_degraded, 1u);
    EXPECT_GE(stats.consolidations_corrupted, 1u);
}

TEST_F(SecurityHippocampusIntegrationTest, ConsolidationPauseResumeWorkflow) {
    connect_all_systems();

    // Start consolidation
    add_consolidation_event(7001, 0.3f, 0.6f);

    // Pause for verification
    int result = security_hippocampus_pause_consolidation(bridge);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(bridge->security_effects.consolidation_paused);

    // Verify
    sec_hippo_consolidation_status_t status;
    float confidence;
    result = security_hippocampus_verify_consolidation(bridge, 7001,
                                                        &status, &confidence);
    EXPECT_EQ(result, 0);

    // Resume
    result = security_hippocampus_resume_consolidation(bridge);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(bridge->security_effects.consolidation_paused);
}

/* ============================================================================
 * Injection Detection Integration Tests
 * ============================================================================ */

TEST_F(SecurityHippocampusIntegrationTest, InjectionDetectionAndBlocking) {
    connect_all_systems();

    // Scan for injections
    sec_hippo_injection_type_t injection;
    float confidence;
    char details[256];

    bool detected = security_hippocampus_detect_injection(bridge, &injection,
                                                           &confidence, details, sizeof(details));

    // Block any detected injection
    if (detected) {
        int result = security_hippocampus_block_injection(bridge, injection);
        EXPECT_EQ(result, 0);
    }

    // Verify stats
    sec_hippo_stats_t stats;
    security_hippocampus_get_stats(bridge, &stats);
    EXPECT_GE(stats.injection_scans, 1u);
}

TEST_F(SecurityHippocampusIntegrationTest, PatternRegistrationAndDetection) {
    connect_all_systems();

    // Register legitimate patterns
    for (uint64_t i = 0; i < 10; i++) {
        int result = security_hippocampus_register_pattern(bridge,
                                                            8000 + i, 0xABCD0000 + i);
        EXPECT_EQ(result, 0);
    }

    // Subsequent injection detection should use registered patterns
    sec_hippo_injection_type_t injection;
    float confidence;

    for (int i = 0; i < 5; i++) {
        security_hippocampus_detect_injection(bridge, &injection, &confidence, nullptr, 0);
    }

    sec_hippo_stats_t stats;
    security_hippocampus_get_stats(bridge, &stats);
    EXPECT_GE(stats.injection_scans, 5u);
}

/* ============================================================================
 * Replay Validation Integration Tests
 * ============================================================================ */

TEST_F(SecurityHippocampusIntegrationTest, ReplayValidationWithEncodings) {
    connect_all_systems();

    // Register encodings
    for (uint64_t i = 0; i < 5; i++) {
        int result = security_hippocampus_register_encoding(bridge,
                                                             9000 + i,
                                                             0xDEAD0000 + i,
                                                             1000000 + i * 10000);
        EXPECT_EQ(result, 0);
    }

    // Create replay sequences linked to encodings
    for (uint64_t i = 0; i < 5; i++) {
        add_replay_sequence(10000 + i, 9000 + i, 0.85f);
    }

    // Validate replays
    for (uint64_t i = 0; i < 5; i++) {
        sec_hippo_replay_status_t status;
        float match_score;

        int result = security_hippocampus_validate_replay(bridge, 10000 + i,
                                                           &status, &match_score);
        EXPECT_EQ(result, 0);
    }

    // Check stats
    sec_hippo_stats_t stats;
    security_hippocampus_get_stats(bridge, &stats);
    EXPECT_GE(stats.replays_validated, 5u);
}

TEST_F(SecurityHippocampusIntegrationTest, ReplayRejectionWorkflow) {
    connect_all_systems();

    // Create a replay sequence
    add_replay_sequence(11000, 0, 0.3f);  // Low match score

    // Validate - should not pass with low score
    sec_hippo_replay_status_t status;
    float match_score;

    int result = security_hippocampus_validate_replay(bridge, 11000,
                                                       &status, &match_score);
    EXPECT_EQ(result, 0);

    // If invalid, reject it
    if (status != SEC_HIPPO_REPLAY_VALID) {
        result = security_hippocampus_reject_replay(bridge, 11000);
        EXPECT_EQ(result, 0);

        // Verify rejection
        sec_hippo_replay_sequence_t seq;
        security_hippocampus_get_replay_info(bridge, 11000, &seq);
        EXPECT_EQ(seq.status, SEC_HIPPO_REPLAY_FORGED);
    }
}

/* ============================================================================
 * Coherence Checking Integration Tests
 * ============================================================================ */

TEST_F(SecurityHippocampusIntegrationTest, CoherenceWithCellActivity) {
    connect_all_systems();

    // Simulate consistent place cell activity
    for (int i = 0; i < 20; i++) {
        int result = security_hippocampus_report_place_cell(bridge, i,
                                                             0.5f + (float)i * 0.01f,
                                                             0.5f + (float)i * 0.01f,
                                                             5.0f + (float)(i % 3));
        EXPECT_EQ(result, 0);
    }

    // Simulate consistent time cell activity
    for (int i = 0; i < 20; i++) {
        int result = security_hippocampus_report_time_cell(bridge, i,
                                                            1000000 + (uint64_t)i * 50000,
                                                            5.0f + (float)(i % 3));
        EXPECT_EQ(result, 0);
    }

    // Check coherence
    sec_hippo_coherence_status_t status;
    float spatial, temporal;

    int result = security_hippocampus_check_coherence(bridge, &status,
                                                       &spatial, &temporal);
    EXPECT_EQ(result, 0);

    // With consistent data, coherence should be good
    EXPECT_GE(spatial, 0.5f);
    EXPECT_GE(temporal, 0.5f);

    // Check individual coherence functions
    float spatial_only, temporal_only;
    security_hippocampus_check_spatial_coherence(bridge, &spatial_only);
    security_hippocampus_check_temporal_coherence(bridge, &temporal_only);

    EXPECT_FLOAT_EQ(spatial, spatial_only);
    EXPECT_FLOAT_EQ(temporal, temporal_only);
}

TEST_F(SecurityHippocampusIntegrationTest, CoherenceDetectsAnomalies) {
    connect_all_systems();

    // Simulate inconsistent place cell activity (large jumps)
    for (int i = 0; i < 20; i++) {
        float x = (i % 2 == 0) ? 0.0f : 1.0f;  // Alternating positions
        float y = (i % 2 == 0) ? 0.0f : 1.0f;

        security_hippocampus_report_place_cell(bridge, i, x, y, 5.0f);
    }

    // Check coherence
    float spatial;
    int result = security_hippocampus_check_spatial_coherence(bridge, &spatial);
    EXPECT_EQ(result, 0);

    // With inconsistent data, spatial coherence should be lower
    EXPECT_LT(spatial, 1.0f);
}

/* ============================================================================
 * Multi-System Integration Tests
 * ============================================================================ */

TEST_F(SecurityHippocampusIntegrationTest, FullWorkflowIntegration) {
    connect_all_systems();

    // 1. Enter sleep
    security_hippocampus_protect_sleep(bridge, SEC_HIPPO_SLEEP_LIGHT_NREM);

    // 2. Add some cell activity
    for (int i = 0; i < 10; i++) {
        security_hippocampus_report_place_cell(bridge, i, (float)i * 0.1f, (float)i * 0.1f, 5.0f);
        security_hippocampus_report_time_cell(bridge, i, 1000000 + (uint64_t)i * 10000, 5.0f);
    }

    // 3. Check coherence
    sec_hippo_coherence_status_t coherence_status;
    float spatial, temporal;
    security_hippocampus_check_coherence(bridge, &coherence_status, &spatial, &temporal);

    // 4. Deep NREM with consolidation
    security_hippocampus_protect_sleep(bridge, SEC_HIPPO_SLEEP_DEEP_NREM);

    for (int i = 0; i < 5; i++) {
        add_consolidation_event(12000 + i, 0.3f, 0.7f);
    }

    // 5. Verify consolidations
    uint32_t verified, failed;
    security_hippocampus_verify_all_consolidations(bridge, &verified, &failed);

    // 6. Detect injections
    sec_hippo_injection_type_t injection;
    float confidence;
    security_hippocampus_detect_injection(bridge, &injection, &confidence, nullptr, 0);

    // 7. Register and validate replays
    security_hippocampus_register_encoding(bridge, 13000, 0xBEEF, 2000000);
    add_replay_sequence(14000, 13000, 0.9f);

    sec_hippo_replay_status_t replay_status;
    float match_score;
    security_hippocampus_validate_replay(bridge, 14000, &replay_status, &match_score);

    // 8. REM sleep
    security_hippocampus_protect_sleep(bridge, SEC_HIPPO_SLEEP_REM);
    security_hippocampus_bridge_update(bridge, 100);

    // 9. Wake up
    security_hippocampus_protect_sleep(bridge, SEC_HIPPO_SLEEP_AWAKE);

    // 10. Verify final state
    sec_hippo_state_info_t state;
    security_hippocampus_get_state(bridge, &state);
    EXPECT_EQ(state.state, SEC_HIPPO_STATE_MONITORING);

    sec_hippo_stats_t stats;
    security_hippocampus_get_stats(bridge, &stats);
    EXPECT_GE(stats.sleep_phases_protected, 4u);
    EXPECT_GE(stats.consolidation_checks, 5u);
    EXPECT_GE(stats.injection_scans, 1u);
    EXPECT_GE(stats.replays_validated, 1u);
    EXPECT_GE(stats.coherence_checks, 1u);
}

TEST_F(SecurityHippocampusIntegrationTest, BridgeUpdateCycle) {
    connect_all_systems();

    // Simulate multiple update cycles
    for (int cycle = 0; cycle < 100; cycle++) {
        // Random sleep phase
        sec_hippo_sleep_phase_t phase =
            static_cast<sec_hippo_sleep_phase_t>(cycle % 5);
        security_hippocampus_set_sleep_phase(bridge, phase);

        // Add some activity
        if (cycle % 10 == 0) {
            add_consolidation_event(15000 + cycle, 0.3f, 0.65f);
        }

        if (cycle % 5 == 0) {
            security_hippocampus_report_place_cell(bridge, cycle % 100,
                                                    (float)(cycle % 10) * 0.1f,
                                                    (float)(cycle % 10) * 0.1f,
                                                    5.0f);
        }

        // Update bridge
        int result = security_hippocampus_bridge_update(bridge, 10);
        EXPECT_EQ(result, 0);
    }

    // Verify bridge is still stable
    sec_hippo_state_info_t state;
    int result = security_hippocampus_get_state(bridge, &state);
    EXPECT_EQ(result, 0);

    sec_hippo_stats_t stats;
    security_hippocampus_get_stats(bridge, &stats);
    EXPECT_GE(stats.awake_events + stats.nrem_deep_events + stats.rem_events, 10u);
}

/* ============================================================================
 * Bio-Async Integration Tests
 * ============================================================================ */

TEST_F(SecurityHippocampusIntegrationTest, BioAsyncIntegration) {
    connect_all_systems();

    // Try to connect bio-async
    int result = security_hippocampus_connect_bio_async(bridge);
    if (result != 0 || !security_hippocampus_is_bio_async_connected(bridge)) {
        GTEST_SKIP() << "Bio-async router not available";
    }

    // Perform operations with bio-async connected
    security_hippocampus_protect_sleep(bridge, SEC_HIPPO_SLEEP_DEEP_NREM);
    security_hippocampus_bridge_update(bridge, 100);

    // Disconnect
    result = security_hippocampus_disconnect_bio_async(bridge);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(security_hippocampus_is_bio_async_connected(bridge));
}

/* ============================================================================
 * State Recovery Tests
 * ============================================================================ */

TEST_F(SecurityHippocampusIntegrationTest, StateRecoveryAfterReset) {
    connect_all_systems();

    // Build up state
    security_hippocampus_protect_sleep(bridge, SEC_HIPPO_SLEEP_DEEP_NREM);
    add_consolidation_event(16001, 0.3f, 0.7f);
    add_replay_sequence(17001, 0, 0.8f);

    for (int i = 0; i < 10; i++) {
        security_hippocampus_report_place_cell(bridge, i, (float)i * 0.1f, (float)i * 0.1f, 5.0f);
    }

    // Verify state exists
    EXPECT_GT(bridge->num_consolidation_events, 0u);
    EXPECT_GT(bridge->num_replay_sequences, 0u);

    // Reset
    int result = security_hippocampus_bridge_reset(bridge);
    EXPECT_EQ(result, 0);

    // Verify state cleared
    EXPECT_EQ(bridge->num_consolidation_events, 0u);
    EXPECT_EQ(bridge->num_replay_sequences, 0u);

    sec_hippo_stats_t stats;
    security_hippocampus_get_stats(bridge, &stats);
    EXPECT_EQ(stats.sleep_protection_activations, 0u);

    // Verify bridge still operational
    result = security_hippocampus_protect_sleep(bridge, SEC_HIPPO_SLEEP_REM);
    EXPECT_EQ(result, 0);
}

TEST_F(SecurityHippocampusIntegrationTest, ConnectionRecovery) {
    // Connect
    connect_all_systems();
    EXPECT_TRUE(security_hippocampus_is_fully_connected(bridge));

    // Disconnect
    security_hippocampus_disconnect_all(bridge);
    EXPECT_FALSE(security_hippocampus_is_fully_connected(bridge));

    // Reconnect
    connect_all_systems();
    EXPECT_TRUE(security_hippocampus_is_fully_connected(bridge));

    // Operations should work
    int result = security_hippocampus_protect_sleep(bridge, SEC_HIPPO_SLEEP_DEEP_NREM);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * Performance Tests
 * ============================================================================ */

TEST_F(SecurityHippocampusIntegrationTest, HighVolumeOperations) {
    connect_all_systems();

    // High volume of operations
    const int iterations = 1000;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; i++) {
        // Various operations
        security_hippocampus_protect_sleep(bridge,
            static_cast<sec_hippo_sleep_phase_t>(i % 5));

        security_hippocampus_report_place_cell(bridge, i % 100,
            (float)(i % 10) * 0.1f, (float)(i % 10) * 0.1f, 5.0f);

        security_hippocampus_report_time_cell(bridge, i % 100,
            1000000 + (uint64_t)i * 1000, 5.0f);

        if (i % 100 == 0) {
            sec_hippo_coherence_status_t status;
            float spatial, temporal;
            security_hippocampus_check_coherence(bridge, &status, &spatial, &temporal);
        }

        if (i % 50 == 0) {
            sec_hippo_injection_type_t injection;
            float confidence;
            security_hippocampus_detect_injection(bridge, &injection, &confidence, nullptr, 0);
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete in reasonable time (< 5 seconds for 1000 iterations)
    EXPECT_LT(duration.count(), 5000);

    // Verify stats accumulated
    sec_hippo_stats_t stats;
    security_hippocampus_get_stats(bridge, &stats);
    EXPECT_GE(stats.sleep_phases_protected, (uint64_t)iterations);
    EXPECT_GE(stats.coherence_checks, 10u);
    EXPECT_GE(stats.injection_scans, 20u);
}

/* ============================================================================
 * Audit Trail Integration Tests
 * ============================================================================ */

TEST_F(SecurityHippocampusIntegrationTest, AuditTrailIntegrity) {
    connect_all_systems();

    // Perform operations that generate audit entries
    security_hippocampus_protect_sleep(bridge, SEC_HIPPO_SLEEP_DEEP_NREM);

    add_consolidation_event(18001, 0.3f, 0.7f);
    sec_hippo_consolidation_status_t status;
    float confidence;
    security_hippocampus_verify_consolidation(bridge, 18001, &status, &confidence);

    sec_hippo_injection_type_t injection;
    security_hippocampus_detect_injection(bridge, &injection, &confidence, nullptr, 0);

    // Check audit log
    sec_hippo_audit_entry_t entries[50];
    size_t count;
    int result = security_hippocampus_get_audit_log(bridge, entries, 50, &count);
    EXPECT_EQ(result, 0);
    EXPECT_GT(count, 0u);

    // Verify entries have valid data
    for (size_t i = 0; i < count; i++) {
        EXPECT_GT(entries[i].timestamp, 0u);
        EXPECT_GE(entries[i].severity, 0.0f);
        EXPECT_LE(entries[i].severity, 1.0f);
    }

    // Clear and verify
    result = security_hippocampus_clear_audit_log(bridge);
    EXPECT_EQ(result, 0);

    security_hippocampus_get_audit_log(bridge, entries, 50, &count);
    EXPECT_EQ(count, 0u);
}

/* ============================================================================
 * Effects Propagation Tests
 * ============================================================================ */

TEST_F(SecurityHippocampusIntegrationTest, EffectsPropagation) {
    connect_all_systems();

    // Initial effects should be neutral
    security_to_hippo_effects_t sec_effects;
    hippo_to_security_effects_t hippo_effects;

    security_hippocampus_get_security_effects(bridge, &sec_effects);
    security_hippocampus_get_hippo_effects(bridge, &hippo_effects);

    EXPECT_FALSE(sec_effects.sleep_protection_active);
    EXPECT_EQ(sec_effects.blocked_ripples, 0u);

    // Activate protection
    security_hippocampus_protect_sleep(bridge, SEC_HIPPO_SLEEP_DEEP_NREM);
    security_hippocampus_apply_security_effects(bridge);

    security_hippocampus_get_security_effects(bridge, &sec_effects);
    EXPECT_TRUE(sec_effects.sleep_protection_active);
    EXPECT_GT(sec_effects.ripple_filter_level, 0.0f);

    // Add cell activity and gather effects
    for (int i = 0; i < 10; i++) {
        security_hippocampus_report_place_cell(bridge, i, (float)i * 0.1f, (float)i * 0.1f, 5.0f);
    }

    security_hippocampus_gather_hippo_effects(bridge);
    security_hippocampus_get_hippo_effects(bridge, &hippo_effects);

    EXPECT_GE(hippo_effects.active_place_cells, 10u);
    EXPECT_GE(hippo_effects.spatial_coherence, 0.0f);
    EXPECT_LE(hippo_effects.spatial_coherence, 1.0f);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
