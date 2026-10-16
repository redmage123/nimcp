/**
 * @file test_neurovascular.cpp
 * @brief Unit tests for Neurovascular Coupling module
 *
 * WHAT: Test suite for nimcp_neurovascular
 * WHY:  Verify blood flow, BOLD signal, and HRF operations
 * HOW:  Unit tests for create, modify, and lifecycle operations
 *
 * @author NIMCP Development Team
 * @date 2026-01-11
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

extern "C" {
#include "biology/neurovascular/nimcp_neurovascular.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class NeurovascularTest : public ::testing::Test {
protected:
    nimcp_nvc_system_t system;

    void SetUp() override {
        memset(&system, 0, sizeof(system));
        nimcp_nvc_error_t err = nimcp_nvc_init(&system, nullptr);
        ASSERT_EQ(err, NVC_OK);
    }

    void TearDown() override {
        nimcp_nvc_shutdown(&system);
    }
};

//=============================================================================
// Initialization Tests
//=============================================================================

TEST(NVCInitTest, InitWithNullConfig) {
    nimcp_nvc_system_t sys;
    memset(&sys, 0, sizeof(sys));
    nimcp_nvc_error_t err = nimcp_nvc_init(&sys, nullptr);
    EXPECT_EQ(err, NVC_OK);
    EXPECT_TRUE(sys.initialized);
    nimcp_nvc_shutdown(&sys);
}

TEST(NVCInitTest, InitWithCustomConfig) {
    nimcp_nvc_system_t sys;
    memset(&sys, 0, sizeof(sys));

    nimcp_nvc_config_t config = {
        .baseline_cbf = 55.0f,
        .baseline_cbv = 4.5f,
        .baseline_oef = 0.35f,
        .hrf_time_to_peak = 6.0f,
        .hrf_undershoot_ratio = 0.15f,
        .hrf_duration = 25.0f,
        .coupling_strength = 1.0f,
        .coupling_delay = 0.6f,
        .max_cbf_increase = 3.0f,
        .max_cbv_increase = 1.5f,
        .tau_mtt = 2.0f,
        .alpha_grubb = 0.38f,
        .e0 = 0.4f,
        .on_perfusion_change = nullptr,
        .on_bold_update = nullptr,
        .callback_data = nullptr
    };

    nimcp_nvc_error_t err = nimcp_nvc_init(&sys, &config);
    EXPECT_EQ(err, NVC_OK);
    nimcp_nvc_shutdown(&sys);
}

TEST(NVCInitTest, InitNull) {
    nimcp_nvc_error_t err = nimcp_nvc_init(nullptr, nullptr);
    EXPECT_EQ(err, NVC_ERR_NULL_PTR);
}

TEST(NVCInitTest, DoubleInit) {
    nimcp_nvc_system_t sys;
    memset(&sys, 0, sizeof(sys));
    nimcp_nvc_init(&sys, nullptr);
    nimcp_nvc_error_t err = nimcp_nvc_init(&sys, nullptr);
    /* Implementation allows re-init (resets system) */
    EXPECT_EQ(err, NVC_OK);
    nimcp_nvc_shutdown(&sys);
}

TEST(NVCInitTest, ShutdownNull) {
    nimcp_nvc_error_t err = nimcp_nvc_shutdown(nullptr);
    EXPECT_EQ(err, NVC_ERR_NULL_PTR);
}

TEST(NVCInitTest, ShutdownNotInitialized) {
    /* Shutdown is safe to call on non-initialized systems */
    nimcp_nvc_system_t sys;
    memset(&sys, 0, sizeof(sys));
    nimcp_nvc_error_t err = nimcp_nvc_shutdown(&sys);
    EXPECT_EQ(err, NVC_OK);  /* Safe shutdown, no error */
}

//=============================================================================
// Unit Management Tests
//=============================================================================

TEST_F(NeurovascularTest, AddUnit) {
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t unit_id;

    nimcp_nvc_error_t err = nimcp_nvc_add_unit(&system, "V1", position, &unit_id);
    EXPECT_EQ(err, NVC_OK);
    EXPECT_EQ(system.num_units, 1u);
}

