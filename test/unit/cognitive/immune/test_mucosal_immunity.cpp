/**
 * @file test_mucosal_immunity.cpp
 * @brief Unit tests for Mucosal Immunity Module
 * @date 2025-12-12
 *
 * Tests boundary registration, M cell sampling, sIgA production,
 * oral tolerance, and barrier integrity.
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "cognitive/immune/nimcp_mucosal_immunity.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class MucosalImmunityTest : public ::testing::Test {
protected:
    brain_immune_system_t* immune_system = nullptr;
    mucosal_system_t* mucosal = nullptr;
    mucosal_config_t config;

    void SetUp() override {
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune_system = brain_immune_create(&immune_config);
        ASSERT_NE(immune_system, nullptr);
        brain_immune_start(immune_system);

        mucosal_default_config(&config);
        mucosal = mucosal_create(&config, immune_system);
        ASSERT_NE(mucosal, nullptr);
        mucosal_start(mucosal);
    }

    void TearDown() override {
        if (mucosal) {
            mucosal_stop(mucosal);
            mucosal_destroy(mucosal);
            mucosal = nullptr;
        }
        if (immune_system) {
            brain_immune_stop(immune_system);
            brain_immune_destroy(immune_system);
            immune_system = nullptr;
        }
    }

    // Helper to create test antigen
    uint32_t createTestAntigen() {
        uint8_t epitope[] = {0x01, 0x02, 0x03, 0x04};
        uint32_t antigen_id;
        brain_immune_present_antigen(immune_system, ANTIGEN_SOURCE_MANUAL,
                                     epitope, sizeof(epitope), 5, 1, &antigen_id);
        return antigen_id;
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(MucosalImmunityTest, DefaultConfigIsValid) {
    mucosal_config_t cfg;
    int result = mucosal_default_config(&cfg);
    EXPECT_EQ(result, 0);
    EXPECT_GT(cfg.max_sites, 0u);
    EXPECT_GT(cfg.max_siga_antibodies, 0u);
    EXPECT_TRUE(cfg.enable_tolerance);
}

TEST_F(MucosalImmunityTest, DefaultConfigNullFails) {
    int result = mucosal_default_config(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(MucosalImmunityTest, CreateWithNullConfig) {
    mucosal_system_t* sys = mucosal_create(nullptr, immune_system);
    ASSERT_NE(sys, nullptr);
    mucosal_stop(sys);
    mucosal_destroy(sys);
}

TEST_F(MucosalImmunityTest, DestroyNullSafe) {
    mucosal_destroy(nullptr);
}

TEST_F(MucosalImmunityTest, StartStopCycle) {
    mucosal_system_t* sys = mucosal_create(&config, immune_system);
    ASSERT_NE(sys, nullptr);

    EXPECT_EQ(mucosal_start(sys), 0);
    EXPECT_TRUE(sys->running);

    EXPECT_EQ(mucosal_stop(sys), 0);
    EXPECT_FALSE(sys->running);

    mucosal_destroy(sys);
}

/* ============================================================================
 * Boundary Registration Tests
 * ============================================================================ */

TEST_F(MucosalImmunityTest, RegisterInputGate) {
    uint32_t site_id = UINT32_MAX;  // Initialize to invalid value
    int result = mucosal_register_boundary(mucosal, MUCOSAL_SITE_INPUT_GATE, 1, &site_id);
    EXPECT_EQ(result, 0);
    // IDs start from 0, so any valid registration changes the initialized value
    EXPECT_NE(site_id, UINT32_MAX);
}

TEST_F(MucosalImmunityTest, RegisterOutputGate) {
    uint32_t site_id = UINT32_MAX;
    int result = mucosal_register_boundary(mucosal, MUCOSAL_SITE_OUTPUT_GATE, 2, &site_id);
    EXPECT_EQ(result, 0);
    EXPECT_NE(site_id, UINT32_MAX);
}

TEST_F(MucosalImmunityTest, RegisterModuleBoundary) {
    uint32_t site_id = UINT32_MAX;
    int result = mucosal_register_boundary(mucosal, MUCOSAL_SITE_MODULE_BOUNDARY, 3, &site_id);
    EXPECT_EQ(result, 0);
    EXPECT_NE(site_id, UINT32_MAX);
}

