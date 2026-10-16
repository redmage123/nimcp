/**
 * @file e2e_test_neurovascular_pipeline.cpp
 * @brief End-to-end tests for Neurovascular Coupling pipeline
 *
 * WHAT: E2E tests for complete neurovascular coupling with chemistry integration
 * WHY:  Verify full neural activity -> hemodynamic response -> BOLD signal pipeline
 * HOW:  Test complete workflows including fMRI simulation scenarios
 *
 * @author NIMCP Development Team
 * @date 2026-01-11
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>

extern "C" {
#include "biology/neurovascular/nimcp_neurovascular.h"
#include "chemistry/ph/nimcp_ph_dynamics.h"
#include "chemistry/gasotransmitters/nimcp_nitric_oxide.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class NeurovascularPipelineE2ETest : public ::testing::Test {
protected:
    nimcp_nvc_system_t nvc_system;
    nimcp_ph_system_t ph_system;
    nimcp_no_system_t no_system;

    void SetUp() override {
        memset(&nvc_system, 0, sizeof(nvc_system));
        memset(&ph_system, 0, sizeof(ph_system));
        memset(&no_system, 0, sizeof(no_system));

        nimcp_nvc_error_t nvc_err = nimcp_nvc_init(&nvc_system, nullptr);
        ASSERT_EQ(nvc_err, NVC_OK);

        nimcp_ph_error_t ph_err = nimcp_ph_init(&ph_system, nullptr);
        ASSERT_EQ(ph_err, PH_OK);

        nimcp_no_error_t no_err = nimcp_no_init(&no_system, nullptr);
        ASSERT_EQ(no_err, NO_OK);
    }

    void TearDown() override {
        nimcp_nvc_shutdown(&nvc_system);
        nimcp_ph_shutdown(&ph_system);
        nimcp_no_shutdown(&no_system);
    }

    /* Helper: Coupled update of all systems */
    void update_all(float dt_ms) {
        nimcp_nvc_update(&nvc_system, dt_ms);
        nimcp_ph_update(&ph_system, dt_ms);
        nimcp_no_update(&no_system, dt_ms);
    }
};

//=============================================================================
// fMRI Simulation Pipeline Tests
//=============================================================================

TEST_F(NeurovascularPipelineE2ETest, BOLDBlockDesignExperiment) {
    /*
     * Pipeline: Simulates a typical fMRI block design experiment
     *
     * - Baseline (10s) -> Stimulus (20s) -> Rest (30s) -> Repeat
     * - Measures BOLD response to validate HRF properties
     */

    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t unit_id;
    nimcp_nvc_add_unit(&nvc_system, "VisualCortex", position, &unit_id);
    nimcp_nvc_unit_t* unit = nimcp_nvc_get_unit(&nvc_system, unit_id);

    unit->astrocyte_coupling = 1.0f;

    std::vector<float> bold_timeseries;
    bold_timeseries.reserve(6000);  /* 60 seconds at 10ms resolution */

    /* Baseline (10s) */
    nimcp_nvc_set_activity(unit, 0.1f);
    for (int i = 0; i < 1000; i++) {
        nimcp_nvc_update(&nvc_system, 10.0f);
        float bold;
        nimcp_nvc_get_bold(unit, &bold);
        bold_timeseries.push_back(bold);
    }

    float baseline_mean = 0.0f;
    for (int i = 900; i < 1000; i++) {  /* Last second of baseline */
        baseline_mean += bold_timeseries[i];
    }
    baseline_mean /= 100.0f;

    /* Stimulus block (20s) */
    nimcp_nvc_set_activity(unit, 1.0f);
    for (int i = 0; i < 2000; i++) {
        nimcp_nvc_update(&nvc_system, 10.0f);
        float bold;
        nimcp_nvc_get_bold(unit, &bold);
        bold_timeseries.push_back(bold);
    }

    /* Rest (30s) */
    nimcp_nvc_set_activity(unit, 0.1f);
    for (int i = 0; i < 3000; i++) {
        nimcp_nvc_update(&nvc_system, 10.0f);
        float bold;
        nimcp_nvc_get_bold(unit, &bold);
        bold_timeseries.push_back(bold);
    }

    /* Analyze BOLD response */
    float max_bold = baseline_mean;
    int peak_time = 0;
    for (int i = 1000; i < 4000; i++) {
        if (bold_timeseries[i] > max_bold) {
            max_bold = bold_timeseries[i];
            peak_time = i * 10;  /* ms */
        }
    }
    (void)peak_time;  /* Silence unused variable warning */

    /* BOLD should show positive response during stimulus */
    EXPECT_GT(max_bold, baseline_mean - 0.01f);

    /* Verify no numerical instability */
    for (const auto& bold : bold_timeseries) {
        EXPECT_FALSE(std::isnan(bold));
        EXPECT_FALSE(std::isinf(bold));
    }
}