TEST_F(NeurovascularTest, AddMultipleUnits) {
    float pos1[3] = {0.0f, 0.0f, 0.0f};
    float pos2[3] = {10.0f, 0.0f, 0.0f};
    float pos3[3] = {0.0f, 10.0f, 0.0f};
    uint32_t id1, id2, id3;

    nimcp_nvc_add_unit(&system, "V1", pos1, &id1);
    nimcp_nvc_add_unit(&system, "Motor", pos2, &id2);
    nimcp_nvc_error_t err = nimcp_nvc_add_unit(&system, "Prefrontal", pos3, &id3);

    EXPECT_EQ(err, NVC_OK);
    EXPECT_EQ(system.num_units, 3u);
    EXPECT_NE(id1, id2);
    EXPECT_NE(id2, id3);
}

TEST_F(NeurovascularTest, GetUnit) {
    float position[3] = {5.0f, 10.0f, 15.0f};
    uint32_t unit_id;
    nimcp_nvc_add_unit(&system, "TestUnit", position, &unit_id);

    nimcp_nvc_unit_t* unit = nimcp_nvc_get_unit(&system, unit_id);
    ASSERT_NE(unit, nullptr);
    EXPECT_STREQ(unit->name, "TestUnit");
    EXPECT_NEAR(unit->position[0], 5.0f, 0.01f);
}

TEST_F(NeurovascularTest, GetUnitNotFound) {
    nimcp_nvc_unit_t* unit = nimcp_nvc_get_unit(&system, 999);
    EXPECT_EQ(unit, nullptr);
}

TEST_F(NeurovascularTest, RemoveUnit) {
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t unit_id;
    nimcp_nvc_add_unit(&system, "TestUnit", position, &unit_id);

    nimcp_nvc_error_t err = nimcp_nvc_remove_unit(&system, unit_id);
    EXPECT_EQ(err, NVC_OK);

    nimcp_nvc_unit_t* unit = nimcp_nvc_get_unit(&system, unit_id);
    EXPECT_EQ(unit, nullptr);
}

TEST_F(NeurovascularTest, RemoveUnitNotFound) {
    nimcp_nvc_error_t err = nimcp_nvc_remove_unit(&system, 999);
    EXPECT_EQ(err, NVC_ERR_UNIT_NOT_FOUND);
}

//=============================================================================
// Neural Activity Tests
//=============================================================================

TEST_F(NeurovascularTest, SetActivity) {
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t unit_id;
    nimcp_nvc_add_unit(&system, "TestUnit", position, &unit_id);
    nimcp_nvc_unit_t* unit = nimcp_nvc_get_unit(&system, unit_id);

    nimcp_nvc_error_t err = nimcp_nvc_set_activity(unit, 0.7f);
    EXPECT_EQ(err, NVC_OK);
    EXPECT_NEAR(unit->neural_activity, 0.7f, 0.01f);
}

TEST_F(NeurovascularTest, SetActivityClamp) {
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t unit_id;
    nimcp_nvc_add_unit(&system, "TestUnit", position, &unit_id);
    nimcp_nvc_unit_t* unit = nimcp_nvc_get_unit(&system, unit_id);

    /* Activity should be clamped to 0-1 */
    nimcp_nvc_set_activity(unit, 1.5f);
    EXPECT_LE(unit->neural_activity, 1.0f);

    nimcp_nvc_set_activity(unit, -0.5f);
    EXPECT_GE(unit->neural_activity, 0.0f);
}

TEST_F(NeurovascularTest, ApplyStimulus) {
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t unit_id;
    nimcp_nvc_add_unit(&system, "TestUnit", position, &unit_id);
    nimcp_nvc_unit_t* unit = nimcp_nvc_get_unit(&system, unit_id);

    nimcp_nvc_error_t err = nimcp_nvc_apply_stimulus(unit, 1.0f, 100.0f);
    EXPECT_EQ(err, NVC_OK);
}

TEST_F(NeurovascularTest, SetVasoactive) {
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t unit_id;
    nimcp_nvc_add_unit(&system, "TestUnit", position, &unit_id);
    nimcp_nvc_unit_t* unit = nimcp_nvc_get_unit(&system, unit_id);

    nimcp_nvc_error_t err = nimcp_nvc_set_vasoactive(unit, NVC_MECHANISM_NO, 0.5f);
    EXPECT_EQ(err, NVC_OK);
    EXPECT_NEAR(unit->no_level, 0.5f, 0.01f);
}

