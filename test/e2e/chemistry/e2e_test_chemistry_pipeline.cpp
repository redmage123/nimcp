/**
 * @file e2e_test_chemistry_pipeline.cpp
 * @brief End-to-end tests for Chemistry Layer pipeline
 *
 * WHAT: E2E tests for pH Dynamics and Nitric Oxide signaling integration
 * WHY:  Verify complete chemistry pipeline from neural activity to chemical modulation
 * HOW:  Test full workflows: activity -> chemical changes -> neural effects
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
#include "chemistry/gasotransmitters/nimcp_nitric_oxide.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class ChemistryPipelineE2ETest : public ::testing::Test {
protected:
    nimcp_ph_system_t ph_system;
    nimcp_no_system_t no_system;

    void SetUp() override {
        memset(&ph_system, 0, sizeof(ph_system));
        memset(&no_system, 0, sizeof(no_system));

        nimcp_ph_error_t ph_err = nimcp_ph_init(&ph_system, nullptr);
        ASSERT_EQ(ph_err, PH_OK);

        nimcp_no_error_t no_err = nimcp_no_init(&no_system, nullptr);
        ASSERT_EQ(no_err, NO_OK);
    }

    void TearDown() override {
        nimcp_ph_shutdown(&ph_system);
        nimcp_no_shutdown(&no_system);
    }

    /* Helper: Simulate coupled update of both systems */
    void update_chemistry(float dt_ms) {
        nimcp_ph_update(&ph_system, dt_ms);
        nimcp_no_update(&no_system, dt_ms);
    }
};

//=============================================================================
// Full Pipeline E2E Tests
//=============================================================================

TEST_F(ChemistryPipelineE2ETest, NeuralActivityToChemicalStatesPipeline) {
    /*
     * Pipeline: Neural Activity -> pH Acidification + NO Production
     *
     * This test verifies that:
     * 1. High neural activity produces metabolic acid (pH drops)
     * 2. High neural activity triggers NO production (via calcium)
     * 3. Both systems respond appropriately
     */

    /* Create matching regions in both systems */
    uint32_t ph_region_id, no_source_id;
    nimcp_ph_add_region(&ph_system, "ActiveRegion", &ph_region_id);
    float position[3] = {0.0f, 0.0f, 0.0f};
    nimcp_no_add_source(&no_system, position, NOS_TYPE_NNOS, &no_source_id);

    nimcp_ph_region_t* ph_region = nimcp_ph_get_region(&ph_system, ph_region_id);
    nimcp_no_source_t* no_source = nimcp_no_get_source(&no_system, no_source_id);

    /* Configure for neural activity response */
    nimcp_ph_add_buffer(ph_region, PH_BUFFER_BICARBONATE, 24.0f);

    /* Baseline measurements */
    float baseline_ph;
    nimcp_ph_get_compartment_ph(ph_region, PH_COMPARTMENT_EXTRACELLULAR, &baseline_ph);
    float baseline_no = no_source->no_concentration;

    /* Simulate high neural activity */
    nimcp_ph_set_activity(ph_region, 1.0f);
    nimcp_no_set_calcium(no_source, 1.0f);

    for (int i = 0; i < 500; i++) {
        update_chemistry(10.0f);
    }

    /* Measure final states */
    float final_ph;
    nimcp_ph_get_compartment_ph(ph_region, PH_COMPARTMENT_EXTRACELLULAR, &final_ph);
    float final_no = no_source->no_concentration;

    /* Activity should cause acidification */
    EXPECT_LT(final_ph, baseline_ph);

    /* Activity should cause NO production */
    EXPECT_GT(final_no, baseline_no);

    /* Verify no numerical instability */
    EXPECT_FALSE(std::isnan(final_ph));
    EXPECT_FALSE(std::isnan(final_no));
}

