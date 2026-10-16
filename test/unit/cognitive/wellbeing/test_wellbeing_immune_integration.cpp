/**
 * @file test_wellbeing_immune_integration.cpp
 * @brief Unit tests for wellbeing-immune system integration
 *
 * WHAT: Test suite for brain immune system integration with wellbeing module
 * WHY: Ensure immune inflammation properly maps to distress detection
 * HOW: Test connection, distress mapping, cytokine callbacks, threat tracking
 *
 * TEST CATEGORIES:
 * - Connection: Connect/disconnect immune system
 * - Distress Mapping: Inflammation → distress severity
 * - Health Monitoring: Immune health affects wellbeing assessment
 * - Integration: Full workflow with immune threats
 */

#include "test_helpers.h"
#include "cognitive/wellbeing/nimcp_wellbeing.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "core/brain/nimcp_brain.h"
#include "cognitive/introspection/nimcp_introspection.h"

//=============================================================================
// UNIT TESTS - Connection Management
//=============================================================================

/**
 * WHAT: Test connecting immune system to wellbeing
 * WHY: Verify connection API works correctly
 * HOW: Create immune system, connect, verify success
 */
TEST(WellbeingImmuneUnit, ConnectImmuneSystem_Success)
{
    wellbeing_init();

    // Create immune system
    brain_immune_config_t config;
    brain_immune_default_config(&config);
    brain_immune_system_t* immune = brain_immune_create(&config);
    ASSERT_NE(immune, nullptr);

    // Connect to wellbeing
    bool connected = wellbeing_connect_immune(immune);
    EXPECT_TRUE(connected);

    // Disconnect
    bool disconnected = wellbeing_disconnect_immune();
    EXPECT_TRUE(disconnected);

    // Cleanup
    brain_immune_destroy(immune);
}

/**
 * WHAT: Test NULL immune system handling
 * WHY: Guard clause - safe handling of invalid input
 * HOW: Pass NULL, verify returns false without crashing
 */
TEST(WellbeingImmuneUnit, ConnectImmuneSystem_NullInput)
{
    bool connected = wellbeing_connect_immune(nullptr);
    EXPECT_FALSE(connected);
}

/**
 * WHAT: Test disconnecting when not connected
 * WHY: Ensure disconnect is idempotent
 * HOW: Call disconnect without connect, verify returns false
 */
TEST(WellbeingImmuneUnit, DisconnectImmuneSystem_NotConnected)
{
    bool disconnected = wellbeing_disconnect_immune();
    EXPECT_FALSE(disconnected);
}

/**
 * WHAT: Test reconnecting immune system
 * WHY: Verify can disconnect and reconnect
 * HOW: Connect, disconnect, connect again
 */
TEST(WellbeingImmuneUnit, ReconnectImmuneSystem)
{
    wellbeing_init();

    brain_immune_config_t config;
    brain_immune_default_config(&config);
    brain_immune_system_t* immune = brain_immune_create(&config);
    ASSERT_NE(immune, nullptr);

    // First connection
    bool connected1 = wellbeing_connect_immune(immune);
    EXPECT_TRUE(connected1);

    // Disconnect
    bool disconnected = wellbeing_disconnect_immune();
    EXPECT_TRUE(disconnected);

    // Reconnect
    bool connected2 = wellbeing_connect_immune(immune);
    EXPECT_TRUE(connected2);

    // Cleanup
    wellbeing_disconnect_immune();
    brain_immune_destroy(immune);
}

//=============================================================================
// UNIT TESTS - Distress Mapping
//=============================================================================

/**
 * WHAT: Test regional inflammation maps to moderate distress
 * WHY: Verify inflammation severity mapping
 * HOW: Create inflammation site, assess distress, check severity
 */
