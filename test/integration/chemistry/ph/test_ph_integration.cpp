/**
 * @file test_ph_integration.cpp
 * @brief Integration tests for pH Dynamics module
 *
 * WHAT: Integration tests for pH system with neural activity and metabolism
 * WHY:  Verify pH dynamics integrates correctly with other neural subsystems
 * HOW:  Test pH-activity coupling, metabolic effects, and cross-module signaling
 *
 * @author NIMCP Development Team
 * @date 2026-01-11
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

extern "C" {
#include "chemistry/ph/nimcp_ph_dynamics.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class PHIntegrationTest : public ::testing::Test {
protected:
    nimcp_ph_system_t system;

    void SetUp() override {
        memset(&system, 0, sizeof(system));
        nimcp_ph_error_t err = nimcp_ph_init(&system, nullptr);
        ASSERT_EQ(err, PH_OK);
    }

    void TearDown() override {
        nimcp_ph_shutdown(&system);
    }
};

//=============================================================================
// Activity-pH Coupling Integration Tests
//=============================================================================

TEST_F(PHIntegrationTest, SustainedActivityCausesAcidification) {
    uint32_t region_id;
    nimcp_ph_add_region(&system, "ActiveRegion", &region_id);
    nimcp_ph_region_t* region = nimcp_ph_get_region(&system, region_id);

    /* Add buffer system for realistic dynamics */
    nimcp_ph_add_buffer(region, PH_BUFFER_BICARBONATE, 24.0f);

    float initial_ph;
    nimcp_ph_get_compartment_ph(region, PH_COMPARTMENT_EXTRACELLULAR, &initial_ph);

    /* Sustained high neural activity produces metabolic acid */
    nimcp_ph_set_activity(region, 1.0f);
    for (int i = 0; i < 500; i++) {
        nimcp_ph_update(&system, 10.0f);
    }

    float final_ph;
    nimcp_ph_get_compartment_ph(region, PH_COMPARTMENT_EXTRACELLULAR, &final_ph);

    /* Activity should cause acidification */
    EXPECT_LT(final_ph, initial_ph);
}

TEST_F(PHIntegrationTest, PHRecoveryAfterActivity) {
    uint32_t region_id;
    nimcp_ph_add_region(&system, "RecoveryRegion", &region_id);
    nimcp_ph_region_t* region = nimcp_ph_get_region(&system, region_id);

    nimcp_ph_add_buffer(region, PH_BUFFER_BICARBONATE, 24.0f);

    /* Create acidification with activity */
    nimcp_ph_set_activity(region, 1.0f);
    for (int i = 0; i < 200; i++) {
        nimcp_ph_update(&system, 10.0f);
    }

    float acidified_ph;
    nimcp_ph_get_compartment_ph(region, PH_COMPARTMENT_EXTRACELLULAR, &acidified_ph);

    /* Stop activity and allow recovery */
    nimcp_ph_set_activity(region, 0.0f);
    for (int i = 0; i < 500; i++) {
        nimcp_ph_update(&system, 10.0f);
    }

    float recovered_ph;
    nimcp_ph_get_compartment_ph(region, PH_COMPARTMENT_EXTRACELLULAR, &recovered_ph);

    /* pH should recover toward normal */
    EXPECT_GT(recovered_ph, acidified_ph);
}

//=============================================================================
// Multi-Region Integration Tests
//=============================================================================

TEST_F(PHIntegrationTest, MultiRegionIndependence) {
    uint32_t region1_id, region2_id;
    nimcp_ph_add_region(&system, "Region1", &region1_id);
    nimcp_ph_add_region(&system, "Region2", &region2_id);

    nimcp_ph_region_t* region1 = nimcp_ph_get_region(&system, region1_id);
    nimcp_ph_region_t* region2 = nimcp_ph_get_region(&system, region2_id);

    /* Set different activity levels */
    nimcp_ph_set_activity(region1, 1.0f);  /* High activity */
    nimcp_ph_set_activity(region2, 0.0f);  /* No activity */

    for (int i = 0; i < 200; i++) {
        nimcp_ph_update(&system, 10.0f);
    }

    float ph1, ph2;
    nimcp_ph_get_compartment_ph(region1, PH_COMPARTMENT_EXTRACELLULAR, &ph1);
    nimcp_ph_get_compartment_ph(region2, PH_COMPARTMENT_EXTRACELLULAR, &ph2);

    /* Regions should have different pH due to different activity */
    EXPECT_NE(ph1, ph2);
    EXPECT_LT(ph1, ph2);  /* Active region more acidic */
}