TEST_F(ChemistryPipelineE2ETest, ChemicalRecoveryAfterActivityPipeline) {
    /*
     * Pipeline: Activity Cessation -> pH Recovery + NO Clearance
     *
     * Tests that chemical states return to baseline after activity stops
     */

    uint32_t ph_region_id, no_source_id;
    nimcp_ph_add_region(&ph_system, "RecoveryRegion", &ph_region_id);
    float position[3] = {0.0f, 0.0f, 0.0f};
    nimcp_no_add_source(&no_system, position, NOS_TYPE_NNOS, &no_source_id);

    nimcp_ph_region_t* ph_region = nimcp_ph_get_region(&ph_system, ph_region_id);
    nimcp_no_source_t* no_source = nimcp_no_get_source(&no_system, no_source_id);

    nimcp_ph_add_buffer(ph_region, PH_BUFFER_BICARBONATE, 24.0f);
    nimcp_ph_set_pump_enabled(ph_region, PH_PUMP_NHE, true);

    /* Phase 1: Activity phase */
    nimcp_ph_set_activity(ph_region, 1.0f);
    nimcp_no_set_calcium(no_source, 1.0f);

    for (int i = 0; i < 200; i++) {
        update_chemistry(10.0f);
    }

    float acidified_ph;
    nimcp_ph_get_compartment_ph(ph_region, PH_COMPARTMENT_EXTRACELLULAR, &acidified_ph);
    float elevated_no = no_source->no_concentration;

    /* Phase 2: Recovery phase */
    nimcp_ph_set_activity(ph_region, 0.0f);
    nimcp_no_set_calcium(no_source, 0.0f);

    for (int i = 0; i < 1000; i++) {
        update_chemistry(10.0f);
    }

    float recovered_ph;
    nimcp_ph_get_compartment_ph(ph_region, PH_COMPARTMENT_EXTRACELLULAR, &recovered_ph);
    float cleared_no = no_source->no_concentration;

    /* pH should recover toward normal */
    EXPECT_GT(recovered_ph, acidified_ph);

    /* NO should clear */
    EXPECT_LT(cleared_no, elevated_no);
}

TEST_F(ChemistryPipelineE2ETest, MultiRegionChemistryPipeline) {
    /*
     * Pipeline: Multiple regions with different activity patterns
     *
     * Tests that chemistry responds correctly to heterogeneous activity
     */

    const int NUM_REGIONS = 4;
    uint32_t ph_ids[NUM_REGIONS], no_ids[NUM_REGIONS];
    float activities[] = {0.0f, 0.3f, 0.7f, 1.0f};

    for (int i = 0; i < NUM_REGIONS; i++) {
        char name[32];
        snprintf(name, sizeof(name), "Region%d", i);
        nimcp_ph_add_region(&ph_system, name, &ph_ids[i]);
        float position[3] = {(float)i * 10.0f, 0.0f, 0.0f};
        nimcp_no_add_source(&no_system, position, NOS_TYPE_NNOS, &no_ids[i]);

        nimcp_ph_region_t* ph_region = nimcp_ph_get_region(&ph_system, ph_ids[i]);
        nimcp_no_source_t* no_source = nimcp_no_get_source(&no_system, no_ids[i]);

        nimcp_ph_add_buffer(ph_region, PH_BUFFER_BICARBONATE, 24.0f);

        nimcp_ph_set_activity(ph_region, activities[i]);
        nimcp_no_set_calcium(no_source, activities[i]);
    }

    for (int i = 0; i < 500; i++) {
        update_chemistry(10.0f);
    }

    /* Collect results */
    float phs[NUM_REGIONS], nos[NUM_REGIONS];
    for (int i = 0; i < NUM_REGIONS; i++) {
        nimcp_ph_region_t* ph_region = nimcp_ph_get_region(&ph_system, ph_ids[i]);
        nimcp_no_source_t* no_source = nimcp_no_get_source(&no_system, no_ids[i]);

        nimcp_ph_get_compartment_ph(ph_region, PH_COMPARTMENT_EXTRACELLULAR, &phs[i]);
        nos[i] = no_source->no_concentration;
    }

    /* Higher activity regions should be more acidic */
    for (int i = 1; i < NUM_REGIONS; i++) {
        EXPECT_LE(phs[i], phs[i-1] + 0.1f);  /* Allow small margin */
    }

    /* Higher activity regions should have more NO */
    for (int i = 1; i < NUM_REGIONS; i++) {
        EXPECT_GE(nos[i], nos[i-1] * 0.9f);  /* Allow some margin */
    }
}

//=============================================================================
// Stress Test E2E
//=============================================================================

