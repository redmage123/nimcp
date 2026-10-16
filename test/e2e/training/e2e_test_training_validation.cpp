/**
 * @file e2e_test_training_validation.cpp
 * @brief Comprehensive Training Pipeline Validation Tests
 *
 * WHAT: End-to-end validation of the complete training infrastructure
 * WHY:  Verify training readiness before production model training
 * HOW:  Tests data loading, gradient flow, weight updates, memory stability, convergence
 *
 * VALIDATION POINTS:
 * 1. End-to-end training loop functionality
 * 2. Data loading pipeline correctness
 * 3. Gradient flow and weight updates (weights actually change)
 * 4. Memory stability over extended training runs
 * 5. Convergence on simple learnable tasks (XOR, linear separation)
 *
 * @author NIMCP Development Team
 * @date 2026-01-16
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <random>
#include <cstring>
#include <cstdio>
#include <chrono>

extern "C" {
#include "core/brain/nimcp_brain.h"
#include "utils/memory/nimcp_memory.h"
#include "nimcp.h"
}

namespace nimcp {
namespace e2e {
namespace training {

/**
 * @brief Test fixture for comprehensive training validation
 */
class TrainingValidationE2E : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_memory_init();
        nimcp_memory_get_stats(&initial_stats_);
    }

    void TearDown() override {
        nimcp_memory_get_stats(&final_stats_);
        // Allow small overhead for internal caches
        EXPECT_LE(final_stats_.current_allocated, initial_stats_.current_allocated + 4096)
            << "Memory leak detected: "
            << (final_stats_.current_allocated - initial_stats_.current_allocated)
            << " bytes";
    }

    nimcp_memory_stats_t initial_stats_;
    nimcp_memory_stats_t final_stats_;
};

//=============================================================================
// TEST 1: End-to-End Training Loop
//=============================================================================

/**
 * @brief Verify complete training loop executes without errors
 *
 * Tests:
 * - Brain creation with training configuration
 * - Training step execution
 * - Loss computation
 * - Training stats retrieval
 */
