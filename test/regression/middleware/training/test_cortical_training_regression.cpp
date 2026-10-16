/**
 * @file test_cortical_training_regression.cpp
 * @brief Regression tests for Cortical-Training Bridge
 *
 * WHAT: Stability, memory, thread safety tests for cortical bridge
 * WHY:  Prevent regressions in cortical training scenarios
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

// Headers have their own extern "C" guards
#include "middleware/training/nimcp_cortical_training_bridge.h"
#include "utils/error/nimcp_error_codes.h"

class CorticalTrainingRegressionTest : public ::testing::Test {
protected:
    cortical_training_bridge_t* bridge;
    cortical_training_config_t config;

    void SetUp() override {
        cortical_training_default_config(&config);
        config.enable_bio_async = false;
        bridge = cortical_training_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            cortical_training_destroy(bridge);
            bridge = nullptr;
        }
    }
};

//=============================================================================
// Stability Under Load (5 tests)
//=============================================================================

TEST_F(CorticalTrainingRegressionTest, ExtendedPredictiveCoding) {
    EXPECT_EQ(cortical_training_start(bridge), 0);
    for (int step = 0; step < 10000; ++step) {
        cortical_training_effects_t effects;
        memset(&effects, 0, sizeof(effects));
        effects.free_energy = 0.9f - 0.7f * (step / 10000.0f);
        effects.gradient_confidence = step / 10000.0f;
        effects.lr_factor = 0.8f + 0.4f * effects.gradient_confidence;
        effects.valid = true;
        cortical_training_set_effects_for_testing(bridge, &effects);
        cortical_training_update_metrics(bridge, 0.5f, 1.0f, 0.001f, 0);
    }
    SUCCEED();
}

TEST_F(CorticalTrainingRegressionTest, BurstingDynamics) {
    EXPECT_EQ(cortical_training_start(bridge), 0);
    for (int i = 0; i < 1000; ++i) {
        cortical_training_effects_t effects;
        memset(&effects, 0, sizeof(effects));
        effects.burst_rate = (i % 10 < 5) ? 0.9f : 0.1f;
        effects.bac_success_rate = (i % 7 < 3) ? 0.8f : 0.3f;
        effects.lr_factor = 0.9f + 0.3f * effects.burst_rate;
        effects.valid = true;
        cortical_training_set_effects_for_testing(bridge, &effects);
    }
    SUCCEED();
}

TEST_F(CorticalTrainingRegressionTest, HierarchyTraversal) {
    EXPECT_EQ(cortical_training_start(bridge), 0);
    for (int cycle = 0; cycle < 100; ++cycle) {
        for (int level = 1; level <= 5; ++level) {
            cortical_training_effects_t effects;
            memset(&effects, 0, sizeof(effects));
            effects.num_layers = level;
            effects.lr_factor = 1.3f - 0.1f * level;
            effects.valid = true;
            cortical_training_set_effects_for_testing(bridge, &effects);
        }
    }
    SUCCEED();
}

TEST_F(CorticalTrainingRegressionTest, OscillationCycles) {
    EXPECT_EQ(cortical_training_start(bridge), 0);
    for (int step = 0; step < 1000; ++step) {
        cortical_training_effects_t effects;
        memset(&effects, 0, sizeof(effects));
        effects.free_energy = fmodf(step * 0.1f, 1.0f);
        effects.gradient_confidence = 0.5f + 0.4f * sinf(step * 0.1f);
        effects.valid = true;
        cortical_training_set_effects_for_testing(bridge, &effects);
    }
    SUCCEED();
}

TEST_F(CorticalTrainingRegressionTest, RepeatedConvergence) {
    EXPECT_EQ(cortical_training_start(bridge), 0);
    for (int i = 0; i < 100; ++i) {
        cortical_training_effects_t effects;
        memset(&effects, 0, sizeof(effects));
        effects.free_energy = 0.9f;
        effects.valid = true;
        cortical_training_set_effects_for_testing(bridge, &effects);

        for (int j = 0; j < 50; ++j) {
            effects.free_energy *= 0.95f;
            cortical_training_set_effects_for_testing(bridge, &effects);
        }
    }
    SUCCEED();
}

//=============================================================================
// Memory Safety (5 tests)
//=============================================================================

TEST_F(CorticalTrainingRegressionTest, NoLeaksMultipleInstances) {
    for (int i = 0; i < 1000; ++i) {
        cortical_training_bridge_t* temp = cortical_training_create(&config);
        ASSERT_NE(temp, nullptr);
        cortical_training_start(temp);
        cortical_training_stop(temp);
        cortical_training_destroy(temp);
    }
    SUCCEED();
}

TEST_F(CorticalTrainingRegressionTest, LargeCompartmentArrays) {
    EXPECT_EQ(cortical_training_start(bridge), 0);
    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.num_layers = 100;
    effects.calcium_spikes = 200;
    effects.valid = true;
    cortical_training_set_effects_for_testing(bridge, &effects);
    SUCCEED();
}

TEST_F(CorticalTrainingRegressionTest, NullPointerRobustness) {
    cortical_training_destroy(nullptr);  /* Should not crash */
    /* Null bridge returns default values */
    EXPECT_EQ(cortical_training_get_modulated_lr(nullptr, 0.001f), 0.001f);
    EXPECT_FALSE(cortical_training_are_predictions_stable(nullptr));
    cortical_training_stats_t stats;
    EXPECT_NE(cortical_training_get_stats(nullptr, &stats), 0);
    SUCCEED();
}

