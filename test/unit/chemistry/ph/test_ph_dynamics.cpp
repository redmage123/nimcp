/**
 * @file test_ph_dynamics.cpp
 * @brief Unit tests for pH Dynamics module
 *
 * WHAT: Test suite for nimcp_ph_dynamics
 * WHY:  Verify pH regulation, buffer systems, and pump operations
 * HOW:  Unit tests for create, modify, and lifecycle operations
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

class PHDynamicsTest : public ::testing::Test {
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
// Initialization Tests
//=============================================================================

TEST(PHInitTest, InitWithNullConfig) {
    nimcp_ph_system_t sys;
    memset(&sys, 0, sizeof(sys));
    nimcp_ph_error_t err = nimcp_ph_init(&sys, nullptr);
    EXPECT_EQ(err, PH_OK);
    EXPECT_TRUE(sys.initialized);
    nimcp_ph_shutdown(&sys);
}

TEST(PHInitTest, InitWithCustomConfig) {
    nimcp_ph_system_t sys;
    memset(&sys, 0, sizeof(sys));

    nimcp_ph_config_t config = {
        .initial_extracellular_ph = 7.35f,
        .initial_intracellular_ph = 7.15f,
        .initial_vesicular_ph = 5.6f,
        .bicarbonate_concentration = 24.0f,
        .phosphate_concentration = 1.0f,
        .protein_concentration = 70.0f,
        .v_atpase_activity = 0.9f,
        .nhe_activity = 0.8f,
        .ph_recovery_rate = 0.1f,
        .activity_acid_factor = 0.05f,
        .acidosis_threshold = 7.2f,
        .alkalosis_threshold = 7.5f,
        .on_status_change = nullptr,
        .on_critical = nullptr,
        .callback_data = nullptr
    };

    nimcp_ph_error_t err = nimcp_ph_init(&sys, &config);
    EXPECT_EQ(err, PH_OK);
    nimcp_ph_shutdown(&sys);
}

TEST(PHInitTest, InitNull) {
    nimcp_ph_error_t err = nimcp_ph_init(nullptr, nullptr);
    EXPECT_EQ(err, PH_ERR_NULL_PTR);
}

TEST(PHInitTest, DoubleInit) {
    nimcp_ph_system_t sys;
    memset(&sys, 0, sizeof(sys));
    nimcp_ph_init(&sys, nullptr);
    nimcp_ph_error_t err = nimcp_ph_init(&sys, nullptr);
    EXPECT_EQ(err, PH_ERR_ALREADY_INITIALIZED);
    nimcp_ph_shutdown(&sys);
}

TEST(PHInitTest, ShutdownNull) {
    nimcp_ph_error_t err = nimcp_ph_shutdown(nullptr);
    EXPECT_EQ(err, PH_ERR_NULL_PTR);
}

TEST(PHInitTest, ShutdownNotInitialized) {
    /* Shutdown is safe to call on non-initialized systems */
    nimcp_ph_system_t sys;
    memset(&sys, 0, sizeof(sys));
    nimcp_ph_error_t err = nimcp_ph_shutdown(&sys);
    EXPECT_EQ(err, PH_OK);  /* Safe shutdown, no error */
}

//=============================================================================
// Region Management Tests
//=============================================================================

TEST_F(PHDynamicsTest, AddRegion) {
    uint32_t region_id;
    nimcp_ph_error_t err = nimcp_ph_add_region(&system, "TestRegion", &region_id);
    EXPECT_EQ(err, PH_OK);
    EXPECT_GE(region_id, 0u);
}

TEST_F(PHDynamicsTest, AddMultipleRegions) {
    uint32_t id1, id2, id3;
    nimcp_ph_add_region(&system, "Region1", &id1);
    nimcp_ph_add_region(&system, "Region2", &id2);
    nimcp_ph_error_t err = nimcp_ph_add_region(&system, "Region3", &id3);

    EXPECT_EQ(err, PH_OK);
    EXPECT_NE(id1, id2);
    EXPECT_NE(id2, id3);
}