TEST_F(PHIntegrationTest, SystemWideMetrics) {
    /* Create multiple regions with varying activity */
    uint32_t ids[5];
    for (int i = 0; i < 5; i++) {
        char name[32];
        snprintf(name, sizeof(name), "Region%d", i);
        nimcp_ph_add_region(&system, name, &ids[i]);
        nimcp_ph_region_t* region = nimcp_ph_get_region(&system, ids[i]);
        nimcp_ph_set_activity(region, i * 0.2f);
    }

    for (int i = 0; i < 100; i++) {
        nimcp_ph_update(&system, 10.0f);
    }

    nimcp_ph_metrics_t metrics;
    nimcp_ph_error_t err = nimcp_ph_get_metrics(&system, &metrics);
    EXPECT_EQ(err, PH_OK);

    /* Verify metrics reflect multi-region state */
    EXPECT_GT(metrics.total_simulation_time, 0.0f);
}

//=============================================================================
// Buffer-Pump Integration Tests
//=============================================================================

TEST_F(PHIntegrationTest, BufferProtectsAgainstAcidosis) {
    uint32_t buffered_id, unbuffered_id;
    nimcp_ph_add_region(&system, "Buffered", &buffered_id);
    nimcp_ph_add_region(&system, "Unbuffered", &unbuffered_id);

    nimcp_ph_region_t* buffered = nimcp_ph_get_region(&system, buffered_id);
    nimcp_ph_region_t* unbuffered = nimcp_ph_get_region(&system, unbuffered_id);

    /* Add strong buffering to one region */
    nimcp_ph_add_buffer(buffered, PH_BUFFER_BICARBONATE, 24.0f);
    nimcp_ph_add_buffer(buffered, PH_BUFFER_PHOSPHATE, 1.0f);
    nimcp_ph_add_buffer(buffered, PH_BUFFER_PROTEIN, 70.0f);

    /* Same activity level for both - activity produces acid */
    nimcp_ph_set_activity(buffered, 0.8f);
    nimcp_ph_set_activity(unbuffered, 0.8f);

    /* Let the system run with activity-induced acid production */
    for (int i = 0; i < 200; i++) {
        nimcp_ph_update(&system, 10.0f);
    }

    float ph_buffered, ph_unbuffered;
    nimcp_ph_get_compartment_ph(buffered, PH_COMPARTMENT_EXTRACELLULAR, &ph_buffered);
    nimcp_ph_get_compartment_ph(unbuffered, PH_COMPARTMENT_EXTRACELLULAR, &ph_unbuffered);

    /* Buffered region should be less acidic (higher pH) */
    EXPECT_GE(ph_buffered, ph_unbuffered);
}

TEST_F(PHIntegrationTest, PumpActivityRestoresPH) {
    uint32_t region_id;
    nimcp_ph_add_region(&system, "PumpRegion", &region_id);
    nimcp_ph_region_t* region = nimcp_ph_get_region(&system, region_id);

    /* Enable pumps */
    nimcp_ph_set_pump_enabled(region, PH_PUMP_NHE, true);
    nimcp_ph_set_pump_activity(region, PH_PUMP_NHE, 1.0f);

    /* Apply acid load */
    nimcp_ph_apply_acid_load(region, PH_COMPARTMENT_INTRACELLULAR, 0.00001f);

    float acidified_ph;
    nimcp_ph_get_compartment_ph(region, PH_COMPARTMENT_INTRACELLULAR, &acidified_ph);

    /* Let pumps work */
    for (int i = 0; i < 500; i++) {
        nimcp_ph_update(&system, 10.0f);
    }

    float recovered_ph;
    nimcp_ph_get_compartment_ph(region, PH_COMPARTMENT_INTRACELLULAR, &recovered_ph);

    /* Pumps should help restore pH */
    EXPECT_GE(recovered_ph, acidified_ph);
}

