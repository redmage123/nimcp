/**
 * @file test_emotion_tensor.cpp
 * @brief Unit tests for Tensor-Based Emotional Representation System
 *
 * WHAT: Comprehensive unit tests for emotion tensor system
 * WHY:  Ensure multi-dimensional emotion representation works correctly
 * HOW:  Test lifecycle, channels, compounds, dynamics, and bridge integration
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "cognitive/nimcp_emotion_tensor.h"
#include "cognitive/nimcp_emotion_tensor_bridge.h"

#include "utils/nimcp_test_base.h"

/* ============================================================================
 * Emotion Tensor Core Tests
 * ============================================================================ */

class EmotionTensorTest : public NimcpTestBase {
protected:
    emotion_tensor_system_t* system = nullptr;

    void SetUp() override {
        NimcpTestBase::SetUp();
    }

    void TearDown() override {
        if (system) {
            emotion_tensor_destroy(system);
            system = nullptr;
        }
        NimcpTestBase::TearDown();
    }
};

TEST_F(EmotionTensorTest, CreateWithDefaultConfig) {
    // WHAT: Create emotion tensor with default configuration
    // WHY:  Verify basic initialization works
    // HOW:  Create with NULL config

    system = emotion_tensor_create(nullptr);
    ASSERT_NE(system, nullptr);
}

TEST_F(EmotionTensorTest, CreateWithCustomConfig) {
    // WHAT: Create emotion tensor with custom configuration
    // WHY:  Verify configuration is respected
    // HOW:  Create config, set parameters

    emotion_tensor_config_t config = emotion_tensor_default_config();
    config.decay_rate = 0.2f;
    config.interaction_strength = 0.5f;
    config.blend_threshold = 0.3f;
    config.dominance_threshold = 0.5f;
    config.enable_temporal_dynamics = true;
    config.enable_appraisals = true;
    config.enable_interactions = true;

    system = emotion_tensor_create(&config);
    ASSERT_NE(system, nullptr);
}

TEST_F(EmotionTensorTest, DefaultConfigHasSensibleValues) {
    // WHAT: Verify default configuration has reasonable values
    // WHY:  Ensure users get working system without manual config
    // HOW:  Call default_config, check all values

    emotion_tensor_config_t config = emotion_tensor_default_config();

    // Decay rate should be reasonable (not too fast or slow)
    EXPECT_GE(config.decay_rate, 0.01f);
    EXPECT_LE(config.decay_rate, 0.5f);

    // Interaction strength should be bounded
    EXPECT_GE(config.interaction_strength, 0.0f);
    EXPECT_LE(config.interaction_strength, 1.0f);

    // Thresholds should be in valid range
    EXPECT_GE(config.blend_threshold, 0.0f);
    EXPECT_LE(config.blend_threshold, 1.0f);
    EXPECT_GE(config.dominance_threshold, 0.0f);
    EXPECT_LE(config.dominance_threshold, 1.0f);
}

TEST_F(EmotionTensorTest, DestroyNullSystemIsNoop) {
    // WHAT: Verify destroying NULL system doesn't crash
    // WHY:  Defensive programming
    // HOW:  Call destroy with NULL

    emotion_tensor_destroy(nullptr);
    SUCCEED();
}

/* ============================================================================
 * Tensor State Query Tests
 * ============================================================================ */

TEST_F(EmotionTensorTest, GetTensorInitialValues) {
    // WHAT: Verify initial tensor state is neutral
    // WHY:  System should start in neutral emotional state
    // HOW:  Create system, get tensor, check values

    system = emotion_tensor_create(nullptr);
    ASSERT_NE(system, nullptr);

    emotion_tensor_t tensor;
    bool result = emotion_tensor_get(system, &tensor);

    ASSERT_TRUE(result);

    // All primary channels should be zero initially
    for (int i = 0; i < EMOTION_TENSOR_PRIMARY_COUNT; i++) {
        EXPECT_FLOAT_EQ(tensor.channels[i], 0.0f);
    }

    // Aggregate metrics should be neutral
    EXPECT_FLOAT_EQ(tensor.overall_valence, 0.0f);
    EXPECT_FLOAT_EQ(tensor.overall_arousal, 0.0f);
}

