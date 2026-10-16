/**
 * @file test_predictive_hierarchy.cpp
 * @brief Unit tests for Predictive Coding Hierarchy
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

/* Headers have their own extern "C" guards - don't wrap them to avoid
 * CUDA C++ function conflicts */
#include "cognitive/predictive/nimcp_predictive_hierarchy.h"

class PredictiveHierarchyTest : public ::testing::Test {
protected:
    predictive_hierarchy_t* hier = nullptr;
    pred_hier_config_t config;
    uint32_t dims[4] = {64, 32, 16, 8};

    void SetUp() override {
        pred_hier_simple_config(&config, 4, dims);
        config.gpu_mode = PRED_HIER_GPU_DISABLED;
    }

    void TearDown() override {
        if (hier) {
            pred_hier_destroy(hier);
            hier = nullptr;
        }
        pred_hier_free_config(&config);
    }

    void FillInput(float* input, float value) {
        for (uint32_t i = 0; i < dims[0]; i++) {
            input[i] = value + (float)i / dims[0];
        }
    }
};

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

TEST_F(PredictiveHierarchyTest, DefaultConfigValid) {
    pred_hier_config_t cfg;
    ASSERT_EQ(pred_hier_default_config(&cfg), NIMCP_SUCCESS);
    EXPECT_EQ(cfg.num_levels, PRED_HIER_DEFAULT_LEVELS);
    EXPECT_GT(cfg.state_update_rate, 0.0f);
    pred_hier_free_config(&cfg);
}

TEST_F(PredictiveHierarchyTest, DefaultConfigNullReturnsError) {
    EXPECT_NE(pred_hier_default_config(nullptr), NIMCP_SUCCESS);
}

TEST_F(PredictiveHierarchyTest, SimpleConfigValid) {
    pred_hier_config_t cfg;
    ASSERT_EQ(pred_hier_simple_config(&cfg, 4, dims), NIMCP_SUCCESS);
    EXPECT_EQ(cfg.num_levels, 4);
    ASSERT_NE(cfg.level_configs, nullptr);
    EXPECT_EQ(cfg.level_configs[0].dim, 64);
    EXPECT_EQ(cfg.level_configs[3].dim, 8);
    pred_hier_free_config(&cfg);
}

TEST_F(PredictiveHierarchyTest, SimpleConfigZeroLevelsReturnsError) {
    pred_hier_config_t cfg;
    EXPECT_NE(pred_hier_simple_config(&cfg, 0, dims), NIMCP_SUCCESS);
}

TEST_F(PredictiveHierarchyTest, SimpleConfigNullDimsReturnsError) {
    pred_hier_config_t cfg;
    EXPECT_NE(pred_hier_simple_config(&cfg, 4, nullptr), NIMCP_SUCCESS);
}

TEST_F(PredictiveHierarchyTest, FreeConfigNullIsSafe) {
    pred_hier_free_config(nullptr);
    SUCCEED();
}

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(PredictiveHierarchyTest, CreateWithDefaultConfig) {
    /* Default config requires level_configs to be set manually or use simple_config */
    pred_hier_config_t cfg;
    uint32_t simple_dims[] = {64, 32, 16};
    pred_hier_simple_config(&cfg, 3, simple_dims);
    cfg.gpu_mode = PRED_HIER_GPU_DISABLED;
    hier = pred_hier_create(&cfg);
    ASSERT_NE(hier, nullptr);
    pred_hier_free_config(&cfg);
}

TEST_F(PredictiveHierarchyTest, CreateWithSimpleConfig) {
    hier = pred_hier_create(&config);
    ASSERT_NE(hier, nullptr);
}

TEST_F(PredictiveHierarchyTest, DestroyNullIsSafe) {
    pred_hier_destroy(nullptr);
    SUCCEED();
}

TEST_F(PredictiveHierarchyTest, ResetSucceeds) {
    hier = pred_hier_create(&config);
    ASSERT_NE(hier, nullptr);

    float input[64];
    FillInput(input, 1.0f);
    pred_hier_forward(hier, input);

    EXPECT_EQ(pred_hier_reset(hier), NIMCP_SUCCESS);
}

TEST_F(PredictiveHierarchyTest, ResetNullReturnsError) {
    EXPECT_NE(pred_hier_reset(nullptr), NIMCP_SUCCESS);
}

/* ============================================================================
 * Level Accessors Tests
 * ============================================================================ */

TEST_F(PredictiveHierarchyTest, NumLevelsReturnsCorrect) {
    hier = pred_hier_create(&config);
    ASSERT_NE(hier, nullptr);
    EXPECT_EQ(pred_hier_num_levels(hier), 4);
}

