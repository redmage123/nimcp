/**
 * @file test_brain_immune.cpp
 * @brief Unit tests for Brain Immune System
 * @version 1.0.0
 * @date 2025-12-11
 *
 * Tests for the brain immune coordination layer including:
 * - Lifecycle (create, destroy, start, stop)
 * - Antigen presentation
 * - B cell activation and memory formation
 * - T cell activation (helper and killer)
 * - Antibody production and neutralization
 * - Cytokine signaling
 * - Inflammation management
 * - Memory response
 * - Affinity computation
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "cognitive/immune/nimcp_brain_immune.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_codes.h"

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class BrainImmuneTest : public ::testing::Test {
protected:
    brain_immune_system_t* system = nullptr;
    brain_immune_config_t config;

    void SetUp() override {
        brain_immune_default_config(&config);
        system = brain_immune_create(&config);
        ASSERT_NE(system, nullptr);
    }

    void TearDown() override {
        if (system) {
            brain_immune_destroy(system);
            system = nullptr;
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(BrainImmuneTest, DefaultConfigIsValid) {
    brain_immune_config_t cfg;
    int result = brain_immune_default_config(&cfg);
    EXPECT_EQ(result, 0);
    EXPECT_GT(cfg.max_antigens, 0u);
    EXPECT_GT(cfg.max_b_cells, 0u);
    EXPECT_GT(cfg.max_t_cells, 0u);
    EXPECT_GT(cfg.max_antibodies, 0u);
    EXPECT_TRUE(cfg.enable_bbb_integration);
    EXPECT_TRUE(cfg.enable_bft_integration);
    EXPECT_TRUE(cfg.enable_swarm_integration);
}

TEST_F(BrainImmuneTest, DefaultConfigNullFails) {
    int result = brain_immune_default_config(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(BrainImmuneTest, CreateWithNullConfigUsesDefaults) {
    brain_immune_system_t* sys = brain_immune_create(nullptr);
    ASSERT_NE(sys, nullptr);
    EXPECT_EQ(sys->config.max_antigens, BRAIN_IMMUNE_MAX_ANTIGENS);
    brain_immune_destroy(sys);
}

TEST_F(BrainImmuneTest, CreateWithCustomConfig) {
    brain_immune_config_t custom_cfg;
    brain_immune_default_config(&custom_cfg);
    custom_cfg.max_antigens = 64;
    custom_cfg.max_b_cells = 128;

    brain_immune_system_t* sys = brain_immune_create(&custom_cfg);
    ASSERT_NE(sys, nullptr);
    EXPECT_EQ(sys->config.max_antigens, 64u);
    EXPECT_EQ(sys->config.max_b_cells, 128u);
    brain_immune_destroy(sys);
}

TEST_F(BrainImmuneTest, StartAndStop) {
    int result = brain_immune_start(system);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(system->running);

    result = brain_immune_stop(system);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(system->running);
}

TEST_F(BrainImmuneTest, StartNullFails) {
    int result = brain_immune_start(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(BrainImmuneTest, StopNullFails) {
    int result = brain_immune_stop(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(BrainImmuneTest, InitialPhaseIsSurveillance) {
    EXPECT_EQ(brain_immune_get_phase(system), IMMUNE_PHASE_SURVEILLANCE);
}

/* ============================================================================
 * Antigen Presentation Tests
 * ============================================================================ */

TEST_F(BrainImmuneTest, PresentAntigen) {
    brain_immune_start(system);

    uint8_t epitope[] = {0xDE, 0xAD, 0xBE, 0xEF};
    uint32_t antigen_id = 0;

    int result = brain_immune_present_antigen(
        system, ANTIGEN_SOURCE_MANUAL,
        epitope, sizeof(epitope),
        5, 100, &antigen_id
    );

    EXPECT_EQ(result, 0);
    EXPECT_GT(antigen_id, 0u);
    EXPECT_EQ(system->antigen_count, 1u);
}

TEST_F(BrainImmuneTest, PresentAntigenTriggersRecognitionPhase) {
    brain_immune_start(system);

    uint8_t epitope[] = {0x01, 0x02, 0x03};
    uint32_t antigen_id;

    brain_immune_present_antigen(
        system, ANTIGEN_SOURCE_MANUAL,
        epitope, sizeof(epitope),
        7, 0, &antigen_id
    );

    EXPECT_EQ(brain_immune_get_phase(system), IMMUNE_PHASE_RECOGNITION);
}

TEST_F(BrainImmuneTest, PresentAntigenNullFails) {
    uint8_t epitope[] = {0x01};
    uint32_t antigen_id;

    int result = brain_immune_present_antigen(
        nullptr, ANTIGEN_SOURCE_MANUAL,
        epitope, sizeof(epitope),
        5, 0, &antigen_id
    );
    EXPECT_EQ(result, -1);

    result = brain_immune_present_antigen(
        system, ANTIGEN_SOURCE_MANUAL,
        nullptr, 0,
        5, 0, &antigen_id
    );
    EXPECT_EQ(result, -1);
}

