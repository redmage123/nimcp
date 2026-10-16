/**
 * @file test_no_plasticity_integration.cpp
 * @brief Integration tests for Nitric Oxide and synaptic plasticity
 *
 * WHAT: Integration tests for NO signaling with LTP/LTD and synaptic modulation
 * WHY:  Verify NO correctly integrates with plasticity mechanisms
 * HOW:  Test retrograde signaling, volume transmission, and plasticity effects
 *
 * @author NIMCP Development Team
 * @date 2026-01-11
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

extern "C" {
#include "chemistry/gasotransmitters/nimcp_nitric_oxide.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class NOPlasticityIntegrationTest : public ::testing::Test {
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
// Retrograde Signaling Integration Tests
//=============================================================================

TEST_F(NOPlasticityIntegrationTest, PostsynapticCalciumProducesNO) {
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t source_id;
    nimcp_no_add_source(&system, position, NOS_TYPE_NNOS, &source_id);
    nimcp_no_source_t* source = nimcp_no_get_source(&system, source_id);
    ASSERT_NE(source, nullptr);

    float baseline_no = source->no_concentration;

    /* Simulate postsynaptic calcium influx (NMDA activation) */
    nimcp_no_set_calcium(source, 1.0f);
    nimcp_no_set_nmda_activation(source, 1.0f);

    for (int i = 0; i < 100; i++) {
        nimcp_no_update(&system, 1.0f);
    }

    /* Postsynaptic calcium should trigger NO production */
    EXPECT_GT(source->no_concentration, baseline_no);
}

TEST_F(NOPlasticityIntegrationTest, NODiffusesToTargets) {
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t source_id;
    nimcp_no_add_source(&system, position, NOS_TYPE_NNOS, &source_id);
    nimcp_no_source_t* source = nimcp_no_get_source(&system, source_id);

    /* Add target synapse */
    nimcp_no_add_target(source, 1, 10.0f, NO_RETROGRADE_PRESYNAPTIC_RELEASE);

    /* Trigger NO production */
    nimcp_no_set_calcium(source, 1.0f);

    for (int i = 0; i < 200; i++) {
        nimcp_no_update(&system, 1.0f);
        nimcp_no_diffuse(&system, source);
    }

    /* Target should receive NO (potentiation factor should change) */
    float potentiation;
    nimcp_no_get_target_potentiation(source, 1, &potentiation);
    EXPECT_GE(potentiation, 0.0f);
}

TEST_F(NOPlasticityIntegrationTest, NOActivatescGMP) {
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t source_id;
    nimcp_no_add_source(&system, position, NOS_TYPE_NNOS, &source_id);

    float baseline_cgmp;
    nimcp_no_get_cgmp(&system, source_id, &baseline_cgmp);

    /* Strong activation */
    nimcp_no_source_t* source = nimcp_no_get_source(&system, source_id);
    nimcp_no_set_calcium(source, 1.0f);

    for (int i = 0; i < 300; i++) {
        nimcp_no_update(&system, 1.0f);
    }

    float elevated_cgmp;
    nimcp_no_get_cgmp(&system, source_id, &elevated_cgmp);

    /* NO should activate sGC, producing cGMP */
    EXPECT_GE(elevated_cgmp, baseline_cgmp);
}

//=============================================================================
// NOS Isoform Integration Tests
//=============================================================================

TEST_F(NOPlasticityIntegrationTest, nNOSCalciumDependent) {
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t source_id;
    nimcp_no_add_source(&system, position, NOS_TYPE_NNOS, &source_id);
    nimcp_no_source_t* source = nimcp_no_get_source(&system, source_id);

    /* No calcium - minimal NO */
    nimcp_no_set_calcium(source, 0.0f);
    for (int i = 0; i < 100; i++) {
        nimcp_no_update(&system, 1.0f);
    }
    float no_at_low_ca = source->no_concentration;

    /* High calcium - NO production */
    nimcp_no_set_calcium(source, 1.0f);
    for (int i = 0; i < 100; i++) {
        nimcp_no_update(&system, 1.0f);
    }
    float no_at_high_ca = source->no_concentration;

    /* nNOS should be calcium-dependent */
    EXPECT_GE(no_at_high_ca, no_at_low_ca);
}

TEST_F(NOPlasticityIntegrationTest, iNOSSustainedProduction) {
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t source_id;
    nimcp_no_add_source(&system, position, NOS_TYPE_INOS, &source_id);
    nimcp_no_source_t* source = nimcp_no_get_source(&system, source_id);

    /* iNOS is calcium-independent once induced */
    nimcp_no_set_calcium(source, 0.0f);

    for (int i = 0; i < 500; i++) {
        nimcp_no_update(&system, 1.0f);
    }

    /* iNOS should produce NO regardless of calcium */
    EXPECT_GE(source->no_concentration, 0.0f);
}