TEST_F(PredictiveHierarchyTest, NumLevelsNullReturnsZero) {
    EXPECT_EQ(pred_hier_num_levels(nullptr), 0);
}

TEST_F(PredictiveHierarchyTest, LevelDimReturnsCorrect) {
    hier = pred_hier_create(&config);
    ASSERT_NE(hier, nullptr);
    EXPECT_EQ(pred_hier_level_dim(hier, 0), 64);
    EXPECT_EQ(pred_hier_level_dim(hier, 1), 32);
    EXPECT_EQ(pred_hier_level_dim(hier, 2), 16);
    EXPECT_EQ(pred_hier_level_dim(hier, 3), 8);
}

TEST_F(PredictiveHierarchyTest, LevelDimInvalidReturnsZero) {
    hier = pred_hier_create(&config);
    ASSERT_NE(hier, nullptr);
    EXPECT_EQ(pred_hier_level_dim(hier, 100), 0);
}

/* ============================================================================
 * Forward Pass Tests
 * ============================================================================ */

TEST_F(PredictiveHierarchyTest, ForwardSucceeds) {
    hier = pred_hier_create(&config);
    ASSERT_NE(hier, nullptr);

    float input[64];
    FillInput(input, 1.0f);

    EXPECT_EQ(pred_hier_forward(hier, input), NIMCP_SUCCESS);
}

TEST_F(PredictiveHierarchyTest, ForwardNullHierReturnsError) {
    float input[64];
    FillInput(input, 1.0f);
    EXPECT_NE(pred_hier_forward(nullptr, input), NIMCP_SUCCESS);
}

TEST_F(PredictiveHierarchyTest, ForwardNullInputReturnsError) {
    hier = pred_hier_create(&config);
    ASSERT_NE(hier, nullptr);
    EXPECT_NE(pred_hier_forward(hier, nullptr), NIMCP_SUCCESS);
}

/* ============================================================================
 * Backward Pass Tests
 * ============================================================================ */

TEST_F(PredictiveHierarchyTest, BackwardSucceeds) {
    hier = pred_hier_create(&config);
    ASSERT_NE(hier, nullptr);

    EXPECT_EQ(pred_hier_backward(hier), NIMCP_SUCCESS);
}

TEST_F(PredictiveHierarchyTest, BackwardNullReturnsError) {
    EXPECT_NE(pred_hier_backward(nullptr), NIMCP_SUCCESS);
}

/* ============================================================================
 * Full Update Tests
 * ============================================================================ */

TEST_F(PredictiveHierarchyTest, UpdateSucceeds) {
    hier = pred_hier_create(&config);
    ASSERT_NE(hier, nullptr);

    float input[64];
    FillInput(input, 1.0f);

    EXPECT_EQ(pred_hier_update(hier, input, nullptr), NIMCP_SUCCESS);
}

TEST_F(PredictiveHierarchyTest, UpdateWithResultSucceeds) {
    hier = pred_hier_create(&config);
    ASSERT_NE(hier, nullptr);

    float input[64];
    FillInput(input, 1.0f);

    pred_hier_result_t* result = pred_hier_result_create(4, dims);
    ASSERT_NE(result, nullptr);

    EXPECT_EQ(pred_hier_update(hier, input, result), NIMCP_SUCCESS);
    EXPECT_EQ(result->num_levels, 4);
    EXPECT_FALSE(std::isnan(result->total_free_energy));

    pred_hier_result_destroy(result);
}

TEST_F(PredictiveHierarchyTest, MultipleUpdatesConverge) {
    hier = pred_hier_create(&config);
    ASSERT_NE(hier, nullptr);

    float input[64];
    FillInput(input, 1.0f);

    float prev_fe = std::numeric_limits<float>::max();
    for (int i = 0; i < 10; i++) {
        pred_hier_update(hier, input, nullptr);
        float fe = pred_hier_compute_free_energy(hier);
        if (!std::isnan(fe) && !std::isnan(prev_fe)) {
            // Free energy should generally decrease or stabilize
            EXPECT_LE(fe, prev_fe * 1.1f); // Allow small increase due to noise
        }
        prev_fe = fe;
    }
}

/* ============================================================================
 * State Access Tests
 * ============================================================================ */

TEST_F(PredictiveHierarchyTest, GetStateSucceeds) {
    hier = pred_hier_create(&config);
    ASSERT_NE(hier, nullptr);

    float state[64];
    EXPECT_EQ(pred_hier_get_state(hier, 0, state), NIMCP_SUCCESS);
}

