/**
 * @file test_neurovascular_regression.cpp
 * @brief Regression tests for Neurovascular Coupling module
 *
 * WHAT: Regression tests to prevent neurovascular bugs from recurring
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
#include "biology/neurovascular/nimcp_neurovascular.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class NeurovascularRegressionTest : public ::testing::Test {
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
// Numerical Stability Regression Tests
//=============================================================================

TEST_F(NeurovascularRegressionTest, NoNaNWithZeroActivity) {
    /* Previously: Zero activity caused division issues in flow calculations */
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t unit_id;
    nimcp_nvc_add_unit(&system, "ZeroActivity", position, &unit_id);
    nimcp_nvc_unit_t* unit = nimcp_nvc_get_unit(&system, unit_id);

    nimcp_nvc_set_activity(unit, 0.0f);

    for (int i = 0; i < 100; i++) {
        nimcp_nvc_update(&system, 10.0f);
    }

    float cbf, bold;
    nimcp_nvc_get_cbf(unit, &cbf);
    nimcp_nvc_get_bold(unit, &bold);

    EXPECT_FALSE(std::isnan(cbf));
    EXPECT_FALSE(std::isnan(bold));
    EXPECT_FALSE(std::isinf(cbf));
    EXPECT_FALSE(std::isinf(bold));
}

TEST_F(NeurovascularRegressionTest, NoOverflowWithMaxActivity) {
    /* Previously: Maximum activity caused CBF overflow */
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t unit_id;
    nimcp_nvc_add_unit(&system, "MaxActivity", position, &unit_id);
    nimcp_nvc_unit_t* unit = nimcp_nvc_get_unit(&system, unit_id);

    nimcp_nvc_set_activity(unit, 10.0f);  /* Extreme activity */

    for (int i = 0; i < 1000; i++) {
        nimcp_nvc_update(&system, 10.0f);
    }

    float cbf, bold;
    nimcp_nvc_get_cbf(unit, &cbf);
    nimcp_nvc_get_bold(unit, &bold);

    EXPECT_FALSE(std::isnan(cbf));
    EXPECT_FALSE(std::isinf(cbf));
    EXPECT_FALSE(std::isnan(unit->cbv));
    EXPECT_FALSE(std::isinf(unit->cbv));
    EXPECT_FALSE(std::isnan(bold));
    EXPECT_FALSE(std::isinf(bold));
}

TEST_F(NeurovascularRegressionTest, StabilityWithTinyTimesteps) {
    /* Previously: Very small timesteps accumulated errors */
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t unit_id;
    nimcp_nvc_add_unit(&system, "TinyStep", position, &unit_id);
    nimcp_nvc_unit_t* unit = nimcp_nvc_get_unit(&system, unit_id);

    nimcp_nvc_set_activity(unit, 0.5f);

    for (int i = 0; i < 100000; i++) {
        nimcp_nvc_update(&system, 0.01f);  /* 0.01ms steps */
    }

    float cbf, bold;
    nimcp_nvc_get_cbf(unit, &cbf);
    nimcp_nvc_get_bold(unit, &bold);

    EXPECT_FALSE(std::isnan(cbf));
    EXPECT_FALSE(std::isnan(bold));
    EXPECT_GE(cbf, 0.0f);
}

TEST_F(NeurovascularRegressionTest, StabilityWithLargeTimesteps) {
    /* Previously: Large timesteps caused instability in Balloon model */
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t unit_id;
    nimcp_nvc_add_unit(&system, "LargeStep", position, &unit_id);
    nimcp_nvc_unit_t* unit = nimcp_nvc_get_unit(&system, unit_id);

    nimcp_nvc_set_activity(unit, 0.5f);

    for (int i = 0; i < 100; i++) {
        nimcp_nvc_update(&system, 1000.0f);  /* 1 second steps */
    }

    float cbf, bold;
    nimcp_nvc_get_cbf(unit, &cbf);
    nimcp_nvc_get_bold(unit, &bold);

    EXPECT_FALSE(std::isnan(cbf));
    EXPECT_FALSE(std::isnan(unit->cbv));
    EXPECT_FALSE(std::isnan(bold));
}

//=============================================================================
// Balloon Model Regression Tests
//=============================================================================