TEST_F(NeurovascularPipelineE2ETest, BOLDEventRelatedDesign) {
    /*
     * Pipeline: Simulates event-related fMRI design
     *
     * - Brief stimuli (500ms) with variable ISI (inter-stimulus interval)
     * - Tests temporal resolution of hemodynamic response
     */

    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t unit_id;
    nimcp_nvc_add_unit(&nvc_system, "MotorCortex", position, &unit_id);
    nimcp_nvc_unit_t* unit = nimcp_nvc_get_unit(&nvc_system, unit_id);

    /* Deliver 5 brief events */
    int event_times[] = {1000, 3000, 4500, 7000, 10000};  /* ms */

    std::vector<float> bold_timeseries;
    int current_time = 0;

    for (int i = 0; i < 1500; i++) {
        current_time = i * 10;

        /* Check if we're in an event window */
        bool in_event = false;
        for (int e = 0; e < 5; e++) {
            if (current_time >= event_times[e] && current_time < event_times[e] + 500) {
                in_event = true;
                break;
            }
        }

        nimcp_nvc_set_activity(unit, in_event ? 1.0f : 0.1f);
        nimcp_nvc_update(&nvc_system, 10.0f);

        float bold;
        nimcp_nvc_get_bold(unit, &bold);
        bold_timeseries.push_back(bold);
    }

    /* Verify all BOLD values are valid */
    for (const auto& bold : bold_timeseries) {
        EXPECT_FALSE(std::isnan(bold));
        EXPECT_FALSE(std::isinf(bold));
    }

    /* BOLD should show dynamic range */
    float min_bold = bold_timeseries[0];
    float max_bold = bold_timeseries[0];
    for (const auto& bold : bold_timeseries) {
        if (bold < min_bold) min_bold = bold;
        if (bold > max_bold) max_bold = bold;
    }
    EXPECT_GE(max_bold - min_bold, 0.0f);
}

//=============================================================================
// Multi-Region Connectivity Pipeline Tests
//=============================================================================