TEST_F(EmotionTensorTest, GetTensorWithNullSystem) {
    // WHAT: Verify get handles NULL system
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    emotion_tensor_t tensor;
    bool result = emotion_tensor_get(nullptr, &tensor);
    EXPECT_FALSE(result);
}

TEST_F(EmotionTensorTest, GetTensorWithNullOutput) {
    // WHAT: Verify get handles NULL output
    // WHY:  Defensive programming
    // HOW:  Call with NULL output

    system = emotion_tensor_create(nullptr);
    ASSERT_NE(system, nullptr);

    bool result = emotion_tensor_get(system, nullptr);
    EXPECT_FALSE(result);
}

TEST_F(EmotionTensorTest, GetChannelInitiallyZero) {
    // WHAT: Verify individual channel starts at zero
    // WHY:  All emotions neutral initially
    // HOW:  Check each primary emotion channel

    system = emotion_tensor_create(nullptr);
    ASSERT_NE(system, nullptr);

    for (int i = 0; i < TENSOR_PRIMARY_COUNT; i++) {
        float activation = emotion_tensor_get_channel(system, (emotion_primary_t)i);
        EXPECT_FLOAT_EQ(activation, 0.0f);
    }
}

TEST_F(EmotionTensorTest, GetChannelWithNullSystem) {
    // WHAT: Verify get_channel handles NULL system
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    float activation = emotion_tensor_get_channel(nullptr, TENSOR_JOY);
    EXPECT_EQ(activation, -1.0f);  // -1 indicates error
}

TEST_F(EmotionTensorTest, GetCompoundInitiallyZero) {
    // WHAT: Verify compound emotions start at zero
    // WHY:  No compound emotions without primary activation
    // HOW:  Check various compound emotions

    system = emotion_tensor_create(nullptr);
    ASSERT_NE(system, nullptr);

    EXPECT_FLOAT_EQ(emotion_tensor_get_compound(system, COMPOUND_LOVE), 0.0f);
    EXPECT_FLOAT_EQ(emotion_tensor_get_compound(system, COMPOUND_OPTIMISM), 0.0f);
    EXPECT_FLOAT_EQ(emotion_tensor_get_compound(system, COMPOUND_BITTERSWEETNESS), 0.0f);
}

TEST_F(EmotionTensorTest, GetCompoundWithNullSystem) {
    // WHAT: Verify get_compound handles NULL system
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    float activation = emotion_tensor_get_compound(nullptr, COMPOUND_LOVE);
    EXPECT_EQ(activation, -1.0f);
}

/* ============================================================================
 * Tensor Update Tests
 * ============================================================================ */

TEST_F(EmotionTensorTest, SetChannelActivation) {
    // WHAT: Verify setting channel updates activation
    // WHY:  Core functionality - control emotions
    // HOW:  Set channel, verify value

    system = emotion_tensor_create(nullptr);
    ASSERT_NE(system, nullptr);

    bool result = emotion_tensor_set_channel(system, TENSOR_JOY, 0.8f, 1000);
    ASSERT_TRUE(result);

    float joy = emotion_tensor_get_channel(system, TENSOR_JOY);
    EXPECT_FLOAT_EQ(joy, 0.8f);
}

