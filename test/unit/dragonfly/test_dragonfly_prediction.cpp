/**
 * @file test_dragonfly_prediction.cpp
 * @brief Unit tests for trajectory prediction and evasion detection module
 *
 * @author NIMCP Team
 * @date 2024-12-27
 */

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "dragonfly/nimcp_dragonfly_prediction.h"

//=============================================================================
// Test Fixture
//=============================================================================

class PredictionTest : public ::testing::Test {
protected:
    dragonfly_predictor_t* pred = nullptr;

    void SetUp() override {
        pred = dragonfly_predictor_create(nullptr);
        ASSERT_NE(pred, nullptr);
    }

    void TearDown() override {
        if (pred) {
            dragonfly_predictor_destroy(pred);
            pred = nullptr;
        }
    }
};

class IMMPredictionTest : public ::testing::Test {
protected:
    dragonfly_predictor_t* pred = nullptr;

    void SetUp() override {
        prediction_config_t config = prediction_imm_config();
        pred = dragonfly_predictor_create(&config);
        ASSERT_NE(pred, nullptr);
    }

    void TearDown() override {
        if (pred) {
            dragonfly_predictor_destroy(pred);
            pred = nullptr;
        }
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(PredictionTest, DefaultConfig) {
    prediction_config_t config = prediction_default_config();
    EXPECT_FALSE(config.enable_imm);
    EXPECT_EQ(config.num_models, 1u);
    EXPECT_EQ(config.models[0], PRED_MODEL_CV);
    EXPECT_GT(config.max_prediction_ms, 0.0f);
    EXPECT_GT(config.prediction_steps, 0u);
}

TEST_F(PredictionTest, IMMConfig) {
    prediction_config_t config = prediction_imm_config();
    EXPECT_TRUE(config.enable_imm);
    EXPECT_EQ(config.num_models, 6u);
    EXPECT_EQ(config.models[0], PRED_MODEL_CV);
    EXPECT_EQ(config.models[1], PRED_MODEL_CA);
    EXPECT_EQ(config.models[2], PRED_MODEL_SINGER);
    EXPECT_EQ(config.models[3], PRED_MODEL_JINK);
    EXPECT_EQ(config.models[4], PRED_MODEL_WEAVE);
    EXPECT_EQ(config.models[5], PRED_MODEL_SPIRAL);
}

TEST_F(PredictionTest, ValidateConfig) {
    prediction_config_t config = prediction_default_config();
    EXPECT_TRUE(prediction_validate_config(&config));

    config.num_models = 0;
    EXPECT_FALSE(prediction_validate_config(&config));

    config = prediction_default_config();
    config.num_models = 100;
    EXPECT_FALSE(prediction_validate_config(&config));

    config = prediction_default_config();
    config.prediction_steps = 0;
    EXPECT_FALSE(prediction_validate_config(&config));

    EXPECT_FALSE(prediction_validate_config(nullptr));
}

TEST_F(PredictionTest, CreateWithCustomConfig) {
    prediction_config_t config = prediction_default_config();
    config.max_prediction_ms = 1000.0f;
    config.prediction_steps = 50;

    dragonfly_predictor_t* custom = dragonfly_predictor_create(&config);
    ASSERT_NE(custom, nullptr);

    prediction_config_t retrieved;
    EXPECT_EQ(dragonfly_predictor_get_config(custom, &retrieved), 0);
    EXPECT_FLOAT_EQ(retrieved.max_prediction_ms, 1000.0f);
    EXPECT_EQ(retrieved.prediction_steps, 50u);

    dragonfly_predictor_destroy(custom);
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(PredictionTest, CreateAndDestroy) {
    dragonfly_predictor_t* p = dragonfly_predictor_create(nullptr);
    ASSERT_NE(p, nullptr);
    dragonfly_predictor_destroy(p);
}

TEST_F(PredictionTest, DestroyNull) {
    dragonfly_predictor_destroy(nullptr);  // Should not crash
}

TEST_F(PredictionTest, Reset) {
    float pos[3] = {10.0f, 0.0f, 0.0f};
    dragonfly_predictor_update(pred, pos, nullptr, 0.016f);

    EXPECT_EQ(dragonfly_predictor_reset(pred), 0);

    // After reset, prediction should fail (no state)
    predicted_state_t state;
    EXPECT_EQ(dragonfly_predictor_get_state_at(pred, 100.0f, &state), -1);
}

//=============================================================================
// Core Prediction Tests
//=============================================================================

TEST_F(PredictionTest, UpdateWithPosition) {
    float pos[3] = {10.0f, 5.0f, 2.0f};
    EXPECT_EQ(dragonfly_predictor_update(pred, pos, nullptr, 0.016f), 0);
}

TEST_F(PredictionTest, UpdateWithVelocity) {
    float pos[3] = {10.0f, 5.0f, 2.0f};
    float vel[3] = {1.0f, 0.5f, 0.0f};
    EXPECT_EQ(dragonfly_predictor_update(pred, pos, vel, 0.016f), 0);
}

TEST_F(PredictionTest, UpdateInvalidDt) {
    float pos[3] = {10.0f, 0.0f, 0.0f};
    EXPECT_EQ(dragonfly_predictor_update(pred, pos, nullptr, 0.0f), -1);
    EXPECT_EQ(dragonfly_predictor_update(pred, pos, nullptr, -0.016f), -1);
}

TEST_F(PredictionTest, PredictStraightLine) {
    // Initialize with position and velocity
    float pos[3] = {0.0f, 0.0f, 0.0f};
    float vel[3] = {10.0f, 0.0f, 0.0f};  // Moving at 10 m/s in +x

    // Multiple updates to establish state
    for (int i = 0; i < 5; i++) {
        pos[0] = i * 0.16f;  // 10 m/s * 0.016s
        dragonfly_predictor_update(pred, pos, vel, 0.016f);
    }

    // Predict 100ms ahead
    predicted_state_t state;
    EXPECT_EQ(dragonfly_predictor_get_state_at(pred, 100.0f, &state), 0);

    // Position should have increased in x
    EXPECT_GT(state.position[0], pos[0]);
    EXPECT_GT(state.confidence, 0.0f);
    EXPECT_LE(state.confidence, 1.0f);
}

TEST_F(PredictionTest, PredictTrajectory) {
    float pos[3] = {0.0f, 0.0f, 0.0f};
    float vel[3] = {5.0f, 2.0f, 0.0f};

    for (int i = 0; i < 5; i++) {
        dragonfly_predictor_update(pred, pos, vel, 0.016f);
        pos[0] += vel[0] * 0.016f;
        pos[1] += vel[1] * 0.016f;
    }

    // Allocate trajectory
    trajectory_prediction_t prediction;
    prediction.num_points = 10;
    prediction.trajectory = dragonfly_trajectory_alloc(10);
    ASSERT_NE(prediction.trajectory, nullptr);

    EXPECT_EQ(dragonfly_predictor_predict(pred, 200.0f, &prediction), 0);

    // Check trajectory is monotonically increasing in time
    for (uint32_t i = 1; i < prediction.num_points; i++) {
        EXPECT_GT(prediction.trajectory[i].time_offset_ms,
                  prediction.trajectory[i-1].time_offset_ms);
    }

    // Check confidence decreases with time
    for (uint32_t i = 1; i < prediction.num_points; i++) {
        EXPECT_LE(prediction.trajectory[i].confidence,
                  prediction.trajectory[i-1].confidence + 0.01f);
    }

    dragonfly_trajectory_free(prediction.trajectory);
}

TEST_F(PredictionTest, PredictWithoutInit) {
    predicted_state_t state;
    EXPECT_EQ(dragonfly_predictor_get_state_at(pred, 100.0f, &state), -1);
}

//=============================================================================
// IMM Filter Tests
//=============================================================================

TEST_F(IMMPredictionTest, ModelProbabilities) {
    float pos[3] = {0.0f, 0.0f, 0.0f};
    float vel[3] = {5.0f, 0.0f, 0.0f};

    for (int i = 0; i < 10; i++) {
        dragonfly_predictor_update(pred, pos, vel, 0.016f);
        pos[0] += vel[0] * 0.016f;
    }

    float probs[6];
    EXPECT_EQ(dragonfly_predictor_get_model_probabilities(pred, probs, 6), 0);

    // Probabilities should sum to ~1
    float sum = 0.0f;
    for (int i = 0; i < 6; i++) {
        EXPECT_GE(probs[i], 0.0f);
        EXPECT_LE(probs[i], 1.0f);
        sum += probs[i];
    }
    EXPECT_NEAR(sum, 1.0f, 0.01f);
}

TEST_F(IMMPredictionTest, DominantModel) {
    float pos[3] = {0.0f, 0.0f, 0.0f};
    float vel[3] = {5.0f, 0.0f, 0.0f};

    // Constant velocity motion should favor CV or CA model
    for (int i = 0; i < 20; i++) {
        dragonfly_predictor_update(pred, pos, vel, 0.016f);
        pos[0] += vel[0] * 0.016f;
    }

    prediction_motion_model_t dom = dragonfly_predictor_get_dominant_model(pred);
    EXPECT_TRUE(dom == PRED_MODEL_CV || dom == PRED_MODEL_CA);
}

//=============================================================================
// Evasion Detection Tests
//=============================================================================

TEST_F(PredictionTest, NoEvasionInitially) {
    float pos[3] = {0.0f, 0.0f, 0.0f};
    dragonfly_predictor_update(pred, pos, nullptr, 0.016f);

    evasion_state_t evasion;
    EXPECT_EQ(dragonfly_predictor_get_evasion(pred, &evasion), 0);
    EXPECT_EQ(evasion.current_type, EVASION_NONE);
    EXPECT_FLOAT_EQ(evasion.maneuver_intensity, 0.0f);
}

TEST_F(PredictionTest, DetectJink) {
    float accel[3] = {20.0f, 10.0f, 0.0f};  // High acceleration
    evasion_type_t type = dragonfly_predictor_detect_evasion(pred, accel);
    EXPECT_EQ(type, EVASION_JINK);
}

TEST_F(PredictionTest, DetectNoEvasion) {
    float accel[3] = {0.5f, 0.5f, 0.0f};  // Low acceleration
    evasion_type_t type = dragonfly_predictor_detect_evasion(pred, accel);
    EXPECT_EQ(type, EVASION_NONE);
}

TEST_F(PredictionTest, EvasionIntensity) {
    float pos[3] = {0.0f, 0.0f, 0.0f};
    dragonfly_predictor_update(pred, pos, nullptr, 0.016f);

    EXPECT_FLOAT_EQ(dragonfly_predictor_get_evasion_intensity(pred), 0.0f);
}

//=============================================================================
// Forward/Inverse Model Tests
//=============================================================================

TEST_F(PredictionTest, ForwardModel) {
    float current[9] = {0, 0, 0, 10, 5, 0, 0, 0, 0};  // pos, vel, accel
    float action[3] = {1, 0, 0};  // Accelerate in x
    float predicted[9];

    EXPECT_EQ(dragonfly_forward_model(pred, current, action, 0.1f, predicted), 0);

    // Position should advance
    EXPECT_GT(predicted[0], current[0]);
    // Velocity should increase
    EXPECT_GT(predicted[3], current[3]);
}

TEST_F(PredictionTest, InverseModel) {
    float current[9] = {0, 0, 0, 10, 5, 0, 0, 0, 0};
    float desired[9] = {0, 0, 0, 15, 5, 0, 0, 0, 0};  // Want higher x velocity
    float action[3];

    EXPECT_EQ(dragonfly_inverse_model(pred, current, desired, 0.1f, action), 0);

    // Action should be positive in x to increase velocity
    EXPECT_GT(action[0], 0.0f);
}

TEST_F(PredictionTest, ForwardInverseConsistency) {
    float current[9] = {0, 0, 0, 10, 5, 2, 1, 0, 0};
    float desired[9] = {0, 0, 0, 12, 6, 2, 0, 0, 0};
    float action[3], predicted[9];

    // Compute action to reach desired
    EXPECT_EQ(dragonfly_inverse_model(pred, current, desired, 0.1f, action), 0);

    // Apply forward model with computed action
    EXPECT_EQ(dragonfly_forward_model(pred, current, action, 0.1f, predicted), 0);

    // Predicted velocity should match desired velocity
    EXPECT_NEAR(predicted[3], desired[3], 0.5f);
    EXPECT_NEAR(predicted[4], desired[4], 0.5f);
}

//=============================================================================
// Facilitation Tests
//=============================================================================

TEST_F(PredictionTest, FacilitationGainWithoutState) {
    float gain = dragonfly_predictor_get_facilitation_gain(pred, 0.0f);
    EXPECT_FLOAT_EQ(gain, 1.0f);  // No boost without prediction
}

TEST_F(PredictionTest, FacilitationGainWithState) {
    float pos[3] = {0.0f, 0.0f, 0.0f};
    float vel[3] = {10.0f, 0.0f, 0.0f};  // Moving in +x direction

    for (int i = 0; i < 5; i++) {
        dragonfly_predictor_update(pred, pos, vel, 0.016f);
        pos[0] += vel[0] * 0.016f;
    }

    // Get predicted direction (should be ~0 radians = +x)
    float pred_dir = dragonfly_predictor_get_predicted_direction(pred);
    EXPECT_NEAR(pred_dir, 0.0f, 0.1f);

    // Gain in predicted direction should be > 1
    float gain_on_target = dragonfly_predictor_get_facilitation_gain(pred, pred_dir);
    EXPECT_GT(gain_on_target, 1.0f);

    // Gain perpendicular should be ~1
    float gain_perp = dragonfly_predictor_get_facilitation_gain(pred, M_PI / 2.0f);
    EXPECT_LT(gain_perp, gain_on_target);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(PredictionTest, GetStats) {
    prediction_stats_t stats;
    EXPECT_EQ(dragonfly_predictor_get_stats(pred, &stats), 0);
    EXPECT_EQ(stats.predictions_made, 0u);
}

TEST_F(PredictionTest, StatsTrackUpdates) {
    float pos[3] = {0.0f, 0.0f, 0.0f};
    for (int i = 0; i < 10; i++) {
        dragonfly_predictor_update(pred, pos, nullptr, 0.016f);
    }

    prediction_stats_t stats;
    dragonfly_predictor_get_stats(pred, &stats);
    // First update initializes state, subsequent ones count as predictions
    EXPECT_GE(stats.predictions_made, 9u);
}

TEST_F(PredictionTest, ResetStats) {
    float pos[3] = {0.0f, 0.0f, 0.0f};
    for (int i = 0; i < 10; i++) {
        dragonfly_predictor_update(pred, pos, nullptr, 0.016f);
    }

    EXPECT_EQ(dragonfly_predictor_reset_stats(pred), 0);

    prediction_stats_t stats;
    dragonfly_predictor_get_stats(pred, &stats);
    EXPECT_EQ(stats.predictions_made, 0u);
}

//=============================================================================
// Utility Function Tests
//=============================================================================

TEST_F(PredictionTest, EvasionName) {
    EXPECT_STREQ(dragonfly_evasion_name(EVASION_NONE), "NONE");
    EXPECT_STREQ(dragonfly_evasion_name(EVASION_JINK), "JINK");
    EXPECT_STREQ(dragonfly_evasion_name(EVASION_BREAK), "BREAK");
    EXPECT_STREQ(dragonfly_evasion_name(EVASION_WEAVE), "WEAVE");
    EXPECT_STREQ(dragonfly_evasion_name(EVASION_SPIRAL), "SPIRAL");
    EXPECT_STREQ(dragonfly_evasion_name(EVASION_COMBINED), "COMBINED");
}

TEST_F(PredictionTest, ModelName) {
    EXPECT_STREQ(dragonfly_model_name(PRED_MODEL_CV), "CONSTANT_VELOCITY");
    EXPECT_STREQ(dragonfly_model_name(PRED_MODEL_CA), "CONSTANT_ACCEL");
    EXPECT_STREQ(dragonfly_model_name(PRED_MODEL_SINGER), "SINGER");
    EXPECT_STREQ(dragonfly_model_name(PRED_MODEL_JINK), "JINK");
    EXPECT_STREQ(dragonfly_model_name(PRED_MODEL_WEAVE), "WEAVE");
    EXPECT_STREQ(dragonfly_model_name(PRED_MODEL_SPIRAL), "SPIRAL");
}

TEST_F(PredictionTest, TrajectoryAllocFree) {
    predicted_state_t* traj = dragonfly_trajectory_alloc(20);
    ASSERT_NE(traj, nullptr);
    dragonfly_trajectory_free(traj);

    EXPECT_EQ(dragonfly_trajectory_alloc(0), nullptr);
    dragonfly_trajectory_free(nullptr);  // Should not crash
}

//=============================================================================
// Null Pointer Tests
//=============================================================================

TEST_F(PredictionTest, NullPointerHandling) {
    float pos[3] = {0, 0, 0};
    float vel[3] = {0, 0, 0};
    predicted_state_t state;
    evasion_state_t evasion;
    prediction_stats_t stats;
    prediction_config_t config;
    float probs[6];
    float action[3];

    EXPECT_EQ(dragonfly_predictor_update(nullptr, pos, vel, 0.016f), -1);
    EXPECT_EQ(dragonfly_predictor_update(pred, nullptr, vel, 0.016f), -1);
    EXPECT_EQ(dragonfly_predictor_reset(nullptr), -1);

    EXPECT_EQ(dragonfly_predictor_get_state_at(nullptr, 100.0f, &state), -1);
    EXPECT_EQ(dragonfly_predictor_get_state_at(pred, 100.0f, nullptr), -1);
    EXPECT_EQ(dragonfly_predictor_get_state_at(pred, -100.0f, &state), -1);

    EXPECT_EQ(dragonfly_predictor_get_evasion(nullptr, &evasion), -1);
    EXPECT_EQ(dragonfly_predictor_get_evasion(pred, nullptr), -1);

    EXPECT_EQ(dragonfly_predictor_detect_evasion(nullptr, pos), EVASION_NONE);
    EXPECT_EQ(dragonfly_predictor_detect_evasion(pred, nullptr), EVASION_NONE);

    EXPECT_FLOAT_EQ(dragonfly_predictor_get_evasion_intensity(nullptr), 0.0f);

    EXPECT_EQ(dragonfly_forward_model(pred, nullptr, action, 0.1f, pos), -1);
    EXPECT_EQ(dragonfly_forward_model(pred, pos, nullptr, 0.1f, pos), -1);
    EXPECT_EQ(dragonfly_forward_model(pred, pos, action, 0.1f, nullptr), -1);
    EXPECT_EQ(dragonfly_forward_model(pred, pos, action, -0.1f, pos), -1);

    EXPECT_EQ(dragonfly_inverse_model(pred, nullptr, pos, 0.1f, action), -1);
    EXPECT_EQ(dragonfly_inverse_model(pred, pos, nullptr, 0.1f, action), -1);
    EXPECT_EQ(dragonfly_inverse_model(pred, pos, pos, 0.1f, nullptr), -1);

    EXPECT_EQ(dragonfly_predictor_get_model_probabilities(nullptr, probs, 6), -1);
    EXPECT_EQ(dragonfly_predictor_get_model_probabilities(pred, nullptr, 6), -1);

    EXPECT_EQ(dragonfly_predictor_get_dominant_model(nullptr), PRED_MODEL_CV);

    EXPECT_FLOAT_EQ(dragonfly_predictor_get_facilitation_gain(nullptr, 0.0f), 1.0f);
    EXPECT_FLOAT_EQ(dragonfly_predictor_get_predicted_direction(nullptr), 0.0f);

    EXPECT_EQ(dragonfly_predictor_get_stats(nullptr, &stats), -1);
    EXPECT_EQ(dragonfly_predictor_get_stats(pred, nullptr), -1);
    EXPECT_EQ(dragonfly_predictor_reset_stats(nullptr), -1);

    EXPECT_EQ(dragonfly_predictor_set_config(nullptr, &config), -1);
    EXPECT_EQ(dragonfly_predictor_set_config(pred, nullptr), -1);
    EXPECT_EQ(dragonfly_predictor_get_config(nullptr, &config), -1);
    EXPECT_EQ(dragonfly_predictor_get_config(pred, nullptr), -1);
}
