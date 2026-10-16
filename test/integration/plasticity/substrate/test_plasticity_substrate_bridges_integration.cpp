/**
 * @file test_plasticity_substrate_bridges_integration.cpp
 * @brief Integration tests for plasticity and neuromodulator substrate bridges
 *
 * WHAT: Test multi-module coordination between substrate and plasticity/neuromodulator systems
 * WHY:  Verify bridges work together correctly in realistic scenarios
 * HOW:  Simulate substrate state changes and verify plasticity/neuromodulator modulation
 *
 * TEST CATEGORIES:
 * - Multi-module coordination (substrate affects multiple plasticity mechanisms)
 * - Cross-effects (plasticity and neuromodulator interactions)
 * - Bio-async messaging between bridges
 * - Metabolic feedback loops
 * - Temperature and ATP co-modulation
 *
 * @author NIMCP Development Team
 * @date 2025-12-19
 */

#include <gtest/gtest.h>
#include <cmath>
#include <thread>
#include <chrono>

#include "plasticity/nimcp_plasticity_substrate_bridge.h"
#include "plasticity/neuromodulators/nimcp_neuromod_substrate_bridge.h"
#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"
#include "async/nimcp_bio_router.h"

//=============================================================================
// Test Fixture
//=============================================================================

class PlasticitySubstrateBridgesIntegrationTest : public ::testing::Test {
protected:
    neural_substrate_t* substrate = nullptr;
    plasticity_substrate_bridge_t* plasticity_bridge = nullptr;
    neuromod_substrate_bridge_t* neuromod_bridge = nullptr;
    neuromodulator_system_t neuromod_system;

    void SetUp() override {
        // Create substrate
        substrate_config_t sub_config = {};
        substrate_config_init(&sub_config);
        sub_config.initial_atp_level = 0.8f;
        sub_config.initial_temperature = 37.0f;
        sub_config.initial_ca_homeostasis = 0.8f;
        sub_config.initial_ion_balance = 0.9f;
        substrate = substrate_create(&sub_config);
        ASSERT_NE(substrate, nullptr);

        // Create neuromodulator system
        neuromodulator_config_t nm_config;
        neuromodulator_default_config(&nm_config);
        neuromod_system = neuromodulator_system_create(&nm_config);
        ASSERT_NE(neuromod_system, nullptr);

        // Create plasticity bridge
        plasticity_substrate_config_t plast_config;
        plasticity_substrate_default_config(&plast_config);
        plast_config.enable_stdp_modulation = true;
        plast_config.enable_bcm_modulation = true;
        plast_config.enable_eligibility_modulation = true;
        plasticity_bridge = plasticity_substrate_bridge_create(&plast_config, substrate);
        ASSERT_NE(plasticity_bridge, nullptr);

        // Create neuromodulator bridge
        neuromod_substrate_config_t nm_sub_config;
        neuromod_substrate_default_config(&nm_sub_config);
        nm_sub_config.enable_atp_synthesis_modulation = true;
        nm_sub_config.enable_calcium_release_modulation = true;
        nm_sub_config.enable_temperature_modulation = true;
        neuromod_bridge = neuromod_substrate_bridge_create(&nm_sub_config, substrate, neuromod_system);
        ASSERT_NE(neuromod_bridge, nullptr);
    }

    void TearDown() override {
        neuromod_substrate_bridge_destroy(neuromod_bridge);
        plasticity_substrate_bridge_destroy(plasticity_bridge);
        neuromodulator_system_destroy(neuromod_system);
        substrate_destroy(substrate);
    }

    void set_substrate_state(float atp, float temp, float ca, float ion) {
        substrate_set_atp_level(substrate, atp);
        substrate_set_temperature(substrate, temp);
        substrate_set_ca_homeostasis(substrate, ca);
        substrate_set_ion_balance(substrate, ion);
    }
};

//=============================================================================
// Multi-Module Coordination Tests
//=============================================================================