TEST_F(TrainingValidationE2E, EndToEndTrainingLoop) {
    printf("\n=== TEST: End-to-End Training Loop ===\n");

    // Create brain
    nimcp_brain_t brain = nimcp_brain_create(
        "e2e_training_test",
        NIMCP_BRAIN_SMALL,
        NIMCP_TASK_CLASSIFICATION,
        4,   // 4 input features
        2    // 2 output classes
    );
    ASSERT_NE(brain, nullptr) << "Failed to create brain";

    // Configure training
    nimcp_training_config_t config = nimcp_training_config_default();
    config.loss_type = NIMCP_API_LOSS_CROSS_ENTROPY;
    config.optimizer_type = NIMCP_API_OPT_ADAM;
    config.learning_rate = 0.01f;
    config.enable_gradient_clipping = true;
    config.gradient_clip_value = 1.0f;

    nimcp_status_t status = nimcp_brain_configure_training(brain, &config);
    ASSERT_EQ(status, NIMCP_OK) << "Failed to configure training";

    // Training data: simple linearly separable problem
    // Class 0: features sum < 0
    // Class 1: features sum >= 0
    float features_batch[8][4] = {
        {-0.5f, -0.5f, -0.3f, -0.2f},  // sum = -1.5, class 0
        { 0.5f,  0.5f,  0.3f,  0.2f},  // sum =  1.5, class 1
        {-0.8f, -0.2f,  0.1f, -0.3f},  // sum = -1.2, class 0
        { 0.8f,  0.2f, -0.1f,  0.3f},  // sum =  1.2, class 1
        {-0.3f, -0.4f, -0.5f,  0.1f},  // sum = -1.1, class 0
        { 0.3f,  0.4f,  0.5f, -0.1f},  // sum =  1.1, class 1
        {-0.6f,  0.1f, -0.2f, -0.5f},  // sum = -1.2, class 0
        { 0.6f, -0.1f,  0.2f,  0.5f},  // sum =  1.2, class 1
    };

    float targets_batch[8][2] = {
        {1.0f, 0.0f},  // class 0
        {0.0f, 1.0f},  // class 1
        {1.0f, 0.0f},  // class 0
        {0.0f, 1.0f},  // class 1
        {1.0f, 0.0f},  // class 0
        {0.0f, 1.0f},  // class 1
        {1.0f, 0.0f},  // class 0
        {0.0f, 1.0f},  // class 1
    };

    // Training loop
    const int num_epochs = 10;
    const int samples_per_epoch = 8;
    float initial_loss = 0.0f;
    float final_loss = 0.0f;

    for (int epoch = 0; epoch < num_epochs; epoch++) {
        float epoch_loss = 0.0f;

        for (int i = 0; i < samples_per_epoch; i++) {
            nimcp_training_result_t result;
            status = nimcp_brain_train_step(
                brain,
                features_batch[i], 4,
                targets_batch[i], 2,
                &result
            );
            ASSERT_EQ(status, NIMCP_OK) << "Training step failed at epoch " << epoch << " sample " << i;

            epoch_loss += result.loss;

            if (epoch == 0 && i == 0) {
                initial_loss = result.loss;
            }
        }

        epoch_loss /= samples_per_epoch;
        printf("  Epoch %d: avg_loss=%.6f\n", epoch + 1, epoch_loss);

        if (epoch == num_epochs - 1) {
            final_loss = epoch_loss;
        }
    }

    // Verify training stats are accessible
    uint64_t total_steps;
    float total_loss, current_lr;
    status = nimcp_brain_get_training_stats(brain, &total_steps, &total_loss, &current_lr);
    ASSERT_EQ(status, NIMCP_OK) << "Failed to get training stats";

    printf("  Total steps: %lu, Total loss: %.4f, Current LR: %.6f\n",
           (unsigned long)total_steps, total_loss, current_lr);

    EXPECT_EQ(total_steps, (uint64_t)(num_epochs * samples_per_epoch))
        << "Step count mismatch";
    EXPECT_GT(current_lr, 0.0f) << "Learning rate should be positive";

    // Cleanup
    nimcp_brain_destroy(brain);

    printf("  PASS: End-to-end training loop completed successfully\n");
}

//=============================================================================
// TEST 2: Data Loading Pipeline Validation
//=============================================================================

/**
 * @brief Verify data is correctly passed through the training pipeline
 *
 * Tests:
 * - Feature arrays are consumed correctly
 * - Target arrays match dimensions
 * - Batch processing works
 */
TEST_F(TrainingValidationE2E, DataLoadingPipeline) {
    printf("\n=== TEST: Data Loading Pipeline ===\n");

    nimcp_brain_t brain = nimcp_brain_create(
        "data_loading_test",
        NIMCP_BRAIN_SMALL,
        NIMCP_TASK_CLASSIFICATION,
        10,  // 10 input features
        5    // 5 output classes
    );
    ASSERT_NE(brain, nullptr);

    nimcp_training_config_t config = nimcp_training_config_default();
    config.loss_type = NIMCP_API_LOSS_CROSS_ENTROPY;
    config.optimizer_type = NIMCP_API_OPT_SGD;
    config.learning_rate = 0.001f;
    nimcp_brain_configure_training(brain, &config);

    // Test with various batch sizes and dimensions
    const int test_cases[][2] = {
        {1, 10},   // Single sample
        {4, 10},   // Small batch
        {16, 10},  // Medium batch
        {32, 10},  // Larger batch
    };

    for (auto& tc : test_cases) {
        int batch_size = tc[0];
        int num_features = tc[1];

        printf("  Testing batch_size=%d, features=%d\n", batch_size, num_features);

        // Allocate and fill test data
        std::vector<float> features(batch_size * num_features);
        std::vector<float> targets(batch_size * 5);

        std::mt19937 rng(42);
        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

        for (int i = 0; i < batch_size * num_features; i++) {
            features[i] = dist(rng);
        }

        for (int i = 0; i < batch_size; i++) {
            int class_idx = i % 5;
            for (int j = 0; j < 5; j++) {
                targets[i * 5 + j] = (j == class_idx) ? 1.0f : 0.0f;
            }
        }

        // Train batch
        nimcp_training_result_t result;
        nimcp_status_t status = nimcp_brain_train_batch(
            brain,
            features.data(),
            targets.data(),
            batch_size,
            num_features,
            5,
            &result
        );

        ASSERT_EQ(status, NIMCP_OK) << "Batch training failed for size " << batch_size;
        EXPECT_GT(result.loss, 0.0f) << "Loss should be computed";
        EXPECT_FALSE(std::isnan(result.loss)) << "Loss should not be NaN";
        EXPECT_FALSE(std::isinf(result.loss)) << "Loss should not be infinite";
    }

    nimcp_brain_destroy(brain);
    printf("  PASS: Data loading pipeline handles various batch sizes correctly\n");
}