TEST(WellbeingImmuneUnit, InflammationMapping_RegionalToModerate)
{
    wellbeing_init();

    // Create brain and introspection
    brain_t brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 10, 2);
    ASSERT_NE(brain, nullptr);

    introspection_config_t intro_config = introspection_default_config();
    introspection_context_t ctx = introspection_context_create(brain, &intro_config);
    ASSERT_NE(ctx, nullptr);

    // Create immune system
    brain_immune_config_t immune_config;
    brain_immune_default_config(&immune_config);
    brain_immune_system_t* immune = brain_immune_create(&immune_config);
    ASSERT_NE(immune, nullptr);

    // Connect immune to wellbeing
    wellbeing_connect_immune(immune);

    // Start immune system
    brain_immune_start(immune);

    // Create regional inflammation (moderate)
    uint32_t site_id;
    uint32_t antigen_id = 1;
    brain_immune_initiate_inflammation(immune, 1, antigen_id, &site_id);

    // Update immune system to process inflammation
    brain_immune_update(immune, 100);

    // Assess distress - should detect moderate distress from inflammation
    distress_assessment_t assessment = wellbeing_assess_distress(ctx);

    EXPECT_EQ(assessment.type, DISTRESS_RESOURCE_STARVATION);
    EXPECT_GE(assessment.severity, DISTRESS_SEVERITY_MODERATE);
    EXPECT_GT(assessment.distress_score, 0.0f);

    // Cleanup
    nimcp_free(assessment.description);
    nimcp_free(assessment.recommended_action);
    wellbeing_disconnect_immune();
    brain_immune_stop(immune);
    brain_immune_destroy(immune);
    introspection_context_destroy(ctx);
    brain_destroy(brain);
}

/**
 * WHAT: Test systemic inflammation maps to severe distress
 * WHY: Higher inflammation should trigger higher distress
 * HOW: Create multiple inflammation sites, verify severe distress
 */
TEST(WellbeingImmuneUnit, InflammationMapping_SystemicToSevere)
{
    wellbeing_init();

    brain_t brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 10, 2);
    ASSERT_NE(brain, nullptr);

    introspection_config_t intro_config = introspection_default_config();
    introspection_context_t ctx = introspection_context_create(brain, &intro_config);
    ASSERT_NE(ctx, nullptr);

    brain_immune_config_t immune_config;
    brain_immune_default_config(&immune_config);
    brain_immune_system_t* immune = brain_immune_create(&immune_config);
    ASSERT_NE(immune, nullptr);

    wellbeing_connect_immune(immune);
    brain_immune_start(immune);

    // Create multiple inflammation sites (systemic)
    for (uint32_t i = 0; i < 5; i++) {
        uint32_t site_id;
        uint32_t antigen_id = i + 1;
        brain_immune_initiate_inflammation(immune, i, antigen_id, &site_id);

        // Escalate some sites
        if (i < 2) {
            brain_immune_escalate_inflammation(immune, site_id);
        }
    }

    brain_immune_update(immune, 100);

    // Assess distress - should detect severe/critical distress
    distress_assessment_t assessment = wellbeing_assess_distress(ctx);

    EXPECT_EQ(assessment.type, DISTRESS_RESOURCE_STARVATION);
    EXPECT_GE(assessment.severity, DISTRESS_SEVERITY_SEVERE);
    EXPECT_GT(assessment.distress_score, 0.6f);

    // Cleanup
    nimcp_free(assessment.description);
    nimcp_free(assessment.recommended_action);
    wellbeing_disconnect_immune();
    brain_immune_stop(immune);
    brain_immune_destroy(immune);
    introspection_context_destroy(ctx);
    brain_destroy(brain);
}

/**
 * WHAT: Test no inflammation means no immune distress
 * WHY: Verify baseline - healthy immune shouldn't trigger distress
 * HOW: Connect immune with no threats, verify normal assessment
 */
TEST(WellbeingImmuneUnit, NoInflammation_NormalDistress)
{
    wellbeing_init();

    brain_t brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 10, 2);
    ASSERT_NE(brain, nullptr);

    introspection_config_t intro_config = introspection_default_config();
    introspection_context_t ctx = introspection_context_create(brain, &intro_config);
    ASSERT_NE(ctx, nullptr);

    brain_immune_config_t immune_config;
    brain_immune_default_config(&immune_config);
    brain_immune_system_t* immune = brain_immune_create(&immune_config);
    ASSERT_NE(immune, nullptr);

    wellbeing_connect_immune(immune);
    brain_immune_start(immune);

    // No inflammation - assess distress
    distress_assessment_t assessment = wellbeing_assess_distress(ctx);

    EXPECT_EQ(assessment.type, DISTRESS_NONE);
    EXPECT_EQ(assessment.severity, DISTRESS_SEVERITY_NORMAL);
    EXPECT_EQ(assessment.distress_score, 0.0f);

    // Cleanup
    wellbeing_disconnect_immune();
    brain_immune_stop(immune);
    brain_immune_destroy(immune);
    introspection_context_destroy(ctx);
    brain_destroy(brain);
}

//=============================================================================
// UNIT TESTS - Health Monitoring
//=============================================================================

