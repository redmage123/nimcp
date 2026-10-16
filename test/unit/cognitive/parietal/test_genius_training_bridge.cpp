/**
 * @file test_genius_training_bridge.cpp
 * @brief Unit tests for Mathematical Genius-Training Bridge
 * @date 2026-01-24
 */

#include <gtest/gtest.h>

extern "C" {
#include "cognitive/parietal/nimcp_genius_training_bridge.h"
}

class GeniusTrainingBridgeTest : public ::testing::Test {
protected:
    genius_training_config_t config;
    genius_training_bridge_t* bridge = nullptr;

    void SetUp() override {
        config = genius_training_config_default();
    }

    void TearDown() override {
        if (bridge) {
            genius_training_destroy(bridge);
            bridge = nullptr;
        }
    }
};

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

TEST_F(GeniusTrainingBridgeTest, DefaultConfigSetsReasonableDefaults) {
    genius_training_config_t cfg = genius_training_config_default();

    EXPECT_GT(cfg.base_learning_rate, 0.0f);
    EXPECT_LT(cfg.base_learning_rate, 1.0f);
    EXPECT_GT(cfg.learning_rate_decay, 0.0f);
    EXPECT_LE(cfg.learning_rate_decay, 1.0f);
    EXPECT_GT(cfg.batch_size, 0u);
    EXPECT_TRUE(cfg.enable_curriculum);
    EXPECT_TRUE(cfg.enable_continual_learning);
    EXPECT_TRUE(cfg.train_gauss_mode);
    EXPECT_TRUE(cfg.train_newton_mode);
    EXPECT_TRUE(cfg.train_erdos_mode);
}

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(GeniusTrainingBridgeTest, CreateWithNullConfigUsesDefaults) {
    bridge = genius_training_create(nullptr);
    ASSERT_NE(bridge, nullptr);
}

TEST_F(GeniusTrainingBridgeTest, CreateWithValidConfigSucceeds) {
    bridge = genius_training_create(&config);
    ASSERT_NE(bridge, nullptr);
}

TEST_F(GeniusTrainingBridgeTest, DestroyNullIsSafe) {
    genius_training_destroy(nullptr);
    // Should not crash
}