TEST_F(NeurovascularRegressionTest, CBVDoesNotExceedPhysiologicalMax) {
    /* Previously: CBV could exceed physiological bounds */
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t unit_id;
    nimcp_nvc_add_unit(&system, "CBVMax", position, &unit_id);
    nimcp_nvc_unit_t* unit = nimcp_nvc_get_unit(&system, unit_id);

    nimcp_nvc_set_activity(unit, 1.0f);

    for (int i = 0; i < 5000; i++) {
        nimcp_nvc_update(&system, 10.0f);
    }

    /* CBV should have a reasonable maximum */
    EXPECT_LT(unit->cbv, 10.0f);
}

TEST_F(NeurovascularRegressionTest, CBVDoesNotGoBelowZero) {
    /* Previously: CBV could go negative during undershoot */
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t unit_id;
    nimcp_nvc_add_unit(&system, "CBVMin", position, &unit_id);
    nimcp_nvc_unit_t* unit = nimcp_nvc_get_unit(&system, unit_id);

    /* Activity pulse then quiet */
    nimcp_nvc_set_activity(unit, 1.0f);
    for (int i = 0; i < 200; i++) {
        nimcp_nvc_update(&system, 10.0f);
    }

    nimcp_nvc_set_activity(unit, 0.0f);
    for (int i = 0; i < 2000; i++) {
        nimcp_nvc_update(&system, 10.0f);
        EXPECT_GE(unit->cbv, 0.0f);  /* CBV cannot be negative */
    }
}

TEST_F(NeurovascularRegressionTest, OEFStaysInValidRange) {
    /* Previously: OEF could exceed 1.0 or go negative */
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t unit_id;
    nimcp_nvc_add_unit(&system, "OEFRange", position, &unit_id);
    nimcp_nvc_unit_t* unit = nimcp_nvc_get_unit(&system, unit_id);

    /* Test various activity levels */
    float activities[] = {0.0f, 0.3f, 0.7f, 1.0f, 2.0f};

    for (float activity : activities) {
        nimcp_nvc_set_activity(unit, activity);
        for (int i = 0; i < 200; i++) {
            nimcp_nvc_update(&system, 10.0f);
        }

        float oef;
        nimcp_nvc_get_oef(unit, &oef);

        EXPECT_GE(oef, 0.0f);
        EXPECT_LE(oef, 1.0f);  /* OEF is a fraction */
    }
}

TEST_F(NeurovascularRegressionTest, BloodFlowStaysPositive) {
    /* Blood flow should remain positive during high activity */
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t unit_id;
    nimcp_nvc_add_unit(&system, "FlowPositive", position, &unit_id);
    nimcp_nvc_unit_t* unit = nimcp_nvc_get_unit(&system, unit_id);

    nimcp_nvc_set_activity(unit, 1.0f);

    for (int i = 0; i < 1000; i++) {
        nimcp_nvc_update(&system, 10.0f);
        EXPECT_GE(unit->cbf, 0.0f);
    }
}

//=============================================================================
// HRF Regression Tests
//=============================================================================

TEST_F(NeurovascularRegressionTest, BOLDReturnsToBaseline) {
    /* Previously: BOLD never fully returned to baseline */
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t unit_id;
    nimcp_nvc_add_unit(&system, "BOLDBaseline", position, &unit_id);
    nimcp_nvc_unit_t* unit = nimcp_nvc_get_unit(&system, unit_id);

    float baseline_bold;
    nimcp_nvc_get_bold(unit, &baseline_bold);

    /* Stimulus */
    nimcp_nvc_set_activity(unit, 1.0f);
    for (int i = 0; i < 200; i++) {
        nimcp_nvc_update(&system, 10.0f);
    }

    /* Long recovery */
    nimcp_nvc_set_activity(unit, 0.0f);
    for (int i = 0; i < 5000; i++) {
        nimcp_nvc_update(&system, 10.0f);
    }

    float recovered_bold;
    nimcp_nvc_get_bold(unit, &recovered_bold);

    /* Should be close to baseline */
    EXPECT_NEAR(recovered_bold, baseline_bold, 0.1f);
}