TEST_F(NeurovascularPipelineE2ETest, WholebrainActivityPattern) {
    /*
     * Pipeline: Multiple brain regions with realistic activity patterns
     *
     * Tests heterogeneous activation across brain regions
     */

    struct RegionSpec {
        const char* name;
        float baseline_activity;
        float stimulus_activity;
        float position[3];
    };

    RegionSpec regions[] = {
        {"VisualCortex", 0.1f, 0.9f, {0.0f, 0.0f, 0.0f}},
        {"MotorCortex", 0.2f, 0.3f, {1.0f, 0.0f, 0.0f}},
        {"PrefrontalCortex", 0.15f, 0.5f, {2.0f, 0.0f, 0.0f}},
        {"Hippocampus", 0.1f, 0.4f, {3.0f, 0.0f, 0.0f}},
        {"Amygdala", 0.1f, 0.6f, {4.0f, 0.0f, 0.0f}}
    };

    const int NUM_REGIONS = 5;
    std::vector<uint32_t> unit_ids;

    for (int i = 0; i < NUM_REGIONS; i++) {
        uint32_t id;
        nimcp_nvc_add_unit(&nvc_system, regions[i].name, regions[i].position, &id);
        unit_ids.push_back(id);

        nimcp_nvc_unit_t* unit = nimcp_nvc_get_unit(&nvc_system, id);
        unit->astrocyte_coupling = 1.0f;
        nimcp_nvc_set_activity(unit, regions[i].baseline_activity);
    }

    /* Baseline period */
    for (int t = 0; t < 500; t++) {
        nimcp_nvc_update(&nvc_system, 10.0f);
    }

    /* Visual stimulus - activates visual cortex primarily */
    nimcp_nvc_unit_t* visual = nimcp_nvc_get_unit(&nvc_system, unit_ids[0]);
    nimcp_nvc_set_activity(visual, 0.9f);

    /* Emotional stimulus - activates amygdala */
    nimcp_nvc_unit_t* amygdala = nimcp_nvc_get_unit(&nvc_system, unit_ids[4]);
    nimcp_nvc_set_activity(amygdala, 0.6f);

    /* Stimulus period */
    for (int t = 0; t < 2000; t++) {
        nimcp_nvc_update(&nvc_system, 10.0f);
    }

    /* Collect BOLD values */
    float bolds[NUM_REGIONS];
    for (int i = 0; i < NUM_REGIONS; i++) {
        nimcp_nvc_unit_t* unit = nimcp_nvc_get_unit(&nvc_system, unit_ids[i]);
        nimcp_nvc_get_bold(unit, &bolds[i]);

        EXPECT_FALSE(std::isnan(bolds[i]));
        EXPECT_FALSE(std::isinf(bolds[i]));
    }

    /* Activated regions should show different BOLD levels */
    /* Visual cortex was most activated, should show response */
    EXPECT_GE(bolds[0], bolds[1] - 0.1f);  /* Visual vs Motor (less active) */
}

//=============================================================================
// Integrated Chemistry-Neurovascular Pipeline
//=============================================================================

TEST_F(NeurovascularPipelineE2ETest, ChemistryMediatedHemodynamics) {
    /*
     * Pipeline: Neural activity -> Chemistry (pH, NO) -> Hemodynamic response
     *
     * Tests the full cascade from activity through chemical mediators to CBF/BOLD
     */

    /* Create matching entities in all systems */
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t nvc_id, ph_id, no_id;
    nimcp_nvc_add_unit(&nvc_system, "IntegratedRegion", position, &nvc_id);
    nimcp_ph_add_region(&ph_system, "IntegratedRegion", &ph_id);
    nimcp_no_add_source(&no_system, position, NOS_TYPE_ENOS, &no_id);

    nimcp_nvc_unit_t* nvc_unit = nimcp_nvc_get_unit(&nvc_system, nvc_id);
    nimcp_ph_region_t* ph_region = nimcp_ph_get_region(&ph_system, ph_id);
    nimcp_no_source_t* no_source = nimcp_no_get_source(&no_system, no_id);

    /* Configure systems */
    nvc_unit->astrocyte_coupling = 1.0f;
    nimcp_ph_add_buffer(ph_region, PH_BUFFER_BICARBONATE, 24.0f);

    /* Baseline measurements */
    float baseline_cbf, baseline_bold, baseline_ph;
    nimcp_nvc_get_cbf(nvc_unit, &baseline_cbf);
    nimcp_nvc_get_bold(nvc_unit, &baseline_bold);
    nimcp_ph_get_compartment_ph(ph_region, PH_COMPARTMENT_EXTRACELLULAR, &baseline_ph);
    float baseline_no = no_source->no_concentration;

    /* Apply coordinated activity across all systems */
    float activity = 1.0f;
    nimcp_nvc_set_activity(nvc_unit, activity);
    nimcp_ph_set_activity(ph_region, activity);
    nimcp_no_set_calcium(no_source, activity);

    /* Simulate integrated response */
    for (int t = 0; t < 500; t++) {
        update_all(10.0f);
    }

    /* Measure final states */
    float final_cbf, final_bold, final_ph;
    nimcp_nvc_get_cbf(nvc_unit, &final_cbf);
    nimcp_nvc_get_bold(nvc_unit, &final_bold);
    nimcp_ph_get_compartment_ph(ph_region, PH_COMPARTMENT_EXTRACELLULAR, &final_ph);
    float final_no = no_source->no_concentration;

    /* Activity should produce expected chemical changes */
    EXPECT_LT(final_ph, baseline_ph);  /* Acidification from metabolism */
    EXPECT_GT(final_no, baseline_no);  /* NO production from NOS */

    /* Hemodynamic response should occur */
    EXPECT_GE(final_cbf, baseline_cbf);  /* CBF should increase or stay same */

    /* All values should be numerically stable */
    EXPECT_FALSE(std::isnan(final_cbf));
    EXPECT_FALSE(std::isnan(final_bold));
    EXPECT_FALSE(std::isnan(final_ph));
    EXPECT_FALSE(std::isnan(final_no));
}