TEST_F(PlasticitySubstrateBridgesIntegrationTest, LowATPAffectsBothBridges) {
    // WHAT: Test that low ATP affects both plasticity and neuromodulation
    // WHY:  Verify coordinated substrate effects

    // Set low ATP
    set_substrate_state(0.3f, 37.0f, 0.8f, 0.9f);

    // Update both bridges
    ASSERT_EQ(plasticity_substrate_update_all(plasticity_bridge), 0);
    ASSERT_EQ(neuromod_substrate_update_effects(neuromod_bridge), 0);

    // Check plasticity effects
    float plast_lr = plasticity_substrate_get_learning_rate_mod(plasticity_bridge);
    EXPECT_LT(plast_lr, 0.5f); // Learning rate reduced

    // Check neuromodulator effects
    float nm_synthesis = neuromod_substrate_get_synthesis_mod(neuromod_bridge, NEUROMOD_BRIDGE_DOPAMINE);
    EXPECT_LT(nm_synthesis, 0.6f); // Synthesis impaired

    // Both should be impaired
    EXPECT_TRUE(plasticity_substrate_is_limited(plasticity_bridge));
    EXPECT_TRUE(neuromod_substrate_is_limited(neuromod_bridge, NEUROMOD_BRIDGE_DOPAMINE));
}

TEST_F(PlasticitySubstrateBridgesIntegrationTest, TemperatureAffectsBothBridges) {
    // WHAT: Test temperature effects on both bridges
    // WHY:  Verify Q10 scaling coordination

    // Set high temperature
    set_substrate_state(0.8f, 39.0f, 0.8f, 0.9f);

    ASSERT_EQ(plasticity_substrate_update_all(plasticity_bridge), 0);
    ASSERT_EQ(neuromod_substrate_update_effects(neuromod_bridge), 0);

    // Check STDP window modulation (should increase)
    float stdp_window = plasticity_substrate_get_stdp_window_mod(plasticity_bridge);
    EXPECT_GT(stdp_window, 1.0f); // Window wider at higher temp

    // Check neuromodulator synthesis (should increase)
    substrate_neuromod_effects_t nm_effects;
    ASSERT_EQ(neuromod_substrate_get_effects(neuromod_bridge, NEUROMOD_BRIDGE_DOPAMINE, &nm_effects), 0);
    EXPECT_GT(nm_effects.temp_synthesis_factor, 1.0f);
}

TEST_F(PlasticitySubstrateBridgesIntegrationTest, CalciumAffectsNeurommodulatorNotPlasticity) {
    // WHAT: Test calcium primarily affects neuromodulator release
    // WHY:  Verify domain-specific effects

    // Set low calcium
    set_substrate_state(0.8f, 37.0f, 0.4f, 0.9f);

    ASSERT_EQ(plasticity_substrate_update_all(plasticity_bridge), 0);
    ASSERT_EQ(neuromod_substrate_update_effects(neuromod_bridge), 0);

    // Plasticity should be mostly unaffected
    float plast_lr = plasticity_substrate_get_learning_rate_mod(plasticity_bridge);
    EXPECT_GT(plast_lr, 0.7f);

    // Neuromodulator release should be impaired
    float nm_release = neuromod_substrate_get_release_mod(neuromod_bridge, NEUROMOD_BRIDGE_SEROTONIN);
    EXPECT_LT(nm_release, 0.6f);
}