TEST_F(PredictiveHierarchyTest, SetStateSucceeds) {
    hier = pred_hier_create(&config);
    ASSERT_NE(hier, nullptr);

    float state[64];
    FillInput(state, 2.0f);

    EXPECT_EQ(pred_hier_set_state(hier, 0, state), NIMCP_SUCCESS);

    float retrieved[64];
    EXPECT_EQ(pred_hier_get_state(hier, 0, retrieved), NIMCP_SUCCESS);

    for (uint32_t i = 0; i < 64; i++) {
        EXPECT_FLOAT_EQ(retrieved[i], state[i]);
    }
}

TEST_F(PredictiveHierarchyTest, GetStateInvalidLevelReturnsError) {
    hier = pred_hier_create(&config);
    ASSERT_NE(hier, nullptr);

    float state[64];
    EXPECT_NE(pred_hier_get_state(hier, 100, state), NIMCP_SUCCESS);
}

/* ============================================================================
 * Prediction/Error Access Tests
 * ============================================================================ */

TEST_F(PredictiveHierarchyTest, GetPredictionSucceeds) {
    hier = pred_hier_create(&config);
    ASSERT_NE(hier, nullptr);

    float input[64];
    FillInput(input, 1.0f);
    pred_hier_forward(hier, input);

    float prediction[64];
    EXPECT_EQ(pred_hier_get_prediction(hier, 0, prediction), NIMCP_SUCCESS);
}

TEST_F(PredictiveHierarchyTest, GetErrorSucceeds) {
    hier = pred_hier_create(&config);
    ASSERT_NE(hier, nullptr);

    float input[64];
    FillInput(input, 1.0f);
    pred_hier_update(hier, input, nullptr);

    float error[64];
    EXPECT_EQ(pred_hier_get_error(hier, 0, error), NIMCP_SUCCESS);
}

/* ============================================================================
 * Precision Tests
 * ============================================================================ */

TEST_F(PredictiveHierarchyTest, GetPrecisionSucceeds) {
    hier = pred_hier_create(&config);
    ASSERT_NE(hier, nullptr);

    float precision[64];
    EXPECT_EQ(pred_hier_get_precision(hier, 0, precision), NIMCP_SUCCESS);

    for (uint32_t i = 0; i < 64; i++) {
        EXPECT_GT(precision[i], 0.0f);
    }
}

TEST_F(PredictiveHierarchyTest, SetPrecisionSucceeds) {
    hier = pred_hier_create(&config);
    ASSERT_NE(hier, nullptr);

    float precision[64];
    for (uint32_t i = 0; i < 64; i++) {
        precision[i] = 2.0f;
    }

    EXPECT_EQ(pred_hier_set_precision(hier, 0, precision), NIMCP_SUCCESS);

    float retrieved[64];
    EXPECT_EQ(pred_hier_get_precision(hier, 0, retrieved), NIMCP_SUCCESS);

    for (uint32_t i = 0; i < 64; i++) {
        EXPECT_FLOAT_EQ(retrieved[i], 2.0f);
    }
}

TEST_F(PredictiveHierarchyTest, UpdatePrecisionSucceeds) {
    hier = pred_hier_create(&config);
    ASSERT_NE(hier, nullptr);

    float input[64];
    FillInput(input, 1.0f);
    pred_hier_update(hier, input, nullptr);

    EXPECT_EQ(pred_hier_update_precision(hier), NIMCP_SUCCESS);
}

/* ============================================================================
 * Free Energy Tests
 * ============================================================================ */

TEST_F(PredictiveHierarchyTest, ComputeFreeEnergyReturnsValue) {
    hier = pred_hier_create(&config);
    ASSERT_NE(hier, nullptr);

    float input[64];
    FillInput(input, 1.0f);
    pred_hier_update(hier, input, nullptr);

    float fe = pred_hier_compute_free_energy(hier);
    EXPECT_FALSE(std::isnan(fe));
}

TEST_F(PredictiveHierarchyTest, ComputeFreeEnergyNullReturnsNaN) {
    float fe = pred_hier_compute_free_energy(nullptr);
    EXPECT_TRUE(std::isnan(fe));
}

TEST_F(PredictiveHierarchyTest, GetLevelFreeEnergyReturnsValue) {
    hier = pred_hier_create(&config);
    ASSERT_NE(hier, nullptr);

    float input[64];
    FillInput(input, 1.0f);
    pred_hier_update(hier, input, nullptr);

    for (uint32_t level = 0; level < 4; level++) {
        float fe = pred_hier_get_level_free_energy(hier, level);
        EXPECT_FALSE(std::isnan(fe));
    }
}