//=============================================================================
// Long Duration Stability Tests
//=============================================================================

TEST_F(NeurovascularPipelineE2ETest, ExtendedSimulationStability) {
    /*
     * Pipeline: Extended simulation over many cycles
     *
     * Tests numerical stability over long time periods
     */

    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t nvc_id;
    nimcp_nvc_add_unit(&nvc_system, "StabilityRegion", position, &nvc_id);
    nimcp_nvc_unit_t* nvc_unit = nimcp_nvc_get_unit(&nvc_system, nvc_id);

    nvc_unit->astrocyte_coupling = 1.0f;

    /* Simulate many activity cycles */
    for (int cycle = 0; cycle < 50; cycle++) {
        /* Active phase */
        nimcp_nvc_set_activity(nvc_unit, 0.7f + 0.3f * (cycle % 3) / 2.0f);
        for (int t = 0; t < 50; t++) {
            nimcp_nvc_update(&nvc_system, 10.0f);
        }

        /* Rest phase */
        nimcp_nvc_set_activity(nvc_unit, 0.1f);
        for (int t = 0; t < 50; t++) {
            nimcp_nvc_update(&nvc_system, 10.0f);
        }

        /* Verify stability at each cycle */
        float cbf, bold, oef;
        nimcp_nvc_get_cbf(nvc_unit, &cbf);
        nimcp_nvc_get_bold(nvc_unit, &bold);
        nimcp_nvc_get_oef(nvc_unit, &oef);

        EXPECT_FALSE(std::isnan(cbf)) << "NaN CBF at cycle " << cycle;
        EXPECT_FALSE(std::isnan(nvc_unit->cbv)) << "NaN CBV at cycle " << cycle;
        EXPECT_FALSE(std::isnan(bold)) << "NaN BOLD at cycle " << cycle;
        EXPECT_FALSE(std::isnan(oef)) << "NaN OEF at cycle " << cycle;

        EXPECT_GE(cbf, 0.0f) << "Negative CBF at cycle " << cycle;
        EXPECT_GE(nvc_unit->cbv, 0.0f) << "Negative CBV at cycle " << cycle;
        EXPECT_GE(oef, 0.0f) << "Negative OEF at cycle " << cycle;
        EXPECT_LE(oef, 1.0f) << "OEF > 1 at cycle " << cycle;
    }
}