TEST_F(GeniusTrainingBridgeTest, ResetSucceeds) {
    bridge = genius_training_create(&config);
    ASSERT_NE(bridge, nullptr);

    int result = genius_training_reset(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(GeniusTrainingBridgeTest, ResetNullFails) {
    int result = genius_training_reset(nullptr);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * Task Management Tests
 * ============================================================================ */

TEST_F(GeniusTrainingBridgeTest, RegisterTaskSucceeds) {
    bridge = genius_training_create(&config);
    ASSERT_NE(bridge, nullptr);

    genius_training_task_t task = {
        .task_id = 0,
        .type = GENIUS_TASK_THEOREM_PROVING,
        .domain = GENIUS_TRAIN_DOMAIN_NUMBER_THEORY,
        .stage = GENIUS_STAGE_NOVICE,
        .difficulty = 0.3f,
        .weight = 1.0f,
        .num_examples = 100,
        .completion_rate = 0.0f,
        .avg_score = 0.0f
    };

    int task_id = genius_training_register_task(bridge, &task);
    EXPECT_GE(task_id, 0);
}

TEST_F(GeniusTrainingBridgeTest, RegisterTaskNullBridgeFails) {
    genius_training_task_t task = {};
    int task_id = genius_training_register_task(nullptr, &task);
    EXPECT_EQ(task_id, -1);
}

TEST_F(GeniusTrainingBridgeTest, RegisterTaskNullTaskFails) {
    bridge = genius_training_create(&config);
    ASSERT_NE(bridge, nullptr);

    int task_id = genius_training_register_task(bridge, nullptr);
    EXPECT_EQ(task_id, -1);
}

TEST_F(GeniusTrainingBridgeTest, UnregisterTaskSucceeds) {
    bridge = genius_training_create(&config);
    ASSERT_NE(bridge, nullptr);

    genius_training_task_t task = {
        .type = GENIUS_TASK_PATTERN_RECOGNITION,
        .domain = GENIUS_TRAIN_DOMAIN_COMBINATORICS
    };
    int task_id = genius_training_register_task(bridge, &task);

    int result = genius_training_unregister_task(bridge, (uint32_t)task_id);
    EXPECT_EQ(result, 0);
}

TEST_F(GeniusTrainingBridgeTest, GetTaskSucceeds) {
    bridge = genius_training_create(&config);
    ASSERT_NE(bridge, nullptr);

    genius_training_task_t task = {
        .type = GENIUS_TASK_CONJECTURE_GENERATION,
        .domain = GENIUS_TRAIN_DOMAIN_ALGEBRA,
        .difficulty = 0.5f
    };
    int task_id = genius_training_register_task(bridge, &task);

    genius_training_task_t retrieved;
    int result = genius_training_get_task(bridge, (uint32_t)task_id, &retrieved);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(retrieved.type, GENIUS_TASK_CONJECTURE_GENERATION);
    EXPECT_EQ(retrieved.domain, GENIUS_TRAIN_DOMAIN_ALGEBRA);
}

/* ============================================================================
 * Training Control Tests
 * ============================================================================ */

TEST_F(GeniusTrainingBridgeTest, StartTrainingSucceeds) {
    bridge = genius_training_create(&config);
    ASSERT_NE(bridge, nullptr);

    int result = genius_training_start(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(GeniusTrainingBridgeTest, PauseTrainingSucceeds) {
    bridge = genius_training_create(&config);
    ASSERT_NE(bridge, nullptr);

    genius_training_start(bridge);
    int result = genius_training_pause(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(GeniusTrainingBridgeTest, ResumeTrainingSucceeds) {
    bridge = genius_training_create(&config);
    ASSERT_NE(bridge, nullptr);

    genius_training_start(bridge);
    genius_training_pause(bridge);
    int result = genius_training_resume(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(GeniusTrainingBridgeTest, StopTrainingSucceeds) {
    bridge = genius_training_create(&config);
    ASSERT_NE(bridge, nullptr);

    genius_training_start(bridge);
    int result = genius_training_stop(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(GeniusTrainingBridgeTest, TrainBatchReturnsLoss) {
    bridge = genius_training_create(&config);
    ASSERT_NE(bridge, nullptr);

    float dummy_inputs[32] = {0};
    float dummy_targets[32] = {0};

    float loss = genius_training_train_batch(bridge, dummy_inputs, dummy_targets, 32);
    EXPECT_GE(loss, 0.0f);
}

TEST_F(GeniusTrainingBridgeTest, TrainBatchNullInputsFails) {
    bridge = genius_training_create(&config);
    ASSERT_NE(bridge, nullptr);

    float loss = genius_training_train_batch(bridge, nullptr, nullptr, 32);
    EXPECT_LT(loss, 0.0f);
}

TEST_F(GeniusTrainingBridgeTest, TrainEpochReturnsLoss) {
    bridge = genius_training_create(&config);
    ASSERT_NE(bridge, nullptr);

    float loss = genius_training_train_epoch(bridge);
    EXPECT_GE(loss, 0.0f);
}

TEST_F(GeniusTrainingBridgeTest, ValidateReturnsAccuracy) {
    bridge = genius_training_create(&config);
    ASSERT_NE(bridge, nullptr);

    float accuracy = genius_training_validate(bridge);
    EXPECT_GE(accuracy, 0.0f);
    EXPECT_LE(accuracy, 1.0f);
}

TEST_F(GeniusTrainingBridgeTest, ConsolidateSucceeds) {
    bridge = genius_training_create(&config);
    ASSERT_NE(bridge, nullptr);

    int result = genius_training_consolidate(bridge);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * Curriculum Tests
 * ============================================================================ */

TEST_F(GeniusTrainingBridgeTest, GetCurriculumStateSucceeds) {
    bridge = genius_training_create(&config);
    ASSERT_NE(bridge, nullptr);

    genius_curriculum_state_t state;
    int result = genius_training_get_curriculum_state(bridge, &state);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(state.current_stage, GENIUS_STAGE_NOVICE);
}

TEST_F(GeniusTrainingBridgeTest, AdvanceCurriculumSucceeds) {
    bridge = genius_training_create(&config);
    ASSERT_NE(bridge, nullptr);

    int new_stage = genius_training_advance_curriculum(bridge);
    EXPECT_EQ(new_stage, GENIUS_STAGE_INTERMEDIATE);

    genius_curriculum_state_t state;
    genius_training_get_curriculum_state(bridge, &state);
    EXPECT_EQ(state.current_stage, GENIUS_STAGE_INTERMEDIATE);
}

TEST_F(GeniusTrainingBridgeTest, SetCurriculumStageSucceeds) {
    bridge = genius_training_create(&config);
    ASSERT_NE(bridge, nullptr);

    int result = genius_training_set_curriculum_stage(bridge, GENIUS_STAGE_ADVANCED);
    EXPECT_EQ(result, 0);

    genius_curriculum_state_t state;
    genius_training_get_curriculum_state(bridge, &state);
    EXPECT_EQ(state.current_stage, GENIUS_STAGE_ADVANCED);
}

TEST_F(GeniusTrainingBridgeTest, SetDomainSucceeds) {
    bridge = genius_training_create(&config);
    ASSERT_NE(bridge, nullptr);

    int result = genius_training_set_domain(bridge, GENIUS_TRAIN_DOMAIN_CALCULUS);
    EXPECT_EQ(result, 0);

    genius_curriculum_state_t state;
    genius_training_get_curriculum_state(bridge, &state);
    EXPECT_EQ(state.current_domain, GENIUS_TRAIN_DOMAIN_CALCULUS);
}

TEST_F(GeniusTrainingBridgeTest, SetInvalidDomainFails) {
    bridge = genius_training_create(&config);
    ASSERT_NE(bridge, nullptr);

    int result = genius_training_set_domain(bridge, (genius_train_domain_t)99);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * Learning Rate Tests
 * ============================================================================ */

TEST_F(GeniusTrainingBridgeTest, GetLearningRateSucceeds) {
    bridge = genius_training_create(&config);
    ASSERT_NE(bridge, nullptr);

    float lr = genius_training_get_learning_rate(bridge);
    EXPECT_GT(lr, 0.0f);
    EXPECT_EQ(lr, config.base_learning_rate);
}

TEST_F(GeniusTrainingBridgeTest, SetLearningRateSucceeds) {
    bridge = genius_training_create(&config);
    ASSERT_NE(bridge, nullptr);

    int result = genius_training_set_learning_rate(bridge, 0.005f);
    EXPECT_EQ(result, 0);

    float lr = genius_training_get_learning_rate(bridge);
    EXPECT_FLOAT_EQ(lr, 0.005f);
}

TEST_F(GeniusTrainingBridgeTest, SetInvalidLearningRateFails) {
    bridge = genius_training_create(&config);
    ASSERT_NE(bridge, nullptr);

    int result = genius_training_set_learning_rate(bridge, -0.001f);
    EXPECT_EQ(result, -1);
}

TEST_F(GeniusTrainingBridgeTest, LRStepDecaysRate) {
    bridge = genius_training_create(&config);
    ASSERT_NE(bridge, nullptr);

    float initial_lr = genius_training_get_learning_rate(bridge);
    float new_lr = genius_training_lr_step(bridge);

    EXPECT_LT(new_lr, initial_lr);
    EXPECT_FLOAT_EQ(new_lr, initial_lr * config.learning_rate_decay);
}

/* ============================================================================
 * State Query Tests
 * ============================================================================ */

TEST_F(GeniusTrainingBridgeTest, GetProgressSucceeds) {
    bridge = genius_training_create(&config);
    ASSERT_NE(bridge, nullptr);

    genius_training_progress_t progress;
    int result = genius_training_get_progress(bridge, &progress);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(progress.total_epochs, 0u);
}

TEST_F(GeniusTrainingBridgeTest, GetModeStateSucceeds) {
    bridge = genius_training_create(&config);
    ASSERT_NE(bridge, nullptr);

    genius_mode_training_state_t state;
    int result = genius_training_get_mode_state(bridge, &state);
    EXPECT_EQ(result, 0);
    EXPECT_GE(state.gauss_skill, 0.0f);
    EXPECT_LE(state.gauss_skill, 1.0f);
}

TEST_F(GeniusTrainingBridgeTest, GetStateSucceeds) {
    bridge = genius_training_create(&config);
    ASSERT_NE(bridge, nullptr);

    genius_training_bridge_state_t state;
    int result = genius_training_get_state(bridge, &state);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(state.state, GENIUS_TRAINING_STATE_IDLE);
}

TEST_F(GeniusTrainingBridgeTest, GetStatsSucceeds) {
    bridge = genius_training_create(&config);
    ASSERT_NE(bridge, nullptr);

    genius_training_stats_t stats;
    int result = genius_training_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
}

TEST_F(GeniusTrainingBridgeTest, ResetStatsSucceeds) {
    bridge = genius_training_create(&config);
    ASSERT_NE(bridge, nullptr);

    // Train a bit first
    genius_training_train_epoch(bridge);

    int result = genius_training_reset_stats(bridge);
    EXPECT_EQ(result, 0);

    genius_training_stats_t stats;
    genius_training_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_examples, 0u);
}

/* ============================================================================
 * Callback Tests
 * ============================================================================ */

static bool epoch_callback_called = false;
static void test_epoch_callback(genius_training_bridge_t*, uint64_t, float, float, void*) {
    epoch_callback_called = true;
}

TEST_F(GeniusTrainingBridgeTest, RegisterEpochCallbackSucceeds) {
    bridge = genius_training_create(&config);
    ASSERT_NE(bridge, nullptr);

    int result = genius_training_register_epoch_callback(bridge, test_epoch_callback, nullptr);
    EXPECT_EQ(result, 0);
}

TEST_F(GeniusTrainingBridgeTest, EpochCallbackInvoked) {
    bridge = genius_training_create(&config);
    ASSERT_NE(bridge, nullptr);

    epoch_callback_called = false;
    genius_training_register_epoch_callback(bridge, test_epoch_callback, nullptr);
    genius_training_train_epoch(bridge);

    EXPECT_TRUE(epoch_callback_called);
}

static bool curriculum_callback_called = false;
static void test_curriculum_callback(genius_training_bridge_t*, genius_curriculum_stage_t,
                                     genius_curriculum_stage_t, genius_train_domain_t, void*) {
    curriculum_callback_called = true;
}

TEST_F(GeniusTrainingBridgeTest, RegisterCurriculumCallbackSucceeds) {
    bridge = genius_training_create(&config);
    ASSERT_NE(bridge, nullptr);

    int result = genius_training_register_curriculum_callback(bridge, test_curriculum_callback, nullptr);
    EXPECT_EQ(result, 0);
}

TEST_F(GeniusTrainingBridgeTest, CurriculumCallbackInvokedOnAdvance) {
    bridge = genius_training_create(&config);
    ASSERT_NE(bridge, nullptr);

    curriculum_callback_called = false;
    genius_training_register_curriculum_callback(bridge, test_curriculum_callback, nullptr);
    genius_training_advance_curriculum(bridge);

    EXPECT_TRUE(curriculum_callback_called);
}

/* ============================================================================
 * Bio-Async Tests
 * ============================================================================ */

TEST_F(GeniusTrainingBridgeTest, BioAsyncInitiallyDisconnected) {
    bridge = genius_training_create(&config);
    ASSERT_NE(bridge, nullptr);

    EXPECT_FALSE(genius_training_is_bio_async_connected(bridge));
}

TEST_F(GeniusTrainingBridgeTest, BioAsyncConnectSucceeds) {
    bridge = genius_training_create(&config);
    ASSERT_NE(bridge, nullptr);

    int result = genius_training_bio_async_connect(bridge);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(genius_training_is_bio_async_connected(bridge));
}

TEST_F(GeniusTrainingBridgeTest, BioAsyncDisconnectSucceeds) {
    bridge = genius_training_create(&config);
    ASSERT_NE(bridge, nullptr);

    genius_training_bio_async_connect(bridge);
    int result = genius_training_bio_async_disconnect(bridge);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(genius_training_is_bio_async_connected(bridge));
}

/* ============================================================================
 * Integration Tests
 * ============================================================================ */

TEST_F(GeniusTrainingBridgeTest, FullTrainingWorkflowSucceeds) {
    bridge = genius_training_create(&config);
    ASSERT_NE(bridge, nullptr);

    // 1. Register training tasks
    genius_training_task_t task1 = {
        .type = GENIUS_TASK_THEOREM_PROVING,
        .domain = GENIUS_TRAIN_DOMAIN_NUMBER_THEORY,
        .stage = GENIUS_STAGE_NOVICE,
        .difficulty = 0.3f
    };
    genius_training_register_task(bridge, &task1);

    genius_training_task_t task2 = {
        .type = GENIUS_TASK_PATTERN_RECOGNITION,
        .domain = GENIUS_TRAIN_DOMAIN_CALCULUS,
        .stage = GENIUS_STAGE_NOVICE,
        .difficulty = 0.2f
    };
    genius_training_register_task(bridge, &task2);

    // 2. Start training
    genius_training_start(bridge);

    // 3. Train a few epochs
    for (int i = 0; i < 3; i++) {
        float loss = genius_training_train_epoch(bridge);
        EXPECT_GE(loss, 0.0f);
    }

    // 4. Validate
    float accuracy = genius_training_validate(bridge);
    EXPECT_GE(accuracy, 0.0f);

    // 5. Check progress
    genius_training_progress_t progress;
    genius_training_get_progress(bridge, &progress);
    EXPECT_EQ(progress.total_epochs, 3u);
    EXPECT_GT(progress.total_batches, 0u);

    // 6. Check mode state improved
    genius_mode_training_state_t mode_state;
    genius_training_get_mode_state(bridge, &mode_state);
    EXPECT_GE(mode_state.gauss_skill, 0.3f);

    // 7. Apply learning rate decay
    float initial_lr = genius_training_get_learning_rate(bridge);
    genius_training_lr_step(bridge);
    float new_lr = genius_training_get_learning_rate(bridge);
    EXPECT_LT(new_lr, initial_lr);

    // 8. Consolidate learning
    genius_training_consolidate(bridge);

    // 9. Stop training
    genius_training_stop(bridge);

    // 10. Check final stats
    genius_training_stats_t stats;
    genius_training_get_stats(bridge, &stats);
    EXPECT_GT(stats.total_examples, 0u);
}

TEST_F(GeniusTrainingBridgeTest, CurriculumProgressionWorks) {
    bridge = genius_training_create(&config);
    ASSERT_NE(bridge, nullptr);

    // Start at novice
    genius_curriculum_state_t state;
    genius_training_get_curriculum_state(bridge, &state);
    EXPECT_EQ(state.current_stage, GENIUS_STAGE_NOVICE);

    // Advance through stages
    genius_training_advance_curriculum(bridge);
    genius_training_get_curriculum_state(bridge, &state);
    EXPECT_EQ(state.current_stage, GENIUS_STAGE_INTERMEDIATE);
    EXPECT_EQ(state.advancements, 1u);

    genius_training_advance_curriculum(bridge);
    genius_training_get_curriculum_state(bridge, &state);
    EXPECT_EQ(state.current_stage, GENIUS_STAGE_ADVANCED);
    EXPECT_EQ(state.stages_completed, 2u);

    // Change domain
    genius_training_set_domain(bridge, GENIUS_TRAIN_DOMAIN_GRAPH_THEORY);
    genius_training_get_curriculum_state(bridge, &state);
    EXPECT_EQ(state.current_domain, GENIUS_TRAIN_DOMAIN_GRAPH_THEORY);
}