TEST_F(PlasticitySubstrateBridgesIntegrationTest, IonBalanceAffectsNeurommodulatorReuptake) {
    // WHAT: Test ion balance affects reuptake transporters
    // WHY:  Verify Na+/K+ gradient effects

    // Set low ion balance
    set_substrate_state(0.8f, 37.0f, 0.8f, 0.4f);

    ASSERT_EQ(neuromod_substrate_update_effects(neuromod_bridge), 0);

    // Check reuptake efficiency for all neuromodulators
    float da_reuptake = neuromod_substrate_get_reuptake_mod(neuromod_bridge, NEUROMOD_BRIDGE_DOPAMINE);
    float ser_reuptake = neuromod_substrate_get_reuptake_mod(neuromod_bridge, NEUROMOD_BRIDGE_SEROTONIN);
    float ne_reuptake = neuromod_substrate_get_reuptake_mod(neuromod_bridge, NEUROMOD_BRIDGE_NOREPINEPHRINE);

    // All should be impaired (quadratic scaling)
    EXPECT_LT(da_reuptake, 0.25f);  // 0.4^2 = 0.16
    EXPECT_LT(ser_reuptake, 0.25f);
    EXPECT_LT(ne_reuptake, 0.25f);
}

//=============================================================================
// Cross-Effects Tests
//=============================================================================

TEST_F(PlasticitySubstrateBridgesIntegrationTest, HighPlasticityLowNeurommodulation) {
    // WHAT: Test scenario where plasticity is fine but neuromodulation is impaired
    // WHY:  Verify independent modulation paths

    // ATP high, calcium low (synthesis OK, release impaired)
    set_substrate_state(0.9f, 37.0f, 0.3f, 0.9f);

    ASSERT_EQ(plasticity_substrate_update_all(plasticity_bridge), 0);
    ASSERT_EQ(neuromod_substrate_update_effects(neuromod_bridge), 0);

    // Plasticity should be good
    EXPECT_FALSE(plasticity_substrate_is_limited(plasticity_bridge));
    float plast_capacity = plasticity_substrate_get_capacity(plasticity_bridge);
    EXPECT_GT(plast_capacity, 0.8f);

    // Neuromodulator release should be impaired
    float nm_release = neuromod_substrate_get_release_mod(neuromod_bridge, NEUROMOD_BRIDGE_ACETYLCHOLINE);
    EXPECT_LT(nm_release, 0.5f);
}

TEST_F(PlasticitySubstrateBridgesIntegrationTest, BCMThresholdShiftUnderStress) {
    // WHAT: Test BCM threshold shifts under metabolic stress
    // WHY:  Verify protective LTD bias

    // Low ATP, normal other parameters
    set_substrate_state(0.4f, 37.0f, 0.8f, 0.9f);

    ASSERT_EQ(plasticity_substrate_update_bcm(plasticity_bridge), 0);

    // BCM threshold should shift up (bias toward LTD)
    float bcm_shift = plasticity_substrate_get_bcm_threshold_shift(plasticity_bridge);
    EXPECT_GT(bcm_shift, 1.2f); // At least 20% increase
}

TEST_F(PlasticitySubstrateBridgesIntegrationTest, EligibilityTraceDecayUnderLowATP) {
    // WHAT: Test eligibility trace decay accelerates with low ATP
    // WHY:  Verify ATP-dependent trace maintenance

    // Low ATP
    set_substrate_state(0.3f, 37.0f, 0.8f, 0.9f);

    ASSERT_EQ(plasticity_substrate_update_eligibility(plasticity_bridge), 0);

    // Get decay modulation
    float decay_mod = plasticity_substrate_get_eligibility_decay_mod(plasticity_bridge);

    // Decay should be faster (higher lambda) under low ATP
    EXPECT_GT(decay_mod, 1.0f);
}

//=============================================================================
// Bio-Async Integration Tests
//=============================================================================

TEST_F(PlasticitySubstrateBridgesIntegrationTest, BioAsyncConnection) {
    // WHAT: Test bio-async connectivity
    // WHY:  Verify bridges can communicate via bio-router

    // Connect both bridges
    int plast_result = plasticity_substrate_connect_bio_async(plasticity_bridge);
    int nm_result = neuromod_substrate_connect_bio_async(neuromod_bridge);

    // May succeed or fail gracefully (router may not be available)
    if (plast_result == 0) {
        EXPECT_TRUE(plasticity_substrate_is_bio_async_connected(plasticity_bridge));
    }
    if (nm_result == 0) {
        EXPECT_TRUE(neuromod_substrate_is_bio_async_connected(neuromod_bridge));
    }

    // Disconnect
    if (plast_result == 0) {
        ASSERT_EQ(plasticity_substrate_disconnect_bio_async(plasticity_bridge), 0);
    }
    if (nm_result == 0) {
        ASSERT_EQ(neuromod_substrate_disconnect_bio_async(neuromod_bridge), 0);
    }
}

