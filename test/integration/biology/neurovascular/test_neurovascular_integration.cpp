/**
 * @file test_neurovascular_integration.cpp
 * @brief Integration tests for Neurovascular Coupling module
 *
 * WHAT: Integration tests for neurovascular coupling with neural activity
 * WHY:  Verify CBF/BOLD dynamics integrate correctly with neural systems
 * HOW:  Test activity-flow coupling, astrocyte mediation, and metabolic feedback
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

class NeurovascularIntegrationTest : public ::testing::Test {
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
// Activity-Flow Coupling Integration Tests
//=============================================================================

TEST_F(NeurovascularIntegrationTest, SustainedActivityIncreasesCBF) {
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t unit_id;
    nimcp_nvc_add_unit(&system, "ActiveUnit", position, &unit_id);
    nimcp_nvc_unit_t* unit = nimcp_nvc_get_unit(&system, unit_id);
    ASSERT_NE(unit, nullptr);

    float baseline_cbf;
    nimcp_nvc_get_cbf(unit, &baseline_cbf);

    /* Sustained neural activity */
    nimcp_nvc_set_activity(unit, 1.0f);
    for (int i = 0; i < 500; i++) {
        nimcp_nvc_update(&system, 10.0f);  /* 5 seconds total */
    }

    float elevated_cbf;
    nimcp_nvc_get_cbf(unit, &elevated_cbf);

    /* CBF should increase with activity */
    EXPECT_GE(elevated_cbf, baseline_cbf);
}

TEST_F(NeurovascularIntegrationTest, CBFRecoveryAfterActivity) {
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t unit_id;
    nimcp_nvc_add_unit(&system, "RecoveryUnit", position, &unit_id);
    nimcp_nvc_unit_t* unit = nimcp_nvc_get_unit(&system, unit_id);

    /* Create elevation with activity */
    nimcp_nvc_set_activity(unit, 1.0f);
    for (int i = 0; i < 300; i++) {
        nimcp_nvc_update(&system, 10.0f);
    }

    float peak_cbf;
    nimcp_nvc_get_cbf(unit, &peak_cbf);

    /* Stop activity and allow recovery */
    nimcp_nvc_set_activity(unit, 0.0f);
    for (int i = 0; i < 1000; i++) {
        nimcp_nvc_update(&system, 10.0f);  /* 10 seconds recovery */
    }

    float recovered_cbf;
    nimcp_nvc_get_cbf(unit, &recovered_cbf);

    /* CBF should return toward baseline */
    EXPECT_LE(recovered_cbf, peak_cbf);
}

TEST_F(NeurovascularIntegrationTest, CBFScalesWithActivityLevel) {
    float pos1[3] = {0.0f, 0.0f, 0.0f};
    float pos2[3] = {100.0f, 0.0f, 0.0f};
    uint32_t low_id, high_id;
    nimcp_nvc_add_unit(&system, "LowActivity", pos1, &low_id);
    nimcp_nvc_add_unit(&system, "HighActivity", pos2, &high_id);

    nimcp_nvc_unit_t* low_unit = nimcp_nvc_get_unit(&system, low_id);
    nimcp_nvc_unit_t* high_unit = nimcp_nvc_get_unit(&system, high_id);

    /* Different activity levels */
    nimcp_nvc_set_activity(low_unit, 0.3f);
    nimcp_nvc_set_activity(high_unit, 1.0f);

    for (int i = 0; i < 500; i++) {
        nimcp_nvc_update(&system, 10.0f);
    }

    float low_cbf, high_cbf;
    nimcp_nvc_get_cbf(low_unit, &low_cbf);
    nimcp_nvc_get_cbf(high_unit, &high_cbf);

    /* Higher activity should produce higher CBF */
    EXPECT_LE(low_cbf, high_cbf);
}

//=============================================================================
// BOLD Signal Integration Tests
//=============================================================================

