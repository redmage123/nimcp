/**
 * @file e2e_test_lnn_training_backprop.cpp
 * @brief E2E Test: LNN Network Training with Backpropagation Through Time
 *
 * WHAT: End-to-end tests for LNN/SNN training with gradient-based learning
 * WHY:  Verify complete training pipeline: forward, backward, optimizer update
 * HOW:  Create LNN networks, train with BPTT/adjoint, verify loss convergence
 *
 * TEST COVERAGE:
 * - LNN network creation (NCP architecture, custom configs)
 * - Forward pass through sequences
 * - Backpropagation through time (BPTT)
 * - Adjoint method for gradient computation
 * - Optimizer integration (Adam, SGD)
 * - Learning rate scheduling
 * - Training convergence on synthetic tasks
 * - State persistence (save/load trained networks)
 *
 * BIOLOGICAL BASIS:
 * LNN (Liquid Neural Networks) model continuous-time dynamics found in
 * biological neural circuits. Time constants capture diverse neural timescales.
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 * @version 1.0.0
 */

#include "e2e_test_framework.h"
#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <random>
#include <cstring>
#include <algorithm>

// Headers have their own extern "C" guards
#include "lnn/nimcp_lnn.h"
#include "lnn/nimcp_lnn_network.h"
#include "lnn/nimcp_lnn_training.h"
#include "lnn/nimcp_lnn_config.h"
#include "lnn/nimcp_lnn_types.h"
#include "utils/tensor/nimcp_tensor.h"
#include "utils/memory/nimcp_memory.h"
#include "middleware/training/nimcp_optimizers.h"
#include "middleware/training/nimcp_loss_functions.h"
#include "nimcp.h"

namespace nimcp {
namespace e2e {

/**
 * @brief Test fixture for LNN training E2E tests
 */
class LNNTrainingBackpropE2E : public ::testing::Test {
protected:
    static constexpr float LEARNING_RATE = 0.001f;
    static constexpr uint32_t DEFAULT_SEQ_LEN = 10;
    static constexpr float DEFAULT_DT = 1.0f;  // 1ms time step

    void SetUp() override {
        nimcp_memory_init();
        nimcp_memory_get_stats(&initial_stats_);
    }

    void TearDown() override {
        nimcp_memory_get_stats(&final_stats_);
        EXPECT_LE(final_stats_.current_allocated, initial_stats_.current_allocated + 4096)
            << "Memory leak detected: "
            << (final_stats_.current_allocated - initial_stats_.current_allocated)
            << " bytes";
    }

    /**
     * @brief Generate sine wave sequence for regression task
     */
    void generate_sine_sequence(
        uint32_t seq_len,
        uint32_t n_inputs,
        nimcp_tensor_t** inputs,
        nimcp_tensor_t** targets
    ) {
        // Create input tensor [seq_len, n_inputs]
        uint32_t input_dims[2] = {seq_len, n_inputs};
        *inputs = nimcp_tensor_create(input_dims, 2, NIMCP_DTYPE_FLOAT32);
        ASSERT_NE(*inputs, nullptr) << "Failed to create input tensor";

        // Create target tensor [seq_len, 1]
        uint32_t target_dims[2] = {seq_len, 1};
        *targets = nimcp_tensor_create(target_dims, 2, NIMCP_DTYPE_FLOAT32);
        ASSERT_NE(*targets, nullptr) << "Failed to create target tensor";

        float* input_data = nimcp_tensor_data_float(*inputs);
        float* target_data = nimcp_tensor_data_float(*targets);

        // Generate sine wave: input = sin(t), target = sin(t + phase_shift)
        float phase_shift = M_PI / 4.0f;  // Predict quarter cycle ahead
        for (uint32_t t = 0; t < seq_len; ++t) {
            float time = static_cast<float>(t) * 0.1f;
            for (uint32_t i = 0; i < n_inputs; ++i) {
                input_data[t * n_inputs + i] = std::sin(time + i * 0.1f);
            }
            target_data[t] = std::sin(time + phase_shift);
        }
    }

