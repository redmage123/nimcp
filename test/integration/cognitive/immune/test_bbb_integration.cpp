/**
 * @file test_bbb_integration.cpp
 * @brief Integration test for BBB-Immune coordination
 * @version 1.0.0
 * @date 2025-12-11
 *
 * Verifies the integration between BBB and Brain Immune System.
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "security/nimcp_blood_brain_barrier.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "utils/memory/nimcp_memory.h"

/**
 * @brief Basic integration test
 *
 * WHAT: Verify BBB and immune system can be connected
 * WHY:  Ensure basic integration works
 * HOW:  Create both systems and connect them
 */
TEST(BBBImmuneIntegration, BasicConnection) {
    // Create BBB
    bbb_config_t bbb_cfg = bbb_default_config();
    bbb_system_t bbb = bbb_system_create(&bbb_cfg);
    ASSERT_NE(bbb, nullptr);

    // Create immune system
    brain_immune_config_t immune_cfg;
    brain_immune_default_config(&immune_cfg);
    brain_immune_system_t* immune = brain_immune_create(&immune_cfg);
    ASSERT_NE(immune, nullptr);

    // Connect them
    bool connected = bbb_connect_immune(bbb, immune);
    EXPECT_TRUE(connected);

    // Cleanup
    brain_immune_destroy(immune);
    bbb_system_destroy(bbb);
}

/**
 * @brief Test threat forwarding
 *
 * WHAT: Verify BBB threats are forwarded to immune
 * WHY:  Core integration feature
 * HOW:  Report BBB threat, check immune system response
 */
TEST(BBBImmuneIntegration, ThreatForwarding) {
    // Setup
    bbb_system_t bbb = bbb_system_create(nullptr);
    brain_immune_system_t* immune = brain_immune_create(nullptr);
    brain_immune_start(immune);
    bbb_connect_immune(bbb, immune);

    // Report threat
    const char* threat = "test_threat";
    bbb_report_threat(
        bbb,
        BBB_THREAT_CODE_INJECTION,
        BBB_SEVERITY_HIGH,
        "Test threat",
        nullptr,
        threat,
        strlen(threat)
    );

    // Update immune system
    brain_immune_update(immune, 100);

    // Verify
    brain_immune_stats_t stats;
    brain_immune_get_stats(immune, &stats);
    EXPECT_GT(stats.antigens_processed, 0u);

    // Cleanup
    brain_immune_stop(immune);
    brain_immune_destroy(immune);
    bbb_system_destroy(bbb);
}