TEST_F(BrainImmuneTest, PresentBBBThreat) {
    brain_immune_start(system);

    uint8_t threat_data[] = {0xFF, 0xFE, 0xFD};
    uint32_t antigen_id = 0;

    int result = brain_immune_present_bbb_threat(
        system,
        BBB_THREAT_BUFFER_OVERFLOW,
        BBB_SEVERITY_HIGH,
        threat_data, sizeof(threat_data),
        &antigen_id
    );

    EXPECT_EQ(result, 0);
    EXPECT_GT(antigen_id, 0u);

    const brain_antigen_t* ag = brain_immune_get_antigen(system, antigen_id);
    ASSERT_NE(ag, nullptr);
    EXPECT_EQ(ag->source, ANTIGEN_SOURCE_BBB);
    EXPECT_EQ(ag->bbb_threat_type, BBB_THREAT_BUFFER_OVERFLOW);
}

TEST_F(BrainImmuneTest, PresentByzantine) {
    brain_immune_start(system);

    uint32_t antigen_id = 0;

    int result = brain_immune_present_byzantine(
        system,
        42,  // node_id
        BFT_BEHAV_EQUIVOCATION,
        nullptr, 0,
        &antigen_id
    );

    EXPECT_EQ(result, 0);
    EXPECT_GT(antigen_id, 0u);

    const brain_antigen_t* ag = brain_immune_get_antigen(system, antigen_id);
    ASSERT_NE(ag, nullptr);
    EXPECT_EQ(ag->source, ANTIGEN_SOURCE_BFT);
    EXPECT_EQ(ag->source_node_id, 42u);
}

TEST_F(BrainImmuneTest, GetNonExistentAntigenReturnsNull) {
    const brain_antigen_t* ag = brain_immune_get_antigen(system, 99999);
    EXPECT_EQ(ag, nullptr);
}

/* ============================================================================
 * B Cell Tests
 * ============================================================================ */

TEST_F(BrainImmuneTest, ActivateBCell) {
    brain_immune_start(system);

    uint8_t epitope[] = {0xAA, 0xBB, 0xCC};
    uint32_t antigen_id;
    brain_immune_present_antigen(system, ANTIGEN_SOURCE_MANUAL,
                                  epitope, sizeof(epitope), 5, 0, &antigen_id);

    uint32_t b_cell_id = 0;
    int result = brain_immune_activate_b_cell(system, antigen_id, &b_cell_id);

    EXPECT_EQ(result, 0);
    EXPECT_GT(b_cell_id, 0u);
    EXPECT_EQ(system->b_cell_count, 1u);
}

TEST_F(BrainImmuneTest, ActivateBCellForNonExistentAntigenFails) {
    brain_immune_start(system);

    uint32_t b_cell_id;
    int result = brain_immune_activate_b_cell(system, 99999, &b_cell_id);
    EXPECT_EQ(result, -1);
}

TEST_F(BrainImmuneTest, BCellToMemory) {
    brain_immune_start(system);

    uint8_t epitope[] = {0x11, 0x22, 0x33};
    uint32_t antigen_id;
    brain_immune_present_antigen(system, ANTIGEN_SOURCE_MANUAL,
                                  epitope, sizeof(epitope), 5, 0, &antigen_id);

    uint32_t b_cell_id;
    brain_immune_activate_b_cell(system, antigen_id, &b_cell_id);

    int result = brain_immune_b_cell_to_memory(system, b_cell_id);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(system->stats.memory_cells, 1u);
}

/* ============================================================================
 * T Cell Tests
 * ============================================================================ */

TEST_F(BrainImmuneTest, ActivateHelperTCell) {
    brain_immune_start(system);

    uint8_t epitope[] = {0x44, 0x55, 0x66};
    uint32_t antigen_id;
    brain_immune_present_antigen(system, ANTIGEN_SOURCE_MANUAL,
                                  epitope, sizeof(epitope), 5, 0, &antigen_id);

    uint32_t t_cell_id = 0;
    int result = brain_immune_activate_helper_t(system, antigen_id, &t_cell_id);

    EXPECT_EQ(result, 0);
    EXPECT_GT(t_cell_id, 0u);
    EXPECT_EQ(system->t_cell_count, 1u);
}

TEST_F(BrainImmuneTest, ActivateKillerTCell) {
    brain_immune_start(system);

    uint8_t epitope[] = {0x77, 0x88, 0x99};
    uint32_t antigen_id;
    brain_immune_present_antigen(system, ANTIGEN_SOURCE_MANUAL,
                                  epitope, sizeof(epitope), 8, 0, &antigen_id);

    uint32_t t_cell_id = 0;
    int result = brain_immune_activate_killer_t(system, antigen_id, &t_cell_id);

    EXPECT_EQ(result, 0);
    EXPECT_GT(t_cell_id, 0u);
}

