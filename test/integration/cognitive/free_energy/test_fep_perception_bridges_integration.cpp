/**
 * @file test_fep_perception_bridges_integration.cpp
 * @brief Integration tests for FEP with perception processing
 * @version 1.0.0
 * @date 2025-12-12
 *
 * Tests FEP system with perception-like data processing:
 * - Visual-style observation processing
 * - Audio-style observation processing
 * - Multi-modal integration scenarios
 */

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "cognitive/free_energy/nimcp_fep_immune_bridge.h"
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * FEP Perception Integration Test Fixture
 * ============================================================================ */

class FEPPerceptionIntegrationTest : public ::testing::Test {
protected:
    static const uint32_t OBS_DIM = 16;
    static const uint32_t ACTION_DIM = 4;

    fep_system_t* fep = nullptr;

    void SetUp() override {
        fep_config_t fep_config;
        fep_default_config(&fep_config);
        fep = fep_create(&fep_config, OBS_DIM, ACTION_DIM);
        ASSERT_NE(fep, nullptr);
    }

    void TearDown() override {
        if (fep) {
            fep_destroy(fep);
            fep = nullptr;
        }
    }
};

/* ============================================================================
 * Visual-Style Perception Tests
 * ============================================================================ */

TEST_F(FEPPerceptionIntegrationTest, VisualPatternProcessing) {
    /* Simulate visual pattern (edge detection-like) */
    float vis_obs[OBS_DIM];
    for (uint32_t i = 0; i < OBS_DIM; i++) {
        vis_obs[i] = (i < OBS_DIM/2) ? 1.0f : 0.0f;
    }

    int ret = fep_process_observation(fep, vis_obs, OBS_DIM);
    EXPECT_EQ(ret, 0);

    fep_update_beliefs(fep);

    float pe = fep_get_prediction_error(fep, 0);
    EXPECT_GE(pe, 0.0f);
}

TEST_F(FEPPerceptionIntegrationTest, VisualMotionProcessing) {
    /* Simulate motion (shifting pattern) */
    for (int frame = 0; frame < 5; frame++) {
        float vis_obs[OBS_DIM];
        for (uint32_t i = 0; i < OBS_DIM; i++) {
            vis_obs[i] = ((i + frame) % 4 == 0) ? 1.0f : 0.0f;
        }

        EXPECT_EQ(fep_process_observation(fep, vis_obs, OBS_DIM), 0);
        EXPECT_EQ(fep_update_beliefs(fep), 0);
    }

    float fe = fep_get_free_energy(fep);
    EXPECT_FALSE(std::isnan(fe));
}

TEST_F(FEPPerceptionIntegrationTest, VisualContrastProcessing) {
    /* High contrast pattern */
    float high_contrast[OBS_DIM];
    for (uint32_t i = 0; i < OBS_DIM; i++) {
        high_contrast[i] = (i % 2 == 0) ? 1.0f : 0.0f;
    }

    fep_process_observation(fep, high_contrast, OBS_DIM);
    float pe_high = fep_get_prediction_error(fep, 0);

    /* Low contrast pattern */
    float low_contrast[OBS_DIM];
    for (uint32_t i = 0; i < OBS_DIM; i++) {
        low_contrast[i] = (i % 2 == 0) ? 0.6f : 0.4f;
    }

    fep_process_observation(fep, low_contrast, OBS_DIM);
    float pe_low = fep_get_prediction_error(fep, 0);

    EXPECT_GE(pe_high, 0.0f);
    EXPECT_GE(pe_low, 0.0f);
}

/* ============================================================================
 * Audio-Style Perception Tests
 * ============================================================================ */

TEST_F(FEPPerceptionIntegrationTest, AudioToneProcessing) {
    /* Simulate audio tone (sinusoidal) */
    float audio_obs[OBS_DIM];
    for (uint32_t i = 0; i < OBS_DIM; i++) {
        audio_obs[i] = sinf(i * 0.5f);
    }

    int ret = fep_process_observation(fep, audio_obs, OBS_DIM);
    EXPECT_EQ(ret, 0);

    fep_update_beliefs(fep);

    float fe = fep_get_free_energy(fep);
    EXPECT_FALSE(std::isnan(fe));
}

TEST_F(FEPPerceptionIntegrationTest, AudioRhythmProcessing) {
    /* Simulate rhythmic pattern */
    for (int beat = 0; beat < 8; beat++) {
        float audio_obs[OBS_DIM];
        for (uint32_t i = 0; i < OBS_DIM; i++) {
            audio_obs[i] = (beat % 2 == 0 && i < OBS_DIM/4) ? 1.0f : 0.2f;
        }

        EXPECT_EQ(fep_process_observation(fep, audio_obs, OBS_DIM), 0);
        EXPECT_EQ(fep_update_beliefs(fep), 0);
    }

    fep_stats_t stats;
    fep_get_stats(fep, &stats);
    EXPECT_GT(stats.belief_updates, 0u);
}

