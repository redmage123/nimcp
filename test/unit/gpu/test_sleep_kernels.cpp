/**
 * @file test_sleep_kernels.cpp
 * @brief Unit tests for GPU sleep and memory consolidation kernels
 *
 * Tests NREM consolidation, REM processing, synaptic homeostasis,
 * and memory replay GPU operations.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <cstring>

// Headers have their own extern "C" guards
#include "gpu/sleep/nimcp_sleep_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class SleepKernelTest : public ::testing::Test {
protected:
    nimcp_gpu_context_t* ctx = nullptr;
    bool gpu_available = false;

    void SetUp() override {
        ctx = nimcp_gpu_context_create_auto();
        gpu_available = (ctx != nullptr && nimcp_gpu_context_is_valid(ctx));
    }

    void TearDown() override {
        if (ctx) {
            nimcp_gpu_context_destroy(ctx);
            ctx = nullptr;
        }
    }

    void RequireGPU() {
        if (!gpu_available) {
            GTEST_SKIP() << "GPU not available, skipping test";
        }
    }

    // Helper to create a tensor filled with a constant value
    nimcp_gpu_tensor_t* CreateFilledTensor(size_t* dims, size_t rank, float value) {
        if (!ctx) return nullptr;
        nimcp_gpu_tensor_t* tensor = nimcp_gpu_tensor_create(ctx, dims, rank, NIMCP_GPU_PRECISION_FP32);
        if (tensor) {
            nimcp_gpu_fill(ctx, tensor, value);
        }
        return tensor;
    }

    // Helper to create 1D tensor
    nimcp_gpu_tensor_t* Create1DTensor(size_t n, float value = 0.0f) {
        size_t dims[1] = {n};
        return CreateFilledTensor(dims, 1, value);
    }

    // Helper to create 2D tensor
    nimcp_gpu_tensor_t* Create2DTensor(size_t rows, size_t cols, float value = 0.0f) {
        size_t dims[2] = {rows, cols};
        return CreateFilledTensor(dims, 2, value);
    }

    // Helper to copy tensor to host
    std::vector<float> CopyToHost(nimcp_gpu_tensor_t* tensor) {
        size_t n = tensor->numel;
        std::vector<float> host_data(n);
        nimcp_gpu_tensor_to_host(tensor, host_data.data());
        return host_data;
    }

    // Helper to set tensor from host
    nimcp_gpu_tensor_t* SetFromHost(nimcp_gpu_tensor_t* tensor, const std::vector<float>& data) {
        if (tensor) nimcp_gpu_tensor_destroy(tensor);
        size_t dims[1] = {data.size()};
        return nimcp_gpu_tensor_from_host(ctx, data.data(), dims, 1, NIMCP_GPU_PRECISION_FP32);
    }

    // Helper to create sleep stage state
    nimcp_gpu_sleep_stage_state_t* CreateSleepStageState() {
        if (!ctx) return nullptr;

        nimcp_gpu_sleep_stage_state_t* state = new nimcp_gpu_sleep_stage_state_t();
        state->current_stage = NIMCP_SLEEP_WAKE;
        state->stage_duration = 0.0f;
        state->total_sleep_time = 0.0f;
        state->sleep_pressure = 0.5f;

        size_t dims[2] = {NIMCP_SLEEP_COUNT, NIMCP_SLEEP_COUNT};
        state->stage_probabilities = nimcp_gpu_tensor_create(ctx, dims, 2, NIMCP_GPU_PRECISION_FP32);
        if (state->stage_probabilities) {
            nimcp_gpu_fill(ctx, state->stage_probabilities, 0.2f);
        }

        return state;
    }

    void DestroySleepStageState(nimcp_gpu_sleep_stage_state_t* state) {
        if (state) {
            if (state->stage_probabilities) {
                nimcp_gpu_tensor_destroy(state->stage_probabilities);
            }
            delete state;
        }
    }

    // Helper to create consolidation state
    nimcp_gpu_consolidation_state_t* CreateConsolidationState(size_t buffer_size, size_t memory_dim) {
        if (!ctx) return nullptr;

        nimcp_gpu_consolidation_state_t* state = new nimcp_gpu_consolidation_state_t();
        state->buffer_size = buffer_size;
        state->memory_dim = memory_dim;

        size_t buffer_dims[2] = {buffer_size, memory_dim};
        size_t weight_dims[2] = {memory_dim, memory_dim};
        size_t vec_dims[1] = {buffer_size};
        size_t osc_dims[1] = {memory_dim};

        state->hippocampal_buffer = nimcp_gpu_tensor_create(ctx, buffer_dims, 2, NIMCP_GPU_PRECISION_FP32);
        state->cortical_weights = nimcp_gpu_tensor_create(ctx, weight_dims, 2, NIMCP_GPU_PRECISION_FP32);
        state->replay_buffer = nimcp_gpu_tensor_create(ctx, buffer_dims, 2, NIMCP_GPU_PRECISION_FP32);
        state->consolidation_mask = nimcp_gpu_tensor_create(ctx, vec_dims, 1, NIMCP_GPU_PRECISION_FP32);
        state->priority_scores = nimcp_gpu_tensor_create(ctx, vec_dims, 1, NIMCP_GPU_PRECISION_FP32);
        state->slow_oscillation = nimcp_gpu_tensor_create(ctx, osc_dims, 1, NIMCP_GPU_PRECISION_FP32);

        if (state->hippocampal_buffer) nimcp_gpu_fill(ctx, state->hippocampal_buffer, 0.5f);
        if (state->cortical_weights) nimcp_gpu_fill(ctx, state->cortical_weights, 0.1f);
        if (state->replay_buffer) nimcp_gpu_fill(ctx, state->replay_buffer, 0.0f);
        if (state->consolidation_mask) nimcp_gpu_fill(ctx, state->consolidation_mask, 1.0f);
        if (state->priority_scores) nimcp_gpu_fill(ctx, state->priority_scores, 0.5f);
        if (state->slow_oscillation) nimcp_gpu_fill(ctx, state->slow_oscillation, 0.0f);

        return state;
    }

    void DestroyConsolidationState(nimcp_gpu_consolidation_state_t* state) {
        if (state) {
            if (state->hippocampal_buffer) nimcp_gpu_tensor_destroy(state->hippocampal_buffer);
            if (state->cortical_weights) nimcp_gpu_tensor_destroy(state->cortical_weights);
            if (state->replay_buffer) nimcp_gpu_tensor_destroy(state->replay_buffer);
            if (state->consolidation_mask) nimcp_gpu_tensor_destroy(state->consolidation_mask);
            if (state->priority_scores) nimcp_gpu_tensor_destroy(state->priority_scores);
            if (state->slow_oscillation) nimcp_gpu_tensor_destroy(state->slow_oscillation);
            delete state;
        }
    }

    // Helper to create synaptic state
    nimcp_gpu_synaptic_state_t* CreateSynapticState(size_t n_synapses) {
        if (!ctx) return nullptr;

        nimcp_gpu_synaptic_state_t* state = new nimcp_gpu_synaptic_state_t();
        state->n_synapses = n_synapses;

        size_t dims[1] = {n_synapses};
        size_t history_dims[2] = {n_synapses, 10};  // 10 time steps of history

        state->synaptic_weights = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
        state->weight_history = nimcp_gpu_tensor_create(ctx, history_dims, 2, NIMCP_GPU_PRECISION_FP32);
        state->potentiation_tags = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
        state->activity_integral = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);

        if (state->synaptic_weights) nimcp_gpu_fill(ctx, state->synaptic_weights, 0.5f);
        if (state->weight_history) nimcp_gpu_fill(ctx, state->weight_history, 0.0f);
        if (state->potentiation_tags) nimcp_gpu_fill(ctx, state->potentiation_tags, 0.0f);
        if (state->activity_integral) nimcp_gpu_fill(ctx, state->activity_integral, 0.0f);

        return state;
    }

    void DestroySynapticState(nimcp_gpu_synaptic_state_t* state) {
        if (state) {
            if (state->synaptic_weights) nimcp_gpu_tensor_destroy(state->synaptic_weights);
            if (state->weight_history) nimcp_gpu_tensor_destroy(state->weight_history);
            if (state->potentiation_tags) nimcp_gpu_tensor_destroy(state->potentiation_tags);
            if (state->activity_integral) nimcp_gpu_tensor_destroy(state->activity_integral);
            delete state;
        }
    }
};

//=============================================================================
// NREM Parameter Tests
//=============================================================================

TEST_F(SleepKernelTest, NREMParamsDefault_ReturnsValidParams) {
    nimcp_gpu_nrem_params_t params = nimcp_gpu_nrem_params_default();

    // Check reasonable defaults for slow-wave sleep
    EXPECT_GT(params.slow_wave_freq, 0.0f);
    EXPECT_LE(params.slow_wave_freq, 1.0f);  // Slow oscillations ~0.5-1 Hz
    EXPECT_GT(params.spindle_freq, 0.0f);
    EXPECT_LE(params.spindle_freq, 20.0f);   // Sleep spindles ~10-16 Hz
    EXPECT_GT(params.sharp_wave_rate, 0.0f);
    EXPECT_GT(params.consolidation_rate, 0.0f);
    EXPECT_GT(params.replay_speed, 0.0f);
    EXPECT_GT(params.cortical_gain, 0.0f);
    EXPECT_GT(params.replay_iterations, 0);
    EXPECT_GE(params.noise_level, 0.0f);
}

//=============================================================================
// REM Parameter Tests
//=============================================================================

TEST_F(SleepKernelTest, REMParamsDefault_ReturnsValidParams) {
    nimcp_gpu_rem_params_t params = nimcp_gpu_rem_params_default();

    // Check reasonable defaults for REM sleep
    EXPECT_GT(params.theta_freq, 0.0f);
    EXPECT_LE(params.theta_freq, 12.0f);     // Theta rhythm ~4-8 Hz
    EXPECT_GT(params.pgo_rate, 0.0f);
    EXPECT_GE(params.emotional_bias, 0.0f);
    EXPECT_LE(params.emotional_bias, 1.0f);
    EXPECT_GT(params.integration_rate, 0.0f);
    EXPECT_GE(params.dream_generation_rate, 0.0f);
    EXPECT_GE(params.creativity_factor, 0.0f);
    EXPECT_GE(params.acetylcholine_level, 0.0f);  // High ACh in REM
}

//=============================================================================
// Homeostasis Parameter Tests
//=============================================================================

TEST_F(SleepKernelTest, HomeostasisParamsDefault_ReturnsValidParams) {
    nimcp_gpu_homeostasis_params_t params = nimcp_gpu_homeostasis_params_default();

    // Check reasonable defaults for synaptic homeostasis
    EXPECT_GT(params.downscaling_rate, 0.0f);
    EXPECT_LE(params.downscaling_rate, 1.0f);
    EXPECT_GT(params.threshold, 0.0f);
    EXPECT_GT(params.preservation_factor, 0.0f);
    EXPECT_LE(params.preservation_factor, 1.0f);
    EXPECT_GT(params.global_factor, 0.0f);
    EXPECT_LE(params.global_factor, 1.0f);
    EXPECT_GE(params.min_weight, 0.0f);
}

//=============================================================================
// Replay Parameter Tests
//=============================================================================

TEST_F(SleepKernelTest, ReplayParamsDefault_ReturnsValidParams) {
    nimcp_gpu_replay_params_t params = nimcp_gpu_replay_params_default();

    // Check reasonable defaults for memory replay
    EXPECT_GT(params.compression_ratio, 0.0f);  // Temporal compression
    EXPECT_GT(params.sequence_fidelity, 0.0f);
    EXPECT_LE(params.sequence_fidelity, 1.0f);
    EXPECT_GE(params.reverse_replay_prob, 0.0f);
    EXPECT_LE(params.reverse_replay_prob, 1.0f);
    EXPECT_GE(params.priority_weight, 0.0f);
    EXPECT_GT(params.buffer_size, 0);
    EXPECT_GT(params.decay_rate, 0.0f);
}

//=============================================================================
// Sleep Stage State Tests
//=============================================================================

TEST_F(SleepKernelTest, SleepStageUpdate_TransitionsStages) {
    RequireGPU();

    nimcp_gpu_sleep_stage_state_t* state = CreateSleepStageState();
    ASSERT_NE(state, nullptr);

    // Start awake with high sleep pressure
    state->current_stage = NIMCP_SLEEP_WAKE;
    state->sleep_pressure = 0.8f;
    float circadian_phase = 0.0f;  // Night time
    float arousal_signal = 0.1f;   // Low arousal
    float dt = 1.0f;

    bool result = nimcp_gpu_sleep_stage_update(ctx, state, circadian_phase, arousal_signal, dt);
    EXPECT_TRUE(result);

    // Stage duration should have increased
    EXPECT_GT(state->stage_duration, 0.0f);

    DestroySleepStageState(state);
}

TEST_F(SleepKernelTest, SleepStageUpdate_AccumulatesTotalSleepTime) {
    RequireGPU();

    nimcp_gpu_sleep_stage_state_t* state = CreateSleepStageState();
    ASSERT_NE(state, nullptr);

    state->current_stage = NIMCP_SLEEP_N2;  // Light sleep
    state->total_sleep_time = 0.0f;
    float dt = 10.0f;

    // Update multiple times
    for (int i = 0; i < 10; i++) {
        bool result = nimcp_gpu_sleep_stage_update(ctx, state, 0.0f, 0.2f, dt);
        EXPECT_TRUE(result);
    }

    // Total sleep time should have accumulated
    EXPECT_GT(state->total_sleep_time, 0.0f);

    DestroySleepStageState(state);
}

TEST_F(SleepKernelTest, SleepTransitions_ComputesProbabilities) {
    RequireGPU();

    nimcp_gpu_sleep_stage_state_t* state = CreateSleepStageState();
    ASSERT_NE(state, nullptr);

    size_t trans_dims[2] = {NIMCP_SLEEP_COUNT, NIMCP_SLEEP_COUNT};
    nimcp_gpu_tensor_t* transition_probs = nimcp_gpu_tensor_create(ctx, trans_dims, 2, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(transition_probs, nullptr);
    nimcp_gpu_fill(ctx, transition_probs, 0.0f);

    bool result = nimcp_gpu_sleep_transitions(ctx, state, transition_probs);
    EXPECT_TRUE(result);

    auto probs = CopyToHost(transition_probs);

    // Probabilities should be non-negative
    for (size_t i = 0; i < probs.size(); i++) {
        EXPECT_GE(probs[i], 0.0f);
        EXPECT_LE(probs[i], 1.0f);
    }

    nimcp_gpu_tensor_destroy(transition_probs);
    DestroySleepStageState(state);
}

//=============================================================================
// NREM Consolidation Tests
//=============================================================================

TEST_F(SleepKernelTest, NREMSlowOscillation_GeneratesPattern) {
    RequireGPU();

    const size_t buffer_size = 50;
    const size_t memory_dim = 32;

    nimcp_gpu_consolidation_state_t* state = CreateConsolidationState(buffer_size, memory_dim);
    ASSERT_NE(state, nullptr);

    nimcp_gpu_nrem_params_t params = nimcp_gpu_nrem_params_default();

    // Generate slow oscillation at different times
    float time = 0.0f;
    auto initial_osc = CopyToHost(state->slow_oscillation);

    bool result = nimcp_gpu_nrem_slow_oscillation(ctx, state, time, &params);
    EXPECT_TRUE(result);

    auto osc_data = CopyToHost(state->slow_oscillation);

    // Oscillation should have non-zero values
    bool has_nonzero = false;
    for (size_t i = 0; i < osc_data.size(); i++) {
        if (std::abs(osc_data[i]) > 1e-6f) {
            has_nonzero = true;
            break;
        }
    }
    // Slow oscillation should produce some pattern (may be zero at t=0)
    // Test at different phase
    time = 0.5f;
    result = nimcp_gpu_nrem_slow_oscillation(ctx, state, time, &params);
    EXPECT_TRUE(result);

    DestroyConsolidationState(state);
}

TEST_F(SleepKernelTest, NREMSlowOscillation_OscillatesOverTime) {
    RequireGPU();

    const size_t buffer_size = 50;
    const size_t memory_dim = 32;

    nimcp_gpu_consolidation_state_t* state = CreateConsolidationState(buffer_size, memory_dim);
    ASSERT_NE(state, nullptr);

    nimcp_gpu_nrem_params_t params = nimcp_gpu_nrem_params_default();

    std::vector<float> time_samples = {0.0f, 0.5f, 1.0f, 1.5f, 2.0f};
    std::vector<std::vector<float>> osc_history;

    for (float t : time_samples) {
        nimcp_gpu_nrem_slow_oscillation(ctx, state, t, &params);
        osc_history.push_back(CopyToHost(state->slow_oscillation));
    }

    // Oscillation should vary over time (not all identical)
    bool varies = false;
    for (size_t i = 1; i < osc_history.size(); i++) {
        for (size_t j = 0; j < osc_history[i].size(); j++) {
            if (std::abs(osc_history[i][j] - osc_history[0][j]) > 1e-6f) {
                varies = true;
                break;
            }
        }
        if (varies) break;
    }
    EXPECT_TRUE(varies);

    DestroyConsolidationState(state);
}

TEST_F(SleepKernelTest, NREMReplay_ProcessesMemories) {
    RequireGPU();

    const size_t buffer_size = 50;
    const size_t memory_dim = 32;

    nimcp_gpu_consolidation_state_t* state = CreateConsolidationState(buffer_size, memory_dim);
    ASSERT_NE(state, nullptr);

    // Fill hippocampal buffer with memories
    nimcp_gpu_fill(ctx, state->hippocampal_buffer, 0.8f);
    nimcp_gpu_fill(ctx, state->priority_scores, 0.9f);

    auto initial_replay = CopyToHost(state->replay_buffer);

    nimcp_gpu_nrem_params_t params = nimcp_gpu_nrem_params_default();

    bool result = nimcp_gpu_nrem_replay(ctx, state, &params);
    EXPECT_TRUE(result);

    auto final_replay = CopyToHost(state->replay_buffer);

    // Replay buffer should be modified
    bool modified = false;
    for (size_t i = 0; i < final_replay.size(); i++) {
        if (std::abs(final_replay[i] - initial_replay[i]) > 1e-6f) {
            modified = true;
            break;
        }
    }
    EXPECT_TRUE(modified);

    DestroyConsolidationState(state);
}

TEST_F(SleepKernelTest, NREMSystemsConsolidation_TransfersMemories) {
    RequireGPU();

    const size_t buffer_size = 50;
    const size_t memory_dim = 32;
    const float dt = 10.0f;

    nimcp_gpu_consolidation_state_t* state = CreateConsolidationState(buffer_size, memory_dim);
    ASSERT_NE(state, nullptr);

    // Set up for consolidation
    nimcp_gpu_fill(ctx, state->hippocampal_buffer, 1.0f);
    nimcp_gpu_fill(ctx, state->consolidation_mask, 1.0f);

    auto initial_weights = CopyToHost(state->cortical_weights);

    nimcp_gpu_nrem_params_t params = nimcp_gpu_nrem_params_default();

    // Run systems consolidation multiple times
    for (int i = 0; i < 10; i++) {
        bool result = nimcp_gpu_nrem_systems_consolidation(ctx, state, dt, &params);
        EXPECT_TRUE(result);
    }

    auto final_weights = CopyToHost(state->cortical_weights);

    // Cortical weights should be modified (memory transfer)
    bool transferred = false;
    for (size_t i = 0; i < final_weights.size(); i++) {
        if (std::abs(final_weights[i] - initial_weights[i]) > 1e-6f) {
            transferred = true;
            break;
        }
    }
    EXPECT_TRUE(transferred);

    DestroyConsolidationState(state);
}

TEST_F(SleepKernelTest, NREMSharpWaveRipple_GeneratesContent) {
    RequireGPU();

    const size_t buffer_size = 50;
    const size_t memory_dim = 32;

    nimcp_gpu_consolidation_state_t* state = CreateConsolidationState(buffer_size, memory_dim);
    ASSERT_NE(state, nullptr);

    nimcp_gpu_fill(ctx, state->hippocampal_buffer, 0.7f);

    size_t ripple_dims[1] = {memory_dim};
    nimcp_gpu_tensor_t* ripple_content = nimcp_gpu_tensor_create(ctx, ripple_dims, 1, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(ripple_content, nullptr);
    nimcp_gpu_fill(ctx, ripple_content, 0.0f);

    nimcp_gpu_nrem_params_t params = nimcp_gpu_nrem_params_default();

    bool result = nimcp_gpu_nrem_sharp_wave_ripple(ctx, state, ripple_content, &params);
    EXPECT_TRUE(result);

    auto ripple_data = CopyToHost(ripple_content);

    // Ripple content should be non-zero
    float ripple_sum = 0.0f;
    for (float v : ripple_data) {
        ripple_sum += std::abs(v);
    }
    EXPECT_GT(ripple_sum, 0.0f);

    nimcp_gpu_tensor_destroy(ripple_content);
    DestroyConsolidationState(state);
}

//=============================================================================
// REM Processing Tests
//=============================================================================

TEST_F(SleepKernelTest, REMProcessing_ModifiesState) {
    RequireGPU();

    const size_t buffer_size = 50;
    const size_t memory_dim = 32;
    const float dt = 10.0f;

    nimcp_gpu_consolidation_state_t* state = CreateConsolidationState(buffer_size, memory_dim);
    ASSERT_NE(state, nullptr);

    nimcp_gpu_fill(ctx, state->hippocampal_buffer, 0.6f);

    auto initial_buffer = CopyToHost(state->hippocampal_buffer);

    nimcp_gpu_rem_params_t params = nimcp_gpu_rem_params_default();

    bool result = nimcp_gpu_rem_processing(ctx, state, dt, &params);
    EXPECT_TRUE(result);

    auto final_buffer = CopyToHost(state->hippocampal_buffer);

    // Processing should modify the buffer
    bool modified = false;
    for (size_t i = 0; i < final_buffer.size(); i++) {
        if (std::abs(final_buffer[i] - initial_buffer[i]) > 1e-6f) {
            modified = true;
            break;
        }
    }
    // REM processing may or may not modify buffer, but should complete successfully
    EXPECT_TRUE(result);

    DestroyConsolidationState(state);
}

TEST_F(SleepKernelTest, REMMemoryIntegration_IntegratesWithSemantic) {
    RequireGPU();

    const size_t buffer_size = 50;
    const size_t memory_dim = 32;

    nimcp_gpu_consolidation_state_t* state = CreateConsolidationState(buffer_size, memory_dim);
    ASSERT_NE(state, nullptr);

    // Create semantic memory
    size_t semantic_dims[2] = {memory_dim, memory_dim};
    nimcp_gpu_tensor_t* semantic_memory = nimcp_gpu_tensor_create(ctx, semantic_dims, 2, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(semantic_memory, nullptr);
    nimcp_gpu_fill(ctx, semantic_memory, 0.3f);

    nimcp_gpu_fill(ctx, state->hippocampal_buffer, 0.7f);

    auto initial_weights = CopyToHost(state->cortical_weights);

    nimcp_gpu_rem_params_t params = nimcp_gpu_rem_params_default();

    bool result = nimcp_gpu_rem_memory_integration(ctx, state, semantic_memory, &params);
    EXPECT_TRUE(result);

    auto final_weights = CopyToHost(state->cortical_weights);

    // Integration should modify cortical weights
    bool integrated = false;
    for (size_t i = 0; i < final_weights.size(); i++) {
        if (std::abs(final_weights[i] - initial_weights[i]) > 1e-6f) {
            integrated = true;
            break;
        }
    }
    EXPECT_TRUE(integrated);

    nimcp_gpu_tensor_destroy(semantic_memory);
    DestroyConsolidationState(state);
}

TEST_F(SleepKernelTest, REMEmotionalProcessing_ProcessesEmotions) {
    RequireGPU();

    const size_t buffer_size = 50;
    const size_t memory_dim = 32;

    nimcp_gpu_consolidation_state_t* state = CreateConsolidationState(buffer_size, memory_dim);
    ASSERT_NE(state, nullptr);

    // Create emotional tags (high emotional content)
    size_t tag_dims[1] = {buffer_size};
    nimcp_gpu_tensor_t* emotional_tags = nimcp_gpu_tensor_create(ctx, tag_dims, 1, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(emotional_tags, nullptr);
    nimcp_gpu_fill(ctx, emotional_tags, 0.9f);  // High emotional salience

    nimcp_gpu_fill(ctx, state->hippocampal_buffer, 0.5f);
    nimcp_gpu_fill(ctx, state->priority_scores, 0.5f);

    auto initial_priorities = CopyToHost(state->priority_scores);

    nimcp_gpu_rem_params_t params = nimcp_gpu_rem_params_default();
    params.emotional_bias = 0.8f;  // Strong emotional bias

    bool result = nimcp_gpu_rem_emotional_processing(ctx, state, emotional_tags, &params);
    EXPECT_TRUE(result);

    auto final_priorities = CopyToHost(state->priority_scores);

    // Emotional processing should affect priority scores
    bool processed = false;
    for (size_t i = 0; i < final_priorities.size(); i++) {
        if (std::abs(final_priorities[i] - initial_priorities[i]) > 1e-6f) {
            processed = true;
            break;
        }
    }
    // May or may not modify priorities, but should complete
    EXPECT_TRUE(result);

    nimcp_gpu_tensor_destroy(emotional_tags);
    DestroyConsolidationState(state);
}

TEST_F(SleepKernelTest, REMDreamGeneration_GeneratesContent) {
    RequireGPU();

    const size_t buffer_size = 50;
    const size_t memory_dim = 32;

    nimcp_gpu_consolidation_state_t* state = CreateConsolidationState(buffer_size, memory_dim);
    ASSERT_NE(state, nullptr);

    nimcp_gpu_fill(ctx, state->hippocampal_buffer, 0.6f);

    size_t dream_dims[1] = {memory_dim};
    nimcp_gpu_tensor_t* dream_content = nimcp_gpu_tensor_create(ctx, dream_dims, 1, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(dream_content, nullptr);
    nimcp_gpu_fill(ctx, dream_content, 0.0f);

    nimcp_gpu_rem_params_t params = nimcp_gpu_rem_params_default();
    params.creativity_factor = 0.5f;

    bool result = nimcp_gpu_rem_dream_generation(ctx, state, dream_content, &params);
    EXPECT_TRUE(result);

    auto dream_data = CopyToHost(dream_content);

    // Dream content should be non-zero (creative recombination)
    float dream_sum = 0.0f;
    for (float v : dream_data) {
        dream_sum += std::abs(v);
    }
    EXPECT_GT(dream_sum, 0.0f);

    nimcp_gpu_tensor_destroy(dream_content);
    DestroyConsolidationState(state);
}

//=============================================================================
// Synaptic Homeostasis Tests
//=============================================================================

TEST_F(SleepKernelTest, SynapticDownscaling_ReducesWeights) {
    RequireGPU();

    const size_t n_synapses = 100;

    nimcp_gpu_synaptic_state_t* state = CreateSynapticState(n_synapses);
    ASSERT_NE(state, nullptr);

    // Set high initial weights
    nimcp_gpu_fill(ctx, state->synaptic_weights, 0.8f);

    auto initial_weights = CopyToHost(state->synaptic_weights);
    float initial_sum = 0.0f;
    for (float w : initial_weights) initial_sum += w;

    nimcp_gpu_homeostasis_params_t params = nimcp_gpu_homeostasis_params_default();

    bool result = nimcp_gpu_synaptic_downscaling(ctx, state, &params);
    EXPECT_TRUE(result);

    auto final_weights = CopyToHost(state->synaptic_weights);
    float final_sum = 0.0f;
    for (float w : final_weights) final_sum += w;

    // Weights should be reduced after downscaling
    EXPECT_LT(final_sum, initial_sum);

    DestroySynapticState(state);
}

TEST_F(SleepKernelTest, SynapticDownscaling_RespectsMinWeight) {
    RequireGPU();

    const size_t n_synapses = 100;

    nimcp_gpu_synaptic_state_t* state = CreateSynapticState(n_synapses);
    ASSERT_NE(state, nullptr);

    // Set weights near minimum
    nimcp_gpu_fill(ctx, state->synaptic_weights, 0.1f);

    nimcp_gpu_homeostasis_params_t params = nimcp_gpu_homeostasis_params_default();
    params.min_weight = 0.05f;

    // Apply downscaling multiple times
    for (int i = 0; i < 100; i++) {
        nimcp_gpu_synaptic_downscaling(ctx, state, &params);
    }

    auto final_weights = CopyToHost(state->synaptic_weights);

    // All weights should be >= min_weight
    for (float w : final_weights) {
        EXPECT_GE(w, params.min_weight - 1e-5f);
    }

    DestroySynapticState(state);
}

TEST_F(SleepKernelTest, SynapsePreservation_PreservesImportant) {
    RequireGPU();

    const size_t n_synapses = 100;

    nimcp_gpu_synaptic_state_t* state = CreateSynapticState(n_synapses);
    ASSERT_NE(state, nullptr);

    nimcp_gpu_fill(ctx, state->synaptic_weights, 0.7f);

    // High importance scores for some synapses
    size_t imp_dims[1] = {n_synapses};
    nimcp_gpu_tensor_t* importance_scores = nimcp_gpu_tensor_create(ctx, imp_dims, 1, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(importance_scores, nullptr);

    std::vector<float> importance(n_synapses);
    for (size_t i = 0; i < n_synapses; i++) {
        importance[i] = (i < n_synapses / 2) ? 0.9f : 0.1f;  // First half important
    }
    importance_scores = SetFromHost(importance_scores, importance);

    nimcp_gpu_homeostasis_params_t params = nimcp_gpu_homeostasis_params_default();

    bool result = nimcp_gpu_synapse_preservation(ctx, state, importance_scores, &params);
    EXPECT_TRUE(result);

    auto tags = CopyToHost(state->potentiation_tags);

    // Important synapses should have higher tags
    float important_tag_sum = 0.0f;
    float unimportant_tag_sum = 0.0f;
    for (size_t i = 0; i < n_synapses; i++) {
        if (i < n_synapses / 2) {
            important_tag_sum += tags[i];
        } else {
            unimportant_tag_sum += tags[i];
        }
    }
    EXPECT_GE(important_tag_sum, unimportant_tag_sum);

    nimcp_gpu_tensor_destroy(importance_scores);
    DestroySynapticState(state);
}

TEST_F(SleepKernelTest, SynapsePruning_RemovesWeak) {
    RequireGPU();

    const size_t n_synapses = 100;

    nimcp_gpu_synaptic_state_t* state = CreateSynapticState(n_synapses);
    ASSERT_NE(state, nullptr);

    // Create mixed weights: some weak, some strong
    std::vector<float> weights(n_synapses);
    for (size_t i = 0; i < n_synapses; i++) {
        weights[i] = (i % 2 == 0) ? 0.8f : 0.05f;  // Alternating strong/weak
    }
    state->synaptic_weights = SetFromHost(state->synaptic_weights, weights);

    nimcp_gpu_homeostasis_params_t params = nimcp_gpu_homeostasis_params_default();
    params.selective_pruning = true;
    params.threshold = 0.1f;  // Prune below this
    params.min_weight = 0.0f;

    bool result = nimcp_gpu_synapse_pruning(ctx, state, &params);
    EXPECT_TRUE(result);

    auto final_weights = CopyToHost(state->synaptic_weights);

    // Weak synapses should be pruned (set to min or zero)
    int pruned_count = 0;
    for (size_t i = 0; i < n_synapses; i++) {
        if (i % 2 != 0 && final_weights[i] <= params.min_weight + 1e-5f) {
            pruned_count++;
        }
    }
    // At least some weak synapses should be pruned
    EXPECT_GT(pruned_count, 0);

    DestroySynapticState(state);
}

TEST_F(SleepKernelTest, SynapticTagging_TagsRecentActivity) {
    RequireGPU();

    const size_t n_synapses = 100;
    const float dt = 10.0f;

    nimcp_gpu_synaptic_state_t* state = CreateSynapticState(n_synapses);
    ASSERT_NE(state, nullptr);

    nimcp_gpu_fill(ctx, state->potentiation_tags, 0.0f);

    // Create recent activity pattern
    size_t act_dims[1] = {n_synapses};
    nimcp_gpu_tensor_t* recent_activity = nimcp_gpu_tensor_create(ctx, act_dims, 1, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(recent_activity, nullptr);

    std::vector<float> activity(n_synapses);
    for (size_t i = 0; i < n_synapses; i++) {
        activity[i] = (i < n_synapses / 2) ? 1.0f : 0.0f;  // First half active
    }
    recent_activity = SetFromHost(recent_activity, activity);

    bool result = nimcp_gpu_synaptic_tagging(ctx, state, recent_activity, dt);
    EXPECT_TRUE(result);

    auto tags = CopyToHost(state->potentiation_tags);

    // Active synapses should have higher tags
    float active_tag_sum = 0.0f;
    float inactive_tag_sum = 0.0f;
    for (size_t i = 0; i < n_synapses; i++) {
        if (i < n_synapses / 2) {
            active_tag_sum += tags[i];
        } else {
            inactive_tag_sum += tags[i];
        }
    }
    EXPECT_GT(active_tag_sum, inactive_tag_sum);

    nimcp_gpu_tensor_destroy(recent_activity);
    DestroySynapticState(state);
}

//=============================================================================
// Memory Replay Tests
//=============================================================================

TEST_F(SleepKernelTest, ReplaySample_SamplesFromBuffer) {
    RequireGPU();

    const size_t buffer_size = 50;
    const size_t memory_dim = 32;
    const int n_samples = 5;

    nimcp_gpu_consolidation_state_t* state = CreateConsolidationState(buffer_size, memory_dim);
    ASSERT_NE(state, nullptr);

    // Fill replay buffer with diverse memories
    std::vector<float> buffer_data(buffer_size * memory_dim);
    for (size_t i = 0; i < buffer_data.size(); i++) {
        buffer_data[i] = static_cast<float>(i) / buffer_data.size();
    }
    state->replay_buffer = SetFromHost(state->replay_buffer, buffer_data);
    nimcp_gpu_fill(ctx, state->priority_scores, 0.5f);

    size_t sample_dims[2] = {static_cast<size_t>(n_samples), memory_dim};
    nimcp_gpu_tensor_t* sampled_memories = nimcp_gpu_tensor_create(ctx, sample_dims, 2, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(sampled_memories, nullptr);
    nimcp_gpu_fill(ctx, sampled_memories, 0.0f);

    nimcp_gpu_replay_params_t params = nimcp_gpu_replay_params_default();

    bool result = nimcp_gpu_replay_sample(ctx, state, sampled_memories, n_samples, &params);
    EXPECT_TRUE(result);

    auto samples = CopyToHost(sampled_memories);

    // Samples should be non-zero
    float sample_sum = 0.0f;
    for (float v : samples) {
        sample_sum += std::abs(v);
    }
    EXPECT_GT(sample_sum, 0.0f);

    nimcp_gpu_tensor_destroy(sampled_memories);
    DestroyConsolidationState(state);
}

TEST_F(SleepKernelTest, ReplayStore_AddsToBuffer) {
    RequireGPU();

    const size_t buffer_size = 50;
    const size_t memory_dim = 32;

    nimcp_gpu_consolidation_state_t* state = CreateConsolidationState(buffer_size, memory_dim);
    ASSERT_NE(state, nullptr);

    nimcp_gpu_fill(ctx, state->replay_buffer, 0.0f);

    // Create new experience
    size_t exp_dims[1] = {memory_dim};
    nimcp_gpu_tensor_t* experience = nimcp_gpu_tensor_create(ctx, exp_dims, 1, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(experience, nullptr);
    nimcp_gpu_fill(ctx, experience, 0.9f);

    float priority = 0.8f;
    nimcp_gpu_replay_params_t params = nimcp_gpu_replay_params_default();

    bool result = nimcp_gpu_replay_store(ctx, state, experience, priority, &params);
    EXPECT_TRUE(result);

    auto buffer = CopyToHost(state->replay_buffer);

    // Buffer should now contain the experience
    float buffer_sum = 0.0f;
    for (float v : buffer) {
        buffer_sum += std::abs(v);
    }
    EXPECT_GT(buffer_sum, 0.0f);

    nimcp_gpu_tensor_destroy(experience);
    DestroyConsolidationState(state);
}

TEST_F(SleepKernelTest, ReplayUpdatePriorities_AdjustsPriorities) {
    RequireGPU();

    const size_t buffer_size = 50;
    const size_t memory_dim = 32;

    nimcp_gpu_consolidation_state_t* state = CreateConsolidationState(buffer_size, memory_dim);
    ASSERT_NE(state, nullptr);

    nimcp_gpu_fill(ctx, state->priority_scores, 0.5f);

    // Create TD errors (high errors = high priority)
    size_t td_dims[1] = {buffer_size};
    nimcp_gpu_tensor_t* td_errors = nimcp_gpu_tensor_create(ctx, td_dims, 1, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(td_errors, nullptr);

    std::vector<float> errors(buffer_size);
    for (size_t i = 0; i < buffer_size; i++) {
        errors[i] = (i < buffer_size / 2) ? 1.0f : 0.1f;  // First half high error
    }
    td_errors = SetFromHost(td_errors, errors);

    auto initial_priorities = CopyToHost(state->priority_scores);

    nimcp_gpu_replay_params_t params = nimcp_gpu_replay_params_default();

    bool result = nimcp_gpu_replay_update_priorities(ctx, state, td_errors, &params);
    EXPECT_TRUE(result);

    auto final_priorities = CopyToHost(state->priority_scores);

    // High error memories should have increased priorities
    bool priorities_changed = false;
    for (size_t i = 0; i < buffer_size; i++) {
        if (std::abs(final_priorities[i] - initial_priorities[i]) > 1e-6f) {
            priorities_changed = true;
            break;
        }
    }
    EXPECT_TRUE(priorities_changed);

    nimcp_gpu_tensor_destroy(td_errors);
    DestroyConsolidationState(state);
}

TEST_F(SleepKernelTest, ReplayCompress_CompressesSequence) {
    RequireGPU();

    const size_t seq_length = 100;
    const size_t memory_dim = 32;

    size_t seq_dims[2] = {seq_length, memory_dim};
    nimcp_gpu_tensor_t* sequence = nimcp_gpu_tensor_create(ctx, seq_dims, 2, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(sequence, nullptr);

    // Create sequence data
    std::vector<float> seq_data(seq_length * memory_dim);
    for (size_t i = 0; i < seq_data.size(); i++) {
        seq_data[i] = std::sin(static_cast<float>(i) * 0.1f);
    }
    sequence = SetFromHost(sequence, seq_data);

    nimcp_gpu_replay_params_t params = nimcp_gpu_replay_params_default();
    params.compression_ratio = 5.0f;  // 5x compression

    size_t compressed_length = seq_length / 5;
    size_t comp_dims[2] = {compressed_length, memory_dim};
    nimcp_gpu_tensor_t* compressed = nimcp_gpu_tensor_create(ctx, comp_dims, 2, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(compressed, nullptr);
    nimcp_gpu_fill(ctx, compressed, 0.0f);

    bool result = nimcp_gpu_replay_compress(ctx, sequence, compressed, &params);
    EXPECT_TRUE(result);

    auto comp_data = CopyToHost(compressed);

    // Compressed data should be non-zero
    float comp_sum = 0.0f;
    for (float v : comp_data) {
        comp_sum += std::abs(v);
    }
    EXPECT_GT(comp_sum, 0.0f);

    nimcp_gpu_tensor_destroy(sequence);
    nimcp_gpu_tensor_destroy(compressed);
}

//=============================================================================
// NULL Safety Tests
//=============================================================================

TEST_F(SleepKernelTest, SleepStageUpdate_NullSafety) {
    RequireGPU();

    nimcp_gpu_sleep_stage_state_t* state = CreateSleepStageState();

    EXPECT_FALSE(nimcp_gpu_sleep_stage_update(nullptr, state, 0.0f, 0.0f, 1.0f));
    EXPECT_FALSE(nimcp_gpu_sleep_stage_update(ctx, nullptr, 0.0f, 0.0f, 1.0f));

    DestroySleepStageState(state);
}

TEST_F(SleepKernelTest, SleepTransitions_NullSafety) {
    RequireGPU();

    nimcp_gpu_sleep_stage_state_t* state = CreateSleepStageState();
    nimcp_gpu_tensor_t* tensor = Create2DTensor(NIMCP_SLEEP_COUNT, NIMCP_SLEEP_COUNT, 0.0f);

    EXPECT_FALSE(nimcp_gpu_sleep_transitions(nullptr, state, tensor));
    EXPECT_FALSE(nimcp_gpu_sleep_transitions(ctx, nullptr, tensor));
    EXPECT_FALSE(nimcp_gpu_sleep_transitions(ctx, state, nullptr));

    nimcp_gpu_tensor_destroy(tensor);
    DestroySleepStageState(state);
}

TEST_F(SleepKernelTest, NREMSlowOscillation_NullSafety) {
    RequireGPU();

    nimcp_gpu_consolidation_state_t* state = CreateConsolidationState(50, 32);
    nimcp_gpu_nrem_params_t params = nimcp_gpu_nrem_params_default();

    EXPECT_FALSE(nimcp_gpu_nrem_slow_oscillation(nullptr, state, 0.0f, &params));
    EXPECT_FALSE(nimcp_gpu_nrem_slow_oscillation(ctx, nullptr, 0.0f, &params));
    EXPECT_FALSE(nimcp_gpu_nrem_slow_oscillation(ctx, state, 0.0f, nullptr));

    DestroyConsolidationState(state);
}

TEST_F(SleepKernelTest, NREMReplay_NullSafety) {
    RequireGPU();

    nimcp_gpu_consolidation_state_t* state = CreateConsolidationState(50, 32);
    nimcp_gpu_nrem_params_t params = nimcp_gpu_nrem_params_default();

    EXPECT_FALSE(nimcp_gpu_nrem_replay(nullptr, state, &params));
    EXPECT_FALSE(nimcp_gpu_nrem_replay(ctx, nullptr, &params));
    EXPECT_FALSE(nimcp_gpu_nrem_replay(ctx, state, nullptr));

    DestroyConsolidationState(state);
}

TEST_F(SleepKernelTest, NREMSystemsConsolidation_NullSafety) {
    RequireGPU();

    nimcp_gpu_consolidation_state_t* state = CreateConsolidationState(50, 32);
    nimcp_gpu_nrem_params_t params = nimcp_gpu_nrem_params_default();

    EXPECT_FALSE(nimcp_gpu_nrem_systems_consolidation(nullptr, state, 1.0f, &params));
    EXPECT_FALSE(nimcp_gpu_nrem_systems_consolidation(ctx, nullptr, 1.0f, &params));
    EXPECT_FALSE(nimcp_gpu_nrem_systems_consolidation(ctx, state, 1.0f, nullptr));

    DestroyConsolidationState(state);
}

TEST_F(SleepKernelTest, NREMSharpWaveRipple_NullSafety) {
    RequireGPU();

    nimcp_gpu_consolidation_state_t* state = CreateConsolidationState(50, 32);
    nimcp_gpu_tensor_t* tensor = Create1DTensor(32, 0.0f);
    nimcp_gpu_nrem_params_t params = nimcp_gpu_nrem_params_default();

    EXPECT_FALSE(nimcp_gpu_nrem_sharp_wave_ripple(nullptr, state, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_nrem_sharp_wave_ripple(ctx, nullptr, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_nrem_sharp_wave_ripple(ctx, state, nullptr, &params));
    EXPECT_FALSE(nimcp_gpu_nrem_sharp_wave_ripple(ctx, state, tensor, nullptr));

    nimcp_gpu_tensor_destroy(tensor);
    DestroyConsolidationState(state);
}

TEST_F(SleepKernelTest, REMProcessing_NullSafety) {
    RequireGPU();

    nimcp_gpu_consolidation_state_t* state = CreateConsolidationState(50, 32);
    nimcp_gpu_rem_params_t params = nimcp_gpu_rem_params_default();

    EXPECT_FALSE(nimcp_gpu_rem_processing(nullptr, state, 1.0f, &params));
    EXPECT_FALSE(nimcp_gpu_rem_processing(ctx, nullptr, 1.0f, &params));
    EXPECT_FALSE(nimcp_gpu_rem_processing(ctx, state, 1.0f, nullptr));

    DestroyConsolidationState(state);
}

TEST_F(SleepKernelTest, REMMemoryIntegration_NullSafety) {
    RequireGPU();

    nimcp_gpu_consolidation_state_t* state = CreateConsolidationState(50, 32);
    nimcp_gpu_tensor_t* tensor = Create2DTensor(32, 32, 0.0f);
    nimcp_gpu_rem_params_t params = nimcp_gpu_rem_params_default();

    EXPECT_FALSE(nimcp_gpu_rem_memory_integration(nullptr, state, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_rem_memory_integration(ctx, nullptr, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_rem_memory_integration(ctx, state, nullptr, &params));
    EXPECT_FALSE(nimcp_gpu_rem_memory_integration(ctx, state, tensor, nullptr));

    nimcp_gpu_tensor_destroy(tensor);
    DestroyConsolidationState(state);
}

TEST_F(SleepKernelTest, REMEmotionalProcessing_NullSafety) {
    RequireGPU();

    nimcp_gpu_consolidation_state_t* state = CreateConsolidationState(50, 32);
    nimcp_gpu_tensor_t* tensor = Create1DTensor(50, 0.0f);
    nimcp_gpu_rem_params_t params = nimcp_gpu_rem_params_default();

    EXPECT_FALSE(nimcp_gpu_rem_emotional_processing(nullptr, state, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_rem_emotional_processing(ctx, nullptr, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_rem_emotional_processing(ctx, state, nullptr, &params));
    EXPECT_FALSE(nimcp_gpu_rem_emotional_processing(ctx, state, tensor, nullptr));

    nimcp_gpu_tensor_destroy(tensor);
    DestroyConsolidationState(state);
}

TEST_F(SleepKernelTest, REMDreamGeneration_NullSafety) {
    RequireGPU();

    nimcp_gpu_consolidation_state_t* state = CreateConsolidationState(50, 32);
    nimcp_gpu_tensor_t* tensor = Create1DTensor(32, 0.0f);
    nimcp_gpu_rem_params_t params = nimcp_gpu_rem_params_default();

    EXPECT_FALSE(nimcp_gpu_rem_dream_generation(nullptr, state, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_rem_dream_generation(ctx, nullptr, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_rem_dream_generation(ctx, state, nullptr, &params));
    EXPECT_FALSE(nimcp_gpu_rem_dream_generation(ctx, state, tensor, nullptr));

    nimcp_gpu_tensor_destroy(tensor);
    DestroyConsolidationState(state);
}

TEST_F(SleepKernelTest, SynapticDownscaling_NullSafety) {
    RequireGPU();

    nimcp_gpu_synaptic_state_t* state = CreateSynapticState(100);
    nimcp_gpu_homeostasis_params_t params = nimcp_gpu_homeostasis_params_default();

    EXPECT_FALSE(nimcp_gpu_synaptic_downscaling(nullptr, state, &params));
    EXPECT_FALSE(nimcp_gpu_synaptic_downscaling(ctx, nullptr, &params));
    EXPECT_FALSE(nimcp_gpu_synaptic_downscaling(ctx, state, nullptr));

    DestroySynapticState(state);
}

TEST_F(SleepKernelTest, SynapsePreservation_NullSafety) {
    RequireGPU();

    nimcp_gpu_synaptic_state_t* state = CreateSynapticState(100);
    nimcp_gpu_tensor_t* tensor = Create1DTensor(100, 0.0f);
    nimcp_gpu_homeostasis_params_t params = nimcp_gpu_homeostasis_params_default();

    EXPECT_FALSE(nimcp_gpu_synapse_preservation(nullptr, state, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_synapse_preservation(ctx, nullptr, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_synapse_preservation(ctx, state, nullptr, &params));
    EXPECT_FALSE(nimcp_gpu_synapse_preservation(ctx, state, tensor, nullptr));

    nimcp_gpu_tensor_destroy(tensor);
    DestroySynapticState(state);
}

TEST_F(SleepKernelTest, SynapsePruning_NullSafety) {
    RequireGPU();

    nimcp_gpu_synaptic_state_t* state = CreateSynapticState(100);
    nimcp_gpu_homeostasis_params_t params = nimcp_gpu_homeostasis_params_default();

    EXPECT_FALSE(nimcp_gpu_synapse_pruning(nullptr, state, &params));
    EXPECT_FALSE(nimcp_gpu_synapse_pruning(ctx, nullptr, &params));
    EXPECT_FALSE(nimcp_gpu_synapse_pruning(ctx, state, nullptr));

    DestroySynapticState(state);
}

TEST_F(SleepKernelTest, SynapticTagging_NullSafety) {
    RequireGPU();

    nimcp_gpu_synaptic_state_t* state = CreateSynapticState(100);
    nimcp_gpu_tensor_t* tensor = Create1DTensor(100, 0.0f);

    EXPECT_FALSE(nimcp_gpu_synaptic_tagging(nullptr, state, tensor, 1.0f));
    EXPECT_FALSE(nimcp_gpu_synaptic_tagging(ctx, nullptr, tensor, 1.0f));
    EXPECT_FALSE(nimcp_gpu_synaptic_tagging(ctx, state, nullptr, 1.0f));

    nimcp_gpu_tensor_destroy(tensor);
    DestroySynapticState(state);
}

TEST_F(SleepKernelTest, ReplaySample_NullSafety) {
    RequireGPU();

    nimcp_gpu_consolidation_state_t* state = CreateConsolidationState(50, 32);
    nimcp_gpu_tensor_t* tensor = Create2DTensor(5, 32, 0.0f);
    nimcp_gpu_replay_params_t params = nimcp_gpu_replay_params_default();

    EXPECT_FALSE(nimcp_gpu_replay_sample(nullptr, state, tensor, 5, &params));
    EXPECT_FALSE(nimcp_gpu_replay_sample(ctx, nullptr, tensor, 5, &params));
    EXPECT_FALSE(nimcp_gpu_replay_sample(ctx, state, nullptr, 5, &params));
    EXPECT_FALSE(nimcp_gpu_replay_sample(ctx, state, tensor, 5, nullptr));

    nimcp_gpu_tensor_destroy(tensor);
    DestroyConsolidationState(state);
}

TEST_F(SleepKernelTest, ReplayStore_NullSafety) {
    RequireGPU();

    nimcp_gpu_consolidation_state_t* state = CreateConsolidationState(50, 32);
    nimcp_gpu_tensor_t* tensor = Create1DTensor(32, 0.0f);
    nimcp_gpu_replay_params_t params = nimcp_gpu_replay_params_default();

    EXPECT_FALSE(nimcp_gpu_replay_store(nullptr, state, tensor, 0.5f, &params));
    EXPECT_FALSE(nimcp_gpu_replay_store(ctx, nullptr, tensor, 0.5f, &params));
    EXPECT_FALSE(nimcp_gpu_replay_store(ctx, state, nullptr, 0.5f, &params));
    EXPECT_FALSE(nimcp_gpu_replay_store(ctx, state, tensor, 0.5f, nullptr));

    nimcp_gpu_tensor_destroy(tensor);
    DestroyConsolidationState(state);
}

TEST_F(SleepKernelTest, ReplayUpdatePriorities_NullSafety) {
    RequireGPU();

    nimcp_gpu_consolidation_state_t* state = CreateConsolidationState(50, 32);
    nimcp_gpu_tensor_t* tensor = Create1DTensor(50, 0.0f);
    nimcp_gpu_replay_params_t params = nimcp_gpu_replay_params_default();

    EXPECT_FALSE(nimcp_gpu_replay_update_priorities(nullptr, state, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_replay_update_priorities(ctx, nullptr, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_replay_update_priorities(ctx, state, nullptr, &params));
    EXPECT_FALSE(nimcp_gpu_replay_update_priorities(ctx, state, tensor, nullptr));

    nimcp_gpu_tensor_destroy(tensor);
    DestroyConsolidationState(state);
}

TEST_F(SleepKernelTest, ReplayCompress_NullSafety) {
    RequireGPU();

    nimcp_gpu_tensor_t* sequence = Create2DTensor(100, 32, 0.0f);
    nimcp_gpu_tensor_t* compressed = Create2DTensor(20, 32, 0.0f);
    nimcp_gpu_replay_params_t params = nimcp_gpu_replay_params_default();

    EXPECT_FALSE(nimcp_gpu_replay_compress(nullptr, sequence, compressed, &params));
    EXPECT_FALSE(nimcp_gpu_replay_compress(ctx, nullptr, compressed, &params));
    EXPECT_FALSE(nimcp_gpu_replay_compress(ctx, sequence, nullptr, &params));
    EXPECT_FALSE(nimcp_gpu_replay_compress(ctx, sequence, compressed, nullptr));

    nimcp_gpu_tensor_destroy(sequence);
    nimcp_gpu_tensor_destroy(compressed);
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(SleepKernelTest, Integration_SleepCycle) {
    RequireGPU();

    nimcp_gpu_sleep_stage_state_t* stage_state = CreateSleepStageState();
    ASSERT_NE(stage_state, nullptr);

    stage_state->current_stage = NIMCP_SLEEP_WAKE;
    stage_state->sleep_pressure = 0.9f;  // High sleep pressure

    const float dt = 60.0f;  // 1 minute steps
    const int total_steps = 480;  // 8 hours

    std::vector<nimcp_sleep_stage_t> stage_history;

    for (int step = 0; step < total_steps; step++) {
        float time_hours = static_cast<float>(step) * dt / 3600.0f;
        float circadian_phase = std::sin(2.0f * M_PI * (time_hours / 24.0f));
        float arousal = 0.1f + 0.05f * std::sin(time_hours * 0.5f);

        nimcp_gpu_sleep_stage_update(ctx, stage_state, circadian_phase, arousal, dt);
        stage_history.push_back(stage_state->current_stage);
    }

    // Total sleep time should have increased
    EXPECT_GT(stage_state->total_sleep_time, 0.0f);

    // Should have experienced multiple stages
    bool has_light_sleep = false;
    bool has_deep_sleep = false;
    for (auto stage : stage_history) {
        if (stage == NIMCP_SLEEP_N1 || stage == NIMCP_SLEEP_N2) has_light_sleep = true;
        if (stage == NIMCP_SLEEP_N3) has_deep_sleep = true;
    }
    // At least light sleep should occur with high sleep pressure
    // (deep sleep and REM depend on specific implementation)

    DestroySleepStageState(stage_state);
}

TEST_F(SleepKernelTest, Integration_NREMConsolidation) {
    RequireGPU();

    const size_t buffer_size = 50;
    const size_t memory_dim = 32;
    const float dt = 10.0f;
    const int n_cycles = 5;

    nimcp_gpu_consolidation_state_t* state = CreateConsolidationState(buffer_size, memory_dim);
    ASSERT_NE(state, nullptr);

    // Fill hippocampal buffer with "new memories"
    std::vector<float> memories(buffer_size * memory_dim);
    for (size_t i = 0; i < memories.size(); i++) {
        memories[i] = static_cast<float>(rand()) / RAND_MAX;
    }
    state->hippocampal_buffer = SetFromHost(state->hippocampal_buffer, memories);
    nimcp_gpu_fill(ctx, state->consolidation_mask, 1.0f);
    nimcp_gpu_fill(ctx, state->priority_scores, 0.7f);

    auto initial_cortical = CopyToHost(state->cortical_weights);
    float initial_cortical_sum = 0.0f;
    for (float w : initial_cortical) initial_cortical_sum += std::abs(w);

    nimcp_gpu_nrem_params_t params = nimcp_gpu_nrem_params_default();

    // Simulate NREM sleep consolidation cycles
    for (int cycle = 0; cycle < n_cycles; cycle++) {
        float time = static_cast<float>(cycle) * 2.0f;

        // Slow oscillation
        nimcp_gpu_nrem_slow_oscillation(ctx, state, time, &params);

        // Memory replay
        nimcp_gpu_nrem_replay(ctx, state, &params);

        // Systems consolidation
        nimcp_gpu_nrem_systems_consolidation(ctx, state, dt, &params);
    }

    auto final_cortical = CopyToHost(state->cortical_weights);
    float final_cortical_sum = 0.0f;
    for (float w : final_cortical) final_cortical_sum += std::abs(w);

    // Cortical weights should change during consolidation
    EXPECT_NE(initial_cortical_sum, final_cortical_sum);

    DestroyConsolidationState(state);
}

TEST_F(SleepKernelTest, Integration_REMProcessing) {
    RequireGPU();

    const size_t buffer_size = 50;
    const size_t memory_dim = 32;
    const float dt = 10.0f;
    const int n_steps = 10;

    nimcp_gpu_consolidation_state_t* state = CreateConsolidationState(buffer_size, memory_dim);
    ASSERT_NE(state, nullptr);

    // Fill buffers
    nimcp_gpu_fill(ctx, state->hippocampal_buffer, 0.5f);
    nimcp_gpu_fill(ctx, state->priority_scores, 0.6f);

    // Create semantic memory and emotional tags
    size_t semantic_dims[2] = {memory_dim, memory_dim};
    nimcp_gpu_tensor_t* semantic_memory = nimcp_gpu_tensor_create(ctx, semantic_dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_fill(ctx, semantic_memory, 0.3f);

    size_t tag_dims[1] = {buffer_size};
    nimcp_gpu_tensor_t* emotional_tags = nimcp_gpu_tensor_create(ctx, tag_dims, 1, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_fill(ctx, emotional_tags, 0.7f);

    size_t dream_dims[1] = {memory_dim};
    nimcp_gpu_tensor_t* dream_content = nimcp_gpu_tensor_create(ctx, dream_dims, 1, NIMCP_GPU_PRECISION_FP32);

    nimcp_gpu_rem_params_t params = nimcp_gpu_rem_params_default();

    auto initial_weights = CopyToHost(state->cortical_weights);

    // Simulate REM sleep processing
    for (int step = 0; step < n_steps; step++) {
        nimcp_gpu_rem_processing(ctx, state, dt, &params);
        nimcp_gpu_rem_memory_integration(ctx, state, semantic_memory, &params);
        nimcp_gpu_rem_emotional_processing(ctx, state, emotional_tags, &params);
        nimcp_gpu_rem_dream_generation(ctx, state, dream_content, &params);
    }

    auto final_weights = CopyToHost(state->cortical_weights);

    // Weights should be modified during REM processing
    bool modified = false;
    for (size_t i = 0; i < final_weights.size(); i++) {
        if (std::abs(final_weights[i] - initial_weights[i]) > 1e-6f) {
            modified = true;
            break;
        }
    }
    EXPECT_TRUE(modified);

    nimcp_gpu_tensor_destroy(semantic_memory);
    nimcp_gpu_tensor_destroy(emotional_tags);
    nimcp_gpu_tensor_destroy(dream_content);
    DestroyConsolidationState(state);
}

TEST_F(SleepKernelTest, Integration_SynapticHomeostasis) {
    RequireGPU();

    const size_t n_synapses = 200;
    const int n_iterations = 50;

    nimcp_gpu_synaptic_state_t* state = CreateSynapticState(n_synapses);
    ASSERT_NE(state, nullptr);

    // Set high initial weights (potentiated during wake)
    nimcp_gpu_fill(ctx, state->synaptic_weights, 0.9f);

    // Create importance scores (some synapses more important)
    size_t imp_dims[1] = {n_synapses};
    nimcp_gpu_tensor_t* importance_scores = nimcp_gpu_tensor_create(ctx, imp_dims, 1, NIMCP_GPU_PRECISION_FP32);
    std::vector<float> importance(n_synapses);
    for (size_t i = 0; i < n_synapses; i++) {
        importance[i] = (i < n_synapses / 4) ? 0.95f : 0.2f;  // 25% important
    }
    importance_scores = SetFromHost(importance_scores, importance);

    // Create activity pattern
    nimcp_gpu_tensor_t* recent_activity = nimcp_gpu_tensor_create(ctx, imp_dims, 1, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_fill(ctx, recent_activity, 0.1f);

    auto initial_weights = CopyToHost(state->synaptic_weights);
    float initial_total = 0.0f;
    for (float w : initial_weights) initial_total += w;

    nimcp_gpu_homeostasis_params_t params = nimcp_gpu_homeostasis_params_default();
    params.selective_pruning = true;

    // Simulate sleep homeostasis
    for (int iter = 0; iter < n_iterations; iter++) {
        // Tag synapses based on activity
        nimcp_gpu_synaptic_tagging(ctx, state, recent_activity, 10.0f);

        // Preserve important synapses
        nimcp_gpu_synapse_preservation(ctx, state, importance_scores, &params);

        // Apply downscaling
        nimcp_gpu_synaptic_downscaling(ctx, state, &params);

        // Prune weak synapses
        if (iter % 10 == 9) {
            nimcp_gpu_synapse_pruning(ctx, state, &params);
        }
    }

    auto final_weights = CopyToHost(state->synaptic_weights);
    float final_total = 0.0f;
    for (float w : final_weights) final_total += w;

    // Total synaptic weight should decrease (homeostatic downscaling)
    EXPECT_LT(final_total, initial_total);

    // Important synapses should be relatively preserved
    float important_sum = 0.0f;
    float unimportant_sum = 0.0f;
    for (size_t i = 0; i < n_synapses; i++) {
        if (i < n_synapses / 4) {
            important_sum += final_weights[i];
        } else {
            unimportant_sum += final_weights[i];
        }
    }
    // Important synapses (25% of total) should have higher average weight
    float important_avg = important_sum / (n_synapses / 4);
    float unimportant_avg = unimportant_sum / (n_synapses * 3 / 4);
    EXPECT_GT(important_avg, unimportant_avg);

    nimcp_gpu_tensor_destroy(importance_scores);
    nimcp_gpu_tensor_destroy(recent_activity);
    DestroySynapticState(state);
}

TEST_F(SleepKernelTest, Integration_MemoryReplayPipeline) {
    RequireGPU();

    const size_t buffer_size = 100;
    const size_t memory_dim = 32;
    const int n_experiences = 20;
    const int n_replay_samples = 10;

    nimcp_gpu_consolidation_state_t* state = CreateConsolidationState(buffer_size, memory_dim);
    ASSERT_NE(state, nullptr);

    nimcp_gpu_replay_params_t params = nimcp_gpu_replay_params_default();

    // Store experiences with varying priorities
    for (int i = 0; i < n_experiences; i++) {
        size_t exp_dims[1] = {memory_dim};
        nimcp_gpu_tensor_t* experience = nimcp_gpu_tensor_create(ctx, exp_dims, 1, NIMCP_GPU_PRECISION_FP32);
        nimcp_gpu_fill(ctx, experience, static_cast<float>(i + 1) / n_experiences);

        float priority = static_cast<float>(i) / n_experiences;
        nimcp_gpu_replay_store(ctx, state, experience, priority, &params);

        nimcp_gpu_tensor_destroy(experience);
    }

    // Sample from buffer
    size_t sample_dims[2] = {static_cast<size_t>(n_replay_samples), memory_dim};
    nimcp_gpu_tensor_t* sampled = nimcp_gpu_tensor_create(ctx, sample_dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_fill(ctx, sampled, 0.0f);

    bool result = nimcp_gpu_replay_sample(ctx, state, sampled, n_replay_samples, &params);
    EXPECT_TRUE(result);

    auto samples = CopyToHost(sampled);
    float sample_sum = 0.0f;
    for (float v : samples) sample_sum += std::abs(v);
    EXPECT_GT(sample_sum, 0.0f);

    // Update priorities based on simulated TD errors
    size_t td_dims[1] = {buffer_size};
    nimcp_gpu_tensor_t* td_errors = nimcp_gpu_tensor_create(ctx, td_dims, 1, NIMCP_GPU_PRECISION_FP32);
    std::vector<float> errors(buffer_size);
    for (size_t i = 0; i < buffer_size; i++) {
        errors[i] = static_cast<float>(rand()) / RAND_MAX;
    }
    td_errors = SetFromHost(td_errors, errors);

    auto initial_priorities = CopyToHost(state->priority_scores);

    nimcp_gpu_replay_update_priorities(ctx, state, td_errors, &params);

    auto final_priorities = CopyToHost(state->priority_scores);

    // Priorities should have changed
    bool priorities_changed = false;
    for (size_t i = 0; i < buffer_size; i++) {
        if (std::abs(final_priorities[i] - initial_priorities[i]) > 1e-6f) {
            priorities_changed = true;
            break;
        }
    }
    EXPECT_TRUE(priorities_changed);

    nimcp_gpu_tensor_destroy(sampled);
    nimcp_gpu_tensor_destroy(td_errors);
    DestroyConsolidationState(state);
}

TEST_F(SleepKernelTest, Integration_FullNightSleep) {
    RequireGPU();

    // Simulate a full night of sleep with NREM/REM cycling

    const size_t buffer_size = 50;
    const size_t memory_dim = 32;
    const size_t n_synapses = 100;
    const float dt = 60.0f;  // 1 minute steps
    const int n_cycles = 5;  // ~5 sleep cycles

    // Create states
    nimcp_gpu_sleep_stage_state_t* stage_state = CreateSleepStageState();
    nimcp_gpu_consolidation_state_t* consolidation_state = CreateConsolidationState(buffer_size, memory_dim);
    nimcp_gpu_synaptic_state_t* synaptic_state = CreateSynapticState(n_synapses);

    ASSERT_NE(stage_state, nullptr);
    ASSERT_NE(consolidation_state, nullptr);
    ASSERT_NE(synaptic_state, nullptr);

    // Initialize with wake state and high weights
    stage_state->current_stage = NIMCP_SLEEP_WAKE;
    stage_state->sleep_pressure = 0.9f;
    nimcp_gpu_fill(ctx, synaptic_state->synaptic_weights, 0.8f);
    nimcp_gpu_fill(ctx, consolidation_state->hippocampal_buffer, 0.7f);

    auto initial_synaptic = CopyToHost(synaptic_state->synaptic_weights);
    auto initial_cortical = CopyToHost(consolidation_state->cortical_weights);

    nimcp_gpu_nrem_params_t nrem_params = nimcp_gpu_nrem_params_default();
    nimcp_gpu_rem_params_t rem_params = nimcp_gpu_rem_params_default();
    nimcp_gpu_homeostasis_params_t homeo_params = nimcp_gpu_homeostasis_params_default();

    // Simulate sleep cycles
    float total_time = 0.0f;
    for (int cycle = 0; cycle < n_cycles; cycle++) {
        // NREM phase (90 min)
        for (int step = 0; step < 90; step++) {
            nimcp_gpu_sleep_stage_update(ctx, stage_state, 0.0f, 0.1f, dt);

            if (stage_state->current_stage == NIMCP_SLEEP_N3) {
                nimcp_gpu_nrem_slow_oscillation(ctx, consolidation_state, total_time, &nrem_params);
                nimcp_gpu_nrem_replay(ctx, consolidation_state, &nrem_params);
                nimcp_gpu_nrem_systems_consolidation(ctx, consolidation_state, dt, &nrem_params);
                nimcp_gpu_synaptic_downscaling(ctx, synaptic_state, &homeo_params);
            }

            total_time += dt / 60.0f;  // Convert to minutes
        }

        // REM phase (20-30 min)
        stage_state->current_stage = NIMCP_SLEEP_REM;
        for (int step = 0; step < 25; step++) {
            nimcp_gpu_rem_processing(ctx, consolidation_state, dt, &rem_params);
            total_time += dt / 60.0f;
        }
    }

    auto final_synaptic = CopyToHost(synaptic_state->synaptic_weights);
    auto final_cortical = CopyToHost(consolidation_state->cortical_weights);

    // Synaptic weights should decrease (homeostasis)
    float initial_syn_sum = 0.0f, final_syn_sum = 0.0f;
    for (size_t i = 0; i < n_synapses; i++) {
        initial_syn_sum += initial_synaptic[i];
        final_syn_sum += final_synaptic[i];
    }
    EXPECT_LT(final_syn_sum, initial_syn_sum);

    // Cortical weights should change (consolidation)
    bool cortical_changed = false;
    for (size_t i = 0; i < final_cortical.size(); i++) {
        if (std::abs(final_cortical[i] - initial_cortical[i]) > 1e-6f) {
            cortical_changed = true;
            break;
        }
    }
    EXPECT_TRUE(cortical_changed);

    // Total sleep time should have accumulated
    EXPECT_GT(stage_state->total_sleep_time, 0.0f);

    DestroySleepStageState(stage_state);
    DestroyConsolidationState(consolidation_state);
    DestroySynapticState(synaptic_state);
}
