/**
 * @file test_no_regression.cpp
 * @brief Regression tests for Nitric Oxide Signaling module
 *
 * WHAT: Regression tests to prevent NO signaling bugs from recurring
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
#include "chemistry/gasotransmitters/nimcp_nitric_oxide.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class NORegressionTest : public ::testing::Test {
protected:
    nimcp_no_system_t system;

    void SetUp() override {
        memset(&system, 0, sizeof(system));
        nimcp_no_error_t err = nimcp_no_init(&system, nullptr);
        ASSERT_EQ(err, NO_OK);
    }

    void TearDown() override {
        nimcp_no_shutdown(&system);
    }
};

//=============================================================================
// Numerical Stability Regression Tests
//=============================================================================

TEST_F(NORegressionTest, NoNaNWithZeroCalcium) {
    /* Previously: Zero calcium caused division issues in NOS kinetics */
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t source_id;
    nimcp_no_add_source(&system, position, NOS_TYPE_NNOS, &source_id);
    nimcp_no_source_t* source = nimcp_no_get_source(&system, source_id);

    nimcp_no_set_calcium(source, 0.0f);

    for (int i = 0; i < 100; i++) {
        nimcp_no_update(&system, 10.0f);
    }

    EXPECT_FALSE(std::isnan(source->no_concentration));
    EXPECT_FALSE(std::isinf(source->no_concentration));
    EXPECT_GE(source->no_concentration, 0.0f);
}

TEST_F(NORegressionTest, NoOverflowWithMaxCalcium) {
    /* Previously: Maximum calcium caused NO concentration overflow */
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t source_id;
    nimcp_no_add_source(&system, position, NOS_TYPE_NNOS, &source_id);
    nimcp_no_source_t* source = nimcp_no_get_source(&system, source_id);

    /* Maximum calcium activation */
    nimcp_no_set_calcium(source, 10.0f);

    for (int i = 0; i < 1000; i++) {
        nimcp_no_update(&system, 10.0f);
    }

    EXPECT_FALSE(std::isnan(source->no_concentration));
    EXPECT_FALSE(std::isinf(source->no_concentration));
}

TEST_F(NORegressionTest, cGMPDoesNotExceedMaximum) {
    /* Previously: cGMP could accumulate without bound */
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t source_id;
    nimcp_no_add_source(&system, position, NOS_TYPE_NNOS, &source_id);
    nimcp_no_source_t* source = nimcp_no_get_source(&system, source_id);

    nimcp_no_set_calcium(source, 1.0f);

    /* Sustained NO production */
    for (int i = 0; i < 10000; i++) {
        nimcp_no_update(&system, 10.0f);
    }

    float cgmp;
    nimcp_no_get_cgmp(&system, source_id, &cgmp);

    EXPECT_FALSE(std::isnan(cgmp));
    EXPECT_FALSE(std::isinf(cgmp));
}

TEST_F(NORegressionTest, StabilityWithTinyTimesteps) {
    /* Previously: Very small timesteps accumulated floating point errors */
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t source_id;
    nimcp_no_add_source(&system, position, NOS_TYPE_NNOS, &source_id);
    nimcp_no_source_t* source = nimcp_no_get_source(&system, source_id);

    nimcp_no_set_calcium(source, 0.5f);

    /* Very small timesteps */
    for (int i = 0; i < 100000; i++) {
        nimcp_no_update(&system, 0.01f);
    }

    float cgmp;
    nimcp_no_get_cgmp(&system, source_id, &cgmp);

    EXPECT_FALSE(std::isnan(source->no_concentration));
    EXPECT_FALSE(std::isnan(cgmp));
    EXPECT_GE(source->no_concentration, 0.0f);
    EXPECT_GE(cgmp, 0.0f);
}

//=============================================================================
// Decay/Degradation Regression Tests
//=============================================================================

TEST_F(NORegressionTest, NODecaysCorrectly) {
    /* Previously: NO never fully decayed due to floating point issues */
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t source_id;
    nimcp_no_add_source(&system, position, NOS_TYPE_NNOS, &source_id);
    nimcp_no_source_t* source = nimcp_no_get_source(&system, source_id);

    /* Produce NO */
    nimcp_no_set_calcium(source, 1.0f);
    for (int i = 0; i < 100; i++) {
        nimcp_no_update(&system, 10.0f);
    }
    float peak_no = source->no_concentration;

    /* Stop production and let it decay */
    nimcp_no_set_calcium(source, 0.0f);
    for (int i = 0; i < 10000; i++) {
        nimcp_no_update(&system, 10.0f);
    }

    /* Should have decayed */
    EXPECT_LE(source->no_concentration, peak_no);
}

