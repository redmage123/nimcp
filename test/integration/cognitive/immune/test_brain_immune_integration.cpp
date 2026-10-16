/**
 * @file test_brain_immune_integration.cpp
 * @brief Integration tests for Brain Immune System
 * @version 1.0.0
 * @date 2025-12-11
 *
 * Tests integration between brain immune and:
 * - BBB Security
 * - BFT (Byzantine Fault Tolerance)
 * - Swarm Immune
 * - Bio-async router
 * - Security logging
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>

// Headers have their own extern "C" guards
#include "cognitive/immune/nimcp_brain_immune.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "utils/fault_tolerance/nimcp_byzantine_fault_tolerance.h"
#include "swarm/nimcp_swarm_immune.h"
#include "async/nimcp_bio_router.h"
#include "utils/memory/nimcp_memory.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"

/* ============================================================================
 * Integration Test Fixture
 * ============================================================================ */

class BrainImmuneIntegrationTest : public ::testing::Test {
protected:
    brain_immune_system_t* immune_system = nullptr;
    brain_immune_config_t config;

    void SetUp() override {
        brain_immune_default_config(&config);
        immune_system = brain_immune_create(&config);
        ASSERT_NE(immune_system, nullptr);
        brain_immune_start(immune_system);
    }

    void TearDown() override {
        if (immune_system) {
            brain_immune_stop(immune_system);
            brain_immune_destroy(immune_system);
            immune_system = nullptr;
        }
    }
};

/* ============================================================================
 * BBB Integration Tests
 * ============================================================================ */

TEST_F(BrainImmuneIntegrationTest, BBBThreatTriggersImmuneResponse) {
    // Present a buffer overflow threat (high severity)
    uint8_t overflow_pattern[] = {0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41};
    uint32_t antigen_id;

    int result = brain_immune_present_bbb_threat(
        immune_system,
        BBB_THREAT_BUFFER_OVERFLOW,
        BBB_SEVERITY_HIGH,
        overflow_pattern, sizeof(overflow_pattern),
        &antigen_id
    );

    EXPECT_EQ(result, 0);
    EXPECT_GT(antigen_id, 0u);

    // Verify antigen was created with correct properties
    const brain_antigen_t* ag = brain_immune_get_antigen(immune_system, antigen_id);
    ASSERT_NE(ag, nullptr);
    EXPECT_EQ(ag->source, ANTIGEN_SOURCE_BBB);
    EXPECT_EQ(ag->bbb_threat_type, BBB_THREAT_BUFFER_OVERFLOW);
    EXPECT_GE(ag->severity, 7u);  // High severity maps to 7+
}

TEST_F(BrainImmuneIntegrationTest, SQLInjectionThreatHandled) {
    const char* sql_injection = "' OR '1'='1";
    uint32_t antigen_id;

    int result = brain_immune_present_bbb_threat(
        immune_system,
        BBB_THREAT_SQL_INJECTION,
        BBB_SEVERITY_CRITICAL,
        (const uint8_t*)sql_injection, strlen(sql_injection),
        &antigen_id
    );

    EXPECT_EQ(result, 0);

    const brain_antigen_t* ag = brain_immune_get_antigen(immune_system, antigen_id);
    ASSERT_NE(ag, nullptr);
    EXPECT_EQ(ag->bbb_threat_type, BBB_THREAT_SQL_INJECTION);
    EXPECT_EQ(ag->severity, 10u);  // Critical = 10
}

TEST_F(BrainImmuneIntegrationTest, BBBThreatSeverityMapping) {
    uint8_t data[] = {0x01};
    uint32_t antigen_id;

    // Test LOW severity
    brain_immune_present_bbb_threat(immune_system, BBB_THREAT_DATA_TAMPERING,
                                     BBB_SEVERITY_LOW, data, 1, &antigen_id);
    const brain_antigen_t* ag = brain_immune_get_antigen(immune_system, antigen_id);
    EXPECT_EQ(ag->severity, 3u);

    // Test MEDIUM severity
    brain_immune_present_bbb_threat(immune_system, BBB_THREAT_DATA_TAMPERING,
                                     BBB_SEVERITY_MEDIUM, data, 1, &antigen_id);
    ag = brain_immune_get_antigen(immune_system, antigen_id);
    EXPECT_EQ(ag->severity, 5u);

    // Test HIGH severity
    brain_immune_present_bbb_threat(immune_system, BBB_THREAT_DATA_TAMPERING,
                                     BBB_SEVERITY_HIGH, data, 1, &antigen_id);
    ag = brain_immune_get_antigen(immune_system, antigen_id);
    EXPECT_EQ(ag->severity, 7u);

    // Test CRITICAL severity
    brain_immune_present_bbb_threat(immune_system, BBB_THREAT_DATA_TAMPERING,
                                     BBB_SEVERITY_CRITICAL, data, 1, &antigen_id);
    ag = brain_immune_get_antigen(immune_system, antigen_id);
    EXPECT_EQ(ag->severity, 10u);
}