//=============================================================================
// Metabolic Feedback Tests
//=============================================================================

TEST_F(PlasticitySubstrateBridgesIntegrationTest, NeurommodulatorSynthesisConsumesATP) {
    // WHAT: Test synthesis events consume ATP
    // WHY:  Verify feedback from neuromodulation to substrate

    float initial_atp = substrate_get_atp_level(substrate);

    // Record multiple synthesis events
    for (int i = 0; i < 100; i++) {
        ASSERT_EQ(neuromod_substrate_record_synthesis(neuromod_bridge, NEUROMOD_BRIDGE_DOPAMINE), 0);
    }

    float final_atp = substrate_get_atp_level(substrate);

    // ATP should decrease (slightly)
    EXPECT_LT(final_atp, initial_atp);
}

TEST_F(PlasticitySubstrateBridgesIntegrationTest, VesicleReleaseConsumesATP) {
    // WHAT: Test vesicle release consumes ATP
    // WHY:  Verify metabolic cost of neurotransmission

    float initial_atp = substrate_get_atp_level(substrate);

    // Record multiple release events
    for (int i = 0; i < 50; i++) {
        ASSERT_EQ(neuromod_substrate_record_release(neuromod_bridge, NEUROMOD_BRIDGE_SEROTONIN, 10), 0);
    }

    float final_atp = substrate_get_atp_level(substrate);
    EXPECT_LT(final_atp, initial_atp);
}

TEST_F(PlasticitySubstrateBridgesIntegrationTest, ReuptakeConsumesATP) {
    // WHAT: Test reuptake consumes ATP (Na+/K+ pump)
    // WHY:  Verify transporter metabolic cost

    float initial_atp = substrate_get_atp_level(substrate);

    for (int i = 0; i < 100; i++) {
        ASSERT_EQ(neuromod_substrate_record_reuptake(neuromod_bridge, NEUROMOD_BRIDGE_NOREPINEPHRINE), 0);
    }

    float final_atp = substrate_get_atp_level(substrate);
    EXPECT_LT(final_atp, initial_atp);
}

//=============================================================================
// Combined Temperature and ATP Tests
//=============================================================================

TEST_F(PlasticitySubstrateBridgesIntegrationTest, LowATPHighTempCompensation) {
    // WHAT: Test how temperature and ATP interact
    // WHY:  Verify non-linear interaction

    // Low ATP but high temperature
    set_substrate_state(0.4f, 39.0f, 0.8f, 0.9f);

    ASSERT_EQ(plasticity_substrate_update_all(plasticity_bridge), 0);
    ASSERT_EQ(neuromod_substrate_update_effects(neuromod_bridge), 0);

    // Get effects
    plasticity_substrate_effects_t plast_effects;
    ASSERT_EQ(plasticity_substrate_get_effects(plasticity_bridge, &plast_effects), 0);

    // STDP should have reduced LR (ATP) but wider window (temp)
    EXPECT_LT(plast_effects.stdp.learning_rate_mod, 0.6f);
    EXPECT_GT(plast_effects.stdp.temperature_factor, 1.0f);
}

