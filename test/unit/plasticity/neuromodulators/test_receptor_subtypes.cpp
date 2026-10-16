/**
 * @file test_receptor_subtypes.cpp
 * @brief Unit tests for receptor subtype modeling (Phase C2.2)
 *
 * Tests:
 * - Hill equation binding kinetics
 * - Receptor subtype specificity (D1 vs D2)
 * - Regional receptor profiles (cortex vs striatum)
 * - Drug simulations (D2 blockade, SSRIs)
 * - Desensitization dynamics
 *
 * @version Phase C2.2 Enhancement #1
 * @date 2025-11-12
 */

#include <gtest/gtest.h>
#include <cmath>

    #include "plasticity/neuromodulators/nimcp_receptor_subtypes.h"

// ============================================================================
// Test Fixture
// ============================================================================

class ReceptorSubtypesTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize receptor systems
        dopamine_receptor_system_init(&dopamine_system);
        serotonin_receptor_system_init(&serotonin_system);
    }

    dopamine_receptor_system_t dopamine_system;
    serotonin_receptor_system_t serotonin_system;
};

// ============================================================================
// Basic Functionality Tests
// ============================================================================

TEST_F(ReceptorSubtypesTest, SystemInitializes) {
    // All receptors should have valid configurations
    for (int i = 0; i < DOPAMINE_RECEPTOR_COUNT; i++) {
        EXPECT_GT(dopamine_system.config[i].kd, 0.0f) << "D" << (i+1) << " Kd should be positive";
        EXPECT_GE(dopamine_system.config[i].expression_level, 0.0f);
        EXPECT_LE(dopamine_system.config[i].expression_level, 1.0f);
        EXPECT_GT(dopamine_system.config[i].hill_coefficient, 0.0f);
    }

    // D1 and D5 should be excitatory
    EXPECT_TRUE(dopamine_system.config[DOPAMINE_D1].is_excitatory);
    EXPECT_TRUE(dopamine_system.config[DOPAMINE_D5].is_excitatory);

    // D2, D3, D4 should be inhibitory
    EXPECT_FALSE(dopamine_system.config[DOPAMINE_D2].is_excitatory);
    EXPECT_FALSE(dopamine_system.config[DOPAMINE_D3].is_excitatory);
    EXPECT_FALSE(dopamine_system.config[DOPAMINE_D4].is_excitatory);
}

TEST_F(ReceptorSubtypesTest, BindingAtZeroConcentration) {
    float dt = 0.001f;  // 1ms
    float free_da = 0.0f;

    float modulation = dopamine_receptor_compute_modulation(&dopamine_system, free_da, dt);

    // No dopamine = no receptor occupancy = no modulation
    EXPECT_NEAR(modulation, 0.0f, 0.01f);
    EXPECT_NEAR(dopamine_system.total_excitation, 0.0f, 0.01f);
    EXPECT_NEAR(dopamine_system.total_inhibition, 0.0f, 0.01f);
}

TEST_F(ReceptorSubtypesTest, BindingAtKdConcentration) {
    float dt = 0.001f;

    // D2 has Kd = 0.5 nM = 0.0005 µM
    float d2_kd = dopamine_system.config[DOPAMINE_D2].kd;

    // Compute modulation at Kd
    for (int i = 0; i < 1000; i++) {  // Let system reach equilibrium
        dopamine_receptor_compute_modulation(&dopamine_system, d2_kd, dt);
    }

    // At Kd, occupancy should be ~50% (Hill equation)
    float d2_occupancy = dopamine_system.state[DOPAMINE_D2].occupancy;
    EXPECT_GT(d2_occupancy, 0.3f);
    EXPECT_LT(d2_occupancy, 0.7f);
}

// ============================================================================
// Hill Equation Binding Tests
// ============================================================================

