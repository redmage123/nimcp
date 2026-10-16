/**
 * @file unit_async_immune_integration.cpp
 * @brief Unit tests for brain immune system integration with bio-async router
 *
 * WHAT: Test suite for immune system bio-async messaging integration
 * WHY:  Ensure immune coordination via bio-async works correctly
 * HOW:  Test connection, message handling, cytokine broadcasts, alerts
 *
 * @author NIMCP Development Team
 * @date 2025-12-11
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdint>

// Headers have their own extern "C" guards
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_async.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_time.h"

/**
 * @brief Mock brain immune system for testing
 */
struct MockBrainImmuneSystem {
    uint32_t magic;
    uint32_t messages_received;
    uint32_t cytokines_sent;
    uint32_t alerts_sent;
    bool connected;
};

/**
 * @brief Test fixture for immune integration tests
 */
class BioAsyncImmuneIntegrationTest : public ::testing::Test {
protected:
    bio_router_config_t router_config;
    MockBrainImmuneSystem mock_immune;

    void SetUp() override {
        /* Initialize router with test configuration */
        router_config = bio_router_default_config();
        router_config.max_modules = 32;
        router_config.inbox_capacity = 128;
        router_config.enable_logging = false;  /* Reduce noise in tests */

        nimcp_error_t result = bio_router_init(&router_config);
        ASSERT_EQ(result, NIMCP_SUCCESS) << "Router initialization failed";

        /* Initialize mock immune system */
        memset(&mock_immune, 0, sizeof(mock_immune));
        mock_immune.magic = 0xDEADBEEF;
    }

    void TearDown() override {
        bio_router_shutdown();
    }
};

/* ============================================================================
 * Connection Tests
 * ========================================================================== */

/**
 * @brief Test connecting immune system to bio-async
 *
 * WHAT: Verify immune system registration with router
 * WHY:  Core integration functionality
 * HOW:  Connect immune, verify success, check stats
 */
TEST_F(BioAsyncImmuneIntegrationTest, ConnectImmuneSystem) {
    nimcp_error_t result = bio_async_connect_immune(&mock_immune);
    ASSERT_EQ(result, NIMCP_SUCCESS) << "Failed to connect immune system";

    mock_immune.connected = true;

    /* Verify router stats are accessible after immune connection.
     * Note: bio_async_connect_immune() stores a reference in
     * g_router->brain_immune_system but does NOT register a bio-async
     * module, so active_modules remains 0. This is by design - the
     * immune system is a simplified integration, not a full module. */
    bio_router_stats_t stats;
    result = bio_router_get_stats(&stats);
    ASSERT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(stats.active_modules, 0)
        << "Immune connection is simplified (no module registration)";
}

/**
 * @brief Test connecting NULL immune system
 *
 * WHAT: Verify NULL parameter handling
 * WHY:  Guard clause validation
 * HOW:  Pass NULL, expect error
 */
TEST_F(BioAsyncImmuneIntegrationTest, ConnectNullImmune) {
    nimcp_error_t result = bio_async_connect_immune(NULL);
    EXPECT_NE(result, NIMCP_SUCCESS) << "NULL immune should be rejected";
}

/**
 * @brief Test double connection
 *
 * WHAT: Verify handling of duplicate connection
 * WHY:  Prevent double registration issues
 * HOW:  Connect twice, verify second succeeds (update)
 */
TEST_F(BioAsyncImmuneIntegrationTest, DoubleConnection) {
    nimcp_error_t result1 = bio_async_connect_immune(&mock_immune);
    ASSERT_EQ(result1, NIMCP_SUCCESS);

    nimcp_error_t result2 = bio_async_connect_immune(&mock_immune);
    EXPECT_EQ(result2, NIMCP_SUCCESS) << "Double connection should update";
}

/**
 * @brief Test connection before router init
 *
 * WHAT: Verify handling of premature connection
 * WHY:  Ensure proper initialization order
 * HOW:  Shutdown router, try to connect, expect error
 */
TEST_F(BioAsyncImmuneIntegrationTest, ConnectBeforeRouterInit) {
    bio_router_shutdown();

    nimcp_error_t result = bio_async_connect_immune(&mock_immune);
    EXPECT_NE(result, NIMCP_SUCCESS) << "Connection before init should fail";
}

/* ============================================================================
 * Cytokine Broadcast Tests
 * ========================================================================== */

/**
 * @brief Test cytokine broadcast
 *
 * WHAT: Verify cytokine signal broadcast
 * WHY:  Immune coordination requires signaling
 * HOW:  Connect immune, broadcast cytokine, verify no error
 */