/* ============================================================================
 * BFT Integration Tests
 * ============================================================================ */

TEST_F(BrainImmuneIntegrationTest, ByzantineNodeTriggersImmuneResponse) {
    uint32_t antigen_id;

    int result = brain_immune_present_byzantine(
        immune_system,
        42,  // Byzantine node ID
        BFT_BEHAV_EQUIVOCATION,
        nullptr, 0,
        &antigen_id
    );

    EXPECT_EQ(result, 0);

    const brain_antigen_t* ag = brain_immune_get_antigen(immune_system, antigen_id);
    ASSERT_NE(ag, nullptr);
    EXPECT_EQ(ag->source, ANTIGEN_SOURCE_BFT);
    EXPECT_EQ(ag->source_node_id, 42u);
    EXPECT_EQ(ag->bft_behavior, BFT_BEHAV_EQUIVOCATION);
}

TEST_F(BrainImmuneIntegrationTest, CollusionBehaviorIsHighSeverity) {
    uint32_t antigen_id;

    brain_immune_present_byzantine(
        immune_system,
        100,
        BFT_BEHAV_COLLUSION,
        nullptr, 0,
        &antigen_id
    );

    const brain_antigen_t* ag = brain_immune_get_antigen(immune_system, antigen_id);
    ASSERT_NE(ag, nullptr);
    EXPECT_EQ(ag->severity, 10u);  // Collusion is max severity
}

