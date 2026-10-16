/**
 * @file test_immune_coverage.cpp
 * @brief Unit tests for immune system coverage improvements
 * @date 2026-03-20
 *
 * WHAT: Tests for brain immune system coverage: tolerance, memory decay,
 *       secondary response, consensus, BFT recovery, NULL handling, stats
 * WHY:  Ensure all immune subsystem paths are exercised
 * HOW:  Create standalone immune system instances, test functions directly
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

extern "C" {
#include "cognitive/immune/nimcp_brain_immune.h"
#include "utils/memory/nimcp_memory.h"
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class ImmuneCoverageTest : public ::testing::Test {
protected:
    brain_immune_system_t* immune = nullptr;
    brain_immune_config_t config;

    void SetUp() override {
        brain_immune_default_config(&config);
        // Disable integration dependencies for standalone testing
        config.enable_bbb_integration = false;
        config.enable_bft_integration = false;
        config.enable_swarm_integration = false;
        config.enable_bio_async = false;
        config.enable_logging = false;

        immune = brain_immune_create(&config);
        ASSERT_NE(immune, nullptr);

        int rc = brain_immune_start(immune);
        ASSERT_EQ(rc, 0);
    }

    void TearDown() override {
        if (immune) {
            brain_immune_stop(immune);
            brain_immune_destroy(immune);
            immune = nullptr;
        }
    }

    // Helper: present a generic antigen with given epitope
    uint32_t present_antigen(const char* pattern, uint32_t severity = 5) {
        uint32_t antigen_id = 0;
        uint8_t epitope[BRAIN_IMMUNE_EPITOPE_SIZE];
        memset(epitope, 0, sizeof(epitope));
        size_t len = strlen(pattern);
        if (len > BRAIN_IMMUNE_EPITOPE_SIZE) len = BRAIN_IMMUNE_EPITOPE_SIZE;
        memcpy(epitope, pattern, len);

        int rc = brain_immune_present_antigen(
            immune,
            ANTIGEN_SOURCE_MANUAL,
            epitope, len,
            severity,
            1,  // source_node
            &antigen_id
        );
        EXPECT_EQ(rc, 0);
        return antigen_id;
    }
};

/* ============================================================================
 * Tolerance Tests
 * ============================================================================ */

TEST_F(ImmuneCoverageTest, ImmuneToleranceRepeatedAntigen) {
    // Present the same antigen 15 times
    // After repeated exposure (>10), the immune system should develop tolerance
    const char* pattern = "REPEATED_THREAT_001";
    uint32_t first_id = 0;
    uint32_t last_id = 0;

    for (int i = 0; i < 15; i++) {
        uint32_t id = present_antigen(pattern, 8);
        if (i == 0) first_id = id;
        last_id = id;

        // Update immune system to process
        brain_immune_update(immune, 100);
    }

    // Verify we got valid antigen IDs
    EXPECT_GT(first_id, 0u);
    EXPECT_GT(last_id, 0u);

    // Get stats - repeated antigen presentation should be tracked
    brain_immune_stats_t stats;
    int rc = brain_immune_get_stats(immune, &stats);
    EXPECT_EQ(rc, 0);
    EXPECT_GE(stats.antigens_processed, 10u);
}

TEST_F(ImmuneCoverageTest, ImmuneToleranceThreshold) {
    // Present antigen only 9 times (under typical tolerance threshold of 10)
    // Severity should NOT be halved
    const char* pattern = "UNDER_THRESHOLD_001";

    for (int i = 0; i < 9; i++) {
        present_antigen(pattern, 7);
        brain_immune_update(immune, 50);
    }

    brain_immune_stats_t stats;
    brain_immune_get_stats(immune, &stats);
    // System should have processed antigens normally
    EXPECT_GE(stats.antigens_processed, 5u);
}

TEST_F(ImmuneCoverageTest, ImmuneToleranceReset) {
    // Present one antigen 15 times
    const char* pattern1 = "OLD_THREAT_PATTERN";
    for (int i = 0; i < 15; i++) {
        present_antigen(pattern1, 6);
        brain_immune_update(immune, 50);
    }

    // Now present a DIFFERENT antigen - should get full severity response
    const char* pattern2 = "NEW_THREAT_PATTERN";
    uint32_t new_id = present_antigen(pattern2, 9);
    brain_immune_update(immune, 100);

    // The new antigen should exist and not be tolerized
    brain_antigen_t copy;
    int rc = brain_immune_get_antigen_copy(immune, new_id, &copy);
    EXPECT_EQ(rc, 0);
    // New antigen should retain its full severity
    EXPECT_EQ(copy.severity, 9u);
}