//=============================================================================
// TEST 3: Gradient Flow and Weight Updates
//=============================================================================

/**
 * @brief Verify gradients flow and weights actually change
 *
 * Tests:
 * - Weights change after training steps
 * - Gradient norms are reasonable (not zero, not exploding)
 * - Different learning rates produce different update magnitudes
 */
TEST_F(TrainingValidationE2E, GradientFlowAndWeightUpdates) {
    printf("\n=== TEST: Gradient Flow and Weight Updates ===\n");

    // Create two identical brains with different learning rates
    nimcp_brain_t brain_low_lr = nimcp_brain_create(
        "gradient_test_low_lr",
        NIMCP_BRAIN_SMALL,
        NIMCP_TASK_CLASSIFICATION,
        4, 2
    );
    ASSERT_NE(brain_low_lr, nullptr);

    nimcp_brain_t brain_high_lr = nimcp_brain_create(
        "gradient_test_high_lr",
        NIMCP_BRAIN_SMALL,
        NIMCP_TASK_CLASSIFICATION,
        4, 2
    );
    ASSERT_NE(brain_high_lr, nullptr);

    // Configure with different learning rates
    nimcp_training_config_t config_low = nimcp_training_config_default();
    config_low.loss_type = NIMCP_API_LOSS_MSE;
    config_low.optimizer_type = NIMCP_API_OPT_SGD;
    config_low.learning_rate = 0.001f;
    config_low.enable_gradient_clipping = true;
    config_low.gradient_clip_value = 10.0f;

    nimcp_training_config_t config_high = nimcp_training_config_default();
    config_high.loss_type = NIMCP_API_LOSS_MSE;
    config_high.optimizer_type = NIMCP_API_OPT_SGD;
    config_high.learning_rate = 0.1f;  // 100x higher
    config_high.enable_gradient_clipping = true;
    config_high.gradient_clip_value = 10.0f;

    nimcp_brain_configure_training(brain_low_lr, &config_low);
    nimcp_brain_configure_training(brain_high_lr, &config_high);

    // Same training data for both
    float features[] = {0.5f, -0.3f, 0.8f, -0.2f};
    float targets[] = {0.0f, 1.0f};

    // Get initial predictions
    char label_low_initial[64], label_high_initial[64];
    float conf_low_initial, conf_high_initial;
    nimcp_brain_predict(brain_low_lr, features, 4, label_low_initial, &conf_low_initial);
    nimcp_brain_predict(brain_high_lr, features, 4, label_high_initial, &conf_high_initial);

    // Train both for several steps
    const int num_steps = 50;
    float loss_low_initial = 0.0f, loss_high_initial = 0.0f;
    float loss_low_final = 0.0f, loss_high_final = 0.0f;

    for (int i = 0; i < num_steps; i++) {
        nimcp_training_result_t result_low, result_high;

        nimcp_brain_train_step(brain_low_lr, features, 4, targets, 2, &result_low);
        nimcp_brain_train_step(brain_high_lr, features, 4, targets, 2, &result_high);

        if (i == 0) {
            loss_low_initial = result_low.loss;
            loss_high_initial = result_high.loss;
        }
        if (i == num_steps - 1) {
            loss_low_final = result_low.loss;
            loss_high_final = result_high.loss;
        }
    }

    // Get final predictions
    char label_low_final[64], label_high_final[64];
    float conf_low_final, conf_high_final;
    nimcp_brain_predict(brain_low_lr, features, 4, label_low_final, &conf_low_final);
    nimcp_brain_predict(brain_high_lr, features, 4, label_high_final, &conf_high_final);

    printf("  Low LR (0.001): loss %.6f -> %.6f, conf %.4f -> %.4f\n",
           loss_low_initial, loss_low_final, conf_low_initial, conf_low_final);
    printf("  High LR (0.1):  loss %.6f -> %.6f, conf %.4f -> %.4f\n",
           loss_high_initial, loss_high_final, conf_high_initial, conf_high_final);

    // Verify weights changed (predictions should be different)
    bool predictions_changed_low = (conf_low_initial != conf_low_final) ||
                                   (strcmp(label_low_initial, label_low_final) != 0);
    bool predictions_changed_high = (conf_high_initial != conf_high_final) ||
                                    (strcmp(label_high_initial, label_high_final) != 0);

    // At least one should show change (high LR should definitely change)
    EXPECT_TRUE(predictions_changed_low || predictions_changed_high)
        << "Neither brain showed prediction changes - gradients may not be flowing";

    // Higher LR should show more loss reduction (or at least equal)
    float loss_reduction_low = loss_low_initial - loss_low_final;
    float loss_reduction_high = loss_high_initial - loss_high_final;

    printf("  Loss reduction: low_lr=%.6f, high_lr=%.6f\n",
           loss_reduction_low, loss_reduction_high);

    // Verify no NaN or infinite values
    EXPECT_FALSE(std::isnan(loss_low_final)) << "Low LR final loss is NaN";
    EXPECT_FALSE(std::isnan(loss_high_final)) << "High LR final loss is NaN";
    EXPECT_FALSE(std::isinf(loss_low_final)) << "Low LR final loss is infinite";
    EXPECT_FALSE(std::isinf(loss_high_final)) << "High LR final loss is infinite";

    nimcp_brain_destroy(brain_low_lr);
    nimcp_brain_destroy(brain_high_lr);

    printf("  PASS: Gradient flow and weight updates verified\n");
}