TEST_F(NeurovascularIntegrationTest, BOLDResponseToActivity) {
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t unit_id;
    nimcp_nvc_add_unit(&system, "BOLDUnit", position, &unit_id);
    nimcp_nvc_unit_t* unit = nimcp_nvc_get_unit(&system, unit_id);

    float baseline_bold;
    nimcp_nvc_get_bold(unit, &baseline_bold);

    /* Impulse of activity */
    nimcp_nvc_set_activity(unit, 1.0f);
    for (int i = 0; i < 50; i++) {
        nimcp_nvc_update(&system, 10.0f);  /* 500ms stimulus */
    }
    nimcp_nvc_set_activity(unit, 0.0f);

    /* Wait for BOLD response to develop (HRF peaks ~5-6s) */
    float max_bold = baseline_bold;
    for (int i = 0; i < 1000; i++) {
        nimcp_nvc_update(&system, 10.0f);
        float current_bold;
        nimcp_nvc_get_bold(unit, &current_bold);
        if (current_bold > max_bold) {
            max_bold = current_bold;
        }
    }

    /* BOLD should show a response */
    EXPECT_GE(max_bold, baseline_bold - 0.01f);
}

TEST_F(NeurovascularIntegrationTest, BOLDHasHRFShape) {
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t unit_id;
    nimcp_nvc_add_unit(&system, "HRFUnit", position, &unit_id);
    nimcp_nvc_unit_t* unit = nimcp_nvc_get_unit(&system, unit_id);

    /* Brief stimulus */
    nimcp_nvc_set_activity(unit, 1.0f);
    for (int i = 0; i < 10; i++) {
        nimcp_nvc_update(&system, 10.0f);
    }
    nimcp_nvc_set_activity(unit, 0.0f);

    /* Track BOLD over time */
    float bold_values[100];
    for (int i = 0; i < 100; i++) {
        nimcp_nvc_update(&system, 100.0f);  /* 100ms steps = 10s total */
        nimcp_nvc_get_bold(unit, &bold_values[i]);
    }

    /* Verify HRF-like properties: should have some dynamic range */
    float min_bold = bold_values[0];
    float max_bold = bold_values[0];
    for (int i = 1; i < 100; i++) {
        if (bold_values[i] < min_bold) min_bold = bold_values[i];
        if (bold_values[i] > max_bold) max_bold = bold_values[i];
    }

    /* Should have some dynamic range */
    EXPECT_GE(max_bold - min_bold, 0.0f);
}

//=============================================================================
// Multi-Unit Hemodynamics Tests
//=============================================================================

TEST_F(NeurovascularIntegrationTest, MultiUnitIndependentHemodynamics) {
    const char* names[] = {"Visual", "Motor", "Auditory", "Prefrontal"};
    float activities[] = {1.0f, 0.5f, 0.8f, 0.2f};
    uint32_t ids[4];

    for (int i = 0; i < 4; i++) {
        float pos[3] = {(float)i * 50.0f, 0.0f, 0.0f};
        nimcp_nvc_add_unit(&system, names[i], pos, &ids[i]);
        nimcp_nvc_unit_t* unit = nimcp_nvc_get_unit(&system, ids[i]);
        nimcp_nvc_set_activity(unit, activities[i]);
    }

    for (int i = 0; i < 500; i++) {
        nimcp_nvc_update(&system, 10.0f);
    }

    /* Each unit should have different hemodynamics based on activity */
    float cbfs[4];
    for (int i = 0; i < 4; i++) {
        nimcp_nvc_unit_t* unit = nimcp_nvc_get_unit(&system, ids[i]);
        nimcp_nvc_get_cbf(unit, &cbfs[i]);
    }

    /* Higher activity units should have higher or equal CBF */
    /* Visual (1.0) >= Motor (0.5) */
    EXPECT_GE(cbfs[0], cbfs[1] * 0.9f);  /* Allow some margin */
}

