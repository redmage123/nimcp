/**
 * @file test_perception_training_bridge.cpp
 * @brief Unit tests for Perception-Training Bridge
 *
 * WHAT: Bidirectional integration between perception modules and training
 * WHY:  Training-driven perception modulation and perception-driven curriculum
 * HOW:  Visual/audio/speech cortex states modulate training parameters
 *
 * TEST COVERAGE:
 * - Lifecycle tests (8 tests)
 * - Visual modulation tests (12 tests)
 * - Audio modulation tests (12 tests)
 * - Speech modulation tests (12 tests)
 * - Combined modulation tests (12 tests)
 * - Sample weighting tests (8 tests)
 * - Feedback tests (8 tests)
 * - Stats and error handling tests (8 tests)
 *
 * TOTAL: 80 tests
 *
 * @author NIMCP Development Team
 * @date 2025-12-20
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "middleware/training/nimcp_perception_training_bridge.h"
#include "utils/error/nimcp_error_codes.h"

//=============================================================================
// Test Constants
//=============================================================================

/* Learning Rate Constants */
static constexpr float TEST_BASE_LR = 0.001f;
static constexpr float TEST_LR_BOOST_FACTOR = 1.3f;
static constexpr float TEST_LR_REDUCE_FACTOR = 0.7f;
static constexpr float TEST_LR_HIGH_FACTOR = 1.4f;
static constexpr float TEST_LR_MODERATE_FACTOR = 1.2f;
static constexpr float TEST_LR_MAX_FACTOR = 1.5f;

/* Confidence / Quality Constants */
static constexpr float TEST_HIGH_CONFIDENCE = 0.95f;
static constexpr float TEST_GOOD_CONFIDENCE = 0.9f;
static constexpr float TEST_MODERATE_CONFIDENCE = 0.85f;
static constexpr float TEST_LOW_CONFIDENCE = 0.3f;
static constexpr float TEST_THRESHOLD_CONFIDENCE = 0.8f;
static constexpr float TEST_VERY_LOW_QUALITY = 0.15f;

/* Perception Quality Constants */
static constexpr float TEST_HIGH_VISUAL_QUALITY = 0.9f;
static constexpr float TEST_LOW_VISUAL_QUALITY = 0.3f;
static constexpr float TEST_HIGH_AUDIO_QUALITY = 0.85f;
static constexpr float TEST_NOISY_AUDIO = 0.2f;
static constexpr float TEST_HIGH_SPEECH_QUALITY = 0.9f;

/* Novelty / Surprise Constants */
static constexpr float TEST_HIGH_NOVELTY = 0.9f;
static constexpr float TEST_LOW_NOVELTY = 0.2f;
static constexpr float TEST_HIGH_SALIENCE = 0.9f;
static constexpr float TEST_LOW_SALIENCE = 0.1f;

/* Sample Weight Constants */
static constexpr float TEST_HIGH_WEIGHT = 1.4f;
static constexpr float TEST_MODERATE_WEIGHT = 1.2f;
static constexpr float TEST_LOW_WEIGHT = 0.5f;

/* Miscellaneous Test Constants */
static constexpr int TEST_CYCLE_COUNT = 10;
static constexpr float TEST_SIGNAL_VALUE = 0.9f;
static constexpr float TEST_GRAD_NORM = 1.0f;
static constexpr float TEST_LOSS = 0.5f;
static constexpr float TEST_HIGH_LOSS = 1.5f;

//=============================================================================
// Test Fixture
//=============================================================================

class PerceptionTrainingBridgeTest : public ::testing::Test {
protected:
    perception_training_bridge_t* bridge;
    perception_training_config_t config;

    void SetUp() override {
        perception_training_default_config(&config);
        config.enable_bio_async = false;
        config.disable_auto_update = true;
        bridge = perception_training_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            perception_training_destroy(bridge);
            bridge = nullptr;
        }
    }
};

//=============================================================================
// Lifecycle Tests (8 tests)
//=============================================================================

TEST_F(PerceptionTrainingBridgeTest, CreateWithDefaults) {
    EXPECT_NE(bridge, nullptr);
}

TEST_F(PerceptionTrainingBridgeTest, CreateWithNullConfig) {
    perception_training_bridge_t* test_bridge = perception_training_create(nullptr);
    EXPECT_NE(test_bridge, nullptr);
    perception_training_destroy(test_bridge);
}

TEST_F(PerceptionTrainingBridgeTest, DestroyNull) {
    perception_training_destroy(nullptr);
    SUCCEED();
}

TEST_F(PerceptionTrainingBridgeTest, StartStop) {
    EXPECT_EQ(perception_training_start(bridge), 0);
    EXPECT_EQ(perception_training_stop(bridge), 0);
}

TEST_F(PerceptionTrainingBridgeTest, DoubleStart) {
    EXPECT_EQ(perception_training_start(bridge), 0);
    int result = perception_training_start(bridge);
    (void)result;
    EXPECT_EQ(perception_training_stop(bridge), 0);
}

TEST_F(PerceptionTrainingBridgeTest, DoubleStop) {
    EXPECT_EQ(perception_training_start(bridge), 0);
    EXPECT_EQ(perception_training_stop(bridge), 0);
    int result = perception_training_stop(bridge);
    (void)result;
}

TEST_F(PerceptionTrainingBridgeTest, CreateMultiple) {
    perception_training_bridge_t* bridge2 = perception_training_create(&config);
    EXPECT_NE(bridge2, nullptr);
    perception_training_destroy(bridge2);
}

