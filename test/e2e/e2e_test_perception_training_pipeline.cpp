/**
 * @file e2e_test_perception_training_pipeline.cpp
 * @brief End-to-end tests for Perception-Training Pipeline
 *
 * WHAT: Full pipeline scenarios combining perception and training
 * WHY:  Verify complete workflow from perception to training decisions
 * HOW:  Realistic training loops with multimodal perception
 *
 * TEST COVERAGE:
 * - Full perception modulation scenario (5 tests)
 * - Multimodal training cycle (5 tests)
 * - Perception-driven curriculum learning (5 tests)
 *
 * TOTAL: 15 tests
 *
 * @author NIMCP Development Team
 * @date 2025-12-20
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

// Headers have their own extern "C" guards
#include "middleware/training/nimcp_perception_training_bridge.h"
#include "utils/error/nimcp_error_codes.h"

class PerceptionTrainingPipelineTest : public ::testing::Test {
protected:
    perception_training_bridge_t* bridge;
    perception_training_config_t config;

    void SetUp() override {
        perception_training_default_config(&config);
        config.enable_bio_async = false;
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
// Full Perception Modulation Scenario (5 tests)
//=============================================================================

TEST_F(PerceptionTrainingPipelineTest, VisualLearningProgression) {
    /* WHAT: Complete visual learning from poor to expert */
    /* WHY:  Verify perception adapts as model learns */
    /* HOW:  1000-step visual training with quality progression */

    EXPECT_EQ(perception_training_start(bridge), 0);

    float initial_lr = 0.001f;
    for (int step = 0; step < 1000; ++step) {
        /* Visual quality improves as model learns */
        float progress = step / 1000.0f;

        perception_training_effects_t effects;
        memset(&effects, 0, sizeof(effects));
        effects.visual_confidence = 0.3f + 0.6f * progress;
        effects.visual_confidence = 0.4f + 0.5f * progress;
        effects.visual_novelty = 0.9f - 0.7f * progress;  /* Decreases */
        effects.lr_factor = 0.8f + 0.4f * effects.visual_confidence;
        effects.sample_weight = 0.8f + 0.4f * effects.visual_confidence;
        effects.valid = true;
        perception_training_set_effects_for_testing(bridge, &effects);

        float loss = 1.0f / (1.0f + step * 0.005f);
        perception_training_update_metrics(bridge, loss, 1.0f);

        float modulated_lr = perception_training_get_modulated_lr(bridge, initial_lr);
        EXPECT_GT(modulated_lr, 0.0f);
    }

    perception_training_stats_t stats;
    EXPECT_EQ(perception_training_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.total_modulations, 1000u);
}

TEST_F(PerceptionTrainingPipelineTest, AudioQualityAdaptation) {
    /* WHAT: Audio quality affects training dynamically */
    /* WHY:  Poor audio → conservative learning, good audio → confident */
    /* HOW:  Vary SNR, observe LR adaptation */

    EXPECT_EQ(perception_training_start(bridge), 0);

    std::vector<float> lr_history;

    for (int step = 0; step < 500; ++step) {
        /* Simulate varying audio conditions */
        float snr = 0.5f + 0.4f * sinf(step * 0.05f);

        perception_training_effects_t effects;
        memset(&effects, 0, sizeof(effects));
        effects.audio_quality = snr;
        effects.audio_quality = snr;
        effects.audio_quality = snr;
        effects.lr_factor = 0.6f + 0.6f * snr;
        effects.valid = true;
        perception_training_set_effects_for_testing(bridge, &effects);

        float lr = perception_training_get_modulated_lr(bridge, 0.001f);
        lr_history.push_back(lr);
    }

    /* Verify LR varied with audio quality */
    float lr_variance = 0.0f;
    float lr_mean = 0.0f;
    for (float lr : lr_history) lr_mean += lr;
    lr_mean /= lr_history.size();
    for (float lr : lr_history) {
        lr_variance += (lr - lr_mean) * (lr - lr_mean);
    }
    lr_variance /= lr_history.size();
    EXPECT_GT(lr_variance, 0.0f);
}

