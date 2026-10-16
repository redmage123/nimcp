/**
 * @file test_dragonfly_cnn_bridge.cpp
 * @brief Unit tests for Dragonfly-to-CNN Training Bridge
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "dragonfly/nimcp_dragonfly_cnn_bridge.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class DragonflyCNNBridgeTest : public ::testing::Test {
protected:
    dragonfly_cnn_bridge_t* bridge = nullptr;
    dragonfly_cnn_config_t config;

    void SetUp() override {
        ASSERT_EQ(0, dragonfly_cnn_bridge_default_config(&config));
    }

    void TearDown() override {
        if (bridge) {
            dragonfly_cnn_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }

    void CreateBridge() {
        bridge = dragonfly_cnn_bridge_create(nullptr, nullptr, &config);
        ASSERT_NE(nullptr, bridge);
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(DragonflyCNNBridgeTest, DefaultConfigValid) {
    dragonfly_cnn_config_t cfg;
    EXPECT_EQ(0, dragonfly_cnn_bridge_default_config(&cfg));

    EXPECT_EQ(CNN_TASK_MOTION_DETECTION, cfg.task);
    EXPECT_GT(cfg.frame_width, 0u);
    EXPECT_GT(cfg.frame_height, 0u);
    EXPECT_GT(cfg.batch_size, 0u);
    EXPECT_GT(cfg.learning_rate, 0.0f);
}

TEST_F(DragonflyCNNBridgeTest, DefaultConfigNullReturnsError) {
    EXPECT_EQ(-1, dragonfly_cnn_bridge_default_config(nullptr));
}

TEST_F(DragonflyCNNBridgeTest, ValidateConfigSuccess) {
    EXPECT_EQ(0, dragonfly_cnn_bridge_validate_config(&config));
}

TEST_F(DragonflyCNNBridgeTest, ValidateConfigNullReturnsError) {
    EXPECT_EQ(-1, dragonfly_cnn_bridge_validate_config(nullptr));
}

TEST_F(DragonflyCNNBridgeTest, ValidateConfigInvalidTask) {
    config.task = (dragonfly_cnn_task_t)99;
    EXPECT_EQ(-1, dragonfly_cnn_bridge_validate_config(&config));
}

TEST_F(DragonflyCNNBridgeTest, ValidateConfigZeroFrameSize) {
    config.frame_width = 0;
    EXPECT_EQ(-1, dragonfly_cnn_bridge_validate_config(&config));
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(DragonflyCNNBridgeTest, CreateWithDefaultConfig) {
    bridge = dragonfly_cnn_bridge_create(nullptr, nullptr, &config);
    EXPECT_NE(nullptr, bridge);
}

TEST_F(DragonflyCNNBridgeTest, CreateWithNullConfigUsesDefaults) {
    bridge = dragonfly_cnn_bridge_create(nullptr, nullptr, nullptr);
    EXPECT_NE(nullptr, bridge);
}

TEST_F(DragonflyCNNBridgeTest, DestroyNullSafe) {
    dragonfly_cnn_bridge_destroy(nullptr);
    /* No crash = success */
}

TEST_F(DragonflyCNNBridgeTest, ResetSuccess) {
    CreateBridge();
    EXPECT_EQ(0, dragonfly_cnn_bridge_reset(bridge));
}

TEST_F(DragonflyCNNBridgeTest, ResetNullReturnsError) {
    EXPECT_EQ(-1, dragonfly_cnn_bridge_reset(nullptr));
}

//=============================================================================
// Data Collection Tests
//=============================================================================

TEST_F(DragonflyCNNBridgeTest, AddFrameSuccess) {
    CreateBridge();

    dragonfly_cnn_frame_t frame;
    frame.width = 64;
    frame.height = 64;
    frame.channels = 1;
    frame.timestamp_ms = 0.0f;
    frame.data = new float[64 * 64];
    memset(frame.data, 0, 64 * 64 * sizeof(float));

    EXPECT_EQ(0, dragonfly_cnn_add_frame(bridge, &frame));

    delete[] frame.data;
}

TEST_F(DragonflyCNNBridgeTest, AddFrameNullReturnsError) {
    CreateBridge();
    EXPECT_EQ(-1, dragonfly_cnn_add_frame(nullptr, nullptr));
    EXPECT_EQ(-1, dragonfly_cnn_add_frame(bridge, nullptr));
}

TEST_F(DragonflyCNNBridgeTest, RecordEpisodeSuccess) {
    CreateBridge();
    EXPECT_EQ(0, dragonfly_cnn_record_episode(bridge, true));
    EXPECT_EQ(0, dragonfly_cnn_record_episode(bridge, false));
}