TEST_F(PredictiveHierarchyTest, GetLevelFreeEnergyInvalidReturnsNaN) {
    hier = pred_hier_create(&config);
    ASSERT_NE(hier, nullptr);

    float fe = pred_hier_get_level_free_energy(hier, 100);
    EXPECT_TRUE(std::isnan(fe));
}

/* ============================================================================
 * Training Tests
 * ============================================================================ */

TEST_F(PredictiveHierarchyTest, SetTrainingModeSucceeds) {
    hier = pred_hier_create(&config);
    ASSERT_NE(hier, nullptr);

    EXPECT_EQ(pred_hier_set_training(hier, true), NIMCP_SUCCESS);
    EXPECT_EQ(pred_hier_set_training(hier, false), NIMCP_SUCCESS);
}

TEST_F(PredictiveHierarchyTest, SetTrainingNullReturnsError) {
    EXPECT_NE(pred_hier_set_training(nullptr, true), NIMCP_SUCCESS);
}

TEST_F(PredictiveHierarchyTest, LearnStepSucceeds) {
    config.enable_learning = true;
    hier = pred_hier_create(&config);
    ASSERT_NE(hier, nullptr);

    pred_hier_set_training(hier, true);

    float input[64];
    FillInput(input, 1.0f);

    float loss;
    EXPECT_EQ(pred_hier_learn_step(hier, input, &loss), NIMCP_SUCCESS);
    EXPECT_FALSE(std::isnan(loss));
}

TEST_F(PredictiveHierarchyTest, LearnStepReducesLoss) {
    config.enable_learning = true;
    hier = pred_hier_create(&config);
    ASSERT_NE(hier, nullptr);

    pred_hier_set_training(hier, true);

    float input[64];
    FillInput(input, 1.0f);

    float losses[5];
    for (int i = 0; i < 5; i++) {
        pred_hier_learn_step(hier, input, &losses[i]);
    }

    // Later losses should generally be lower
    EXPECT_LE(losses[4], losses[0] * 1.1f);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(PredictiveHierarchyTest, GetStatsSucceeds) {
    hier = pred_hier_create(&config);
    ASSERT_NE(hier, nullptr);

    pred_hier_stats_t stats;
    EXPECT_EQ(pred_hier_get_stats(hier, &stats), NIMCP_SUCCESS);
    EXPECT_EQ(stats.forward_passes, 0);
    EXPECT_EQ(stats.backward_passes, 0);
}

TEST_F(PredictiveHierarchyTest, StatsUpdateAfterPasses) {
    hier = pred_hier_create(&config);
    ASSERT_NE(hier, nullptr);

    float input[64];
    FillInput(input, 1.0f);

    pred_hier_forward(hier, input);
    pred_hier_backward(hier);

    pred_hier_stats_t stats;
    pred_hier_get_stats(hier, &stats);
    EXPECT_EQ(stats.forward_passes, 1);
    EXPECT_EQ(stats.backward_passes, 1);
}

TEST_F(PredictiveHierarchyTest, ResetStatsSucceeds) {
    hier = pred_hier_create(&config);
    ASSERT_NE(hier, nullptr);

    float input[64];
    FillInput(input, 1.0f);
    pred_hier_forward(hier, input);

    EXPECT_EQ(pred_hier_reset_stats(hier), NIMCP_SUCCESS);

    pred_hier_stats_t stats;
    pred_hier_get_stats(hier, &stats);
    EXPECT_EQ(stats.forward_passes, 0);
}

/* ============================================================================
 * Bio-Async Tests
 * ============================================================================ */

TEST_F(PredictiveHierarchyTest, ConnectBioAsyncSucceeds) {
    hier = pred_hier_create(&config);
    ASSERT_NE(hier, nullptr);

    EXPECT_EQ(pred_hier_connect_bio_async(hier), NIMCP_SUCCESS);
}

TEST_F(PredictiveHierarchyTest, DisconnectBioAsyncSucceeds) {
    hier = pred_hier_create(&config);
    ASSERT_NE(hier, nullptr);

    EXPECT_EQ(pred_hier_disconnect_bio_async(hier), NIMCP_SUCCESS);
}

/* ============================================================================
 * Result Management Tests
 * ============================================================================ */

TEST_F(PredictiveHierarchyTest, ResultCreateDestroy) {
    pred_hier_result_t* result = pred_hier_result_create(4, dims);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->num_levels, 4);
    ASSERT_NE(result->level_results, nullptr);

    for (uint32_t i = 0; i < 4; i++) {
        EXPECT_NE(result->level_results[i].prediction, nullptr);
        EXPECT_NE(result->level_results[i].error, nullptr);
    }

    pred_hier_result_destroy(result);
    SUCCEED();
}