TEST_F(PHDynamicsTest, GetRegion) {
    uint32_t region_id;
    nimcp_ph_add_region(&system, "TestRegion", &region_id);

    nimcp_ph_region_t* region = nimcp_ph_get_region(&system, region_id);
    ASSERT_NE(region, nullptr);
    EXPECT_STREQ(region->name, "TestRegion");
}

TEST_F(PHDynamicsTest, GetRegionNotFound) {
    nimcp_ph_region_t* region = nimcp_ph_get_region(&system, 999);
    EXPECT_EQ(region, nullptr);
}

TEST_F(PHDynamicsTest, RemoveRegion) {
    uint32_t region_id;
    nimcp_ph_add_region(&system, "TestRegion", &region_id);

    nimcp_ph_error_t err = nimcp_ph_remove_region(&system, region_id);
    EXPECT_EQ(err, PH_OK);

    nimcp_ph_region_t* region = nimcp_ph_get_region(&system, region_id);
    EXPECT_EQ(region, nullptr);
}

TEST_F(PHDynamicsTest, RemoveRegionNotFound) {
    nimcp_ph_error_t err = nimcp_ph_remove_region(&system, 999);
    EXPECT_EQ(err, PH_ERR_REGION_NOT_FOUND);
}

//=============================================================================
// pH Control Tests
//=============================================================================

TEST_F(PHDynamicsTest, SetCompartmentPH) {
    uint32_t region_id;
    nimcp_ph_add_region(&system, "TestRegion", &region_id);
    nimcp_ph_region_t* region = nimcp_ph_get_region(&system, region_id);

    nimcp_ph_error_t err = nimcp_ph_set_compartment_ph(region, PH_COMPARTMENT_EXTRACELLULAR, 7.3f);
    EXPECT_EQ(err, PH_OK);

    float ph;
    nimcp_ph_get_compartment_ph(region, PH_COMPARTMENT_EXTRACELLULAR, &ph);
    EXPECT_NEAR(ph, 7.3f, 0.01f);
}

TEST_F(PHDynamicsTest, SetCompartmentPHNull) {
    nimcp_ph_error_t err = nimcp_ph_set_compartment_ph(nullptr, PH_COMPARTMENT_EXTRACELLULAR, 7.3f);
    EXPECT_EQ(err, PH_ERR_NULL_PTR);
}

TEST_F(PHDynamicsTest, GetCompartmentPH) {
    uint32_t region_id;
    nimcp_ph_add_region(&system, "TestRegion", &region_id);
    nimcp_ph_region_t* region = nimcp_ph_get_region(&system, region_id);

    float ph;
    nimcp_ph_error_t err = nimcp_ph_get_compartment_ph(region, PH_COMPARTMENT_EXTRACELLULAR, &ph);
    EXPECT_EQ(err, PH_OK);
    EXPECT_NEAR(ph, PH_EXTRACELLULAR_NORMAL, 0.1f);
}

TEST_F(PHDynamicsTest, GetCompartmentPHNull) {
    nimcp_ph_error_t err = nimcp_ph_get_compartment_ph(nullptr, PH_COMPARTMENT_EXTRACELLULAR, nullptr);
    EXPECT_EQ(err, PH_ERR_NULL_PTR);
}

TEST_F(PHDynamicsTest, ApplyAcidLoad) {
    uint32_t region_id;
    nimcp_ph_add_region(&system, "TestRegion", &region_id);
    nimcp_ph_region_t* region = nimcp_ph_get_region(&system, region_id);

    float initial_ph;
    nimcp_ph_get_compartment_ph(region, PH_COMPARTMENT_EXTRACELLULAR, &initial_ph);

    /* Use a tiny load (mM) - even small amounts cause significant pH shifts */
    nimcp_ph_error_t err = nimcp_ph_apply_acid_load(region, PH_COMPARTMENT_EXTRACELLULAR, 0.00001f);
    EXPECT_EQ(err, PH_OK);

    float new_ph;
    nimcp_ph_get_compartment_ph(region, PH_COMPARTMENT_EXTRACELLULAR, &new_ph);
    EXPECT_LT(new_ph, initial_ph);
}

