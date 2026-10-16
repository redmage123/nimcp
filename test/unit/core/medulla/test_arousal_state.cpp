/**
 * @file test_arousal_state.cpp
 * @brief Unit tests for the arousal state module
 *
 * WHAT: Tests for arousal state management with hysteresis
 * WHY:  Ensure stable arousal transitions and state management
 * HOW:  Use GoogleTest framework with state transition validation
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "core/medulla/nimcp_arousal_state.h"
#include "utils/error/nimcp_error_codes.h"

//=============================================================================
// Test Fixture
//=============================================================================

class ArousalStateTest : public ::testing::Test {
protected:
    arousal_state_t* arousal = nullptr;

    void SetUp() override {
        arousal_state_config_t config;
        arousal_state_default_config(&config);
        arousal = arousal_state_create(&config);
        ASSERT_NE(arousal, nullptr);
    }

    void TearDown() override {
        if (arousal) {
            arousal_state_destroy(arousal);
            arousal = nullptr;
        }
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(ArousalStateTest, DefaultConfig) {
    arousal_state_config_t config;
    int result = arousal_state_default_config(&config);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Verify default values are set
    EXPECT_GT(config.hysteresis_margin, 0.0f);
    EXPECT_GT(config.min_dwell_time_ms, 0.0f);
    EXPECT_GT(config.max_rate_per_sec, 0.0f);
}

TEST_F(ArousalStateTest, CreateWithNullConfig) {
    // Should use defaults when config is NULL
    arousal_state_t* a = arousal_state_create(nullptr);
    EXPECT_NE(a, nullptr);
    if (a) arousal_state_destroy(a);
}

TEST_F(ArousalStateTest, DestroyNull) {
    // Should not crash with NULL
    arousal_state_destroy(nullptr);
}

//=============================================================================
// State Query Tests
//=============================================================================

TEST_F(ArousalStateTest, GetState) {
    arousal_state_enum_t state;
    int result = arousal_state_get_state(arousal, &state);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GE((int)state, 0);
    EXPECT_LT((int)state, (int)AROUSAL_STATE_COUNT);
}

TEST_F(ArousalStateTest, GetLevel) {
    float level;
    int result = arousal_state_get_level(arousal, &level);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GE(level, 0.0f);
    EXPECT_LE(level, 1.0f);
}

TEST_F(ArousalStateTest, GetStateNullOutput) {
    int result = arousal_state_get_state(arousal, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ArousalStateTest, GetLevelNullOutput) {
    int result = arousal_state_get_level(arousal, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

//=============================================================================
// Stimulus Tests
//=============================================================================

TEST_F(ArousalStateTest, ApplyStimulus) {
    float initial;
    arousal_state_get_level(arousal, &initial);

    // Apply positive stimulus
    int result = arousal_state_apply_stimulus(arousal, 0.2f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(ArousalStateTest, ApplyStimulusNullState) {
    int result = arousal_state_apply_stimulus(nullptr, 0.2f);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

//=============================================================================
// Target Level Tests
//=============================================================================

TEST_F(ArousalStateTest, SetTarget) {
    int result = arousal_state_set_target(arousal, 0.7f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(ArousalStateTest, SetTargetClamped) {
    // Set out of range target - function rejects invalid values
    int result = arousal_state_set_target(arousal, 1.5f);
    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAMETER);  // Rejected, not clamped
}

TEST_F(ArousalStateTest, SetTargetNullState) {
    int result = arousal_state_set_target(nullptr, 0.5f);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

//=============================================================================
// Update Tests
//=============================================================================

TEST_F(ArousalStateTest, Update) {
    int result = arousal_state_update(arousal, 100.0f);  // 100ms delta
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(ArousalStateTest, UpdateNullState) {
    int result = arousal_state_update(nullptr, 100.0f);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

//=============================================================================
// State Name Tests
//=============================================================================

TEST_F(ArousalStateTest, StateNames) {
    const char* name = arousal_state_get_state_name(AROUSAL_STATE_COMA);
    EXPECT_NE(name, nullptr);
    EXPECT_STRNE(name, "");

    name = arousal_state_get_state_name(AROUSAL_STATE_PANIC);
    EXPECT_NE(name, nullptr);
    EXPECT_STRNE(name, "");

    name = arousal_state_get_state_name(AROUSAL_STATE_RELAXED);
    EXPECT_NE(name, nullptr);
    EXPECT_STRNE(name, "");
}

//=============================================================================
// Bio-async Tests
//=============================================================================

TEST_F(ArousalStateTest, BioAsyncConnection) {
    // Initially not connected
    bool connected = arousal_state_is_bio_async_connected(arousal);
    EXPECT_FALSE(connected);

    // Try to connect (may fail if router not available)
    int result = arousal_state_connect_bio_async(arousal);
    // Result depends on whether bio-async router is initialized

    // Disconnect
    arousal_state_disconnect_bio_async(arousal);
    connected = arousal_state_is_bio_async_connected(arousal);
    EXPECT_FALSE(connected);
}

TEST_F(ArousalStateTest, BioAsyncNullState) {
    bool connected = arousal_state_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);
}

//=============================================================================
// Continuous Parameter Computation Tests
//=============================================================================

TEST_F(ArousalStateTest, ComputeParametersNullOutput) {
    int result = arousal_compute_parameters(0.5f, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ArousalStateTest, ComputeParametersSuccess) {
    arousal_params_t params;
    int result = arousal_compute_parameters(0.5f, &params);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(ArousalStateTest, ComputeParametersClampsInput) {
    arousal_params_t params_low, params_high;

    // Values below 0 should clamp to 0
    int result = arousal_compute_parameters(-0.5f, &params_low);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    arousal_params_t params_zero;
    arousal_compute_parameters(0.0f, &params_zero);

    EXPECT_FLOAT_EQ(params_low.cortical_activation, params_zero.cortical_activation);
    EXPECT_FLOAT_EQ(params_low.cognitive_gain, params_zero.cognitive_gain);

    // Values above 1 should clamp to 1
    result = arousal_compute_parameters(1.5f, &params_high);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    arousal_params_t params_one;
    arousal_compute_parameters(1.0f, &params_one);

    EXPECT_FLOAT_EQ(params_high.cortical_activation, params_one.cortical_activation);
    EXPECT_FLOAT_EQ(params_high.cognitive_gain, params_one.cognitive_gain);
}

TEST_F(ArousalStateTest, AllParametersInValidRange) {
    // Test at multiple points across the full range
    float test_levels[] = {0.0f, 0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f};

    for (float level : test_levels) {
        arousal_params_t params;
        int result = arousal_compute_parameters(level, &params);
        ASSERT_EQ(result, NIMCP_SUCCESS) << "Failed at level " << level;

        EXPECT_GE(params.cortical_activation, 0.0f) << "cortical_activation < 0 at level " << level;
        EXPECT_LE(params.cortical_activation, 1.0f) << "cortical_activation > 1 at level " << level;

        EXPECT_GE(params.cognitive_gain, 0.0f) << "cognitive_gain < 0 at level " << level;
        EXPECT_LE(params.cognitive_gain, 1.0f) << "cognitive_gain > 1 at level " << level;

        EXPECT_GE(params.sensory_gating, 0.0f) << "sensory_gating < 0 at level " << level;
        EXPECT_LE(params.sensory_gating, 1.0f) << "sensory_gating > 1 at level " << level;

        EXPECT_GE(params.muscle_tone, 0.0f) << "muscle_tone < 0 at level " << level;
        EXPECT_LE(params.muscle_tone, 1.0f) << "muscle_tone > 1 at level " << level;

        EXPECT_GE(params.metabolic_rate, 0.0f) << "metabolic_rate < 0 at level " << level;
        EXPECT_LE(params.metabolic_rate, 1.1f) << "metabolic_rate > 1.1 at level " << level;

        EXPECT_GE(params.vigilance_factor, 0.0f) << "vigilance_factor < 0 at level " << level;
        EXPECT_LE(params.vigilance_factor, 1.0f) << "vigilance_factor > 1 at level " << level;

        EXPECT_GE(params.emotional_reactivity, 0.0f) << "emotional_reactivity < 0 at level " << level;
        EXPECT_LE(params.emotional_reactivity, 1.0f) << "emotional_reactivity > 1 at level " << level;

        EXPECT_GE(params.memory_consolidation, 0.0f) << "memory_consolidation < 0 at level " << level;
        EXPECT_LE(params.memory_consolidation, 1.0f) << "memory_consolidation > 1 at level " << level;

        EXPECT_GE(params.free_energy, 0.0f) << "free_energy < 0 at level " << level;
        EXPECT_LE(params.free_energy, 1.0f) << "free_energy > 1 at level " << level;
    }
}

//=============================================================================
// Monotonic Scaling Tests (no binary jumps)
//=============================================================================

TEST_F(ArousalStateTest, CorticalActivationMonotonicallyIncreasing) {
    // Cortical activation should increase monotonically with arousal
    float prev_value = -1.0f;
    for (int i = 0; i <= 100; i++) {
        float level = (float)i / 100.0f;
        arousal_params_t params;
        arousal_compute_parameters(level, &params);

        EXPECT_GE(params.cortical_activation, prev_value)
            << "Cortical activation decreased at level " << level;
        prev_value = params.cortical_activation;
    }
}

TEST_F(ArousalStateTest, MuscleToneMonotonicallyIncreasing) {
    // Muscle tone should increase monotonically with arousal
    float prev_value = -1.0f;
    for (int i = 0; i <= 100; i++) {
        float level = (float)i / 100.0f;
        arousal_params_t params;
        arousal_compute_parameters(level, &params);

        EXPECT_GE(params.muscle_tone, prev_value)
            << "Muscle tone decreased at level " << level;
        prev_value = params.muscle_tone;
    }
}

TEST_F(ArousalStateTest, MetabolicRateMonotonicallyIncreasing) {
    // Metabolic rate: linear, should be strictly monotonic
    float prev_value = -1.0f;
    for (int i = 0; i <= 100; i++) {
        float level = (float)i / 100.0f;
        arousal_params_t params;
        arousal_compute_parameters(level, &params);

        EXPECT_GE(params.metabolic_rate, prev_value)
            << "Metabolic rate decreased at level " << level;
        prev_value = params.metabolic_rate;
    }
}

TEST_F(ArousalStateTest, VigilanceFactorMonotonicallyIncreasing) {
    float prev_value = -1.0f;
    for (int i = 0; i <= 100; i++) {
        float level = (float)i / 100.0f;
        arousal_params_t params;
        arousal_compute_parameters(level, &params);

        EXPECT_GE(params.vigilance_factor, prev_value)
            << "Vigilance factor decreased at level " << level;
        prev_value = params.vigilance_factor;
    }
}

TEST_F(ArousalStateTest, EmotionalReactivityMonotonicallyIncreasing) {
    float prev_value = -1.0f;
    for (int i = 0; i <= 100; i++) {
        float level = (float)i / 100.0f;
        arousal_params_t params;
        arousal_compute_parameters(level, &params);

        EXPECT_GE(params.emotional_reactivity, prev_value)
            << "Emotional reactivity decreased at level " << level;
        prev_value = params.emotional_reactivity;
    }
}

//=============================================================================
// Inverted-U (Yerkes-Dodson) Shape Tests
//=============================================================================

TEST_F(ArousalStateTest, CognitiveGainPeaksAtModerateArousal) {
    arousal_params_t params_low, params_mid, params_high;

    arousal_compute_parameters(0.1f, &params_low);
    arousal_compute_parameters(0.6f, &params_mid);   // Optimal
    arousal_compute_parameters(0.95f, &params_high);

    // Peak should be higher than both extremes
    EXPECT_GT(params_mid.cognitive_gain, params_low.cognitive_gain);
    EXPECT_GT(params_mid.cognitive_gain, params_high.cognitive_gain);
}

TEST_F(ArousalStateTest, FreeEnergyMinimalAtOptimalArousal) {
    arousal_params_t params_low, params_optimal, params_high;

    arousal_compute_parameters(0.05f, &params_low);
    arousal_compute_parameters(0.6f, &params_optimal);   // Alert = optimal
    arousal_compute_parameters(0.95f, &params_high);

    // Free energy should be lowest at optimal
    EXPECT_LT(params_optimal.free_energy, params_low.free_energy);
    EXPECT_LT(params_optimal.free_energy, params_high.free_energy);
}

TEST_F(ArousalStateTest, MemoryConsolidationPeaksAtLowModerateArousal) {
    arousal_params_t params_deep_sleep, params_drowsy, params_panic;

    arousal_compute_parameters(0.05f, &params_deep_sleep);
    arousal_compute_parameters(0.35f, &params_drowsy);   // Optimal for memory consolidation
    arousal_compute_parameters(0.95f, &params_panic);

    // Memory consolidation should peak around drowsy/light-sleep level
    EXPECT_GT(params_drowsy.memory_consolidation, params_deep_sleep.memory_consolidation);
    EXPECT_GT(params_drowsy.memory_consolidation, params_panic.memory_consolidation);
}

//=============================================================================
// Smooth Curve Tests (no step discontinuities)
//=============================================================================

TEST_F(ArousalStateTest, NoBinaryJumpsInParameters) {
    // Ensure no parameter changes by more than a small delta between adjacent
    // points. Step functions would show large jumps at boundaries.
    const float step = 0.01f;
    const float max_delta = 0.1f;  // No param should jump >0.1 in a 0.01 step

    arousal_params_t prev;
    arousal_compute_parameters(0.0f, &prev);

    for (float level = step; level <= 1.0f + 0.001f; level += step) {
        arousal_params_t curr;
        arousal_compute_parameters(level, &curr);

        EXPECT_LT(std::fabs(curr.cortical_activation - prev.cortical_activation), max_delta)
            << "Cortical activation jumped at level " << level;
        EXPECT_LT(std::fabs(curr.cognitive_gain - prev.cognitive_gain), max_delta)
            << "Cognitive gain jumped at level " << level;
        EXPECT_LT(std::fabs(curr.sensory_gating - prev.sensory_gating), max_delta)
            << "Sensory gating jumped at level " << level;
        EXPECT_LT(std::fabs(curr.muscle_tone - prev.muscle_tone), max_delta)
            << "Muscle tone jumped at level " << level;
        EXPECT_LT(std::fabs(curr.metabolic_rate - prev.metabolic_rate), max_delta)
            << "Metabolic rate jumped at level " << level;
        EXPECT_LT(std::fabs(curr.vigilance_factor - prev.vigilance_factor), max_delta)
            << "Vigilance factor jumped at level " << level;
        EXPECT_LT(std::fabs(curr.emotional_reactivity - prev.emotional_reactivity), max_delta)
            << "Emotional reactivity jumped at level " << level;
        EXPECT_LT(std::fabs(curr.memory_consolidation - prev.memory_consolidation), max_delta)
            << "Memory consolidation jumped at level " << level;
        EXPECT_LT(std::fabs(curr.free_energy - prev.free_energy), max_delta)
            << "Free energy jumped at level " << level;

        prev = curr;
    }
}

//=============================================================================
// Boundary Value Tests
//=============================================================================

TEST_F(ArousalStateTest, ComaLevelHasMinimalActivation) {
    arousal_params_t params;
    arousal_compute_parameters(0.0f, &params);

    EXPECT_LT(params.cortical_activation, 0.05f);  // Nearly zero
    EXPECT_LT(params.muscle_tone, 0.1f);            // Atonia
    EXPECT_NEAR(params.metabolic_rate, 0.3f, 0.01f); // Basal only
    EXPECT_LT(params.vigilance_factor, 0.05f);       // No vigilance
    EXPECT_LT(params.emotional_reactivity, 0.01f);   // No reactivity
}

TEST_F(ArousalStateTest, PanicLevelHasMaximalActivation) {
    arousal_params_t params;
    arousal_compute_parameters(1.0f, &params);

    EXPECT_GT(params.cortical_activation, 0.95f);   // Full activation
    EXPECT_GT(params.muscle_tone, 0.95f);            // Rigid
    EXPECT_NEAR(params.metabolic_rate, 1.0f, 0.01f); // Maximum
    EXPECT_GT(params.vigilance_factor, 0.9f);         // Maximum vigilance
    EXPECT_GT(params.emotional_reactivity, 0.5f);     // High reactivity
}

TEST_F(ArousalStateTest, AlertLevelHasOptimalCognition) {
    arousal_params_t params;
    arousal_compute_parameters(0.6f, &params);

    // At optimal arousal, cognitive gain should be near peak
    EXPECT_GT(params.cognitive_gain, 0.9f);
    // Free energy should be near minimum
    EXPECT_LT(params.free_energy, 0.1f);
}

//=============================================================================
// Label-from-Level Tests
//=============================================================================

TEST_F(ArousalStateTest, LabelFromLevelCoversAllStates) {
    // Verify all enum states can be produced from appropriate levels
    EXPECT_EQ(arousal_state_label_from_level(0.02f), AROUSAL_STATE_COMA);
    EXPECT_EQ(arousal_state_label_from_level(0.10f), AROUSAL_STATE_DEEP_SLEEP);
    EXPECT_EQ(arousal_state_label_from_level(0.20f), AROUSAL_STATE_LIGHT_SLEEP);
    EXPECT_EQ(arousal_state_label_from_level(0.35f), AROUSAL_STATE_DROWSY);
    EXPECT_EQ(arousal_state_label_from_level(0.47f), AROUSAL_STATE_RELAXED);
    EXPECT_EQ(arousal_state_label_from_level(0.62f), AROUSAL_STATE_ALERT);
    EXPECT_EQ(arousal_state_label_from_level(0.77f), AROUSAL_STATE_VIGILANT);
    EXPECT_EQ(arousal_state_label_from_level(0.90f), AROUSAL_STATE_HYPERAROUSED);
    EXPECT_EQ(arousal_state_label_from_level(0.97f), AROUSAL_STATE_PANIC);
}

TEST_F(ArousalStateTest, LabelFromLevelClampsOutOfRange) {
    // Below 0 should produce COMA
    EXPECT_EQ(arousal_state_label_from_level(-0.5f), AROUSAL_STATE_COMA);
    // Above 1 should produce PANIC
    EXPECT_EQ(arousal_state_label_from_level(1.5f), AROUSAL_STATE_PANIC);
}

//=============================================================================
// State Manager Convenience Wrapper Tests
//=============================================================================

TEST_F(ArousalStateTest, GetParametersFromManagerNullState) {
    arousal_params_t params;
    int result = arousal_state_get_parameters(nullptr, &params);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ArousalStateTest, GetParametersFromManagerNullOutput) {
    int result = arousal_state_get_parameters(arousal, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ArousalStateTest, GetParametersFromManagerSuccess) {
    arousal_params_t params;
    int result = arousal_state_get_parameters(arousal, &params);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Initial state is RELAXED (level ~0.47), verify reasonable values
    EXPECT_GT(params.cortical_activation, 0.5f);
    EXPECT_GT(params.cognitive_gain, 0.3f);
    EXPECT_GT(params.metabolic_rate, 0.5f);
}

TEST_F(ArousalStateTest, GetParametersMatchesDirectCompute) {
    // Verify the convenience wrapper returns the same results as
    // calling arousal_compute_parameters directly with the same level
    float level;
    arousal_state_get_level(arousal, &level);

    arousal_params_t from_manager, from_direct;
    arousal_state_get_parameters(arousal, &from_manager);
    arousal_compute_parameters(level, &from_direct);

    EXPECT_FLOAT_EQ(from_manager.cortical_activation, from_direct.cortical_activation);
    EXPECT_FLOAT_EQ(from_manager.cognitive_gain, from_direct.cognitive_gain);
    EXPECT_FLOAT_EQ(from_manager.sensory_gating, from_direct.sensory_gating);
    EXPECT_FLOAT_EQ(from_manager.muscle_tone, from_direct.muscle_tone);
    EXPECT_FLOAT_EQ(from_manager.metabolic_rate, from_direct.metabolic_rate);
    EXPECT_FLOAT_EQ(from_manager.vigilance_factor, from_direct.vigilance_factor);
    EXPECT_FLOAT_EQ(from_manager.emotional_reactivity, from_direct.emotional_reactivity);
    EXPECT_FLOAT_EQ(from_manager.memory_consolidation, from_direct.memory_consolidation);
    EXPECT_FLOAT_EQ(from_manager.free_energy, from_direct.free_energy);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
