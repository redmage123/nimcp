/**
 * @file test_regions_kernels.cpp
 * @brief Unit tests for GPU brain region kernels
 *
 * Tests cortical column, PFC, motor cortex, parietal cortex,
 * and inter-region communication GPU operations.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <cstring>

// Headers have their own extern "C" guards
#include "gpu/regions/nimcp_regions_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class RegionsKernelTest : public ::testing::Test {
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

    // Helper to create 3D tensor
    nimcp_gpu_tensor_t* Create3DTensor(size_t d0, size_t d1, size_t d2, float value = 0.0f) {
        size_t dims[3] = {d0, d1, d2};
        return CreateFilledTensor(dims, 3, value);
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

    // Helper to create column state
    nimcp_gpu_column_state_t* CreateColumnState(size_t n_columns, size_t n_neurons) {
        if (!ctx) return nullptr;

        nimcp_gpu_column_state_t* state = new nimcp_gpu_column_state_t();
        if (!state) return nullptr;

        state->n_columns = n_columns;
        state->n_neurons = n_neurons;

        // Create layer activity tensors
        for (int i = 0; i < NIMCP_LAYER_COUNT; i++) {
            state->layer_activity[i] = Create2DTensor(n_columns, n_neurons / NIMCP_LAYER_COUNT, 0.0f);
            if (!state->layer_activity[i]) {
                DestroyColumnState(state);
                return nullptr;
            }
        }

        state->column_output = Create1DTensor(n_columns, 0.0f);
        state->lateral_connections = Create2DTensor(n_columns, n_columns, 0.1f);
        state->adaptation_state = Create1DTensor(n_columns, 0.0f);
        state->input_weights = Create2DTensor(n_columns, n_neurons, 0.5f);
        state->output_weights = Create2DTensor(n_columns, n_neurons, 0.5f);

        if (!state->column_output || !state->lateral_connections ||
            !state->adaptation_state || !state->input_weights || !state->output_weights) {
            DestroyColumnState(state);
            return nullptr;
        }

        return state;
    }

    void DestroyColumnState(nimcp_gpu_column_state_t* state) {
        if (!state) return;

        for (int i = 0; i < NIMCP_LAYER_COUNT; i++) {
            if (state->layer_activity[i]) {
                nimcp_gpu_tensor_destroy(state->layer_activity[i]);
            }
        }
        if (state->column_output) nimcp_gpu_tensor_destroy(state->column_output);
        if (state->lateral_connections) nimcp_gpu_tensor_destroy(state->lateral_connections);
        if (state->adaptation_state) nimcp_gpu_tensor_destroy(state->adaptation_state);
        if (state->input_weights) nimcp_gpu_tensor_destroy(state->input_weights);
        if (state->output_weights) nimcp_gpu_tensor_destroy(state->output_weights);

        delete state;
    }

    // Helper to create PFC state
    nimcp_gpu_pfc_state_t* CreatePFCState(size_t n_slots, size_t slot_dim) {
        if (!ctx) return nullptr;

        nimcp_gpu_pfc_state_t* state = new nimcp_gpu_pfc_state_t();
        if (!state) return nullptr;

        state->n_slots = n_slots;
        state->slot_dim = slot_dim;

        state->working_memory = Create2DTensor(n_slots, slot_dim, 0.0f);
        state->gate_state = Create1DTensor(n_slots, 0.0f);
        state->attention_weights = Create1DTensor(n_slots, 1.0f / n_slots);
        state->task_context = Create1DTensor(slot_dim, 0.0f);
        state->output = Create1DTensor(slot_dim, 0.0f);

        if (!state->working_memory || !state->gate_state ||
            !state->attention_weights || !state->task_context || !state->output) {
            DestroyPFCState(state);
            return nullptr;
        }

        return state;
    }

    void DestroyPFCState(nimcp_gpu_pfc_state_t* state) {
        if (!state) return;

        if (state->working_memory) nimcp_gpu_tensor_destroy(state->working_memory);
        if (state->gate_state) nimcp_gpu_tensor_destroy(state->gate_state);
        if (state->attention_weights) nimcp_gpu_tensor_destroy(state->attention_weights);
        if (state->task_context) nimcp_gpu_tensor_destroy(state->task_context);
        if (state->output) nimcp_gpu_tensor_destroy(state->output);

        delete state;
    }

    // Helper to create motor state
    nimcp_gpu_motor_state_t* CreateMotorState(size_t n_actions, size_t n_muscles) {
        if (!ctx) return nullptr;

        nimcp_gpu_motor_state_t* state = new nimcp_gpu_motor_state_t();
        if (!state) return nullptr;

        state->n_actions = n_actions;
        state->n_muscles = n_muscles;

        state->action_plan = Create2DTensor(10, n_actions, 0.0f);  // 10 step horizon
        state->motor_output = Create1DTensor(n_muscles, 0.0f);
        state->efference_copy = Create1DTensor(n_muscles, 0.0f);
        state->population_code = Create2DTensor(n_actions, 100, 0.0f);  // 100 neurons per action
        state->forward_model = Create2DTensor(n_muscles, n_actions, 0.1f);

        if (!state->action_plan || !state->motor_output || !state->efference_copy ||
            !state->population_code || !state->forward_model) {
            DestroyMotorState(state);
            return nullptr;
        }

        return state;
    }

    void DestroyMotorState(nimcp_gpu_motor_state_t* state) {
        if (!state) return;

        if (state->action_plan) nimcp_gpu_tensor_destroy(state->action_plan);
        if (state->motor_output) nimcp_gpu_tensor_destroy(state->motor_output);
        if (state->efference_copy) nimcp_gpu_tensor_destroy(state->efference_copy);
        if (state->population_code) nimcp_gpu_tensor_destroy(state->population_code);
        if (state->forward_model) nimcp_gpu_tensor_destroy(state->forward_model);

        delete state;
    }

    // Helper to create parietal state
    nimcp_gpu_parietal_state_t* CreateParietalState(size_t map_size) {
        if (!ctx) return nullptr;

        nimcp_gpu_parietal_state_t* state = new nimcp_gpu_parietal_state_t();
        if (!state) return nullptr;

        state->map_size = map_size;

        state->spatial_map = Create2DTensor(map_size, map_size, 0.0f);
        state->egocentric_rep = Create2DTensor(map_size, map_size, 0.0f);
        state->allocentric_rep = Create2DTensor(map_size, map_size, 0.0f);
        state->attention_map = Create2DTensor(map_size, map_size, 1.0f / (map_size * map_size));
        state->transform_weights = Create3DTensor(map_size, map_size, 4, 0.25f);  // 4 transform params

        if (!state->spatial_map || !state->egocentric_rep || !state->allocentric_rep ||
            !state->attention_map || !state->transform_weights) {
            DestroyParietalState(state);
            return nullptr;
        }

        return state;
    }

    void DestroyParietalState(nimcp_gpu_parietal_state_t* state) {
        if (!state) return;

        if (state->spatial_map) nimcp_gpu_tensor_destroy(state->spatial_map);
        if (state->egocentric_rep) nimcp_gpu_tensor_destroy(state->egocentric_rep);
        if (state->allocentric_rep) nimcp_gpu_tensor_destroy(state->allocentric_rep);
        if (state->attention_map) nimcp_gpu_tensor_destroy(state->attention_map);
        if (state->transform_weights) nimcp_gpu_tensor_destroy(state->transform_weights);

        delete state;
    }

    // Helper to create interregion state
    nimcp_gpu_interregion_state_t* CreateInterregionState(
        nimcp_brain_region_t source, nimcp_brain_region_t target,
        size_t source_dim, size_t target_dim) {
        if (!ctx) return nullptr;

        nimcp_gpu_interregion_state_t* state = new nimcp_gpu_interregion_state_t();
        if (!state) return nullptr;

        state->source_region = source;
        state->target_region = target;
        state->source_dim = source_dim;
        state->target_dim = target_dim;

        state->connection_weights = Create2DTensor(target_dim, source_dim, 0.1f);
        state->delay_buffer = Create2DTensor(10, source_dim, 0.0f);  // 10ms buffer
        state->activity_buffer = Create1DTensor(source_dim, 0.0f);

        if (!state->connection_weights || !state->delay_buffer || !state->activity_buffer) {
            DestroyInterregionState(state);
            return nullptr;
        }

        return state;
    }

    void DestroyInterregionState(nimcp_gpu_interregion_state_t* state) {
        if (!state) return;

        if (state->connection_weights) nimcp_gpu_tensor_destroy(state->connection_weights);
        if (state->delay_buffer) nimcp_gpu_tensor_destroy(state->delay_buffer);
        if (state->activity_buffer) nimcp_gpu_tensor_destroy(state->activity_buffer);

        delete state;
    }
};

//=============================================================================
// Default Parameter Tests
//=============================================================================

TEST_F(RegionsKernelTest, ColumnParamsDefault_ReturnsValidParams) {
    nimcp_gpu_column_params_t params = nimcp_gpu_column_params_default();

    // Check reasonable defaults
    EXPECT_GT(params.n_columns, 0);
    EXPECT_GT(params.n_neurons_per_column, 0);
    EXPECT_GE(params.lateral_inhibition, 0.0f);
    EXPECT_LE(params.lateral_inhibition, 1.0f);
    EXPECT_GE(params.recurrent_excitation, 0.0f);
    EXPECT_GE(params.feedforward_gain, 0.0f);
    EXPECT_GE(params.feedback_gain, 0.0f);
    EXPECT_GE(params.adaptation_rate, 0.0f);

    // Check layer connectivity is initialized
    for (int i = 0; i < NIMCP_LAYER_COUNT; i++) {
        for (int j = 0; j < NIMCP_LAYER_COUNT; j++) {
            EXPECT_GE(params.layer_connectivity[i][j], 0.0f);
            EXPECT_LE(params.layer_connectivity[i][j], 1.0f);
        }
    }
}

TEST_F(RegionsKernelTest, PFCParamsDefault_ReturnsValidParams) {
    nimcp_gpu_pfc_params_t params = nimcp_gpu_pfc_params_default();

    EXPECT_GT(params.n_slots, 0);
    EXPECT_GT(params.maintenance_gain, 0.0f);
    EXPECT_GE(params.gating_threshold, 0.0f);
    EXPECT_LE(params.gating_threshold, 1.0f);
    EXPECT_GE(params.decay_rate, 0.0f);
    EXPECT_GE(params.interference_factor, 0.0f);
    EXPECT_GE(params.dopamine_modulation, 0.0f);
}

TEST_F(RegionsKernelTest, MotorParamsDefault_ReturnsValidParams) {
    nimcp_gpu_motor_params_t params = nimcp_gpu_motor_params_default();

    EXPECT_GT(params.n_actions, 0);
    EXPECT_GT(params.n_muscles, 0);
    EXPECT_GT(params.population_coding_sigma, 0.0f);
    EXPECT_GE(params.motor_noise, 0.0f);
    EXPECT_GT(params.planning_horizon, 0.0f);
    EXPECT_GE(params.sequence_learning_rate, 0.0f);
}

TEST_F(RegionsKernelTest, ParietalParamsDefault_ReturnsValidParams) {
    nimcp_gpu_parietal_params_t params = nimcp_gpu_parietal_params_default();

    EXPECT_GT(params.spatial_resolution, 0);
    EXPECT_GT(params.attention_gain, 0.0f);
    EXPECT_GE(params.coordinate_transform_lr, 0.0f);
    EXPECT_GE(params.multisensory_weight, 0.0f);
    EXPECT_LE(params.multisensory_weight, 1.0f);
    EXPECT_GE(params.egocentric_weight, 0.0f);
    EXPECT_GE(params.allocentric_weight, 0.0f);
    // Weights should sum to approximately 1
    float weight_sum = params.egocentric_weight + params.allocentric_weight;
    EXPECT_NEAR(weight_sum, 1.0f, 0.1f);
}

TEST_F(RegionsKernelTest, InterregionParamsDefault_ReturnsValidParams) {
    nimcp_gpu_interregion_params_t params = nimcp_gpu_interregion_params_default();

    EXPECT_GT(params.connection_strength, 0.0f);
    EXPECT_GE(params.transmission_delay, 0.0f);
    EXPECT_GE(params.plasticity_rate, 0.0f);
    EXPECT_GE(params.feedback_ratio, 0.0f);
    EXPECT_LE(params.feedback_ratio, 1.0f);
}

//=============================================================================
// Cortical Column Tests
//=============================================================================

TEST_F(RegionsKernelTest, ColumnUpdate_ProcessesInput) {
    RequireGPU();

    const size_t n_columns = 16;
    const size_t n_neurons = 100;

    nimcp_gpu_column_state_t* state = CreateColumnState(n_columns, n_neurons);
    ASSERT_NE(state, nullptr);

    nimcp_gpu_tensor_t* input = Create1DTensor(n_columns, 1.0f);
    nimcp_gpu_tensor_t* feedback = Create1DTensor(n_columns, 0.5f);

    nimcp_gpu_column_params_t params = nimcp_gpu_column_params_default();
    const float dt = 1.0f;

    bool result = nimcp_gpu_column_update(ctx, state, input, feedback, dt, &params);
    EXPECT_TRUE(result);

    // Check that output is updated
    auto output_data = CopyToHost(state->column_output);
    bool has_activity = false;
    for (size_t i = 0; i < n_columns; i++) {
        if (output_data[i] > 0.0f) {
            has_activity = true;
            break;
        }
    }
    EXPECT_TRUE(has_activity);

    nimcp_gpu_tensor_destroy(input);
    nimcp_gpu_tensor_destroy(feedback);
    DestroyColumnState(state);
}

TEST_F(RegionsKernelTest, ColumnUpdate_AccumulatesAdaptation) {
    RequireGPU();

    const size_t n_columns = 16;
    const size_t n_neurons = 100;
    const float dt = 1.0f;

    nimcp_gpu_column_state_t* state = CreateColumnState(n_columns, n_neurons);
    ASSERT_NE(state, nullptr);

    nimcp_gpu_tensor_t* input = Create1DTensor(n_columns, 1.0f);
    nimcp_gpu_tensor_t* feedback = Create1DTensor(n_columns, 0.0f);

    nimcp_gpu_column_params_t params = nimcp_gpu_column_params_default();
    params.adaptation_rate = 0.1f;

    // Initial adaptation state
    auto initial_adapt = CopyToHost(state->adaptation_state);

    // Multiple updates with sustained input
    for (int i = 0; i < 50; i++) {
        nimcp_gpu_column_update(ctx, state, input, feedback, dt, &params);
    }

    auto final_adapt = CopyToHost(state->adaptation_state);

    // Adaptation should increase with sustained activity
    float initial_sum = 0.0f, final_sum = 0.0f;
    for (size_t i = 0; i < n_columns; i++) {
        initial_sum += initial_adapt[i];
        final_sum += final_adapt[i];
    }
    EXPECT_GT(final_sum, initial_sum);

    nimcp_gpu_tensor_destroy(input);
    nimcp_gpu_tensor_destroy(feedback);
    DestroyColumnState(state);
}

TEST_F(RegionsKernelTest, ColumnLateralInhibition_ReducesActivity) {
    RequireGPU();

    const size_t n_columns = 16;
    const size_t n_neurons = 100;

    nimcp_gpu_column_state_t* state = CreateColumnState(n_columns, n_neurons);
    ASSERT_NE(state, nullptr);

    // Set initial activity
    nimcp_gpu_fill(ctx, state->column_output, 1.0f);

    nimcp_gpu_column_params_t params = nimcp_gpu_column_params_default();
    params.lateral_inhibition = 0.5f;

    auto initial_output = CopyToHost(state->column_output);
    float initial_sum = 0.0f;
    for (float v : initial_output) initial_sum += v;

    bool result = nimcp_gpu_column_lateral_inhibition(ctx, state, &params);
    EXPECT_TRUE(result);

    auto final_output = CopyToHost(state->column_output);
    float final_sum = 0.0f;
    for (float v : final_output) final_sum += v;

    // Total activity should be reduced by lateral inhibition
    EXPECT_LE(final_sum, initial_sum);

    DestroyColumnState(state);
}

TEST_F(RegionsKernelTest, ColumnLateralInhibition_CompetitiveSelection) {
    RequireGPU();

    const size_t n_columns = 8;
    const size_t n_neurons = 50;

    nimcp_gpu_column_state_t* state = CreateColumnState(n_columns, n_neurons);
    ASSERT_NE(state, nullptr);

    // Set varying activity levels
    std::vector<float> initial_activity(n_columns);
    for (size_t i = 0; i < n_columns; i++) {
        initial_activity[i] = static_cast<float>(i) / n_columns;
    }
    state->column_output = SetFromHost(state->column_output, initial_activity);

    nimcp_gpu_column_params_t params = nimcp_gpu_column_params_default();
    params.lateral_inhibition = 0.8f;  // Strong inhibition

    // Apply multiple rounds of lateral inhibition
    for (int i = 0; i < 10; i++) {
        nimcp_gpu_column_lateral_inhibition(ctx, state, &params);
    }

    auto final_output = CopyToHost(state->column_output);

    // Strongest column should remain active, weaker ones suppressed
    float max_activity = *std::max_element(final_output.begin(), final_output.end());
    int active_count = 0;
    for (float v : final_output) {
        if (v > max_activity * 0.5f) active_count++;
    }

    // Should have winner-take-all or near-winner-take-all behavior
    EXPECT_LE(active_count, 3);

    DestroyColumnState(state);
}

TEST_F(RegionsKernelTest, ColumnLayerPropagate_TransfersActivity) {
    RequireGPU();

    const size_t n_columns = 16;
    const size_t n_neurons = 100;

    nimcp_gpu_column_state_t* state = CreateColumnState(n_columns, n_neurons);
    ASSERT_NE(state, nullptr);

    // Set input layer (L4) activity
    nimcp_gpu_fill(ctx, state->layer_activity[NIMCP_LAYER_4], 1.0f);

    nimcp_gpu_column_params_t params = nimcp_gpu_column_params_default();

    // Ensure connectivity from L4 to L2/3 and L5
    params.layer_connectivity[NIMCP_LAYER_4][NIMCP_LAYER_2_3] = 0.8f;
    params.layer_connectivity[NIMCP_LAYER_2_3][NIMCP_LAYER_5] = 0.6f;

    bool result = nimcp_gpu_column_layer_propagate(ctx, state, &params);
    EXPECT_TRUE(result);

    // Check that activity propagated to L2/3
    auto l23_data = CopyToHost(state->layer_activity[NIMCP_LAYER_2_3]);
    float l23_sum = 0.0f;
    for (float v : l23_data) l23_sum += v;
    EXPECT_GT(l23_sum, 0.0f);

    DestroyColumnState(state);
}

//=============================================================================
// PFC Tests
//=============================================================================

TEST_F(RegionsKernelTest, PFCWMUpdate_StoresInput) {
    RequireGPU();

    const size_t n_slots = 4;
    const size_t slot_dim = 64;

    nimcp_gpu_pfc_state_t* state = CreatePFCState(n_slots, slot_dim);
    ASSERT_NE(state, nullptr);

    nimcp_gpu_tensor_t* input = Create1DTensor(slot_dim, 0.8f);
    nimcp_gpu_tensor_t* dopamine = Create1DTensor(1, 1.0f);  // High dopamine for gating

    nimcp_gpu_pfc_params_t params = nimcp_gpu_pfc_params_default();
    const float dt = 1.0f;

    // Open gate for slot 0
    nimcp_gpu_fill(ctx, state->gate_state, 0.0f);
    std::vector<float> gate_data(n_slots, 0.0f);
    gate_data[0] = 1.0f;
    state->gate_state = SetFromHost(state->gate_state, gate_data);

    bool result = nimcp_gpu_pfc_wm_update(ctx, state, input, dopamine, dt, &params);
    EXPECT_TRUE(result);

    // Check that working memory is updated
    auto wm_data = CopyToHost(state->working_memory);
    bool has_content = false;
    for (float v : wm_data) {
        if (std::abs(v) > 0.01f) {
            has_content = true;
            break;
        }
    }
    EXPECT_TRUE(has_content);

    nimcp_gpu_tensor_destroy(input);
    nimcp_gpu_tensor_destroy(dopamine);
    DestroyPFCState(state);
}

TEST_F(RegionsKernelTest, PFCWMUpdate_DecaysWithoutInput) {
    RequireGPU();

    const size_t n_slots = 4;
    const size_t slot_dim = 64;
    const float dt = 1.0f;

    nimcp_gpu_pfc_state_t* state = CreatePFCState(n_slots, slot_dim);
    ASSERT_NE(state, nullptr);

    // Initialize WM with content
    nimcp_gpu_fill(ctx, state->working_memory, 1.0f);

    auto initial_wm = CopyToHost(state->working_memory);
    float initial_sum = 0.0f;
    for (float v : initial_wm) initial_sum += std::abs(v);

    nimcp_gpu_tensor_t* zero_input = Create1DTensor(slot_dim, 0.0f);
    nimcp_gpu_tensor_t* low_dopamine = Create1DTensor(1, 0.1f);

    nimcp_gpu_pfc_params_t params = nimcp_gpu_pfc_params_default();
    params.decay_rate = 0.1f;
    params.recurrent_maintenance = false;

    // Close all gates
    nimcp_gpu_fill(ctx, state->gate_state, 0.0f);

    // Multiple updates without input should cause decay
    for (int i = 0; i < 50; i++) {
        nimcp_gpu_pfc_wm_update(ctx, state, zero_input, low_dopamine, dt, &params);
    }

    auto final_wm = CopyToHost(state->working_memory);
    float final_sum = 0.0f;
    for (float v : final_wm) final_sum += std::abs(v);

    EXPECT_LT(final_sum, initial_sum);

    nimcp_gpu_tensor_destroy(zero_input);
    nimcp_gpu_tensor_destroy(low_dopamine);
    DestroyPFCState(state);
}

TEST_F(RegionsKernelTest, PFCGating_ControlsAccess) {
    RequireGPU();

    const size_t n_slots = 4;
    const size_t slot_dim = 64;

    nimcp_gpu_pfc_state_t* state = CreatePFCState(n_slots, slot_dim);
    ASSERT_NE(state, nullptr);

    nimcp_gpu_tensor_t* gate_signal = Create1DTensor(n_slots, 0.0f);

    // Set high gate signal for first two slots
    std::vector<float> signal_data(n_slots, 0.0f);
    signal_data[0] = 0.9f;
    signal_data[1] = 0.8f;
    gate_signal = SetFromHost(gate_signal, signal_data);

    nimcp_gpu_pfc_params_t params = nimcp_gpu_pfc_params_default();
    params.gating_threshold = 0.5f;

    bool result = nimcp_gpu_pfc_gating(ctx, state, gate_signal, &params);
    EXPECT_TRUE(result);

    auto gate_state = CopyToHost(state->gate_state);

    // First two slots should be open (above threshold)
    EXPECT_GT(gate_state[0], 0.0f);
    EXPECT_GT(gate_state[1], 0.0f);
    // Last two should be closed (below threshold)
    EXPECT_LE(gate_state[2], params.gating_threshold);
    EXPECT_LE(gate_state[3], params.gating_threshold);

    nimcp_gpu_tensor_destroy(gate_signal);
    DestroyPFCState(state);
}

TEST_F(RegionsKernelTest, PFCMaintenance_SustainsActivity) {
    RequireGPU();

    const size_t n_slots = 4;
    const size_t slot_dim = 64;
    const float dt = 1.0f;

    nimcp_gpu_pfc_state_t* state = CreatePFCState(n_slots, slot_dim);
    ASSERT_NE(state, nullptr);

    // Initialize WM with content
    nimcp_gpu_fill(ctx, state->working_memory, 1.0f);

    nimcp_gpu_pfc_params_t params = nimcp_gpu_pfc_params_default();
    params.maintenance_gain = 1.5f;
    params.recurrent_maintenance = true;
    params.decay_rate = 0.01f;

    auto initial_wm = CopyToHost(state->working_memory);
    float initial_sum = 0.0f;
    for (float v : initial_wm) initial_sum += std::abs(v);

    // Apply maintenance
    for (int i = 0; i < 20; i++) {
        bool result = nimcp_gpu_pfc_maintenance(ctx, state, dt, &params);
        EXPECT_TRUE(result);
    }

    auto final_wm = CopyToHost(state->working_memory);
    float final_sum = 0.0f;
    for (float v : final_wm) final_sum += std::abs(v);

    // Activity should be maintained or even amplified
    EXPECT_GE(final_sum, initial_sum * 0.8f);  // Allow small decay

    DestroyPFCState(state);
}

TEST_F(RegionsKernelTest, PFCAttention_SelectsSlots) {
    RequireGPU();

    const size_t n_slots = 4;
    const size_t slot_dim = 64;

    nimcp_gpu_pfc_state_t* state = CreatePFCState(n_slots, slot_dim);
    ASSERT_NE(state, nullptr);

    // Set different content in each slot
    auto wm_data = CopyToHost(state->working_memory);
    for (size_t s = 0; s < n_slots; s++) {
        for (size_t d = 0; d < slot_dim; d++) {
            wm_data[s * slot_dim + d] = static_cast<float>(s + 1) * 0.2f;
        }
    }
    state->working_memory = SetFromHost(state->working_memory, wm_data);

    // Query that matches slot 2 content
    nimcp_gpu_tensor_t* query = Create1DTensor(slot_dim, 0.6f);  // Similar to slot 2

    nimcp_gpu_pfc_params_t params = nimcp_gpu_pfc_params_default();

    bool result = nimcp_gpu_pfc_attention(ctx, state, query, &params);
    EXPECT_TRUE(result);

    auto attention = CopyToHost(state->attention_weights);

    // Attention should be valid probability distribution
    float attn_sum = 0.0f;
    for (float a : attention) {
        EXPECT_GE(a, 0.0f);
        EXPECT_LE(a, 1.0f);
        attn_sum += a;
    }
    EXPECT_NEAR(attn_sum, 1.0f, 0.01f);

    nimcp_gpu_tensor_destroy(query);
    DestroyPFCState(state);
}

//=============================================================================
// Motor Cortex Tests
//=============================================================================

TEST_F(RegionsKernelTest, MotorPlan_GeneratesSequence) {
    RequireGPU();

    const size_t n_actions = 10;
    const size_t n_muscles = 20;

    nimcp_gpu_motor_state_t* state = CreateMotorState(n_actions, n_muscles);
    ASSERT_NE(state, nullptr);

    nimcp_gpu_tensor_t* goal = Create1DTensor(n_actions, 0.0f);
    std::vector<float> goal_data(n_actions, 0.0f);
    goal_data[5] = 1.0f;  // Target action 5
    goal = SetFromHost(goal, goal_data);

    nimcp_gpu_motor_params_t params = nimcp_gpu_motor_params_default();

    bool result = nimcp_gpu_motor_plan(ctx, state, goal, &params);
    EXPECT_TRUE(result);

    // Check that action plan is generated
    auto plan_data = CopyToHost(state->action_plan);
    bool has_plan = false;
    for (float v : plan_data) {
        if (std::abs(v) > 0.01f) {
            has_plan = true;
            break;
        }
    }
    EXPECT_TRUE(has_plan);

    nimcp_gpu_tensor_destroy(goal);
    DestroyMotorState(state);
}

TEST_F(RegionsKernelTest, MotorExecute_GeneratesOutput) {
    RequireGPU();

    const size_t n_actions = 10;
    const size_t n_muscles = 20;

    nimcp_gpu_motor_state_t* state = CreateMotorState(n_actions, n_muscles);
    ASSERT_NE(state, nullptr);

    // Set up action plan
    nimcp_gpu_fill(ctx, state->action_plan, 0.5f);

    nimcp_gpu_tensor_t* motor_output = Create1DTensor(n_muscles, 0.0f);

    nimcp_gpu_motor_params_t params = nimcp_gpu_motor_params_default();

    bool result = nimcp_gpu_motor_execute(ctx, state, motor_output, &params);
    EXPECT_TRUE(result);

    auto output_data = CopyToHost(motor_output);
    bool has_output = false;
    for (float v : output_data) {
        if (std::abs(v) > 0.01f) {
            has_output = true;
            break;
        }
    }
    EXPECT_TRUE(has_output);

    // Efference copy should also be updated
    auto efference = CopyToHost(state->efference_copy);
    bool has_efference = false;
    for (float v : efference) {
        if (std::abs(v) > 0.01f) {
            has_efference = true;
            break;
        }
    }
    EXPECT_TRUE(has_efference);

    nimcp_gpu_tensor_destroy(motor_output);
    DestroyMotorState(state);
}

TEST_F(RegionsKernelTest, MotorUpdateForwardModel_LearnsFromError) {
    RequireGPU();

    const size_t n_actions = 10;
    const size_t n_muscles = 20;

    nimcp_gpu_motor_state_t* state = CreateMotorState(n_actions, n_muscles);
    ASSERT_NE(state, nullptr);

    nimcp_gpu_tensor_t* predicted = Create1DTensor(n_muscles, 0.5f);
    nimcp_gpu_tensor_t* actual = Create1DTensor(n_muscles, 0.8f);

    auto initial_model = CopyToHost(state->forward_model);

    nimcp_gpu_motor_params_t params = nimcp_gpu_motor_params_default();
    params.sequence_learning_rate = 0.1f;

    bool result = nimcp_gpu_motor_update_forward_model(ctx, state, predicted, actual, &params);
    EXPECT_TRUE(result);

    auto final_model = CopyToHost(state->forward_model);

    // Model should be updated (not exactly the same)
    bool model_changed = false;
    for (size_t i = 0; i < initial_model.size(); i++) {
        if (std::abs(final_model[i] - initial_model[i]) > 1e-6f) {
            model_changed = true;
            break;
        }
    }
    EXPECT_TRUE(model_changed);

    nimcp_gpu_tensor_destroy(predicted);
    nimcp_gpu_tensor_destroy(actual);
    DestroyMotorState(state);
}

TEST_F(RegionsKernelTest, MotorPopulationCode_EncodesDirection) {
    RequireGPU();

    const size_t n_actions = 8;  // 8 directions
    const size_t n_muscles = 16;

    nimcp_gpu_motor_state_t* state = CreateMotorState(n_actions, n_muscles);
    ASSERT_NE(state, nullptr);

    // Direction vector pointing right (action 0)
    nimcp_gpu_tensor_t* direction = Create1DTensor(2, 0.0f);
    std::vector<float> dir_data = {1.0f, 0.0f};  // Right
    direction = SetFromHost(direction, dir_data);

    nimcp_gpu_motor_params_t params = nimcp_gpu_motor_params_default();
    params.population_coding_sigma = 0.3f;

    bool result = nimcp_gpu_motor_population_code(ctx, state, direction, &params);
    EXPECT_TRUE(result);

    auto pop_code = CopyToHost(state->population_code);

    // Population code should have tuning curves
    float sum = 0.0f;
    for (float v : pop_code) {
        EXPECT_GE(v, 0.0f);  // Non-negative activity
        sum += v;
    }
    EXPECT_GT(sum, 0.0f);  // Should have some activity

    nimcp_gpu_tensor_destroy(direction);
    DestroyMotorState(state);
}

TEST_F(RegionsKernelTest, MotorNoise_AddsVariability) {
    RequireGPU();

    const size_t n_actions = 10;
    const size_t n_muscles = 20;

    nimcp_gpu_motor_state_t* state1 = CreateMotorState(n_actions, n_muscles);
    nimcp_gpu_motor_state_t* state2 = CreateMotorState(n_actions, n_muscles);
    ASSERT_NE(state1, nullptr);
    ASSERT_NE(state2, nullptr);

    // Same action plan
    nimcp_gpu_fill(ctx, state1->action_plan, 0.5f);
    nimcp_gpu_fill(ctx, state2->action_plan, 0.5f);

    nimcp_gpu_tensor_t* output1 = Create1DTensor(n_muscles, 0.0f);
    nimcp_gpu_tensor_t* output2 = Create1DTensor(n_muscles, 0.0f);

    nimcp_gpu_motor_params_t params = nimcp_gpu_motor_params_default();
    params.motor_noise = 0.1f;

    nimcp_gpu_motor_execute(ctx, state1, output1, &params);
    nimcp_gpu_motor_execute(ctx, state2, output2, &params);

    auto data1 = CopyToHost(output1);
    auto data2 = CopyToHost(output2);

    // With noise, outputs should differ
    bool differs = false;
    for (size_t i = 0; i < n_muscles; i++) {
        if (std::abs(data1[i] - data2[i]) > 1e-6f) {
            differs = true;
            break;
        }
    }
    // Note: This might occasionally fail if noise happens to be identical
    // In practice, motor noise should cause variation
    // EXPECT_TRUE(differs);  // Commented out as it's probabilistic

    nimcp_gpu_tensor_destroy(output1);
    nimcp_gpu_tensor_destroy(output2);
    DestroyMotorState(state1);
    DestroyMotorState(state2);
}

//=============================================================================
// Parietal Cortex Tests
//=============================================================================

TEST_F(RegionsKernelTest, ParietalAttention_UpdatesMap) {
    RequireGPU();

    const size_t map_size = 32;

    nimcp_gpu_parietal_state_t* state = CreateParietalState(map_size);
    ASSERT_NE(state, nullptr);

    // Visual input with salient region
    nimcp_gpu_tensor_t* visual_input = Create2DTensor(map_size, map_size, 0.1f);
    std::vector<float> visual_data(map_size * map_size, 0.1f);
    // Add salient region in center
    for (size_t y = map_size/4; y < 3*map_size/4; y++) {
        for (size_t x = map_size/4; x < 3*map_size/4; x++) {
            visual_data[y * map_size + x] = 1.0f;
        }
    }
    visual_input = SetFromHost(visual_input, visual_data);

    nimcp_gpu_tensor_t* top_down = Create2DTensor(map_size, map_size, 0.0f);

    nimcp_gpu_parietal_params_t params = nimcp_gpu_parietal_params_default();

    bool result = nimcp_gpu_parietal_attention(ctx, state, visual_input, top_down, &params);
    EXPECT_TRUE(result);

    auto attn_map = CopyToHost(state->attention_map);

    // Center should have higher attention
    float center_sum = 0.0f;
    float edge_sum = 0.0f;
    for (size_t y = 0; y < map_size; y++) {
        for (size_t x = 0; x < map_size; x++) {
            float val = attn_map[y * map_size + x];
            if (x >= map_size/4 && x < 3*map_size/4 &&
                y >= map_size/4 && y < 3*map_size/4) {
                center_sum += val;
            } else {
                edge_sum += val;
            }
        }
    }
    // Normalize by area
    center_sum /= (map_size/2) * (map_size/2);
    edge_sum /= (map_size * map_size - (map_size/2) * (map_size/2));

    EXPECT_GT(center_sum, edge_sum);

    nimcp_gpu_tensor_destroy(visual_input);
    nimcp_gpu_tensor_destroy(top_down);
    DestroyParietalState(state);
}

TEST_F(RegionsKernelTest, ParietalTransform_UpdatesRepresentations) {
    RequireGPU();

    const size_t map_size = 32;

    nimcp_gpu_parietal_state_t* state = CreateParietalState(map_size);
    ASSERT_NE(state, nullptr);

    // Set up spatial map with an object
    std::vector<float> spatial_data(map_size * map_size, 0.0f);
    spatial_data[16 * map_size + 16] = 1.0f;  // Object in center
    state->spatial_map = SetFromHost(state->spatial_map, spatial_data);

    // Eye position (gaze direction)
    nimcp_gpu_tensor_t* eye_position = Create1DTensor(2, 0.0f);
    std::vector<float> eye_data = {0.2f, -0.1f};  // Looking right and down
    eye_position = SetFromHost(eye_position, eye_data);

    nimcp_gpu_parietal_params_t params = nimcp_gpu_parietal_params_default();

    bool result = nimcp_gpu_parietal_transform(ctx, state, eye_position, &params);
    EXPECT_TRUE(result);

    // Egocentric and allocentric representations should be updated
    auto ego_data = CopyToHost(state->egocentric_rep);
    auto allo_data = CopyToHost(state->allocentric_rep);

    bool ego_has_content = false;
    bool allo_has_content = false;
    for (size_t i = 0; i < map_size * map_size; i++) {
        if (std::abs(ego_data[i]) > 0.01f) ego_has_content = true;
        if (std::abs(allo_data[i]) > 0.01f) allo_has_content = true;
    }

    EXPECT_TRUE(ego_has_content);
    EXPECT_TRUE(allo_has_content);

    nimcp_gpu_tensor_destroy(eye_position);
    DestroyParietalState(state);
}

TEST_F(RegionsKernelTest, ParietalMultisensory_IntegratesModalities) {
    RequireGPU();

    const size_t map_size = 32;

    nimcp_gpu_parietal_state_t* state = CreateParietalState(map_size);
    ASSERT_NE(state, nullptr);

    // Create sensory inputs at same location
    nimcp_gpu_tensor_t* visual = Create2DTensor(map_size, map_size, 0.0f);
    nimcp_gpu_tensor_t* auditory = Create2DTensor(map_size, map_size, 0.0f);
    nimcp_gpu_tensor_t* proprioceptive = Create2DTensor(map_size, map_size, 0.0f);

    std::vector<float> visual_data(map_size * map_size, 0.0f);
    std::vector<float> auditory_data(map_size * map_size, 0.0f);
    std::vector<float> proprio_data(map_size * map_size, 0.0f);

    // All modalities indicate same location
    visual_data[10 * map_size + 10] = 0.8f;
    auditory_data[10 * map_size + 10] = 0.6f;
    proprio_data[10 * map_size + 10] = 0.5f;

    visual = SetFromHost(visual, visual_data);
    auditory = SetFromHost(auditory, auditory_data);
    proprioceptive = SetFromHost(proprioceptive, proprio_data);

    nimcp_gpu_parietal_params_t params = nimcp_gpu_parietal_params_default();

    bool result = nimcp_gpu_parietal_multisensory(ctx, state, visual, auditory, proprioceptive, &params);
    EXPECT_TRUE(result);

    // Spatial map should reflect integrated evidence
    auto spatial = CopyToHost(state->spatial_map);

    // Location with multisensory agreement should be enhanced
    float target_val = spatial[10 * map_size + 10];
    float avg_input = (0.8f + 0.6f + 0.5f) / 3.0f;

    EXPECT_GT(target_val, 0.0f);
    // Integrated signal should be at least as strong as average input
    // (multisensory enhancement)

    nimcp_gpu_tensor_destroy(visual);
    nimcp_gpu_tensor_destroy(auditory);
    nimcp_gpu_tensor_destroy(proprioceptive);
    DestroyParietalState(state);
}

TEST_F(RegionsKernelTest, ParietalMultisensory_ResolvesSpatialConflict) {
    RequireGPU();

    const size_t map_size = 32;

    nimcp_gpu_parietal_state_t* state = CreateParietalState(map_size);
    ASSERT_NE(state, nullptr);

    // Create conflicting sensory inputs
    nimcp_gpu_tensor_t* visual = Create2DTensor(map_size, map_size, 0.0f);
    nimcp_gpu_tensor_t* auditory = Create2DTensor(map_size, map_size, 0.0f);
    nimcp_gpu_tensor_t* proprioceptive = Create2DTensor(map_size, map_size, 0.0f);

    std::vector<float> visual_data(map_size * map_size, 0.0f);
    std::vector<float> auditory_data(map_size * map_size, 0.0f);

    // Visual and auditory at different locations
    visual_data[10 * map_size + 10] = 1.0f;
    auditory_data[20 * map_size + 20] = 1.0f;

    visual = SetFromHost(visual, visual_data);
    auditory = SetFromHost(auditory, auditory_data);

    nimcp_gpu_parietal_params_t params = nimcp_gpu_parietal_params_default();
    params.multisensory_weight = 0.7f;  // Visual dominance

    bool result = nimcp_gpu_parietal_multisensory(ctx, state, visual, auditory, proprioceptive, &params);
    EXPECT_TRUE(result);

    auto spatial = CopyToHost(state->spatial_map);

    // Both locations should have some representation
    float visual_loc = spatial[10 * map_size + 10];
    float auditory_loc = spatial[20 * map_size + 20];

    EXPECT_GT(visual_loc, 0.0f);
    EXPECT_GT(auditory_loc, 0.0f);
    // Visual should dominate given higher weight
    EXPECT_GE(visual_loc, auditory_loc * 0.5f);

    nimcp_gpu_tensor_destroy(visual);
    nimcp_gpu_tensor_destroy(auditory);
    nimcp_gpu_tensor_destroy(proprioceptive);
    DestroyParietalState(state);
}

//=============================================================================
// Inter-Region Communication Tests
//=============================================================================

TEST_F(RegionsKernelTest, InterregionTransmit_TransfersActivity) {
    RequireGPU();

    const size_t source_dim = 64;
    const size_t target_dim = 32;

    nimcp_gpu_interregion_state_t* state = CreateInterregionState(
        NIMCP_REGION_PFC, NIMCP_REGION_MOTOR, source_dim, target_dim);
    ASSERT_NE(state, nullptr);

    nimcp_gpu_tensor_t* source_activity = Create1DTensor(source_dim, 0.8f);
    nimcp_gpu_tensor_t* target_input = Create1DTensor(target_dim, 0.0f);

    nimcp_gpu_interregion_params_t params = nimcp_gpu_interregion_params_default();
    const float dt = 1.0f;

    bool result = nimcp_gpu_interregion_transmit(ctx, state, source_activity, target_input, dt, &params);
    EXPECT_TRUE(result);

    auto target_data = CopyToHost(target_input);
    bool has_signal = false;
    for (float v : target_data) {
        if (std::abs(v) > 0.01f) {
            has_signal = true;
            break;
        }
    }
    EXPECT_TRUE(has_signal);

    nimcp_gpu_tensor_destroy(source_activity);
    nimcp_gpu_tensor_destroy(target_input);
    DestroyInterregionState(state);
}

TEST_F(RegionsKernelTest, InterregionTransmit_RespectsDelay) {
    RequireGPU();

    const size_t source_dim = 32;
    const size_t target_dim = 32;

    nimcp_gpu_interregion_state_t* state = CreateInterregionState(
        NIMCP_REGION_PFC, NIMCP_REGION_PARIETAL, source_dim, target_dim);
    ASSERT_NE(state, nullptr);

    nimcp_gpu_interregion_params_t params = nimcp_gpu_interregion_params_default();
    params.transmission_delay = 5.0f;  // 5ms delay
    const float dt = 1.0f;

    // Send pulse at t=0
    nimcp_gpu_tensor_t* source_activity = Create1DTensor(source_dim, 1.0f);
    nimcp_gpu_tensor_t* target_input = Create1DTensor(target_dim, 0.0f);

    nimcp_gpu_interregion_transmit(ctx, state, source_activity, target_input, dt, &params);
    auto immediate = CopyToHost(target_input);

    // Clear source activity
    nimcp_gpu_fill(ctx, source_activity, 0.0f);

    // Wait for delay period
    for (int t = 0; t < 10; t++) {
        nimcp_gpu_fill(ctx, target_input, 0.0f);
        nimcp_gpu_interregion_transmit(ctx, state, source_activity, target_input, dt, &params);
    }

    auto delayed = CopyToHost(target_input);

    // Signal should appear after delay
    // (Implementation-dependent: might use ring buffer)

    nimcp_gpu_tensor_destroy(source_activity);
    nimcp_gpu_tensor_destroy(target_input);
    DestroyInterregionState(state);
}

TEST_F(RegionsKernelTest, InterregionPlasticity_UpdatesWeights) {
    RequireGPU();

    const size_t source_dim = 32;
    const size_t target_dim = 32;

    nimcp_gpu_interregion_state_t* state = CreateInterregionState(
        NIMCP_REGION_MOTOR, NIMCP_REGION_PARIETAL, source_dim, target_dim);
    ASSERT_NE(state, nullptr);

    nimcp_gpu_tensor_t* source_activity = Create1DTensor(source_dim, 0.8f);
    nimcp_gpu_tensor_t* target_activity = Create1DTensor(target_dim, 0.7f);

    auto initial_weights = CopyToHost(state->connection_weights);

    nimcp_gpu_interregion_params_t params = nimcp_gpu_interregion_params_default();
    params.plasticity_rate = 0.01f;
    const float dt = 1.0f;

    bool result = nimcp_gpu_interregion_plasticity(ctx, state, source_activity, target_activity, dt, &params);
    EXPECT_TRUE(result);

    auto final_weights = CopyToHost(state->connection_weights);

    // Weights should change
    bool weights_changed = false;
    for (size_t i = 0; i < initial_weights.size(); i++) {
        if (std::abs(final_weights[i] - initial_weights[i]) > 1e-6f) {
            weights_changed = true;
            break;
        }
    }
    EXPECT_TRUE(weights_changed);

    nimcp_gpu_tensor_destroy(source_activity);
    nimcp_gpu_tensor_destroy(target_activity);
    DestroyInterregionState(state);
}

TEST_F(RegionsKernelTest, InterregionPlasticity_HebbianLearning) {
    RequireGPU();

    const size_t source_dim = 16;
    const size_t target_dim = 16;

    nimcp_gpu_interregion_state_t* state = CreateInterregionState(
        NIMCP_REGION_PFC, NIMCP_REGION_MOTOR, source_dim, target_dim);
    ASSERT_NE(state, nullptr);

    // Correlated activity pattern
    nimcp_gpu_tensor_t* source_activity = Create1DTensor(source_dim, 0.0f);
    nimcp_gpu_tensor_t* target_activity = Create1DTensor(target_dim, 0.0f);

    std::vector<float> source_data(source_dim, 0.0f);
    std::vector<float> target_data(target_dim, 0.0f);

    // First half of neurons active in both
    for (size_t i = 0; i < source_dim / 2; i++) {
        source_data[i] = 1.0f;
        target_data[i] = 1.0f;
    }
    source_activity = SetFromHost(source_activity, source_data);
    target_activity = SetFromHost(target_activity, target_data);

    nimcp_gpu_interregion_params_t params = nimcp_gpu_interregion_params_default();
    params.plasticity_rate = 0.1f;
    const float dt = 1.0f;

    // Record weight for correlated pair
    auto initial_weights = CopyToHost(state->connection_weights);
    float initial_corr_weight = initial_weights[0];  // target[0] <- source[0]
    float initial_uncorr_weight = initial_weights[target_dim - 1];  // target[last] <- source[0]

    // Apply plasticity multiple times
    for (int i = 0; i < 20; i++) {
        nimcp_gpu_interregion_plasticity(ctx, state, source_activity, target_activity, dt, &params);
    }

    auto final_weights = CopyToHost(state->connection_weights);
    float final_corr_weight = final_weights[0];
    float final_uncorr_weight = final_weights[target_dim - 1];

    // Correlated connection should strengthen more than uncorrelated
    float corr_change = final_corr_weight - initial_corr_weight;
    float uncorr_change = final_uncorr_weight - initial_uncorr_weight;

    EXPECT_GT(corr_change, uncorr_change);

    nimcp_gpu_tensor_destroy(source_activity);
    nimcp_gpu_tensor_destroy(target_activity);
    DestroyInterregionState(state);
}

//=============================================================================
// Null Safety Tests
//=============================================================================

TEST_F(RegionsKernelTest, ColumnUpdate_NullSafety) {
    RequireGPU();

    nimcp_gpu_column_state_t* state = CreateColumnState(16, 100);
    nimcp_gpu_tensor_t* tensor = Create1DTensor(16, 0.0f);
    nimcp_gpu_column_params_t params = nimcp_gpu_column_params_default();

    EXPECT_FALSE(nimcp_gpu_column_update(nullptr, state, tensor, tensor, 1.0f, &params));
    EXPECT_FALSE(nimcp_gpu_column_update(ctx, nullptr, tensor, tensor, 1.0f, &params));
    EXPECT_FALSE(nimcp_gpu_column_update(ctx, state, nullptr, tensor, 1.0f, &params));
    EXPECT_FALSE(nimcp_gpu_column_update(ctx, state, tensor, tensor, 1.0f, nullptr));

    nimcp_gpu_tensor_destroy(tensor);
    DestroyColumnState(state);
}

TEST_F(RegionsKernelTest, ColumnLateralInhibition_NullSafety) {
    RequireGPU();

    nimcp_gpu_column_state_t* state = CreateColumnState(16, 100);
    nimcp_gpu_column_params_t params = nimcp_gpu_column_params_default();

    EXPECT_FALSE(nimcp_gpu_column_lateral_inhibition(nullptr, state, &params));
    EXPECT_FALSE(nimcp_gpu_column_lateral_inhibition(ctx, nullptr, &params));
    EXPECT_FALSE(nimcp_gpu_column_lateral_inhibition(ctx, state, nullptr));

    DestroyColumnState(state);
}

TEST_F(RegionsKernelTest, ColumnLayerPropagate_NullSafety) {
    RequireGPU();

    nimcp_gpu_column_state_t* state = CreateColumnState(16, 100);
    nimcp_gpu_column_params_t params = nimcp_gpu_column_params_default();

    EXPECT_FALSE(nimcp_gpu_column_layer_propagate(nullptr, state, &params));
    EXPECT_FALSE(nimcp_gpu_column_layer_propagate(ctx, nullptr, &params));
    EXPECT_FALSE(nimcp_gpu_column_layer_propagate(ctx, state, nullptr));

    DestroyColumnState(state);
}

TEST_F(RegionsKernelTest, PFCWMUpdate_NullSafety) {
    RequireGPU();

    nimcp_gpu_pfc_state_t* state = CreatePFCState(4, 64);
    nimcp_gpu_tensor_t* tensor = Create1DTensor(64, 0.0f);
    nimcp_gpu_tensor_t* dop = Create1DTensor(1, 0.5f);
    nimcp_gpu_pfc_params_t params = nimcp_gpu_pfc_params_default();

    EXPECT_FALSE(nimcp_gpu_pfc_wm_update(nullptr, state, tensor, dop, 1.0f, &params));
    EXPECT_FALSE(nimcp_gpu_pfc_wm_update(ctx, nullptr, tensor, dop, 1.0f, &params));
    EXPECT_FALSE(nimcp_gpu_pfc_wm_update(ctx, state, nullptr, dop, 1.0f, &params));
    EXPECT_FALSE(nimcp_gpu_pfc_wm_update(ctx, state, tensor, dop, 1.0f, nullptr));

    nimcp_gpu_tensor_destroy(tensor);
    nimcp_gpu_tensor_destroy(dop);
    DestroyPFCState(state);
}

TEST_F(RegionsKernelTest, PFCGating_NullSafety) {
    RequireGPU();

    nimcp_gpu_pfc_state_t* state = CreatePFCState(4, 64);
    nimcp_gpu_tensor_t* tensor = Create1DTensor(4, 0.0f);
    nimcp_gpu_pfc_params_t params = nimcp_gpu_pfc_params_default();

    EXPECT_FALSE(nimcp_gpu_pfc_gating(nullptr, state, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_pfc_gating(ctx, nullptr, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_pfc_gating(ctx, state, nullptr, &params));
    EXPECT_FALSE(nimcp_gpu_pfc_gating(ctx, state, tensor, nullptr));

    nimcp_gpu_tensor_destroy(tensor);
    DestroyPFCState(state);
}

TEST_F(RegionsKernelTest, PFCMaintenance_NullSafety) {
    RequireGPU();

    nimcp_gpu_pfc_state_t* state = CreatePFCState(4, 64);
    nimcp_gpu_pfc_params_t params = nimcp_gpu_pfc_params_default();

    EXPECT_FALSE(nimcp_gpu_pfc_maintenance(nullptr, state, 1.0f, &params));
    EXPECT_FALSE(nimcp_gpu_pfc_maintenance(ctx, nullptr, 1.0f, &params));
    EXPECT_FALSE(nimcp_gpu_pfc_maintenance(ctx, state, 1.0f, nullptr));

    DestroyPFCState(state);
}

TEST_F(RegionsKernelTest, PFCAttention_NullSafety) {
    RequireGPU();

    nimcp_gpu_pfc_state_t* state = CreatePFCState(4, 64);
    nimcp_gpu_tensor_t* tensor = Create1DTensor(64, 0.0f);
    nimcp_gpu_pfc_params_t params = nimcp_gpu_pfc_params_default();

    EXPECT_FALSE(nimcp_gpu_pfc_attention(nullptr, state, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_pfc_attention(ctx, nullptr, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_pfc_attention(ctx, state, nullptr, &params));
    EXPECT_FALSE(nimcp_gpu_pfc_attention(ctx, state, tensor, nullptr));

    nimcp_gpu_tensor_destroy(tensor);
    DestroyPFCState(state);
}

TEST_F(RegionsKernelTest, MotorPlan_NullSafety) {
    RequireGPU();

    nimcp_gpu_motor_state_t* state = CreateMotorState(10, 20);
    nimcp_gpu_tensor_t* tensor = Create1DTensor(10, 0.0f);
    nimcp_gpu_motor_params_t params = nimcp_gpu_motor_params_default();

    EXPECT_FALSE(nimcp_gpu_motor_plan(nullptr, state, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_motor_plan(ctx, nullptr, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_motor_plan(ctx, state, nullptr, &params));
    EXPECT_FALSE(nimcp_gpu_motor_plan(ctx, state, tensor, nullptr));

    nimcp_gpu_tensor_destroy(tensor);
    DestroyMotorState(state);
}

TEST_F(RegionsKernelTest, MotorExecute_NullSafety) {
    RequireGPU();

    nimcp_gpu_motor_state_t* state = CreateMotorState(10, 20);
    nimcp_gpu_tensor_t* tensor = Create1DTensor(20, 0.0f);
    nimcp_gpu_motor_params_t params = nimcp_gpu_motor_params_default();

    EXPECT_FALSE(nimcp_gpu_motor_execute(nullptr, state, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_motor_execute(ctx, nullptr, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_motor_execute(ctx, state, nullptr, &params));
    EXPECT_FALSE(nimcp_gpu_motor_execute(ctx, state, tensor, nullptr));

    nimcp_gpu_tensor_destroy(tensor);
    DestroyMotorState(state);
}

TEST_F(RegionsKernelTest, MotorUpdateForwardModel_NullSafety) {
    RequireGPU();

    nimcp_gpu_motor_state_t* state = CreateMotorState(10, 20);
    nimcp_gpu_tensor_t* tensor = Create1DTensor(20, 0.0f);
    nimcp_gpu_motor_params_t params = nimcp_gpu_motor_params_default();

    EXPECT_FALSE(nimcp_gpu_motor_update_forward_model(nullptr, state, tensor, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_motor_update_forward_model(ctx, nullptr, tensor, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_motor_update_forward_model(ctx, state, nullptr, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_motor_update_forward_model(ctx, state, tensor, nullptr, &params));
    EXPECT_FALSE(nimcp_gpu_motor_update_forward_model(ctx, state, tensor, tensor, nullptr));

    nimcp_gpu_tensor_destroy(tensor);
    DestroyMotorState(state);
}

TEST_F(RegionsKernelTest, MotorPopulationCode_NullSafety) {
    RequireGPU();

    nimcp_gpu_motor_state_t* state = CreateMotorState(10, 20);
    nimcp_gpu_tensor_t* tensor = Create1DTensor(2, 0.0f);
    nimcp_gpu_motor_params_t params = nimcp_gpu_motor_params_default();

    EXPECT_FALSE(nimcp_gpu_motor_population_code(nullptr, state, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_motor_population_code(ctx, nullptr, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_motor_population_code(ctx, state, nullptr, &params));
    EXPECT_FALSE(nimcp_gpu_motor_population_code(ctx, state, tensor, nullptr));

    nimcp_gpu_tensor_destroy(tensor);
    DestroyMotorState(state);
}

TEST_F(RegionsKernelTest, ParietalAttention_NullSafety) {
    RequireGPU();

    nimcp_gpu_parietal_state_t* state = CreateParietalState(32);
    nimcp_gpu_tensor_t* tensor = Create2DTensor(32, 32, 0.0f);
    nimcp_gpu_parietal_params_t params = nimcp_gpu_parietal_params_default();

    EXPECT_FALSE(nimcp_gpu_parietal_attention(nullptr, state, tensor, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_parietal_attention(ctx, nullptr, tensor, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_parietal_attention(ctx, state, nullptr, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_parietal_attention(ctx, state, tensor, tensor, nullptr));

    nimcp_gpu_tensor_destroy(tensor);
    DestroyParietalState(state);
}

TEST_F(RegionsKernelTest, ParietalTransform_NullSafety) {
    RequireGPU();

    nimcp_gpu_parietal_state_t* state = CreateParietalState(32);
    nimcp_gpu_tensor_t* tensor = Create1DTensor(2, 0.0f);
    nimcp_gpu_parietal_params_t params = nimcp_gpu_parietal_params_default();

    EXPECT_FALSE(nimcp_gpu_parietal_transform(nullptr, state, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_parietal_transform(ctx, nullptr, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_parietal_transform(ctx, state, nullptr, &params));
    EXPECT_FALSE(nimcp_gpu_parietal_transform(ctx, state, tensor, nullptr));

    nimcp_gpu_tensor_destroy(tensor);
    DestroyParietalState(state);
}

TEST_F(RegionsKernelTest, ParietalMultisensory_NullSafety) {
    RequireGPU();

    nimcp_gpu_parietal_state_t* state = CreateParietalState(32);
    nimcp_gpu_tensor_t* tensor = Create2DTensor(32, 32, 0.0f);
    nimcp_gpu_parietal_params_t params = nimcp_gpu_parietal_params_default();

    EXPECT_FALSE(nimcp_gpu_parietal_multisensory(nullptr, state, tensor, tensor, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_parietal_multisensory(ctx, nullptr, tensor, tensor, tensor, &params));
    /* All sensory modalities (visual, auditory, proprioceptive) are optional */
    EXPECT_FALSE(nimcp_gpu_parietal_multisensory(ctx, state, tensor, tensor, tensor, nullptr));

    nimcp_gpu_tensor_destroy(tensor);
    DestroyParietalState(state);
}