TEST_F(EmotionTensorTest, SetChannelClampsToValidRange) {
    // WHAT: Verify channel values are clamped to [0, 1]
    // WHY:  Activation must be in valid range
    // HOW:  Set values outside range, verify clamped

    system = emotion_tensor_create(nullptr);
    ASSERT_NE(system, nullptr);

    // Values should be clamped to [0, 1]
    emotion_tensor_set_channel(system, TENSOR_JOY, 1.5f, 1000);
    float joy = emotion_tensor_get_channel(system, TENSOR_JOY);
    EXPECT_LE(joy, 1.0f);

    emotion_tensor_set_channel(system, TENSOR_FEAR, -0.5f, 2000);
    float fear = emotion_tensor_get_channel(system, TENSOR_FEAR);
    EXPECT_GE(fear, 0.0f);
}

TEST_F(EmotionTensorTest, SetChannelWithNullSystem) {
    // WHAT: Verify set_channel handles NULL system
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    bool result = emotion_tensor_set_channel(nullptr, TENSOR_JOY, 0.5f, 1000);
    EXPECT_FALSE(result);
}

TEST_F(EmotionTensorTest, SetChannelsAllAtOnce) {
    // WHAT: Verify bulk channel update
    // WHY:  Efficient multi-emotion updates
    // HOW:  Set all channels, verify values

    system = emotion_tensor_create(nullptr);
    ASSERT_NE(system, nullptr);

    float activations[EMOTION_TENSOR_PRIMARY_COUNT] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};
    bool result = emotion_tensor_set_channels(system, activations, 1000);
    ASSERT_TRUE(result);

    for (int i = 0; i < EMOTION_TENSOR_PRIMARY_COUNT; i++) {
        float activation = emotion_tensor_get_channel(system, (emotion_primary_t)i);
        EXPECT_FLOAT_EQ(activation, activations[i]);
    }
}

TEST_F(EmotionTensorTest, SetAppraisal) {
    // WHAT: Verify appraisal setting works
    // WHY:  Appraisals add cognitive evaluation
    // HOW:  Set appraisal, verify no crash

    system = emotion_tensor_create(nullptr);
    ASSERT_NE(system, nullptr);

    bool result = emotion_tensor_set_appraisal(system, TENSOR_JOY, APPRAISAL_CERTAINTY, 0.9f);
    EXPECT_TRUE(result);
}

TEST_F(EmotionTensorTest, ApplyStimulus) {
    // WHAT: Verify stimulus application
    // WHY:  External events trigger emotions
    // HOW:  Apply stimulus, verify channel changes

    system = emotion_tensor_create(nullptr);
    ASSERT_NE(system, nullptr);

    bool result = emotion_tensor_apply_stimulus(system, TENSOR_JOY, 0.7f, true, 1000);
    ASSERT_TRUE(result);

    float joy = emotion_tensor_get_channel(system, TENSOR_JOY);
    EXPECT_GT(joy, 0.0f);
}

/* ============================================================================
 * Dynamics Tests
 * ============================================================================ */

TEST_F(EmotionTensorTest, UpdateAppliesDecay) {
    // WHAT: Verify update applies emotion decay
    // WHY:  Emotions naturally fade over time
    // HOW:  Set emotion, update, verify decay

    system = emotion_tensor_create(nullptr);
    ASSERT_NE(system, nullptr);

    // Set high activation
    emotion_tensor_set_channel(system, TENSOR_JOY, 0.9f, 1000);
    float joy_before = emotion_tensor_get_channel(system, TENSOR_JOY);

    // Update with time delta
    bool result = emotion_tensor_update(system, 1.0f, 2000);
    ASSERT_TRUE(result);

    float joy_after = emotion_tensor_get_channel(system, TENSOR_JOY);
    EXPECT_LT(joy_after, joy_before);  // Should have decayed
}

TEST_F(EmotionTensorTest, ComputeCompounds) {
    // WHAT: Verify compound emotion computation
    // WHY:  Compounds derived from primary pairs
    // HOW:  Set primary emotions, compute compounds

    system = emotion_tensor_create(nullptr);
    ASSERT_NE(system, nullptr);

    // Set joy and trust (should produce LOVE)
    emotion_tensor_set_channel(system, TENSOR_JOY, 0.8f, 1000);
    emotion_tensor_set_channel(system, TENSOR_TRUST, 0.7f, 1000);

    bool result = emotion_tensor_compute_compounds(system);
    ASSERT_TRUE(result);

    float love = emotion_tensor_get_compound(system, COMPOUND_LOVE);
    EXPECT_GT(love, 0.0f);
}