TEST_F(ChemistryPipelineE2ETest, LongDurationSimulationPipeline) {
    /*
     * Pipeline: Extended simulation with varying activity
     *
     * Tests stability over long simulations with activity cycling
     */

    uint32_t ph_region_id, no_source_id;
    nimcp_ph_add_region(&ph_system, "LongDuration", &ph_region_id);
    float position[3] = {0.0f, 0.0f, 0.0f};
    nimcp_no_add_source(&no_system, position, NOS_TYPE_NNOS, &no_source_id);

    nimcp_ph_region_t* ph_region = nimcp_ph_get_region(&ph_system, ph_region_id);
    nimcp_no_source_t* no_source = nimcp_no_get_source(&no_system, no_source_id);

    nimcp_ph_add_buffer(ph_region, PH_BUFFER_BICARBONATE, 24.0f);
    nimcp_ph_set_pump_enabled(ph_region, PH_PUMP_NHE, true);

    /* Simulate day/night cycles (20 cycles of activity/rest) */
    for (int cycle = 0; cycle < 20; cycle++) {
        /* Active period */
        nimcp_ph_set_activity(ph_region, 0.8f);
        nimcp_no_set_calcium(no_source, 0.8f);
        for (int i = 0; i < 100; i++) {
            update_chemistry(10.0f);
        }

        /* Rest period */
        nimcp_ph_set_activity(ph_region, 0.1f);
        nimcp_no_set_calcium(no_source, 0.1f);
        for (int i = 0; i < 100; i++) {
            update_chemistry(10.0f);
        }
    }

    /* Final state should be stable and within physiological bounds */
    float final_ph;
    nimcp_ph_get_compartment_ph(ph_region, PH_COMPARTMENT_EXTRACELLULAR, &final_ph);
    float final_no = no_source->no_concentration;

    EXPECT_FALSE(std::isnan(final_ph));
    EXPECT_FALSE(std::isinf(final_ph));
    EXPECT_GE(final_ph, PH_MINIMUM_VIABLE);
    EXPECT_LE(final_ph, PH_MAXIMUM_VIABLE);

    EXPECT_FALSE(std::isnan(final_no));
    EXPECT_FALSE(std::isinf(final_no));
    EXPECT_GE(final_no, 0.0f);
}

TEST_F(ChemistryPipelineE2ETest, HighThroughputPipeline) {
    /*
     * Pipeline: Many regions updated rapidly
     *
     * Tests performance with many chemical entities
     */

    const int NUM_ENTITIES = 50;
    std::vector<uint32_t> ph_ids, no_ids;

    for (int i = 0; i < NUM_ENTITIES; i++) {
        char name[32];
        snprintf(name, sizeof(name), "Entity%d", i);

        uint32_t ph_id, no_id;
        nimcp_ph_error_t ph_err = nimcp_ph_add_region(&ph_system, name, &ph_id);
        float position[3] = {(float)i * 10.0f, 0.0f, 0.0f};
        nimcp_no_error_t no_err = nimcp_no_add_source(&no_system, position, NOS_TYPE_NNOS, &no_id);

        if (ph_err == PH_OK && no_err == NO_OK) {
            ph_ids.push_back(ph_id);
            no_ids.push_back(no_id);

            nimcp_ph_region_t* ph_region = nimcp_ph_get_region(&ph_system, ph_id);
            nimcp_no_source_t* no_source = nimcp_no_get_source(&no_system, no_id);

            nimcp_ph_set_activity(ph_region, (float)i / NUM_ENTITIES);
            nimcp_no_set_calcium(no_source, (float)i / NUM_ENTITIES);
        }
    }

    /* Rapid updates */
    for (int i = 0; i < 100; i++) {
        update_chemistry(1.0f);  /* 1ms steps for high frequency */
    }

    /* Verify all entities are stable */
    for (size_t i = 0; i < ph_ids.size(); i++) {
        nimcp_ph_region_t* ph_region = nimcp_ph_get_region(&ph_system, ph_ids[i]);
        nimcp_no_source_t* no_source = nimcp_no_get_source(&no_system, no_ids[i]);

        float ph;
        nimcp_ph_get_compartment_ph(ph_region, PH_COMPARTMENT_EXTRACELLULAR, &ph);
        float no = no_source->no_concentration;

        EXPECT_FALSE(std::isnan(ph));
        EXPECT_FALSE(std::isnan(no));
    }
}

//=============================================================================
// Cross-System Effect E2E Tests
//=============================================================================

