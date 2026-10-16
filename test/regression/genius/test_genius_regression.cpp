/**
 * @file test_genius_regression.cpp
 * @brief Regression tests for genius profiles module
 *
 * WHAT: Tests that verify genius profiles produce consistent, reproducible results
 * WHY:  Ensure parameter values, state transitions, and behaviors remain stable
 * HOW:  Compare against known-good values and verify deterministic behavior
 *
 * Regression categories:
 *   1. Profile parameter consistency (traits, regions, connectivity)
 *   2. Eidetic preset reproducibility
 *   3. Blending algorithm determinism
 *   4. State transition sequences
 *   5. Error code stability
 *   6. Bridge lifecycle determinism
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <array>

extern "C" {
#include "core/brain/genius/nimcp_genius_profiles.h"
#include "core/brain/genius/nimcp_genius_types.h"
#include "core/brain/genius/nimcp_genius_traits.h"
}

namespace nimcp {
namespace test {
namespace regression {

/* ============================================================================
 * TEST FIXTURE
 * ============================================================================ */

class GeniusRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create bridge with minimal configuration for isolated testing
        genius_profiles_config_t config;
        ASSERT_EQ(GENIUS_ERROR_SUCCESS, genius_profiles_config_default(&config));

        // Disable external system integration for regression tests
        config.enable_bio_async = false;
        config.enable_mesh_coordination = false;
        config.enable_immune_modulation = false;
        config.enable_health_agent = false;
        config.enable_training_integration = false;
        config.enable_kg_wiring = false;

        bridge_ = genius_profiles_bridge_create(&config);
        ASSERT_NE(nullptr, bridge_);
    }

    void TearDown() override {
        if (bridge_) {
            genius_profiles_bridge_destroy(bridge_);
            bridge_ = nullptr;
        }
    }

    genius_profiles_bridge_t* bridge_ = nullptr;

    // Tolerance for floating point comparisons
    static constexpr float kFloatTolerance = 1e-6f;

    // Helper to compare floats with tolerance
    static bool FloatEqual(float a, float b, float tolerance = kFloatTolerance) {
        return std::fabs(a - b) < tolerance;
    }
};

/* ============================================================================
 * PROFILE PARAMETER CONSISTENCY TESTS
 * ============================================================================ */

/**
 * Test: Mathematical genius profile parameters are consistent
 * Regression: Ensure trait values match documented specifications
 */
TEST_F(GeniusRegressionTest, MathematicalProfileParameters) {
    const genius_profile_t* profile = genius_profile_get(GENIUS_TYPE_MATHEMATICAL);
    ASSERT_NE(nullptr, profile);

    // Documented mathematical genius parameters
    EXPECT_EQ(GENIUS_TYPE_MATHEMATICAL, profile->type);
    EXPECT_STREQ("Mathematical", profile->name);

    // Working memory capacity should be enhanced (10 documented)
    EXPECT_EQ(10u, profile->traits.working_memory_capacity);

    // Pattern sensitivity should be 2.5 (enhanced for mathematical pattern recognition)
    EXPECT_TRUE(FloatEqual(2.5f, profile->traits.pattern_sensitivity));

    // Abstraction level should be 3.0 (high abstract reasoning)
    EXPECT_TRUE(FloatEqual(3.0f, profile->traits.abstraction_level));

    // Parietal enhancement (Einstein's enlarged inferior parietal lobules)
    EXPECT_TRUE(FloatEqual(2.0f, profile->parietal.size_multiplier));

    // Strong left hemisphere for symbolic processing
    EXPECT_GE(profile->lateralization.logical_reasoning_dominance, 0.9f);

    // Strong parietal-prefrontal connectivity
    EXPECT_TRUE(FloatEqual(2.0f, profile->connectivity.parietal_prefrontal));
}

/**
 * Test: Visual/Artistic genius profile parameters are consistent
 */
TEST_F(GeniusRegressionTest, VisualArtisticProfileParameters) {
    const genius_profile_t* profile = genius_profile_get(GENIUS_TYPE_VISUAL_ARTISTIC);
    ASSERT_NE(nullptr, profile);

    EXPECT_EQ(GENIUS_TYPE_VISUAL_ARTISTIC, profile->type);

    // Enhanced occipital (visual processing)
    EXPECT_GE(profile->occipital.size_multiplier, 1.5f);

    // Right hemisphere dominance for spatial
    EXPECT_LE(profile->lateralization.spatial_dominance, 0.3f);

    // Strong dorsal stream
    EXPECT_GE(profile->connectivity.occipital_parietal, 1.5f);

    // Mental imagery vividness should be high
    EXPECT_GE(profile->traits.mental_imagery_vividness, 2.5f);
}