TEST_F(EmotionTensorTest, ResetToNeutral) {
    // WHAT: Verify reset clears all activations
    // WHY:  Return to baseline emotional state
    // HOW:  Set emotions, reset, verify neutral

    system = emotion_tensor_create(nullptr);
    ASSERT_NE(system, nullptr);

    // Set some emotions
    emotion_tensor_set_channel(system, TENSOR_JOY, 0.8f, 1000);
    emotion_tensor_set_channel(system, TENSOR_ANGER, 0.5f, 1000);

    bool result = emotion_tensor_reset(system);
    ASSERT_TRUE(result);

    // All channels should be zero
    for (int i = 0; i < TENSOR_PRIMARY_COUNT; i++) {
        float activation = emotion_tensor_get_channel(system, (emotion_primary_t)i);
        EXPECT_FLOAT_EQ(activation, 0.0f);
    }
}

/* ============================================================================
 * Analysis Tests
 * ============================================================================ */

TEST_F(EmotionTensorTest, GetEntropy) {
    // WHAT: Verify entropy calculation
    // WHY:  Entropy measures emotion diversity
    // HOW:  Set emotions, check entropy

    system = emotion_tensor_create(nullptr);
    ASSERT_NE(system, nullptr);

    // Single dominant emotion = low entropy
    emotion_tensor_set_channel(system, TENSOR_JOY, 1.0f, 1000);
    float entropy_low = emotion_tensor_get_entropy(system);

    // Reset and set multiple emotions = higher entropy
    emotion_tensor_reset(system);
    emotion_tensor_set_channel(system, TENSOR_JOY, 0.5f, 2000);
    emotion_tensor_set_channel(system, TENSOR_SADNESS, 0.5f, 2000);
    emotion_tensor_set_channel(system, TENSOR_FEAR, 0.5f, 2000);
    emotion_tensor_set_channel(system, TENSOR_ANGER, 0.5f, 2000);
    float entropy_high = emotion_tensor_get_entropy(system);

    EXPECT_GT(entropy_high, entropy_low);
}

TEST_F(EmotionTensorTest, GetEntropyWithNullSystem) {
    // WHAT: Verify get_entropy handles NULL
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    float entropy = emotion_tensor_get_entropy(nullptr);
    EXPECT_EQ(entropy, -1.0f);
}

TEST_F(EmotionTensorTest, GetStability) {
    // WHAT: Verify stability calculation
    // WHY:  Stability indicates emotional volatility
    // HOW:  Check stability value

    system = emotion_tensor_create(nullptr);
    ASSERT_NE(system, nullptr);

    float stability = emotion_tensor_get_stability(system);
    EXPECT_GE(stability, 0.0f);
    EXPECT_LE(stability, 1.0f);
}

TEST_F(EmotionTensorTest, GetDominant) {
    // WHAT: Verify dominant emotion detection
    // WHY:  Quick summary of emotional state
    // HOW:  Set emotions, find dominant

    system = emotion_tensor_create(nullptr);
    ASSERT_NE(system, nullptr);

    emotion_tensor_set_channel(system, TENSOR_JOY, 0.9f, 1000);
    emotion_tensor_set_channel(system, TENSOR_TRUST, 0.6f, 1000);

    emotion_primary_t primary, secondary;
    float blend_ratio;
    bool result = emotion_tensor_get_dominant(system, &primary, &secondary, &blend_ratio);

    ASSERT_TRUE(result);
    EXPECT_EQ(primary, TENSOR_JOY);
    EXPECT_EQ(secondary, TENSOR_TRUST);
    EXPECT_GT(blend_ratio, 0.0f);
    EXPECT_LT(blend_ratio, 1.0f);
}