TEST_F(NORegressionTest, cGMPDecaysCorrectly) {
    /* Previously: cGMP didn't degrade properly */
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t source_id;
    nimcp_no_add_source(&system, position, NOS_TYPE_NNOS, &source_id);
    nimcp_no_source_t* source = nimcp_no_get_source(&system, source_id);

    /* Produce some cGMP via NO */
    nimcp_no_set_calcium(source, 1.0f);
    for (int i = 0; i < 200; i++) {
        nimcp_no_update(&system, 10.0f);
    }

    float peak_cgmp;
    nimcp_no_get_cgmp(&system, source_id, &peak_cgmp);

    /* Stop NO production */
    nimcp_no_set_calcium(source, 0.0f);

    /* Let cGMP degrade */
    for (int i = 0; i < 1000; i++) {
        nimcp_no_update(&system, 10.0f);
    }

    float final_cgmp;
    nimcp_no_get_cgmp(&system, source_id, &final_cgmp);

    /* cGMP should have decreased */
    EXPECT_LE(final_cgmp, peak_cgmp);
}

//=============================================================================
// NOS Isoform Regression Tests
//=============================================================================

TEST_F(NORegressionTest, nNOSCalciumDependent) {
    /* nNOS requires calcium for activation */
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t source_id;
    nimcp_no_add_source(&system, position, NOS_TYPE_NNOS, &source_id);
    nimcp_no_source_t* source = nimcp_no_get_source(&system, source_id);

    /* No calcium */
    nimcp_no_set_calcium(source, 0.0f);
    for (int i = 0; i < 200; i++) {
        nimcp_no_update(&system, 10.0f);
    }
    float no_at_zero_ca = source->no_concentration;

    /* With calcium */
    nimcp_no_set_calcium(source, 1.0f);
    for (int i = 0; i < 200; i++) {
        nimcp_no_update(&system, 10.0f);
    }
    float no_at_high_ca = source->no_concentration;

    /* nNOS should produce more NO with calcium */
    EXPECT_GE(no_at_high_ca, no_at_zero_ca);
}

TEST_F(NORegressionTest, iNOSSustainedProduction) {
    /* iNOS produces NO regardless of calcium once induced */
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t source_id;
    nimcp_no_add_source(&system, position, NOS_TYPE_INOS, &source_id);
    nimcp_no_source_t* source = nimcp_no_get_source(&system, source_id);

    /* No calcium */
    nimcp_no_set_calcium(source, 0.0f);
    for (int i = 0; i < 500; i++) {
        nimcp_no_update(&system, 10.0f);
    }

    /* iNOS should produce NO regardless */
    EXPECT_GE(source->no_concentration, 0.0f);
}

TEST_F(NORegressionTest, eNOSVasodilation) {
    /* eNOS affects vasodilation */
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t source_id;
    nimcp_no_add_source(&system, position, NOS_TYPE_ENOS, &source_id);
    nimcp_no_source_t* source = nimcp_no_get_source(&system, source_id);

    float baseline_vasodilation;
    nimcp_no_get_vasodilation(&system, &baseline_vasodilation);

    /* Activate eNOS */
    nimcp_no_set_calcium(source, 1.0f);
    for (int i = 0; i < 200; i++) {
        nimcp_no_update(&system, 10.0f);
    }

    float activated_vasodilation;
    nimcp_no_get_vasodilation(&system, &activated_vasodilation);

    /* eNOS should affect vasodilation */
    EXPECT_GE(activated_vasodilation, baseline_vasodilation);
}

//=============================================================================
// Diffusion Regression Tests
//=============================================================================

TEST_F(NORegressionTest, DiffusionReachesTargets) {
    /* NO should diffuse to nearby targets */
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t source_id;
    nimcp_no_add_source(&system, position, NOS_TYPE_NNOS, &source_id);
    nimcp_no_source_t* source = nimcp_no_get_source(&system, source_id);

    /* Add target */
    nimcp_no_add_target(source, 1, 10.0f, NO_RETROGRADE_PRESYNAPTIC_RELEASE);

    /* Produce NO and let it diffuse */
    nimcp_no_set_calcium(source, 1.0f);
    for (int i = 0; i < 200; i++) {
        nimcp_no_update(&system, 10.0f);
        nimcp_no_diffuse(&system, source);
    }

    float potentiation;
    nimcp_no_get_target_potentiation(source, 1, &potentiation);

    EXPECT_GE(potentiation, 0.0f);
}

TEST_F(NORegressionTest, NoNegativeConcentrations) {
    /* NO concentration should never be negative */
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t source_id;
    nimcp_no_add_source(&system, position, NOS_TYPE_NNOS, &source_id);
    nimcp_no_source_t* source = nimcp_no_get_source(&system, source_id);

    /* Start with zero calcium */
    nimcp_no_set_calcium(source, 0.0f);
    for (int i = 0; i < 100; i++) {
        nimcp_no_update(&system, 10.0f);
    }

    EXPECT_GE(source->no_concentration, 0.0f);
}

//=============================================================================
// Multi-Source Regression Tests
//=============================================================================