/**
 * Test: Musical genius profile parameters are consistent
 */
TEST_F(GeniusRegressionTest, MusicalProfileParameters) {
    const genius_profile_t* profile = genius_profile_get(GENIUS_TYPE_MUSICAL);
    ASSERT_NE(nullptr, profile);

    EXPECT_EQ(GENIUS_TYPE_MUSICAL, profile->type);

    // Enlarged planum temporale
    EXPECT_GE(profile->temporal.size_multiplier, 2.0f);

    // Enhanced cerebellum timing
    EXPECT_GE(profile->cerebellum.precision_multiplier, 1.5f);

    // Auditory eidetic strength (Mozart could replay Miserere)
    EXPECT_GE(profile->traits.eidetic_auditory_strength, 2.5f);

    // Right for melody, left for rhythm
    EXPECT_LE(profile->lateralization.music_melody_dominance, 0.4f);
    EXPECT_GE(profile->lateralization.music_rhythm_dominance, 0.6f);
}

/**
 * Test: Scientific genius profile parameters are consistent
 */
TEST_F(GeniusRegressionTest, ScientificProfileParameters) {
    const genius_profile_t* profile = genius_profile_get(GENIUS_TYPE_SCIENTIFIC);
    ASSERT_NE(nullptr, profile);

    EXPECT_EQ(GENIUS_TYPE_SCIENTIFIC, profile->type);

    // Tesla's visualization ability
    EXPECT_GE(profile->traits.eidetic_visual_strength, 2.5f);
    EXPECT_GE(profile->traits.eidetic_spatial_strength, 2.5f);

    // Cross-domain association (Darwin)
    EXPECT_GE(profile->traits.association_strength, 2.5f);

    // Enhanced prefrontal for sustained focus
    EXPECT_GE(profile->prefrontal.size_multiplier, 1.5f);
}

/**
 * Test: Athletic genius profile parameters are consistent
 */
TEST_F(GeniusRegressionTest, AthleticProfileParameters) {
    const genius_profile_t* profile = genius_profile_get(GENIUS_TYPE_ATHLETIC);
    ASSERT_NE(nullptr, profile);

    EXPECT_EQ(GENIUS_TYPE_ATHLETIC, profile->type);

    // Enhanced motor cortex
    EXPECT_GE(profile->motor.size_multiplier, 1.5f);
    EXPECT_GE(profile->motor.processing_speed_multiplier, 1.5f);

    // Superior cerebellum
    EXPECT_GE(profile->cerebellum.size_multiplier, 1.5f);

    // Fast skill acquisition
    EXPECT_GE(profile->traits.skill_acquisition_rate, 2.0f);

    // Easy flow state entry
    EXPECT_LE(profile->traits.flow_state_threshold, 0.7f);
}

/**
 * Test: Financial genius profile parameters are consistent
 */
TEST_F(GeniusRegressionTest, FinancialProfileParameters) {
    const genius_profile_t* profile = genius_profile_get(GENIUS_TYPE_FINANCIAL);
    ASSERT_NE(nullptr, profile);

    EXPECT_EQ(GENIUS_TYPE_FINANCIAL, profile->type);

    // Risk assessment and decision-making
    EXPECT_GE(profile->prefrontal.size_multiplier, 1.5f);

    // Pattern recognition in time series
    EXPECT_GE(profile->traits.pattern_sensitivity, 2.0f);

    // Patience (lower = more patient, Buffett long-term)
    EXPECT_LE(profile->traits.temporal_discounting_factor, 0.5f);

    // Stress resilience under market volatility
    EXPECT_GE(profile->traits.stress_resilience, 2.0f);

    // Risk calibration (accurate probability assessment)
    EXPECT_GE(profile->traits.risk_calibration, 1.5f);
}

/**
 * Test: Strategic genius profile parameters are consistent
 */
TEST_F(GeniusRegressionTest, StrategicProfileParameters) {
    const genius_profile_t* profile = genius_profile_get(GENIUS_TYPE_STRATEGIC);
    ASSERT_NE(nullptr, profile);

    EXPECT_EQ(GENIUS_TYPE_STRATEGIC, profile->type);

    // Theory of mind network (Napoleon reading opponents)
    EXPECT_GE(profile->connectivity.theory_of_mind_network, 1.5f);

    // Risk assessment
    EXPECT_GE(profile->traits.risk_calibration, 1.5f);

    // Spatial memory for maps/positions
    EXPECT_GE(profile->traits.eidetic_spatial_strength, 1.5f);
}