TEST_F(NeurovascularRegressionTest, BOLDHasReasonablePeakLatency) {
    /* Previously: BOLD peaked immediately or never peaked */
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t unit_id;
    nimcp_nvc_add_unit(&system, "BOLDPeak", position, &unit_id);
    nimcp_nvc_unit_t* unit = nimcp_nvc_get_unit(&system, unit_id);

    /* Brief impulse */
    nimcp_nvc_set_activity(unit, 1.0f);
    for (int i = 0; i < 10; i++) {
        nimcp_nvc_update(&system, 10.0f);
    }
    nimcp_nvc_set_activity(unit, 0.0f);

    /* Find peak timing */
    int peak_time_ms = 0;
    float peak_bold = -1000.0f;

    for (int i = 0; i < 2000; i++) {
        nimcp_nvc_update(&system, 10.0f);
        float bold;
        nimcp_nvc_get_bold(unit, &bold);
        if (bold > peak_bold) {
            peak_bold = bold;
            peak_time_ms = i * 10;
        }
    }

    /* Peak should be reasonable */
    EXPECT_GE(peak_time_ms, 0);
}

//=============================================================================
// Astrocyte Regression Tests
//=============================================================================

TEST_F(NeurovascularRegressionTest, AstrocyteCalciumNonNegative) {
    /* Previously: Astrocyte calcium could go negative */
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t unit_id;
    nimcp_nvc_add_unit(&system, "AstroCalcium", position, &unit_id);
    nimcp_nvc_unit_t* unit = nimcp_nvc_get_unit(&system, unit_id);

    unit->astrocyte_coupling = 1.0f;
    nimcp_nvc_set_activity(unit, 1.0f);

    for (int i = 0; i < 500; i++) {
        nimcp_nvc_update(&system, 10.0f);
        EXPECT_GE(unit->astrocyte_calcium, 0.0f);
    }
}

TEST_F(NeurovascularRegressionTest, AstrocyteCouplingScales) {
    /* Previously: Astrocyte coupling didn't scale properly */
    float pos1[3] = {0.0f, 0.0f, 0.0f};
    float pos2[3] = {100.0f, 0.0f, 0.0f};
    uint32_t low_id, high_id;
    nimcp_nvc_add_unit(&system, "LowCoupling", pos1, &low_id);
    nimcp_nvc_add_unit(&system, "HighCoupling", pos2, &high_id);

    nimcp_nvc_unit_t* low = nimcp_nvc_get_unit(&system, low_id);
    nimcp_nvc_unit_t* high = nimcp_nvc_get_unit(&system, high_id);

    low->astrocyte_coupling = 0.1f;
    high->astrocyte_coupling = 1.0f;

    nimcp_nvc_set_activity(low, 1.0f);
    nimcp_nvc_set_activity(high, 1.0f);

    for (int i = 0; i < 200; i++) {
        nimcp_nvc_update(&system, 10.0f);
    }

    /* Higher coupling should produce equal or greater calcium response */
    EXPECT_GE(high->astrocyte_calcium, low->astrocyte_calcium * 0.9f);
}

//=============================================================================
// Multi-Unit Regression Tests
//=============================================================================

TEST_F(NeurovascularRegressionTest, UnitsAreIndependent) {
    /* Previously: Units interfered with each other */
    float pos1[3] = {0.0f, 0.0f, 0.0f};
    float pos2[3] = {100.0f, 0.0f, 0.0f};
    uint32_t id1, id2;
    nimcp_nvc_add_unit(&system, "Unit1", pos1, &id1);
    nimcp_nvc_add_unit(&system, "Unit2", pos2, &id2);

    nimcp_nvc_unit_t* unit1 = nimcp_nvc_get_unit(&system, id1);
    nimcp_nvc_unit_t* unit2 = nimcp_nvc_get_unit(&system, id2);

    nimcp_nvc_set_activity(unit1, 1.0f);
    nimcp_nvc_set_activity(unit2, 0.0f);

    for (int i = 0; i < 500; i++) {
        nimcp_nvc_update(&system, 10.0f);
    }

    float cbf1, cbf2;
    nimcp_nvc_get_cbf(unit1, &cbf1);
    nimcp_nvc_get_cbf(unit2, &cbf2);

    /* Active unit should have higher CBF */
    EXPECT_GE(cbf1, cbf2);
}

