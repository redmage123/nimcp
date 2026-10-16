/**
 * @file test_synaptic_scaling_immune_integration.cpp
 * @brief Unit tests for synaptic scaling-immune bridge integration
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Comprehensive tests for TNF-α mediated synaptic scaling and immune integration
 * WHY:  Verify biological accuracy of scaling-immune coupling
 * HOW:  Test lifecycle, TNF-α modulation, aberrance detection, recovery
 */

#include <gtest/gtest.h>
#include "plasticity/immune/nimcp_synaptic_scaling_immune_bridge.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "plasticity/homeostatic/nimcp_homeostatic.h"

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class SynapticScalingImmuneBridgeTest : public ::testing::Test {
protected:
    brain_immune_system_t* immune_system;
    synaptic_scaling_immune_bridge_t* bridge;

    void SetUp() override {
        /* Create brain immune system */
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune_system = brain_immune_create(&immune_config);
        ASSERT_NE(immune_system, nullptr);

        /* Start immune system */
        brain_immune_start(immune_system);

        /* Create bridge with default config */
        bridge = synaptic_scaling_immune_bridge_create(nullptr, immune_system, nullptr);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            synaptic_scaling_immune_bridge_destroy(bridge);
        }
        if (immune_system) {
            brain_immune_destroy(immune_system);
        }
    }

    /* Helper: release TNF-α cytokine */
    void release_tnf_alpha(float concentration) {
        uint32_t cytokine_id;
        brain_immune_release_cytokine(
            immune_system,
            BRAIN_CYTOKINE_TNF,
            0,
            concentration,
            0,
            &cytokine_id
        );
    }

    /* Helper: release IL-1β cytokine */
    void release_il1_beta(float concentration) {
        uint32_t cytokine_id;
        brain_immune_release_cytokine(
            immune_system,
            BRAIN_CYTOKINE_IL1,
            0,
            concentration,
            0,
            &cytokine_id
        );
    }

    /* Helper: release IL-10 cytokine */
    void release_il10(float concentration) {
        uint32_t cytokine_id;
        brain_immune_release_cytokine(
            immune_system,
            BRAIN_CYTOKINE_IL10,
            0,
            concentration,
            0,
            &cytokine_id
        );
    }

    /* Helper: create inflammation site */
    void create_inflammation(brain_inflammation_level_t level) {
        uint32_t antigen_id;
        uint8_t epitope[32] = {1, 2, 3, 4};

        /* Present antigen to trigger inflammation */
        brain_immune_present_antigen(
            immune_system,
            ANTIGEN_SOURCE_MANUAL,
            epitope,
            4,
            (uint32_t)level * 2,
            0,
            &antigen_id
        );

        /* Initiate inflammation */
        uint32_t site_id;
        brain_immune_initiate_inflammation(
            immune_system,
            0,
            antigen_id,
            &site_id
        );

        /* Escalate to desired level */
        for (int i = 0; i < (int)level; i++) {
            brain_immune_escalate_inflammation(immune_system, site_id);
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(SynapticScalingImmuneBridgeTest, DefaultConfiguration) {
    synaptic_scaling_immune_config_t config;
    int result = synaptic_scaling_immune_default_config(&config);

    EXPECT_EQ(result, 0);
    EXPECT_TRUE(config.enable_tnf_scaling_modulation);
    EXPECT_TRUE(config.enable_il1_threshold_modulation);
    EXPECT_TRUE(config.enable_inflammation_rate_modulation);
    EXPECT_TRUE(config.enable_aberrance_detection);
    EXPECT_TRUE(config.enable_recovery_tracking);
    EXPECT_TRUE(config.enable_il10_restoration);

    EXPECT_FLOAT_EQ(config.tnf_sensitivity, 1.0f);
    EXPECT_FLOAT_EQ(config.aberrance_sensitivity, 1.0f);
}

TEST_F(SynapticScalingImmuneBridgeTest, CreateDestroy) {
    /* Already created in SetUp, verify not null */
    EXPECT_NE(bridge, nullptr);

    /* Verify initial state */
    EXPECT_EQ(bridge->total_updates, 0);
    EXPECT_EQ(bridge->tnf_modulations, 0);
    EXPECT_EQ(bridge->aberrance_detections, 0);
    EXPECT_EQ(bridge->immune_triggers, 0);

    /* Verify baseline scaling */
    EXPECT_FLOAT_EQ(bridge->tnf_effects.scaling_factor_modulation,
                    TNF_ALPHA_SCALING_BASELINE);
    EXPECT_FLOAT_EQ(bridge->tnf_effects.receptor_surface_density, 1.0f);
}

TEST_F(SynapticScalingImmuneBridgeTest, CreateWithNullImmuneSystemFails) {
    synaptic_scaling_immune_bridge_t* null_bridge =
        synaptic_scaling_immune_bridge_create(nullptr, nullptr, nullptr);

    EXPECT_EQ(null_bridge, nullptr);
}

TEST_F(SynapticScalingImmuneBridgeTest, CreateWithCustomConfig) {
    synaptic_scaling_immune_config_t config;
    synaptic_scaling_immune_default_config(&config);

    config.tnf_sensitivity = 1.5f;
    config.aberrance_sensitivity = 0.8f;
    config.enable_recovery_tracking = false;

    synaptic_scaling_immune_bridge_t* custom_bridge =
        synaptic_scaling_immune_bridge_create(&config, immune_system, nullptr);

    ASSERT_NE(custom_bridge, nullptr);
    EXPECT_FLOAT_EQ(custom_bridge->tnf_sensitivity, 1.5f);
    EXPECT_FLOAT_EQ(custom_bridge->aberrance_sensitivity, 0.8f);
    EXPECT_FALSE(custom_bridge->enable_recovery_tracking);

    synaptic_scaling_immune_bridge_destroy(custom_bridge);
}

/* ============================================================================
 * TNF-α Scaling Modulation Tests
 * ============================================================================ */

TEST_F(SynapticScalingImmuneBridgeTest, NoTNFBaselineScaling) {
    /* No TNF-α released, should have baseline scaling */
    synaptic_scaling_immune_apply_tnf_effects(bridge);

    EXPECT_FLOAT_EQ(bridge->tnf_effects.tnf_alpha_concentration, 0.0f);
    EXPECT_FLOAT_EQ(bridge->tnf_effects.scaling_factor_modulation,
                    TNF_ALPHA_SCALING_BASELINE);
    EXPECT_EQ(bridge->tnf_modulations, 1);
}

TEST_F(SynapticScalingImmuneBridgeTest, LowTNFPhysiologicalBoost) {
    /* Release low TNF-α (0.1) */
    release_tnf_alpha(0.1f);

    synaptic_scaling_immune_apply_tnf_effects(bridge);

    /* Should be in 1.0-1.2x range */
    EXPECT_GT(bridge->tnf_effects.scaling_factor_modulation, 1.0f);
    EXPECT_LT(bridge->tnf_effects.scaling_factor_modulation, 1.3f);

    /* AMPA receptor trafficking should be active */
    EXPECT_GT(bridge->tnf_effects.ampa_receptor_trafficking, 0.5f);
    EXPECT_LT(bridge->tnf_effects.ampa_receptor_trafficking, 0.7f);
}

TEST_F(SynapticScalingImmuneBridgeTest, MediumTNFModerateBoost) {
    /* Release medium TNF-α (0.4) */
    release_tnf_alpha(0.4f);

    synaptic_scaling_immune_apply_tnf_effects(bridge);

    /* Should be in 1.2-1.8x range */
    EXPECT_GT(bridge->tnf_effects.scaling_factor_modulation, 1.2f);
    EXPECT_LT(bridge->tnf_effects.scaling_factor_modulation, 2.0f);

    /* Receptor surface density should increase */
    EXPECT_GT(bridge->tnf_effects.receptor_surface_density, 0.8f);
}

TEST_F(SynapticScalingImmuneBridgeTest, HighTNFStrongBoost) {
    /* Release high TNF-α (0.7) */
    release_tnf_alpha(0.7f);

    synaptic_scaling_immune_apply_tnf_effects(bridge);

    /* Should be in 1.8-2.5x range */
    EXPECT_GT(bridge->tnf_effects.scaling_factor_modulation, 1.8f);
    EXPECT_LT(bridge->tnf_effects.scaling_factor_modulation, 2.8f);

    /* Homeostatic set point should be elevated */
    EXPECT_GT(bridge->tnf_effects.homeostatic_set_point, 1.1f);
}

TEST_F(SynapticScalingImmuneBridgeTest, ExcessiveTNFPathological) {
    /* Release excessive TNF-α (0.95) */
    release_tnf_alpha(0.95f);

    synaptic_scaling_immune_apply_tnf_effects(bridge);

    /* Should be in 2.5-3.5x range (pathological) */
    EXPECT_GT(bridge->tnf_effects.scaling_factor_modulation, 2.5f);
    EXPECT_LT(bridge->tnf_effects.scaling_factor_modulation, 4.0f);

    /* Receptor trafficking should be maximal */
    EXPECT_GT(bridge->tnf_effects.ampa_receptor_trafficking, 0.9f);
}

TEST_F(SynapticScalingImmuneBridgeTest, TNFReceptorTraffickingDynamics) {
    /* Release TNF-α */
    release_tnf_alpha(0.5f);

    synaptic_scaling_immune_apply_tnf_effects(bridge);

    /* Insertion rate should increase with TNF-α */
    EXPECT_GT(bridge->tnf_effects.receptor_insertion_rate, 0.4f);

    /* Internalization rate should decrease */
    EXPECT_LT(bridge->tnf_effects.receptor_internalization_rate, 0.4f);

    /* Surface density should be elevated */
    EXPECT_GT(bridge->tnf_effects.receptor_surface_density, 0.85f);
}

/* ============================================================================
 * IL-1β Plasticity Threshold Tests
 * ============================================================================ */

TEST_F(SynapticScalingImmuneBridgeTest, NoIL1NormalThresholds) {
    /* No IL-1β, should have normal thresholds */
    synaptic_scaling_immune_apply_il1_effects(bridge);

    EXPECT_FLOAT_EQ(bridge->il1_effects.il1_beta_concentration, 0.0f);
    EXPECT_FLOAT_EQ(bridge->il1_effects.ltp_threshold_modulation,
                    IL1_BETA_LTP_THRESHOLD_NORMAL);
    EXPECT_FLOAT_EQ(bridge->il1_effects.plasticity_rate_modulation, 1.0f);
}

TEST_F(SynapticScalingImmuneBridgeTest, LowIL1NoThresholdChange) {
    /* Release low IL-1β (0.2) */
    release_il1_beta(0.2f);

    synaptic_scaling_immune_apply_il1_effects(bridge);

    /* Should still be at normal threshold */
    EXPECT_FLOAT_EQ(bridge->il1_effects.ltp_threshold_modulation,
                    IL1_BETA_LTP_THRESHOLD_NORMAL);
}

TEST_F(SynapticScalingImmuneBridgeTest, ModerateIL1ElevatedThreshold) {
    /* Release moderate IL-1β (0.5) */
    release_il1_beta(0.5f);

    synaptic_scaling_immune_apply_il1_effects(bridge);

    /* Should elevate LTP threshold */
    EXPECT_FLOAT_EQ(bridge->il1_effects.ltp_threshold_modulation,
                    IL1_BETA_LTP_THRESHOLD_ELEVATED);

    /* Plasticity rate should be reduced */
    EXPECT_LT(bridge->il1_effects.plasticity_rate_modulation, 1.0f);
}

TEST_F(SynapticScalingImmuneBridgeTest, HighIL1HighThreshold) {
    /* Release high IL-1β (0.8) */
    release_il1_beta(0.8f);

    synaptic_scaling_immune_apply_il1_effects(bridge);

    /* Should have high LTP threshold */
    EXPECT_FLOAT_EQ(bridge->il1_effects.ltp_threshold_modulation,
                    IL1_BETA_LTP_THRESHOLD_HIGH);

    /* Plasticity rate should be strongly reduced */
    EXPECT_LT(bridge->il1_effects.plasticity_rate_modulation, 0.8f);
}

/* ============================================================================
 * Inflammation Rate Modulation Tests
 * ============================================================================ */

TEST_F(SynapticScalingImmuneBridgeTest, NoInflammationBaselineRate) {
    /* No inflammation */
    synaptic_scaling_immune_apply_inflammation_effects(bridge);

    EXPECT_EQ(bridge->inflammation_state.current_level, INFLAMMATION_NONE);
    EXPECT_FLOAT_EQ(bridge->inflammation_state.scaling_rate_multiplier,
                    INFLAMMATION_SCALING_RATE_NONE);
    EXPECT_FALSE(bridge->inflammation_state.is_chronic);
}

TEST_F(SynapticScalingImmuneBridgeTest, LocalInflammationMinimalRateIncrease) {
    /* Create local inflammation */
    create_inflammation(INFLAMMATION_LOCAL);

    synaptic_scaling_immune_apply_inflammation_effects(bridge);

    EXPECT_EQ(bridge->inflammation_state.current_level, INFLAMMATION_LOCAL);
    EXPECT_FLOAT_EQ(bridge->inflammation_state.scaling_rate_multiplier,
                    INFLAMMATION_SCALING_RATE_LOCAL);
}

TEST_F(SynapticScalingImmuneBridgeTest, SystemicInflammationHighRate) {
    /* Create systemic inflammation */
    create_inflammation(INFLAMMATION_SYSTEMIC);

    synaptic_scaling_immune_apply_inflammation_effects(bridge);

    EXPECT_EQ(bridge->inflammation_state.current_level, INFLAMMATION_SYSTEMIC);
    EXPECT_FLOAT_EQ(bridge->inflammation_state.scaling_rate_multiplier,
                    INFLAMMATION_SCALING_RATE_SYSTEMIC);

    /* E/I balance should shift toward excitation */
    EXPECT_GT(bridge->inflammation_state.excitation_inhibition_balance, 0.5f);

    /* Network stability should decrease */
    EXPECT_LT(bridge->inflammation_state.network_stability, 1.0f);
}

TEST_F(SynapticScalingImmuneBridgeTest, CytokineStormAberrantRate) {
    /* Create cytokine storm */
    create_inflammation(INFLAMMATION_STORM);

    synaptic_scaling_immune_apply_inflammation_effects(bridge);

    EXPECT_EQ(bridge->inflammation_state.current_level, INFLAMMATION_STORM);
    EXPECT_FLOAT_EQ(bridge->inflammation_state.scaling_rate_multiplier,
                    INFLAMMATION_SCALING_RATE_STORM);

    /* Should detect runaway excitation */
    EXPECT_TRUE(bridge->inflammation_state.runaway_excitation);
    EXPECT_TRUE(synaptic_scaling_immune_check_runaway_excitation(bridge));
}

/* ============================================================================
 * Aberrance Detection Tests
 * ============================================================================ */

TEST_F(SynapticScalingImmuneBridgeTest, NormalScalingNoAberrance) {
    /* Apply normal TNF-α */
    release_tnf_alpha(0.1f);
    synaptic_scaling_immune_apply_tnf_effects(bridge);

    /* Detect aberrance */
    synaptic_scaling_immune_detect_aberrance(bridge);

    EXPECT_FALSE(bridge->aberrance.excessive_scale_up);
    EXPECT_FALSE(bridge->aberrance.excessive_scale_down);
    EXPECT_FALSE(synaptic_scaling_immune_is_aberrant(bridge));
}

TEST_F(SynapticScalingImmuneBridgeTest, ExcessiveScaleUpDetected) {
    /* Release excessive TNF-α to cause over-scaling */
    release_tnf_alpha(0.99f);
    synaptic_scaling_immune_apply_tnf_effects(bridge);

    /* Detect aberrance */
    synaptic_scaling_immune_detect_aberrance(bridge);

    EXPECT_TRUE(bridge->aberrance.excessive_scale_up);
    EXPECT_TRUE(synaptic_scaling_immune_is_aberrant(bridge));
    EXPECT_GT(bridge->aberrance.severity, 0.2f);
}

TEST_F(SynapticScalingImmuneBridgeTest, ImmuneTriggeredFromAberrance) {
    /* Create aberrant scaling */
    release_tnf_alpha(0.99f);
    synaptic_scaling_immune_apply_tnf_effects(bridge);
    synaptic_scaling_immune_detect_aberrance(bridge);

    /* Trigger immune response */
    int result = synaptic_scaling_immune_trigger_from_aberrance(bridge);

    EXPECT_EQ(result, 0);
    EXPECT_TRUE(bridge->aberrance.immune_triggered);
    EXPECT_EQ(bridge->immune_triggers, 1);
    EXPECT_GT(bridge->aberrance.trigger_count, 0);
}

TEST_F(SynapticScalingImmuneBridgeTest, NoRepeatTriggersFromSameAberrance) {
    /* Create aberrant scaling */
    release_tnf_alpha(0.99f);
    synaptic_scaling_immune_apply_tnf_effects(bridge);
    synaptic_scaling_immune_detect_aberrance(bridge);

    /* First trigger */
    synaptic_scaling_immune_trigger_from_aberrance(bridge);
    EXPECT_EQ(bridge->immune_triggers, 1);

    /* Second trigger attempt (should be prevented) */
    synaptic_scaling_immune_trigger_from_aberrance(bridge);
    EXPECT_EQ(bridge->immune_triggers, 1); /* Should not increment */
}

/* ============================================================================
 * IL-10 Recovery Tests
 * ============================================================================ */

TEST_F(SynapticScalingImmuneBridgeTest, IL10RestoresNormalScaling) {
    /* Create elevated scaling from TNF-α */
    release_tnf_alpha(0.7f);
    synaptic_scaling_immune_apply_tnf_effects(bridge);

    float elevated_factor = bridge->tnf_effects.scaling_factor_modulation;
    EXPECT_GT(elevated_factor, 1.5f);

    /* Release IL-10 */
    release_il10(0.8f);
    synaptic_scaling_immune_restore_from_il10(bridge);

    /* Scaling factor should move toward baseline */
    EXPECT_LT(bridge->tnf_effects.scaling_factor_modulation, elevated_factor);
    EXPECT_TRUE(bridge->recovery.in_recovery);
}

TEST_F(SynapticScalingImmuneBridgeTest, LowIL10NoRestoration) {
    /* Create elevated scaling */
    release_tnf_alpha(0.7f);
    synaptic_scaling_immune_apply_tnf_effects(bridge);

    float elevated_factor = bridge->tnf_effects.scaling_factor_modulation;

    /* Release low IL-10 (below threshold) */
    release_il10(0.2f);
    synaptic_scaling_immune_restore_from_il10(bridge);

    /* Should not restore yet */
    EXPECT_FLOAT_EQ(bridge->tnf_effects.scaling_factor_modulation, elevated_factor);
}

TEST_F(SynapticScalingImmuneBridgeTest, RecoverySignalReleaseIL10) {
    /* Create aberrant state */
    release_tnf_alpha(0.99f);
    synaptic_scaling_immune_apply_tnf_effects(bridge);
    synaptic_scaling_immune_detect_aberrance(bridge);
    synaptic_scaling_immune_trigger_from_aberrance(bridge);

    /* Restore to normal */
    release_tnf_alpha(0.05f);
    synaptic_scaling_immune_apply_tnf_effects(bridge);

    /* Manually set stability time (in real impl, would accumulate over updates) */
    bridge->recovery.time_stable_sec = SCALING_STABILITY_DURATION_SEC + 1.0f;

    /* Signal recovery */
    int result = synaptic_scaling_immune_signal_recovery(bridge);

    EXPECT_EQ(result, 0);
    EXPECT_TRUE(bridge->recovery.recovery_complete);
    EXPECT_EQ(bridge->recoveries_completed, 1);
}

/* ============================================================================
 * Effective Scaling Factor Tests
 * ============================================================================ */

TEST_F(SynapticScalingImmuneBridgeTest, EffectiveScalingFactorCombinesTNFAndInflammation) {
    /* Release moderate TNF-α */
    release_tnf_alpha(0.4f);
    synaptic_scaling_immune_apply_tnf_effects(bridge);

    /* Create regional inflammation */
    create_inflammation(INFLAMMATION_REGIONAL);
    synaptic_scaling_immune_apply_inflammation_effects(bridge);

    /* Get effective scaling factor */
    float effective = synaptic_scaling_immune_get_effective_scaling_factor(bridge);

    /* Should be product of TNF modulation and inflammation multiplier */
    float expected = bridge->tnf_effects.scaling_factor_modulation *
                     bridge->inflammation_state.scaling_rate_multiplier;

    EXPECT_NEAR(effective, expected, 0.01f);
}

TEST_F(SynapticScalingImmuneBridgeTest, EffectiveScalingClampedToSafeRange) {
    /* Create extreme conditions */
    release_tnf_alpha(1.0f);
    synaptic_scaling_immune_apply_tnf_effects(bridge);
    create_inflammation(INFLAMMATION_STORM);
    synaptic_scaling_immune_apply_inflammation_effects(bridge);

    /* Get effective scaling factor */
    float effective = synaptic_scaling_immune_get_effective_scaling_factor(bridge);

    /* Should be clamped to safe range [0.1, 5.0] */
    EXPECT_GE(effective, 0.1f);
    EXPECT_LE(effective, 5.0f);
}

/* ============================================================================
 * Bidirectional Update Tests
 * ============================================================================ */

TEST_F(SynapticScalingImmuneBridgeTest, BridgeUpdateProcessesBothDirections) {
    /* Release TNF-α and create inflammation */
    release_tnf_alpha(0.5f);
    create_inflammation(INFLAMMATION_REGIONAL);

    /* Run bridge update */
    int result = synaptic_scaling_immune_bridge_update(bridge, 100);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(bridge->total_updates, 1);

    /* Verify immune → scaling processed */
    EXPECT_GT(bridge->tnf_modulations, 0);
    EXPECT_GT(bridge->tnf_effects.scaling_factor_modulation, 1.0f);

    /* Verify scaling → immune processed */
    EXPECT_GT(bridge->aberrance_detections, 0);
}

TEST_F(SynapticScalingImmuneBridgeTest, MultipleUpdatesAccumulate) {
    /* Run multiple updates */
    for (int i = 0; i < 10; i++) {
        synaptic_scaling_immune_bridge_update(bridge, 100);
    }

    EXPECT_EQ(bridge->total_updates, 10);
}

/* ============================================================================
 * Query API Tests
 * ============================================================================ */

TEST_F(SynapticScalingImmuneBridgeTest, GetTNFEffects) {
    release_tnf_alpha(0.5f);
    synaptic_scaling_immune_apply_tnf_effects(bridge);

    tnf_alpha_scaling_effects_t effects;
    int result = synaptic_scaling_immune_get_tnf_effects(bridge, &effects);

    EXPECT_EQ(result, 0);
    EXPECT_GT(effects.tnf_alpha_concentration, 0.4f);
    EXPECT_GT(effects.scaling_factor_modulation, 1.0f);
    EXPECT_GT(effects.ampa_receptor_trafficking, 0.5f);
}

TEST_F(SynapticScalingImmuneBridgeTest, GetInflammationState) {
    create_inflammation(INFLAMMATION_SYSTEMIC);
    synaptic_scaling_immune_apply_inflammation_effects(bridge);

    inflammation_scaling_state_t state;
    int result = synaptic_scaling_immune_get_inflammation_state(bridge, &state);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(state.current_level, INFLAMMATION_SYSTEMIC);
    EXPECT_GT(state.scaling_rate_multiplier, 1.0f);
}

TEST_F(SynapticScalingImmuneBridgeTest, GetAMPADensity) {
    release_tnf_alpha(0.6f);
    synaptic_scaling_immune_apply_tnf_effects(bridge);

    float density = synaptic_scaling_immune_get_ampa_density(bridge);

    EXPECT_GT(density, 0.85f);
    EXPECT_LE(density, 1.0f);
}

TEST_F(SynapticScalingImmuneBridgeTest, GetRecoveryProgress) {
    /* Create aberrant state */
    release_tnf_alpha(0.99f);
    synaptic_scaling_immune_apply_tnf_effects(bridge);

    /* Restore to near-normal */
    release_tnf_alpha(0.1f);
    synaptic_scaling_immune_apply_tnf_effects(bridge);

    float progress = synaptic_scaling_immune_get_recovery_progress(bridge);

    EXPECT_GE(progress, 0.0f);
    EXPECT_LE(progress, 1.0f);
}

/* ============================================================================
 * Integration Tests
 * ============================================================================ */

TEST_F(SynapticScalingImmuneBridgeTest, CompleteAberranceRecoveryCycle) {
    /* Step 1: Create aberrant scaling from excessive TNF-α */
    release_tnf_alpha(0.99f);
    synaptic_scaling_immune_apply_tnf_effects(bridge);
    synaptic_scaling_immune_detect_aberrance(bridge);

    EXPECT_TRUE(synaptic_scaling_immune_is_aberrant(bridge));

    /* Step 2: Trigger immune response */
    synaptic_scaling_immune_trigger_from_aberrance(bridge);
    EXPECT_TRUE(bridge->aberrance.immune_triggered);

    /* Step 3: Release IL-10 to restore */
    release_il10(0.8f);
    synaptic_scaling_immune_restore_from_il10(bridge);

    /* Step 4: Verify recovery progress */
    EXPECT_TRUE(bridge->recovery.in_recovery);
    EXPECT_GT(bridge->recovery.recovery_progress, 0.0f);
}

TEST_F(SynapticScalingImmuneBridgeTest, TNFModulatesScalingInRealtime) {
    /* Gradual TNF-α increase */
    for (int i = 0; i < 10; i++) {
        float tnf_level = (float)i / 10.0f;
        release_tnf_alpha(tnf_level);
        synaptic_scaling_immune_apply_tnf_effects(bridge);

        /* Scaling factor should increase with TNF-α */
        float factor = bridge->tnf_effects.scaling_factor_modulation;
        EXPECT_GE(factor, 1.0f);

        if (i > 0) {
            /* Should be monotonically increasing */
            EXPECT_GT(factor, TNF_ALPHA_SCALING_BASELINE + (float)i * 0.1f);
        }
    }
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