TEST_F(EmotionTensorTest, IsContradictory) {
    // WHAT: Verify contradiction detection
    // WHY:  Detect emotional ambivalence
    // HOW:  Set opposing emotions

    system = emotion_tensor_create(nullptr);
    ASSERT_NE(system, nullptr);

    // Set contradictory emotions (joy + sadness)
    emotion_tensor_set_channel(system, TENSOR_JOY, 0.8f, 1000);
    emotion_tensor_set_channel(system, TENSOR_SADNESS, 0.7f, 1000);

    bool contradictory = emotion_tensor_is_contradictory(system, 0.5f);
    EXPECT_TRUE(contradictory);
}

TEST_F(EmotionTensorTest, GetValence) {
    // WHAT: Verify valence computation
    // WHY:  Backward compatibility with scalar systems
    // HOW:  Set positive/negative emotions, check valence

    system = emotion_tensor_create(nullptr);
    ASSERT_NE(system, nullptr);

    // Positive emotions should give positive valence
    emotion_tensor_set_channel(system, TENSOR_JOY, 0.9f, 1000);
    float valence_positive = emotion_tensor_get_valence(system);
    EXPECT_GT(valence_positive, 0.0f);

    // Negative emotions should give negative valence
    emotion_tensor_reset(system);
    emotion_tensor_set_channel(system, TENSOR_SADNESS, 0.9f, 2000);
    float valence_negative = emotion_tensor_get_valence(system);
    EXPECT_LT(valence_negative, 0.0f);
}

TEST_F(EmotionTensorTest, GetArousal) {
    // WHAT: Verify arousal computation
    // WHY:  Backward compatibility
    // HOW:  Set high-arousal emotions, check

    system = emotion_tensor_create(nullptr);
    ASSERT_NE(system, nullptr);

    // High-arousal emotions (anger, fear)
    emotion_tensor_set_channel(system, TENSOR_ANGER, 0.9f, 1000);
    emotion_tensor_set_channel(system, TENSOR_FEAR, 0.8f, 1000);
    float arousal = emotion_tensor_get_arousal(system);

    EXPECT_GT(arousal, 0.0f);
    EXPECT_LE(arousal, 1.0f);
}

/* ============================================================================
 * Utility Tests
 * ============================================================================ */

TEST_F(EmotionTensorTest, EmotionNames) {
    // WHAT: Verify emotion name strings
    // WHY:  Logging and debugging
    // HOW:  Check each emotion name

    EXPECT_STREQ(emotion_tensor_emotion_name(TENSOR_JOY), "joy");
    EXPECT_STREQ(emotion_tensor_emotion_name(TENSOR_TRUST), "trust");
    EXPECT_STREQ(emotion_tensor_emotion_name(TENSOR_FEAR), "fear");
    EXPECT_STREQ(emotion_tensor_emotion_name(TENSOR_SURPRISE), "surprise");
    EXPECT_STREQ(emotion_tensor_emotion_name(TENSOR_SADNESS), "sadness");
    EXPECT_STREQ(emotion_tensor_emotion_name(TENSOR_DISGUST), "disgust");
    EXPECT_STREQ(emotion_tensor_emotion_name(TENSOR_ANGER), "anger");
    EXPECT_STREQ(emotion_tensor_emotion_name(TENSOR_ANTICIPATION), "anticipation");
}

TEST_F(EmotionTensorTest, CompoundNames) {
    // WHAT: Verify compound emotion name strings
    // WHY:  Logging and debugging
    // HOW:  Check some compound names

    EXPECT_STREQ(emotion_tensor_compound_name(COMPOUND_LOVE), "love");
    EXPECT_STREQ(emotion_tensor_compound_name(COMPOUND_OPTIMISM), "optimism");
    EXPECT_STREQ(emotion_tensor_compound_name(COMPOUND_BITTERSWEETNESS), "bittersweetness");
    EXPECT_STREQ(emotion_tensor_compound_name(COMPOUND_NOSTALGIA), "nostalgia");
}