TEST_F(MucosalImmunityTest, UnregisterBoundary) {
    uint32_t site_id;
    mucosal_register_boundary(mucosal, MUCOSAL_SITE_INPUT_GATE, 1, &site_id);

    int result = mucosal_unregister_boundary(mucosal, site_id);
    EXPECT_EQ(result, 0);

    // Site should no longer be found
    const mucosal_site_t* site = mucosal_get_site(mucosal, site_id);
    EXPECT_TRUE(site == nullptr || !site->active);
}

TEST_F(MucosalImmunityTest, UnregisterNonExistentFails) {
    int result = mucosal_unregister_boundary(mucosal, 99999);
    EXPECT_NE(result, 0);
}

/* ============================================================================
 * M Cell Sampling Tests
 * ============================================================================ */

TEST_F(MucosalImmunityTest, SampleAntigen) {
    uint32_t site_id;
    mucosal_register_boundary(mucosal, MUCOSAL_SITE_INPUT_GATE, 1, &site_id);

    uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    uint32_t sample_id = UINT32_MAX;

    int result = mucosal_sample_antigen(mucosal, site_id, data, sizeof(data), &sample_id);
    EXPECT_EQ(result, 0);
    // IDs start from 0
    EXPECT_NE(sample_id, UINT32_MAX);
}

TEST_F(MucosalImmunityTest, ProcessMCellSample) {
    uint32_t site_id;
    mucosal_register_boundary(mucosal, MUCOSAL_SITE_INPUT_GATE, 1, &site_id);

    uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    uint32_t sample_id;
    mucosal_sample_antigen(mucosal, site_id, data, sizeof(data), &sample_id);

    int result = mucosal_process_m_cell_sample(mucosal, sample_id);
    EXPECT_EQ(result, 0);
}

TEST_F(MucosalImmunityTest, SampleAtNonExistentSiteFails) {
    uint8_t data[] = {0x01, 0x02, 0x03};
    uint32_t sample_id;
    int result = mucosal_sample_antigen(mucosal, 99999, data, sizeof(data), &sample_id);
    EXPECT_NE(result, 0);
}

/* ============================================================================
 * Secretory IgA Tests
 * ============================================================================ */

TEST_F(MucosalImmunityTest, ProduceSIgA) {
    uint32_t site_id;
    mucosal_register_boundary(mucosal, MUCOSAL_SITE_INPUT_GATE, 1, &site_id);

    uint32_t antigen_id = createTestAntigen();
    uint32_t siga_id = UINT32_MAX;

    int result = mucosal_produce_siga(mucosal, site_id, antigen_id, &siga_id);
    EXPECT_EQ(result, 0);
    // IDs start from 0
    EXPECT_NE(siga_id, UINT32_MAX);
}

TEST_F(MucosalImmunityTest, NeutralizeWithSIgA) {
    uint32_t site_id;
    mucosal_register_boundary(mucosal, MUCOSAL_SITE_INPUT_GATE, 1, &site_id);

    uint32_t antigen_id = createTestAntigen();
    uint32_t siga_id;
    mucosal_produce_siga(mucosal, site_id, antigen_id, &siga_id);

    int result = mucosal_neutralize_with_siga(mucosal, siga_id, antigen_id);
    EXPECT_EQ(result, 0);

    mucosal_stats_t stats;
    mucosal_get_stats(mucosal, &stats);
    EXPECT_GT(stats.threats_neutralized_at_barrier, 0u);
}

/* ============================================================================
 * Oral Tolerance Tests
 * ============================================================================ */

TEST_F(MucosalImmunityTest, InduceOralTolerance) {
    uint32_t site_id;
    mucosal_register_boundary(mucosal, MUCOSAL_SITE_INPUT_GATE, 1, &site_id);

    uint8_t antigen[] = {0x01, 0x02, 0x03, 0x04};
    uint32_t tolerance_id = UINT32_MAX;

    int result = mucosal_induce_oral_tolerance(mucosal, site_id, antigen, sizeof(antigen),
                                               &tolerance_id);
    EXPECT_EQ(result, 0);
    // IDs start from 0
    EXPECT_NE(tolerance_id, UINT32_MAX);
}