TEST_F(PHDynamicsTest, ApplyBaseLoad) {
    uint32_t region_id;
    nimcp_ph_add_region(&system, "TestRegion", &region_id);
    nimcp_ph_region_t* region = nimcp_ph_get_region(&system, region_id);

    float initial_ph;
    nimcp_ph_get_compartment_ph(region, PH_COMPARTMENT_EXTRACELLULAR, &initial_ph);

    /* Use a tiny load (mM) - even small amounts cause significant pH shifts */
    nimcp_ph_error_t err = nimcp_ph_apply_base_load(region, PH_COMPARTMENT_EXTRACELLULAR, 0.00001f);
    EXPECT_EQ(err, PH_OK);

    float new_ph;
    nimcp_ph_get_compartment_ph(region, PH_COMPARTMENT_EXTRACELLULAR, &new_ph);
    EXPECT_GT(new_ph, initial_ph);
}

//=============================================================================
// Buffer System Tests
//=============================================================================

TEST_F(PHDynamicsTest, AddBuffer) {
    uint32_t region_id;
    nimcp_ph_add_region(&system, "TestRegion", &region_id);
    nimcp_ph_region_t* region = nimcp_ph_get_region(&system, region_id);

    nimcp_ph_error_t err = nimcp_ph_add_buffer(region, PH_BUFFER_BICARBONATE, 24.0f);
    EXPECT_EQ(err, PH_OK);
    EXPECT_EQ(region->num_buffers, 1u);
}

TEST_F(PHDynamicsTest, AddMultipleBuffers) {
    uint32_t region_id;
    nimcp_ph_add_region(&system, "TestRegion", &region_id);
    nimcp_ph_region_t* region = nimcp_ph_get_region(&system, region_id);

    nimcp_ph_add_buffer(region, PH_BUFFER_BICARBONATE, 24.0f);
    nimcp_ph_add_buffer(region, PH_BUFFER_PHOSPHATE, 1.0f);
    nimcp_ph_error_t err = nimcp_ph_add_buffer(region, PH_BUFFER_PROTEIN, 70.0f);

    EXPECT_EQ(err, PH_OK);
    EXPECT_EQ(region->num_buffers, 3u);
}

TEST_F(PHDynamicsTest, GetBufferingCapacity) {
    uint32_t region_id;
    nimcp_ph_add_region(&system, "TestRegion", &region_id);
    nimcp_ph_region_t* region = nimcp_ph_get_region(&system, region_id);

    nimcp_ph_add_buffer(region, PH_BUFFER_BICARBONATE, 24.0f);

    float capacity;
    nimcp_ph_error_t err = nimcp_ph_get_buffering_capacity(region, PH_COMPARTMENT_EXTRACELLULAR, &capacity);
    EXPECT_EQ(err, PH_OK);
    EXPECT_GT(capacity, 0.0f);
}

TEST_F(PHDynamicsTest, CalculateBufferResponse) {
    uint32_t region_id;
    nimcp_ph_add_region(&system, "TestRegion", &region_id);
    nimcp_ph_region_t* region = nimcp_ph_get_region(&system, region_id);

    nimcp_ph_add_buffer(region, PH_BUFFER_BICARBONATE, 24.0f);

    float delta_ph;
    nimcp_ph_error_t err = nimcp_ph_calculate_buffer_response(region, 0.001f, &delta_ph);
    EXPECT_EQ(err, PH_OK);
}

//=============================================================================
// Proton Pump Tests
//=============================================================================