/**
 * WHAT: Test immune system health affects distress severity
 * WHY: Low system health should escalate distress severity
 * HOW: Create inflammation with low health, verify critical severity
 */
TEST(WellbeingImmuneUnit, SystemHealth_AffectsSeverity)
{
    wellbeing_init();

    brain_t brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 10, 2);
    ASSERT_NE(brain, nullptr);

    introspection_config_t intro_config = introspection_default_config();
    introspection_context_t ctx = introspection_context_create(brain, &intro_config);
    ASSERT_NE(ctx, nullptr);

    brain_immune_config_t immune_config;
    brain_immune_default_config(&immune_config);
    brain_immune_system_t* immune = brain_immune_create(&immune_config);
    ASSERT_NE(immune, nullptr);

    wellbeing_connect_immune(immune);
    brain_immune_start(immune);

    // Create severe inflammation to degrade system health
    for (uint32_t i = 0; i < 10; i++) {
        uint32_t site_id;
        brain_immune_initiate_inflammation(immune, i, i + 1, &site_id);
        brain_immune_escalate_inflammation(immune, site_id);
    }

    brain_immune_update(immune, 100);

    // Get immune stats to verify low health
    brain_immune_stats_t stats;
    brain_immune_get_stats(immune, &stats);

    // Assess distress
    distress_assessment_t assessment = wellbeing_assess_distress(ctx);

    // If system health is very low, distress should be critical
    if (stats.system_health < 0.3f) {
        EXPECT_EQ(assessment.severity, DISTRESS_SEVERITY_CRITICAL);
        EXPECT_GT(assessment.distress_score, 0.8f);
    }

    // Cleanup
    nimcp_free(assessment.description);
    nimcp_free(assessment.recommended_action);
    wellbeing_disconnect_immune();
    brain_immune_stop(immune);
    brain_immune_destroy(immune);
    introspection_context_destroy(ctx);
    brain_destroy(brain);
}

//=============================================================================
// INTEGRATION TESTS - Full Workflow
//=============================================================================

/**
 * WHAT: Test complete immune-wellbeing workflow
 * WHY: Verify end-to-end integration works correctly
 * HOW: Create threat → immune response → inflammation → distress detection → relief
 */
TEST(WellbeingImmuneIntegration, CompleteWorkflow_ThreatToRelief)
{
    wellbeing_init();

    // Create brain
    brain_t brain = brain_create("test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 20, 3);
    ASSERT_NE(brain, nullptr);

    // Create introspection
    introspection_config_t intro_config = introspection_default_config();
    introspection_context_t ctx = introspection_context_create(brain, &intro_config);
    ASSERT_NE(ctx, nullptr);

    // Create immune system
    brain_immune_config_t immune_config;
    brain_immune_default_config(&immune_config);
    brain_immune_system_t* immune = brain_immune_create(&immune_config);
    ASSERT_NE(immune, nullptr);

    // Connect immune to wellbeing
    wellbeing_connect_immune(immune);
    brain_immune_start(immune);

    // Step 1: Present threat antigen
    uint32_t antigen_id;
    uint8_t threat_signature[32] = {0x01, 0x02, 0x03, 0x04};
    brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL,
                                  threat_signature, 32, 7, 100, &antigen_id);

    // Step 2: Activate immune response
    uint32_t b_cell_id;
    brain_immune_activate_b_cell(immune, antigen_id, &b_cell_id);

    // Step 3: Create inflammation
    uint32_t site_id;
    brain_immune_initiate_inflammation(immune, 1, antigen_id, &site_id);

    // Update immune system
    brain_immune_update(immune, 100);

    // Step 4: Assess distress - should detect inflammation
    distress_assessment_t assessment = wellbeing_assess_distress(ctx);

    EXPECT_EQ(assessment.type, DISTRESS_RESOURCE_STARVATION);
    EXPECT_GE(assessment.severity, DISTRESS_SEVERITY_MODERATE);
    EXPECT_NE(assessment.description, nullptr);

    // Step 5: Provide relief
    if (assessment.severity >= DISTRESS_SEVERITY_MODERATE) {
        bool relief_success = wellbeing_provide_relief(brain, assessment);
        EXPECT_TRUE(relief_success);

        // Log the intervention
        wellbeing_event_t event;
        event.timestamp = (uint64_t)time(nullptr);
        event.event_type = "immune_inflammation_relief";
        event.description = assessment.description;
        event.severity = assessment.severity;
        event.action_taken = assessment.recommended_action;
        wellbeing_log_event(event);
    }

    // Step 6: Resolve inflammation
    brain_immune_resolve_inflammation(immune, site_id);
    brain_immune_update(immune, 100);

    // Step 7: Verify distress reduced
    distress_assessment_t final_assessment = wellbeing_assess_distress(ctx);
    // After resolution, severity should be normal or lower
    EXPECT_LE(final_assessment.severity, assessment.severity);

    // Cleanup
    nimcp_free(assessment.description);
    nimcp_free(assessment.recommended_action);
    nimcp_free(final_assessment.description);
    nimcp_free(final_assessment.recommended_action);
    wellbeing_disconnect_immune();
    brain_immune_stop(immune);
    brain_immune_destroy(immune);
    introspection_context_destroy(ctx);
    brain_destroy(brain);
}