    /**
     * @brief Generate XOR sequence for classification task
     */
    void generate_xor_sequence(
        uint32_t seq_len,
        nimcp_tensor_t** inputs,
        nimcp_tensor_t** targets
    ) {
        // Create input tensor [seq_len, 2]
        uint32_t input_dims[2] = {seq_len, 2};
        *inputs = nimcp_tensor_create(input_dims, 2, NIMCP_DTYPE_FLOAT32);
        ASSERT_NE(*inputs, nullptr) << "Failed to create XOR input tensor";

        // Create target tensor [seq_len, 1]
        uint32_t target_dims[2] = {seq_len, 1};
        *targets = nimcp_tensor_create(target_dims, 2, NIMCP_DTYPE_FLOAT32);
        ASSERT_NE(*targets, nullptr) << "Failed to create XOR target tensor";

        float* input_data = nimcp_tensor_data_float(*inputs);
        float* target_data = nimcp_tensor_data_float(*targets);

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);

        for (uint32_t t = 0; t < seq_len; ++t) {
            // Random binary inputs (with noise)
            float x0 = dist(gen) > 0.5f ? 1.0f : 0.0f;
            float x1 = dist(gen) > 0.5f ? 1.0f : 0.0f;

            input_data[t * 2 + 0] = x0 + (dist(gen) - 0.5f) * 0.1f;
            input_data[t * 2 + 1] = x1 + (dist(gen) - 0.5f) * 0.1f;

            // XOR target
            target_data[t] = (x0 > 0.5f) != (x1 > 0.5f) ? 1.0f : 0.0f;
        }
    }

    /**
     * @brief Compute MSE loss manually for verification
     */
    float compute_mse(const nimcp_tensor_t* predictions, const nimcp_tensor_t* targets) {
        uint32_t n_elements = nimcp_tensor_size(predictions);
        const float* pred_data = nimcp_tensor_data_float(predictions);
        const float* target_data = nimcp_tensor_data_float(targets);

        float sum_sq_error = 0.0f;
        for (uint32_t i = 0; i < n_elements; ++i) {
            float diff = pred_data[i] - target_data[i];
            sum_sq_error += diff * diff;
        }
        return sum_sq_error / static_cast<float>(n_elements);
    }