TEST_F(MucosalImmunityTest, CheckToleranceEstablished) {
    uint32_t site_id;
    mucosal_register_boundary(mucosal, MUCOSAL_SITE_INPUT_GATE, 1, &site_id);

    uint8_t antigen[] = {0x01, 0x02, 0x03, 0x04};
    uint32_t tolerance_id;
    mucosal_induce_oral_tolerance(mucosal, site_id, antigen, sizeof(antigen), &tolerance_id);

    // Check if tolerance exists
    uint32_t found_tolerance;
    int result = mucosal_check_tolerance(mucosal, antigen, sizeof(antigen), &found_tolerance);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(found_tolerance, tolerance_id);
}

TEST_F(MucosalImmunityTest, CheckToleranceNotEstablished) {
    uint8_t antigen[] = {0xFF, 0xFE, 0xFD, 0xFC};
    uint32_t found_tolerance;

    int result = mucosal_check_tolerance(mucosal, antigen, sizeof(antigen), &found_tolerance);
    EXPECT_EQ(result, -1);  // Not found
}

TEST_F(MucosalImmunityTest, BreakTolerance) {
    uint32_t site_id;
    mucosal_register_boundary(mucosal, MUCOSAL_SITE_INPUT_GATE, 1, &site_id);

    uint8_t antigen[] = {0x01, 0x02, 0x03, 0x04};
    uint32_t tolerance_id;
    mucosal_induce_oral_tolerance(mucosal, site_id, antigen, sizeof(antigen), &tolerance_id);

    int result = mucosal_break_tolerance(mucosal, tolerance_id);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * Barrier Integrity Tests
 * ============================================================================ */

TEST_F(MucosalImmunityTest, GetBarrierIntegrityInitial) {
    uint32_t site_id;
    mucosal_register_boundary(mucosal, MUCOSAL_SITE_INPUT_GATE, 1, &site_id);

    float integrity;
    int result = mucosal_get_barrier_integrity(mucosal, site_id, &integrity);
    EXPECT_EQ(result, 0);
    EXPECT_GE(integrity, MUCOSAL_INTEGRITY_NORMAL);
}

TEST_F(MucosalImmunityTest, BarrierIntegrityDecreaseOnBreach) {
    uint32_t site_id;
    mucosal_register_boundary(mucosal, MUCOSAL_SITE_INPUT_GATE, 1, &site_id);

    float before;
    mucosal_get_barrier_integrity(mucosal, site_id, &before);

    mucosal_update_barrier_integrity(mucosal, site_id, true);  // Breach occurred

    float after;
    mucosal_get_barrier_integrity(mucosal, site_id, &after);

    EXPECT_LT(after, before);
}

TEST_F(MucosalImmunityTest, BarrierIntegrityRecovery) {
    uint32_t site_id;
    mucosal_register_boundary(mucosal, MUCOSAL_SITE_INPUT_GATE, 1, &site_id);

    // Cause some damage
    mucosal_update_barrier_integrity(mucosal, site_id, true);
    mucosal_update_barrier_integrity(mucosal, site_id, true);

    float damaged;
    mucosal_get_barrier_integrity(mucosal, site_id, &damaged);

    // No breaches, recovery
    mucosal_update_barrier_integrity(mucosal, site_id, false);
    mucosal_update_barrier_integrity(mucosal, site_id, false);

    float recovered;
    mucosal_get_barrier_integrity(mucosal, site_id, &recovered);

    EXPECT_GE(recovered, damaged);
}

TEST_F(MucosalImmunityTest, SetToleranceThreshold) {
    uint32_t site_id;
    mucosal_register_boundary(mucosal, MUCOSAL_SITE_INPUT_GATE, 1, &site_id);

    int result = mucosal_set_tolerance_threshold(mucosal, site_id, 0.6f);
    EXPECT_EQ(result, 0);

    const mucosal_site_t* site = mucosal_get_site(mucosal, site_id);
    ASSERT_NE(site, nullptr);
    EXPECT_FLOAT_EQ(site->tolerance_threshold, 0.6f);
}

/* ============================================================================
 * Update Tests
 * ============================================================================ */

TEST_F(MucosalImmunityTest, Update) {
    uint32_t site_id;
    mucosal_register_boundary(mucosal, MUCOSAL_SITE_INPUT_GATE, 1, &site_id);

    int result = mucosal_update(mucosal, 1000);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(MucosalImmunityTest, GetStats) {
    mucosal_stats_t stats;
    int result = mucosal_get_stats(mucosal, &stats);
    EXPECT_EQ(result, 0);
}

TEST_F(MucosalImmunityTest, StatsTrackSites) {
    uint32_t site_id1, site_id2;
    mucosal_register_boundary(mucosal, MUCOSAL_SITE_INPUT_GATE, 1, &site_id1);
    mucosal_register_boundary(mucosal, MUCOSAL_SITE_OUTPUT_GATE, 2, &site_id2);

    mucosal_stats_t stats;
    mucosal_get_stats(mucosal, &stats);
    EXPECT_EQ(stats.active_sites, 2u);
}

/* ============================================================================
 * Query API Tests
 * ============================================================================ */

TEST_F(MucosalImmunityTest, GetSiteById) {
    uint32_t site_id;
    mucosal_register_boundary(mucosal, MUCOSAL_SITE_INPUT_GATE, 1, &site_id);

    const mucosal_site_t* site = mucosal_get_site(mucosal, site_id);
    ASSERT_NE(site, nullptr);
    EXPECT_EQ(site->id, site_id);
    EXPECT_EQ(site->site_type, MUCOSAL_SITE_INPUT_GATE);
}

TEST_F(MucosalImmunityTest, GetSiteNonExistent) {
    const mucosal_site_t* site = mucosal_get_site(mucosal, 99999);
    EXPECT_EQ(site, nullptr);
}

/* ============================================================================
 * String Conversion Tests
 * ============================================================================ */

TEST_F(MucosalImmunityTest, SiteTypeToString) {
    EXPECT_STREQ(mucosal_site_type_to_string(MUCOSAL_SITE_INPUT_GATE), "INPUT_GATE");
    EXPECT_STREQ(mucosal_site_type_to_string(MUCOSAL_SITE_OUTPUT_GATE), "OUTPUT_GATE");
    EXPECT_STREQ(mucosal_site_type_to_string(MUCOSAL_SITE_MODULE_BOUNDARY), "MODULE_BOUNDARY");
}

TEST_F(MucosalImmunityTest, SIgAStateToString) {
    EXPECT_STREQ(mucosal_siga_state_to_string(MUCOSAL_SIGA_INACTIVE), "INACTIVE");
    EXPECT_STREQ(mucosal_siga_state_to_string(MUCOSAL_SIGA_ACTIVE), "ACTIVE");
    EXPECT_STREQ(mucosal_siga_state_to_string(MUCOSAL_SIGA_DECAYED), "DECAYED");
}

TEST_F(MucosalImmunityTest, ToleranceStateToString) {
    EXPECT_STREQ(mucosal_tolerance_state_to_string(TOLERANCE_NONE), "NONE");
    EXPECT_STREQ(mucosal_tolerance_state_to_string(TOLERANCE_ESTABLISHED), "ESTABLISHED");
    EXPECT_STREQ(mucosal_tolerance_state_to_string(TOLERANCE_BROKEN), "BROKEN");
}

TEST_F(MucosalImmunityTest, MCellStateToString) {
    EXPECT_STREQ(mucosal_m_cell_state_to_string(M_CELL_SAMPLING), "SAMPLING");
    EXPECT_STREQ(mucosal_m_cell_state_to_string(M_CELL_PRESENTING), "PRESENTING");
}

/* ============================================================================
 * Edge Cases
 * ============================================================================ */

TEST_F(MucosalImmunityTest, NullSystemChecks) {
    uint32_t site_id;
    EXPECT_NE(mucosal_register_boundary(nullptr, MUCOSAL_SITE_INPUT_GATE, 1, &site_id), 0);
    EXPECT_NE(mucosal_update(nullptr, 1000), 0);
}

TEST_F(MucosalImmunityTest, MaxSites) {
    for (size_t i = 0; i < config.max_sites + 5; i++) {
        uint32_t site_id;
        mucosal_register_boundary(mucosal, MUCOSAL_SITE_INPUT_GATE, (uint32_t)i, &site_id);
    }

    EXPECT_LE(mucosal->site_count, config.max_sites);
}

TEST_F(MucosalImmunityTest, EmptyDataSample) {
    uint32_t site_id;
    mucosal_register_boundary(mucosal, MUCOSAL_SITE_INPUT_GATE, 1, &site_id);

    uint32_t sample_id;
    int result = mucosal_sample_antigen(mucosal, site_id, nullptr, 0, &sample_id);
    EXPECT_NE(result, 0);
}