/**
 * Test: Literary genius profile parameters are consistent
 */
TEST_F(GeniusRegressionTest, LiteraryProfileParameters) {
    const genius_profile_t* profile = genius_profile_get(GENIUS_TYPE_LITERARY);
    ASSERT_NE(nullptr, profile);

    EXPECT_EQ(GENIUS_TYPE_LITERARY, profile->type);

    // Broca-Wernicke connection (language production-comprehension)
    EXPECT_GE(profile->connectivity.broca_wernicke, 1.5f);

    // Semantic network strength
    EXPECT_GE(profile->connectivity.semantic_network_strength, 1.5f);

    // Left hemisphere language dominance
    EXPECT_GE(profile->lateralization.language_dominance, 0.7f);

    // Verbal memory
    EXPECT_GE(profile->traits.eidetic_verbal_strength, 1.5f);
}

/**
 * Test: All profile types return valid profiles
 */
TEST_F(GeniusRegressionTest, AllProfileTypesValid) {
    for (int i = 0; i < GENIUS_TYPE_COUNT; i++) {
        genius_type_t type = static_cast<genius_type_t>(i);
        const genius_profile_t* profile = genius_profile_get(type);

        if (type == GENIUS_TYPE_POLYMATH) {
            // Polymath is dynamic, may return NULL or base config
            continue;
        }

        ASSERT_NE(nullptr, profile) << "Profile type " << i << " returned NULL";
        EXPECT_EQ(type, profile->type) << "Profile type mismatch for type " << i;
        EXPECT_GT(strlen(profile->name), 0u) << "Empty name for type " << i;
    }
}

/* ============================================================================
 * EIDETIC PRESET REPRODUCIBILITY TESTS
 * ============================================================================ */

/**
 * Test: Tesla eidetic preset returns consistent values
 */
TEST_F(GeniusRegressionTest, EideticPresetTesla) {
    const eidetic_memory_config_t* tesla = eidetic_config_tesla();
    ASSERT_NE(nullptr, tesla);

    // Tesla: Visual-spatial dominant
    EXPECT_GE(tesla->visual_eidetic, 2.5f);
    EXPECT_GE(tesla->spatial_eidetic, 2.5f);

    // High mental simulation capability
    EXPECT_GE(tesla->simulation_duration_sec, 30.0f);
    EXPECT_GE(tesla->manipulation_capability, 2.0f);

    // Hippocampus enhancements for spatial memory
    EXPECT_GE(tesla->hippocampus.place_cell_multiplier, 2.0f);
}

/**
 * Test: Mozart eidetic preset returns consistent values
 */
TEST_F(GeniusRegressionTest, EideticPresetMozart) {
    const eidetic_memory_config_t* mozart = eidetic_config_mozart();
    ASSERT_NE(nullptr, mozart);

    // Mozart: Auditory dominant
    EXPECT_GE(mozart->auditory_eidetic, 2.5f);

    // Could replay entire symphonies
    EXPECT_GE(mozart->working_memory.auditory_buffer_size, 128u);

    // High encoding speed for music
    EXPECT_GE(mozart->encoding_speed, 2.0f);
}

/**
 * Test: von Neumann eidetic preset returns consistent values
 */
TEST_F(GeniusRegressionTest, EideticPresetVonNeumann) {
    const eidetic_memory_config_t* vonneumann = eidetic_config_vonneumann();
    ASSERT_NE(nullptr, vonneumann);

    // von Neumann: Numerical/verbal dominant
    EXPECT_GE(vonneumann->numerical_eidetic, 2.5f);
    EXPECT_GE(vonneumann->verbal_eidetic, 2.0f);

    // Photographic text memory
    EXPECT_GE(vonneumann->retrieval_accuracy, 0.95f);
}

/**
 * Test: Kim Peek eidetic preset returns consistent values
 */
TEST_F(GeniusRegressionTest, EideticPresetKimPeek) {
    const eidetic_memory_config_t* kimpeek = eidetic_config_kim_peek();
    ASSERT_NE(nullptr, kimpeek);

    // Kim Peek: Encyclopedic (factual recall)
    EXPECT_GE(kimpeek->semantic.concept_capacity_multiplier, 4u);

    // High capacity autobiographical
    EXPECT_GE(kimpeek->autobiographical.capacity_multiplier, 5u);

    // Very low forgetting
    EXPECT_GE(kimpeek->decay_resistance, 5.0f);
}