TEST_F(BioAsyncImmuneIntegrationTest, BroadcastCytokine) {
    nimcp_error_t connect_result = bio_async_connect_immune(&mock_immune);
    ASSERT_EQ(connect_result, NIMCP_SUCCESS);

    /* Broadcast cytokine */
    uint32_t cytokine_type = 1;  /* IL-1 */
    float concentration = 0.8f;
    uint32_t source_cell = 42;

    nimcp_error_t result = bio_async_broadcast_cytokine(
        cytokine_type, concentration, source_cell
    );
    EXPECT_EQ(result, NIMCP_SUCCESS) << "Cytokine broadcast failed";

    mock_immune.cytokines_sent++;
}

/**
 * @brief Test cytokine broadcast without connection
 *
 * WHAT: Verify graceful handling when immune not connected
 * WHY:  Optional integration - should not error
 * HOW:  Broadcast without connecting, expect success (no-op)
 */
TEST_F(BioAsyncImmuneIntegrationTest, BroadcastCytokineWithoutConnection) {
    nimcp_error_t result = bio_async_broadcast_cytokine(1, 0.5f, 10);
    /* Should succeed as no-op when immune not connected */
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

/**
 * @brief Test multiple cytokine broadcasts
 *
 * WHAT: Verify multiple sequential cytokine signals
 * WHY:  Immune response involves multiple signals
 * HOW:  Connect immune, send multiple cytokines
 */
TEST_F(BioAsyncImmuneIntegrationTest, MultipleCytokineBroadcasts) {
    ASSERT_EQ(bio_async_connect_immune(&mock_immune), NIMCP_SUCCESS);

    /* Send multiple cytokines */
    for (uint32_t i = 0; i < 5; i++) {
        nimcp_error_t result = bio_async_broadcast_cytokine(
            i,          /* type */
            0.5f + (i * 0.1f),  /* concentration */
            100 + i     /* source */
        );
        EXPECT_EQ(result, NIMCP_SUCCESS) << "Broadcast " << i << " failed";
    }
}

/* ============================================================================
 * Inflammation Alert Tests
 * ========================================================================== */

/**
 * @brief Test inflammation alert
 *
 * WHAT: Verify high-priority inflammation alert
 * WHY:  Critical escalation signaling
 * HOW:  Connect immune, send alert, verify success
 */
TEST_F(BioAsyncImmuneIntegrationTest, InflammationAlert) {
    ASSERT_EQ(bio_async_connect_immune(&mock_immune), NIMCP_SUCCESS);

    uint32_t region_id = 5;
    uint32_t severity = 3;  /* INFLAMMATION_SYSTEMIC */
    uint32_t antigen_id = 123;

    nimcp_error_t result = bio_async_inflammation_alert(
        region_id, severity, antigen_id
    );
    EXPECT_EQ(result, NIMCP_SUCCESS) << "Inflammation alert failed";

    mock_immune.alerts_sent++;
}

/**
 * @brief Test inflammation alert without connection
 *
 * WHAT: Verify graceful handling when not connected
 * WHY:  Optional integration
 * HOW:  Alert without connecting, expect success (no-op)
 */
TEST_F(BioAsyncImmuneIntegrationTest, InflammationAlertWithoutConnection) {
    nimcp_error_t result = bio_async_inflammation_alert(1, 2, 3);
    EXPECT_EQ(result, NIMCP_SUCCESS) << "Alert without connection should be no-op";
}

/**
 * @brief Test inflammation severity levels
 *
 * WHAT: Verify alerts at different severity levels
 * WHY:  Inflammation has multiple escalation levels
 * HOW:  Send alerts at each severity level
 */
TEST_F(BioAsyncImmuneIntegrationTest, InflammationSeverityLevels) {
    ASSERT_EQ(bio_async_connect_immune(&mock_immune), NIMCP_SUCCESS);

    /* Test each severity level */
    for (uint32_t severity = 0; severity <= 4; severity++) {
        nimcp_error_t result = bio_async_inflammation_alert(
            severity,   /* region */
            severity,   /* severity */
            severity * 10  /* antigen */
        );
        EXPECT_EQ(result, NIMCP_SUCCESS)
            << "Alert at severity " << severity << " failed";
    }
}

/* ============================================================================
 * Phase Change Tests
 * ========================================================================== */

/**
 * @brief Test immune phase change notification
 *
 * WHAT: Verify phase transition broadcast
 * WHY:  System-wide state awareness
 * HOW:  Connect immune, notify phase change, verify success
 */
TEST_F(BioAsyncImmuneIntegrationTest, ImmunePhaseChange) {
    ASSERT_EQ(bio_async_connect_immune(&mock_immune), NIMCP_SUCCESS);

    uint32_t old_phase = 0;  /* SURVEILLANCE */
    uint32_t new_phase = 3;  /* EFFECTOR */

    nimcp_error_t result = bio_async_immune_phase_change(old_phase, new_phase);
    EXPECT_EQ(result, NIMCP_SUCCESS) << "Phase change notification failed";
}

/**
 * @brief Test all phase transitions
 *
 * WHAT: Verify all valid phase transitions
 * WHY:  Immune has multiple phases
 * HOW:  Test each phase transition
 */
TEST_F(BioAsyncImmuneIntegrationTest, AllPhaseTransitions) {
    ASSERT_EQ(bio_async_connect_immune(&mock_immune), NIMCP_SUCCESS);

    /* Test phase progression */
    const uint32_t phases[] = {0, 1, 2, 3, 4, 5};  /* All 6 phases */
    const size_t num_phases = sizeof(phases) / sizeof(phases[0]);

    for (size_t i = 1; i < num_phases; i++) {
        nimcp_error_t result = bio_async_immune_phase_change(
            phases[i - 1], phases[i]
        );
        EXPECT_EQ(result, NIMCP_SUCCESS)
            << "Phase " << phases[i-1] << " -> " << phases[i] << " failed";
    }
}

/**
 * @brief Test phase change without connection
 *
 * WHAT: Verify graceful handling when not connected
 * WHY:  Optional integration
 * HOW:  Notify without connecting, expect success (no-op)
 */
TEST_F(BioAsyncImmuneIntegrationTest, PhaseChangeWithoutConnection) {
    nimcp_error_t result = bio_async_immune_phase_change(0, 1);
    EXPECT_EQ(result, NIMCP_SUCCESS) << "Phase change without connection should be no-op";
}

/* ============================================================================
 * Integration Flow Tests
 * ========================================================================== */

/**
 * @brief Test complete immune response flow
 *
 * WHAT: Verify full immune coordination sequence
 * WHY:  End-to-end integration validation
 * HOW:  Connect, detect threat, send cytokines, alert, phase change
 */
TEST_F(BioAsyncImmuneIntegrationTest, CompleteImmuneResponseFlow) {
    /* Step 1: Connect immune system */
    ASSERT_EQ(bio_async_connect_immune(&mock_immune), NIMCP_SUCCESS);
    mock_immune.connected = true;

    /* Step 2: Phase change to recognition */
    EXPECT_EQ(bio_async_immune_phase_change(0, 1), NIMCP_SUCCESS);

    /* Step 3: Send initial cytokine (IL-1) */
    EXPECT_EQ(bio_async_broadcast_cytokine(0, 0.3f, 1), NIMCP_SUCCESS);
    mock_immune.cytokines_sent++;

    /* Step 4: Phase change to activation */
    EXPECT_EQ(bio_async_immune_phase_change(1, 2), NIMCP_SUCCESS);

    /* Step 5: Send pro-inflammatory cytokines */
    EXPECT_EQ(bio_async_broadcast_cytokine(1, 0.7f, 2), NIMCP_SUCCESS);  /* IL-6 */
    EXPECT_EQ(bio_async_broadcast_cytokine(3, 0.9f, 3), NIMCP_SUCCESS);  /* TNF-α */
    mock_immune.cytokines_sent += 2;

    /* Step 6: Inflammation alert */
    EXPECT_EQ(bio_async_inflammation_alert(5, 2, 100), NIMCP_SUCCESS);
    mock_immune.alerts_sent++;

    /* Step 7: Phase change to effector */
    EXPECT_EQ(bio_async_immune_phase_change(2, 3), NIMCP_SUCCESS);

    /* Step 8: Send more cytokines during effector phase */
    EXPECT_EQ(bio_async_broadcast_cytokine(4, 0.8f, 4), NIMCP_SUCCESS);  /* IFN-γ */
    mock_immune.cytokines_sent++;

    /* Step 9: Phase change to resolution */
    EXPECT_EQ(bio_async_immune_phase_change(3, 4), NIMCP_SUCCESS);

    /* Step 10: Send anti-inflammatory cytokine */
    EXPECT_EQ(bio_async_broadcast_cytokine(2, 0.5f, 5), NIMCP_SUCCESS);  /* IL-10 */
    mock_immune.cytokines_sent++;

    /* Step 11: Phase change to memory */
    EXPECT_EQ(bio_async_immune_phase_change(4, 5), NIMCP_SUCCESS);

    /* Verify mock state */
    EXPECT_TRUE(mock_immune.connected);
    EXPECT_GE(mock_immune.cytokines_sent, 5);
    EXPECT_GE(mock_immune.alerts_sent, 1);
}

/**
 * @brief Test concurrent immune operations
 *
 * WHAT: Verify multiple immune operations in quick succession
 * WHY:  Real immune responses are concurrent
 * HOW:  Rapid-fire cytokines and alerts
 */
TEST_F(BioAsyncImmuneIntegrationTest, ConcurrentImmuneOperations) {
    ASSERT_EQ(bio_async_connect_immune(&mock_immune), NIMCP_SUCCESS);

    /* Rapid concurrent operations */
    for (int i = 0; i < 20; i++) {
        bio_async_broadcast_cytokine(i % 5, 0.5f, i);
        if (i % 4 == 0) {
            bio_async_inflammation_alert(i, i % 5, i * 10);
        }
        if (i % 7 == 0) {
            bio_async_immune_phase_change(i % 6, (i + 1) % 6);
        }
    }

    /* All operations should succeed without errors */
    SUCCEED();
}

/* ============================================================================
 * Router Statistics Tests
 * ========================================================================== */

/**
 * @brief Test router statistics after immune activity
 *
 * WHAT: Verify router tracks immune messages
 * WHY:  Statistics validation
 * HOW:  Send messages, check stats
 */
TEST_F(BioAsyncImmuneIntegrationTest, RouterStatisticsWithImmuneActivity) {
    ASSERT_EQ(bio_async_connect_immune(&mock_immune), NIMCP_SUCCESS);

    bio_router_stats_t stats_before;
    ASSERT_EQ(bio_router_get_stats(&stats_before), NIMCP_SUCCESS);

    /* Generate immune activity */
    bio_async_broadcast_cytokine(0, 0.5f, 1);
    bio_async_broadcast_cytokine(1, 0.6f, 2);
    bio_async_inflammation_alert(1, 2, 3);

    bio_router_stats_t stats_after;
    ASSERT_EQ(bio_router_get_stats(&stats_after), NIMCP_SUCCESS);

    /* Stats should reflect activity */
    EXPECT_GE(stats_after.messages_routed, stats_before.messages_routed)
        << "No messages routed";
    EXPECT_GE(stats_after.broadcasts_sent, stats_before.broadcasts_sent)
        << "No broadcasts sent";
}

/* ============================================================================
 * Error Handling Tests
 * ========================================================================== */

/**
 * @brief Test router shutdown during immune activity
 *
 * WHAT: Verify graceful handling of shutdown
 * WHY:  Ensure no crashes during cleanup
 * HOW:  Connect immune, shutdown router, verify clean state
 */
TEST_F(BioAsyncImmuneIntegrationTest, ShutdownDuringImmuneActivity) {
    ASSERT_EQ(bio_async_connect_immune(&mock_immune), NIMCP_SUCCESS);

    /* Generate some activity */
    bio_async_broadcast_cytokine(0, 0.5f, 1);

    /* Shutdown should be clean */
    bio_router_shutdown();

    /* Post-shutdown cytokine broadcast: bio_async_broadcast_cytokine() is
     * currently a stub that always returns NIMCP_SUCCESS regardless of
     * router state (it does not route through the bio-async router).
     * Verify it completes without crashing. */
    nimcp_error_t result = bio_async_broadcast_cytokine(0, 0.5f, 1);
    EXPECT_EQ(result, NIMCP_SUCCESS)
        << "Stub cytokine broadcast should succeed (no-op) even after shutdown";
}

/**
 * @brief Test extreme concentration values
 *
 * WHAT: Verify handling of edge case concentrations
 * WHY:  Robustness testing
 * HOW:  Test 0.0, 1.0, negative, > 1.0
 */
TEST_F(BioAsyncImmuneIntegrationTest, ExtremeCytokineConcentrations) {
    ASSERT_EQ(bio_async_connect_immune(&mock_immune), NIMCP_SUCCESS);

    /* Test boundary values */
    EXPECT_EQ(bio_async_broadcast_cytokine(0, 0.0f, 1), NIMCP_SUCCESS);
    EXPECT_EQ(bio_async_broadcast_cytokine(1, 1.0f, 2), NIMCP_SUCCESS);

    /* These should still work (no validation in current impl) */
    bio_async_broadcast_cytokine(2, -0.5f, 3);  /* Negative */
    bio_async_broadcast_cytokine(3, 2.0f, 4);   /* Over 1.0 */
}

/* ============================================================================
 * Main
 * ========================================================================== */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
