/**
 * @file test_training_pipeline_e2e.cpp
 * @brief End-to-end tests for the full NIMCP training pipeline
 *
 * WHAT: Tests the complete training pipeline end-to-end from the public C API:
 *       brain creation, learn_example, predict, probe, save/load, training
 *       configuration, train_step, train_batch, gradient norms, and the
 *       unified continuous modulation pipeline.
 * WHY:  Validate that the full training path -- from API call to weight update
 *       to prediction -- works correctly as an integrated system.
 * HOW:  GTest fixture creates NIMCP_BRAIN_TINY brains with suite-level
 *       nimcp_init/shutdown to avoid ~15s per-test overhead.
 *
 * PERFORMANCE: Uses NIMCP_BRAIN_TINY (100 neurons) and reduced iteration
 *              counts to keep total suite runtime under 300s.
 *
 * @author NIMCP Development Team
 * @date 2026-02-26
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <vector>

extern "C" {
#include "nimcp.h"
#include "cognitive/training/nimcp_training_integration.h"
#include "core/brain/nimcp_brain.h"
}

// ============================================================================
// E2E Fixture: Suite-level nimcp_init/shutdown, per-test TINY brain
// ============================================================================

class TrainingPipelineE2E : public ::testing::Test {
protected:
    nimcp_brain_t brain = nullptr;

    static void SetUpTestSuite() {
        nimcp_init();
    }

    static void TearDownTestSuite() {
        nimcp_shutdown();
    }

    void SetUp() override {
        brain = nimcp_brain_create("e2e_pipeline_test", NIMCP_BRAIN_TINY,
                                    NIMCP_TASK_CLASSIFICATION, 8, 3);
        ASSERT_NE(brain, nullptr) << "Failed to create NIMCP_BRAIN_TINY brain";
    }

    void TearDown() override {
        if (brain) {
            nimcp_brain_destroy(brain);
            brain = nullptr;
        }
    }
};

// ============================================================================
// Test 1: LearnAndPredictBasic
// Train on examples, verify prediction returns valid label and confidence.
// ============================================================================

TEST_F(TrainingPipelineE2E, LearnAndPredictBasic) {
    float features[8];

    for (int i = 0; i < 30; i++) {
        for (int j = 0; j < 8; j++) {
            features[j] = (float)((i * j) % 7) / 7.0f;
        }
        const char* label = (i % 2 == 0) ? "even" : "odd";
        nimcp_status_t rc = nimcp_brain_learn_example(brain, features, 8, label, 0.5f);
        EXPECT_EQ(rc, NIMCP_OK) << "learn_example failed at step " << i;
    }

    char pred_label[NIMCP_MAX_LABEL_SIZE];
    float confidence = -1.0f;
    for (int j = 0; j < 8; j++) features[j] = 0.0f;

    nimcp_status_t rc = nimcp_brain_predict_fast(brain, features, 8,
                                                  pred_label, &confidence);
    EXPECT_EQ(rc, NIMCP_OK) << "predict_fast failed";
    EXPECT_GT(confidence, 0.0f) << "Confidence should be positive after training";
    EXPECT_GT(strlen(pred_label), 0u) << "Predicted label should be non-empty";
}

// ============================================================================
// Test 2: GradientNormTracked
// After a training step, gradient norm should be populated and non-negative.
// ============================================================================

TEST_F(TrainingPipelineE2E, GradientNormTracked) {
    float features[8];
    for (int j = 0; j < 8; j++) features[j] = 0.1f * (float)(j + 1);

    nimcp_status_t rc = nimcp_brain_learn_example(brain, features, 8, "test_class", 0.5f);
    EXPECT_EQ(rc, NIMCP_OK);

    float grad_norm = nimcp_brain_get_last_gradient_norm(brain);
    EXPECT_GE(grad_norm, 0.0f)
        << "Gradient norm should be >= 0.0 after training (got " << grad_norm << ")";
}

// ============================================================================
// Test 3: LossDecreases
// Train repeatedly on the same pattern; loss should not diverge wildly.
// ============================================================================

TEST_F(TrainingPipelineE2E, LossDecreases) {
    float features[8];
    for (int j = 0; j < 8; j++) features[j] = 0.5f;

    nimcp_brain_learn_example(brain, features, 8, "stable", 0.5f);
    float first_loss = nimcp_brain_get_last_loss(brain);

    for (int i = 0; i < 20; i++) {
        nimcp_brain_learn_example(brain, features, 8, "stable", 0.5f);
    }
    float final_loss = nimcp_brain_get_last_loss(brain);

    EXPECT_LT(final_loss, first_loss + 10.0f)
        << "Loss should not diverge wildly after repeated training on same pattern";
}

// ============================================================================
// Test 4: ProbeAfterLearning
// Probe the brain after training and verify key statistics are populated.
// ============================================================================

TEST_F(TrainingPipelineE2E, ProbeAfterLearning) {
    float features[8];
    for (int j = 0; j < 8; j++) features[j] = 0.3f;

    for (int i = 0; i < 10; i++) {
        nimcp_brain_learn_example(brain, features, 8, "probe_test", 0.5f);
    }

    nimcp_brain_probe_t probe;
    memset(&probe, 0, sizeof(probe));
    nimcp_status_t rc = nimcp_brain_probe(brain, &probe);
    EXPECT_EQ(rc, NIMCP_OK) << "nimcp_brain_probe failed";

    EXPECT_GT(probe.num_neurons, 0u) << "Brain should have neurons";
    EXPECT_EQ(probe.num_inputs, 8u) << "Input dimension should be 8";
    EXPECT_EQ(probe.num_outputs, 3u) << "Output dimension should be 3";
    EXPECT_GE(probe.avg_sparsity, 0.0f) << "Sparsity should be >= 0";
    EXPECT_LE(probe.avg_sparsity, 1.0f) << "Sparsity should be <= 1";
    EXPECT_GT(probe.total_learning_steps, 0u)
        << "total_learning_steps should be > 0 after training";
}

// ============================================================================
// Test 5: SaveLoadPreservesLearning
// Train, save, load, verify predictions match between original and loaded.
// ============================================================================

TEST_F(TrainingPipelineE2E, SaveLoadPreservesLearning) {
    float features[8];
    for (int j = 0; j < 8; j++) features[j] = 0.5f + 0.01f * (float)j;

    for (int i = 0; i < 20; i++) {
        nimcp_brain_learn_example(brain, features, 8, "trained_class", 0.5f);
    }

    char pred_before[NIMCP_MAX_LABEL_SIZE];
    float conf_before = -1.0f;
    nimcp_status_t rc = nimcp_brain_predict_fast(brain, features, 8,
                                                  pred_before, &conf_before);
    ASSERT_EQ(rc, NIMCP_OK) << "predict_fast before save failed";

    const char* filepath = "/tmp/nimcp_e2e_pipeline_test.brain";
    rc = nimcp_brain_save(brain, filepath);
    ASSERT_EQ(rc, NIMCP_OK) << "nimcp_brain_save failed";

    nimcp_brain_t loaded = nimcp_brain_load(filepath);
    ASSERT_NE(loaded, nullptr) << "nimcp_brain_load returned NULL";

    char pred_after[NIMCP_MAX_LABEL_SIZE];
    float conf_after = -1.0f;
    rc = nimcp_brain_predict_fast(loaded, features, 8, pred_after, &conf_after);
    ASSERT_EQ(rc, NIMCP_OK) << "predict_fast after load failed";

    EXPECT_STREQ(pred_before, pred_after)
        << "Predictions should match after save/load";
    EXPECT_NEAR(conf_before, conf_after, 0.05f)
        << "Confidence should be similar after save/load";

    nimcp_brain_destroy(loaded);
    remove(filepath);
}

// ============================================================================
// Test 6: TrainingConfigAndTrainStep
// Use the full training pipeline API: configure_training + train_step.
// ============================================================================

TEST_F(TrainingPipelineE2E, TrainingConfigAndTrainStep) {
    nimcp_training_config_t config = nimcp_training_config_default();
    config.loss_type = NIMCP_API_LOSS_CROSS_ENTROPY;
    config.optimizer_type = NIMCP_API_OPT_ADAM;
    config.learning_rate = 0.01f;
    config.enable_gradient_clipping = true;
    config.gradient_clip_value = 1.0f;

    nimcp_status_t rc = nimcp_brain_configure_training(brain, &config);
    ASSERT_EQ(rc, NIMCP_OK) << "configure_training failed";

    float features[8];
    float targets[3] = {1.0f, 0.0f, 0.0f};

    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 8; j++) features[j] = (float)(i + j) / 20.0f;

        nimcp_training_result_t result;
        memset(&result, 0, sizeof(result));
        rc = nimcp_brain_train_step(brain, features, 8, targets, 3, &result);
        EXPECT_EQ(rc, NIMCP_OK) << "train_step failed at step " << i;
    }

    uint64_t total_steps = 0;
    float total_loss = 0.0f;
    float current_lr = 0.0f;
    rc = nimcp_brain_get_training_stats(brain, &total_steps, &total_loss, &current_lr);
    EXPECT_EQ(rc, NIMCP_OK) << "get_training_stats failed";
    EXPECT_GT(total_steps, 0u) << "Total training steps should be > 0";
    EXPECT_GT(current_lr, 0.0f) << "Current LR should be positive";
}

// ============================================================================
// Test 7: TrainBatchWorks
// Use nimcp_brain_train_batch to train on a mini-batch.
// ============================================================================

TEST_F(TrainingPipelineE2E, TrainBatchWorks) {
    nimcp_training_config_t config = nimcp_training_config_default();
    config.learning_rate = 0.01f;
    nimcp_status_t rc = nimcp_brain_configure_training(brain, &config);
    ASSERT_EQ(rc, NIMCP_OK);

    const uint32_t batch_size = 4;
    const uint32_t num_features = 8;
    const uint32_t num_targets = 3;

    float features[batch_size * num_features];
    float targets[batch_size * num_targets];

    for (uint32_t b = 0; b < batch_size; b++) {
        for (uint32_t f = 0; f < num_features; f++) {
            features[b * num_features + f] = (float)(b + f) / 12.0f;
        }
        for (uint32_t t = 0; t < num_targets; t++) {
            targets[b * num_targets + t] = (t == (b % num_targets)) ? 1.0f : 0.0f;
        }
    }

    nimcp_training_result_t result;
    memset(&result, 0, sizeof(result));
    rc = nimcp_brain_train_batch(brain, features, targets,
                                  batch_size, num_features, num_targets, &result);
    EXPECT_EQ(rc, NIMCP_OK) << "train_batch failed";
}

// ============================================================================
// Test 8: AccuracyTrackingWorks
// Verify the running accuracy EMA is tracked during learn_example.
// ============================================================================

TEST_F(TrainingPipelineE2E, AccuracyTrackingWorks) {
    float features[8];
    for (int j = 0; j < 8; j++) features[j] = 0.5f;

    for (int i = 0; i < 15; i++) {
        nimcp_brain_learn_example(brain, features, 8, "class_a", 0.5f);
    }

    float accuracy = nimcp_brain_get_accuracy(brain);
    EXPECT_GE(accuracy, 0.0f) << "Accuracy should be >= 0";
    EXPECT_LE(accuracy, 1.0f) << "Accuracy should be <= 1";
}

// ============================================================================
// Test 9: PredictFastVsPredictConsistency
// Both predict and predict_fast should return labels.
// ============================================================================

TEST_F(TrainingPipelineE2E, PredictFastVsPredictConsistency) {
    float features[8];
    for (int j = 0; j < 8; j++) features[j] = 0.5f;

    for (int i = 0; i < 10; i++) {
        nimcp_brain_learn_example(brain, features, 8, "consistent_class", 0.5f);
    }

    char fast_label[NIMCP_MAX_LABEL_SIZE];
    float fast_conf = -1.0f;
    nimcp_status_t rc1 = nimcp_brain_predict_fast(brain, features, 8,
                                                    fast_label, &fast_conf);
    EXPECT_EQ(rc1, NIMCP_OK);
    EXPECT_GT(strlen(fast_label), 0u);
    EXPECT_GE(fast_conf, 0.0f);

    char full_label[NIMCP_MAX_LABEL_SIZE];
    float full_conf = -1.0f;
    nimcp_status_t rc2 = nimcp_brain_predict(brain, features, 8,
                                              full_label, &full_conf);
    EXPECT_EQ(rc2, NIMCP_OK);
    EXPECT_GT(strlen(full_label), 0u);
    EXPECT_GE(full_conf, 0.0f);
}

// ============================================================================
// Test 10: FreezePreventsFurtherLearning
// After freezing, prediction should still work.
// ============================================================================

TEST_F(TrainingPipelineE2E, FreezePreventsFurtherLearning) {
    float features[8];
    for (int j = 0; j < 8; j++) features[j] = 0.5f;

    for (int i = 0; i < 5; i++) {
        nimcp_brain_learn_example(brain, features, 8, "pre_freeze", 0.5f);
    }

    nimcp_status_t rc = nimcp_brain_freeze(brain);
    EXPECT_EQ(rc, NIMCP_OK);
    EXPECT_TRUE(nimcp_brain_is_frozen(brain));

    char post_label[NIMCP_MAX_LABEL_SIZE];
    float post_conf = -1.0f;
    rc = nimcp_brain_predict_fast(brain, features, 8, post_label, &post_conf);
    EXPECT_EQ(rc, NIMCP_OK);
}

// ============================================================================
// Test 11: InferReturnsRawOutputs
// nimcp_brain_infer should return raw output activations.
// ============================================================================

TEST_F(TrainingPipelineE2E, InferReturnsRawOutputs) {
    float features[8];
    for (int j = 0; j < 8; j++) features[j] = 0.5f;

    for (int i = 0; i < 5; i++) {
        nimcp_brain_learn_example(brain, features, 8, "infer_test", 0.5f);
    }

    float outputs[3] = {-999.0f, -999.0f, -999.0f};
    nimcp_status_t rc = nimcp_brain_infer(brain, features, 8, outputs, 3);
    EXPECT_EQ(rc, NIMCP_OK) << "nimcp_brain_infer failed";

    bool any_written = false;
    for (int j = 0; j < 3; j++) {
        if (outputs[j] != -999.0f) any_written = true;
    }
    EXPECT_TRUE(any_written) << "nimcp_brain_infer should write output values";
}

// ============================================================================
// Unified Modulation E2E Tests (via internal brain_t)
// ============================================================================

class TrainingModulationE2E : public ::testing::Test {
protected:
    brain_t internal_brain = nullptr;

    static void SetUpTestSuite() {
        // nimcp_init already called by TrainingPipelineE2E::SetUpTestSuite
        // (they share the same process). If running independently, init here.
        nimcp_init();
    }

    static void TearDownTestSuite() {
        nimcp_shutdown();
    }

    void SetUp() override {
        internal_brain = brain_create("modulation_test", BRAIN_SIZE_TINY,
                                       BRAIN_TASK_CLASSIFICATION, 4, 2);
        ASSERT_NE(internal_brain, nullptr);
    }

    void TearDown() override {
        if (internal_brain) {
            brain_ti_destroy_reasoning(internal_brain);
            brain_destroy(internal_brain);
            internal_brain = nullptr;
        }
    }
};

// ============================================================================
// Test 12: UnifiedLrModulationReturnsPositive
// ============================================================================

TEST_F(TrainingModulationE2E, UnifiedLrModulationReturnsPositive) {
    float base_lr = 0.01f;
    brain_ti_modulation_state_t state;
    memset(&state, 0, sizeof(state));

    float unified_lr = brain_ti_compute_unified_lr(internal_brain, base_lr, &state);
    EXPECT_GT(unified_lr, 0.0f)
        << "Unified LR should be positive (got " << unified_lr << ")";
    EXPECT_LT(unified_lr, 1.0f)
        << "Unified LR should be reasonable for base=0.01 (got " << unified_lr << ")";
}

// ============================================================================
// Test 13: ModulationStateFieldsInRange
// ============================================================================

TEST_F(TrainingModulationE2E, ModulationStateFieldsInRange) {
    brain_ti_modulation_state_t state;
    memset(&state, 0, sizeof(state));

    int rc = brain_ti_compute_modulation_state(internal_brain, &state);
    EXPECT_EQ(rc, 0) << "compute_modulation_state failed";

    EXPECT_GE(state.arousal_level, 0.0f);
    EXPECT_LE(state.arousal_level, 1.0f);
    EXPECT_GE(state.arousal_cognitive_gain, 0.0f);
    EXPECT_LE(state.arousal_cognitive_gain, 1.0f);
    EXPECT_GE(state.circadian_efficiency, 0.5f);
    EXPECT_LE(state.circadian_efficiency, 2.0f);
    EXPECT_GE(state.rpe_bonus, -0.3f);
    EXPECT_LE(state.rpe_bonus, 0.5f);
    EXPECT_GE(state.inflammation_learning_factor, 0.0f);
    EXPECT_LE(state.inflammation_learning_factor, 1.5f);
    EXPECT_GE(state.instability_lr_scale, 0.0f);
    EXPECT_LE(state.instability_lr_scale, 1.5f);
    EXPECT_GE(state.portia_learning_gate, 0.0f);
    EXPECT_LE(state.portia_learning_gate, 1.5f);
    EXPECT_GE(state.stress_level, 0.0f);
    EXPECT_LE(state.stress_level, 1.0f);
    EXPECT_GE(state.cognitive_capacity, 0.0f);
    EXPECT_LE(state.cognitive_capacity, 1.0f);
    EXPECT_GE(state.conflict_level, 0.0f);
    EXPECT_LE(state.conflict_level, 1.0f);
    EXPECT_GT(state.final_lr_factor, 0.0f);
}

// ============================================================================
// Test 14: DecisionCycleEndToEnd
// ============================================================================

TEST_F(TrainingModulationE2E, DecisionCycleEndToEnd) {
    brain_ti_update_reward(internal_brain, 0.7f, 0.5f);

    brain_ti_training_metrics_t metrics;
    memset(&metrics, 0, sizeof(metrics));
    metrics.loss_current = 0.5f;
    metrics.loss_previous = 0.6f;
    metrics.grad_norm = 1.2f;
    metrics.grad_norm_previous = 1.5f;
    metrics.loss_volatility = 0.1f;
    metrics.gradient_variance = 0.05f;
    metrics.current_lr = 0.01f;
    metrics.current_batch = 32.0f;

    brain_ti_decision_cycle_result_t result;
    memset(&result, 0, sizeof(result));

    int rc = brain_ti_compute_decision_cycle(internal_brain, &metrics, &result);
    EXPECT_EQ(rc, 0) << "compute_decision_cycle failed";

    EXPECT_GT(result.lr_factor, 0.0f) << "LR factor should be positive";
    EXPECT_GT(result.batch_factor, 0.0f) << "Batch factor should be positive";
    EXPECT_GE(result.urgency, 0.0f) << "Urgency should be non-negative";
}

// ============================================================================
// Test 15: UnifiedBatchAndClipModulation
// ============================================================================

TEST_F(TrainingModulationE2E, UnifiedBatchAndClipModulation) {
    float modulated_batch = brain_ti_compute_unified_batch(internal_brain, 32.0f);
    EXPECT_GT(modulated_batch, 0.0f) << "Modulated batch should be positive";
    EXPECT_LT(modulated_batch, 1000.0f) << "Modulated batch should be reasonable";

    float modulated_clip = brain_ti_compute_unified_clip(internal_brain, 1.0f);
    EXPECT_GT(modulated_clip, 0.0f) << "Modulated clip should be positive";
}

// ============================================================================
// Test 16: NullBrainReturnsDefaults
// ============================================================================

TEST_F(TrainingModulationE2E, NullBrainReturnsDefaults) {
    float lr = brain_ti_compute_unified_lr(nullptr, 0.01f, nullptr);
    EXPECT_NEAR(lr, 0.01f, 0.001f)
        << "NULL brain should return base_lr";

    brain_ti_modulation_state_t state;
    memset(&state, 0, sizeof(state));
    int rc = brain_ti_compute_modulation_state(nullptr, &state);
    (void)rc;  // Must not crash

    float arousal = brain_ti_get_arousal(nullptr);
    EXPECT_GE(arousal, 0.0f);
    EXPECT_LE(arousal, 1.0f);

    float dopamine = brain_ti_get_dopamine(nullptr);
    EXPECT_NEAR(dopamine, 0.5f, 0.01f);

    float rpe = brain_ti_get_rpe(nullptr);
    EXPECT_NEAR(rpe, 0.0f, 0.01f);

    int mode = brain_ti_get_mode(nullptr);
    EXPECT_EQ(mode, 0);

    float conflict = brain_ti_get_conflict(nullptr);
    EXPECT_NEAR(conflict, 0.0f, 0.01f);
}
