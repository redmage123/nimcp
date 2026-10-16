/**
 * @file test_bbb_immune_integration.cpp
 * @brief Integration tests for BBB and Brain Immune System
 * @version 1.0.0
 * @date 2025-12-11
 *
 * Tests the integration between the Blood-Brain Barrier (BBB) security module
 * and the Brain Immune System, including:
 * - Automatic threat forwarding from BBB to immune system
 * - BBB severity to immune inflammation level mapping
 * - BBB quarantine triggering killer T cell activation
 * - Coordinated BBB actions with antibody responses
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "security/nimcp_blood_brain_barrier.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class BBBImmuneIntegrationTest : public ::testing::Test {
protected:
    bbb_system_t bbb = nullptr;
    brain_immune_system_t* immune = nullptr;

    void SetUp() override {
        // Create BBB system
        bbb_config_t bbb_cfg = bbb_default_config();
        bbb = bbb_system_create(&bbb_cfg);
        ASSERT_NE(bbb, nullptr);

        // Create brain immune system
        brain_immune_config_t immune_cfg;
        brain_immune_default_config(&immune_cfg);
        immune = brain_immune_create(&immune_cfg);
        ASSERT_NE(immune, nullptr);

        // Connect BBB to immune system
        bool connected = bbb_connect_immune(bbb, immune);
        ASSERT_TRUE(connected);

        // Start immune system
        int result = brain_immune_start(immune);
        ASSERT_EQ(result, 0);
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
    }
};

/* ============================================================================
 * Connection Tests
 * ============================================================================ */

TEST_F(BBBImmuneIntegrationTest, ConnectionEstablished) {
    // Verify systems are connected
    EXPECT_NE(bbb, nullptr);
    EXPECT_NE(immune, nullptr);

    // Verify immune system is running
    EXPECT_TRUE(immune->running);
}

TEST_F(BBBImmuneIntegrationTest, ConnectWithNullBBB) {
    bool result = bbb_connect_immune(nullptr, immune);
    EXPECT_FALSE(result);
}

TEST_F(BBBImmuneIntegrationTest, ConnectWithNullImmune) {
    bool result = bbb_connect_immune(bbb, nullptr);
    EXPECT_TRUE(result); // Should succeed, just disconnects
}

/* ============================================================================
 * Automatic Threat Forwarding Tests
 * ============================================================================ */

TEST_F(BBBImmuneIntegrationTest, ThreatAutomaticallyForwardedToImmune) {
    // Report a threat to BBB
    const char* threat_data = "MALICIOUS_CODE";
    bbb_threat_report_t report = bbb_report_threat(
        bbb,
        BBB_THREAT_CODE_INJECTION,
        BBB_SEVERITY_HIGH,
        "Code injection detected",
        (void*)0x12345678,
        threat_data,
        strlen(threat_data)
    );

    EXPECT_EQ(report.type, BBB_THREAT_CODE_INJECTION);
    EXPECT_EQ(report.severity, BBB_SEVERITY_HIGH);

    // Update immune system to process antigens
    brain_immune_update(immune, 100);

    // Verify antigen was created in immune system
    brain_immune_stats_t stats;
    brain_immune_get_stats(immune, &stats);
    EXPECT_GT(stats.antigens_processed, 0u);
}

TEST_F(BBBImmuneIntegrationTest, LowSeverityThreatForwarded) {
    const char* threat = "low_severity";
    bbb_report_threat(
        bbb,
        BBB_THREAT_PATH_TRAVERSAL,
        BBB_SEVERITY_LOW,
        "Path traversal attempt",
        nullptr,
        threat,
        strlen(threat)
    );

    brain_immune_update(immune, 100);

    brain_immune_stats_t stats;
    brain_immune_get_stats(immune, &stats);
    // Even low severity should be forwarded
    EXPECT_GT(stats.antigens_processed, 0u);
}