TEST_F(PHDynamicsTest, SetPumpActivity) {
    uint32_t region_id;
    nimcp_ph_add_region(&system, "TestRegion", &region_id);
    nimcp_ph_region_t* region = nimcp_ph_get_region(&system, region_id);

    nimcp_ph_error_t err = nimcp_ph_set_pump_activity(region, PH_PUMP_V_ATPASE, 0.8f);
    EXPECT_EQ(err, PH_OK);

    float activity;
    nimcp_ph_get_pump_activity(region, PH_PUMP_V_ATPASE, &activity);
    EXPECT_NEAR(activity, 0.8f, 0.01f);
}

TEST_F(PHDynamicsTest, GetPumpActivity) {
    uint32_t region_id;
    nimcp_ph_add_region(&system, "TestRegion", &region_id);
    nimcp_ph_region_t* region = nimcp_ph_get_region(&system, region_id);

    float activity;
    nimcp_ph_error_t err = nimcp_ph_get_pump_activity(region, PH_PUMP_NHE, &activity);
    EXPECT_EQ(err, PH_OK);
}

TEST_F(PHDynamicsTest, SetPumpEnabled) {
    uint32_t region_id;
    nimcp_ph_add_region(&system, "TestRegion", &region_id);
    nimcp_ph_region_t* region = nimcp_ph_get_region(&system, region_id);

    nimcp_ph_error_t err = nimcp_ph_set_pump_enabled(region, PH_PUMP_V_ATPASE, false);
    EXPECT_EQ(err, PH_OK);
    EXPECT_FALSE(region->pumps[PH_PUMP_V_ATPASE].enabled);

    err = nimcp_ph_set_pump_enabled(region, PH_PUMP_V_ATPASE, true);
    EXPECT_EQ(err, PH_OK);
    EXPECT_TRUE(region->pumps[PH_PUMP_V_ATPASE].enabled);
}

//=============================================================================
// Update Tests
//=============================================================================

TEST_F(PHDynamicsTest, UpdateSystem) {
    uint32_t region_id;
    nimcp_ph_add_region(&system, "TestRegion", &region_id);

    nimcp_ph_error_t err = nimcp_ph_update(&system, 1.0f);
    EXPECT_EQ(err, PH_OK);
    EXPECT_EQ(system.update_count, 1u);
}

TEST_F(PHDynamicsTest, UpdateMultiple) {
    uint32_t region_id;
    nimcp_ph_add_region(&system, "TestRegion", &region_id);

    for (int i = 0; i < 100; i++) {
        nimcp_ph_error_t err = nimcp_ph_update(&system, 1.0f);
        EXPECT_EQ(err, PH_OK);
    }
    EXPECT_EQ(system.update_count, 100u);
}

TEST_F(PHDynamicsTest, UpdateRegion) {
    uint32_t region_id;
    nimcp_ph_add_region(&system, "TestRegion", &region_id);
    nimcp_ph_region_t* region = nimcp_ph_get_region(&system, region_id);

    nimcp_ph_error_t err = nimcp_ph_update_region(&system, region, 1.0f);
    EXPECT_EQ(err, PH_OK);
}

TEST_F(PHDynamicsTest, SetActivity) {
    uint32_t region_id;
    nimcp_ph_add_region(&system, "TestRegion", &region_id);
    nimcp_ph_region_t* region = nimcp_ph_get_region(&system, region_id);

    nimcp_ph_error_t err = nimcp_ph_set_activity(region, 0.5f);
    EXPECT_EQ(err, PH_OK);
    EXPECT_NEAR(region->activity_level, 0.5f, 0.01f);
}