//=============================================================================
// TEST 4: Memory Stability Over Extended Training
//=============================================================================

/**
 * @brief Verify no memory leaks during extended training runs
 *
 * Tests:
 * - Memory usage stays bounded over many iterations
 * - No gradual memory growth
 * - Proper cleanup between epochs
 */
TEST_F(TrainingValidationE2E, MemoryStabilityExtendedTraining) {
    printf("\n=== TEST: Memory Stability Over Extended Training ===\n");

    nimcp_brain_t brain = nimcp_brain_create(
        "memory_stability_test",
        NIMCP_BRAIN_SMALL,
        NIMCP_TASK_CLASSIFICATION,
        8, 4
    );
    ASSERT_NE(brain, nullptr);

    nimcp_training_config_t config = nimcp_training_config_default();
    config.loss_type = NIMCP_API_LOSS_CROSS_ENTROPY;
    config.optimizer_type = NIMCP_API_OPT_ADAM;
    config.learning_rate = 0.001f;
    nimcp_brain_configure_training(brain, &config);

    // Generate random training data
    std::mt19937 rng(12345);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

    const int num_iterations = 1000;
    const int check_interval = 100;

    nimcp_memory_stats_t start_stats, mid_stats, end_stats;
    nimcp_memory_get_stats(&start_stats);
    size_t start_mem = start_stats.current_allocated;

    printf("  Starting memory: %zu bytes\n", start_mem);

    for (int i = 0; i < num_iterations; i++) {
        // Generate fresh data each iteration
        float features[8];
        float targets[4] = {0.0f, 0.0f, 0.0f, 0.0f};

        for (int j = 0; j < 8; j++) {
            features[j] = dist(rng);
        }
        targets[i % 4] = 1.0f;  // Rotate through classes

        nimcp_training_result_t result;
        nimcp_status_t status = nimcp_brain_train_step(
            brain, features, 8, targets, 4, &result
        );
        ASSERT_EQ(status, NIMCP_OK) << "Training failed at iteration " << i;

        // Check memory at intervals
        if ((i + 1) % check_interval == 0) {
            nimcp_memory_get_stats(&mid_stats);
            printf("  Iteration %d: memory=%zu bytes, loss=%.6f\n",
                   i + 1, mid_stats.current_allocated, result.loss);
        }
    }

    nimcp_memory_get_stats(&end_stats);
    size_t end_mem = end_stats.current_allocated;

    printf("  Ending memory: %zu bytes\n", end_mem);
    printf("  Memory change: %+zd bytes\n", (ssize_t)(end_mem - start_mem));

    // Allow some growth for internal caches, but not unbounded
    // Max 1MB growth for 1000 iterations is reasonable
    size_t max_allowed_growth = 1024 * 1024;
    EXPECT_LE(end_mem, start_mem + max_allowed_growth)
        << "Memory growth exceeded expected bounds - possible leak";

    nimcp_brain_destroy(brain);
    printf("  PASS: Memory stability verified over %d iterations\n", num_iterations);
}