TEST_F(BrainImmuneTest, HelperTHelpsBCell) {
    brain_immune_start(system);

    uint8_t epitope[] = {0xAA, 0xBB};
    uint32_t antigen_id;
    brain_immune_present_antigen(system, ANTIGEN_SOURCE_MANUAL,
                                  epitope, sizeof(epitope), 5, 0, &antigen_id);

    uint32_t b_cell_id, t_cell_id;
    brain_immune_activate_b_cell(system, antigen_id, &b_cell_id);
    brain_immune_activate_helper_t(system, antigen_id, &t_cell_id);

    int result = brain_immune_t_help_b(system, t_cell_id, b_cell_id);
    EXPECT_EQ(result, 0);

    // B cell should transition to plasma state
    brain_b_cell_t* b_cell = nullptr;
    for (size_t i = 0; i < system->b_cell_count; i++) {
        if (system->b_cells[i].id == b_cell_id) {
            b_cell = &system->b_cells[i];
            break;
        }
    }
    ASSERT_NE(b_cell, nullptr);
    EXPECT_TRUE(b_cell->received_t_help);
    EXPECT_EQ(b_cell->state, B_CELL_PLASMA);
}

/* ============================================================================
 * Antibody Tests
 * ============================================================================ */

TEST_F(BrainImmuneTest, ProduceAntibody) {
    brain_immune_start(system);

    // Setup: present antigen, activate B cell, get T help to become plasma
    uint8_t epitope[] = {0x12, 0x34};
    uint32_t antigen_id, b_cell_id, t_cell_id;
    brain_immune_present_antigen(system, ANTIGEN_SOURCE_MANUAL,
                                  epitope, sizeof(epitope), 5, 0, &antigen_id);
    brain_immune_activate_b_cell(system, antigen_id, &b_cell_id);
    brain_immune_activate_helper_t(system, antigen_id, &t_cell_id);
    brain_immune_t_help_b(system, t_cell_id, b_cell_id);

    uint32_t antibody_id = 0;
    int result = brain_immune_produce_antibody(system, b_cell_id, ANTIBODY_IGG, &antibody_id);

    EXPECT_EQ(result, 0);
    EXPECT_GT(antibody_id, 0u);
    EXPECT_EQ(system->antibody_count, 1u);
}

TEST_F(BrainImmuneTest, ProduceAntibodyFromNonPlasmaFails) {
    brain_immune_start(system);

    uint8_t epitope[] = {0xAB, 0xCD};
    uint32_t antigen_id, b_cell_id;
    brain_immune_present_antigen(system, ANTIGEN_SOURCE_MANUAL,
                                  epitope, sizeof(epitope), 5, 0, &antigen_id);
    brain_immune_activate_b_cell(system, antigen_id, &b_cell_id);
    // Note: B cell is activated, not plasma

    uint32_t antibody_id;
    int result = brain_immune_produce_antibody(system, b_cell_id, ANTIBODY_IGM, &antibody_id);
    EXPECT_EQ(result, -1);
}

TEST_F(BrainImmuneTest, Neutralize) {
    brain_immune_start(system);

    // Full immune response setup
    uint8_t epitope[] = {0xEE, 0xFF};
    uint32_t antigen_id, b_cell_id, t_cell_id, antibody_id;
    brain_immune_present_antigen(system, ANTIGEN_SOURCE_MANUAL,
                                  epitope, sizeof(epitope), 5, 0, &antigen_id);
    brain_immune_activate_b_cell(system, antigen_id, &b_cell_id);
    brain_immune_activate_helper_t(system, antigen_id, &t_cell_id);
    brain_immune_t_help_b(system, t_cell_id, b_cell_id);
    brain_immune_produce_antibody(system, b_cell_id, ANTIBODY_IGG, &antibody_id);

    EXPECT_FALSE(brain_immune_is_neutralized(system, antigen_id));

    int result = brain_immune_neutralize(system, antigen_id, antibody_id);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(brain_immune_is_neutralized(system, antigen_id));
    EXPECT_EQ(system->stats.threats_neutralized, 1u);
}

/* ============================================================================
 * Cytokine Tests
 * ============================================================================ */

TEST_F(BrainImmuneTest, ReleaseCytokine) {
    brain_immune_start(system);

    uint32_t cytokine_id = 0;
    int result = brain_immune_release_cytokine(
        system,
        BRAIN_CYTOKINE_IL6,
        0,  // source cell
        0.5f,  // concentration
        0,  // broadcast
        &cytokine_id
    );

    EXPECT_EQ(result, 0);
    EXPECT_GT(cytokine_id, 0u);
    EXPECT_EQ(system->cytokine_count, 1u);
    EXPECT_EQ(system->stats.cytokines_released, 1u);
}

TEST_F(BrainImmuneTest, BroadcastAlert) {
    brain_immune_start(system);

    uint8_t epitope[] = {0x01};
    uint32_t antigen_id;
    brain_immune_present_antigen(system, ANTIGEN_SOURCE_MANUAL,
                                  epitope, sizeof(epitope), 5, 0, &antigen_id);

    int result = brain_immune_broadcast_alert(system, antigen_id, INFLAMMATION_REGIONAL);
    EXPECT_EQ(result, 0);
    EXPECT_GT(system->cytokine_count, 0u);
}

/* ============================================================================
 * Inflammation Tests
 * ============================================================================ */

TEST_F(BrainImmuneTest, InitiateInflammation) {
    brain_immune_start(system);

    uint8_t epitope[] = {0xAA};
    uint32_t antigen_id;
    brain_immune_present_antigen(system, ANTIGEN_SOURCE_MANUAL,
                                  epitope, sizeof(epitope), 5, 0, &antigen_id);

    uint32_t site_id = 0;
    int result = brain_immune_initiate_inflammation(system, 1, antigen_id, &site_id);

    EXPECT_EQ(result, 0);
    EXPECT_GT(site_id, 0u);
    EXPECT_EQ(system->inflammation_count, 1u);
}