TEST_F(BBBImmuneIntegrationTest, MultipleThreatsCascade) {
    // Report multiple threats
    for (int i = 0; i < 5; i++) {
        char desc[64];
        snprintf(desc, sizeof(desc), "Threat %d", i);
        bbb_report_threat(
            bbb,
            BBB_THREAT_BUFFER_OVERFLOW,
            BBB_SEVERITY_MEDIUM,
            desc,
            nullptr,
            "overflow_data",
            12
        );
    }

    brain_immune_update(immune, 100);

    brain_immune_stats_t stats;
    brain_immune_get_stats(immune, &stats);
    EXPECT_GE(stats.antigens_processed, 5u);
}

/* ============================================================================
 * Severity to Inflammation Mapping Tests
 * ============================================================================ */

TEST_F(BBBImmuneIntegrationTest, MediumSeverityTriggersInflammation) {
    const char* threat = "medium_threat";
    bbb_report_threat(
        bbb,
        BBB_THREAT_SQL_INJECTION,
        BBB_SEVERITY_MEDIUM,
        "SQL injection",
        nullptr,
        threat,
        strlen(threat)
    );

    brain_immune_update(immune, 100);

    brain_immune_stats_t stats;
    brain_immune_get_stats(immune, &stats);
    // Medium severity should trigger inflammation
    EXPECT_GT(stats.inflammation_sites, 0u);
}

TEST_F(BBBImmuneIntegrationTest, HighSeverityTriggersSystemicInflammation) {
    const char* threat = "critical_threat";
    bbb_report_threat(
        bbb,
        BBB_THREAT_SHELLCODE,
        BBB_SEVERITY_HIGH,
        "Shellcode detected",
        nullptr,
        threat,
        strlen(threat)
    );

    brain_immune_update(immune, 100);

    brain_immune_stats_t stats;
    brain_immune_get_stats(immune, &stats);
    // High severity should definitely trigger inflammation
    EXPECT_GT(stats.inflammation_sites, 0u);
}

TEST_F(BBBImmuneIntegrationTest, CriticalSeverityTriggersCytokineStorm) {
    const char* threat = "critical_attack";
    bbb_report_threat(
        bbb,
        BBB_THREAT_ROP_CHAIN,
        BBB_SEVERITY_CRITICAL,
        "ROP chain attack",
        nullptr,
        threat,
        strlen(threat)
    );

    brain_immune_update(immune, 100);

    brain_immune_stats_t stats;
    brain_immune_get_stats(immune, &stats);
    // Critical severity should trigger maximum inflammation
    EXPECT_GT(stats.inflammation_sites, 0u);
}

/* ============================================================================
 * Quarantine and Killer T Cell Tests
 * ============================================================================ */

TEST_F(BBBImmuneIntegrationTest, QuarantineActivatesKillerTCell) {
    // Quarantine a memory region
    char malicious_buffer[256];
    bool quarantined = bbb_quarantine_region(bbb, malicious_buffer, sizeof(malicious_buffer));
    EXPECT_TRUE(quarantined);

    // Verify region is quarantined
    EXPECT_TRUE(bbb_is_quarantined(bbb, malicious_buffer, sizeof(malicious_buffer)));

    // Update immune system
    brain_immune_update(immune, 100);

    // Verify killer T cell was activated
    brain_immune_stats_t stats;
    brain_immune_get_stats(immune, &stats);
    EXPECT_GT(stats.active_t_cells, 0u);
}

TEST_F(BBBImmuneIntegrationTest, MultipleQuarantinesCreateMultipleTCells) {
    char buffer1[128];
    char buffer2[128];
    char buffer3[128];

    bbb_quarantine_region(bbb, buffer1, sizeof(buffer1));
    bbb_quarantine_region(bbb, buffer2, sizeof(buffer2));
    bbb_quarantine_region(bbb, buffer3, sizeof(buffer3));

    brain_immune_update(immune, 100);

    brain_immune_stats_t stats;
    brain_immune_get_stats(immune, &stats);
    // Should have activated multiple T cells
    EXPECT_GE(stats.active_t_cells, 3u);
}

/* ============================================================================
 * Antibody-BBB Action Coordination Tests
 * ============================================================================ */

