/**
 * @file test_ph_regression.cpp
 * @brief Regression tests for pH Dynamics module
 *
 * WHAT: Regression tests to prevent pH dynamics bugs from recurring
 * WHY:  Ensure fixed issues remain fixed across code changes
 * HOW:  Test specific edge cases and previously-found bugs
 *
 * @author NIMCP Development Team
 * @date 2026-01-11
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>

extern "C" {
#include "chemistry/ph/nimcp_ph_dynamics.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class PHRegressionTest : public ::testing::Test {
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
// Numerical Stability Regression Tests
//=============================================================================

TEST_F(PHRegressionTest, NoNaNWithZeroBuffer) {
    /* Previously: Zero buffer concentration caused division by zero */
    uint32_t region_id;
    nimcp_ph_add_region(&system, "ZeroBuffer", &region_id);
    nimcp_ph_region_t* region = nimcp_ph_get_region(&system, region_id);

    /* Don't add any buffers - should still work */
    nimcp_ph_set_activity(region, 1.0f);

    for (int i = 0; i < 100; i++) {
        nimcp_ph_update(&system, 10.0f);
    }

    float ph;
    nimcp_ph_get_compartment_ph(region, PH_COMPARTMENT_EXTRACELLULAR, &ph);

    EXPECT_FALSE(std::isnan(ph));
    EXPECT_FALSE(std::isinf(ph));
}

TEST_F(PHRegressionTest, NoOverflowWithExtremeAcidLoad) {
    /* Previously: Large acid loads caused pH to underflow to NaN */
    uint32_t region_id;
    nimcp_ph_add_region(&system, "ExtremeLoad", &region_id);
    nimcp_ph_region_t* region = nimcp_ph_get_region(&system, region_id);

    nimcp_ph_add_buffer(region, PH_BUFFER_BICARBONATE, 24.0f);

    /* Apply extreme but valid acid load */
    for (int i = 0; i < 100; i++) {
        nimcp_ph_apply_acid_load(region, PH_COMPARTMENT_EXTRACELLULAR, 0.0001f);
        nimcp_ph_update(&system, 10.0f);
    }

    float ph;
    nimcp_ph_get_compartment_ph(region, PH_COMPARTMENT_EXTRACELLULAR, &ph);

    EXPECT_FALSE(std::isnan(ph));
    EXPECT_FALSE(std::isinf(ph));
    EXPECT_GE(ph, 0.0f);  /* pH can't be negative */
    EXPECT_LE(ph, 14.0f); /* pH scale maximum */
}

TEST_F(PHRegressionTest, NoUnderflowWithExtremeBaseLoad) {
    /* Previously: Large base loads caused pH to overflow */
    uint32_t region_id;
    nimcp_ph_add_region(&system, "ExtremeBase", &region_id);
    nimcp_ph_region_t* region = nimcp_ph_get_region(&system, region_id);

    nimcp_ph_add_buffer(region, PH_BUFFER_BICARBONATE, 24.0f);

    /* Apply extreme base load */
    for (int i = 0; i < 100; i++) {
        nimcp_ph_apply_base_load(region, PH_COMPARTMENT_EXTRACELLULAR, 0.0001f);
        nimcp_ph_update(&system, 10.0f);
    }

    float ph;
    nimcp_ph_get_compartment_ph(region, PH_COMPARTMENT_EXTRACELLULAR, &ph);

    EXPECT_FALSE(std::isnan(ph));
    EXPECT_FALSE(std::isinf(ph));
    EXPECT_GE(ph, 0.0f);
    EXPECT_LE(ph, 14.0f);
}

