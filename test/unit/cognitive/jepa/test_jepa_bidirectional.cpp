/**
 * @file test_jepa_bidirectional.cpp
 * @brief Unit tests for Bidirectional JEPA Predictor
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

/* Headers have their own extern "C" guards - don't wrap them to avoid
 * CUDA C++ function conflicts */
#include "cognitive/jepa/nimcp_jepa_bidirectional.h"
#include "cognitive/jepa/nimcp_jepa_latent.h"

class JepaBidirectionalTest : public ::testing::Test {
protected:
    jepa_bidirectional_t* bidir = nullptr;
    jepa_bidir_config_t config;

    void SetUp() override {
        jepa_bidir_default_config(&config);
        config.embedding_dim = 64;
        config.hidden_dim = 128;
        config.enable_forward = true;
        config.enable_backward = true;
        config.enable_lateral = false;
        config.enable_hierarchical = false;
        config.gpu_mode = JEPA_BIDIR_GPU_DISABLED;
    }

    void TearDown() override {
        if (bidir) {
            jepa_bidirectional_destroy(bidir);
            bidir = nullptr;
        }
    }
};

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

TEST_F(JepaBidirectionalTest, DefaultConfigValid) {
    jepa_bidir_config_t cfg;
    ASSERT_EQ(jepa_bidir_default_config(&cfg), NIMCP_SUCCESS);
    EXPECT_EQ(cfg.embedding_dim, 256);
    EXPECT_EQ(cfg.hidden_dim, 512);
    EXPECT_TRUE(cfg.enable_forward);
    EXPECT_TRUE(cfg.enable_backward);
    EXPECT_FALSE(cfg.enable_lateral);
    EXPECT_TRUE(cfg.enable_fep);
}

TEST_F(JepaBidirectionalTest, ValidateConfigNullReturnsError) {
    EXPECT_NE(jepa_bidir_validate_config(nullptr), NIMCP_SUCCESS);
}

TEST_F(JepaBidirectionalTest, ValidateConfigInvalidDimReturnsError) {
    config.embedding_dim = 0;
    EXPECT_NE(jepa_bidir_validate_config(&config), NIMCP_SUCCESS);
}

TEST_F(JepaBidirectionalTest, ValidateConfigInvalidLRReturnsError) {
    config.learning_rate = -0.1f;
    EXPECT_NE(jepa_bidir_validate_config(&config), NIMCP_SUCCESS);
}

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(JepaBidirectionalTest, CreateWithDefaultConfig) {
    bidir = jepa_bidirectional_create(nullptr);
    ASSERT_NE(bidir, nullptr);
}

TEST_F(JepaBidirectionalTest, CreateWithCustomConfig) {
    bidir = jepa_bidirectional_create(&config);
    ASSERT_NE(bidir, nullptr);
}

TEST_F(JepaBidirectionalTest, DestroyNullIsSafe) {
    jepa_bidirectional_destroy(nullptr);
    SUCCEED();
}

TEST_F(JepaBidirectionalTest, ResetClearsState) {
    bidir = jepa_bidirectional_create(&config);
    ASSERT_NE(bidir, nullptr);

    EXPECT_EQ(jepa_bidirectional_reset(bidir), NIMCP_SUCCESS);
}

TEST_F(JepaBidirectionalTest, ResetNullReturnsError) {
    EXPECT_NE(jepa_bidirectional_reset(nullptr), NIMCP_SUCCESS);
}

/* ============================================================================
 * Direction Tests
 * ============================================================================ */

TEST_F(JepaBidirectionalTest, ForwardDirectionEnabled) {
    bidir = jepa_bidirectional_create(&config);
    ASSERT_NE(bidir, nullptr);

    EXPECT_TRUE(jepa_bidirectional_is_direction_enabled(bidir, JEPA_DIR_FORWARD));
}

TEST_F(JepaBidirectionalTest, BackwardDirectionEnabled) {
    bidir = jepa_bidirectional_create(&config);
    ASSERT_NE(bidir, nullptr);

    EXPECT_TRUE(jepa_bidirectional_is_direction_enabled(bidir, JEPA_DIR_BACKWARD));
}

TEST_F(JepaBidirectionalTest, LateralDirectionDisabledByDefault) {
    bidir = jepa_bidirectional_create(&config);
    ASSERT_NE(bidir, nullptr);

    EXPECT_FALSE(jepa_bidirectional_is_direction_enabled(bidir, JEPA_DIR_LATERAL));
}