TEST_F(ChemistryPipelineE2ETest, NeuralModulationOutputPipeline) {
    /*
     * Pipeline: Chemistry -> Neural Modulation Effects
     *
     * Tests that chemistry produces appropriate modulation signals
     */

    uint32_t ph_region_id, no_source_id;
    nimcp_ph_add_region(&ph_system, "ModulationRegion", &ph_region_id);
    float position[3] = {0.0f, 0.0f, 0.0f};
    nimcp_no_add_source(&no_system, position, NOS_TYPE_NNOS, &no_source_id);

    nimcp_ph_region_t* ph_region = nimcp_ph_get_region(&ph_system, ph_region_id);
    nimcp_no_source_t* no_source = nimcp_no_get_source(&no_system, no_source_id);

    /* Get baseline modifiers */
    float baseline_ph_conductance, baseline_no_plasticity;
    nimcp_ph_get_conductance_modifier(&ph_system, ph_region_id, &baseline_ph_conductance);
    nimcp_no_get_plasticity_modifier(&no_system, &baseline_no_plasticity);

    /* Induce chemical changes */
    nimcp_ph_set_compartment_ph(ph_region, PH_COMPARTMENT_EXTRACELLULAR, 6.9f);  /* Acidosis */
    nimcp_no_set_calcium(no_source, 1.0f);  /* Activate NO */

    for (int i = 0; i < 200; i++) {
        update_chemistry(10.0f);
    }

    /* Get modulated values */
    float acidotic_conductance, no_enhanced_plasticity;
    nimcp_ph_get_conductance_modifier(&ph_system, ph_region_id, &acidotic_conductance);
    nimcp_no_get_plasticity_modifier(&no_system, &no_enhanced_plasticity);

    /* Acidosis should reduce conductance */
    EXPECT_LT(acidotic_conductance, baseline_ph_conductance);

    /* NO should modulate plasticity */
    EXPECT_GE(no_enhanced_plasticity, 0.0f);

    /* Modifiers should be in valid range */
    EXPECT_GE(acidotic_conductance, 0.0f);
    EXPECT_LE(acidotic_conductance, 2.0f);
    EXPECT_GE(no_enhanced_plasticity, 0.0f);
    EXPECT_LE(no_enhanced_plasticity, 10.0f);
}

//=============================================================================
// Pathological State E2E Tests
//=============================================================================

TEST_F(ChemistryPipelineE2ETest, AcidosisDetectionPipeline) {
    /*
     * Pipeline: Activity -> Acidosis -> Critical State Detection
     *
     * Tests that severe acidosis is properly detected
     */

    uint32_t ph_region_id;
    nimcp_ph_add_region(&ph_system, "AcidosisRegion", &ph_region_id);
    nimcp_ph_region_t* ph_region = nimcp_ph_get_region(&ph_system, ph_region_id);

    /* Start normal */
    EXPECT_FALSE(nimcp_ph_is_critical(ph_region, PH_COMPARTMENT_EXTRACELLULAR));

    /* Induce severe acidosis */
    nimcp_ph_set_compartment_ph(ph_region, PH_COMPARTMENT_EXTRACELLULAR, 6.5f);
    nimcp_ph_update(&ph_system, 1.0f);

    /* Should be detected as critical */
    EXPECT_TRUE(nimcp_ph_is_critical(ph_region, PH_COMPARTMENT_EXTRACELLULAR));

    nimcp_ph_status_t status = nimcp_ph_get_status(ph_region);
    EXPECT_NE(status, PH_STATUS_NORMAL);
}

TEST_F(ChemistryPipelineE2ETest, ExcitotoxicityScenarioPipeline) {
    /*
     * Pipeline: Excessive activity -> Chemical dysregulation
     *
     * Models pathological over-activation (excitotoxicity scenario)
     */

    uint32_t ph_region_id, no_source_id;
    nimcp_ph_add_region(&ph_system, "ExcitotoxicRegion", &ph_region_id);
    float position[3] = {0.0f, 0.0f, 0.0f};
    nimcp_no_add_source(&no_system, position, NOS_TYPE_INOS, &no_source_id);  /* iNOS for inflammation */

    nimcp_ph_region_t* ph_region = nimcp_ph_get_region(&ph_system, ph_region_id);
    nimcp_no_source_t* no_source = nimcp_no_get_source(&no_system, no_source_id);

    /* Sustained maximum activity (excitotoxic) */
    nimcp_ph_set_activity(ph_region, 1.0f);
    nimcp_no_set_calcium(no_source, 1.0f);

    for (int i = 0; i < 1000; i++) {
        update_chemistry(10.0f);
    }

    /* Chemistry should still be numerically stable */
    float final_ph;
    nimcp_ph_get_compartment_ph(ph_region, PH_COMPARTMENT_EXTRACELLULAR, &final_ph);
    float final_no = no_source->no_concentration;

    EXPECT_FALSE(std::isnan(final_ph));
    EXPECT_FALSE(std::isnan(final_no));
    EXPECT_FALSE(std::isinf(final_ph));
    EXPECT_FALSE(std::isinf(final_no));

    /* Values should be clamped within physiological limits even under stress */
    EXPECT_GE(final_ph, 0.0f);
    EXPECT_LE(final_ph, 14.0f);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