TEST_F(PHDynamicsTest, ActivityCausesAcidification) {
    uint32_t region_id;
    nimcp_ph_add_region(&system, "TestRegion", &region_id);
    nimcp_ph_region_t* region = nimcp_ph_get_region(&system, region_id);

    float initial_ph;
    nimcp_ph_get_compartment_ph(region, PH_COMPARTMENT_EXTRACELLULAR, &initial_ph);

    /* High activity should cause acidification */
    nimcp_ph_set_activity(region, 1.0f);
    for (int i = 0; i < 100; i++) {
        nimcp_ph_update(&system, 10.0f);
    }

    float final_ph;
    nimcp_ph_get_compartment_ph(region, PH_COMPARTMENT_EXTRACELLULAR, &final_ph);
    EXPECT_LT(final_ph, initial_ph);
}

//=============================================================================
// Effects API Tests
//=============================================================================

TEST_F(PHDynamicsTest, GetConductanceModifier) {
    uint32_t region_id;
    nimcp_ph_add_region(&system, "TestRegion", &region_id);

    float modifier;
    nimcp_ph_error_t err = nimcp_ph_get_conductance_modifier(&system, region_id, &modifier);
    EXPECT_EQ(err, PH_OK);
    EXPECT_GE(modifier, 0.0f);
    EXPECT_LE(modifier, 1.5f);
}

TEST_F(PHDynamicsTest, GetReleaseModifier) {
    uint32_t region_id;
    nimcp_ph_add_region(&system, "TestRegion", &region_id);

    float modifier;
    nimcp_ph_error_t err = nimcp_ph_get_release_modifier(&system, region_id, &modifier);
    EXPECT_EQ(err, PH_OK);
    EXPECT_GE(modifier, 0.0f);
}

TEST_F(PHDynamicsTest, GetMetabolicModifier) {
    uint32_t region_id;
    nimcp_ph_add_region(&system, "TestRegion", &region_id);

    float modifier;
    nimcp_ph_error_t err = nimcp_ph_get_metabolic_modifier(&system, region_id, &modifier);
    EXPECT_EQ(err, PH_OK);
    EXPECT_GE(modifier, 0.0f);
}

TEST_F(PHDynamicsTest, GetFunctionModifier) {
    uint32_t region_id;
    nimcp_ph_add_region(&system, "TestRegion", &region_id);

    float modifier;
    nimcp_ph_error_t err = nimcp_ph_get_function_modifier(&system, &modifier);
    EXPECT_EQ(err, PH_OK);
    EXPECT_GE(modifier, 0.0f);
}

TEST_F(PHDynamicsTest, AcidosisReducesFunction) {
    uint32_t region_id;
    nimcp_ph_add_region(&system, "TestRegion", &region_id);
    nimcp_ph_region_t* region = nimcp_ph_get_region(&system, region_id);

    float normal_modifier;
    nimcp_ph_get_conductance_modifier(&system, region_id, &normal_modifier);

    /* Apply acidosis */
    nimcp_ph_set_compartment_ph(region, PH_COMPARTMENT_EXTRACELLULAR, 6.8f);
    nimcp_ph_update(&system, 1.0f);

    float acidic_modifier;
    nimcp_ph_get_conductance_modifier(&system, region_id, &acidic_modifier);

    EXPECT_LT(acidic_modifier, normal_modifier);
}

//=============================================================================
// Status and Metrics Tests
//=============================================================================

TEST_F(PHDynamicsTest, GetStatus) {
    uint32_t region_id;
    nimcp_ph_add_region(&system, "TestRegion", &region_id);
    nimcp_ph_region_t* region = nimcp_ph_get_region(&system, region_id);

    nimcp_ph_status_t status = nimcp_ph_get_status(region);
    EXPECT_EQ(status, PH_STATUS_NORMAL);
}