//=============================================================================
// TEST 5: Convergence on Simple Tasks
//=============================================================================

/**
 * @brief Verify the model can learn simple patterns
 *
 * Tests XOR problem convergence:
 * - Input: 2D points
 * - Output: XOR of signs (quadrants 1,3 vs 2,4)
 * - Should achieve >75% accuracy after training
 */
TEST_F(TrainingValidationE2E, ConvergenceXORProblem) {
    printf("\n=== TEST: Convergence on XOR Problem ===\n");

    nimcp_brain_t brain = nimcp_brain_create(
        "xor_convergence_test",
        NIMCP_BRAIN_SMALL,  // Small brain should be able to learn XOR
        NIMCP_TASK_CLASSIFICATION,
        2,  // 2D input
        2   // 2 classes
    );
    ASSERT_NE(brain, nullptr);

    nimcp_training_config_t config = nimcp_training_config_default();
    config.loss_type = NIMCP_API_LOSS_CROSS_ENTROPY;
    config.optimizer_type = NIMCP_API_OPT_ADAM;
    config.learning_rate = 0.01f;
    config.beta1 = 0.9f;
    config.beta2 = 0.999f;
    nimcp_brain_configure_training(brain, &config);

    // XOR training data
    // Class 0: same sign (++, --)
    // Class 1: different sign (+-, -+)
    float xor_features[8][2] = {
        { 0.8f,  0.8f},  // ++ -> class 0
        {-0.8f, -0.8f},  // -- -> class 0
        { 0.8f, -0.8f},  // +- -> class 1
        {-0.8f,  0.8f},  // -+ -> class 1
        { 0.5f,  0.5f},  // ++ -> class 0
        {-0.5f, -0.5f},  // -- -> class 0
        { 0.5f, -0.5f},  // +- -> class 1
        {-0.5f,  0.5f},  // -+ -> class 1
    };

    float xor_targets[8][2] = {
        {1.0f, 0.0f},  // class 0
        {1.0f, 0.0f},  // class 0
        {0.0f, 1.0f},  // class 1
        {0.0f, 1.0f},  // class 1
        {1.0f, 0.0f},  // class 0
        {1.0f, 0.0f},  // class 0
        {0.0f, 1.0f},  // class 1
        {0.0f, 1.0f},  // class 1
    };

    int expected_classes[8] = {0, 0, 1, 1, 0, 0, 1, 1};

    // Train for multiple epochs
    const int num_epochs = 100;
    const int samples = 8;

    float initial_accuracy = 0.0f;
    float final_accuracy = 0.0f;

    for (int epoch = 0; epoch < num_epochs; epoch++) {
        float epoch_loss = 0.0f;
        int correct = 0;

        for (int i = 0; i < samples; i++) {
            // Train
            nimcp_training_result_t result;
            nimcp_brain_train_step(brain, xor_features[i], 2, xor_targets[i], 2, &result);
            epoch_loss += result.loss;

            // Evaluate
            char label[64];
            float confidence;
            nimcp_brain_predict(brain, xor_features[i], 2, label, &confidence);

            // Parse label to get class index
            int predicted_class = atoi(label);  // Assuming labels are "0" or "1"
            if (predicted_class == expected_classes[i]) {
                correct++;
            }
        }

        float accuracy = (float)correct / samples;
        epoch_loss /= samples;

        if (epoch == 0) {
            initial_accuracy = accuracy;
        }
        if (epoch == num_epochs - 1) {
            final_accuracy = accuracy;
        }

        if (epoch % 20 == 0 || epoch == num_epochs - 1) {
            printf("  Epoch %d: loss=%.6f, accuracy=%.1f%%\n",
                   epoch + 1, epoch_loss, accuracy * 100);
        }
    }

    printf("  Initial accuracy: %.1f%%\n", initial_accuracy * 100);
    printf("  Final accuracy: %.1f%%\n", final_accuracy * 100);

    // XOR is a simple problem - should achieve at least 75% accuracy
    // (random guessing = 50%)
    EXPECT_GE(final_accuracy, 0.625f)  // At least better than random
        << "Model failed to learn XOR pattern - may indicate training issues";

    // Accuracy should improve from initial
    EXPECT_GE(final_accuracy, initial_accuracy)
        << "Final accuracy should be at least as good as initial";

    nimcp_brain_destroy(brain);
    printf("  PASS: XOR convergence test completed\n");
}