TEST_F(PredictiveHierarchyTest, ResultDestroyNullIsSafe) {
    pred_hier_result_destroy(nullptr);
    SUCCEED();
}

TEST_F(PredictiveHierarchyTest, ResultCreateZeroLevelsReturnsNull) {
    pred_hier_result_t* result = pred_hier_result_create(0, dims);
    EXPECT_EQ(result, nullptr);
}

/* ============================================================================
 * String Conversion Tests
 * ============================================================================ */

TEST_F(PredictiveHierarchyTest, UpdateModeToString) {
    EXPECT_STREQ(pred_hier_update_mode_to_string(PRED_HIER_UPDATE_SEQUENTIAL), "SEQUENTIAL");
    EXPECT_STREQ(pred_hier_update_mode_to_string(PRED_HIER_UPDATE_PARALLEL), "PARALLEL");
    EXPECT_STREQ(pred_hier_update_mode_to_string(PRED_HIER_UPDATE_INTERLEAVED), "INTERLEAVED");
}

TEST_F(PredictiveHierarchyTest, GenModelToString) {
    EXPECT_STREQ(pred_hier_gen_model_to_string(PRED_HIER_GEN_LINEAR), "LINEAR");
    EXPECT_STREQ(pred_hier_gen_model_to_string(PRED_HIER_GEN_MLP), "MLP");
    EXPECT_STREQ(pred_hier_gen_model_to_string(PRED_HIER_GEN_CONV), "CONV");
    EXPECT_STREQ(pred_hier_gen_model_to_string(PRED_HIER_GEN_ATTENTION), "ATTENTION");
}

/* ============================================================================
 * Update Mode Tests
 * ============================================================================ */

TEST_F(PredictiveHierarchyTest, SequentialUpdateWorks) {
    config.update_mode = PRED_HIER_UPDATE_SEQUENTIAL;
    hier = pred_hier_create(&config);
    ASSERT_NE(hier, nullptr);

    float input[64];
    FillInput(input, 1.0f);

    EXPECT_EQ(pred_hier_update(hier, input, nullptr), NIMCP_SUCCESS);
}

TEST_F(PredictiveHierarchyTest, ParallelUpdateWorks) {
    config.update_mode = PRED_HIER_UPDATE_PARALLEL;
    hier = pred_hier_create(&config);
    ASSERT_NE(hier, nullptr);

    float input[64];
    FillInput(input, 1.0f);

    EXPECT_EQ(pred_hier_update(hier, input, nullptr), NIMCP_SUCCESS);
}

TEST_F(PredictiveHierarchyTest, InterleavedUpdateWorks) {
    config.update_mode = PRED_HIER_UPDATE_INTERLEAVED;
    hier = pred_hier_create(&config);
    ASSERT_NE(hier, nullptr);

    float input[64];
    FillInput(input, 1.0f);

    EXPECT_EQ(pred_hier_update(hier, input, nullptr), NIMCP_SUCCESS);
}

/* ============================================================================
 * FEP Integration Tests
 * ============================================================================ */

TEST_F(PredictiveHierarchyTest, FEPEnabledWorks) {
    config.enable_fep = true;
    hier = pred_hier_create(&config);
    ASSERT_NE(hier, nullptr);

    float input[64];
    FillInput(input, 1.0f);

    pred_hier_result_t* result = pred_hier_result_create(4, dims);
    ASSERT_NE(result, nullptr);

    EXPECT_EQ(pred_hier_update(hier, input, result), NIMCP_SUCCESS);
    EXPECT_FALSE(std::isnan(result->complexity));
    EXPECT_FALSE(std::isnan(result->accuracy));

    pred_hier_result_destroy(result);
}

/* ============================================================================
 * Lateral Connections Tests
 * ============================================================================ */

TEST_F(PredictiveHierarchyTest, LateralConnectionsWork) {
    config.enable_lateral = true;
    hier = pred_hier_create(&config);
    ASSERT_NE(hier, nullptr);

    float input[64];
    FillInput(input, 1.0f);

    EXPECT_EQ(pred_hier_update(hier, input, nullptr), NIMCP_SUCCESS);
}