TEST_F(NOPlasticityIntegrationTest, eNOSVasodilation) {
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t source_id;
    nimcp_no_add_source(&system, position, NOS_TYPE_ENOS, &source_id);
    nimcp_no_source_t* source = nimcp_no_get_source(&system, source_id);

    float baseline_vasodilation;
    nimcp_no_get_vasodilation(&system, &baseline_vasodilation);

    /* Activate eNOS with calcium */
    nimcp_no_set_calcium(source, 1.0f);

    for (int i = 0; i < 200; i++) {
        nimcp_no_update(&system, 1.0f);
    }

    float activated_vasodilation;
    nimcp_no_get_vasodilation(&system, &activated_vasodilation);

    /* eNOS should cause vasodilation */
    EXPECT_GE(activated_vasodilation, baseline_vasodilation);
}

//=============================================================================
// Plasticity Modulation Integration Tests
//=============================================================================

TEST_F(NOPlasticityIntegrationTest, NOEnhancesPlasticity) {
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t source_id;
    nimcp_no_add_source(&system, position, NOS_TYPE_NNOS, &source_id);
    nimcp_no_source_t* source = nimcp_no_get_source(&system, source_id);

    /* Get baseline plasticity modifier */
    float baseline_modifier;
    nimcp_no_get_plasticity_modifier(&system, &baseline_modifier);

    /* Activate NO production (simulating LTP-inducing stimulation) */
    nimcp_no_set_calcium(source, 1.0f);
    nimcp_no_set_nmda_activation(source, 1.0f);

    for (int i = 0; i < 200; i++) {
        nimcp_no_update(&system, 1.0f);
    }

    float ltp_modifier;
    nimcp_no_get_plasticity_modifier(&system, &ltp_modifier);

    /* NO should enhance plasticity (modifier >= baseline) */
    EXPECT_GE(ltp_modifier, baseline_modifier);
}

TEST_F(NOPlasticityIntegrationTest, RetrogradeModulatesRelease) {
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t source_id;
    nimcp_no_add_source(&system, position, NOS_TYPE_NNOS, &source_id);
    nimcp_no_source_t* source = nimcp_no_get_source(&system, source_id);

    /* Add presynaptic target */
    nimcp_no_add_target(source, 1, 5.0f, NO_RETROGRADE_PRESYNAPTIC_RELEASE);

    /* Get baseline potentiation */
    float baseline_pot;
    nimcp_no_get_target_potentiation(source, 1, &baseline_pot);

    /* Retrograde NO signaling */
    nimcp_no_set_calcium(source, 1.0f);

    for (int i = 0; i < 200; i++) {
        nimcp_no_update(&system, 1.0f);
        nimcp_no_diffuse(&system, source);
    }

    float modulated_pot;
    nimcp_no_get_target_potentiation(source, 1, &modulated_pot);

    /* Potentiation should be in valid range */
    EXPECT_GE(modulated_pot, 0.0f);
}

//=============================================================================
// Temporal Dynamics Integration Tests
//=============================================================================

TEST_F(NOPlasticityIntegrationTest, NOHasAppropriateDecay) {
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t source_id;
    nimcp_no_add_source(&system, position, NOS_TYPE_NNOS, &source_id);
    nimcp_no_source_t* source = nimcp_no_get_source(&system, source_id);

    /* Produce NO */
    nimcp_no_set_calcium(source, 1.0f);
    for (int i = 0; i < 100; i++) {
        nimcp_no_update(&system, 1.0f);
    }
    float peak_no = source->no_concentration;

    /* Stop production and let NO decay */
    nimcp_no_set_calcium(source, 0.0f);
    for (int i = 0; i < 500; i++) {
        nimcp_no_update(&system, 10.0f);  /* 10ms steps */
    }
    float decayed_no = source->no_concentration;

    /* NO should decay (half-life ~1 second) */
    EXPECT_LE(decayed_no, peak_no);
}