TEST_F(JepaBidirectionalTest, EnableDisableDirection) {
    bidir = jepa_bidirectional_create(&config);
    ASSERT_NE(bidir, nullptr);

    EXPECT_EQ(jepa_bidirectional_set_direction_enabled(bidir, JEPA_DIR_LATERAL, true),
              NIMCP_SUCCESS);
    EXPECT_TRUE(jepa_bidirectional_is_direction_enabled(bidir, JEPA_DIR_LATERAL));

    EXPECT_EQ(jepa_bidirectional_set_direction_enabled(bidir, JEPA_DIR_LATERAL, false),
              NIMCP_SUCCESS);
    EXPECT_FALSE(jepa_bidirectional_is_direction_enabled(bidir, JEPA_DIR_LATERAL));
}

/* ============================================================================
 * Prediction Tests
 * ============================================================================ */

TEST_F(JepaBidirectionalTest, PredictForwardSucceeds) {
    bidir = jepa_bidirectional_create(&config);
    ASSERT_NE(bidir, nullptr);

    jepa_latent_t* input = jepa_latent_create_dim(config.embedding_dim);
    ASSERT_NE(input, nullptr);

    for (uint32_t i = 0; i < config.embedding_dim; i++) {
        input->embedding[i] = (float)i / config.embedding_dim;
    }

    jepa_bidir_result_t* result = jepa_bidir_result_create(config.embedding_dim);
    ASSERT_NE(result, nullptr);

    EXPECT_EQ(jepa_bidirectional_predict(bidir, JEPA_DIR_FORWARD, input, result),
              NIMCP_SUCCESS);
    EXPECT_EQ(result->direction, JEPA_DIR_FORWARD);
    EXPECT_GT(result->precision, 0.0f);
    EXPECT_GE(result->confidence, 0.0f);
    EXPECT_LE(result->confidence, 1.0f);

    jepa_latent_destroy(input);
    jepa_bidir_result_destroy(result);
}

TEST_F(JepaBidirectionalTest, PredictBackwardSucceeds) {
    bidir = jepa_bidirectional_create(&config);
    ASSERT_NE(bidir, nullptr);

    jepa_latent_t* input = jepa_latent_create_dim(config.embedding_dim);
    ASSERT_NE(input, nullptr);

    jepa_bidir_result_t* result = jepa_bidir_result_create(config.embedding_dim);
    ASSERT_NE(result, nullptr);

    EXPECT_EQ(jepa_bidirectional_predict(bidir, JEPA_DIR_BACKWARD, input, result),
              NIMCP_SUCCESS);
    EXPECT_EQ(result->direction, JEPA_DIR_BACKWARD);

    jepa_latent_destroy(input);
    jepa_bidir_result_destroy(result);
}

TEST_F(JepaBidirectionalTest, PredictDisabledDirectionFails) {
    bidir = jepa_bidirectional_create(&config);
    ASSERT_NE(bidir, nullptr);

    jepa_latent_t* input = jepa_latent_create_dim(config.embedding_dim);
    ASSERT_NE(input, nullptr);

    jepa_bidir_result_t* result = jepa_bidir_result_create(config.embedding_dim);
    ASSERT_NE(result, nullptr);

    EXPECT_NE(jepa_bidirectional_predict(bidir, JEPA_DIR_LATERAL, input, result),
              NIMCP_SUCCESS);

    jepa_latent_destroy(input);
    jepa_bidir_result_destroy(result);
}

TEST_F(JepaBidirectionalTest, PredictNullInputFails) {
    bidir = jepa_bidirectional_create(&config);
    ASSERT_NE(bidir, nullptr);

    jepa_bidir_result_t result;
    EXPECT_NE(jepa_bidirectional_predict(bidir, JEPA_DIR_FORWARD, nullptr, &result),
              NIMCP_SUCCESS);
}

TEST_F(JepaBidirectionalTest, PredictInvalidDirectionFails) {
    bidir = jepa_bidirectional_create(&config);
    ASSERT_NE(bidir, nullptr);

    jepa_latent_t* input = jepa_latent_create_dim(config.embedding_dim);
    jepa_bidir_result_t result;

    EXPECT_NE(jepa_bidirectional_predict(bidir, JEPA_DIR_COUNT, input, &result),
              NIMCP_SUCCESS);

    jepa_latent_destroy(input);
}

/* ============================================================================
 * Free Energy Tests
 * ============================================================================ */