TEST_F(PHRegressionTest, StabilityWithRapidUpdates) {
    /* Previously: Very small timesteps accumulated numerical errors */
    uint32_t region_id;
    nimcp_ph_add_region(&system, "RapidUpdate", &region_id);
    nimcp_ph_region_t* region = nimcp_ph_get_region(&system, region_id);

    nimcp_ph_add_buffer(region, PH_BUFFER_BICARBONATE, 24.0f);
    nimcp_ph_set_activity(region, 0.5f);

    /* Very small timesteps */
    for (int i = 0; i < 10000; i++) {
        nimcp_ph_update(&system, 0.1f);
    }

    float ph;
    nimcp_ph_get_compartment_ph(region, PH_COMPARTMENT_EXTRACELLULAR, &ph);

    EXPECT_FALSE(std::isnan(ph));
    EXPECT_FALSE(std::isinf(ph));
    EXPECT_GE(ph, PH_MINIMUM_VIABLE);
    EXPECT_LE(ph, PH_MAXIMUM_VIABLE);
}

//=============================================================================
// Boundary Condition Regression Tests
//=============================================================================

TEST_F(PHRegressionTest, PHClampsAtPhysiologicalLimits) {
    /* Previously: pH could exceed physiological bounds */
    uint32_t region_id;
    nimcp_ph_add_region(&system, "ClampTest", &region_id);
    nimcp_ph_region_t* region = nimcp_ph_get_region(&system, region_id);

    /* Try to set pH outside valid range */
    nimcp_ph_set_compartment_ph(region, PH_COMPARTMENT_EXTRACELLULAR, 15.0f);

    float ph;
    nimcp_ph_get_compartment_ph(region, PH_COMPARTMENT_EXTRACELLULAR, &ph);

    /* Should be clamped to maximum */
    EXPECT_LE(ph, 14.0f);

    /* Try to set pH below minimum */
    nimcp_ph_set_compartment_ph(region, PH_COMPARTMENT_EXTRACELLULAR, -1.0f);
    nimcp_ph_get_compartment_ph(region, PH_COMPARTMENT_EXTRACELLULAR, &ph);

    EXPECT_GE(ph, 0.0f);
}

TEST_F(PHRegressionTest, BufferCapacityRespectsLimits) {
    /* Previously: Buffer capacity could become negative */
    uint32_t region_id;
    nimcp_ph_add_region(&system, "BufferLimit", &region_id);
    nimcp_ph_region_t* region = nimcp_ph_get_region(&system, region_id);

    /* Add buffer then try to exhaust it */
    nimcp_ph_add_buffer(region, PH_BUFFER_BICARBONATE, 0.001f);  /* Very small amount */

    /* Massive acid load */
    nimcp_ph_apply_acid_load(region, PH_COMPARTMENT_EXTRACELLULAR, 0.01f);

    float buffer_capacity;
    nimcp_ph_get_buffering_capacity(region, PH_COMPARTMENT_EXTRACELLULAR, &buffer_capacity);

    /* Buffer capacity should not be negative */
    EXPECT_GE(buffer_capacity, 0.0f);
}

TEST_F(PHRegressionTest, ActivityDoesNotExceedOne) {
    /* Previously: Activity could be set >1.0 causing runaway effects */
    uint32_t region_id;
    nimcp_ph_add_region(&system, "ActivityLimit", &region_id);
    nimcp_ph_region_t* region = nimcp_ph_get_region(&system, region_id);

    /* Try to set activity above 1.0 */
    nimcp_ph_set_activity(region, 10.0f);

    /* Access activity directly from struct - should be clamped */
    EXPECT_LE(region->activity_level, 1.0f);
}

//=============================================================================
// State Machine Regression Tests
//=============================================================================

TEST_F(PHRegressionTest, StatusTransitionsCorrectly) {
    /* Previously: Status didn't update after pH changes */
    uint32_t region_id;
    nimcp_ph_add_region(&system, "StatusTransition", &region_id);
    nimcp_ph_region_t* region = nimcp_ph_get_region(&system, region_id);

    nimcp_ph_status_t initial_status = nimcp_ph_get_status(region);
    (void)initial_status;  /* Silence unused variable warning */

    /* Induce acidosis */
    nimcp_ph_set_compartment_ph(region, PH_COMPARTMENT_EXTRACELLULAR, 6.8f);
    nimcp_ph_update(&system, 1.0f);

    nimcp_ph_status_t acidic_status = nimcp_ph_get_status(region);

    /* Status should reflect acidosis */
    EXPECT_NE(acidic_status, PH_STATUS_NORMAL);

    /* Return to normal */
    nimcp_ph_set_compartment_ph(region, PH_COMPARTMENT_EXTRACELLULAR, 7.4f);
    nimcp_ph_update(&system, 1.0f);

    nimcp_ph_status_t recovered_status = nimcp_ph_get_status(region);

    /* Should reflect normal */
    EXPECT_EQ(recovered_status, PH_STATUS_NORMAL);
}