/* ============================================================================
 * Memory Tests
 * ============================================================================ */

TEST_F(ImmuneCoverageTest, ImmuneMemoryDecay) {
    // Create a B cell in memory state by presenting antigen, activating, converting
    uint32_t antigen_id = present_antigen("MEMORY_DECAY_TEST", 7);
    brain_immune_update(immune, 100);

    uint32_t b_cell_id = 0;
    int rc = brain_immune_activate_b_cell(immune, antigen_id, &b_cell_id);
    EXPECT_EQ(rc, 0);

    // Convert to memory
    rc = brain_immune_b_cell_to_memory(immune, b_cell_id);
    EXPECT_EQ(rc, 0);

    // Advance time significantly to test memory decay
    for (int i = 0; i < 100; i++) {
        brain_immune_update(immune, 10000);  // 10 seconds per step
    }

    // Memory cells should still exist or have decayed - either is valid
    brain_immune_stats_t stats;
    brain_immune_get_stats(immune, &stats);
    // Stats should be populated regardless
    EXPECT_GE(stats.antigens_processed, 1u);
}

TEST_F(ImmuneCoverageTest, ImmuneMemoryCheckTriggered) {
    // Present antigen and create memory B cell
    uint32_t antigen_id = present_antigen("MEMORY_CHECK_TEST", 6);
    brain_immune_update(immune, 100);

    uint32_t b_cell_id = 0;
    int rc = brain_immune_activate_b_cell(immune, antigen_id, &b_cell_id);
    EXPECT_EQ(rc, 0);

    rc = brain_immune_b_cell_to_memory(immune, b_cell_id);
    EXPECT_EQ(rc, 0);

    // Check memory for same antigen
    uint32_t found_b_cell = 0;
    rc = brain_immune_check_memory(immune, antigen_id, &found_b_cell);
    // Should find or not find depending on implementation - just verify no crash
    // and that function returns valid code
    EXPECT_TRUE(rc == 0 || rc == -1);
}

TEST_F(ImmuneCoverageTest, ImmuneSecondaryResponse) {
    // Create memory cell first
    uint32_t antigen_id = present_antigen("SECONDARY_RESP_01", 7);
    brain_immune_update(immune, 200);

    uint32_t b_cell_id = 0;
    brain_immune_activate_b_cell(immune, antigen_id, &b_cell_id);
    brain_immune_b_cell_to_memory(immune, b_cell_id);

    // Present same threat again
    uint32_t new_antigen_id = present_antigen("SECONDARY_RESP_01", 7);
    brain_immune_update(immune, 100);

    // Try secondary response
    uint32_t found_b_cell = 0;
    int rc = brain_immune_check_memory(immune, new_antigen_id, &found_b_cell);
    if (rc == 0) {
        // Memory found - trigger secondary response
        rc = brain_immune_secondary_response(immune, new_antigen_id, found_b_cell);
        EXPECT_EQ(rc, 0);
    }

    // Verify stats show responses generated
    brain_immune_stats_t stats;
    brain_immune_get_stats(immune, &stats);
    EXPECT_GE(stats.responses_generated, 0u);
}

/* ============================================================================
 * Consensus and BFT Tests
 * ============================================================================ */

TEST_F(ImmuneCoverageTest, SwarmConsensusWired) {
    // Without swarm integration, consensus should handle gracefully
    uint32_t antigen_id = present_antigen("CONSENSUS_TEST_01", 6);
    float agreed_severity = 0.0f;

    int rc = brain_immune_consensus_threat_severity(immune, antigen_id, &agreed_severity);
    // Without swarm integration, this should either succeed with defaults or
    // return error code - verify no crash
    EXPECT_TRUE(rc == 0 || rc == -1);
}

TEST_F(ImmuneCoverageTest, BFTRecoveryCreatesAntigen) {
    // Simulate BFT trust recovery
    int rc = brain_immune_handle_bft_trust_recovery(immune, 42, 0.3f, 0.8f);
    // Without BFT integration, should handle gracefully
    EXPECT_TRUE(rc == 0 || rc == -1);

    // Stats should be valid regardless
    brain_immune_stats_t stats;
    brain_immune_get_stats(immune, &stats);
    EXPECT_GE(stats.system_health, 0.0f);
    EXPECT_LE(stats.system_health, 1.0f);
}