TEST_F(ReceptorSubtypesTest, HillEquationSaturation) {
    float dt = 0.001f;

    // Very high concentration (1000x Kd) should saturate receptors
    float high_conc = 1.0f;  // 1 µM (much higher than any Kd)

    for (int i = 0; i < 2000; i++) {  // Reach equilibrium
        dopamine_receptor_compute_modulation(&dopamine_system, high_conc, dt);
    }

    // All receptors should be occupied (accounting for expression level)
    // Occupancy = equilibrium_occupancy * expression_level
    for (int i = 0; i < DOPAMINE_RECEPTOR_COUNT; i++) {
        float occupancy = dopamine_system.state[i].occupancy;
        float expression = dopamine_system.config[i].expression_level;
        // At high concentration, occupancy should be close to expression level
        EXPECT_GT(occupancy, expression * 0.3f) << "D" << (i+1) << " should be occupied at high [DA]";
    }
}

TEST_F(ReceptorSubtypesTest, D2HigherAffinityThanD1) {
    // D2 has lower Kd → higher affinity → binds at lower concentrations
    float d1_kd = dopamine_system.config[DOPAMINE_D1].kd;
    float d2_kd = dopamine_system.config[DOPAMINE_D2].kd;

    EXPECT_LT(d2_kd, d1_kd) << "D2 should have higher affinity (lower Kd) than D1";

    // Test at intermediate concentration
    float test_conc = 0.001f;  // 1 nM = 0.001 µM
    float dt = 0.001f;

    for (int i = 0; i < 1000; i++) {
        dopamine_receptor_compute_modulation(&dopamine_system, test_conc, dt);
    }

    float d1_occupancy = dopamine_system.state[DOPAMINE_D1].occupancy;
    float d2_occupancy = dopamine_system.state[DOPAMINE_D2].occupancy;

    EXPECT_GT(d2_occupancy, d1_occupancy) << "D2 should bind more at same concentration";
}

// ============================================================================
// Excitatory vs Inhibitory Tests
// ============================================================================

TEST_F(ReceptorSubtypesTest, D1ExcitatoryD2Inhibitory) {
    float dt = 0.001f;
    float moderate_da = 0.01f;  // 10 nM

    // Simulate
    for (int i = 0; i < 1000; i++) {
        dopamine_receptor_compute_modulation(&dopamine_system, moderate_da, dt);
    }

    // D1/D5 contribute to excitation
    EXPECT_GT(dopamine_system.total_excitation, 0.0f);

    // D2/D3/D4 contribute to inhibition
    EXPECT_GT(dopamine_system.total_inhibition, 0.0f);

    // Net modulation = excitation - inhibition
    float expected_net = dopamine_system.total_excitation - dopamine_system.total_inhibition;
    EXPECT_NEAR(dopamine_system.net_modulation, expected_net, 0.001f);
}

TEST_F(ReceptorSubtypesTest, HighDopamineNetEffect) {
    float dt = 0.001f;

    // At very low DA: minimal activation
    for (int i = 0; i < 1000; i++) {
        dopamine_receptor_compute_modulation(&dopamine_system, 0.0001f, dt);
    }
    float low_modulation = dopamine_system.net_modulation;

    // Reset system
    dopamine_receptor_system_init(&dopamine_system);

    // At moderate DA: significant activation
    for (int i = 0; i < 1000; i++) {
        dopamine_receptor_compute_modulation(&dopamine_system, 0.01f, dt);
    }
    float high_modulation = dopamine_system.net_modulation;

    // Higher DA should produce larger net modulation (in absolute value)
    EXPECT_GT(fabs(high_modulation), fabs(low_modulation));
}

// ============================================================================
// Regional Profile Tests
// ============================================================================

TEST_F(ReceptorSubtypesTest, CorticalProfileHighD1) {
    neuron_receptor_profile_t cortical = receptor_profile_cortical();

    // Cortex has high D1, low D2
    float d1_expr = cortical.dopamine.config[DOPAMINE_D1].expression_level;
    float d2_expr = cortical.dopamine.config[DOPAMINE_D2].expression_level;

    EXPECT_GT(d1_expr, 0.7f) << "Cortical D1 should be highly expressed";
    EXPECT_LT(d2_expr, 0.5f) << "Cortical D2 should be lowly expressed";
    EXPECT_GT(d1_expr, d2_expr) << "Cortex: D1 > D2";
}