TEST_F(PHRegressionTest, CriticalFlagSetsCorrectly) {
    /* Previously: Critical flag didn't set when pH was dangerous */
    uint32_t region_id;
    nimcp_ph_add_region(&system, "CriticalFlag", &region_id);
    nimcp_ph_region_t* region = nimcp_ph_get_region(&system, region_id);

    EXPECT_FALSE(nimcp_ph_is_critical(region, PH_COMPARTMENT_EXTRACELLULAR));

    /* Set dangerously low pH */
    nimcp_ph_set_compartment_ph(region, PH_COMPARTMENT_EXTRACELLULAR, 6.5f);
    nimcp_ph_update(&system, 1.0f);

    EXPECT_TRUE(nimcp_ph_is_critical(region, PH_COMPARTMENT_EXTRACELLULAR));
}

//=============================================================================
// Multi-Region Regression Tests
//=============================================================================

TEST_F(PHRegressionTest, RegionsDoNotInterfere) {
    /* Previously: Updating one region affected another */
    uint32_t id1, id2;
    nimcp_ph_add_region(&system, "Region1", &id1);
    nimcp_ph_add_region(&system, "Region2", &id2);

    nimcp_ph_region_t* region1 = nimcp_ph_get_region(&system, id1);
    nimcp_ph_region_t* region2 = nimcp_ph_get_region(&system, id2);

    /* Set different pH values */
    nimcp_ph_set_compartment_ph(region1, PH_COMPARTMENT_EXTRACELLULAR, 7.0f);
    nimcp_ph_set_compartment_ph(region2, PH_COMPARTMENT_EXTRACELLULAR, 7.8f);

    /* Update only affects internal state, not cross-region */
    nimcp_ph_update(&system, 100.0f);

    float ph1, ph2;
    nimcp_ph_get_compartment_ph(region1, PH_COMPARTMENT_EXTRACELLULAR, &ph1);
    nimcp_ph_get_compartment_ph(region2, PH_COMPARTMENT_EXTRACELLULAR, &ph2);

    /* Regions should maintain their independence */
    EXPECT_NE(ph1, ph2);
}

TEST_F(PHRegressionTest, MaxRegionsHandledCorrectly) {
    /* Previously: Adding many regions caused memory corruption */
    std::vector<uint32_t> ids;

    /* Add maximum number of regions */
    for (int i = 0; i < 100; i++) {
        char name[32];
        snprintf(name, sizeof(name), "Region%d", i);
        uint32_t id;
        nimcp_ph_error_t err = nimcp_ph_add_region(&system, name, &id);
        if (err == PH_OK) {
            ids.push_back(id);
        }
    }

    /* Verify all added regions are accessible */
    for (size_t i = 0; i < ids.size(); i++) {
        nimcp_ph_region_t* region = nimcp_ph_get_region(&system, ids[i]);
        EXPECT_NE(region, nullptr);

        float ph;
        nimcp_ph_get_compartment_ph(region, PH_COMPARTMENT_EXTRACELLULAR, &ph);
        EXPECT_FALSE(std::isnan(ph));
    }
}

//=============================================================================
// Pump Regression Tests
//=============================================================================