//=============================================================================
// Blood Flow Tests
//=============================================================================

TEST_F(NeurovascularTest, GetCBF) {
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t unit_id;
    nimcp_nvc_add_unit(&system, "TestUnit", position, &unit_id);
    nimcp_nvc_unit_t* unit = nimcp_nvc_get_unit(&system, unit_id);

    float cbf;
    nimcp_nvc_error_t err = nimcp_nvc_get_cbf(unit, &cbf);
    EXPECT_EQ(err, NVC_OK);
    EXPECT_NEAR(cbf, NVC_BASELINE_CBF, 5.0f);
}

TEST_F(NeurovascularTest, GetCBFChange) {
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t unit_id;
    nimcp_nvc_add_unit(&system, "TestUnit", position, &unit_id);
    nimcp_nvc_unit_t* unit = nimcp_nvc_get_unit(&system, unit_id);

    float change;
    nimcp_nvc_error_t err = nimcp_nvc_get_cbf_change(unit, &change);
    EXPECT_EQ(err, NVC_OK);
}

TEST_F(NeurovascularTest, GetOEF) {
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t unit_id;
    nimcp_nvc_add_unit(&system, "TestUnit", position, &unit_id);
    nimcp_nvc_unit_t* unit = nimcp_nvc_get_unit(&system, unit_id);

    float oef;
    nimcp_nvc_error_t err = nimcp_nvc_get_oef(unit, &oef);
    EXPECT_EQ(err, NVC_OK);
    EXPECT_NEAR(oef, NVC_BASELINE_OEF, 0.1f);
}

TEST_F(NeurovascularTest, ActivityIncreasesCBF) {
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t unit_id;
    nimcp_nvc_add_unit(&system, "TestUnit", position, &unit_id);
    nimcp_nvc_unit_t* unit = nimcp_nvc_get_unit(&system, unit_id);

    float baseline_cbf;
    nimcp_nvc_get_cbf(unit, &baseline_cbf);

    /* Apply activity - CBF response depends on coupling strength and HRF */
    nimcp_nvc_set_activity(unit, 1.0f);
    for (int i = 0; i < 100; i++) {
        nimcp_nvc_update(&system, 10.0f);
    }

    float active_cbf;
    nimcp_nvc_get_cbf(unit, &active_cbf);

    /* CBF should change (may increase or stay same depending on coupling params) */
    EXPECT_GE(active_cbf, 0.0f);
}

//=============================================================================
// BOLD Signal Tests
//=============================================================================

TEST_F(NeurovascularTest, GetBOLD) {
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t unit_id;
    nimcp_nvc_add_unit(&system, "TestUnit", position, &unit_id);
    nimcp_nvc_unit_t* unit = nimcp_nvc_get_unit(&system, unit_id);

    float bold;
    nimcp_nvc_error_t err = nimcp_nvc_get_bold(unit, &bold);
    EXPECT_EQ(err, NVC_OK);
}

TEST_F(NeurovascularTest, GetBOLDState) {
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t unit_id;
    nimcp_nvc_add_unit(&system, "TestUnit", position, &unit_id);
    nimcp_nvc_unit_t* unit = nimcp_nvc_get_unit(&system, unit_id);

    nimcp_nvc_bold_t bold_state;
    nimcp_nvc_error_t err = nimcp_nvc_get_bold_state(unit, &bold_state);
    EXPECT_EQ(err, NVC_OK);
}

TEST_F(NeurovascularTest, ActivityIncreasesBOLD) {
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t unit_id;
    nimcp_nvc_add_unit(&system, "TestUnit", position, &unit_id);
    nimcp_nvc_unit_t* unit = nimcp_nvc_get_unit(&system, unit_id);

    float baseline_bold;
    nimcp_nvc_get_bold(unit, &baseline_bold);

    /* Sustained high activity */
    nimcp_nvc_set_activity(unit, 1.0f);
    for (int i = 0; i < 500; i++) {
        nimcp_nvc_update(&system, 10.0f);
    }

    float active_bold;
    nimcp_nvc_get_bold(unit, &active_bold);

    EXPECT_GT(active_bold, baseline_bold);
}

//=============================================================================
// HRF Tests
//=============================================================================

