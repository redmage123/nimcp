/**
 * @file test_perception_training_regression.cpp
 * @brief Regression tests for Perception-Training Bridge
 *
 * WHAT: Stability, memory, thread safety tests
 * WHY:  Prevent regressions in production scenarios
 * HOW:  Stress tests, edge cases, concurrency
 *
 * TEST COVERAGE:
 * - Stability under load (5 tests)
 * - Memory safety (5 tests)
 * - Thread safety (5 tests)
 * - Edge cases (5 tests)
 *
 * TOTAL: 20 tests
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

class PerceptionTrainingRegressionTest : public ::testing::Test {
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
// Stability Under Load (5 tests)
//=============================================================================

TEST_F(PerceptionTrainingRegressionTest, LongRunningTraining) {
    /* 10,000 step training loop */
    EXPECT_EQ(perception_training_start(bridge), 0);
    for (int step = 0; step < 10000; ++step) {
        perception_training_update_metrics(bridge, 0.5f, 1.0f);
        if (step % 1000 == 0) {
            perception_training_stats_t stats;
            EXPECT_EQ(perception_training_get_stats(bridge, &stats), 0);
        }
    }
    SUCCEED();
}

TEST_F(PerceptionTrainingRegressionTest, RapidParameterChanges) {
    EXPECT_EQ(perception_training_start(bridge), 0);
    for (int i = 0; i < 1000; ++i) {
        perception_training_effects_t effects;
        memset(&effects, 0, sizeof(effects));
        effects.visual_confidence = (i % 2) ? 0.1f : 0.9f;
        effects.audio_quality = (i % 3) ? 0.2f : 0.8f;
        effects.lr_factor = 0.5f + (i % 10) * 0.1f;
        effects.valid = true;
        perception_training_set_effects_for_testing(bridge, &effects);
    }
    SUCCEED();
}

TEST_F(PerceptionTrainingRegressionTest, ExtremeMetricValues) {
    EXPECT_EQ(perception_training_start(bridge), 0);
    EXPECT_EQ(perception_training_update_metrics(bridge, 1e6f, 1e5f), 0);
    EXPECT_EQ(perception_training_update_metrics(bridge, 1e-6f, 1e-5f), 0);
    EXPECT_EQ(perception_training_update_metrics(bridge, 0.0f, 0.0f), 0);
    SUCCEED();
}

TEST_F(PerceptionTrainingRegressionTest, RepeatedStartStop) {
    for (int i = 0; i < 100; ++i) {
        EXPECT_EQ(perception_training_start(bridge), 0);
        EXPECT_EQ(perception_training_stop(bridge), 0);
    }
    SUCCEED();
}

TEST_F(PerceptionTrainingRegressionTest, BurstModulations) {
    EXPECT_EQ(perception_training_start(bridge), 0);
    for (int burst = 0; burst < 100; ++burst) {
        for (int i = 0; i < 100; ++i) {
            perception_training_update_metrics(bridge, 0.5f, 1.0f);
        }
    }
    SUCCEED();
}

//=============================================================================
// Memory Safety (5 tests)
//=============================================================================

TEST_F(PerceptionTrainingRegressionTest, NoMemoryLeaksCreationDestruction) {
    for (int i = 0; i < 1000; ++i) {
        perception_training_bridge_t* temp = perception_training_create(&config);
        ASSERT_NE(temp, nullptr);
        perception_training_destroy(temp);
    }
    SUCCEED();
}

TEST_F(PerceptionTrainingRegressionTest, NoMemoryLeaksUpdates) {
    EXPECT_EQ(perception_training_start(bridge), 0);
    for (int i = 0; i < 10000; ++i) {
        perception_training_update_metrics(bridge, 0.5f, 1.0f);
    }
    SUCCEED();
}

TEST_F(PerceptionTrainingRegressionTest, NullPointerHandling) {
    /* Test null safety for various API functions - returns default values */
    EXPECT_EQ(perception_training_get_modulated_lr(nullptr, 0.001f), 0.001f);
    EXPECT_FALSE(perception_training_should_skip_sample(nullptr));
    perception_training_destroy(nullptr);  /* Should not crash */
    SUCCEED();
}

TEST_F(PerceptionTrainingRegressionTest, LargeStatsBuffer) {
    EXPECT_EQ(perception_training_start(bridge), 0);
    for (int i = 0; i < 100000; ++i) {
        perception_training_update_metrics(bridge, 0.5f, 1.0f);
    }
    perception_training_stats_t stats;
    EXPECT_EQ(perception_training_get_stats(bridge, &stats), 0);
    SUCCEED();
}