/**
 * Test: Wiltshire eidetic preset returns consistent values
 */
TEST_F(GeniusRegressionTest, EideticPresetWiltshire) {
    const eidetic_memory_config_t* wiltshire = eidetic_config_wiltshire();
    ASSERT_NE(nullptr, wiltshire);

    // Wiltshire: Visual-artistic (city panoramas)
    EXPECT_GE(wiltshire->visual_eidetic, 3.0f);

    // Extreme detail granularity
    EXPECT_GE(wiltshire->detail_granularity, 2.5f);

    // Long visual persistence
    EXPECT_GE(wiltshire->simulation_duration_sec, 30.0f);
}

/**
 * Test: Eidetic presets are deterministic (same pointer each call)
 */
TEST_F(GeniusRegressionTest, EideticPresetsAreDeterministic) {
    // Each call should return the same static pointer
    EXPECT_EQ(eidetic_config_tesla(), eidetic_config_tesla());
    EXPECT_EQ(eidetic_config_mozart(), eidetic_config_mozart());
    EXPECT_EQ(eidetic_config_vonneumann(), eidetic_config_vonneumann());
    EXPECT_EQ(eidetic_config_kim_peek(), eidetic_config_kim_peek());
    EXPECT_EQ(eidetic_config_wiltshire(), eidetic_config_wiltshire());
}

/* ============================================================================
 * BLENDING ALGORITHM DETERMINISM TESTS
 * ============================================================================ */

/**
 * Test: Blending same types with same weights produces same result
 */
TEST_F(GeniusRegressionTest, BlendingIsDeterministic) {
    genius_type_t types1[] = {GENIUS_TYPE_MATHEMATICAL, GENIUS_TYPE_VISUAL_ARTISTIC};
    float weights1[] = {0.6f, 0.4f};

    // First blend
    genius_error_t result1 = genius_profiles_blend(bridge_, types1, weights1, 2);
    EXPECT_EQ(GENIUS_ERROR_SUCCESS, result1);

    // Get state after first blend
    genius_activation_state_t state1 = genius_profiles_get_state(bridge_);

    // Reset and blend again
    ASSERT_EQ(GENIUS_ERROR_SUCCESS, genius_profiles_bridge_reset(bridge_));

    genius_type_t types2[] = {GENIUS_TYPE_MATHEMATICAL, GENIUS_TYPE_VISUAL_ARTISTIC};
    float weights2[] = {0.6f, 0.4f};

    genius_error_t result2 = genius_profiles_blend(bridge_, types2, weights2, 2);
    EXPECT_EQ(GENIUS_ERROR_SUCCESS, result2);

    genius_activation_state_t state2 = genius_profiles_get_state(bridge_);

    // Results should be identical
    EXPECT_EQ(result1, result2);
    EXPECT_EQ(state1, state2);
}

/**
 * Test: Blend weights are normalized consistently
 */
TEST_F(GeniusRegressionTest, BlendWeightNormalization) {
    // Weights that don't sum to 1.0 should be handled consistently
    genius_type_t types[] = {GENIUS_TYPE_MATHEMATICAL, GENIUS_TYPE_SCIENTIFIC};
    float weights[] = {0.3f, 0.3f};  // Sum = 0.6

    genius_error_t result = genius_profiles_blend(bridge_, types, weights, 2);

    // Should either normalize or reject - but be consistent
    if (result == GENIUS_ERROR_SUCCESS) {
        EXPECT_EQ(GENIUS_STATE_BLENDED, genius_profiles_get_state(bridge_));
    } else {
        EXPECT_EQ(GENIUS_ERROR_BLEND_FAILED, result);
    }
}

/**
 * Test: Polymath creation produces consistent results
 */
TEST_F(GeniusRegressionTest, PolymathCreationConsistency) {
    // Da Vinci style: 60% artistic, 40% scientific
    genius_error_t result = genius_profiles_create_polymath(
        bridge_,
        GENIUS_TYPE_VISUAL_ARTISTIC,
        GENIUS_TYPE_SCIENTIFIC,
        0.4f
    );
    EXPECT_EQ(GENIUS_ERROR_SUCCESS, result);

    genius_activation_state_t state = genius_profiles_get_state(bridge_);
    EXPECT_EQ(GENIUS_STATE_BLENDED, state);
}