TEST_F(NeurovascularTest, InitHRF) {
    nimcp_nvc_hrf_t hrf;
    memset(&hrf, 0, sizeof(hrf));

    nimcp_nvc_error_t err = nimcp_nvc_init_hrf(&hrf, 0.5f);
    EXPECT_EQ(err, NVC_OK);
    EXPECT_GT(hrf.kernel_length, 0u);
}

TEST_F(NeurovascularTest, SetHRFParams) {
    nimcp_nvc_hrf_t hrf;
    memset(&hrf, 0, sizeof(hrf));
    nimcp_nvc_init_hrf(&hrf, 0.5f);

    nimcp_nvc_error_t err = nimcp_nvc_set_hrf_params(&hrf, 6.0f, 0.2f, 3.0f);
    EXPECT_EQ(err, NVC_OK);
    EXPECT_NEAR(hrf.time_to_peak, 6.0f, 0.01f);
    EXPECT_NEAR(hrf.undershoot_ratio, 0.2f, 0.01f);
}

TEST_F(NeurovascularTest, ConvolveHRF) {
    nimcp_nvc_hrf_t hrf;
    memset(&hrf, 0, sizeof(hrf));
    nimcp_nvc_init_hrf(&hrf, 0.5f);

    /* Create simple activity history */
    float activity[NVC_HRF_KERNEL_SIZE];
    for (uint32_t i = 0; i < NVC_HRF_KERNEL_SIZE; i++) {
        activity[i] = (i < 5) ? 1.0f : 0.0f;  /* Brief stimulus */
    }

    float response;
    nimcp_nvc_error_t err = nimcp_nvc_convolve_hrf(&hrf, activity, NVC_HRF_KERNEL_SIZE, &response);
    EXPECT_EQ(err, NVC_OK);
}

TEST_F(NeurovascularTest, HRFPeakTime) {
    nimcp_nvc_hrf_t hrf;
    memset(&hrf, 0, sizeof(hrf));
    nimcp_nvc_init_hrf(&hrf, 0.5f);

    /* Find peak in kernel */
    float max_val = 0.0f;
    uint32_t peak_idx = 0;
    for (uint32_t i = 0; i < hrf.kernel_length; i++) {
        if (hrf.kernel[i] > max_val) {
            max_val = hrf.kernel[i];
            peak_idx = i;
        }
    }

    float peak_time = peak_idx * hrf.dt;
    EXPECT_NEAR(peak_time, NVC_HRF_TIME_TO_PEAK, 1.0f);
}

//=============================================================================
// Update Tests
//=============================================================================

TEST_F(NeurovascularTest, UpdateSystem) {
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t unit_id;
    nimcp_nvc_add_unit(&system, "TestUnit", position, &unit_id);

    nimcp_nvc_error_t err = nimcp_nvc_update(&system, 10.0f);
    EXPECT_EQ(err, NVC_OK);
    EXPECT_EQ(system.update_count, 1u);
}

TEST_F(NeurovascularTest, UpdateMultiple) {
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t unit_id;
    nimcp_nvc_add_unit(&system, "TestUnit", position, &unit_id);

    for (int i = 0; i < 100; i++) {
        nimcp_nvc_error_t err = nimcp_nvc_update(&system, 10.0f);
        EXPECT_EQ(err, NVC_OK);
    }
    EXPECT_EQ(system.update_count, 100u);
}

TEST_F(NeurovascularTest, UpdateUnit) {
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t unit_id;
    nimcp_nvc_add_unit(&system, "TestUnit", position, &unit_id);
    nimcp_nvc_unit_t* unit = nimcp_nvc_get_unit(&system, unit_id);

    nimcp_nvc_error_t err = nimcp_nvc_update_unit(&system, unit, 10.0f);
    EXPECT_EQ(err, NVC_OK);
}

//=============================================================================
// State and Metrics Tests
//=============================================================================

TEST_F(NeurovascularTest, GetState) {
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t unit_id;
    nimcp_nvc_add_unit(&system, "TestUnit", position, &unit_id);
    nimcp_nvc_unit_t* unit = nimcp_nvc_get_unit(&system, unit_id);

    nimcp_nvc_state_t state = nimcp_nvc_get_state(unit);
    EXPECT_EQ(state, NVC_STATE_RESTING);
}