TEST_F(JepaBidirectionalTest, ComputeFreeEnergyReturnsValue) {
    bidir = jepa_bidirectional_create(&config);
    ASSERT_NE(bidir, nullptr);

    float fe = jepa_bidirectional_compute_free_energy(bidir);
    EXPECT_FALSE(std::isnan(fe));
}

TEST_F(JepaBidirectionalTest, ComputeFreeEnergyNullReturnsNaN) {
    float fe = jepa_bidirectional_compute_free_energy(nullptr);
    EXPECT_TRUE(std::isnan(fe));
}

/* ============================================================================
 * Precision Tests
 * ============================================================================ */

TEST_F(JepaBidirectionalTest, GetPrecisionReturnsValue) {
    bidir = jepa_bidirectional_create(&config);
    ASSERT_NE(bidir, nullptr);

    float prec = jepa_bidirectional_get_precision(bidir, JEPA_DIR_FORWARD);
    EXPECT_GT(prec, 0.0f);
    EXPECT_FALSE(std::isnan(prec));
}

TEST_F(JepaBidirectionalTest, SetPrecisionSucceeds) {
    bidir = jepa_bidirectional_create(&config);
    ASSERT_NE(bidir, nullptr);

    EXPECT_EQ(jepa_bidirectional_set_precision(bidir, JEPA_DIR_FORWARD, 2.0f),
              NIMCP_SUCCESS);

    float prec = jepa_bidirectional_get_precision(bidir, JEPA_DIR_FORWARD);
    EXPECT_FLOAT_EQ(prec, 2.0f);
}

TEST_F(JepaBidirectionalTest, SetPrecisionInvalidValueFails) {
    bidir = jepa_bidirectional_create(&config);
    ASSERT_NE(bidir, nullptr);

    EXPECT_NE(jepa_bidirectional_set_precision(bidir, JEPA_DIR_FORWARD, -1.0f),
              NIMCP_SUCCESS);
}

TEST_F(JepaBidirectionalTest, UpdatePrecisionSucceeds) {
    bidir = jepa_bidirectional_create(&config);
    ASSERT_NE(bidir, nullptr);

    float initial_prec = jepa_bidirectional_get_precision(bidir, JEPA_DIR_FORWARD);

    EXPECT_EQ(jepa_bidirectional_update_precision(bidir, JEPA_DIR_FORWARD, 0.1f),
              NIMCP_SUCCESS);

    float new_prec = jepa_bidirectional_get_precision(bidir, JEPA_DIR_FORWARD);
    EXPECT_NE(new_prec, initial_prec);
}

/* ============================================================================
 * Training Tests
 * ============================================================================ */

TEST_F(JepaBidirectionalTest, SetTrainingModeSucceeds) {
    bidir = jepa_bidirectional_create(&config);
    ASSERT_NE(bidir, nullptr);

    EXPECT_EQ(jepa_bidirectional_set_training(bidir, true), NIMCP_SUCCESS);
    EXPECT_EQ(jepa_bidirectional_set_training(bidir, false), NIMCP_SUCCESS);
}