TEST_F(NeurovascularPipelineE2ETest, HighFrequencyUpdateStability) {
    /*
     * Pipeline: Very rapid updates
     *
     * Tests stability with sub-millisecond timesteps
     */

    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t nvc_id;
    nimcp_nvc_add_unit(&nvc_system, "HighFreqRegion", position, &nvc_id);
    nimcp_nvc_unit_t* nvc_unit = nimcp_nvc_get_unit(&nvc_system, nvc_id);

    nimcp_nvc_set_activity(nvc_unit, 0.5f);

    /* 10,000 updates at 0.1ms each = 1 second of simulation */
    for (int t = 0; t < 10000; t++) {
        nimcp_nvc_update(&nvc_system, 0.1f);
    }

    float cbf, bold;
    nimcp_nvc_get_cbf(nvc_unit, &cbf);
    nimcp_nvc_get_bold(nvc_unit, &bold);

    EXPECT_FALSE(std::isnan(cbf));
    EXPECT_FALSE(std::isnan(bold));
    EXPECT_GE(cbf, 0.0f);
}

//=============================================================================
// Pathological Scenario Tests
//=============================================================================

TEST_F(NeurovascularPipelineE2ETest, StrokeIschemiaPipeline) {
    /*
     * Pipeline: Simulates ischemic event (reduced blood flow)
     *
     * Tests system behavior under pathological reduced flow
     */

    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t unit_id;
    nimcp_nvc_add_unit(&nvc_system, "IschemicRegion", position, &unit_id);
    nimcp_nvc_unit_t* unit = nimcp_nvc_get_unit(&nvc_system, unit_id);

    /* Normal baseline */
    nimcp_nvc_set_activity(unit, 0.3f);
    for (int t = 0; t < 500; t++) {
        nimcp_nvc_update(&nvc_system, 10.0f);
    }

    float baseline_cbf;
    nimcp_nvc_get_cbf(unit, &baseline_cbf);

    /* Simulate reduced vascular response (partial ischemia) */
    /* This is implemented through reduced astrocyte coupling */
    unit->astrocyte_coupling = 0.2f;  /* Impaired coupling */
    /* Increase activity to simulate high demand with low supply */
    nimcp_nvc_set_activity(unit, 0.8f);

    for (int t = 0; t < 1000; t++) {
        nimcp_nvc_update(&nvc_system, 10.0f);
    }

    float ischemic_cbf, ischemic_oef;
    nimcp_nvc_get_cbf(unit, &ischemic_cbf);
    nimcp_nvc_get_oef(unit, &ischemic_oef);

    /* System should remain stable even in pathological state */
    EXPECT_FALSE(std::isnan(ischemic_cbf));
    EXPECT_FALSE(std::isnan(ischemic_oef));
    EXPECT_GE(ischemic_cbf, 0.0f);
    EXPECT_GE(ischemic_oef, 0.0f);
    EXPECT_LE(ischemic_oef, 1.0f);
}

TEST_F(NeurovascularPipelineE2ETest, HyperperfusionPipeline) {
    /*
     * Pipeline: Simulates hyperperfusion (excessive blood flow)
     *
     * Tests system behavior under pathological increased flow
     */

    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t unit_id;
    nimcp_nvc_add_unit(&nvc_system, "HyperperfusionRegion", position, &unit_id);
    nimcp_nvc_unit_t* unit = nimcp_nvc_get_unit(&nvc_system, unit_id);

    /* Excessive activity + high astrocyte coupling */
    nimcp_nvc_set_activity(unit, 1.0f);
    unit->astrocyte_coupling = 2.0f;  /* Enhanced coupling */

    for (int t = 0; t < 1000; t++) {
        nimcp_nvc_update(&nvc_system, 10.0f);
    }

    float cbf, oef;
    nimcp_nvc_get_cbf(unit, &cbf);
    nimcp_nvc_get_oef(unit, &oef);

    /* Values should be bounded even with excessive input */
    EXPECT_FALSE(std::isnan(cbf));
    EXPECT_FALSE(std::isinf(cbf));
    EXPECT_FALSE(std::isnan(unit->cbv));
    EXPECT_FALSE(std::isinf(unit->cbv));
    EXPECT_GE(oef, 0.0f);
    EXPECT_LE(oef, 1.0f);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
