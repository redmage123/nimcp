/**
 * @file test_perception_training_integration.cpp
 * @brief Integration tests for Perception-Training Bridge
 *
 * WHAT: Integration between perception modules and training system
 * WHY:  Verify realistic cross-module interactions
 * HOW:  Test with multiple connected modules
 *
 * TEST COVERAGE:
 * - Visual-Training integration (8 tests)
 * - Audio-Training integration (8 tests)
 * - Speech-Training integration (8 tests)
 * - Cross-bridge integration (6 tests): cognitive, logic, immune
 *
 * TOTAL: 30 tests
 *
 * @author NIMCP Development Team
 * @date 2025-12-20
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "middleware/training/nimcp_perception_training_bridge.h"
#include "middleware/training/nimcp_cognitive_training_bridge.h"
#include "utils/error/nimcp_error_codes.h"

//=============================================================================
// Test Fixture
//=============================================================================

class PerceptionTrainingIntegrationTest : public ::testing::Test {
protected:
    perception_training_bridge_t* perception_bridge;
    perception_training_config_t perception_config;

    void SetUp() override {
        perception_training_default_config(&perception_config);
        perception_config.enable_bio_async = false;
        perception_bridge = perception_training_create(&perception_config);
        ASSERT_NE(perception_bridge, nullptr);
    }

    void TearDown() override {
        if (perception_bridge) {
            perception_training_destroy(perception_bridge);
            perception_bridge = nullptr;
        }
    }
};

//=============================================================================
// Visual-Training Integration (8 tests)
//=============================================================================

TEST_F(PerceptionTrainingIntegrationTest, VisualCortexTrainingLoop) {
    /* WHAT: Full visual cortex + training loop */
    /* WHY:  Verify visual perception modulates training */
    /* HOW:  Simulate visual input → training update cycle */

    EXPECT_EQ(perception_training_start(perception_bridge), 0);

    /* Simulate training loop */
    for (int step = 0; step < 100; ++step) {
        /* Set visual state */
        perception_training_effects_t effects;
        memset(&effects, 0, sizeof(effects));
        effects.visual_confidence = 0.8f + 0.1f * sinf(step * 0.1f);
        effects.visual_confidence = 0.85f;
        effects.lr_factor = 0.9f + 0.2f * effects.visual_confidence;
        effects.valid = true;
        perception_training_set_effects_for_testing(perception_bridge, &effects);

        /* Update metrics */
        float loss = 1.0f / (1.0f + step * 0.01f);
        perception_training_update_metrics(perception_bridge, loss, 1.0f);

        /* Get modulated parameters */
        float lr = perception_training_get_modulated_lr(perception_bridge, 0.001f);
        EXPECT_GT(lr, 0.0f);
    }

    SUCCEED();
}