/* ============================================================================
 * STATE TRANSITION SEQUENCE TESTS
 * ============================================================================ */

/**
 * Test: Activation state sequence is consistent
 */
TEST_F(GeniusRegressionTest, ActivationStateSequence) {
    // Initial state
    EXPECT_EQ(GENIUS_STATE_INACTIVE, genius_profiles_get_state(bridge_));

    // Activate
    ASSERT_EQ(GENIUS_ERROR_SUCCESS,
              genius_profiles_activate(bridge_, GENIUS_TYPE_MATHEMATICAL, 1.0f));

    // Should be active (may have been activating briefly)
    genius_activation_state_t state = genius_profiles_get_state(bridge_);
    EXPECT_TRUE(state == GENIUS_STATE_ACTIVE || state == GENIUS_STATE_ACTIVATING);

    // Deactivate
    ASSERT_EQ(GENIUS_ERROR_SUCCESS, genius_profiles_deactivate(bridge_));
    EXPECT_EQ(GENIUS_STATE_INACTIVE, genius_profiles_get_state(bridge_));
}

/**
 * Test: Flow state transitions follow expected sequence
 */
TEST_F(GeniusRegressionTest, FlowStateTransitionSequence) {
    // Activate first
    ASSERT_EQ(GENIUS_ERROR_SUCCESS,
              genius_profiles_activate(bridge_, GENIUS_TYPE_MATHEMATICAL, 1.0f));

    // Enter flow with balanced challenge/skill
    genius_error_t result = genius_profiles_enter_flow(bridge_, 0.7f, 0.7f);

    if (result == GENIUS_ERROR_SUCCESS) {
        EXPECT_EQ(GENIUS_STATE_FLOW, genius_profiles_get_state(bridge_));
        EXPECT_GT(genius_profiles_get_flow_depth(bridge_), 0.0f);

        // Exit flow
        ASSERT_EQ(GENIUS_ERROR_SUCCESS,
                  genius_profiles_exit_flow(bridge_, "test_exit"));
        EXPECT_NE(GENIUS_STATE_FLOW, genius_profiles_get_state(bridge_));
    }
}

/**
 * Test: Fatigue transitions follow expected sequence
 */
TEST_F(GeniusRegressionTest, FatigueTransitionSequence) {
    ASSERT_EQ(GENIUS_ERROR_SUCCESS,
              genius_profiles_activate(bridge_, GENIUS_TYPE_MATHEMATICAL, 1.0f));

    // Initial fatigue should be low
    float initial_fatigue = genius_profiles_get_fatigue(bridge_);
    EXPECT_LE(initial_fatigue, 0.1f);

    // Update with high activity for a long time
    for (int i = 0; i < 100; i++) {
        genius_profiles_update_fatigue(bridge_, 1000, 1.0f);  // 1 second of max activity
    }

    // Fatigue should increase
    float final_fatigue = genius_profiles_get_fatigue(bridge_);
    EXPECT_GT(final_fatigue, initial_fatigue);
}

/* ============================================================================
 * ERROR CODE STABILITY TESTS
 * ============================================================================ */

/**
 * Test: Error codes for invalid operations are consistent
 */
TEST_F(GeniusRegressionTest, ErrorCodeConsistency) {
    // Null pointer errors
    EXPECT_EQ(GENIUS_ERROR_NULL_POINTER,
              genius_profiles_config_default(nullptr));

    // Invalid type errors
    EXPECT_EQ(nullptr, genius_profile_get(GENIUS_TYPE_INVALID));
    EXPECT_EQ(nullptr, genius_profile_get(static_cast<genius_type_t>(999)));

    // Operations on inactive bridge return invalid state
    EXPECT_EQ(GENIUS_STATE_INACTIVE, genius_profiles_get_state(bridge_));
    EXPECT_EQ(GENIUS_ERROR_INVALID_STATE,
              genius_profiles_enter_flow(bridge_, 0.5f, 0.5f));
}

/**
 * Test: Error messages are consistent
 */