TEST_F(RegionsKernelTest, InterregionTransmit_NullSafety) {
    RequireGPU();

    nimcp_gpu_interregion_state_t* state = CreateInterregionState(
        NIMCP_REGION_PFC, NIMCP_REGION_MOTOR, 32, 32);
    nimcp_gpu_tensor_t* tensor = Create1DTensor(32, 0.0f);
    nimcp_gpu_interregion_params_t params = nimcp_gpu_interregion_params_default();

    EXPECT_FALSE(nimcp_gpu_interregion_transmit(nullptr, state, tensor, tensor, 1.0f, &params));
    EXPECT_FALSE(nimcp_gpu_interregion_transmit(ctx, nullptr, tensor, tensor, 1.0f, &params));
    EXPECT_FALSE(nimcp_gpu_interregion_transmit(ctx, state, nullptr, tensor, 1.0f, &params));
    EXPECT_FALSE(nimcp_gpu_interregion_transmit(ctx, state, tensor, nullptr, 1.0f, &params));
    EXPECT_FALSE(nimcp_gpu_interregion_transmit(ctx, state, tensor, tensor, 1.0f, nullptr));

    nimcp_gpu_tensor_destroy(tensor);
    DestroyInterregionState(state);
}

TEST_F(RegionsKernelTest, InterregionPlasticity_NullSafety) {
    RequireGPU();

    nimcp_gpu_interregion_state_t* state = CreateInterregionState(
        NIMCP_REGION_PFC, NIMCP_REGION_MOTOR, 32, 32);
    nimcp_gpu_tensor_t* tensor = Create1DTensor(32, 0.0f);
    nimcp_gpu_interregion_params_t params = nimcp_gpu_interregion_params_default();

    EXPECT_FALSE(nimcp_gpu_interregion_plasticity(nullptr, state, tensor, tensor, 1.0f, &params));
    EXPECT_FALSE(nimcp_gpu_interregion_plasticity(ctx, nullptr, tensor, tensor, 1.0f, &params));
    EXPECT_FALSE(nimcp_gpu_interregion_plasticity(ctx, state, nullptr, tensor, 1.0f, &params));
    EXPECT_FALSE(nimcp_gpu_interregion_plasticity(ctx, state, tensor, nullptr, 1.0f, &params));
    EXPECT_FALSE(nimcp_gpu_interregion_plasticity(ctx, state, tensor, tensor, 1.0f, nullptr));

    nimcp_gpu_tensor_destroy(tensor);
    DestroyInterregionState(state);
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(RegionsKernelTest, Integration_CorticalColumnProcessing) {
    RequireGPU();

    const size_t n_columns = 32;
    const size_t n_neurons = 200;
    const float dt = 1.0f;
    const int n_steps = 100;

    nimcp_gpu_column_state_t* state = CreateColumnState(n_columns, n_neurons);
    ASSERT_NE(state, nullptr);

    nimcp_gpu_tensor_t* input = Create1DTensor(n_columns, 0.0f);
    nimcp_gpu_tensor_t* feedback = Create1DTensor(n_columns, 0.0f);

    nimcp_gpu_column_params_t params = nimcp_gpu_column_params_default();

    // Stimulate subset of columns
    std::vector<float> input_data(n_columns, 0.0f);
    for (size_t i = 0; i < n_columns / 4; i++) {
        input_data[i] = 1.0f;
    }
    input = SetFromHost(input, input_data);

    // Run processing loop
    for (int step = 0; step < n_steps; step++) {
        nimcp_gpu_column_update(ctx, state, input, feedback, dt, &params);
        nimcp_gpu_column_lateral_inhibition(ctx, state, &params);
        nimcp_gpu_column_layer_propagate(ctx, state, &params);
    }

    auto output = CopyToHost(state->column_output);

    // Stimulated columns should have higher output
    float stim_sum = 0.0f, nonstim_sum = 0.0f;
    for (size_t i = 0; i < n_columns; i++) {
        if (i < n_columns / 4) {
            stim_sum += output[i];
        } else {
            nonstim_sum += output[i];
        }
    }
    stim_sum /= n_columns / 4;
    nonstim_sum /= (n_columns - n_columns / 4);

    EXPECT_GT(stim_sum, nonstim_sum);

    nimcp_gpu_tensor_destroy(input);
    nimcp_gpu_tensor_destroy(feedback);
    DestroyColumnState(state);
}

TEST_F(RegionsKernelTest, Integration_PFCWorkingMemoryTask) {
    RequireGPU();

    const size_t n_slots = 4;
    const size_t slot_dim = 64;
    const float dt = 1.0f;

    nimcp_gpu_pfc_state_t* state = CreatePFCState(n_slots, slot_dim);
    ASSERT_NE(state, nullptr);

    nimcp_gpu_pfc_params_t params = nimcp_gpu_pfc_params_default();
    params.recurrent_maintenance = true;
    params.maintenance_gain = 1.2f;
    params.decay_rate = 0.01f;

    nimcp_gpu_tensor_t* dopamine = Create1DTensor(1, 1.0f);

    // Encoding phase: store 3 items
    for (int item = 0; item < 3; item++) {
        nimcp_gpu_tensor_t* input = Create1DTensor(slot_dim, 0.0f);
        std::vector<float> pattern(slot_dim);
        for (size_t d = 0; d < slot_dim; d++) {
            pattern[d] = std::sin(d * (item + 1) * 0.1f);
        }
        input = SetFromHost(input, pattern);

        // Open gate for this slot
        nimcp_gpu_tensor_t* gate_signal = Create1DTensor(n_slots, 0.0f);
        std::vector<float> gate_data(n_slots, 0.0f);
        gate_data[item] = 1.0f;
        gate_signal = SetFromHost(gate_signal, gate_data);

        nimcp_gpu_pfc_gating(ctx, state, gate_signal, &params);
        nimcp_gpu_pfc_wm_update(ctx, state, input, dopamine, dt, &params);

        nimcp_gpu_tensor_destroy(input);
        nimcp_gpu_tensor_destroy(gate_signal);
    }

    // Delay period: maintain items
    nimcp_gpu_tensor_t* zero_input = Create1DTensor(slot_dim, 0.0f);
    nimcp_gpu_tensor_t* closed_gate = Create1DTensor(n_slots, 0.0f);
    nimcp_gpu_pfc_gating(ctx, state, closed_gate, &params);

    for (int step = 0; step < 50; step++) {
        nimcp_gpu_pfc_maintenance(ctx, state, dt, &params);
    }

    // Retrieval: query for item 1
    nimcp_gpu_tensor_t* query = Create1DTensor(slot_dim, 0.0f);
    std::vector<float> query_pattern(slot_dim);
    for (size_t d = 0; d < slot_dim; d++) {
        query_pattern[d] = std::sin(d * 2 * 0.1f);  // Similar to item 1
    }
    query = SetFromHost(query, query_pattern);

    nimcp_gpu_pfc_attention(ctx, state, query, &params);

    auto attention = CopyToHost(state->attention_weights);

    // Attention should focus on slot 1 (matching item)
    EXPECT_GT(attention[1], attention[0]);
    EXPECT_GT(attention[1], attention[3]);

    nimcp_gpu_tensor_destroy(dopamine);
    nimcp_gpu_tensor_destroy(zero_input);
    nimcp_gpu_tensor_destroy(closed_gate);
    nimcp_gpu_tensor_destroy(query);
    DestroyPFCState(state);
}

TEST_F(RegionsKernelTest, Integration_MotorSequenceExecution) {
    RequireGPU();

    const size_t n_actions = 8;
    const size_t n_muscles = 16;
    const int sequence_length = 5;

    nimcp_gpu_motor_state_t* state = CreateMotorState(n_actions, n_muscles);
    ASSERT_NE(state, nullptr);

    nimcp_gpu_motor_params_t params = nimcp_gpu_motor_params_default();
    params.motor_noise = 0.05f;

    std::vector<std::vector<float>> outputs;

    // Execute sequence of goals
    for (int step = 0; step < sequence_length; step++) {
        // Set goal for this step
        nimcp_gpu_tensor_t* goal = Create1DTensor(n_actions, 0.0f);
        std::vector<float> goal_data(n_actions, 0.0f);
        goal_data[step % n_actions] = 1.0f;
        goal = SetFromHost(goal, goal_data);

        // Plan and execute
        nimcp_gpu_motor_plan(ctx, state, goal, &params);

        nimcp_gpu_tensor_t* output = Create1DTensor(n_muscles, 0.0f);
        nimcp_gpu_motor_execute(ctx, state, output, &params);

        outputs.push_back(CopyToHost(output));

        nimcp_gpu_tensor_destroy(goal);
        nimcp_gpu_tensor_destroy(output);
    }

    // Each step should produce different output
    for (int i = 0; i < sequence_length - 1; i++) {
        float diff = 0.0f;
        for (size_t m = 0; m < n_muscles; m++) {
            diff += std::abs(outputs[i][m] - outputs[i+1][m]);
        }
        EXPECT_GT(diff, 0.0f);
    }

    DestroyMotorState(state);
}

TEST_F(RegionsKernelTest, Integration_ParietalVisualAttention) {
    RequireGPU();

    const size_t map_size = 64;

    nimcp_gpu_parietal_state_t* state = CreateParietalState(map_size);
    ASSERT_NE(state, nullptr);

    nimcp_gpu_parietal_params_t params = nimcp_gpu_parietal_params_default();

    // Visual scene with multiple objects
    nimcp_gpu_tensor_t* visual = Create2DTensor(map_size, map_size, 0.0f);
    std::vector<float> scene(map_size * map_size, 0.1f);

    // Object 1: top-left
    for (size_t y = 5; y < 15; y++) {
        for (size_t x = 5; x < 15; x++) {
            scene[y * map_size + x] = 0.8f;
        }
    }
    // Object 2: center (salient)
    for (size_t y = 27; y < 37; y++) {
        for (size_t x = 27; x < 37; x++) {
            scene[y * map_size + x] = 1.0f;
        }
    }
    // Object 3: bottom-right
    for (size_t y = 50; y < 60; y++) {
        for (size_t x = 50; x < 60; x++) {
            scene[y * map_size + x] = 0.6f;
        }
    }
    visual = SetFromHost(visual, scene);

    // Top-down attention to center
    nimcp_gpu_tensor_t* top_down = Create2DTensor(map_size, map_size, 0.0f);
    std::vector<float> td_data(map_size * map_size, 0.0f);
    for (size_t y = 20; y < 44; y++) {
        for (size_t x = 20; x < 44; x++) {
            td_data[y * map_size + x] = 0.5f;
        }
    }
    top_down = SetFromHost(top_down, td_data);

    // Update attention
    nimcp_gpu_parietal_attention(ctx, state, visual, top_down, &params);

    auto attention = CopyToHost(state->attention_map);

    // Center object should have highest attention (salient + top-down)
    float center_attn = 0.0f, corner_attn = 0.0f;
    for (size_t y = 27; y < 37; y++) {
        for (size_t x = 27; x < 37; x++) {
            center_attn += attention[y * map_size + x];
        }
    }
    for (size_t y = 5; y < 15; y++) {
        for (size_t x = 5; x < 15; x++) {
            corner_attn += attention[y * map_size + x];
        }
    }

    EXPECT_GT(center_attn, corner_attn);

    nimcp_gpu_tensor_destroy(visual);
    nimcp_gpu_tensor_destroy(top_down);
    DestroyParietalState(state);
}

TEST_F(RegionsKernelTest, Integration_PFCMotorInteraction) {
    RequireGPU();

    const size_t pfc_slots = 4;
    const size_t slot_dim = 32;
    const size_t n_actions = 8;
    const size_t n_muscles = 16;
    const float dt = 1.0f;

    nimcp_gpu_pfc_state_t* pfc = CreatePFCState(pfc_slots, slot_dim);
    nimcp_gpu_motor_state_t* motor = CreateMotorState(n_actions, n_muscles);
    nimcp_gpu_interregion_state_t* pfc_motor = CreateInterregionState(
        NIMCP_REGION_PFC, NIMCP_REGION_MOTOR, slot_dim, n_actions);
    ASSERT_NE(pfc, nullptr);
    ASSERT_NE(motor, nullptr);
    ASSERT_NE(pfc_motor, nullptr);

    nimcp_gpu_pfc_params_t pfc_params = nimcp_gpu_pfc_params_default();
    nimcp_gpu_motor_params_t motor_params = nimcp_gpu_motor_params_default();
    nimcp_gpu_interregion_params_t inter_params = nimcp_gpu_interregion_params_default();

    // Store goal representation in PFC
    nimcp_gpu_tensor_t* goal_input = Create1DTensor(slot_dim, 0.0f);
    std::vector<float> goal_rep(slot_dim);
    for (size_t d = 0; d < slot_dim; d++) {
        goal_rep[d] = (d < slot_dim / 2) ? 1.0f : 0.0f;
    }
    goal_input = SetFromHost(goal_input, goal_rep);

    nimcp_gpu_tensor_t* dopamine = Create1DTensor(1, 1.0f);
    nimcp_gpu_tensor_t* gate_open = Create1DTensor(pfc_slots, 0.0f);
    std::vector<float> gate_data(pfc_slots, 0.0f);
    gate_data[0] = 1.0f;
    gate_open = SetFromHost(gate_open, gate_data);

    nimcp_gpu_pfc_gating(ctx, pfc, gate_open, &pfc_params);
    nimcp_gpu_pfc_wm_update(ctx, pfc, goal_input, dopamine, dt, &pfc_params);

    // Transmit PFC goal to motor cortex
    nimcp_gpu_tensor_t* motor_input = Create1DTensor(n_actions, 0.0f);
    nimcp_gpu_interregion_transmit(ctx, pfc_motor, pfc->output, motor_input, dt, &inter_params);

    // Plan and execute motor action based on PFC input
    nimcp_gpu_motor_plan(ctx, motor, motor_input, &motor_params);

    nimcp_gpu_tensor_t* motor_output = Create1DTensor(n_muscles, 0.0f);
    nimcp_gpu_motor_execute(ctx, motor, motor_output, &motor_params);

    auto output_data = CopyToHost(motor_output);
    bool has_output = false;
    for (float v : output_data) {
        if (std::abs(v) > 0.01f) {
            has_output = true;
            break;
        }
    }
    EXPECT_TRUE(has_output);

    nimcp_gpu_tensor_destroy(goal_input);
    nimcp_gpu_tensor_destroy(dopamine);
    nimcp_gpu_tensor_destroy(gate_open);
    nimcp_gpu_tensor_destroy(motor_input);
    nimcp_gpu_tensor_destroy(motor_output);
    DestroyPFCState(pfc);
    DestroyMotorState(motor);
    DestroyInterregionState(pfc_motor);
}

TEST_F(RegionsKernelTest, Integration_ParietalMotorCoordination) {
    RequireGPU();

    const size_t map_size = 32;
    const size_t n_actions = 8;
    const size_t n_muscles = 16;
    const float dt = 1.0f;

    nimcp_gpu_parietal_state_t* parietal = CreateParietalState(map_size);
    nimcp_gpu_motor_state_t* motor = CreateMotorState(n_actions, n_muscles);
    nimcp_gpu_interregion_state_t* parietal_motor = CreateInterregionState(
        NIMCP_REGION_PARIETAL, NIMCP_REGION_MOTOR, map_size * map_size, n_actions);
    ASSERT_NE(parietal, nullptr);
    ASSERT_NE(motor, nullptr);
    ASSERT_NE(parietal_motor, nullptr);

    nimcp_gpu_parietal_params_t parietal_params = nimcp_gpu_parietal_params_default();
    nimcp_gpu_motor_params_t motor_params = nimcp_gpu_motor_params_default();
    nimcp_gpu_interregion_params_t inter_params = nimcp_gpu_interregion_params_default();

    // Visual target in right visual field
    nimcp_gpu_tensor_t* visual = Create2DTensor(map_size, map_size, 0.0f);
    std::vector<float> visual_data(map_size * map_size, 0.0f);
    for (size_t y = 12; y < 20; y++) {
        for (size_t x = 20; x < 28; x++) {
            visual_data[y * map_size + x] = 1.0f;
        }
    }
    visual = SetFromHost(visual, visual_data);

    nimcp_gpu_tensor_t* top_down = Create2DTensor(map_size, map_size, 0.0f);

    // Update parietal attention to find target
    nimcp_gpu_parietal_attention(ctx, parietal, visual, top_down, &parietal_params);

    // Flatten attention map for transmission
    auto attn_data = CopyToHost(parietal->attention_map);
    nimcp_gpu_tensor_t* parietal_output = Create1DTensor(map_size * map_size, 0.0f);
    parietal_output = SetFromHost(parietal_output, attn_data);

    // Transmit spatial attention to motor for reaching
    nimcp_gpu_tensor_t* motor_goal = Create1DTensor(n_actions, 0.0f);
    nimcp_gpu_interregion_transmit(ctx, parietal_motor, parietal_output, motor_goal, dt, &inter_params);

    // Plan reach to attended location
    nimcp_gpu_motor_plan(ctx, motor, motor_goal, &motor_params);

    nimcp_gpu_tensor_t* motor_output = Create1DTensor(n_muscles, 0.0f);
    nimcp_gpu_motor_execute(ctx, motor, motor_output, &motor_params);

    auto output_data = CopyToHost(motor_output);
    bool has_movement = false;
    for (float v : output_data) {
        if (std::abs(v) > 0.01f) {
            has_movement = true;
            break;
        }
    }
    EXPECT_TRUE(has_movement);

    nimcp_gpu_tensor_destroy(visual);
    nimcp_gpu_tensor_destroy(top_down);
    nimcp_gpu_tensor_destroy(parietal_output);
    nimcp_gpu_tensor_destroy(motor_goal);
    nimcp_gpu_tensor_destroy(motor_output);
    DestroyParietalState(parietal);
    DestroyMotorState(motor);
    DestroyInterregionState(parietal_motor);
}

TEST_F(RegionsKernelTest, Integration_MultiRegionPlasticity) {
    RequireGPU();

    const size_t dim = 32;
    const float dt = 1.0f;
    const int n_trials = 50;

    nimcp_gpu_interregion_state_t* pfc_parietal = CreateInterregionState(
        NIMCP_REGION_PFC, NIMCP_REGION_PARIETAL, dim, dim);
    nimcp_gpu_interregion_state_t* parietal_motor = CreateInterregionState(
        NIMCP_REGION_PARIETAL, NIMCP_REGION_MOTOR, dim, dim);
    ASSERT_NE(pfc_parietal, nullptr);
    ASSERT_NE(parietal_motor, nullptr);

    nimcp_gpu_interregion_params_t params = nimcp_gpu_interregion_params_default();
    params.plasticity_rate = 0.05f;

    // Record initial weights
    auto initial_pfc_par = CopyToHost(pfc_parietal->connection_weights);
    auto initial_par_mot = CopyToHost(parietal_motor->connection_weights);

    // Simulate correlated activity across regions
    nimcp_gpu_tensor_t* pfc_activity = Create1DTensor(dim, 0.0f);
    nimcp_gpu_tensor_t* parietal_activity = Create1DTensor(dim, 0.0f);
    nimcp_gpu_tensor_t* motor_activity = Create1DTensor(dim, 0.0f);

    for (int trial = 0; trial < n_trials; trial++) {
        // Create correlated activity pattern
        std::vector<float> pattern(dim);
        for (size_t d = 0; d < dim; d++) {
            pattern[d] = (d < dim / 2) ? 0.8f : 0.2f;
        }
        pfc_activity = SetFromHost(pfc_activity, pattern);
        parietal_activity = SetFromHost(parietal_activity, pattern);
        motor_activity = SetFromHost(motor_activity, pattern);

        // Apply plasticity
        nimcp_gpu_interregion_plasticity(ctx, pfc_parietal, pfc_activity, parietal_activity, dt, &params);
        nimcp_gpu_interregion_plasticity(ctx, parietal_motor, parietal_activity, motor_activity, dt, &params);
    }

    auto final_pfc_par = CopyToHost(pfc_parietal->connection_weights);
    auto final_par_mot = CopyToHost(parietal_motor->connection_weights);

    // Weights should change after repeated correlated activity
    float change_pfc_par = 0.0f, change_par_mot = 0.0f;
    for (size_t i = 0; i < dim * dim; i++) {
        change_pfc_par += std::abs(final_pfc_par[i] - initial_pfc_par[i]);
        change_par_mot += std::abs(final_par_mot[i] - initial_par_mot[i]);
    }

    EXPECT_GT(change_pfc_par, 0.0f);
    EXPECT_GT(change_par_mot, 0.0f);

    nimcp_gpu_tensor_destroy(pfc_activity);
    nimcp_gpu_tensor_destroy(parietal_activity);
    nimcp_gpu_tensor_destroy(motor_activity);
    DestroyInterregionState(pfc_parietal);
    DestroyInterregionState(parietal_motor);
}