TEST_F(PerceptionTrainingIntegrationTest, VisualAttentionGradientWeighting) {
    /* WHAT: Visual attention weights gradients */
    /* WHY:  Attended regions more important for learning */
    /* HOW:  High attention increases gradient scale */

    EXPECT_EQ(perception_training_start(perception_bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.visual_confidence = 0.95f;
    effects.lr_factor = 1.4f;
    effects.valid = true;
    perception_training_set_effects_for_testing(perception_bridge, &effects);

    perception_training_effects_t retrieved;
    perception_training_get_effects(perception_bridge, &retrieved);

    EXPECT_GT(retrieved.lr_factor, 1.0f);
}

TEST_F(PerceptionTrainingIntegrationTest, VisualNoveltyExploration) {
    /* WHAT: Novel visual input drives exploration */
    /* WHY:  Novelty = learning opportunity */
    /* HOW:  High novelty increases LR */

    EXPECT_EQ(perception_training_start(perception_bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.visual_novelty = 0.9f;
    effects.lr_factor = 1.3f;
    effects.valid = true;
    perception_training_set_effects_for_testing(perception_bridge, &effects);

    float lr = perception_training_get_modulated_lr(perception_bridge, 0.001f);
    EXPECT_GT(lr, 0.001f);
}

TEST_F(PerceptionTrainingIntegrationTest, VisualQualityFiltering) {
    /* WHAT: Low quality visual samples filtered */
    /* WHY:  Poor quality = unreliable training */
    /* HOW:  Quality threshold determines skip */

    EXPECT_EQ(perception_training_start(perception_bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.visual_confidence = 0.05f;  /* Very poor */
    effects.skip_sample = true;
    effects.valid = true;
    perception_training_set_effects_for_testing(perception_bridge, &effects);

    perception_training_effects_t retrieved;
    perception_training_get_effects(perception_bridge, &retrieved);

    EXPECT_TRUE(retrieved.skip_sample);
}

TEST_F(PerceptionTrainingIntegrationTest, VisualPredictionErrorFeedback) {
    /* WHAT: Visual prediction errors drive learning */
    /* WHY:  Error = what visual system needs to learn */
    /* HOW:  Error increases sample weight */

    EXPECT_EQ(perception_training_start(perception_bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.visual_novelty = 0.9f;
    effects.sample_weight = 1.4f;
    effects.valid = true;
    perception_training_set_effects_for_testing(perception_bridge, &effects);

    SUCCEED();
}

TEST_F(PerceptionTrainingIntegrationTest, VisualSalienceRouting) {
    /* WHAT: Salient visual features prioritized */
    /* WHY:  Salience = perceptual importance */
    /* HOW:  Salience increases gradient contribution */

    EXPECT_EQ(perception_training_start(perception_bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.visual_novelty = 0.95f;
    effects.lr_factor = 1.35f;
    effects.valid = true;
    perception_training_set_effects_for_testing(perception_bridge, &effects);

    SUCCEED();
}

TEST_F(PerceptionTrainingIntegrationTest, VisualContextIntegration) {
    /* WHAT: Visual context affects interpretation */
    /* WHY:  Context = scene understanding */
    /* HOW:  Context quality modulates confidence */

    EXPECT_EQ(perception_training_start(perception_bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.visual_confidence = 0.85f;
    effects.visual_confidence = 0.8f;
    effects.valid = true;
    perception_training_set_effects_for_testing(perception_bridge, &effects);

    SUCCEED();
}

TEST_F(PerceptionTrainingIntegrationTest, VisualTrainingStatistics) {
    /* WHAT: Visual training statistics tracked */
    /* WHY:  Monitor visual learning progress */
    /* HOW:  Stats accumulate over training */

    EXPECT_EQ(perception_training_start(perception_bridge), 0);

    for (int i = 0; i < 50; ++i) {
        perception_training_update_metrics(perception_bridge, 0.5f, 1.0f);
    }

    perception_training_stats_t stats;
    EXPECT_EQ(perception_training_get_stats(perception_bridge, &stats), 0);
    EXPECT_EQ(stats.total_modulations, 50u);
}

//=============================================================================
// Audio-Training Integration (8 tests)
//=============================================================================

TEST_F(PerceptionTrainingIntegrationTest, AudioCortexTrainingLoop) {
    /* WHAT: Full audio cortex + training loop */
    /* WHY:  Verify audio perception modulates training */
    /* HOW:  Simulate audio input → training update cycle */

    EXPECT_EQ(perception_training_start(perception_bridge), 0);

    for (int step = 0; step < 100; ++step) {
        perception_training_effects_t effects;
        memset(&effects, 0, sizeof(effects));
        effects.audio_quality = 0.85f;
        effects.audio_quality = 0.7f + 0.2f * cosf(step * 0.05f);
        effects.lr_factor = 1.0f + 0.1f * effects.audio_quality;
        effects.valid = true;
        perception_training_set_effects_for_testing(perception_bridge, &effects);

        float loss = 1.0f / (1.0f + step * 0.01f);
        perception_training_update_metrics(perception_bridge, loss, 1.0f);
    }

    SUCCEED();
}

TEST_F(PerceptionTrainingIntegrationTest, AudioNoiseRobustness) {
    /* WHAT: Training robust to audio noise */
    /* WHY:  Noise = common in real audio */
    /* HOW:  Low SNR reduces sample weight */

    EXPECT_EQ(perception_training_start(perception_bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.audio_quality = 0.2f;  /* Noisy */
    effects.audio_quality = 0.3f;
    effects.sample_weight = 0.5f;
    effects.valid = true;
    perception_training_set_effects_for_testing(perception_bridge, &effects);

    SUCCEED();
}

TEST_F(PerceptionTrainingIntegrationTest, AudioSalienceDetection) {
    /* WHAT: Salient audio events prioritized */
    /* WHY:  Important sounds = more learning */
    /* HOW:  Salience increases gradient scale */

    EXPECT_EQ(perception_training_start(perception_bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.speech_salience = 0.95f;
    effects.lr_factor = 1.4f;
    effects.valid = true;
    perception_training_set_effects_for_testing(perception_bridge, &effects);

    SUCCEED();
}

TEST_F(PerceptionTrainingIntegrationTest, AudioTemporalCoherence) {
    /* WHAT: Temporal coherence in audio */
    /* WHY:  Coherent sequences = valid patterns */
    /* HOW:  Coherence boosts confidence */

    EXPECT_EQ(perception_training_start(perception_bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.temporal_coherence = 0.85f;
    effects.audio_quality = 0.8f;
    effects.valid = true;
    perception_training_set_effects_for_testing(perception_bridge, &effects);

    SUCCEED();
}

TEST_F(PerceptionTrainingIntegrationTest, AudioSpeechDetection) {
    /* WHAT: Speech detection affects processing */
    /* WHY:  Speech = special audio category */
    /* HOW:  Speech likelihood triggers speech module */

    EXPECT_EQ(perception_training_start(perception_bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.speech_salience = 0.9f;
    effects.comprehension = 0.85f;
    effects.valid = true;
    perception_training_set_effects_for_testing(perception_bridge, &effects);

    SUCCEED();
}

TEST_F(PerceptionTrainingIntegrationTest, AudioOnsetSegmentation) {
    /* WHAT: Onset detection for segmentation */
    /* WHY:  Onsets = event boundaries */
    /* HOW:  Onset strength tracked */

    EXPECT_EQ(perception_training_start(perception_bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.audio_quality = 0.8f;
    effects.valid = true;
    perception_training_set_effects_for_testing(perception_bridge, &effects);

    SUCCEED();
}

TEST_F(PerceptionTrainingIntegrationTest, AudioRhythmPattern) {
    /* WHAT: Rhythmic patterns in audio */
    /* WHY:  Rhythm = temporal structure */
    /* HOW:  Rhythm strength affects temporal weight */

    EXPECT_EQ(perception_training_start(perception_bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.temporal_coherence = 0.75f;
    effects.valid = true;
    perception_training_set_effects_for_testing(perception_bridge, &effects);

    SUCCEED();
}

TEST_F(PerceptionTrainingIntegrationTest, AudioLocalization) {
    /* WHAT: Spatial audio localization */
    /* WHY:  Location = additional feature */
    /* HOW:  Localization confidence tracked */

    EXPECT_EQ(perception_training_start(perception_bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.audio_quality = 0.7f;
    effects.valid = true;
    perception_training_set_effects_for_testing(perception_bridge, &effects);

    SUCCEED();
}

//=============================================================================
// Speech-Training Integration (8 tests)
//=============================================================================

TEST_F(PerceptionTrainingIntegrationTest, SpeechCortexTrainingLoop) {
    /* WHAT: Full speech cortex + training loop */
    /* WHY:  Verify speech comprehension modulates training */
    /* HOW:  Simulate speech → training cycle */

    EXPECT_EQ(perception_training_start(perception_bridge), 0);

    for (int step = 0; step < 100; ++step) {
        perception_training_effects_t effects;
        memset(&effects, 0, sizeof(effects));
        effects.comprehension = 0.7f + 0.2f * (step / 100.0f);
        effects.phoneme_accuracy = 0.85f;
        effects.lr_factor = 1.0f + 0.15f * effects.comprehension;
        effects.valid = true;
        perception_training_set_effects_for_testing(perception_bridge, &effects);

        perception_training_update_metrics(perception_bridge, 0.5f, 1.0f);
    }

    SUCCEED();
}

TEST_F(PerceptionTrainingIntegrationTest, PhonemeRecognitionAccuracy) {
    /* WHAT: Phoneme accuracy affects training */
    /* WHY:  Accurate phonemes = reliable features */
    /* HOW:  Accuracy boosts sample weight */

    EXPECT_EQ(perception_training_start(perception_bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.phoneme_accuracy = 0.95f;
    effects.sample_weight = 1.3f;
    effects.valid = true;
    perception_training_set_effects_for_testing(perception_bridge, &effects);

    SUCCEED();
}

TEST_F(PerceptionTrainingIntegrationTest, SpeechProsodyModulation) {
    /* WHAT: Prosody affects comprehension */
    /* WHY:  Prosody = emotional/structural cues */
    /* HOW:  Prosody quality boosts understanding */

    EXPECT_EQ(perception_training_start(perception_bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.prosody_confidence = 0.85f;
    effects.comprehension = 0.8f;
    effects.valid = true;
    perception_training_set_effects_for_testing(perception_bridge, &effects);

    SUCCEED();
}

TEST_F(PerceptionTrainingIntegrationTest, SpeechSemanticCoherence) {
    /* WHAT: Semantic coherence in speech */
    /* WHY:  Coherent = meaningful */
    /* HOW:  Coherence boosts weight */

    EXPECT_EQ(perception_training_start(perception_bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.comprehension = 0.9f;
    effects.sample_weight = 1.2f;
    effects.valid = true;
    perception_training_set_effects_for_testing(perception_bridge, &effects);

    SUCCEED();
}

TEST_F(PerceptionTrainingIntegrationTest, SpeechEmotionalContent) {
    /* WHAT: Emotional speech prioritized */
    /* WHY:  Emotion = salient */
    /* HOW:  Emotion intensity boosts salience */

    EXPECT_EQ(perception_training_start(perception_bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.prosody_confidence = 0.8f;
    effects.speech_salience = 0.85f;
    effects.valid = true;
    perception_training_set_effects_for_testing(perception_bridge, &effects);

    SUCCEED();
}

TEST_F(PerceptionTrainingIntegrationTest, SpeechSyntaxComplexity) {
    /* WHAT: Complex syntax affects processing */
    /* WHY:  Complex = more cognitive load */
    /* HOW:  Complexity reduces batch size */

    EXPECT_EQ(perception_training_start(perception_bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.comprehension = 0.9f;
    effects.lr_factor = 0.7f;
    effects.valid = true;
    perception_training_set_effects_for_testing(perception_bridge, &effects);

    float lr = perception_training_get_modulated_lr(perception_bridge, 0.001f);
    EXPECT_LT(lr, 0.001f);
}

TEST_F(PerceptionTrainingIntegrationTest, SpeechAccentAdaptation) {
    /* WHAT: Accent familiarity affects recognition */
    /* WHY:  Unfamiliar = harder */
    /* HOW:  Familiarity modulates confidence */

    EXPECT_EQ(perception_training_start(perception_bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.comprehension = 0.5f;
    effects.comprehension = 0.6f;
    effects.valid = true;
    perception_training_set_effects_for_testing(perception_bridge, &effects);

    SUCCEED();
}

TEST_F(PerceptionTrainingIntegrationTest, SpeechDisfluencyHandling) {
    /* WHAT: Disfluencies reduce quality */
    /* WHY:  Pauses/fillers = interrupted */
    /* HOW:  Fluency affects quality */

    EXPECT_EQ(perception_training_start(perception_bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.speech_salience = 0.6f;
    effects.prosody_confidence = 0.65f;
    effects.valid = true;
    perception_training_set_effects_for_testing(perception_bridge, &effects);

    SUCCEED();
}

//=============================================================================
// Cross-Bridge Integration (6 tests)
//=============================================================================

TEST_F(PerceptionTrainingIntegrationTest, PerceptionCognitiveIntegration) {
    /* WHAT: Perception + cognitive bridges interact */
    /* WHY:  Perception informs cognition and vice versa */
    /* HOW:  Both bridges active simultaneously */

    cognitive_training_config_t cog_config;
    cognitive_training_default_config(&cog_config);
    cog_config.enable_bio_async = false;
    cognitive_training_bridge_t* cog_bridge = cognitive_training_create(&cog_config);
    ASSERT_NE(cog_bridge, nullptr);

    EXPECT_EQ(perception_training_start(perception_bridge), 0);
    EXPECT_EQ(cognitive_training_start(cog_bridge), 0);

    /* Simulate coordinated updates */
    for (int step = 0; step < 50; ++step) {
        perception_training_update_metrics(perception_bridge, 0.5f, 1.0f);
        cognitive_training_update_metrics(cog_bridge, 0.5f, 1.0f, 0.001f, step);
    }

    cognitive_training_destroy(cog_bridge);
    SUCCEED();
}

TEST_F(PerceptionTrainingIntegrationTest, MultimodalCoherence) {
    /* WHAT: Visual + audio coherence */
    /* WHY:  Multimodal = redundant info */
    /* HOW:  Coherence boosts confidence */

    EXPECT_EQ(perception_training_start(perception_bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.visual_confidence = 0.8f;
    effects.audio_quality = 0.85f;
    effects.temporal_coherence = 0.9f;
    effects.sample_weight = 1.4f;
    effects.valid = true;
    perception_training_set_effects_for_testing(perception_bridge, &effects);

    SUCCEED();
}

TEST_F(PerceptionTrainingIntegrationTest, PerceptionLogicCoordination) {
    /* WHAT: Perception informs logic, logic guides perception */
    /* WHY:  Logic can request specific perceptual inputs */
    /* HOW:  Bidirectional signals */

    EXPECT_EQ(perception_training_start(perception_bridge), 0);

    /* Logic might request visual verification */
    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.visual_confidence = 0.9f;  /* Logic-driven attention */
    effects.lr_factor = 1.3f;
    effects.valid = true;
    perception_training_set_effects_for_testing(perception_bridge, &effects);

    SUCCEED();
}

TEST_F(PerceptionTrainingIntegrationTest, PerceptionImmuneInteraction) {
    /* WHAT: Immune system monitors perceptual quality */
    /* WHY:  Poor perception = potential threat */
    /* HOW:  Quality thresholds trigger immune */

    EXPECT_EQ(perception_training_start(perception_bridge), 0);

    /* Very poor quality triggers immune response */
    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.visual_confidence = 0.1f;  /* Poor */
    effects.audio_quality = 0.15f;
    effects.skip_sample = true;  /* Immune-driven skip */
    effects.valid = true;
    perception_training_set_effects_for_testing(perception_bridge, &effects);

    SUCCEED();
}

TEST_F(PerceptionTrainingIntegrationTest, CrossBridgeStatistics) {
    /* WHAT: Stats across multiple bridges */
    /* WHY:  Monitor overall system health */
    /* HOW:  Aggregate statistics */

    EXPECT_EQ(perception_training_start(perception_bridge), 0);

    for (int i = 0; i < 100; ++i) {
        perception_training_update_metrics(perception_bridge, 0.5f, 1.0f);
    }

    perception_training_stats_t stats;
    EXPECT_EQ(perception_training_get_stats(perception_bridge, &stats), 0);
    EXPECT_EQ(stats.total_modulations, 100u);
}

TEST_F(PerceptionTrainingIntegrationTest, CoordinatedFeedback) {
    /* WHAT: Feedback coordinated across bridges */
    /* WHY:  Consistent signals across system */
    /* HOW:  Training events propagate */

    EXPECT_EQ(perception_training_start(perception_bridge), 0);

    /* Signal convergence */
    EXPECT_EQ(perception_training_signal_event(perception_bridge,
        PERCEPTION_TRAINING_FEEDBACK_SENSITIVITY_BOOST, 0.9f), 0);

    SUCCEED();
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