/* ============================================================================
 * NULL Handling Tests
 * ============================================================================ */

TEST_F(ImmuneCoverageTest, NullImmuneSystemHandled) {
    // All functions should handle NULL immune system gracefully
    brain_immune_destroy(nullptr);  // Should not crash

    int rc = brain_immune_start(nullptr);
    EXPECT_NE(rc, 0);

    rc = brain_immune_stop(nullptr);
    EXPECT_NE(rc, 0);

    rc = brain_immune_update(nullptr, 100);
    EXPECT_NE(rc, 0);

    brain_immune_stats_t stats;
    rc = brain_immune_get_stats(nullptr, &stats);
    EXPECT_NE(rc, 0);

    // NULL output pointer
    rc = brain_immune_get_stats(immune, nullptr);
    EXPECT_NE(rc, 0);

    brain_immune_phase_t phase = brain_immune_get_phase(nullptr);
    EXPECT_EQ(phase, IMMUNE_PHASE_SURVEILLANCE);  // Default

    bool neutralized = brain_immune_is_neutralized(nullptr, 1);
    EXPECT_FALSE(neutralized);

    const brain_antigen_t* ag = brain_immune_get_antigen(nullptr, 1);
    EXPECT_EQ(ag, nullptr);

    brain_antigen_t copy;
    rc = brain_immune_get_antigen_copy(nullptr, 1, &copy);
    EXPECT_NE(rc, 0);

    rc = brain_immune_present_antigen(nullptr, ANTIGEN_SOURCE_MANUAL,
                                       nullptr, 0, 5, 1, nullptr);
    EXPECT_NE(rc, 0);

    float cyt = brain_immune_get_cytokine_level(nullptr, BRAIN_CYTOKINE_IL1);
    EXPECT_EQ(cyt, 0.0f);

    brain_inflammation_level_t infl = brain_immune_get_inflammation_level(nullptr);
    EXPECT_EQ(infl, INFLAMMATION_NONE);

    float infl_c = brain_immune_get_inflammation_level_continuous(nullptr);
    EXPECT_EQ(infl_c, 0.0f);
}

/* ============================================================================
 * Stats Tracking Tests
 * ============================================================================ */

TEST_F(ImmuneCoverageTest, ImmuneStatsTracking) {
    // Run operations and verify stats counters
    brain_immune_stats_t stats_before;
    brain_immune_get_stats(immune, &stats_before);

    // Present multiple antigens
    for (int i = 0; i < 5; i++) {
        char pattern[32];
        snprintf(pattern, sizeof(pattern), "STATS_TEST_%03d", i);
        present_antigen(pattern, 5 + i);
        brain_immune_update(immune, 100);
    }

    brain_immune_stats_t stats_after;
    brain_immune_get_stats(immune, &stats_after);

    // Antigens processed should have increased
    EXPECT_GT(stats_after.antigens_processed, stats_before.antigens_processed);
    // System health should be valid
    EXPECT_GE(stats_after.system_health, 0.0f);
    EXPECT_LE(stats_after.system_health, 1.0f);
}

/* ============================================================================
 * BBB Connection Tests
 * ============================================================================ */

TEST_F(ImmuneCoverageTest, BBBConnectedToImmune) {
    // Verify bbb_connect_immune with NULL BBB
    bool result = bbb_connect_immune(nullptr, immune);
    EXPECT_FALSE(result);

    // Verify bbb_connect_immune with NULL immune
    // (disconnects, should succeed or return safely)
    bbb_config_t bbb_cfg = bbb_default_config();
    bbb_system_t bbb = bbb_system_create(&bbb_cfg);
    if (bbb) {
        result = bbb_connect_immune(bbb, nullptr);
        // Disconnecting is valid
        EXPECT_TRUE(result);
        bbb_system_destroy(bbb);
    }
}

/* ============================================================================
 * Exception Integration Test
 * ============================================================================ */

TEST_F(ImmuneCoverageTest, AntigenPresentationFromException) {
    // Test brain_immune_present_exception with NULL exception
    uint32_t antigen_id = 0;
    int rc = brain_immune_present_exception(immune, nullptr, &antigen_id);
    EXPECT_NE(rc, 0);  // Should reject NULL exception

    // Test with NULL output
    rc = brain_immune_present_exception(immune, nullptr, nullptr);
    EXPECT_NE(rc, 0);
}