TEST_F(DragonflyCNNBridgeTest, RecordEpisodeNullReturnsError) {
    EXPECT_EQ(-1, dragonfly_cnn_record_episode(nullptr, true));
}

TEST_F(DragonflyCNNBridgeTest, ExtractFeaturesSuccess) {
    CreateBridge();

    float features[128];
    int extracted = dragonfly_cnn_extract_features(bridge, features, 128);
    EXPECT_GE(extracted, 0);
}

TEST_F(DragonflyCNNBridgeTest, ExtractFeaturesNullReturnsError) {
    CreateBridge();
    float features[128];
    EXPECT_EQ(-1, dragonfly_cnn_extract_features(nullptr, features, 128));
    EXPECT_EQ(-1, dragonfly_cnn_extract_features(bridge, nullptr, 128));
}

TEST_F(DragonflyCNNBridgeTest, GenerateSampleSuccess) {
    CreateBridge();

    dragonfly_cnn_sample_t sample;
    EXPECT_EQ(0, dragonfly_cnn_generate_sample(bridge, &sample));
}

//=============================================================================
// Training Tests
//=============================================================================

TEST_F(DragonflyCNNBridgeTest, TrainStepRequiresTrainingMode) {
    CreateBridge();
    /* Not in training mode */
    EXPECT_EQ(-1.0f, dragonfly_cnn_train_step(bridge));
}

TEST_F(DragonflyCNNBridgeTest, TrainStepInTrainingMode) {
    CreateBridge();
    dragonfly_cnn_set_training(bridge, true);

    float loss = dragonfly_cnn_train_step(bridge);
    EXPECT_GE(loss, 0.0f);
}

TEST_F(DragonflyCNNBridgeTest, TrainBatchSuccess) {
    CreateBridge();
    dragonfly_cnn_set_training(bridge, true);

    dragonfly_cnn_sample_t samples[4];
    memset(samples, 0, sizeof(samples));

    float loss = dragonfly_cnn_train_batch(bridge, samples, 4);
    EXPECT_GE(loss, 0.0f);
}

TEST_F(DragonflyCNNBridgeTest, EvaluateSuccess) {
    CreateBridge();

    dragonfly_cnn_sample_t samples[4];
    memset(samples, 0, sizeof(samples));

    float loss = dragonfly_cnn_evaluate(bridge, samples, 4);
    EXPECT_GE(loss, 0.0f);
}

TEST_F(DragonflyCNNBridgeTest, SetLearningRateSuccess) {
    CreateBridge();
    EXPECT_EQ(0, dragonfly_cnn_set_learning_rate(bridge, 0.01f));
}

TEST_F(DragonflyCNNBridgeTest, SetLearningRateInvalidReturnsError) {
    CreateBridge();
    EXPECT_EQ(-1, dragonfly_cnn_set_learning_rate(bridge, -0.01f));
    EXPECT_EQ(-1, dragonfly_cnn_set_learning_rate(bridge, 0.0f));
}

//=============================================================================
// Inference Tests
//=============================================================================

TEST_F(DragonflyCNNBridgeTest, InferSuccess) {
    CreateBridge();

    float output[64];
    int num_outputs = dragonfly_cnn_infer(bridge, output, 64);
    EXPECT_GE(num_outputs, 0);
}

TEST_F(DragonflyCNNBridgeTest, DetectMotionNoFrames) {
    CreateBridge();
    /* No frames added yet */
    float motion = dragonfly_cnn_detect_motion(bridge);
    EXPECT_EQ(0.0f, motion);
}

TEST_F(DragonflyCNNBridgeTest, DetectMotionWithFrames) {
    CreateBridge();

    /* Add two frames */
    for (int f = 0; f < 2; f++) {
        dragonfly_cnn_frame_t frame;
        frame.width = 64;
        frame.height = 64;
        frame.channels = 1;
        frame.timestamp_ms = (float)f * 16.0f;
        frame.data = new float[64 * 64];
        for (int i = 0; i < 64 * 64; i++) {
            frame.data[i] = (float)f * 0.1f + (float)i * 0.001f;
        }
        dragonfly_cnn_add_frame(bridge, &frame);
        delete[] frame.data;
    }

    float motion = dragonfly_cnn_detect_motion(bridge);
    EXPECT_GE(motion, 0.0f);
    EXPECT_LE(motion, 1.0f);
}

TEST_F(DragonflyCNNBridgeTest, EstimateVelocitySuccess) {
    CreateBridge();

    float vx, vy;
    EXPECT_EQ(0, dragonfly_cnn_estimate_velocity(bridge, &vx, &vy));
}