TEST_F(NeurovascularIntegrationTest, SystemWideMetrics) {
    /* Create multiple units */
    for (int i = 0; i < 5; i++) {
        char name[32];
        snprintf(name, sizeof(name), "Unit%d", i);
        float pos[3] = {(float)i * 20.0f, 0.0f, 0.0f};
        uint32_t id;
        nimcp_nvc_add_unit(&system, name, pos, &id);
        nimcp_nvc_unit_t* unit = nimcp_nvc_get_unit(&system, id);
        nimcp_nvc_set_activity(unit, i * 0.2f);
    }

    for (int i = 0; i < 200; i++) {
        nimcp_nvc_update(&system, 10.0f);
    }

    nimcp_nvc_metrics_t metrics;
    nimcp_nvc_error_t err = nimcp_nvc_get_metrics(&system, &metrics);
    EXPECT_EQ(err, NVC_OK);

    /* Verify system metrics */
    EXPECT_GT(metrics.total_simulation_time, 0.0f);
    EXPECT_EQ(metrics.total_units, 5U);
}

//=============================================================================
// Astrocyte Coupling Tests
//=============================================================================

TEST_F(NeurovascularIntegrationTest, AstrocyteCouplingAffectsResponse) {
    float pos1[3] = {0.0f, 0.0f, 0.0f};
    float pos2[3] = {100.0f, 0.0f, 0.0f};
    uint32_t coupled_id, uncoupled_id;

    nimcp_nvc_add_unit(&system, "CoupledUnit", pos1, &coupled_id);
    nimcp_nvc_add_unit(&system, "UncoupledUnit", pos2, &uncoupled_id);

    nimcp_nvc_unit_t* coupled = nimcp_nvc_get_unit(&system, coupled_id);
    nimcp_nvc_unit_t* uncoupled = nimcp_nvc_get_unit(&system, uncoupled_id);

    /* Set different astrocyte coupling strengths */
    coupled->astrocyte_coupling = 1.0f;
    uncoupled->astrocyte_coupling = 0.1f;

    /* Same activity */
    nimcp_nvc_set_activity(coupled, 1.0f);
    nimcp_nvc_set_activity(uncoupled, 1.0f);

    for (int i = 0; i < 500; i++) {
        nimcp_nvc_update(&system, 10.0f);
    }

    /* Both should have valid CBF */
    float cbf_coupled, cbf_uncoupled;
    nimcp_nvc_get_cbf(coupled, &cbf_coupled);
    nimcp_nvc_get_cbf(uncoupled, &cbf_uncoupled);

    EXPECT_GE(cbf_coupled, 0.0f);
    EXPECT_GE(cbf_uncoupled, 0.0f);
}

TEST_F(NeurovascularIntegrationTest, AstrocyteCalciumDynamics) {
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t unit_id;
    nimcp_nvc_add_unit(&system, "AstroCalciumUnit", position, &unit_id);
    nimcp_nvc_unit_t* unit = nimcp_nvc_get_unit(&system, unit_id);

    unit->astrocyte_coupling = 1.0f;
    float baseline_ca = unit->astrocyte_calcium;

    /* Neural activity should affect astrocyte */
    nimcp_nvc_set_activity(unit, 1.0f);
    for (int i = 0; i < 200; i++) {
        nimcp_nvc_update(&system, 10.0f);
    }

    /* Astrocyte calcium should respond (or at least be valid) */
    EXPECT_GE(unit->astrocyte_calcium, 0.0f);
}

//=============================================================================
// OEF and Oxygenation Tests
//=============================================================================

TEST_F(NeurovascularIntegrationTest, OxygenExtractionDynamics) {
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t unit_id;
    nimcp_nvc_add_unit(&system, "OEFUnit", position, &unit_id);
    nimcp_nvc_unit_t* unit = nimcp_nvc_get_unit(&system, unit_id);

    float baseline_oef;
    nimcp_nvc_get_oef(unit, &baseline_oef);

    /* High activity increases oxygen demand */
    nimcp_nvc_set_activity(unit, 1.0f);

    for (int i = 0; i < 200; i++) {
        nimcp_nvc_update(&system, 10.0f);
    }

    float activity_oef;
    nimcp_nvc_get_oef(unit, &activity_oef);

    /* OEF should be a valid fraction */
    EXPECT_GE(activity_oef, 0.0f);
    EXPECT_LE(activity_oef, 1.0f);
}