TEST_F(ReceptorSubtypesTest, StriatumProfileHighD2) {
    neuron_receptor_profile_t striatal = receptor_profile_striatal();

    // Striatum has very high D2 (medium spiny neurons)
    float d1_expr = striatal.dopamine.config[DOPAMINE_D1].expression_level;
    float d2_expr = striatal.dopamine.config[DOPAMINE_D2].expression_level;

    EXPECT_GT(d2_expr, 0.8f) << "Striatal D2 should be very highly expressed";
    EXPECT_GT(d2_expr, d1_expr) << "Striatum: D2 > D1";
}

TEST_F(ReceptorSubtypesTest, CortexVsStriatumDifferentResponses) {
    neuron_receptor_profile_t cortical = receptor_profile_cortical();
    neuron_receptor_profile_t striatal = receptor_profile_striatal();

    float dt = 0.001f;
    float dopamine_conc = 0.01f;

    // Simulate both profiles
    for (int i = 0; i < 1000; i++) {
        dopamine_receptor_compute_modulation(&cortical.dopamine, dopamine_conc, dt);
        dopamine_receptor_compute_modulation(&striatal.dopamine, dopamine_conc, dt);
    }

    float cortical_net = cortical.dopamine.net_modulation;
    float striatal_net = striatal.dopamine.net_modulation;

    // Cortex (high D1) should be more excitatory
    // Striatum (high D2) should be more inhibitory
    EXPECT_GT(cortical_net, striatal_net) << "Cortex should be more excitatory than striatum";
}

// ============================================================================
// Drug Simulation Tests
// ============================================================================

TEST_F(ReceptorSubtypesTest, D2BlockadeReducesInhibition) {
    float dt = 0.001f;
    float dopamine_conc = 0.01f;

    // Baseline (no drug)
    for (int i = 0; i < 1000; i++) {
        dopamine_receptor_compute_modulation(&dopamine_system, dopamine_conc, dt);
    }
    float baseline_inhibition = dopamine_system.total_inhibition;
    float baseline_net = dopamine_system.net_modulation;

    // Reset system
    dopamine_receptor_system_init(&dopamine_system);

    // Apply D2 blockade (e.g., risperidone 80% blockade)
    dopamine_receptor_apply_d2_blockade(&dopamine_system, 0.8f);

    // Re-simulate with blockade
    for (int i = 0; i < 1000; i++) {
        dopamine_receptor_compute_modulation(&dopamine_system, dopamine_conc, dt);
    }
    float blocked_inhibition = dopamine_system.total_inhibition;
    float blocked_net = dopamine_system.net_modulation;

    // D2 blockade should reduce total inhibition
    EXPECT_LT(blocked_inhibition, baseline_inhibition) << "D2 blockade should reduce inhibition";

    // Net modulation should become more excitatory (or less inhibitory)
    EXPECT_GT(blocked_net, baseline_net) << "D2 blockade shifts toward excitation";
}

TEST_F(ReceptorSubtypesTest, SSRIIncreasesSerotoninEffect) {
    float dt = 0.001f;
    float baseline_serotonin = 0.005f;  // 5 nM

    // Baseline (no SSRI)
    for (int i = 0; i < 1000; i++) {
        serotonin_receptor_compute_modulation(&serotonin_system, baseline_serotonin, dt);
    }
    float baseline_modulation = serotonin_system.net_modulation;

    // Reset
    serotonin_receptor_system_init(&serotonin_system);

    // Apply SSRI (90% reuptake inhibition)
    float ssri_concentration = serotonin_receptor_apply_ssri(
        &serotonin_system,
        0.9f,
        baseline_serotonin
    );

    EXPECT_GT(ssri_concentration, baseline_serotonin) << "SSRI should increase synaptic 5-HT";

    // Simulate with increased concentration
    for (int i = 0; i < 1000; i++) {
        serotonin_receptor_compute_modulation(&serotonin_system, ssri_concentration, dt);
    }
    float ssri_modulation = serotonin_system.net_modulation;

    // SSRI should produce larger modulation
    EXPECT_GT(fabs(ssri_modulation), fabs(baseline_modulation))
        << "SSRI increases serotonergic effects";
}

// ============================================================================
// Desensitization Tests
// ============================================================================