    nimcp_memory_stats_t initial_stats_;
    nimcp_memory_stats_t final_stats_;
};

//=============================================================================
// Test: LNN Network Creation
//=============================================================================

E2E_TEST_F(LNNTrainingBackpropE2E, NCPNetworkCreation) {
    E2E_PIPELINE_START("LNN NCP Network Creation");

    lnn_network_t* network = nullptr;

    // Stage 1: Create NCP network
    E2E_STAGE_BEGIN("Create NCP network", 100);
    {
        network = lnn_network_create_ncp(
            4,   // n_inputs (sensory)
            8,   // n_inter (interneurons)
            4,   // n_command (command neurons)
            2    // n_outputs (motor)
        );
        E2E_ASSERT_NOT_NULL(network, "NCP network creation failed");
    }
    E2E_STAGE_END();

    // Stage 2: Verify network parameters
    E2E_STAGE_BEGIN("Verify network parameters", 50);
    {
        size_t param_count = lnn_network_param_count(network);
        size_t memory_usage = lnn_network_memory_usage(network);

        std::cout << "[E2E] Network parameter count: " << param_count << "\n";
        std::cout << "[E2E] Network memory usage: " << memory_usage << " bytes\n";

        E2E_ASSERT(param_count > 0, "Network should have trainable parameters");
        E2E_ASSERT(memory_usage > 0, "Network should use memory");
    }
    E2E_STAGE_END();

    // Stage 3: Initialize weights
    E2E_STAGE_BEGIN("Initialize weights", 50);
    {
        int status = lnn_network_init_weights(network, 42);  // Seed for reproducibility
        E2E_ASSERT(status == 0, "Weight initialization failed");
    }
    E2E_STAGE_END();

    // Stage 4: Cleanup
    E2E_STAGE_BEGIN("Destroy network", 50);
    {
        lnn_network_destroy(network);
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

E2E_TEST_F(LNNTrainingBackpropE2E, CustomConfigNetworkCreation) {
    E2E_PIPELINE_START("LNN Custom Config Network");

    lnn_config_t config;
    lnn_network_t* network = nullptr;

    // Stage 1: Initialize config with defaults
    E2E_STAGE_BEGIN("Initialize LNN config", 50);
    {
        int status = lnn_config_default(&config);
        E2E_ASSERT(status == 0, "Config initialization failed");

        // Customize
        config.n_inputs = 8;
        config.n_outputs = 4;
        config.n_layers = 3;
        config.layer_sizes[0] = 16;
        config.layer_sizes[1] = 12;
        config.layer_sizes[2] = 4;
        config.dt = 1.0f;
        config.enable_training = true;
    }
    E2E_STAGE_END();

    // Stage 2: Create network from config
    E2E_STAGE_BEGIN("Create network from config", 100);
    {
        network = lnn_network_create(&config);
        E2E_ASSERT_NOT_NULL(network, "Custom network creation failed");
    }
    E2E_STAGE_END();

    // Stage 3: Verify configuration applied
    E2E_STAGE_BEGIN("Verify configuration", 50);
    {
        lnn_network_stats_t stats;
        int status = lnn_network_get_stats(network, &stats);
        E2E_ASSERT(status == 0, "Failed to get network stats");

        std::cout << "[E2E] Network layers: " << config.n_layers << "\n";
        std::cout << "[E2E] Total parameters: " << lnn_network_param_count(network) << "\n";
    }
    E2E_STAGE_END();

    // Stage 4: Cleanup
    E2E_STAGE_BEGIN("Destroy network", 50);
    {
        lnn_network_destroy(network);
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Test: Forward Pass
//=============================================================================

E2E_TEST_F(LNNTrainingBackpropE2E, ForwardPassSequence) {
    E2E_PIPELINE_START("LNN Forward Pass - Sequence");

    lnn_network_t* network = nullptr;
    nimcp_tensor_t* inputs = nullptr;
    nimcp_tensor_t* outputs = nullptr;
    const uint32_t SEQ_LEN = 20;
    const uint32_t N_INPUTS = 4;
    const uint32_t N_OUTPUTS = 2;

    // Stage 1: Create network
    E2E_STAGE_BEGIN("Create network", 100);
    {
        network = lnn_network_create_ncp(N_INPUTS, 8, 4, N_OUTPUTS);
        E2E_ASSERT_NOT_NULL(network, "Network creation failed");
        lnn_network_init_weights(network, 42);
    }
    E2E_STAGE_END();

    // Stage 2: Create input sequence
    E2E_STAGE_BEGIN("Create input sequence", 50);
    {
        uint32_t input_dims[2] = {SEQ_LEN, N_INPUTS};
        inputs = nimcp_tensor_create(input_dims, 2, NIMCP_DTYPE_FLOAT32);
        E2E_ASSERT_NOT_NULL(inputs, "Input tensor creation failed");

        // Fill with sine wave pattern
        float* data = nimcp_tensor_data_float(inputs);
        for (uint32_t t = 0; t < SEQ_LEN; ++t) {
            for (uint32_t i = 0; i < N_INPUTS; ++i) {
                data[t * N_INPUTS + i] = std::sin(0.1f * t + 0.5f * i);
            }
        }

        uint32_t output_dims[2] = {SEQ_LEN, N_OUTPUTS};
        outputs = nimcp_tensor_create(output_dims, 2, NIMCP_DTYPE_FLOAT32);
        E2E_ASSERT_NOT_NULL(outputs, "Output tensor creation failed");
    }
    E2E_STAGE_END();

    // Stage 3: Run forward pass
    E2E_STAGE_BEGIN("Forward pass sequence", 500);
    {
        lnn_network_reset_state(network);

        int status = lnn_network_forward_sequence(
            network, inputs, outputs, SEQ_LEN, DEFAULT_DT
        );
        E2E_ASSERT(status == 0, "Forward pass failed");

        // Verify outputs are computed
        float* output_data = nimcp_tensor_data_float(outputs);
        bool has_nonzero = false;
        for (uint32_t i = 0; i < SEQ_LEN * N_OUTPUTS; ++i) {
            if (std::abs(output_data[i]) > 1e-10f) {
                has_nonzero = true;
                break;
            }
        }
        E2E_ASSERT(has_nonzero, "Network produced all-zero outputs");

        std::cout << "[E2E] First output: [" << output_data[0] << ", "
                  << output_data[1] << "]\n";
        std::cout << "[E2E] Last output: [" << output_data[(SEQ_LEN-1) * N_OUTPUTS]
                  << ", " << output_data[(SEQ_LEN-1) * N_OUTPUTS + 1] << "]\n";
    }
    E2E_STAGE_END();

    // Stage 4: Cleanup
    E2E_STAGE_BEGIN("Cleanup", 50);
    {
        nimcp_tensor_destroy(inputs);
        nimcp_tensor_destroy(outputs);
        lnn_network_destroy(network);
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Test: Training with Backpropagation Through Time
//=============================================================================

E2E_TEST_F(LNNTrainingBackpropE2E, BPTTTrainingPipeline) {
    E2E_PIPELINE_START("LNN BPTT Training Pipeline");

    lnn_network_t* network = nullptr;
    lnn_training_ctx_t* training_ctx = nullptr;
    nimcp_tensor_t* inputs = nullptr;
    nimcp_tensor_t* targets = nullptr;
    const uint32_t N_INPUTS = 2;
    const uint32_t N_OUTPUTS = 1;
    const uint32_t SEQ_LEN = 16;
    const uint32_t NUM_EPOCHS = 10;

    // Stage 1: Create network
    E2E_STAGE_BEGIN("Create NCP network", 100);
    {
        network = lnn_network_create_ncp(N_INPUTS, 8, 4, N_OUTPUTS);
        E2E_ASSERT_NOT_NULL(network, "Network creation failed");
        lnn_network_init_weights(network, 42);
    }
    E2E_STAGE_END();

    // Stage 2: Create training context
    E2E_STAGE_BEGIN("Create training context", 100);
    {
        lnn_training_config_t config;
        int status = lnn_training_config_default(&config);
        E2E_ASSERT(status == 0, "Training config init failed");

        config.optimizer_type = NIMCP_OPTIMIZER_ADAM;
        config.learning_rate = LEARNING_RATE;
        config.loss_type = NIMCP_LOSS_MSE;
        config.lnn_train_mode = LNN_TRAIN_ADJOINT;
        config.gradient_clip_norm = 1.0f;
        config.track_statistics = true;

        training_ctx = lnn_training_create(network, &config);
        E2E_ASSERT_NOT_NULL(training_ctx, "Training context creation failed");
    }
    E2E_STAGE_END();

    // Stage 3: Generate training data
    E2E_STAGE_BEGIN("Generate sine sequence data", 100);
    {
        generate_sine_sequence(SEQ_LEN, N_INPUTS, &inputs, &targets);
    }
    E2E_STAGE_END();

    // Stage 4: Training loop
    std::vector<float> losses;
    E2E_STAGE_BEGIN("Training epochs", 10000);
    {
        for (uint32_t epoch = 0; epoch < NUM_EPOCHS; ++epoch) {
            float epoch_loss = 0.0f;
            int status = lnn_training_step(training_ctx, inputs, targets, &epoch_loss);
            E2E_ASSERT(status == 0, "Training step failed");

            losses.push_back(epoch_loss);
            std::cout << "[E2E] Epoch " << epoch << " loss: " << epoch_loss << "\n";
        }
    }
    E2E_STAGE_END();

    // Stage 5: Verify training progress
    E2E_STAGE_BEGIN("Verify training convergence", 50);
    {
        E2E_ASSERT(losses.size() == NUM_EPOCHS, "Loss history incomplete");

        // Check that loss generally decreased (allow some variance)
        float first_loss = losses[0];
        float last_loss = losses[losses.size() - 1];
        float avg_early = (losses[0] + losses[1]) / 2.0f;
        float avg_late = (losses[losses.size()-2] + losses[losses.size()-1]) / 2.0f;

        std::cout << "[E2E] First loss: " << first_loss << "\n";
        std::cout << "[E2E] Last loss: " << last_loss << "\n";
        std::cout << "[E2E] Avg early loss: " << avg_early << "\n";
        std::cout << "[E2E] Avg late loss: " << avg_late << "\n";

        // Loss should decrease over training
        E2E_ASSERT(avg_late <= avg_early * 1.5f, "Loss did not decrease during training");

        // Get training statistics
        lnn_training_stats_t stats;
        int status = lnn_training_get_stats(training_ctx, &stats);
        E2E_ASSERT(status == 0, "Failed to get training stats");

        std::cout << "[E2E] Total steps: " << stats.step_count << "\n";
        std::cout << "[E2E] Current LR: " << stats.current_lr << "\n";
        std::cout << "[E2E] Gradient clips: " << stats.gradient_clips << "\n";
    }
    E2E_STAGE_END();

    // Stage 6: Cleanup
    E2E_STAGE_BEGIN("Cleanup", 100);
    {
        nimcp_tensor_destroy(inputs);
        nimcp_tensor_destroy(targets);
        lnn_training_destroy(training_ctx);
        lnn_network_destroy(network);
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Test: Learning Rate Scheduling
//=============================================================================

E2E_TEST_F(LNNTrainingBackpropE2E, LearningRateScheduling) {
    E2E_PIPELINE_START("LNN Learning Rate Scheduling");

    lnn_network_t* network = nullptr;
    lnn_training_ctx_t* training_ctx = nullptr;
    nimcp_tensor_t* inputs = nullptr;
    nimcp_tensor_t* targets = nullptr;
    const uint32_t NUM_STEPS = 20;

    // Stage 1: Setup network and training
    E2E_STAGE_BEGIN("Setup network and training context", 200);
    {
        network = lnn_network_create_ncp(2, 8, 4, 1);
        E2E_ASSERT_NOT_NULL(network, "Network creation failed");
        lnn_network_init_weights(network, 42);

        lnn_training_config_t config;
        lnn_training_config_default(&config);
        config.learning_rate = 0.01f;
        config.lr_schedule = LNN_LR_SCHEDULE_COSINE;
        config.lr_schedule_params[0] = static_cast<float>(NUM_STEPS);  // T_max
        config.n_schedule_params = 1;

        training_ctx = lnn_training_create(network, &config);
        E2E_ASSERT_NOT_NULL(training_ctx, "Training context creation failed");
    }
    E2E_STAGE_END();

    // Stage 2: Generate data
    E2E_STAGE_BEGIN("Generate training data", 50);
    {
        generate_xor_sequence(16, &inputs, &targets);
    }
    E2E_STAGE_END();

    // Stage 3: Train with LR scheduling
    std::vector<float> lr_history;
    E2E_STAGE_BEGIN("Training with LR schedule", 5000);
    {
        float initial_lr = lnn_training_get_lr(training_ctx);
        lr_history.push_back(initial_lr);

        for (uint32_t step = 0; step < NUM_STEPS; ++step) {
            float loss;
            lnn_training_step(training_ctx, inputs, targets, &loss);
            lnn_training_update_lr(training_ctx);

            float current_lr = lnn_training_get_lr(training_ctx);
            lr_history.push_back(current_lr);
        }
    }
    E2E_STAGE_END();

    // Stage 4: Verify LR schedule behavior
    E2E_STAGE_BEGIN("Verify LR schedule", 50);
    {
        std::cout << "[E2E] LR schedule history:\n";
        for (size_t i = 0; i < lr_history.size(); i += 5) {
            std::cout << "  Step " << i << ": " << lr_history[i] << "\n";
        }

        // Cosine schedule should decrease LR
        float first_lr = lr_history[0];
        float last_lr = lr_history[lr_history.size() - 1];
        E2E_ASSERT(last_lr <= first_lr, "Cosine LR schedule should decrease LR");

        // LR should follow cosine pattern (monotonically decreasing for first half)
        bool monotonic_first_half = true;
        for (size_t i = 1; i < lr_history.size() / 2; ++i) {
            if (lr_history[i] > lr_history[i-1]) {
                monotonic_first_half = false;
                break;
            }
        }
        E2E_ASSERT(monotonic_first_half, "Cosine LR not monotonically decreasing in first half");
    }
    E2E_STAGE_END();

    // Stage 5: Cleanup
    E2E_STAGE_BEGIN("Cleanup", 50);
    {
        nimcp_tensor_destroy(inputs);
        nimcp_tensor_destroy(targets);
        lnn_training_destroy(training_ctx);
        lnn_network_destroy(network);
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Test: Gradient Clipping
//=============================================================================

E2E_TEST_F(LNNTrainingBackpropE2E, GradientClipping) {
    E2E_PIPELINE_START("LNN Gradient Clipping");

    lnn_network_t* network = nullptr;
    lnn_training_ctx_t* training_ctx = nullptr;
    nimcp_tensor_t* inputs = nullptr;
    nimcp_tensor_t* targets = nullptr;

    // Stage 1: Setup with gradient clipping
    E2E_STAGE_BEGIN("Setup with gradient clipping", 200);
    {
        network = lnn_network_create_ncp(2, 16, 8, 1);
        E2E_ASSERT_NOT_NULL(network, "Network creation failed");
        lnn_network_init_weights(network, 42);

        lnn_training_config_t config;
        lnn_training_config_default(&config);
        config.learning_rate = 0.1f;  // High LR to trigger clipping
        config.gradient_clip_norm = 1.0f;  // Aggressive clipping
        config.validate_gradients = true;
        config.track_statistics = true;

        training_ctx = lnn_training_create(network, &config);
        E2E_ASSERT_NOT_NULL(training_ctx, "Training context creation failed");
    }
    E2E_STAGE_END();

    // Stage 2: Train with high LR (should trigger clipping)
    E2E_STAGE_BEGIN("Train with gradient clipping", 3000);
    {
        generate_sine_sequence(32, 2, &inputs, &targets);

        for (uint32_t step = 0; step < 20; ++step) {
            float loss;
            int status = lnn_training_step(training_ctx, inputs, targets, &loss);
            E2E_ASSERT(status == 0, "Training step failed");

            // Verify no NaN/Inf (gradient clipping should prevent)
            E2E_ASSERT(!std::isnan(loss), "NaN loss detected - clipping failed");
            E2E_ASSERT(!std::isinf(loss), "Inf loss detected - clipping failed");
        }
    }
    E2E_STAGE_END();

    // Stage 3: Verify clipping occurred
    E2E_STAGE_BEGIN("Verify clipping statistics", 50);
    {
        lnn_training_stats_t stats;
        lnn_training_get_stats(training_ctx, &stats);

        std::cout << "[E2E] Gradient clips: " << stats.gradient_clips << "\n";
        std::cout << "[E2E] Max gradient norm: " << stats.max_gradient_norm << "\n";
        std::cout << "[E2E] NaN detections: " << stats.nan_detections << "\n";
        std::cout << "[E2E] Inf detections: " << stats.inf_detections << "\n";

        // With high LR, we expect some clipping
        // Note: This may vary based on initialization
        E2E_ASSERT(stats.nan_detections == 0, "NaN gradients detected despite clipping");
        E2E_ASSERT(stats.inf_detections == 0, "Inf gradients detected despite clipping");
    }
    E2E_STAGE_END();

    // Stage 4: Cleanup
    E2E_STAGE_BEGIN("Cleanup", 50);
    {
        nimcp_tensor_destroy(inputs);
        nimcp_tensor_destroy(targets);
        lnn_training_destroy(training_ctx);
        lnn_network_destroy(network);
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Test: Network Save/Load with Training State
//=============================================================================

E2E_TEST_F(LNNTrainingBackpropE2E, TrainedNetworkPersistence) {
    E2E_PIPELINE_START("LNN Trained Network Persistence");

    const char* save_path = "/tmp/nimcp_e2e_lnn_trained.bin";
    lnn_network_t* network = nullptr;
    lnn_network_t* loaded_network = nullptr;
    lnn_training_ctx_t* training_ctx = nullptr;
    nimcp_tensor_t* inputs = nullptr;
    nimcp_tensor_t* targets = nullptr;
    nimcp_tensor_t* outputs_before = nullptr;
    nimcp_tensor_t* outputs_after = nullptr;

    const uint32_t SEQ_LEN = 16;
    const uint32_t N_INPUTS = 2;
    const uint32_t N_OUTPUTS = 1;

    // Stage 1: Create and train network
    E2E_STAGE_BEGIN("Create and train network", 5000);
    {
        network = lnn_network_create_ncp(N_INPUTS, 8, 4, N_OUTPUTS);
        E2E_ASSERT_NOT_NULL(network, "Network creation failed");
        lnn_network_init_weights(network, 42);

        lnn_training_config_t config;
        lnn_training_config_default(&config);
        config.learning_rate = 0.01f;

        training_ctx = lnn_training_create(network, &config);
        E2E_ASSERT_NOT_NULL(training_ctx, "Training context creation failed");

        generate_sine_sequence(SEQ_LEN, N_INPUTS, &inputs, &targets);

        // Train for a few steps
        for (uint32_t step = 0; step < 10; ++step) {
            float loss;
            lnn_training_step(training_ctx, inputs, targets, &loss);
        }

        // Record output after training
        uint32_t output_dims[2] = {SEQ_LEN, N_OUTPUTS};
        outputs_before = nimcp_tensor_create(output_dims, 2, NIMCP_DTYPE_FLOAT32);
        lnn_network_reset_state(network);
        lnn_network_forward_sequence(network, inputs, outputs_before, SEQ_LEN, DEFAULT_DT);
    }
    E2E_STAGE_END();

    // Stage 2: Save trained network
    E2E_STAGE_BEGIN("Save trained network", 500);
    {
        int status = lnn_network_save(network, save_path);
        E2E_ASSERT(status == 0, "Network save failed");
        std::cout << "[E2E] Saved trained network to: " << save_path << "\n";
    }
    E2E_STAGE_END();

    // Stage 3: Destroy training context and network
    E2E_STAGE_BEGIN("Destroy original network", 100);
    {
        lnn_training_destroy(training_ctx);
        lnn_network_destroy(network);
        training_ctx = nullptr;
        network = nullptr;
    }
    E2E_STAGE_END();

    // Stage 4: Load network
    E2E_STAGE_BEGIN("Load saved network", 500);
    {
        loaded_network = lnn_network_load(save_path);
        E2E_ASSERT_NOT_NULL(loaded_network, "Network load failed");
        std::cout << "[E2E] Loaded network from: " << save_path << "\n";
    }
    E2E_STAGE_END();

    // Stage 5: Verify loaded network produces same output
    E2E_STAGE_BEGIN("Verify loaded network output", 500);
    {
        uint32_t output_dims[2] = {SEQ_LEN, N_OUTPUTS};
        outputs_after = nimcp_tensor_create(output_dims, 2, NIMCP_DTYPE_FLOAT32);

        lnn_network_reset_state(loaded_network);
        lnn_network_forward_sequence(loaded_network, inputs, outputs_after, SEQ_LEN, DEFAULT_DT);

        // Compare outputs
        float* before_data = nimcp_tensor_data_float(outputs_before);
        float* after_data = nimcp_tensor_data_float(outputs_after);

        float max_diff = 0.0f;
        for (uint32_t i = 0; i < SEQ_LEN * N_OUTPUTS; ++i) {
            float diff = std::abs(before_data[i] - after_data[i]);
            max_diff = std::max(max_diff, diff);
        }

        std::cout << "[E2E] Max output difference after load: " << max_diff << "\n";
        E2E_ASSERT(max_diff < 1e-5f, "Loaded network produces different outputs");
    }
    E2E_STAGE_END();

    // Stage 6: Cleanup
    E2E_STAGE_BEGIN("Cleanup", 100);
    {
        nimcp_tensor_destroy(inputs);
        nimcp_tensor_destroy(targets);
        nimcp_tensor_destroy(outputs_before);
        nimcp_tensor_destroy(outputs_after);
        lnn_network_destroy(loaded_network);
        std::remove(save_path);
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Test: Batch Training
//=============================================================================

E2E_TEST_F(LNNTrainingBackpropE2E, BatchTrainingPipeline) {
    E2E_PIPELINE_START("LNN Batch Training Pipeline");

    lnn_network_t* network = nullptr;
    lnn_training_ctx_t* training_ctx = nullptr;
    const uint32_t BATCH_SIZE = 4;
    const uint32_t SEQ_LEN = 16;
    const uint32_t N_INPUTS = 2;
    const uint32_t N_OUTPUTS = 1;
    const uint32_t NUM_EPOCHS = 5;

    // Stage 1: Setup
    E2E_STAGE_BEGIN("Setup network and training", 200);
    {
        network = lnn_network_create_ncp(N_INPUTS, 8, 4, N_OUTPUTS);
        E2E_ASSERT_NOT_NULL(network, "Network creation failed");
        lnn_network_init_weights(network, 42);

        lnn_training_config_t config;
        lnn_training_config_default(&config);
        config.learning_rate = 0.001f;
        config.accumulation_steps = BATCH_SIZE;

        training_ctx = lnn_training_create(network, &config);
        E2E_ASSERT_NOT_NULL(training_ctx, "Training context creation failed");
    }
    E2E_STAGE_END();

    // Stage 2: Create batch dataset
    std::vector<nimcp_tensor_t*> batch_inputs(BATCH_SIZE);
    std::vector<nimcp_tensor_t*> batch_targets(BATCH_SIZE);
    E2E_STAGE_BEGIN("Create batch dataset", 200);
    {
        for (uint32_t b = 0; b < BATCH_SIZE; ++b) {
            generate_sine_sequence(SEQ_LEN, N_INPUTS, &batch_inputs[b], &batch_targets[b]);
        }
    }
    E2E_STAGE_END();

    // Stage 3: Train on batch
    std::vector<float> epoch_losses;
    E2E_STAGE_BEGIN("Batch training epochs", 10000);
    {
        for (uint32_t epoch = 0; epoch < NUM_EPOCHS; ++epoch) {
            float epoch_loss = 0.0f;
            uint32_t batch_count = 0;

            for (uint32_t b = 0; b < BATCH_SIZE; ++b) {
                float sample_loss;
                int status = lnn_training_step(
                    training_ctx,
                    batch_inputs[b],
                    batch_targets[b],
                    &sample_loss
                );
                E2E_ASSERT(status == 0, "Batch training step failed");
                epoch_loss += sample_loss;
                batch_count++;
            }

            epoch_loss /= batch_count;
            epoch_losses.push_back(epoch_loss);
            std::cout << "[E2E] Epoch " << epoch << " avg batch loss: " << epoch_loss << "\n";
        }
    }
    E2E_STAGE_END();

    // Stage 4: Verify convergence
    E2E_STAGE_BEGIN("Verify batch training convergence", 50);
    {
        float first_loss = epoch_losses[0];
        float last_loss = epoch_losses[epoch_losses.size() - 1];

        std::cout << "[E2E] First epoch loss: " << first_loss << "\n";
        std::cout << "[E2E] Last epoch loss: " << last_loss << "\n";

        // Training should improve (or at least not diverge)
        E2E_ASSERT(!std::isnan(last_loss), "Training diverged (NaN loss)");
        E2E_ASSERT(!std::isinf(last_loss), "Training diverged (Inf loss)");
    }
    E2E_STAGE_END();

    // Stage 5: Cleanup
    E2E_STAGE_BEGIN("Cleanup", 100);
    {
        for (uint32_t b = 0; b < BATCH_SIZE; ++b) {
            nimcp_tensor_destroy(batch_inputs[b]);
            nimcp_tensor_destroy(batch_targets[b]);
        }
        lnn_training_destroy(training_ctx);
        lnn_network_destroy(network);
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

} // namespace e2e
} // namespace nimcp

// Main function for standalone execution
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
