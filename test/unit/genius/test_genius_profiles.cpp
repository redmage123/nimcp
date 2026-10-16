/**
 * @file test_genius_profiles.cpp
 * @brief Comprehensive unit tests for genius profiles module
 *
 * Test Categories:
 * 1. Profile Retrieval Tests - Get static profiles by type
 * 2. Bridge Lifecycle Tests - Create, destroy, reset
 * 3. Profile Activation Tests - Activate, deactivate, blend
 * 4. State Query Tests - State, fatigue, flow
 * 5. Eidetic Memory Tests - Eidetic config presets
 * 6. Error Handling Tests - Invalid inputs, edge cases
 * 7. Bio-Async Integration Tests - Message handling
 * 8. Immune Integration Tests - Modulation, degradation
 *
 * @author NIMCP Development Team
 * @date 2026-02-03
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

extern "C" {
#include "core/brain/genius/nimcp_genius_profiles.h"
#include "core/brain/genius/nimcp_genius_types.h"
#include "core/brain/genius/nimcp_genius_traits.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class GeniusProfilesTest : public ::testing::Test {
protected:
    genius_profiles_bridge_t* bridge = nullptr;
    genius_profiles_config_t config;

    void SetUp() override {
        // Get default config
        ASSERT_EQ(genius_profiles_config_default(&config), GENIUS_ERROR_SUCCESS);

        // Disable external integrations for unit testing
        config.enable_bio_async = false;
        config.enable_mesh_coordination = false;
        config.enable_training_integration = false;
        config.enable_rcog_integration = false;
        config.enable_ccog_integration = false;
        config.enable_quantum_optimization = false;
        config.enable_kg_wiring = false;

        // Create bridge
        bridge = genius_profiles_bridge_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            genius_profiles_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

//=============================================================================
// 1. PROFILE RETRIEVAL TESTS
//=============================================================================

TEST_F(GeniusProfilesTest, GetMathematicalProfile) {
    const genius_profile_t* profile = genius_profile_get(GENIUS_TYPE_MATHEMATICAL);
    ASSERT_NE(profile, nullptr);

    EXPECT_EQ(profile->type, GENIUS_TYPE_MATHEMATICAL);
    EXPECT_STREQ(profile->name, "Mathematical");
    EXPECT_TRUE(profile->is_builtin);

    // Check key mathematical traits
    EXPECT_GE(profile->traits.working_memory_capacity, 10);
    EXPECT_GT(profile->traits.pattern_sensitivity, 2.0f);
    EXPECT_GT(profile->traits.abstraction_level, 2.0f);

    // Check parietal enhancement (Einstein's enlarged parietal)
    EXPECT_GE(profile->parietal.size_multiplier, 2.0f);
}

TEST_F(GeniusProfilesTest, GetVisualArtisticProfile) {
    const genius_profile_t* profile = genius_profile_get(GENIUS_TYPE_VISUAL_ARTISTIC);
    ASSERT_NE(profile, nullptr);

    EXPECT_EQ(profile->type, GENIUS_TYPE_VISUAL_ARTISTIC);
    EXPECT_STREQ(profile->name, "Visual/Artistic");

    // Check visual traits
    EXPECT_GT(profile->traits.mental_imagery_vividness, 2.0f);
    EXPECT_GT(profile->traits.eidetic_visual_strength, 2.0f);

    // Check occipital enhancement
    EXPECT_GE(profile->occipital.size_multiplier, 2.0f);
}

TEST_F(GeniusProfilesTest, GetMusicalProfile) {
    const genius_profile_t* profile = genius_profile_get(GENIUS_TYPE_MUSICAL);
    ASSERT_NE(profile, nullptr);

    EXPECT_EQ(profile->type, GENIUS_TYPE_MUSICAL);
    EXPECT_STREQ(profile->name, "Musical");

    // Check auditory eidetic (Mozart could replay after one hearing)
    EXPECT_GE(profile->traits.eidetic_auditory_strength, 3.0f);

    // Check temporal enhancement (planum temporale)
    EXPECT_GE(profile->temporal.size_multiplier, 2.0f);

    // Check cerebellum timing
    EXPECT_GE(profile->cerebellum.size_multiplier, 1.5f);
}

TEST_F(GeniusProfilesTest, GetLiteraryProfile) {
    const genius_profile_t* profile = genius_profile_get(GENIUS_TYPE_LITERARY);
    ASSERT_NE(profile, nullptr);

    EXPECT_EQ(profile->type, GENIUS_TYPE_LITERARY);

    // Check language connectivity
    EXPECT_GT(profile->connectivity.broca_wernicke, 2.0f);
    EXPECT_GT(profile->connectivity.semantic_network_strength, 2.0f);
}

TEST_F(GeniusProfilesTest, GetScientificProfile) {
    const genius_profile_t* profile = genius_profile_get(GENIUS_TYPE_SCIENTIFIC);
    ASSERT_NE(profile, nullptr);

    EXPECT_EQ(profile->type, GENIUS_TYPE_SCIENTIFIC);

    // Check Tesla-style eidetic visualization
    EXPECT_GE(profile->traits.eidetic_visual_strength, 3.0f);
    EXPECT_GE(profile->traits.eidetic_spatial_strength, 3.0f);
    EXPECT_GE(profile->traits.mental_simulation_fidelity, 2.5f);
}

TEST_F(GeniusProfilesTest, GetAthleticProfile) {
    const genius_profile_t* profile = genius_profile_get(GENIUS_TYPE_ATHLETIC);
    ASSERT_NE(profile, nullptr);

    EXPECT_EQ(profile->type, GENIUS_TYPE_ATHLETIC);

    // Check motor enhancement
    EXPECT_GE(profile->motor.size_multiplier, 2.0f);
    EXPECT_GE(profile->motor.processing_speed_multiplier, 2.0f);

    // Check cerebellum
    EXPECT_GE(profile->cerebellum.size_multiplier, 2.0f);

    // Check flow state ease
    EXPECT_LE(profile->traits.flow_state_threshold, 0.5f);
}

TEST_F(GeniusProfilesTest, GetStrategicProfile) {
    const genius_profile_t* profile = genius_profile_get(GENIUS_TYPE_STRATEGIC);
    ASSERT_NE(profile, nullptr);

    EXPECT_EQ(profile->type, GENIUS_TYPE_STRATEGIC);

    // Check social cognition
    EXPECT_GT(profile->connectivity.theory_of_mind_network, 2.0f);
    EXPECT_GT(profile->connectivity.prefrontal_amygdala, 1.5f);

    // Check risk assessment
    EXPECT_GT(profile->traits.risk_calibration, 2.0f);
}

TEST_F(GeniusProfilesTest, GetFinancialProfile) {
    const genius_profile_t* profile = genius_profile_get(GENIUS_TYPE_FINANCIAL);
    ASSERT_NE(profile, nullptr);

    EXPECT_EQ(profile->type, GENIUS_TYPE_FINANCIAL);

    // Check Buffett-style patience (low temporal discounting)
    EXPECT_LE(profile->traits.temporal_discounting_factor, 0.5f);

    // Check risk calibration (Soros reflexivity)
    EXPECT_GT(profile->traits.risk_calibration, 2.0f);

    // Check stress resilience
    EXPECT_GT(profile->traits.stress_resilience, 2.0f);

    // Check loss aversion resistance
    EXPECT_GT(profile->traits.loss_aversion_resistance, 2.0f);
}

TEST_F(GeniusProfilesTest, GetPolymathProfile) {
    const genius_profile_t* profile = genius_profile_get(GENIUS_TYPE_POLYMATH);
    ASSERT_NE(profile, nullptr);

    EXPECT_EQ(profile->type, GENIUS_TYPE_POLYMATH);

    // Check cross-domain transfer
    EXPECT_GE(profile->traits.transfer_learning_ability, 3.0f);

    // Check association strength (Da Vinci cross-domain thinking)
    EXPECT_GE(profile->traits.association_strength, 3.0f);

    // Check corpus callosum enhancement
    EXPECT_GT(profile->connectivity.callosum_cognitive_gain, 1.5f);
}

TEST_F(GeniusProfilesTest, GetAllProfileTypes) {
    for (int i = 0; i < GENIUS_TYPE_COUNT; i++) {
        genius_type_t type = static_cast<genius_type_t>(i);
        const genius_profile_t* profile = genius_profile_get(type);

        ASSERT_NE(profile, nullptr) << "Failed to get profile for type " << i;
        EXPECT_EQ(profile->type, type);
        EXPECT_TRUE(profile->is_builtin);
        EXPECT_GT(profile->version, 0u);
    }
}

TEST_F(GeniusProfilesTest, GetInvalidProfileReturnsNull) {
    EXPECT_EQ(genius_profile_get(GENIUS_TYPE_INVALID), nullptr);
    EXPECT_EQ(genius_profile_get(static_cast<genius_type_t>(-1)), nullptr);
    EXPECT_EQ(genius_profile_get(static_cast<genius_type_t>(GENIUS_TYPE_COUNT)), nullptr);
    EXPECT_EQ(genius_profile_get(static_cast<genius_type_t>(100)), nullptr);
}

//=============================================================================
// 2. PROFILE TYPE NAME/DESCRIPTION TESTS
//=============================================================================

TEST_F(GeniusProfilesTest, TypeNameReturnsCorrectStrings) {
    EXPECT_STREQ(genius_profile_type_name(GENIUS_TYPE_MATHEMATICAL), "Mathematical");
    EXPECT_STREQ(genius_profile_type_name(GENIUS_TYPE_VISUAL_ARTISTIC), "Visual/Artistic");
    EXPECT_STREQ(genius_profile_type_name(GENIUS_TYPE_MUSICAL), "Musical");
    EXPECT_STREQ(genius_profile_type_name(GENIUS_TYPE_LITERARY), "Literary");
    EXPECT_STREQ(genius_profile_type_name(GENIUS_TYPE_SCIENTIFIC), "Scientific");
    EXPECT_STREQ(genius_profile_type_name(GENIUS_TYPE_ATHLETIC), "Athletic");
    EXPECT_STREQ(genius_profile_type_name(GENIUS_TYPE_STRATEGIC), "Strategic");
    EXPECT_STREQ(genius_profile_type_name(GENIUS_TYPE_FINANCIAL), "Financial");
    EXPECT_STREQ(genius_profile_type_name(GENIUS_TYPE_POLYMATH), "Polymath");
}

TEST_F(GeniusProfilesTest, TypeNameInvalidReturnsUnknown) {
    EXPECT_STREQ(genius_type_name(GENIUS_TYPE_INVALID), "UNKNOWN");
    EXPECT_STREQ(genius_type_name(static_cast<genius_type_t>(100)), "UNKNOWN");
}

TEST_F(GeniusProfilesTest, TypeDescriptionNotEmpty) {
    for (int i = 0; i < GENIUS_TYPE_COUNT; i++) {
        genius_type_t type = static_cast<genius_type_t>(i);
        const char* desc = genius_profile_type_description(type);

        ASSERT_NE(desc, nullptr);
        EXPECT_GT(strlen(desc), 10u) << "Description too short for type " << i;
    }
}

TEST_F(GeniusProfilesTest, TypeExemplarsNotEmpty) {
    for (int i = 0; i < GENIUS_TYPE_COUNT; i++) {
        genius_type_t type = static_cast<genius_type_t>(i);
        const char* exemplars = genius_profile_type_exemplars(type);

        ASSERT_NE(exemplars, nullptr);
        EXPECT_GT(strlen(exemplars), 5u) << "Exemplars too short for type " << i;
    }
}

//=============================================================================
// 3. BRIDGE LIFECYCLE TESTS
//=============================================================================

TEST_F(GeniusProfilesTest, BridgeCreateWithDefaultConfig) {
    genius_profiles_bridge_t* b = genius_profiles_bridge_create(nullptr);
    ASSERT_NE(b, nullptr);

    EXPECT_EQ(genius_profiles_get_state(b), GENIUS_STATE_INACTIVE);
    EXPECT_TRUE(genius_profiles_is_ready(b));

    genius_profiles_bridge_destroy(b);
}

TEST_F(GeniusProfilesTest, BridgeCreateWithCustomConfig) {
    genius_profiles_config_t custom_config;
    genius_profiles_config_default(&custom_config);
    custom_config.enable_immune_modulation = false;
    custom_config.health_heartbeat_ms = 2000;

    genius_profiles_bridge_t* b = genius_profiles_bridge_create(&custom_config);
    ASSERT_NE(b, nullptr);

    genius_profiles_bridge_destroy(b);
}

TEST_F(GeniusProfilesTest, BridgeDestroyNull) {
    // Should not crash
    genius_profiles_bridge_destroy(nullptr);
}

TEST_F(GeniusProfilesTest, BridgeReset) {
    // Activate a profile first
    ASSERT_EQ(genius_profiles_activate(bridge, GENIUS_TYPE_MATHEMATICAL, 1.0f), GENIUS_ERROR_SUCCESS);
    EXPECT_EQ(genius_profiles_get_state(bridge), GENIUS_STATE_ACTIVE);

    // Reset
    ASSERT_EQ(genius_profiles_bridge_reset(bridge), GENIUS_ERROR_SUCCESS);
    EXPECT_EQ(genius_profiles_get_state(bridge), GENIUS_STATE_INACTIVE);
}

TEST_F(GeniusProfilesTest, BridgeResetNull) {
    EXPECT_EQ(genius_profiles_bridge_reset(nullptr), GENIUS_ERROR_NULL_POINTER);
}

//=============================================================================
// 4. PROFILE ACTIVATION TESTS
//=============================================================================

TEST_F(GeniusProfilesTest, ActivateSingleProfile) {
    ASSERT_EQ(genius_profiles_activate(bridge, GENIUS_TYPE_MATHEMATICAL, 1.0f), GENIUS_ERROR_SUCCESS);

    EXPECT_EQ(genius_profiles_get_state(bridge), GENIUS_STATE_ACTIVE);

    const genius_profile_t* active = genius_profiles_get_active(bridge);
    ASSERT_NE(active, nullptr);
    EXPECT_EQ(active->type, GENIUS_TYPE_MATHEMATICAL);
}

TEST_F(GeniusProfilesTest, ActivateWithStrength) {
    ASSERT_EQ(genius_profiles_activate(bridge, GENIUS_TYPE_VISUAL_ARTISTIC, 0.75f), GENIUS_ERROR_SUCCESS);

    EXPECT_EQ(genius_profiles_get_state(bridge), GENIUS_STATE_ACTIVE);
}

TEST_F(GeniusProfilesTest, ActivateStrengthClamping) {
    // Negative strength should be clamped to 0
    ASSERT_EQ(genius_profiles_activate(bridge, GENIUS_TYPE_MUSICAL, -0.5f), GENIUS_ERROR_SUCCESS);

    // Very high strength should be clamped to 1
    ASSERT_EQ(genius_profiles_activate(bridge, GENIUS_TYPE_LITERARY, 5.0f), GENIUS_ERROR_SUCCESS);
}

TEST_F(GeniusProfilesTest, DeactivateProfile) {
    ASSERT_EQ(genius_profiles_activate(bridge, GENIUS_TYPE_SCIENTIFIC, 1.0f), GENIUS_ERROR_SUCCESS);
    EXPECT_EQ(genius_profiles_get_state(bridge), GENIUS_STATE_ACTIVE);

    ASSERT_EQ(genius_profiles_deactivate(bridge), GENIUS_ERROR_SUCCESS);
    EXPECT_EQ(genius_profiles_get_state(bridge), GENIUS_STATE_INACTIVE);
    EXPECT_EQ(genius_profiles_get_active(bridge), nullptr);
}

TEST_F(GeniusProfilesTest, DeactivateWhenInactive) {
    EXPECT_EQ(genius_profiles_get_state(bridge), GENIUS_STATE_INACTIVE);
    EXPECT_EQ(genius_profiles_deactivate(bridge), GENIUS_ERROR_NOT_ACTIVE);
}

TEST_F(GeniusProfilesTest, ActivateInvalidType) {
    EXPECT_EQ(genius_profiles_activate(bridge, GENIUS_TYPE_INVALID, 1.0f), GENIUS_ERROR_INVALID_TYPE);
    EXPECT_EQ(genius_profiles_activate(bridge, static_cast<genius_type_t>(-1), 1.0f), GENIUS_ERROR_INVALID_TYPE);
    EXPECT_EQ(genius_profiles_activate(bridge, static_cast<genius_type_t>(100), 1.0f), GENIUS_ERROR_INVALID_TYPE);
}

TEST_F(GeniusProfilesTest, ActivateNull) {
    EXPECT_EQ(genius_profiles_activate(nullptr, GENIUS_TYPE_MATHEMATICAL, 1.0f), GENIUS_ERROR_NULL_POINTER);
}

//=============================================================================
// 5. PROFILE BLENDING TESTS (POLYMATH MODE)
//=============================================================================

TEST_F(GeniusProfilesTest, BlendTwoProfiles) {
    genius_type_t types[2] = { GENIUS_TYPE_VISUAL_ARTISTIC, GENIUS_TYPE_SCIENTIFIC };
    float weights[2] = { 0.6f, 0.4f };

    ASSERT_EQ(genius_profiles_blend(bridge, types, weights, 2), GENIUS_ERROR_SUCCESS);
    EXPECT_EQ(genius_profiles_get_state(bridge), GENIUS_STATE_BLENDED);
}

TEST_F(GeniusProfilesTest, CreatePolymath) {
    // Da Vinci style: 60% artistic, 40% scientific
    ASSERT_EQ(genius_profiles_create_polymath(bridge,
                                              GENIUS_TYPE_VISUAL_ARTISTIC,
                                              GENIUS_TYPE_SCIENTIFIC,
                                              0.4f), GENIUS_ERROR_SUCCESS);

    EXPECT_EQ(genius_profiles_get_state(bridge), GENIUS_STATE_BLENDED);
}

TEST_F(GeniusProfilesTest, BlendWeightNormalization) {
    // Weights don't sum to 1 - should be normalized
    genius_type_t types[2] = { GENIUS_TYPE_MATHEMATICAL, GENIUS_TYPE_MUSICAL };
    float weights[2] = { 2.0f, 1.0f };  // Will be normalized to 0.67, 0.33

    ASSERT_EQ(genius_profiles_blend(bridge, types, weights, 2), GENIUS_ERROR_SUCCESS);
}

TEST_F(GeniusProfilesTest, BlendMaxProfiles) {
    genius_type_t types[GENIUS_MAX_ACTIVE_PROFILES] = {
        GENIUS_TYPE_MATHEMATICAL,
        GENIUS_TYPE_VISUAL_ARTISTIC,
        GENIUS_TYPE_MUSICAL,
        GENIUS_TYPE_LITERARY
    };
    float weights[GENIUS_MAX_ACTIVE_PROFILES] = { 0.25f, 0.25f, 0.25f, 0.25f };

    ASSERT_EQ(genius_profiles_blend(bridge, types, weights, GENIUS_MAX_ACTIVE_PROFILES), GENIUS_ERROR_SUCCESS);
}

TEST_F(GeniusProfilesTest, BlendZeroCount) {
    genius_type_t types[1] = { GENIUS_TYPE_MATHEMATICAL };
    float weights[1] = { 1.0f };

    EXPECT_EQ(genius_profiles_blend(bridge, types, weights, 0), GENIUS_ERROR_BLEND_FAILED);
}

TEST_F(GeniusProfilesTest, BlendTooManyProfiles) {
    genius_type_t types[5] = {
        GENIUS_TYPE_MATHEMATICAL,
        GENIUS_TYPE_VISUAL_ARTISTIC,
        GENIUS_TYPE_MUSICAL,
        GENIUS_TYPE_LITERARY,
        GENIUS_TYPE_SCIENTIFIC
    };
    float weights[5] = { 0.2f, 0.2f, 0.2f, 0.2f, 0.2f };

    EXPECT_EQ(genius_profiles_blend(bridge, types, weights, 5), GENIUS_ERROR_BLEND_FAILED);
}

TEST_F(GeniusProfilesTest, BlendNull) {
    genius_type_t types[2] = { GENIUS_TYPE_MATHEMATICAL, GENIUS_TYPE_MUSICAL };
    float weights[2] = { 0.5f, 0.5f };

    EXPECT_EQ(genius_profiles_blend(nullptr, types, weights, 2), GENIUS_ERROR_NULL_POINTER);
    EXPECT_EQ(genius_profiles_blend(bridge, nullptr, weights, 2), GENIUS_ERROR_NULL_POINTER);
    EXPECT_EQ(genius_profiles_blend(bridge, types, nullptr, 2), GENIUS_ERROR_NULL_POINTER);
}

//=============================================================================
// 6. STATE QUERY TESTS
//=============================================================================

TEST_F(GeniusProfilesTest, GetStateInactive) {
    EXPECT_EQ(genius_profiles_get_state(bridge), GENIUS_STATE_INACTIVE);
}

TEST_F(GeniusProfilesTest, GetStateNull) {
    EXPECT_EQ(genius_profiles_get_state(nullptr), GENIUS_STATE_ERROR);
}

TEST_F(GeniusProfilesTest, GetActiveWhenInactive) {
    EXPECT_EQ(genius_profiles_get_active(bridge), nullptr);
}

TEST_F(GeniusProfilesTest, GetFatigueInitiallyZero) {
    EXPECT_FLOAT_EQ(genius_profiles_get_fatigue(bridge), 0.0f);
}

TEST_F(GeniusProfilesTest, GetFlowDepthInitiallyZero) {
    EXPECT_FLOAT_EQ(genius_profiles_get_flow_depth(bridge), 0.0f);
}

TEST_F(GeniusProfilesTest, IsReadyWhenInactive) {
    EXPECT_TRUE(genius_profiles_is_ready(bridge));
}

TEST_F(GeniusProfilesTest, IsReadyWhenActive) {
    genius_profiles_activate(bridge, GENIUS_TYPE_MATHEMATICAL, 1.0f);
    EXPECT_TRUE(genius_profiles_is_ready(bridge));
}

TEST_F(GeniusProfilesTest, IsReadyNull) {
    EXPECT_FALSE(genius_profiles_is_ready(nullptr));
}

//=============================================================================
// 7. EIDETIC MEMORY PRESET TESTS
//=============================================================================

TEST_F(GeniusProfilesTest, EideticTeslaPreset) {
    const eidetic_memory_config_t* tesla = eidetic_config_tesla();
    ASSERT_NE(tesla, nullptr);

    // Tesla: visual-spatial dominant
    EXPECT_GE(tesla->visual_eidetic, 3.0f);
    EXPECT_GE(tesla->spatial_eidetic, 3.0f);
    EXPECT_TRUE(tesla->enable_instant_recall);
}

TEST_F(GeniusProfilesTest, EideticMozartPreset) {
    const eidetic_memory_config_t* mozart = eidetic_config_mozart();
    ASSERT_NE(mozart, nullptr);

    // Mozart: auditory dominant
    EXPECT_GE(mozart->auditory_eidetic, 3.0f);
    EXPECT_GT(mozart->working_memory.auditory_buffer_size, 256u);
}

TEST_F(GeniusProfilesTest, EideticVonNeumannPreset) {
    const eidetic_memory_config_t* vn = eidetic_config_vonneumann();
    ASSERT_NE(vn, nullptr);

    // von Neumann: verbal/numerical dominant
    EXPECT_GE(vn->verbal_eidetic, 3.0f);
    EXPECT_GE(vn->numerical_eidetic, 3.0f);
    EXPECT_GE(vn->working_memory.capacity_boost, 8);
}

TEST_F(GeniusProfilesTest, EideticKimPeekPreset) {
    const eidetic_memory_config_t* kp = eidetic_config_kim_peek();
    ASSERT_NE(kp, nullptr);

    // Kim Peek: encyclopedic factual recall
    EXPECT_GE(kp->retrieval_accuracy, 0.99f);
    EXPECT_TRUE(kp->autobiographical.enable_flashbulb_mode);
}

TEST_F(GeniusProfilesTest, EideticWiltshirePreset) {
    const eidetic_memory_config_t* wiltshire = eidetic_config_wiltshire();
    ASSERT_NE(wiltshire, nullptr);

    // Stephen Wiltshire: visual-artistic
    EXPECT_GE(wiltshire->visual_eidetic, 3.0f);
    EXPECT_GT(wiltshire->working_memory.visual_buffer_size, 512u);
}

TEST_F(GeniusProfilesTest, EideticBaselineInit) {
    eidetic_memory_config_t config;
    eidetic_memory_config_init_baseline(&config);

    // Baseline should have no eidetic enhancement
    EXPECT_FLOAT_EQ(config.visual_eidetic, 0.0f);
    EXPECT_FLOAT_EQ(config.auditory_eidetic, 0.0f);
    EXPECT_FLOAT_EQ(config.spatial_eidetic, 0.0f);
    EXPECT_FLOAT_EQ(config.encoding_speed, 1.0f);
}

//=============================================================================
// 8. FLOW STATE TESTS
//=============================================================================

TEST_F(GeniusProfilesTest, EnterFlowWhenActive) {
    genius_profiles_activate(bridge, GENIUS_TYPE_MATHEMATICAL, 1.0f);

    // Challenge/skill balance
    ASSERT_EQ(genius_profiles_enter_flow(bridge, 0.7f, 0.7f), GENIUS_ERROR_SUCCESS);
    EXPECT_EQ(genius_profiles_get_state(bridge), GENIUS_STATE_FLOW);
    EXPECT_GT(genius_profiles_get_flow_depth(bridge), 0.0f);
}

TEST_F(GeniusProfilesTest, EnterFlowMismatch) {
    genius_profiles_activate(bridge, GENIUS_TYPE_ATHLETIC, 1.0f);

    // Challenge too high relative to skill
    EXPECT_EQ(genius_profiles_enter_flow(bridge, 0.9f, 0.2f), GENIUS_ERROR_CONTEXT_MISMATCH);
}

TEST_F(GeniusProfilesTest, EnterFlowWhenInactive) {
    EXPECT_EQ(genius_profiles_enter_flow(bridge, 0.5f, 0.5f), GENIUS_ERROR_INVALID_STATE);
}

TEST_F(GeniusProfilesTest, ExitFlow) {
    genius_profiles_activate(bridge, GENIUS_TYPE_MUSICAL, 1.0f);
    genius_profiles_enter_flow(bridge, 0.5f, 0.5f);

    ASSERT_EQ(genius_profiles_exit_flow(bridge, "test complete"), GENIUS_ERROR_SUCCESS);
    EXPECT_NE(genius_profiles_get_state(bridge), GENIUS_STATE_FLOW);
    EXPECT_FLOAT_EQ(genius_profiles_get_flow_depth(bridge), 0.0f);
}

TEST_F(GeniusProfilesTest, ExitFlowWhenNotInFlow) {
    genius_profiles_activate(bridge, GENIUS_TYPE_MATHEMATICAL, 1.0f);
    EXPECT_EQ(genius_profiles_exit_flow(bridge, "not in flow"), GENIUS_ERROR_INVALID_STATE);
}

//=============================================================================
// 9. FATIGUE TESTS
//=============================================================================

TEST_F(GeniusProfilesTest, FatigueIncreases) {
    genius_profiles_activate(bridge, GENIUS_TYPE_SCIENTIFIC, 1.0f);

    float initial = genius_profiles_get_fatigue(bridge);

    // High activity should increase fatigue
    genius_profiles_update_fatigue(bridge, 60000, 1.0f);  // 60 seconds at full activity

    float after = genius_profiles_get_fatigue(bridge);
    EXPECT_GT(after, initial);
}

TEST_F(GeniusProfilesTest, FatigueRecovery) {
    genius_profiles_activate(bridge, GENIUS_TYPE_LITERARY, 1.0f);

    // First increase fatigue
    genius_profiles_update_fatigue(bridge, 60000, 1.0f);
    float fatigued = genius_profiles_get_fatigue(bridge);

    // Then recover with low activity
    genius_profiles_update_fatigue(bridge, 60000, 0.0f);
    float recovered = genius_profiles_get_fatigue(bridge);

    EXPECT_LT(recovered, fatigued);
}

TEST_F(GeniusProfilesTest, FatigueClamped) {
    genius_profiles_activate(bridge, GENIUS_TYPE_STRATEGIC, 1.0f);

    // Extreme values should stay in [0, 1]
    genius_profiles_update_fatigue(bridge, 10000000, 1.0f);  // Very long high activity
    EXPECT_LE(genius_profiles_get_fatigue(bridge), 1.0f);

    genius_profiles_update_fatigue(bridge, 10000000, 0.0f);  // Very long recovery
    EXPECT_GE(genius_profiles_get_fatigue(bridge), 0.0f);
}

//=============================================================================
// 10. IMMUNE MODULATION TESTS
//=============================================================================

TEST_F(GeniusProfilesTest, ImmuneModulationApplied) {
    ASSERT_EQ(genius_profiles_apply_immune_modulation(bridge, 0.5f, 0.3f), GENIUS_ERROR_SUCCESS);
}

TEST_F(GeniusProfilesTest, ImmuneModulationDegradation) {
    genius_profiles_activate(bridge, GENIUS_TYPE_MATHEMATICAL, 1.0f);

    // High inflammation should degrade
    genius_profiles_apply_immune_modulation(bridge, 0.9f, 0.8f);
    EXPECT_EQ(genius_profiles_get_state(bridge), GENIUS_STATE_DEGRADED);
    EXPECT_FALSE(genius_profiles_is_ready(bridge));
}

TEST_F(GeniusProfilesTest, ImmuneModulationRecovery) {
    genius_profiles_activate(bridge, GENIUS_TYPE_VISUAL_ARTISTIC, 1.0f);

    // Degrade
    genius_profiles_apply_immune_modulation(bridge, 0.9f, 0.8f);
    EXPECT_EQ(genius_profiles_get_state(bridge), GENIUS_STATE_DEGRADED);

    // Recovery
    genius_profiles_apply_immune_modulation(bridge, 0.2f, 0.2f);
    EXPECT_NE(genius_profiles_get_state(bridge), GENIUS_STATE_DEGRADED);
}

TEST_F(GeniusProfilesTest, ImmuneModulationNull) {
    EXPECT_EQ(genius_profiles_apply_immune_modulation(nullptr, 0.5f, 0.5f), GENIUS_ERROR_NULL_POINTER);
}

//=============================================================================
// 11. TRAITS INITIALIZATION TESTS
//=============================================================================

TEST_F(GeniusProfilesTest, TraitsBaselineInit) {
    genius_traits_t traits;
    genius_traits_init_baseline(&traits);

    EXPECT_EQ(traits.working_memory_capacity, 7u);
    EXPECT_FLOAT_EQ(traits.working_memory_decay_factor, 1.0f);
    EXPECT_FLOAT_EQ(traits.sustained_attention_duration, 1.0f);
    EXPECT_FLOAT_EQ(traits.pattern_sensitivity, 1.0f);
    EXPECT_FLOAT_EQ(traits.eidetic_visual_strength, 0.0f);
}

TEST_F(GeniusProfilesTest, TraitsBaselineNull) {
    // Should not crash
    genius_traits_init_baseline(nullptr);
}

TEST_F(GeniusProfilesTest, RegionConfigBaselineInit) {
    genius_region_config_t config;
    genius_region_config_init_baseline(&config);

    EXPECT_FLOAT_EQ(config.size_multiplier, 1.0f);
    EXPECT_FLOAT_EQ(config.processing_speed_multiplier, 1.0f);
    EXPECT_FLOAT_EQ(config.stdp_a_plus_multiplier, 1.0f);
}

TEST_F(GeniusProfilesTest, ConnectivityBaselineInit) {
    genius_connectivity_t conn;
    genius_connectivity_init_baseline(&conn);

    EXPECT_FLOAT_EQ(conn.parietal_prefrontal, 1.0f);
    EXPECT_FLOAT_EQ(conn.callosum_cognitive_gain, 1.0f);
    EXPECT_FLOAT_EQ(conn.broca_wernicke, 1.0f);
}

TEST_F(GeniusProfilesTest, LateralizationBalancedInit) {
    genius_lateralization_t lat;
    genius_lateralization_init_balanced(&lat);

    // Check for reasonable lateralization values (0.0-1.0 range)
    EXPECT_GE(lat.language_dominance, 0.0f);
    EXPECT_LE(lat.language_dominance, 1.0f);
    EXPECT_GE(lat.spatial_dominance, 0.0f);
    EXPECT_LE(lat.spatial_dominance, 1.0f);
}

//=============================================================================
// 12. TYPE VALIDATION TESTS
//=============================================================================

TEST_F(GeniusProfilesTest, TypeIsValid) {
    EXPECT_TRUE(genius_type_is_valid(GENIUS_TYPE_MATHEMATICAL));
    EXPECT_TRUE(genius_type_is_valid(GENIUS_TYPE_POLYMATH));
    EXPECT_TRUE(genius_type_is_valid(static_cast<genius_type_t>(0)));
    EXPECT_TRUE(genius_type_is_valid(static_cast<genius_type_t>(GENIUS_TYPE_COUNT - 1)));
}

TEST_F(GeniusProfilesTest, TypeIsInvalid) {
    EXPECT_FALSE(genius_type_is_valid(GENIUS_TYPE_INVALID));
    EXPECT_FALSE(genius_type_is_valid(static_cast<genius_type_t>(-1)));
    EXPECT_FALSE(genius_type_is_valid(static_cast<genius_type_t>(GENIUS_TYPE_COUNT)));
    EXPECT_FALSE(genius_type_is_valid(static_cast<genius_type_t>(100)));
}

//=============================================================================
// 13. ERROR CODE TESTS
//=============================================================================

TEST_F(GeniusProfilesTest, ErrorMessageNotNull) {
    EXPECT_NE(genius_error_message(GENIUS_ERROR_SUCCESS), nullptr);
    EXPECT_NE(genius_error_message(GENIUS_ERROR_NULL_POINTER), nullptr);
    EXPECT_NE(genius_error_message(GENIUS_ERROR_INVALID_TYPE), nullptr);
}

TEST_F(GeniusProfilesTest, ErrorMessageSuccess) {
    EXPECT_STREQ(genius_error_message(GENIUS_ERROR_SUCCESS), "Success");
}

TEST_F(GeniusProfilesTest, ErrorMessageUnknown) {
    EXPECT_STREQ(genius_error_message(static_cast<genius_error_t>(0xFFFF)), "Unknown error");
}

//=============================================================================
// 14. STATE NAME TESTS
//=============================================================================

TEST_F(GeniusProfilesTest, StateNameValid) {
    EXPECT_STREQ(genius_state_name(GENIUS_STATE_INACTIVE), "Inactive");
    EXPECT_STREQ(genius_state_name(GENIUS_STATE_ACTIVE), "Active");
    EXPECT_STREQ(genius_state_name(GENIUS_STATE_BLENDED), "Blended");
    EXPECT_STREQ(genius_state_name(GENIUS_STATE_FLOW), "Flow");
    EXPECT_STREQ(genius_state_name(GENIUS_STATE_FATIGUED), "Fatigued");
    EXPECT_STREQ(genius_state_name(GENIUS_STATE_DEGRADED), "Degraded");
}

TEST_F(GeniusProfilesTest, StateNameInvalid) {
    EXPECT_STREQ(genius_state_name(static_cast<genius_activation_state_t>(100)), "Unknown");
}

//=============================================================================
// 15. HEARTBEAT TESTS
//=============================================================================

TEST_F(GeniusProfilesTest, HeartbeatSucceeds) {
    ASSERT_EQ(genius_profiles_bridge_heartbeat(bridge), GENIUS_ERROR_SUCCESS);
}

TEST_F(GeniusProfilesTest, HeartbeatNull) {
    EXPECT_EQ(genius_profiles_bridge_heartbeat(nullptr), GENIUS_ERROR_NULL_POINTER);
}

TEST_F(GeniusProfilesTest, HealthAgentStartStop) {
    ASSERT_EQ(genius_profiles_start_health_agent(bridge), GENIUS_ERROR_SUCCESS);
    ASSERT_EQ(genius_profiles_stop_health_agent(bridge), GENIUS_ERROR_SUCCESS);
}

//=============================================================================
// 16. CONFIG TEST MODE TESTS
//=============================================================================

TEST_F(GeniusProfilesTest, ConfigDefaultTestModeDisabled) {
    // Default config should have test_mode disabled
    genius_profiles_config_t default_config;
    ASSERT_EQ(genius_profiles_config_default(&default_config), GENIUS_ERROR_SUCCESS);
    EXPECT_FALSE(default_config.test_mode);
}

TEST_F(GeniusProfilesTest, ConfigTestModeCanBeEnabled) {
    // Test mode can be enabled in config
    genius_profiles_config_t test_config;
    ASSERT_EQ(genius_profiles_config_default(&test_config), GENIUS_ERROR_SUCCESS);
    test_config.test_mode = true;
    EXPECT_TRUE(test_config.test_mode);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