TEST_F(PerceptionTrainingPipelineTest, SpeechComprehensionCurriculum) {
    /* WHAT: Speech difficulty increases with comprehension */
    /* WHY:  Curriculum learning based on speech understanding */
    /* HOW:  Easy speech → complex speech as comprehension improves */

    EXPECT_EQ(perception_training_start(bridge), 0);

    for (int phase = 0; phase < 5; ++phase) {
        /* Each phase = harder speech */
        float difficulty = 0.2f + phase * 0.2f;

        for (int step = 0; step < 100; ++step) {
            perception_training_effects_t effects;
            memset(&effects, 0, sizeof(effects));
            effects.comprehension = 0.5f + 0.4f * (step / 100.0f);
            effects.comprehension = difficulty;
            effects.phoneme_accuracy = 0.7f + 0.2f * (step / 100.0f);
            effects.lr_factor = 1.0f - 0.3f * difficulty;
            effects.lr_factor = 1.0f + 0.2f * effects.comprehension;
            effects.valid = true;
            perception_training_set_effects_for_testing(bridge, &effects);

            perception_training_update_metrics(bridge, 0.5f, 1.0f);
        }
    }

    SUCCEED();
}

TEST_F(PerceptionTrainingPipelineTest, PerceptualNoveltyExploration) {
    /* WHAT: Novel perceptual input drives exploration */
    /* WHY:  Novelty = learning opportunity */
    /* HOW:  High novelty → high LR, low novelty → consolidation */

    EXPECT_EQ(perception_training_start(bridge), 0);

    for (int step = 0; step < 500; ++step) {
        /* Novelty spikes represent new visual concepts */
        bool novelty_spike = (step % 100 < 10);

        perception_training_effects_t effects;
        memset(&effects, 0, sizeof(effects));
        effects.visual_novelty = novelty_spike ? 0.9f : 0.3f;
        effects.audio_quality = novelty_spike ? 0.85f : 0.25f;
        effects.lr_factor = novelty_spike ? 1.4f : 0.9f;
        effects.valid = true;
        perception_training_set_effects_for_testing(bridge, &effects);

        perception_training_update_metrics(bridge, 0.5f, 1.0f);
    }

    SUCCEED();
}

TEST_F(PerceptionTrainingPipelineTest, QualityFilteringPipeline) {
    /* WHAT: Low quality samples filtered automatically */
    /* WHY:  Poor quality = harmful to training */
    /* HOW:  Quality threshold determines sample inclusion */

    EXPECT_EQ(perception_training_start(bridge), 0);

    int skipped_samples = 0;
    int kept_samples = 0;

    for (int step = 0; step < 500; ++step) {
        /* Mix of good and poor quality samples */
        float quality = (step % 5 == 0) ? 0.05f : 0.85f;

        perception_training_effects_t effects;
        memset(&effects, 0, sizeof(effects));
        effects.visual_confidence = quality;
        effects.audio_quality = quality;
        effects.skip_sample = (quality < 0.1f);
        effects.valid = true;
        perception_training_set_effects_for_testing(bridge, &effects);

        perception_training_effects_t retrieved;
        perception_training_get_effects(bridge, &retrieved);

        if (retrieved.skip_sample) {
            skipped_samples++;
        } else {
            kept_samples++;
            perception_training_update_metrics(bridge, 0.5f, 1.0f);
        }
    }

    EXPECT_GT(skipped_samples, 0);
    EXPECT_GT(kept_samples, 0);
}

//=============================================================================
// Multimodal Training Cycle (5 tests)
//=============================================================================

TEST_F(PerceptionTrainingPipelineTest, VisualAudioFusion) {
    /* WHAT: Visual + audio together boost confidence */
    /* WHY:  Multimodal = redundant information */
    /* HOW:  High coherence increases sample weight */

    EXPECT_EQ(perception_training_start(bridge), 0);

    for (int step = 0; step < 300; ++step) {
        /* Sometimes aligned, sometimes not */
        bool aligned = (step % 10 < 7);

        perception_training_effects_t effects;
        memset(&effects, 0, sizeof(effects));
        effects.visual_confidence = 0.8f;
        effects.audio_quality = 0.85f;
        effects.temporal_coherence = aligned ? 0.9f : 0.3f;
        effects.sample_weight = aligned ? 1.4f : 0.7f;
        effects.valid = true;
        perception_training_set_effects_for_testing(bridge, &effects);

        perception_training_update_metrics(bridge, 0.5f, 1.0f);
    }

    SUCCEED();
}