TEST_F(JepaBidirectionalTest, TrainStepSucceeds) {
    bidir = jepa_bidirectional_create(&config);
    ASSERT_NE(bidir, nullptr);

    jepa_latent_t* input = jepa_latent_create_dim(config.embedding_dim);
    jepa_latent_t* target = jepa_latent_create_dim(config.embedding_dim);
    ASSERT_NE(input, nullptr);
    ASSERT_NE(target, nullptr);

    for (uint32_t i = 0; i < config.embedding_dim; i++) {
        input->embedding[i] = (float)i / config.embedding_dim;
        target->embedding[i] = (float)(i + 1) / config.embedding_dim;
    }

    float loss = 0.0f;
    EXPECT_EQ(jepa_bidirectional_train_step(bidir, JEPA_DIR_FORWARD, input, target, &loss),
              NIMCP_SUCCESS);
    EXPECT_GE(loss, 0.0f);

    jepa_latent_destroy(input);
    jepa_latent_destroy(target);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(JepaBidirectionalTest, GetStatsSucceeds) {
    bidir = jepa_bidirectional_create(&config);
    ASSERT_NE(bidir, nullptr);

    jepa_bidir_stats_t stats;
    EXPECT_EQ(jepa_bidirectional_get_stats(bidir, &stats), NIMCP_SUCCESS);
    EXPECT_EQ(stats.total_predictions, 0);
}

TEST_F(JepaBidirectionalTest, ResetStatsSucceeds) {
    bidir = jepa_bidirectional_create(&config);
    ASSERT_NE(bidir, nullptr);

    EXPECT_EQ(jepa_bidirectional_reset_stats(bidir), NIMCP_SUCCESS);
}

TEST_F(JepaBidirectionalTest, PredictionCountIncrements) {
    bidir = jepa_bidirectional_create(&config);
    ASSERT_NE(bidir, nullptr);

    jepa_latent_t* input = jepa_latent_create_dim(config.embedding_dim);
    ASSERT_NE(input, nullptr);

    jepa_bidir_result_t* result = jepa_bidir_result_create(config.embedding_dim);
    ASSERT_NE(result, nullptr);

    jepa_bidirectional_predict(bidir, JEPA_DIR_FORWARD, input, result);
    jepa_bidirectional_predict(bidir, JEPA_DIR_FORWARD, input, result);

    jepa_bidir_stats_t stats;
    jepa_bidirectional_get_stats(bidir, &stats);
    EXPECT_EQ(stats.total_predictions, 2);
    EXPECT_EQ(stats.forward_predictions, 2);

    jepa_latent_destroy(input);
    jepa_bidir_result_destroy(result);
}

/* ============================================================================
 * String Conversion Tests
 * ============================================================================ */

TEST_F(JepaBidirectionalTest, DirectionToString) {
    EXPECT_STREQ(jepa_direction_to_string(JEPA_DIR_FORWARD), "FORWARD");
    EXPECT_STREQ(jepa_direction_to_string(JEPA_DIR_BACKWARD), "BACKWARD");
    EXPECT_STREQ(jepa_direction_to_string(JEPA_DIR_LATERAL), "LATERAL");
    EXPECT_STREQ(jepa_direction_to_string(JEPA_DIR_HIERARCHICAL_UP), "HIERARCHICAL_UP");
    EXPECT_STREQ(jepa_direction_to_string(JEPA_DIR_HIERARCHICAL_DOWN), "HIERARCHICAL_DOWN");
    EXPECT_STREQ(jepa_direction_to_string(JEPA_DIR_MASKED), "MASKED");
    EXPECT_STREQ(jepa_direction_to_string(JEPA_DIR_ASSOCIATIVE), "ASSOCIATIVE");
}

TEST_F(JepaBidirectionalTest, StateToString) {
    EXPECT_STREQ(jepa_bidir_state_to_string(JEPA_BIDIR_STATE_IDLE), "IDLE");
    EXPECT_STREQ(jepa_bidir_state_to_string(JEPA_BIDIR_STATE_PREDICTING), "PREDICTING");
    EXPECT_STREQ(jepa_bidir_state_to_string(JEPA_BIDIR_STATE_TRAINING), "TRAINING");
    EXPECT_STREQ(jepa_bidir_state_to_string(JEPA_BIDIR_STATE_ERROR), "ERROR");
}

/* ============================================================================
 * Result Management Tests
 * ============================================================================ */

TEST_F(JepaBidirectionalTest, ResultCreateDestroy) {
    jepa_bidir_result_t* result = jepa_bidir_result_create(64);
    ASSERT_NE(result, nullptr);
    ASSERT_NE(result->prediction, nullptr);

    jepa_bidir_result_destroy(result);
    SUCCEED();
}

TEST_F(JepaBidirectionalTest, ResultDestroyNullIsSafe) {
    jepa_bidir_result_destroy(nullptr);
    SUCCEED();
}

TEST_F(JepaBidirectionalTest, MultiResultCreateDestroy) {
    jepa_bidir_multi_result_t* result = jepa_bidir_multi_result_create(3, 64);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->num_results, 3);

    for (uint32_t i = 0; i < result->num_results; i++) {
        EXPECT_NE(result->results[i].prediction, nullptr);
    }

    jepa_bidir_multi_result_destroy(result);
    SUCCEED();
}

/* ============================================================================
 * Bio-Async Tests
 * ============================================================================ */

TEST_F(JepaBidirectionalTest, ConnectBioAsyncSucceeds) {
    bidir = jepa_bidirectional_create(&config);
    ASSERT_NE(bidir, nullptr);

    EXPECT_EQ(jepa_bidirectional_connect_bio_async(bidir), NIMCP_SUCCESS);
}

TEST_F(JepaBidirectionalTest, DisconnectBioAsyncSucceeds) {
    bidir = jepa_bidirectional_create(&config);
    ASSERT_NE(bidir, nullptr);

    EXPECT_EQ(jepa_bidirectional_disconnect_bio_async(bidir), NIMCP_SUCCESS);
}