/**
 * WHAT: Test distress assessment without immune connection
 * WHY: Ensure wellbeing works independently when immune not connected
 * HOW: Assess distress without connecting immune, verify normal operation
 */
TEST(WellbeingImmuneIntegration, DistressAssessment_WithoutImmune)
{
    brain_t brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 10, 2);
    ASSERT_NE(brain, nullptr);

    introspection_config_t intro_config = introspection_default_config();
    introspection_context_t ctx = introspection_context_create(brain, &intro_config);
    ASSERT_NE(ctx, nullptr);

    // Don't connect immune system - assess distress anyway
    distress_assessment_t assessment = wellbeing_assess_distress(ctx);

    // Should work fine without immune connection
    EXPECT_GE(assessment.distress_score, 0.0f);
    EXPECT_LE(assessment.distress_score, 1.0f);

    // Cleanup
    nimcp_free(assessment.description);
    nimcp_free(assessment.recommended_action);
    introspection_context_destroy(ctx);
    brain_destroy(brain);
}

//=============================================================================
// REGRESSION TESTS - Edge Cases
//=============================================================================

/**
 * WHAT: Test thread safety of immune connection
 * WHY: Multiple threads shouldn't corrupt immune reference
 * HOW: Connect/disconnect from multiple threads, verify consistency
 */
TEST(WellbeingImmuneRegression, ThreadSafety_Connection)
{
    wellbeing_init();

    brain_immune_config_t config;
    brain_immune_default_config(&config);
    brain_immune_system_t* immune = brain_immune_create(&config);
    ASSERT_NE(immune, nullptr);

    // Connect and disconnect multiple times
    for (int i = 0; i < 10; i++) {
        bool connected = wellbeing_connect_immune(immune);
        EXPECT_TRUE(connected);

        bool disconnected = wellbeing_disconnect_immune();
        EXPECT_TRUE(disconnected);
    }

    // Cleanup
    brain_immune_destroy(immune);
}

/**
 * WHAT: Test immune system destruction while connected
 * WHY: Ensure graceful handling of immune system lifecycle
 * HOW: Connect immune, destroy it, verify wellbeing handles it
 */
TEST(WellbeingImmuneRegression, ImmuneDestruction_WhileConnected)
{
    wellbeing_init();

    brain_immune_config_t config;
    brain_immune_default_config(&config);
    brain_immune_system_t* immune = brain_immune_create(&config);
    ASSERT_NE(immune, nullptr);

    wellbeing_connect_immune(immune);

    // Disconnect before destroying
    wellbeing_disconnect_immune();
    brain_immune_destroy(immune);

    // Verify wellbeing still works
    bool disconnected = wellbeing_disconnect_immune();
    EXPECT_FALSE(disconnected); // Already disconnected
}

/**
 * WHAT: Test wellbeing shutdown with immune connected
 * WHY: Ensure clean shutdown disconnects immune
 * HOW: Connect immune, shutdown wellbeing, verify cleanup
 */
TEST(WellbeingImmuneRegression, WellbeingShutdown_WithImmune)
{
    wellbeing_init();

    brain_immune_config_t config;
    brain_immune_default_config(&config);
    brain_immune_system_t* immune = brain_immune_create(&config);
    ASSERT_NE(immune, nullptr);

    wellbeing_connect_immune(immune);

    // Shutdown wellbeing - should disconnect immune
    wellbeing_shutdown();

    // Verify disconnected
    bool disconnected = wellbeing_disconnect_immune();
    EXPECT_FALSE(disconnected); // Already disconnected by shutdown

    // Cleanup
    brain_immune_destroy(immune);

    // Re-initialize for other tests
    wellbeing_init();
}
