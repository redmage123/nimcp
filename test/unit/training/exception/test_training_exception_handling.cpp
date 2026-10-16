/**
 * @file test_training_exception_handling.cpp
 * @brief Unit tests for Training Module Exception Handling
 *
 * WHAT: Test exception handling for training subsystem components
 * WHY:  Verify proper exception creation, handling, and recovery for training errors
 * HOW:  Test gradient explosion/vanishing, NaN/Inf detection, divergence, checkpoint/rollback
 *
 * TEST CATEGORIES:
 * - Gradient Anomaly Exceptions (explosion, vanishing)
 * - Numerical Stability Exceptions (NaN/Inf detection)
 * - Training Divergence Exceptions
 * - Checkpoint/Rollback Exception Handling
 * - Brain Exception Training Context
 * - Recovery Strategy Verification
 *
 * @author NIMCP Development Team
 * @date 2026-01-16
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <limits>

extern "C" {
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "training/nimcp_gradient_scaling.h"
#include "training/nimcp_mixed_precision.h"
#include "training/nimcp_training_checkpoint.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class TrainingExceptionHandlingTest : public ::testing::Test {
protected:
    static std::atomic<int> handler_call_count;
    static std::atomic<int> last_exception_code;
    static std::atomic<nimcp_exception_category_t> last_exception_category;
    static std::atomic<bool> recovery_attempted;

    nimcp_handler_registration_t* test_handler_reg;

    void SetUp() override {
        handler_call_count = 0;
        last_exception_code = 0;
        last_exception_category = EXCEPTION_CATEGORY_GENERIC;
        recovery_attempted = false;

        nimcp_exception_system_init();

        // Register test handler
        nimcp_handler_options_t options;
        nimcp_handler_default_options(&options);
        options.name = "training_test_handler";
        options.handler = test_exception_handler;
        options.priority = NIMCP_HANDLER_PRIORITY_HIGH;
        options.user_data = nullptr;
        test_handler_reg = nimcp_handler_register(&options);
    }

    void TearDown() override {
        if (test_handler_reg) {
            nimcp_handler_unregister(test_handler_reg);
            test_handler_reg = nullptr;
        }
        nimcp_exception_clear_current();
        nimcp_exception_system_shutdown();
    }

    static bool test_exception_handler(nimcp_exception_t* ex, void* user_data) {
        (void)user_data;
        handler_call_count++;
        last_exception_code = ex->code;
        last_exception_category = ex->category;
        return false;  // Don't consume - allow chain to continue
    }

    static int test_recovery_callback(nimcp_exception_t* ex, nimcp_exception_recovery_action_t action, void* user_data) {
        (void)ex;
        (void)action;
        (void)user_data;
        recovery_attempted = true;
        return 0;  // Success
    }
};

std::atomic<int> TrainingExceptionHandlingTest::handler_call_count(0);
std::atomic<int> TrainingExceptionHandlingTest::last_exception_code(0);
std::atomic<nimcp_exception_category_t> TrainingExceptionHandlingTest::last_exception_category(EXCEPTION_CATEGORY_GENERIC);
std::atomic<bool> TrainingExceptionHandlingTest::recovery_attempted(false);

//=============================================================================
// Gradient Explosion Exception Tests
//=============================================================================

class GradientExplosionExceptionTest : public TrainingExceptionHandlingTest {};

TEST_F(GradientExplosionExceptionTest, CreateBrainException_GradientExplosion) {
    // WHAT: Create brain exception for gradient explosion
    // WHY:  Gradient explosion is a common training failure mode

    nimcp_brain_exception_t* ex = nimcp_brain_exception_create(
        NIMCP_ERROR_BACKWARD_PASS,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        0,  // brain_id
        "training_layer_5",
        "Gradient explosion detected: norm=%.2e exceeds threshold", 1e12f
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->base.code, NIMCP_ERROR_BACKWARD_PASS);
    EXPECT_EQ(ex->base.category, EXCEPTION_CATEGORY_BRAIN);
    EXPECT_EQ(ex->base.severity, EXCEPTION_SEVERITY_SEVERE);
    EXPECT_EQ(ex->base.type, EXCEPTION_TYPE_BRAIN);

    // Set brain-specific fields
    ex->gradient_norm = 1e12f;
    ex->learning_diverged = false;
    ex->has_nan_weights = false;

    EXPECT_EQ(ex->gradient_norm, 1e12f);
    EXPECT_STREQ(ex->region_name, "training_layer_5");

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

TEST_F(GradientExplosionExceptionTest, GradientExplosion_DispatchToHandler) {
    // WHAT: Test gradient explosion exception dispatches correctly
    // WHY:  Verify handler chain receives training exceptions

    nimcp_brain_exception_t* ex = nimcp_brain_exception_create(
        NIMCP_ERROR_BACKWARD_PASS,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        1, "layer_fc1",
        "Gradient explosion: norm exceeds safe threshold"
    );
    ASSERT_NE(ex, nullptr);

    handler_call_count = 0;
    nimcp_exception_dispatch((nimcp_exception_t*)ex);

    EXPECT_GE(handler_call_count.load(), 1);
    EXPECT_EQ(last_exception_code.load(), NIMCP_ERROR_BACKWARD_PASS);
    EXPECT_EQ(last_exception_category.load(), EXCEPTION_CATEGORY_BRAIN);

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

TEST_F(GradientExplosionExceptionTest, GradientExplosion_RecoveryStrategy) {
    // WHAT: Test recovery strategy for gradient explosion
    // WHY:  Verify correct recovery action is suggested

    nimcp_brain_exception_t* ex = nimcp_brain_exception_create(
        NIMCP_ERROR_BACKWARD_PASS,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        0, "conv_layer",
        "Gradient explosion in convolutional layer"
    );
    ASSERT_NE(ex, nullptr);

    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy((nimcp_exception_t*)ex, &strategy);

    // Brain exceptions should suggest GC or rollback
    EXPECT_TRUE(
        strategy.primary_action == EXCEPTION_RECOVERY_GC ||
        strategy.primary_action == EXCEPTION_RECOVERY_ROLLBACK ||
        strategy.primary_action == EXCEPTION_RECOVERY_REDUCE_LOAD
    );

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

TEST_F(GradientExplosionExceptionTest, GradientExplosion_EpitopeGeneration) {
    // WHAT: Test immune epitope generation for gradient explosion
    // WHY:  Epitopes enable immune system pattern matching

    nimcp_brain_exception_t* ex = nimcp_brain_exception_create(
        NIMCP_ERROR_BACKWARD_PASS,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        0, "layer_test",
        "Gradient explosion for epitope test"
    );
    ASSERT_NE(ex, nullptr);

    size_t epitope_len = nimcp_exception_generate_epitope((nimcp_exception_t*)ex);
    EXPECT_GT(epitope_len, 0u);
    EXPECT_LE(epitope_len, NIMCP_EXCEPTION_EPITOPE_SIZE);
    EXPECT_EQ(ex->base.epitope_len, epitope_len);

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

//=============================================================================
// Gradient Vanishing Exception Tests
//=============================================================================

class GradientVanishingExceptionTest : public TrainingExceptionHandlingTest {};

TEST_F(GradientVanishingExceptionTest, CreateBrainException_GradientVanishing) {
    // WHAT: Create brain exception for vanishing gradients
    // WHY:  Vanishing gradients prevent learning in deep networks

    nimcp_brain_exception_t* ex = nimcp_brain_exception_create(
        NIMCP_ERROR_LEARNING_FAILED,
        EXCEPTION_SEVERITY_WARNING,
        __FILE__, __LINE__, __func__,
        0, "deep_layer_20",
        "Gradient vanishing detected: norm=%.2e below threshold", 1e-12f
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->base.code, NIMCP_ERROR_LEARNING_FAILED);
    EXPECT_EQ(ex->base.severity, EXCEPTION_SEVERITY_WARNING);

    ex->gradient_norm = 1e-12f;
    EXPECT_FLOAT_EQ(ex->gradient_norm, 1e-12f);

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

TEST_F(GradientVanishingExceptionTest, GradientVanishing_ContextInformation) {
    // WHAT: Test context information for gradient vanishing
    // WHY:  Context helps diagnose training issues

    nimcp_brain_exception_t* ex = nimcp_brain_exception_create(
        NIMCP_ERROR_LEARNING_FAILED,
        EXCEPTION_SEVERITY_WARNING,
        __FILE__, __LINE__, __func__,
        2, "layer_12",
        "Vanishing gradient in layer 12"
    );
    ASSERT_NE(ex, nullptr);

    // Set context
    nimcp_exception_set_context((nimcp_exception_t*)ex, "layer_depth", "12");
    nimcp_exception_set_context((nimcp_exception_t*)ex, "activation", "sigmoid");
    nimcp_exception_set_context((nimcp_exception_t*)ex, "learning_rate", "0.001");

    EXPECT_STREQ(nimcp_exception_get_context((nimcp_exception_t*)ex, "layer_depth"), "12");
    EXPECT_STREQ(nimcp_exception_get_context((nimcp_exception_t*)ex, "activation"), "sigmoid");
    EXPECT_EQ(nimcp_exception_context_count((nimcp_exception_t*)ex), 3u);

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

//=============================================================================
// NaN/Inf Detection Exception Tests
//=============================================================================

class NumericalStabilityExceptionTest : public TrainingExceptionHandlingTest {};

TEST_F(NumericalStabilityExceptionTest, CreateException_NaNDetection) {
    // WHAT: Create exception for NaN detection in weights/gradients
    // WHY:  NaN values corrupt training and must be handled

    nimcp_brain_exception_t* ex = nimcp_brain_exception_create(
        NIMCP_ERROR_FORWARD_PASS,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        0, "fc_layer_weights",
        "NaN detected in weights at indices [45, 128]"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->base.code, NIMCP_ERROR_FORWARD_PASS);
    ex->has_nan_weights = true;
    EXPECT_TRUE(ex->has_nan_weights);

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

TEST_F(NumericalStabilityExceptionTest, CreateException_InfDetection) {
    // WHAT: Create exception for Inf detection in weights/gradients
    // WHY:  Inf values indicate overflow and training instability

    nimcp_brain_exception_t* ex = nimcp_brain_exception_create(
        NIMCP_ERROR_BACKWARD_PASS,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        0, "attention_scores",
        "Inf detected in attention scores after softmax"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->base.code, NIMCP_ERROR_BACKWARD_PASS);

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

TEST_F(NumericalStabilityExceptionTest, NaN_ImmunePresentation) {
    // WHAT: Test NaN exception presentation to immune system
    // WHY:  Immune system should learn NaN patterns for prevention

    nimcp_brain_exception_t* ex = nimcp_brain_exception_create(
        NIMCP_ERROR_FORWARD_PASS,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        0, "layer_nan",
        "NaN for immune presentation test"
    );
    ASSERT_NE(ex, nullptr);

    nimcp_immune_response_t response;
    std::memset(&response, 0, sizeof(response));
    int result = nimcp_exception_present_to_immune((nimcp_exception_t*)ex, &response);
    EXPECT_EQ(result, 0);

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

TEST_F(NumericalStabilityExceptionTest, DetectNaNInGradients) {
    // WHAT: Verify NaN detection in gradient arrays
    // WHY:  Early NaN detection prevents training corruption

    std::vector<float> gradients = {0.5f, 1.2f, NAN, 0.3f, -0.8f};
    bool has_nan = false;

    for (float g : gradients) {
        if (std::isnan(g)) {
            has_nan = true;
            break;
        }
    }

    EXPECT_TRUE(has_nan);

    if (has_nan) {
        nimcp_brain_exception_t* ex = nimcp_brain_exception_create(
            NIMCP_ERROR_FORWARD_PASS,
            EXCEPTION_SEVERITY_SEVERE,
            __FILE__, __LINE__, __func__,
            0, "gradient_check",
            "NaN detected in gradient array at index 2"
        );
        ASSERT_NE(ex, nullptr);
        nimcp_exception_dispatch((nimcp_exception_t*)ex);
        EXPECT_GE(handler_call_count.load(), 1);
        nimcp_exception_unref((nimcp_exception_t*)ex);
    }
}

TEST_F(NumericalStabilityExceptionTest, DetectInfInGradients) {
    // WHAT: Verify Inf detection in gradient arrays
    // WHY:  Early Inf detection prevents overflow propagation

    std::vector<float> gradients = {0.5f, 1.2f, std::numeric_limits<float>::infinity(), 0.3f};
    bool has_inf = false;

    for (float g : gradients) {
        if (std::isinf(g)) {
            has_inf = true;
            break;
        }
    }

    EXPECT_TRUE(has_inf);

    if (has_inf) {
        nimcp_brain_exception_t* ex = nimcp_brain_exception_create(
            NIMCP_ERROR_BACKWARD_PASS,
            EXCEPTION_SEVERITY_SEVERE,
            __FILE__, __LINE__, __func__,
            0, "gradient_check",
            "Inf detected in gradient array at index 2"
        );
        ASSERT_NE(ex, nullptr);
        nimcp_exception_dispatch((nimcp_exception_t*)ex);
        EXPECT_GE(handler_call_count.load(), 1);
        nimcp_exception_unref((nimcp_exception_t*)ex);
    }
}

//=============================================================================
// Training Divergence Exception Tests
//=============================================================================

class TrainingDivergenceExceptionTest : public TrainingExceptionHandlingTest {};

TEST_F(TrainingDivergenceExceptionTest, CreateException_LossDivergence) {
    // WHAT: Create exception for loss divergence
    // WHY:  Diverging loss indicates failed training

    nimcp_brain_exception_t* ex = nimcp_brain_exception_create(
        NIMCP_ERROR_INFERENCE_FAILED,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        0, "training_loop",
        "Training diverged: loss increased from %.4f to %.4f over %d steps",
        0.5f, 1e8f, 100
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->base.code, NIMCP_ERROR_INFERENCE_FAILED);
    ex->learning_diverged = true;
    EXPECT_TRUE(ex->learning_diverged);

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

TEST_F(TrainingDivergenceExceptionTest, Divergence_RecoveryRollback) {
    // WHAT: Test rollback recovery for divergence
    // WHY:  Rollback to checkpoint is standard recovery

    nimcp_brain_exception_t* ex = nimcp_brain_exception_create(
        NIMCP_ERROR_INFERENCE_FAILED,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        0, "training",
        "Training diverged, attempting rollback"
    );
    ASSERT_NE(ex, nullptr);

    // Register rollback recovery callback
    nimcp_register_recovery_callback(EXCEPTION_RECOVERY_ROLLBACK, test_recovery_callback, nullptr);

    recovery_attempted = false;
    int result = nimcp_execute_recovery((nimcp_exception_t*)ex, EXCEPTION_RECOVERY_ROLLBACK);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(recovery_attempted.load());

    nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_ROLLBACK);
    nimcp_exception_unref((nimcp_exception_t*)ex);
}

TEST_F(TrainingDivergenceExceptionTest, Divergence_SetContext_Metrics) {
    // WHAT: Test context with training metrics
    // WHY:  Metrics help diagnose divergence cause

    nimcp_brain_exception_t* ex = nimcp_brain_exception_create(
        NIMCP_ERROR_INFERENCE_FAILED,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        0, "training",
        "Training divergence detected"
    );
    ASSERT_NE(ex, nullptr);

    nimcp_exception_set_context((nimcp_exception_t*)ex, "initial_loss", "0.5");
    nimcp_exception_set_context((nimcp_exception_t*)ex, "final_loss", "1e8");
    nimcp_exception_set_context((nimcp_exception_t*)ex, "steps_since_improvement", "500");
    nimcp_exception_set_context((nimcp_exception_t*)ex, "learning_rate", "0.01");
    nimcp_exception_set_context((nimcp_exception_t*)ex, "batch_size", "32");

    EXPECT_EQ(nimcp_exception_context_count((nimcp_exception_t*)ex), 5u);
    EXPECT_STREQ(nimcp_exception_get_context((nimcp_exception_t*)ex, "initial_loss"), "0.5");
    EXPECT_STREQ(nimcp_exception_get_context((nimcp_exception_t*)ex, "final_loss"), "1e8");

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

//=============================================================================
// Checkpoint/Rollback Exception Tests
//=============================================================================

class CheckpointExceptionTest : public TrainingExceptionHandlingTest {};

TEST_F(CheckpointExceptionTest, CreateException_CheckpointFailure) {
    // WHAT: Create exception for checkpoint save failure
    // WHY:  Checkpoint failures risk losing training progress

    nimcp_io_exception_t* ex = nimcp_io_exception_create(
        NIMCP_ERROR_FILE_WRITE,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "/checkpoints/model_epoch_50.ckpt",
        "Failed to save checkpoint: disk full"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->base.code, NIMCP_ERROR_FILE_WRITE);
    EXPECT_EQ(ex->base.category, EXCEPTION_CATEGORY_IO);
    EXPECT_STREQ(ex->path, "/checkpoints/model_epoch_50.ckpt");

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

TEST_F(CheckpointExceptionTest, CreateException_CheckpointCorruption) {
    // WHAT: Create exception for corrupted checkpoint
    // WHY:  Corrupted checkpoints must be detected before loading

    nimcp_io_exception_t* ex = nimcp_io_exception_create(
        NIMCP_ERROR_CHECKPOINT_LOAD,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "/checkpoints/best_model.ckpt",
        "Checkpoint corrupted: CRC mismatch"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->base.code, NIMCP_ERROR_CHECKPOINT_LOAD);

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

TEST_F(CheckpointExceptionTest, CreateException_RollbackFailure) {
    // WHAT: Create exception for rollback failure
    // WHY:  Failed rollback is critical and needs escalation

    nimcp_brain_exception_t* ex = nimcp_brain_exception_create(
        NIMCP_ERROR_CHECKPOINT_LOAD,
        EXCEPTION_SEVERITY_CRITICAL,
        __FILE__, __LINE__, __func__,
        0, "rollback",
        "Rollback failed: checkpoint not found or corrupted"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->base.code, NIMCP_ERROR_CHECKPOINT_LOAD);
    EXPECT_EQ(ex->base.severity, EXCEPTION_SEVERITY_CRITICAL);

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

TEST_F(CheckpointExceptionTest, Checkpoint_RecoveryEmergencySave) {
    // WHAT: Test emergency save recovery for checkpoint failure
    // WHY:  Emergency save is fallback when normal save fails

    nimcp_io_exception_t* ex = nimcp_io_exception_create(
        NIMCP_ERROR_FILE_WRITE,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "/checkpoints/model.ckpt",
        "Checkpoint save failed"
    );
    ASSERT_NE(ex, nullptr);

    nimcp_register_recovery_callback(EXCEPTION_RECOVERY_EMERGENCY_SAVE, test_recovery_callback, nullptr);

    recovery_attempted = false;
    int result = nimcp_execute_recovery((nimcp_exception_t*)ex, EXCEPTION_RECOVERY_EMERGENCY_SAVE);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(recovery_attempted.load());

    nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_EMERGENCY_SAVE);
    nimcp_exception_unref((nimcp_exception_t*)ex);
}

//=============================================================================
// Mixed Precision Training Exception Tests
//=============================================================================

class MixedPrecisionExceptionTest : public TrainingExceptionHandlingTest {};

TEST_F(MixedPrecisionExceptionTest, CreateException_OverflowDetection) {
    // WHAT: Create exception for FP16 overflow
    // WHY:  FP16 has limited range and overflows easily

    nimcp_brain_exception_t* ex = nimcp_brain_exception_create(
        NIMCP_ERROR_BACKWARD_PASS,
        EXCEPTION_SEVERITY_WARNING,
        __FILE__, __LINE__, __func__,
        0, "amp_scaler",
        "FP16 overflow detected, reducing loss scale from %.0f to %.0f",
        65536.0f, 32768.0f
    );

    ASSERT_NE(ex, nullptr);
    nimcp_exception_set_context((nimcp_exception_t*)ex, "dtype", "FP16");
    nimcp_exception_set_context((nimcp_exception_t*)ex, "old_scale", "65536");
    nimcp_exception_set_context((nimcp_exception_t*)ex, "new_scale", "32768");

    EXPECT_STREQ(nimcp_exception_get_context((nimcp_exception_t*)ex, "dtype"), "FP16");

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

TEST_F(MixedPrecisionExceptionTest, CreateException_UnderflowDetection) {
    // WHAT: Create exception for FP16 underflow (gradients too small)
    // WHY:  Underflow zeros out gradients preventing learning

    nimcp_brain_exception_t* ex = nimcp_brain_exception_create(
        NIMCP_ERROR_LEARNING_FAILED,
        EXCEPTION_SEVERITY_WARNING,
        __FILE__, __LINE__, __func__,
        0, "amp_scaler",
        "FP16 underflow: gradients too small, increasing loss scale"
    );

    ASSERT_NE(ex, nullptr);
    nimcp_exception_set_context((nimcp_exception_t*)ex, "dtype", "FP16");
    nimcp_exception_set_context((nimcp_exception_t*)ex, "scale_action", "increase");

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

//=============================================================================
// Gradient Scaling Integration Exception Tests
//=============================================================================

class GradientScalingExceptionTest : public TrainingExceptionHandlingTest {};

TEST_F(GradientScalingExceptionTest, GradientClipping_ExceptionOnExtreme) {
    // WHAT: Test exception when gradients need extreme clipping
    // WHY:  Extreme clipping may indicate training issues

    std::vector<float> gradients = {1e10f, -1e10f, 1e9f, -1e9f};
    float max_norm = 1.0f;

    float original_norm = gs_clip_by_norm(gradients.data(), gradients.size(), max_norm);

    // If clipping ratio is extreme, create warning exception
    float clip_ratio = original_norm / max_norm;
    if (clip_ratio > 1000.0f) {
        nimcp_brain_exception_t* ex = nimcp_brain_exception_create(
            NIMCP_ERROR_BACKWARD_PASS,
            EXCEPTION_SEVERITY_WARNING,
            __FILE__, __LINE__, __func__,
            0, "gradient_scaling",
            "Extreme gradient clipping: ratio=%.2e", clip_ratio
        );
        ASSERT_NE(ex, nullptr);
        nimcp_exception_dispatch((nimcp_exception_t*)ex);
        EXPECT_GE(handler_call_count.load(), 1);
        nimcp_exception_unref((nimcp_exception_t*)ex);
    }
}

TEST_F(GradientScalingExceptionTest, SurrogateGradient_InvalidBeta) {
    // WHAT: Test handling of invalid surrogate gradient parameters
    // WHY:  Invalid parameters should be caught early

    // Beta of 0 might cause division issues depending on surrogate type
    float beta = 0.0f;
    float value = gs_surrogate_value(0.0f, GS_SURROGATE_SUPERSPIKE, beta);

    // Value should not be NaN even with beta=0
    EXPECT_FALSE(std::isnan(value));
}

//=============================================================================
// Try/Catch Exception Tests for Training
//=============================================================================

class TrainingTryCatchTest : public TrainingExceptionHandlingTest {};

TEST_F(TrainingTryCatchTest, TryCatch_GradientException) {
    // WHAT: Test NIMCP_TRY/CATCH with training exceptions
    // WHY:  Verify exception mechanism works for training code

    bool exception_caught = false;
    nimcp_error_t caught_code = 0;

    NIMCP_TRY {
        // Simulate gradient explosion detection
        nimcp_brain_exception_t* ex = nimcp_brain_exception_create(
            NIMCP_ERROR_BACKWARD_PASS,
            EXCEPTION_SEVERITY_SEVERE,
            __FILE__, __LINE__, __func__,
            0, "test_layer",
            "Test gradient explosion in try block"
        );
        if (ex) {
            nimcp_exception_raise((nimcp_exception_t*)ex);
        }
    }
    NIMCP_CATCH(nimcp_exception_t, ex) {
        exception_caught = true;
        caught_code = ex->code;
        nimcp_exception_unref(ex);
    }
    NIMCP_END_TRY;

    EXPECT_TRUE(exception_caught);
    EXPECT_EQ(caught_code, NIMCP_ERROR_BACKWARD_PASS);
}

TEST_F(TrainingTryCatchTest, TryCatch_NoException) {
    // WHAT: Test NIMCP_TRY/CATCH with no exception thrown
    // WHY:  Normal execution should proceed without issues

    bool try_executed = false;
    bool catch_executed = false;

    NIMCP_TRY {
        try_executed = true;
        // Normal operation - no exception
    }
    NIMCP_CATCH(nimcp_exception_t, ex) {
        catch_executed = true;
        nimcp_exception_unref(ex);
    }
    NIMCP_END_TRY;

    EXPECT_TRUE(try_executed);
    EXPECT_FALSE(catch_executed);
}

//=============================================================================
// Aggregate Exception Tests for Training
//=============================================================================

class TrainingAggregateExceptionTest : public TrainingExceptionHandlingTest {};

TEST_F(TrainingAggregateExceptionTest, AggregateMultipleTrainingErrors) {
    // WHAT: Aggregate multiple training errors into single exception
    // WHY:  Batch errors should be reported together

    nimcp_aggregate_exception_t* agg = nimcp_aggregate_exception_create(
        NIMCP_ERROR_LEARNING_FAILED,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Multiple training errors occurred in epoch 50"
    );
    ASSERT_NE(agg, nullptr);

    // Add gradient explosion
    nimcp_brain_exception_t* grad_ex = nimcp_brain_exception_create(
        NIMCP_ERROR_BACKWARD_PASS,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        0, "layer_1",
        "Gradient explosion in layer 1"
    );
    ASSERT_NE(grad_ex, nullptr);
    nimcp_aggregate_exception_add(agg, (nimcp_exception_t*)grad_ex);

    // Add NaN detection
    nimcp_brain_exception_t* nan_ex = nimcp_brain_exception_create(
        NIMCP_ERROR_FORWARD_PASS,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        0, "layer_3",
        "NaN detected in layer 3"
    );
    ASSERT_NE(nan_ex, nullptr);
    nimcp_aggregate_exception_add(agg, (nimcp_exception_t*)nan_ex);

    EXPECT_EQ(nimcp_aggregate_exception_count(agg), 2u);

    nimcp_exception_t* child0 = nimcp_aggregate_exception_get(agg, 0);
    ASSERT_NE(child0, nullptr);
    EXPECT_EQ(child0->code, NIMCP_ERROR_BACKWARD_PASS);

    nimcp_exception_t* child1 = nimcp_aggregate_exception_get(agg, 1);
    ASSERT_NE(child1, nullptr);
    EXPECT_EQ(child1->code, NIMCP_ERROR_FORWARD_PASS);

    nimcp_exception_unref((nimcp_exception_t*)agg);
}

//=============================================================================
// Exception Chaining Tests for Training
//=============================================================================

class TrainingExceptionChainingTest : public TrainingExceptionHandlingTest {};

TEST_F(TrainingExceptionChainingTest, ChainCauseExceptions) {
    // WHAT: Chain related training exceptions with cause
    // WHY:  Root cause analysis requires exception chaining

    // Root cause: NaN in weights
    nimcp_brain_exception_t* cause = nimcp_brain_exception_create(
        NIMCP_ERROR_FORWARD_PASS,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        0, "weights",
        "NaN in weights"
    );
    ASSERT_NE(cause, nullptr);

    // Effect: Gradient explosion due to NaN propagation
    nimcp_brain_exception_t* effect = nimcp_brain_exception_create(
        NIMCP_ERROR_BACKWARD_PASS,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        0, "gradients",
        "Gradient explosion caused by NaN weights"
    );
    ASSERT_NE(effect, nullptr);

    nimcp_exception_set_cause((nimcp_exception_t*)effect, (nimcp_exception_t*)cause);

    nimcp_exception_t* retrieved_cause = nimcp_exception_get_cause((nimcp_exception_t*)effect);
    ASSERT_NE(retrieved_cause, nullptr);
    EXPECT_EQ(retrieved_cause->code, NIMCP_ERROR_FORWARD_PASS);

    nimcp_exception_unref((nimcp_exception_t*)effect);
    // cause is freed automatically when effect is freed (due to chaining)
}

//=============================================================================
// Severity Level Tests for Training
//=============================================================================

class TrainingSeverityTest : public TrainingExceptionHandlingTest {};

TEST_F(TrainingSeverityTest, SeverityLevels_Appropriate) {
    // WHAT: Verify severity levels are appropriate for training errors
    // WHY:  Severity affects immune response and recovery priority

    // Warning: minor gradient issues
    nimcp_brain_exception_t* warning = nimcp_brain_exception_create(
        NIMCP_ERROR_LEARNING_FAILED,
        EXCEPTION_SEVERITY_WARNING,
        __FILE__, __LINE__, __func__,
        0, "layer", "Minor gradient vanishing"
    );
    ASSERT_NE(warning, nullptr);
    EXPECT_EQ(warning->base.severity, EXCEPTION_SEVERITY_WARNING);
    nimcp_exception_unref((nimcp_exception_t*)warning);

    // Severe: training issues requiring intervention
    nimcp_brain_exception_t* severe = nimcp_brain_exception_create(
        NIMCP_ERROR_BACKWARD_PASS,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        0, "layer", "Gradient explosion"
    );
    ASSERT_NE(severe, nullptr);
    EXPECT_EQ(severe->base.severity, EXCEPTION_SEVERITY_SEVERE);
    nimcp_exception_unref((nimcp_exception_t*)severe);

    // Critical: training failed, needs rollback
    nimcp_brain_exception_t* critical = nimcp_brain_exception_create(
        NIMCP_ERROR_INFERENCE_FAILED,
        EXCEPTION_SEVERITY_CRITICAL,
        __FILE__, __LINE__, __func__,
        0, "training", "Training completely diverged"
    );
    ASSERT_NE(critical, nullptr);
    EXPECT_EQ(critical->base.severity, EXCEPTION_SEVERITY_CRITICAL);
    nimcp_exception_unref((nimcp_exception_t*)critical);
}

//=============================================================================
// Recovery Action Tests
//=============================================================================

class TrainingRecoveryActionTest : public TrainingExceptionHandlingTest {};

TEST_F(TrainingRecoveryActionTest, VerifyRecoveryActionStrings) {
    // WHAT: Verify recovery action string conversion
    // WHY:  Human-readable action names for logging

    EXPECT_STREQ(nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_NONE), "NONE");
    EXPECT_STREQ(nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_RETRY), "RETRY");
    EXPECT_STREQ(nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_GC), "GC");
    EXPECT_STREQ(nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_ROLLBACK), "ROLLBACK");
    EXPECT_STREQ(nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_REDUCE_LOAD), "REDUCE_LOAD");
    EXPECT_STREQ(nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_EMERGENCY_SAVE), "EMERGENCY_SAVE");
    EXPECT_STREQ(nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_GRACEFUL_SHUTDOWN), "GRACEFUL_SHUTDOWN");
}

TEST_F(TrainingRecoveryActionTest, MultipleRecoveryCallbacks) {
    // WHAT: Test multiple recovery callbacks for different actions
    // WHY:  Different training errors need different recovery strategies

    static bool gc_called = false;
    static bool rollback_called = false;

    auto gc_callback = [](nimcp_exception_t*, nimcp_exception_recovery_action_t, void*) -> int {
        gc_called = true;
        return 0;
    };

    auto rollback_callback = [](nimcp_exception_t*, nimcp_exception_recovery_action_t, void*) -> int {
        rollback_called = true;
        return 0;
    };

    nimcp_register_recovery_callback(EXCEPTION_RECOVERY_GC, gc_callback, nullptr);
    nimcp_register_recovery_callback(EXCEPTION_RECOVERY_ROLLBACK, rollback_callback, nullptr);

    nimcp_brain_exception_t* ex = nimcp_brain_exception_create(
        NIMCP_ERROR_BACKWARD_PASS,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        0, "test", "Test recovery"
    );
    ASSERT_NE(ex, nullptr);

    gc_called = false;
    rollback_called = false;

    nimcp_execute_recovery((nimcp_exception_t*)ex, EXCEPTION_RECOVERY_GC);
    EXPECT_TRUE(gc_called);
    EXPECT_FALSE(rollback_called);

    gc_called = false;
    nimcp_execute_recovery((nimcp_exception_t*)ex, EXCEPTION_RECOVERY_ROLLBACK);
    EXPECT_FALSE(gc_called);
    EXPECT_TRUE(rollback_called);

    nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_GC);
    nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_ROLLBACK);
    nimcp_exception_unref((nimcp_exception_t*)ex);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
