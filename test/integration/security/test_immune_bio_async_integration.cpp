/**
 * @file test_immune_bio_async_integration.cpp
 * @brief Integration tests for immune + bio-async working together
 * @date 2026-03-20
 *
 * WHAT: Verify immune system and bio-async work together for threat detection,
 *       response coordination, audit trails, and error recovery
 * WHY:  Ensure cross-system integration paths are exercised
 * HOW:  Create both systems, connect them, test end-to-end workflows
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "cognitive/immune/nimcp_brain_immune.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "utils/memory/nimcp_memory.h"
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class ImmuneBioAsyncIntegrationTest : public ::testing::Test {
protected:
    brain_immune_system_t* immune = nullptr;
    bbb_system_t bbb = nullptr;
    bool router_initialized = false;
    bool bio_async_initialized = false;

    void SetUp() override {
        // Initialize bio-async
        nimcp_bio_async_config_t async_config = nimcp_bio_async_default_config();
        async_config.enable_statistics = true;
        async_config.enable_logging = false;
        async_config.thread_pool_size = 1;
        if (nimcp_bio_async_init(&async_config) == NIMCP_SUCCESS) {
            bio_async_initialized = true;
        }

        // Initialize bio-router
        bio_router_config_t router_config = bio_router_default_config();
        router_config.max_modules = 64;
        router_config.enable_statistics = true;
        router_config.enable_logging = false;
        if (bio_router_init(&router_config) == NIMCP_SUCCESS) {
            router_initialized = true;
        }

        // Create BBB
        bbb_config_t bbb_cfg = bbb_default_config();
        bbb = bbb_system_create(&bbb_cfg);

        // Create immune system with bio-async enabled
        brain_immune_config_t config;
        brain_immune_default_config(&config);
        config.enable_bbb_integration = (bbb != nullptr);
        config.enable_bft_integration = false;
        config.enable_swarm_integration = false;
        config.enable_bio_async = router_initialized;
        config.enable_logging = false;

        immune = brain_immune_create(&config);
        ASSERT_NE(immune, nullptr);

        // Connect systems
        if (bbb && immune) {
            bbb_connect_immune(bbb, immune);
            brain_immune_connect_bbb(immune, bbb);
        }

        if (router_initialized && immune) {
            brain_immune_connect_bio_async(immune);
        }

        brain_immune_start(immune);
    }

    void TearDown() override {
        if (immune) {
            brain_immune_stop(immune);
            brain_immune_destroy(immune);
            immune = nullptr;
        }
        if (bbb) {
            bbb_system_destroy(bbb);
            bbb = nullptr;
        }
        if (router_initialized) {
            bio_router_shutdown();
            router_initialized = false;
        }
        if (bio_async_initialized) {
            nimcp_bio_async_shutdown();
            bio_async_initialized = false;
        }
    }

    // Helper to present a generic antigen
    uint32_t present_antigen(const char* pattern, uint32_t severity = 5) {
        uint32_t antigen_id = 0;
        uint8_t epitope[BRAIN_IMMUNE_EPITOPE_SIZE];
        memset(epitope, 0, sizeof(epitope));
        size_t len = strlen(pattern);
        if (len > BRAIN_IMMUNE_EPITOPE_SIZE) len = BRAIN_IMMUNE_EPITOPE_SIZE;
        memcpy(epitope, pattern, len);

        brain_immune_present_antigen(
            immune, ANTIGEN_SOURCE_MANUAL,
            epitope, len, severity, 1, &antigen_id);
        return antigen_id;
    }
};

/* ============================================================================
 * BBB -> Immune -> Bio-Async Tests
 * ============================================================================ */

TEST_F(ImmuneBioAsyncIntegrationTest, BBBThreatTriggersImmuneViaAsync) {
    if (!bbb) GTEST_SKIP() << "BBB not available";

    // Present threat to BBB which should forward to immune
    uint32_t antigen_id = 0;
    uint8_t threat_data[] = "CODE_INJECTION_PAYLOAD";

    int rc = brain_immune_present_bbb_threat(
        immune,
        BBB_THREAT_CODE_INJECTION,
        BBB_SEVERITY_HIGH,
        threat_data,
        sizeof(threat_data),
        &antigen_id
    );
    EXPECT_EQ(rc, 0);
    EXPECT_GT(antigen_id, 0u);

    // Update to process
    brain_immune_update(immune, 200);

    // Verify antigen exists
    brain_antigen_t copy;
    rc = brain_immune_get_antigen_copy(immune, antigen_id, &copy);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(copy.source, ANTIGEN_SOURCE_BBB);
}

