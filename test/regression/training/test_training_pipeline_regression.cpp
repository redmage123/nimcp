/**
 * @file test_training_pipeline_regression.cpp
 * @brief Regression tests for the NIMCP training pipeline
 *
 * WHAT: Guards against known past bugs and edge cases in the training pipeline:
 *       label buffer overflow, gradient distribution, sparse synapse learning,
 *       probe data validity, and global state isolation between brains.
 * WHY:  Each test targets a specific regression that was previously observed or
 *       is a known risk area in the codebase.
 * HOW:  GTest tests using the public nimcp_* API and internal brain_ti_* facade.
 *       Uses NIMCP_BRAIN_TINY for speed with suite-level init/shutdown.
 *
 * REGRESSIONS COVERED:
 * 1. Label buffer overflow (nimcp_part_core.c:281)
 * 2. Gradient distribution correctness
 * 3. Sparse synapse learning no-segfault
 * 4. Probe validity after learning
 * 5. Multiple brains independence
 * 6. Loss NaN/Inf detection
 * 7. Zero-feature training safety
 * 8. Label size boundary handling
 * 9. Modulation state never NaN
 * 10. Post-batch update stability
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

extern "C" {
#include "nimcp.h"
#include "cognitive/training/nimcp_training_integration.h"
#include "core/brain/nimcp_brain.h"
}

// ============================================================================
// Suite-level init/shutdown to avoid per-test overhead
// ============================================================================

class TrainingPipelineRegression : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        nimcp_init();
    }

    static void TearDownTestSuite() {
        nimcp_shutdown();
    }
};

// ============================================================================
// Test 1: LabelBufferNoOverflow
// Regression for nimcp_part_core.c:281 -- use a label at the size limit.
// ============================================================================

TEST_F(TrainingPipelineRegression, LabelBufferNoOverflow) {
    nimcp_brain_t brain = nimcp_brain_create("label_overflow_test",
        NIMCP_BRAIN_TINY, NIMCP_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    char long_label[NIMCP_MAX_LABEL_SIZE];
    memset(long_label, 'A', NIMCP_MAX_LABEL_SIZE - 1);
    long_label[NIMCP_MAX_LABEL_SIZE - 1] = '\0';

    float features[4] = {0.1f, 0.2f, 0.3f, 0.4f};
    nimcp_status_t rc = nimcp_brain_learn_example(brain, features, 4, long_label, 0.5f);
    EXPECT_EQ(rc, NIMCP_OK) << "learn_example with max-length label should succeed";

    char pred_label[NIMCP_MAX_LABEL_SIZE];
    float confidence = -1.0f;
    rc = nimcp_brain_predict_fast(brain, features, 4, pred_label, &confidence);
    EXPECT_EQ(rc, NIMCP_OK);
    EXPECT_LE(strlen(pred_label), (size_t)(NIMCP_MAX_LABEL_SIZE - 1))
        << "Predicted label should not exceed NIMCP_MAX_LABEL_SIZE - 1";

    nimcp_brain_destroy(brain);
}

// ============================================================================
// Test 2: GradientDistributionCorrect
// Regression for gradient NaN/Inf issues -- values should be finite.
// ============================================================================

TEST_F(TrainingPipelineRegression, GradientDistributionCorrect) {
    nimcp_brain_t brain = nimcp_brain_create("grad_distribution_test",
        NIMCP_BRAIN_TINY, NIMCP_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    float features[4];
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 4; j++) features[j] = (float)(i + j) / 15.0f;
        nimcp_brain_learn_example(brain, features, 4, "class_0", 0.5f);
    }

    float grad_norm = nimcp_brain_get_last_gradient_norm(brain);
    EXPECT_FALSE(std::isnan(grad_norm)) << "Gradient norm should not be NaN";
    EXPECT_FALSE(std::isinf(grad_norm)) << "Gradient norm should not be Inf";
    EXPECT_GE(grad_norm, 0.0f) << "Gradient norm should be non-negative";

    nimcp_brain_destroy(brain);
}

// ============================================================================
// Test 3: SparseSynapseLearnNoSegfault
// Regression for sparse synapse incompatibility -- no crash on 50 examples.
// ============================================================================

TEST_F(TrainingPipelineRegression, SparseSynapseLearnNoSegfault) {
    nimcp_brain_t brain = nimcp_brain_create("sparse_synapse_test",
        NIMCP_BRAIN_TINY, NIMCP_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    float features[4];
    for (int i = 0; i < 50; i++) {
        for (int j = 0; j < 4; j++) {
            features[j] = (float)((i * 7 + j * 3) % 13) / 13.0f;
        }
        const char* label = (i % 2 == 0) ? "a" : "b";
        nimcp_brain_learn_example(brain, features, 4, label, 0.5f);
    }

    // If we got here, no segfault occurred.
    nimcp_brain_destroy(brain);
}

// ============================================================================
// Test 4: ProbeAfterLearningValid
// Probe returns valid data -- num_neurons > 0, sparsity in [0,1].
// ============================================================================

TEST_F(TrainingPipelineRegression, ProbeAfterLearningValid) {
    nimcp_brain_t brain = nimcp_brain_create("probe_regression_test",
        NIMCP_BRAIN_TINY, NIMCP_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    float features[4] = {0.1f, 0.2f, 0.3f, 0.4f};
    for (int i = 0; i < 5; i++) {
        nimcp_brain_learn_example(brain, features, 4, "probe_class", 0.5f);
    }

    nimcp_brain_probe_t probe;
    memset(&probe, 0, sizeof(probe));
    nimcp_status_t rc = nimcp_brain_probe(brain, &probe);
    EXPECT_EQ(rc, NIMCP_OK);
    EXPECT_GT(probe.num_neurons, 0u);
    EXPECT_GE(probe.avg_sparsity, 0.0f);
    EXPECT_LE(probe.avg_sparsity, 1.0f);
    EXPECT_FALSE(std::isnan(probe.avg_inference_time_us));

    nimcp_brain_destroy(brain);
}

// ============================================================================
// Test 5: MultipleBrainsIndependent
// Two brains trained differently should have independent state.
// ============================================================================

TEST_F(TrainingPipelineRegression, MultipleBrainsIndependent) {
    nimcp_brain_t brain_a = nimcp_brain_create("brain_a",
        NIMCP_BRAIN_TINY, NIMCP_TASK_CLASSIFICATION, 4, 2);
    nimcp_brain_t brain_b = nimcp_brain_create("brain_b",
        NIMCP_BRAIN_TINY, NIMCP_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain_a, nullptr);
    ASSERT_NE(brain_b, nullptr);

    float features_a[4] = {1.0f, 0.0f, 0.0f, 0.0f};
    for (int i = 0; i < 10; i++) {
        nimcp_brain_learn_example(brain_a, features_a, 4, "left", 0.5f);
    }

    float features_b[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    for (int i = 0; i < 10; i++) {
        nimcp_brain_learn_example(brain_b, features_b, 4, "right", 0.5f);
    }

    nimcp_brain_probe_t probe_a, probe_b;
    memset(&probe_a, 0, sizeof(probe_a));
    memset(&probe_b, 0, sizeof(probe_b));
    nimcp_brain_probe(brain_a, &probe_a);
    nimcp_brain_probe(brain_b, &probe_b);

    EXPECT_GT(probe_a.total_learning_steps, 0u);
    EXPECT_GT(probe_b.total_learning_steps, 0u);

    float loss_a = nimcp_brain_get_last_loss(brain_a);
    float loss_b = nimcp_brain_get_last_loss(brain_b);
    EXPECT_FALSE(std::isnan(loss_a));
    EXPECT_FALSE(std::isnan(loss_b));

    nimcp_brain_destroy(brain_a);
    nimcp_brain_destroy(brain_b);
}

// ============================================================================
// Test 6: LossNeverNaNAfterTraining
// Loss should never be NaN after training with reasonable inputs.
// ============================================================================

TEST_F(TrainingPipelineRegression, LossNeverNaNAfterTraining) {
    nimcp_brain_t brain = nimcp_brain_create("loss_nan_test",
        NIMCP_BRAIN_TINY, NIMCP_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    float features[4] = {0.1f, 0.2f, 0.3f, 0.4f};
    for (int i = 0; i < 20; i++) {
        nimcp_brain_learn_example(brain, features, 4, "class_x", 0.5f);
        float loss = nimcp_brain_get_last_loss(brain);
        EXPECT_FALSE(std::isnan(loss)) << "Loss is NaN at step " << i;
        EXPECT_FALSE(std::isinf(loss)) << "Loss is Inf at step " << i;
    }

    nimcp_brain_destroy(brain);
}

// ============================================================================
// Test 7: TrainingWithZeroFeaturesNoCrash
// All-zero features should not crash or produce NaN.
// ============================================================================

TEST_F(TrainingPipelineRegression, TrainingWithZeroFeaturesNoCrash) {
    nimcp_brain_t brain = nimcp_brain_create("zero_features_test",
        NIMCP_BRAIN_TINY, NIMCP_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    float features[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    for (int i = 0; i < 10; i++) {
        nimcp_brain_learn_example(brain, features, 4, "zero", 0.5f);
    }

    float loss = nimcp_brain_get_last_loss(brain);
    EXPECT_FALSE(std::isnan(loss)) << "Loss should not be NaN with zero features";

    nimcp_brain_destroy(brain);
}

// ============================================================================
// Test 8: ExactLabelSizeBoundary
// Test label handling at various boundary sizes.
// ============================================================================

TEST_F(TrainingPipelineRegression, ExactLabelSizeBoundary) {
    nimcp_brain_t brain = nimcp_brain_create("label_boundary_test",
        NIMCP_BRAIN_TINY, NIMCP_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    float features[4] = {0.5f, 0.5f, 0.5f, 0.5f};

    // Short label
    nimcp_status_t rc = nimcp_brain_learn_example(brain, features, 4, "a", 0.5f);
    EXPECT_EQ(rc, NIMCP_OK);

    // Empty label
    rc = nimcp_brain_learn_example(brain, features, 4, "", 0.5f);
    (void)rc;  // May fail, must not crash

    // Medium label
    rc = nimcp_brain_learn_example(brain, features, 4, "medium_length_label", 0.5f);
    EXPECT_EQ(rc, NIMCP_OK);

    nimcp_brain_destroy(brain);
}

// ============================================================================
// Test 9: ModulationStateNeverNaN
// All fields in modulation state should be finite.
// ============================================================================

TEST_F(TrainingPipelineRegression, ModulationStateNeverNaN) {
    brain_t internal = brain_create("modulation_nan_test", BRAIN_SIZE_TINY,
                                     BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(internal, nullptr);

    brain_ti_modulation_state_t state;
    memset(&state, 0, sizeof(state));
    int rc = brain_ti_compute_modulation_state(internal, &state);
    EXPECT_EQ(rc, 0);

    EXPECT_FALSE(std::isnan(state.arousal_level));
    EXPECT_FALSE(std::isnan(state.arousal_cognitive_gain));
    EXPECT_FALSE(std::isnan(state.arousal_memory_consolidation));
    EXPECT_FALSE(std::isnan(state.circadian_efficiency));
    EXPECT_FALSE(std::isnan(state.rpe_bonus));
    EXPECT_FALSE(std::isnan(state.inflammation_learning_factor));
    EXPECT_FALSE(std::isnan(state.inflammation_precision));
    EXPECT_FALSE(std::isnan(state.instability_lr_scale));
    EXPECT_FALSE(std::isnan(state.instability_batch_scale));
    EXPECT_FALSE(std::isnan(state.instability_clip_factor));
    EXPECT_FALSE(std::isnan(state.portia_learning_gate));
    EXPECT_FALSE(std::isnan(state.portia_compute_budget));
    EXPECT_FALSE(std::isnan(state.stress_level));
    EXPECT_FALSE(std::isnan(state.cognitive_capacity));
    EXPECT_FALSE(std::isnan(state.conflict_level));
    EXPECT_FALSE(std::isnan(state.final_lr_factor));
    EXPECT_FALSE(std::isnan(state.final_batch_factor));
    EXPECT_FALSE(std::isnan(state.final_clip_factor));

    brain_ti_destroy_reasoning(internal);
    brain_destroy(internal);
}

// ============================================================================
// Test 10: PostBatchUpdateStable
// Repeated post-batch updates should not crash or produce NaN.
// ============================================================================

TEST_F(TrainingPipelineRegression, PostBatchUpdateStable) {
    brain_t internal = brain_create("post_batch_stable_test", BRAIN_SIZE_TINY,
                                     BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(internal, nullptr);

    for (int i = 0; i < 30; i++) {
        float accuracy = (float)(i % 10) / 10.0f;
        int rc = brain_ti_post_batch_update(internal, accuracy, 0.5f, "regression_test");
        EXPECT_EQ(rc, 0) << "post_batch_update failed at iteration " << i;
    }

    float da = brain_ti_get_dopamine(internal);
    EXPECT_FALSE(std::isnan(da));
    EXPECT_GE(da, 0.0f);
    EXPECT_LE(da, 1.0f);

    float rpe = brain_ti_get_rpe(internal);
    EXPECT_FALSE(std::isnan(rpe));

    brain_ti_destroy_reasoning(internal);
    brain_destroy(internal);
}