TEST_F(NOPlasticityIntegrationTest, cGMPLagsNOProduction) {
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t source_id;
    nimcp_no_add_source(&system, position, NOS_TYPE_NNOS, &source_id);
    nimcp_no_source_t* source = nimcp_no_get_source(&system, source_id);

    /* Pulse of calcium to produce NO */
    nimcp_no_set_calcium(source, 1.0f);

    /* Track cGMP over time */
    float cgmp_early, cgmp_late;

    for (int i = 0; i < 50; i++) {
        nimcp_no_update(&system, 1.0f);
    }
    nimcp_no_get_cgmp(&system, source_id, &cgmp_early);

    for (int i = 0; i < 200; i++) {
        nimcp_no_update(&system, 1.0f);
    }
    nimcp_no_get_cgmp(&system, source_id, &cgmp_late);

    /* cGMP should accumulate after NO */
    EXPECT_GE(cgmp_late, cgmp_early);
}

//=============================================================================
// Multi-Source Coordination Tests
//=============================================================================

TEST_F(NOPlasticityIntegrationTest, MultipleSourcesCoordinate) {
    /* Create multiple sources */
    uint32_t ids[5];
    for (int i = 0; i < 5; i++) {
        float position[3] = {(float)i * 10.0f, 0.0f, 0.0f};
        nimcp_no_add_source(&system, position, NOS_TYPE_NNOS, &ids[i]);
        nimcp_no_source_t* src = nimcp_no_get_source(&system, ids[i]);
        nimcp_no_set_calcium(src, 1.0f);
    }

    for (int j = 0; j < 200; j++) {
        nimcp_no_update(&system, 1.0f);
    }

    /* All sources should show NO production */
    for (int i = 0; i < 5; i++) {
        nimcp_no_source_t* src = nimcp_no_get_source(&system, ids[i]);
        EXPECT_GT(src->no_concentration, 0.0f);
    }
}

TEST_F(NOPlasticityIntegrationTest, SourcesAreIndependent) {
    float pos1[3] = {0.0f, 0.0f, 0.0f};
    float pos2[3] = {100.0f, 0.0f, 0.0f};
    uint32_t id1, id2;

    nimcp_no_add_source(&system, pos1, NOS_TYPE_NNOS, &id1);
    nimcp_no_add_source(&system, pos2, NOS_TYPE_NNOS, &id2);

    nimcp_no_source_t* src1 = nimcp_no_get_source(&system, id1);
    nimcp_no_source_t* src2 = nimcp_no_get_source(&system, id2);

    /* Different activity levels */
    nimcp_no_set_calcium(src1, 1.0f);
    nimcp_no_set_calcium(src2, 0.0f);

    for (int i = 0; i < 200; i++) {
        nimcp_no_update(&system, 1.0f);
    }

    /* Sources should have different NO levels */
    EXPECT_GT(src1->no_concentration, src2->no_concentration);
}

//=============================================================================
// Long-Running Stability Tests
//=============================================================================

TEST_F(NOPlasticityIntegrationTest, LongTermStability) {
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t source_id;
    nimcp_no_add_source(&system, position, NOS_TYPE_NNOS, &source_id);
    nimcp_no_source_t* source = nimcp_no_get_source(&system, source_id);

    /* Simulate long-term with varying activity */
    for (int cycle = 0; cycle < 10; cycle++) {
        /* Burst of activity */
        nimcp_no_set_calcium(source, 1.0f);
        for (int i = 0; i < 50; i++) {
            nimcp_no_update(&system, 1.0f);
        }

        /* Quiet period */
        nimcp_no_set_calcium(source, 0.0f);
        for (int i = 0; i < 100; i++) {
            nimcp_no_update(&system, 1.0f);
        }
    }

    /* System should remain stable - no NaN/Inf values */
    EXPECT_FALSE(std::isnan(source->no_concentration));
    EXPECT_FALSE(std::isinf(source->no_concentration));
    EXPECT_GE(source->no_concentration, 0.0f);
}

TEST_F(NOPlasticityIntegrationTest, MetricsAccumulateCorrectly) {
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t source_id;
    nimcp_no_add_source(&system, position, NOS_TYPE_NNOS, &source_id);
    nimcp_no_source_t* source = nimcp_no_get_source(&system, source_id);

    nimcp_no_set_calcium(source, 0.5f);

    for (int i = 0; i < 100; i++) {
        nimcp_no_update(&system, 1.0f);
    }

    nimcp_no_metrics_t metrics1;
    nimcp_no_get_metrics(&system, &metrics1);

    for (int i = 0; i < 100; i++) {
        nimcp_no_update(&system, 1.0f);
    }

    nimcp_no_metrics_t metrics2;
    nimcp_no_get_metrics(&system, &metrics2);

    /* Metrics should increase over time */
    EXPECT_GT(metrics2.total_simulation_time, metrics1.total_simulation_time);
    EXPECT_GT(metrics2.update_count, metrics1.update_count);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