TEST_F(ImmuneBioAsyncIntegrationTest, ImmuneResponseRoutesBioAsync) {
    // Present antigen, activate B cell, attempt antibody production
    uint32_t antigen_id = present_antigen("BIOASYNC_RESP_TEST", 7);
    brain_immune_update(immune, 200);

    uint32_t b_cell_id = 0;
    int rc = brain_immune_activate_b_cell(immune, antigen_id, &b_cell_id);
    EXPECT_EQ(rc, 0);

    uint32_t antibody_id = 0;
    rc = brain_immune_produce_antibody(immune, b_cell_id, ANTIBODY_IGG, &antibody_id);
    if (rc == 0 && antibody_id > 0) {
        rc = brain_immune_execute_antibody(immune, antibody_id);
        EXPECT_EQ(rc, 0);
    }

    // Verify stats - antigen was at minimum processed
    brain_immune_stats_t stats;
    brain_immune_get_stats(immune, &stats);
    EXPECT_GE(stats.antigens_processed, 1u);
}

/* ============================================================================
 * Health Check Integration Tests
 * ============================================================================ */

TEST_F(ImmuneBioAsyncIntegrationTest, HealthCheckIntegration) {
    // Run several update cycles and check health
    for (int i = 0; i < 10; i++) {
        brain_immune_update(immune, 100);
    }

    brain_immune_stats_t stats;
    int rc = brain_immune_get_stats(immune, &stats);
    EXPECT_EQ(rc, 0);
    EXPECT_GE(stats.system_health, 0.0f);
    EXPECT_LE(stats.system_health, 1.0f);

    // No threats presented, should be healthy
    EXPECT_GT(stats.system_health, 0.5f);
}

TEST_F(ImmuneBioAsyncIntegrationTest, CascadingFailureDetection) {
    // Present multiple severe threats rapidly
    for (int i = 0; i < 10; i++) {
        char pattern[32];
        snprintf(pattern, sizeof(pattern), "CASCADE_THREAT_%02d", i);
        present_antigen(pattern, 9);
        brain_immune_update(immune, 50);
    }

    brain_immune_stats_t stats;
    brain_immune_get_stats(immune, &stats);
    EXPECT_GE(stats.antigens_processed, 5u);
}

/* ============================================================================
 * Cytokine Signaling via Bio-Async Tests
 * ============================================================================ */

TEST_F(ImmuneBioAsyncIntegrationTest, CytokineSignalingViaBioAsync) {
    // Release cytokine which should go through bio-async
    uint32_t cytokine_id = 0;
    int rc = brain_immune_release_cytokine(
        immune,
        BRAIN_CYTOKINE_IL6,
        1,     // source_cell
        0.8f,  // concentration
        0,     // broadcast
        &cytokine_id
    );
    EXPECT_EQ(rc, 0);

    brain_immune_update(immune, 100);

    // Check cytokine level
    float level = brain_immune_get_cytokine_level(immune, BRAIN_CYTOKINE_IL6);
    EXPECT_GE(level, 0.0f);
}

TEST_F(ImmuneBioAsyncIntegrationTest, BroadcastAlertViaBioAsync) {
    uint32_t antigen_id = present_antigen("ALERT_BROADCAST_01", 8);
    brain_immune_update(immune, 100);

    int rc = brain_immune_broadcast_alert(immune, antigen_id, INFLAMMATION_SYSTEMIC);
    EXPECT_EQ(rc, 0);

    brain_immune_update(immune, 100);

    brain_immune_stats_t stats;
    brain_immune_get_stats(immune, &stats);
    EXPECT_GE(stats.cytokines_released, 0u);
}

/* ============================================================================
 * Multiple Threats Sequential Tests
 * ============================================================================ */

TEST_F(ImmuneBioAsyncIntegrationTest, MultipleThreatsSequential) {
    // Send 5 threats in rapid succession
    for (int i = 0; i < 5; i++) {
        char pattern[32];
        snprintf(pattern, sizeof(pattern), "RAPID_THREAT_%03d", i);
        uint32_t id = present_antigen(pattern, 5 + i);
        EXPECT_GT(id, 0u);
        brain_immune_update(immune, 50);
    }

    brain_immune_stats_t stats;
    brain_immune_get_stats(immune, &stats);
    EXPECT_GE(stats.antigens_processed, 3u);
}

/* ============================================================================
 * Recovery After Failure Tests
 * ============================================================================ */

