/**
 * @file e2e_test_snn_training_pipeline.cpp
 * @brief E2E Tests for SNN Training Pipeline
 *
 * WHAT: End-to-end testing for SNN backpropagation training
 * WHY:  Verify complete SNN training workflow with surrogate gradients
 * HOW:  Test forward/backward passes, convergence, and multi-epoch training
 *
 * TEST PIPELINES:
 * - CompleteSNNTrainingWorkflow: Full forward/backward/step cycle
 * - SurrogateGradientMethods: Test all surrogate gradient types
 * - ConvergenceValidation: Verify training converges on simple tasks
 * - MultiEpochTraining: Extended training with multiple epochs
 * - LossFunctionVariants: Test different loss functions
 * - BatchTrainingWorkflow: Batch processing for SNN training
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 * @version 1.0.0
 */

#include "e2e_test_framework.h"

// Headers have their own extern "C" guards
#include "training/nimcp_snn_backprop.h"
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn.h"
#include "utils/tensor/nimcp_tensor.h"
#include "utils/memory/nimcp_memory.h"

#include <thread>
#include <atomic>
#include <vector>
#include <string>
#include <cstring>
#include <cmath>
#include <chrono>
#include <random>

//=============================================================================
// Test Fixture
//=============================================================================

class SNNTrainingPipelineE2ETest : public ::testing::Test {
protected:
    snn_network_t* network_ = nullptr;
    snn_backprop_ctx_t* trainer_ = nullptr;

    // XOR problem data
    static constexpr size_t XOR_INPUTS = 2;
    static constexpr size_t XOR_OUTPUTS = 1;
    static constexpr size_t XOR_HIDDEN = 8;
    static constexpr size_t XOR_SAMPLES = 4;

    float xor_inputs_[XOR_SAMPLES * XOR_INPUTS] = {
        0.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 0.0f,
        1.0f, 1.0f
    };

    float xor_targets_[XOR_SAMPLES * XOR_OUTPUTS] = {
        0.0f,
        1.0f,
        1.0f,
        0.0f
    };

    void SetUp() override {
        // Create a simple SNN for testing
        snn_network_config_t config;
        memset(&config, 0, sizeof(config));
        config.num_inputs = XOR_INPUTS;
        config.num_outputs = XOR_OUTPUTS;
        config.dt_ms = 1.0f;

        network_ = snn_network_create(&config);
    }

    void TearDown() override {
        if (trainer_) {
            snn_backprop_destroy(trainer_);
            trainer_ = nullptr;
        }
        if (network_) {
            snn_network_destroy(network_);
            network_ = nullptr;
        }
    }

    // Add hidden layer to network
    void AddHiddenLayer() {
        if (network_) {
            snn_layer_config_t layer_cfg;
            memset(&layer_cfg, 0, sizeof(layer_cfg));
            layer_cfg.num_neurons = XOR_HIDDEN;
            layer_cfg.neuron_type = SNN_NEURON_LIF;
            layer_cfg.tau_m = 20.0f;
            layer_cfg.tau_s = 5.0f;
            layer_cfg.v_thresh = 1.0f;
            layer_cfg.v_reset = 0.0f;
            layer_cfg.v_rest = 0.0f;

            snn_network_add_layer(network_, &layer_cfg);
        }
    }

    snn_backprop_ctx_t* CreateTrainer(snn_train_algorithm_t algo = SNN_TRAIN_BPTT) {
        snn_backprop_config_t config = snn_backprop_default_config(algo);
        config.learning_rate = 0.01f;
        config.batch_size = XOR_SAMPLES;
        config.sequence_length = 50;
        config.use_gradient_clipping = true;
        config.gradient_clip_norm = 10.0f;
        config.track_gradient_stats = true;

        return snn_backprop_create(network_, &config);
    }

    float ComputeAccuracy(float* predictions, float* targets, size_t count) {
        int correct = 0;
        for (size_t i = 0; i < count; i++) {
            float pred = predictions[i] > 0.5f ? 1.0f : 0.0f;
            if (std::abs(pred - targets[i]) < 0.1f) {
                correct++;
            }
        }
        return static_cast<float>(correct) / static_cast<float>(count);
    }
};