TEST_F(BrainImmuneTest, EscalateInflammation) {
    brain_immune_start(system);

    uint8_t epitope[] = {0xBB};
    uint32_t antigen_id, site_id;
    brain_immune_present_antigen(system, ANTIGEN_SOURCE_MANUAL,
                                  epitope, sizeof(epitope), 5, 0, &antigen_id);
    brain_immune_initiate_inflammation(system, 1, antigen_id, &site_id);

    int result = brain_immune_escalate_inflammation(system, site_id);
    EXPECT_EQ(result, 0);

    // Find site and check level increased
    brain_inflammation_site_t* site = nullptr;
    for (size_t i = 0; i < system->inflammation_count; i++) {
        if (system->inflammation_sites[i].id == site_id) {
            site = &system->inflammation_sites[i];
            break;
        }
    }
    ASSERT_NE(site, nullptr);
    EXPECT_EQ(site->level, INFLAMMATION_REGIONAL);
}

TEST_F(BrainImmuneTest, ResolveInflammation) {
    brain_immune_start(system);

    uint8_t epitope[] = {0xCC};
    uint32_t antigen_id, site_id;
    brain_immune_present_antigen(system, ANTIGEN_SOURCE_MANUAL,
                                  epitope, sizeof(epitope), 5, 0, &antigen_id);
    brain_immune_initiate_inflammation(system, 1, antigen_id, &site_id);

    int result = brain_immune_resolve_inflammation(system, site_id);
    EXPECT_EQ(result, 0);

    // Anti-inflammatory cytokine should be released
    bool found_il10 = false;
    for (size_t i = 0; i < system->cytokine_count; i++) {
        if (system->cytokines[i].type == BRAIN_CYTOKINE_IL10) {
            found_il10 = true;
            break;
        }
    }
    EXPECT_TRUE(found_il10);
}

/* ============================================================================
 * Memory Response Tests
 * ============================================================================ */

TEST_F(BrainImmuneTest, CheckMemoryNoMatch) {
    brain_immune_start(system);

    uint8_t epitope[] = {0x11};
    uint32_t antigen_id;
    brain_immune_present_antigen(system, ANTIGEN_SOURCE_MANUAL,
                                  epitope, sizeof(epitope), 5, 0, &antigen_id);

    uint32_t b_cell_id;
    int result = brain_immune_check_memory(system, antigen_id, &b_cell_id);
    EXPECT_EQ(result, -1);  // No memory yet
}

TEST_F(BrainImmuneTest, SecondaryResponse) {
    brain_immune_start(system);

    // First exposure - create memory through full immune cycle
    uint8_t epitope[] = {0x22, 0x33};
    uint32_t antigen_id, b_cell_id, t_cell_id, antibody_id;
    brain_immune_present_antigen(system, ANTIGEN_SOURCE_MANUAL,
                                  epitope, sizeof(epitope), 5, 0, &antigen_id);
    brain_immune_activate_b_cell(system, antigen_id, &b_cell_id);
    brain_immune_activate_helper_t(system, antigen_id, &t_cell_id);
    brain_immune_t_help_b(system, t_cell_id, b_cell_id);

    // Produce antibody to transition B cell to PLASMA state
    brain_immune_produce_antibody(system, b_cell_id, ANTIBODY_IGG, &antibody_id);

    // Neutralize - this will auto-convert B cell to memory
    brain_immune_neutralize(system, antigen_id, antibody_id);

    // Verify memory cell was created
    EXPECT_GE(system->stats.memory_cells, 1u);

    // Second exposure with same epitope
    // Note: With auto-recognition, secondary response is triggered automatically
    uint32_t antigen_id2;
    size_t antibody_count_before = system->antibody_count;
    brain_immune_present_antigen(system, ANTIGEN_SOURCE_MANUAL,
                                  epitope, sizeof(epitope), 5, 0, &antigen_id2);

    // Auto-recognition should have triggered secondary response and produced antibody
    EXPECT_GT(system->antibody_count, antibody_count_before);
}

/* ============================================================================
 * Affinity Computation Tests
 * ============================================================================ */

TEST_F(BrainImmuneTest, ComputeAffinityIdentical) {
    uint8_t pattern[] = {0x01, 0x02, 0x03, 0x04};
    float affinity = brain_immune_compute_affinity(
        pattern, sizeof(pattern),
        pattern, sizeof(pattern)
    );
    EXPECT_FLOAT_EQ(affinity, 1.0f);
}

TEST_F(BrainImmuneTest, ComputeAffinityDifferent) {
    // With fuzzy matching, even "different" patterns have some bit-level similarity
    // This enables cross-reactive immunity against threat variants
    uint8_t pattern1[] = {0x01, 0x02, 0x03, 0x04};
    uint8_t pattern2[] = {0xFF, 0xFE, 0xFD, 0xFC};
    float affinity = brain_immune_compute_affinity(
        pattern1, sizeof(pattern1),
        pattern2, sizeof(pattern2)
    );
    // Low affinity but not zero due to bit-level matching
    EXPECT_GT(affinity, 0.0f);
    EXPECT_LT(affinity, 0.5f);  // Should be low
}