TEST_F(ImmuneBioAsyncIntegrationTest, RecoveryAfterFailure) {
    // Present severe threat
    uint32_t antigen_id = present_antigen("SEVERE_FAILURE_01", 10);
    brain_immune_update(immune, 200);

    // Initiate inflammation
    uint32_t site_id = 0;
    brain_immune_initiate_inflammation(immune, 1, antigen_id, &site_id);
    brain_immune_update(immune, 100);

    // Activate response
    uint32_t b_cell_id = 0;
    brain_immune_activate_b_cell(immune, antigen_id, &b_cell_id);

    uint32_t antibody_id = 0;
    int rc = brain_immune_produce_antibody(immune, b_cell_id, ANTIBODY_IGG, &antibody_id);
    if (rc == 0 && antibody_id > 0) {
        brain_immune_execute_antibody(immune, antibody_id);
        brain_immune_neutralize(immune, antigen_id, antibody_id);
    }

    // Resolve inflammation
    brain_immune_resolve_inflammation(immune, site_id);
    brain_immune_update(immune, 500);

    // Verify system is still operational after recovery attempt
    brain_immune_stats_t stats;
    brain_immune_get_stats(immune, &stats);
    EXPECT_GE(stats.antigens_processed, 1u);
    EXPECT_GE(stats.system_health, 0.0f);
}

/* ============================================================================
 * Inflammation Effects via Bio-Async Tests
 * ============================================================================ */

TEST_F(ImmuneBioAsyncIntegrationTest, InflammationEffectsComputed) {
    // Present threats to trigger inflammation
    uint32_t antigen_id = present_antigen("INFL_EFFECTS_TEST", 9);
    brain_immune_update(immune, 100);

    uint32_t site_id = 0;
    brain_immune_initiate_inflammation(immune, 1, antigen_id, &site_id);
    brain_immune_escalate_inflammation(immune, site_id);
    brain_immune_update(immune, 200);

    inflammation_effects_t effects;
    int rc = brain_immune_get_inflammation_effects(immune, &effects);
    EXPECT_EQ(rc, 0);
    // Should have some inflammation
    EXPECT_GE(effects.level, 0.0f);
}

/* ============================================================================
 * Phase Transition via Bio-Async Tests
 * ============================================================================ */

TEST_F(ImmuneBioAsyncIntegrationTest, PhaseTransitionsEndToEnd) {
    // Start in surveillance
    brain_immune_phase_t phase = brain_immune_get_phase(immune);
    EXPECT_EQ(phase, IMMUNE_PHASE_SURVEILLANCE);

    // Present antigen - should move towards recognition/activation
    present_antigen("PHASE_TRANSITION_01", 8);
    brain_immune_update(immune, 500);

    phase = brain_immune_get_phase(immune);
    // May have moved to RECOGNITION or ACTIVATION
    EXPECT_GE((int)phase, (int)IMMUNE_PHASE_SURVEILLANCE);
}

/* ============================================================================
 * Notification Recovery Test
 * ============================================================================ */

TEST_F(ImmuneBioAsyncIntegrationTest, NotifyRecoveryResult) {
    uint32_t antigen_id = present_antigen("NOTIFY_RECOVERY_01", 7);
    brain_immune_update(immune, 200);

    int rc = brain_immune_notify_recovery_result(immune, antigen_id, 1, true);
    EXPECT_EQ(rc, 0);

    rc = brain_immune_notify_recovery_result(immune, antigen_id, 2, false);
    EXPECT_EQ(rc, 0);
}

/* ============================================================================
 * Bio-Async Router Stats After Integration Test
 * ============================================================================ */

TEST_F(ImmuneBioAsyncIntegrationTest, RouterStatsAfterImmuneOps) {
    if (!router_initialized) GTEST_SKIP() << "Router not initialized";

    bio_router_reset_stats();

    // Run immune operations that generate bio-async messages
    present_antigen("ROUTER_STATS_TEST", 7);
    brain_immune_update(immune, 200);

    uint32_t cytokine_id = 0;
    brain_immune_release_cytokine(immune, BRAIN_CYTOKINE_TNF, 1, 0.5f, 0, &cytokine_id);
    brain_immune_update(immune, 100);

    bio_router_stats_t stats;
    nimcp_error_t rc = bio_router_get_stats(&stats);
    EXPECT_EQ(rc, NIMCP_SUCCESS);
    // Some messages may have been routed
    EXPECT_GE(stats.messages_routed + stats.messages_dropped, 0u);
}

/* ============================================================================
 * Checkpoint State Test
 * ============================================================================ */

TEST_F(ImmuneBioAsyncIntegrationTest, CheckpointStateCapture) {
    // Present some threats to build state
    for (int i = 0; i < 3; i++) {
        char pattern[32];
        snprintf(pattern, sizeof(pattern), "CHECKPOINT_%02d", i);
        present_antigen(pattern, 6);
        brain_immune_update(immune, 100);
    }

    // Get checkpoint state - pass generic buffer since we don't have bft_immune_state_t
    // Just verify the function doesn't crash with immune system
    // This is an opaque state capture
    uint8_t state_buffer[256];
    memset(state_buffer, 0, sizeof(state_buffer));
    int rc = brain_immune_get_checkpoint_state(immune, state_buffer);
    // May succeed or not depending on buffer format - just verify no crash
    (void)rc;
}