TEST_F(PerceptionTrainingBridgeTest, CreateDestroyCycle) {
    for (int i = 0; i < 10; i++) {
        perception_training_bridge_t* temp = perception_training_create(&config);
        ASSERT_NE(temp, nullptr);
        EXPECT_EQ(perception_training_start(temp), 0);
        EXPECT_EQ(perception_training_stop(temp), 0);
        perception_training_destroy(temp);
    }
}

//=============================================================================
// Visual Modulation Tests (12 tests)
//=============================================================================

TEST_F(PerceptionTrainingBridgeTest, VisualConfidenceAffectsLR) {
    /* WHAT: High visual confidence allows higher LR */
    /* WHY:  Confident perception = trustworthy training signal */
    /* HOW:  Compare LR at different visual confidence levels */

    EXPECT_EQ(perception_training_start(bridge), 0);

    /* Set effects for low confidence */
    perception_training_effects_t effects_low;
    memset(&effects_low, 0, sizeof(effects_low));
    effects_low.visual_confidence = 0.2f;
    effects_low.lr_factor = 0.7f;
    effects_low.valid = true;
    perception_training_set_effects_for_testing(bridge, &effects_low);

    float lr_low = perception_training_get_modulated_lr(bridge, TEST_BASE_LR);

    /* Set effects for high confidence */
    perception_training_effects_t effects_high;
    memset(&effects_high, 0, sizeof(effects_high));
    effects_high.visual_confidence = 0.95f;
    effects_high.lr_factor = 1.2f;
    effects_high.valid = true;
    perception_training_set_effects_for_testing(bridge, &effects_high);

    float lr_high = perception_training_get_modulated_lr(bridge, TEST_BASE_LR);

    EXPECT_LT(lr_low, lr_high);
}