TEST_F(GeniusRegressionTest, ErrorMessageConsistency) {
    // Each error code should have a non-empty message
    EXPECT_STRNE("", genius_error_message(GENIUS_ERROR_SUCCESS));
    EXPECT_STRNE("", genius_error_message(GENIUS_ERROR_NULL_POINTER));
    EXPECT_STRNE("", genius_error_message(GENIUS_ERROR_INVALID_TYPE));
    EXPECT_STRNE("", genius_error_message(GENIUS_ERROR_BLEND_FAILED));

    // Messages should be deterministic
    EXPECT_STREQ(genius_error_message(GENIUS_ERROR_SUCCESS),
                 genius_error_message(GENIUS_ERROR_SUCCESS));
}

/* ============================================================================
 * BRIDGE LIFECYCLE DETERMINISM TESTS
 * ============================================================================ */

/**
 * Test: Bridge creation with same config produces equivalent bridges
 */
TEST_F(GeniusRegressionTest, BridgeCreationDeterminism) {
    genius_profiles_config_t config;
    ASSERT_EQ(GENIUS_ERROR_SUCCESS, genius_profiles_config_default(&config));

    genius_profiles_bridge_t* bridge1 = genius_profiles_bridge_create(&config);
    genius_profiles_bridge_t* bridge2 = genius_profiles_bridge_create(&config);

    ASSERT_NE(nullptr, bridge1);
    ASSERT_NE(nullptr, bridge2);

    // Different pointers
    EXPECT_NE(bridge1, bridge2);

    // Same initial state
    EXPECT_EQ(genius_profiles_get_state(bridge1), genius_profiles_get_state(bridge2));
    EXPECT_FLOAT_EQ(genius_profiles_get_fatigue(bridge1),
                    genius_profiles_get_fatigue(bridge2));
    EXPECT_FLOAT_EQ(genius_profiles_get_flow_depth(bridge1),
                    genius_profiles_get_flow_depth(bridge2));

    genius_profiles_bridge_destroy(bridge1);
    genius_profiles_bridge_destroy(bridge2);
}

/**
 * Test: Bridge reset returns to known state
 */
TEST_F(GeniusRegressionTest, BridgeResetToKnownState) {
    // Modify bridge state
    ASSERT_EQ(GENIUS_ERROR_SUCCESS,
              genius_profiles_activate(bridge_, GENIUS_TYPE_MATHEMATICAL, 1.0f));

    for (int i = 0; i < 10; i++) {
        genius_profiles_update_fatigue(bridge_, 1000, 0.8f);
    }

    // Reset
    ASSERT_EQ(GENIUS_ERROR_SUCCESS, genius_profiles_bridge_reset(bridge_));

    // Should be back to initial state
    EXPECT_EQ(GENIUS_STATE_INACTIVE, genius_profiles_get_state(bridge_));
    EXPECT_LE(genius_profiles_get_fatigue(bridge_), 0.01f);
    EXPECT_FLOAT_EQ(0.0f, genius_profiles_get_flow_depth(bridge_));
}

/**
 * Test: Default config values are consistent
 */
TEST_F(GeniusRegressionTest, DefaultConfigConsistency) {
    genius_profiles_config_t config1, config2;

    ASSERT_EQ(GENIUS_ERROR_SUCCESS, genius_profiles_config_default(&config1));
    ASSERT_EQ(GENIUS_ERROR_SUCCESS, genius_profiles_config_default(&config2));

    // All fields should be identical
    EXPECT_EQ(config1.enable_bio_async, config2.enable_bio_async);
    EXPECT_EQ(config1.enable_mesh_coordination, config2.enable_mesh_coordination);
    EXPECT_EQ(config1.enable_immune_modulation, config2.enable_immune_modulation);
    EXPECT_EQ(config1.enable_bbb_validation, config2.enable_bbb_validation);
    EXPECT_EQ(config1.enable_health_agent, config2.enable_health_agent);
    EXPECT_EQ(config1.enable_training_integration, config2.enable_training_integration);
    EXPECT_EQ(config1.enable_snn_integration, config2.enable_snn_integration);
    EXPECT_EQ(config1.enable_stdp, config2.enable_stdp);
    EXPECT_EQ(config1.enable_kg_wiring, config2.enable_kg_wiring);
    EXPECT_FLOAT_EQ(config1.base_learning_rate, config2.base_learning_rate);
}

/* ============================================================================
 * TRAIT INITIALIZATION CONSISTENCY TESTS
 * ============================================================================ */

/**
 * Test: Baseline trait initialization is deterministic
 */