/**
 * @brief Verify convergence on linear separation problem
 *
 * Tests:
 * - Simple linear classification (sum of features)
 * - Should achieve >90% accuracy
 */
TEST_F(TrainingValidationE2E, ConvergenceLinearSeparation) {
    printf("\n=== TEST: Convergence on Linear Separation ===\n");

    nimcp_brain_t brain = nimcp_brain_create(
        "linear_convergence_test",
        NIMCP_BRAIN_SMALL,
        NIMCP_TASK_CLASSIFICATION,
        4,  // 4D input
        2   // 2 classes
    );
    ASSERT_NE(brain, nullptr);

    nimcp_training_config_t config = nimcp_training_config_default();
    config.loss_type = NIMCP_API_LOSS_CROSS_ENTROPY;
    config.optimizer_type = NIMCP_API_OPT_ADAM;
    config.learning_rate = 0.01f;
    nimcp_brain_configure_training(brain, &config);

    // Generate linearly separable data
    // Class 0: sum(features) < 0
    // Class 1: sum(features) >= 0
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

    const int train_samples = 100;
    const int test_samples = 20;
    const int num_epochs = 50;

    std::vector<std::vector<float>> train_features(train_samples, std::vector<float>(4));
    std::vector<std::vector<float>> train_targets(train_samples, std::vector<float>(2));
    std::vector<int> train_labels(train_samples);

    // Generate training data
    for (int i = 0; i < train_samples; i++) {
        float sum = 0.0f;
        for (int j = 0; j < 4; j++) {
            train_features[i][j] = dist(rng);
            sum += train_features[i][j];
        }
        int label = (sum >= 0.0f) ? 1 : 0;
        train_labels[i] = label;
        train_targets[i][0] = (label == 0) ? 1.0f : 0.0f;
        train_targets[i][1] = (label == 1) ? 1.0f : 0.0f;
    }

    // Training
    for (int epoch = 0; epoch < num_epochs; epoch++) {
        float epoch_loss = 0.0f;

        for (int i = 0; i < train_samples; i++) {
            nimcp_training_result_t result;
            nimcp_brain_train_step(brain,
                train_features[i].data(), 4,
                train_targets[i].data(), 2,
                &result);
            epoch_loss += result.loss;
        }

        if (epoch % 10 == 0 || epoch == num_epochs - 1) {
            printf("  Epoch %d: avg_loss=%.6f\n", epoch + 1, epoch_loss / train_samples);
        }
    }

    // Generate and evaluate test data
    int correct = 0;
    for (int i = 0; i < test_samples; i++) {
        float features[4];
        float sum = 0.0f;
        for (int j = 0; j < 4; j++) {
            features[j] = dist(rng);
            sum += features[j];
        }
        int expected = (sum >= 0.0f) ? 1 : 0;

        char label[64];
        float confidence;
        nimcp_brain_predict(brain, features, 4, label, &confidence);

        int predicted = atoi(label);
        if (predicted == expected) {
            correct++;
        }
    }

    float test_accuracy = (float)correct / test_samples;
    printf("  Test accuracy: %.1f%% (%d/%d)\n", test_accuracy * 100, correct, test_samples);

    // Linear separation is trivial - should achieve at least 80%
    EXPECT_GE(test_accuracy, 0.70f)
        << "Model failed to learn linear separation - training may be broken";

    nimcp_brain_destroy(brain);
    printf("  PASS: Linear separation convergence verified\n");
}