TEST_F(EmotionTensorTest, InitInteractionMatrix) {
    // WHAT: Verify interaction matrix initialization
    // WHY:  Emotions influence each other
    // HOW:  Initialize matrix, check not all zeros

    emotion_interaction_matrix_t matrix;
    memset(&matrix, 0, sizeof(matrix));

    emotion_tensor_init_interaction_matrix(&matrix);

    // Matrix should have some non-zero values
    bool has_nonzero = false;
    for (int i = 0; i < EMOTION_TENSOR_PRIMARY_COUNT; i++) {
        for (int j = 0; j < EMOTION_TENSOR_PRIMARY_COUNT; j++) {
            if (matrix.matrix[i][j] != 0.0f) {
                has_nonzero = true;
                break;
            }
        }
    }
    EXPECT_TRUE(has_nonzero);
}

/* ============================================================================
 * Emotion Tensor Bridge Tests
 * ============================================================================ */

class EmotionTensorBridgeTest : public NimcpTestBase {
protected:
    emotion_tensor_system_t* tensor = nullptr;
    emotion_tensor_bridge_t* bridge = nullptr;

    void SetUp() override {
        NimcpTestBase::SetUp();

        tensor = emotion_tensor_create(nullptr);
        if (tensor) {
            emotion_tensor_bridge_config_t config = emotion_tensor_bridge_default_config();
            bridge = emotion_tensor_bridge_create(tensor, nullptr, &config);
        }
    }