TEST_F(GeniusRegressionTest, BaselineTraitInitialization) {
    genius_traits_t traits1, traits2;

    genius_traits_init_baseline(&traits1);
    genius_traits_init_baseline(&traits2);

    // All values should be identical
    EXPECT_EQ(traits1.working_memory_capacity, traits2.working_memory_capacity);
    EXPECT_FLOAT_EQ(traits1.working_memory_decay_factor, traits2.working_memory_decay_factor);
    EXPECT_FLOAT_EQ(traits1.pattern_sensitivity, traits2.pattern_sensitivity);
    EXPECT_FLOAT_EQ(traits1.eidetic_visual_strength, traits2.eidetic_visual_strength);

    // Baseline values should be 1.0 for multipliers
    EXPECT_FLOAT_EQ(1.0f, traits1.pattern_sensitivity);
    EXPECT_FLOAT_EQ(1.0f, traits1.abstraction_level);

    // Eidetic baseline should be 0.0 (no eidetic ability)
    EXPECT_FLOAT_EQ(0.0f, traits1.eidetic_visual_strength);
    EXPECT_FLOAT_EQ(0.0f, traits1.eidetic_auditory_strength);
}

/**
 * Test: Region config initialization is deterministic
 */
TEST_F(GeniusRegressionTest, RegionConfigInitialization) {
    genius_region_config_t config1, config2;

    genius_region_config_init_baseline(&config1);
    genius_region_config_init_baseline(&config2);

    EXPECT_FLOAT_EQ(config1.size_multiplier, config2.size_multiplier);
    EXPECT_FLOAT_EQ(config1.processing_speed_multiplier, config2.processing_speed_multiplier);
    EXPECT_EQ(config1.enable_flags, config2.enable_flags);

    // Baseline should be 1.0 for multipliers
    EXPECT_FLOAT_EQ(1.0f, config1.size_multiplier);
    EXPECT_FLOAT_EQ(1.0f, config1.learning_rate_multiplier);

    // No custom params by default
    EXPECT_EQ(0u, config1.custom_param_count);
}

/**
 * Test: Connectivity initialization is deterministic
 */
TEST_F(GeniusRegressionTest, ConnectivityInitialization) {
    genius_connectivity_t conn1, conn2;

    genius_connectivity_init_baseline(&conn1);
    genius_connectivity_init_baseline(&conn2);

    EXPECT_FLOAT_EQ(conn1.parietal_prefrontal, conn2.parietal_prefrontal);
    EXPECT_FLOAT_EQ(conn1.hippocampus_occipital, conn2.hippocampus_occipital);
    EXPECT_FLOAT_EQ(conn1.broca_wernicke, conn2.broca_wernicke);

    // Baseline should be 1.0
    EXPECT_FLOAT_EQ(1.0f, conn1.parietal_prefrontal);
    EXPECT_FLOAT_EQ(1.0f, conn1.callosum_cognitive_gain);
}

/**
 * Test: Lateralization initialization is deterministic
 */
TEST_F(GeniusRegressionTest, LateralizationInitialization) {
    genius_lateralization_t lat1, lat2;

    genius_lateralization_init_balanced(&lat1);
    genius_lateralization_init_balanced(&lat2);

    EXPECT_FLOAT_EQ(lat1.language_dominance, lat2.language_dominance);
    EXPECT_FLOAT_EQ(lat1.spatial_dominance, lat2.spatial_dominance);
    EXPECT_FLOAT_EQ(lat1.logical_reasoning_dominance, lat2.logical_reasoning_dominance);

    // Language should be left-biased (>0.5)
    EXPECT_GT(lat1.language_dominance, 0.5f);

    // Spatial should be right-biased (<0.5)
    EXPECT_LT(lat1.spatial_dominance, 0.5f);
}

/* ============================================================================
 * TYPE NAME/DESCRIPTION CONSISTENCY TESTS
 * ============================================================================ */

/**
 * Test: Type names are consistent
 */
TEST_F(GeniusRegressionTest, TypeNameConsistency) {
    EXPECT_STREQ("Mathematical", genius_type_name(GENIUS_TYPE_MATHEMATICAL));
    EXPECT_STREQ("Visual/Artistic", genius_type_name(GENIUS_TYPE_VISUAL_ARTISTIC));
    EXPECT_STREQ("Musical", genius_type_name(GENIUS_TYPE_MUSICAL));
    EXPECT_STREQ("Literary", genius_type_name(GENIUS_TYPE_LITERARY));
    EXPECT_STREQ("Scientific", genius_type_name(GENIUS_TYPE_SCIENTIFIC));
    EXPECT_STREQ("Athletic", genius_type_name(GENIUS_TYPE_ATHLETIC));
    EXPECT_STREQ("Strategic", genius_type_name(GENIUS_TYPE_STRATEGIC));
    EXPECT_STREQ("Financial", genius_type_name(GENIUS_TYPE_FINANCIAL));
    EXPECT_STREQ("Polymath", genius_type_name(GENIUS_TYPE_POLYMATH));

    EXPECT_STREQ("UNKNOWN", genius_type_name(GENIUS_TYPE_INVALID));
    EXPECT_STREQ("UNKNOWN", genius_type_name(static_cast<genius_type_t>(999)));
}