TEST_F(PHRegressionTest, DisabledPumpDoesNotAffectPH) {
    /* Previously: Disabled pumps still affected pH */
    uint32_t region_id;
    nimcp_ph_add_region(&system, "PumpDisabled", &region_id);
    nimcp_ph_region_t* region = nimcp_ph_get_region(&system, region_id);

    /* Apply acid load without enabling pump */
    nimcp_ph_set_pump_enabled(region, PH_PUMP_NHE, false);

    float before_ph;
    nimcp_ph_get_compartment_ph(region, PH_COMPARTMENT_INTRACELLULAR, &before_ph);

    nimcp_ph_apply_acid_load(region, PH_COMPARTMENT_INTRACELLULAR, 0.00001f);

    for (int i = 0; i < 100; i++) {
        nimcp_ph_update(&system, 10.0f);
    }

    float after_ph;
    nimcp_ph_get_compartment_ph(region, PH_COMPARTMENT_INTRACELLULAR, &after_ph);

    /* Without pump, pH should remain acidified (not recover) */
    EXPECT_LE(after_ph, before_ph);
}

TEST_F(PHRegressionTest, PumpActivityScalesCorrectly) {
    /* Previously: Pump activity didn't scale effect properly */
    uint32_t low_id, high_id;
    nimcp_ph_add_region(&system, "LowPump", &low_id);
    nimcp_ph_add_region(&system, "HighPump", &high_id);

    nimcp_ph_region_t* low_pump = nimcp_ph_get_region(&system, low_id);
    nimcp_ph_region_t* high_pump = nimcp_ph_get_region(&system, high_id);

    /* Same acid load, different pump activities */
    nimcp_ph_apply_acid_load(low_pump, PH_COMPARTMENT_INTRACELLULAR, 0.00001f);
    nimcp_ph_apply_acid_load(high_pump, PH_COMPARTMENT_INTRACELLULAR, 0.00001f);

    nimcp_ph_set_pump_enabled(low_pump, PH_PUMP_NHE, true);
    nimcp_ph_set_pump_enabled(high_pump, PH_PUMP_NHE, true);

    nimcp_ph_set_pump_activity(low_pump, PH_PUMP_NHE, 0.1f);
    nimcp_ph_set_pump_activity(high_pump, PH_PUMP_NHE, 1.0f);

    for (int i = 0; i < 500; i++) {
        nimcp_ph_update(&system, 10.0f);
    }

    float low_ph, high_ph;
    nimcp_ph_get_compartment_ph(low_pump, PH_COMPARTMENT_INTRACELLULAR, &low_ph);
    nimcp_ph_get_compartment_ph(high_pump, PH_COMPARTMENT_INTRACELLULAR, &high_ph);

    /* Higher pump activity should result in better pH recovery */
    EXPECT_GE(high_ph, low_ph);
}

//=============================================================================
// Memory/Resource Regression Tests
//=============================================================================

TEST_F(PHRegressionTest, InitShutdownCycle) {
    /* Previously: Multiple init/shutdown cycles caused memory leak */
    for (int cycle = 0; cycle < 10; cycle++) {
        nimcp_ph_system_t temp_system;
        memset(&temp_system, 0, sizeof(temp_system));

        nimcp_ph_error_t err = nimcp_ph_init(&temp_system, nullptr);
        EXPECT_EQ(err, PH_OK);

        uint32_t id;
        nimcp_ph_add_region(&temp_system, "TempRegion", &id);

        err = nimcp_ph_shutdown(&temp_system);
        EXPECT_EQ(err, PH_OK);
    }
    /* No memory leak assertion - would need valgrind */
}

TEST_F(PHRegressionTest, MetricsAccumulateCorrectly) {
    /* Previously: Metrics didn't accumulate over time */
    uint32_t region_id;
    nimcp_ph_add_region(&system, "MetricsRegion", &region_id);
    nimcp_ph_region_t* region = nimcp_ph_get_region(&system, region_id);

    nimcp_ph_set_activity(region, 0.5f);

    for (int i = 0; i < 100; i++) {
        nimcp_ph_update(&system, 10.0f);
    }

    nimcp_ph_metrics_t metrics1;
    nimcp_ph_get_metrics(&system, &metrics1);

    for (int i = 0; i < 100; i++) {
        nimcp_ph_update(&system, 10.0f);
    }

    nimcp_ph_metrics_t metrics2;
    nimcp_ph_get_metrics(&system, &metrics2);

    /* Metrics should increase over time */
    EXPECT_GT(metrics2.total_simulation_time, metrics1.total_simulation_time);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