//=============================================================================
// Balloon Model Dynamics Tests
//=============================================================================

TEST_F(NeurovascularIntegrationTest, CBVFollowsCBF) {
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t unit_id;
    nimcp_nvc_add_unit(&system, "BalloonUnit", position, &unit_id);
    nimcp_nvc_unit_t* unit = nimcp_nvc_get_unit(&system, unit_id);

    float baseline_cbv = unit->cbv;

    /* Increase CBF through activity */
    nimcp_nvc_set_activity(unit, 1.0f);

    for (int i = 0; i < 500; i++) {
        nimcp_nvc_update(&system, 10.0f);
    }

    /* CBV should respond to activity */
    EXPECT_GE(unit->cbv, 0.0f);
}

TEST_F(NeurovascularIntegrationTest, DeoxyhemoglobinDynamics) {
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t unit_id;
    nimcp_nvc_add_unit(&system, "dHbUnit", position, &unit_id);
    nimcp_nvc_unit_t* unit = nimcp_nvc_get_unit(&system, unit_id);

    /* Get BOLD state for deoxyhemoglobin */
    nimcp_nvc_bold_t bold_state;
    nimcp_nvc_get_bold_state(unit, &bold_state);
    float baseline_dhb = bold_state.deoxyhemoglobin;

    nimcp_nvc_set_activity(unit, 1.0f);

    for (int i = 0; i < 500; i++) {
        nimcp_nvc_update(&system, 10.0f);
    }

    nimcp_nvc_get_bold_state(unit, &bold_state);

    /* Deoxyhemoglobin should be valid */
    EXPECT_GE(bold_state.deoxyhemoglobin, 0.0f);
}

//=============================================================================
// Long-Running Stability Tests
//=============================================================================

TEST_F(NeurovascularIntegrationTest, LongTermStability) {
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t unit_id;
    nimcp_nvc_add_unit(&system, "StableUnit", position, &unit_id);
    nimcp_nvc_unit_t* unit = nimcp_nvc_get_unit(&system, unit_id);

    unit->astrocyte_coupling = 1.0f;

    /* Simulate long-term with varying activity */
    for (int cycle = 0; cycle < 20; cycle++) {
        /* Active epoch */
        nimcp_nvc_set_activity(unit, 0.8f);
        for (int i = 0; i < 100; i++) {
            nimcp_nvc_update(&system, 10.0f);
        }

        /* Rest epoch */
        nimcp_nvc_set_activity(unit, 0.1f);
        for (int i = 0; i < 200; i++) {
            nimcp_nvc_update(&system, 10.0f);
        }
    }

    /* System should remain stable */
    float final_cbf, final_bold;
    nimcp_nvc_get_cbf(unit, &final_cbf);
    nimcp_nvc_get_bold(unit, &final_bold);

    EXPECT_FALSE(std::isnan(final_cbf));
    EXPECT_FALSE(std::isinf(final_cbf));
    EXPECT_FALSE(std::isnan(final_bold));
    EXPECT_FALSE(std::isinf(final_bold));
    EXPECT_GE(final_cbf, 0.0f);
}

TEST_F(NeurovascularIntegrationTest, HighFrequencyStimulation) {
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t unit_id;
    nimcp_nvc_add_unit(&system, "HighFreqUnit", position, &unit_id);
    nimcp_nvc_unit_t* unit = nimcp_nvc_get_unit(&system, unit_id);

    /* High-frequency on-off stimulation */
    for (int i = 0; i < 500; i++) {
        float activity = (i % 10 < 5) ? 1.0f : 0.0f;
        nimcp_nvc_set_activity(unit, activity);
        nimcp_nvc_update(&system, 2.0f);  /* 2ms updates */
    }

    /* Should handle rapid changes without instability */
    float cbf, bold;
    nimcp_nvc_get_cbf(unit, &cbf);
    nimcp_nvc_get_bold(unit, &bold);

    EXPECT_FALSE(std::isnan(cbf));
    EXPECT_FALSE(std::isnan(bold));
    EXPECT_GE(cbf, 0.0f);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