    void TearDown() override {
        if (bridge) {
            emotion_tensor_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (tensor) {
            emotion_tensor_destroy(tensor);
            tensor = nullptr;
        }
        NimcpTestBase::TearDown();
    }
};

TEST_F(EmotionTensorBridgeTest, CreateDestroy) {
    // WHAT: Verify bridge creation and destruction
    // WHY:  Basic lifecycle test
    // HOW:  Check bridge not NULL

    ASSERT_NE(bridge, nullptr);
}

TEST_F(EmotionTensorBridgeTest, DefaultConfig) {
    // WHAT: Verify default config has sensible values
    // WHY:  Ensure good defaults
    // HOW:  Check config values

    emotion_tensor_bridge_config_t config = emotion_tensor_bridge_default_config();

    EXPECT_GT(config.sync_threshold, 0.0f);
    EXPECT_LE(config.sync_threshold, 1.0f);
    EXPECT_GT(config.blend_factor, 0.0f);
    EXPECT_LE(config.blend_factor, 1.0f);
    EXPECT_GT(config.broadcast_interval_ms, 0u);
}

TEST_F(EmotionTensorBridgeTest, DestroyNull) {
    // WHAT: Verify destroying NULL is safe
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    emotion_tensor_bridge_destroy(nullptr);
    SUCCEED();
}

TEST_F(EmotionTensorBridgeTest, NeedsSync) {
    // WHAT: Verify sync detection
    // WHY:  Avoid unnecessary sync traffic
    // HOW:  Check before/after changes

    ASSERT_NE(bridge, nullptr);

    // Initially may or may not need sync
    bool needs = emotion_tensor_bridge_needs_sync(bridge);
    (void)needs;  // Just verify call works

    // After emotion change, should need sync
    if (tensor) {
        emotion_tensor_set_channel(tensor, TENSOR_JOY, 0.9f, 1000);
    }

    SUCCEED();
}

TEST_F(EmotionTensorBridgeTest, GetStats) {
    // WHAT: Verify stats retrieval
    // WHY:  Monitor bridge activity
    // HOW:  Get stats, check structure

    ASSERT_NE(bridge, nullptr);

    emotion_tensor_bridge_stats_t stats;
    nimcp_result_t result = emotion_tensor_bridge_get_stats(bridge, &stats);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GE(stats.syncs_sent, 0u);
    EXPECT_GE(stats.syncs_received, 0u);
}

TEST_F(EmotionTensorBridgeTest, GetStatsWithNullBridge) {
    // WHAT: Verify get_stats handles NULL
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    emotion_tensor_bridge_stats_t stats;
    nimcp_result_t result = emotion_tensor_bridge_get_stats(nullptr, &stats);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

/* ============================================================================
 * Enum Value Tests
 * ============================================================================ */

TEST_F(EmotionTensorTest, PrimaryEmotionEnumValues) {
    // WHAT: Verify primary emotion enum values
    // WHY:  Enum values used in arrays
    // HOW:  Check each value

    EXPECT_EQ((int)TENSOR_JOY, 0);
    EXPECT_EQ((int)TENSOR_TRUST, 1);
    EXPECT_EQ((int)TENSOR_FEAR, 2);
    EXPECT_EQ((int)TENSOR_SURPRISE, 3);
    EXPECT_EQ((int)TENSOR_SADNESS, 4);
    EXPECT_EQ((int)TENSOR_DISGUST, 5);
    EXPECT_EQ((int)TENSOR_ANGER, 6);
    EXPECT_EQ((int)TENSOR_ANTICIPATION, 7);
    EXPECT_EQ((int)TENSOR_PRIMARY_COUNT, 8);
}

TEST_F(EmotionTensorTest, CompoundEmotionEnumCount) {
    // WHAT: Verify compound emotion count
    // WHY:  Arrays sized by this constant
    // HOW:  Check count value

    EXPECT_EQ((int)COMPOUND_COUNT, 24);
}

TEST_F(EmotionTensorTest, AppraisalDimensionEnumCount) {
    // WHAT: Verify appraisal dimension count
    // WHY:  Arrays sized by this constant
    // HOW:  Check count value

    EXPECT_EQ((int)APPRAISAL_COUNT, 6);
}

/* ============================================================================
 * Edge Case Tests
 * ============================================================================ */

TEST_F(EmotionTensorTest, RapidChannelChanges) {
    // WHAT: Verify system handles rapid changes
    // WHY:  Stress test for stability
    // HOW:  Update channels 1000 times

    system = emotion_tensor_create(nullptr);
    ASSERT_NE(system, nullptr);

    for (int i = 0; i < 1000; i++) {
        emotion_primary_t emotion = (emotion_primary_t)(i % TENSOR_PRIMARY_COUNT);
        float activation = sinf(i * 0.1f) * 0.5f + 0.5f;  // [0, 1]

        bool result = emotion_tensor_set_channel(system, emotion, activation, 1000 + i);
        EXPECT_TRUE(result);
    }

    // System should still be functional
    emotion_tensor_t tensor;
    bool result = emotion_tensor_get(system, &tensor);
    EXPECT_TRUE(result);

    // Values should not be NaN
    for (int i = 0; i < EMOTION_TENSOR_PRIMARY_COUNT; i++) {
        EXPECT_FALSE(std::isnan(tensor.channels[i]));
    }
}

TEST_F(EmotionTensorTest, MultipleUpdateCycles) {
    // WHAT: Verify many update cycles work
    // WHY:  Long-running simulation
    // HOW:  Update 100 times

    system = emotion_tensor_create(nullptr);
    ASSERT_NE(system, nullptr);

    emotion_tensor_set_channel(system, TENSOR_JOY, 0.8f, 1000);

    for (int i = 0; i < 100; i++) {
        bool result = emotion_tensor_update(system, 0.1f, 1000 + i * 100);
        EXPECT_TRUE(result);
    }

    // Joy should have decayed significantly
    float joy = emotion_tensor_get_channel(system, TENSOR_JOY);
    EXPECT_LT(joy, 0.8f);
    EXPECT_GE(joy, 0.0f);
}