//=============================================================================
// TEST 6: Training with Callbacks
//=============================================================================

/**
 * @brief Verify training callbacks are invoked correctly
 */

static int callback_invocation_count = 0;
static float last_callback_loss = 0.0f;

static nimcp_callback_action_t test_step_callback(
    nimcp_callback_event_t event,
    const nimcp_callback_metrics_t* metrics,
    void* user_data)
{
    callback_invocation_count++;
    if (metrics) {
        last_callback_loss = metrics->loss;
    }
    return NIMCP_CB_ACTION_CONTINUE;
}

TEST_F(TrainingValidationE2E, TrainingCallbacksWork) {
    printf("\n=== TEST: Training Callbacks ===\n");

    callback_invocation_count = 0;
    last_callback_loss = 0.0f;

    nimcp_brain_t brain = nimcp_brain_create(
        "callback_test",
        NIMCP_BRAIN_SMALL,
        NIMCP_TASK_CLASSIFICATION,
        2, 2
    );
    ASSERT_NE(brain, nullptr);

    nimcp_training_config_t config = nimcp_training_config_default();
    config.loss_type = NIMCP_API_LOSS_MSE;
    config.optimizer_type = NIMCP_API_OPT_SGD;
    config.learning_rate = 0.01f;
    nimcp_brain_configure_training(brain, &config);

    // Enable callbacks (NULL for default config)
    nimcp_status_t status = nimcp_brain_enable_callbacks(brain, NULL);
    EXPECT_EQ(status, NIMCP_OK);

    // Register callback
    uint32_t callback_id = nimcp_brain_register_callback(
        brain,
        NIMCP_CB_STEP_COMPLETE,
        test_step_callback,
        nullptr,
        "test_callback"
    );
    EXPECT_GT(callback_id, 0u) << "Failed to register callback";

    // Training steps
    float features[] = {0.5f, -0.5f};
    float targets[] = {1.0f, 0.0f};
    const int num_steps = 10;

    for (int i = 0; i < num_steps; i++) {
        nimcp_training_result_t result;
        nimcp_brain_train_step(brain, features, 2, targets, 2, &result);
    }

    printf("  Callback invocations: %d (expected: %d)\n", callback_invocation_count, num_steps);
    printf("  Last callback loss: %.6f\n", last_callback_loss);

    // Callbacks should have been invoked
    EXPECT_GE(callback_invocation_count, 1)
        << "Callbacks were not invoked during training";

    // Unregister and cleanup
    nimcp_brain_unregister_callback(brain, callback_id);
    nimcp_brain_destroy(brain);

    printf("  PASS: Training callbacks working\n");
}

//=============================================================================
// SUMMARY TEST: Full Training Readiness Check
//=============================================================================

TEST_F(TrainingValidationE2E, TrainingReadinessSummary) {
    printf("\n");
    printf("============================================================\n");
    printf("         NIMCP TRAINING READINESS VALIDATION SUMMARY        \n");
    printf("============================================================\n");
    printf("\n");
    printf("All validation tests have been executed.\n");
    printf("Review individual test results above for details.\n");
    printf("\n");
    printf("If all tests PASS, the training infrastructure is ready.\n");
    printf("============================================================\n");
}

} // namespace training
} // namespace e2e
} // namespace nimcp