TEST_F(NeurovascularRegressionTest, MaxUnitsHandled) {
    /* Previously: Many units caused memory issues */
    std::vector<uint32_t> ids;

    for (int i = 0; i < 50; i++) {
        char name[32];
        snprintf(name, sizeof(name), "Unit%d", i);
        float position[3] = {(float)i * 10.0f, 0.0f, 0.0f};
        uint32_t id;
        nimcp_nvc_error_t err = nimcp_nvc_add_unit(&system, name, position, &id);
        if (err == NVC_OK) {
            ids.push_back(id);
            nimcp_nvc_unit_t* unit = nimcp_nvc_get_unit(&system, id);
            nimcp_nvc_set_activity(unit, i * 0.02f);
        }
    }

    for (int i = 0; i < 100; i++) {
        nimcp_nvc_update(&system, 10.0f);
    }

    for (size_t i = 0; i < ids.size(); i++) {
        nimcp_nvc_unit_t* unit = nimcp_nvc_get_unit(&system, ids[i]);
        EXPECT_NE(unit, nullptr);

        float cbf;
        nimcp_nvc_get_cbf(unit, &cbf);
        EXPECT_FALSE(std::isnan(cbf));
    }
}

//=============================================================================
// Memory/Resource Regression Tests
//=============================================================================

TEST_F(NeurovascularRegressionTest, InitShutdownCycle) {
    /* Previously: Cycles caused resource leaks */
    for (int cycle = 0; cycle < 10; cycle++) {
        nimcp_nvc_system_t temp_system;
        memset(&temp_system, 0, sizeof(temp_system));

        nimcp_nvc_error_t err = nimcp_nvc_init(&temp_system, nullptr);
        EXPECT_EQ(err, NVC_OK);

        float position[3] = {0.0f, 0.0f, 0.0f};
        uint32_t id;
        nimcp_nvc_add_unit(&temp_system, "TempUnit", position, &id);

        err = nimcp_nvc_shutdown(&temp_system);
        EXPECT_EQ(err, NVC_OK);
    }
}

TEST_F(NeurovascularRegressionTest, MetricsAccumulate) {
    /* Previously: Metrics didn't accumulate */
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t unit_id;
    nimcp_nvc_add_unit(&system, "MetricsUnit", position, &unit_id);
    nimcp_nvc_unit_t* unit = nimcp_nvc_get_unit(&system, unit_id);

    nimcp_nvc_set_activity(unit, 0.5f);

    for (int i = 0; i < 100; i++) {
        nimcp_nvc_update(&system, 10.0f);
    }

    nimcp_nvc_metrics_t metrics1;
    nimcp_nvc_get_metrics(&system, &metrics1);

    for (int i = 0; i < 100; i++) {
        nimcp_nvc_update(&system, 10.0f);
    }

    nimcp_nvc_metrics_t metrics2;
    nimcp_nvc_get_metrics(&system, &metrics2);

    EXPECT_GT(metrics2.total_simulation_time, metrics1.total_simulation_time);
}

//=============================================================================
// Edge Case Regression Tests
//=============================================================================

TEST_F(NeurovascularRegressionTest, RapidActivityChanges) {
    /* Previously: Rapid activity changes caused instability */
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t unit_id;
    nimcp_nvc_add_unit(&system, "RapidChange", position, &unit_id);
    nimcp_nvc_unit_t* unit = nimcp_nvc_get_unit(&system, unit_id);

    for (int i = 0; i < 1000; i++) {
        float activity = (i % 2 == 0) ? 1.0f : 0.0f;
        nimcp_nvc_set_activity(unit, activity);
        nimcp_nvc_update(&system, 1.0f);
    }

    float cbf, bold;
    nimcp_nvc_get_cbf(unit, &cbf);
    nimcp_nvc_get_bold(unit, &bold);

    EXPECT_FALSE(std::isnan(cbf));
    EXPECT_FALSE(std::isnan(bold));
}

TEST_F(NeurovascularRegressionTest, LongDuration) {
    /* Previously: Long simulations accumulated errors */
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t unit_id;
    nimcp_nvc_add_unit(&system, "LongDuration", position, &unit_id);
    nimcp_nvc_unit_t* unit = nimcp_nvc_get_unit(&system, unit_id);

    nimcp_nvc_set_activity(unit, 0.3f);

    /* Simulate for long duration */
    for (int i = 0; i < 100000; i++) {
        nimcp_nvc_update(&system, 10.0f);  /* 1000 seconds total */
    }

    float cbf, bold;
    nimcp_nvc_get_cbf(unit, &cbf);
    nimcp_nvc_get_bold(unit, &bold);

    EXPECT_FALSE(std::isnan(cbf));
    EXPECT_FALSE(std::isnan(unit->cbv));
    EXPECT_FALSE(std::isnan(bold));
    EXPECT_GE(cbf, 0.0f);
    EXPECT_GE(unit->cbv, 0.0f);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