//=============================================================================
// Effects Integration Tests
//=============================================================================

TEST_F(PHIntegrationTest, AcidosisReducesConductance) {
    uint32_t region_id;
    nimcp_ph_add_region(&system, "ConductanceRegion", &region_id);
    nimcp_ph_region_t* region = nimcp_ph_get_region(&system, region_id);

    /* Get baseline conductance modifier */
    float baseline_modifier;
    nimcp_ph_get_conductance_modifier(&system, region_id, &baseline_modifier);

    /* Induce acidosis */
    nimcp_ph_set_compartment_ph(region, PH_COMPARTMENT_EXTRACELLULAR, 6.9f);
    nimcp_ph_update(&system, 1.0f);

    float acidotic_modifier;
    nimcp_ph_get_conductance_modifier(&system, region_id, &acidotic_modifier);

    /* Acidosis should reduce conductance */
    EXPECT_LT(acidotic_modifier, baseline_modifier);
}

TEST_F(PHIntegrationTest, VesicularPHAffectsRelease) {
    uint32_t region_id;
    nimcp_ph_add_region(&system, "VesicleRegion", &region_id);
    nimcp_ph_region_t* region = nimcp_ph_get_region(&system, region_id);

    /* Get baseline release modifier */
    float baseline_release;
    nimcp_ph_get_release_modifier(&system, region_id, &baseline_release);

    /* Alkalinize vesicles (impairs neurotransmitter loading) */
    nimcp_ph_set_compartment_ph(region, PH_COMPARTMENT_VESICULAR, 6.5f);
    nimcp_ph_update(&system, 1.0f);

    float impaired_release;
    nimcp_ph_get_release_modifier(&system, region_id, &impaired_release);

    /* Alkaline vesicles should impair release */
    EXPECT_LE(impaired_release, baseline_release);
}

//=============================================================================
// Long-Running Stability Tests
//=============================================================================

TEST_F(PHIntegrationTest, LongTermStability) {
    uint32_t region_id;
    nimcp_ph_add_region(&system, "StableRegion", &region_id);
    nimcp_ph_region_t* region = nimcp_ph_get_region(&system, region_id);

    /* Add multiple buffer systems for better stability */
    nimcp_ph_add_buffer(region, PH_BUFFER_BICARBONATE, 24.0f);
    nimcp_ph_add_buffer(region, PH_BUFFER_PHOSPHATE, 1.0f);
    nimcp_ph_add_buffer(region, PH_BUFFER_PROTEIN, 70.0f);
    nimcp_ph_set_pump_enabled(region, PH_PUMP_NHE, true);
    nimcp_ph_set_pump_activity(region, PH_PUMP_NHE, 1.0f);

    /* Simulate long-term operation with low-moderate activity */
    for (int cycle = 0; cycle < 10; cycle++) {
        /* Active phase with low-moderate activity */
        nimcp_ph_set_activity(region, 0.3f);
        for (int i = 0; i < 30; i++) {
            nimcp_ph_update(&system, 10.0f);
        }

        /* Long rest phase for recovery */
        nimcp_ph_set_activity(region, 0.0f);
        for (int i = 0; i < 200; i++) {
            nimcp_ph_update(&system, 10.0f);
        }
    }

    /* System should remain stable - values should be valid */
    float final_ph;
    nimcp_ph_get_compartment_ph(region, PH_COMPARTMENT_EXTRACELLULAR, &final_ph);

    /* Verify numerical stability - no NaN or inf */
    EXPECT_FALSE(std::isnan(final_ph));
    EXPECT_FALSE(std::isinf(final_ph));

    /* pH should remain within physiological bounds */
    EXPECT_GE(final_ph, PH_MINIMUM_VIABLE);
    EXPECT_LE(final_ph, PH_MAXIMUM_VIABLE);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