TEST_F(BrainImmuneTest, ComputeAffinityPartial) {
    // Half the bytes match exactly, others differ
    // With fuzzy matching: exact=0.5*0.5 + bit similarity + length
    uint8_t pattern1[] = {0x01, 0x02, 0x03, 0x04};
    uint8_t pattern2[] = {0x01, 0x02, 0xFF, 0xFF};
    float affinity = brain_immune_compute_affinity(
        pattern1, sizeof(pattern1),
        pattern2, sizeof(pattern2)
    );
    // Should be moderate - higher than 0.5 due to bit similarity in non-matching bytes
    EXPECT_GT(affinity, 0.4f);
    EXPECT_LT(affinity, 0.8f);
}

TEST_F(BrainImmuneTest, ComputeAffinityNullReturnsZero) {
    uint8_t pattern[] = {0x01};
    float affinity = brain_immune_compute_affinity(nullptr, 0, pattern, 1);
    EXPECT_FLOAT_EQ(affinity, 0.0f);
}

/* ============================================================================
 * Update and Stats Tests
 * ============================================================================ */

TEST_F(BrainImmuneTest, Update) {
    brain_immune_start(system);

    int result = brain_immune_update(system, 100);
    EXPECT_EQ(result, 0);
}

TEST_F(BrainImmuneTest, UpdateWhenNotRunningFails) {
    // System not started - returns -1 (FEP/immune bridge convention)
    int result = brain_immune_update(system, 100);
    EXPECT_EQ(result, -1);
}