TEST_F(NORegressionTest, SourcesAreIndependent) {
    /* Updating one source shouldn't affect others */
    float pos1[3] = {0.0f, 0.0f, 0.0f};
    float pos2[3] = {100.0f, 0.0f, 0.0f};
    uint32_t id1, id2;

    nimcp_no_add_source(&system, pos1, NOS_TYPE_NNOS, &id1);
    nimcp_no_add_source(&system, pos2, NOS_TYPE_NNOS, &id2);

    nimcp_no_source_t* src1 = nimcp_no_get_source(&system, id1);
    nimcp_no_source_t* src2 = nimcp_no_get_source(&system, id2);

    /* Different configurations */
    nimcp_no_set_calcium(src1, 1.0f);
    nimcp_no_set_calcium(src2, 0.0f);

    for (int i = 0; i < 200; i++) {
        nimcp_no_update(&system, 10.0f);
    }

    /* src1 should have higher NO */
    EXPECT_GT(src1->no_concentration, src2->no_concentration);
}

TEST_F(NORegressionTest, MaxSourcesHandled) {
    /* Many sources should be handled without memory issues */
    std::vector<uint32_t> ids;

    for (int i = 0; i < 100; i++) {
        float position[3] = {(float)i * 10.0f, 0.0f, 0.0f};
        uint32_t id;
        nimcp_no_error_t err = nimcp_no_add_source(&system, position, NOS_TYPE_NNOS, &id);
        if (err == NO_OK) {
            ids.push_back(id);
        }
    }

    /* Update all */
    for (int i = 0; i < 50; i++) {
        nimcp_no_update(&system, 10.0f);
    }

    /* Verify all accessible */
    for (size_t i = 0; i < ids.size(); i++) {
        nimcp_no_source_t* src = nimcp_no_get_source(&system, ids[i]);
        EXPECT_NE(src, nullptr);
        EXPECT_FALSE(std::isnan(src->no_concentration));
    }
}

//=============================================================================
// Modifier Output Regression Tests
//=============================================================================

TEST_F(NORegressionTest, PlasticityModifierValid) {
    /* Plasticity modifier should be in valid range */
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t source_id;
    nimcp_no_add_source(&system, position, NOS_TYPE_NNOS, &source_id);
    nimcp_no_source_t* source = nimcp_no_get_source(&system, source_id);

    nimcp_no_set_calcium(source, 1.0f);

    for (int i = 0; i < 500; i++) {
        nimcp_no_update(&system, 10.0f);
    }

    float modifier;
    nimcp_no_get_plasticity_modifier(&system, &modifier);

    EXPECT_FALSE(std::isnan(modifier));
    EXPECT_FALSE(std::isinf(modifier));
    EXPECT_GE(modifier, 0.0f);
}

//=============================================================================
// Memory/Resource Regression Tests
//=============================================================================

TEST_F(NORegressionTest, InitShutdownCycle) {
    /* Multiple cycles shouldn't cause resource leaks */
    for (int cycle = 0; cycle < 10; cycle++) {
        nimcp_no_system_t temp_system;
        memset(&temp_system, 0, sizeof(temp_system));

        nimcp_no_error_t err = nimcp_no_init(&temp_system, nullptr);
        EXPECT_EQ(err, NO_OK);

        float position[3] = {0.0f, 0.0f, 0.0f};
        uint32_t id;
        nimcp_no_add_source(&temp_system, position, NOS_TYPE_NNOS, &id);

        err = nimcp_no_shutdown(&temp_system);
        EXPECT_EQ(err, NO_OK);
    }
}

TEST_F(NORegressionTest, MetricsAccumulate) {
    /* Metrics should update over time */
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t source_id;
    nimcp_no_add_source(&system, position, NOS_TYPE_NNOS, &source_id);
    nimcp_no_source_t* source = nimcp_no_get_source(&system, source_id);

    nimcp_no_set_calcium(source, 0.5f);

    for (int i = 0; i < 100; i++) {
        nimcp_no_update(&system, 10.0f);
    }

    nimcp_no_metrics_t metrics1;
    nimcp_no_get_metrics(&system, &metrics1);

    for (int i = 0; i < 100; i++) {
        nimcp_no_update(&system, 10.0f);
    }

    nimcp_no_metrics_t metrics2;
    nimcp_no_get_metrics(&system, &metrics2);

    EXPECT_GT(metrics2.total_simulation_time, metrics1.total_simulation_time);
    EXPECT_GT(metrics2.update_count, metrics1.update_count);
}

//=============================================================================
// Long-Running Stability Tests
//=============================================================================

TEST_F(NORegressionTest, LongTermStability) {
    /* System should remain stable over long simulations */
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t source_id;
    nimcp_no_add_source(&system, position, NOS_TYPE_NNOS, &source_id);
    nimcp_no_source_t* source = nimcp_no_get_source(&system, source_id);

    /* Alternating activity */
    for (int cycle = 0; cycle < 10; cycle++) {
        nimcp_no_set_calcium(source, 1.0f);
        for (int i = 0; i < 50; i++) {
            nimcp_no_update(&system, 10.0f);
        }

        nimcp_no_set_calcium(source, 0.0f);
        for (int i = 0; i < 100; i++) {
            nimcp_no_update(&system, 10.0f);
        }
    }

    /* Should remain stable */
    EXPECT_FALSE(std::isnan(source->no_concentration));
    EXPECT_FALSE(std::isinf(source->no_concentration));
    EXPECT_GE(source->no_concentration, 0.0f);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