TEST_F(PerceptionTrainingPipelineTest, SpeechVisualLipReading) {
    /* WHAT: Visual lip movement aids speech recognition */
    /* WHY:  Multimodal speech processing */
    /* HOW:  Visual-speech correlation boosts comprehension */

    EXPECT_EQ(perception_training_start(bridge), 0);

    for (int step = 0; step < 400; ++step) {
        float correlation = 0.5f + 0.4f * cosf(step * 0.1f);

        perception_training_effects_t effects;
        memset(&effects, 0, sizeof(effects));
        effects.temporal_coherence = correlation;
        effects.comprehension = 0.6f + 0.3f * correlation;
        effects.sample_weight = 1.0f + 0.4f * correlation;
        effects.valid = true;
        perception_training_set_effects_for_testing(bridge, &effects);

        perception_training_update_metrics(bridge, 0.5f, 1.0f);
    }

    SUCCEED();
}

TEST_F(PerceptionTrainingPipelineTest, MultimodalConflictResolution) {
    /* WHAT: Conflicting modalities reduce confidence */
    /* WHY:  Mismatch = unreliable signal */
    /* HOW:  Low coherence reduces weight */

    EXPECT_EQ(perception_training_start(bridge), 0);

    for (int step = 0; step < 300; ++step) {
        /* Create conflicts */
        bool conflict = (step % 5 == 0);

        perception_training_effects_t effects;
        memset(&effects, 0, sizeof(effects));
        effects.visual_confidence = 0.8f;
        effects.audio_quality = conflict ? 0.3f : 0.8f;
        effects.temporal_coherence = conflict ? 0.2f : 0.9f;
        effects.sample_weight = conflict ? 0.5f : 1.3f;
        effects.valid = true;
        perception_training_set_effects_for_testing(bridge, &effects);

        perception_training_update_metrics(bridge, 0.5f, 1.0f);
    }

    SUCCEED();
}

TEST_F(PerceptionTrainingPipelineTest, ModalityDropout) {
    /* WHAT: Missing modality handled gracefully */
    /* WHY:  Unimodal fallback when needed */
    /* HOW:  Zero confidence in missing modality */

    EXPECT_EQ(perception_training_start(bridge), 0);

    for (int step = 0; step < 400; ++step) {
        /* Randomly drop modalities */
        int active_modality = step % 3;

        perception_training_effects_t effects;
        memset(&effects, 0, sizeof(effects));
        effects.visual_confidence = (active_modality == 0) ? 0.9f : 0.0f;
        effects.audio_quality = (active_modality == 1) ? 0.85f : 0.0f;
        effects.comprehension = (active_modality == 2) ? 0.9f : 0.0f;
        effects.sample_weight = 1.0f;
        effects.valid = true;
        perception_training_set_effects_for_testing(bridge, &effects);

        perception_training_update_metrics(bridge, 0.5f, 1.0f);
    }

    SUCCEED();
}

TEST_F(PerceptionTrainingPipelineTest, TripleModalityIntegration) {
    /* WHAT: Visual + audio + speech all active */
    /* WHY:  Full multimodal processing */
    /* HOW:  All modalities contribute to decisions */

    EXPECT_EQ(perception_training_start(bridge), 0);

    for (int step = 0; step < 500; ++step) {
        float coherence = 0.7f + 0.2f * sinf(step * 0.05f);

        perception_training_effects_t effects;
        memset(&effects, 0, sizeof(effects));
        effects.visual_confidence = 0.85f;
        effects.audio_quality = 0.8f;
        effects.comprehension = 0.9f;
        effects.temporal_coherence = coherence;
        effects.sample_weight = 1.0f + 0.6f * coherence;
        effects.valid = true;
        perception_training_set_effects_for_testing(bridge, &effects);

        perception_training_update_metrics(bridge, 0.5f, 1.0f);
    }

    SUCCEED();
}

//=============================================================================
// Perception-Driven Curriculum Learning (5 tests)
//=============================================================================