TEST_F(PlasticitySubstrateBridgesIntegrationTest, HypothermiaSuppressesBoth) {
    // WHAT: Test hypothermia reduces both plasticity and neuromodulation
    // WHY:  Verify cold suppression

    // Low temperature
    set_substrate_state(0.8f, 34.0f, 0.8f, 0.9f);

    ASSERT_EQ(plasticity_substrate_update_all(plasticity_bridge), 0);
    ASSERT_EQ(neuromod_substrate_update_effects(neuromod_bridge), 0);

    // Plasticity should be suppressed
    float stdp_window = plasticity_substrate_get_stdp_window_mod(plasticity_bridge);
    EXPECT_LT(stdp_window, 1.0f);

    // Neuromodulator kinetics slowed
    substrate_neuromod_effects_t nm_effects;
    ASSERT_EQ(neuromod_substrate_get_effects(neuromod_bridge, NEUROMOD_BRIDGE_DOPAMINE, &nm_effects), 0);
    EXPECT_LT(nm_effects.temp_synthesis_factor, 1.0f);
    EXPECT_LT(nm_effects.temp_reuptake_factor, 1.0f);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(PlasticitySubstrateBridgesIntegrationTest, StatisticsTracking) {
    // WHAT: Test bridges track statistics correctly
    // WHY:  Verify monitoring capabilities

    // Cause various effects
    set_substrate_state(0.3f, 37.0f, 0.8f, 0.9f);
    ASSERT_EQ(plasticity_substrate_update_all(plasticity_bridge), 0);

    set_substrate_state(0.9f, 39.0f, 0.8f, 0.9f);
    ASSERT_EQ(plasticity_substrate_update_all(plasticity_bridge), 0);

    set_substrate_state(0.8f, 37.0f, 0.3f, 0.9f);
    ASSERT_EQ(neuromod_substrate_update_effects(neuromod_bridge), 0);

    // Get statistics
    plasticity_substrate_stats_t plast_stats;
    ASSERT_EQ(plasticity_substrate_get_stats(plasticity_bridge, &plast_stats), 0);

    neuromod_substrate_stats_t nm_stats;
    ASSERT_EQ(neuromod_substrate_get_stats(neuromod_bridge, &nm_stats), 0);

    // Should have recorded events
    EXPECT_GT(plast_stats.total_updates, 0);
    EXPECT_GT(plast_stats.atp_limited_events, 0);

    EXPECT_GT(nm_stats.total_updates, 0);
}

//=============================================================================
// Edge Cases and Stress Tests
//=============================================================================

TEST_F(PlasticitySubstrateBridgesIntegrationTest, ExtremeLowATP) {
    // WHAT: Test extreme ATP depletion
    // WHY:  Verify graceful degradation

    set_substrate_state(0.05f, 37.0f, 0.8f, 0.9f);

    ASSERT_EQ(plasticity_substrate_update_all(plasticity_bridge), 0);
    ASSERT_EQ(neuromod_substrate_update_effects(neuromod_bridge), 0);

    // Both should be severely limited
    EXPECT_TRUE(plasticity_substrate_is_limited(plasticity_bridge));

    float plast_lr = plasticity_substrate_get_learning_rate_mod(plasticity_bridge);
    EXPECT_LT(plast_lr, 0.2f);

    float nm_capacity = neuromod_substrate_get_capacity(neuromod_bridge, NEUROMOD_BRIDGE_DOPAMINE);
    EXPECT_LT(nm_capacity, 0.3f);
}

TEST_F(PlasticitySubstrateBridgesIntegrationTest, MultipleRapidUpdates) {
    // WHAT: Test rapid successive updates
    // WHY:  Verify stability under high update frequency

    for (int i = 0; i < 100; i++) {
        // Vary substrate state
        float atp = 0.5f + 0.3f * sinf(i * 0.1f);
        float temp = 37.0f + 2.0f * cosf(i * 0.15f);
        set_substrate_state(atp, temp, 0.8f, 0.9f);

        ASSERT_EQ(plasticity_substrate_update_all(plasticity_bridge), 0);
        ASSERT_EQ(neuromod_substrate_update_effects(neuromod_bridge), 0);
    }

    // Should still be functional
    plasticity_substrate_stats_t stats;
    ASSERT_EQ(plasticity_substrate_get_stats(plasticity_bridge, &stats), 0);
    EXPECT_EQ(stats.total_updates, 100);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