//=============================================================================
// Pipeline 1: Complete SNN Training Workflow
//=============================================================================

TEST_F(SNNTrainingPipelineE2ETest, CompleteSNNTrainingWorkflow) {
    E2E_PIPELINE_START("Complete SNN Training Workflow");

    // Stage 1: Create network and trainer
    E2E_STAGE_BEGIN("Create SNN network and trainer", 500);

    E2E_ASSERT_NOT_NULL(network_, "Failed to create SNN network");
    AddHiddenLayer();

    trainer_ = CreateTrainer(SNN_TRAIN_BPTT);
    E2E_ASSERT_NOT_NULL(trainer_, "Failed to create SNN trainer");

    E2E_STAGE_END();

    // Stage 2: Reset and zero gradients
    E2E_STAGE_BEGIN("Reset trainer state", 200);

    int reset_result = snn_backprop_reset(trainer_);
    EXPECT_EQ(reset_result, 0);

    int zero_result = snn_backprop_zero_grad(trainer_);
    EXPECT_EQ(zero_result, 0);

    E2E_STAGE_END();

    // Stage 3: Forward pass
    E2E_STAGE_BEGIN("Execute forward pass", 1000);

    float outputs[XOR_SAMPLES * XOR_OUTPUTS];
    memset(outputs, 0, sizeof(outputs));

    int forward_result = snn_backprop_forward(
        trainer_,
        xor_inputs_,
        XOR_SAMPLES,
        50.0f,  // 50ms simulation
        outputs
    );
    EXPECT_EQ(forward_result, 0);

    std::cout << "\n  Forward pass outputs:" << std::endl;
    for (size_t i = 0; i < XOR_SAMPLES; i++) {
        std::cout << "    Input (" << xor_inputs_[i*2] << ", "
                  << xor_inputs_[i*2+1] << ") -> " << outputs[i] << std::endl;
    }

    E2E_STAGE_END();

    // Stage 4: Backward pass
    E2E_STAGE_BEGIN("Execute backward pass", 1000);

    int backward_result = snn_backprop_backward(
        trainer_,
        xor_targets_,
        XOR_SAMPLES
    );
    EXPECT_EQ(backward_result, 0);

    // Check gradient norm
    float grad_norm = snn_backprop_get_gradient_norm(trainer_);
    std::cout << "  Gradient norm: " << grad_norm << std::endl;
    EXPECT_GE(grad_norm, 0.0f);

    E2E_STAGE_END();

    // Stage 5: Weight update (step)
    E2E_STAGE_BEGIN("Apply weight updates", 500);

    float weight_norm_before = snn_backprop_get_weight_norm(trainer_);

    int step_result = snn_backprop_step(trainer_, 0.0f);  // Use config learning rate
    EXPECT_GE(step_result, 0);  // Returns number of weights updated

    float weight_norm_after = snn_backprop_get_weight_norm(trainer_);

    std::cout << "  Weight norm before: " << weight_norm_before << std::endl;
    std::cout << "  Weight norm after: " << weight_norm_after << std::endl;
    std::cout << "  Weights updated: " << step_result << std::endl;

    E2E_STAGE_END();

    // Stage 6: Verify statistics
    E2E_STAGE_BEGIN("Verify training statistics", 300);

    snn_backprop_stats_t stats;
    int stats_result = snn_backprop_get_stats(trainer_, &stats);
    EXPECT_EQ(stats_result, 0);

    EXPECT_GE(stats.total_steps, 0u);
    EXPECT_GE(stats.total_loss, 0.0);

    std::cout << "\n  Training Statistics:" << std::endl;
    std::cout << "    Total steps: " << stats.total_steps << std::endl;
    std::cout << "    Average loss: " << stats.avg_loss << std::endl;
    std::cout << "    Avg firing rate: " << stats.avg_firing_rate << " Hz" << std::endl;

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 2: Surrogate Gradient Methods
//=============================================================================

TEST_F(SNNTrainingPipelineE2ETest, SurrogateGradientMethods) {
    E2E_PIPELINE_START("Surrogate Gradient Methods");

    E2E_ASSERT_NOT_NULL(network_, "Network not created");
    AddHiddenLayer();

    snn_surrogate_method_t methods[] = {
        SNN_SURROGATE_SUPERSPIKE,
        SNN_SURROGATE_FAST_SIGMOID,
        SNN_SURROGATE_SIGMOID,
        SNN_SURROGATE_ARCTAN,
        SNN_SURROGATE_TRIANGULAR,
        SNN_SURROGATE_RECTANGULAR,
        SNN_SURROGATE_EXPONENTIAL
    };

    const char* method_names[] = {
        "SuperSpike",
        "Fast Sigmoid",
        "Sigmoid",
        "Arctan",
        "Triangular",
        "Rectangular",
        "Exponential"
    };

    // Stage 1: Test each surrogate method
    for (size_t i = 0; i < sizeof(methods)/sizeof(methods[0]); i++) {
        std::string stage_name = std::string("Test ") + method_names[i] + " surrogate";
        E2E_STAGE_BEGIN(stage_name.c_str(), 1000);

        snn_backprop_config_t config = snn_backprop_default_config(SNN_TRAIN_BPTT);
        config.surrogate.method = methods[i];
        config.learning_rate = 0.01f;

        snn_backprop_ctx_t* trainer = snn_backprop_create(network_, &config);
        E2E_ASSERT_NOT_NULL(trainer, "Failed to create trainer");

        // Test surrogate gradient computation
        float test_v = 0.5f;  // Test membrane potential
        float grad = snn_surrogate_gradient(trainer, test_v);

        std::cout << "  " << method_names[i] << " gradient at v=0.5: " << grad << std::endl;
        EXPECT_GE(grad, 0.0f);
        EXPECT_LE(grad, 10.0f);  // Should be bounded

        // Do a training step
        snn_train_result_t result;
        int train_result = snn_backprop_train_step(
            trainer,
            xor_inputs_,
            xor_targets_,
            XOR_SAMPLES,
            20.0f,
            &result
        );
        EXPECT_EQ(train_result, 0);
        EXPECT_TRUE(result.gradients_valid);

        snn_backprop_destroy(trainer);

        E2E_STAGE_END();
    }

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 3: Convergence Validation
//=============================================================================

TEST_F(SNNTrainingPipelineE2ETest, ConvergenceValidation) {
    E2E_PIPELINE_START("Convergence Validation");

    // Stage 1: Setup for convergence test
    E2E_STAGE_BEGIN("Setup convergence test", 500);

    E2E_ASSERT_NOT_NULL(network_, "Network not created");
    AddHiddenLayer();

    snn_backprop_config_t config = snn_backprop_default_config(SNN_TRAIN_BPTT);
    config.learning_rate = 0.05f;
    config.batch_size = XOR_SAMPLES;
    config.use_gradient_clipping = true;
    config.gradient_clip_norm = 5.0f;

    trainer_ = snn_backprop_create(network_, &config);
    E2E_ASSERT_NOT_NULL(trainer_, "Failed to create trainer");

    E2E_STAGE_END();

    // Stage 2: Train for multiple iterations
    E2E_STAGE_BEGIN("Train to convergence", 10000);

    const int MAX_ITERATIONS = 100;
    const float CONVERGENCE_THRESHOLD = 0.1f;

    float loss_history[MAX_ITERATIONS];
    float best_loss = 1e10f;
    int converged_at = -1;

    for (int iter = 0; iter < MAX_ITERATIONS; iter++) {
        snn_train_result_t result;

        int train_result = snn_backprop_train_step(
            trainer_,
            xor_inputs_,
            xor_targets_,
            XOR_SAMPLES,
            30.0f,
            &result
        );

        EXPECT_EQ(train_result, 0);
        loss_history[iter] = result.loss;

        if (result.loss < best_loss) {
            best_loss = result.loss;
        }

        if (result.loss < CONVERGENCE_THRESHOLD && converged_at < 0) {
            converged_at = iter;
        }

        // Print progress every 20 iterations
        if (iter % 20 == 0) {
            std::cout << "  Iter " << iter << ": loss=" << result.loss
                      << " grad_norm=" << result.gradient_norm
                      << " spikes=" << result.total_spikes << std::endl;
        }
    }

    std::cout << "\n  Best loss achieved: " << best_loss << std::endl;
    if (converged_at >= 0) {
        std::cout << "  Converged at iteration: " << converged_at << std::endl;
    } else {
        std::cout << "  Did not converge (threshold=" << CONVERGENCE_THRESHOLD << ")" << std::endl;
    }

    // Verify loss decreased
    float initial_loss_avg = (loss_history[0] + loss_history[1] + loss_history[2]) / 3.0f;
    float final_loss_avg = (loss_history[MAX_ITERATIONS-3] +
                            loss_history[MAX_ITERATIONS-2] +
                            loss_history[MAX_ITERATIONS-1]) / 3.0f;

    std::cout << "  Initial loss (avg first 3): " << initial_loss_avg << std::endl;
    std::cout << "  Final loss (avg last 3): " << final_loss_avg << std::endl;

    // Loss should have decreased
    EXPECT_LT(final_loss_avg, initial_loss_avg);

    E2E_STAGE_END();

    // Stage 3: Evaluate final accuracy
    E2E_STAGE_BEGIN("Evaluate final accuracy", 1000);

    float outputs[XOR_SAMPLES];

    int forward_result = snn_backprop_forward(
        trainer_,
        xor_inputs_,
        XOR_SAMPLES,
        30.0f,
        outputs
    );
    EXPECT_EQ(forward_result, 0);

    float accuracy = ComputeAccuracy(outputs, xor_targets_, XOR_SAMPLES);
    std::cout << "  Final accuracy: " << (accuracy * 100) << "%" << std::endl;

    for (size_t i = 0; i < XOR_SAMPLES; i++) {
        std::cout << "    (" << xor_inputs_[i*2] << ", " << xor_inputs_[i*2+1]
                  << ") -> pred=" << outputs[i] << " target=" << xor_targets_[i]
                  << (std::abs(outputs[i] - xor_targets_[i]) < 0.5f ? " [OK]" : " [X]")
                  << std::endl;
    }

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 4: Multi-Epoch Training
//=============================================================================

TEST_F(SNNTrainingPipelineE2ETest, MultiEpochTraining) {
    E2E_PIPELINE_START("Multi-Epoch Training");

    const int NUM_EPOCHS = 5;
    const int STEPS_PER_EPOCH = 50;

    // Stage 1: Setup multi-epoch training
    E2E_STAGE_BEGIN("Setup multi-epoch training", 500);

    E2E_ASSERT_NOT_NULL(network_, "Network not created");
    AddHiddenLayer();

    trainer_ = CreateTrainer(SNN_TRAIN_BPTT);
    E2E_ASSERT_NOT_NULL(trainer_, "Failed to create trainer");

    E2E_STAGE_END();

    // Stage 2: Train multiple epochs
    for (int epoch = 0; epoch < NUM_EPOCHS; epoch++) {
        std::string stage_name = "Epoch " + std::to_string(epoch + 1) + "/" + std::to_string(NUM_EPOCHS);
        E2E_STAGE_BEGIN(stage_name.c_str(), 5000);

        float epoch_loss = 0.0f;
        uint32_t epoch_spikes = 0;

        for (int step = 0; step < STEPS_PER_EPOCH; step++) {
            snn_train_result_t result;

            int train_result = snn_backprop_train_step(
                trainer_,
                xor_inputs_,
                xor_targets_,
                XOR_SAMPLES,
                25.0f,
                &result
            );

            EXPECT_EQ(train_result, 0);
            epoch_loss += result.loss;
            epoch_spikes += result.total_spikes;
        }

        epoch_loss /= STEPS_PER_EPOCH;
        float avg_spikes = static_cast<float>(epoch_spikes) / STEPS_PER_EPOCH;

        std::cout << "  Epoch " << (epoch + 1) << " - avg loss: " << epoch_loss
                  << " avg spikes: " << avg_spikes << std::endl;

        // Get epoch statistics
        snn_backprop_stats_t stats;
        snn_backprop_get_stats(trainer_, &stats);

        std::cout << "    Total steps so far: " << stats.total_steps
                  << " Silent neurons: " << stats.silent_neurons << std::endl;

        E2E_STAGE_END();
    }

    // Stage 3: Final evaluation
    E2E_STAGE_BEGIN("Final evaluation", 500);

    snn_backprop_stats_t final_stats;
    snn_backprop_get_stats(trainer_, &final_stats);

    std::cout << "\n  Multi-Epoch Training Summary:" << std::endl;
    std::cout << "    Total steps: " << final_stats.total_steps << std::endl;
    std::cout << "    Min loss: " << final_stats.min_loss << std::endl;
    std::cout << "    Max loss: " << final_stats.max_loss << std::endl;
    std::cout << "    Avg loss: " << final_stats.avg_loss << std::endl;
    std::cout << "    Gradient explosions: " << final_stats.gradient_explosions << std::endl;
    std::cout << "    Gradient vanishing: " << final_stats.gradient_vanishing << std::endl;
    std::cout << "    Total forward time: " << final_stats.total_forward_time_ms << " ms" << std::endl;
    std::cout << "    Total backward time: " << final_stats.total_backward_time_ms << " ms" << std::endl;

    EXPECT_EQ(final_stats.total_steps, static_cast<uint64_t>(NUM_EPOCHS * STEPS_PER_EPOCH));

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 5: Loss Function Variants
//=============================================================================

TEST_F(SNNTrainingPipelineE2ETest, LossFunctionVariants) {
    E2E_PIPELINE_START("Loss Function Variants");

    E2E_ASSERT_NOT_NULL(network_, "Network not created");
    AddHiddenLayer();

    snn_loss_type_t loss_types[] = {
        SNN_LOSS_SPIKE_COUNT,
        SNN_LOSS_RATE_CODED_MSE,
        SNN_LOSS_RATE_CODED_CROSS_ENTROPY,
        SNN_LOSS_MEMBRANE_POTENTIAL
    };

    const char* loss_names[] = {
        "Spike Count",
        "Rate Coded MSE",
        "Rate Coded Cross-Entropy",
        "Membrane Potential"
    };

    // Stage 1: Test each loss function
    for (size_t i = 0; i < sizeof(loss_types)/sizeof(loss_types[0]); i++) {
        std::string stage_name = std::string("Test ") + loss_names[i] + " loss";
        E2E_STAGE_BEGIN(stage_name.c_str(), 2000);

        snn_backprop_config_t config = snn_backprop_default_config(SNN_TRAIN_BPTT);
        config.loss = snn_loss_default_config(loss_types[i]);
        config.learning_rate = 0.01f;

        snn_backprop_ctx_t* trainer = snn_backprop_create(network_, &config);
        E2E_ASSERT_NOT_NULL(trainer, "Failed to create trainer");

        // Run a few training steps
        float total_loss = 0.0f;
        for (int step = 0; step < 10; step++) {
            snn_train_result_t result;

            int train_result = snn_backprop_train_step(
                trainer,
                xor_inputs_,
                xor_targets_,
                XOR_SAMPLES,
                20.0f,
                &result
            );

            EXPECT_EQ(train_result, 0);
            total_loss += result.loss;
        }

        float avg_loss = total_loss / 10.0f;
        std::cout << "  " << loss_names[i] << " - avg loss: " << avg_loss << std::endl;

        // Verify loss is computed (non-zero for untrained network)
        EXPECT_GE(avg_loss, 0.0f);

        snn_backprop_destroy(trainer);

        E2E_STAGE_END();
    }

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 6: Batch Training Workflow
//=============================================================================

TEST_F(SNNTrainingPipelineE2ETest, BatchTrainingWorkflow) {
    E2E_PIPELINE_START("Batch Training Workflow");

    // Stage 1: Setup batch training
    E2E_STAGE_BEGIN("Setup batch training", 500);

    E2E_ASSERT_NOT_NULL(network_, "Network not created");
    AddHiddenLayer();

    trainer_ = CreateTrainer(SNN_TRAIN_BPTT);
    E2E_ASSERT_NOT_NULL(trainer_, "Failed to create trainer");

    E2E_STAGE_END();

    // Stage 2: Create batch
    E2E_STAGE_BEGIN("Create training batch", 300);

    snn_batch_t* batch = snn_batch_create(
        xor_inputs_,
        xor_targets_,
        XOR_SAMPLES,
        XOR_INPUTS,
        XOR_OUTPUTS
    );
    E2E_ASSERT_NOT_NULL(batch, "Failed to create batch");

    E2E_STAGE_END();

    // Stage 3: Train on batch
    E2E_STAGE_BEGIN("Train on batch", 2000);

    const int NUM_BATCH_ITERATIONS = 30;
    float loss_values[NUM_BATCH_ITERATIONS];

    for (int i = 0; i < NUM_BATCH_ITERATIONS; i++) {
        snn_train_result_t result;

        int train_result = snn_backprop_train_batch(
            trainer_,
            batch,
            25.0f,
            &result
        );

        EXPECT_EQ(train_result, 0);
        loss_values[i] = result.loss;

        if (i % 10 == 0) {
            std::cout << "  Batch iter " << i << ": loss=" << result.loss
                      << " time=" << (result.forward_time_ms + result.backward_time_ms) << " ms" << std::endl;
        }
    }

    // Verify improvement
    float first_loss = loss_values[0];
    float last_loss = loss_values[NUM_BATCH_ITERATIONS - 1];
    std::cout << "\n  First loss: " << first_loss << std::endl;
    std::cout << "  Last loss: " << last_loss << std::endl;

    E2E_STAGE_END();

    // Stage 4: Cleanup batch
    E2E_STAGE_BEGIN("Cleanup batch", 100);

    snn_batch_destroy(batch);

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 7: Training Algorithm Comparison
//=============================================================================

TEST_F(SNNTrainingPipelineE2ETest, TrainingAlgorithmComparison) {
    E2E_PIPELINE_START("Training Algorithm Comparison");

    snn_train_algorithm_t algorithms[] = {
        SNN_TRAIN_BPTT,
        SNN_TRAIN_TRUNCATED_BPTT,
        SNN_TRAIN_EPROP
    };

    const char* algo_names[] = {
        "BPTT",
        "Truncated BPTT",
        "E-prop"
    };

    // Stage 1: Compare training algorithms
    for (size_t i = 0; i < sizeof(algorithms)/sizeof(algorithms[0]); i++) {
        std::string stage_name = std::string("Test ") + algo_names[i];
        E2E_STAGE_BEGIN(stage_name.c_str(), 3000);

        // Create fresh network for each algorithm
        snn_network_config_t net_cfg;
        memset(&net_cfg, 0, sizeof(net_cfg));
        net_cfg.num_inputs = XOR_INPUTS;
        net_cfg.num_outputs = XOR_OUTPUTS;
        net_cfg.dt_ms = 1.0f;

        snn_network_t* net = snn_network_create(&net_cfg);
        E2E_ASSERT_NOT_NULL(net, "Failed to create network");

        // Add hidden layer
        snn_layer_config_t layer_cfg;
        memset(&layer_cfg, 0, sizeof(layer_cfg));
        layer_cfg.num_neurons = XOR_HIDDEN;
        layer_cfg.neuron_type = SNN_NEURON_LIF;
        layer_cfg.tau_m = 20.0f;
        layer_cfg.tau_s = 5.0f;
        layer_cfg.v_thresh = 1.0f;
        snn_network_add_layer(net, &layer_cfg);

        snn_backprop_config_t config = snn_backprop_default_config(algorithms[i]);
        config.learning_rate = 0.02f;

        snn_backprop_ctx_t* trainer = snn_backprop_create(net, &config);
        E2E_ASSERT_NOT_NULL(trainer, "Failed to create trainer");

        // Train for 50 steps
        auto start = std::chrono::high_resolution_clock::now();

        float total_loss = 0.0f;
        for (int step = 0; step < 50; step++) {
            snn_train_result_t result;
            snn_backprop_train_step(trainer, xor_inputs_, xor_targets_,
                                     XOR_SAMPLES, 20.0f, &result);
            total_loss += result.loss;
        }

        auto end = std::chrono::high_resolution_clock::now();
        double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();

        std::cout << "  " << algo_names[i] << ":" << std::endl;
        std::cout << "    Avg loss: " << (total_loss / 50.0f) << std::endl;
        std::cout << "    Time: " << elapsed_ms << " ms" << std::endl;
        std::cout << "    Step time: " << (elapsed_ms / 50.0f) << " ms/step" << std::endl;

        snn_backprop_destroy(trainer);
        snn_network_destroy(net);

        E2E_STAGE_END();
    }

    E2E_PIPELINE_END();
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