TEST_F(DragonflyCNNBridgeTest, PredictTrajectorySuccess) {
    CreateBridge();

    float predictions[20];  /* 10 steps * 2 (x,y) */
    EXPECT_EQ(0, dragonfly_cnn_predict_trajectory(bridge, predictions, 10));
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(DragonflyCNNBridgeTest, ConnectDragonflySuccess) {
    CreateBridge();
    int dummy;
    EXPECT_EQ(0, dragonfly_cnn_connect_dragonfly(bridge, (dragonfly_system_t*)&dummy));
}

TEST_F(DragonflyCNNBridgeTest, ConnectTrainerSuccess) {
    CreateBridge();
    int dummy;
    EXPECT_EQ(0, dragonfly_cnn_connect_trainer(bridge, &dummy));
}

TEST_F(DragonflyCNNBridgeTest, IsTrainingInitiallyFalse) {
    CreateBridge();
    EXPECT_FALSE(dragonfly_cnn_is_training(bridge));
}

TEST_F(DragonflyCNNBridgeTest, SetTrainingMode) {
    CreateBridge();
    EXPECT_EQ(0, dragonfly_cnn_set_training(bridge, true));
    EXPECT_TRUE(dragonfly_cnn_is_training(bridge));
    EXPECT_EQ(0, dragonfly_cnn_set_training(bridge, false));
    EXPECT_FALSE(dragonfly_cnn_is_training(bridge));
}

//=============================================================================
// SNN Conversion Tests
//=============================================================================

TEST_F(DragonflyCNNBridgeTest, PrepareSNNConversionSuccess) {
    CreateBridge();
    EXPECT_EQ(0, dragonfly_cnn_prepare_snn_conversion(bridge));
}

TEST_F(DragonflyCNNBridgeTest, GetActivationStatsSuccess) {
    CreateBridge();
    float mean, std;
    EXPECT_EQ(0, dragonfly_cnn_get_activation_stats(bridge, 0, &mean, &std));
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(DragonflyCNNBridgeTest, GetStatsSuccess) {
    CreateBridge();
    dragonfly_cnn_stats_t stats;
    EXPECT_EQ(0, dragonfly_cnn_bridge_get_stats(bridge, &stats));
}

TEST_F(DragonflyCNNBridgeTest, GetStatsNullReturnsError) {
    CreateBridge();
    dragonfly_cnn_stats_t stats;
    EXPECT_EQ(-1, dragonfly_cnn_bridge_get_stats(nullptr, &stats));
    EXPECT_EQ(-1, dragonfly_cnn_bridge_get_stats(bridge, nullptr));
}

TEST_F(DragonflyCNNBridgeTest, ResetStatsSuccess) {
    CreateBridge();
    EXPECT_EQ(0, dragonfly_cnn_bridge_reset_stats(bridge));
}

TEST_F(DragonflyCNNBridgeTest, StatsUpdatedAfterTraining) {
    CreateBridge();
    dragonfly_cnn_set_training(bridge, true);

    /* Train a few steps */
    for (int i = 0; i < 5; i++) {
        dragonfly_cnn_train_step(bridge);
    }

    dragonfly_cnn_stats_t stats;
    dragonfly_cnn_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(5u, stats.batches_processed);
}

//=============================================================================
// Utility Tests
//=============================================================================

TEST_F(DragonflyCNNBridgeTest, TaskNameValid) {
    EXPECT_STREQ("motion_detection", dragonfly_cnn_task_name(CNN_TASK_MOTION_DETECTION));
    EXPECT_STREQ("velocity_estimation", dragonfly_cnn_task_name(CNN_TASK_VELOCITY_ESTIMATION));
    EXPECT_STREQ("target_classification", dragonfly_cnn_task_name(CNN_TASK_TARGET_CLASSIFICATION));
}

TEST_F(DragonflyCNNBridgeTest, TaskNameInvalid) {
    EXPECT_STREQ("unknown", dragonfly_cnn_task_name((dragonfly_cnn_task_t)99));
}

TEST_F(DragonflyCNNBridgeTest, FeatureModeNameValid) {
    EXPECT_STREQ("raw_frames", dragonfly_cnn_feature_mode_name(CNN_FEATURE_RAW_FRAMES));
    EXPECT_STREQ("motion_vectors", dragonfly_cnn_feature_mode_name(CNN_FEATURE_MOTION_VECTORS));
}

TEST_F(DragonflyCNNBridgeTest, FeatureModeNameInvalid) {
    EXPECT_STREQ("unknown", dragonfly_cnn_feature_mode_name((dragonfly_cnn_feature_mode_t)99));
}