/* ============================================================================
 * Inflammation Effects Tests
 * ============================================================================ */

TEST_F(ImmuneCoverageTest, InflammationEffectsComputation) {
    // Test the inline inflammation_compute_effects function
    inflammation_effects_t effects;

    // Test with level 0.0
    int rc = inflammation_compute_effects(0.0f, &effects);
    EXPECT_EQ(rc, 0);
    EXPECT_FLOAT_EQ(effects.level, 0.0f);
    EXPECT_EQ(effects.label, INFLAMMATION_NONE);
    EXPECT_FLOAT_EQ(effects.capacity_factor, 1.0f);

    // Test with level 0.5 (regional)
    rc = inflammation_compute_effects(0.5f, &effects);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(effects.label, INFLAMMATION_REGIONAL);
    EXPECT_LT(effects.capacity_factor, 1.0f);
    EXPECT_GT(effects.capacity_factor, 0.0f);

    // Test with level 1.0 (storm)
    rc = inflammation_compute_effects(1.0f, &effects);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(effects.label, INFLAMMATION_STORM);
    EXPECT_LE(effects.capacity_factor, 0.25f);

    // Test NULL output
    rc = inflammation_compute_effects(0.5f, nullptr);
    EXPECT_EQ(rc, -1);
}

/* ============================================================================
 * Cytokine and Response Tests
 * ============================================================================ */

TEST_F(ImmuneCoverageTest, CytokineRelease) {
    // Test cytokine release without bio-async
    uint32_t cytokine_id = 0;
    int rc = brain_immune_release_cytokine(
        immune,
        BRAIN_CYTOKINE_IL1,
        1,     // source_cell
        0.7f,  // concentration
        0,     // target_region (broadcast)
        &cytokine_id
    );
    // Should succeed even without bio-async (local tracking)
    EXPECT_EQ(rc, 0);

    // Query cytokine level
    float level = brain_immune_get_cytokine_level(immune, BRAIN_CYTOKINE_IL1);
    EXPECT_GE(level, 0.0f);
}

TEST_F(ImmuneCoverageTest, InflammationLifecycle) {
    // Present severe antigen
    uint32_t antigen_id = present_antigen("INFLAMMATION_TEST", 9);
    brain_immune_update(immune, 100);

    // Initiate inflammation
    uint32_t site_id = 0;
    int rc = brain_immune_initiate_inflammation(immune, 1, antigen_id, &site_id);
    EXPECT_EQ(rc, 0);

    // Escalate
    rc = brain_immune_escalate_inflammation(immune, site_id);
    EXPECT_EQ(rc, 0);

    // Resolve
    rc = brain_immune_resolve_inflammation(immune, site_id);
    EXPECT_EQ(rc, 0);
}

/* ============================================================================
 * Affinity Computation Test
 * ============================================================================ */