TEST_F(PHDynamicsTest, StatusReflectsAcidosis) {
    uint32_t region_id;
    nimcp_ph_add_region(&system, "TestRegion", &region_id);
    nimcp_ph_region_t* region = nimcp_ph_get_region(&system, region_id);

    /* pH 7.3 with target 7.4 gives deviation of -0.1 (mild acidosis range: -0.15 to -0.05) */
    nimcp_ph_set_compartment_ph(region, PH_COMPARTMENT_EXTRACELLULAR, 7.3f);
    nimcp_ph_update(&system, 1.0f);

    nimcp_ph_status_t status = nimcp_ph_get_status(region);
    EXPECT_EQ(status, PH_STATUS_MILD_ACIDOSIS);
}

TEST_F(PHDynamicsTest, GetMetrics) {
    uint32_t region_id;
    nimcp_ph_add_region(&system, "TestRegion", &region_id);
    nimcp_ph_update(&system, 1.0f);

    nimcp_ph_metrics_t metrics;
    nimcp_ph_error_t err = nimcp_ph_get_metrics(&system, &metrics);
    EXPECT_EQ(err, PH_OK);
    EXPECT_GT(metrics.total_simulation_time, 0.0f);
}

TEST_F(PHDynamicsTest, IsCritical) {
    uint32_t region_id;
    nimcp_ph_add_region(&system, "TestRegion", &region_id);
    nimcp_ph_region_t* region = nimcp_ph_get_region(&system, region_id);

    /* Normal pH should not be critical */
    bool critical = nimcp_ph_is_critical(region, PH_COMPARTMENT_EXTRACELLULAR);
    EXPECT_FALSE(critical);

    /* Severe acidosis should be critical */
    nimcp_ph_set_compartment_ph(region, PH_COMPARTMENT_EXTRACELLULAR, 6.5f);
    critical = nimcp_ph_is_critical(region, PH_COMPARTMENT_EXTRACELLULAR);
    EXPECT_TRUE(critical);
}

//=============================================================================
// Reset Tests
//=============================================================================

TEST_F(PHDynamicsTest, Reset) {
    uint32_t region_id;
    nimcp_ph_add_region(&system, "TestRegion", &region_id);
    nimcp_ph_region_t* region = nimcp_ph_get_region(&system, region_id);

    nimcp_ph_set_compartment_ph(region, PH_COMPARTMENT_EXTRACELLULAR, 6.8f);
    nimcp_ph_update(&system, 100.0f);

    nimcp_ph_error_t err = nimcp_ph_reset(&system);
    EXPECT_EQ(err, PH_OK);
    EXPECT_EQ(system.update_count, 0u);
}

//=============================================================================
// Error String Tests
//=============================================================================

TEST(PHErrorTest, AllErrorsHaveStrings) {
    EXPECT_NE(nimcp_ph_error_string(PH_OK), nullptr);
    EXPECT_NE(nimcp_ph_error_string(PH_ERR_NULL_PTR), nullptr);
    EXPECT_NE(nimcp_ph_error_string(PH_ERR_INVALID_PARAM), nullptr);
    EXPECT_NE(nimcp_ph_error_string(PH_ERR_NOT_INITIALIZED), nullptr);
    EXPECT_NE(nimcp_ph_error_string(PH_ERR_ALREADY_INITIALIZED), nullptr);
    EXPECT_NE(nimcp_ph_error_string(PH_ERR_NO_MEMORY), nullptr);
    EXPECT_NE(nimcp_ph_error_string(PH_ERR_REGION_NOT_FOUND), nullptr);
    EXPECT_NE(nimcp_ph_error_string(PH_ERR_REGION_FULL), nullptr);
    EXPECT_NE(nimcp_ph_error_string(PH_ERR_CRITICAL_ACIDOSIS), nullptr);
    EXPECT_NE(nimcp_ph_error_string(PH_ERR_CRITICAL_ALKALOSIS), nullptr);
    EXPECT_NE(nimcp_ph_error_string(PH_ERR_PUMP_FAILURE), nullptr);
}

TEST(PHErrorTest, UnknownErrorCode) {
    const char* str = nimcp_ph_error_string((nimcp_ph_error_t)-999);
    EXPECT_NE(str, nullptr);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