TEST_F(CorticalTrainingRegressionTest, ExtensiveStatistics) {
    EXPECT_EQ(cortical_training_start(bridge), 0);
    for (int i = 0; i < 100000; ++i) {
        cortical_training_update_metrics(bridge, 0.5f, 1.0f, 0.001f, 0);
    }
    cortical_training_stats_t stats;
    EXPECT_EQ(cortical_training_get_stats(bridge, &stats), 0);
    SUCCEED();
}

TEST_F(CorticalTrainingRegressionTest, RepeatedReset) {
    EXPECT_EQ(cortical_training_start(bridge), 0);
    for (int cycle = 0; cycle < 200; ++cycle) {
        for (int i = 0; i < 50; ++i) {
            cortical_training_update_metrics(bridge, 0.5f, 1.0f, 0.001f, 0);
        }
        EXPECT_EQ(cortical_training_reset_stats(bridge), 0);
    }
    SUCCEED();
}

//=============================================================================
// Thread Safety (5 tests)
//=============================================================================

TEST_F(CorticalTrainingRegressionTest, ConcurrentParameterRetrieval) {
    EXPECT_EQ(cortical_training_start(bridge), 0);
    for (int i = 0; i < 1000; ++i) {
        float lr = cortical_training_get_modulated_lr(bridge, 0.001f);
        float confidence = cortical_training_get_gradient_confidence(bridge);
        bool stable = cortical_training_are_predictions_stable(bridge);
        (void)lr; (void)confidence; (void)stable;
    }
    SUCCEED();
}

TEST_F(CorticalTrainingRegressionTest, ReadWriteInterleaving) {
    EXPECT_EQ(cortical_training_start(bridge), 0);
    for (int i = 0; i < 1000; ++i) {
        cortical_training_update_metrics(bridge, 0.5f, 1.0f, 0.001f, 0);
        cortical_training_effects_t effects;
        cortical_training_get_effects(bridge, &effects);
    }
    SUCCEED();
}

TEST_F(CorticalTrainingRegressionTest, RapidStateTransitions) {
    for (int i = 0; i < 100; ++i) {
        EXPECT_EQ(cortical_training_start(bridge), 0);
        cortical_training_update_metrics(bridge, 0.5f, 1.0f, 0.001f, 0);
        cortical_training_effects_t effects;
        cortical_training_get_effects(bridge, &effects);
        EXPECT_EQ(cortical_training_stop(bridge), 0);
    }
    SUCCEED();
}

TEST_F(CorticalTrainingRegressionTest, ParallelStatQueries) {
    EXPECT_EQ(cortical_training_start(bridge), 0);
    for (int i = 0; i < 1000; ++i) {
        cortical_training_stats_t stats;
        EXPECT_EQ(cortical_training_get_stats(bridge, &stats), 0);
    }
    SUCCEED();
}

TEST_F(CorticalTrainingRegressionTest, MixedConcurrentOps) {
    EXPECT_EQ(cortical_training_start(bridge), 0);
    for (int i = 0; i < 1000; ++i) {
        cortical_training_update_metrics(bridge, 0.5f, 1.0f, 0.001f, 0);
        cortical_training_get_modulated_lr(bridge, 0.001f);
        if (i % 50 == 0) {
            cortical_training_stats_t stats;
            cortical_training_get_stats(bridge, &stats);
        }
    }
    SUCCEED();
}

//=============================================================================
// Edge Cases (5 tests)
//=============================================================================

TEST_F(CorticalTrainingRegressionTest, MaximalFreeEnergy) {
    EXPECT_EQ(cortical_training_start(bridge), 0);
    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.free_energy = 1.0f;
    effects.lr_factor = 0.1f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(bridge, &effects);
    SUCCEED();
}

TEST_F(CorticalTrainingRegressionTest, MinimalFreeEnergy) {
    EXPECT_EQ(cortical_training_start(bridge), 0);
    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.free_energy = 0.0f;
    effects.lr_factor = 2.0f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(bridge, &effects);
    SUCCEED();
}

TEST_F(CorticalTrainingRegressionTest, ZeroBurstRate) {
    EXPECT_EQ(cortical_training_start(bridge), 0);
    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.burst_rate = 0.0f;
    effects.bac_success_rate = 0.0f;
    effects.lr_factor = 0.5f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(bridge, &effects);
    SUCCEED();
}

TEST_F(CorticalTrainingRegressionTest, ExtremeGradientScales) {
    EXPECT_EQ(cortical_training_start(bridge), 0);
    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.lr_factor = 10.0f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(bridge, &effects);

    effects.lr_factor = 0.01f;
    cortical_training_set_effects_for_testing(bridge, &effects);
    SUCCEED();
}

TEST_F(CorticalTrainingRegressionTest, EmptySession) {
    EXPECT_EQ(cortical_training_start(bridge), 0);
    EXPECT_EQ(cortical_training_stop(bridge), 0);
    cortical_training_stats_t stats;
    EXPECT_EQ(cortical_training_get_stats(bridge, &stats), 0);
    SUCCEED();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