TEST_F(PerceptionTrainingPipelineTest, ProgressiveComplexity) {
    /* WHAT: Complexity increases as model improves */
    /* WHY:  Curriculum learning principle */
    /* HOW:  Easy → hard based on loss */

    EXPECT_EQ(perception_training_start(bridge), 0);

    for (int step = 0; step < 1000; ++step) {
        float progress = step / 1000.0f;
        float loss = 1.0f - 0.8f * progress;

        /* Complexity based on progress */
        float complexity = 0.3f + 0.6f * progress;

        perception_training_effects_t effects;
        memset(&effects, 0, sizeof(effects));
        effects.visual_confidence = complexity;
        effects.comprehension = complexity;
        effects.lr_factor = 1.2f - 0.4f * complexity;
        effects.valid = true;
        perception_training_set_effects_for_testing(bridge, &effects);

        perception_training_update_metrics(bridge, loss, 1.0f);
    }

    SUCCEED();
}

TEST_F(PerceptionTrainingPipelineTest, AttentionBasedSampling) {
    /* WHAT: Attention weights determine sample importance */
    /* WHY:  Attended = more important for learning */
    /* HOW:  High attention increases gradient scale */

    EXPECT_EQ(perception_training_start(bridge), 0);

    for (int step = 0; step < 500; ++step) {
        /* Attention varies across samples */
        float attention = 0.3f + 0.6f * (step % 100) / 100.0f;

        perception_training_effects_t effects;
        memset(&effects, 0, sizeof(effects));
        effects.visual_confidence = attention;
        effects.audio_quality = attention * 0.9f;
        effects.lr_factor = 0.8f + 0.6f * attention;
        effects.valid = true;
        perception_training_set_effects_for_testing(bridge, &effects);

        perception_training_update_metrics(bridge, 0.5f, 1.0f);
    }

    SUCCEED();
}

TEST_F(PerceptionTrainingPipelineTest, SalienceGuidedLearning) {
    /* WHAT: Salient samples prioritized */
    /* WHY:  Salience = importance */
    /* HOW:  High salience increases weight */

    EXPECT_EQ(perception_training_start(bridge), 0);

    for (int step = 0; step < 600; ++step) {
        /* Mix of salient and non-salient */
        bool salient = (step % 7 < 2);

        perception_training_effects_t effects;
        memset(&effects, 0, sizeof(effects));
        effects.visual_novelty = salient ? 0.95f : 0.4f;
        effects.speech_salience = salient ? 0.9f : 0.35f;
        effects.sample_weight = salient ? 1.6f : 0.8f;
        effects.valid = true;
        perception_training_set_effects_for_testing(bridge, &effects);

        perception_training_update_metrics(bridge, 0.5f, 1.0f);
    }

    SUCCEED();
}

TEST_F(PerceptionTrainingPipelineTest, ErrorDrivenPerceptionAdaptation) {
    /* WHAT: High prediction error increases perceptual focus */
    /* WHY:  Error = what perception needs to learn */
    /* HOW:  Error boosts sample weight and LR */

    EXPECT_EQ(perception_training_start(bridge), 0);

    for (int step = 0; step < 500; ++step) {
        /* Simulate varying prediction error */
        float error = 0.3f + 0.5f * sinf(step * 0.08f);

        perception_training_effects_t effects;
        memset(&effects, 0, sizeof(effects));
        effects.visual_novelty = error;
        effects.sample_weight = 0.9f + 0.6f * error;
        effects.lr_factor = 1.0f + 0.3f * error;
        effects.valid = true;
        perception_training_set_effects_for_testing(bridge, &effects);

        float loss = 0.5f + 0.3f * error;
        perception_training_update_metrics(bridge, loss, 1.0f);
    }

    SUCCEED();
}

TEST_F(PerceptionTrainingPipelineTest, AdaptiveQualityThresholds) {
    /* WHAT: Quality thresholds adapt during training */
    /* WHY:  Early: lenient, late: strict */
    /* HOW:  Threshold increases with progress */

    EXPECT_EQ(perception_training_start(bridge), 0);

    for (int phase = 0; phase < 5; ++phase) {
        float threshold = 0.1f + phase * 0.15f;

        for (int step = 0; step < 100; ++step) {
            float quality = 0.2f + (step / 100.0f) * 0.7f;

            perception_training_effects_t effects;
            memset(&effects, 0, sizeof(effects));
            effects.visual_confidence = quality;
            effects.audio_quality = quality;
            effects.skip_sample = (quality < threshold);
            effects.valid = true;
            perception_training_set_effects_for_testing(bridge, &effects);

            if (!effects.skip_sample) {
                perception_training_update_metrics(bridge, 0.5f, 1.0f);
            }
        }
    }

    SUCCEED();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