/**
 * Test: Type exemplars are consistent
 */
TEST_F(GeniusRegressionTest, TypeExemplarConsistency) {
    // Mathematical should include Gauss
    const char* math_exemplars = genius_type_exemplars(GENIUS_TYPE_MATHEMATICAL);
    EXPECT_NE(nullptr, strstr(math_exemplars, "Gauss"));

    // Musical should include Mozart
    const char* music_exemplars = genius_type_exemplars(GENIUS_TYPE_MUSICAL);
    EXPECT_NE(nullptr, strstr(music_exemplars, "Mozart"));

    // Scientific should include Tesla
    const char* sci_exemplars = genius_type_exemplars(GENIUS_TYPE_SCIENTIFIC);
    EXPECT_NE(nullptr, strstr(sci_exemplars, "Tesla"));

    // Financial should include Buffett
    const char* fin_exemplars = genius_type_exemplars(GENIUS_TYPE_FINANCIAL);
    EXPECT_NE(nullptr, strstr(fin_exemplars, "Buffett"));
}

/**
 * Test: State names are consistent
 */
TEST_F(GeniusRegressionTest, StateNameConsistency) {
    EXPECT_STREQ("Inactive", genius_state_name(GENIUS_STATE_INACTIVE));
    EXPECT_STREQ("Activating", genius_state_name(GENIUS_STATE_ACTIVATING));
    EXPECT_STREQ("Active", genius_state_name(GENIUS_STATE_ACTIVE));
    EXPECT_STREQ("Blended", genius_state_name(GENIUS_STATE_BLENDED));
    EXPECT_STREQ("Fatigued", genius_state_name(GENIUS_STATE_FATIGUED));
    EXPECT_STREQ("Recovering", genius_state_name(GENIUS_STATE_RECOVERING));
    EXPECT_STREQ("Flow", genius_state_name(GENIUS_STATE_FLOW));
    EXPECT_STREQ("Degraded", genius_state_name(GENIUS_STATE_DEGRADED));
    EXPECT_STREQ("Error", genius_state_name(GENIUS_STATE_ERROR));
}

/* ============================================================================
 * BIO-ASYNC MESSAGE CONSTANT CONSISTENCY
 * ============================================================================ */

/**
 * Test: Bio-async message constants are in valid range
 */
TEST_F(GeniusRegressionTest, BioAsyncMessageRangeConsistency) {
    // All message types should be in 0x1F00-0x1FFF range
    EXPECT_GE(BIO_MSG_GENIUS_PROFILE_ACTIVATE, 0x1F00);
    EXPECT_LE(BIO_MSG_GENIUS_PROFILE_ACTIVATE, 0x1FFF);

    EXPECT_GE(BIO_MSG_GENIUS_EIDETIC_ENCODE, 0x1F00);
    EXPECT_LE(BIO_MSG_GENIUS_EIDETIC_ENCODE, 0x1FFF);

    EXPECT_GE(BIO_MSG_GENIUS_TRAINING_START, 0x1F00);
    EXPECT_LE(BIO_MSG_GENIUS_TRAINING_START, 0x1FFF);

    EXPECT_GE(BIO_MSG_GENIUS_MESH_PROPOSE, 0x1F00);
    EXPECT_LE(BIO_MSG_GENIUS_MESH_PROPOSE, 0x1FFF);

    EXPECT_EQ(0x1FFF, BIO_MSG_GENIUS_MAX);
}

/**
 * Test: Module ID is consistent
 */
TEST_F(GeniusRegressionTest, ModuleIdConsistency) {
    EXPECT_EQ(0x1F00, BIO_MODULE_GENIUS_PROFILES);
    EXPECT_STREQ("genius_profiles", GENIUS_PROFILES_MODULE_NAME);
}

}  // namespace regression
}  // namespace test
}  // namespace nimcp