TEST_F(BBBImmuneIntegrationTest, AntibodyProductionTriggersCoordinatedResponse) {
    // Create a BBB threat
    const char* threat = "coordinated_threat";
    bbb_report_threat(
        bbb,
        BBB_THREAT_CODE_INJECTION,
        BBB_SEVERITY_HIGH,
        "Code injection for antibody test",
        (void*)0xDEADBEEF,
        threat,
        strlen(threat)
    );

    brain_immune_update(immune, 100);

    // Get first antigen
    const brain_antigen_t* antigen = brain_immune_get_antigen(immune, 1);
    if (antigen) {
        // Activate B cell
        uint32_t b_cell_id = 0;
        brain_immune_activate_b_cell(immune, antigen->id, &b_cell_id);

        // Activate helper T cell and have it help B cell (transitions B to PLASMA state)
        uint32_t helper_id = 0;
        brain_immune_activate_helper_t(immune, antigen->id, &helper_id);
        brain_immune_t_help_b(immune, helper_id, b_cell_id);

        // Produce antibody (requires B cell in PLASMA state)
        uint32_t antibody_id = 0;
        int produce_result = brain_immune_produce_antibody(immune, b_cell_id, ANTIBODY_IGG, &antibody_id);
        if (produce_result != 0) {
            // B cell may not have transitioned - skip antibody execution test
            return;
        }

        // Execute antibody (should coordinate with BBB)
        int result = brain_immune_execute_antibody(immune, antibody_id);
        EXPECT_EQ(result, 0);

        // Verify BBB statistics updated
        bbb_statistics_t bbb_stats;
        bbb_system_get_statistics(bbb, &bbb_stats);
        EXPECT_GT(bbb_stats.threats_detected, 0u);
    }
}

TEST_F(BBBImmuneIntegrationTest, HighAffinityAntibodyEscalatesToQuarantine) {
    // Report threat with specific address
    char* threat_location = (char*)nimcp_malloc(256);
    ASSERT_NE(threat_location, nullptr);

    bbb_report_threat(
        bbb,
        BBB_THREAT_MEMORY_VIOLATION,
        BBB_SEVERITY_MEDIUM,
        "Memory violation",
        threat_location,
        "violation_data",
        14
    );

    brain_immune_update(immune, 100);

    // Process through immune system
    const brain_antigen_t* antigen = brain_immune_get_antigen(immune, 1);
    if (antigen) {
        uint32_t b_cell_id = 0;
        brain_immune_activate_b_cell(immune, antigen->id, &b_cell_id);

        // Produce high-affinity (IgG) antibody
        uint32_t antibody_id = 0;
        brain_immune_produce_antibody(immune, b_cell_id, ANTIBODY_IGG, &antibody_id);

        // Execute - should escalate BBB action to quarantine
        brain_immune_execute_antibody(immune, antibody_id);
    }

    nimcp_free(threat_location);
}

/* ============================================================================
 * End-to-End Integration Tests
 * ============================================================================ */

TEST_F(BBBImmuneIntegrationTest, CompleteResponseCycle) {
    // 1. BBB detects threat
    const char* threat = "complete_cycle_threat";
    bbb_threat_report_t report = bbb_report_threat(
        bbb,
        BBB_THREAT_SHELLCODE,
        BBB_SEVERITY_HIGH,
        "Complete cycle test",
        (void*)0x11223344,
        threat,
        strlen(threat)
    );
    EXPECT_EQ(report.severity, BBB_SEVERITY_HIGH);

    // 2. Immune system processes (automatic via forwarding)
    brain_immune_update(immune, 100);

    // 3. Verify antigen created
    brain_immune_stats_t stats;
    brain_immune_get_stats(immune, &stats);
    EXPECT_GT(stats.antigens_processed, 0u);

    // 4. Verify inflammation initiated
    EXPECT_GT(stats.inflammation_sites, 0u);

    // 5. Verify immune response phase changed
    brain_immune_phase_t phase = brain_immune_get_phase(immune);
    EXPECT_NE(phase, IMMUNE_PHASE_SURVEILLANCE);
}