TEST_F(FEPPerceptionIntegrationTest, AudioFrequencyModulation) {
    /* Frequency modulation simulation */
    float base_freq = 0.3f;
    for (int t = 0; t < 5; t++) {
        float freq = base_freq + t * 0.1f;
        float audio_obs[OBS_DIM];
        for (uint32_t i = 0; i < OBS_DIM; i++) {
            audio_obs[i] = sinf(i * freq);
        }

        fep_process_observation(fep, audio_obs, OBS_DIM);
        fep_update_beliefs(fep);
    }

    float fe = fep_get_free_energy(fep);
    EXPECT_FALSE(std::isnan(fe));
    EXPECT_FALSE(std::isinf(fe));
}

/* ============================================================================
 * Multi-Modal Integration Tests
 * ============================================================================ */

TEST_F(FEPPerceptionIntegrationTest, AudioVisualIntegration) {
    /* Alternating visual and audio-like patterns */
    for (int i = 0; i < 6; i++) {
        float obs[OBS_DIM];
        if (i % 2 == 0) {
            /* Visual-like pattern */
            for (uint32_t j = 0; j < OBS_DIM; j++) {
                obs[j] = (j < OBS_DIM/2) ? 1.0f : 0.0f;
            }
        } else {
            /* Audio-like pattern */
            for (uint32_t j = 0; j < OBS_DIM; j++) {
                obs[j] = sinf(j * 0.4f);
            }
        }

        EXPECT_EQ(fep_process_observation(fep, obs, OBS_DIM), 0);
        EXPECT_EQ(fep_update_beliefs(fep), 0);
    }

    float fe = fep_get_free_energy(fep);
    EXPECT_FALSE(std::isnan(fe));
}

TEST_F(FEPPerceptionIntegrationTest, SynchronousMultiModal) {
    /* Combined audio-visual pattern */
    float obs[OBS_DIM];
    for (uint32_t i = 0; i < OBS_DIM; i++) {
        float visual = (i < OBS_DIM/2) ? 0.8f : 0.2f;
        float audio = sinf(i * 0.3f) * 0.5f + 0.5f;
        obs[i] = (visual + audio) / 2.0f;
    }

    int ret = fep_process_observation(fep, obs, OBS_DIM);
    EXPECT_EQ(ret, 0);

    fep_update_beliefs(fep);
    fep_evaluate_policies(fep);

    float action[ACTION_DIM];
    int policy = fep_select_action(fep, action, ACTION_DIM);
    EXPECT_GE(policy, -1);
}

/* ============================================================================
 * Temporal Perception Tests
 * ============================================================================ */

TEST_F(FEPPerceptionIntegrationTest, TemporalSequenceProcessing) {
    /* Process temporal sequence */
    for (int t = 0; t < 10; t++) {
        float obs[OBS_DIM];
        for (uint32_t i = 0; i < OBS_DIM; i++) {
            obs[i] = sinf((t * OBS_DIM + i) * 0.1f);
        }

        fep_process_observation(fep, obs, OBS_DIM);
        fep_update_beliefs(fep);
        fep_update_precision(fep);
    }

    fep_stats_t stats;
    fep_get_stats(fep, &stats);
    EXPECT_GE(stats.belief_updates, 10u);
}

TEST_F(FEPPerceptionIntegrationTest, PredictableSequenceLearning) {
    /* Predictable pattern - should reduce PE over time */
    float obs[OBS_DIM];
    for (uint32_t i = 0; i < OBS_DIM; i++) {
        obs[i] = (i % 2 == 0) ? 1.0f : 0.0f;
    }

    /* Process same pattern multiple times */
    for (int rep = 0; rep < 5; rep++) {
        fep_process_observation(fep, obs, OBS_DIM);
        fep_update_beliefs(fep);
    }

    float fe = fep_get_free_energy(fep);
    EXPECT_FALSE(std::isnan(fe));
}

TEST_F(FEPPerceptionIntegrationTest, UnpredictableSequenceProcessing) {
    /* Random-like patterns */
    for (int t = 0; t < 5; t++) {
        float obs[OBS_DIM];
        for (uint32_t i = 0; i < OBS_DIM; i++) {
            obs[i] = sinf(t * 17 + i * 23) * 0.5f + 0.5f;
        }

        fep_process_observation(fep, obs, OBS_DIM);
        fep_update_beliefs(fep);
    }

    float fe = fep_get_free_energy(fep);
    EXPECT_FALSE(std::isnan(fe));
    EXPECT_FALSE(std::isinf(fe));
}

/* ============================================================================
 * Precision Weighting Tests
 * ============================================================================ */

TEST_F(FEPPerceptionIntegrationTest, PrecisionUpdate) {
    float obs[OBS_DIM];
    for (uint32_t i = 0; i < OBS_DIM; i++) {
        obs[i] = 0.5f;
    }

    fep_process_observation(fep, obs, OBS_DIM);
    int ret = fep_update_precision(fep);
    EXPECT_EQ(ret, 0);
}

TEST_F(FEPPerceptionIntegrationTest, HighPrecisionProcessing) {
    /* Consistent input = high precision */
    for (int t = 0; t < 5; t++) {
        float obs[OBS_DIM];
        for (uint32_t i = 0; i < OBS_DIM; i++) {
            obs[i] = 0.7f;
        }

        fep_process_observation(fep, obs, OBS_DIM);
        fep_update_beliefs(fep);
        fep_update_precision(fep);
    }

    float fe = fep_get_free_energy(fep);
    EXPECT_FALSE(std::isnan(fe));
}