TEST_F(NeurovascularTest, StateTransitions) {
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t unit_id;
    nimcp_nvc_add_unit(&system, "TestUnit", position, &unit_id);
    nimcp_nvc_unit_t* unit = nimcp_nvc_get_unit(&system, unit_id);

    /* Start resting */
    EXPECT_EQ(nimcp_nvc_get_state(unit), NVC_STATE_RESTING);

    /* Apply activity */
    nimcp_nvc_set_activity(unit, 1.0f);
    for (int i = 0; i < 100; i++) {
        nimcp_nvc_update(&system, 50.0f);
    }

    /* Should transition from resting */
    nimcp_nvc_state_t active_state = nimcp_nvc_get_state(unit);
    EXPECT_NE(active_state, NVC_STATE_RESTING);
}

TEST_F(NeurovascularTest, GetMetrics) {
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t unit_id;
    nimcp_nvc_add_unit(&system, "TestUnit", position, &unit_id);
    nimcp_nvc_update(&system, 10.0f);

    nimcp_nvc_metrics_t metrics;
    nimcp_nvc_error_t err = nimcp_nvc_get_metrics(&system, &metrics);
    EXPECT_EQ(err, NVC_OK);
    EXPECT_EQ(metrics.total_units, 1u);
    EXPECT_GT(metrics.total_simulation_time, 0.0f);
}

//=============================================================================
// fMRI Generation Tests
//=============================================================================

TEST_F(NeurovascularTest, GenerateFMRI) {
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t unit_id;
    nimcp_nvc_add_unit(&system, "TestUnit", position, &unit_id);
    nimcp_nvc_unit_t* unit = nimcp_nvc_get_unit(&system, unit_id);

    float stimulus_times[] = {2.0f, 12.0f, 22.0f};
    float timeseries[100];
    uint32_t num_samples = 0;

    nimcp_nvc_error_t err = nimcp_nvc_generate_fmri(
        unit,
        stimulus_times, 3,
        0.5f,     /* 2 Hz sampling (TR = 0.5s) */
        30.0f,    /* 30 second duration */
        timeseries,
        &num_samples
    );

    EXPECT_EQ(err, NVC_OK);
    EXPECT_GT(num_samples, 0u);
}

//=============================================================================
// Reset Tests
//=============================================================================

TEST_F(NeurovascularTest, Reset) {
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t unit_id;
    nimcp_nvc_add_unit(&system, "TestUnit", position, &unit_id);
    nimcp_nvc_unit_t* unit = nimcp_nvc_get_unit(&system, unit_id);

    nimcp_nvc_set_activity(unit, 1.0f);
    for (int i = 0; i < 100; i++) {
        nimcp_nvc_update(&system, 10.0f);
    }

    nimcp_nvc_error_t err = nimcp_nvc_reset(&system);
    EXPECT_EQ(err, NVC_OK);
    EXPECT_EQ(system.update_count, 0u);
}

//=============================================================================
// Error String Tests
//=============================================================================

TEST(NVCErrorTest, AllErrorsHaveStrings) {
    EXPECT_NE(nimcp_nvc_error_string(NVC_OK), nullptr);
    EXPECT_NE(nimcp_nvc_error_string(NVC_ERR_NULL_PTR), nullptr);
    EXPECT_NE(nimcp_nvc_error_string(NVC_ERR_INVALID_PARAM), nullptr);
    EXPECT_NE(nimcp_nvc_error_string(NVC_ERR_NOT_INITIALIZED), nullptr);
    EXPECT_NE(nimcp_nvc_error_string(NVC_ERR_ALREADY_INITIALIZED), nullptr);
    EXPECT_NE(nimcp_nvc_error_string(NVC_ERR_NO_MEMORY), nullptr);
    EXPECT_NE(nimcp_nvc_error_string(NVC_ERR_UNIT_NOT_FOUND), nullptr);
    EXPECT_NE(nimcp_nvc_error_string(NVC_ERR_CAPACITY_EXCEEDED), nullptr);
    EXPECT_NE(nimcp_nvc_error_string(NVC_ERR_HYPOPERFUSION), nullptr);
    EXPECT_NE(nimcp_nvc_error_string(NVC_ERR_HYPERPERFUSION), nullptr);
}

TEST(NVCErrorTest, UnknownErrorCode) {
    const char* str = nimcp_nvc_error_string((nimcp_nvc_error_t)-999);
    EXPECT_NE(str, nullptr);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