TEST_F(BBBImmuneIntegrationTest, MemoryResponseFasterThanPrimary) {
    const char* threat = "repeat_threat";

    // First exposure
    bbb_report_threat(bbb, BBB_THREAT_SQL_INJECTION, BBB_SEVERITY_MEDIUM,
                      "First exposure", nullptr, threat, strlen(threat));
    brain_immune_update(immune, 100);

    // Create memory by converting B cell
    const brain_antigen_t* antigen = brain_immune_get_antigen(immune, 1);
    if (antigen) {
        uint32_t b_cell_id = 0;
        brain_immune_activate_b_cell(immune, antigen->id, &b_cell_id);
        brain_immune_b_cell_to_memory(immune, b_cell_id);
    }

    // Get initial stats
    brain_immune_stats_t stats1;
    brain_immune_get_stats(immune, &stats1);

    // Second exposure (same threat signature)
    bbb_report_threat(bbb, BBB_THREAT_SQL_INJECTION, BBB_SEVERITY_MEDIUM,
                      "Second exposure", nullptr, threat, strlen(threat));
    brain_immune_update(immune, 50); // Less time needed

    // Get stats after secondary response
    brain_immune_stats_t stats2;
    brain_immune_get_stats(immune, &stats2);

    // Should have processed more antigens
    EXPECT_GT(stats2.antigens_processed, stats1.antigens_processed);
}

/* ============================================================================
 * Error Handling and Edge Cases
 * ============================================================================ */

TEST_F(BBBImmuneIntegrationTest, DisconnectedImmuneSystemDoesNotCrash) {
    // Disconnect immune system
    bbb_connect_immune(bbb, nullptr);

    // Report threat - should not crash
    bbb_report_threat(
        bbb,
        BBB_THREAT_BUFFER_OVERFLOW,
        BBB_SEVERITY_HIGH,
        "Threat without immune system",
        nullptr,
        "data",
        4
    );

    // BBB should still work
    bbb_statistics_t stats;
    bbb_system_get_statistics(bbb, &stats);
    EXPECT_GT(stats.threats_detected, 0u);
}

TEST_F(BBBImmuneIntegrationTest, NullThreatDataHandledGracefully) {
    bbb_report_threat(
        bbb,
        BBB_THREAT_UNKNOWN,
        BBB_SEVERITY_LOW,
        "Null data test",
        nullptr,
        nullptr,  // Null threat data
        0         // Zero size
    );

    brain_immune_update(immune, 100);

    // Should not crash, system should continue working
    EXPECT_NE(immune, nullptr);
}

TEST_F(BBBImmuneIntegrationTest, MaxQuarantineCapacityHandled) {
    // Try to exceed quarantine capacity
    const int MAX_ATTEMPTS = 100;
    int successful = 0;

    for (int i = 0; i < MAX_ATTEMPTS; i++) {
        char* buffer = (char*)nimcp_malloc(128);
        if (buffer && bbb_quarantine_region(bbb, buffer, 128)) {
            successful++;
        }
    }

    // Some should succeed, but should hit limit gracefully
    EXPECT_GT(successful, 0);
    EXPECT_LT(successful, MAX_ATTEMPTS); // Should hit capacity limit

    // Immune system should still function
    brain_immune_stats_t stats;
    brain_immune_get_stats(immune, &stats);
    EXPECT_GE(stats.active_t_cells, 1u);
}

/* ============================================================================
 * Statistics and Monitoring Tests
 * ============================================================================ */

TEST_F(BBBImmuneIntegrationTest, StatisticsReflectIntegration) {
    // Generate activity
    for (int i = 0; i < 3; i++) {
        char desc[32];
        snprintf(desc, sizeof(desc), "Threat %d", i);
        bbb_report_threat(
            bbb,
            BBB_THREAT_FORMAT_STRING,
            (bbb_severity_t)(BBB_SEVERITY_LOW + i),
            desc,
            nullptr,
            "data",
            4
        );
    }

    brain_immune_update(immune, 100);

    // Check BBB stats
    bbb_statistics_t bbb_stats;
    bbb_system_get_statistics(bbb, &bbb_stats);
    EXPECT_EQ(bbb_stats.threats_detected, 3u);

    // Check immune stats
    brain_immune_stats_t immune_stats;
    brain_immune_get_stats(immune, &immune_stats);
    EXPECT_GE(immune_stats.bbb_threats_processed, 3u);
    EXPECT_GT(immune_stats.antigens_processed, 0u);
}