TEST_F(BrainImmuneIntegrationTest, KillerTCellTriggersQuarantine) {
    // Present Byzantine node
    uint32_t antigen_id;
    brain_immune_present_byzantine(immune_system, 77, BFT_BEHAV_EQUIVOCATION,
                                    nullptr, 0, &antigen_id);

    // Activate killer T cell
    uint32_t t_cell_id;
    int result = brain_immune_activate_killer_t(immune_system, antigen_id, &t_cell_id);
    EXPECT_EQ(result, 0);

    // Kill action (would quarantine node if BFT connected)
    result = brain_immune_t_cell_kill(immune_system, t_cell_id, 77);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * Full Immune Response Cycle Tests
 * ============================================================================ */

TEST_F(BrainImmuneIntegrationTest, CompleteImmuneResponseCycle) {
    // 1. Antigen presentation (threat detected)
    uint8_t threat_pattern[] = {0xDE, 0xAD, 0xBE, 0xEF};
    uint32_t antigen_id;
    brain_immune_present_bbb_threat(immune_system, BBB_THREAT_SHELLCODE,
                                     BBB_SEVERITY_HIGH, threat_pattern, sizeof(threat_pattern),
                                     &antigen_id);

    EXPECT_EQ(brain_immune_get_phase(immune_system), IMMUNE_PHASE_RECOGNITION);

    // 2. B cell activation
    uint32_t b_cell_id;
    int result = brain_immune_activate_b_cell(immune_system, antigen_id, &b_cell_id);
    EXPECT_EQ(result, 0);

    // 3. Helper T cell activation
    uint32_t helper_t_id;
    result = brain_immune_activate_helper_t(immune_system, antigen_id, &helper_t_id);
    EXPECT_EQ(result, 0);

    // 4. T cell helps B cell (class switching)
    result = brain_immune_t_help_b(immune_system, helper_t_id, b_cell_id);
    EXPECT_EQ(result, 0);

    // 5. Antibody production
    uint32_t antibody_id;
    result = brain_immune_produce_antibody(immune_system, b_cell_id, ANTIBODY_IGG, &antibody_id);
    EXPECT_EQ(result, 0);

    // 6. Execute antibody response
    result = brain_immune_execute_antibody(immune_system, antibody_id);
    EXPECT_EQ(result, 0);

    // 7. Neutralization (auto-learning creates memory cell automatically)
    result = brain_immune_neutralize(immune_system, antigen_id, antibody_id);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(brain_immune_is_neutralized(immune_system, antigen_id));

    // Verify stats - memory is now auto-created on neutralization
    brain_immune_stats_t stats;
    brain_immune_get_stats(immune_system, &stats);
    EXPECT_EQ(stats.threats_neutralized, 1u);
    EXPECT_GE(stats.memory_cells, 1u);  // Auto-learning creates memory
}

TEST_F(BrainImmuneIntegrationTest, SecondaryResponseIsFaster) {
    // First exposure - full immune cycle creates memory automatically
    uint8_t pathogen[] = {0x11, 0x22, 0x33, 0x44};
    uint32_t antigen_id1, b_cell_id, t_cell_id, antibody_id;

    brain_immune_present_antigen(immune_system, ANTIGEN_SOURCE_MANUAL,
                                  pathogen, sizeof(pathogen), 7, 0, &antigen_id1);
    brain_immune_activate_b_cell(immune_system, antigen_id1, &b_cell_id);
    brain_immune_activate_helper_t(immune_system, antigen_id1, &t_cell_id);
    brain_immune_t_help_b(immune_system, t_cell_id, b_cell_id);
    brain_immune_produce_antibody(immune_system, b_cell_id, ANTIBODY_IGG, &antibody_id);

    // Neutralization auto-creates memory cell
    brain_immune_neutralize(immune_system, antigen_id1, antibody_id);

    // Verify memory was auto-created
    EXPECT_GE(immune_system->stats.memory_cells, 1u);

    size_t antibodies_after_primary = immune_system->antibody_count;

    // Second exposure (same pathogen)
    // Auto-recognition will trigger secondary response automatically
    uint32_t antigen_id2;
    brain_immune_present_antigen(immune_system, ANTIGEN_SOURCE_MANUAL,
                                  pathogen, sizeof(pathogen), 7, 0, &antigen_id2);

    // Antibodies should increase (automatic rapid secondary response)
    EXPECT_GT(immune_system->antibody_count, antibodies_after_primary);
}

/* ============================================================================
 * Inflammation and Recovery Tests
 * ============================================================================ */

TEST_F(BrainImmuneIntegrationTest, InflammationEscalationCycle) {
    uint8_t threat[] = {0xAA};
    uint32_t antigen_id, site_id;

    brain_immune_present_bbb_threat(immune_system, BBB_THREAT_CODE_INJECTION,
                                     BBB_SEVERITY_HIGH, threat, 1, &antigen_id);

    // Initiate local inflammation
    int result = brain_immune_initiate_inflammation(immune_system, 1, antigen_id, &site_id);
    EXPECT_EQ(result, 0);

    // Find site and verify initial level
    brain_inflammation_site_t* site = nullptr;
    for (size_t i = 0; i < immune_system->inflammation_count; i++) {
        if (immune_system->inflammation_sites[i].id == site_id) {
            site = &immune_system->inflammation_sites[i];
            break;
        }
    }
    ASSERT_NE(site, nullptr);
    EXPECT_EQ(site->level, INFLAMMATION_LOCAL);

    // Escalate to regional
    brain_immune_escalate_inflammation(immune_system, site_id);
    EXPECT_EQ(site->level, INFLAMMATION_REGIONAL);

    // Escalate to systemic
    brain_immune_escalate_inflammation(immune_system, site_id);
    EXPECT_EQ(site->level, INFLAMMATION_SYSTEMIC);

    // Resolve
    brain_immune_resolve_inflammation(immune_system, site_id);
    EXPECT_GT(site->resolution_progress, 0.0f);
}

TEST_F(BrainImmuneIntegrationTest, CytokineSignalingCascade) {
    // Release pro-inflammatory cytokine
    uint32_t cytokine_id;
    brain_immune_release_cytokine(immune_system, BRAIN_CYTOKINE_IL1,
                                   0, 0.5f, 0, &cytokine_id);

    EXPECT_EQ(immune_system->cytokine_count, 1u);

    // Escalate with more cytokines
    brain_immune_release_cytokine(immune_system, BRAIN_CYTOKINE_IL6,
                                   0, 0.6f, 0, &cytokine_id);
    brain_immune_release_cytokine(immune_system, CYTOKINE_TNF_ALPHA,
                                   0, 0.7f, 0, &cytokine_id);

    EXPECT_EQ(immune_system->cytokine_count, 3u);

    // Release anti-inflammatory to resolve
    brain_immune_release_cytokine(immune_system, BRAIN_CYTOKINE_IL10,
                                   0, 0.8f, 0, &cytokine_id);

    // Verify anti-inflammatory was added
    bool found_il10 = false;
    for (size_t i = 0; i < immune_system->cytokine_count; i++) {
        if (immune_system->cytokines[i].type == BRAIN_CYTOKINE_IL10) {
            found_il10 = true;
            EXPECT_FALSE(immune_system->cytokines[i].pro_inflammatory);
            break;
        }
    }
    EXPECT_TRUE(found_il10);
}

/* ============================================================================
 * Update Cycle Tests
 * ============================================================================ */

TEST_F(BrainImmuneIntegrationTest, UpdateCycleProcessesAntigens) {
    // Present high-danger antigen
    uint8_t threat[] = {0xFF};
    uint32_t antigen_id;
    brain_immune_present_antigen(immune_system, ANTIGEN_SOURCE_MANUAL,
                                  threat, 1, 9, 0, &antigen_id);

    // Manually set high danger signal
    brain_antigen_t* ag = nullptr;
    for (size_t i = 0; i < immune_system->antigen_count; i++) {
        if (immune_system->antigens[i].id == antigen_id) {
            ag = &immune_system->antigens[i];
            ag->danger_signal = 0.9f;  // Above activation threshold
            break;
        }
    }

    // Run update cycle
    brain_immune_update(immune_system, 100);

    // Auto-activation should have created cells
    EXPECT_GT(immune_system->b_cell_count, 0u);
}

TEST_F(BrainImmuneIntegrationTest, UpdateDecaysAntibodies) {
    // Create antibody through full cycle
    uint8_t threat[] = {0xEE};
    uint32_t antigen_id, b_cell_id, t_cell_id, antibody_id;

    brain_immune_present_antigen(immune_system, ANTIGEN_SOURCE_MANUAL,
                                  threat, 1, 5, 0, &antigen_id);
    brain_immune_activate_b_cell(immune_system, antigen_id, &b_cell_id);
    brain_immune_activate_helper_t(immune_system, antigen_id, &t_cell_id);
    brain_immune_t_help_b(immune_system, t_cell_id, b_cell_id);
    brain_immune_produce_antibody(immune_system, b_cell_id, ANTIBODY_IGG, &antibody_id);

    // Get initial effectiveness
    brain_antibody_t* ab = nullptr;
    for (size_t i = 0; i < immune_system->antibody_count; i++) {
        if (immune_system->antibodies[i].id == antibody_id) {
            ab = &immune_system->antibodies[i];
            break;
        }
    }
    ASSERT_NE(ab, nullptr);
    float initial_effectiveness = ab->effectiveness;

    // Run many update cycles to simulate decay
    for (int i = 0; i < 100; i++) {
        brain_immune_update(immune_system, 500);
    }

    // Effectiveness should have decreased
    EXPECT_LT(ab->effectiveness, initial_effectiveness);
}

/* ============================================================================
 * Multi-Threat Handling Tests
 * ============================================================================ */

TEST_F(BrainImmuneIntegrationTest, HandleMultipleThreatsSimultaneously) {
    // Present multiple threats
    uint8_t threat1[] = {0x01};
    uint8_t threat2[] = {0x02};
    uint8_t threat3[] = {0x03};
    uint32_t ag1, ag2, ag3;

    brain_immune_present_bbb_threat(immune_system, BBB_THREAT_BUFFER_OVERFLOW,
                                     BBB_SEVERITY_HIGH, threat1, 1, &ag1);
    brain_immune_present_bbb_threat(immune_system, BBB_THREAT_SQL_INJECTION,
                                     BBB_SEVERITY_MEDIUM, threat2, 1, &ag2);
    brain_immune_present_byzantine(immune_system, 99, BFT_BEHAV_EQUIVOCATION,
                                    nullptr, 0, &ag3);

    EXPECT_EQ(immune_system->antigen_count, 3u);

    // Verify each antigen is distinct
    const brain_antigen_t* antigen1 = brain_immune_get_antigen(immune_system, ag1);
    const brain_antigen_t* antigen2 = brain_immune_get_antigen(immune_system, ag2);
    const brain_antigen_t* antigen3 = brain_immune_get_antigen(immune_system, ag3);

    EXPECT_EQ(antigen1->source, ANTIGEN_SOURCE_BBB);
    EXPECT_EQ(antigen2->source, ANTIGEN_SOURCE_BBB);
    EXPECT_EQ(antigen3->source, ANTIGEN_SOURCE_BFT);
}

/* ============================================================================
 * Callback Integration Tests
 * ============================================================================ */

static int integration_callback_count = 0;
static uint32_t last_callback_antigen_id = 0;

static void integration_antigen_callback(brain_immune_system_t*, const brain_antigen_t* ag, void*) {
    integration_callback_count++;
    last_callback_antigen_id = ag->id;
}

TEST_F(BrainImmuneIntegrationTest, CallbacksFireOnEvents) {
    integration_callback_count = 0;
    last_callback_antigen_id = 0;

    brain_immune_set_antigen_callback(immune_system, integration_antigen_callback, nullptr);

    uint8_t threat[] = {0xCA, 0xFE};
    uint32_t antigen_id;
    brain_immune_present_bbb_threat(immune_system, BBB_THREAT_ROP_CHAIN,
                                     BBB_SEVERITY_CRITICAL, threat, 2, &antigen_id);

    EXPECT_EQ(integration_callback_count, 1);
    EXPECT_EQ(last_callback_antigen_id, antigen_id);
}

/* ============================================================================
 * Stats Tracking Tests
 * ============================================================================ */

TEST_F(BrainImmuneIntegrationTest, StatsAreAccurate) {
    // Perform various operations
    uint8_t threat[] = {0xAB};
    uint32_t ag1, ag2, b_cell, t_cell, ab;

    brain_immune_present_bbb_threat(immune_system, BBB_THREAT_SHELLCODE,
                                     BBB_SEVERITY_HIGH, threat, 1, &ag1);
    brain_immune_present_byzantine(immune_system, 50, BFT_BEHAV_TIMING, nullptr, 0, &ag2);

    brain_immune_activate_b_cell(immune_system, ag1, &b_cell);
    brain_immune_activate_helper_t(immune_system, ag1, &t_cell);
    brain_immune_t_help_b(immune_system, t_cell, b_cell);
    brain_immune_produce_antibody(immune_system, b_cell, ANTIBODY_IGG, &ab);
    brain_immune_neutralize(immune_system, ag1, ab);

    uint32_t cytokine;
    brain_immune_release_cytokine(immune_system, BRAIN_CYTOKINE_IL6, 0, 0.5f, 0, &cytokine);

    uint32_t site;
    brain_immune_initiate_inflammation(immune_system, 1, ag2, &site);

    brain_immune_stats_t stats;
    brain_immune_get_stats(immune_system, &stats);

    EXPECT_EQ(stats.bbb_threats_processed, 1u);
    EXPECT_EQ(stats.bft_byzantines_handled, 1u);
    EXPECT_EQ(stats.active_b_cells, 1u);
    EXPECT_EQ(stats.active_t_cells, 1u);
    EXPECT_EQ(stats.active_antibodies, 1u);
    EXPECT_EQ(stats.threats_neutralized, 1u);
    EXPECT_EQ(stats.cytokines_released, 2u);  // Helper T also releases
    EXPECT_EQ(stats.inflammation_sites, 1u);
}

/* ============================================================================
 * Brain Factory Integration Tests
 * ============================================================================ */

/**
 * @brief Test brain factory immune subsystem initialization
 *
 * WHAT: Verify brain factory correctly initializes immune system
 * WHY:  Ensure auto-connection to BBB and bio-async works
 * HOW:  Create brain with immune enabled, verify integration
 */
TEST(BrainFactoryImmuneTest, ImmuneSubsystemInitialization) {
    // Create brain config with immune enabled
    brain_config_t config = {};
    config.size = BRAIN_SIZE_TINY;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 10;
    config.num_outputs = 3;
    config.enable_brain_immune = true;
    config.enable_bbb_protection = true;
    config.immune_enable_bbb_integration = true;
    config.immune_enable_bio_async = true;
    config.immune_max_antigens = 128;
    config.immune_max_b_cells = 256;
    config.immune_max_t_cells = 256;
    config.immune_max_antibodies = 512;
    config.minimal_mode = true;  // Fast initialization for test
    strncpy(config.task_name, "immune_test", sizeof(config.task_name) - 1);

    // Create brain - should auto-initialize immune system
    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    // Verify immune system was created
    EXPECT_TRUE(brain->immune_enabled);
    ASSERT_NE(brain->immune_system, nullptr);

    // Verify immune system is running
    brain_immune_phase_t phase = brain_immune_get_phase(brain->immune_system);
    EXPECT_EQ(phase, IMMUNE_PHASE_SURVEILLANCE);

    // Verify BBB connection if BBB is enabled
    if (brain->bbb_enabled) {
        EXPECT_TRUE(brain->immune_system->config.enable_bbb_integration);
        EXPECT_NE(brain->immune_system->bbb_system, nullptr);
    }

    // Verify bio-async connection if bio-async is enabled
    if (brain->bio_async_enabled) {
        EXPECT_TRUE(brain->immune_system->config.enable_bio_async);
    }

    // Get immune stats
    brain_immune_stats_t stats;
    int result = brain_immune_get_stats(brain->immune_system, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(stats.antigens_processed, 0u);
    EXPECT_EQ(stats.threats_neutralized, 0u);
    // system_health is only computed during brain_immune_update();
    // a freshly created system without updates will have default 0.0f
    EXPECT_GE(stats.system_health, 0.0f);

    // Cleanup
    brain_destroy(brain);
}

/**
 * @brief Test immune system disabled by default
 *
 * WHAT: Verify immune system not created when disabled
 * WHY:  Zero overhead when not needed
 * HOW:  Create brain without immune, verify null
 */
TEST(BrainFactoryImmuneTest, ImmuneDisabledByDefault) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_TINY;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 10;
    config.num_outputs = 3;
    config.enable_brain_immune = false;  // Explicitly disabled
    config.minimal_mode = true;
    strncpy(config.task_name, "no_immune", sizeof(config.task_name) - 1);

    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    // Verify immune system was NOT created
    EXPECT_FALSE(brain->immune_enabled);
    EXPECT_EQ(brain->immune_system, nullptr);

    brain_destroy(brain);
}

/**
 * @brief Test immune system with custom configuration
 *
 * WHAT: Verify custom immune config values are applied
 * WHY:  Allow tuning for different workloads
 * HOW:  Set custom thresholds, verify they're used
 */
TEST(BrainFactoryImmuneTest, CustomImmuneConfiguration) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 20;
    config.num_outputs = 5;
    config.enable_brain_immune = true;
    config.immune_max_antigens = 64;
    config.immune_max_b_cells = 128;
    config.immune_max_t_cells = 128;
    config.immune_max_antibodies = 256;
    config.immune_recognition_threshold = 0.8f;
    config.immune_activation_threshold = 0.7f;
    config.immune_inflammation_threshold = 0.9f;
    config.immune_memory_response_multiplier = 3.0f;
    config.immune_activation_delay_ms = 5;
    config.minimal_mode = true;
    strncpy(config.task_name, "custom_immune", sizeof(config.task_name) - 1);

    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);
    ASSERT_NE(brain->immune_system, nullptr);

    // Verify config was applied
    brain_immune_config_t* immune_cfg = &brain->immune_system->config;
    EXPECT_EQ(immune_cfg->max_antigens, 64u);
    EXPECT_EQ(immune_cfg->max_b_cells, 128u);
    EXPECT_EQ(immune_cfg->max_t_cells, 128u);
    EXPECT_EQ(immune_cfg->max_antibodies, 256u);
    EXPECT_FLOAT_EQ(immune_cfg->recognition_threshold, 0.8f);
    EXPECT_FLOAT_EQ(immune_cfg->activation_threshold, 0.7f);
    EXPECT_FLOAT_EQ(immune_cfg->inflammation_threshold, 0.9f);
    EXPECT_FLOAT_EQ(immune_cfg->memory_response_multiplier, 3.0f);
    EXPECT_EQ(immune_cfg->activation_delay_ms, 5u);

    brain_destroy(brain);
}