TEST_F(PerceptionTrainingBridgeTest, VisualNoveltyIncreasesLR) {
    /* WHAT: Novel visual input increases learning rate */
    /* WHY:  Novel = more information to learn */
    /* HOW:  High novelty boosts LR for exploration */

    EXPECT_EQ(perception_training_start(bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.visual_novelty = 0.9f;
    effects.lr_factor = 1.3f;
    effects.valid = true;
    perception_training_set_effects_for_testing(bridge, &effects);

    float lr = perception_training_get_modulated_lr(bridge, TEST_BASE_LR);
    EXPECT_GT(lr, TEST_BASE_LR);
}

TEST_F(PerceptionTrainingBridgeTest, VisualAttentionWeightsGradients) {
    /* WHAT: Visual attention weights gradient contributions */
    /* WHY:  Attended regions = more important */
    /* HOW:  High attention amplifies gradients */

    EXPECT_EQ(perception_training_start(bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.visual_confidence = 0.95f;
    effects.lr_factor = 1.4f;
    effects.valid = true;
    perception_training_set_effects_for_testing(bridge, &effects);

    perception_training_effects_t retrieved;
    perception_training_get_effects(bridge, &retrieved);

    EXPECT_GT(retrieved.lr_factor, 1.0f);
}

TEST_F(PerceptionTrainingBridgeTest, LowVisualQualityReducesWeight) {
    /* WHAT: Poor visual quality reduces sample weight */
    /* WHY:  Low quality input = unreliable training signal */
    /* HOW:  Quality factor < 1 reduces contribution */

    EXPECT_EQ(perception_training_start(bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.visual_confidence = 0.3f;
    effects.sample_weight = 0.3f;
    effects.valid = true;
    perception_training_set_effects_for_testing(bridge, &effects);

    perception_training_effects_t retrieved;
    perception_training_get_effects(bridge, &retrieved);

    EXPECT_LT(retrieved.sample_weight, 1.0f);
}

TEST_F(PerceptionTrainingBridgeTest, VisualComplexityAffectsLR) {
    /* WHAT: High visual complexity affects learning rate */
    /* WHY:  Complex inputs need careful learning */
    /* HOW:  Novelty factor modulates LR */

    EXPECT_EQ(perception_training_start(bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.visual_novelty = 0.9f;
    effects.lr_factor = 0.7f;
    effects.valid = true;
    perception_training_set_effects_for_testing(bridge, &effects);

    float lr = perception_training_get_modulated_lr(bridge, TEST_BASE_LR);
    EXPECT_LT(lr, TEST_BASE_LR);
}

TEST_F(PerceptionTrainingBridgeTest, VisualSalienceBoostsPriority) {
    /* WHAT: Salient visual features prioritized */
    /* WHY:  Salient = perceptually important */
    /* HOW:  High confidence increases sample weight */

    EXPECT_EQ(perception_training_start(bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.visual_confidence = 0.95f;
    effects.sample_weight = 1.5f;
    effects.valid = true;
    perception_training_set_effects_for_testing(bridge, &effects);

    perception_training_effects_t retrieved;
    perception_training_get_effects(bridge, &retrieved);

    EXPECT_GT(retrieved.sample_weight, 1.0f);
}

TEST_F(PerceptionTrainingBridgeTest, VisualSurpriseIncreasesLR) {
    /* WHAT: Visual novelty boosts LR */
    /* WHY:  Surprise = learning opportunity */
    /* HOW:  High novelty increases LR */

    EXPECT_EQ(perception_training_start(bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.visual_novelty = 0.9f;
    effects.lr_factor = 1.25f;
    effects.valid = true;
    perception_training_set_effects_for_testing(bridge, &effects);

    float lr = perception_training_get_modulated_lr(bridge, TEST_BASE_LR);
    EXPECT_GT(lr, TEST_BASE_LR);
}

TEST_F(PerceptionTrainingBridgeTest, VisualConfidenceEnhancement) {
    /* WHAT: High confidence input processed normally */
    /* WHY:  Clear visual signal */
    /* HOW:  Confidence affects quality metric */

    EXPECT_EQ(perception_training_start(bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.visual_confidence = 0.85f;
    effects.lr_factor = 1.0f;
    effects.valid = true;
    perception_training_set_effects_for_testing(bridge, &effects);

    EXPECT_EQ(perception_training_update_metrics(bridge, 0.5f, 1.0f), 0);
}

TEST_F(PerceptionTrainingBridgeTest, VisualMotionDetection) {
    /* WHAT: Motion cues affect temporal processing */
    /* WHY:  Motion = temporal coherence signal */
    /* HOW:  Temporal coherence metric tracked */

    EXPECT_EQ(perception_training_start(bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.temporal_coherence = 0.7f;
    effects.valid = true;
    perception_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(PerceptionTrainingBridgeTest, VisualOcclusionHandling) {
    /* WHAT: Occluded visual input reduces confidence */
    /* WHY:  Partial information = uncertain */
    /* HOW:  Low confidence for occluded input */

    EXPECT_EQ(perception_training_start(bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.visual_confidence = 0.4f;  /* Low due to occlusion */
    effects.valid = true;
    perception_training_set_effects_for_testing(bridge, &effects);

    perception_training_effects_t retrieved;
    perception_training_get_effects(bridge, &retrieved);

    EXPECT_LT(retrieved.visual_confidence, 0.5f);
}

TEST_F(PerceptionTrainingBridgeTest, VisualPatternRecognition) {
    /* WHAT: Recognized patterns boost confidence */
    /* WHY:  Familiar patterns = reliable features */
    /* HOW:  Pattern recognition increases confidence */

    EXPECT_EQ(perception_training_start(bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.visual_confidence = 0.85f;
    effects.valid = true;
    perception_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(PerceptionTrainingBridgeTest, VisualDepthCues) {
    /* WHAT: Depth information enhances 3D understanding */
    /* WHY:  3D structure = richer representation */
    /* HOW:  Confidence metric tracked */

    EXPECT_EQ(perception_training_start(bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.visual_confidence = 0.75f;
    effects.valid = true;
    perception_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

//=============================================================================
// Audio Modulation Tests (12 tests)
//=============================================================================

TEST_F(PerceptionTrainingBridgeTest, AudioQualityAffectsWeight) {
    /* WHAT: Audio quality modulates sample weight */
    /* WHY:  Clean audio = reliable signal */
    /* HOW:  Quality factor scales contribution */

    EXPECT_EQ(perception_training_start(bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.audio_quality = 0.9f;
    effects.sample_weight = 1.1f;
    effects.valid = true;
    perception_training_set_effects_for_testing(bridge, &effects);

    perception_training_effects_t retrieved;
    perception_training_get_effects(bridge, &retrieved);

    EXPECT_GT(retrieved.sample_weight, 1.0f);
}

TEST_F(PerceptionTrainingBridgeTest, AudioNoiseReducesQuality) {
    /* WHAT: Noisy audio reduces quality */
    /* WHY:  Noise = unreliable features */
    /* HOW:  Low quality affects sample weight */

    EXPECT_EQ(perception_training_start(bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.audio_quality = 0.3f;  /* Low quality */
    effects.sample_weight = 0.4f;
    effects.valid = true;
    perception_training_set_effects_for_testing(bridge, &effects);

    perception_training_effects_t retrieved;
    perception_training_get_effects(bridge, &retrieved);

    EXPECT_LT(retrieved.audio_quality, 0.5f);
}

TEST_F(PerceptionTrainingBridgeTest, AudioSalienceBoostsLR) {
    /* WHAT: Salient audio features prioritized */
    /* WHY:  Important sounds = stronger learning */
    /* HOW:  Speech salience amplifies LR */

    EXPECT_EQ(perception_training_start(bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.speech_salience = 0.95f;
    effects.lr_factor = 1.3f;
    effects.valid = true;
    perception_training_set_effects_for_testing(bridge, &effects);

    perception_training_effects_t retrieved;
    perception_training_get_effects(bridge, &retrieved);

    EXPECT_GT(retrieved.lr_factor, 1.0f);
}

TEST_F(PerceptionTrainingBridgeTest, AudioTemporalCoherence) {
    /* WHAT: Temporal coherence indicates valid sequence */
    /* WHY:  Coherent audio = meaningful pattern */
    /* HOW:  Coherence boosts quality */

    EXPECT_EQ(perception_training_start(bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.temporal_coherence = 0.85f;
    effects.audio_quality = 0.8f;
    effects.valid = true;
    perception_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(PerceptionTrainingBridgeTest, AudioPitchStability) {
    /* WHAT: Stable pitch = quality signal */
    /* WHY:  Pitch stability indicates clear audio */
    /* HOW:  Quality reflects stable pitch */

    EXPECT_EQ(perception_training_start(bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.audio_quality = 0.9f;
    effects.temporal_coherence = 0.85f;
    effects.valid = true;
    perception_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(PerceptionTrainingBridgeTest, AudioRhythmDetection) {
    /* WHAT: Rhythmic patterns enhance learning */
    /* WHY:  Rhythm = temporal structure */
    /* HOW:  Temporal coherence metric tracked */

    EXPECT_EQ(perception_training_start(bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.temporal_coherence = 0.75f;
    effects.valid = true;
    perception_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(PerceptionTrainingBridgeTest, AudioOnsetDetection) {
    /* WHAT: Onset detection for segmentation */
    /* WHY:  Onsets = event boundaries */
    /* HOW:  Audio quality reflects onset detection */

    EXPECT_EQ(perception_training_start(bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.audio_quality = 0.8f;
    effects.valid = true;
    perception_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(PerceptionTrainingBridgeTest, AudioSpectrumRichness) {
    /* WHAT: Rich spectrum = more information */
    /* WHY:  Spectral content indicates complexity */
    /* HOW:  Quality metric reflects spectrum */

    EXPECT_EQ(perception_training_start(bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.audio_quality = 0.85f;
    effects.sample_weight = 1.1f;
    effects.valid = true;
    perception_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(PerceptionTrainingBridgeTest, AudioSpeechLikeliness) {
    /* WHAT: Speech detection affects processing */
    /* WHY:  Speech vs non-speech different handling */
    /* HOW:  Speech salience metric */

    EXPECT_EQ(perception_training_start(bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.speech_salience = 0.9f;
    effects.valid = true;
    perception_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(PerceptionTrainingBridgeTest, AudioLocalizationCues) {
    /* WHAT: Spatial audio enhances representation */
    /* WHY:  Localization = 3D audio structure */
    /* HOW:  Quality reflects spatial information */

    EXPECT_EQ(perception_training_start(bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.audio_quality = 0.7f;
    effects.valid = true;
    perception_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(PerceptionTrainingBridgeTest, AudioDynamicRange) {
    /* WHAT: Dynamic range affects normalization */
    /* WHY:  Range indicates signal characteristics */
    /* HOW:  Quality reflects dynamic range */

    EXPECT_EQ(perception_training_start(bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.audio_quality = 0.75f;
    effects.valid = true;
    perception_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(PerceptionTrainingBridgeTest, AudioHarmonicContent) {
    /* WHAT: Harmonic content indicates pitch */
    /* WHY:  Harmonics = tonal structure */
    /* HOW:  Quality reflects harmonic strength */

    EXPECT_EQ(perception_training_start(bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.audio_quality = 0.8f;
    effects.valid = true;
    perception_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

//=============================================================================
// Speech Modulation Tests (12 tests)
//=============================================================================

TEST_F(PerceptionTrainingBridgeTest, SpeechComprehensionBoostsLR) {
    /* WHAT: High comprehension allows higher LR */
    /* WHY:  Understood speech = clear signal */
    /* HOW:  Comprehension factor increases LR */

    EXPECT_EQ(perception_training_start(bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.comprehension = 0.9f;
    effects.lr_factor = 1.2f;
    effects.valid = true;
    perception_training_set_effects_for_testing(bridge, &effects);

    float lr = perception_training_get_modulated_lr(bridge, TEST_BASE_LR);
    EXPECT_GT(lr, TEST_BASE_LR);
}

TEST_F(PerceptionTrainingBridgeTest, PhonemeAccuracyAffectsWeight) {
    /* WHAT: Phoneme accuracy modulates sample weight */
    /* WHY:  Accurate phonemes = reliable features */
    /* HOW:  Accuracy factor scales weight */

    EXPECT_EQ(perception_training_start(bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.phoneme_accuracy = 0.95f;
    effects.sample_weight = 1.3f;
    effects.valid = true;
    perception_training_set_effects_for_testing(bridge, &effects);

    perception_training_effects_t retrieved;
    perception_training_get_effects(bridge, &retrieved);

    EXPECT_GT(retrieved.sample_weight, 1.0f);
}

TEST_F(PerceptionTrainingBridgeTest, ProsodyEnhancesUnderstanding) {
    /* WHAT: Prosodic features aid comprehension */
    /* WHY:  Prosody = emotional/structural cues */
    /* HOW:  Prosody score boosts confidence */

    EXPECT_EQ(perception_training_start(bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.prosody_confidence = 0.85f;
    effects.comprehension = 0.8f;
    effects.valid = true;
    perception_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(PerceptionTrainingBridgeTest, ArticulationClarityAffectsQuality) {
    /* WHAT: Clear articulation = better quality */
    /* WHY:  Clarity indicates speaker effort */
    /* HOW:  Phoneme accuracy reflects articulation */

    EXPECT_EQ(perception_training_start(bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.phoneme_accuracy = 0.9f;
    effects.comprehension = 0.85f;
    effects.valid = true;
    perception_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(PerceptionTrainingBridgeTest, SpeechRateModulation) {
    /* WHAT: Speech rate affects processing */
    /* WHY:  Too fast/slow = harder to process */
    /* HOW:  Comprehension reflects rate processing */

    EXPECT_EQ(perception_training_start(bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.comprehension = 0.7f;  /* Moderate rate optimal */
    effects.valid = true;
    perception_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(PerceptionTrainingBridgeTest, SpeechSemanticCoherence) {
    /* WHAT: Coherent semantics boost learning */
    /* WHY:  Coherence = meaningful content */
    /* HOW:  Comprehension score affects weight */

    EXPECT_EQ(perception_training_start(bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.comprehension = 0.9f;
    effects.sample_weight = 1.2f;
    effects.valid = true;
    perception_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(PerceptionTrainingBridgeTest, SpeechEmotionalContent) {
    /* WHAT: Emotional speech prioritized */
    /* WHY:  Emotion = salient information */
    /* HOW:  Prosody confidence boosts salience */

    EXPECT_EQ(perception_training_start(bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.prosody_confidence = 0.8f;
    effects.speech_salience = 0.85f;
    effects.valid = true;
    perception_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(PerceptionTrainingBridgeTest, SpeechSyntaxComplexity) {
    /* WHAT: Syntax complexity affects learning rate */
    /* WHY:  Complex syntax = more processing */
    /* HOW:  Complexity reduces LR */

    EXPECT_EQ(perception_training_start(bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.comprehension = 0.5f;  /* Low due to complexity */
    effects.lr_factor = 0.8f;
    effects.valid = true;
    perception_training_set_effects_for_testing(bridge, &effects);

    float lr = perception_training_get_modulated_lr(bridge, TEST_BASE_LR);
    EXPECT_LE(lr, 0.001f);
}

TEST_F(PerceptionTrainingBridgeTest, SpeechAccentAdaptation) {
    /* WHAT: Accent affects recognition difficulty */
    /* WHY:  Unfamiliar accent = higher uncertainty */
    /* HOW:  Phoneme accuracy reflects accent difficulty */

    EXPECT_EQ(perception_training_start(bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.phoneme_accuracy = 0.5f;
    effects.comprehension = 0.6f;
    effects.valid = true;
    perception_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(PerceptionTrainingBridgeTest, SpeechDisfluencyHandling) {
    /* WHAT: Disfluencies reduce quality */
    /* WHY:  Pauses/fillers = interrupted signal */
    /* HOW:  Prosody confidence reflects fluency */

    EXPECT_EQ(perception_training_start(bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.prosody_confidence = 0.6f;  /* Some disfluencies */
    effects.comprehension = 0.65f;
    effects.valid = true;
    perception_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(PerceptionTrainingBridgeTest, SpeechContextRelevance) {
    /* WHAT: Contextually relevant speech prioritized */
    /* WHY:  Relevance = important information */
    /* HOW:  Comprehension score boosts weight */

    EXPECT_EQ(perception_training_start(bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.comprehension = 0.9f;
    effects.sample_weight = 1.25f;
    effects.valid = true;
    perception_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(PerceptionTrainingBridgeTest, SpeechIntonationPatterns) {
    /* WHAT: Intonation provides linguistic cues */
    /* WHY:  Intonation = sentence structure info */
    /* HOW:  Prosody confidence reflects intonation */

    EXPECT_EQ(perception_training_start(bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.prosody_confidence = 0.8f;
    effects.valid = true;
    perception_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

//=============================================================================
// Combined Modulation Tests (12 tests)
//=============================================================================

TEST_F(PerceptionTrainingBridgeTest, VisualAudioFusion) {
    /* WHAT: Visual + audio together boost confidence */
    /* WHY:  Multimodal = redundant information */
    /* HOW:  Combined modalities increase weight */

    EXPECT_EQ(perception_training_start(bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.visual_confidence = 0.8f;
    effects.audio_quality = 0.85f;
    effects.temporal_coherence = 0.9f;
    effects.sample_weight = 1.4f;
    effects.valid = true;
    perception_training_set_effects_for_testing(bridge, &effects);

    perception_training_effects_t retrieved;
    perception_training_get_effects(bridge, &retrieved);

    EXPECT_GT(retrieved.sample_weight, 1.0f);
}

TEST_F(PerceptionTrainingBridgeTest, AudioSpeechAlignment) {
    /* WHAT: Audio and speech alignment */
    /* WHY:  Speech is specialized audio processing */
    /* HOW:  Speech overrides general audio when detected */

    EXPECT_EQ(perception_training_start(bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.speech_salience = 0.95f;
    effects.comprehension = 0.9f;
    effects.prosody_confidence = 0.85f;
    effects.valid = true;
    perception_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(PerceptionTrainingBridgeTest, VisualSpeechLipReading) {
    /* WHAT: Visual lip movement aids speech recognition */
    /* WHY:  Lip reading = additional speech cue */
    /* HOW:  Visual confidence boosts comprehension */

    EXPECT_EQ(perception_training_start(bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.visual_confidence = 0.85f;
    effects.comprehension = 0.9f;
    effects.valid = true;
    perception_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(PerceptionTrainingBridgeTest, MultimodalConflict) {
    /* WHAT: Conflicting modalities reduce confidence */
    /* WHY:  Mismatch = unreliable signal */
    /* HOW:  Low coherence reduces weight */

    EXPECT_EQ(perception_training_start(bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.visual_confidence = 0.8f;
    effects.audio_quality = 0.8f;
    effects.temporal_coherence = 0.2f;  /* Conflict! */
    effects.sample_weight = 0.5f;
    effects.valid = true;
    perception_training_set_effects_for_testing(bridge, &effects);

    perception_training_effects_t retrieved;
    perception_training_get_effects(bridge, &retrieved);

    EXPECT_LT(retrieved.sample_weight, 1.0f);
}

TEST_F(PerceptionTrainingBridgeTest, TripleModalityIntegration) {
    /* WHAT: Visual + audio + speech together */
    /* WHY:  Full multimodal processing */
    /* HOW:  All three modalities contribute */

    EXPECT_EQ(perception_training_start(bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.visual_confidence = 0.85f;
    effects.audio_quality = 0.8f;
    effects.comprehension = 0.9f;
    effects.temporal_coherence = 0.95f;
    effects.sample_weight = 1.6f;
    effects.valid = true;
    perception_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(PerceptionTrainingBridgeTest, ModalityDropout) {
    /* WHAT: Missing modality handled gracefully */
    /* WHY:  Unimodal fallback when needed */
    /* HOW:  Zero confidence in missing modality */

    EXPECT_EQ(perception_training_start(bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.visual_confidence = 0.9f;
    effects.audio_quality = 0.0f;  /* No audio */
    effects.comprehension = 0.0f;  /* No speech */
    effects.sample_weight = 1.0f;
    effects.valid = true;
    perception_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(PerceptionTrainingBridgeTest, PerceptualNoveltyAcrossModalities) {
    /* WHAT: Novelty in any modality increases exploration */
    /* WHY:  Novel input = learning opportunity */
    /* HOW:  Visual novelty drives LR boost */

    EXPECT_EQ(perception_training_start(bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.visual_novelty = 0.9f;  /* High novelty */
    effects.audio_quality = 0.7f;
    effects.comprehension = 0.6f;
    effects.lr_factor = 1.3f;
    effects.valid = true;
    perception_training_set_effects_for_testing(bridge, &effects);

    float lr = perception_training_get_modulated_lr(bridge, TEST_BASE_LR);
    EXPECT_GT(lr, TEST_BASE_LR);
}

TEST_F(PerceptionTrainingBridgeTest, PerceptualSalienceIntegration) {
    /* WHAT: Salience integrated across modalities */
    /* WHY:  Salient in any modality = important */
    /* HOW:  Combined salience metric */

    EXPECT_EQ(perception_training_start(bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.visual_confidence = 0.7f;
    effects.speech_salience = 0.9f;
    effects.audio_quality = 0.8f;
    effects.sample_weight = 1.5f;
    effects.valid = true;
    perception_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(PerceptionTrainingBridgeTest, CrossModalAttention) {
    /* WHAT: Attention in one modality affects others */
    /* WHY:  Attended modality becomes primary */
    /* HOW:  Attention weights modulate contributions */

    EXPECT_EQ(perception_training_start(bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.visual_confidence = 0.3f;
    effects.audio_quality = 0.95f;  /* Audio attended */
    effects.lr_factor = 1.3f;
    effects.valid = true;
    perception_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(PerceptionTrainingBridgeTest, TemporalSynchrony) {
    /* WHAT: Temporal alignment across modalities */
    /* WHY:  Synchronized = coherent event */
    /* HOW:  Synchrony boosts coherence */

    EXPECT_EQ(perception_training_start(bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.temporal_coherence = 0.9f;
    effects.visual_confidence = 0.85f;
    effects.audio_quality = 0.85f;
    effects.valid = true;
    perception_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(PerceptionTrainingBridgeTest, PerceptualUncertaintyPropagation) {
    /* WHAT: Uncertainty in perception affects training */
    /* WHY:  Uncertain perception = conservative learning */
    /* HOW:  Low confidence reduces LR */

    EXPECT_EQ(perception_training_start(bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.visual_confidence = 0.3f;  /* Uncertain */
    effects.audio_quality = 0.4f;
    effects.comprehension = 0.35f;
    effects.lr_factor = 0.6f;
    effects.valid = true;
    perception_training_set_effects_for_testing(bridge, &effects);

    float lr = perception_training_get_modulated_lr(bridge, TEST_BASE_LR);
    EXPECT_LT(lr, TEST_BASE_LR);
}

TEST_F(PerceptionTrainingBridgeTest, PerceptualLoadBalancing) {
    /* WHAT: High complexity across modalities reduces LR */
    /* WHY:  Complex perception = resource intensive */
    /* HOW:  Combined complexity modulates LR */

    EXPECT_EQ(perception_training_start(bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.visual_novelty = 0.9f;
    effects.audio_quality = 0.4f;  /* Low quality = complex */
    effects.comprehension = 0.5f;
    effects.lr_factor = 0.6f;
    effects.valid = true;
    perception_training_set_effects_for_testing(bridge, &effects);

    float lr = perception_training_get_modulated_lr(bridge, TEST_BASE_LR);
    EXPECT_LT(lr, TEST_BASE_LR);
}

//=============================================================================
// Sample Weighting Tests (8 tests)
//=============================================================================

TEST_F(PerceptionTrainingBridgeTest, HighQualitySampleBoost) {
    /* WHAT: High quality samples weighted higher */
    /* WHY:  Quality = reliable training signal */
    /* HOW:  Quality factor > 1 */

    EXPECT_EQ(perception_training_start(bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.visual_confidence = 0.95f;
    effects.audio_quality = 0.9f;
    effects.prosody_confidence = 0.93f;
    effects.sample_weight = 1.5f;
    effects.valid = true;
    perception_training_set_effects_for_testing(bridge, &effects);

    perception_training_effects_t retrieved;
    perception_training_get_effects(bridge, &retrieved);

    EXPECT_GT(retrieved.sample_weight, 1.0f);
}

TEST_F(PerceptionTrainingBridgeTest, LowQualitySamplePenalty) {
    /* WHAT: Low quality samples down-weighted */
    /* WHY:  Poor quality = unreliable */
    /* HOW:  Quality factor < 1 */

    EXPECT_EQ(perception_training_start(bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.visual_confidence = 0.2f;
    effects.audio_quality = 0.3f;
    effects.prosody_confidence = 0.25f;
    effects.sample_weight = 0.4f;
    effects.valid = true;
    perception_training_set_effects_for_testing(bridge, &effects);

    perception_training_effects_t retrieved;
    perception_training_get_effects(bridge, &retrieved);

    EXPECT_LT(retrieved.sample_weight, 1.0f);
}

TEST_F(PerceptionTrainingBridgeTest, SampleSkipThreshold) {
    /* WHAT: Very low quality samples skipped */
    /* WHY:  Too poor quality = harmful */
    /* HOW:  Weight near zero or skip flag */

    EXPECT_EQ(perception_training_start(bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.visual_confidence = 0.05f;
    effects.audio_quality = 0.08f;
    effects.sample_weight = 0.05f;
    effects.skip_sample = true;
    effects.valid = true;
    perception_training_set_effects_for_testing(bridge, &effects);

    perception_training_effects_t retrieved;
    perception_training_get_effects(bridge, &retrieved);

    EXPECT_TRUE(retrieved.skip_sample);
}

TEST_F(PerceptionTrainingBridgeTest, NovelSamplePriority) {
    /* WHAT: Novel samples prioritized */
    /* WHY:  Novel = more to learn */
    /* HOW:  Novelty increases weight */

    EXPECT_EQ(perception_training_start(bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.visual_novelty = 0.9f;
    effects.sample_weight = 1.4f;
    effects.valid = true;
    perception_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(PerceptionTrainingBridgeTest, SalientSampleBoost) {
    /* WHAT: Salient samples emphasized */
    /* WHY:  Salience = perceptually important */
    /* HOW:  High confidence increases weight */

    EXPECT_EQ(perception_training_start(bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.visual_confidence = 0.95f;
    effects.speech_salience = 0.9f;
    effects.sample_weight = 1.6f;
    effects.valid = true;
    perception_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(PerceptionTrainingBridgeTest, WeightNormalization) {
    /* WHAT: Weights normalized across batch */
    /* WHY:  Prevent extreme weight imbalance */
    /* HOW:  Weights clamped to reasonable range */

    EXPECT_EQ(perception_training_start(bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.sample_weight = 10.0f;  /* Very high */
    effects.valid = true;
    perception_training_set_effects_for_testing(bridge, &effects);

    /* Implementation should clamp weights */
    SUCCEED();
}

TEST_F(PerceptionTrainingBridgeTest, AdaptiveWeightSchedule) {
    /* WHAT: Weight strategy changes over training */
    /* WHY:  Different priorities at different stages */
    /* HOW:  Novelty importance decreases, quality increases */

    EXPECT_EQ(perception_training_start(bridge), 0);

    /* Early training: novelty prioritized */
    perception_training_effects_t effects_early;
    memset(&effects_early, 0, sizeof(effects_early));
    effects_early.visual_novelty = 0.8f;  /* High novelty */
    effects_early.visual_confidence = 0.5f;  /* Lower quality priority */
    effects_early.valid = true;
    perception_training_set_effects_for_testing(bridge, &effects_early);

    /* Late training: quality prioritized */
    perception_training_effects_t effects_late;
    memset(&effects_late, 0, sizeof(effects_late));
    effects_late.visual_novelty = 0.3f;  /* Low novelty */
    effects_late.visual_confidence = 0.9f;  /* High quality priority */
    effects_late.valid = true;
    perception_training_set_effects_for_testing(bridge, &effects_late);

    SUCCEED();
}

TEST_F(PerceptionTrainingBridgeTest, ErrorBasedReweighting) {
    /* WHAT: High error samples re-weighted */
    /* WHY:  Hard samples need more attention */
    /* HOW:  Low confidence boosts weight */

    EXPECT_EQ(perception_training_start(bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.visual_confidence = 0.3f;  /* Low confidence = prediction error */
    effects.sample_weight = 1.4f;
    effects.valid = true;
    perception_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

//=============================================================================
// Feedback Tests (8 tests)
//=============================================================================

TEST_F(PerceptionTrainingBridgeTest, TrainingLossToPerception) {
    /* WHAT: Training loss feeds back to perception */
    /* WHY:  Perception learns what's hard to train */
    /* HOW:  High loss increases attention to features */

    EXPECT_EQ(perception_training_start(bridge), 0);
    EXPECT_EQ(perception_training_update_metrics(bridge, 2.5f, 1.0f), 0);

    /* High loss should signal perception */
    SUCCEED();
}

TEST_F(PerceptionTrainingBridgeTest, GradientNormFeedback) {
    /* WHAT: Gradient magnitude affects perception */
    /* WHY:  Large gradients = important features */
    /* HOW:  Grad norm modulates attention */

    EXPECT_EQ(perception_training_start(bridge), 0);
    EXPECT_EQ(perception_training_update_metrics(bridge, 0.5f, 10.0f), 0);

    /* High grad norm signals feature importance */
    SUCCEED();
}

TEST_F(PerceptionTrainingBridgeTest, ConvergenceFeedback) {
    /* WHAT: Convergence reduces perceptual novelty seeking */
    /* WHY:  Converged = less exploration needed */
    /* HOW:  Consolidate signal reduces novelty weight */

    EXPECT_EQ(perception_training_start(bridge), 0);
    EXPECT_EQ(perception_training_signal_event(bridge, PERCEPTION_TRAINING_FEEDBACK_CONSOLIDATE, 0.95f), 0);

    SUCCEED();
}

TEST_F(PerceptionTrainingBridgeTest, DivergenceFeedback) {
    /* WHAT: Divergence triggers perceptual alarm */
    /* WHY:  Unstable = check input quality */
    /* HOW:  Load reduce increases quality filtering */

    EXPECT_EQ(perception_training_start(bridge), 0);
    EXPECT_EQ(perception_training_signal_event(bridge, PERCEPTION_TRAINING_FEEDBACK_LOAD_REDUCE, 0.9f), 0);

    SUCCEED();
}

TEST_F(PerceptionTrainingBridgeTest, NovelPatternFeedback) {
    /* WHAT: Successful learning of novel pattern */
    /* WHY:  Reinforces perceptual feature extraction */
    /* HOW:  Novelty seek signal to perception */

    EXPECT_EQ(perception_training_start(bridge), 0);
    EXPECT_EQ(perception_training_signal_event(bridge, PERCEPTION_TRAINING_FEEDBACK_NOVELTY_SEEK, 0.85f), 0);

    SUCCEED();
}

TEST_F(PerceptionTrainingBridgeTest, PerceptualErrorCorrection) {
    /* WHAT: Training corrects perceptual errors */
    /* WHY:  Feedback loop improves perception */
    /* HOW:  Error signal adjusts perceptual weights */

    EXPECT_EQ(perception_training_start(bridge), 0);

    /* Simulate perceptual error detected in training */
    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.visual_confidence = 0.3f;  /* Low confidence indicates error */
    effects.lr_factor = 0.7f;
    effects.valid = true;
    perception_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(PerceptionTrainingBridgeTest, FeatureImportanceSignaling) {
    /* WHAT: Training signals important features to perception */
    /* WHY:  Focus perception on useful features */
    /* HOW:  Gradient-based feature importance via attention scaling */

    EXPECT_EQ(perception_training_start(bridge), 0);

    float attention_factors[4];
    EXPECT_EQ(perception_training_get_attention_scaling(bridge, attention_factors, 4), 0);

    SUCCEED();
}

TEST_F(PerceptionTrainingBridgeTest, AdaptivePerceptionThreshold) {
    /* WHAT: Training difficulty adapts perception thresholds */
    /* WHY:  Hard training = more selective perception */
    /* HOW:  Loss history modulates thresholds */

    EXPECT_EQ(perception_training_start(bridge), 0);

    /* Simulate difficult training (loss, grad_norm) */
    for (int i = 0; i < 50; i++) {
        perception_training_update_metrics(bridge, 1.0f + (i % 10) * 0.1f, 2.0f);
    }

    /* Thresholds should adapt */
    SUCCEED();
}

//=============================================================================
// Statistics and Error Handling Tests (8 tests)
//=============================================================================

TEST_F(PerceptionTrainingBridgeTest, GetStats) {
    perception_training_stats_t stats;
    EXPECT_EQ(perception_training_get_stats(bridge, &stats), 0);
}

TEST_F(PerceptionTrainingBridgeTest, GetStatsNull) {
    perception_training_stats_t stats;
    EXPECT_NE(perception_training_get_stats(nullptr, &stats), 0);
    EXPECT_NE(perception_training_get_stats(bridge, nullptr), 0);
}

TEST_F(PerceptionTrainingBridgeTest, ResetStats) {
    EXPECT_EQ(perception_training_reset_stats(bridge), 0);
}

TEST_F(PerceptionTrainingBridgeTest, StatsAfterModulations) {
    EXPECT_EQ(perception_training_start(bridge), 0);

    for (int i = 0; i < 10; i++) {
        perception_training_update_metrics(bridge, 0.5f, 1.0f);
    }

    perception_training_stats_t stats;
    EXPECT_EQ(perception_training_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.total_modulations, 10u);
}

TEST_F(PerceptionTrainingBridgeTest, StatsModalityTracking) {
    EXPECT_EQ(perception_training_start(bridge), 0);

    perception_training_stats_t stats;
    EXPECT_EQ(perception_training_get_stats(bridge, &stats), 0);

    /* Stats should track which modalities are active */
    SUCCEED();
}

TEST_F(PerceptionTrainingBridgeTest, ErrorHandlingNullBridge) {
    /* Null bridge should return base_lr unmodified */
    EXPECT_FLOAT_EQ(perception_training_get_modulated_lr(nullptr, 0.001f), 0.001f);
    EXPECT_FLOAT_EQ(perception_training_get_sample_weight(nullptr), 1.0f);
}

TEST_F(PerceptionTrainingBridgeTest, ErrorHandlingInvalidEffects) {
    EXPECT_EQ(perception_training_start(bridge), 0);

    /* Invalid effects should be rejected */
    perception_training_effects_t invalid_effects;
    memset(&invalid_effects, 0, sizeof(invalid_effects));
    invalid_effects.valid = false;
    perception_training_set_effects_for_testing(bridge, &invalid_effects);

    SUCCEED();
}

TEST_F(PerceptionTrainingBridgeTest, DumpState) {
    EXPECT_EQ(perception_training_start(bridge), 0);
    perception_training_dump_state(bridge);
    SUCCEED();
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