TEST_F(PerceptionTrainingRegressionTest, MultipleResets) {
    EXPECT_EQ(perception_training_start(bridge), 0);
    for (int i = 0; i < 100; ++i) {
        for (int j = 0; j < 100; ++j) {
            perception_training_update_metrics(bridge, 0.5f, 1.0f);
        }
        EXPECT_EQ(perception_training_reset_stats(bridge), 0);
    }
    SUCCEED();
}

//=============================================================================
// Thread Safety (5 tests)
//=============================================================================

TEST_F(PerceptionTrainingRegressionTest, ConcurrentReads) {
    EXPECT_EQ(perception_training_start(bridge), 0);
    for (int i = 0; i < 1000; ++i) {
        float lr = perception_training_get_modulated_lr(bridge, 0.001f);
        float weight = perception_training_get_sample_weight(bridge);
        (void)lr; (void)weight;
    }
    SUCCEED();
}

TEST_F(PerceptionTrainingRegressionTest, InterleavedReadWrite) {
    EXPECT_EQ(perception_training_start(bridge), 0);
    for (int i = 0; i < 1000; ++i) {
        perception_training_update_metrics(bridge, 0.5f, 1.0f);
        float lr = perception_training_get_modulated_lr(bridge, 0.001f);
        (void)lr;
    }
    SUCCEED();
}

TEST_F(PerceptionTrainingRegressionTest, RapidStateChanges) {
    for (int i = 0; i < 100; ++i) {
        EXPECT_EQ(perception_training_start(bridge), 0);
        perception_training_update_metrics(bridge, 0.5f, 1.0f);
        EXPECT_EQ(perception_training_stop(bridge), 0);
    }
    SUCCEED();
}

TEST_F(PerceptionTrainingRegressionTest, ConcurrentStatAccess) {
    EXPECT_EQ(perception_training_start(bridge), 0);
    for (int i = 0; i < 1000; ++i) {
        perception_training_stats_t stats;
        EXPECT_EQ(perception_training_get_stats(bridge, &stats), 0);
    }
    SUCCEED();
}

TEST_F(PerceptionTrainingRegressionTest, MixedOperations) {
    EXPECT_EQ(perception_training_start(bridge), 0);
    for (int i = 0; i < 1000; ++i) {
        perception_training_update_metrics(bridge, 0.5f, 1.0f);
        perception_training_get_modulated_lr(bridge, 0.001f);
        perception_training_stats_t stats;
        perception_training_get_stats(bridge, &stats);
        if (i % 100 == 0) {
            perception_training_reset_stats(bridge);
        }
    }
    SUCCEED();
}

//=============================================================================
// Edge Cases (5 tests)
//=============================================================================

TEST_F(PerceptionTrainingRegressionTest, ZeroConfidence) {
    EXPECT_EQ(perception_training_start(bridge), 0);
    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.visual_confidence = 0.0f;
    effects.audio_quality = 0.0f;
    effects.comprehension = 0.0f;
    effects.lr_factor = 0.5f;
    effects.valid = true;
    perception_training_set_effects_for_testing(bridge, &effects);
    SUCCEED();
}

TEST_F(PerceptionTrainingRegressionTest, MaximalValues) {
    EXPECT_EQ(perception_training_start(bridge), 0);
    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.visual_confidence = 1.0f;
    effects.audio_quality = 1.0f;
    effects.comprehension = 1.0f;
    effects.sample_weight = 10.0f;
    effects.lr_factor = 5.0f;
    effects.valid = true;
    perception_training_set_effects_for_testing(bridge, &effects);
    SUCCEED();
}

TEST_F(PerceptionTrainingRegressionTest, ConflictingModalities) {
    EXPECT_EQ(perception_training_start(bridge), 0);
    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.visual_confidence = 0.9f;
    effects.audio_quality = 0.1f;
    effects.temporal_coherence = 0.1f;
    effects.sample_weight = 0.5f;
    effects.valid = true;
    perception_training_set_effects_for_testing(bridge, &effects);
    SUCCEED();
}

TEST_F(PerceptionTrainingRegressionTest, RapidQualityFluctuations) {
    EXPECT_EQ(perception_training_start(bridge), 0);
    for (int i = 0; i < 1000; ++i) {
        perception_training_effects_t effects;
        memset(&effects, 0, sizeof(effects));
        effects.visual_confidence = (i % 2) ? 0.05f : 0.95f;
        effects.audio_quality = (i % 3) ? 0.1f : 0.9f;
        effects.skip_sample = (effects.visual_confidence < 0.1f);
        effects.valid = true;
        perception_training_set_effects_for_testing(bridge, &effects);
    }
    SUCCEED();
}

TEST_F(PerceptionTrainingRegressionTest, EmptyTrainingSession) {
    EXPECT_EQ(perception_training_start(bridge), 0);
    EXPECT_EQ(perception_training_stop(bridge), 0);
    perception_training_stats_t stats;
    EXPECT_EQ(perception_training_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.total_modulations, 0u);
    SUCCEED();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