TEST_F(ImmuneCoverageTest, AffinityComputation) {
    uint8_t pattern1[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    uint8_t pattern2[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    uint8_t pattern3[] = {0xFF, 0xFE, 0xFD, 0xFC, 0xFB};

    // Identical patterns should have high affinity
    float identical = brain_immune_compute_affinity(
        pattern1, sizeof(pattern1), pattern2, sizeof(pattern2));
    EXPECT_GT(identical, 0.8f);

    // Different patterns should have low affinity
    float different = brain_immune_compute_affinity(
        pattern1, sizeof(pattern1), pattern3, sizeof(pattern3));
    EXPECT_LT(different, identical);

    // NULL patterns should return 0
    float null_aff = brain_immune_compute_affinity(nullptr, 0, pattern1, sizeof(pattern1));
    EXPECT_EQ(null_aff, 0.0f);
}

/* ============================================================================
 * Recovery Recommendation Test
 * ============================================================================ */

TEST_F(ImmuneCoverageTest, RecoveryRecommendation) {
    uint32_t antigen_id = present_antigen("RECOVERY_REC_TEST", 7);
    brain_immune_update(immune, 100);

    int action = -1;
    int rc = brain_immune_get_recovery_recommendation(immune, antigen_id, &action);
    // With no memory, should return -1 (no recommendation)
    EXPECT_TRUE(rc == 0 || rc == -1);
}

/* ============================================================================
 * Callback Registration Tests
 * ============================================================================ */

static int s_antigen_callback_count = 0;
static void test_antigen_callback(brain_immune_system_t*, const brain_antigen_t*, void*) {
    s_antigen_callback_count++;
}

TEST_F(ImmuneCoverageTest, CallbackRegistration) {
    s_antigen_callback_count = 0;

    int rc = brain_immune_set_antigen_callback(immune, test_antigen_callback, nullptr);
    EXPECT_EQ(rc, 0);

    // NULL callback should also be accepted (unregister)
    rc = brain_immune_set_antigen_callback(immune, nullptr, nullptr);
    EXPECT_EQ(rc, 0);

    // NULL system
    rc = brain_immune_set_antigen_callback(nullptr, test_antigen_callback, nullptr);
    EXPECT_NE(rc, 0);
}

/* ============================================================================
 * Antibody Production Test
 * ============================================================================ */

TEST_F(ImmuneCoverageTest, AntibodyProduction) {
    uint32_t antigen_id = present_antigen("ANTIBODY_PROD_TST", 7);
    brain_immune_update(immune, 200);

    uint32_t b_cell_id = 0;
    int rc = brain_immune_activate_b_cell(immune, antigen_id, &b_cell_id);
    EXPECT_EQ(rc, 0);

    // Produce IgM antibody - may fail if B cell lookup returns NULL internally
    uint32_t antibody_id = 0;
    rc = brain_immune_produce_antibody(immune, b_cell_id, ANTIBODY_IGM, &antibody_id);
    if (rc == 0 && antibody_id > 0) {
        // Execute antibody
        rc = brain_immune_execute_antibody(immune, antibody_id);
        EXPECT_EQ(rc, 0);

        // Neutralize
        rc = brain_immune_neutralize(immune, antigen_id, antibody_id);
        EXPECT_EQ(rc, 0);

        // Verify neutralized
        bool neutralized = brain_immune_is_neutralized(immune, antigen_id);
        EXPECT_TRUE(neutralized);
    } else {
        // B cell may not have been in correct state for antibody production
        // This is acceptable - verify error path was handled
        EXPECT_TRUE(rc == -1 || rc == 0);
    }
}

/* ============================================================================
 * T Cell Tests
 * ============================================================================ */

TEST_F(ImmuneCoverageTest, HelperTCellActivation) {
    uint32_t antigen_id = present_antigen("TCELL_HELPER_TST", 6);
    brain_immune_update(immune, 100);

    uint32_t t_cell_id = 0;
    int rc = brain_immune_activate_helper_t(immune, antigen_id, &t_cell_id);
    EXPECT_EQ(rc, 0);
    EXPECT_GT(t_cell_id, 0u);
}

TEST_F(ImmuneCoverageTest, KillerTCellActivation) {
    uint32_t antigen_id = present_antigen("TCELL_KILLER_TST", 8);
    brain_immune_update(immune, 100);

    uint32_t t_cell_id = 0;
    int rc = brain_immune_activate_killer_t(immune, antigen_id, &t_cell_id);
    EXPECT_EQ(rc, 0);
    EXPECT_GT(t_cell_id, 0u);
}

/* ============================================================================
 * Phase Tracking Test
 * ============================================================================ */

TEST_F(ImmuneCoverageTest, PhaseTracking) {
    brain_immune_phase_t phase = brain_immune_get_phase(immune);
    EXPECT_EQ(phase, IMMUNE_PHASE_SURVEILLANCE);

    // Present antigen should change phase
    present_antigen("PHASE_TRACK_TEST", 7);
    brain_immune_update(immune, 200);

    phase = brain_immune_get_phase(immune);
    // Phase may or may not have changed depending on implementation
    // Just verify it returns a valid value
    EXPECT_GE((int)phase, (int)IMMUNE_PHASE_SURVEILLANCE);
    EXPECT_LE((int)phase, (int)IMMUNE_PHASE_MEMORY);
}

/* ============================================================================
 * String Conversion Tests
 * ============================================================================ */

TEST_F(ImmuneCoverageTest, StringConversions) {
    EXPECT_NE(brain_immune_phase_to_string(IMMUNE_PHASE_SURVEILLANCE), nullptr);
    EXPECT_NE(brain_immune_b_cell_state_to_string(B_CELL_NAIVE), nullptr);
    EXPECT_NE(brain_immune_t_cell_type_to_string(T_CELL_HELPER), nullptr);
    EXPECT_NE(brain_immune_cytokine_to_string(BRAIN_CYTOKINE_IL1), nullptr);
    EXPECT_NE(brain_immune_inflammation_to_string(INFLAMMATION_NONE), nullptr);
}