TEST_F(BrainImmuneTest, GetStats) {
    brain_immune_start(system);

    brain_immune_stats_t stats;
    int result = brain_immune_get_stats(system, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(stats.active_b_cells, 0u);
    EXPECT_EQ(stats.threats_neutralized, 0u);
}

TEST_F(BrainImmuneTest, GetStatsNullFails) {
    // NULL stats pointer triggers NIMCP_ERROR_NULL_POINTER
    int result = brain_immune_get_stats(system, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);

    // NULL system pointer triggers NIMCP_ERROR_NULL_POINTER
    brain_immune_stats_t stats;
    result = brain_immune_get_stats(nullptr, &stats);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * String Conversion Tests
 * ============================================================================ */

TEST_F(BrainImmuneTest, PhaseToString) {
    EXPECT_STREQ(brain_immune_phase_to_string(IMMUNE_PHASE_SURVEILLANCE), "SURVEILLANCE");
    EXPECT_STREQ(brain_immune_phase_to_string(IMMUNE_PHASE_EFFECTOR), "EFFECTOR");
}

TEST_F(BrainImmuneTest, BCellStateToString) {
    EXPECT_STREQ(brain_immune_b_cell_state_to_string(B_CELL_NAIVE), "NAIVE");
    EXPECT_STREQ(brain_immune_b_cell_state_to_string(B_CELL_PLASMA), "PLASMA");
}

TEST_F(BrainImmuneTest, TCellTypeToString) {
    EXPECT_STREQ(brain_immune_t_cell_type_to_string(T_CELL_HELPER), "HELPER");
    EXPECT_STREQ(brain_immune_t_cell_type_to_string(T_CELL_KILLER), "KILLER");
}

TEST_F(BrainImmuneTest, CytokineToString) {
    EXPECT_STREQ(brain_immune_cytokine_to_string(BRAIN_CYTOKINE_IL6), "IL-6");
    EXPECT_STREQ(brain_immune_cytokine_to_string(BRAIN_CYTOKINE_TNF), "TNF-alpha");
}

TEST_F(BrainImmuneTest, InflammationToString) {
    EXPECT_STREQ(brain_immune_inflammation_to_string(INFLAMMATION_LOCAL), "LOCAL");
    EXPECT_STREQ(brain_immune_inflammation_to_string(INFLAMMATION_STORM), "CYTOKINE_STORM");
}

/* ============================================================================
 * Callback Tests
 * ============================================================================ */

static int antigen_callback_count = 0;
static void test_antigen_callback(brain_immune_system_t*, const brain_antigen_t*, void*) {
    antigen_callback_count++;
}

TEST_F(BrainImmuneTest, AntigenCallback) {
    antigen_callback_count = 0;
    brain_immune_set_antigen_callback(system, test_antigen_callback, nullptr);
    brain_immune_start(system);

    uint8_t epitope[] = {0x01};
    uint32_t antigen_id;
    brain_immune_present_antigen(system, ANTIGEN_SOURCE_MANUAL,
                                  epitope, sizeof(epitope), 5, 0, &antigen_id);

    EXPECT_EQ(antigen_callback_count, 1);
}

static int neutralize_callback_count = 0;
static void test_neutralize_callback(brain_immune_system_t*, uint32_t, const brain_antibody_t*, void*) {
    neutralize_callback_count++;
}

TEST_F(BrainImmuneTest, NeutralizeCallback) {
    neutralize_callback_count = 0;
    brain_immune_set_neutralize_callback(system, test_neutralize_callback, nullptr);
    brain_immune_start(system);

    // Full response to trigger neutralization
    uint8_t epitope[] = {0x99};
    uint32_t antigen_id, b_cell_id, t_cell_id, antibody_id;
    brain_immune_present_antigen(system, ANTIGEN_SOURCE_MANUAL,
                                  epitope, sizeof(epitope), 5, 0, &antigen_id);
    brain_immune_activate_b_cell(system, antigen_id, &b_cell_id);
    brain_immune_activate_helper_t(system, antigen_id, &t_cell_id);
    brain_immune_t_help_b(system, t_cell_id, b_cell_id);
    brain_immune_produce_antibody(system, b_cell_id, ANTIBODY_IGG, &antibody_id);
    brain_immune_neutralize(system, antigen_id, antibody_id);

    EXPECT_EQ(neutralize_callback_count, 1);
}

/* ============================================================================
 * Edge Cases
 * ============================================================================ */

TEST_F(BrainImmuneTest, CapacityLimit) {
    brain_immune_config_t small_cfg;
    brain_immune_default_config(&small_cfg);
    small_cfg.max_antigens = 2;

    brain_immune_system_t* small_sys = brain_immune_create(&small_cfg);
    ASSERT_NE(small_sys, nullptr);
    brain_immune_start(small_sys);

    uint8_t epitope[] = {0x01};
    uint32_t antigen_id;

    // First two should succeed
    EXPECT_EQ(brain_immune_present_antigen(small_sys, ANTIGEN_SOURCE_MANUAL,
                                            epitope, 1, 5, 0, &antigen_id), 0);
    EXPECT_EQ(brain_immune_present_antigen(small_sys, ANTIGEN_SOURCE_MANUAL,
                                            epitope, 1, 5, 0, &antigen_id), 0);

    // Third should fail
    EXPECT_EQ(brain_immune_present_antigen(small_sys, ANTIGEN_SOURCE_MANUAL,
                                            epitope, 1, 5, 0, &antigen_id), -1);

    brain_immune_destroy(small_sys);
}

TEST_F(BrainImmuneTest, EpitopeTruncation) {
    brain_immune_start(system);

    // Create epitope larger than max size
    uint8_t large_epitope[128];
    memset(large_epitope, 0xAA, sizeof(large_epitope));

    uint32_t antigen_id;
    int result = brain_immune_present_antigen(
        system, ANTIGEN_SOURCE_MANUAL,
        large_epitope, sizeof(large_epitope),
        5, 0, &antigen_id
    );

    EXPECT_EQ(result, 0);

    const brain_antigen_t* ag = brain_immune_get_antigen(system, antigen_id);
    ASSERT_NE(ag, nullptr);
    EXPECT_EQ(ag->epitope_len, BRAIN_IMMUNE_EPITOPE_SIZE);
}

/* ============================================================================
 * Continuous Inflammation Model Tests
 * ============================================================================ */

TEST_F(BrainImmuneTest, ContinuousLevelFromLabel) {
    /* Verify label-to-continuous mapping produces expected midpoints */
    EXPECT_FLOAT_EQ(inflammation_level_to_continuous(INFLAMMATION_NONE), 0.0f);
    EXPECT_FLOAT_EQ(inflammation_level_to_continuous(INFLAMMATION_LOCAL), 0.175f);
    EXPECT_FLOAT_EQ(inflammation_level_to_continuous(INFLAMMATION_REGIONAL), 0.45f);
    EXPECT_FLOAT_EQ(inflammation_level_to_continuous(INFLAMMATION_SYSTEMIC), 0.725f);
    EXPECT_FLOAT_EQ(inflammation_level_to_continuous(INFLAMMATION_STORM), 0.925f);
}

TEST_F(BrainImmuneTest, LabelFromContinuousRoundtrips) {
    /* Label derived from midpoint should return the same label */
    EXPECT_EQ(inflammation_level_from_continuous(0.0f), INFLAMMATION_NONE);
    EXPECT_EQ(inflammation_level_from_continuous(0.175f), INFLAMMATION_LOCAL);
    EXPECT_EQ(inflammation_level_from_continuous(0.45f), INFLAMMATION_REGIONAL);
    EXPECT_EQ(inflammation_level_from_continuous(0.725f), INFLAMMATION_SYSTEMIC);
    EXPECT_EQ(inflammation_level_from_continuous(0.925f), INFLAMMATION_STORM);
}

TEST_F(BrainImmuneTest, LabelFromContinuousBoundaries) {
    /* Test boundary values */
    EXPECT_EQ(inflammation_level_from_continuous(0.04f), INFLAMMATION_NONE);
    EXPECT_EQ(inflammation_level_from_continuous(0.05f), INFLAMMATION_LOCAL);
    EXPECT_EQ(inflammation_level_from_continuous(0.29f), INFLAMMATION_LOCAL);
    EXPECT_EQ(inflammation_level_from_continuous(0.30f), INFLAMMATION_REGIONAL);
    EXPECT_EQ(inflammation_level_from_continuous(0.59f), INFLAMMATION_REGIONAL);
    EXPECT_EQ(inflammation_level_from_continuous(0.60f), INFLAMMATION_SYSTEMIC);
    EXPECT_EQ(inflammation_level_from_continuous(0.84f), INFLAMMATION_SYSTEMIC);
    EXPECT_EQ(inflammation_level_from_continuous(0.85f), INFLAMMATION_STORM);
    EXPECT_EQ(inflammation_level_from_continuous(1.0f), INFLAMMATION_STORM);
}

TEST_F(BrainImmuneTest, ComputeFactorEndpoints) {
    /* At level=0.0, should return the 'at_none' value exactly */
    EXPECT_FLOAT_EQ(inflammation_compute_factor(0.0f, 1.0f, 0.2f), 1.0f);
    /* At level=1.0, should return the 'at_storm' value exactly */
    EXPECT_FLOAT_EQ(inflammation_compute_factor(1.0f, 1.0f, 0.2f), 0.2f);
}

TEST_F(BrainImmuneTest, ComputeFactorMonotonicity) {
    /* Factor should decrease monotonically as level increases (when at_storm < at_none) */
    float prev = inflammation_compute_factor(0.0f, 1.0f, 0.1f);
    for (float level = 0.1f; level <= 1.01f; level += 0.1f) {
        float current = inflammation_compute_factor(level, 1.0f, 0.1f);
        EXPECT_LE(current, prev) << "Non-monotonic at level=" << level;
        prev = current;
    }
}

TEST_F(BrainImmuneTest, ComputeFactorIncreasingValues) {
    /* When at_storm > at_none, factor should increase (e.g. quorum factor) */
    float prev = inflammation_compute_factor(0.0f, 1.0f, 1.6f);
    for (float level = 0.1f; level <= 1.01f; level += 0.1f) {
        float current = inflammation_compute_factor(level, 1.0f, 1.6f);
        EXPECT_GE(current, prev) << "Non-increasing at level=" << level;
        prev = current;
    }
}

TEST_F(BrainImmuneTest, ComputeEffectsNone) {
    inflammation_effects_t effects;
    int result = inflammation_compute_effects(0.0f, &effects);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(effects.label, INFLAMMATION_NONE);
    EXPECT_FLOAT_EQ(effects.level, 0.0f);
    EXPECT_FLOAT_EQ(effects.capacity_factor, 1.0f);
    EXPECT_FLOAT_EQ(effects.damage_accumulation, 0.0f);
    EXPECT_FLOAT_EQ(effects.resource_demand, 0.0f);
}

TEST_F(BrainImmuneTest, ComputeEffectsStorm) {
    inflammation_effects_t effects;
    int result = inflammation_compute_effects(1.0f, &effects);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(effects.label, INFLAMMATION_STORM);
    EXPECT_FLOAT_EQ(effects.level, 1.0f);
    EXPECT_NEAR(effects.capacity_factor, 0.2f, 0.01f);
    EXPECT_FLOAT_EQ(effects.damage_accumulation, 1.0f);
    EXPECT_FLOAT_EQ(effects.resource_demand, 1.0f);
}

TEST_F(BrainImmuneTest, ComputeEffectsNull) {
    int result = inflammation_compute_effects(0.5f, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(BrainImmuneTest, ComputeEffectsClamps) {
    /* Negative level should be clamped to 0.0 */
    inflammation_effects_t effects;
    inflammation_compute_effects(-0.5f, &effects);
    EXPECT_FLOAT_EQ(effects.level, 0.0f);
    EXPECT_EQ(effects.label, INFLAMMATION_NONE);

    /* Over 1.0 should be clamped to 1.0 */
    inflammation_compute_effects(2.0f, &effects);
    EXPECT_FLOAT_EQ(effects.level, 1.0f);
    EXPECT_EQ(effects.label, INFLAMMATION_STORM);
}

TEST_F(BrainImmuneTest, ComputeEffectsAntiInflammatoryPeaks) {
    /* Anti-inflammatory drive should peak at 0.5 (bell curve) */
    inflammation_effects_t effects_low, effects_mid, effects_high;
    inflammation_compute_effects(0.2f, &effects_low);
    inflammation_compute_effects(0.5f, &effects_mid);
    inflammation_compute_effects(0.8f, &effects_high);

    EXPECT_GT(effects_mid.anti_inflammatory_drive, effects_low.anti_inflammatory_drive);
    EXPECT_GT(effects_mid.anti_inflammatory_drive, effects_high.anti_inflammatory_drive);
    EXPECT_FLOAT_EQ(effects_mid.anti_inflammatory_drive, 1.0f);  /* 4 * 0.5 * 0.5 = 1.0 */
}

TEST_F(BrainImmuneTest, ContinuousInflammationSiteInit) {
    /* Inflammation site should have continuous level set on initiation */
    brain_immune_start(system);

    uint8_t epitope[] = {0xDD};
    uint32_t antigen_id, site_id;
    brain_immune_present_antigen(system, ANTIGEN_SOURCE_MANUAL,
                                  epitope, sizeof(epitope), 5, 0, &antigen_id);
    brain_immune_initiate_inflammation(system, 1, antigen_id, &site_id);

    brain_inflammation_site_t* site = nullptr;
    for (size_t i = 0; i < system->inflammation_count; i++) {
        if (system->inflammation_sites[i].id == site_id) {
            site = &system->inflammation_sites[i];
            break;
        }
    }
    ASSERT_NE(site, nullptr);
    EXPECT_FLOAT_EQ(site->inflammation_level, 0.175f);  /* LOCAL midpoint */
    EXPECT_EQ(site->level, INFLAMMATION_LOCAL);
}

TEST_F(BrainImmuneTest, ContinuousEscalationIncreasesContinuousLevel) {
    brain_immune_start(system);

    uint8_t epitope[] = {0xEE};
    uint32_t antigen_id, site_id;
    brain_immune_present_antigen(system, ANTIGEN_SOURCE_MANUAL,
                                  epitope, sizeof(epitope), 5, 0, &antigen_id);
    brain_immune_initiate_inflammation(system, 1, antigen_id, &site_id);

    /* Get initial level */
    brain_inflammation_site_t* site = nullptr;
    for (size_t i = 0; i < system->inflammation_count; i++) {
        if (system->inflammation_sites[i].id == site_id) {
            site = &system->inflammation_sites[i];
            break;
        }
    }
    ASSERT_NE(site, nullptr);
    float initial_level = site->inflammation_level;

    /* Escalate */
    brain_immune_escalate_inflammation(system, site_id);
    EXPECT_GT(site->inflammation_level, initial_level);
    EXPECT_NEAR(site->inflammation_level, initial_level + 0.25f, 0.001f);

    /* Escalate again */
    float mid_level = site->inflammation_level;
    brain_immune_escalate_inflammation(system, site_id);
    EXPECT_GT(site->inflammation_level, mid_level);
}

TEST_F(BrainImmuneTest, ContinuousEscalationCapsAtOne) {
    brain_immune_start(system);

    uint8_t epitope[] = {0xFF};
    uint32_t antigen_id, site_id;
    brain_immune_present_antigen(system, ANTIGEN_SOURCE_MANUAL,
                                  epitope, sizeof(epitope), 5, 0, &antigen_id);
    brain_immune_initiate_inflammation(system, 1, antigen_id, &site_id);

    /* Escalate many times */
    for (int i = 0; i < 10; i++) {
        brain_immune_escalate_inflammation(system, site_id);
    }

    brain_inflammation_site_t* site = nullptr;
    for (size_t i = 0; i < system->inflammation_count; i++) {
        if (system->inflammation_sites[i].id == site_id) {
            site = &system->inflammation_sites[i];
            break;
        }
    }
    ASSERT_NE(site, nullptr);
    EXPECT_LE(site->inflammation_level, 1.0f);
    EXPECT_EQ(site->level, INFLAMMATION_STORM);
}

TEST_F(BrainImmuneTest, GetInflammationLevelContinuousAPI) {
    brain_immune_start(system);

    /* No inflammation -> 0.0 */
    float level = brain_immune_get_inflammation_level_continuous(system);
    EXPECT_FLOAT_EQ(level, 0.0f);

    /* After initiating inflammation -> positive */
    uint8_t epitope[] = {0xAB};
    uint32_t antigen_id, site_id;
    brain_immune_present_antigen(system, ANTIGEN_SOURCE_MANUAL,
                                  epitope, sizeof(epitope), 5, 0, &antigen_id);
    brain_immune_initiate_inflammation(system, 1, antigen_id, &site_id);

    level = brain_immune_get_inflammation_level_continuous(system);
    EXPECT_GT(level, 0.0f);
    EXPECT_LE(level, 1.0f);
}

TEST_F(BrainImmuneTest, GetInflammationEffectsAPI) {
    brain_immune_start(system);

    /* No inflammation */
    inflammation_effects_t effects;
    int result = brain_immune_get_inflammation_effects(system, &effects);
    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(effects.level, 0.0f);
    EXPECT_FLOAT_EQ(effects.capacity_factor, 1.0f);

    /* After inflammation */
    uint8_t epitope[] = {0xAC};
    uint32_t antigen_id, site_id;
    brain_immune_present_antigen(system, ANTIGEN_SOURCE_MANUAL,
                                  epitope, sizeof(epitope), 5, 0, &antigen_id);
    brain_immune_initiate_inflammation(system, 1, antigen_id, &site_id);

    result = brain_immune_get_inflammation_effects(system, &effects);
    EXPECT_EQ(result, 0);
    EXPECT_GT(effects.level, 0.0f);
    EXPECT_LT(effects.capacity_factor, 1.0f);
}

TEST_F(BrainImmuneTest, StatsIncludeContinuousLevel) {
    brain_immune_start(system);

    /* After initiating inflammation, stats should reflect continuous level */
    uint8_t epitope[] = {0xAD};
    uint32_t antigen_id, site_id;
    brain_immune_present_antigen(system, ANTIGEN_SOURCE_MANUAL,
                                  epitope, sizeof(epitope), 5, 0, &antigen_id);
    brain_immune_initiate_inflammation(system, 1, antigen_id, &site_id);

    brain_immune_stats_t stats;
    brain_immune_get_stats(system, &stats);

    EXPECT_GT(stats.inflammation_level_continuous, 0.0f);
    EXPECT_LE(stats.inflammation_level_continuous, 1.0f);
    EXPECT_EQ(stats.inflammation_level,
              inflammation_level_from_continuous(stats.inflammation_level_continuous));
}