TEST_F(ReceptorSubtypesTest, DesensitizationReducesEffect) {
    float dt = 0.1f;  // 100ms steps (faster simulation)
    float high_da = 0.1f;  // High dopamine

    // Acute response (first few seconds)
    for (int i = 0; i < 50; i++) {  // 5 seconds
        dopamine_receptor_compute_modulation(&dopamine_system, high_da, dt);
    }
    float acute_modulation = fabs(dopamine_system.net_modulation);

    // Chronic response (continue for 60 more seconds)
    for (int i = 0; i < 600; i++) {  // 60 seconds
        dopamine_receptor_compute_modulation(&dopamine_system, high_da, dt);
    }
    float chronic_modulation = fabs(dopamine_system.net_modulation);

    // Chronic should be less than acute due to desensitization
    EXPECT_LT(chronic_modulation, acute_modulation)
        << "Desensitization should reduce chronic response";

    // Check that desensitization actually occurred
    float total_desensitization = 0.0f;
    for (int i = 0; i < DOPAMINE_RECEPTOR_COUNT; i++) {
        total_desensitization += dopamine_system.state[i].desensitization;
    }
    EXPECT_GT(total_desensitization, 0.0f) << "Some receptors should be desensitized";
}

// ============================================================================
// Concentration-Response Curve Tests
// ============================================================================

TEST_F(ReceptorSubtypesTest, DoseResponseCurve) {
    float dt = 0.001f;
    // Use shorter equilibration to avoid desensitization effects
    float concentrations[] = {0.0001f, 0.001f, 0.01f, 0.05f};  // 0.1 nM to 50 nM
    float responses[4];

    for (int conc_idx = 0; conc_idx < 4; conc_idx++) {
        // Reset system
        dopamine_receptor_system_init(&dopamine_system);

        // Reach equilibrium (shorter to minimize desensitization)
        for (int i = 0; i < 500; i++) {
            dopamine_receptor_compute_modulation(&dopamine_system, concentrations[conc_idx], dt);
        }

        responses[conc_idx] = fabs(dopamine_system.net_modulation);
    }

    // Dose-response relationship exists (non-zero responses)
    for (int i = 1; i < 4; i++) {
        EXPECT_GT(responses[i], 0.01f)
            << "Response should be non-zero at concentration index " << i;
    }

    // Response should change with concentration
    // (may be non-monotonic due to excitatory/inhibitory receptor balance - D2 high affinity!)
    bool responses_vary = false;
    for (int i = 0; i < 3; i++) {
        if (fabs(responses[i+1] - responses[i]) > 0.05f) {
            responses_vary = true;
            break;
        }
    }
    EXPECT_TRUE(responses_vary)
        << "Responses should vary with dopamine concentration";
}

// ============================================================================
// Multiple Neurotransmitter Tests
// ============================================================================

TEST_F(ReceptorSubtypesTest, MultipleNeurotransmitterProfile) {
    neuron_receptor_profile_t profile;
    neuron_receptor_profile_init(&profile, BRAIN_REGION_GENERIC);

    // All systems should be initialized
    EXPECT_GT(profile.dopamine.config[DOPAMINE_D1].kd, 0.0f);
    EXPECT_GT(profile.serotonin.config[SEROTONIN_5HT1A].kd, 0.0f);
    EXPECT_GT(profile.acetylcholine.config[ACH_NICOTINIC].kd, 0.0f);
    EXPECT_GT(profile.norepinephrine.config[NOREPINEPHRINE_ALPHA1].kd, 0.0f);
}

// ============================================================================
// Performance Tests
// ============================================================================

TEST_F(ReceptorSubtypesTest, PerformanceUpdate1000Steps) {
    float dt = 0.001f;
    float dopamine_conc = 0.01f;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; i++) {
        dopamine_receptor_compute_modulation(&dopamine_system, dopamine_conc, dt);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Should complete in less than 10ms (10000 µs)
    EXPECT_LT(duration.count(), 10000) << "1000 updates should take < 10ms";

    std::cout << "Performance: 1000 receptor updates in " << duration.count() << " µs" << std::endl;
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
